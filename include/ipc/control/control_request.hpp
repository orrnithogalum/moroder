#pragma once

#include "../request.hpp"
#include <cstdint>
#include <string>

namespace ipc {

class ControlRequest : public Request {
public:
    explicit ControlRequest(std::string command, std::uint8_t duration = 0) : duration_(duration), command_(std::move(command)) {}

    nlohmann::json to_json() const override {
        std::string final_command = command_;

        if(command_ == "forward" || command_ == "backward") {
            final_command += " ";
            final_command += std::to_string(duration_);
        }

        nlohmann::json j{
            {"action", "control"},
            {"command", final_command}
        };

        return j;
    }

private:
    const std::uint8_t duration_;
    const std::string command_;
};

}