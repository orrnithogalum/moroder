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
    std::string getStreamUrl() const {
        return this->url;
    }
};

}
