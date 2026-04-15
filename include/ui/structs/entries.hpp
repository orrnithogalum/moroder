#pragma once

#include <string>

namespace ui {

struct SimpleEntry {
    std::string top;
    std::string bottom;
};

struct ImageEntry {
    std::string image_url;

    std::string top;
    std::string bottom;
};

}
