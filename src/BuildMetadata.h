#pragma once
#include <string_view>
#include <cstddef>

inline constexpr unsigned BuildMetadataProtocolVersion = 1;
inline constexpr std::size_t BuildMetadataJsonBufferSize = 1536;

// Describes the invoked binary, never the running tray instance. 0: written, 1: failure.
int WriteBuildInfo(std::string_view version, std::string_view revision, std::string_view sourceState) noexcept;
