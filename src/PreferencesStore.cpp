#include "PreferencesStore.h"
#include "Diagnostics.h"
#include <windows.h>
#include <array>

namespace {
constexpr const wchar_t* kKey = L"Software\\RedLight";
std::optional<DWORD> ReadLegacy(const wchar_t* name, bool& warning) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_DWORD, NULL, &value, &size);
    if (result == ERROR_SUCCESS) return value;
    if (result != ERROR_FILE_NOT_FOUND) {
        warning = true;
        Diagnostics::LogFormat("Legacy preference %ls unavailable (error %ld).", name, result);
    }
    return std::nullopt;
}
}
LoadedPreferences LoadPreferences() {
    LoadedPreferences result;
    std::array<char, 4096> text = {};
    DWORD bytes = static_cast<DWORD>(text.size());
    const LSTATUS read = RegGetValueW(HKEY_CURRENT_USER, kKey, L"SettingsProfile", RRF_RT_REG_BINARY, NULL, text.data(), &bytes);
    if (read == ERROR_SUCCESS) {
        const auto parsed = ParsePreferences(std::string_view(text.data(), bytes));
        if (parsed.preferences) result.preferences = *parsed.preferences;
        else {
            result.warning = true;
            Diagnostics::LogFormat("Saved profile invalid at line %u: %s. Using safe defaults.", parsed.line, parsed.error);
        }
        return result;
    }
    if (read != ERROR_FILE_NOT_FOUND) {
        result.warning = true;
        Diagnostics::LogFormat("Saved profile unavailable (error %ld). Using safe defaults.", read);
        return result;
    }
    // Migrate prior individual values only when the versioned record is absent.
    if (const auto value = ReadLegacy(L"RedMode", result.warning)) {
        result.legacy = true;
        if (*value <= 1) result.preferences.mode = *value ? RedMode::LumaRed : RedMode::StrictRed;
        else result.warning = true;
    }
    if (const auto value = ReadLegacy(L"LumaProfile", result.warning)) {
        result.legacy = true;
        if (IsValidLumaProfile(*value)) result.preferences.luma = static_cast<LumaProfile>(*value);
        else result.warning = true;
    }
    if (const auto value = ReadLegacy(L"BackendPreference", result.warning)) {
        result.legacy = true;
        if (IsValidBackendPreference(*value)) result.preferences.backend = static_cast<BackendPreference>(*value);
        else result.warning = true;
    }
    if (const auto value = ReadLegacy(L"SessionLimitMinutes", result.warning)) {
        result.legacy = true;
        if (*value <= SessionTimer::maxMinutes) result.preferences.sessionMinutes = *value;
        else result.warning = true;
    }
    if (result.legacy) Diagnostics::Log("Loaded legacy preferences; next save will use one versioned settings record.");
    if (result.warning) Diagnostics::Log("Some legacy preferences were invalid; defaults retained for those fields.");
    return result;
}
bool SavePreferences(const AppPreferences& preferences) {
    const auto text = SerializePreferences(preferences);
    if (!text) { Diagnostics::Log("Refusing to save invalid preferences."); return false; }
    // One value prevents partial updates across separate per-setting registry writes.
    const LSTATUS result = RegSetKeyValueW(HKEY_CURRENT_USER, kKey, L"SettingsProfile", REG_BINARY,
        text->data(), static_cast<DWORD>(text->size()));
    if (result != ERROR_SUCCESS) Diagnostics::LogFormat("Saving settings profile failed (error %ld).", result);
    return result == ERROR_SUCCESS;
}
