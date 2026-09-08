#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ironpulse::core {

class Base64 {
public:
    static std::string encode(const std::uint8_t* data, std::size_t length) {
        static constexpr char kTable[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        std::string out;
        out.reserve(((length + 2) / 3) * 4);

        std::size_t i = 0;
        while (i + 3 <= length) {
            const std::uint32_t n = (static_cast<std::uint32_t>(data[i]) << 16) |
                                    (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                    static_cast<std::uint32_t>(data[i + 2]);
            out += kTable[(n >> 18) & 0x3F];
            out += kTable[(n >> 12) & 0x3F];
            out += kTable[(n >> 6) & 0x3F];
            out += kTable[n & 0x3F];
            i += 3;
        }

        const std::size_t remaining = length - i;
        if (remaining == 1) {
            const std::uint32_t n = static_cast<std::uint32_t>(data[i]) << 16;
            out += kTable[(n >> 18) & 0x3F];
            out += kTable[(n >> 12) & 0x3F];
            out += "==";
        } else if (remaining == 2) {
            const std::uint32_t n =
                (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8);
            out += kTable[(n >> 18) & 0x3F];
            out += kTable[(n >> 12) & 0x3F];
            out += kTable[(n >> 6) & 0x3F];
            out += "=";
        }

        return out;
    }

    template <typename Container>
    static std::string encode(const Container& bytes) {
        return encode(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    }
};

}  // namespace ironpulse::core
