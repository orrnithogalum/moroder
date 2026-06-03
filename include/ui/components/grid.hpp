#pragma once

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

#include "../../../include/services/player.hpp"
#include "../structs/entries.hpp"

namespace ui {

enum class GridStyle {
    List,
    Tile
};

constexpr int GRID_TILE_COVER_WIDTH = 20;
constexpr int GRID_TILE_COVER_HEIGHT = 10;
constexpr int GRID_TILE_WIDTH = 23;
constexpr int GRID_TILE_HEIGHT = 14;

struct GridData {
    std::string category_name;

    std::vector<music::ApiResult> results;
    std::vector<std::string> entries_spoof;
    std::vector<ImageEntry> entries;

    bool is_loading = true;
};

void getGridData(services::Player::PlayerState* state, std::string category, GridData* data);
void getLibraryGridData(services::Player::PlayerState* state, const std::string& filter, GridData* data);

ftxui::Component Grid(GridData* data, int* selected, bool* focused, int rows, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_press, GridStyle style = GridStyle::List);
}
