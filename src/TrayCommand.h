#pragma once
#include "CommandIds.h"
#include "FilterControl.h"
#include <windows.h>
#include <cstdint>

struct TrayCommandPostResult { bool queued; std::uint32_t errorCode; };
TrayCommandPostResult TryPostTrayCommand(HWND owner, unsigned command) noexcept;

inline bool TrayCommandHasRestoreIntent(unsigned command, bool active, bool needsRestore, bool restoreRequested) noexcept {
    if (command == ID_TRAY_RESTORE) return true;
    if (command != ID_TRAY_TOGGLE && command != ID_TRAY_ENABLE) return false;
    return SelectFilterControlIntent(command == ID_TRAY_TOGGLE, false, false,
        restoreRequested, active, needsRestore) == FilterControlIntent::Restore;
}

enum class TrayCommandDelivery { Queued, QueueFailed, RestoreRequested };
template<class Restore>
TrayCommandDelivery DeliverTrayCommand(HWND owner, unsigned command, bool restoreIntent, bool& restoreRequested, Restore&& restore) {
    if (TryPostTrayCommand(owner, command).queued) return TrayCommandDelivery::Queued;
    if (!restoreIntent) return TrayCommandDelivery::QueueFailed;
    // Includes ordinary busy deferral; this does not claim restoration succeeded.
    try { restore(); }
    catch (...) { restoreRequested = true; throw; }
    return TrayCommandDelivery::RestoreRequested;
}
