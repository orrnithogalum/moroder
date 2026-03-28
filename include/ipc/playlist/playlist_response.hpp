#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class PlaylistResponse : public Response {
public:
    std::vector<music::IStreamable> results;

    explicit PlaylistResponse() : Response() {}

    void addItem(nlohmann::json& j) {
        results.push_back(music::Song::from_json(j));
    }
};

}
