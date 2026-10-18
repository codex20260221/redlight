#pragma once
#include <windows.h>
#include <cstdint>

// Tick resolution is intentionally sufficient for diagnostics; no timer is created.
inline std::uint64_t BackendMonotonicMilliseconds() noexcept { return GetTickCount64(); }
