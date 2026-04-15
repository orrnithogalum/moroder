#include "../../../include/ui/components/sidebar.hpp"

#include "../../../include/ui/structs/entries.hpp"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>

using namespace ftxui;

void ui::getSidebarData(services::Player::PlayerState* state, ui::SidebarData* data) {
    data->entries.clear();
    data->entries_spoof.clear();

    data->entries = {
        {"  Home", ""},
        {"  Library", ""},
        {"  New playlist", ""},
    };

    data->entries_spoof = {
        "01",
        "02",
        "03",
    };

    for (auto& playlist : state->library_playlists) {
        data->entries_spoof.emplace_back(playlist.ref.id);
        data->entries.emplace_back(ui::SimpleEntry{ playlist.ref.title, playlist.ref.author });
    }

    data->is_loading = state->loading["library_playlists"] == services::Player::LoadingState::Loading;
}

Component ui::Sidebar(ui::SidebarData* data, int* selected, bool* focused) {
    MenuOption options;

    options.entries_option.animated_colors.foreground = AnimatedColorOption {
        .inactive = Color::White,
        .active = Color::Default
    };

    options.entries_option.transform = [data, focused](const EntryState& entry_state) {
        const auto& item = data->entries[entry_state.index];

        if(entry_state.index == 2) {
            return vbox({
                separator() | color(Color::RGB(100, 100, 100)),
                hbox({
                    text("  "),
                    vbox({
                        text(""),
                        text(item.top) | (entry_state.active && *focused ? color(Color::White) | bold : color(Color::RGB(170, 170, 170))),
                        text(""),
                    })
                })
            }) | (entry_state.active && *focused ? color(Color::White) | bold : color(Color::RGB(170, 170, 170)));
        }

        if(entry_state.index > 2) {
            return hbox({
                text("  "),
                vbox({
                    text(""),
                    text(item.top) | (entry_state.active && *focused ? color(Color::White) | bold : color(Color::RGB(170, 170, 170))),
                    text(item.bottom) | (entry_state.active && *focused ? color(Color::RGB(170, 170, 170)) | bold : color(Color::RGB(70, 70, 70))),
                })
            });
        }

        return hbox({
            text("  "),
            vbox({
                text(item.top) | (entry_state.active && *focused ? color(Color::White) | bold : color(Color::RGB(170, 170, 170))),
                text("")
            }) | (entry_state.active && *focused ? color(Color::White) | bold : color(Color::RGB(170, 170, 170)))
        });
    };

    return Menu(&data->entries_spoof, selected, options);
}
