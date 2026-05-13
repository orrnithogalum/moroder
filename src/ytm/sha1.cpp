#include "../../include/ytm/sha1.hpp"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace {

inline uint32_t rol(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }

void transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];

    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }

    for (int i = 16; i < 80; ++i) {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;

        if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }

        uint32_t tmp = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = tmp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}

namespace ytm {

std::string sha1Hex(const std::string& input) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
    size_t         len  = input.size();
    uint64_t       bits = static_cast<uint64_t>(len) * 8;

    size_t i = 0;
    for (; i + 64 <= len; i += 64) {
        transform(state, data + i);
    }

    uint8_t tail[128] = {0};
    size_t  rem       = len - i;
    std::memcpy(tail, data + i, rem);
    tail[rem] = 0x80;

    size_t total = (rem < 56) ? 64 : 128;
    for (int b = 0; b < 8; ++b) {
        tail[total - 1 - b] = static_cast<uint8_t>((bits >> (8 * b)) & 0xFF);
    }

    transform(state, tail);
    if (total == 128) transform(state, tail + 64);

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (uint32_t s : state) out << std::setw(8) << s;

    return out.str();
}

}
