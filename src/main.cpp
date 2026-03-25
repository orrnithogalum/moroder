#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "../include/services/player.hpp"
#include "../include/config/config.hpp"
#include "../include/utils/utils.hpp"

#define APP_NAME_HUMAN "Moroder"
#define APP_NAME "moroder"

using namespace ftxui;

int main(int argc, char *argv[]) {
    Config::app_name = APP_NAME;

    utils::ensure_dir(utils::resolve_path(MORODER_PYTHON_PATH));
    utils::ensure_dir(utils::resolve_path(MORODER_LOG_PATH));

    auto logger = spdlog::basic_logger_mt(
        APP_NAME,
        utils::resolve_path(MORODER_LOG_PATH).string() + "/" + APP_NAME + ".log",
        true
    );

    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);

    services::Player player(APP_NAME, APP_NAME_HUMAN, 1481401025964540125);

    auto screen = ScreenInteractive::Fullscreen();

    // Trigger UI update when request completes
    player.setOnRequestCompletedCallback([&](services::Player::Command::Type type) {
        screen.PostEvent(Event::Custom);
    });

    std::string query;
    int selected_index = 0;                 // track selected search result
    bool browsing_results = false;          // true if navigating results

    auto input = Input(&query, "Search");

    input |= CatchEvent([&](Event event) {
        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;
        }

        auto& results = state_copy.search_results;

        if(event == Event::Return || event == Event::r) {
            if(event == Event::r && !browsing_results) {
                return false;
            }

            if (browsing_results) {
                // Log selected song if browsing results
                if (!results.empty() && selected_index >= 0 && selected_index < (int)results.size()) {
                    auto& r = results[selected_index];
                    std::visit([&](auto&& data) {
                        using T = std::decay_t<decltype(data)>;
                        if constexpr (std::is_same_v<T, music::SongRef>) {

                            if(event == Event::r) {
                                player.radio(data);
                            } else {
                                player.queue(data);
                            }

                        } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                            player.queue(data);

                        } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                            player.queue(data);

                        }
                    }, r.data);
                }
            } else {
                // Enter pressed in input → trigger search
                player.search(query);
                selected_index = 0;
            }
            return true;

        } else if (event == Event::ArrowDown) {
            if (!results.empty()) {
                browsing_results = true;
                selected_index = (selected_index + 1) % results.size();
            }
            return true;

        } else if (event == Event::ArrowUp) {
            if (!results.empty()) {
                browsing_results = true;
                selected_index = (selected_index - 1 + results.size()) % results.size();
            }
            return true;

        } else {
            browsing_results = false; // any other key → back to typing mode
            return false;
        }
    });

    auto renderer = Renderer(input, [&] {
        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;
        }

        std::vector<Element> result_elements;

        if (state_copy.is_loading_search) {
            result_elements.push_back(text("Loading...") | italic | dim);
        } else {
            for (size_t i = 0; i < state_copy.search_results.size(); ++i) {
                auto& r = state_copy.search_results[i];
                std::string label = r.resultType;

                std::visit([&](auto&& data) {
                    using T = std::decay_t<decltype(data)>;
                    if constexpr (std::is_same_v<T, music::SongRef>) {
                        label = "[SONG] " + data.title;
                    } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                        label = "[ALBUM] " + data.title;
                    } else if constexpr (std::is_same_v<T, music::ArtistRef>) {
                        label = "[ARTIST] " + data.name;
                    } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                        label = "[PLAYLIST] " + data.title;
                    }
                }, r.data);

                // Highlight the selected element
                if ((int)i == selected_index) {
                    result_elements.push_back(text("> " + label) | inverted);
                } else {
                    result_elements.push_back(text("  " + label));
                }
            }
        }

        return vbox({
            text(state_copy.is_streaming_audio ? "streaming" : "stopped"),
            text("Search") | bold,
            input->Render(),
            separator(),
            vbox(std::move(result_elements)) | frame
        });
    });

    auto main_component = Container::Vertical({
        input
    });

    auto ui = Renderer(main_component, [&] {
        return renderer->Render();
    });

    screen.Loop(ui);
}
