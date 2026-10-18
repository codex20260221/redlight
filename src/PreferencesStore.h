#pragma once
#include "Preferences.h"

struct LoadedPreferences {
    AppPreferences preferences;
    bool warning = false;
    bool legacy = false;
};
LoadedPreferences LoadPreferences();
bool SavePreferences(const AppPreferences& preferences);
