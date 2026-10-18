#pragma once
#include <cstdint>
#include <optional>

struct DisplayContextSnapshot {
    unsigned monitors, virtualWidth, virtualHeight;
    bool remoteSessionIndicated;
    std::int32_t virtualLeft = 0, virtualTop = 0;
    std::int64_t virtualRight() const noexcept { return static_cast<std::int64_t>(virtualLeft) + virtualWidth; }
    std::int64_t virtualBottom() const noexcept { return static_cast<std::int64_t>(virtualTop) + virtualHeight; }
    bool operator==(const DisplayContextSnapshot&) const = default;
};
enum class DisplayContextReason { Startup, DisplayChange, PowerTransition, SessionTransition, ExplorerRestart, ExplicitVerification, ManualRefresh };
inline const char* DisplayContextReasonName(DisplayContextReason reason) noexcept {
    switch (reason) {
    case DisplayContextReason::Startup: return "Startup";
    case DisplayContextReason::DisplayChange: return "Display change";
    case DisplayContextReason::PowerTransition: return "Power transition";
    case DisplayContextReason::SessionTransition: return "Session transition";
    case DisplayContextReason::ExplorerRestart: return "Explorer restart";
    case DisplayContextReason::ExplicitVerification: return "Explicit verification";
    case DisplayContextReason::ManualRefresh: return "Manual refresh";
    }
    return "Unknown";
}
// Logical Windows metrics only; no output/color/monitor-device inspection.
std::optional<DisplayContextSnapshot> CaptureDisplayContext() noexcept;

class DisplayContextCache {
public:
    void record(std::optional<DisplayContextSnapshot> value, DisplayContextReason reason, std::uint64_t completed) noexcept {
        attempted_ = true;
        lastReason_ = reason;
        lastFailed_ = !value || !value->monitors || !value->virtualWidth || !value->virtualHeight;
        if (lastFailed_) return; // Retain the prior successful data and its original age.
        snapshot_ = value;
        recorded_ = completed;
        captureReason_ = reason;
    }
    const std::optional<DisplayContextSnapshot>& snapshot() const noexcept { return snapshot_; }
    std::optional<std::uint64_t> ageSeconds(std::uint64_t now) const noexcept {
        if (!recorded_) return std::nullopt;
        return (now - *recorded_) / 1000;
    }
    bool attempted() const noexcept { return attempted_; }
    bool lastFailed() const noexcept { return lastFailed_; }
    DisplayContextReason lastReason() const noexcept { return lastReason_; }
    DisplayContextReason captureReason() const noexcept { return captureReason_; }
private:
    std::optional<DisplayContextSnapshot> snapshot_;
    std::optional<std::uint64_t> recorded_;
    bool attempted_ = false, lastFailed_ = false;
    DisplayContextReason lastReason_ = DisplayContextReason::Startup, captureReason_ = DisplayContextReason::Startup;
};

// Refresh logical metrics and timestamp after capture; never inspects a color backend.
bool RefreshDisplayContextCache(DisplayContextCache& cache, DisplayContextReason reason) noexcept;
