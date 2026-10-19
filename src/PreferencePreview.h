#pragma once
#include "ProfileComparison.h"
#include <array>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

// Complete bounded preview, prepared before a native confirmation or any mutation.
inline std::optional<std::string_view> FormatPreferencePreview(const AppPreferences& current,
    const AppPreferences& candidate, std::span<char> output, std::optional<unsigned> inputSchema = std::nullopt) noexcept {
    const auto mask = ProfileDifferenceMask(candidate, current);
    if (!mask || output.empty()) return std::nullopt;
    char provenance[256] = {};
    if (inputSchema) {
        const auto defaults = DefaultsForProfileSchema(*inputSchema);
        if (!defaults || (defaults->shortcuts && candidate.shortcuts != ShortcutProfile::Default) ||
            (defaults->expiryWarning && candidate.expiryWarning != ExpiryWarning::Off) ||
            (defaults->brightness && candidate.brightnessPercent != DefaultRedBrightness) || (defaults->fadeIn && candidate.fadeIn != FadeInDuration::Off) ||
            (defaults->schedule && candidate.schedule != Scheduling::DailyTimes{})) return std::nullopt;
        const char* fields = defaults->shortcuts ? "Shortcuts = Default; Expiry warning = Off; Red brightness = 100%; Fade-in = Off; Schedule = Off (00:00 / 00:00)" :
            (defaults->expiryWarning ? "Expiry warning = Off; Red brightness = 100%; Fade-in = Off; Schedule = Off (00:00 / 00:00)" :
                (defaults->brightness ? "Red brightness = 100%; Fade-in = Off; Schedule = Off (00:00 / 00:00)" :
                    (defaults->fadeIn ? "Fade-in = Off; Schedule = Off (00:00 / 00:00)" : (defaults->schedule ? "Schedule = Off (00:00 / 00:00)" : "None; all fields explicit"))));
        const int length = std::snprintf(provenance, sizeof(provenance),
            "Input profile version: %u; canonical export version: %u\nDefaults supplied: %s\n\n",
            *inputSchema, PreferencesProfileVersion, fields);
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(provenance)) return std::nullopt;
    }
    constexpr std::array<const char*, 11> labels{"Red mode", "Luma detail", "Next-start backend", "Session limit", "Shortcuts", "Expiry warning", "Red brightness", "Fade-in duration", "Daily schedule", "Enable time", "Restore time"};
    static_assert(labels.size() == ProfileDifferenceFieldNames.size());
    char changed[256];
    std::size_t used = 0;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if ((*mask & (1u << index)) == 0) continue;
        const int length = std::snprintf(changed + used, sizeof(changed) - used, "%s%s", used ? ", " : "", labels[index]);
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(changed) - used) return std::nullopt;
        used += static_cast<std::size_t>(length);
    }
    if (!used) {
        const int length = std::snprintf(changed, sizeof(changed), "None; current preference values are identical");
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(changed)) return std::nullopt;
    }
    char enableAt[6], restoreAt[6];
    if (!Scheduling::FormatMinute(candidate.schedule.enableMinute, enableAt) || !Scheduling::FormatMinute(candidate.schedule.restoreMinute, restoreAt)) return std::nullopt;
    const int length = std::snprintf(output.data(), output.size(),
        "Apply these preferences?\n\n%sChanges from current choices: %s\n\nMode: %s\nLuma detail: %s\nBackend on next start: %s\n"
        "Session limit: %u minutes (0 = unlimited)\nExpiry warning: %s\nShortcut profile: %s\nRed brightness: %u%%\nFade-in on activation: %s\n"
        "Daily schedule: %s\nEnable at: %s / Restore at: %s (Windows local time)\n%s\n\n"
        "The display will be restored now, even when preference values are identical. Enabled schedules may activate at future triggers. Backend selection applies on the next start.\n"
        "Luma, brightness, and fade-in are retained for Magnification; Gamma uses Strict Red at 100%% without fading.\n"
        "The Windows startup entry is managed separately and is unchanged.",
        provenance, changed, RedModeDisplayName(candidate.mode), LumaProfileName(candidate.luma), BackendPreferenceName(candidate.backend),
        candidate.sessionMinutes, ExpiryWarningName(candidate.expiryWarning), ShortcutProfileName(candidate.shortcuts), candidate.brightnessPercent, FadeInName(candidate.fadeIn),
        candidate.schedule.enabled ? "Enabled (automatic activation)" : "Off", enableAt, restoreAt,
        candidate.schedule.enabled ? "Only future triggers run while RedLight and controls are available. Startup/wake never catches up; missed times are skipped." : "Scheduling Off makes no automatic display changes.");
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
