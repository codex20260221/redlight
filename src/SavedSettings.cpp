#include "SavedSettings.h"
#include "PreferencesStore.h"
#include "StandardOutput.h"
#include "Diagnostics.h"
#include "PreferencesJson.h"
#include <cstdio>

namespace {
int WriteSettingsResult(std::string_view text, int result) noexcept {
    if (WriteStandardOutput(text)) return result;
    Diagnostics::Log("Saved-settings query could not write standard output; discard partial output.");
    return 1;
}
}
int QuerySavedSettings() noexcept {
    try {
        const auto loaded = LoadPreferences();
        if (loaded.warning || !ValidPreferences(loaded.preferences)) {
            Diagnostics::Log("Saved-settings query refused unreadable or invalid storage.");
            return WriteSettingsResult("{\"protocol\":1,\"status\":\"settings-error\",\"preferences\":null}\n", 1);
        }
        char preferences[PreferencesJsonBufferSize];
        const auto value = FormatPreferencesJson(loaded.preferences, preferences);
        if (!value) { Diagnostics::Log("Saved-settings preferences could not be formatted completely."); return 1; }
        char output[512];
        const int length = std::snprintf(output, sizeof(output),
            "{\"protocol\":1,\"status\":\"saved-settings\",\"source\":\"%s\",\"profile_schema\":%u,\"preferences\":%.*s}\n",
            PreferenceSourceToken(loaded.source), PreferencesProfileVersion, static_cast<int>(value->size()), value->data());
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
            Diagnostics::Log("Saved-settings result could not fit its checked output buffer; no output was written.");
            return 1;
        }
        return WriteSettingsResult(std::string_view(output, static_cast<std::size_t>(length)), 0);
    } catch (...) {
        Diagnostics::Log("Saved-settings query encountered an internal failure before complete output.");
        return 1;
    }
}
