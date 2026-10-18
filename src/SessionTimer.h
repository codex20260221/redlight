#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

// Pure monotonic-time policy. The caller owns the Win32 timer and restore calls.
class SessionTimer {
public:
    static constexpr unsigned maxMinutes = 240;
    static constexpr std::uint64_t retryMilliseconds = 5000;

    bool configure(unsigned minutes, std::uint64_t now, bool filterActive) {
        if (minutes > maxMinutes) return false;
        minutes_ = minutes;
        cancel();
        if (filterActive) start(now);
        return true;
    }
    void start(std::uint64_t now) {
        started_ = now;
        armed_ = minutes_ != 0;
        attempted_ = false;
        warningAttempted_ = false;
    }
    void cancel() { armed_ = false; attempted_ = false; warningAttempted_ = false; }
    bool armed() const { return armed_; }
    unsigned minutes() const { return minutes_; }
    std::uint64_t remainingMilliseconds(std::uint64_t now) const {
        if (!armed_) return 0;
        const auto elapsed = now - started_; // unsigned subtraction tolerates clock wrap
        const auto duration = static_cast<std::uint64_t>(minutes_) * 60000;
        return elapsed >= duration ? 0 : duration - elapsed;
    }
    bool canRestart(std::uint64_t now, bool filterActive) const {
        return filterActive && armed_ && remainingMilliseconds(now) != 0;
    }
    bool restart(std::uint64_t now, bool filterActive) {
        if (!canRestart(now, filterActive)) return false;
        start(now);
        return true;
    }
    std::optional<std::uint64_t> restoreRetryMilliseconds(std::uint64_t now) const {
        if (!armed_ || remainingMilliseconds(now) != 0) return std::nullopt;
        if (!attempted_) return 0;
        const auto elapsed = now - lastAttempt_;
        return elapsed >= retryMilliseconds ? 0 : retryMilliseconds - elapsed;
    }
    bool restoreDue(std::uint64_t now) const {
        const auto retry = restoreRetryMilliseconds(now);
        return retry && *retry == 0;
    }
    void noteRestoreAttempt(std::uint64_t now) { lastAttempt_ = now; attempted_ = true; }
    bool restoreAttempted() const { return armed_ && attempted_; }
    bool warningAttempted() const { return armed_ && warningAttempted_; }
    bool warningDue(std::uint64_t now, unsigned leadSeconds) const {
        if (!armed_ || warningAttempted_ || leadSeconds == 0 || leadSeconds > 300) return false;
        const auto remaining = remainingMilliseconds(now);
        return remaining != 0 && remaining <= static_cast<std::uint64_t>(leadSeconds) * 1000;
    }
    void noteWarningAttempt() { warningAttempted_ = true; } // At most one attempt, even if Windows suppresses delivery.

private:
    unsigned minutes_ = 0;
    bool armed_ = false;
    bool attempted_ = false;
    bool warningAttempted_ = false;
    std::uint64_t started_ = 0;
    std::uint64_t lastAttempt_ = 0;
};

enum class NativeCountdownTimerState { NotArmed, Armed, CleanupPending, Unavailable };
inline NativeCountdownTimerState CountdownTimerState(bool armed, bool installed) noexcept {
    if (armed) return installed ? NativeCountdownTimerState::Armed : NativeCountdownTimerState::Unavailable;
    return installed ? NativeCountdownTimerState::CleanupPending : NativeCountdownTimerState::NotArmed;
}
inline const char* NativeCountdownTimerStateName(NativeCountdownTimerState state) noexcept {
    switch (state) {
    case NativeCountdownTimerState::NotArmed: return "Not armed";
    case NativeCountdownTimerState::Armed: return "Armed (native registration confirmed)";
    case NativeCountdownTimerState::CleanupPending: return "Cleanup pending (native removal unconfirmed)";
    case NativeCountdownTimerState::Unavailable: return "Armed without confirmed native registration";
    }
    return "Unknown";
}

inline std::optional<unsigned> ParseSessionMinutes(std::wstring_view text) {
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t')) text.remove_suffix(1);
    if (text.empty()) return std::nullopt;
    unsigned value = 0;
    for (const wchar_t digit : text) {
        if (digit < L'0' || digit > L'9') return std::nullopt;
        value = value * 10 + static_cast<unsigned>(digit - L'0');
        if (value > SessionTimer::maxMinutes) return std::nullopt;
    }
    return value;
}
