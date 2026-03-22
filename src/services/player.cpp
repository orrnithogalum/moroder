#include "../../include/services/player.hpp"

#include "../../include/config/config.hpp"

#include <spdlog/spdlog.h>
#include <string>
#include <mutex>

services::Player::Player(const std::string_view& app_name, const std::string_view& app_name_human, const uint64_t app_id) {
    this->app_name = app_name;

    mpris_service = Mpris::make(this->app_name);
    mpv_service = std::make_unique<MPV>();
    music_service = std::make_unique<Music>(this->app_name);
    social_service = std::make_unique<Social>(app_id);

    if (!mpris_service) {
        spdlog::error("Mpris service initialisation failed.");
    } else if (!music_service) {
        spdlog::error("Music service initialisation failed.");
    } else if (!social_service) {
        spdlog::error("Social service initialisation failed.");
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
        if(p < 0) {
            mpv_service->seekBackward(p);
        } else {
            mpv_service->seekForward(p);
        }

        social_service->setPosition(mpv_service->getStreamPosition());
        mpris_service->setPosition(mpv_service->getStreamPosition());
    });

    mpris_service->onSetPosition([&] (int64_t p) {
        mpv_service->setPosition(p);

        social_service->setPosition(mpv_service->getStreamPosition());
        mpris_service->setPosition(mpv_service->getStreamPosition());
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
        bool should_skip = false;
        {
            std::lock_guard lock(state_mutex);

            state.is_streaming_audio = false;

            /* Since we use mpv playlist feature for buffering, we need to increment queue position no matter what.
            - This means that queue position can be equal or greater than the queue size
            - So we check for that.
            */
            this->state.queue_position++;

            if (state.queue_position < state.song_queue.size()) {
                state.current_song = state.song_queue[state.queue_position];
                should_skip = true;
            }
        }

        /* If should skip:
        - We don't actually call the skip method because mpv will autoplay by itself
        */
        if(should_skip) {
            mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);
            this->updateMprisControls();
            this->updateMprisData();
            this->updateSocialData();
        }
    });

    mpv_service->setOnStreamStart([this] {
        {
            std::lock_guard lock(state_mutex);
            state.current_song.duration = mpv_service->getStreamDuration();
        }

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Playing);
        this->updateMprisControls();
        this->updateMprisData();
        this->updateSocialData();
    });
}

void services::Player::worker_loop() {
    while (true) {

        Command cmd;

        {
            std::unique_lock lock(command_mutex);

            command_cv.wait(lock, [this] {
                return !command_queue.empty() || !running;
            });

            if (!running && command_queue.empty()) {
                return;
            }

            cmd = command_queue.front();
            command_queue.pop();
        }

        switch (cmd.type) {

        case Command::Empty: {
            break;
        }

        case Command::Search: {
            ipc::SearchResponse response = music_service->search(cmd.query);

            {
                std::lock_guard lock(state_mutex);
                state.search_results = response.results;
                state.is_loading_search = false;
            }

            break;
        }

        case Command::Stream: {
            music::Song song;

            // If the album isn't found, fetch one from network.
            Config cfg = Config::get();
            if ((cmd.song_ref.album.id.empty() || cmd.song_ref.album.title.empty()) && cfg.FETCH_ALBUMS) {
                song = music_service->getSong(cmd.song_ref).song;

            } else {
                song.ref = cmd.song_ref;
                song.album_title = song.ref.album.title;
            }

            song.url = "https://www.youtube.com/watch?v=" + song.ref.id;
            song.duration = 0;

            {
                std::lock_guard lock(state_mutex);

                state.song_queue.push_back(song);
                state.queue_position = state.song_queue.size() - 1;

                state.is_streaming_audio = true;
                state.current_song = song;

                state.song_position = 0;
            }

            mpv_service->load(song.url);

            break;
        }

        case Command::Radio: {
            ipc::RadioResponse response = music_service->radio(cmd.song_ref);

            bool should_stream = false;

            {
                std::lock_guard lock(state_mutex);

                if (!state.is_streaming_audio) {
                    should_stream = true;
                }
            }

            for (const music::SongRef& ref : response.results) {
                {
                    std::lock_guard lock(command_mutex);
                    command_queue.push(Command(ref, should_stream ? Command::Stream : Command::Queue));
                }

                if(should_stream) {
                    should_stream = false;
                }
            }

            command_cv.notify_one();

            break;
        }

        case Command::Queue: {
            music::Song song;

            // If the album isn't found, fetch one from network.
            Config cfg = Config::get();
            if ((cmd.song_ref.album.id.empty() || cmd.song_ref.album.title.empty()) && cfg.FETCH_ALBUMS) {
                song = music_service->getSong(cmd.song_ref).song;

            } else {
                song.ref = cmd.song_ref;
                song.album_title = song.ref.album.title;
            }

            song.url = "https://www.youtube.com/watch?v=" + song.ref.id;
            song.duration = 0;

            {
                std::lock_guard lock(state_mutex);
                state.song_queue.push_back(song);
            }

            mpv_service->load(song.url);
            this->updateMprisControls();

            break;
        }

        }

        notifyRequestCompleted(cmd.type);
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
        state.is_loading_search = true;
        state.search_results.clear();

        command_queue.push(Command(query));
    }

    command_cv.notify_one();
}

void services::Player::radio(const music::SongRef& song) {
    {
        std::lock_guard lock(command_mutex);
        command_queue.push(Command(song, Command::Radio));
    }

    command_cv.notify_one();
}

void services::Player::stream(const music::SongRef& song) {
    {
        std::lock_guard lock(command_mutex);
        command_queue.push(Command(song, Command::Stream));
    }

    command_cv.notify_one();
}

void services::Player::queue(const music::SongRef& song) {
    bool should_stream = false;

    {
        std::lock_guard lock(state_mutex);

        if (!state.is_streaming_audio) {
            should_stream = true;
        }
    }

    if (should_stream) {
        stream(song);
        spdlog::info("Stream song: " + song.title + " by " + song.artists[0].name);

    } else {
        {
            std::lock_guard lock(command_mutex);
            command_queue.push(Command(song, Command::Queue));
        }

        command_cv.notify_one();
        spdlog::info("Queued song: " + song.title + " by " + song.artists[0].name);
    }
}

void services::Player::skipForward() {
    music::Song next_song;
    bool should_skip = true;

    {
        std::lock_guard lock(state_mutex);
        if (state.queue_position + 1 >= state.song_queue.size()) {
            state.is_streaming_audio = false;

            should_skip = false;

            spdlog::warn("Tried to skip to next song, but queue is done");

        } else {
            state.is_streaming_audio = true;

            state.queue_position++;
            state.current_song = state.song_queue[state.queue_position];
        }

    }

    if(should_skip) {
        spdlog::info("Skipping to next song");

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
    music::Song previous_song;
    bool should_skip = true;

    {
        std::lock_guard lock(state_mutex);

        if (state.queue_position == 0 || state.song_queue.empty()) {
            state.is_streaming_audio = false;
            should_skip = false;

            spdlog::warn("Tried to skip to previous song, but already at start of queue.");

        } else {
            state.queue_position--;
            previous_song = state.song_queue[state.queue_position];
            state.current_song = previous_song;
        }
    }

    if(should_skip) {
        spdlog::info("Skipping to previous song");

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);
        mpv_service->skipBackward();
        this->updateMprisControls();
        this->updateMprisData();
        this->updateSocialData();
    }
}

void services::Player::updateMprisControls() {
    {
        std::lock_guard lock(state_mutex);
        mpris_service->setIsNextPossible(state.queue_position + 1 < state.song_queue.size());
        mpris_service->setIsPreviousPossible(state.queue_position > 0 && state.song_queue.size() > 1);
        mpris_service->updatePlayerControls();
    }
}

void services::Player::updateMprisData() {
    music::Song video;

    {
        std::lock_guard lock(state_mutex);
        video = state.current_song;
    }

    mpris_service->setMetadata({
        { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/" + this->app_name + "/track/" + video.ref.id) },
        { services::Field::Album,   sdbus::Variant(video.album_title) },
        { services::Field::Title,   sdbus::Variant(video.ref.title) },
        { services::Field::Artist,  sdbus::Variant(video.ref.artists[0].name) },
        { services::Field::Length,  sdbus::Variant(video.duration) },
        { services::Field::ArtUrl,  sdbus::Variant(video.ref.thumbnail) }
    });
    mpris_service->setPosition(0);
    mpris_service->sendSeekedSignal(0);
}

void services::Player::updateSocialData() {
    music::Song video;

    {
        std::lock_guard lock(state_mutex);
        video = state.current_song;
    }

    social_service->setStatus(
        video.ref.title,
        video.ref.artists[0].name,
        video.album_title,
        video.ref.thumbnail,
        video.duration
    );
}
