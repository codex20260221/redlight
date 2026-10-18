#pragma once
#include "Preferences.h"

enum class BrightnessPreferenceResult { Rejected, BackendFailed, Applied, PreferredOnly };
inline bool CanChangeBrightness(const DisplayFilter& filter, bool busy, bool applying, bool restoreRequested) noexcept {
    return !busy && !applying && !restoreRequested &&
        (!filter.supportsBrightness() || !filter.needsRestore() || filter.isActive());
}
inline BrightnessPreferenceResult UpdateBrightnessPreference(DisplayFilter& filter, AppPreferences& preferences, unsigned percent) {
    if (!ValidRedBrightness(percent)) return BrightnessPreferenceResult::Rejected;
    if (!filter.supportsBrightness()) {
        preferences.brightnessPercent = percent; // No gamma API/error/ownership mutation.
        return BrightnessPreferenceResult::PreferredOnly;
    }
    if (filter.needsRestore() && !filter.isActive()) return BrightnessPreferenceResult::Rejected;
    if (!filter.setBrightnessPercent(percent)) return BrightnessPreferenceResult::BackendFailed;
    preferences.brightnessPercent = percent;
    return BrightnessPreferenceResult::Applied;
}
