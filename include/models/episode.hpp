#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct EpisodeRef {
    std::string id;
    std::string title;
    std::string podcastId;

    static EpisodeRef from_json(const nlohmann::json& j) {
        EpisodeRef e;

        e.id = j.value("videoId", "");
        e.title = j.value("title", "");

        if (j.contains("podcast") && j["podcast"].is_object()) {
            e.podcastId = j["podcast"].value("id", "");
        } else {
            e.podcastId = "";
        }

        return e;
    }
};

}