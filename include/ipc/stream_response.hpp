#pragma once

#include "response.hpp"

namespace ipc {

class StreamResponse : public Response {
public:
    std::string id;

    explicit StreamResponse() : Response() {}

    explicit StreamResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("id")) {
            return;
        }

        id = json_["id"].get<std::string>();
    }
};

}