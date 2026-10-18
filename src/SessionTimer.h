#pragma once
#include <cstdint>

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
