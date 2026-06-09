#pragma once

#include "../../models/song.hpp"
#include "../response.hpp"

namespace ipc {

class LibrarySongsResponse : public Response {
public:
    std::vector<std::shared_ptr<music::IStreamable>> results;

    explicit LibrarySongsResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.emplace_back(std::make_shared<music::Song>(music::Song::from_json(j)));
    }
};

}
