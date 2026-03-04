#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct PodcastRef {
    std::string id;
    std::string name;

    static PodcastRef from_json(const nlohmann::json& j) {
        PodcastRef a;
        a.id = j.value("browseId", "");
        a.name = j.value("artist", "");
        return a;
    }
};

}