#include "SessionNotifications.h"
#include "Diagnostics.h"
#include "UiSafety.h"
#include <wtsapi32.h>

bool SessionNotifications::ensure(HWND owner) {
    if (changing_ || !owner || (registered_ && owner != owner_)) {
        lastError_ = changing_ ? ERROR_BUSY : (!owner ? ERROR_INVALID_PARAMETER : ERROR_INVALID_STATE);
        Diagnostics::LogFormat("Session-notification registration request rejected (error %lu); ownership unchanged.", lastError_);
        return false;
    }
    if (registered_) return true; // One unregister per successful register; do not register twice.
    ScopedFlag changing(changing_);
    if (!WTSRegisterSessionNotification(owner, NOTIFY_FOR_THIS_SESSION)) {
        lastError_ = GetLastError();
        Diagnostics::LogFormat("WTSRegisterSessionNotification failed (error %lu); registration remains unavailable.", lastError_);
        return false;
    }
    owner_ = owner;
    registered_ = true;
    lastError_ = ERROR_SUCCESS;
    Diagnostics::Log("Session recovery notifications registered for the current session.");
    return true;
}
bool SessionNotifications::release() {
    if (changing_) { lastError_ = ERROR_BUSY; return false; }
    if (!registered_) return true;
    ScopedFlag changing(changing_);
    if (!WTSUnRegisterSessionNotification(owner_)) {
        lastError_ = GetLastError();
        Diagnostics::LogFormat("WTSUnRegisterSessionNotification failed (error %lu); ownership retained for retry.", lastError_);
        return false;
    }
    registered_ = false;
    owner_ = nullptr;
    lastError_ = ERROR_SUCCESS;
    Diagnostics::Log("Session recovery notifications unregistered.");
    return true;
}
