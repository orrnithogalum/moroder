/* LIBRARY SONGS
- Returns a user's ytmusic library songs.
- Python endpoint in python/modules/private/library_songs.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class LibrarySongsRequest : public Request {
public:
    explicit LibrarySongsRequest() = default;

    nlohmann::json to_json() const override {
        nlohmann::json j{
            {"action", "get_library_songs"},
        };

        return j;
    }
};

}
