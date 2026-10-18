#include "Diagnostics.h"
#include "RecentDiagnostics.h"
#include <windows.h>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
HANDLE g_logFile = INVALID_HANDLE_VALUE;
bool g_initialized = false;
SRWLOCK g_lock = SRWLOCK_INIT;
std::array<std::wstring, 4> g_paths;
Diagnostics::Status g_status;
RecentDiagnosticsBuffer g_recent;
constexpr ULONGLONG kMaxLogSizeBytes = 1ull << 20;
constexpr std::size_t kMessageLimit = 1900;

struct Guard {
    Guard() noexcept { AcquireSRWLockExclusive(&g_lock); }
    ~Guard() { ReleaseSRWLockExclusive(&g_lock); }
};
std::wstring BuildLogDirectory(DWORD* failure = nullptr) {
    std::wstring directory(32768, L'\0');
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory.data(), static_cast<DWORD>(directory.size()));
    if (!length || length >= directory.size()) {
        if (failure) *failure = length ? ERROR_FILENAME_EXCED_RANGE : GetLastError();
        return {};
    }
    directory.resize(length);
    directory += L"\\RedLight";
    if (!CreateDirectoryW(directory.c_str(), NULL)) {
        const DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) { if (failure) *failure = error; return {}; }
        const DWORD attributes = GetFileAttributesW(directory.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (failure) *failure = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_DIRECTORY;
            return {};
        }
    }
    return directory;
}
void FileFailure(const char* action, DWORD error) noexcept {
    const bool changed = g_status.lastFileError != error || g_status.file == Diagnostics::FileState::Available;
    g_status.lastFileError = error;
    g_status.file = g_logFile == INVALID_HANDLE_VALUE ? Diagnostics::FileState::Unavailable : Diagnostics::FileState::Degraded;
    if (changed) {
        char text[256];
        std::snprintf(text, sizeof(text), "RedLight: diagnostics %s failed (error %lu); continuing with memory and debugger output.\n", action, error);
        g_recent.append(text);
        OutputDebugStringA(text);
    }
}
bool PreparePaths() {
    DWORD directoryError = 0;
    const auto directory = BuildLogDirectory(&directoryError);
    if (directory.empty()) { FileFailure("directory", directoryError ? directoryError : ERROR_PATH_NOT_FOUND); return false; }
    constexpr const wchar_t* names[] = {L"redlight.log", L"redlight.1.log", L"redlight.2.log", L"redlight.3.log"};
    std::array<std::wstring, 4> paths;
    for (unsigned i = 0; i < paths.size(); ++i) paths[i] = directory + L"\\" + names[i];
    g_paths = std::move(paths);
    return true;
}
bool OpenCurrentLog() noexcept {
    g_logFile = CreateFileW(g_paths[0].c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_logFile == INVALID_HANDLE_VALUE) { FileFailure("open", GetLastError()); return false; }
    return true;
}
bool CloseCurrentLog() noexcept {
    if (g_logFile == INVALID_HANDLE_VALUE) return true;
    if (!CloseHandle(g_logFile)) { FileFailure("close", GetLastError()); return false; }
    g_logFile = INVALID_HANDLE_VALUE;
    return true;
}
bool RotateLogs() noexcept {
    if (!CloseCurrentLog()) return false;
    for (int archive = 3; archive > 0; --archive) {
        if (!MoveFileExW(g_paths[archive - 1].c_str(), g_paths[archive].c_str(), MOVEFILE_REPLACE_EXISTING)) {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) continue;
            OpenCurrentLog(); // Keep recovery possible even after a partial archive move.
            FileFailure("rotation", error);
            return false;
        }
    }
    return OpenCurrentLog();
}
bool CurrentSize(ULONGLONG& bytes) noexcept {
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(g_logFile, &size)) { FileFailure("size query", GetLastError()); return false; }
    if (size.QuadPart < 0) { FileFailure("invalid file size", ERROR_INVALID_DATA); return false; }
    bytes = static_cast<ULONGLONG>(size.QuadPart);
    return true;
}
bool HaveCapacity(DWORD lineBytes) noexcept {
    ULONGLONG bytes = 0;
    if (!CurrentSize(bytes)) return false;
    if (bytes <= kMaxLogSizeBytes && lineBytes <= kMaxLogSizeBytes - bytes) return true;
    if (!RotateLogs() || !CurrentSize(bytes)) return false;
    if (bytes > kMaxLogSizeBytes || lineBytes > kMaxLogSizeBytes - bytes) {
        FileFailure("capacity after rotation", ERROR_DISK_FULL);
        return false;
    }
    return true;
}

void WriteLine(const char* message, bool formatTruncated = false) noexcept {
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    const bool truncated = formatTruncated || strnlen_s(message, kMessageLimit + 1) > kMessageLimit;
    char line[2048];
    const int length = std::snprintf(line, sizeof(line),
        "%04u-%02u-%02u %02u:%02u:%02u.%03u %.*s%s\r\n",
        static_cast<unsigned>(time.wYear), static_cast<unsigned>(time.wMonth), static_cast<unsigned>(time.wDay),
        static_cast<unsigned>(time.wHour), static_cast<unsigned>(time.wMinute), static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        static_cast<int>(kMessageLimit), message, truncated ? " [truncated]" : "");
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(line)) {
        OutputDebugStringA("RedLight: diagnostics line formatting failed.\n");
        return;
    }
    Guard guard;
    g_recent.append(std::string_view(line, static_cast<std::size_t>(length)));
    OutputDebugStringA(line);
    if (!g_initialized) return;
    if (truncated) ++g_status.truncatedLines;
    if (g_logFile == INVALID_HANDLE_VALUE || !HaveCapacity(static_cast<DWORD>(length))) {
        ++g_status.droppedLines;
        return;
    }
    DWORD written = 0;
    const bool wrote = WriteFile(g_logFile, line, static_cast<DWORD>(length), &written, NULL) != FALSE;
    if (!wrote || written != static_cast<DWORD>(length)) {
        FileFailure("write", wrote ? ERROR_WRITE_FAULT : GetLastError());
        ++g_status.droppedLines;
        return;
    }
    g_status.file = Diagnostics::FileState::Available;
    g_status.lastFileError = 0;
}
}
namespace Diagnostics {
std::wstring LogDirectory() { return BuildLogDirectory(); }
Status GetStatus() noexcept { Guard guard; return g_status; }
void ClearRecentLog() noexcept { Guard guard; g_recent.clear(); }
std::optional<std::string> RecentLogText() noexcept {
    try {
        // Allocate before taking the logger lock; snapshot construction does no I/O/logging.
        std::string text(RecentDiagnosticsBuffer::maximumTextBytes, '\0');
        Guard guard;
        const auto formatted = g_recent.format(text);
        if (!formatted) return std::nullopt;
        text.resize(formatted->size());
        return text;
    } catch (...) { return std::nullopt; }
}
void Initialize() noexcept {
    Guard guard;
    if (g_initialized) return;
    g_recent.clear();
    g_status = {};
    g_initialized = true;
    try {
        if (PreparePaths() && OpenCurrentLog() && HaveCapacity(0)) g_status.file = FileState::Available;
    } catch (...) { FileFailure("initialization", ERROR_NOT_ENOUGH_MEMORY); }
}
bool RetryFileLogging() noexcept {
    {
        Guard guard;
        if (!g_initialized) return false;
        try {
            if (g_logFile == INVALID_HANDLE_VALUE && (!PreparePaths() || !OpenCurrentLog())) return false;
            if (!HaveCapacity(0)) return false;
        } catch (...) { FileFailure("retry initialization", ERROR_NOT_ENOUGH_MEMORY); return false; }
    }
    // Check an actual write, not merely that the file could be reopened.
    Log("File logging retry requested.");
    return GetStatus().file == FileState::Available;
}
void Log(const char* message) noexcept { if (message) WriteLine(message); }
void LogFormat(const char* format, ...) noexcept {
    if (!format) return;
    char message[1024] = {};
    va_list args;
    va_start(args, format);
    const int result = vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);
    WriteLine(message, result < 0);
}
void Shutdown() noexcept {
    Guard guard;
    if (!g_initialized) return;
    if (g_logFile != INVALID_HANDLE_VALUE && !FlushFileBuffers(g_logFile)) FileFailure("flush", GetLastError());
    if (!CloseCurrentLog()) return; // Retain the live handle/state for a later retry.
    g_initialized = false;
    g_status.file = FileState::NotInitialized;
}
}  // namespace Diagnostics
