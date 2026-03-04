#pragma once

#include "spdlog/spdlog.h"
#include <nlohmann/json.hpp>

#include <string>

namespace ipc {

struct SearchResult {
    std::string category;
    std::string resultType;

    // Only one will be populated based on type
    // std::optional<Song> song;
    // std::optional<Video> video;
    // std::optional<Album> album;
    // ... Playlist, Episode, ArtistResult

    static SearchResult from_json(const nlohmann::json& j) {
        SearchResult res;

        res.category = j.contains("category") && j["category"].is_string() ? j["category"].get<std::string>() : "";
        res.resultType = j.contains("resultType") && j["resultType"].is_string() ? j["resultType"].get<std::string>() : "";

        if (res.resultType == "song") {
            // res.song = Song::from_json(j);
        } else if (res.resultType == "video") {
            // res.video = Video::from_json(j);
        } else if (res.resultType == "album") {
            // res.album = Album::from_json(j);
        } else {
            spdlog::warn("Encountered unkown result type: " + res.resultType);
        }

        return res;
    }
};

}