#pragma once
#include <windows.h>
#include "BackendPreference.h"
#include "DisplayFilter.h"
#include "ShortcutProfile.h"

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
};
// Ownership of the returned root menu transfers to the caller.
HMENU BuildTrayMenu(const TrayMenuState& state);
