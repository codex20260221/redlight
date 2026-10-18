#include "Diagnostics.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace {

HANDLE g_logFile = INVALID_HANDLE_VALUE;
bool g_initialized = false;
SRWLOCK g_lock = SRWLOCK_INIT;

constexpr ULONGLONG kMaxLogSizeBytes = 1ull << 20;

std::wstring BuildLogDirectory() {
    std::wstring directory(32768, L'\0');
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory.data(), static_cast<DWORD>(directory.size()));
    if (length == 0 || length >= directory.size()) {
        return {};
    }

    directory.resize(length);
    directory += L"\\RedLight";

    if (!CreateDirectoryW(directory.c_str(), NULL)) {
        const DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            return {};
        }
    }

    return directory;
}

std::wstring BuildLogPath(const wchar_t* fileName) {
    std::wstring directory = BuildLogDirectory();
    if (directory.empty()) {
        return {};
    }

    directory += L"\\";
    directory += fileName ? fileName : L"";
    return directory;
}

std::string BuildTimestamp() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);

    char timestamp[64] = {};
    std::snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        static_cast<unsigned>(st.wYear),
        static_cast<unsigned>(st.wMonth),
        static_cast<unsigned>(st.wDay),
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned>(st.wMilliseconds));
    return timestamp;
}

void WriteDebugFailure(const char* action, const std::wstring& path, DWORD error) {
    char buffer[512] = {};
    std::snprintf(buffer, sizeof(buffer), "RedLight: diagnostics %s failed for %ls (error %lu)\n",
        action ? action : "operation",
        path.c_str(),
        static_cast<unsigned long>(error));
    OutputDebugStringA(buffer);
}

bool IsLogTooLarge(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return false;
    }

    const ULONGLONG size = (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) |
        static_cast<ULONGLONG>(data.nFileSizeLow);
    return size > kMaxLogSizeBytes;
}

void BestEffortDeleteFile(const std::wstring& path) {
    if (!DeleteFileW(path.c_str())) {
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            WriteDebugFailure("delete", path, error);
        }
    }
}

void BestEffortMoveFile(const std::wstring& from, const std::wstring& to) {
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return;
        }

        std::wstring path = from;
        path += L" -> ";
        path += to;
        WriteDebugFailure("rename", path, error);
    }
}

void RotateLogsIfNeeded() {
    const std::wstring logPath = BuildLogPath(L"redlight.log");
    if (logPath.empty() || !IsLogTooLarge(logPath)) {
        return;
    }

    const std::wstring log1Path = BuildLogPath(L"redlight.1.log");
    const std::wstring log2Path = BuildLogPath(L"redlight.2.log");
    const std::wstring log3Path = BuildLogPath(L"redlight.3.log");
    if (log1Path.empty() || log2Path.empty() || log3Path.empty()) {
        return;
    }

    BestEffortDeleteFile(log3Path);
    BestEffortMoveFile(log2Path, log3Path);
    BestEffortMoveFile(log1Path, log2Path);
    BestEffortMoveFile(logPath, log1Path);
}

void OpenLogFile() {
    const std::wstring path = BuildLogPath(L"redlight.log");
    if (path.empty()) {
        return;
    }

    RotateLogsIfNeeded();

    g_logFile = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (g_logFile == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("RedLight: diagnostics log file unavailable; continuing with OutputDebugStringA only.\n");
    }
}

void WriteLine(const char* message) {
    std::string line = BuildTimestamp();
    line += " ";
    line += message ? message : "";
    line += "\r\n";

    OutputDebugStringA(line.c_str());

    if (g_logFile == INVALID_HANDLE_VALUE) {
        return;
    }

    AcquireSRWLockExclusive(&g_lock);
    DWORD written = 0;
    if (!WriteFile(g_logFile, line.c_str(), static_cast<DWORD>(line.size()), &written, NULL) || written != line.size()) {
        OutputDebugStringA("RedLight: diagnostics write failed.\n");
    }
    ReleaseSRWLockExclusive(&g_lock);
}

}  // namespace

namespace Diagnostics {

std::wstring LogDirectory() { return BuildLogDirectory(); }

void Initialize() {
    if (g_initialized) {
        return;
    }

    OpenLogFile();
    g_initialized = true;
}

void Log(const char* message) {
    if (!message) {
        return;
    }

    if (!g_initialized) {
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
        return;
    }

    WriteLine(message);
}

void LogFormat(const char* format, ...) {
    if (!format) {
        return;
    }

    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    (void)vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);

    Log(buffer);
}

void Shutdown() {
    if (!g_initialized) {
        return;
    }

    if (g_logFile != INVALID_HANDLE_VALUE) {
        AcquireSRWLockExclusive(&g_lock);
        if (!FlushFileBuffers(g_logFile)) OutputDebugStringA("RedLight: diagnostics flush failed.\n");
        if (!CloseHandle(g_logFile)) OutputDebugStringA("RedLight: diagnostics close failed.\n");
        g_logFile = INVALID_HANDLE_VALUE;
        ReleaseSRWLockExclusive(&g_lock);
    }

    g_initialized = false;
}

}  // namespace Diagnostics
