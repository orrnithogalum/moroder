/* PLAYER
- Service that wraps: Music, Mpris and Social into one service
- Adds queuing support
*/

#pragma once

#include "mpv.hpp"
#include "mpris.hpp"
#include "music.hpp"
#include "social.hpp"

#include <queue>

namespace services {

class Player {
public:
    /* PlayerState
    - Shared player state between all threads
    */
    struct PlayerState {
        std::deque<music::Song> song_queue;

        std::vector<ipc::SearchResult> search_results;
        music::Song current_song;

        int64_t song_position = 0;
        int queue_position = 0;

        bool is_loading_search = false;
        bool is_streaming_audio = false;
    };

    /* Command
    - Player commands to queue
    - A threads loops over queued commands and executes them
    */
    struct Command {
        enum Type {
            Empty,
            Queue,
            Radio,
            Search,
            Stream,
        };

        Type type;

        /* We could use std::variant here, but I'm too lazy.
        - Problem for future me.
        */
        std::string query;
        music::SongRef song_ref;

        explicit Command() : type(Empty) {}
        explicit Command(Type t) : type(t) {}
        explicit Command(const std::string& q) : query(q), type(Search) {}
        explicit Command(const music::SongRef& s, Type t) : song_ref(s), type(t) {}

    };

    PlayerState state;
    std::mutex state_mutex;

    Player(const std::string_view& app_name, const std::string_view& app_name_human, const uint64_t app_id);
    ~Player();

    /* Player controls
    - Basic playback operations
    */
    void skipForward();
    void skipBackward();

    void search(const std::string& query);
    void queue(const music::SongRef& song);
    void radio(const music::SongRef& song);
    void stream(const music::SongRef& song);

    using RequestCompletedCallback = std::function<void(Command::Type)>;

    void setOnRequestCompletedCallback(RequestCompletedCallback cb) {
        std::lock_guard lock(callback_mutex);
        on_request_completed = std::move(cb);
    }

private:
    std::string app_name;

    std::condition_variable command_cv;
    std::mutex command_mutex;
    std::thread worker_thread;

    bool running = true;

    std::queue<Command> command_queue;

    std::unique_ptr<MPV> mpv_service;
    std::unique_ptr<Mpris> mpris_service;
    std::unique_ptr<Music> music_service;
    std::unique_ptr<Social> social_service;

    RequestCompletedCallback on_request_completed;
    std::mutex callback_mutex;

    void notifyRequestCompleted(Command::Type type) {
        std::lock_guard lock(callback_mutex);
        if (on_request_completed) {
            on_request_completed(type);
        }
    }

    void worker_loop();

    /* updateMprisControls
    - Updates the mpris buttons (activated / deactivated) based on queue position.
    */
    void updateMprisControls();

    /* update...Data
    - Updates the album / thumbnail / other song data to external services.
    */
    void updateMprisData();
    void updateSocialData();
};

}
