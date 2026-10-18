#pragma once
#include <windows.h>
#include "BackendPreference.h"
#include "DisplayFilter.h"
#include "ShortcutProfile.h"
#include "ExpiryWarning.h"
#include "StartupRegistration.h"

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
    bool backendActive = false, restoreRequested = false, canToggle = true;
    bool canClearRecentDiagnostics = true;
    unsigned brightnessPercent = DefaultRedBrightness;
    bool supportsBrightness = true, canChangeBrightness = true;
    StartupEntryState startupEntry = StartupEntryState::Unavailable;
    bool canEnableStartup = false, canRemoveStartup = false, canRefreshStartup = true;
    bool startupPathAvailable = false;
};
// Ownership of the returned root menu transfers to the caller.
HMENU BuildTrayMenu(const TrayMenuState& state);
// Bind a menu's displayed toggle intent before its asynchronous command is posted.
unsigned ResolveTrayMenuCommand(unsigned command, const TrayMenuState& state) noexcept;
