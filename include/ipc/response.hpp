#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "spdlog/spdlog.h"

namespace ipc {

class Response {
public:
    explicit Response() {}

    explicit Response(const std::string& raw) {
        json_ = nlohmann::json::parse(raw);
        status_ = json_.value("status", "error");
        
        if (status_ == "error") {
            message_ = json_.value("message", "Unknown error");
        }

        spdlog::info("Received reponse from python server: " + json_.dump(4));
    }

    bool ok() const {
        return status_ == "ok";
    }

    const std::string& message() const {
        return message_;
    }

protected:
    nlohmann::json json_;

    std::string message_;
    std::string status_;
};

}