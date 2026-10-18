#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <string_view>

inline constexpr wchar_t RedLightRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t RedLightRunValue[] = L"RedLight";
inline constexpr std::size_t StartupCommandMaxCharacters = 260;
enum class StartupEntryState { Unavailable, Off, ThisExecutable, DifferentEntry };
inline const char* StartupEntryDescription(StartupEntryState state) noexcept {
    switch (state) {
    case StartupEntryState::Off: return "Not configured";
    case StartupEntryState::ThisExecutable: return "Configured for this executable (Windows may override or delay)";
    case StartupEntryState::DifferentEntry: return "Different startup entry; review before replacing/removing";
    default: return "Unavailable; refresh before changing";
    }
}
struct StartupEntrySnapshot {
    StartupEntryState state = StartupEntryState::Unavailable;
    std::array<wchar_t, StartupCommandMaxCharacters + 1> command{};
    std::array<wchar_t, StartupCommandMaxCharacters + 1> proposedCommand{}; // The user reviews both existing and proposed values.
    std::uint32_t error = 0;
    bool operator==(const StartupEntrySnapshot&) const = default;
    bool readable() const noexcept { return state == StartupEntryState::Off || state == StartupEntryState::ThisExecutable || state == StartupEntryState::DifferentEntry; }
};
enum class StartupChangeResult { NotConfirmed, Unavailable, ChangedSinceReview, NativeFailed, VerificationFailed, Confirmed };

class StartupRegistration {
public:
    bool initialize() noexcept; // Captures own portable path and reads only; never registers automatically.
    void refresh() noexcept;
    const StartupEntrySnapshot& snapshot() const noexcept { return snapshot_; }
    std::wstring_view command() const noexcept { return command_.data(); }
    DWORD pathError() const noexcept { return pathError_; }
    std::uint32_t lastChangeError() const noexcept { return lastChangeError_; }
    // A fresh read must match exactly what the user reviewed; explicit consent is mandatory.
    StartupChangeResult change(bool enable, StartupEntrySnapshot reviewed, bool userConfirmed) noexcept;
private:
    std::array<wchar_t, StartupCommandMaxCharacters + 1> command_{};
    DWORD pathError_ = ERROR_SUCCESS;
    std::uint32_t lastChangeError_ = 0;
    StartupEntrySnapshot snapshot_;
};
