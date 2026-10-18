#pragma once
#include "CommandIds.h"
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
inline bool CommandMayChangeFilter(unsigned command) noexcept {
    if (command >= ID_EXPIRY_WARNING_BASE && command <= ID_EXPIRY_WARNING_BASE + 3) return false;
    switch (command) {
    case ID_TRAY_ABOUT:
    case ID_TRAY_LOGS:
    case ID_TRAY_COPY_REPORT:
    case ID_TRAY_COPY_PROFILE:
    case ID_TRAY_VERIFY_BACKEND:
    case ID_TRAY_RETRY_LOGGING:
    case ID_TRAY_RESET_BACKEND_COUNTS:
    case ID_TRAY_SAVE_PREFERENCES:
        return false;
    default:
        return true;
    }
}
// Never unwind a C++ exception across a native window callback boundary.
template<class Result, class Action, class Failure>
Result GuardUiCall(Action&& action, Failure&& failure) noexcept {
    try { return action(); }
    catch (const std::exception& error) {
        try { return failure(error.what()); } catch (...) { return Result{}; }
    } catch (...) {
        try { return failure("Unknown C++ exception"); } catch (...) { return Result{}; }
    }
}
