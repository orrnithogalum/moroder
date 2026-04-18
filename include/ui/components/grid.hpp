#pragma once

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

#include "../../../include/services/player.hpp"
#include "../structs/entries.hpp"

namespace ui {

struct GridData {
    std::string category_name;

    std::vector<music::ApiResult> results;
    std::vector<std::string> entries_spoof;
    std::vector<ImageEntry> entries;

    bool is_loading = true;
};

void getGridData(services::Player::PlayerState* state, std::string category, GridData* data);
ftxui::Component Grid(GridData* data, int* selected, bool* focused, int rows);

}
