#pragma once
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

struct UtcTimestamp {
    unsigned year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, millisecond = 0;
    bool operator==(const UtcTimestamp&) const = default;
};
inline constexpr std::size_t UtcTimestampBufferSize = 32;
// Windows calendar range, with signed six-digit years above 9999. No clock/API calls.
inline std::optional<std::string_view> FormatUtcTimestamp(const UtcTimestamp& time, std::span<char> output) noexcept {
    if (output.empty() || time.year < 1601 || time.year > 30827 || time.month < 1 || time.month > 12 ||
        time.hour > 23 || time.minute > 59 || time.second > 59 || time.millisecond > 999) return std::nullopt;
    constexpr unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0);
    const auto maximumDay = days[time.month - 1] + (time.month == 2 && leap ? 1u : 0u);
    if (time.day < 1 || time.day > maximumDay) return std::nullopt;
    const int length = std::snprintf(output.data(), output.size(), time.year <= 9999 ? "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ" : "+%06u-%02u-%02uT%02u:%02u:%02u.%03uZ",
        time.year, time.month, time.day, time.hour, time.minute, time.second, time.millisecond);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
