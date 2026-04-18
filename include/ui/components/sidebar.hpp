#pragma once

#include "../../../include/services/player.hpp"
#include "../structs/entries.hpp"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/component_base.hpp>
#include <vector>

namespace ui {

struct SidebarData {
    std::vector<std::string> entries_spoof;
    std::vector<ui::SimpleEntry> entries;

    bool is_loading = true;

    std::function<void()> onHome;
};

void getSidebarData(services::Player::PlayerState* state, SidebarData* data);
ftxui::Component Sidebar(SidebarData* data, int* selected, bool* focused);

}
