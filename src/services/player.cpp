#include "../../include/services/player.hpp"

#include "../../include/config/config.hpp"

#include <spdlog/spdlog.h>
#include <cstdint>
#include <memory>
#include <string>
#include <mutex>
#include <sys/stat.h>
#include <type_traits>

services::Player::Player(const std::string_view& app_name, const std::string_view& app_name_human, const uint64_t app_id) {
    this->app_name = app_name;

    mpris_service = Mpris::make(this->app_name);
    mpv_service = std::make_unique<MPV>();
    music_service = std::make_unique<Music>(this->app_name);
    social_service = std::make_unique<Social>(app_id);

    if (!mpris_service) {
        spdlog::error("PLAYER: mpris service initialisation failed.");
    } else if (!music_service) {
        spdlog::error("PLAYER: music service initialisation failed.");
    } else if (!social_service) {
        spdlog::error("PLAYER: social service initialisation failed.");
    }

    worker_thread = std::thread(&Player::worker_loop, this);

    mpris_service->setHumanName(app_name_human);
    mpris_service->onQuit([&] {});

    mpris_service->onPause([&] {
        {
            std::lock_guard lock(state_mutex);
            state.is_streaming_audio = false;
        }

        mpv_service->pause();
        social_service->pause();

        mpris_service->setPosition(mpv_service->getStreamPosition());
        mpris_service->setPlaybackStatus(services::PlaybackStatus::Paused);
    });

    mpris_service->onToggle([&] {
        {
            std::lock_guard lock(state_mutex);
            state.is_streaming_audio = !state.is_streaming_audio;
        }

        if(state.is_streaming_audio) {
            mpv_service->pause();
            social_service->pause();
        } else {
            mpv_service->resume();
            social_service->resume();
        }

        mpris_service->setPlaybackStatus(state.is_streaming_audio ? services::PlaybackStatus::Playing : services::PlaybackStatus::Paused);
    });

    mpris_service->onStop([&] {
        {
            std::lock_guard lock(state_mutex);
            state.is_streaming_audio = false;
        }

        mpv_service->stop();
        social_service->removeStatus();
        mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);
    });

    mpris_service->onPlay([&] {
        {
            std::lock_guard lock(state_mutex);
            state.is_streaming_audio = true;
        }

        mpv_service->resume();
        social_service->resume();
        social_service->setPosition(mpv_service->getStreamPosition());

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Playing);
    });

    mpris_service->onSeek([&] (int64_t p) {
        uint64_t new_position = mpv_service->getStreamPosition();

        if(p < 0) {
            mpv_service->seekBackward(p);
            new_position -= p;
        } else {
            mpv_service->seekForward(p);
            new_position += p;
        }

        social_service->setPosition(new_position);
        mpris_service->setPosition(new_position);
    });

    mpris_service->onSetPosition([&] (int64_t p) {
        mpv_service->setPosition(p);

        social_service->setPosition(p);
        mpris_service->setPosition(p);
    });

    mpris_service->onLoopStatusChanged([&] (services::LoopStatus status) { });
    mpris_service->onShuffleChanged([&] (bool shuffle) { });

    mpris_service->onNext([this](){
        skipForward();
    });

    mpris_service->onPrevious([this](){
        skipBackward();
    });

    mpris_service->setIsNextPossible(false);
    mpris_service->setIsPreviousPossible(false);

    mpris_service->startLoopAsync();

    mpv_service->setOnStreamEnd([this] {
        spdlog::info("PLAYER: Stream end");
        mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);

        std::shared_ptr<music::IStreamable> radio_next;
        music::Radio state_radio;

        bool start_next_radio = false;
        bool to_radio_skip = false;
        bool should_skip = false;

        {
            std::lock_guard lock(state_mutex);

            state.is_streaming_audio = false;

            /* Since we use mpv playlist feature for buffering, we need to increment queue position no matter what.
            - This means that queue position can be equal or greater than the queue size
            - So we check for that.
            */
            this->state.queue_position++;

            if(!this->state.radio_queue.empty() && state.queue_position >= state.user_queue.size()) {
                to_radio_skip = true;

                radio_next = state.radio_queue.front();
                state.radio_queue.pop_front();
                state.current = radio_next;
            }

            if(this->state.autoplay && this->state.radio_queue.empty()) {
                start_next_radio = true;
                state_radio = state.radio;
            }

            if (state.queue_position < state.user_queue.size()) {
                state.current = state.user_queue[state.queue_position];
                should_skip = true;
            }
        }

        if(start_next_radio) {
            {
                std::unique_lock lock(command_mutex);
                command_queue.push(QueueNextRadioCommand{state_radio});
            }

            command_cv.notify_one();
        }

        if(to_radio_skip) {
            {
                std::unique_lock lock(command_mutex);
                command_queue.push(QueueStreamableCommand{
                    .streamable=radio_next,
                    .fetch_album=false,
                    .start_radio=false,
                    .start_fresh=false,
                    .queue_in_radio=false,
                });
            }
            command_cv.notify_one();
        }

        /* If should skip:
        - We don't actually call the skip method because mpv will autoplay by itself
        */
        if(should_skip || to_radio_skip) {
            this->updateMprisControls();
            this->updateMprisData();
            this->updateSocialData();

        } else {
            this->resetMprisData();
        }
    });

    mpv_service->setOnStreamStart([this] {
        spdlog::info("PLAYER: Stream start");
        uint64_t song_duration = mpv_service->getStreamDuration();
        {
            std::lock_guard lock(state_mutex);
            state.current->setDuration(song_duration);
            state.is_streaming_audio = true;
        }

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Playing);
        this->updateMprisControls();
        this->updateMprisData();
        this->updateSocialData();
    });
}

void services::Player::worker_loop() {
    while (true) {
        std::unique_lock lock(command_mutex);

        command_cv.wait(lock, [this] {
            return !command_queue.empty() || !running;
        });

        if (!running && command_queue.empty()) {
            break;
        }

        Command cmd = std::move(command_queue.front());
        command_queue.pop();

        lock.unlock();

        std::visit([this](auto&& c) {
            using T = std::decay_t<decltype(c)>;

            if constexpr (std::is_same_v<T, SearchCommand>) {
                spdlog::info("PLAYER: SearchCommand");

                {
                    std::lock_guard lock(state_mutex);
                    state.is_loading_search = true;
                    state.search_results.clear();
                }

                std::vector<music::SearchResult> res = music_service->getSearch(c.query);

                {
                    std::lock_guard lock(state_mutex);
                    state.is_loading_search = false;
                    state.search_results = res;
                }
            }

            else if constexpr (std::is_same_v<T, QueueStreamableCommand>) {
                spdlog::info("PLAYER: QueueStreamableCommand");

                if (!c.streamable) {
                    spdlog::warn("PLAYER: QueueStreamableCommand, streamable is nullptr");
                    return;
                }

                bool should_queue = false;
                bool is_queue_empty = false;

                {
                    std::lock_guard lock(state_mutex);
                    should_queue = state.is_streaming_audio;
                    is_queue_empty = state.user_queue.empty() && state.radio_queue.empty();
                }

                std::shared_ptr<music::IStreamable> streamable;

                if (auto song_ptr = std::dynamic_pointer_cast<music::Song>(c.streamable)) {
                    music::Song song;

                    if (c.fetch_album) {
                        spdlog::info("PLAYER: QueueStreamableCommand, fetching album data");
                        song = music_service->getSong(song_ptr->ref);

                    } else if(!song_ptr->album.id.empty() && !song_ptr->album.title.empty()) {
                        spdlog::info("PLAYER: QueueStreamableCommand, album already provided");
                        song = *song_ptr;

                    } else {
                        spdlog::info("PLAYER: QueueStreamableCommand, skipping album data");
                        song.setRef(song_ptr->ref);
                    }

                    streamable = std::make_shared<music::Song>(song);

                } else if (auto episode_ptr = std::dynamic_pointer_cast<music::Episode>(c.streamable)) {
                    music::Episode episode = music_service->getEpisode(episode_ptr->ref);
                    streamable = std::make_shared<music::Episode>(episode);

                } else {
                    spdlog::warn("PLAYER: QueueStreamableCommand, unknown streamable type");
                    return;
                }

                if(c.start_fresh) {
                    mpv_service->clearQueue();
                    this->resetMprisData();

                    {
                        std::lock_guard lock(state_mutex);
                        state.user_queue.clear();
                        state.radio_queue.clear();
                        state.queue_position = 0;
                        state.current = streamable;
                    }
                }

                if(c.start_radio) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_queue.push(QueueStreamableRadioCommand{streamable});
                    }
                    command_cv.notify_one();
                }

                {
                    std::lock_guard lock(state_mutex);

                    if(c.queue_in_radio) {
                        state.radio_queue.push_back(streamable);

                    } else {
                        state.user_queue.push_back(streamable);
                    }

                    if(!should_queue && !c.queue_in_radio) {
                        state.queue_position = state.user_queue.size() - 1;
                        state.is_streaming_audio = true;
                        state.current = state.user_queue.back();
                    }
                }

                if(!c.queue_in_radio) {
                    mpv_service->load(streamable->getStreamUrl());
                }

                if(should_queue) {
                    this->updateMprisControls();
                }
            }

            else if constexpr (std::is_same_v<T, QueueStreamableContainerCommand>) {
                spdlog::info("PLAYER: QueueStreamableContainerCommand");

                if (!c.container) {
                    spdlog::warn("PLAYER: QueueStreamableContainerCommand, container is nullptr");
                    return;
                }

                std::shared_ptr<music::IStreamableContainer> container;

                if (auto album_ptr = std::dynamic_pointer_cast<music::Album>(c.container)) {
                    music::Album album = music_service->getAlbum(album_ptr->ref);
                    container = std::make_shared<music::Album>(album);

                } else if (auto playlist_ptr = std::dynamic_pointer_cast<music::Playlist>(c.container)) {
                    music::Playlist playlist = music_service->getPlaylist(playlist_ptr->ref);
                    container = std::make_shared<music::Playlist>(playlist);

                } else {
                    spdlog::warn("PLAYER: QueueStreamableContainerCommand, unknown streamable type");
                    return;
                }

                for(auto streamable : container->getStreamables()) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_queue.push(QueueStreamableCommand{
                            .streamable=streamable,
                            .fetch_album=false,
                            .start_radio=false,
                            .start_fresh=c.start_fresh,
                            .queue_in_radio=false
                        });
                    }

                    if(c.start_fresh) {
                        c.start_fresh = false;
                    }
                }

                if(c.start_radio) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_queue.push(QueueStreamableContainerRadioCommand{container});
                    }
                    command_cv.notify_one();
                }
            }

            else if constexpr (std::is_same_v<T, QueueStreamableRadioCommand>) {
                spdlog::info("PLAYER: QueueStreamableRadioCommand");

                music::Radio radio;

                if (auto song_ptr = std::dynamic_pointer_cast<music::Song>(c.streamable)) {
                    radio = music_service->getRadio(song_ptr->ref);

                } else if (auto episode_ptr = std::dynamic_pointer_cast<music::Episode>(c.streamable)) {
                    radio = music_service->getRadio(episode_ptr->ref);

                } else {
                    spdlog::warn("PLAYER: QueueStreamableRadioCommand, unknown streamable type");
                    return;
                }

                {
                    std::lock_guard lock(state_mutex);
                    this->state.radio = radio;
                    this->state.radio_queue.clear();
                }

                std::shared_ptr<music::IStreamableContainer> container = std::make_shared<music::Radio>(radio);

                for(auto streamable : container->getStreamables()) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_queue.push(QueueStreamableCommand{
                            .streamable=streamable,
                            .fetch_album=false,
                            .start_radio=false,
                            .start_fresh=false,
                            .queue_in_radio=true
                        });
                    }
                }
                command_cv.notify_one();
            }

            else if constexpr (std::is_same_v<T, QueueStreamableContainerRadioCommand>) {
                spdlog::info("PLAYER: QueueStreamableContainerRadioCommand");
                music::Radio radio;

                if (auto playlist_ptr = std::dynamic_pointer_cast<music::Playlist>(c.container)) {
                    radio = music_service->getRadio(playlist_ptr->ref);

                } else if (auto album_ptr = std::dynamic_pointer_cast<music::Album>(c.container)){
                    radio = music_service->getRadio(album_ptr->ref);

                } else {
                    spdlog::warn("PLAYER: QueueStreamableRadioCommand, unsupported streamable type");
                    return;
                }

                {
                    std::lock_guard lock(state_mutex);
                    this->state.radio = radio;
                    this->state.radio_queue.clear();
                }

                std::shared_ptr<music::IStreamableContainer> container = std::make_shared<music::Radio>(radio);

                for(auto streamable : container->getStreamables()) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_queue.push(QueueStreamableCommand{
                            .streamable=streamable,
                            .fetch_album=false,
                            .start_radio=false,
                            .start_fresh=false,
                            .queue_in_radio=true
                        });
                    }
                }
                command_cv.notify_one();

            } else if constexpr (std::is_same_v<T, QueueNextRadioCommand>) {
                spdlog::info("PLAYER: QueueNextRadioCommand");

                music::Radio state_radio;

                {
                    std::lock_guard lock(state_mutex);
                    state_radio = this->state.radio;
                }

                if(state_radio.continuation.empty()) {
                    spdlog::warn("PLAYER: QueueNextRadioCommand, no continuation");
                    return;
                }

                music::Radio new_radio = music_service->getRadioNext(state_radio);

                {
                    std::lock_guard lock(state_mutex);
                    this->state.radio = new_radio;
                }

                std::shared_ptr<music::IStreamableContainer> container = std::make_shared<music::Radio>(new_radio);

                for(auto streamable : container->getStreamables()) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_queue.push(QueueStreamableCommand{
                            .streamable=streamable,
                            .fetch_album=false,
                            .start_radio=false,
                            .start_fresh=false,
                            .queue_in_radio=true
                        });
                    }
                }
                command_cv.notify_one();
            }


        }, cmd);

        notifyRequestCompleted();
    }
}

services::Player::~Player() {
    {
        std::lock_guard lock(command_mutex);
        running = false;
    }

    command_cv.notify_all();

    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

void services::Player::search(const std::string& query) {
    {
        std::lock_guard lock(command_mutex);
        command_queue.push(SearchCommand{query});
    }

    command_cv.notify_one();
}

void services::Player::queue(std::shared_ptr<music::IStreamable> streamable, const bool fresh, const bool radio) {
    Config cfg = Config::get();

    {
        std::lock_guard lock(command_mutex);
        command_queue.push(QueueStreamableCommand{
            .streamable=streamable,
            .fetch_album=cfg.FETCH_ALBUMS,
            .start_radio=radio,
            .start_fresh=fresh,
            .queue_in_radio=false
        });
    }

    command_cv.notify_one();
}

void services::Player::queue(std::shared_ptr<music::IStreamableContainer> container, const bool fresh, const bool radio) {
    Config cfg = Config::get();

    {
        std::lock_guard lock(command_mutex);
        command_queue.push(QueueStreamableContainerCommand{
            .container=container,
            .fetch_albums=false,
            .start_fresh=fresh,
            .start_radio=radio,
            .queue_in_radio=false
        });
    }

    command_cv.notify_one();
}

void services::Player::skipForward() {
    bool to_radio_skip = false;
    bool should_skip = true;

    std::shared_ptr<music::IStreamable> radio_next;

    {
        std::lock_guard lock(state_mutex);
        if (state.queue_position + 1 >= state.user_queue.size() && state.radio_queue.empty()) {
            state.is_streaming_audio = false;
            should_skip = false;

            spdlog::warn("PLAYER: tried to skip to next song, but queue is done");

        } else if (state.queue_position < state.user_queue.size() && state.radio_queue.empty()){
            state.queue_position++;
            state.current = state.user_queue[state.queue_position];

        } else if (state.queue_position + 1 >= state.user_queue.size() && !state.radio_queue.empty()) {
            state.queue_position++;
            radio_next = state.radio_queue.front();

            state.current = radio_next;
            state.user_queue.push_back(radio_next);
            state.radio_queue.pop_front();

            to_radio_skip = true;

            spdlog::info("PLAYER: skipping to radio");

        } else if (state.queue_position < state.user_queue.size() && !state.radio_queue.empty()){
            state.queue_position++;
            state.current = state.user_queue[state.queue_position];
        }
    }

    if(to_radio_skip) {
        spdlog::info("PLAYER: loading next radio song");
        mpv_service->load(radio_next->getStreamUrl());
    }

    if(should_skip) {
        spdlog::info("PLAYER: skipping to next song");

        mpv_service->skipForward();

        /* skipping
        - We keep these even if onSongStart handles skipping
        - because they don't wait for file load.
        */
        mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);
        this->updateMprisControls();
        this->updateMprisData();
        this->updateSocialData();
    }
}

void services::Player::skipBackward() {
    bool should_skip = true;

    {
        std::lock_guard lock(state_mutex);

        if (state.queue_position == 0 || state.user_queue.empty()) {
            state.is_streaming_audio = false;
            should_skip = false;

            spdlog::warn("PLAYER: tried to skip to previous song, but already at start of queue.");

        } else {
            state.queue_position--;
            state.current = state.user_queue[state.queue_position];
        }
    }

    if(should_skip) {
        spdlog::info("PLAYER: skipping to previous song");

        mpv_service->skipBackward();

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);
        this->updateMprisControls();
        this->updateMprisData();
        this->updateSocialData();
    }
}

void services::Player::updateMprisControls() {
    {
        std::lock_guard lock(state_mutex);
        mpris_service->setIsNextPossible((state.queue_position + 1 < state.user_queue.size()) || (!state.radio_queue.empty()));
        mpris_service->setIsPreviousPossible(state.queue_position > 0 && state.user_queue.size() > 1);
        mpris_service->updatePlayerControls();
    }
}

void services::Player::updateMprisData() {
    std::shared_ptr<music::IStreamable> current;

    {
        std::lock_guard lock(state_mutex);
        current = state.current;
    }

    if (!current) {
        return;
    }

    std::string hash_id = std::to_string(std::hash<std::string>{}(current->getStreamUrl()));
    spdlog::info("PLAYER: hashed id, " + hash_id);

    if (auto song = std::dynamic_pointer_cast<music::Song>(current)) {
        mpris_service->setMetadata({
            { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/track/" + hash_id) },
            { services::Field::Album,   sdbus::Variant(song->album.title) },
            { services::Field::Title,   sdbus::Variant(song->ref.title) },
            { services::Field::Artist,  sdbus::Variant(song->ref.artists.empty() ? "" : song->ref.artists[0].name) },
            { services::Field::Length,  sdbus::Variant(current->getDuration()) },
            { services::Field::ArtUrl,  sdbus::Variant(song->ref.thumbnail_large) }
        });
    }
    else if (auto episode = std::dynamic_pointer_cast<music::Episode>(current)) {
        mpris_service->setMetadata({
            { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/track/" + hash_id) },
            { services::Field::Title,   sdbus::Variant(episode->ref.title) },
            { services::Field::Artist,  sdbus::Variant(episode->ref.podcast.name) },
            { services::Field::Length,  sdbus::Variant(current->getDuration()) },
            { services::Field::ArtUrl,  sdbus::Variant(episode->ref.thumbnail_large) }
        });
    }

    mpris_service->setPosition(0);
    mpris_service->sendSeekedSignal(0);
}

void services::Player::resetMprisData() {
    mpris_service->setMetadata({});

    mpris_service->setPosition(0);
    mpris_service->sendSeekedSignal(0);
}

void services::Player::updateSocialData() {
    std::shared_ptr<music::IStreamable> current;

    {
        std::lock_guard lock(state_mutex);
        current = state.current;
    }

    if (!current) {
        return;
    }

    if (auto song = std::dynamic_pointer_cast<music::Song>(current)) {
        social_service->setStatus(
            song->ref.title,
            song->ref.artists.empty() ? "" : song->ref.artists[0].name,
            song->album.title,
            song->ref.thumbnail_large,
            current->getDuration()
        );
    }
    else if (auto episode = std::dynamic_pointer_cast<music::Episode>(current)) {
        social_service->setStatus(
            episode->ref.title,
            episode->ref.podcast.name,
            episode->ref.podcast.name,
            episode->ref.thumbnail_large,
            current->getDuration()
        );
    }
}

void services::Player::removeFromUserQueue(uint16_t index) {
    bool should_skip = false;

    {
        std::lock_guard lock(state_mutex);
        if (index < state.user_queue.size()) {
            state.user_queue.erase(state.user_queue.begin() + index);

        } else {
            spdlog::warn("PLAYER: tried to remove song at {} but index is invalid ({}).", index, state.user_queue.size());
            return;
        }

        if(index < state.queue_position) {
            state.queue_position -= 1;
        }

        if(index == state.queue_position) {
            should_skip = true;
        }
    }

    if(should_skip) {
        this->skipForward();
    }

    mpv_service->removeAt(index);
    this->updateMprisControls();
}

void services::Player::removeFromRadioQueue(uint16_t index) {
    {
        std::lock_guard lock(state_mutex);

        if(index < state.radio_queue.size()) {
            state.radio_queue.erase(state.radio_queue.begin() + index);
        }
    }

    this->updateMprisControls();
}
