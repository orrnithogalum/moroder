#include <algorithm>
#include <cstdint>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/stat.h>
#include <memory>
#include <mutex>

#include "../include/ui/components/playback_bar.hpp"
#include "../include/ui/components/search_bar.hpp"
#include "../include/ui/components/sidebar.hpp"
#include "../include/ui/components/content.hpp"
#include "../include/ui/constants/colors.hpp"

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

    int spinner_frame = 0;

    int sidebar_selected = 0;
    bool sidebar_focused = false;

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
    std::vector<Component> main_content_components = { Renderer([]{ return emptyElement(); }) };
    auto main_content = Container::Vertical(main_content_components);

    auto layout = Container::Vertical({
        Container::Horizontal({
            sidebar,
            Renderer([] {
                return vbox({
                    separator() | color(ui::GetColor(ui::MColor::SEPARATOR)),
                    text(" ")
                });
            }),
            Container::Vertical({
                search_bar,
                main_content
            }),
        }),
        playback_bar
    });

    search_bar_data.onSearch = [&player, &main_content, &screen](const std::string& value) {
        spdlog::info("SEARCHBAR: enter pressed with value, " + value);

        main_content->DetachAllChildren();
        main_content->Add(Renderer([] { return emptyElement(); }));

        music::Song song2;
        music::SongRef song2ref;

        music::ArtistRef artist2ref;
        artist2ref.id = "UCbIB3Oh5BezJe3sR0BEk0cw";
        artist2ref.name = "Claire Laffut";

        song2ref.id = "eey0WS_tPUM";
        song2ref.title = "Vérité";
        song2ref.thumbnail_small = "https://lh3.googleusercontent.com/bT84KwawD-7yHQ44FdJycxjk4ZOuUL6BjGgaxtA94JuAB5lxb_X40Y0Zqw0gLq_vgSA37C8GtGwFsq9b=w60-h60-l90-rj";
        song2ref.thumbnail_large = "https://lh3.googleusercontent.com/bT84KwawD-7yHQ44FdJycxjk4ZOuUL6BjGgaxtA94JuAB5lxb_X40Y0Zqw0gLq_vgSA37C8GtGwFsq9b=w120-h120-l90-rj";
        song2ref.artists.push_back(artist2ref);

        song2.setRef(song2ref);
        std::shared_ptr<music::IStreamable> streamable2;
        streamable2 = std::make_shared<music::Song>(song2);

        player.queue(streamable2, false, false);

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
        }

        spinner_frame++;
        ui::getSidebarData(&state_copy, &sidebar_data);
        ui::getPlaybackData(&state_copy, &playback_bar_data, screen.dimx());

        // Build main content, home as default
        ui::buildHome(&state_copy, main_content_items, main_content_components, main_content);

        return vbox({
            hbox({
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
                }) | size(WIDTH, EQUAL, 30),

                text(" "),
                separator() | color(ui::GetColor(ui::MColor::SEPARATOR)),
                text(" "),

                vbox({
                    hbox({
                        search_bar->Render() | flex,
                        vbox({
                            text(" "),
                            state_copy.is_logged_in ? (text("") | size(WIDTH, EQUAL, 3) | color(ui::GetColor(ui::MColor::SUCESS))) : text("") | size(WIDTH, EQUAL, 3) |  color(ui::GetColor(ui::MColor::ERROR)),
                            text(" ")
                        }) | align_right
                    }),

                    separator() | color(ui::GetColor(ui::MColor::SEPARATOR)),

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

    screen.Loop(ui);
}
