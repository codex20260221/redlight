#pragma once
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

namespace Scheduling {
struct DailyTimes {
    bool enabled = false;
    unsigned enableMinute = 0, restoreMinute = 0;
    bool valid() const noexcept {
        return enableMinute < 1440 && restoreMinute < 1440 && (!enabled || enableMinute != restoreMinute);
    }
    bool operator==(const DailyTimes&) const = default;
};
inline std::optional<unsigned> ParseMinute(std::string_view text) noexcept {
    if (text.size() != 5 || text[2] != ':') return std::nullopt;
    for (const unsigned index : {0u, 1u, 3u, 4u}) if (text[index] < '0' || text[index] > '9') return std::nullopt;
    const auto hour = static_cast<unsigned>((text[0] - '0') * 10 + text[1] - '0');
    const auto minute = static_cast<unsigned>((text[3] - '0') * 10 + text[4] - '0');
    return hour < 24 && minute < 60 ? std::optional<unsigned>(hour * 60 + minute) : std::nullopt;
}
inline std::optional<std::string_view> FormatMinute(unsigned minute, std::span<char> output) noexcept {
    if (minute >= 1440 || output.size() < 6) return std::nullopt;
    const auto length = std::snprintf(output.data(), output.size(), "%02u:%02u", minute / 60, minute % 60);
    return length == 5 ? std::optional<std::string_view>(std::string_view(output.data(), 5)) : std::nullopt;
}
}
