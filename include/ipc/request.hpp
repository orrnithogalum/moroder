/* REQUEST
- All classes that inherit from this are in this folder and are used to send data to the python process in json format
- Some requests are of similar format but code was still duplicated for readability.
- Some requests take in custom args (see include/ipc/browse/ include/ipc/control/).
*/

#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include <spdlog/spdlog.h>

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
