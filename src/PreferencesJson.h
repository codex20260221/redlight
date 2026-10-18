#pragma once
#include "Preferences.h"
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

inline constexpr std::size_t PreferencesJsonBufferSize = 256;
inline std::optional<std::string_view> FormatPreferencesJson(const AppPreferences& value, std::span<char> output) noexcept {
    if (!ValidPreferences(value) || output.empty()) return std::nullopt;
    const int length = std::snprintf(output.data(), output.size(),
        "{\"mode\":\"%s\",\"luma\":\"%s\",\"backend\":\"%s\",\"session_minutes\":%u,\"hotkeys\":\"%s\",\"expiry_warning\":\"%s\"}",
        PreferenceModeToken(value.mode), PreferenceLumaToken(value.luma), PreferenceBackendToken(value.backend), value.sessionMinutes,
        ShortcutProfileToken(value.shortcuts), ExpiryWarningToken(value.expiryWarning));
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
