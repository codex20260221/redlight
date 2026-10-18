#pragma once
#include "RedFadeIn.h"
#include "DisplayFilter.h"
#include "VerificationCache.h"
#include "SessionTimer.h"

inline bool WantsFadeIn(const DisplayFilter& filter, unsigned target, FadeInDuration duration) noexcept {
    return filter.supportsBrightness() && ValidRedBrightness(target) && target > 1 &&
        ValidFadeInDuration(duration) && duration != FadeInDuration::Off;
}
inline bool PrepareFadeInActivation(DisplayFilter& filter, unsigned target, FadeInDuration duration) {
    if (!ValidRedBrightness(target) || !ValidFadeInDuration(duration) || filter.isActive() || filter.needsRestore()) return false;
    if (!filter.supportsBrightness()) return true; // Gamma preference-only; never alter its error/ramp/ownership.
    return filter.setBrightnessPercent(WantsFadeIn(filter, target, duration) ? 1 : target);
}
inline bool StartFadeInAfterEnable(RedFadeIn& fade, const DisplayFilter& filter, unsigned target, FadeInDuration duration, std::uint64_t now) {
    fade.cancel();
    return WantsFadeIn(filter, target, duration) && filter.isActive() && filter.needsRestore() &&
        filter.brightnessPercent() == 1 && !filter.lastError()[0] && fade.start(target, duration, now);
}
enum class FadeInStep { Idle, Waiting, Applied, Completed, Cancelled, BackendFailed, Expired };
inline FadeInStep AdvanceFadeIn(RedFadeIn& fade, DisplayFilter& filter, BackendVerificationCache& verification,
    const SessionTimer& session, std::uint64_t now, bool busy, bool restoreRequested) {
    if (!fade.active()) return FadeInStep::Idle;
    if (restoreRequested) { fade.cancel(); return FadeInStep::Cancelled; }
    if (busy) return FadeInStep::Waiting;
    if (session.armed() && session.remainingMilliseconds(now) == 0) { fade.cancel(); return FadeInStep::Expired; }
    if (!filter.supportsBrightness() || !filter.isActive() || !filter.needsRestore() || filter.lastError()[0] ||
        filter.brightnessPercent() != fade.confirmedPercent() || verification.result().state == BackendVerification::Changed ||
        verification.result().state == BackendVerification::Unavailable) { fade.cancel(); return FadeInStep::Cancelled; }
    const auto wanted = fade.due(now);
    if (!wanted) return FadeInStep::Waiting;
    verification.beforeBrightnessSelection(filter, *wanted);
    if (!filter.setBrightnessPercent(*wanted)) { fade.cancel(); return FadeInStep::BackendFailed; }
    // A nested restoration request may have canceled the plan during the API.
    if (!fade.confirm(*wanted)) return FadeInStep::Cancelled;
    return fade.active() ? FadeInStep::Applied : FadeInStep::Completed;
}
