#pragma once
#include "Preferences.h"

enum class PreferenceSource { Defaults, VersionedRecord, LegacyValues };
inline const char* PreferenceSourceName(PreferenceSource source) noexcept {
    switch (source) {
    case PreferenceSource::Defaults: return "Defaults";
    case PreferenceSource::VersionedRecord: return "Versioned record";
    case PreferenceSource::LegacyValues: return "Legacy values";
    }
    return "Unknown";
}

struct LoadedPreferences {
    AppPreferences preferences;
    bool warning = false;
    bool legacy = false;
    PreferenceSource source = PreferenceSource::Defaults;
};
LoadedPreferences LoadPreferences();
bool SavePreferences(const AppPreferences& preferences) noexcept;
