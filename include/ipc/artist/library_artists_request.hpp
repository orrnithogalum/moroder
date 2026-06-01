/* LIBRARY ARTISTS
- Returns a user's ytmusic library artists.
- Python endpoint in python/modules/private/library_artists.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class LibraryArtistsRequest : public Request {
public:
    explicit LibraryArtistsRequest() = default;

    nlohmann::json to_json() const override {
        nlohmann::json j{
            {"action", "get_library_artists"},
        };

        return j;
    }
};

}
