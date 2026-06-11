#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include <functional>
#include <string>
#include <vector>

namespace ui {

struct ChipEntry {
    std::string id;
    std::string label;

    bool enabled = false;
};

struct ChipsData {
    std::vector<ChipEntry> entries;
    std::vector<std::string> entries_spoof;
    bool exclusive = false;
};

void setChipsData(ChipsData* data, const std::vector<ChipEntry>& entries);
void toggleChip(ChipsData *data, const std::string &id);

ftxui::Component Chips(ChipsData* data, int* selected, bool* focused, std::function<bool(const ftxui::Event&, const ChipEntry&)> on_press);

}
