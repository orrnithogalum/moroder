/* CONTROL
- Allows player controls (play, pause, forward, backward etc.).
- Python endpoint in python/services/control.py
*/

#pragma once

#include "../request.hpp"

#include <string>

namespace ipc {

class ControlRequest : public Request {
public:
    explicit ControlRequest(const std::string& command, const uint64_t arg = 0) : arg_(arg), command_(std::move(command)) {}

    nlohmann::json to_json() const override {
        std::string final_command = command_;

        if(command_ == "forward" || command_ == "backward" || command_ == "setpos") {
            final_command += " ";
            final_command += std::to_string(arg_);
        }

        nlohmann::json j{
            {"action", "control"},
            {"command", final_command}
        };

        return j;
    }

private:
    const std::string command_;
    const int arg_;
};

}