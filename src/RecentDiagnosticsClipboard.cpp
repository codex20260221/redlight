#include "RecentDiagnosticsClipboard.h"
#include "Clipboard.h"
#include "Diagnostics.h"

bool CopyRecentDiagnosticsToClipboard(HWND owner) noexcept {
    try {
        const auto text = Diagnostics::RecentLogText();
        if (!text) return false;
        return CopyUtf8TextToClipboard(owner, *text);
    } catch (...) { return false; }
}
