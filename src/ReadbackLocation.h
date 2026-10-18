#pragma once
#include "DisplayFilter.h"
#include <cstdio>
#include <span>
#include <string_view>

inline constexpr std::size_t ReadbackLocationBufferSize = 96;
inline std::optional<std::string_view> FormatReadbackLocation(std::optional<BackendValueLocation> location, bool gammaRamp, std::span<char> output) noexcept {
    if (output.empty()) return std::nullopt;
    if (location && (gammaRamp ? location->row >= 3 || location->column >= 256 : location->row >= 5 || location->column >= 5)) return std::nullopt;
    int length = 0;
    if (!location) length = std::snprintf(output.data(), output.size(), "none recorded");
    else if (gammaRamp) {
        constexpr const char* channels[] = {"red", "green", "blue"};
        length = std::snprintf(output.data(), output.size(), "gamma %s channel (%u), entry %u (zero-based)", channels[location->row], location->row, location->column);
    } else length = std::snprintf(output.data(), output.size(), "matrix row %u, column %u (zero-based)", location->row, location->column);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
