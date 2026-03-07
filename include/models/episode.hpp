/* EPISODE
- The episode object for a ytmusicapi search result of type "episode"
- Only includes information that is present in all search results of type "episode"
- Only used for podcasts (I think)
*/

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