#include "ScheduleSession.h"
#include <windows.h>
#include <wtsapi32.h>
#include <cstring>

ScheduleSessionResult ReadScheduleSession() noexcept {
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    SetLastError(ERROR_SUCCESS);
    const bool queried = WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &buffer, &bytes) != FALSE;
    const auto error = queried ? ERROR_SUCCESS : GetLastError(); // Capture before WTSFreeMemory.
    WTSINFOEXW info = {};
    const bool complete = queried && buffer && bytes >= sizeof(info);
    if (complete) std::memcpy(&info, buffer, sizeof(info)); // Do not assume a caller-supplied buffer's alignment.
    if (buffer) WTSFreeMemory(buffer); // Void API; no last-error contract.
    if (!queried) return {ScheduleSessionState::Unavailable, static_cast<std::uint32_t>(error)};
    if (!complete || info.Level != 1) return {};
    const auto& session = info.Data.WTSInfoExLevel1;
    std::uint32_t state = 0;
    static_assert(sizeof(state) == sizeof(session.SessionState));
    std::memcpy(&state, &session.SessionState, sizeof(state));
    if (state > WTSInit ||
        (session.SessionFlags != WTS_SESSIONSTATE_LOCK && session.SessionFlags != WTS_SESSIONSTATE_UNLOCK)) return {};
    return {state == WTSActive && session.SessionFlags == WTS_SESSIONSTATE_UNLOCK ?
        ScheduleSessionState::Ready : ScheduleSessionState::LockedOrInactive};
}
