/* ARTIST
- The artist object for a ytmusicapi search result of type "artist"
- Only includes information that is present in all search results of type "artist"
*/

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct ArtistRef {
    std::string id;
    std::string name;

    std::string thumbnail_large;
    std::string thumbnail_small;

    static ArtistRef from_json(const nlohmann::json& j) {
        ArtistRef a;

        if (j.contains("artists") && j["artists"].is_array() && !j["artists"].empty()) {
            const auto& first_artist = j["artists"].front();
            a.id = first_artist.value("id", "");
            a.name = first_artist.value("name", "");
        } else {
            a.id = j.value("browseId", "");
            a.name = j.value("artist", "");
        }

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
