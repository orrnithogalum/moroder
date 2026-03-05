#pragma once

#include <optional>

#include "../models/video.hpp"
#include "../models/song.hpp"
#include "request.hpp"

namespace ipc {

class StreamRequest : public Request {
public:
    explicit StreamRequest(const music::SongRef& song) : song_(std::move(song)) {}
    explicit StreamRequest(const music::VideoRef& video) : video_(std::move(video)) {}

    nlohmann::json to_json() const override {
        if (song_) {
            return {
                {"action", "stream"},
                {"id", song_->id}
            };
        }
        
        else {
            return {
                {"action", "stream"},
                {"id", video_->id}
            };
        }
    }

private:
    std::optional<music::SongRef> song_;
    std::optional<music::VideoRef> video_;
};

}