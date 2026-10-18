#include "StandardOutput.h"
#include <windows.h>
#include <algorithm>
#include <limits>

bool WriteStandardOutput(std::string_view text) noexcept {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    bool attached = false;
    if (!output || output == INVALID_HANDLE_VALUE) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) return false;
        attached = true;
        output = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    bool succeeded = output && output != INVALID_HANDLE_VALUE;
    while (succeeded && !text.empty()) {
        const auto count = static_cast<DWORD>((std::min)(text.size(), static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(output, text.data(), count, &written, nullptr) || written == 0 || written > count) {
            succeeded = false;
            break;
        }
        text.remove_prefix(written);
    }
    if (attached && !FreeConsole()) succeeded = false;
    return succeeded;
}
