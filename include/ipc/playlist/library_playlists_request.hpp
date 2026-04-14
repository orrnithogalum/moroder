/* LIBRARY PLAYLISTS
- Returns a user's ytmusic playlists.
- Python endpoint in python/modules/private/library_playlists.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class LibraryPlaylistsRequest : public Request {
public:
    explicit LibraryPlaylistsRequest() = default;

    nlohmann::json to_json() const override {
        nlohmann::json j{
            {"action", "get_library_playlists"},
        };

        return j;
    }
};

}
