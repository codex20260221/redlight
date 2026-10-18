#pragma once
#include "CountdownTimerDiagnostics.h"
#include <windows.h>
#include <optional>

// Window-bound countdown only. Release explicitly while the window is valid;
// failed removal retains the exact owner/id for retry. No destructor API calls.
class CountdownTimerRegistration {
public:
    CountdownTimerRegistration() = default;
    CountdownTimerRegistration(const CountdownTimerRegistration&) = delete;
    CountdownTimerRegistration& operator=(const CountdownTimerRegistration&) = delete;
    bool install(HWND owner, UINT_PTR id) noexcept;
    bool remove() noexcept;
    bool installed() const noexcept { return owner_ != nullptr; }
    // Historical failure survives successful retry and no-op removal.
    std::optional<CountdownTimerFailure> lastFailure() const noexcept { return lastFailure_; }
private:
    bool failed(CountdownTimerApi operation, DWORD error) noexcept;
    HWND owner_ = nullptr;
    UINT_PTR id_ = 0;
    std::optional<CountdownTimerFailure> lastFailure_;
};
