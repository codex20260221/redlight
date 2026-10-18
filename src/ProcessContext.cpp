#include "ProcessContext.h"
#include "Diagnostics.h"
#include <windows.h>

ProcessContextSnapshot CaptureProcessContext() noexcept {
    const DWORD process = GetCurrentProcessId();
    DWORD session = 0;
    SetLastError(ERROR_SUCCESS);
    if (ProcessIdToSessionId(process, &session)) {
        Diagnostics::LogFormat("Startup process %lu; Windows session %lu.", static_cast<unsigned long>(process), static_cast<unsigned long>(session));
        return {static_cast<std::uint32_t>(process), static_cast<std::uint32_t>(session), 0};
    }
    const DWORD error = GetLastError();
    if (error) Diagnostics::LogFormat("Startup session lookup failed for process %lu (error %lu).", static_cast<unsigned long>(process), static_cast<unsigned long>(error));
    else Diagnostics::LogFormat("Startup session lookup failed for process %lu (no native error code).", static_cast<unsigned long>(process));
    return {static_cast<std::uint32_t>(process), std::nullopt, static_cast<std::uint32_t>(error)};
}
