/* PLAYLIST
- Returns track list for a given playlist.
- Python endpoint in python/modules/playlist.py
*/

#pragma once

#include "../../models/playlist.hpp"
#include "../../config/config.hpp"
#include "../request.hpp"

namespace ipc {

class PlaylistRequest : public Request {
public:
    explicit PlaylistRequest(const music::PlaylistRef& ref) : ref_(std::move(ref)) {}

    nlohmann::json to_json() const override {
        const Config cfg = Config::get();
        return {
            {"action", "get_playlist"},
            {"id", ref_.id}
        };
    }

private:
    const music::PlaylistRef ref_;
};

}
