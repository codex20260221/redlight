#pragma once
#include <array>
#include <optional>
#include <string_view>

enum class FadeInDuration : unsigned { Off = 0, Seconds5 = 5, Seconds15 = 15, Seconds30 = 30, Minute = 60 };
inline constexpr std::array FadeInOptions{FadeInDuration::Off, FadeInDuration::Seconds5, FadeInDuration::Seconds15, FadeInDuration::Seconds30, FadeInDuration::Minute};
inline constexpr bool ValidFadeInDuration(FadeInDuration value) noexcept {
    for (const auto option : FadeInOptions) if (value == option) return true;
    return false;
}
inline const char* FadeInToken(FadeInDuration value) noexcept {
    switch (value) {
    case FadeInDuration::Off: return "off";
    case FadeInDuration::Seconds5: return "5s";
    case FadeInDuration::Seconds15: return "15s";
    case FadeInDuration::Seconds30: return "30s";
    case FadeInDuration::Minute: return "1m";
    }
    return "invalid";
}
inline const char* FadeInName(FadeInDuration value) noexcept {
    switch (value) {
    case FadeInDuration::Off: return "Off (immediate)";
    case FadeInDuration::Seconds5: return "5 seconds";
    case FadeInDuration::Seconds15: return "15 seconds";
    case FadeInDuration::Seconds30: return "30 seconds";
    case FadeInDuration::Minute: return "1 minute";
    }
    return "Unknown";
}
inline std::optional<FadeInDuration> ParseFadeInDuration(std::string_view text) noexcept {
    for (const auto option : FadeInOptions) if (text == FadeInToken(option)) return option;
    return std::nullopt;
}
