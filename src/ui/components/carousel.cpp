#include "../../../include/ui/components/carousel.hpp"
// #include "image_view.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

void ui::getCarouselData(services::Player::PlayerState* state, std::string category, CarouselData* data) {

}

ftxui::Component ui::Carousel(CarouselData* data, int* selected, bool* focused) {
    MenuOption options;
    options.direction = Direction::Right;
    options.Horizontal();

    options.entries_option.transform = [data, focused](const EntryState& state) {
        const auto& item = data->entries[state.index];

        return hbox({
            vbox({
                // image_view(item.image_url) | size(WIDTH, EQUAL, 20) | size(HEIGHT, EQUAL, 10) | borderStyled(BorderStyle::EMPTY),
                filler() | size(WIDTH, EQUAL, 20) | size(HEIGHT, EQUAL, 10) | borderStyled(BorderStyle::EMPTY),

                hbox({
                    text(" "),
                    vbox({
                        text(item.top) | (state.active && *focused ? color(Color::White) | bold : color(Color::RGB(170, 170, 170))),
                        text(item.bottom) | (state.active && *focused ? color(Color::RGB(170, 170, 170)) | bold : color(Color::RGB(70, 70, 70))),
                    }),
                }),
            }),
            text(" "),
        });
    };

    return Menu(&data->entries_spoof, selected, options);
}
