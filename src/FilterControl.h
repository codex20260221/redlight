#pragma once

enum class FilterControlIntent { None, Restore, Enable };
inline FilterControlIntent SelectFilterControlIntent(bool toggle, bool busy, bool applyingPreferences,
    bool restoreRequested, bool active, bool needsRestore) noexcept {
    if (busy || applyingPreferences) return FilterControlIntent::None;
    if (restoreRequested || (needsRestore && !active)) return FilterControlIntent::Restore;
    if (active || needsRestore) return toggle ? FilterControlIntent::Restore : FilterControlIntent::None;
    return FilterControlIntent::Enable;
}
