#include "ProfileValidation.h"
#include "ProfileComparison.h"
#include "ProfileSave.h"
#include "Preferences.h"
#include "JsonString.h"
#include "StandardOutput.h"
#include "Diagnostics.h"
#include <windows.h>
#include <array>
#include <cstdio>

namespace {
enum class ProfileFileAction { Validate, Normalize, Compare, Save };
struct File {
    explicit File(HANDLE value) noexcept : handle(value) {}
    ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    bool close() noexcept {
        if (handle == INVALID_HANDLE_VALUE) return true;
        if (!CloseHandle(handle)) {
            error = GetLastError();
            Diagnostics::LogFormat("Profile file close failed (error %lu).", error);
            return false;
        }
        handle = INVALID_HANDLE_VALUE;
        return true;
    }
    HANDLE handle;
    DWORD error = ERROR_SUCCESS;
};
struct ErrorMode {
    ~ErrorMode() { restore(); }
    ErrorMode() = default;
    ErrorMode(const ErrorMode&) = delete;
    ErrorMode& operator=(const ErrorMode&) = delete;
    bool suppressDialogs() noexcept {
        changed = SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS, &previous) != FALSE;
        return changed;
    }
    bool restore() noexcept {
        if (!changed) return true;
        if (!SetThreadErrorMode(previous, nullptr)) {
            error = GetLastError();
            Diagnostics::LogFormat("Profile file thread error-mode restore failed (error %lu).", error);
            return false;
        }
        changed = false;
        return true;
    }
    DWORD previous = 0;
    DWORD error = ERROR_SUCCESS;
    bool changed = false;
};
struct ReadResult {
    std::array<char, 4097> bytes{}; // One extra byte proves the parser's 4096-byte bound was exceeded.
    std::size_t length = 0;
    const char* stage = nullptr;
    DWORD error = ERROR_SUCCESS;
};
ReadResult ReadProfile(const wchar_t* path) noexcept {
    ReadResult result;
    if (!path || !*path) { result.stage = "arguments"; result.error = ERROR_INVALID_PARAMETER; return result; }
    ErrorMode mode;
    if (!mode.suppressDialogs()) { result.stage = "error-mode"; result.error = GetLastError(); return result; }
    File file(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (file.handle == INVALID_HANDLE_VALUE) { result.stage = "open"; result.error = GetLastError(); }
    else {
        SetLastError(ERROR_SUCCESS);
        const DWORD type = GetFileType(file.handle);
        if (type != FILE_TYPE_DISK) {
            result.stage = "type";
            const DWORD error = GetLastError();
            result.error = type == FILE_TYPE_UNKNOWN && error != ERROR_SUCCESS ? error : ERROR_INVALID_DATA;
        } else while (result.length < result.bytes.size()) {
            const auto count = static_cast<DWORD>(result.bytes.size() - result.length);
            DWORD received = 0;
            if (!ReadFile(file.handle, result.bytes.data() + result.length, count, &received, nullptr)) {
                const DWORD error = GetLastError();
                if (error != ERROR_HANDLE_EOF) { result.stage = "read"; result.error = error; }
                break;
            }
            if (received > count) { result.stage = "read"; result.error = ERROR_INVALID_DATA; break; }
            result.length += received;
            if (received == 0) break;
        }
    }
    if (!file.close() && !result.stage) { result.stage = "close"; result.error = file.error; }
    if (!mode.restore() && !result.stage) { result.stage = "error-mode-restore"; result.error = mode.error; }
    return result;
}
int WriteResult(std::string_view text, int exitCode) noexcept {
    if (WriteStandardOutput(text)) return exitCode;
    Diagnostics::Log("Offline profile command could not write standard output; discard any partial output.");
    return 1;
}
int WriteValidatedProfile(const PreferencesParseResult& parsed) noexcept {
    if (!parsed.preferences || parsed.inputSchema == 0 || parsed.inputSchema > PreferencesProfileVersion) return 1;
    const auto& value = *parsed.preferences;
    const char* defaulted = parsed.inputSchema == 1 ? "\"hotkeys\",\"expiry_warning\"" :
        (parsed.inputSchema == 2 ? "\"expiry_warning\"" : "");
    char output[1024];
    const int length = std::snprintf(output, sizeof(output),
        "{\"protocol\":1,\"status\":\"valid-profile\",\"valid\":true,\"input_schema\":%u,\"profile_schema\":%u,"
        "\"schema_upgrade_required\":%s,\"defaulted_fields\":[%s],\"preferences\":{"
        "\"mode\":\"%s\",\"luma\":\"%s\",\"backend\":\"%s\",\"session_minutes\":%u,\"hotkeys\":\"%s\",\"expiry_warning\":\"%s\"}}\n",
        parsed.inputSchema, PreferencesProfileVersion, parsed.inputSchema < PreferencesProfileVersion ? "true" : "false", defaulted,
        PreferenceModeToken(value.mode), PreferenceLumaToken(value.luma), PreferenceBackendToken(value.backend), value.sessionMinutes,
        ShortcutProfileToken(value.shortcuts), ExpiryWarningToken(value.expiryWarning));
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
        Diagnostics::Log("Offline profile inspection could not fit its checked JSON buffer; no output was written.");
        return 1;
    }
    return WriteResult(std::string_view(output, static_cast<std::size_t>(length)), 0);
}
int ProfileFileCommand(const wchar_t* path, ProfileFileAction action) {
    const bool normalize = action == ProfileFileAction::Normalize;
    const auto read = ReadProfile(path);
    char output[1024];
    int length = 0;
    int exitCode = 0;
    if (read.stage) {
        Diagnostics::LogFormat("Profile file error at %s (error %lu).", read.stage, read.error);
        if (normalize) return 1;
        length = std::snprintf(output, sizeof(output),
            "{\"protocol\":1,\"status\":\"file-error\",\"valid\":false,\"stage\":\"%s\",\"error_code\":%lu}\n", read.stage, read.error);
        exitCode = 1;
    } else {
        const auto parsed = ParsePreferences(std::string_view(read.bytes.data(), read.length));
        if (parsed.preferences) {
            if (action == ProfileFileAction::Save) return SaveSettingsProfileForNextStart(*parsed.preferences);
            if (action == ProfileFileAction::Compare) return CompareSavedSettingsProfile(*parsed.preferences);
            if (normalize) {
                const auto text = SerializePreferences(*parsed.preferences);
                if (!text) { Diagnostics::Log("Offline profile normalization could not serialize the validated settings."); return 1; }
                return WriteResult(*text, 0);
            }
            return WriteValidatedProfile(parsed);
        }
        if (normalize) {
            Diagnostics::LogFormat("Offline profile normalization rejected invalid data at line %u: %s.",
                parsed.line, parsed.error ? parsed.error : "Invalid profile");
            return 4;
        }
        std::array<char, 512> reason{};
        const auto quoted = QuoteJsonString(parsed.error ? parsed.error : "Invalid profile", reason);
        if (!quoted) { Diagnostics::Log("Offline profile validation reason exceeded its JSON buffer."); return 1; }
        length = std::snprintf(output, sizeof(output),
            "{\"protocol\":1,\"status\":\"invalid-profile\",\"valid\":false,\"line\":%u,\"reason\":%.*s}\n",
            parsed.line, static_cast<int>(quoted->size()), quoted->data());
        exitCode = 4;
    }
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
        Diagnostics::Log("Offline profile validation result could not be formatted within its JSON buffer.");
        return 1;
    }
    return WriteResult(std::string_view(output, static_cast<std::size_t>(length)), exitCode);
}
}

int ValidateSettingsProfileFile(const wchar_t* path) noexcept {
    try { return ProfileFileCommand(path, ProfileFileAction::Validate); }
    catch (...) {
        Diagnostics::Log("Offline profile validation failed while constructing its result.");
        return 1;
    }
}

int NormalizeSettingsProfileFile(const wchar_t* path) noexcept {
    try { return ProfileFileCommand(path, ProfileFileAction::Normalize); }
    catch (...) {
        Diagnostics::Log("Offline profile normalization failed before complete output; discard any partial output.");
        return 1;
    }
}

int CompareSettingsProfileFile(const wchar_t* path) noexcept {
    try { return ProfileFileCommand(path, ProfileFileAction::Compare); }
    catch (...) {
        Diagnostics::Log("Offline profile-file comparison failed before complete output.");
        return 1;
    }
}

int SaveSettingsProfileFile(const wchar_t* path) noexcept {
    try { return ProfileFileCommand(path, ProfileFileAction::Save); }
    catch (...) {
        Diagnostics::Log("Offline profile-file save failed before complete output.");
        return 1;
    }
}
