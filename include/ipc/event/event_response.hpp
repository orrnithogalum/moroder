/* EVENT
- Used for receiving events
- Usually mpv events like end of a stream.
- Python event logic in pyhon/server.py (mpv_reader_loop)
*/

#pragma once

#include "../response.hpp"

namespace ipc {

class EventResponse : public Response {
public:
    std::string name;
    
    explicit EventResponse() : Response() {}

    explicit EventResponse(const std::string& raw) : Response(raw) {
        if (!ok() || !json_.contains("name")) {
            return;
        }

        name = json_.value("name", "");
    }
};

}