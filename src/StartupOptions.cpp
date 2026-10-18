#include "StartupOptions.h"
#include "Diagnostics.h"
#include <windows.h>
#include <shellapi.h>
#include <vector>

namespace {
struct Arguments {
    explicit Arguments(LPWSTR* value) noexcept : data(value) {}
    Arguments(const Arguments&) = delete;
    Arguments& operator=(const Arguments&) = delete;
    LPWSTR* data;
    ~Arguments() {
        if (data && LocalFree(data)) Diagnostics::LogFormat("Freeing command-line arguments failed (error %lu).", GetLastError());
    }
};
}

StartupOptions GetStartupOptions() noexcept {
    try {
        int count = 0;
        Arguments arguments{CommandLineToArgvW(GetCommandLineW(), &count)};
        if (!arguments.data || count < 1) {
            Diagnostics::LogFormat("Reading native command-line arguments failed (error %lu).", GetLastError());
            return {StartupAction::Failed, std::nullopt};
        }
        std::vector<std::wstring_view> views;
        views.reserve(static_cast<std::size_t>(count - 1));
        for (int i = 1; i < count; ++i) views.emplace_back(arguments.data[i]);
        return ParseStartupOptions(views);
    } catch (...) {
        Diagnostics::Log("Constructing startup options failed; no display backend was initialized.");
        return {StartupAction::Failed, std::nullopt};
    }
}
