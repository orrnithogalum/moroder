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

struct QueueEntry {
    std::string id;
    std::string top;
    std::string bottom;
    std::string url;

    bool is_divider = false;
    std::string divider_label;
};

void buildHome(services::Player::PlayerState* state, std::deque<ContentEntry>& items, ftxui::Component main_container, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_press);
void buildSearch(services::Player::PlayerState* state, std::deque<ContentEntry>& items, ftxui::Component main_container, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_press);
void buildQueue(services::Player::PlayerState* state, std::deque<ContentEntry>& items, ftxui::Component main_container);

}
