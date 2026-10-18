#pragma once
#include "SessionTimer.h"
#include "DisplayFilter.h"

inline bool CanChangeSessionLimit(const SessionTimer& timer, std::uint64_t now, const DisplayFilter& filter,
    BackendVerification verification, bool restoreRequested) {
    const bool active = filter.isActive();
    if (restoreRequested || (filter.needsRestore() && !active)) return false;
    if (!active) return true;
    if (filter.lastError()[0] || (verification != BackendVerification::NotChecked && verification != BackendVerification::Inactive && verification != BackendVerification::Matches)) return false;
    // Once the deadline is reached, preserve restoration and its retry cadence.
    return !timer.armed() || timer.remainingMilliseconds(now) != 0;
}
inline bool CanRestartCountdown(const SessionTimer& timer, std::uint64_t now, const DisplayFilter& filter,
    BackendVerification verification, bool restoreRequested) {
    return CanChangeSessionLimit(timer, now, filter, verification, restoreRequested) && timer.canRestart(now, filter.isActive());
}
