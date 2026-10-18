#pragma once
#include <span>
#include <string_view>

enum class StartupAction { Tray, Restore, Help, Version, Invalid };
inline StartupAction ParseArguments(std::span<const std::wstring_view> args) {
    if (args.empty()) return StartupAction::Tray;
    if (args.size() != 1) return StartupAction::Invalid;
    if (args[0] == L"--restore" || args[0] == L"--reset-display") return StartupAction::Restore;
    if (args[0] == L"--help" || args[0] == L"-h" || args[0] == L"/?") return StartupAction::Help;
    if (args[0] == L"--version") return StartupAction::Version;
    return StartupAction::Invalid;
}
