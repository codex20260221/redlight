#pragma once
#include <cstdint>
#include <string>

namespace Diagnostics {
enum class FileState { NotInitialized, Available, Degraded, Unavailable };
struct Status {
    FileState file = FileState::NotInitialized;
    std::uint64_t droppedLines = 0;
    std::uint64_t truncatedLines = 0;
    std::uint32_t lastFileError = 0;
};
inline const char* FileStateName(FileState state) {
    switch (state) {
    case FileState::NotInitialized: return "Not initialized";
    case FileState::Available: return "Available";
    case FileState::Degraded: return "Degraded (some records were not written)";
    case FileState::Unavailable: return "Unavailable (debugger output only)";
    }
    return "Unknown";
}
std::wstring LogDirectory();
Status GetStatus() noexcept;
bool RetryFileLogging() noexcept;
void Initialize() noexcept;
void Log(const char* message) noexcept;
void LogFormat(const char* format, ...) noexcept;
void Shutdown() noexcept;
}  // namespace Diagnostics
