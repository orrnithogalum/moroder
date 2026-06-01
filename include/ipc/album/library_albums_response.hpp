#pragma once

#include "../../models/album.hpp"
#include "../response.hpp"

namespace ipc {

class LibraryAlbumsResponse : public Response {
public:
    std::vector<music::Album> results;

    explicit LibraryAlbumsResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.emplace_back(music::Album::from_json(j));
    }
};

}
