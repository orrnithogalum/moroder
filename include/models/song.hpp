/* SONG REF & SONG
- The song object for a ytmusicapi search result of type "song" or "video" (same thing)
- SongRef only includes information that is present in all search results of type "song" or "video"
- Song is the full song object with all details, only reliably obtainable after fetching song info from ytmusicapi
*/

#pragma once

#include "artist.hpp"

#include <cstdint>
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

    uint64_t duration = 0;

    std::string album_title;

    static Song from_json(const nlohmann::json& j) {
        Song s;

        s.album_title = j.value("album", "");

        return s;
    }
};

}