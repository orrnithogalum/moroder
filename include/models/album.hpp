/* ALBUM
- The album object for a ytmusicapi search result of type "album"
- Only includes information that is present in all search results of type "album"
*/

#pragma once

#include "artist.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace music {

struct AlbumRef {
    std::string id;
    std::string title;
    std::string year;
    std::string thumbnail;
    std::vector<ArtistRef> artists;

    static AlbumRef from_json(const nlohmann::json& j) {
        AlbumRef a;
        a.id = j.value("browseId", "");
        a.title = j.value("title", "");
        a.year = j.value("year", "");

        if (j.contains("artists") && j["artists"].is_array()) {
            for (const auto& artist : j["artists"]) {
                ArtistRef ref;

                ref.name = artist.value("name", "");

                if (artist.contains("id") && artist["id"].is_string()) {
                    ref.id = artist["id"].get<std::string>();
                } else {
                    ref.id = "";
                }

                a.artists.push_back(std::move(ref));
            }
        }

        if (j.contains("thumbnails") && j["thumbnails"].is_array() && !j["thumbnails"].empty()) {
            a.thumbnail = j["thumbnails"].back().value("url", "");
        } else {
            a.thumbnail = "";
        }

        return a;
    }
};

}