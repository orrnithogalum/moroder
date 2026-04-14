#pragma once

#include "../../models/playlist.hpp"
#include "../response.hpp"

namespace ipc {

class LibraryPlaylistsResponse : public Response {
public:
    std::vector<music::Playlist> results;

    explicit LibraryPlaylistsResponse() : Response() {}

    void addItem(const nlohmann::json& j) {
        results.emplace_back(music::Playlist::from_json(j));
    }
};

}
