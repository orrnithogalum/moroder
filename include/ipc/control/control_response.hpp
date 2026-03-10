#pragma once

#include "../response.hpp"

namespace ipc {

class ControlResponse : public Response {
public:
    uint64_t position = 0;

    explicit ControlResponse() : Response() {}

    explicit ControlResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("position")) {
            return;
        }

        position = json_["position"].get<uint64_t>();
    }
};

}