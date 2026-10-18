#include "HotkeyManager.h"
#include "CommandIds.h"
#include "Diagnostics.h"

bool HotkeyManager::set(unsigned slot, const std::optional<ShortcutBinding>& target) {
    if (actual_[slot] == target) return true;
    const int id = static_cast<int>(slot == 0 ? ID_HOTKEY_TOGGLE : ID_HOTKEY_RESTORE);
    if (actual_[slot]) {
        if (!UnregisterHotKey(owner_, id)) {
            Diagnostics::LogFormat("UnregisterHotKey(%d) failed (error %lu); keeping actual binding.", id, GetLastError());
            return false;
        }
        actual_[slot].reset();
    }
    if (target) {
        if (!RegisterHotKey(owner_, id, target->modifiers | MOD_NOREPEAT, target->key)) {
            Diagnostics::LogFormat("RegisterHotKey(%d, %s) failed (error %lu).", id, ShortcutBindingName(*target), GetLastError());
            return false;
        }
        actual_[slot] = target;
    }
    return true;
}
bool HotkeyManager::initialize(HWND owner, ShortcutProfile profile) {
    if (initialized_ || !IsValidShortcutProfile(static_cast<unsigned>(profile))) return false;
    owner_ = owner;
    initialized_ = true;
    const auto wanted = ShortcutBindings(profile);
    const bool toggle = set(0, wanted[0]);
    const bool restore = set(1, wanted[1]);
    return toggle && restore;
}
bool HotkeyManager::change(ShortcutProfile profile) {
    if (!initialized_ || !IsValidShortcutProfile(static_cast<unsigned>(profile))) return false;
    const auto before = actual_;
    const auto wanted = ShortcutBindings(profile);
    if (set(0, wanted[0]) && set(1, wanted[1])) return true;
    const bool toggleRestored = set(0, before[0]);
    const bool restoreRestored = set(1, before[1]);
    Diagnostics::Log(toggleRestored && restoreRestored ? "Shortcut change failed; previous actual registrations restored." :
        "Shortcut rollback incomplete; actual registrations remain tracked for recovery.");
    return false;
}
bool HotkeyManager::release() {
    if (!initialized_) return true;
    const bool toggleReleased = set(0, std::nullopt);
    const bool restoreReleased = set(1, std::nullopt);
    if (toggleReleased && restoreReleased) { initialized_ = false; owner_ = NULL; }
    return toggleReleased && restoreReleased;
}
bool HotkeyManager::accepts(unsigned id, unsigned modifiers, unsigned key) const {
    const unsigned slot = id == ID_HOTKEY_TOGGLE ? 0 : 1;
    if (id != ID_HOTKEY_TOGGLE && id != ID_HOTKEY_RESTORE) return false;
    return actual_[slot] && actual_[slot]->modifiers == (modifiers & 0xf) && actual_[slot]->key == key;
}
