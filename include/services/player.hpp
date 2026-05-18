/* PLAYER
- Service that wraps: Music, Mpris and Social into one service
- Adds queuing support
- Makes sure all music_service commands are synchronous
- Give a shared pointer on a struct ref and it will queue the thing for you
*/

#pragma once

#include "social.hpp"
#include "mpris.hpp"
#include "music.hpp"
#include "mpv.hpp"

#include <cstdint>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

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
        bool start_fresh = false;
        bool queue_in_radio = false;
    };

    struct QueueStreamableContainerRadioCommand {
        std::shared_ptr<music::IStreamableContainer> container;
    };

    struct QueueStreamableContainerCommand {
        std::shared_ptr<music::IStreamableContainer> container;

        bool fetch_albums = false;
        bool start_fresh  = false;
        bool start_radio  = true;
        bool queue_in_radio = false;
    };

    struct QueueNextRadioCommand {
        music::Radio radio;

        bool auto_play = false;
    };

    struct HomeCommand {};
    struct LibraryPlaylistsCommand {};
    struct IsLoggedInCommand {};

public:
    /* PlayerState
    - Shared player state between all threads
    */
    enum class Flags {
        Ongoing,
        Done,

        True,
        False
    };

    struct PlayerState {
        int queue_position;

        std::deque<std::shared_ptr<music::IStreamable>> user_queue;
        std::deque<std::shared_ptr<music::IStreamable>> radio_queue;

        std::shared_ptr<music::IStreamable> current;

        std::unordered_map<std::string, std::vector<music::ApiResult>> home;
        std::vector<music::Playlist> library_playlists;
        std::vector<music::ApiResult> search_results;

        std::unordered_map<std::string, Flags> flags;

        bool is_logged_in = false;
        bool autoplay = true;

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
    void skipTo(uint16_t index);

    void getHome();
    void isLoggedIn();
    void getLibraryPlaylists();
    void search(const std::string& query);

    void queue(std::shared_ptr<music::IStreamable> streamable, const bool fresh = true, const bool radio = true);
    void queue(std::shared_ptr<music::IStreamableContainer> container, const bool fresh = true, const bool radio = true);
    void removeAt(uint16_t index);

    void setOnStreamStart(std::function<void()> cb) {
        on_stream_start = std::move(cb);
    }

    void setOnStreamEnd(std::function<void()> cb) {
        on_stream_end = std::move(cb);
    }

    void setOnRequestCompletedCallback(std::function<void()> cb) {
        std::lock_guard lock(callback_mutex);
        on_request_completed = std::move(cb);
    }

    void setOnPositionTick(std::function<void(uint64_t position, uint64_t duration)> cb) {
        on_position_tick = std::move(cb);
    }

    void detachUI();

private:
    std::string app_name;

    std::condition_variable command_cv;
    std::mutex callback_mutex;
    std::mutex command_mutex;

    using Command = std::variant<
        SearchCommand,
        QueueStreamableRadioCommand,
        QueueStreamableCommand,

        QueueStreamableContainerRadioCommand,
        QueueStreamableContainerCommand,

        QueueNextRadioCommand,

        HomeCommand,
        LibraryPlaylistsCommand,
        IsLoggedInCommand
    >;

    std::queue<Command> command_queue;

    std::unique_ptr<MPV> mpv_service;
    std::unique_ptr<Mpris> mpris_service;
    std::unique_ptr<Music> music_service;
    std::unique_ptr<Social> social_service;

    std::thread worker_thread;

    bool running = true;

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
    void removeFromUserQueue(uint16_t index);
    void removeFromRadioQueue(uint16_t index);

    std::function<void()> on_stream_start;
    std::function<void()> on_stream_end;

    std::function<void()> on_request_completed;
    std::function<void(uint64_t position, uint64_t duration)> on_position_tick;

    std::thread position_tick_thread;
    std::mutex position_tick_mutex;
    std::condition_variable position_tick_cv;
    bool position_tick_running = false;

    void startPositionTick();
    void stopPositionTick();
};

}
