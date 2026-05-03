#pragma once

#include <string>

namespace ytm {

// Hex-encoded SHA-1 of `input`. Used only for the SAPISIDHASH auth header.
std::string sha1Hex(const std::string& input);

}
