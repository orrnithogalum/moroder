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
        music::Song current_song;

        int64_t song_position = 0;
        int queue_position = 0;

        bool is_loading_search = false;
        bool is_streaming_audio = false;
        bool is_loading_song = false;
    };

    struct Command {
        enum Type {
            Empty,
            Search,
            Stream,
        };

        Type type;
        std::string query;
        music::SongRef song;

        explicit Command() : type(Empty) {}
        explicit Command(const std::string& q) : type(Search), query(q) {}
        explicit Command(const music::SongRef& s) : type(Stream), song(s) {}
    };

    PlayerState state;
    std::mutex state_mutex;

    Player(const std::string_view& app_name, const std::string_view& app_name_human, const uint64_t app_id);
    ~Player();

    using RequestCompletedCallback = std::function<void(Command::Type)>;

    void setOnRequestCompletedCallback(RequestCompletedCallback cb) {
        std::lock_guard lock(callback_mutex);
        on_request_completed = std::move(cb);
    }

    void search(const std::string& query);
    void stream(const music::SongRef& song);

private:
    std::condition_variable command_cv;
    std::mutex command_mutex;
    std::thread worker_thread;
    
    bool running = true;

    std::queue<Command> command_queue;

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
};

}