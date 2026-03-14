#include "../../include/services/player.hpp"
#include <spdlog/spdlog.h>

services::Player::Player(const std::string_view& app_name, const uint64_t app_id) {
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

        case Command::Search: {
            ipc::SearchResponse response = music_service->search(cmd.query);

            {
                std::lock_guard lock(state_mutex);
                state.search_results = response.results;
                state.loading_search = false;
            }

            break;
        }

        }

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
        state.loading_search = true;
        state.search_results.clear();

        command_queue.push(Command{
            Command::Search,
            query
        });
    }

    command_cv.notify_one();
}