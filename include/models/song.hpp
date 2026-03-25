/* SONG REF & SONG
- The song object for a ytmusicapi search result of type "song" or "video" (same thing)
- SongRef only includes information that is present in all search results of type "song" or "video"
- Song is the full song object with all details, only reliably obtainable after fetching song info from ytmusicapi
*/

#pragma once

#include "album.hpp"
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

    std::string thumbnail_large;
    std::string thumbnail_small;

    music::AlbumRef album;

    std::vector<ArtistRef> artists;

    static SongRef from_json(const nlohmann::json& j) {
        SongRef s;

        s.id = j.value("videoId", "");
        s.title = j.value("title", "");

        if (j.contains("views") && j["views"].is_string()) {
            s.views = j["views"].get<std::string>();
        } else {
            s.views = "";
        }

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

        /* Formats:
        - Search result: thumbnails
        - Radio result: thumbnail
        - Don't ask me why that is
        */
        if ((j.contains("thumbnails") && j["thumbnails"].is_array() && !j["thumbnails"].empty()) ||
             j.contains("thumbnail")  && j["thumbnail"].is_array()  && !j["thumbnail"].empty()
        ) {
            s.thumbnail_large = j["thumbnails"].back().value("url", "");
            s.thumbnail_small = j["thumbnails"].front().value("url", "");

        } else {
            s.thumbnail_large = "";
            s.thumbnail_small = "";
        }

        /* Formats:
        - Search result: album: {id, title}
        - Radio result: album: [{id, title}]
        - Don't ask me why that is
        */
        if (j.contains("album")) {
            if (j["album"].is_object()) {
                const auto& album_json = j["album"];
                s.album.title = album_json.value("name", "");
                s.album.id = album_json.value("id", "");

            } else if (j["album"].is_array() && !j["album"].empty()) {
                const auto& album_json = j["album"].front();
                s.album.title = album_json.value("name", "");
                s.album.id = album_json.value("id", "");

            } else {
                s.album = music::AlbumRef{};
            }

        } else {
            s.album = music::AlbumRef{};
        }

        return s;
    }
};

struct Song {
    SongRef ref;

    uint64_t duration = 0;

    std::string album_title;
    std::string url;

    static Song from_json(const nlohmann::json& j) {
        Song s;

        s.album_title = j.value("album", "");

        return s;
    }
};

}
