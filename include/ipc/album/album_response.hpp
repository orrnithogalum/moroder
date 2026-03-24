#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class AlbumResponse : public Response {
public:
    std::vector<music::SongRef> results;

    explicit AlbumResponse() : Response() {}

    void addItem(nlohmann::json& j, const music::AlbumRef& album) {
        nlohmann::json album_json = {
            {"id", album.id},
            {"name", album.title},
        };
        j["album"] = album_json;

        nlohmann::json thumbnail = {
            {"url", album.thumbnail}
        };

        j["thumbnails"] = { thumbnail };

        results.push_back(music::SongRef::from_json(j));
    }
};

}
