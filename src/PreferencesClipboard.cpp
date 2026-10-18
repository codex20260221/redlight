#include "PreferencesClipboard.h"
#include "Clipboard.h"
#include "Diagnostics.h"

std::optional<std::string_view> FormatCurrentPreferencesJson(const CurrentPreferencesSnapshot& snapshot, std::span<char> output) noexcept {
    if (output.empty() || std::string_view(PreferenceSourceToken(snapshot.startupSource)) == "unknown") return std::nullopt;
    char preferences[PreferencesJsonBufferSize];
    const auto value = FormatPreferencesJson(snapshot.preferences, preferences);
    if (!value) return std::nullopt;
    const int length = std::snprintf(output.data(), output.size(),
        "{\"protocol\":1,\"status\":\"current-preferences\",\"source\":\"current-session\",\"profile_schema\":%u,"
        "\"settings_unsaved\":%s,\"busy\":%s,\"startup_source\":\"%s\",\"startup_load_warning\":%s,\"preferences\":%.*s}\n",
        PreferencesProfileVersion, snapshot.settingsUnsaved ? "true" : "false", snapshot.busy ? "true" : "false",
        PreferenceSourceToken(snapshot.startupSource), snapshot.startupLoadWarning ? "true" : "false", static_cast<int>(value->size()), value->data());
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
bool CopyCurrentPreferencesJsonToClipboard(HWND owner, const CurrentPreferencesSnapshot& snapshot) noexcept {
    try {
        char output[CurrentPreferencesJsonBufferSize];
        const auto text = FormatCurrentPreferencesJson(snapshot, output);
        if (!text) { Diagnostics::Log("Current preference JSON refused an invalid or incomplete snapshot."); return false; }
        return CopyUtf8TextToClipboard(owner, *text);
    } catch (...) {
        Diagnostics::Log("Current preference JSON clipboard preparation failed.");
        return false;
    }
}
