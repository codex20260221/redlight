#pragma once
#include <windows.h>
#include "ShortcutProfile.h"

// Track each actual registration separately, including partial rollback failures.
class HotkeyManager {
public:
    bool initialize(HWND owner, ShortcutProfile profile);
    bool change(ShortcutProfile profile);
    bool release();
    const std::optional<ShortcutBinding>& toggle() const { return actual_[0]; }
    const std::optional<ShortcutBinding>& restore() const { return actual_[1]; }
    bool accepts(unsigned id, unsigned modifiers, unsigned key) const;
private:
    bool set(unsigned slot, const std::optional<ShortcutBinding>& target);
    HWND owner_ = NULL;
    bool initialized_ = false;
    std::array<std::optional<ShortcutBinding>, 2> actual_{};
};
