#include "../../../include/ui/components/sidebar.hpp"

#include "../../../include/ui/constants/colors.hpp"
#include "../../../include/ui/structs/entries.hpp"
#include "spdlog/spdlog.h"

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
    };

    data->entries_spoof = {
        "01",
        "02",
    };

    if(state->is_logged_in) {
        data->entries.push_back({"  New playlist", ""});
        data->entries_spoof.push_back("03");
    }

    for (auto& playlist : state->library_playlists) {
        data->results.emplace_back(playlist);
        data->entries_spoof.emplace_back(playlist.ref.id);
        data->entries.emplace_back(ui::SimpleEntry{ playlist.ref.title, playlist.ref.author });
    }

    data->is_loading = state->flags["library_playlists"] == services::Player::Flags::Ongoing;
}

Component ui::Sidebar(ui::SidebarData* data, int* selected, bool* focused) {
    MenuOption options;

    options.entries_option.animated_colors.foreground = AnimatedColorOption {
        .inactive = ui::GetColor(ui::MColor::WHITE),
        .active = ui::GetColor(ui::MColor::DEFAULT)
    };

    options.entries_option.transform = [data, focused](const EntryState& entry_state) {
        const auto& item = data->entries[entry_state.index];

        if(entry_state.index == 2) {
            return vbox({
                separator() | color(ui::GetColor(ui::MColor::SEPARATOR_PRIMARY)),
                hbox({
                    text("  "),
                    vbox({
                        text(""),
                        text(item.top) | (entry_state.active && *focused ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),
                        text(""),
                    })
                })
            }) | (entry_state.active && *focused ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY)));
        }

        if(entry_state.index > 2) {
            return hbox({
                text("  "),
                vbox({
                    text(""),
                    text(item.top) | (entry_state.active && *focused ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),
                    text(item.bottom) | (entry_state.active && *focused ? color(ui::GetColor(ui::MColor::TEXT_BOTTOM_PRIMARY)) | bold : color(ui::GetColor(ui::MColor::TEXT_BOTTOM_SECONDARY))),
                })
            });
        }

        return hbox({
            text("  "),
            vbox({
                text(item.top) | (entry_state.active && *focused ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY))),
                text("")
            }) | (entry_state.active && *focused ? color(ui::GetColor(ui::MColor::TEXT_TOP_PRIMARY)) | bold : color(ui::GetColor(ui::MColor::TEXT_TOP_SECONDARY)))
        });
    };

    options.on_enter = [data, selected] {
        if(data->entries_spoof[*selected] == "01") {
            data->onHome();

        } else if (*selected > 2) {
            spdlog::info("SIDEBAR: pressed enter on playlist, " + data->results[*selected].ref.title);
        }
    };

    return Menu(&data->entries_spoof, selected, options);
}
