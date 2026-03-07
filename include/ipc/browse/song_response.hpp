#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class SongResponse : public Response {
public:
    music::Song song;

    explicit SongResponse() : Response() {}

    explicit SongResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("song")) {
            return;
        }

        const auto& s = json_["song"];
        this->song = music::Song::from_json(s);
    }
};

}