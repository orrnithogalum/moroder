#include "../include/ui/components/playback_bar.hpp"
#include "../include/ui/components/search_bar.hpp"
#include "../include/ui/components/sidebar.hpp"
#include "../include/ui/components/content.hpp"
#include "../include/ui/constants/states.hpp"
#include "../include/ui/constants/colors.hpp"

#include "../include/services/player.hpp"
#include "../include/config/config.hpp"
#include "../include/setup/setup.hpp"
#include "../include/utils/utils.hpp"

#include "image_view.hpp"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>

#define APP_NAME_HUMAN "Moroder"
#define APP_NAME "moroder"

namespace fs = std::filesystem;

using namespace ftxui;

int main(int argc, char *argv[]) {
    Config::app_name = APP_NAME;

    utils::ensure_dir(utils::resolve_path(MORODER_LOG_PATH));

    auto logger = spdlog::basic_logger_mt(APP_NAME, utils::resolve_path(MORODER_LOG_PATH).string() + "/" + APP_NAME + ".log", true);
    logger->flush_on(spdlog::level::info);

    spdlog::set_default_logger(logger);

    /* Subcommands run before anything else is built.
    - setup has to work on a machine with no config and no session, so it must
      not go through Player, which needs both
    */
    const std::string command = argc > 1 ? argv[1] : "";

    if (command == "setup") {
        return setup::run(argc, argv);
    }

    /* Anonymous mode
    - Everything except the account works without a session: search, radio and
      playback all go out unauthenticated
    - The library and the home page have nothing to fill themselves with, and
      every request is slower, so this is opt-in rather than a silent fallback
    */
    const bool anonymous = command == "--anonymous" || command == "-a";
    Config::anonymous = anonymous;

    if (command == "--help" || command == "-h") {
        std::cout << "usage: " << APP_NAME << " [command]\n\n"
                  << "  setup         sign in to YouTube Music and fill in the config\n"
                  << "  --anonymous   run without an account, -a for short\n"
                  << "  --help        this text\n\n"
                  << "Run with no arguments to start the player.\n";
        return 0;
    }

    if (!command.empty() && !anonymous) {
        std::cerr << "unknown command: " << command << "\n"
                  << "try `" << APP_NAME << " --help`\n";
        return 2;
    }

    /* A missing browser.json means the user has not run setup, and every
    library request would fail with an auth error they cannot act on. Say so
    once, here, rather than letting the library page explain it later.
    */
    if (!anonymous && !fs::exists(Config::get().YTM_COOKIES_PATH / "browser.json")) {
        std::cerr << "No YouTube Music session found.\n"
                  << "Run `" << APP_NAME << " setup` to sign in, or `" << APP_NAME
                  << " --anonymous` to run without an account.\n";
        return 1;
    }

    if (anonymous) {
        spdlog::info("MAIN: starting without an account");
    }

    services::Player player(APP_NAME, APP_NAME_HUMAN, 1481401025964540125);
    if(!player.isInitialized()) {
        return 1;
    }

    auto screen = ScreenInteractive::Fullscreen();

    screen.TrackMouse(false);

    player.setOnRequestCompletedCallback([&screen]() {
        screen.PostEvent(Event::Custom);
    });

    ftxui::setOnImageLoadedCallback([&screen](){
        screen.PostEvent(Event::Custom);
    });

    const Config& cfg = Config::get();

    ftxui::setImageCacheMaxSize(cfg.MAX_IMAGE_CACHE_SIZE);
    ftxui::setImageResizeCacheMaxSize(cfg.MAX_RESIZED_IMAGE_CACHE_SIZE);
    ftxui::setImageCharCacheMaxSize(cfg.MAX_IMAGE_CHAR_CACHE_SIZE);

    // Caps concurrent background image-loader threads (default is 6).
    // With 40+ thumbnails potentially uncached at once (grid + carousels +
    // queue + playback bar), leave this in place rather than letting every
    // uncached image spawn its own thread simultaneously.
    ftxui::setMaxConcurrentImageLoads(cfg.MAX_CONCURRENT_IMAGE_LOADS);


    player.isLoggedIn();
    player.getHome();
    player.getLibraryPlaylists();
    player.getLibraryAlbums();
    player.getLibraryArtists();
    player.getLibraryPodcasts();
    player.getLibrarySongs();

    int spinner_frame = 0;
    int sidebar_selected = 0;

    bool sidebar_hidden  = false;
    bool sidebar_focused = false;
    bool audio_playing = false;

    auto current_state = ui::State::HOME;

    ui::SearchBarData search_bar_data = {};
    auto search_bar = ui::SearchBar(&search_bar_data);

    ui::PlaybackData playback_bar_data = {};
    auto playback_bar = ui::PlaybackBar(&playback_bar_data);

    player.setOnPositionTick([&screen, &playback_bar_data](uint64_t position, uint64_t duration) {
        // Remove two seconds to account for UI refresh period
        // This is just a fix so that the UI progress bar actually reaches song end
        duration -= services::Player::LOOP_REWIND_EPSILON;

        if (duration <= 0) return;

        int progress = (position * 100) / duration;
        progress = std::clamp(progress, 0, 100);

        playback_bar_data.progress = progress;
        screen.PostEvent(Event::Custom);
    });

    player.setOnStreamStart([&screen, &playback_bar_data] {
        playback_bar_data.progress = 0;
        screen.PostEvent(Event::Custom);
    });

    player.setOnStreamEnd([&screen, &playback_bar_data] {
        playback_bar_data.progress = 0;
        screen.PostEvent(Event::Custom);
    });

    std::deque<ui::ContentEntry> main_content_items;
    auto main_content = Container::Vertical({ Renderer([]{ return emptyElement(); }) });

    std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_item_press =
    [&current_state, &player, &main_content_items, &main_content, &screen, &cfg](const ftxui::Event& event, const music::ApiResult& result) {
        if(!Config::isKey(event, cfg.KEY_ADD_TO_QUEUE) && !Config::isKey(event, cfg.KEY_PLAY_NOW)) {
            return false;
        }

        spdlog::info("ITEM: key pressed on result, " + result.resultType);
        bool should_queue = Config::isKey(event, cfg.KEY_ADD_TO_QUEUE);

        bool queued = false;

        std::visit([&](auto&& data) {
            using T = std::decay_t<decltype(data)>;

            auto make_streamable = [&](auto&& obj) {
                obj.setRef(data);
                return std::make_shared<std::decay_t<decltype(obj)>>(std::move(obj));
            };

            if constexpr (std::is_same_v<T, music::SongRef>) {
                auto streamable = make_streamable(music::Song{});
                player.queue(streamable, !should_queue, !should_queue);
                queued = true;

            } else if constexpr (std::is_same_v<T, music::AlbumRef>) {
                auto container = make_streamable(music::Album{});
                player.queue(container, !should_queue, !should_queue);
                queued = true;

            } else if constexpr (std::is_same_v<T, music::EpisodeRef>) {
                auto streamable = make_streamable(music::Episode{});
                player.queue(streamable, !should_queue, !should_queue);
                queued = true;

            } else if constexpr (std::is_same_v<T, music::PlaylistRef>) {
                auto container = make_streamable(music::Playlist{});
                player.queue(container, !should_queue, !should_queue);
                queued = true;
            }

        }, result.data);

        if (!queued) {
            spdlog::info("ITEM: nothing queued for result type, " + result.resultType);
            return true;
        }

        current_state = ui::State::QUEUE;

        main_content_items.clear();
        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        screen.PostEvent(Event::Custom);

        return true;
    };

    std::function<bool(const ftxui::Event&, const music::Playlist&)> on_sidebar_press =
    [&current_state, &player, &main_content_items, &main_content, &screen, &cfg](const ftxui::Event& event, const music::Playlist& playlist) {
        if(!Config::isKey(event, cfg.KEY_ADD_TO_QUEUE) && !Config::isKey(event, cfg.KEY_PLAY_NOW)) {
            return false;
        }

        spdlog::info("SIDEBAR: key pressed on playlist, " + playlist.ref.title);
        bool should_queue = Config::isKey(event, cfg.KEY_ADD_TO_QUEUE);

        auto container = std::make_shared<music::Playlist>(playlist);
        player.queue(container, !should_queue, !should_queue);

        current_state = ui::State::QUEUE;

        main_content_items.clear();
        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        screen.PostEvent(Event::Custom);

        return true;
    };

    std::function<bool(const ftxui::Event&, const int)> on_queue_press =
    [&current_state, &player, &main_content_items, &main_content, &screen, &cfg](const ftxui::Event& event, const int queue_index) {
        if(!Config::isKey(event, cfg.KEY_REMOVE_FROM_QUEUE) && !Config::isKey(event, cfg.KEY_PLAY_NOW)) {
            return false;
        }

        spdlog::info("QUEUE: key pressed on item, " + std::to_string(queue_index));
        bool should_remove = Config::isKey(event, cfg.KEY_REMOVE_FROM_QUEUE);

        if(!should_remove) {
            player.skipTo(queue_index);

        } else {
            player.removeAt(queue_index);
        }

        main_content_items.clear();
        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        screen.PostEvent(Event::Custom);

        return true;
    };

    std::function<bool(const ftxui::Event&, const ui::ChipEntry&)> on_chip_press =
    [&main_content_items, &screen, &cfg](const ftxui::Event& event, const ui::ChipEntry& chip) {
        if(!Config::isKey(event, cfg.KEY_PLAY_NOW)) {
            return false;
        }

        spdlog::info("CHIPS: pressed chip, " + chip.id + " (" + chip.label + ")");

        for (auto& entry : main_content_items) {
            if (entry.category == "__library_filters__") {
                ui::toggleChip(&entry.chips_data, chip.id);
                break;
            }
        }

        screen.PostEvent(Event::Custom);
        return true;
    };

    ui::SidebarData sidebar_data = {};
    auto sidebar = ui::Sidebar(&sidebar_data, &sidebar_selected, &sidebar_focused, on_sidebar_press);

    auto sidebar_container = Container::Horizontal({
        sidebar
    });

    auto main_content_container = Container::Horizontal({
        sidebar_container,
        Renderer([] {
            return vbox({
                separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),
                text(" ")
            });
        }),
        Container::Vertical({
            search_bar,
            main_content
        }),
    });

    auto layout = Container::Vertical({
        main_content_container,
        playback_bar
    });

    sidebar_data.onHome = [&current_state, &main_content_items, &main_content, &screen] {
        spdlog::info("SIDEBAR: Home pressed");

        current_state = ui::State::HOME;

        main_content_items.clear();
        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        screen.PostEvent(Event::Custom);
    };

    sidebar_data.onLibrary = [&current_state, &main_content_items, &main_content, &screen] {
        spdlog::info("SIDEBAR: Library pressed");
        current_state = ui::State::LIBRARY;

        main_content_items.clear();
        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        screen.PostEvent(Event::Custom);
    };

    search_bar_data.onSearch = [&current_state, &player, &main_content_items, &main_content, &screen](const std::string& value) {
        spdlog::info("SEARCHBAR: enter pressed with value, " + value);
        current_state = ui::State::SEARCH;

        player.search(value);

        main_content_items.clear();
        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        screen.PostEvent(Event::Custom);
    };

    auto ui = Renderer(layout, [&] {
        sidebar_focused = sidebar->Focused();

        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;

            if (player.state.current) {
                state_copy.current = player.state.current->clone();
            }

            state_copy.user_queue.clear();
            for (auto& item : player.state.user_queue) {
                state_copy.user_queue.push_back(item->clone());
            }

            state_copy.radio_queue.clear();
            for (auto& item : player.state.radio_queue) {
                state_copy.radio_queue.push_back(item->clone());
            }

            audio_playing = state_copy.flags["audio"] == services::Player::Flags::Ongoing;
        }

        spinner_frame++;

        ui::getSidebarData(&state_copy, &sidebar_data);
        ui::getPlaybackData(&state_copy, &playback_bar_data, screen.dimx());

        if(current_state == ui::State::HOME) {
            ui::buildHome(&state_copy, main_content_items, main_content, on_item_press, [&player, &main_content, &main_content_items, &screen]{
                player.getHome();

                if(player.state.flags["library_playlists"] != services::Player::Flags::Done) {
                    player.getLibraryPlaylists();
                }

                main_content_items.clear();
                main_content->DetachAllChildren();
                main_content->Add(Renderer([] { return emptyElement(); }));

                screen.PostEvent(Event::Custom);
            });

        } else if(current_state == ui::State::SEARCH && state_copy.flags["search"] == services::Player::Flags::Done) {
            ui::buildSearch(&state_copy, main_content_items, main_content, on_item_press, [&player, &search_bar_data, &main_content, &main_content_items, &screen] {
                player.search(search_bar_data.value);

                main_content_items.clear();
                main_content->DetachAllChildren();
                main_content->Add(Renderer([] { return emptyElement(); }));

                screen.PostEvent(Event::Custom);
            });

        } else if(current_state == ui::State::QUEUE) {
            ui::buildQueue(&state_copy, main_content_items, main_content, on_queue_press);

        } else if(current_state == ui::State::LIBRARY) {
            int library_rows = std::max(1, (screen.dimy() - 12) / ui::GRID_TILE_HEIGHT);

            ui::buildLibrary(&state_copy, main_content_items, main_content, library_rows, search_bar_data.value, on_chip_press, on_item_press,
                [&player, &screen] {
                    player.retryLibrary();
                    screen.PostEvent(Event::Custom);
                });
        }

        return vbox({
            hbox({
                !sidebar_hidden
                    ? vbox({
                        text(" "),
                        hbox({
                            text("") | color(ui::GetColor(ui::MColor::ACCENT_PRIMARY)),
                            text("  "),
                            text(APP_NAME_HUMAN) | color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY))
                        }) | bold | center,

                        text(" "),
                        text(" "),

                        sidebar->Render() | yframe
                    }) | size(WIDTH, EQUAL, 30)
                    : emptyElement(),

                !sidebar_hidden
                    ? hbox({
                        text(" "),
                        separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),
                        text(" "),
                    })
                    : emptyElement(),

                vbox({
                    hbox({
                        search_bar->Render() | flex,
                        vbox({
                            text(" "),
                            state_copy.is_logged_in ?
                                text("") | size(WIDTH, EQUAL, 3) | color(ui::GetColor(ui::MColor::SUCCESS)) :
                                text("") | size(WIDTH, EQUAL, 3) |  color(ui::GetColor(ui::MColor::ERROR)),
                            text(" ")
                        }) | align_right
                    }),

                    separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),

                    hbox({
                        text(" "),
                        [&] {
                            const std::string error_key =
                                  current_state == ui::State::HOME    ? "home"
                                : current_state == ui::State::SEARCH  ? "search"
                                : current_state == ui::State::LIBRARY ? "library"
                                                                      : "";

                            const bool has_error = !error_key.empty() && state_copy.errors.count(error_key) > 0;

                            /* The signed-out notice on home is an ErrorBox too,
                            and wants the same centred, flexed layout even though
                            there is no entry in errors for it.
                            */
                            const bool notice = current_state == ui::State::HOME
                                && state_copy.flags["is_logged_in"] == services::Player::Flags::Done
                                && !state_copy.is_logged_in;

                            return (has_error || notice)
                                ? main_content->Render() | yframe | flex
                                : current_state != ui::State::QUEUE
                                    ? main_content->Render() | yframe | yflex
                                    : main_content->Render() | xflex;
                        }(),
                    }) | yflex,

                    ((main_content_items.empty() || sidebar_data.is_loading) && state_copy.is_logged_in) ?
                        vbox({
                            filler(),
                            hbox({
                                filler(),
                                spinner(15, spinner_frame)
                            })
                        }) | flex :
                        emptyElement(),

                }) | flex
            }) | flex,

            (playback_bar_data.is_playing  && state_copy.flags["paused"] == services::Player::Flags::False) ||
            (!playback_bar_data.is_playing && state_copy.flags["paused"] == services::Player::Flags::True)
                ? playback_bar->Render()
                : emptyElement(),

            Config::get().EXTRA_BOTTOM_PADDING ? text("") : emptyElement()
        });
    });

    ui = CatchEvent(ui, [&search_bar, &screen, &sidebar_hidden, &sidebar, &sidebar_container, &audio_playing, &current_state, &player, &cfg](Event event){
        if(search_bar->Focused()) {
            return false;
        }

        if(Config::isKey(event, cfg.KEY_QUIT)) {
            screen.Exit();
            return true;
        }


        if(Config::isKey(event, cfg.KEY_QUEUE_VIEW) && audio_playing) {
            current_state = ui::State::QUEUE;
            return true;
        }

        if(Config::isKey(event, cfg.KEY_TOGGLE_SIDEBAR)) {
            sidebar_hidden = !sidebar_hidden;

            if(sidebar_hidden) {
                sidebar->Detach();
            } else {
                sidebar_container->Add(sidebar);
            }

            return true;
        }

        if(Config::isKey(event, cfg.KEY_FOCUS_SEARCH)) {
            search_bar->TakeFocus();
            return true;
        }

        if(Config::isKey(event, cfg.KEY_SKIP_BACKWARD)) {
            player.skipBackward();
            return true;
        }

        if(Config::isKey(event, cfg.KEY_SKIP_FORWARD)) {
            player.skipForward();
            return true;
        }

        if(Config::isKey(event, cfg.KEY_TOGGLE_PAUSE)) {
            player.togglePause();
            return true;
        }

        // The playback bar reads the mode out of the state copy it takes each
        // render, so returning true is all the redraw this needs.
        if(Config::isKey(event, cfg.KEY_TOGGLE_LOOP)) {
            player.cycleLoop();
            return true;
        }

        return false;
    });

    screen.Loop(ui);
    player.detachUI();
}
