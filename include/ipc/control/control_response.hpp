#pragma once

#include "../response.hpp"

namespace ipc {

class ControlResponse : public Response {
public:
    float position;

    explicit ControlResponse() : Response() {}

    explicit ControlResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("position")) {
            return;
        }

        position = json_.value("position", 0.0f);
    }
};

}