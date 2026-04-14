/* RADIO NEXT
- Returns the next radio results for a given radio.
- Python endpoint in python/modules/radio.py
*/

#pragma once

#include "../../config/config.hpp"
#include "../../models/radio.hpp"
#include "../request.hpp"

namespace ipc {

class RadioNextRequest : public Request {
public:
    explicit RadioNextRequest(const music::Radio& radio) : radio_(std::move(radio)) {}

    nlohmann::json to_json() const override {
        const Config cfg = Config::get();
        return {
            {"action", "radio_next"},
            {"limit", cfg.RADIO_RESULT_LIMIT},
            {"continuation", radio_.continuation},
            {"id", radio_.seed_id},
            {"type", radio_.type}
        };
    }

private:
    const music::Radio radio_;
};

}
