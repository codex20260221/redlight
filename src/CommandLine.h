#pragma once
#include "BackendPreference.h"
#include <algorithm>
#include <span>
#include <string>
#include <string_view>

enum class StartupAction { Tray, Restore, Help, HelpText, Version, BuildInfo, Settings, Status, ValidateProfile, NormalizeProfile, CompareProfile, SaveProfile, ExportProfile, Invalid, Failed };
enum class StartupErrorOutput { Dialog, Json, LogOnly };
struct StartupOptions {
    StartupAction action = StartupAction::Tray;
    std::optional<BackendPreference> backendOverride;
    std::wstring profilePath;
    StartupErrorOutput errorOutput = StartupErrorOutput::Dialog;
};
inline StartupOptions ParseStartupOptions(std::span<const std::wstring_view> args) {
    if (args.empty()) return {};
    const bool jsonErrors = std::any_of(args.begin(), args.end(), [](std::wstring_view arg) {
        return arg.starts_with(L"--status") || arg.starts_with(L"--validate-profile") || arg.starts_with(L"--build-info") ||
            arg.starts_with(L"--compare-profile") || arg.starts_with(L"--save-profile") || arg.starts_with(L"--settings");
    });
    const bool quietText = std::any_of(args.begin(), args.end(), [](std::wstring_view arg) {
        return arg.starts_with(L"--export-profile") || arg.starts_with(L"--normalize-profile") || arg.starts_with(L"--help-text");
    });
    const auto errorOutput = jsonErrors ? StartupErrorOutput::Json : (quietText ? StartupErrorOutput::LogOnly : StartupErrorOutput::Dialog);
    const auto invalid = [&] { return StartupOptions{StartupAction::Invalid, std::nullopt, {}, errorOutput}; };
    if (args.size() != 1) return invalid();
    const auto arg = args[0];
    if (arg == L"--restore" || arg == L"--reset-display") return {StartupAction::Restore, std::nullopt};
    if (arg == L"--help" || arg == L"-h" || arg == L"/?") return {StartupAction::Help, std::nullopt};
    if (arg == L"--help-text") return {StartupAction::HelpText, std::nullopt, {}, StartupErrorOutput::LogOnly};
    if (arg == L"--version") return {StartupAction::Version, std::nullopt};
    if (arg == L"--build-info") return {StartupAction::BuildInfo, std::nullopt, {}, StartupErrorOutput::Json};
    if (arg == L"--settings") return {StartupAction::Settings, std::nullopt, {}, StartupErrorOutput::Json};
    if (arg == L"--status") return {StartupAction::Status, std::nullopt, {}, StartupErrorOutput::Json};
    if (arg == L"--export-profile") return {StartupAction::ExportProfile, std::nullopt, {}, StartupErrorOutput::LogOnly};
    struct FileOption { std::wstring_view prefix; StartupAction action; StartupErrorOutput output; };
    constexpr FileOption fileOptions[] = {
        {L"--validate-profile=", StartupAction::ValidateProfile, StartupErrorOutput::Json},
        {L"--normalize-profile=", StartupAction::NormalizeProfile, StartupErrorOutput::LogOnly},
        {L"--compare-profile=", StartupAction::CompareProfile, StartupErrorOutput::Json},
        {L"--save-profile=", StartupAction::SaveProfile, StartupErrorOutput::Json}
    };
    for (const auto& option : fileOptions) {
        if (!arg.starts_with(option.prefix)) continue;
        const auto path = arg.substr(option.prefix.size());
        if (!path.empty() && path.size() <= 32767 && path.find(L'\0') == std::wstring_view::npos)
            return {option.action, std::nullopt, std::wstring(path), option.output};
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
