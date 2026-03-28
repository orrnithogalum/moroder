/* EPISODE
- The episode object for a ytmusicapi search result of type "episode"
- Only includes information that is present in all search results of type "episode"
- Only used for podcasts (I think)
*/

#pragma once

#include "../interfaces/streamable.hpp"
#include "podcast.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct EpisodeRef {
    std::string id;
    std::string title;

    std::string thumbnail_large;
    std::string thumbnail_small;

    music::PodcastRef podcast;

    static EpisodeRef from_json(const nlohmann::json& j) {
        EpisodeRef e;

        e.id = j.value("videoId", "");
        e.title = j.value("title", "");

        if (j.contains("podcast") && j["podcast"].is_object()) {
            e.podcast.id = j["podcast"].value("id", "");
            e.podcast.name = j["podcast"].value("name", "");
        } else {
            e.podcast.id = "";
            e.podcast.name = "";
        }

        if (j.contains("thumbnails") && j["thumbnails"].is_array() && !j["thumbnails"].empty()) {
            e.thumbnail_large = j["thumbnails"].back().value("url", "");
            e.thumbnail_small = j["thumbnails"].front().value("url", "");
        } else {
            e.thumbnail_large = "";
            e.thumbnail_small = "";
        }

        return e;
    }
};

struct Episode : IStreamable {
    uint64_t duration;
    EpisodeRef ref;

    std::string url;

    static Episode from_json(const nlohmann::json& j) {
        Episode e;

        e.duration = 0;
        e.ref = EpisodeRef::from_json(j);
        e.url = "https://www.youtube.com/watch?v=" + e.ref.id;

        return e;
    }

    std::string getStreamUrl() const override {
        return this->url;
    }
};

}
