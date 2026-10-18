#include "PreferencesClipboard.h"
#include "Clipboard.h"
#include "Diagnostics.h"

bool CopyCurrentPreferencesJsonToClipboard(HWND owner, const CurrentPreferencesSnapshot& snapshot) noexcept {
    try {
        char output[CurrentPreferencesJsonBufferSize];
        const auto text = FormatCurrentPreferencesJson(snapshot, output);
        if (!text) { Diagnostics::Log("Current preference JSON refused an invalid or incomplete snapshot."); return false; }
        return CopyUtf8TextToClipboard(owner, *text);
    } catch (...) {
        Diagnostics::Log("Current preference JSON clipboard preparation failed.");
        return false;
    }
}
