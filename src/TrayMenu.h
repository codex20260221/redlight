#pragma once
#include <windows.h>
#include "BackendPreference.h"
#include "DisplayFilter.h"
#include "ShortcutProfile.h"
#include "ExpiryWarning.h"

struct TrayMenuState {
    bool needsRestore;
    RedMode mode;
    bool supportsLuma;
    LumaProfile luma;
    BackendPreference backendPreference;
    unsigned sessionMinutes;
    ShortcutProfile shortcutProfile;
    const char* toggleShortcut;
    const char* restoreShortcut;
    ExpiryWarning expiryWarning;
    bool preferencesUnsaved;
    bool canChangeSessionLimit, canRestartCountdown;
    bool canClearRestorationHistory = true;
};
// Ownership of the returned root menu transfers to the caller.
HMENU BuildTrayMenu(const TrayMenuState& state);
