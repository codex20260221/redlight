#pragma once
#include "Preferences.h"

// Explicit saved-settings change only, guarded against a same-session instance.
// 0 saved, 6 instance present, 4 invalid candidate, 1 guard/storage/cleanup/output failure.
int SaveSettingsProfileForNextStart(const AppPreferences& candidate) noexcept;
