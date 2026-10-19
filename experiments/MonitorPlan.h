#pragma once
#include <cstdint>
#include <limits>
#include <span>

// Geometry research only. Nothing here enumerates monitors, calls Win32, or
// establishes physical monitor/input/cursor/color isolation.
namespace MonitorPrototype {
struct Rect {
    std::int32_t left = 0, top = 0, right = 0, bottom = 0;
    bool operator==(const Rect&) const = default;
};
struct Region {
    std::uint64_t key = 0; // Transient key within this supplied snapshot, not a durable output identity.
    Rect bounds;
    bool selected = false;
    bool operator==(const Region&) const = default;
};
struct Slot {
    std::uint64_t key = 0, revision = 0;
    Rect source, destination;
    std::int32_t width = 0, height = 0;
    bool operator==(const Slot&) const = default;
};
struct Context {
    std::uint64_t snapshotRevision = 0, selectionRevision = 0;
    bool backendClear = false, pixelCoordinatesSupplied = false, independentSourcesSupplied = false;
};
enum class Result { Subset, AllMonitors, NoSelection, RecoveryOwned, StaleSnapshot, InvalidSnapshot,
    UnknownCoordinates, UnknownSources, OverlappingRegions, BufferTooSmall };
struct PlanResult { Result result; std::size_t count = 0; };

inline PlanResult Preflight(std::span<const Region> regions, const Context& context, std::span<Slot> output) noexcept {
    if (!context.backendClear) return {Result::RecoveryOwned};
    if (!context.snapshotRevision || context.snapshotRevision != context.selectionRevision) return {Result::StaleSnapshot};
    if (regions.empty()) return {Result::InvalidSnapshot};
    std::size_t selected = 0;
    const auto width = [](const Rect& value) { return static_cast<std::int64_t>(value.right) - value.left; };
    const auto height = [](const Rect& value) { return static_cast<std::int64_t>(value.bottom) - value.top; };
    for (std::size_t index = 0; index < regions.size(); ++index) {
        const auto& region = regions[index];
        if (!region.key || width(region.bounds) <= 0 || height(region.bounds) <= 0 ||
            width(region.bounds) > (std::numeric_limits<std::int32_t>::max)() || height(region.bounds) > (std::numeric_limits<std::int32_t>::max)())
            return {Result::InvalidSnapshot};
        for (std::size_t prior = 0; prior < index; ++prior) if (regions[prior].key == region.key) return {Result::InvalidSnapshot};
        selected += region.selected;
    }
    if (!selected) return {Result::NoSelection};
    // This delegates no operation: an all-monitor request belongs to the existing
    // global path, not to a set of experimental hosts. Publish no control slots.
    if (selected == regions.size()) return {Result::AllMonitors};
    if (!context.pixelCoordinatesSupplied) return {Result::UnknownCoordinates};
    if (!context.independentSourcesSupplied) return {Result::UnknownSources};
    for (std::size_t index = 0; index < regions.size(); ++index) for (std::size_t prior = 0; prior < index; ++prior) {
        const auto& a = regions[index].bounds;
        const auto& b = regions[prior].bounds;
        if (a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom) return {Result::OverlappingRegions};
    }
    if (output.size() < selected) return {Result::BufferTooSmall};
    // All validation precedes publication; failure leaves caller storage intact.
    std::size_t used = 0;
    for (const auto& region : regions) if (region.selected)
        output[used++] = {region.key, context.snapshotRevision, region.bounds, region.bounds,
            static_cast<std::int32_t>(width(region.bounds)), static_cast<std::int32_t>(height(region.bounds))};
    return {Result::Subset, used};
}
} // namespace MonitorPrototype
