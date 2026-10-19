#pragma once
#include "MonitorPlan.h"
#include <array>
#include <optional>

namespace MonitorPrototype {
inline constexpr unsigned MaxTopologyOutputs = 8;
struct DisplayEndpoint {
    std::uint32_t adapterLow = 0;
    std::int32_t adapterHigh = 0;
    std::uint32_t id = 0;
    bool operator==(const DisplayEndpoint&) const = default;
};
struct TopologyOutput {
    Region region; // Transient canonical key; never a persisted physical identity.
    DisplayEndpoint source, target;
    std::array<wchar_t, 32> gdiName{};
    std::array<wchar_t, 128> targetPath{};
    unsigned rotation = 0;
    bool primary = false;
    bool operator==(const TopologyOutput&) const = default;
};
struct WindowedTopology {
    std::array<TopologyOutput, MaxTopologyOutputs> outputs{};
    unsigned count = 0;
    bool operator==(const WindowedTopology&) const = default;
};
enum class TopologyStage {
    None, DpiContext, BufferSizes, Capacity, Query, InvalidData, SharedSource,
    SourceName, TargetName, Enumeration, MonitorInfo, MonitorMapping, Changed, TooSlow
};
struct TopologyReadResult {
    std::optional<WindowedTopology> snapshot;
    TopologyStage stage = TopologyStage::None;
    std::uint32_t nativeError = 0; // Only direct CCD return statuses; no undocumented last-error interpretation.
    std::uint64_t captureMilliseconds = 0; // Valid only with a snapshot.
};
// Read-only research. Requires PMv2, two matching complete snapshots, and bounded
// accepted capture latency. Native reads are synchronous, not interrupted here.
// No DPI changes, persistence, windows, display writes or owner
// acquisition. The future probe must revalidate before showing any control.
TopologyReadResult ReadWindowedTopology() noexcept;
}
