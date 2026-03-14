#pragma once

#include "mpris.hpp"
#include "music.hpp"
#include "social.hpp"

#include <queue>

namespace services {

class Player {
public:
    struct PlayerState {
        std::vector<ipc::SearchResult> search_results;
        bool loading_search = false;
    };

    PlayerState state;
    std::mutex state_mutex;

    Player(const std::string_view& app_name, const uint64_t app_id);
    ~Player();

    using RequestCompletedCallback = std::function<void()>;

    void setOnRequestCompletedCallback(RequestCompletedCallback cb) {
        std::lock_guard lock(callback_mutex);
        on_request_completed = std::move(cb);
    }

    void search(const std::string& query);

private:
    std::condition_variable command_cv;
    std::mutex command_mutex;
    std::thread worker_thread;
    
    bool running = true;

    struct Command {
        enum Type {
            Search
        };

        Type type;
        std::string query;
    };

    std::queue<Command> command_queue;

    std::unique_ptr<Mpris> mpris_service;
    std::unique_ptr<Music> music_service;
    std::unique_ptr<Social> social_service;

    RequestCompletedCallback on_request_completed;
    std::mutex callback_mutex;
    
    int64_t song_position = 0;

    int queue_position = 0;

    void notifyRequestCompleted() {
        std::lock_guard lock(callback_mutex);
        if (on_request_completed) {
            on_request_completed();
        }
    }

    void worker_loop();
};

}