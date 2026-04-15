#pragma once

#include "../structs/entries.hpp"

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

namespace ui {

struct CarouselData {
    std::vector<ImageEntry> entries;
    std::vector<std::string> entries_spoof;

    bool is_loading = false;
};

ftxui::Component Carousel(CarouselData* data, int* selected, bool* focused);

}
