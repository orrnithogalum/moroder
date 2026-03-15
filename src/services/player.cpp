#include "../../include/services/player.hpp"

#include <mutex>
#include <spdlog/spdlog.h>

services::Player::Player(const std::string_view& app_name, const std::string_view& app_name_human, const uint64_t app_id) {
    mpris_service = Mpris::make(app_name);
    music_service = std::make_unique<Music>(app_name);
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
        state.is_streaming_audio = false;
        
        ipc::ControlResponse control_response = music_service->pause();
        social_service->pause();
        mpris_service->setPosition(static_cast<uint64_t>(control_response.position));

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Paused);
    });

    mpris_service->onToggle([&] {
        state.is_streaming_audio = !state.is_streaming_audio;

        if(state.is_streaming_audio) {
            music_service->pause();
            social_service->pause();
        } else {
            music_service->resume();
            social_service->resume();
        }

        mpris_service->setPlaybackStatus(state.is_streaming_audio ? services::PlaybackStatus::Playing : services::PlaybackStatus::Paused);
    });
    
    mpris_service->onStop([&] {
        state.is_streaming_audio = false;
        music_service->stop();
        social_service->removeStatus();

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Stopped);
    });
    
    mpris_service->onPlay([&] {
        state.is_streaming_audio = true;
        music_service->resume();
        social_service->resume();
        social_service->setPosition(state.song_position);

        mpris_service->setPlaybackStatus(services::PlaybackStatus::Playing);
    });
    
    mpris_service->onSeek([&] (int64_t p) {
        state.song_position += p;
        
        if(p < 0) {
            music_service->backward(p);
        } else {
            music_service->forward(p);
        }

        social_service->setPosition(state.song_position);  
        mpris_service->setPosition(state.song_position);
    });

    mpris_service->onSetPosition([&] (int64_t p) {
        state.song_position = p;

        music_service->setPosition(state.song_position);
        social_service->setPosition(state.song_position);
        mpris_service->setPosition(state.song_position);
    });

    mpris_service->onLoopStatusChanged([&] (services::LoopStatus status) { });
    mpris_service->onShuffleChanged([&] (bool shuffle) { });

    mpris_service->onNext([this](){
        skipSongForward();
    });

    mpris_service->onPrevious([this](){
        skipSongBackward();
    });

    mpris_service->setIsNextPossible(false);
    mpris_service->setIsPreviousPossible(false);

    mpris_service->startLoopAsync();
    
    music_service->setOnStreamDone([this] {
        {
            std::lock_guard lock(state_mutex);
            this->state.is_loading_song = false;
            this->state.is_streaming_audio = false;
        }
        skipSongForward();
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
            music::Song video = music_service->getSong(cmd.song).song;
            ipc::StreamResponse response = music_service->stream(cmd.song);

            video.duration = response.duration;

            {
                std::lock_guard lock(state_mutex);
                state.is_streaming_audio = true;
                state.is_loading_song = false;
                state.current_song = video;
                state.song_position = 0;
            }

            this->updateMprisControls();

            mpris_service->setMetadata({
                { services::Field::TrackId, sdbus::Variant(services::OBJECT_PATH + "/track/" + video.ref.id) },
                { services::Field::Album,   sdbus::Variant(video.album_title) },
                { services::Field::Title,   sdbus::Variant(video.ref.title) },
                { services::Field::Artist,  sdbus::Variant(video.ref.artists[0].name) },
                { services::Field::Length,  sdbus::Variant(video.duration) },
                { services::Field::ArtUrl,  sdbus::Variant(video.ref.thumbnail) }
            });
            mpris_service->setPlaybackStatus(services::PlaybackStatus::Playing);
            mpris_service->setPosition(0);
            mpris_service->sendSeekedSignal(0);

            social_service->setStatus(
                video.ref.title,
                video.ref.artists[0].name,
                video.album_title,
                video.ref.thumbnail,
                video.duration
            );
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

void services::Player::stream(const music::SongRef& song) {
    {
        std::lock_guard lock(state_mutex);
        state.is_loading_song = true;
    }

    {
        std::lock_guard lock(command_mutex);
        command_queue.push(Command(song));
    }

    command_cv.notify_one();
}

void services::Player::queueSong(const music::SongRef& song) {
    bool should_stream = false;

    {
        std::lock_guard lock(state_mutex);

        state.song_queue.push_back(song);

        if (!state.is_streaming_audio && !state.is_loading_song) {
            state.queue_position = state.song_queue.size() - 1;
            should_stream = true;
        }
    }

    updateMprisControls();

    if (should_stream)
        stream(song);

    spdlog::info("Queued song: " + song.title + " by " + song.artists[0].name);
}

void services::Player::skipSongForward() {
    music::SongRef next_song;
    bool should_skip = true;

    {
        std::lock_guard lock(state_mutex);
        if (state.queue_position + 1 >= state.song_queue.size()) {
            state.is_streaming_audio = false;
            should_skip = false;
            
            spdlog::warn("Tried to skip to next song, but queue is done.");
                        
        } else {
            state.queue_position++;
            next_song = state.song_queue[state.queue_position];
        }

    }

    if(should_skip) {
        stream(next_song);
    }
}

void services::Player::skipSongBackward() {
    music::SongRef previous_song;

    {
        std::lock_guard lock(state_mutex);

        if (state.queue_position == 0 || state.song_queue.empty()) {
            spdlog::warn("Tried to skip to previous song, but already at start of queue.");
            return;
        }

        state.queue_position--;
        previous_song = state.song_queue[state.queue_position];
    }

    stream(previous_song);
}

void services::Player::updateMprisControls() {
    {
        std::lock_guard lock(state_mutex);  
        mpris_service->setIsNextPossible(state.queue_position + 1 < state.song_queue.size());
        mpris_service->setIsPreviousPossible(state.queue_position > 0 && state.song_queue.size() > 1);
        mpris_service->updatePlayerControls();
    }
}