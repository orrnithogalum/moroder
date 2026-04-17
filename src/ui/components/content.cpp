#include "../../../include/ui/components/content.hpp"


void ui::buildHome(services::Player::PlayerState* state, std::deque<ContentEntry>& main_content_items, std::vector<ftxui::Component>& main_content_components, ftxui::Component main_content) {
    for (auto& [category, _] : state->home) {
        const std::string& category_ref = category;

        auto already_exists = std::any_of(
            main_content_items.begin(), main_content_items.end(),
            [&](const ContentEntry& item) {
                return item.category == category_ref;
            }
        );

        if (!already_exists) {
            main_content_items.push_back(ContentEntry{});
            ContentEntry& item = main_content_items.back();

            item.category = category;
            item.selected = 0;
            item.focused = false;

            if (utils::lower(category) == "quick picks") {
                ui::getGridData(state, category, &item.grid_data);
                item.component = Grid(&item.grid_data, &item.selected, &item.focused, 4);
            } else {
                item.component = Carousel(&item.data, &item.selected, &item.focused);
            }

            main_content_components.push_back(item.component);
            main_content->Add(item.component);
        }
    }

    for (auto& item : main_content_items) {
        ui::getCarouselData(state, item.category, &item.data);
        item.focused = item.component->Focused();
    }
}
