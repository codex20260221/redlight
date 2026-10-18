#pragma once
#include "Utf8.h"
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

inline constexpr std::size_t CommandHelpBufferSize = 4096;
inline constexpr char CommandHelpBody[] =
    "Usage: RedLight.exe [option]\n\n"
    "--restore / --reset-display: restore display colors\n"
    "--help: show this help dialog\n--help-text: write this help to standard output\n"
    "--version: show version dialog\n"
    "--build-info: report this binary's build metadata as JSON\n"
    "--settings: report saved/default preferences as JSON\n"
    "--status: report cached tray state as JSON without starting it\n"
    "--validate-profile=<file>: inspect/validate a file offline (schema/settings JSON)\n"
    "--normalize-profile=<file>: write a canonical current-format profile\n"
    "--compare-profile=<file>: compare exact saved/candidate values (JSON)\n"
    "--save-profile=<file>: explicitly save for next start (JSON; no running instance)\n"
    "--export-profile: write saved/default preferences as profile text\n"
    "--backend=auto|magnification|gamma: one-start backend override\n\n"
    "Without options, start in the tray with the filter off.\n"
    "Default shortcuts: Ctrl+Alt+R toggles; Ctrl+Alt+Shift+R restores.\n"
    "Expert controls offers alternative shortcut profiles.\n"
    "See README for JSON fields, exit codes, and PowerShell examples.\n";

inline std::optional<std::string_view> FormatCommandLineHelp(std::string_view version, std::span<char> output) noexcept {
    if (version.empty() || version.size() > 128 || !IsValidUtf8(version) || output.empty()) return std::nullopt;
    for (const unsigned char value : version) if (value < 0x20 || value == 0x7f) return std::nullopt;
    const int length = std::snprintf(output.data(), output.size(), "RedLight %.*s\n\n%s", static_cast<int>(version.size()), version.data(), CommandHelpBody);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}

// 0 complete UTF-8 output, 1 formatting/output failure. Never opens a dialog.
int WriteCommandLineHelp(std::string_view version) noexcept;
