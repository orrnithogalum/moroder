#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <memory>

#include "../include/ui/components/search_bar.hpp"
#include "../include/ui/components/carousel.hpp"
#include "../include/ui/components/sidebar.hpp"

#include "../include/services/player.hpp"
#include "../include/config/config.hpp"
#include "../include/utils/utils.hpp"

#include "image_view.hpp"

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
    player.setOnRequestCompletedCallback([&]() {
        screen.PostEvent(Event::Custom);
    });

    std::string query;
    int selected_index = 0;
    bool browsing_results = false;

    auto input = Input(&query, "Search");

    input |= CatchEvent([&](Event event) {
        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;
        }

        auto& results = state_copy.search_results;

        if(event == Event::Return || event == Event::r) {
            if((event == Event::r) && !browsing_results) {
                return false;
            }

            if (browsing_results) {
                // Log selected song if browsing results
                if (!results.empty() && selected_index >= 0 && selected_index < (int)results.size()) {
                    auto& r = results[selected_index];
                    std::visit([&](auto&& data) {
                        using T = std::decay_t<decltype(data)>;
                        if constexpr (std::is_same_v<T, music::SongRef>) {
                            std::shared_ptr<music::IStreamable> streamable;

                            // Resolve the SongRef to a real (empty) Song object
                            music::Song song;
                            song.setRef(data);

                            streamable = std::make_shared<music::Song>(song);

                            // Push to the player queue
                            if (event == Event::r) {
                                // player.radio(streamable);  // Assuming radio accepts IStreamable pointer
                                // player.removeFromUserQueue(0);
                                player.getLibraryPlaylists();

                            } else {
                                player.queue(streamable);  // Queue expects IStreamable pointer

                                // music::Song song2;
                                // music::SongRef song2ref;

                                // music::ArtistRef artist2ref;
                                // artist2ref.id = "UCbIB3Oh5BezJe3sR0BEk0cw";
                                // artist2ref.name = "Claire Laffut";

                                // song2ref.id = "eey0WS_tPUM";
                                // song2ref.title = "Vérité";
                                // song2ref.thumbnail_small = "https://lh3.googleusercontent.com/bT84KwawD-7yHQ44FdJycxjk4ZOuUL6BjGgaxtA94JuAB5lxb_X40Y0Zqw0gLq_vgSA37C8GtGwFsq9b=w60-h60-l90-rj";
                                // song2ref.thumbnail_large = "https://lh3.googleusercontent.com/bT84KwawD-7yHQ44FdJycxjk4ZOuUL6BjGgaxtA94JuAB5lxb_X40Y0Zqw0gLq_vgSA37C8GtGwFsq9b=w120-h120-l90-rj";
                                // song2ref.artists.push_back(artist2ref);

                                // song2.setRef(song2ref);
                                // std::shared_ptr<music::IStreamable> streamable2;
                                // streamable2 = std::make_shared<music::Song>(song2);

                                // player.queue(streamable2, false, false);

                                // music::Episode episode3;
                                // music::EpisodeRef episode3ref;

                                // music::PodcastRef podcast3ref;

                                // podcast3ref.id = "MPSPPL6NdkXsPL07Il2hEQGcLI4dg_LTg7xA2L";
                                // podcast3ref.name = "Lofi Girl - Radios";

                                // episode3ref.id = "SnX4knSvyko";
                                // episode3ref.title = "bossa lofi radio chill music for relaxing days";
                                // episode3ref.thumbnail_small = "https://i.ytimg.com/vi/SnX4knSvyko/hqdefault.jpg?sqp=-oaymwEWCOADEI4CIAQqCggAEOADGC0guwJIWg&rs=AMzJL3kB0kHdPtvmYdpmDC8yJjZ8dbEJIA";
                                // episode3ref.thumbnail_large = "https://i.ytimg.com/vi/SnX4knSvyko/hqdefault.jpg?sqp=-oaymwEWCOADEI4CIAQqCggAEOADGC0guwJIWg&rs=AMzJL3kB0kHdPtvmYdpmDC8yJjZ8dbEJIA";
                                // episode3ref.podcast = podcast3ref;

                                // episode3.setRef(episode3ref);
                                // std::shared_ptr<music::IStreamable> streamable3;
                                // streamable3 = std::make_shared<music::Episode>(episode3);

                                // player.queue(streamable3, false, false);
                            }

                            // spdlog::info("Queued SongRef as IStreamable: {}", data.ref);

                        } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                            // player.queue(data);
                            std::shared_ptr<music::IStreamableContainer> container;

                            music::Album album;
                            album.setRef(data);

                            container = std::make_shared<music::Album>(album);
                            player.queue(container);

                        } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                            std::shared_ptr<music::IStreamable> streamable;

                            music::Episode episode;
                            episode.setRef(data);

                            streamable = std::make_shared<music::Episode>(episode);
                            player.queue(streamable);

                        } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                            std::shared_ptr<music::IStreamableContainer> streamable;

                            music::Playlist playlist;
                            playlist.setRef(data);

                            streamable = std::make_shared<music::Playlist>(playlist);
                            player.queue(streamable);

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

        if (state_copy.loading["search"] == services::Player::LoadingState::Loading) {
            result_elements.emplace_back(text("Loading...") | italic | dim);

        } else {
            for (size_t i = 0; i < state_copy.search_results.size(); ++i) {
                auto& r = state_copy.search_results[i];

                std::string top_label = r.resultType;
                std::string bottom_label = "";
                std::string thumb = "";

                std::visit([&](auto&& data) {
                    using T = std::decay_t<decltype(data)>;

                    if constexpr (std::is_same_v<T, music::SongRef>) {
                        top_label = "󰎇 " + data.title;
                        bottom_label = data.artists[0].name;
                        thumb = data.thumbnail_small;

                    } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                        top_label = "󰀥 " + data.title;
                        bottom_label = data.artists[0].name;
                        thumb = data.thumbnail_small;

                    } else if constexpr (std::is_same_v<T, music::ArtistRef>) {
                        top_label = "󰠃 " + data.name;
                        thumb = data.thumbnail_small;

                    } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                        top_label = "󰲸 " + data.title;
                        bottom_label = data.author;
                        thumb = data.thumbnail_small;

                    } else if constexpr (std::is_same_v<T, music::PodcastRef>) {
                        top_label = " " + data.name;
                        thumb = data.thumbnail_small;

                    } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                        top_label = " " + data.title;
                        bottom_label = data.podcast.name;
                        thumb = data.thumbnail_small;
                    }
                }, r.data);

                ftxui::Element thumb_box;

                /* IMPORTANT:
                - image_view can cause a variety of crashed: (unsupported format, can't reach network if given a url, etc.)
                - keeping it like this for testing
                */
                auto cell = [](const std::string& path){ return ftxui::image_view(path); };

                if (!thumb.empty() && thumb.rfind("https://", 0) == 0) {
                    thumb_box = ftxui::filler() | flex | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2); // cell(thumb)
                } else {
                    thumb_box = ftxui::filler() | flex | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);
                }

                top_label = top_label.substr(0, 30) + "...";
                bottom_label = top_label.substr(0, 30) + "...";

                // Highlight the selected element
                if ((int)i == selected_index) {
                    result_elements.emplace_back(
                        vbox(
                            hbox(
                                thumb_box | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2),
                                text(" "),
                                separator(),
                                text(" "),
                                vbox(
                                    text(top_label) | bold,
                                    text(bottom_label) | dim
                                )
                            ) | inverted,
                            text("")
                        )
                    );
                } else {
                    result_elements.emplace_back(
                        vbox(
                            hbox(
                                thumb_box | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2),
                                text(" "),
                                separator(),
                                text(" "),
                                vbox(
                                    text(top_label) | bold,
                                    text(bottom_label) | dim
                                )
                            ),
                            text("")
                        )
                    );
                }
            }
        }

        return vbox({
            text("[DEBUG] PLAYER STATUS"),
            text(state_copy.loading["audio"] == services::Player::LoadingState::Loading ? "streaming" : "not streaming") | color(state_copy.loading["audio"] == services::Player::LoadingState::Loading ? Color::Green : Color::Blue),
            text(state_copy.loading["search"] == services::Player::LoadingState::Loading ? "loading search" : "not searching") | color(state_copy.loading["search"] == services::Player::LoadingState::Loading ? Color::Green : Color::Blue),
            text(state_copy.autoplay ? "autoplay: true" : "autoplay: false") | color(state_copy.autoplay ? Color::Green : Color::Blue),
            text("Queue position: " + std::to_string(state_copy.queue_position)) | color(Color::Default),
            text("Queue size: " + std::to_string(state_copy.user_queue.size())) | color(Color::Default),
            text("Radio queue size: " + std::to_string(state_copy.radio_queue.size())) | color(Color::Default),
            text(""),
            text("Search") | bold,
            input->Render(),
            separator(),
            text(""),
            vbox(std::move(result_elements)) | frame
        });
    });

    ftxui::setOnImageLoadedCallback([&](){
        screen.PostEvent(Event::Custom);
    });

    auto main_component = Container::Vertical({
        input
    });

    auto queue_renderer = Renderer([&] {
        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;
        }

        std::vector<Element> user_queue_elements;
        std::vector<Element> radio_queue_elements;

        // Populate user queue elements
        for (const auto& item : state_copy.user_queue) {
            std::string label;

            if (auto song = dynamic_cast<music::Song*>(item.get())) {
                label = "Song: " + song->ref.title;
            } else if (auto episode = dynamic_cast<music::Episode*>(item.get())) {
                label = "Episode: " + episode->ref.title;
            } else {
                label = "Unknown item";
            }

            label = label.substr(0, 30) + "...";
            user_queue_elements.push_back(text(label));
        }

        // Populate radio queue elements
        for (const auto& item : state_copy.radio_queue) {
            std::string label;

            if (auto song = dynamic_cast<music::Song*>(item.get())) {
                label = "Song: " + song->ref.title;
            } else if (auto episode = dynamic_cast<music::Episode*>(item.get())) {
                label = "Episode: " + episode->ref.title;
            } else {
                label = "Unknown item";
            }

            radio_queue_elements.push_back(text(label));
        }

        return vbox({
            vbox({
                text("User Queue") | bold | center,
                vbox(std::move(user_queue_elements)) | frame
            }) | flex,
            text(""),
            separator(),
            text(""),
            vbox({
                text("Radio Queue") | bold | center,
                vbox(std::move(radio_queue_elements)) | frame
            }) | flex
        }) | flex;
    });

    auto ui = Renderer(main_component, [&] {
        return hbox({
            renderer->Render() | frame | flex,
            text(" "),
            separator(),
            text(" "),
            queue_renderer->Render() | frame | flex
        });
    });

    // screen.Loop(ui);

    // Non-Debug ui:

    player.isLoggedIn();
    player.getLibraryPlaylists();

    int spinner_frame = 0;

    int sidebar_selected = 0;
    bool sidebar_focused = false;

    ui::SidebarData sidebar_data = {};
    auto sidebar = ui::Sidebar(&sidebar_data, &sidebar_selected, &sidebar_focused);

    ui::SearchBarData search_bar_data = {};
    auto search_bar = ui::SearchBar(&search_bar_data);

    int carousel_selected = 0;
    bool carousel_focused = false;

    ui::CarouselData carousel_data = {};
    auto carousel = ui::Carousel(&carousel_data, &carousel_selected, &carousel_focused);

    Component vertical_separator = Renderer([] {
        return vbox({
            separator() | color(Color::RGB(100, 100, 100)),
            text("")
        });
    });

    auto layout = Container::Horizontal({
        sidebar,
        vertical_separator,
        search_bar,
        carousel
    });

    ui = Renderer(layout, [&] {
        sidebar_focused = sidebar->Focused();

        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;
        }

        spinner_frame++;
        ui::getSidebarData(&state_copy, &sidebar_data);

        return hbox({
            vbox({
                text(" "),
                hbox({
                    text("") | color(Color::Red1),
                    text("  "),
                    text("Moroder")
                }) | bold | center,

                text(" "),
                text(" "),

                sidebar->Render() | yframe,
                sidebar_data.is_loading ? (vbox({filler(), spinner(15, spinner_frame), filler()}) | center | flex) : emptyElement()
            }) | size(WIDTH, EQUAL, 30),

            text(" "),
            separator() | color(Color::RGB(100, 100, 100)),
            text(" "),

            vbox({
                hbox({
                    search_bar->Render() | flex,
                    vbox({
                        text(""),
                        state_copy.is_logged_in ? (text("") | size(WIDTH, EQUAL, 3) | color(Color::Green1)) : text("") | size(WIDTH, EQUAL, 3) | color(Color::Red1),
                        text("")
                    }) | align_right
                }),

                separator() | color(Color::RGB(100, 100, 100)),
                text(" "),

                text(" "),
                text(" "),
                carousel->Render()
            }) | flex
        });
    });

    screen.Loop(ui);
}
