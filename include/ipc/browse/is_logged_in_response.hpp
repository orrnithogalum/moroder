#pragma once

#include "../response.hpp"

namespace ipc {

class IsLoggedInResponse : public Response {
public:
    bool is_logged_in = false;

    explicit IsLoggedInResponse() : Response() {}

    explicit IsLoggedInResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("value") || !json_["value"].is_boolean()) {
            return;
        }

        this->is_logged_in = json_["value"];
    }
};

}
