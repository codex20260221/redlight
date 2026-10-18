#pragma once
#include "VerificationCache.h"
#include <span>
#include <string_view>

struct VerificationJsonSnapshot {
    bool gammaBackend = false;
    BackendVerificationReport report;
    std::optional<std::uint64_t> ageSeconds, durationMilliseconds;
};
inline VerificationJsonSnapshot CaptureVerificationJsonSnapshot(const BackendVerificationCache& cache, bool gammaBackend, std::uint64_t now) noexcept {
    return {gammaBackend, cache.result(), cache.ageSeconds(now), cache.durationMilliseconds()};
}
inline constexpr unsigned VerificationJsonProtocolVersion = 1;
inline constexpr std::size_t VerificationJsonBufferSize = 1536;
// Cached data only. Success includes a complete newline-terminated JSON object and a trailing NUL.
std::optional<std::string_view> FormatVerificationJson(const VerificationJsonSnapshot& snapshot, std::span<char> output) noexcept;
