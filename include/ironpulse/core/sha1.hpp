#pragma once

// Minimal SHA-1 implementation used solely to compute the
// Sec-WebSocket-Accept header during the RFC 6455 handshake, where the
// spec mandates SHA-1. Not intended (and not suitable) for any
// security-sensitive use — do not reuse this for password hashing,
// signatures, or anything else that needs a cryptographically robust
// primitive.

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace ironpulse::core {

class Sha1 {
public:
    static std::array<std::uint8_t, 20> digest(const std::string& input) {
        std::uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

        std::string message = input;
        const std::uint64_t bit_length = static_cast<std::uint64_t>(message.size()) * 8;

        message += static_cast<char>(0x80);
        while (message.size() % 64 != 56) {
            message += static_cast<char>(0x00);
        }
        for (int i = 7; i >= 0; --i) {
            message += static_cast<char>((bit_length >> (i * 8)) & 0xFF);
        }

        for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
            std::uint32_t w[80];
            for (std::size_t i = 0; i < 16; ++i) {
                w[i] =
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(message[chunk + i * 4])) << 24) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(message[chunk + i * 4 + 1]))
                     << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(message[chunk + i * 4 + 2])) << 8) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(message[chunk + i * 4 + 3])));
            }
            for (std::size_t i = 16; i < 80; ++i) {
                w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            }

            std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

            for (int i = 0; i < 80; ++i) {
                std::uint32_t f, k;
                if (i < 20) {
                    f = (b & c) | (~b & d);
                    k = 0x5A827999;
                } else if (i < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                } else if (i < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }

                const std::uint32_t temp = rotl(a, 5) + f + e + k + w[i];
                e = d;
                d = c;
                c = rotl(b, 30);
                b = a;
                a = temp;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
        }

        std::array<std::uint8_t, 20> result{};
        for (std::size_t i = 0; i < 5; ++i) {
            result[i * 4] = static_cast<std::uint8_t>((h[i] >> 24) & 0xFF);
            result[i * 4 + 1] = static_cast<std::uint8_t>((h[i] >> 16) & 0xFF);
            result[i * 4 + 2] = static_cast<std::uint8_t>((h[i] >> 8) & 0xFF);
            result[i * 4 + 3] = static_cast<std::uint8_t>(h[i] & 0xFF);
        }
        return result;
    }

private:
    static std::uint32_t rotl(std::uint32_t value, int bits) {
        return (value << bits) | (value >> (32 - bits));
    }
};

}  // namespace ironpulse::core
