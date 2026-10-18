#pragma once
#include "CommandIds.h"
#include "StatusQuery.h"
#include "PreferencesQuery.h"
#include <exception>

class ScopedFlag {
public:
    explicit ScopedFlag(bool& flag) noexcept : flag_(flag), previous_(flag) { flag_ = true; }
    ~ScopedFlag() { flag_ = previous_; }
    ScopedFlag(const ScopedFlag&) = delete;
    ScopedFlag& operator=(const ScopedFlag&) = delete;
private:
    bool& flag_;
    bool previous_;
};
inline bool CommandMayChangeFilter(unsigned command, bool supportsLuma = true, bool supportsBrightness = true) noexcept {
    if (command >= ID_EXPIRY_WARNING_BASE && command <= ID_EXPIRY_WARNING_BASE + 3) return false;
    if (!supportsLuma && command >= ID_LUMA_PROFILE_BASE && command <= ID_LUMA_PROFILE_BASE + 2) return false;
    if (!supportsBrightness && ((command >= ID_RED_BRIGHTNESS_BASE + 1 && command <= ID_RED_BRIGHTNESS_BASE + 100) ||
        command == ID_TRAY_CUSTOM_BRIGHTNESS)) return false; // Gamma edits only a future Magnification preference.
    switch (command) {
    case ID_TRAY_ABOUT:
    case ID_TRAY_LOGS:
    case ID_TRAY_COPY_REPORT:
    case ID_TRAY_COPY_PROFILE:
    case ID_TRAY_VERIFY_BACKEND:
    case ID_TRAY_RETRY_LOGGING:
    case ID_TRAY_RESET_BACKEND_COUNTS:
    case ID_TRAY_SAVE_PREFERENCES:
    case ID_TRAY_RETRY_SESSION:
    case ID_TRAY_RETRY_SHORTCUTS:
    case ID_TRAY_CLEAR_RESTORATION_HISTORY:
    case ID_TRAY_COPY_STATUS_JSON:
    case ID_TRAY_REFRESH_DISPLAY_CONTEXT:
    case ID_TRAY_COPY_RECENT_DIAGNOSTICS:
    case ID_TRAY_CLEAR_RECENT_DIAGNOSTICS:
    case ID_TRAY_COPY_PREFERENCES_JSON:
    case ID_TRAY_COPY_RESTORATION_JSON:
    case ID_TRAY_COPY_VERIFICATION_JSON:
    case ID_TRAY_STARTUP_ENABLE:
    case ID_TRAY_STARTUP_REMOVE:
    case ID_TRAY_STARTUP_REFRESH:
        return false;
    default:
        return true;
    }
}
enum class UiFailureRecovery { None, Restore, Defer };
inline bool IsReadOnlyTrayQueryMessage(unsigned message) noexcept {
    return message == WM_REDLIGHT_STATUS || message == WM_REDLIGHT_PREFERENCES;
}
inline UiFailureRecovery RecoveryAfterUiFailure(bool mayChangeFilter, bool restoreRequested, bool busy) noexcept {
    if (!mayChangeFilter && !restoreRequested) return UiFailureRecovery::None;
    return busy ? UiFailureRecovery::Defer : UiFailureRecovery::Restore;
}
// Never unwind a C++ exception across a native window callback boundary.
template<class Result, class Action, class Failure>
Result GuardUiCall(Action&& action, Failure&& failure, Result fallback = Result{}) noexcept {
    try { return action(); }
    catch (const std::exception& error) {
        try { return failure(error.what()); } catch (...) { return fallback; }
    } catch (...) {
        try { return failure("Unknown C++ exception"); } catch (...) { return fallback; }
    }
}
