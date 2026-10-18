#pragma once
#include "FadeInDuration.h"
#include "RedBrightness.h"
#include <cstdint>
#include <optional>

// Elapsed-time policy only. The caller first confirms red-only 1% activation,
// and records a later level only after a checked backend write succeeds.
class RedFadeIn {
public:
    bool start(unsigned target, FadeInDuration duration, std::uint64_t now) noexcept {
        if (!ValidRedBrightness(target) || !ValidFadeInDuration(duration) || target == 1 || duration == FadeInDuration::Off) return false;
        target_ = target; duration_ = duration; started_ = now; confirmed_ = 1; active_ = true;
        return true;
    }
    void cancel() noexcept { active_ = false; }
    bool active() const noexcept { return active_; }
    unsigned target() const noexcept { return target_; }
    unsigned confirmedPercent() const noexcept { return confirmed_; }
    FadeInDuration duration() const noexcept { return duration_; }
    std::optional<unsigned> due(std::uint64_t now) const noexcept {
        if (!active_) return std::nullopt;
        const auto elapsed = now - started_; // Monotonic unsigned subtraction tolerates wrap.
        const auto total = static_cast<std::uint64_t>(duration_) * 1000;
        // Multiplication is bounded because elapsed < total <= 60000 here.
        const auto wanted = elapsed >= total ? target_ : 1u + static_cast<unsigned>(elapsed * (target_ - 1) / total);
        return wanted > confirmed_ ? std::optional<unsigned>(wanted) : std::nullopt;
    }
    bool confirm(unsigned percent) noexcept {
        if (!active_ || percent < confirmed_ || percent > target_) return false;
        confirmed_ = percent;
        if (percent == target_) active_ = false;
        return true;
    }
private:
    bool active_ = false;
    unsigned target_ = 100, confirmed_ = 1;
    FadeInDuration duration_ = FadeInDuration::Off;
    std::uint64_t started_ = 0;
};
