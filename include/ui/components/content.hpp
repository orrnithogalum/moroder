#pragma once

#include "carousel.hpp"
#include "chips.hpp"
#include "grid.hpp"

namespace ui {

struct ContentEntry {
    std::string category;

    ui::CarouselData carousel_data;
    ui::GridData grid_data;
    ui::ChipsData chips_data;

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
void buildQueue(services::Player::PlayerState* state, std::deque<ContentEntry>& items, ftxui::Component main_container, std::function<bool(const ftxui::Event&, const int)> on_queue_press);
void buildLibrary(services::Player::PlayerState* state, std::deque<ContentEntry>& main_content_items, ftxui::Component main_content, int rows, std::function<bool(const ftxui::Event&, const ui::ChipEntry&)> on_chip_press, std::function<bool(const ftxui::Event&, const music::ApiResult&)> on_item_press);

}
