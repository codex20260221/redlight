#include "VerificationClipboard.h"
#include "Clipboard.h"
#include "Diagnostics.h"

bool CopyVerificationJsonToClipboard(HWND owner, const VerificationJsonSnapshot& snapshot) noexcept {
    try {
        char output[VerificationJsonBufferSize];
        const auto text = FormatVerificationJson(snapshot, output);
        if (!text) { Diagnostics::Log("Verification JSON refused an invalid or incomplete cached snapshot."); return false; }
        return CopyUtf8TextToClipboard(owner, *text);
    } catch (...) {
        Diagnostics::Log("Verification JSON clipboard preparation failed.");
        return false;
    }
}
