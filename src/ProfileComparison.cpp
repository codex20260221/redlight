#include "ProfileComparison.h"
#include "PreferencesStore.h"
#include "StandardOutput.h"
#include "Diagnostics.h"
#include <cstdio>

namespace {
int ComparisonFormatFailure() noexcept {
    Diagnostics::Log("Offline profile comparison result could not fit its checked output buffer; no output was written.");
    return 1;
}
int WriteComparison(std::string_view output, int result) noexcept {
    if (WriteStandardOutput(output)) return result;
    Diagnostics::Log("Offline profile comparison could not write standard output; discard partial output.");
    return 1;
}
}
int CompareSavedSettingsProfile(const AppPreferences& candidate) noexcept {
    try {
        if (!ValidPreferences(candidate)) {
            Diagnostics::Log("Offline profile comparison received invalid candidate preferences.");
            return WriteComparison("{\"protocol\":1,\"status\":\"invalid-profile\",\"match\":null}\n", 4);
        }
        const auto loaded = LoadPreferences();
        const auto mask = loaded.warning ? std::nullopt : ProfileDifferenceMask(candidate, loaded.preferences);
        if (!mask) {
            Diagnostics::Log("Offline profile comparison refused unreadable or invalid stored preferences.");
            return WriteComparison("{\"protocol\":1,\"status\":\"settings-error\",\"match\":null}\n", 1);
        }
        char output[1024];
        int length = std::snprintf(output, sizeof(output),
            "{\"protocol\":1,\"status\":\"profiles-%s\",\"match\":%s,\"differences\":[", *mask ? "differ" : "match", *mask ? "false" : "true");
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) return ComparisonFormatFailure();
        auto used = static_cast<std::size_t>(length);
        bool first = true;
        for (std::size_t index = 0; index < ProfileDifferenceFieldNames.size(); ++index) {
            if ((*mask & (1u << index)) == 0) continue;
            length = std::snprintf(output + used, sizeof(output) - used, "%s\"%s\"", first ? "" : ",", ProfileDifferenceFieldNames[index]);
            if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output) - used) return ComparisonFormatFailure();
            used += static_cast<std::size_t>(length);
            first = false;
        }
        length = std::snprintf(output + used, sizeof(output) - used,
            "],\"saved_source\":\"%s\",\"changes\":{", PreferenceSourceToken(loaded.source));
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output) - used) return ComparisonFormatFailure();
        used += static_cast<std::size_t>(length);
        const auto tokens = [](const AppPreferences& value) {
            return std::array<const char*, ProfileDifferenceFieldNames.size()>{PreferenceModeToken(value.mode), PreferenceLumaToken(value.luma),
                PreferenceBackendToken(value.backend), nullptr, ShortcutProfileToken(value.shortcuts), ExpiryWarningToken(value.expiryWarning), nullptr};
        };
        const auto savedTokens = tokens(loaded.preferences), candidateTokens = tokens(candidate);
        first = true;
        for (std::size_t index = 0; index < ProfileDifferenceFieldNames.size(); ++index) {
            if ((*mask & (1u << index)) == 0) continue;
            if (savedTokens[index]) {
                length = std::snprintf(output + used, sizeof(output) - used,
                    "%s\"%s\":{\"saved\":\"%s\",\"candidate\":\"%s\"}", first ? "" : ",", ProfileDifferenceFieldNames[index], savedTokens[index], candidateTokens[index]);
            } else {
                length = std::snprintf(output + used, sizeof(output) - used,
                    "%s\"%s\":{\"saved\":%u,\"candidate\":%u}", first ? "" : ",", ProfileDifferenceFieldNames[index],
                    index == 3 ? loaded.preferences.sessionMinutes : loaded.preferences.brightnessPercent,
                    index == 3 ? candidate.sessionMinutes : candidate.brightnessPercent);
            }
            if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output) - used) return ComparisonFormatFailure();
            used += static_cast<std::size_t>(length);
            first = false;
        }
        constexpr std::string_view ending = "}}\n";
        if (ending.size() > sizeof(output) - used) return ComparisonFormatFailure();
        for (const auto character : ending) output[used++] = character;
        return WriteComparison(std::string_view(output, used), *mask ? 5 : 0);
    } catch (...) {
        Diagnostics::Log("Offline profile comparison encountered an internal error; discard any partial output.");
        return 1;
    }
}
