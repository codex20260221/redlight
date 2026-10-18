#pragma once
#include <string_view>

inline constexpr unsigned BuildMetadataProtocolVersion = 1;

// Describes the invoked binary, never the running tray instance. 0: written, 1: failure.
int WriteBuildInfo(std::string_view version, std::string_view revision, std::string_view sourceState) noexcept;
