#pragma once
#include "BackendPreference.h"
#include "DisplayFilter.h"
#include "SessionTimer.h"
#include "ShortcutProfile.h"
#include "ExpiryWarning.h"
#include "Utf8.h"
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

inline constexpr unsigned PreferencesProfileVersion = 3;

struct AppPreferences {
    RedMode mode = RedMode::LumaRed;
    LumaProfile luma = LumaProfile::Classic;
    BackendPreference backend = BackendPreference::Automatic;
    unsigned sessionMinutes = 0;
    ShortcutProfile shortcuts = ShortcutProfile::Default;
    ExpiryWarning expiryWarning = ExpiryWarning::Off;
    bool operator==(const AppPreferences&) const = default;
};
inline bool ValidPreferences(const AppPreferences& value) {
    return (value.mode == RedMode::StrictRed || value.mode == RedMode::LumaRed) &&
        IsValidLumaProfile(static_cast<unsigned>(value.luma)) &&
        IsValidBackendPreference(static_cast<unsigned>(value.backend)) && value.sessionMinutes <= SessionTimer::maxMinutes &&
        IsValidShortcutProfile(static_cast<unsigned>(value.shortcuts)) && IsValidExpiryWarning(value.expiryWarning);
}
inline const char* PreferenceModeToken(RedMode value) noexcept {
    switch (value) {
    case RedMode::StrictRed: return "strict";
    case RedMode::LumaRed: return "luma";
    }
    return "invalid";
}
inline const char* PreferenceLumaToken(LumaProfile value) noexcept {
    switch (value) {
    case LumaProfile::Classic: return "classic";
    case LumaProfile::GreenDetail: return "green";
    case LumaProfile::EqualDetail: return "equal";
    }
    return "invalid";
}
inline const char* PreferenceBackendToken(BackendPreference value) noexcept {
    switch (value) {
    case BackendPreference::Automatic: return "auto";
    case BackendPreference::Magnification: return "magnification";
    case BackendPreference::Gamma: return "gamma";
    }
    return "invalid";
}
inline std::optional<std::string> SerializePreferences(const AppPreferences& value) {
    if (!ValidPreferences(value)) return std::nullopt;
    char text[256];
    const int length = std::snprintf(text, sizeof(text),
        "[RedLightProfile]\r\nversion=%u\r\nmode=%s\r\nluma=%s\r\nbackend=%s\r\nsession_minutes=%u\r\nhotkeys=%s\r\nexpiry_warning=%s\r\n",
        PreferencesProfileVersion, PreferenceModeToken(value.mode), PreferenceLumaToken(value.luma), PreferenceBackendToken(value.backend),
        value.sessionMinutes, ShortcutProfileToken(value.shortcuts), ExpiryWarningToken(value.expiryWarning));
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(text)) return std::nullopt;
    return std::string(text, static_cast<std::size_t>(length));
}
struct PreferencesParseResult {
    std::optional<AppPreferences> preferences;
    const char* error = nullptr;
    unsigned line = 0;
};
inline std::string_view TrimProfileField(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}
inline PreferencesParseResult ParsePreferences(std::string_view input) {
    if (input.size() > 4096 || input.find('\0') != std::string_view::npos) return {std::nullopt, "Profile is too large or contains a NUL byte", 0};
    if (!IsValidUtf8(input)) return {std::nullopt, "Profile contains invalid UTF-8", 0};
    if (input.size() >= 3 && static_cast<unsigned char>(input[0]) == 0xef &&
        static_cast<unsigned char>(input[1]) == 0xbb && static_cast<unsigned char>(input[2]) == 0xbf) input.remove_prefix(3);
    AppPreferences value;
    unsigned seen = 0, lineNumber = 0, version = 0;
    bool header = false;
    while (!input.empty()) {
        ++lineNumber;
        const auto newline = input.find('\n');
        auto line = TrimProfileField(input.substr(0, newline));
        input.remove_prefix(newline == std::string_view::npos ? input.size() : newline + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        if (line == "[RedLightProfile]") {
            if (header) return {std::nullopt, "Duplicate profile section", lineNumber};
            header = true;
            continue;
        }
        if (!header) return {std::nullopt, "Missing RedLightProfile section", lineNumber};
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) return {std::nullopt, "Expected key=value", lineNumber};
        const auto key = TrimProfileField(line.substr(0, equals));
        const auto field = TrimProfileField(line.substr(equals + 1));
        unsigned flag = 0;
        if (key == "version") {
            flag = 1;
            if (field != "1" && field != "2" && field != "3") return {std::nullopt, "Unsupported profile version", lineNumber};
            version = field == "1" ? 1 : (field == "2" ? 2 : 3);
        } else if (key == "mode") {
            flag = 2;
            if (field == "strict") value.mode = RedMode::StrictRed;
            else if (field == "luma") value.mode = RedMode::LumaRed;
            else return {std::nullopt, "Invalid red mode", lineNumber};
        } else if (key == "luma") {
            flag = 4;
            if (field == "classic") value.luma = LumaProfile::Classic;
            else if (field == "green") value.luma = LumaProfile::GreenDetail;
            else if (field == "equal") value.luma = LumaProfile::EqualDetail;
            else return {std::nullopt, "Invalid luma preset", lineNumber};
        } else if (key == "backend") {
            flag = 8;
            if (field == "auto") value.backend = BackendPreference::Automatic;
            else if (field == "magnification") value.backend = BackendPreference::Magnification;
            else if (field == "gamma") value.backend = BackendPreference::Gamma;
            else return {std::nullopt, "Invalid backend preference", lineNumber};
        } else if (key == "session_minutes") {
            flag = 16;
            unsigned minutes = 0;
            const auto parsed = std::from_chars(field.data(), field.data() + field.size(), minutes);
            if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size() || minutes > SessionTimer::maxMinutes)
                return {std::nullopt, "Session limit must be an integer from 0 to 240", lineNumber};
            value.sessionMinutes = minutes;
        } else if (key == "hotkeys") {
            flag = 32;
            const auto profile = ParseShortcutProfile(field);
            if (!profile) return {std::nullopt, "Invalid shortcut profile", lineNumber};
            value.shortcuts = *profile;
        } else if (key == "expiry_warning") {
            flag = 64;
            const auto warning = ParseExpiryWarning(field);
            if (!warning) return {std::nullopt, "Invalid expiry warning", lineNumber};
            value.expiryWarning = *warning;
        } else return {std::nullopt, "Unknown profile field", lineNumber};
        if (seen & flag) return {std::nullopt, "Duplicate profile field", lineNumber};
        seen |= flag;
    }
    if (version == 1 && (seen & 32)) return {std::nullopt, "Shortcut settings require profile version 2", lineNumber};
    if (version < 3 && (seen & 64)) return {std::nullopt, "Expiry warnings require profile version 3", lineNumber};
    if (!header || seen != (version == 3 ? 127u : (version == 2 ? 63u : 31u))) return {std::nullopt, "Profile section or required fields are missing", lineNumber};
    return {value, nullptr, 0};
}
