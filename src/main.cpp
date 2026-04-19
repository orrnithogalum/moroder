#include "../include/ui/components/playback_bar.hpp"
#include "../include/ui/components/search_bar.hpp"
#include "../include/ui/components/sidebar.hpp"
#include "../include/ui/components/content.hpp"
#include "../include/ui/constants/states.hpp"
#include "../include/ui/constants/colors.hpp"

#include "../include/services/player.hpp"
#include "../include/config/config.hpp"
#include "../include/utils/utils.hpp"

#include "image_view.hpp"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>

#define APP_NAME_HUMAN "Moroder"
#define APP_NAME "moroder"

using namespace ftxui;

int main(int argc, char *argv[]) {
    Config::app_name = APP_NAME;

    utils::ensure_dir(utils::resolve_path(MORODER_PYTHON_PATH));
    utils::ensure_dir(utils::resolve_path(MORODER_LOG_PATH));

    auto logger = spdlog::basic_logger_mt(APP_NAME, utils::resolve_path(MORODER_LOG_PATH).string() + "/" + APP_NAME + ".log", true);
    logger->flush_on(spdlog::level::info);

    spdlog::set_default_logger(logger);

    services::Player player(APP_NAME, APP_NAME_HUMAN, 1481401025964540125);
    auto screen = ScreenInteractive::Fullscreen();

    screen.TrackMouse(false);

    player.setOnRequestCompletedCallback([&screen]() {
        screen.PostEvent(Event::Custom);
    });

    ftxui::setOnImageLoadedCallback([&screen](){
        screen.PostEvent(Event::Custom);
    });

    ftxui::setImageCacheMaxSize(200);
    ftxui::setImageResizeCacheMaxSize(200);
    ftxui::setImageCharCacheMaxSize(20000);

    player.isLoggedIn();
    player.getLibraryPlaylists();
    player.getHome();

    int spinner_frame    = 0;
    int sidebar_selected = 0;

    bool sidebar_hidden  = false;
    bool sidebar_focused = false;

    auto current_state = ui::State::HOME;

    ui::SidebarData sidebar_data = {};
    auto sidebar = ui::Sidebar(&sidebar_data, &sidebar_selected, &sidebar_focused);

    ui::SearchBarData search_bar_data = {};
    auto search_bar = ui::SearchBar(&search_bar_data);

    ui::PlaybackData playback_bar_data = {};
    auto playback_bar = ui::PlaybackBar(&playback_bar_data);

    player.setOnPositionTick([&screen, &playback_bar_data](uint64_t position, uint64_t duration) {
        int progress = (position * 100) / duration;
        progress = std::clamp(progress, 0, 100);

        playback_bar_data.progress = progress;
        screen.PostEvent(Event::Custom);
    });

    std::deque<ui::ContentEntry> main_content_items;
    auto main_content = Container::Vertical({ Renderer([]{ return emptyElement(); }) });

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

    search_bar_data.onSearch = [&current_state, &player, &main_content_items, &main_content, &screen](const std::string& value) {
        spdlog::info("SEARCHBAR: enter pressed with value, " + value);
        current_state = ui::State::QUEUE;

        // player.search(value);





        music::Song song2;
        music::SongRef song2ref;

        music::ArtistRef artist2ref;
        artist2ref.id = "UCGz-eguN8tcic5kUG4s1ZgA";
        artist2ref.name = "Tame Impala";

        song2ref.id = "NMRhx71bGo4";
        song2ref.title = "Let It Happen";
        song2ref.thumbnail_small = "https://lh3.googleusercontent.com/J67cuSWAzGMlj8d9orcAZjPHsl8RWcXIXkT1d8mGmx9jmXPvXkYpFzuLnucmaqJwVMqxPlSq1GbqPeQy";
        song2ref.thumbnail_large = "https://lh3.googleusercontent.com/J67cuSWAzGMlj8d9orcAZjPHsl8RWcXIXkT1d8mGmx9jmXPvXkYpFzuLnucmaqJwVMqxPlSq1GbqPeQy";
        song2ref.artists.push_back(artist2ref);

        song2.setRef(song2ref);
        std::shared_ptr<music::IStreamable> streamable2;
        streamable2 = std::make_shared<music::Song>(song2);

        player.queue(streamable2, true, true);






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
        }

        spinner_frame++;
        ui::getSidebarData(&state_copy, &sidebar_data);
        ui::getPlaybackData(&state_copy, &playback_bar_data, screen.dimx());

        if(current_state == ui::State::HOME) {
            ui::buildHome(&state_copy, main_content_items, main_content);

        } else if(current_state == ui::State::SEARCH && state_copy.loading["search"] == services::Player::LoadingState::Done) {
            ui::buildSearch(&state_copy, main_content_items, main_content);

        } else if(current_state == ui::State::QUEUE) {
            ui::buildQueue(&state_copy, main_content_items, main_content);
        }

        return vbox({
            hbox({
                !sidebar_hidden
                    ? vbox({
                        text(" "),
                        hbox({
                            text("") | color(Color::Red1),
                            text("  "),
                            text(APP_NAME_HUMAN)
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
                                text("") | size(WIDTH, EQUAL, 3) | color(ui::GetColor(ui::MColor::SUCESS)) :
                                text("") | size(WIDTH, EQUAL, 3) |  color(ui::GetColor(ui::MColor::ERROR)),
                            text(" ")
                        }) | align_right
                    }),

                    separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),

                    hbox({
                        text(" "),
                        main_content->Render() | yframe | yflex
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

            playback_bar_data.is_playing
                ? playback_bar->Render()
                : emptyElement()
        });
    });

    ui = CatchEvent(ui, [&search_bar, &sidebar_hidden, &sidebar, &sidebar_container](Event event){
        if(event == Event::s && !search_bar->Focused()) {
            sidebar_hidden = !sidebar_hidden;

            if(sidebar_hidden) {
                sidebar->Detach();
            } else {
                sidebar_container->Add(sidebar);
            }

            return true;
        }

        return false;
    });

    screen.Loop(ui);
}
