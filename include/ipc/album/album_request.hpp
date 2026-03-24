/* ALBUM
- Returns track list for a given album.
- Python endpoint in python/services/album.py
*/

#pragma once

#include "../../config/config.hpp"
#include "../../models/album.hpp"
#include "../request.hpp"

namespace ipc {

class AlbumRequest : public Request {
public:
    explicit AlbumRequest(const music::AlbumRef& ref) : ref_(std::move(ref)) {}

    nlohmann::json to_json() const override {
        Config cfg = Config::get();
        return {
            {"action", "get_album"},
            {"id", ref_.id}
        };
    }

private:
    const music::AlbumRef ref_;
};

}
