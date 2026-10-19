#pragma once
#include <cstdint>

enum class ScheduleSessionState { Ready, LockedOrInactive, Unavailable };
struct ScheduleSessionResult {
    ScheduleSessionState state = ScheduleSessionState::Unavailable;
    std::uint32_t nativeError = 0;
};
// Current local calling session only. No activation, registration, or persistent state.
ScheduleSessionResult ReadScheduleSession() noexcept;
