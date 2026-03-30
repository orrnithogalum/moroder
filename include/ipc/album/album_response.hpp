#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class AlbumResponse : public Response {
public:
    std::vector<std::shared_ptr<music::IStreamable>> results;

    explicit AlbumResponse() : Response() {}

    void addItem(nlohmann::json& j, const music::AlbumRef& album) {
        nlohmann::json album_json = {
            {"id", album.id},
            {"name", album.title},
        };
        j["album"] = album_json;

        nlohmann::json thumbnails = {
            { {"url", album.thumbnail_small} },
            { {"url", album.thumbnail_large} }
        };

        j["thumbnails"] = thumbnails;

        results.emplace_back(std::make_shared<music::Song>(music::Song::from_json(j)));
    }
};

}
