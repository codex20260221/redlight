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
    }
    void cancel() { armed_ = false; attempted_ = false; }
    bool armed() const { return armed_; }
    unsigned minutes() const { return minutes_; }
    std::uint64_t remainingMilliseconds(std::uint64_t now) const {
        if (!armed_) return 0;
        const auto elapsed = now - started_; // unsigned subtraction tolerates clock wrap
        const auto duration = static_cast<std::uint64_t>(minutes_) * 60000;
        return elapsed >= duration ? 0 : duration - elapsed;
    }
    bool restoreDue(std::uint64_t now) const {
        return armed_ && remainingMilliseconds(now) == 0 &&
            (!attempted_ || now - lastAttempt_ >= retryMilliseconds);
    }
    void noteRestoreAttempt(std::uint64_t now) { lastAttempt_ = now; attempted_ = true; }

private:
    unsigned minutes_ = 0;
    bool armed_ = false;
    bool attempted_ = false;
    std::uint64_t started_ = 0;
    std::uint64_t lastAttempt_ = 0;
};

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
