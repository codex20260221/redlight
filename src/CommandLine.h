#pragma once
#include "BackendPreference.h"
#include <span>
#include <string_view>

enum class StartupAction { Tray, Restore, Help, Version, Invalid };
struct StartupOptions {
    StartupAction action = StartupAction::Tray;
    std::optional<BackendPreference> backendOverride;
};
inline StartupOptions ParseStartupOptions(std::span<const std::wstring_view> args) {
    if (args.empty()) return {};
    if (args.size() != 1) return {StartupAction::Invalid, std::nullopt};
    const auto arg = args[0];
    if (arg == L"--restore" || arg == L"--reset-display") return {StartupAction::Restore, std::nullopt};
    if (arg == L"--help" || arg == L"-h" || arg == L"/?") return {StartupAction::Help, std::nullopt};
    if (arg == L"--version") return {StartupAction::Version, std::nullopt};
    if (arg.starts_with(L"--backend=")) {
        if (const auto preference = ParseBackendPreference(arg.substr(10))) {
            return {StartupAction::Tray, preference};
        }
    }
    return {StartupAction::Invalid, std::nullopt};
}
inline StartupAction ParseArguments(std::span<const std::wstring_view> args) {
    return ParseStartupOptions(args).action;
}
