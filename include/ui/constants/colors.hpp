#pragma once

#include <ftxui/dom/elements.hpp>

namespace ui {

enum class MColor {
    DEFAULT,
    WHITE,
    BLACK,

    ACCENT_PRIMARY,

    TEXT_TOP_PRIMARY,
    TEXT_TOP_SECONDARY,
    TEXT_BOTTOM_PRIMARY,
    TEXT_BOTTOM_SECONDARY,

    SEPARATOR_PRIMARY,
    SEPARATOR_SECONDARY,

    SUCCESS,
    ERROR,
};

ftxui::Color GetColor(MColor id);

}
