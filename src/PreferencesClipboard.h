#pragma once
#include "PreferencesJson.h"
#include "PreferencesStore.h"
#include <windows.h>

struct CurrentPreferencesSnapshot {
    AppPreferences preferences;
    bool settingsUnsaved = false, busy = false;
    PreferenceSource startupSource = PreferenceSource::Defaults;
    bool startupLoadWarning = false;
};
inline constexpr std::size_t CurrentPreferencesJsonBufferSize = 512;
std::optional<std::string_view> FormatCurrentPreferencesJson(const CurrentPreferencesSnapshot& snapshot, std::span<char> output) noexcept;
bool CopyCurrentPreferencesJsonToClipboard(HWND owner, const CurrentPreferencesSnapshot& snapshot) noexcept;
