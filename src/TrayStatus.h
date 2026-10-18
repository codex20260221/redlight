#pragma once
#include "DisplayFilter.h"
#include "ShortcutProfile.h"
#include "Diagnostics.h"
#include "SessionTimer.h"
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

enum class StatusBackend : unsigned { Magnification = 0, Gamma = 1 };
enum class StatusShortcut : unsigned { None = 0, R = 1, F9 = 2, F10 = 3 };
struct TrayStatus {
    bool backendActive = false, restorePending = false, backendError = false, supportsLuma = true;
    StatusBackend backend = StatusBackend::Magnification;
    RedMode mode = RedMode::LumaRed;
    LumaProfile luma = LumaProfile::Classic;
    BackendVerification verification = BackendVerification::NotChecked;
    unsigned sessionMinutes = 0;
    bool timerArmed = false;
    unsigned remainingSeconds = 0;
    ShortcutProfile shortcutProfile = ShortcutProfile::Default;
    StatusShortcut toggle = StatusShortcut::None, restore = StatusShortcut::None;
    bool settingsUnsaved = false, sessionNotifications = false, busy = false;
    Diagnostics::FileState fileLogging = Diagnostics::FileState::NotInitialized;
    bool missingFileRecords = false, truncatedRecords = false, fileError = false;
    bool operator==(const TrayStatus&) const = default;
};
inline bool ValidTrayStatus(const TrayStatus& s) {
    return static_cast<unsigned>(s.backend) <= 1 && (s.mode == RedMode::StrictRed || s.mode == RedMode::LumaRed) &&
        IsValidLumaProfile(static_cast<unsigned>(s.luma)) && static_cast<unsigned>(s.verification) <= 4 &&
        s.sessionMinutes <= SessionTimer::maxMinutes && s.remainingSeconds <= s.sessionMinutes * 60 &&
        (s.timerArmed ? s.sessionMinutes != 0 : s.remainingSeconds == 0) &&
        (!s.backendActive || s.restorePending) && IsValidShortcutProfile(static_cast<unsigned>(s.shortcutProfile)) &&
        static_cast<unsigned>(s.toggle) <= 3 && static_cast<unsigned>(s.restore) <= 3 &&
        static_cast<unsigned>(s.fileLogging) <= 3 &&
        (s.backend == StatusBackend::Magnification ? s.supportsLuma : (!s.supportsLuma && s.mode == RedMode::StrictRed));
}
inline std::optional<std::uint64_t> PackTrayStatus(const TrayStatus& s) {
    if (!ValidTrayStatus(s)) return std::nullopt;
    std::uint64_t word = 0x5201000000000000ull; // 'R', protocol 1; fixed-width process-local message payload.
    auto put = [&](std::uint64_t value, unsigned shift) { word |= value << shift; };
    put(s.backendActive, 0); put(s.restorePending, 1); put(s.backendError, 2); put(s.supportsLuma, 3);
    put(static_cast<unsigned>(s.backend), 4); put(s.mode == RedMode::LumaRed, 5); put(static_cast<unsigned>(s.luma), 6);
    put(static_cast<unsigned>(s.verification), 8); put(s.sessionMinutes, 11); put(s.timerArmed, 19); put(s.remainingSeconds, 20);
    put(static_cast<unsigned>(s.shortcutProfile), 34); put(static_cast<unsigned>(s.toggle), 36); put(static_cast<unsigned>(s.restore), 38);
    put(s.settingsUnsaved, 40); put(s.sessionNotifications, 41); put(s.busy, 42); put(static_cast<unsigned>(s.fileLogging), 43);
    put(s.missingFileRecords, 45); put(s.truncatedRecords, 46); put(s.fileError, 47);
    return word;
}
inline std::optional<TrayStatus> UnpackTrayStatus(std::uint64_t word) {
    if ((word >> 48) != 0x5201) return std::nullopt;
    auto get = [&](unsigned shift, std::uint64_t mask) { return static_cast<unsigned>((word >> shift) & mask); };
    TrayStatus s;
    s.backendActive = get(0, 1) != 0; s.restorePending = get(1, 1) != 0; s.backendError = get(2, 1) != 0; s.supportsLuma = get(3, 1) != 0;
    s.backend = static_cast<StatusBackend>(get(4, 1)); s.mode = get(5, 1) ? RedMode::LumaRed : RedMode::StrictRed;
    s.luma = static_cast<LumaProfile>(get(6, 3)); s.verification = static_cast<BackendVerification>(get(8, 7));
    s.sessionMinutes = get(11, 255); s.timerArmed = get(19, 1) != 0; s.remainingSeconds = get(20, 16383);
    s.shortcutProfile = static_cast<ShortcutProfile>(get(34, 3)); s.toggle = static_cast<StatusShortcut>(get(36, 3)); s.restore = static_cast<StatusShortcut>(get(38, 3));
    s.settingsUnsaved = get(40, 1) != 0; s.sessionNotifications = get(41, 1) != 0; s.busy = get(42, 1) != 0;
    s.fileLogging = static_cast<Diagnostics::FileState>(get(43, 3));
    s.missingFileRecords = get(45, 1) != 0; s.truncatedRecords = get(46, 1) != 0; s.fileError = get(47, 1) != 0;
    return ValidTrayStatus(s) ? std::optional<TrayStatus>(s) : std::nullopt;
}
inline StatusShortcut StatusShortcutKey(const std::optional<ShortcutBinding>& binding) {
    if (!binding) return StatusShortcut::None;
    if (binding->key == 'R') return StatusShortcut::R;
    if (binding->key == 0x78) return StatusShortcut::F9;
    if (binding->key == 0x79) return StatusShortcut::F10;
    return StatusShortcut::None;
}
inline const char* StatusShortcutName(StatusShortcut key, bool restore) {
    if (key == StatusShortcut::None) return "none";
    const unsigned vk = key == StatusShortcut::R ? 'R' : (key == StatusShortcut::F9 ? 0x78 : 0x79);
    return ShortcutBindingName({restore ? 7u : 3u, vk});
}
inline const char* TrayStatusToken(const TrayStatus& s) {
    if (s.busy) return "busy";
    if (s.restorePending && (s.backendError || !s.backendActive)) return "recovery-needed";
    if (s.verification == BackendVerification::Changed) return "backend-changed";
    if (s.verification == BackendVerification::Unavailable) return "verification-unavailable";
    return s.backendActive ? "on" : "off";
}
inline std::optional<std::string> TrayStatusJson(const TrayStatus& s) {
    if (!ValidTrayStatus(s)) return std::nullopt;
    const auto boolean = [](bool value) { return value ? "true" : "false"; };
    char remaining[16];
    if (s.timerArmed) std::snprintf(remaining, sizeof(remaining), "%u", s.remainingSeconds);
    else std::snprintf(remaining, sizeof(remaining), "null");
    const char* token = TrayStatusToken(s);
    const char* verification = s.verification == BackendVerification::NotChecked ? "not-checked" :
        (s.verification == BackendVerification::Inactive ? "inactive" : (s.verification == BackendVerification::Matches ? "matches" :
        (s.verification == BackendVerification::Changed ? "changed" : "unavailable")));
    const bool active = std::string_view(token) == "on";
    const char* fileState = s.fileLogging == Diagnostics::FileState::Available ? "available" :
        (s.fileLogging == Diagnostics::FileState::Degraded ? "degraded" : (s.fileLogging == Diagnostics::FileState::Unavailable ? "unavailable" : "not-initialized"));
    char text[2048];
    const int length = std::snprintf(text, sizeof(text),
        "{\n  \"protocol\":1,\n  \"status\":\"%s\",\n  \"active\":%s,\n  \"backend_active\":%s,\n  \"restore_pending\":%s,\n  \"backend_error\":%s,\n  \"verification\":\"%s\",\n"
        "  \"backend\":\"%s\",\n  \"red_mode\":\"%s\",\n  \"luma_profile\":\"%s\",\n  \"luma_profile_scope\":\"%s\",\n"
        "  \"session_limit_minutes\":%u,\n  \"remaining_seconds\":%s,\n  \"shortcut_profile\":\"%s\",\n"
        "  \"toggle_shortcut\":\"%s\",\n  \"restore_shortcut\":\"%s\",\n  \"settings_unsaved\":%s,\n"
        "  \"session_notifications\":%s,\n  \"file_logging\":\"%s\",\n  \"file_records_missing\":%s,\n"
        "  \"records_truncated\":%s,\n  \"file_error\":%s\n}\n",
        token, boolean(active), boolean(s.backendActive), boolean(s.restorePending), boolean(s.backendError), verification,
        s.backend == StatusBackend::Magnification ? "magnification" : "gamma", s.mode == RedMode::LumaRed ? "luma" : "strict",
        s.luma == LumaProfile::Classic ? "classic" : (s.luma == LumaProfile::GreenDetail ? "green" : "equal"),
        s.supportsLuma ? "backend-selection" : "magnification-preference",
        s.sessionMinutes, remaining, ShortcutProfileToken(s.shortcutProfile), StatusShortcutName(s.toggle, false), StatusShortcutName(s.restore, true),
        boolean(s.settingsUnsaved), boolean(s.sessionNotifications), fileState, boolean(s.missingFileRecords), boolean(s.truncatedRecords), boolean(s.fileError));
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(text)) return std::nullopt;
    return std::string(text, static_cast<std::size_t>(length));
}
