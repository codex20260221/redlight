#pragma once
#include <array>
#include <optional>
#include <string_view>

enum class ExpiryWarning : unsigned { Off = 0, Seconds30 = 30, OneMinute = 60, FiveMinutes = 300 };
inline constexpr std::array<ExpiryWarning, 4> ExpiryWarningOptions{
    ExpiryWarning::Off, ExpiryWarning::Seconds30, ExpiryWarning::OneMinute, ExpiryWarning::FiveMinutes};
inline bool IsValidExpiryWarning(ExpiryWarning value) {
    return value == ExpiryWarning::Off || value == ExpiryWarning::Seconds30 || value == ExpiryWarning::OneMinute || value == ExpiryWarning::FiveMinutes;
}
inline unsigned ExpiryWarningSeconds(ExpiryWarning value) { return IsValidExpiryWarning(value) ? static_cast<unsigned>(value) : 0; }
inline const char* ExpiryWarningName(ExpiryWarning value) {
    switch (value) {
    case ExpiryWarning::Off: return "Off";
    case ExpiryWarning::Seconds30: return "30 seconds";
    case ExpiryWarning::OneMinute: return "1 minute";
    case ExpiryWarning::FiveMinutes: return "5 minutes";
    }
    return "Unknown";
}
inline const char* ExpiryWarningToken(ExpiryWarning value) {
    switch (value) {
    case ExpiryWarning::Off: return "off";
    case ExpiryWarning::Seconds30: return "30s";
    case ExpiryWarning::OneMinute: return "1m";
    case ExpiryWarning::FiveMinutes: return "5m";
    }
    return "invalid";
}
inline std::optional<ExpiryWarning> ParseExpiryWarning(std::string_view value) {
    for (const auto option : ExpiryWarningOptions) if (value == ExpiryWarningToken(option)) return option;
    return std::nullopt;
}
