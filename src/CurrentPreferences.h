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
    static_assert(PreferencesProfileVersion == 6, "Packet tag 0x53 is bound to profile schema 6; review the wire format before changing it.");
    static_assert(ExpiryWarningOptions.size() == 4 && FadeInOptions.size() <= 8);
    if (!ValidCurrentPreferences(snapshot)) return std::nullopt;
    const auto& value = snapshot.preferences;
    std::uint64_t word = 0x5300000000000000ull;
    const auto put = [&](std::uint64_t bits, unsigned shift) { word |= bits << shift; };
    put(value.mode == RedMode::LumaRed, 0); put(static_cast<unsigned>(value.luma), 1);
    put(static_cast<unsigned>(value.backend), 3); put(value.sessionMinutes, 5); put(static_cast<unsigned>(value.shortcuts), 13);
    for (unsigned index = 0; index < ExpiryWarningOptions.size(); ++index) if (value.expiryWarning == ExpiryWarningOptions[index]) put(index, 15);
    put(snapshot.settingsUnsaved, 17); put(snapshot.busy, 18); put(static_cast<unsigned>(snapshot.startupSource), 19); put(snapshot.startupLoadWarning, 21);
    put(value.brightnessPercent, 22);
    for (unsigned index = 0; index < FadeInOptions.size(); ++index) if (value.fadeIn == FadeInOptions[index]) put(index, 29);
    put(value.schedule.enabled, 32); put(value.schedule.enableMinute, 33); put(value.schedule.restoreMinute, 44);
    return word;
}
inline std::optional<CurrentPreferencesSnapshot> UnpackCurrentPreferences(std::uint64_t word) noexcept {
    static_assert(PreferencesProfileVersion == 6);
    if ((word >> 56) != 0x53 || (word & (1ull << 55))) return std::nullopt;
    const auto get = [&](unsigned shift, unsigned mask) { return static_cast<unsigned>((word >> shift) & mask); };
    CurrentPreferencesSnapshot result;
    result.preferences.mode = get(0, 1) ? RedMode::LumaRed : RedMode::StrictRed;
    result.preferences.luma = static_cast<LumaProfile>(get(1, 3)); result.preferences.backend = static_cast<BackendPreference>(get(3, 3));
    result.preferences.sessionMinutes = get(5, 255); result.preferences.shortcuts = static_cast<ShortcutProfile>(get(13, 3));
    result.preferences.expiryWarning = ExpiryWarningOptions[get(15, 3)];
    result.settingsUnsaved = get(17, 1) != 0; result.busy = get(18, 1) != 0;
    result.startupSource = static_cast<PreferenceSource>(get(19, 3)); result.startupLoadWarning = get(21, 1) != 0;
    result.preferences.brightnessPercent = get(22, 127);
    const auto fadeIndex = get(29, 7);
    if (fadeIndex >= FadeInOptions.size()) return std::nullopt;
    result.preferences.fadeIn = FadeInOptions[fadeIndex];
    result.preferences.schedule = {get(32, 1) != 0, get(33, 2047), get(44, 2047)};
    return ValidCurrentPreferences(result) ? std::optional<CurrentPreferencesSnapshot>(result) : std::nullopt;
}
