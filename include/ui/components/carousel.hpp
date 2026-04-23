#pragma once

#include "../../../include/services/player.hpp"
#include "../structs/entries.hpp"

#include <ftxui/component/component.hpp>
#include <string>
#include <vector>

namespace ui {

struct CarouselData {
    std::string category_name;

    std::vector<music::ApiResult> results;
    std::vector<std::string> entries_spoof;
    std::vector<ImageEntry> entries;

    bool is_loading = true;

};

void getCarouselData(services::Player::PlayerState* state, std::string category, CarouselData* data);
ftxui::Component Carousel(CarouselData* data, int* selected, bool* focused, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_press);

}
