#pragma once
#include <cstdint>
#include <string_view>

// RFC 3629 scalar-value encoding: reject overlong forms, surrogates and > U+10FFFF.
inline bool IsValidUtf8(std::string_view text) noexcept {
    std::size_t position = 0;
    while (position < text.size()) {
        const auto first = static_cast<unsigned char>(text[position]);
        if (first <= 0x7f) { ++position; continue; }
        unsigned length = 0;
        std::uint32_t scalar = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { length = 2; scalar = first & 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { length = 3; scalar = first & 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { length = 4; scalar = first & 0x07; minimum = 0x10000; }
        else return false;
        if (text.size() - position < length) return false;
        for (unsigned i = 1; i < length; ++i) {
            const auto next = static_cast<unsigned char>(text[position + i]);
            if ((next & 0xc0) != 0x80) return false;
            scalar = (scalar << 6) | (next & 0x3f);
        }
        if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) return false;
        position += length;
    }
    return true;
}
