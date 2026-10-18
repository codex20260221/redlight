#include "PreferencesQuery.h"
#include "CurrentPreferences.h"
#include "Diagnostics.h"
#include "StandardOutput.h"
#include <windows.h>

static_assert(sizeof(DWORD_PTR) == sizeof(std::uint64_t), "Preference IPC requires Windows x64.");
namespace {
int WritePreferenceQuery(std::string_view output, int result) noexcept {
    if (WriteStandardOutput(output)) return result;
    Diagnostics::Log("Current preference query could not write standard output; discard partial output.");
    return 1;
}
}
int QueryRunningPreferences() noexcept {
    try {
        const HWND tray = FindWindowA("RedLight.TrayWindow", "RedLight");
        if (!tray) return WritePreferenceQuery("{\"protocol\":1,\"status\":\"not-running\",\"preferences\":null}\n", 3);
        DWORD_PTR reply = 0;
        if (!SendMessageTimeoutA(tray, WM_REDLIGHT_PREFERENCES, REDLIGHT_PREFERENCES_PROTOCOL, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &reply)) {
            const DWORD error = GetLastError();
            Diagnostics::LogFormat("Current preference query failed or timed out (error %lu).", error);
            return WritePreferenceQuery("{\"protocol\":1,\"status\":\"query-failed\",\"preferences\":null}\n", 1);
        }
        const auto snapshot = UnpackCurrentPreferences(static_cast<std::uint64_t>(reply));
        char output[CurrentPreferencesJsonBufferSize];
        const auto text = snapshot ? FormatCurrentPreferencesJson(*snapshot, output) : std::nullopt;
        if (!text) {
            Diagnostics::Log("Current preference query received an unsupported or invalid response.");
            return WritePreferenceQuery("{\"protocol\":1,\"status\":\"invalid-response\",\"preferences\":null}\n", 1);
        }
        return WritePreferenceQuery(*text, 0);
    } catch (...) {
        Diagnostics::Log("Current preference query failed while preparing output.");
        return 1;
    }
}
