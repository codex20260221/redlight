#pragma once
#include "Utf8.h"
#include <optional>
#include <span>
#include <string_view>

// Encode a UTF-8 string with JSON quotes; incomplete output is never returned.
inline std::optional<std::string_view> QuoteJsonString(std::string_view input, std::span<char> output) noexcept {
    if (!IsValidUtf8(input)) return std::nullopt;
    std::size_t position = 0;
    const auto put = [&](char value) {
        if (position == output.size()) return false;
        output[position++] = value;
        return true;
    };
    if (!put('"')) return std::nullopt;
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char value : input) {
        if (value == '"' || value == '\\') {
            if (!put('\\') || !put(static_cast<char>(value))) return std::nullopt;
        } else if (value < 0x20) {
            const char escaped[] = {'\\', 'u', '0', '0', hex[value >> 4], hex[value & 15]};
            for (const char character : escaped) if (!put(character)) return std::nullopt;
        } else if (!put(static_cast<char>(value))) return std::nullopt;
    }
    if (!put('"')) return std::nullopt;
    return std::string_view(output.data(), position);
}
