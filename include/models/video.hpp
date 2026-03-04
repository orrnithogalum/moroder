#pragma once

#include "artist.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace music {

struct VideoRef {
    std::string id;
    std::string title;
    std::string views;
    std::string thumbnail;
    std::vector<ArtistRef> artists;

    static VideoRef from_json(const nlohmann::json& j) {
        VideoRef s;

        s.id = j.value("videoId", "");
        s.title = j.value("title", "");
        s.views = j.value("views", "");

        if (j.contains("artists") && j["artists"].is_array()) {
            for (const auto& a : j["artists"]) {
                ArtistRef ref;

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