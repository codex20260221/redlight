#pragma once
#include "BackendPreference.h"
#include <algorithm>
#include <span>
#include <string>
#include <string_view>

enum class StartupAction { Tray, Restore, Help, Version, Status, ValidateProfile, Invalid, Failed };
struct StartupOptions {
    StartupAction action = StartupAction::Tray;
    std::optional<BackendPreference> backendOverride;
    std::wstring profilePath;
    bool jsonErrors = false;
};
inline StartupOptions ParseStartupOptions(std::span<const std::wstring_view> args) {
    if (args.empty()) return {};
    const bool jsonErrors = std::any_of(args.begin(), args.end(), [](std::wstring_view arg) {
        return arg.starts_with(L"--status") || arg.starts_with(L"--validate-profile");
    });
    const auto invalid = [&] { return StartupOptions{StartupAction::Invalid, std::nullopt, {}, jsonErrors}; };
    if (args.size() != 1) return invalid();
    const auto arg = args[0];
    if (arg == L"--restore" || arg == L"--reset-display") return {StartupAction::Restore, std::nullopt};
    if (arg == L"--help" || arg == L"-h" || arg == L"/?") return {StartupAction::Help, std::nullopt};
    if (arg == L"--version") return {StartupAction::Version, std::nullopt};
    if (arg == L"--status") return {StartupAction::Status, std::nullopt, {}, true};
    if (arg.starts_with(L"--validate-profile=")) {
        const auto path = arg.substr(19);
        if (!path.empty() && path.size() <= 32767 && path.find(L'\0') == std::wstring_view::npos)
            return {StartupAction::ValidateProfile, std::nullopt, std::wstring(path), true};
        return invalid();
    }
    if (arg.starts_with(L"--backend=")) {
        if (const auto preference = ParseBackendPreference(arg.substr(10))) {
            return {StartupAction::Tray, preference};
        }
    }
    return invalid();
}
inline StartupAction ParseArguments(std::span<const std::wstring_view> args) {
    return ParseStartupOptions(args).action;
}
