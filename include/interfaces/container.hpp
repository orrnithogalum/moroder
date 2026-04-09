/* CONTAINER
- Interface for any class that contains a list of streamables.
- Albums, Playlists & Radio
*/

#pragma once

#include "streamable.hpp"

#include <memory>
#include <vector>

namespace music {

class IStreamableContainer {
public:
    virtual ~IStreamableContainer() = default;

protected:
    std::vector<std::shared_ptr<IStreamable>> tracks;

public:
    virtual std::vector<std::shared_ptr<IStreamable>> getStreamables() const {
        return tracks;
    }

    void addStreamable(std::shared_ptr<IStreamable> streamable) {
        tracks.emplace_back(std::move(streamable));
    }
};

}
