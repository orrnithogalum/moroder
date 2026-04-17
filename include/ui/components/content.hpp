#pragma once

#include "carousel.hpp"
#include "grid.hpp"

namespace ui {

struct ContentEntry {
    ui::GridData grid_data;
    ui::CarouselData data;
    std::string category;

    int selected = 0;
    bool focused = false;

    ftxui::Component component;
};

void buildHome(services::Player::PlayerState* state, std::deque<ContentEntry>& items, std::vector<ftxui::Component>& components, ftxui::Component main_container);

}
