#pragma once
#include <optional>
#include <string_view>

inline constexpr unsigned DefaultRedBrightness = 100;
inline constexpr bool ValidRedBrightness(unsigned percent) noexcept { return percent >= 1 && percent <= 100; }
inline std::optional<unsigned> ParseRedBrightness(std::wstring_view text) noexcept {
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t')) text.remove_suffix(1);
    if (text.empty()) return std::nullopt;
    unsigned value = 0;
    for (const auto character : text) {
        if (character < L'0' || character > L'9') return std::nullopt;
        value = value * 10 + static_cast<unsigned>(character - L'0');
        if (value > 100) return std::nullopt; // Bound before another multiplication.
    }
    return ValidRedBrightness(value) ? std::optional<unsigned>(value) : std::nullopt;
}
