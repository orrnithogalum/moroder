#include "../../../include/ui/constants/colors.hpp"
#include "../../../include/config/config.hpp"

#include <spdlog/spdlog.h>

#include <array>

namespace {

/* toColor
- Config::parseColor returns {-1, -1, -1} for anything it could not read, so a
  bad entry keeps the built-in colour instead of turning an element black
*/
ftxui::Color toColor(const std::array<int, 3>& rgb, ftxui::Color fallback, const char* key) {
    if (rgb[0] < 0 || rgb[1] < 0 || rgb[2] < 0) {
        spdlog::warn("COLORS: {} is not a valid {{r, g, b}} colour, keeping the built-in", key);
        return fallback;
    }

    return ftxui::Color::RGB(rgb[0], rgb[1], rgb[2]);
}

/* Palette
- GetColor runs inside every transform lambda, so it is called many times per
  frame; the config is read once here instead
*/
struct Palette {
    ftxui::Color accent_primary        = ftxui::Color::Red1;
    ftxui::Color text_top_primary      = ftxui::Color::RGB(255, 255, 255);
    ftxui::Color text_top_secondary    = ftxui::Color::RGB(170, 170, 170);
    ftxui::Color text_bottom_primary   = ftxui::Color::RGB(170, 170, 170);
    ftxui::Color text_bottom_secondary = ftxui::Color::RGB( 70,  70,  70);
    ftxui::Color separator_primary     = ftxui::Color::RGB(100, 100, 100);
    ftxui::Color separator_secondary   = ftxui::Color::RGB( 50,  50,  50);
};

const Palette& palette() {
    static const Palette loaded = [] {
        Palette out;

        try {
            const Config& cfg = Config::get();

            out.accent_primary        = toColor(cfg.COLOR_ACCENT_PRIMARY,        out.accent_primary,        "COLOR_ACCENT_PRIMARY");
            out.text_top_primary      = toColor(cfg.COLOR_TEXT_TOP_PRIMARY,      out.text_top_primary,      "COLOR_TEXT_TOP_PRIMARY");
            out.text_top_secondary    = toColor(cfg.COLOR_TEXT_TOP_SECONDARY,    out.text_top_secondary,    "COLOR_TEXT_TOP_SECONDARY");
            out.text_bottom_primary   = toColor(cfg.COLOR_TEXT_BOTTOM_PRIMARY,   out.text_bottom_primary,   "COLOR_TEXT_BOTTOM_PRIMARY");
            out.text_bottom_secondary = toColor(cfg.COLOR_TEXT_BOTTOM_SECONDARY, out.text_bottom_secondary, "COLOR_TEXT_BOTTOM_SECONDARY");
            out.separator_primary     = toColor(cfg.COLOR_SEPARATOR_PRIMARY,     out.separator_primary,     "COLOR_SEPARATOR_PRIMARY");
            out.separator_secondary   = toColor(cfg.COLOR_SEPARATOR_SECONDARY,   out.separator_secondary,   "COLOR_SEPARATOR_SECONDARY");

        } catch (const std::exception& e) {
            // An unreadable config must not take the interface with it.
            spdlog::warn("COLORS: could not read the config, using built-in colours, {}", e.what());
        }

        return out;
    }();

    return loaded;
}

}

namespace ui {

ftxui::Color GetColor(MColor id) {
    using namespace ftxui;

    switch (id) {
        case MColor::DEFAULT:               return Color::Default;
        case MColor::WHITE:                 return Color::RGB(255, 255, 255);
        case MColor::BLACK:                 return Color::RGB(  0,   0,   0);

        case MColor::ACCENT_PRIMARY:        return palette().accent_primary;

        case MColor::TEXT_TOP_PRIMARY:      return palette().text_top_primary;
        case MColor::TEXT_TOP_SECONDARY:    return palette().text_top_secondary;
        case MColor::TEXT_BOTTOM_PRIMARY:   return palette().text_bottom_primary;
        case MColor::TEXT_BOTTOM_SECONDARY: return palette().text_bottom_secondary;

        case MColor::SEPARATOR_PRIMARY:     return palette().separator_primary;
        case MColor::SEPARATOR_SECONDARY:   return palette().separator_secondary;

        case MColor::SUCCESS:               return Color::Green1;
        case MColor::ERROR:                 return Color::Red1;
    }

    return Color::Default;
}

}
