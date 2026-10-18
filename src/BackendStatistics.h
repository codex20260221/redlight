#pragma once
#include <cstdint>
#include <limits>

struct BackendOperationStatistics {
    std::uint64_t requests = 0, failures = 0;
    unsigned inProgress = 0;
    bool operator==(const BackendOperationStatistics&) const = default;
};
struct BackendStatistics {
    BackendOperationStatistics initialization, activation, restoration, verification, shutdown;
    bool operator==(const BackendStatistics&) const = default;
};
inline void IncrementBackendCounter(std::uint64_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint64_t>::max)()) ++value;
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
    explicit RecordBackendOperation(BackendOperationStatistics& statistics) noexcept : statistics_(statistics) {
        IncrementBackendCounter(statistics_.requests);
        tracked_ = statistics_.inProgress != (std::numeric_limits<unsigned>::max)();
        if (tracked_) ++statistics_.inProgress;
    }
    ~RecordBackendOperation() {
        if (!succeeded_) IncrementBackendCounter(statistics_.failures);
        if (tracked_) --statistics_.inProgress;
    }
    RecordBackendOperation(const RecordBackendOperation&) = delete;
    RecordBackendOperation& operator=(const RecordBackendOperation&) = delete;
    bool success() noexcept { succeeded_ = true; return true; }
private:
    BackendOperationStatistics& statistics_;
    bool succeeded_ = false;
    bool tracked_ = false;
};
