#include <windows.h>
#include <wingdi.h>
#include "WindowedTopology.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <tuple>

namespace MonitorPrototype {
namespace {
constexpr UINT32 QueryFlags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
constexpr unsigned MaxModes = MaxTopologyOutputs * 4;
TopologyReadResult Failure(TopologyStage stage, LONG error = ERROR_SUCCESS) noexcept {
    return {std::nullopt, stage, static_cast<std::uint32_t>(error)};
}
DisplayEndpoint Endpoint(const LUID& adapter, UINT32 id) noexcept { return {adapter.LowPart, adapter.HighPart, id}; }
template<class T> std::uint32_t Bits(const T& value) noexcept {
    static_assert(sizeof(T) == sizeof(std::uint32_t));
    std::uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result;
}
template<std::size_t N> bool Name(const wchar_t (&native)[N], std::array<wchar_t, N>& output) noexcept {
    const auto end = std::find(std::begin(native), std::end(native), L'\0');
    if (end == std::begin(native) || end == std::end(native)) return false;
    for (auto at = std::begin(native); at != end; ++at) if (*at < 32) return false;
    output.fill(0); std::copy(std::begin(native), end, output.begin()); return true;
}
bool Same(const RECT& a, const RECT& b) noexcept { return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom; }
bool DpiReady() noexcept {
    const auto context = GetThreadDpiAwarenessContext();
    return context && AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}
struct Monitor {
    HMONITOR handle = nullptr;
    RECT bounds{};
    std::array<wchar_t, 32> name{};
    bool primary = false;
};
struct Monitors {
    std::array<Monitor, MaxTopologyOutputs> entries{};
    unsigned count = 0;
    TopologyStage failure = TopologyStage::None;
};
BOOL CALLBACK Collect(HMONITOR handle, HDC dc, RECT* bounds, LPARAM data) noexcept {
    auto& result = *reinterpret_cast<Monitors*>(data);
    if (result.count == result.entries.size()) { result.failure = TopologyStage::Capacity; return FALSE; }
    if (!handle || dc || !bounds) { result.failure = TopologyStage::InvalidData; return FALSE; }
    MONITORINFOEXW info = {}; info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(handle, static_cast<MONITORINFO*>(&info))) { result.failure = TopologyStage::MonitorInfo; return FALSE; }
    Monitor candidate; candidate.handle = handle; candidate.bounds = info.rcMonitor; candidate.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    if (info.cbSize != sizeof(info) || !Same(*bounds, info.rcMonitor) || !Name(info.szDevice, candidate.name)) {
        result.failure = TopologyStage::InvalidData; return FALSE;
    }
    for (unsigned index = 0; index < result.count; ++index) if (result.entries[index].handle == handle || result.entries[index].name == candidate.name) {
        result.failure = TopologyStage::MonitorMapping; return FALSE;
    }
    result.entries[result.count++] = candidate;
    return TRUE;
}
TopologyReadResult Capture() noexcept {
    std::array<DISPLAYCONFIG_PATH_INFO, MaxTopologyOutputs> paths{};
    std::array<DISPLAYCONFIG_MODE_INFO, MaxModes> modes{};
    UINT32 pathCount = 0, modeCount = 0;
    bool queried = false;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const LONG sized = GetDisplayConfigBufferSizes(QueryFlags, &pathCount, &modeCount);
        if (sized != ERROR_SUCCESS) return Failure(TopologyStage::BufferSizes, sized);
        if (!pathCount || !modeCount) return Failure(TopologyStage::InvalidData);
        if (pathCount > paths.size() || modeCount > modes.size()) return Failure(TopologyStage::Capacity);
        const auto pathCapacity = pathCount, modeCapacity = modeCount;
        const LONG status = QueryDisplayConfig(QueryFlags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        if (status == ERROR_INSUFFICIENT_BUFFER) continue;
        if (status != ERROR_SUCCESS) return Failure(TopologyStage::Query, status);
        if (!pathCount || !modeCount || pathCount > pathCapacity || modeCount > modeCapacity) return Failure(TopologyStage::InvalidData);
        queried = true; break;
    }
    if (!queried) return Failure(TopologyStage::Query, ERROR_INSUFFICIENT_BUFFER);
    WindowedTopology snapshot; snapshot.count = pathCount;
    for (unsigned index = 0; index < pathCount; ++index) {
        const auto& path = paths[index]; auto& output = snapshot.outputs[index];
        output.source = Endpoint(path.sourceInfo.adapterId, path.sourceInfo.id);
        output.target = Endpoint(path.targetInfo.adapterId, path.targetInfo.id);
        const auto sourceIndex = path.flags & DISPLAYCONFIG_PATH_SUPPORT_VIRTUAL_MODE ? path.sourceInfo.sourceModeInfoIdx : path.sourceInfo.modeInfoIdx;
        output.rotation = Bits(path.targetInfo.rotation);
        if (!(path.flags & DISPLAYCONFIG_PATH_ACTIVE) || (path.flags & ~DISPLAYCONFIG_PATH_VALID_FLAGS) || !path.targetInfo.targetAvailable ||
            !(path.sourceInfo.statusFlags & DISPLAYCONFIG_SOURCE_IN_USE) || !(path.targetInfo.statusFlags & DISPLAYCONFIG_TARGET_IN_USE) ||
            sourceIndex >= modeCount || output.rotation < 1 || output.rotation > 4) return Failure(TopologyStage::InvalidData);
        const auto& mode = modes[sourceIndex];
        if (Bits(mode.infoType) != DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE || Endpoint(mode.adapterId, mode.id) != output.source) return Failure(TopologyStage::InvalidData);
        const auto& source = mode.sourceMode;
        const auto right = static_cast<std::int64_t>(source.position.x) + source.width;
        const auto bottom = static_cast<std::int64_t>(source.position.y) + source.height;
        constexpr auto maximum = (std::numeric_limits<std::int32_t>::max)();
        if (!source.width || !source.height || source.width > maximum || source.height > maximum || right > maximum || bottom > maximum) return Failure(TopologyStage::InvalidData);
        output.region.bounds = {source.position.x, source.position.y, static_cast<std::int32_t>(right), static_cast<std::int32_t>(bottom)};
        for (unsigned prior = 0; prior < index; ++prior) {
            if (snapshot.outputs[prior].source == output.source) return Failure(TopologyStage::SharedSource);
            if (snapshot.outputs[prior].target == output.target) return Failure(TopologyStage::InvalidData);
        }
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
        sourceName.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(sourceName), path.sourceInfo.adapterId, path.sourceInfo.id};
        LONG status = DisplayConfigGetDeviceInfo(&sourceName.header);
        if (status != ERROR_SUCCESS) return Failure(TopologyStage::SourceName, status);
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
        targetName.header = {DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, sizeof(targetName), path.targetInfo.adapterId, path.targetInfo.id};
        status = DisplayConfigGetDeviceInfo(&targetName.header);
        if (status != ERROR_SUCCESS) return Failure(TopologyStage::TargetName, status);
        if (!Name(sourceName.viewGdiDeviceName, output.gdiName) || !Name(targetName.monitorDevicePath, output.targetPath)) return Failure(TopologyStage::InvalidData);
        for (unsigned prior = 0; prior < index; ++prior) if (snapshot.outputs[prior].gdiName == output.gdiName || snapshot.outputs[prior].targetPath == output.targetPath)
            return Failure(TopologyStage::MonitorMapping);
    }
    Monitors monitors;
    const bool enumerated = EnumDisplayMonitors(nullptr, nullptr, Collect, reinterpret_cast<LPARAM>(&monitors)) != FALSE;
    if (monitors.failure != TopologyStage::None) return Failure(monitors.failure);
    if (!enumerated) return Failure(TopologyStage::Enumeration);
    if (monitors.count != pathCount) return Failure(TopologyStage::MonitorMapping);
    unsigned primaryCount = 0;
    for (unsigned index = 0; index < pathCount; ++index) {
        auto& output = snapshot.outputs[index];
        const auto found = std::find_if(monitors.entries.begin(), monitors.entries.begin() + monitors.count, [&](const Monitor& monitor) { return monitor.name == output.gdiName; });
        if (found == monitors.entries.begin() + monitors.count) return Failure(TopologyStage::MonitorMapping);
        const auto& bounds = output.region.bounds;
        if (!Same(found->bounds, {bounds.left, bounds.top, bounds.right, bounds.bottom})) return Failure(TopologyStage::MonitorMapping);
        output.primary = found->primary; primaryCount += output.primary;
        for (unsigned prior = 0; prior < index; ++prior) {
            const auto& other = snapshot.outputs[prior].region.bounds;
            if (bounds.left < other.right && other.left < bounds.right && bounds.top < other.bottom && other.top < bounds.bottom) return Failure(TopologyStage::MonitorMapping);
        }
    }
    if (primaryCount != 1) return Failure(TopologyStage::MonitorMapping);
    std::sort(snapshot.outputs.begin(), snapshot.outputs.begin() + pathCount, [](const TopologyOutput& a, const TopologyOutput& b) {
        return std::tie(a.source.adapterHigh, a.source.adapterLow, a.source.id) < std::tie(b.source.adapterHigh, b.source.adapterLow, b.source.id);
    });
    for (unsigned index = 0; index < pathCount; ++index) snapshot.outputs[index].region.key = index + 1;
    return {snapshot};
}
}

TopologyReadResult ReadWindowedTopology() noexcept {
    if (!DpiReady()) return Failure(TopologyStage::DpiContext);
    const auto started = GetTickCount64();
    auto first = Capture(); if (!first.snapshot) return first;
    const auto second = Capture(); if (!second.snapshot) return second;
    if (!DpiReady()) return Failure(TopologyStage::DpiContext);
    const auto elapsed = GetTickCount64() - started;
    if (elapsed > 2000) return Failure(TopologyStage::TooSlow);
    if (*first.snapshot != *second.snapshot) return Failure(TopologyStage::Changed);
    first.captureMilliseconds = elapsed;
    return first;
}
}
