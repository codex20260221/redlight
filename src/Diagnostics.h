#pragma once
#include <cstdint>
#include <string>
#include <optional>

namespace Diagnostics {
enum class FileState { NotInitialized, Available, Degraded, Unavailable };
struct Status {
    FileState file = FileState::NotInitialized;
    std::uint64_t droppedLines = 0;
    std::uint64_t truncatedLines = 0;
    std::uint32_t lastFileError = 0;
};
struct RecentLogStatus {
    unsigned retainedRecords = 0, capacity = 0;
    std::uint64_t overwrittenRecords = 0;
    bool operator==(const RecentLogStatus&) const = default;
};
inline const char* FileStateName(FileState state) {
    switch (state) {
    case FileState::NotInitialized: return "Not initialized";
    case FileState::Available: return "Available";
    case FileState::Degraded: return "Degraded (some records were not written)";
    case FileState::Unavailable: return "Unavailable (memory and debugger output)";
    }
    return "Unknown";
}
std::wstring LogDirectory();
Status GetStatus() noexcept;
RecentLogStatus GetRecentLogStatus() noexcept;
std::optional<std::string> RecentLogText() noexcept;
void ClearRecentLog() noexcept;
bool RetryFileLogging() noexcept;
void Initialize() noexcept;
void Log(const char* message) noexcept;
void LogFormat(const char* format, ...) noexcept;
void Shutdown() noexcept;
}  // namespace Diagnostics
