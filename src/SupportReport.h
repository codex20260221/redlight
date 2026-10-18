#pragma once
#include "DisplayFilter.h"
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

struct SupportReportState {
    std::string_view version, backend, startupPolicy, nextStartupPolicy, redMode, lumaProfile, lastError;
    BackendVerification verification = BackendVerification::NotChecked;
    bool commandLineOverride = false;
    bool preferencesUnsaved = false;
    std::string_view shortcutProfile = "Default", toggleBinding = "none", restoreBinding = "none";
    bool active = false, needsRestore = false;
    bool toggleHotkey = false, restoreHotkey = false, sessionNotifications = false;
    unsigned sessionMinutes = 0;
    std::optional<std::uint64_t> remainingSeconds;
};
inline std::string BuildSupportReport(const SupportReportState& state) {
    std::ostringstream report;
    report << "RedLight support report\r\nVersion: " << state.version << " (Windows x64)\r\n"
        << "Backend: " << state.backend << "\r\nStartup policy: " << state.startupPolicy
        << (state.commandLineOverride ? " (command-line override)" : "")
        << "\r\nNext-start preference: " << state.nextStartupPolicy << "\r\nMode: " << state.redMode
        << "\r\nLuma detail: " << state.lumaProfile << "\r\nState: "
        << FilterStatusDescription(state.active, state.needsRestore, !state.lastError.empty(), state.verification)
        << "\r\nBackend verification: " << BackendVerificationName(state.verification)
        << "\r\nSettings persistence: " << (state.preferencesUnsaved ? "unsaved changes" : "saved/default")
        << "\r\nRecovery data pending: " << (state.needsRestore ? "yes" : "no")
        << "\r\nShortcut preference: " << state.shortcutProfile
        << "\r\nToggle shortcut: " << (state.toggleHotkey ? "registered" : "unavailable") << " (" << state.toggleBinding << ")"
        << "\r\nRestore shortcut: " << (state.restoreHotkey ? "registered" : "unavailable") << " (" << state.restoreBinding << ")"
        << "\r\nSession notifications: " << (state.sessionNotifications ? "registered" : "unavailable")
        << "\r\nSession limit: ";
    if (state.sessionMinutes) report << state.sessionMinutes << " minutes";
    else report << "unlimited";
    report << "\r\nCountdown: ";
    if (state.remainingSeconds) report << *state.remainingSeconds << " seconds remaining";
    else report << "not armed";
    report << "\r\nLast backend error: " << (state.lastError.empty() ? "none" : state.lastError)
        << "\r\nStatus reflects backend call results; no display output is measured.\r\n";
    return report.str();
}
