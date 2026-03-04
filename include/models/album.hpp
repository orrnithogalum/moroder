#pragma once

#include "artist.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace music {

struct Album {
    std::string id;
    std::string title;
    std::string year;
    std::string thumbnail;
    std::vector<Artist> artists;

    static Album from_json(const nlohmann::json& j) {
        Album a;
        a.id = j.value("browseId", "");
        a.title = j.value("title", "");
        a.year = j.value("year", "");

        if (j.contains("artists") && j["artists"].is_array()) {
            for (const auto& artist : j["artists"]) {
                Artist ref;

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