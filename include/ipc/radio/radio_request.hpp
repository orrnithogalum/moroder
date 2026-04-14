/* RADIO
- Returns radio results for a given song.
- Python endpoint in python/modules/radio.py
*/

#pragma once

#include "../../config/config.hpp"
#include "../request.hpp"

namespace ipc {

class RadioRequest : public Request {
public:
    explicit RadioRequest(const std::string id, const std::string type) : stream_id(std::move(id)), req_type(std::move(type)) {}

    nlohmann::json to_json() const override {
        const Config cfg = Config::get();
        return {
            {"action", "radio"},
            {"limit", cfg.RADIO_RESULT_LIMIT},
            {"id", stream_id},
            {"type", req_type}
        };
    }

private:
    const std::string stream_id;
    const std::string req_type;
};

}
