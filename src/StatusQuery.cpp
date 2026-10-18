#include "StatusQuery.h"
#include "TrayStatus.h"
#include "StandardOutput.h"
#include <windows.h>

static_assert(sizeof(DWORD_PTR) == sizeof(std::uint64_t), "Status protocol requires Windows x64.");

int QueryRunningStatus() noexcept {
    try {
        const HWND tray = FindWindowA("RedLight.TrayWindow", "RedLight");
        if (!tray) {
            return WriteStandardOutput("{\"protocol\":1,\"status\":\"not-running\",\"active\":false}\n") ? 3 : 1;
        }
        DWORD_PTR reply = 0;
        SetLastError(ERROR_SUCCESS); // The API can fail without setting an error.
        if (!SendMessageTimeoutA(tray, WM_REDLIGHT_STATUS, REDLIGHT_STATUS_PROTOCOL, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &reply)) {
            const DWORD error = GetLastError();
            if (error) Diagnostics::LogFormat("Status query failed or timed out (error %lu).", error);
            else Diagnostics::Log("Status query failed or timed out; no native error code was provided.");
            WriteStandardOutput("{\"protocol\":1,\"status\":\"query-failed\",\"active\":false}\n");
            return 1;
        }
        const auto state = UnpackTrayStatus(static_cast<std::uint64_t>(reply));
        char output[TrayStatusJsonBufferSize];
        const auto json = state ? FormatTrayStatusJson(*state, output) : std::nullopt;
        if (!json) {
            Diagnostics::Log("Status query received an unsupported or invalid response.");
            WriteStandardOutput("{\"protocol\":1,\"status\":\"invalid-response\",\"active\":false}\n");
            return 1;
        }
        if (WriteStandardOutput(*json)) return 0;
        Diagnostics::Log("Status query could not write standard output.");
    } catch (...) {
        Diagnostics::Log("Status query failed while constructing its response.");
    }
    return 1;
}
