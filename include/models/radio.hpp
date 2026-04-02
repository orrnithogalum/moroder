/* RADIO
- A list of songs provided by the yt music algorithm
*/

#pragma once

#include "../interfaces/container.hpp"
#include <string>

namespace music {

struct Radio : IStreamableContainer {
public:
    std::string seed_id;
    std::string continuation;
    std::string type;
};

}
