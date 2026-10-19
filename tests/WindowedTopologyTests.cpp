#include <windows.h>
#include <wingdi.h>
#include "WindowedTopology.h"
#include "MemoryFault.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
using namespace MonitorPrototype;
enum class Change { None, Bounds, Source, Target, TargetPath, GdiName, Rotation, Primary, Ignored };
struct Fake {
    std::array<DISPLAYCONFIG_PATH_INFO, 9> paths{};
    std::array<DISPLAYCONFIG_MODE_INFO, 36> modes{};
    std::array<MONITORINFOEXW, 9> monitors{};
    std::array<std::array<wchar_t, 32>, 9> sourceNames{};
    std::array<std::array<wchar_t, 128>, 9> targetNames{};
    UINT32 pathSize = 2, modeSize = 4, queryPaths = 2, queryModes = 4;
    unsigned monitorCount = 2, sizes = 0, queries = 0, sourceReads = 0, targetReads = 0, enumerations = 0, monitorReads = 0, dpiReads = 0, errorReads = 0, ticks = 0;
    LONG sizesError = 0, queryError = 0, sourceError = 0, targetError = 0;
    unsigned insufficient = 0;
    bool enumOk = true, infoOk = true, dpiOk = true, dpiNull = false, dpiChanges = false, reversePaths = false, reverseMonitors = false;
    bool badParameters = false, callbackNull = false, callbackDc = false, callbackRect = false, duplicateHandle = false, wrongSize = false;
    bool failedCallbackAsSuccess = false;
    std::array<std::uint64_t, 2> tickValues{1000, 1002};
    Change change = Change::None;
} fake;
bool invalidDuringRun = false;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool Same(const LUID& a, const LUID& b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
HMONITOR MonitorHandle(unsigned index) { return reinterpret_cast<HMONITOR>(std::uintptr_t{index + 1}); }
void Reset(unsigned outputs = 2) {
    invalidDuringRun = invalidDuringRun || fake.badParameters || fake.errorReads;
    fake = {}; fake.pathSize = fake.queryPaths = outputs; fake.modeSize = fake.queryModes = outputs * 2; fake.monitorCount = outputs;
    for (unsigned index = 0; index < outputs && index < fake.paths.size(); ++index) {
        const auto left = static_cast<LONG>(index) * 1920 - 1920;
        auto& path = fake.paths[index]; auto& mode = fake.modes[index * 2]; auto& monitor = fake.monitors[index];
        path.sourceInfo = {{100, 7}, index + 1, {}, 1}; path.sourceInfo.modeInfoIdx = index * 2;
        path.targetInfo = {{200, 8}, index + 11, index * 2 + 1, 1, TRUE, 1}; path.flags = DISPLAYCONFIG_PATH_ACTIVE;
        mode = {DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE, index + 1, {100, 7}, {1920, 1080, 4, {left, 0}}};
        monitor.cbSize = sizeof(monitor); monitor.rcMonitor = {left, 0, left + 1920, 1080}; monitor.rcWork = {left, 0, left + 1920, 1040};
        monitor.dwFlags = index == (outputs == 1 ? 0u : 1u) ? MONITORINFOF_PRIMARY : 0;
        swprintf_s(monitor.szDevice, L"\\\\.\\DISPLAY%u", index + 1);
        std::copy_n(monitor.szDevice, 32, fake.sourceNames[index].begin());
        swprintf_s(fake.targetNames[index].data(), fake.targetNames[index].size(), L"\\?\\DISPLAY#target%u", index + 1);
    }
}
void Shift(unsigned index, LONG left) {
    fake.modes[index * 2].sourceMode.position.x = left;
    fake.monitors[index].rcMonitor.left = left; fake.monitors[index].rcMonitor.right = left + 1920;
}
void ModifySecondCapture() {
    switch (fake.change) {
    case Change::None: break;
    case Change::Bounds: Shift(0, -3840); break;
    case Change::Source: fake.paths[0].sourceInfo.id += 10; fake.modes[0].id += 10; break;
    case Change::Target: fake.paths[0].targetInfo.id += 10; break;
    case Change::TargetPath: fake.targetNames[0][5] = L'X'; break;
    case Change::GdiName: fake.sourceNames[0][4] = L'X'; fake.monitors[0].szDevice[4] = L'X'; break;
    case Change::Rotation: fake.paths[0].targetInfo.rotation = 2; break;
    case Change::Primary: fake.monitors[0].dwFlags = MONITORINFOF_PRIMARY; fake.monitors[1].dwFlags = 0; break;
    case Change::Ignored:
        ++fake.monitors[0].rcWork.bottom;
        fake.sourceNames[0].back() = L'Z'; fake.targetNames[0].back() = L'Z'; fake.monitors[0].szDevice[31] = L'Z';
        break;
    }
}
void ConfirmedMappingAndCanonicalOrder() {
    Reset(); fake.reversePaths = fake.reverseMonitors = true;
    fake.paths[1].flags |= DISPLAYCONFIG_PATH_SUPPORT_VIRTUAL_MODE;
    fake.paths[1].sourceInfo.cloneGroupId = 0xffff; fake.paths[1].sourceInfo.sourceModeInfoIdx = 2;
    const auto result = ReadWindowedTopology();
    Check(result.snapshot && result.stage == TopologyStage::None && result.nativeError == 0 && result.captureMilliseconds == 2 &&
        result.snapshot->count == 2 && result.snapshot->outputs[0].region == Region{1, {-1920, 0, 0, 1080}, false} &&
        result.snapshot->outputs[1].region == Region{2, {0, 0, 1920, 1080}, false} && !result.snapshot->outputs[0].primary && result.snapshot->outputs[1].primary,
        "physical-pixel source/full-monitor bounds, negative origin, virtual source index and canonical transient keys agree across captures");
    Check(fake.queries == 2 && fake.sizes == 2 && fake.enumerations == 2 && fake.monitorReads == 4 && fake.sourceReads == 4 && fake.targetReads == 4 && fake.dpiReads == 2,
        "every active path is named/mapped twice without relying on native enumeration order");
}
void DpiContextIsReadOnlyAndRequired() {
    for (unsigned invalid = 0; invalid < 3; ++invalid) {
        Reset(); fake.dpiNull = invalid == 0; fake.dpiOk = invalid != 1; fake.dpiChanges = invalid == 2;
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == TopologyStage::DpiContext && result.nativeError == 0 &&
            fake.queries == (invalid == 2 ? 2u : 0u), "null/non-PMv2/changed thread awareness cannot grant pixel-coordinate confidence");
    }
}
void PortraitAdaptersAndExactCoordinateLimits() {
    Reset(); fake.paths[0].sourceInfo.adapterId = {900, -3}; fake.modes[0].adapterId = {900, -3};
    fake.paths[0].targetInfo.adapterId = {700, -7}; fake.paths[0].targetInfo.rotation = 2;
    fake.modes[0].sourceMode = {1080, 1920, 4, {-1080, -800}};
    fake.monitors[0].rcMonitor = {-1080, -800, 0, 1120};
    auto result = ReadWindowedTopology();
    Check(result.snapshot && result.snapshot->outputs[0].region.bounds == Rect{-1080, -800, 0, 1120} &&
        result.snapshot->outputs[0].source == DisplayEndpoint{900, -3, 1} && result.snapshot->outputs[0].target == DisplayEndpoint{700, -7, 11} &&
        result.snapshot->outputs[0].rotation == 2, "portrait bounds and distinct signed adapter/source/target identities are preserved without assuming one GPU");
    Reset(); const auto minimum = (std::numeric_limits<std::int32_t>::min)(), maximum = (std::numeric_limits<std::int32_t>::max)();
    fake.modes[0].sourceMode.position.x = minimum; fake.modes[0].sourceMode.width = static_cast<UINT32>(maximum);
    fake.monitors[0].rcMonitor = {minimum, 0, -1, 1080};
    fake.modes[2].sourceMode.position.x = maximum - 1920; fake.monitors[1].rcMonitor = {maximum - 1920, 0, maximum, 1080};
    Check(ReadWindowedTopology().snapshot.has_value(), "representable full source rectangles at both signed coordinate limits are accepted without arithmetic wrap");
    Reset(1); const auto one = ReadWindowedTopology();
    Check(one.snapshot && one.snapshot->count == 1, "one desktop output is represented without inventing a subset selection");
    Reset(); fake.sourceNames[0].fill(L'\u03a9'); fake.sourceNames[0].back() = 0;
    std::copy(fake.sourceNames[0].begin(), fake.sourceNames[0].end(), fake.monitors[0].szDevice);
    fake.targetNames[0].fill(L'\u03a9'); fake.targetNames[0].back() = 0;
    Check(ReadWindowedTopology().snapshot.has_value(), "maximum terminated Unicode identities remain complete and bounded");
}
void DirectNativeErrorsExcludeStaleLastError() {
    for (unsigned stage = 0; stage < 4; ++stage) for (const LONG code : {5L, 50L, 4321L}) {
        Reset();
        if (stage == 0) fake.sizesError = code;
        if (stage == 1) fake.queryError = code;
        if (stage == 2) fake.sourceError = code;
        if (stage == 3) fake.targetError = code;
        constexpr std::array stages{TopologyStage::BufferSizes, TopologyStage::Query, TopologyStage::SourceName, TopologyStage::TargetName};
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == stages[stage] && result.nativeError == static_cast<std::uint32_t>(code) && fake.errorReads == 0,
            "CCD APIs use returned status codes, never thread last-error as a substitute");
    }
}
void BufferRacesAreBoundedAndDoNotPublish() {
    for (unsigned retries = 0; retries <= 3; ++retries) {
        Reset(); fake.insufficient = retries;
        const auto result = ReadWindowedTopology();
        Check(result.snapshot.has_value() == (retries < 3) && fake.queries == (retries < 3 ? retries + 2 : 3) && fake.sizes == fake.queries,
            "insufficient-buffer races re-read sizes with at most three attempts per capture");
        if (retries == 3) Check(result.stage == TopologyStage::Query && result.nativeError == ERROR_INSUFFICIENT_BUFFER, "persistent race reports its direct failure without partial state");
    }
}
void CapacityAndSuccessfulCountValidation() {
    for (unsigned invalid = 0; invalid < 6; ++invalid) {
        Reset();
        if (invalid == 0) fake.pathSize = 9;
        if (invalid == 1) fake.modeSize = 33;
        if (invalid == 2) fake.pathSize = 0;
        if (invalid == 3) fake.modeSize = 0;
        if (invalid == 4) fake.queryPaths = 3;
        if (invalid == 5) fake.queryModes = 5;
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == (invalid < 2 ? TopologyStage::Capacity : TopologyStage::InvalidData),
            "empty/over-limit estimates or impossible success counts cannot expose unfilled arrays");
    }
    Reset(8); const auto exact = ReadWindowedTopology();
    Check(exact.snapshot && exact.snapshot->count == 8, "all eight supported outputs publish only after complete mapping");
}
void InvalidNativePathAndSourceModes() {
    for (unsigned invalid = 0; invalid < 16; ++invalid) {
        Reset(); auto& path = fake.paths[0]; auto& mode = fake.modes[0];
        if (invalid == 0) path.flags = 0;
        if (invalid == 1) path.flags |= 0x100;
        if (invalid == 2) path.targetInfo.targetAvailable = FALSE;
        if (invalid == 3) path.sourceInfo.modeInfoIdx = 0xffffffffu;
        if (invalid == 4) path.targetInfo.rotation = 0;
        if (invalid == 5) path.targetInfo.rotation = 5;
        if (invalid == 6) mode.infoType = static_cast<DISPLAYCONFIG_MODE_INFO_TYPE>(2);
        if (invalid == 7) ++mode.id;
        if (invalid == 8) ++mode.adapterId.HighPart;
        if (invalid == 9) mode.sourceMode.width = 0;
        if (invalid == 10) mode.sourceMode.height = 0xffffffffu;
        if (invalid == 11) mode.sourceMode.position.x = (std::numeric_limits<std::int32_t>::max)();
        if (invalid == 12) mode.sourceMode.position.y = (std::numeric_limits<std::int32_t>::max)();
        if (invalid == 13) { path.flags |= DISPLAYCONFIG_PATH_SUPPORT_VIRTUAL_MODE; path.sourceInfo.sourceModeInfoIdx = 0xffff; }
        if (invalid == 14) path.sourceInfo.statusFlags = 0;
        if (invalid == 15) path.targetInfo.statusFlags = 0;
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == TopologyStage::InvalidData, "invalid active paths, source union/index/identity, orientation, or widened bounds fail closed");
    }
}
void SharedSourcesAndAmbiguousTargets() {
    Reset(); fake.paths[1].sourceInfo = fake.paths[0].sourceInfo;
    Check(ReadWindowedTopology().stage == TopologyStage::SharedSource, "two physical targets sharing one source are refused before apparent desktop rectangles can imply independence");
    Reset(); fake.paths[1].targetInfo = fake.paths[0].targetInfo;
    Check(ReadWindowedTopology().stage == TopologyStage::InvalidData, "a target cannot be mapped from two different source records");
    for (const bool source : {false, true}) {
        Reset(); if (source) fake.sourceNames[1] = fake.sourceNames[0]; else fake.targetNames[1] = fake.targetNames[0];
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == TopologyStage::MonitorMapping, "duplicate source GDI names or target device paths cannot grant a selection receipt");
    }
}
void DeviceNamesAreBoundedAndCanonical() {
    for (unsigned invalid = 0; invalid < 6; ++invalid) {
        Reset();
        if (invalid == 0) fake.sourceNames[0].fill(L'X');
        if (invalid == 1) fake.targetNames[0].fill(L'X');
        if (invalid == 2) fake.sourceNames[0][0] = 0;
        if (invalid == 3) fake.targetNames[0][0] = 0;
        if (invalid == 4) fake.sourceNames[0][0] = L'\n';
        if (invalid == 5) fake.targetNames[0][0] = L'\t';
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == TopologyStage::InvalidData, "empty/unterminated/control-containing device identities fail before matching or publication");
    }
    Reset(); fake.change = Change::Ignored;
    Check(ReadWindowedTopology().snapshot.has_value(), "unused name tails and work-area changes do not invalidate the full-bounds semantic snapshot");
}
void EnumerationAndMonitorFailuresHaveNoInventedCause() {
    for (unsigned invalid = 0; invalid < 9; ++invalid) {
        Reset();
        if (invalid == 0) fake.enumOk = false;
        if (invalid == 1) fake.infoOk = false;
        if (invalid == 2) fake.callbackNull = true;
        if (invalid == 3) fake.callbackDc = true;
        if (invalid == 4) fake.callbackRect = true;
        if (invalid == 5) fake.duplicateHandle = true;
        if (invalid == 6) fake.wrongSize = true;
        if (invalid == 7) std::fill_n(fake.monitors[0].szDevice, 32, L'X');
        if (invalid == 8) fake.monitorCount = 1;
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.nativeError == 0 && fake.errorReads == 0, "BOOL-only enumeration/info failures and malformed callbacks never report stale undocumented last-error values");
        if (invalid == 0) Check(result.stage == TopologyStage::Enumeration, "enumeration stage preserved");
        if (invalid == 1) Check(result.stage == TopologyStage::MonitorInfo, "callback API failure stage preserved");
    }
    Reset(9); fake.pathSize = fake.queryPaths = 2; fake.modeSize = fake.queryModes = 4; fake.failedCallbackAsSuccess = true;
    Check(ReadWindowedTopology().stage == TopologyStage::Capacity && fake.monitorReads == 8, "a stopped/overflowing callback cannot publish even if enumeration reports success");
}
void RectanglesPrimaryAndCompleteMapping() {
    for (unsigned invalid = 0; invalid < 6; ++invalid) {
        Reset();
        if (invalid == 0) ++fake.monitors[0].rcMonitor.right;
        if (invalid == 1) fake.monitors[0].szDevice[4] = L'Z';
        if (invalid == 2) Shift(1, -10);
        if (invalid == 3) fake.monitors[0].dwFlags = MONITORINFOF_PRIMARY;
        if (invalid == 4) fake.monitors[1].dwFlags = 0;
        if (invalid == 5) std::copy_n(fake.monitors[0].szDevice, 32, fake.monitors[1].szDevice);
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == TopologyStage::MonitorMapping, "unmatched/full-bound disagreements, overlaps, duplicate names and missing/multiple primary flags cannot publish");
    }
}
void SemanticChangesRejectTheWholeCapture() {
    for (const auto change : {Change::Bounds, Change::Source, Change::Target, Change::TargetPath, Change::GdiName, Change::Rotation, Change::Primary}) {
        Reset(); fake.change = change;
        const auto result = ReadWindowedTopology();
        Check(!result.snapshot && result.stage == TopologyStage::Changed && result.nativeError == 0, "source/target/device-path/name/geometry/rotation/primary changes invalidate the complete receipt");
    }
}
void CaptureLatencyWrapAndAllocationIsolation() {
    for (const auto elapsed : {0ull, 2000ull, 2001ull}) {
        Reset(); fake.tickValues = {1000, 1000 + elapsed}; const auto result = ReadWindowedTopology();
        Check(result.snapshot.has_value() == (elapsed <= 2000), "the whole double-capture deadline is bounded independently of display state");
    }
    Reset(); fake.tickValues = {(std::numeric_limits<std::uint64_t>::max)() - 1, 0};
    Check(ReadWindowedTopology().snapshot.has_value(), "unsigned monotonic wrap preserves short captures");
    Reset(); fake.tickValues = {1000, 999}; Check(ReadWindowedTopology().stage == TopologyStage::TooSlow, "backward clock data fails closed");
    Reset(); TopologyReadResult result; bool pending = false;
    { TestMemory::FailNextAllocation failure; result = ReadWindowedTopology(); pending = TestMemory::AllocationFailurePending(); }
    Check(result.snapshot && pending && fake.errorReads == 0 && !fake.badParameters, "fixed-capacity capture allocates no C++ memory and uses only read-only native queries");
}
}

DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext() { ++fake.dpiReads; return fake.dpiNull ? nullptr : DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2; }
BOOL AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT context, DPI_AWARENESS_CONTEXT expected) {
    if (context != DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 || expected != DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) fake.badParameters = true;
    return fake.dpiOk && !(fake.dpiChanges && fake.dpiReads > 1);
}
std::uint64_t GetTickCount64() { if (fake.ticks >= 2) { fake.badParameters = true; return 0; } return fake.tickValues[fake.ticks++]; }
DWORD GetLastError() { ++fake.errorReads; return 0xbad; }
LONG GetDisplayConfigBufferSizes(UINT32 flags, UINT32* paths, UINT32* modes) {
    ++fake.sizes; if (flags != (QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE) || !paths || !modes) fake.badParameters = true;
    *paths = fake.pathSize; *modes = fake.modeSize; return fake.sizesError;
}
LONG QueryDisplayConfig(UINT32 flags, UINT32* paths, DISPLAYCONFIG_PATH_INFO* pathData, UINT32* modes, DISPLAYCONFIG_MODE_INFO* modeData, DISPLAYCONFIG_TOPOLOGY_ID* topology) {
    ++fake.queries;
    if (flags != (QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE) || !paths || !modes || !pathData || !modeData || topology ||
        *paths != fake.pathSize || *modes != fake.modeSize) fake.badParameters = true;
    if (fake.queries <= fake.insufficient) return ERROR_INSUFFICIENT_BUFFER;
    if (fake.queryError) return fake.queryError;
    if (fake.queries == fake.insufficient + 2) ModifySecondCapture();
    std::copy_n(fake.paths.begin(), (std::min)(*paths, fake.queryPaths), pathData);
    std::copy_n(fake.modes.begin(), (std::min)(*modes, fake.queryModes), modeData);
    if (fake.reversePaths && fake.queries > fake.insufficient + 1) std::reverse(pathData, pathData + (std::min)(*paths, fake.queryPaths));
    *paths = fake.queryPaths; *modes = fake.queryModes; return ERROR_SUCCESS;
}
LONG DisplayConfigGetDeviceInfo(DISPLAYCONFIG_DEVICE_INFO_HEADER* header) {
    const bool source = header->type == DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    if (source) ++fake.sourceReads; else ++fake.targetReads;
    if ((source && header->size != sizeof(DISPLAYCONFIG_SOURCE_DEVICE_NAME)) ||
        (!source && (header->type != DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME || header->size != sizeof(DISPLAYCONFIG_TARGET_DEVICE_NAME)))) fake.badParameters = true;
    for (unsigned index = 0; index < fake.pathSize; ++index) {
        const auto& path = fake.paths[index];
        if (Same(header->adapterId, source ? path.sourceInfo.adapterId : path.targetInfo.adapterId) && header->id == (source ? path.sourceInfo.id : path.targetInfo.id)) {
            if (source) {
                auto* name = reinterpret_cast<DISPLAYCONFIG_SOURCE_DEVICE_NAME*>(header);
                std::copy(fake.sourceNames[index].begin(), fake.sourceNames[index].end(), name->viewGdiDeviceName); return fake.sourceError;
            }
            auto* name = reinterpret_cast<DISPLAYCONFIG_TARGET_DEVICE_NAME*>(header);
            std::copy(fake.targetNames[index].begin(), fake.targetNames[index].end(), name->monitorDevicePath); return fake.targetError;
        }
    }
    fake.badParameters = true; return ERROR_INVALID_PARAMETER;
}
BOOL EnumDisplayMonitors(HDC dc, const RECT* clip, MONITORENUMPROC callback, LPARAM data) {
    ++fake.enumerations; if (dc || clip || !callback || !data) fake.badParameters = true;
    if (!fake.enumOk) return FALSE;
    for (unsigned at = 0; at < fake.monitorCount; ++at) {
        const unsigned index = fake.reverseMonitors ? fake.monitorCount - at - 1 : at;
        RECT bounds = fake.monitors[index].rcMonitor; if (fake.callbackRect) ++bounds.left;
        const auto handle = fake.callbackNull ? nullptr : MonitorHandle(fake.duplicateHandle ? 0 : index);
        if (!callback(handle, fake.callbackDc ? reinterpret_cast<HDC>(std::uintptr_t{1}) : nullptr, &bounds, data)) return fake.failedCallbackAsSuccess;
    }
    return TRUE;
}
BOOL GetMonitorInfoW(HMONITOR handle, MONITORINFO* info) {
    ++fake.monitorReads; const auto index = reinterpret_cast<std::uintptr_t>(handle) - 1;
    if (index >= fake.monitors.size() || !info || info->cbSize != sizeof(MONITORINFOEXW)) { fake.badParameters = true; return FALSE; }
    *static_cast<MONITORINFOEXW*>(info) = fake.monitors[index]; if (fake.wrongSize) info->cbSize = 0; return fake.infoOk;
}

int main() {
    const std::pair<const char*, void(*)()> tests[] = {
        {"Topology confirmed mapping/canonical order", ConfirmedMappingAndCanonicalOrder}, {"Topology read-only DPI gate", DpiContextIsReadOnlyAndRequired},
        {"Topology portrait/adapters/coordinate limits", PortraitAdaptersAndExactCoordinateLimits},
        {"Topology direct error provenance", DirectNativeErrorsExcludeStaleLastError}, {"Topology bounded buffer races", BufferRacesAreBoundedAndDoNotPublish},
        {"Topology capacity/count validation", CapacityAndSuccessfulCountValidation}, {"Topology invalid native paths/modes", InvalidNativePathAndSourceModes},
        {"Topology clone/ambiguous targets", SharedSourcesAndAmbiguousTargets}, {"Topology bounded canonical identities", DeviceNamesAreBoundedAndCanonical},
        {"Topology enumeration/callback failures", EnumerationAndMonitorFailuresHaveNoInventedCause}, {"Topology complete geometry/primary mapping", RectanglesPrimaryAndCompleteMapping},
        {"Topology semantic change rejection", SemanticChangesRejectTheWholeCapture}, {"Topology latency/wrap/allocation isolation", CaptureLatencyWrapAndAllocationIsolation}
    };
    unsigned failures = 0;
    for (const auto& [name, run] : tests) {
        Reset(); invalidDuringRun = false;
        try { run(); Check(!invalidDuringRun && !fake.badParameters && fake.errorReads == 0, "native argument/error-contract isolation violation"); std::printf("PASS %s\n", name); }
        catch (const std::exception& error) { ++failures; std::printf("FAIL %s: %s\n", name, error.what()); }
    }
    return failures ? 1 : 0;
}
