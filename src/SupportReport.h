#pragma once
#include "DisplayFilter.h"
#include "ShortcutDiagnostics.h"
#include "RestorationHistory.h"
#include "SessionTimer.h"
#include "CountdownTimerDiagnostics.h"
#include "ReadbackLocation.h"
#include "DisplayContext.h"
#include "Diagnostics.h"
#include "UtcTimestamp.h"
#include "ProcessContext.h"
#include "SessionNotificationDiagnostics.h"
#include "SchedulePreferences.h"
#include <cstdint>
#include <optional>
#include <sstream>
#include <iomanip>
#include <limits>
#include <string>
#include <string_view>

struct SupportReportState {
    std::string_view version, backend, startupPolicy, nextStartupPolicy, redMode, lumaProfile, lastError;
    std::string_view preferredRedMode = "Not reported", preferredLumaProfile = "Not reported";
    std::optional<unsigned> brightnessPercent, preferredBrightnessPercent;
    std::optional<bool> adjustableBrightness;
    std::string_view preferredFadeIn = "Not reported", fadeDuration = "Not reported";
    bool fadeActive = false;
    std::optional<unsigned> fadeTarget;
    std::optional<bool> fadeTimerInstalled;
    std::optional<CountdownTimerFailure> fadeTimerFailure;
    std::optional<Scheduling::DailyTimes> dailySchedule;
    std::string_view scheduleStatus = "Not reported";
    std::optional<bool> scheduleArmed, scheduleTimerInstalled;
    std::uint32_t scheduleError = 0;
    std::optional<CountdownTimerFailure> scheduleTimerFailure;
    std::string_view windowsStartupEntry = "Not reported";
    std::uint32_t windowsStartupReadError = 0, windowsStartupChangeError = 0;
    std::string_view windowsStartupPath = "Not reported";
    std::uint32_t windowsStartupPathError = 0;
    std::string_view buildRevision = "unversioned", sourceState = "unknown";
    std::string_view backendLifecycle = "Not reported";
    std::string_view startupFallbackReason = "Not reported";
    std::optional<UtcTimestamp> capturedUtc;
    std::optional<ProcessContextSnapshot> processContext;
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
    std::optional<SessionNotificationFailure> sessionNotificationFailure;
    unsigned sessionMinutes = 0;
    std::string_view expiryWarning = "Off";
    BackendStatistics backendStatistics{};
    std::optional<std::uint64_t> remainingSeconds;
    std::optional<NativeCountdownTimerState> nativeCountdownTimer;
    std::optional<CountdownTimerFailure> countdownTimerFailure;
    bool expiryRestoreAttempted = false, expiryWarningAttempted = false;
    std::optional<std::uint64_t> expiryRetryMilliseconds;
    std::optional<std::uint64_t> expiryOverdueMilliseconds;
    RestorationHistory restorationHistory;
    std::uint64_t reportTick = 0;
    DisplayContextCache displayContext;
};
inline std::string BuildSupportReport(const SupportReportState& state) {
    std::ostringstream report;
    report.exceptions(std::ios::badbit | std::ios::failbit);
    report << "RedLight support report\r\nCaptured UTC: ";
    if (state.capturedUtc) {
        char timestamp[UtcTimestampBufferSize];
        report << FormatUtcTimestamp(*state.capturedUtc, timestamp).value_or("unavailable (invalid UTC calendar fields)");
    } else report << "not recorded";
    report << "\r\nStartup process ID: ";
    if (state.processContext) {
        report << state.processContext->processId << "\r\nStartup Windows session ID: ";
        if (state.processContext->sessionId) report << *state.processContext->sessionId;
        else if (state.processContext->sessionError) report << "unavailable (native error " << state.processContext->sessionError << ")";
        else report << "unavailable (no native error code)";
    } else report << "not recorded\r\nStartup Windows session ID: not recorded";
    report << "\r\nVersion: " << state.version << " (Windows x64)\r\n"
        << "Build revision: " << state.buildRevision << " (" << state.sourceState << ")\r\n"
        << "Backend: " << state.backend << "\r\nBackend lifecycle: " << state.backendLifecycle << "\r\nStartup policy: " << state.startupPolicy
        << (state.commandLineOverride ? " (command-line override)" : "")
        << "\r\nAutomatic startup fallback reason: " << state.startupFallbackReason
        << "\r\nNext-start preference: " << state.nextStartupPolicy << "\r\nMode: " << state.redMode
        << "\r\nLuma detail: " << state.lumaProfile
        << "\r\nPreferred red mode: " << state.preferredRedMode << "\r\nPreferred Luma detail: " << state.preferredLumaProfile
        << "\r\nRed brightness selection: ";
    if (state.brightnessPercent) report << *state.brightnessPercent << '%';
    else report << "not reported";
    report << "\r\nPreferred red brightness: ";
    if (state.preferredBrightnessPercent) report << *state.preferredBrightnessPercent << '%';
    else report << "not reported";
    report << "\r\nAdjustable brightness on current backend: ";
    if (state.adjustableBrightness) report << (*state.adjustableBrightness ? "yes" : "no; fixed at 100% (preference retained for Magnification)");
    else report << "not reported";
    report << "\r\nFade-in next activation (Magnification only): " << state.preferredFadeIn
        << "\r\nFade progress: " << (state.fadeActive ? "running" : "not running");
    if (state.fadeActive && state.fadeTarget) report << " toward " << *state.fadeTarget << "% over " << state.fadeDuration;
    report << "\r\nNative fade timer: ";
    if (state.fadeTimerInstalled) report << (*state.fadeTimerInstalled ? "installed (may remain after failed cleanup)" : "not installed");
    else report << "not reported";
    report << "\r\nLast fade timer API failure (retained after retry): ";
    if (state.fadeTimerFailure) report << CountdownTimerApiName(state.fadeTimerFailure->operation) << " (error " << state.fadeTimerFailure->errorCode << ")";
    else report << "none recorded";
    report << "\r\nDaily schedule preference: ";
    if (state.dailySchedule && state.dailySchedule->valid()) {
        char enableAt[6], restoreAt[6];
        if (Scheduling::FormatMinute(state.dailySchedule->enableMinute, enableAt) && Scheduling::FormatMinute(state.dailySchedule->restoreMinute, restoreAt))
            report << (state.dailySchedule->enabled ? "daily" : "Off") << " (" << enableAt << " enable / " << restoreAt << " restore; Windows local time)";
        else report << "invalid";
    } else report << "not reported";
    report << "\r\nSchedule runtime armed: ";
    if (state.scheduleArmed) report << (*state.scheduleArmed ? "yes (future boundaries only)" : "no"); else report << "not reported";
    report << "\r\nSchedule status/last result: " << state.scheduleStatus << "\r\nSchedule native error: " << state.scheduleError << "\r\nNative schedule timer: ";
    if (state.scheduleTimerInstalled) report << (*state.scheduleTimerInstalled ? "owned (may remain after failed cleanup)" : "not installed"); else report << "not reported";
    report << "\r\nLast schedule timer API failure (retained after retry): ";
    if (state.scheduleTimerFailure) report << CountdownTimerApiName(state.scheduleTimerFailure->operation) << " (error " << state.scheduleTimerFailure->errorCode << ")";
    else report << "none recorded";
    report << "\r\nState: "
        << FilterStatusDescription(state.active, state.needsRestore, !state.lastError.empty(), state.verification, state.restoreRequested)
        << "\r\nBackend verification: " << BackendVerificationName(state.verification);
    report << "\r\nStart with Windows entry: " << state.windowsStartupEntry
        << "\r\nStartup entry read/change errors: " << state.windowsStartupReadError << " / " << state.windowsStartupChangeError
        << "\r\nStartup executable path: " << state.windowsStartupPath << " (error " << state.windowsStartupPathError << ")";
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
        char first[ReadbackLocationBufferSize], maximum[ReadbackLocationBufferSize];
        const auto firstLocation = FormatReadbackLocation(readback.firstMismatch, readback.gammaRamp, first);
        const auto maximumLocation = FormatReadbackLocation(readback.maximumDifferenceAt, readback.gammaRamp, maximum);
        report << "\r\nFirst mismatch location: " << firstLocation.value_or("unavailable (invalid cached location)")
            << "\r\nLargest finite difference location: " << maximumLocation.value_or("unavailable (invalid cached location)");
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
        << "\r\nLast session-notification error: " << state.sessionNotificationError;
    report << "\r\nLast session-notification API failure: ";
    if (state.sessionNotificationFailure) {
        report << SessionNotificationApiName(state.sessionNotificationFailure->operation);
        if (state.sessionNotificationFailure->errorCode) report << " (native error " << state.sessionNotificationFailure->errorCode << ")";
        else report << " (no native error code)";
    } else report << "none recorded";
    report << "\r\nSession limit: ";
    if (state.sessionMinutes) report << state.sessionMinutes << " minutes";
    else report << "unlimited";
    report << "\r\nExpiry warning: " << state.expiryWarning << "\r\nCountdown: ";
    if (state.remainingSeconds) report << *state.remainingSeconds << " seconds remaining";
    else report << "not armed";
    report << "\r\nNative countdown timer: " << (state.nativeCountdownTimer ? NativeCountdownTimerStateName(*state.nativeCountdownTimer) : "Not reported")
        << "\r\nLast countdown timer API failure (retained after retry): ";
    if (state.countdownTimerFailure) {
        report << CountdownTimerApiName(state.countdownTimerFailure->operation);
        if (state.countdownTimerFailure->errorCode) report << " (error " << state.countdownTimerFailure->errorCode << ")";
        else report << " (no native error code)";
    } else report << "none recorded";
    report << "\r\nExpiry warning attempted this countdown: " << (state.expiryWarningAttempted ? "yes" : "no")
        << "\r\nExpiry restoration attempted this countdown: " << (state.expiryRestoreAttempted ? "yes" : "no")
        << "\r\nExpiry retry policy: ";
    if (!state.expiryRetryMilliseconds) report << "not applicable (countdown off or unexpired)";
    else if (*state.expiryRetryMilliseconds == 0) report << "retry allowed now (message processing/busy state may delay)";
    else report << *state.expiryRetryMilliseconds << " milliseconds until retry allowed";
    report << "\r\nCountdown overdue: ";
    if (state.expiryOverdueMilliseconds) report << *state.expiryOverdueMilliseconds << " milliseconds since session deadline (processing/restoration may be delayed)";
    else report << "not recorded (not expired or not armed)";
    report << "\r\nBackend counters since startup/last reset (requests / failures):";
    const auto counts = [&](const char* name, const BackendOperationStatistics& value) {
        report << "\r\n  " << name << ": " << value.requests << " / " << value.failures;
        const auto& timing = value.timing;
        if (timing.completed) {
            const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
            report << "; timed " << timing.completed;
            if (timing.completed == maximum) report << " (at limit)";
            report << ", last " << timing.lastMilliseconds << " ms, peak " << timing.maximumMilliseconds << " ms, inclusive total " << timing.totalMilliseconds << " ms";
            if (timing.totalMilliseconds == maximum) report << " (at limit)";
            if (const auto mean = BackendMeanMilliseconds(timing)) {
                const auto flags = report.flags(); const auto precision = report.precision();
                report << "; mean " << std::fixed << std::setprecision(3) << *mean << " ms";
                report.flags(flags); report.precision(precision);
            } else report << "; mean unavailable (count or total at limit)";
        } else report << "; timing not recorded";
    };
    counts("Initialize", state.backendStatistics.initialization);
    counts("Activate", state.backendStatistics.activation);
    counts("Restore/ownership cleanup", state.backendStatistics.restoration);
    counts("Verify (mismatch/unavailable counted as failures)", state.backendStatistics.verification);
    counts("Shutdown", state.backendStatistics.shutdown);
    report << "\r\nCounters include idempotent requests and nested recovery calls."
        << "\r\nTiming uses elapsed tick milliseconds, includes failures/no-ops/nested calls, and overlaps across categories."
        << "\r\nMeans use completed timed scopes only; count/total limits make the mean unavailable."
        << "\r\nCached Windows display context: ";
    if (!state.displayContext.attempted()) report << "not captured";
    else report << "last refresh " << DisplayContextReasonName(state.displayContext.lastReason()) << ", "
        << (state.displayContext.lastFailed() ? "unavailable (prior successful data retained if present)" : "succeeded");
    if (state.displayContext.snapshot()) {
        const auto& context = *state.displayContext.snapshot();
        report << "\r\nVisible monitors: " << context.monitors << "\r\nVirtual desktop metrics: " << context.virtualWidth << " x " << context.virtualHeight << " pixels"
            << "\r\nVirtual desktop origin (left/top): " << context.virtualLeft << ", " << context.virtualTop << " pixels"
            << "\r\nVirtual desktop right/bottom (exclusive): " << context.virtualRight() << ", " << context.virtualBottom() << " pixels"
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
