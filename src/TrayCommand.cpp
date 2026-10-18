#include "TrayCommand.h"
#include "Diagnostics.h"

TrayCommandPostResult TryPostTrayCommand(HWND owner, unsigned command) noexcept {
    if (!owner) {
        Diagnostics::Log("Refused tray command posting without an owner window.");
        return {false, ERROR_INVALID_WINDOW_HANDLE};
    }
    if (PostMessageA(owner, WM_COMMAND, static_cast<WPARAM>(command), 0)) return {true, 0};
    const DWORD error = GetLastError();
    Diagnostics::LogFormat("Posting tray command %u failed (error %lu).", command, error);
    return {false, error};
}
