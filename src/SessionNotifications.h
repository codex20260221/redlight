#pragma once
#include <windows.h>
#include "SessionNotificationDiagnostics.h"
#include <optional>

// The owner explicitly releases registration while its window is still valid.
class SessionNotifications {
public:
    SessionNotifications() = default;
    SessionNotifications(const SessionNotifications&) = delete;
    SessionNotifications& operator=(const SessionNotifications&) = delete;
    bool ensure(HWND owner);
    bool release();
    bool isRegistered() const noexcept { return registered_; }
    DWORD lastError() const noexcept { return lastError_; }
    std::optional<SessionNotificationFailure> lastFailure() const noexcept { return lastFailure_; }
private:
    HWND owner_ = nullptr;
    bool registered_ = false, changing_ = false;
    DWORD lastError_ = ERROR_SUCCESS;
    std::optional<SessionNotificationFailure> lastFailure_;
};
