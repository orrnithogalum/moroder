#include "../../../include/ui/constants/colors.hpp"

namespace ui {

ftxui::Color GetColor(MColor id) {
    using namespace ftxui;

    switch (id) {
        case MColor::DEFAULT:               return Color::Default;
        case MColor::WHITE:                 return Color::RGB(255, 255, 255);
        case MColor::BLACK:                 return Color::RGB(  0,   0,   0);

        case MColor::ACCENT_PRIMARY:        return Color::Red1;

        case MColor::TEXT_TOP_PRIMARY:      return Color::RGB(255, 255, 255);
        case MColor::TEXT_TOP_SECONDARY:    return Color::RGB(170, 170, 170);
        case MColor::TEXT_BOTTOM_PRIMARY:   return Color::RGB(170, 170, 170);
        case MColor::TEXT_BOTTOM_SECONDARY: return Color::RGB( 70,  70,  70);

        case MColor::SEPARATOR_PRIMARY:     return Color::RGB(100, 100, 100);
        case MColor::SEPARATOR_SECONDARY:   return Color::RGB( 50,  50,  50);

        case MColor::SUCESS:                return Color::Green1;
        case MColor::ERROR:                 return Color::Red1;
    }

    return Color::Default;
}

}
