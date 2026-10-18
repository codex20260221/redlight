#pragma once
#include "Preferences.h"

enum class LumaPreferenceResult { Rejected, BackendFailed, Applied, PreferredOnly };
inline LumaPreferenceResult UpdateLumaPreference(DisplayFilter& filter, AppPreferences& preferences, LumaProfile profile) {
    if (!IsValidLumaProfile(static_cast<unsigned>(profile))) return LumaPreferenceResult::Rejected;
    if (!filter.supportsRedMode(RedMode::LumaRed)) {
        preferences.luma = profile;
        return LumaPreferenceResult::PreferredOnly;
    }
    if (filter.needsRestore() && !filter.isActive()) return LumaPreferenceResult::Rejected;
    if (!filter.setLumaProfile(profile)) return LumaPreferenceResult::BackendFailed;
    preferences.luma = profile;
    return LumaPreferenceResult::Applied;
}
