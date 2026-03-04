#pragma once

#include "artist.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace music {

struct Song {
    std::string id;
    std::string title;
    std::string views;
    std::string thumbnail;
    std::vector<Artist> artists;

    static Song from_json(const nlohmann::json& j) {
        Song s;

        s.id = j.value("videoId", "");
        s.title = j.value("title", "");
        s.views = j.value("views", "");

        if (j.contains("artists") && j["artists"].is_array()) {
            for (const auto& a : j["artists"]) {
                Artist ref;

                ref.name = a.value("name", "");

                if (a.contains("id") && a["id"].is_string()) {
                    ref.id = a["id"].get<std::string>();
                } else {
                    ref.id = "";
                }

                s.artists.push_back(std::move(ref));
            }
        }

        if (j.contains("thumbnails") && j["thumbnails"].is_array() && !j["thumbnails"].empty()) {
            s.thumbnail = j["thumbnails"].back().value("url", "");
        } else {
            s.thumbnail = "";
        }

        return s;
    }
};

}