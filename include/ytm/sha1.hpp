/* SHA-1
- needed only to build the SAPISIDHASH authorization header.
- Kept self-contained so the project doesn't pick up a libcrypto dependency for one hash.
- This is the standard FIPS 180-1 construction.
*/

#pragma once

#include <string>

namespace ytm {

// Hex-encoded SHA-1 of `input`. Used only for the SAPISIDHASH auth header.
std::string sha1Hex(const std::string& input);

}
