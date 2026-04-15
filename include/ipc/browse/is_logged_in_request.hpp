/* LOGGED IN
- Returns a bool if user is logged in or not.
- Python endpoint in python/server.py
*/

#pragma once

#include "../request.hpp"

namespace ipc {

class IsLoggedInRequest : public Request {
public:
    explicit IsLoggedInRequest() = default;

    nlohmann::json to_json() const override {

        nlohmann::json j{
            {"action", "is_logged_in"},
        };

        return j;
    }
};

}
