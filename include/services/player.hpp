/* PLAYER
- Service that wraps: Music, Mpris and Social into one service
- Adds queuing support
- Makes sure all music_service commands are synchronous
- Give a shared pointer on a struct ref and it will queue the thing for you
*/

#pragma once

#include "mpv.hpp"
#include "mpris.hpp"
#include "music.hpp"
#include "social.hpp"

#include <queue>

namespace services {

class Player {
private:
    struct SearchCommand {
        std::string query;
    };

    struct QueueStreamableRadioCommand {
        std::shared_ptr<music::IStreamable> streamable;
    };

    struct QueueStreamableCommand {
        std::shared_ptr<music::IStreamable> streamable;
        bool fetch_album = true;

        bool start_radio = true;
        bool queue_in_radio = false;
    };

    struct QueueStreamableContainerRadioCommand {
        std::shared_ptr<music::IStreamableContainer> container;
    };

    struct QueueStreamableContainerCommand {
        std::shared_ptr<music::IStreamableContainer> container;

        bool fetch_albums = false;
        bool start_radio  = true;
    };

    struct QueueNextRadioCommand {
        music::Radio radio;
    };

public:
    /* PlayerState
    - Shared player state between all threads
    */
    struct PlayerState {
        int queue_position;

        std::deque<std::shared_ptr<music::IStreamable>> user_queue;
        std::deque<std::shared_ptr<music::IStreamable>> radio_queue;

        std::shared_ptr<music::IStreamable> current;
        std::vector<music::SearchResult> search_results;

        bool autoplay = true;
        bool is_loading_search = false;
        bool is_streaming_audio = false;

        music::Radio radio;
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

    void queue(std::shared_ptr<music::IStreamable> streamable);
    void queue(std::shared_ptr<music::IStreamableContainer> container);

    using RequestCompletedCallback = std::function<void()>;

    void setOnRequestCompletedCallback(RequestCompletedCallback cb) {
        on_request_completed = std::move(cb);
    }

private:
    std::string app_name;

    std::condition_variable command_cv;
    std::mutex command_mutex;

    using Command = std::variant<
        SearchCommand,
        QueueStreamableRadioCommand,
        QueueStreamableCommand,

        QueueStreamableContainerRadioCommand,
        QueueStreamableContainerCommand,

        QueueNextRadioCommand
    >;

    std::queue<Command> command_queue;

    std::unique_ptr<MPV> mpv_service;
    std::unique_ptr<Mpris> mpris_service;
    std::unique_ptr<Music> music_service;
    std::unique_ptr<Social> social_service;

    std::thread worker_thread;

    RequestCompletedCallback on_request_completed;

    bool running = true;

    void notifyRequestCompleted() {
        if (on_request_completed) {
            on_request_completed();
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

    void resetMprisData();
};

}
