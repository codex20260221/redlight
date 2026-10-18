#include "StatusClipboard.h"
#include "Clipboard.h"
#include "Diagnostics.h"

bool CopyTrayStatusJsonToClipboard(HWND owner, const TrayStatus& snapshot) noexcept {
    try {
        const auto text = TrayStatusJson(snapshot);
        if (!text) {
            Diagnostics::Log("Status JSON clipboard export refused an inconsistent cached snapshot.");
            return false;
        }
        if (CopyUtf8TextToClipboard(owner, *text)) return true;
        Diagnostics::Log("Cached status JSON could not be copied to the clipboard.");
    } catch (...) {
        Diagnostics::Log("Status JSON clipboard export encountered an internal preparation failure.");
    }
    return false;
}
