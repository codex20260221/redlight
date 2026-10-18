#pragma once
#include <cstdint>
#include <limits>

struct BackendOperationTiming {
    std::uint64_t completed = 0, lastMilliseconds = 0, maximumMilliseconds = 0, totalMilliseconds = 0;
    bool operator==(const BackendOperationTiming&) const = default;
};
struct BackendOperationStatistics {
    std::uint64_t requests = 0, failures = 0;
    unsigned inProgress = 0;
    BackendOperationTiming timing;
    bool operator==(const BackendOperationStatistics&) const = default;
};
struct BackendStatistics {
    BackendOperationStatistics initialization, activation, restoration, verification, shutdown;
    bool operator==(const BackendStatistics&) const = default;
};
inline void IncrementBackendCounter(std::uint64_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint64_t>::max)()) ++value;
}
inline void AddBackendCounter(std::uint64_t& value, std::uint64_t amount) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    value = amount > maximum - value ? maximum : value + amount;
}
inline bool ResetBackendStatistics(BackendStatistics& statistics) noexcept {
    if (statistics.initialization.inProgress || statistics.activation.inProgress || statistics.restoration.inProgress ||
        statistics.verification.inProgress || statistics.shutdown.inProgress) return false;
    statistics = {};
    return true;
}
// Count method requests (including no-ops) and unsuccessful outcomes, including unwinding.
class RecordBackendOperation {
public:
    using Clock = std::uint64_t (*)() noexcept;
    explicit RecordBackendOperation(BackendOperationStatistics& statistics, Clock clock = nullptr) noexcept
        : statistics_(statistics), clock_(clock), started_(clock ? clock() : 0) {
        IncrementBackendCounter(statistics_.requests);
        tracked_ = statistics_.inProgress != (std::numeric_limits<unsigned>::max)();
        if (tracked_) ++statistics_.inProgress;
    }
    ~RecordBackendOperation() {
        if (!succeeded_) IncrementBackendCounter(statistics_.failures);
        if (clock_) {
            const auto elapsed = clock_() - started_;
            auto& timing = statistics_.timing;
            IncrementBackendCounter(timing.completed);
            timing.lastMilliseconds = elapsed;
            if (elapsed > timing.maximumMilliseconds) timing.maximumMilliseconds = elapsed;
            AddBackendCounter(timing.totalMilliseconds, elapsed);
        }
        if (tracked_) --statistics_.inProgress;
    }
    RecordBackendOperation(const RecordBackendOperation&) = delete;
    RecordBackendOperation& operator=(const RecordBackendOperation&) = delete;
    bool success() noexcept { succeeded_ = true; return true; }
private:
    BackendOperationStatistics& statistics_;
    Clock clock_;
    std::uint64_t started_;
    bool succeeded_ = false;
    bool tracked_ = false;
};
