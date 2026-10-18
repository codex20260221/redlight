#pragma once
#include "DisplayFilter.h"
#include "ShortcutDiagnostics.h"
#include "RestorationHistory.h"
#include "SessionTimer.h"
#include "DisplayContext.h"
#include "Diagnostics.h"
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
    std::optional<std::uint64_t> verificationAgeSeconds, verificationDurationMilliseconds;
    bool commandLineOverride = false;
    bool preferencesUnsaved = false;
    std::string_view startupPreferenceSource = "Not reported";
    bool startupPreferenceWarning = false;
    std::string_view shortcutProfile = "Default", toggleBinding = "none", restoreBinding = "none";
    bool shortcutProfileApplied = false;
    std::optional<ShortcutApiFailure> shortcutFailure;
    bool shortcutRollbackIncomplete = false;
    std::string_view fileLogging = "Not reported";
    std::uint64_t droppedLogLines = 0, truncatedLogLines = 0;
    std::uint32_t lastFileError = 0;
    std::optional<Diagnostics::RecentLogStatus> recentLog;
    bool active = false, needsRestore = false;
    bool restoreRequested = false;
    bool toggleHotkey = false, restoreHotkey = false, sessionNotifications = false;
    std::uint32_t sessionNotificationError = 0;
    unsigned sessionMinutes = 0;
    std::string_view expiryWarning = "Off";
    BackendStatistics backendStatistics{};
    std::optional<std::uint64_t> remainingSeconds;
    std::optional<NativeCountdownTimerState> nativeCountdownTimer;
    bool expiryRestoreAttempted = false, expiryWarningAttempted = false;
    std::optional<std::uint64_t> expiryRetryMilliseconds;
    RestorationHistory restorationHistory;
    std::uint64_t reportTick = 0;
    DisplayContextCache displayContext;
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
    report << "\r\nVerification age: ";
    if (state.verificationAgeSeconds) report << *state.verificationAgeSeconds << " seconds since check completed";
    else report << "not recorded";
    report << "\r\nVerification duration: ";
    if (state.verificationDurationMilliseconds) report << *state.verificationDurationMilliseconds << " milliseconds";
    else report << "not recorded";
    if (state.readback) {
        const auto& readback = *state.readback;
        report << "\r\nRead-back values/mismatches: " << readback.values << " / " << readback.mismatches
            << "\r\nLargest finite difference: " << std::setprecision(9) << readback.maximumDifference
            << (readback.gammaRamp ? " (gamma fraction of full scale)" : " (matrix coefficient)")
            << "\r\nNonfinite values: " << (readback.nonFiniteValues ? "yes" : "no")
            << "\r\nUnexpected green/blue entries: " << (readback.unexpectedGreenBlue ? "yes" : "no");
        if (readback.gammaRamp) report << "\r\nRed black endpoint mismatch: " << (readback.redBlackEndpointMismatch ? "yes" : "no")
            << "\r\nRed white endpoint mismatch: " << (readback.redWhiteEndpointMismatch ? "yes" : "no");
    }
    report << "\r\nVerification error code: " << state.verificationError
        << "\r\nSettings persistence: " << (state.preferencesUnsaved ? "unsaved changes" : "saved/default")
        << "\r\nStartup preference source: " << state.startupPreferenceSource
        << "\r\nStartup preference load warnings: " << (state.startupPreferenceWarning ? "yes" : "no")
        << "\r\nRecovery data pending: " << (state.needsRestore ? "yes" : "no")
        << "\r\nControl restore requested: " << (state.restoreRequested ? "yes" : "no")
        << "\r\nShortcut preference: " << state.shortcutProfile
        << "\r\nShortcut preference applied: " << (state.shortcutProfileApplied ? "yes" : "no")
        << "\r\nLast shortcut API failure: ";
    if (state.shortcutFailure) {
        const auto& failure = *state.shortcutFailure;
        report << ShortcutApiName(failure.operation) << " (id " << failure.id << ", error " << failure.errorCode
            << ", rollback " << (failure.duringRollback ? "yes" : "no") << ")";
    } else report << "none";
    report << "\r\nShortcut rollback incomplete: " << (state.shortcutRollbackIncomplete ? "yes" : "no")
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
    report << "\r\nNative countdown timer: " << (state.nativeCountdownTimer ? NativeCountdownTimerStateName(*state.nativeCountdownTimer) : "Not reported")
        << "\r\nExpiry warning attempted this countdown: " << (state.expiryWarningAttempted ? "yes" : "no")
        << "\r\nExpiry restoration attempted this countdown: " << (state.expiryRestoreAttempted ? "yes" : "no")
        << "\r\nExpiry retry policy: ";
    if (!state.expiryRetryMilliseconds) report << "not applicable (countdown off or unexpired)";
    else if (*state.expiryRetryMilliseconds == 0) report << "retry allowed now (message processing/busy state may delay)";
    else report << *state.expiryRetryMilliseconds << " milliseconds until retry allowed";
    report << "\r\nBackend counters since startup/last reset (requests / failures):";
    const auto counts = [&](const char* name, const BackendOperationStatistics& value) {
        report << "\r\n  " << name << ": " << value.requests << " / " << value.failures;
        const auto& timing = value.timing;
        if (timing.completed) report << "; timed " << timing.completed << ", last " << timing.lastMilliseconds
            << " ms, peak " << timing.maximumMilliseconds << " ms, inclusive total " << timing.totalMilliseconds << " ms";
        else report << "; timing not recorded";
    };
    counts("Initialize", state.backendStatistics.initialization);
    counts("Activate", state.backendStatistics.activation);
    counts("Restore/ownership cleanup", state.backendStatistics.restoration);
    counts("Verify (mismatch/unavailable counted as failures)", state.backendStatistics.verification);
    counts("Shutdown", state.backendStatistics.shutdown);
    report << "\r\nCounters include idempotent requests and nested recovery calls."
        << "\r\nTiming uses elapsed tick milliseconds, includes failures/no-ops/nested calls, and overlaps across categories."
        << "\r\nCached Windows display context: ";
    if (!state.displayContext.attempted()) report << "not captured";
    else report << "last refresh " << DisplayContextReasonName(state.displayContext.lastReason()) << ", "
        << (state.displayContext.lastFailed() ? "unavailable (prior successful data retained if present)" : "succeeded");
    if (state.displayContext.snapshot()) {
        const auto& context = *state.displayContext.snapshot();
        report << "\r\nVisible monitors: " << context.monitors << "\r\nVirtual desktop metrics: " << context.virtualWidth << " x " << context.virtualHeight << " pixels"
            << "\r\nRemote-session metric: " << (context.remoteSessionIndicated ? "set" : "not set")
            << "\r\nContext captured after: " << DisplayContextReasonName(state.displayContext.captureReason())
            << "\r\nContext age: " << state.displayContext.ageSeconds(state.reportTick).value_or(0) << " seconds";
    }
    report << "\r\nContext is logical cached Windows data; it does not qualify HDR, per-monitor backend coverage, or physical output."
        << "\r\nRecent tray restoration requests (newest completed scope first; age since request):";
    if (!state.restorationHistory.size()) report << "\r\n  none recorded";
    for (std::size_t index = 0; index < state.restorationHistory.size(); ++index) {
        const auto& event = *state.restorationHistory.newest(index);
        report << "\r\n  " << RestoreRequestAgeSeconds(event, state.reportTick) << " seconds ago: " << RestoreReasonName(event.reason)
            << " - " << RestoreOutcomeName(event.outcome) << " (request duration: ";
        if (event.durationMilliseconds) report << *event.durationMilliseconds << " milliseconds";
        else report << "not recorded";
        report << ")";
    }
    report << "\r\nOlder restoration records overwritten: " << state.restorationHistory.overwritten()
        << "\r\nRestoration request scopes in progress: " << state.restorationHistory.inProgress()
        << "\r\nRequest durations include control/UI work, dialog wait, and nested scopes; nested times overlap. Deferred attempts/replay are separate scopes."
        << "\r\nHistory is in-memory control activity; it does not measure display output.";
    report << "\r\nRecent diagnostic memory: ";
    if (state.recentLog && state.recentLog->capacity && state.recentLog->retainedRecords <= state.recentLog->capacity) {
        const auto& recent = *state.recentLog;
        report << recent.retainedRecords << " / " << recent.capacity << " records retained; " << recent.overwrittenRecords << " earlier records overwritten";
    } else report << "not reported";
    report << "\r\nFile logging: " << state.fileLogging << "\r\nMissing file records: " << state.droppedLogLines
        << "\r\nTruncated records: " << state.truncatedLogLines << "\r\nLast file error: " << state.lastFileError
        << "\r\nLast backend error: " << (state.lastError.empty() ? "none" : state.lastError)
        << "\r\nStatus reflects backend call results; no display output is measured.\r\n";
    return report.str();
}
