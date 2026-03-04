#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace music {

struct Playlist {
    std::string id;
    std::string title;
    std::string author;
    std::string thumbnail;

    static Playlist from_json(const nlohmann::json& j) {
        Playlist p;

        // id: can be browseId or playlistId
        if (j.contains("browseId") && j["browseId"].is_string()) {
            p.id = j["browseId"].get<std::string>();
        } else if (j.contains("playlistId") && j["playlistId"].is_string()) {
            p.id = j["playlistId"].get<std::string>();
        } else {
            p.id = "";
        }

        p.title = j.value("title", "");

        // author can be string / array
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