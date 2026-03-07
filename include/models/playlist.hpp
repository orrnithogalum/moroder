/* PLAYLIST
- The playlist object for a ytmusicapi search result of type "playlist"
- Only includes information that is present in all search results of type "playlist"
*/

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct PlaylistRef {
    std::string id;
    std::string title;
    std::string author;
    std::string thumbnail;

    static PlaylistRef from_json(const nlohmann::json& j) {
        PlaylistRef p;

        // id: can be browseId or playlistId (for some reason)
        if (j.contains("browseId") && j["browseId"].is_string()) {
            p.id = j["browseId"].get<std::string>();
        } else if (j.contains("playlistId") && j["playlistId"].is_string()) {
            p.id = j["playlistId"].get<std::string>();
        } else {
            p.id = "";
        }

        p.title = j.value("title", "");

        // author can be string / array if there are multiple artists.
        if (j.contains("author")) {
            const auto& a = j["author"];

            if (a.is_string()) {
                p.author = a.get<std::string>();
            }
            else if (a.is_array() && !a.empty()) {
                if (a[0].contains("name") && a[0]["name"].is_string()) {
                    p.author = a[0]["name"].get<std::string>();
                } else {
                    p.author = "";
                }
            }
            else {
                p.author = "";
            }
        } else {
            p.author = "";
        }

        if (j.contains("thumbnails") && j["thumbnails"].is_array() && !j["thumbnails"].empty()) {
            p.thumbnail = j["thumbnails"].back().value("url", "");
        } else {
            p.thumbnail = "";
        }

        return p;
    }
};

}