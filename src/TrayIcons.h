#pragma once
#include <windows.h>
#include "DisplayFilter.h"
#include <array>

enum class TraySymbol { Off, StrictRed, LumaRed, Attention };

// Uses cached state only. Recovery and unconfirmed output outrank an active mode.
inline TraySymbol SelectTraySymbol(bool active, bool needsRestore, bool hasError,
    RedMode mode, BackendVerification verification, bool restoreRequested = false) noexcept {
    if (restoreRequested || (needsRestore && (!active || hasError)) ||
        verification == BackendVerification::Changed || verification == BackendVerification::Unavailable)
        return TraySymbol::Attention;
    if (!active) return TraySymbol::Off;
    return mode == RedMode::StrictRed ? TraySymbol::StrictRed : TraySymbol::LumaRed;
}

class TrayIcons {
public:
    bool initialize(HINSTANCE instance);
    HICON get(TraySymbol symbol) const noexcept;
private:
    // Resource handles loaded with LR_SHARED: Windows owns them, including partial loads.
    std::array<HICON, 4> icons_{};
};
