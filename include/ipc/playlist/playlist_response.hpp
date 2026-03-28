#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class PlaylistResponse : public Response {
public:
    std::vector<std::shared_ptr<music::IStreamable>> results;

    explicit PlaylistResponse() : Response() {}

    void addItem(nlohmann::json& j) {
        results.push_back(std::make_shared<music::Song>(music::Song::from_json(j)));
    }
};

}
