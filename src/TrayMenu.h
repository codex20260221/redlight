#pragma once
#include <windows.h>
#include "BackendPreference.h"
#include "DisplayFilter.h"

struct TrayMenuState {
    bool needsRestore;
    RedMode mode;
    bool supportsLuma;
    LumaProfile luma;
    BackendPreference backendPreference;
    unsigned sessionMinutes;
    bool toggleHotkey, restoreHotkey;
};
// Ownership of the returned root menu transfers to the caller.
HMENU BuildTrayMenu(const TrayMenuState& state);
