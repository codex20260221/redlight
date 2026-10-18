#include "ProfileExport.h"
#include "PreferencesStore.h"
#include "StandardOutput.h"
#include "Diagnostics.h"

int ExportSavedSettingsProfile() noexcept {
    try {
        const auto loaded = LoadPreferences();
        if (loaded.warning) {
            Diagnostics::Log("Saved profile export refused: stored preferences are unreadable or invalid.");
            return 1;
        }
        const auto text = SerializePreferences(loaded.preferences);
        if (!text) {
            Diagnostics::Log("Saved profile export could not serialize the loaded preferences.");
            return 1;
        }
        if (WriteStandardOutput(*text)) return 0;
        Diagnostics::Log("Saved profile export could not write standard output; discard any partial output.");
    } catch (...) {
        Diagnostics::Log("Saved profile export failed during serialization; no preference write was attempted.");
    }
    return 1;
}
