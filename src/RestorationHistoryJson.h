#pragma once
#include "RestorationHistory.h"
#include <span>
#include <string_view>

inline constexpr std::size_t RestorationHistoryJsonBufferSize = 4096;
// Completed entries only, newest completion first; now is supplied by the caller.
std::optional<std::string_view> FormatRestorationHistoryJson(const RestorationHistory& history, std::uint64_t now, std::span<char> output) noexcept;
