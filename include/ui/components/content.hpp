#pragma once

#include "carousel.hpp"
#include "grid.hpp"

namespace ui {

struct ContentEntry {
    std::string category;

    ui::GridData grid_data;
    ui::CarouselData data;

    int selected = 0;
    bool focused = false;

    ftxui::Component component;
    music::ApiResult result;
};

void buildHome(services::Player::PlayerState* state, std::deque<ContentEntry>& items, ftxui::Component main_container);
void buildSearch(services::Player::PlayerState* state, std::deque<ContentEntry>& items, ftxui::Component main_container);

}
