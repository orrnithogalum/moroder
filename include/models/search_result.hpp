#pragma once

#include "playlist.hpp"
#include "episode.hpp"
#include "artist.hpp"
#include "album.hpp"
#include "song.hpp"

#include <nlohmann/json.hpp>
#include <variant>
#include <string>

namespace ipc {

using ResultData = std::variant<
    std::monostate,
    music::Song,
    music::Album,
    music::Artist,
    music::Playlist,
    music::Episode
>;

struct SearchResult {
    std::string category;
    std::string resultType;
    ResultData data;

    static SearchResult from_json(const nlohmann::json& j) {
        SearchResult res;

        res.category = j.contains("category") && j["category"].is_string() ? j["category"].get<std::string>() : "";
        res.resultType = j.contains("resultType") && j["resultType"].is_string() ? j["resultType"].get<std::string>() : "";

        if (res.resultType == "song" || res.resultType == "video") {
            res.data = music::Song::from_json(j);

        } else if (res.resultType == "album") {
            res.data = music::Album::from_json(j);

        } else if (res.resultType == "artist" || res.resultType == "podcast") {
            res.data = music::Artist::from_json(j);

        } else if (res.resultType == "playlist") {
            res.data = music::Playlist::from_json(j);

        } else if (res.resultType == "episode") {
            res.data = music::Episode::from_json(j);

        } else {
            res.data = std::monostate{};
        }

        return res;
    }
};

}