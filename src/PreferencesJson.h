#pragma once
#include "Preferences.h"
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

inline constexpr std::size_t PreferencesJsonBufferSize = 256;
inline std::optional<std::string_view> FormatPreferencesJson(const AppPreferences& value, std::span<char> output) noexcept {
    if (!ValidPreferences(value) || output.empty()) return std::nullopt;
    char enableAt[6], restoreAt[6];
    if (!Scheduling::FormatMinute(value.schedule.enableMinute, enableAt) || !Scheduling::FormatMinute(value.schedule.restoreMinute, restoreAt)) return std::nullopt;
    const int length = std::snprintf(output.data(), output.size(),
        "{\"mode\":\"%s\",\"luma\":\"%s\",\"backend\":\"%s\",\"session_minutes\":%u,\"hotkeys\":\"%s\",\"expiry_warning\":\"%s\",\"brightness_percent\":%u,\"fade_in\":\"%s\",\"schedule\":\"%s\",\"enable_at\":\"%s\",\"restore_at\":\"%s\"}",
        PreferenceModeToken(value.mode), PreferenceLumaToken(value.luma), PreferenceBackendToken(value.backend), value.sessionMinutes,
        ShortcutProfileToken(value.shortcuts), ExpiryWarningToken(value.expiryWarning), value.brightnessPercent, FadeInToken(value.fadeIn),
        value.schedule.enabled ? "daily" : "off", enableAt, restoreAt);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
