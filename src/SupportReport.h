#pragma once
#include "DisplayFilter.h"
#include <cstdint>
#include <optional>
#include <sstream>
#include <iomanip>
#include <string>
#include <string_view>

struct SupportReportState {
    std::string_view version, backend, startupPolicy, nextStartupPolicy, redMode, lumaProfile, lastError;
    std::string_view preferredRedMode = "Not reported", preferredLumaProfile = "Not reported";
    std::string_view buildRevision = "unversioned", sourceState = "unknown";
    BackendVerification verification = BackendVerification::NotChecked;
    std::optional<BackendReadbackDetails> readback;
    std::uint32_t verificationError = 0;
    bool commandLineOverride = false;
    bool preferencesUnsaved = false;
    std::string_view startupPreferenceSource = "Not reported";
    bool startupPreferenceWarning = false;
    std::string_view shortcutProfile = "Default", toggleBinding = "none", restoreBinding = "none";
    std::string_view fileLogging = "Not reported";
    std::uint64_t droppedLogLines = 0, truncatedLogLines = 0;
    std::uint32_t lastFileError = 0;
    bool active = false, needsRestore = false;
    bool restoreRequested = false;
    bool toggleHotkey = false, restoreHotkey = false, sessionNotifications = false;
    std::uint32_t sessionNotificationError = 0;
    unsigned sessionMinutes = 0;
    std::string_view expiryWarning = "Off";
    BackendStatistics backendStatistics{};
    std::optional<std::uint64_t> remainingSeconds;
};
inline std::string BuildSupportReport(const SupportReportState& state) {
    std::ostringstream report;
    report.exceptions(std::ios::badbit | std::ios::failbit);
    report << "RedLight support report\r\nVersion: " << state.version << " (Windows x64)\r\n"
        << "Build revision: " << state.buildRevision << " (" << state.sourceState << ")\r\n"
        << "Backend: " << state.backend << "\r\nStartup policy: " << state.startupPolicy
        << (state.commandLineOverride ? " (command-line override)" : "")
        << "\r\nNext-start preference: " << state.nextStartupPolicy << "\r\nMode: " << state.redMode
        << "\r\nLuma detail: " << state.lumaProfile
        << "\r\nPreferred red mode: " << state.preferredRedMode << "\r\nPreferred Luma detail: " << state.preferredLumaProfile
        << "\r\nState: "
        << FilterStatusDescription(state.active, state.needsRestore, !state.lastError.empty(), state.verification, state.restoreRequested)
        << "\r\nBackend verification: " << BackendVerificationName(state.verification);
    if (state.readback) {
        const auto& readback = *state.readback;
        report << "\r\nRead-back values/mismatches: " << readback.values << " / " << readback.mismatches
            << "\r\nLargest finite difference: " << std::setprecision(9) << readback.maximumDifference
            << (readback.gammaRamp ? " (gamma fraction of full scale)" : " (matrix coefficient)")
            << "\r\nNonfinite values: " << (readback.nonFiniteValues ? "yes" : "no")
            << "\r\nUnexpected green/blue entries: " << (readback.unexpectedGreenBlue ? "yes" : "no");
    }
    report << "\r\nVerification error code: " << state.verificationError
        << "\r\nSettings persistence: " << (state.preferencesUnsaved ? "unsaved changes" : "saved/default")
        << "\r\nStartup preference source: " << state.startupPreferenceSource
        << "\r\nStartup preference load warnings: " << (state.startupPreferenceWarning ? "yes" : "no")
        << "\r\nRecovery data pending: " << (state.needsRestore ? "yes" : "no")
        << "\r\nControl restore requested: " << (state.restoreRequested ? "yes" : "no")
        << "\r\nShortcut preference: " << state.shortcutProfile
        << "\r\nToggle shortcut: " << (state.toggleHotkey ? "registered" : "unavailable") << " (" << state.toggleBinding << ")"
        << "\r\nRestore shortcut: " << (state.restoreHotkey ? "registered" : "unavailable") << " (" << state.restoreBinding << ")"
        << "\r\nSession notifications: " << (state.sessionNotifications ? "registered" : "unavailable")
        << "\r\nLast session-notification error: " << state.sessionNotificationError
        << "\r\nSession limit: ";
    if (state.sessionMinutes) report << state.sessionMinutes << " minutes";
    else report << "unlimited";
    report << "\r\nExpiry warning: " << state.expiryWarning << "\r\nCountdown: ";
    if (state.remainingSeconds) report << *state.remainingSeconds << " seconds remaining";
    else report << "not armed";
    report << "\r\nBackend counters since startup/last reset (requests / failures):";
    const auto counts = [&](const char* name, const BackendOperationStatistics& value) {
        report << "\r\n  " << name << ": " << value.requests << " / " << value.failures;
    };
    counts("Initialize", state.backendStatistics.initialization);
    counts("Activate", state.backendStatistics.activation);
    counts("Restore/ownership cleanup", state.backendStatistics.restoration);
    counts("Verify (mismatch/unavailable counted as failures)", state.backendStatistics.verification);
    counts("Shutdown", state.backendStatistics.shutdown);
    report << "\r\nCounters include idempotent requests and nested recovery calls."
        << "\r\nFile logging: " << state.fileLogging << "\r\nMissing file records: " << state.droppedLogLines
        << "\r\nTruncated records: " << state.truncatedLogLines << "\r\nLast file error: " << state.lastFileError
        << "\r\nLast backend error: " << (state.lastError.empty() ? "none" : state.lastError)
        << "\r\nStatus reflects backend call results; no display output is measured.\r\n";
    return report.str();
}
