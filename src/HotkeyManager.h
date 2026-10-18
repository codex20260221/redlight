#pragma once
#include <windows.h>
#include "ShortcutProfile.h"
#include "ShortcutDiagnostics.h"

// Track each actual registration separately, including partial rollback failures.
class HotkeyManager {
public:
    bool initialize(HWND owner, ShortcutProfile profile);
    bool change(ShortcutProfile profile);
    bool release();
    const std::optional<ShortcutBinding>& toggle() const { return actual_[0]; }
    const std::optional<ShortcutBinding>& restore() const { return actual_[1]; }
    bool accepts(unsigned id, unsigned modifiers, unsigned key) const;
    bool matches(ShortcutProfile profile) const noexcept {
        return IsValidShortcutProfile(static_cast<unsigned>(profile)) && actual_ == ShortcutBindings(profile);
    }
    const std::optional<ShortcutApiFailure>& lastFailure() const noexcept { return failure_; }
    bool rollbackIncomplete() const noexcept { return rollbackIncomplete_; }
private:
    bool set(unsigned slot, const std::optional<ShortcutBinding>& target, bool rollback = false);
    HWND owner_ = NULL;
    bool initialized_ = false;
    std::array<std::optional<ShortcutBinding>, 2> actual_{};
    std::optional<ShortcutApiFailure> failure_;
    bool rollbackIncomplete_ = false;
};
