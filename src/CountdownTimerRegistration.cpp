#include "CountdownTimerRegistration.h"
#include "Diagnostics.h"

bool CountdownTimerRegistration::failed(CountdownTimerApi operation, DWORD error) noexcept {
    lastFailure_ = CountdownTimerFailure{operation, error};
    if (error) Diagnostics::LogFormat("%s(countdown) failed or was rejected (error %lu); native ownership unchanged.", CountdownTimerApiName(operation), error);
    else Diagnostics::LogFormat("%s(countdown) failed; no native error code was provided; native ownership unchanged.", CountdownTimerApiName(operation));
    return false;
}
bool CountdownTimerRegistration::install(HWND owner, UINT_PTR id) noexcept {
    if (!owner || id == 0) return failed(CountdownTimerApi::Install, ERROR_INVALID_PARAMETER);
    if (installed() && (owner != owner_ || id != id_)) return failed(CountdownTimerApi::Install, ERROR_INVALID_STATE);
    SetLastError(ERROR_SUCCESS);
    if (!SetTimer(owner, id, 1000, nullptr)) {
        const DWORD error = GetLastError();
        return failed(CountdownTimerApi::Install, error);
    }
    // For a nonnull window, KillTimer requires the requested id, not a new
    // thread-timer identifier. Replacing a timer retains the same ownership.
    owner_ = owner;
    id_ = id;
    return true;
}
bool CountdownTimerRegistration::remove() noexcept {
    if (!installed()) return true;
    SetLastError(ERROR_SUCCESS);
    if (!KillTimer(owner_, id_)) {
        const DWORD error = GetLastError();
        return failed(CountdownTimerApi::Remove, error);
    }
    owner_ = nullptr;
    id_ = 0;
    return true;
}
