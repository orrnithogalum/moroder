#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class RadioResponse : public Response {
public:
    std::vector<music::SongRef> results;

    explicit RadioResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.push_back(music::SongRef::from_json(j));
    }
};

}
