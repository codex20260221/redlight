// Production Diagnostics.cpp linked exclusively to in-memory Win32 file API fakes.
#include "Diagnostics.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
struct File { std::vector<char> bytes; };
struct Handle { std::shared_ptr<File> file; DWORD access; bool closed = false; };
struct Fake {
    std::wstring environment = L"C:\\InMemory\\Unicode-测试";
    std::map<std::wstring, std::shared_ptr<File>> files;
    std::set<std::wstring> directories;
    std::vector<std::unique_ptr<Handle>> handles;
    bool environmentPresent = true, failDirectory = false, failOpen = false;
    bool failMove = false, failSize = false, failWrite = false, shortWrite = false, failClose = false, failFlush = false;
    std::string lastDebug;
    unsigned writes = 0;
} fake;
thread_local DWORD lastError = 0;
std::wstring Path(unsigned archive = 0) {
    return fake.environment + L"\\RedLight\\redlight" +
        (archive ? L"." + std::to_wstring(archive) : L"") + L".log";
}
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Reset() {
    fake.failClose = false; fake.failFlush = false;
    Diagnostics::Shutdown();
    fake = {};
    lastError = 0;
}
void UnicodeAndBoundedLines() {
    Diagnostics::Initialize();
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::Available, "Unicode file path opens");
    Diagnostics::Log("first line");
    const std::string longMessage(8192, 'x');
    Diagnostics::Log(longMessage.c_str());
    Diagnostics::LogFormat("%s", longMessage.c_str());
    const auto status = Diagnostics::GetStatus();
    Check(status.truncatedLines == 2 && status.droppedLines == 0, "raw and formatted truncation counted");
    const auto& bytes = fake.files.at(Path())->bytes;
    const std::string text(bytes.begin(), bytes.end());
    Check(text.find("2026-10-18") != std::string::npos && text.find("[truncated]") != std::string::npos, "timestamp and truncation marker recorded");
    Check(bytes.size() < 4096, "individual messages bounded");
    Diagnostics::Shutdown();
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::NotInitialized, "shutdown closes logger");
}
void RuntimeRotation() {
    Diagnostics::Initialize();
    const std::string message(512, 'r');
    for (unsigned i = 0; i < 8500; ++i) Diagnostics::Log(message.c_str());
    Check(fake.files.size() == 4 && fake.files.contains(Path(3)), "runtime rotation retains only three archives and current log");
    for (const auto& [path, file] : fake.files) {
        (void)path;
        Check(file->bytes.size() <= (1u << 20), "normal writes never cross rotation limit");
    }
    Check(Diagnostics::GetStatus().droppedLines == 0, "successful rotation drops no records");
}
void RotationFailureAndRecovery() {
    auto seeded = std::make_shared<File>(); seeded->bytes.resize(1u << 20, 's');
    fake.files[Path()] = seeded;
    Diagnostics::Initialize();
    fake.failMove = true;
    Diagnostics::Log("must not grow full log while rotation fails");
    auto status = Diagnostics::GetStatus();
    Check(status.file == Diagnostics::FileState::Degraded && status.droppedLines == 1 && status.lastFileError == ERROR_ACCESS_DENIED, "rotation failure visible");
    Check(fake.files.at(Path())->bytes.size() == (1u << 20), "failed rotation cannot grow current file");
    fake.failMove = false;
    Diagnostics::Log("retry after sharing failure");
    status = Diagnostics::GetStatus();
    Check(status.file == Diagnostics::FileState::Available && status.droppedLines == 1 && fake.files.contains(Path(1)), "rotation retry recovers and preserves gap count");
}
void FileFailures() {
    Diagnostics::Initialize();
    fake.failWrite = true;
    Diagnostics::Log("disk-full failure");
    auto status = Diagnostics::GetStatus();
    Check(status.file == Diagnostics::FileState::Degraded && status.lastFileError == ERROR_DISK_FULL && status.droppedLines == 1, "write failure status");
    fake.failWrite = false; fake.failSize = true;
    Diagnostics::Log("size failure");
    Check(Diagnostics::GetStatus().droppedLines == 2 && fake.files.at(Path())->bytes.empty(), "unverified capacity never writes");
    fake.failSize = false; fake.shortWrite = true;
    Diagnostics::Log("partial write");
    Check(Diagnostics::GetStatus().lastFileError == ERROR_WRITE_FAULT && Diagnostics::GetStatus().droppedLines == 3, "partial write visible");
    fake.shortWrite = false;
    Diagnostics::Log("write recovered");
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::Available && Diagnostics::GetStatus().droppedLines == 3, "recovered writes retain historical gap count");
}
void CloseFailureRetry() {
    Diagnostics::Initialize();
    fake.failClose = true;
    Diagnostics::Shutdown();
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::Degraded, "close failure retains live state");
    Diagnostics::Log("handle remains tracked after failed close");
    Check(fake.writes == 1, "failed close did not discard usable handle");
    fake.failClose = false;
    Diagnostics::Shutdown();
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::NotInitialized, "close retry succeeds");
}
void FlushFailureStillCloses() {
    Diagnostics::Initialize();
    Diagnostics::Log("before flush failure");
    fake.failFlush = true;
    Diagnostics::Shutdown();
    const auto status = Diagnostics::GetStatus();
    Check(status.file == Diagnostics::FileState::NotInitialized && status.lastFileError == ERROR_WRITE_FAULT,
        "flush failure is recorded while successful close still completes shutdown");
    Check(fake.lastDebug.find("flush") != std::string::npos, "flush failure reaches debugger output");
}
void DirectoryAndOpenFailures() {
    fake.files[fake.environment + L"\\RedLight"] = std::make_shared<File>();
    Diagnostics::Initialize();
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::Unavailable && Diagnostics::GetStatus().lastFileError == ERROR_DIRECTORY,
        "existing file cannot masquerade as diagnostics folder");
    Check(Diagnostics::LogDirectory().empty(), "folder action cannot return an ordinary file");
    Reset(); fake.failOpen = true;
    Diagnostics::Initialize(); Diagnostics::Log("debugger fallback");
    Check(Diagnostics::GetStatus().file == Diagnostics::FileState::Unavailable && Diagnostics::GetStatus().droppedLines == 1, "unavailable disk logging keeps debugger fallback and gap count");
    Check(fake.lastDebug.find("debugger fallback") != std::string::npos, "fallback still receives record");
    Reset(); fake.environmentPresent = false;
    Diagnostics::Initialize();
    Check(Diagnostics::GetStatus().lastFileError == ERROR_ENVVAR_NOT_FOUND, "missing environment cause retained");
}
void RetryFileLogging() {
    fake.failOpen = true;
    Diagnostics::Initialize();
    Diagnostics::Log("unavailable record");
    fake.failOpen = false;
    Check(Diagnostics::RetryFileLogging(), "retry can recover from failed initial open");
    Check(Diagnostics::GetStatus().droppedLines == 1, "retry preserves missing-record count");
    fake.failWrite = true;
    Check(!Diagnostics::RetryFileLogging() && Diagnostics::GetStatus().droppedLines == 2, "retry cannot claim success when write still fails");
    fake.failWrite = false;
    Check(Diagnostics::RetryFileLogging() && Diagnostics::GetStatus().droppedLines == 2, "write retry recovers without losing history");
}
void ConcurrentRecords() {
    Diagnostics::Initialize();
    std::vector<std::thread> threads;
    for (unsigned worker = 0; worker < 4; ++worker) threads.emplace_back([worker] {
        for (unsigned record = 0; record < 100; ++record) Diagnostics::LogFormat("worker=%u record=%u", worker, record);
    });
    for (auto& thread : threads) thread.join();
    const auto& data = fake.files.at(Path())->bytes;
    Check(std::count(data.begin(), data.end(), '\n') == 400 && Diagnostics::GetStatus().droppedLines == 0, "thread-safe complete records");
}
Handle* OpenHandle(HANDLE raw) {
    if (!raw || raw == INVALID_HANDLE_VALUE) { lastError = ERROR_INVALID_HANDLE; return nullptr; }
    auto* handle = static_cast<Handle*>(raw);
    if (handle->closed || !handle->file) { lastError = ERROR_INVALID_HANDLE; return nullptr; }
    return handle;
}
}
DWORD GetLastError() { return lastError; }
void AcquireSRWLockExclusive(SRWLOCK* lock) { lock->mutex.lock(); }
void ReleaseSRWLockExclusive(SRWLOCK* lock) { lock->mutex.unlock(); }
DWORD GetEnvironmentVariableW(const wchar_t*, wchar_t* output, DWORD capacity) {
    if (!fake.environmentPresent) { lastError = ERROR_ENVVAR_NOT_FOUND; return 0; }
    const DWORD length = static_cast<DWORD>(fake.environment.size());
    if (capacity <= length) return length + 1;
    std::copy(fake.environment.begin(), fake.environment.end(), output); output[length] = 0;
    return length;
}
BOOL CreateDirectoryW(const wchar_t* path, void*) {
    if (fake.failDirectory) { lastError = ERROR_ACCESS_DENIED; return 0; }
    if (fake.directories.contains(path) || fake.files.contains(path)) { lastError = ERROR_ALREADY_EXISTS; return 0; }
    fake.directories.insert(path); return 1;
}
DWORD GetFileAttributesW(const wchar_t* path) {
    if (fake.directories.contains(path)) return FILE_ATTRIBUTE_DIRECTORY;
    if (fake.files.contains(path)) return FILE_ATTRIBUTE_NORMAL;
    lastError = ERROR_FILE_NOT_FOUND; return INVALID_FILE_ATTRIBUTES;
}
HANDLE CreateFileW(const wchar_t* path, DWORD access, DWORD, void*, DWORD, DWORD, HANDLE) {
    if (fake.failOpen) { lastError = ERROR_ACCESS_DENIED; return INVALID_HANDLE_VALUE; }
    auto& file = fake.files[path];
    if (!file) { file = std::make_shared<File>(); file->bytes.reserve(1u << 20); }
    auto handle = std::make_unique<Handle>(); handle->file = file; handle->access = access;
    HANDLE raw = handle.get(); fake.handles.push_back(std::move(handle)); return raw;
}
BOOL MoveFileExW(const wchar_t* source, const wchar_t* destination, DWORD) {
    const auto found = fake.files.find(source);
    if (found == fake.files.end()) { lastError = ERROR_FILE_NOT_FOUND; return 0; }
    if (fake.failMove) { lastError = ERROR_ACCESS_DENIED; return 0; }
    fake.files[destination] = found->second; fake.files.erase(found); return 1;
}
BOOL CloseHandle(HANDLE raw) {
    auto* handle = OpenHandle(raw); if (!handle) return 0;
    if (fake.failClose) { lastError = ERROR_ACCESS_DENIED; return 0; }
    handle->closed = true; handle->file.reset(); return 1;
}
BOOL GetFileSizeEx(HANDLE raw, LARGE_INTEGER* size) {
    auto* handle = OpenHandle(raw); if (!handle) return 0;
    if (fake.failSize || !(handle->access & FILE_READ_ATTRIBUTES)) { lastError = ERROR_ACCESS_DENIED; return 0; }
    size->QuadPart = static_cast<long long>(handle->file->bytes.size()); return 1;
}
BOOL WriteFile(HANDLE raw, const void* bytes, DWORD length, DWORD* written, void*) {
    auto* handle = OpenHandle(raw); if (!handle) return 0;
    if (fake.failWrite) { lastError = ERROR_DISK_FULL; *written = 0; return 0; }
    const DWORD amount = fake.shortWrite && length ? length - 1 : length;
    const auto* data = static_cast<const char*>(bytes);
    handle->file->bytes.insert(handle->file->bytes.end(), data, data + amount);
    *written = amount; ++fake.writes; return 1;
}
BOOL FlushFileBuffers(HANDLE raw) {
    if (!OpenHandle(raw)) return 0;
    if (fake.failFlush) { lastError = ERROR_WRITE_FAULT; return 0; }
    return 1;
}
void GetLocalTime(SYSTEMTIME* time) { *time = {2026, 10, 0, 18, 12, 0, 0, 1}; }
void OutputDebugStringA(const char* text) { fake.lastDebug = text; }
int main() {
    const std::pair<const char*, void(*)()> tests[] = {
        {"Unicode paths and bounded records", UnicodeAndBoundedLines}, {"Runtime archive rotation", RuntimeRotation},
        {"Rotation failure and retry", RotationFailureAndRecovery}, {"File write/size failures", FileFailures},
        {"Close failure retry", CloseFailureRetry}, {"Flush failure", FlushFailureStillCloses}, {"Directory/open failures", DirectoryAndOpenFailures},
        {"File logging retry", RetryFileLogging}, {"Concurrent complete records", ConcurrentRecords}
    };
    int failures = 0;
    for (const auto& [name, run] : tests) {
        Reset();
        try { run(); std::printf("PASS %s\n", name); }
        catch (const std::exception& error) { ++failures; std::printf("FAIL %s: %s\n", name, error.what()); }
    }
    fake.failClose = false; fake.failFlush = false;
    Diagnostics::Shutdown();
    return failures ? 1 : 0;
}
