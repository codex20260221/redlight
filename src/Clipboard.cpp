#include "Clipboard.h"
#include "Diagnostics.h"
#include <cstring>
#include <limits>

namespace {
void FreeClipboardBlock(HGLOBAL block) {
    if (GlobalFree(block)) Diagnostics::LogFormat("GlobalFree(clipboard) failed (error %lu).", GetLastError());
}
}

bool CopyUtf8TextToClipboard(HWND owner, std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), NULL, 0);
    if (!length) {
        Diagnostics::LogFormat("Clipboard UTF-8 conversion failed (error %lu).", GetLastError());
        return false;
    }
    const auto bytes = (static_cast<std::size_t>(length) + 1) * sizeof(wchar_t);
    HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!block) { Diagnostics::Log("Clipboard allocation failed."); return false; }
    auto* buffer = static_cast<wchar_t*>(GlobalLock(block));
    if (!buffer) { FreeClipboardBlock(block); Diagnostics::Log("Clipboard memory lock failed."); return false; }
    const int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), buffer, length);
    buffer[length] = L'\0';
    SetLastError(ERROR_SUCCESS);
    const BOOL stillLocked = GlobalUnlock(block);
    const DWORD unlockError = GetLastError();
    if ((!stillLocked && unlockError != ERROR_SUCCESS) || converted != length) {
        Diagnostics::LogFormat("Clipboard text preparation failed (unlock error %lu).", unlockError);
        FreeClipboardBlock(block);
        return false;
    }
    // Allocate and prepare before opening/emptying the user's clipboard.
    if (!OpenClipboard(owner)) {
        Diagnostics::LogFormat("OpenClipboard failed (error %lu).", GetLastError());
        FreeClipboardBlock(block);
        return false;
    }
    bool transferred = false;
    if (!EmptyClipboard()) Diagnostics::LogFormat("EmptyClipboard failed (error %lu).", GetLastError());
    else if (!SetClipboardData(CF_UNICODETEXT, block)) Diagnostics::LogFormat("SetClipboardData failed (error %lu).", GetLastError());
    else transferred = true; // Windows now owns the memory block.
    const bool closed = CloseClipboard() != FALSE;
    if (!closed) Diagnostics::LogFormat("CloseClipboard failed (error %lu).", GetLastError());
    if (!transferred) FreeClipboardBlock(block);
    return transferred && closed;
}
