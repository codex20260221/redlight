#include "RestorationHistoryClipboard.h"
#include "Clipboard.h"
#include "Diagnostics.h"

bool CopyRestorationHistoryJsonToClipboard(HWND owner, const RestorationHistory& snapshot, std::uint64_t now) noexcept {
    try {
        char output[RestorationHistoryJsonBufferSize];
        const auto text = FormatRestorationHistoryJson(snapshot, now, output);
        if (!text) { Diagnostics::Log("Restoration-history JSON refused an invalid or incomplete snapshot."); return false; }
        return CopyUtf8TextToClipboard(owner, *text);
    } catch (...) {
        Diagnostics::Log("Restoration-history JSON clipboard preparation failed.");
        return false;
    }
}
