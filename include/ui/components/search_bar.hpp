#pragma once

#include <ftxui/component/component.hpp>
#include <string>

namespace ui {

struct SearchBarData {
    std::string value;
    std::function<void(const std::string&)> onSearch;
};

ftxui::Component SearchBar(SearchBarData* data);

}
