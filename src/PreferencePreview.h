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
            (defaults->brightness && candidate.brightnessPercent != DefaultRedBrightness)) return std::nullopt;
        const char* fields = defaults->shortcuts ? "Shortcuts = Default; Expiry warning = Off; Red brightness = 100%" :
            (defaults->expiryWarning ? "Expiry warning = Off; Red brightness = 100%" :
                (defaults->brightness ? "Red brightness = 100%" : "None; all fields explicit"));
        const int length = std::snprintf(provenance, sizeof(provenance),
            "Input profile version: %u; canonical export version: %u\nDefaults supplied: %s\n\n",
            *inputSchema, PreferencesProfileVersion, fields);
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(provenance)) return std::nullopt;
    }
    constexpr std::array<const char*, 7> labels{"Red mode", "Luma detail", "Next-start backend", "Session limit", "Shortcuts", "Expiry warning", "Red brightness"};
    static_assert(labels.size() == ProfileDifferenceFieldNames.size());
    char changed[192];
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
    const int length = std::snprintf(output.data(), output.size(),
        "Apply these preferences?\n\n%sChanges from current choices: %s\n\nMode: %s\nLuma detail: %s\nBackend on next start: %s\n"
        "Session limit: %u minutes (0 = unlimited)\nExpiry warning: %s\nShortcut profile: %s\nRed brightness: %u%%\n\n"
        "The display will be restored and stay off, even when preference values are identical. Backend selection applies on the next start.\n"
        "Luma and brightness preferences are retained for Magnification; Gamma uses Strict Red at 100%%.\n"
        "The Windows startup entry is managed separately and is unchanged.",
        provenance, changed, RedModeDisplayName(candidate.mode), LumaProfileName(candidate.luma), BackendPreferenceName(candidate.backend),
        candidate.sessionMinutes, ExpiryWarningName(candidate.expiryWarning), ShortcutProfileName(candidate.shortcuts), candidate.brightnessPercent);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
