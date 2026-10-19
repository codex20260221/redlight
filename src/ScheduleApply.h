#pragma once
#include "ScheduleRuntime.h"
#include "CountdownTimerRegistration.h"

namespace Scheduling {
enum class ApplyResult { Rejected, ClockUnavailable, TimerFailed, SaveFailed, Applied, AppliedCleanupPending, Interrupted, AppliedInterrupted, AppliedClockUnavailable };
// Prepare, confirm native timer, persist, then publish. Preferences are supplied
// by the caller so profiles/startup can use the same runtime transaction.
template<class Save, class RefreshClock>
ApplyResult Apply(Runtime& runtime, CountdownTimerRegistration& timer, HWND owner, UINT_PTR id,
    DailyTimes times, const std::optional<ScheduleClockReading>& clock, Save&& save, RefreshClock&& refreshClock, bool newLifetime = true) {
    if (!times.valid()) return ApplyResult::Rejected;
    Runtime prepared = runtime;
    if (!prepared.configure(times, clock, newLifetime)) return ApplyResult::ClockUnavailable;
    const auto stopRevision = runtime.stopRevision();
    const bool previouslyInstalled = timer.installed();
    if (times.enabled && !timer.install(owner, id)) {
        runtime.stop(); // A failed replacement cannot confirm that the old timer still runs.
        return ApplyResult::TimerFailed;
    }
    if (runtime.stoppedSince(stopRevision)) { timer.remove(); return ApplyResult::Interrupted; }
    if (!save()) {
        if (times.enabled && !previouslyInstalled) timer.remove(); // Failed cleanup remains owned; old logical state is retained.
        return ApplyResult::SaveFailed;
    }
    if (runtime.stoppedSince(stopRevision)) { timer.remove(); return ApplyResult::AppliedInterrupted; }
    if (times.enabled) {
        const auto fresh = refreshClock(); // Persistence/installation may have crossed a trigger minute.
        if (runtime.stoppedSince(stopRevision)) { timer.remove(); return ApplyResult::AppliedInterrupted; }
        if (!prepared.configure(times, fresh, false)) {
            runtime.stop(); timer.remove();
            return ApplyResult::AppliedClockUnavailable;
        }
    }
    runtime = prepared;
    if (!times.enabled && !timer.remove()) return ApplyResult::AppliedCleanupPending;
    return ApplyResult::Applied;
}
}
