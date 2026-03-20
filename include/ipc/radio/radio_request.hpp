/* RADIO
- Returns radio results for a given song.
- Python endpoint in python/services/radio.py
*/

#pragma once

#include "../../config/config.hpp"
#include "../../models/song.hpp"
#include "../request.hpp"

namespace ipc {

class RadioRequest : public Request {
public:
    explicit RadioRequest(const music::SongRef& ref) : ref_(std::move(ref)) {}

    nlohmann::json to_json() const override {
        Config cfg = Config::get();
        return {
            {"action", "radio"},
            {"limit", cfg.RADIO_RESULT_LIMIT},
            {"id", ref_.id}
        };
    }

private:
    const music::SongRef ref_;
};

}
