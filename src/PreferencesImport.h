#pragma once
#include "PreferencesStore.h"

enum class PreferenceImportResult { Rejected, RestoreFailed, SaveFailed, Saved };

// The UI supplies its restore path so normal recovery diagnostics/timer cancellation
// remain in force. Validation and restoration must precede any persistent change.
template<class Restore>
PreferenceImportResult SaveImportedPreferences(const AppPreferences& candidate, Restore&& restore) {
    if (!ValidPreferences(candidate)) return PreferenceImportResult::Rejected;
    if (!restore()) return PreferenceImportResult::RestoreFailed;
    if (!SavePreferences(candidate)) return PreferenceImportResult::SaveFailed;
    return PreferenceImportResult::Saved;
}
