/* SEARCH RESULT
- The object that encapsulates all search and home result types of ytmusicapi
- Result data can only be of a single type in music::<Type>
*/

#pragma once

#include "../utils/utils.hpp"
#include "playlist.hpp"
#include "episode.hpp"
#include "podcast.hpp"
#include "artist.hpp"
#include "album.hpp"
#include "song.hpp"
#include "spdlog/spdlog.h"

#include <nlohmann/json.hpp>
#include <variant>
#include <string>

namespace music {

using ResultData = std::variant<
    std::monostate,
    music::SongRef,
    music::AlbumRef,
    music::ArtistRef,
    music::PlaylistRef,
    music::EpisodeRef,
    music::PodcastRef
>;

struct ApiResult {
    std::string category;
    std::string resultType;
    ResultData data;

    static ApiResult from_json(const nlohmann::json& j) {
        ApiResult res;

        res.category = j.contains("category") && j["category"].is_string() ? j["category"].get<std::string>() : "";

        res.resultType = (j.contains("resultType") && j["resultType"].is_string())
            ? j["resultType"].get<std::string>()
            : (j.contains("type") && j["type"].is_string())
            ? j["type"].get<std::string>()
            : "";

        if(res.resultType.empty() && j.contains("videoId")) {
            res.resultType = "song";

        } else if (res.resultType.empty() && j.contains("playlistId")) {
            res.resultType = "playlist";

        } else if (res.resultType.empty() && j.contains("subscribers")) {
            res.resultType = "artist";

        } else if (res.resultType.empty() && j.contains("podcastId")) {
            res.resultType = "podcast";
        }

        res.category = utils::lower(res.category);
        res.resultType = utils::lower(res.resultType);

        if (res.resultType == "song" || res.resultType == "video" || res.resultType == "single") {
            spdlog::info("APIRESULT: parsing a song / video");
            res.data = music::SongRef::from_json(j);

        } else if (res.resultType == "album" || res.resultType == "ep"){
            spdlog::info("APIRESULT: parsing an album");
            res.data = music::AlbumRef::from_json(j);

        } else if (res.resultType == "artist") {
            spdlog::info("APIRESULT: parsing an artist");
            res.data = music::ArtistRef::from_json(j);

        } else if (res.resultType == "playlist") {
            spdlog::info("APIRESULT: parsing a playlist");
            res.data = music::PlaylistRef::from_json(j);

        } else if (res.resultType == "episode") {
            spdlog::info("APIRESULT: parsing an episode");
            res.data = music::EpisodeRef::from_json(j);

        } else if(res.resultType == "podcast") {
            spdlog::info("APIRESULT: parsing a podcast");
            res.data = music::PodcastRef::from_json(j);

        } else {
            spdlog::warn("APIRESULT: Could not parse item of type, " + res.resultType);
            res.data = std::monostate{};
        }

        return res;
    }
};

}
