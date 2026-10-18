#include "DisplayContext.h"
#include "Diagnostics.h"
#include <windows.h>

std::optional<DisplayContextSnapshot> CaptureDisplayContext() noexcept {
    const int monitors = GetSystemMetrics(SM_CMONITORS);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const bool remote = GetSystemMetrics(SM_REMOTESESSION) != 0;
    if (monitors <= 0 || width <= 0 || height <= 0) {
        // GetSystemMetrics supplies no extended GetLastError information.
        Diagnostics::LogFormat("Display context unavailable: monitors=%d, virtual width=%d, height=%d.", monitors, width, height);
        return std::nullopt;
    }
    // Zero and negative origins are valid. Preserve signed desktop coordinates.
    return DisplayContextSnapshot{static_cast<unsigned>(monitors), static_cast<unsigned>(width), static_cast<unsigned>(height), remote, left, top};
}

bool RefreshDisplayContextCache(DisplayContextCache& cache, DisplayContextReason reason) noexcept {
    const auto snapshot = CaptureDisplayContext();
    cache.record(snapshot, reason, GetTickCount64());
    return !cache.lastFailed();
}
