/* SEARCH RESULT
- The object that encapsulates all search result types of ytmusicapi
- Result data can only be of a single type in music::<Type>
*/

#pragma once

#include "playlist.hpp"
#include "episode.hpp"
#include "podcast.hpp"
#include "artist.hpp"
#include "album.hpp"
#include "song.hpp"

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

struct SearchResult {
    std::string category;
    std::string resultType;
    ResultData data;

    static SearchResult from_json(const nlohmann::json& j) {
        SearchResult res;

        res.category = j.contains("category") && j["category"].is_string() ? j["category"].get<std::string>() : "";
        res.resultType = j.contains("resultType") && j["resultType"].is_string() ? j["resultType"].get<std::string>() : "";

        if (res.resultType == "song" || res.resultType == "video") {
            res.data = music::SongRef::from_json(j);

        } else if (res.resultType == "album") {
            res.data = music::AlbumRef::from_json(j);

        } else if (res.resultType == "artist") {
            res.data = music::ArtistRef::from_json(j);

        } else if (res.resultType == "playlist") {
            res.data = music::PlaylistRef::from_json(j);

        } else if (res.resultType == "episode") {
            res.data = music::EpisodeRef::from_json(j);

        } else if(res.resultType == "podcast") {
            res.data = music::PodcastRef::from_json(j);

        } else {
            res.data = std::monostate{};
        }

        return res;
    }
};

}
