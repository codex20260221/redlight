#pragma once
#include "Preferences.h"
#include <array>
#include <optional>

inline constexpr std::array<const char*, 7> ProfileDifferenceFieldNames{
    "mode", "luma", "backend", "session_minutes", "hotkeys", "expiry_warning", "brightness_percent"
};
inline std::optional<unsigned> ProfileDifferenceMask(const AppPreferences& candidate, const AppPreferences& saved) noexcept {
    if (!ValidPreferences(candidate) || !ValidPreferences(saved)) return std::nullopt;
    return (candidate.mode != saved.mode ? 1u : 0u) | (candidate.luma != saved.luma ? 2u : 0u) |
        (candidate.backend != saved.backend ? 4u : 0u) | (candidate.sessionMinutes != saved.sessionMinutes ? 8u : 0u) |
        (candidate.shortcuts != saved.shortcuts ? 16u : 0u) | (candidate.expiryWarning != saved.expiryWarning ? 32u : 0u) |
        (candidate.brightnessPercent != saved.brightnessPercent ? 64u : 0u);
}
// Compare an already validated candidate to persisted/default preferences without writes.
// JSON includes saved source, changed field names, and typed saved/candidate value pairs.
// 0 match, 5 different, 4 invalid candidate, 1 settings/output/internal failure.
int CompareSavedSettingsProfile(const AppPreferences& candidate) noexcept;
