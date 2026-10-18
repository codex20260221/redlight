#include "ProfileValidation.h"
#include "Preferences.h"
#include "JsonString.h"
#include "StandardOutput.h"
#include "Diagnostics.h"
#include <windows.h>
#include <array>
#include <cstdio>

namespace {
struct File {
    explicit File(HANDLE value) noexcept : handle(value) {}
    ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    bool close() noexcept {
        if (handle == INVALID_HANDLE_VALUE) return true;
        if (!CloseHandle(handle)) {
            error = GetLastError();
            Diagnostics::LogFormat("Profile validation file close failed (error %lu).", error);
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
            Diagnostics::LogFormat("Profile validation thread error-mode restore failed (error %lu).", error);
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
    Diagnostics::Log("Offline profile validation could not write standard output.");
    return 1;
}
int ValidateProfileCommand(const wchar_t* path) {
    const auto read = ReadProfile(path);
    char output[1024];
    int length = 0;
    int exitCode = 0;
    if (read.stage) {
        Diagnostics::LogFormat("Profile validation file error at %s (error %lu).", read.stage, read.error);
        length = std::snprintf(output, sizeof(output),
            "{\"protocol\":1,\"status\":\"file-error\",\"valid\":false,\"stage\":\"%s\",\"error_code\":%lu}\n", read.stage, read.error);
        exitCode = 1;
    } else {
        const auto parsed = ParsePreferences(std::string_view(read.bytes.data(), read.length));
        if (parsed.preferences) {
            return WriteResult("{\"protocol\":1,\"status\":\"valid-profile\",\"valid\":true}\n", 0);
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
    try { return ValidateProfileCommand(path); }
    catch (...) {
        Diagnostics::Log("Offline profile validation failed while constructing its result.");
        return 1;
    }
}
