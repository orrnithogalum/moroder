#pragma once

#include "carousel.hpp"
#include "grid.hpp"

namespace ui {

struct HomeEntry {
    ui::GridData grid_data;
    ui::CarouselData data;
    std::string category;

    int selected = 0;
    bool focused = false;

    ftxui::Component component;
};

}
