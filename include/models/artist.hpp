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

    static ArtistRef from_json(const nlohmann::json& j) {
        ArtistRef a;
        a.id = j.value("browseId", "");
        a.name = j.value("artist", "");
        return a;
    }
};

}