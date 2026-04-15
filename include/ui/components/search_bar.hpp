#pragma once

#include <ftxui/component/component.hpp>
#include <string>

namespace ui {

struct SearchBarData {
    std::string value;
};

ftxui::Component SearchBar(SearchBarData* data);

}
