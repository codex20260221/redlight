// Production Diagnostics.cpp linked exclusively to in-memory Win32 file API fakes.
#include "Diagnostics.h"
#include "MemoryFault.h"
#include "RecentDiagnostics.h"
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
    bool snapshotDuringWrite = false, clearDuringWrite = false;
    bool snapshotDuringWriteSucceeded = false, clearDuringWriteSucceeded = false;
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
void RecentLogRetainsUnavailableRecords() {
    fake.failOpen = true;
    Diagnostics::Initialize();
    Diagnostics::Log("record retained despite unavailable file");
    const auto status = Diagnostics::GetStatus();
    const auto debug = fake.lastDebug;
    const auto writes = fake.writes;
    const auto text = Diagnostics::RecentLogText();
    Check(text && text->find("diagnostics open failed") != std::string::npos &&
        text->find("record retained despite unavailable file") != std::string::npos &&
        text->find("Retained records: 2/32") != std::string::npos, "file-open warning and skipped file record remain shareable in memory");
    const auto after = Diagnostics::GetStatus();
    Check(after.file == status.file && after.droppedLines == status.droppedLines && after.lastFileError == status.lastFileError &&
        fake.writes == writes && fake.lastDebug == debug && fake.files.empty(), "snapshot reading performs no logging/file retry or health mutation");
    fake.failOpen = false;
    Check(Diagnostics::RetryFileLogging() && Diagnostics::RecentLogText()->find("record retained despite unavailable file") != std::string::npos,
        "logging retry preserves earlier retained failure records");
}
void RecentLogLifecycleAndOrder() {
    Diagnostics::Initialize();
    for (unsigned record = 0; record < 40; ++record) Diagnostics::LogFormat("retained-%02u", record);
    auto text = Diagnostics::RecentLogText();
    Check(text && text->find("Retained records: 32/32") != std::string::npos && text->find("Earlier records overwritten: 8") != std::string::npos &&
        text->find("retained-07") == std::string::npos && text->find("retained-08") < text->find("retained-09") &&
        text->ends_with("retained-39\r\n"), "production recording keeps the last 32 complete records in chronological order");
    Diagnostics::Initialize();
    Check(Diagnostics::RecentLogText() == text, "idempotent initialization does not erase live diagnostics");
    Diagnostics::Shutdown();
    Check(Diagnostics::RecentLogText() == text, "successful shutdown leaves memory history available");
    Diagnostics::Initialize();
    text = Diagnostics::RecentLogText();
    Check(text && text->find("Retained records: 0/32") != std::string::npos && text->find("retained-39") == std::string::npos,
        "new logging lifecycle resets visible records and overwrite count");
}
void RecentLogTruncationAndUnicode() {
    Diagnostics::Initialize();
    Diagnostics::Log("Unicode retained: \xe2\x82\xac");
    const std::string longUnicode = std::string(1899, 'x') + "\xe2\x82\xac";
    Diagnostics::Log(longUnicode.c_str());
    const auto text = Diagnostics::RecentLogText();
    Check(text && text->size() < RecentDiagnosticsBuffer::maximumTextBytes && IsValidUtf8(*text) &&
        text->find("Unicode retained: \xe2\x82\xac") != std::string::npos && text->find("\\xE2 [truncated]") != std::string::npos &&
        Diagnostics::GetStatus().truncatedLines == 1, "complete formatted records preserve valid Unicode and escape a truncated multibyte tail without losing the truncation marker");
}
void RecentLogPreparationFailure() {
    Diagnostics::Initialize();
    Diagnostics::Log("record survives snapshot allocation failure");
    const auto debug = fake.lastDebug;
    const auto writes = fake.writes;
    bool failed = false, consumed = false;
    {
        TestMemory::FailNextAllocation fail;
        failed = !Diagnostics::RecentLogText();
        consumed = !TestMemory::AllocationFailurePending();
    }
    Check(failed && consumed && fake.writes == writes && fake.lastDebug == debug &&
        Diagnostics::RecentLogText()->find("record survives snapshot allocation failure") != std::string::npos,
        "snapshot allocation failure is contained without logging, file I/O, history loss, or held-lock leakage");
}
void RecentLogConcurrentSnapshots() {
    Diagnostics::Initialize();
    std::vector<std::thread> workers;
    for (unsigned worker = 0; worker < 2; ++worker) workers.emplace_back([worker] {
        for (unsigned record = 0; record < 50; ++record) Diagnostics::LogFormat("snapshot-worker=%u record=%u", worker, record);
    });
    bool valid = true;
    for (unsigned sample = 0; sample < 50; ++sample) {
        const auto text = Diagnostics::RecentLogText();
        valid = text && text->starts_with("RedLight recent diagnostics\r\n") && IsValidUtf8(*text) && valid;
    }
    for (auto& worker : workers) worker.join();
    const auto text = Diagnostics::RecentLogText();
    std::size_t count = 0, position = 0;
    if (text) while ((position = text->find("snapshot-worker=", position)) != std::string::npos) { ++count; ++position; }
    Check(valid && text && count == 32 && text->find("Earlier records overwritten: 68") != std::string::npos &&
        Diagnostics::GetStatus().droppedLines == 0, "concurrent snapshots and writers remain complete, bounded, and synchronized");
}
void RecentLogClearPreservesFileAndHealth() {
    Diagnostics::Initialize();
    for (unsigned record = 0; record < 40; ++record) Diagnostics::LogFormat("before-clear-%u", record);
    const auto bytes = fake.files.at(Path())->bytes;
    const auto status = Diagnostics::GetStatus();
    const auto writes = fake.writes;
    const auto debug = fake.lastDebug;
    bool unused = false;
    {
        TestMemory::FailNextAllocation fail;
        Diagnostics::ClearRecentLog();
        unused = TestMemory::AllocationFailurePending();
    }
    const auto text = Diagnostics::RecentLogText();
    const auto after = Diagnostics::GetStatus();
    Check(unused && text && text->find("Retained records: 0/32") != std::string::npos &&
        text->find("Earlier records overwritten: 0") != std::string::npos && text->find("before-clear-") == std::string::npos &&
        fake.files.at(Path())->bytes == bytes && fake.writes == writes && fake.lastDebug == debug &&
        after.file == status.file && after.droppedLines == status.droppedLines && after.truncatedLines == status.truncatedLines && after.lastFileError == status.lastFileError,
        "clearing visible memory history allocates nothing and leaves file bytes, debugger output, and file-health history unchanged");
    Diagnostics::Log("after-clear");
    const auto fresh = Diagnostics::RecentLogText();
    Check(fresh && fresh->find("Retained records: 1/32") != std::string::npos && fresh->find("Earlier records overwritten: 0") != std::string::npos &&
        fresh->find("after-clear") != std::string::npos && fresh->find("before-clear-") == std::string::npos && fake.writes == writes + 1,
        "later records begin a fresh memory history while ordinary file logging continues");
}
void RecentLogClearPreservesUnavailableStorage() {
    fake.failOpen = true;
    Diagnostics::Initialize(); Diagnostics::Log("old unavailable record");
    const auto before = Diagnostics::GetStatus();
    Diagnostics::ClearRecentLog();
    const auto after = Diagnostics::GetStatus();
    Check(after.file == Diagnostics::FileState::Unavailable && after.file == before.file &&
        after.droppedLines == before.droppedLines && after.lastFileError == before.lastFileError && fake.files.empty() && fake.writes == 0 &&
        Diagnostics::RecentLogText()->find("old unavailable record") == std::string::npos,
        "memory clear neither hides disk failures nor attempts file creation/recovery");
    fake.failOpen = false;
    Check(Diagnostics::RetryFileLogging() && Diagnostics::GetStatus().droppedLines == before.droppedLines &&
        Diagnostics::RecentLogText()->find("old unavailable record") == std::string::npos &&
        Diagnostics::RecentLogText()->find("File logging retry requested") != std::string::npos,
        "later file recovery preserves health history and cannot reintroduce cleared records");
}
void RecentLogConcurrentClearAndSnapshots() {
    Diagnostics::Initialize();
    std::vector<std::thread> workers;
    for (unsigned worker = 0; worker < 2; ++worker) workers.emplace_back([worker] {
        for (unsigned record = 0; record < 50; ++record) Diagnostics::LogFormat("clear-worker=%u record=%u", worker, record);
    });
    bool valid = true;
    for (unsigned sample = 0; sample < 50; ++sample) {
        Diagnostics::ClearRecentLog();
        const auto text = Diagnostics::RecentLogText();
        valid = text && text->starts_with("RedLight recent diagnostics\r\n") && IsValidUtf8(*text) && valid;
    }
    for (auto& worker : workers) worker.join();
    const auto& bytes = fake.files.at(Path())->bytes;
    Diagnostics::ClearRecentLog();
    const auto final = Diagnostics::RecentLogText();
    Check(valid && final && final->find("Retained records: 0/32") != std::string::npos &&
        final->find("Earlier records overwritten: 0") != std::string::npos && std::count(bytes.begin(), bytes.end(), '\n') == 100 &&
        Diagnostics::GetStatus().droppedLines == 0, "concurrent clear/snapshot/write operations remain synchronized and never remove file records");
}
void RecentLogSnapshotWhileFileWriteOwnsLock() {
    Diagnostics::Initialize();
    fake.snapshotDuringWrite = true;
    Diagnostics::Log("snapshot inside pending file write");
    Check(fake.snapshotDuringWriteSucceeded && fake.writes == 1 && Diagnostics::GetStatus().file == Diagnostics::FileState::Available &&
        Diagnostics::RecentLogText()->find("snapshot inside pending file write") != std::string::npos,
        "memory snapshot succeeds from a fake I/O hook while production file serialization is still locked");
}
void RecentLogClearWhileFileWriteOwnsLock() {
    Diagnostics::Initialize();
    Diagnostics::Log("record before the I/O hook");
    fake.snapshotDuringWrite = true; fake.clearDuringWrite = true;
    Diagnostics::Log("snapshot inside pending file write");
    const auto text = Diagnostics::RecentLogText();
    const auto& bytes = fake.files.at(Path())->bytes;
    Check(fake.snapshotDuringWriteSucceeded && fake.clearDuringWriteSucceeded && text &&
        text->find("Retained records: 0/32") != std::string::npos && text->find("Earlier records overwritten: 0") != std::string::npos &&
        fake.writes == 2 && std::count(bytes.begin(), bytes.end(), '\n') == 2 && Diagnostics::GetStatus().droppedLines == 0,
        "memory clear/snapshot finish while file I/O owns its lock, without losing either serialized file record");
    fake.snapshotDuringWrite = false; fake.clearDuringWrite = false;
    fake.failWrite = true;
    Diagnostics::Log("later file failure remains visible");
    Check(Diagnostics::RecentLogText()->find("diagnostics write failed") != std::string::npos && Diagnostics::GetStatus().droppedLines == 1,
        "file-error paths can safely record fresh memory warnings with the separate lock");
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
    if (fake.snapshotDuringWrite) {
        const auto text = Diagnostics::RecentLogText();
        fake.snapshotDuringWriteSucceeded = text && text->find("snapshot inside pending file write") != std::string::npos;
    }
    if (fake.clearDuringWrite) {
        Diagnostics::ClearRecentLog();
        const auto text = Diagnostics::RecentLogText();
        fake.clearDuringWriteSucceeded = text && text->find("Retained records: 0/32") != std::string::npos;
    }
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
        {"File logging retry", RetryFileLogging}, {"Concurrent complete records", ConcurrentRecords},
        {"Recent logs with unavailable storage", RecentLogRetainsUnavailableRecords}, {"Recent log lifecycle/order", RecentLogLifecycleAndOrder},
        {"Recent log truncation/Unicode", RecentLogTruncationAndUnicode}, {"Recent log allocation failure", RecentLogPreparationFailure},
        {"Recent log concurrent snapshots", RecentLogConcurrentSnapshots},
        {"Recent log clear preserves file/health", RecentLogClearPreservesFileAndHealth}, {"Recent log clear with unavailable storage", RecentLogClearPreservesUnavailableStorage},
        {"Recent log concurrent clear/snapshots", RecentLogConcurrentClearAndSnapshots},
        {"Recent log snapshot during locked file I/O", RecentLogSnapshotWhileFileWriteOwnsLock},
        {"Recent log clear during locked file I/O", RecentLogClearWhileFileWriteOwnsLock}
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
