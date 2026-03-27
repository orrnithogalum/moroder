/* PODCAST
- The podcast object for a ytmusicapi search result of type "podcast"
- Only includes information that is present in all search results of type "podcast"
*/

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct PodcastRef {
    std::string id;
    std::string name;

    std::string thumbnail_large;
    std::string thumbnail_small;

    static PodcastRef from_json(const nlohmann::json& j) {
        PodcastRef a;
        a.id = j.value("browseId", "");
        a.name = j.value("title", "");

        if (j.contains("thumbnails") && j["thumbnails"].is_array() && !j["thumbnails"].empty()) {
            a.thumbnail_large = j["thumbnails"].back().value("url", "");
            a.thumbnail_small = j["thumbnails"].front().value("url", "");
        } else {
            a.thumbnail_large = "";
            a.thumbnail_small = "";
        }

        return a;
    }
};

}
