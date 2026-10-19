#include "RestorationHistoryJson.h"
#include <cstdio>
#include <iterator>

namespace {
constexpr const char* reasons[] = {"manual", "countdown", "timer-failure", "display-change", "power-transition", "session-transition",
    "explorer-restart", "tray-failure", "external-request", "preferences", "exit", "callback-failure", "deferred-request", "daily-schedule"};
constexpr const char* outcomes[] = {"interrupted", "deferred", "already-clear", "restored", "failed"};
static_assert(std::size(reasons) == static_cast<unsigned>(RestoreReason::DailySchedule) + 1);
static_assert(std::size(outcomes) == static_cast<unsigned>(RestoreOutcome::Failed) + 1);
}

std::optional<std::string_view> FormatRestorationHistoryJson(const RestorationHistory& history, std::uint64_t now, std::span<char> output) noexcept {
    if (output.empty()) return std::nullopt;
    std::size_t length = 0;
    const auto append = [&](const char* format, auto... args) {
        const int written = std::snprintf(output.data() + length, output.size() - length, format, args...);
        if (written < 0 || static_cast<std::size_t>(written) >= output.size() - length) return false;
        length += static_cast<std::size_t>(written);
        return true;
    };
    if (!append("{\"protocol\":%u,\"status\":\"restoration-history\",\"order\":\"newest-completed-first\",\"retained\":%zu,\"capacity\":%zu,"
        "\"overwritten\":%llu,\"requests_in_progress\":%u,\"events\":[", RestorationHistoryJsonProtocolVersion, history.size(), history.capacity,
        static_cast<unsigned long long>(history.overwritten()), history.inProgress())) return std::nullopt;
    for (std::size_t index = 0; index < history.size(); ++index) {
        const auto* event = history.newest(index);
        if (!event || static_cast<unsigned>(event->reason) >= std::size(reasons) || static_cast<unsigned>(event->outcome) >= std::size(outcomes)) return std::nullopt;
        char duration[32] = "null";
        if (event->durationMilliseconds) {
            const int written = std::snprintf(duration, sizeof(duration), "%llu", static_cast<unsigned long long>(*event->durationMilliseconds));
            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(duration)) return std::nullopt;
        }
        if (!append("%s{\"reason\":\"%s\",\"outcome\":\"%s\",\"age_seconds\":%llu,\"duration_ms\":%s}", index ? "," : "",
            reasons[static_cast<unsigned>(event->reason)], outcomes[static_cast<unsigned>(event->outcome)],
            static_cast<unsigned long long>(RestoreRequestAgeSeconds(*event, now)), duration)) return std::nullopt;
    }
    if (!append("]}\n")) return std::nullopt;
    return std::string_view(output.data(), length);
}
