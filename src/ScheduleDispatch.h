#pragma once
#include "ScheduleRuntime.h"
#include "ScheduleSession.h"

namespace Scheduling {
enum class EnableResult { Skipped, AlreadyActive, SessionBlocked, Stale, Requested };
template<class Allowed, class Session, class Tick, class Enable>
EnableResult DispatchEnable(DisplayFilter& filter, const ScheduleClockReading& clock, Allowed&& allowed,
    Session&& session, Tick&& tick, Enable&& enable) {
    if (!allowed()) return EnableResult::Skipped;
    if (filter.isActive()) return EnableResult::AlreadyActive;
    const auto ready = session();
    if (ready.state != ScheduleSessionState::Ready) return EnableResult::SessionBlocked;
    const auto elapsed = tick() - clock.startedTick;
    if (clock.millisecondsInMinute >= 60000 || elapsed > 2000 || elapsed >= 60000 - clock.millisecondsInMinute) return EnableResult::Stale;
    if (!allowed()) return EnableResult::Skipped; // A restore/callback may have run during the session query.
    if (filter.isActive()) return EnableResult::AlreadyActive;
    enable(); // Request only; actual activation success remains the backend's responsibility.
    return EnableResult::Requested;
}
}
