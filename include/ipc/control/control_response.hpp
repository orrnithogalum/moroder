#pragma once

#include "../response.hpp"

namespace ipc {

class ControlResponse : public Response {
public:
    explicit ControlResponse() : Response() {}

    explicit ControlResponse(const std::string& raw) : Response(raw) {
        if (!ok()) {
            return;
        }
    }
};

}