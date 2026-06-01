#pragma once

#include "../../models/artist.hpp"
#include "../response.hpp"

namespace ipc {

class LibraryArtistsResponse : public Response {
public:
    std::vector<music::ArtistRef> results;

    explicit LibraryArtistsResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.emplace_back(music::ArtistRef::from_json(j));
    }
};

}
