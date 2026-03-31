#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <memory>
// #include <variant>
// #include <vector>
// #include <iostream>

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

    // std::unique_ptr<services::Music> music_service = std::make_unique<services::Music>(APP_NAME);

    // std::vector<music::SearchResult> results = music_service->getSearch("hand picked str-23");
    // std::vector<music::SearchResult> results = music_service->getSearch("giorgio by moroder daft punk");
    // std::vector<music::SearchResult> results = music_service->getSearch("wan show");

    // for (const auto& result : results) {
    //     std::visit([&](auto&& value) {
    //         using T = std::decay_t<decltype(value)>;

    //         if constexpr (std::is_same_v<T, std::monostate>) {
    //             std::cout << "Empty result\n";

    //         } else if constexpr (std::is_same_v<T, music::SongRef>) {
    //             std::cout << "Song: " << value.title << "\n";

    //             music::Song song = music_service->getSong(value);

    //             std::cout << song.ref.id << std::endl;
    //             std::cout << song.ref.artists[0].name << std::endl;
    //             std::cout << song.ref.thumbnail_small << std::endl;
    //             std::cout << song.ref.title << std::endl;
    //             std::cout << song.album.title << std::endl;
    //             std::cout << song.getStreamUrl() << std::endl;
    //             std::cout << song.album.id << std::endl;
    //             std::cout << "-------"<< std::endl;

    //             music::Radio radio = music_service->getRadio(value);
    //             for (const auto& streamable : radio.getStreamables()) {
    //                 if (auto song = std::dynamic_pointer_cast<music::Song>(streamable)) {
    //                     std::cout << "This is a Song: " << song->ref.title << "\n";
    //                     std::cout << song->ref.id << std::endl;
    //                     std::cout << song->ref.artists[0].name << std::endl;
    //                     std::cout << song->ref.thumbnail_small << std::endl;
    //                     std::cout << song->ref.title << std::endl;
    //                     std::cout << song->album.id << std::endl;
    //                     std::cout << song->album.title << std::endl;
    //                     std::cout << song->getStreamUrl() << std::endl;
    //                     std::cout << "-------"<< std::endl;

    //                 } else if (auto episode = std::dynamic_pointer_cast<music::Episode>(streamable)) {
    //                     std::cout << "This is an Episode: " << episode->ref.title << "\n";

    //                 } else {
    //                     std::cout << "Unknown streamable type\n";
    //                 }
    //             }

    //             std::cout << radio.continuation << std::endl;

    //             music::Radio radio_cont = music_service->getRadioNext(radio);
    //             for (const auto& streamable : radio_cont.getStreamables()) {
    //                 if (auto song = std::dynamic_pointer_cast<music::Song>(streamable)) {
    //                     std::cout << "This is a Song: " << song->ref.title << "\n";
    //                     std::cout << song->ref.id << std::endl;
    //                     std::cout << song->ref.artists[0].name << std::endl;
    //                     std::cout << song->ref.thumbnail_small << std::endl;
    //                     std::cout << song->ref.title << std::endl;
    //                     std::cout << song->album.id << std::endl;
    //                     std::cout << song->album.title << std::endl;
    //                     std::cout << song->getStreamUrl() << std::endl;
    //                     std::cout << "-------"<< std::endl;

    //                 } else if (auto episode = std::dynamic_pointer_cast<music::Episode>(streamable)) {
    //                     std::cout << "This is an Episode: " << episode->ref.title << "\n";

    //                 } else {
    //                     std::cout << "Unknown streamable type\n";
    //                 }
    //             }

    //             std::cout << radio.continuation << std::endl;

    //             radio_cont = music_service->getRadioNext(radio_cont);
    //             for (const auto& streamable : radio_cont.getStreamables()) {
    //                 if (auto song = std::dynamic_pointer_cast<music::Song>(streamable)) {
    //                     std::cout << "This is a Song: " << song->ref.title << "\n";
    //                     std::cout << song->ref.id << std::endl;
    //                     std::cout << song->ref.artists[0].name << std::endl;
    //                     std::cout << song->ref.thumbnail_small << std::endl;
    //                     std::cout << song->ref.title << std::endl;
    //                     std::cout << song->album.id << std::endl;
    //                     std::cout << song->album.title << std::endl;
    //                     std::cout << song->getStreamUrl() << std::endl;
    //                     std::cout << "-------"<< std::endl;

    //                 } else if (auto episode = std::dynamic_pointer_cast<music::Episode>(streamable)) {
    //                     std::cout << "This is an Episode: " << episode->ref.title << "\n";

    //                 } else {
    //                     std::cout << "Unknown streamable type\n";
    //                 }
    //             }

    //             exit(0);

    //         } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
    //             std::cout << "Album: " << value.title << "\n";

    //             music::Album album = music_service->getAlbum(value);

    //             for(const auto& streamable : album.getStreamables()) {
    //                 if (auto song = std::dynamic_pointer_cast<music::Song>(streamable)) {
    //                     std::cout << "This is a Song: " << song->ref.title << "\n";
    //                     std::cout << song->ref.id << std::endl;
    //                     std::cout << song->ref.artists[0].name << std::endl;
    //                     std::cout << song->ref.thumbnail_small << std::endl;
    //                     std::cout << song->ref.title << std::endl;
    //                     std::cout << song->album.id << std::endl;
    //                     std::cout << song->album.title << std::endl;
    //                     std::cout << song->getStreamUrl() << std::endl;
    //                     std::cout << "-------"<< std::endl;

    //                 } else if (auto episode = std::dynamic_pointer_cast<music::Episode>(streamable)) {
    //                     std::cout << "This is an Episode: " << episode->ref.title << "\n";

    //                 } else {
    //                     std::cout << "Unknown streamable type\n";
    //                 }
    //             }

    //             exit(0);

    //         } else if constexpr (std::is_same_v<T, music::ArtistRef>) {
    //             std::cout << "Artist: " << value.name << "\n";

    //         } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
    //             std::cout << "Playlist: " << value.title << "\n";

    //             music::Playlist playlist = music_service->getPlaylist(value);

    //             for(const auto& streamable : playlist.getStreamables()) {
    //                 if (auto song = std::dynamic_pointer_cast<music::Song>(streamable)) {
    //                     std::cout << "This is a Song: " << song->ref.title << "\n";
    //                     std::cout << song->ref.id << std::endl;
    //                     std::cout << song->ref.artists[0].name << std::endl;
    //                     std::cout << song->ref.thumbnail_small << std::endl;
    //                     std::cout << song->ref.title << std::endl;
    //                     std::cout << song->album.id << std::endl;
    //                     std::cout << song->album.title << std::endl;
    //                     std::cout << song->getStreamUrl() << std::endl;
    //                     std::cout << "-------"<< std::endl;

    //                 } else if (auto episode = std::dynamic_pointer_cast<music::Episode>(streamable)) {
    //                     std::cout << "This is an Episode: " << episode->ref.title << "\n";

    //                 } else {
    //                     std::cout << "Unknown streamable type\n";
    //                 }
    //             }

    //             exit(0);

    //         } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
    //             std::cout << "Episode: " << value.title << "\n";

    //             music::Episode episode = music_service->getEpisode(value);

    //             std::cout << episode.ref.id << std::endl;
    //             std::cout << episode.ref.thumbnail_small << std::endl;
    //             std::cout << episode.ref.title << std::endl;
    //             std::cout << episode.getStreamUrl() << std::endl;
    //             std::cout << episode.ref.podcast.id << std::endl;
    //             std::cout << episode.ref.podcast.name << std::endl;
    //             std::cout << "-------"<< std::endl;

    //             exit(0);

    //         } else if constexpr (std::is_same_v<T, music::PodcastRef>) {
    //             std::cout << "Podcast: " << value.name << "\n";

    //         }
    //     }, result.data);
    // }



    services::Player player(APP_NAME, APP_NAME_HUMAN, 1481401025964540125);

    auto screen = ScreenInteractive::Fullscreen();

    // Trigger UI update when request completes
    player.setOnRequestCompletedCallback([&]() {
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
                            std::shared_ptr<music::IStreamable> streamable;

                            // Resolve the SongRef to a real (empty) Song object
                            music::Song song;
                            song.setRef(data);

                            streamable = std::make_shared<music::Song>(song);

                            // Push to the player queue
                            if (event == Event::r) {
                                // player.radio(streamable);  // Assuming radio accepts IStreamable pointer
                            } else {
                                player.queue(streamable);  // Queue expects IStreamable pointer
                            }

                            // spdlog::info("Queued SongRef as IStreamable: {}", data.ref);

                        } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                            // player.queue(data);

                        } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                            std::shared_ptr<music::IStreamable> streamable;

                            music::Episode episode;
                            episode.setRef(data);

                            streamable = std::make_shared<music::Episode>(episode);

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

        if (state_copy.is_loading_search) {
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
                    thumb_box = ftxui::filler() | flex | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2); // cell(thumb) | flex | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);
                } else {
                    thumb_box = ftxui::filler() | flex | size(WIDTH, EQUAL, 4) | size(HEIGHT, EQUAL, 2);
                }

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
            text(state_copy.is_streaming_audio ? "streaming" : "not streaming") | color(state_copy.is_streaming_audio ? Color::Green : Color::Blue),
            text(state_copy.is_loading_search ? "loading search" : "not searching") | color(state_copy.is_streaming_audio ? Color::Green : Color::Blue),
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

    auto ui = Renderer(main_component, [&] {
        return renderer->Render();
    });

    screen.Loop(ui);
}
