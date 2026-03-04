#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct Artist {
    std::string id;
    std::string name;

    static Artist from_json(const nlohmann::json& j) {
        Artist a;
        a.id = j.value("browseId", "");
        a.name = j.value("artist", "");
        return a;
    }
};

}