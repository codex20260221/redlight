#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

enum class RestoreReason {
    Manual, Countdown, TimerFailure, DisplayChange, PowerTransition, SessionTransition,
    ExplorerRestart, TrayFailure, ExternalRequest, Preferences, Exit, CallbackFailure, DeferredRequest
};
enum class RestoreOutcome { Interrupted, Deferred, AlreadyClear, Restored, Failed };
inline const char* RestoreReasonName(RestoreReason reason) noexcept {
    switch (reason) {
    case RestoreReason::Manual: return "Manual control";
    case RestoreReason::Countdown: return "Countdown expiry/retry";
    case RestoreReason::TimerFailure: return "Countdown timer failure";
    case RestoreReason::DisplayChange: return "Display change";
    case RestoreReason::PowerTransition: return "Power transition";
    case RestoreReason::SessionTransition: return "Session transition";
    case RestoreReason::ExplorerRestart: return "Explorer restart";
    case RestoreReason::TrayFailure: return "Tray update failure";
    case RestoreReason::ExternalRequest: return "External restore request";
    case RestoreReason::Preferences: return "Confirmed preference import/reset";
    case RestoreReason::Exit: return "Exit/session ending";
    case RestoreReason::CallbackFailure: return "Callback failure recovery";
    case RestoreReason::DeferredRequest: return "Deferred restore replay";
    }
    return "Unknown";
}
inline const char* RestoreOutcomeName(RestoreOutcome outcome) noexcept {
    switch (outcome) {
    case RestoreOutcome::Interrupted: return "Interrupted before a result";
    case RestoreOutcome::Deferred: return "Deferred while busy";
    case RestoreOutcome::AlreadyClear: return "No owned restoration needed";
    case RestoreOutcome::Restored: return "Backend restoration succeeded";
    case RestoreOutcome::Failed: return "Backend restoration failed";
    }
    return "Unknown";
}
struct RestoreEvent {
    RestoreReason reason;
    RestoreOutcome outcome;
    std::uint64_t requestedAt;
};
inline std::uint64_t RestoreRequestAgeSeconds(const RestoreEvent& event, std::uint64_t now) noexcept {
    return (now - event.requestedAt) / 1000;
}
class RestorationHistory {
public:
    static constexpr std::size_t capacity = 8;
    void record(RestoreEvent event) noexcept {
        events_[next_] = event;
        next_ = (next_ + 1) % capacity;
        if (count_ < capacity) ++count_;
        else if (overwritten_ != (std::numeric_limits<std::uint64_t>::max)()) ++overwritten_;
    }
    std::size_t size() const noexcept { return count_; }
    std::uint64_t overwritten() const noexcept { return overwritten_; }
    const RestoreEvent* newest(std::size_t index) const noexcept {
        if (index >= count_) return nullptr;
        return &events_[(next_ + capacity - 1 - index) % capacity];
    }
private:
    std::array<RestoreEvent, capacity> events_{};
    std::size_t next_ = 0, count_ = 0;
    std::uint64_t overwritten_ = 0;
};

// Observation only: never invokes the backend or changes recovery decisions.
class RecordRestorationRequest {
public:
    RecordRestorationRequest(RestorationHistory& history, RestoreReason reason, std::uint64_t now) noexcept
        : history_(history), event_{reason, RestoreOutcome::Interrupted, now} {}
    ~RecordRestorationRequest() { history_.record(event_); }
    RecordRestorationRequest(const RecordRestorationRequest&) = delete;
    RecordRestorationRequest& operator=(const RecordRestorationRequest&) = delete;
    void finish(RestoreOutcome outcome) noexcept { event_.outcome = outcome; }
private:
    RestorationHistory& history_;
    RestoreEvent event_;
};
