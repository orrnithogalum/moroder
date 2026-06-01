/* LIBRARY PODCASTS
- Returns a user's ytmusic library podcasts.
- Python endpoint in python/modules/private/library_podcasts.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class LibraryPodcastsRequest : public Request {
public:
    explicit LibraryPodcastsRequest() = default;

    nlohmann::json to_json() const override {
        nlohmann::json j{
            {"action", "get_library_podcasts"},
        };

        return j;
    }
};

}
