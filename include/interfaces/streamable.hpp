/* STREAMABLE
- Interface for any class that can be streamed (audio).
- Songs and podcasts can be streamed
*/

#pragma once

#include <string>

class IStreamable {
public:
    virtual ~IStreamable() = default;
    virtual std::string getStreamUrl() const = 0;
};
