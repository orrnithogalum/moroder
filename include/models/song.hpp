#pragma once

#include "artist.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace music {

struct SongRef {
    std::string id;
    std::string title;
    std::string views;
    std::string thumbnail;
    std::vector<ArtistRef> artists;

    static SongRef from_json(const nlohmann::json& j) {
        SongRef s;

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

struct Song {
    SongRef ref;

    int duration_seconds = 0;
    std::string view_count;

    std::string publish_date;
    std::string upload_date;

    static Song from_json(const nlohmann::json& j) {
        Song s;

        if (j.contains("videoDetails")) {
            const auto& vd = j["videoDetails"];

            s.ref.id = vd.value("videoId", "");
            s.ref.title = vd.value("title", "");

            if (vd.contains("thumbnail") &&
                vd["thumbnail"].contains("thumbnails") &&
                vd["thumbnail"]["thumbnails"].is_array() &&
                !vd["thumbnail"]["thumbnails"].empty()) {

                s.ref.thumbnail =
                    vd["thumbnail"]["thumbnails"].back().value("url", "");
            }

            s.duration_seconds = std::stoi(vd.value("lengthSeconds", "0"));
            s.view_count = vd.value("viewCount", "");
        }

        if (j.contains("microformat") &&
            j["microformat"].contains("microformatDataRenderer")) {

            const auto& mf = j["microformat"]["microformatDataRenderer"];

            s.publish_date = mf.value("publishDate", "");
            s.upload_date = mf.value("uploadDate", "");
        }

        return s;
    }
};

}