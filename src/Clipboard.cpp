#include "Clipboard.h"
#include "Diagnostics.h"
#include <cstring>
#include <limits>
#include <algorithm>

namespace {
void FreeClipboardBlock(HGLOBAL block) {
    if (GlobalFree(block)) Diagnostics::LogFormat("GlobalFree(clipboard) failed (error %lu).", GetLastError());
}
}

bool CopyUtf8TextToClipboard(HWND owner, std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) return false;
    if (text.find('\0') != std::string_view::npos) {
        Diagnostics::Log("Clipboard export rejected embedded NUL bytes to avoid a truncated successful copy.");
        return false;
    }
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

std::optional<std::string> ReadUtf8TextFromClipboard(HWND owner, std::size_t maxBytes) {
    if (maxBytes == 0 || maxBytes > 65536) return std::nullopt;
    // Allocate before opening the clipboard so conversion cannot allocate while it is held.
    std::string text(maxBytes, '\0');
    if (!OpenClipboard(owner)) {
        Diagnostics::LogFormat("OpenClipboard(read) failed (error %lu).", GetLastError());
        return std::nullopt;
    }
    const char* failedAction = nullptr;
    DWORD failureCode = 0;
    auto failure = [&](const char* action) {
        if (!failedAction) { failedAction = action; failureCode = GetLastError(); }
    };
    bool read = false;
    int converted = 0;
    HGLOBAL block = GetClipboardData(CF_UNICODETEXT);
    if (!block) failure("GetClipboardData");
    else {
        const auto bytes = GlobalSize(block);
        if (bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t) != 0) {
            failedAction = "invalid UTF-16 memory size";
            failureCode = ERROR_INVALID_DATA;
        }
        else {
            const auto* buffer = static_cast<const wchar_t*>(GlobalLock(block));
            if (!buffer) failure("GlobalLock");
            else {
                const auto capacity = (std::min)(bytes / sizeof(wchar_t), maxBytes + 1);
                std::size_t length = 0;
                while (length < capacity && buffer[length]) ++length;
                if (length == capacity) {
                    failedAction = "missing terminator or profile too large";
                    failureCode = ERROR_INVALID_DATA;
                } else if (length == 0) read = true;
                else {
                    converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer, static_cast<int>(length),
                        text.data(), static_cast<int>(maxBytes), NULL, NULL);
                    read = converted != 0;
                    if (!read) failure("WideCharToMultiByte");
                }
                SetLastError(ERROR_SUCCESS);
                if (!GlobalUnlock(block) && GetLastError() != ERROR_SUCCESS) { read = false; failure("GlobalUnlock"); }
            }
        }
    }
    if (!CloseClipboard()) { read = false; failure("CloseClipboard"); }
    if (failedAction) Diagnostics::LogFormat("Clipboard read %s failed (error %lu).", failedAction, failureCode);
    if (!read) return std::nullopt;
    text.resize(static_cast<std::size_t>(converted));
    return text;
}
