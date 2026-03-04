#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace ipc {

class Request {
public:
    virtual ~Request() = default;
    
    virtual nlohmann::json to_json() const = 0;

    std::string serialize() const {
        return to_json().dump() + "\n";
    }
};

}