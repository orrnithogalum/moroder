/* RESPONSE
- All classes that inherit from this are in this folder and are used to receive data from the python process in json format
- Some responses are of similar format but code was still duplicated for readability.
- No response can take in args in constructor.
- All json parsing is done in cpp, except for some endpoints since the json response can be so long the buffer is overloaded
  (see include/ipc/browse/song_response.hpp and python/services/get_song.py).
*/

#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include <spdlog/spdlog.h>

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
