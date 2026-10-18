#pragma once
#include <cstdint>
#include <optional>

struct ProcessContextSnapshot {
    std::uint32_t processId = 0;
    std::optional<std::uint32_t> sessionId;
    std::uint32_t sessionError = 0;
    bool operator==(const ProcessContextSnapshot&) const = default;
};
// Self only; checked session lookup, no process handles or display APIs.
ProcessContextSnapshot CaptureProcessContext() noexcept;
