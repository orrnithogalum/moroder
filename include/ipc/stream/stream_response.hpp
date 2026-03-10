#pragma once

#include "../response.hpp"

namespace ipc {

class StreamResponse : public Response {
public:
    std::string id = "";
    uint64_t duration = 0;

    explicit StreamResponse() : Response() {}

    explicit StreamResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("id") || !json_.contains("duration")) {
            return;
        }

        id = json_["id"].get<std::string>();
        duration = json_["duration"].get<uint64_t>();
    }
};

}