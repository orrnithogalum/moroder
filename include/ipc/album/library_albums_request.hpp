/* LIBRARY ALBUMS
- Returns a user's ytmusic albums.
- Python endpoint in python/modules/private/library_albums.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class LibraryAlbumsRequest : public Request {
public:
    explicit LibraryAlbumsRequest() = default;

    nlohmann::json to_json() const override {
        nlohmann::json j{
            {"action", "get_library_albums"},
        };

        return j;
    }
};

}
