/* STREAMABLE
- Interface for any class that can be streamed (audio).
- Songs and Podcast episodes can be streamed
*/

#pragma once

#include <cstdint>
#include <string>

namespace music {

class IStreamable {
public:
    virtual ~IStreamable() = default;

protected:
    uint64_t duration;
    std::string url;

public:
    void setDuration(uint64_t d) {
        this->duration = d;
    }

    uint64_t getDuration() {
        return this->duration;
    }

    std::string getStreamUrl() const {
        return this->url;
    }
};

}
