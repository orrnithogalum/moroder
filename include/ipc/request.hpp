#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "spdlog/spdlog.h"

namespace ipc {

class Request {
public:
    virtual ~Request() = default;
    
    virtual nlohmann::json to_json() const = 0;

    std::string serialize() const {
        spdlog::info("Sending request to python server: " + to_json().dump(4));
        return to_json().dump() + "\n";
    }
};

}