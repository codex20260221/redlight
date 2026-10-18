#pragma once
#include "PreferencesJson.h"
#include "PreferencesStore.h"
#include <cstdint>

struct CurrentPreferencesSnapshot {
    AppPreferences preferences;
    bool settingsUnsaved = false, busy = false;
    PreferenceSource startupSource = PreferenceSource::Defaults;
    bool startupLoadWarning = false;
    bool operator==(const CurrentPreferencesSnapshot&) const = default;
};
inline bool ValidCurrentPreferences(const CurrentPreferencesSnapshot& snapshot) noexcept {
    return ValidPreferences(snapshot.preferences) && static_cast<unsigned>(snapshot.startupSource) <= static_cast<unsigned>(PreferenceSource::LegacyValues);
}
inline constexpr std::size_t CurrentPreferencesJsonBufferSize = 512;
std::optional<std::string_view> FormatCurrentPreferencesJson(const CurrentPreferencesSnapshot& snapshot, std::span<char> output) noexcept;

// Separate from status protocol 1. Positive x64 LRESULT; unused payload bits
// must stay zero and the canonical profile schema must agree with this binary.
inline std::optional<std::uint64_t> PackCurrentPreferences(const CurrentPreferencesSnapshot& snapshot) noexcept {
    static_assert(PreferencesProfileVersion > 0 && PreferencesProfileVersion <= 255);
    if (!ValidCurrentPreferences(snapshot)) return std::nullopt;
    const auto& value = snapshot.preferences;
    std::uint64_t word = 0x5202000000000000ull;
    const auto put = [&](std::uint64_t bits, unsigned shift) { word |= bits << shift; };
    put(value.mode == RedMode::LumaRed, 0); put(static_cast<unsigned>(value.luma), 1);
    put(static_cast<unsigned>(value.backend), 3); put(value.sessionMinutes, 5); put(static_cast<unsigned>(value.shortcuts), 13);
    put(static_cast<unsigned>(value.expiryWarning), 15); put(snapshot.settingsUnsaved, 24); put(snapshot.busy, 25);
    put(static_cast<unsigned>(snapshot.startupSource), 26); put(snapshot.startupLoadWarning, 28); put(PreferencesProfileVersion, 29);
    put(value.brightnessPercent, 37); // Schema 4; older clients already reject a differing profile schema.
    // Schema 5 uses bits 44-46 for an option index, leaving bit 47 reserved.
    for (unsigned index = 0; index < FadeInOptions.size(); ++index) if (value.fadeIn == FadeInOptions[index]) put(index, 44);
    return word;
}
inline std::optional<CurrentPreferencesSnapshot> UnpackCurrentPreferences(std::uint64_t word) noexcept {
    constexpr auto payloadMask = (1ull << 47) - 1;
    if ((word >> 48) != 0x5202 || (word & 0x0000ffffffffffffull & ~payloadMask) != 0 ||
        ((word >> 29) & 255) != PreferencesProfileVersion) return std::nullopt;
    const auto get = [&](unsigned shift, unsigned mask) { return static_cast<unsigned>((word >> shift) & mask); };
    CurrentPreferencesSnapshot result;
    result.preferences.mode = get(0, 1) ? RedMode::LumaRed : RedMode::StrictRed;
    result.preferences.luma = static_cast<LumaProfile>(get(1, 3)); result.preferences.backend = static_cast<BackendPreference>(get(3, 3));
    result.preferences.sessionMinutes = get(5, 255); result.preferences.shortcuts = static_cast<ShortcutProfile>(get(13, 3));
    result.preferences.expiryWarning = static_cast<ExpiryWarning>(get(15, 511));
    result.settingsUnsaved = get(24, 1) != 0; result.busy = get(25, 1) != 0;
    result.startupSource = static_cast<PreferenceSource>(get(26, 3)); result.startupLoadWarning = get(28, 1) != 0;
    result.preferences.brightnessPercent = get(37, 127);
    const auto fadeIndex = get(44, 7);
    if (fadeIndex >= FadeInOptions.size()) return std::nullopt;
    result.preferences.fadeIn = FadeInOptions[fadeIndex];
    return ValidCurrentPreferences(result) ? std::optional<CurrentPreferencesSnapshot>(result) : std::nullopt;
}
