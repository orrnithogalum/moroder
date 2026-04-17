#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <memory>

#include "../include/ui/components/search_bar.hpp"
#include "../include/ui/components/carousel.hpp"
#include "../include/ui/components/sidebar.hpp"
#include "../include/ui/components/content.hpp"
#include "../include/ui/components/grid.hpp"

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

    player.setOnRequestCompletedCallback([&]() {
        screen.PostEvent(Event::Custom);
    });

    ftxui::setOnImageLoadedCallback([&](){
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

    std::deque<ui::ContentEntry> main_content_items;
    std::vector<Component> main_content_components = { Renderer([]{ return emptyElement(); }) };

    auto main_content = Container::Vertical(main_content_components);

    auto layout = Container::Horizontal({
        sidebar,
        Renderer([] {
            return vbox({
                separator() | color(Color::RGB(100, 100, 100)),
                text(" ")
            });
        }),
        Container::Vertical({
            search_bar,
            main_content
        }),
    });

    search_bar_data.onSearch = [&player, &main_content, &screen](const std::string& value) {
        spdlog::info("SEARCHBAR: enter pressed with value, " + value);

        main_content->DetachAllChildren();
        screen.PostEvent(Event::Custom);
    };

    auto ui = Renderer(layout, [&] {
        sidebar_focused = sidebar->Focused();

        services::Player::PlayerState state_copy;
        {
            std::lock_guard lock(player.state_mutex);
            state_copy = player.state;
        }

        spinner_frame++;
        ui::getSidebarData(&state_copy, &sidebar_data);

        for (auto& [category, _] : state_copy.home) {
            const std::string& category_ref = category;

            auto already_exists = std::any_of(
                main_content_items.begin(), main_content_items.end(),
                [&](const ui::ContentEntry& item) {
                    return item.category == category_ref;
                }
            );

            if (!already_exists) {
                main_content_items.push_back(ui::ContentEntry{});
                ui::ContentEntry& item = main_content_items.back();

                item.category = category;
                item.selected = 0;
                item.focused = false;

                if(utils::lower(category) == "quick picks") {
                    ui::getGridData(&state_copy, category, &item.grid_data);
                    item.component = ui::Grid(&item.grid_data, &item.selected, &item.focused, 4);
                } else {
                    item.component = ui::Carousel(&item.data, &item.selected, &item.focused);
                }

                main_content_components.push_back(item.component);
                main_content->Add(item.component);
            }
        }

        for (auto& item : main_content_items) {
            ui::getCarouselData(&state_copy, item.category, &item.data);
            item.focused = item.component->Focused();
        }

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
            }) | size(WIDTH, EQUAL, 30),

            text(" "),
            separator() | color(Color::RGB(100, 100, 100)),
            text(" "),

            vbox({
                hbox({
                    search_bar->Render() | flex,
                    vbox({
                        text(" "),
                        state_copy.is_logged_in ? (text("") | size(WIDTH, EQUAL, 3) | color(Color::Green1)) : text("") | size(WIDTH, EQUAL, 3) | color(Color::Red1),
                        text(" ")
                    }) | align_right
                }),

                separator() | color(Color::RGB(100, 100, 100)),

                hbox({
                    text(" "),
                    main_content->Render() | yframe | yflex
                }) | yflex,

                ((main_content_items.empty() || sidebar_data.is_loading) && state_copy.is_logged_in) ||
                state_copy.loading["search"] == services::Player::LoadingState::Loading ?
                    vbox({
                        filler(),
                        hbox({
                            filler(),
                            spinner(15, spinner_frame)
                        })
                    }) | flex :
                    emptyElement(),

            }) | flex
        });
    });

    screen.Loop(ui);
}
