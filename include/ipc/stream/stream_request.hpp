#pragma once

#include <optional>

#include "../../models/song.hpp"
#include "../request.hpp"

namespace ipc {

class StreamRequest : public Request {
public:
    explicit StreamRequest(const music::SongRef& song) : song_(std::move(song)) {}

    nlohmann::json to_json() const override {
        return {
            {"action", "stream"},
            {"id", song_->id}
        };
    }

private:
    std::optional<music::SongRef> song_;
};

}