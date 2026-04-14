/* HOME
- Returns a ytmusic homepage, divided into categories.
- Python endpoint in python/modules/home.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class HomeRequest : public Request {
public:
    explicit HomeRequest() = default;

    nlohmann::json to_json() const override {
        nlohmann::json j{
            {"action", "home"},
        };

        return j;
    }
};

}
