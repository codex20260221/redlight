#pragma once
#include "DailySchedule.h"
#include "ScheduleClock.h"
#include "DisplayFilter.h"
#include "SessionTimer.h"
#include <limits>

namespace Scheduling {
class Runtime {
public:
    bool configure(DailyTimes times, const std::optional<ScheduleClockReading>& reading = std::nullopt, bool newLifetime = true) noexcept {
        if (!times.valid() || (times.enabled && !reading)) return false;
        DailySchedule policy = policy_;
        if ((newLifetime || times != settings_) && !policy.configure(times)) return false;
        if (times.enabled && !policy.rebase(sample(*reading))) return false;
        policy_ = policy; enabled_ = times.enabled; settings_ = times;
        zone_ = reading ? std::optional<ScheduleZoneIdentity>(reading->zone) : std::nullopt;
        return true;
    }
    void invalidate() noexcept { policy_.invalidate(); zone_.reset(); }
    void stop() noexcept {
        enabled_ = false; invalidate();
        if (stopRevision_ != (std::numeric_limits<std::uint64_t>::max)()) ++stopRevision_;
    }
    std::uint64_t stopRevision() const noexcept { return stopRevision_; }
    bool stoppedSince(std::uint64_t revision) const noexcept {
        return stopRevision_ != revision || stopRevision_ == (std::numeric_limits<std::uint64_t>::max)();
    }
    bool enabled() const noexcept { return enabled_; }
    bool hasBaseline() const noexcept { return zone_.has_value(); }
    Event observe(const ScheduleClockResult& clock, bool allowEnable) noexcept {
        if (!enabled_) return Event::None;
        if (!clock.reading) { invalidate(); return Event::InvalidSample; }
        const bool continuous = zone_ && *zone_ == clock.reading->zone;
        zone_ = clock.reading->zone;
        return policy_.observe(sample(*clock.reading), allowEnable, continuous);
    }
private:
    static Sample sample(const ScheduleClockReading& value) noexcept {
        return {value.year, value.month, value.day, value.minute, value.millisecondsInMinute, value.utcMilliseconds, value.awakeMilliseconds};
    }
    DailySchedule policy_;
    DailyTimes settings_;
    std::optional<ScheduleZoneIdentity> zone_;
    bool enabled_ = false;
    std::uint64_t stopRevision_ = 0;
};
// Modal/configuration intervals are not catch-up windows. A caller that has
// published a fresh post-transaction baseline can retain it explicitly.
class ObservationPause {
public:
    explicit ObservationPause(Runtime& runtime) noexcept : runtime_(runtime) { runtime_.invalidate(); }
    ~ObservationPause() { if (!fresh_) runtime_.invalidate(); }
    ObservationPause(const ObservationPause&) = delete;
    ObservationPause& operator=(const ObservationPause&) = delete;
    void keepFresh() noexcept { fresh_ = true; }
private:
    Runtime& runtime_;
    bool fresh_ = false;
};
inline bool CanEnable(const DisplayFilter& filter, const SessionTimer& countdown, std::uint64_t now,
    BackendVerification verification, bool busy, bool preferencesApplying, bool restoreRequested,
    bool trayReady, bool hooksReady, bool timerInstalled, bool suspended) {
    return !busy && !preferencesApplying && !restoreRequested && trayReady && hooksReady && timerInstalled && !suspended &&
        !filter.lastError()[0] && (!filter.needsRestore() || filter.isActive()) &&
        (verification == BackendVerification::NotChecked || verification == BackendVerification::Inactive || verification == BackendVerification::Matches) &&
        (!countdown.armed() || countdown.remainingMilliseconds(now) != 0);
}
}
