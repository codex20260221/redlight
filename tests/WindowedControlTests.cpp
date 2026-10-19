#include "WindowedRedControl.h"
#include "MemoryFault.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
using namespace MonitorPrototype;
HWND Host() { return reinterpret_cast<HWND>(std::uintptr_t{1}); }
HWND Peer() { return reinterpret_cast<HWND>(std::uintptr_t{2}); }
HWND Child() { return reinterpret_cast<HWND>(std::uintptr_t{3}); }
HINSTANCE Instance() { return reinterpret_cast<HINSTANCE>(std::uintptr_t{4}); }
enum class Call : unsigned { Init, Create, ColorSet, FiltersSet, TransformSet, SourceSet, ColorGet, FilterCount, FiltersGet, TransformGet, SourceGet, Destroy, Uninit, Count };
enum class Outcome { Normal, Fail, FailAfterApply, Ignore, Mismatch, Nonfinite, Green, Black, Duplicate, Reverse, CountChanged, UnknownHost };
struct Fake {
    std::array<Outcome, static_cast<unsigned>(Call::Count)> outcomes{};
    std::array<Call, 128> events{};
    unsigned calls = 0, initializes = 0, uninitializes = 0, destroys = 0, diagnostics = 0, errorReads = 0, globalCalls = 0;
    bool runtime = false, child = false, host = true, peer = true, hostVisible = false, peerVisible = false, childVisible = false;
    bool badParameters = false, geometryRead = true, wrongParent = false, moveOnRead = false, showOnRead = false, styleRead = true;
    RECT bounds{-1920, 0, 0, 1080}, client{0, 0, 1920, 1080}, source{};
    MAGCOLOREFFECT effect{};
    MAGTRANSFORM transform{};
    std::array<HWND, 16> exclusions{};
    int count = 0;
    DWORD mode = MW_FILTERMODE_EXCLUDE;
    WindowedRedControl* cancel = nullptr;
    Call cancelAt = Call::Count;
} fake;
bool invalidDuringRun = false;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
unsigned Index(Call call) { return static_cast<unsigned>(call); }
Outcome Record(Call call, HWND target = nullptr) {
    if (target && target != Child()) fake.badParameters = true;
    if (fake.calls < fake.events.size()) fake.events[fake.calls++] = call;
    else fake.badParameters = true;
    if (fake.cancel && fake.cancelAt == call) { fake.cancelAt = Call::Count; fake.cancel->shutdown(); }
    return fake.outcomes[Index(call)];
}
bool Applies(Outcome result) { return result != Outcome::Fail && result != Outcome::Ignore; }
BOOL Succeeds(Outcome result) { return result != Outcome::Fail && result != Outcome::FailAfterApply; }
std::array<Region, 2> Regions() { return {{{1, {-1920, 0, 0, 1080}, true}, {2, {0, 0, 1920, 1080}, false}}}; }
Context Clear() { return {7, 7, true, true, true}; }
bool Prepare(WindowedRedControl& control, RedMode mode = RedMode::LumaRed, LumaProfile profile = LumaProfile::Classic, unsigned brightness = 37) {
    const auto regions = Regions(); const std::array hosts{Host(), Peer()};
    return control.prepare(Host(), Instance(), regions, Clear(), hosts, mode, profile, brightness);
}
void Reset() { invalidDuringRun = invalidDuringRun || fake.badParameters || fake.errorReads || fake.globalCalls; fake = {}; }

void HiddenPreparationOrderingAndCleanup() {
    WindowedRedControl control;
    Check(Prepare(control) && control.prepared() && control.ownsResources() && control.child() == Child() && fake.runtime && fake.child &&
        !fake.hostVisible && !fake.peerVisible && !fake.childVisible, "only a confirmed hidden child is prepared");
    const std::array expected{Call::Init, Call::Create, Call::ColorSet, Call::FiltersSet, Call::TransformSet, Call::SourceSet,
        Call::ColorGet, Call::FilterCount, Call::FiltersGet, Call::TransformGet, Call::SourceGet};
    Check(fake.calls == expected.size() && std::equal(expected.begin(), expected.end(), fake.events.begin()) && fake.count == 2 &&
        fake.exclusions[0] == Host() && fake.exclusions[1] == Peer() && fake.source.left == -1920 && fake.source.right == 0,
        "red-only setup precedes read-back; every supplied host is excluded and negative full bounds are preserved");
    Check(control.shutdown() && !control.prepared() && !control.ownsResources() && fake.destroys == 1 && fake.uninitializes == 1 && !fake.runtime && !fake.child,
        "child destruction precedes runtime release without destroying borrowed hosts");
    const auto calls = fake.calls;
    Check(control.shutdown() && fake.calls == calls && fake.host && fake.peer, "cleanup is idempotent and borrowed hosts remain owned by their caller");
}
void EveryBrightnessModeAndProfile() {
    for (const auto mode : {RedMode::StrictRed, RedMode::LumaRed}) for (unsigned profile = 0; profile < 3; ++profile) for (unsigned percent = 1; percent <= 100; ++percent) {
        Reset(); WindowedRedControl control;
        Check(Prepare(control, mode, static_cast<LumaProfile>(profile), percent), "every existing red mode/profile/whole brightness prepares");
        const auto weights = LumaProfileWeights(static_cast<LumaProfile>(profile)); const float scale = static_cast<float>(percent) / 100.0f;
        for (unsigned row = 0; row < 5; ++row) for (unsigned column = 0; column < 5; ++column) {
            const float expected = (row == 3 && column == 3) || (row == 4 && column == 4) ? 1.0f :
                (column == 0 && row < 3 ? (mode == RedMode::StrictRed ? (row == 0 ? scale : 0) : weights[row] * scale) : 0);
            Check(fake.effect.transform[row][column] == expected, "black/green/blue offsets are exact zero; alpha and 100% coefficients are preserved");
        }
        Check(control.shutdown(), "release each isolated configuration");
    }
}
void InvalidPlansNeverInitialize() {
    for (unsigned invalid = 0; invalid < 15; ++invalid) {
        Reset(); WindowedRedControl control; auto regions = Regions(); auto context = Clear(); std::array hosts{Host(), Peer()};
        auto instance = Instance(); unsigned percent = 37; RedMode mode = RedMode::StrictRed; LumaProfile profile = LumaProfile::Classic;
        if (invalid == 0) context.backendClear = false;
        if (invalid == 1) context.selectionRevision = 6;
        if (invalid == 2) context.pixelCoordinatesSupplied = false;
        if (invalid == 3) context.independentSourcesSupplied = false;
        if (invalid == 4) regions[1].selected = true;
        if (invalid == 5) regions[0].selected = false;
        if (invalid == 6) regions[1].bounds.left = -1;
        if (invalid == 7) instance = nullptr;
        if (invalid == 8) percent = 0;
        if (invalid == 9) percent = 101;
        if (invalid == 10) mode = static_cast<RedMode>(99);
        if (invalid == 11) profile = static_cast<LumaProfile>(99);
        if (invalid == 12) hosts[0] = Peer();
        if (invalid == 13) hosts[1] = Host();
        if (invalid == 14) hosts[1] = nullptr;
        Check(!control.prepare(Host(), instance, regions, context, hosts, mode, profile, percent) && control.failure() == ControlStage::InvalidRequest &&
            !control.prepared() && !control.ownsResources() && fake.calls == 0, "invalid topology/selection/consent/options fail before native Magnification work");
    }
}
void HiddenHostAndGeometryGates() {
    for (unsigned invalid = 0; invalid < 8; ++invalid) {
        Reset(); WindowedRedControl control;
        if (invalid == 0) fake.hostVisible = true;
        if (invalid == 1) fake.peerVisible = true;
        if (invalid == 2) fake.host = false;
        if (invalid == 3) fake.peer = false;
        if (invalid == 4) fake.geometryRead = false;
        if (invalid == 5) ++fake.bounds.left;
        if (invalid == 6) --fake.client.right;
        if (invalid == 7) fake.client.left = 1;
        Check(!Prepare(control) && !control.ownsResources() && fake.initializes == 0 && fake.calls == 0 &&
            control.failure() == (invalid < 4 ? ControlStage::HostNotHidden : ControlStage::HostGeometry), "unproven hidden/full/client geometry cannot initialize or create a child");
    }
}
void ExclusionBoundsAndCompleteMembership() {
    const auto regions = Regions();
    for (const unsigned count : {0u, 17u}) {
        Reset(); WindowedRedControl control; std::array<HWND, 17> hosts{}; hosts.fill(Peer()); hosts[0] = Host();
        Check(!control.prepare(Host(), Instance(), regions, Clear(), std::span<const HWND>(hosts.data(), count), RedMode::StrictRed, LumaProfile::Classic, 100) &&
            control.failure() == ControlStage::InvalidRequest && fake.calls == 0, "empty and oversized exclusion lists fail before native work");
    }
    Reset(); WindowedRedControl control; std::array<HWND, 16> hosts{}; hosts[0] = Host();
    for (unsigned index = 1; index < hosts.size(); ++index) hosts[index] = reinterpret_cast<HWND>(std::uintptr_t{4} + index);
    Check(control.prepare(Host(), Instance(), regions, Clear(), hosts, RedMode::StrictRed, LumaProfile::Classic, 100) &&
        fake.count == 16 && fake.exclusions == hosts && control.shutdown(), "the exact bounded capacity is confirmed without truncating any host");
    for (const auto call : {Call::FilterCount, Call::FiltersGet}) for (const auto outcome : {Outcome::CountChanged, Outcome::UnknownHost}) {
        Reset(); WindowedRedControl failed; fake.outcomes[Index(call)] = outcome;
        if (call == Call::FilterCount && outcome == Outcome::UnknownHost) continue; // Membership is returned only by the second read.
        Check(!Prepare(failed) && failed.failure() == ControlStage::ExclusionsMismatch && !failed.ownsResources(),
            "changed counts and substituted host handles fail even when calls report success");
    }
}
void EveryNativePreparationFailure() {
    constexpr std::array stages{ControlStage::Initialize, ControlStage::Create, ControlStage::ColorWrite, ControlStage::ExclusionsWrite,
        ControlStage::TransformWrite, ControlStage::SourceWrite, ControlStage::ColorRead, ControlStage::ExclusionsRead,
        ControlStage::ExclusionsRead, ControlStage::TransformRead, ControlStage::SourceRead};
    for (unsigned index = 0; index < stages.size(); ++index) {
        Reset(); WindowedRedControl control; fake.outcomes[index] = Outcome::Fail;
        Check(!Prepare(control) && !control.prepared() && !control.ownsResources() && control.failure() == stages[index] &&
            fake.uninitializes == (index == 0 ? 0u : 1u) && fake.destroys == (index <= 1 ? 0u : 1u),
            "every native failure fails closed and releases only confirmed owned child/runtime references");
    }
}
void FailedOrIgnoredSettersCannotConfirm() {
    for (const auto call : {Call::ColorSet, Call::FiltersSet, Call::TransformSet, Call::SourceSet}) for (const auto outcome : {Outcome::FailAfterApply, Outcome::Ignore}) {
        Reset(); WindowedRedControl control; fake.outcomes[Index(call)] = outcome;
        Check(!Prepare(control) && !control.prepared() && !control.ownsResources() && fake.destroys == 1 && fake.uninitializes == 1,
            "failed-but-applied setters cannot be accepted by matching output; ignored writes fail subsequent read-back");
    }
}
void UnsafeColorReadbackNeverPrepares() {
    for (const auto outcome : {Outcome::Mismatch, Outcome::Nonfinite, Outcome::Green, Outcome::Black}) {
        Reset(); WindowedRedControl control; fake.outcomes[Index(Call::ColorGet)] = outcome;
        Check(!Prepare(control) && control.failure() == ControlStage::ColorMismatch && !control.prepared() && !control.ownsResources(),
            "nonfinite/mismatching/red-only-zero violations reject the hidden child");
    }
}
void SourceTransformAndExclusionReadbacks() {
    for (const auto call : {Call::FilterCount, Call::FiltersGet, Call::TransformGet, Call::SourceGet}) {
        Reset(); WindowedRedControl control; fake.outcomes[Index(call)] = Outcome::Mismatch;
        Check(!Prepare(control) && !control.prepared() && !control.ownsResources(), "wrong filter mode/count, matrix or source cannot prepare a child");
    }
    for (const auto outcome : {Outcome::Duplicate, Outcome::Reverse}) {
        Reset(); WindowedRedControl control; fake.outcomes[Index(Call::FiltersGet)] = outcome;
        const bool prepared = Prepare(control);
        Check(prepared == (outcome == Outcome::Reverse), "exclusion membership is complete/unique and independent of order");
        Check(control.shutdown(), "release the independently ordered set");
    }
    Reset(); WindowedRedControl control; fake.outcomes[Index(Call::TransformGet)] = Outcome::Nonfinite;
    Check(!Prepare(control) && control.failure() == ControlStage::TransformMismatch, "nonfinite spatial transform cannot pass identity comparison");
}
void CleanupFailureRetainsExactOwner() {
    for (const auto call : {Call::Destroy, Call::Uninit}) {
        Reset(); WindowedRedControl control; Check(Prepare(control), "prepare cleanup owner");
        fake.outcomes[Index(call)] = Outcome::Fail;
        Check(!control.shutdown() && !control.prepared() && control.ownsResources() &&
            control.cleanupFailure() == (call == Call::Destroy ? ControlStage::Destroy : ControlStage::Uninitialize) &&
            (call != Call::Destroy || fake.uninitializes == 0), "failed destruction blocks runtime release; failed uninitialization retains its own reference");
        const auto calls = fake.calls;
        Check(!Prepare(control) && control.failure() == ControlStage::AlreadyOwned && fake.calls == calls, "pending cleanup blocks reinitialization or new control writes");
        fake.outcomes[Index(call)] = Outcome::Normal;
        Check(control.shutdown() && !control.ownsResources() && control.cleanupFailure() == ControlStage::None &&
            fake.destroys == (call == Call::Destroy ? 2u : 1u), "cleanup retry neither loses the child nor destroys an already-released one twice");
    }
}
void ConfigurationAndCleanupFailuresStayDistinct() {
    Reset(); WindowedRedControl control; fake.outcomes[Index(Call::ColorGet)] = Outcome::Green; fake.outcomes[Index(Call::Destroy)] = Outcome::Fail;
    Check(!Prepare(control) && control.failure() == ControlStage::ColorMismatch && control.cleanupFailure() == ControlStage::Destroy &&
        control.ownsResources() && fake.uninitializes == 0, "a failed configuration keeps its primary failure beside retryable cleanup failure");
    fake.outcomes[Index(Call::Destroy)] = Outcome::Normal; fake.outcomes[Index(Call::Uninit)] = Outcome::Fail;
    Check(!control.shutdown() && control.failure() == ControlStage::ColorMismatch && control.cleanupFailure() == ControlStage::Uninitialize &&
        control.ownsResources() && !control.child(), "cleanup-only failure retains runtime but never creates a replacement child");
    fake.outcomes[Index(Call::Uninit)] = Outcome::Normal;
    Check(control.shutdown() && control.failure() == ControlStage::ColorMismatch, "successful release preserves the causal failure for reporting");
}
void NestedCancellationCannotPublishPreparedState() {
    for (unsigned index = 0; index <= Index(Call::SourceGet); ++index) {
        Reset(); WindowedRedControl control; fake.cancel = &control; fake.cancelAt = static_cast<Call>(index);
        Check(!Prepare(control) && !control.prepared() && !control.ownsResources() && control.failure() == ControlStage::Canceled &&
            fake.uninitializes == 1 && fake.destroys == (index == 0 ? 0u : 1u), "a cancellation during any native preparation call wins before confirmation and cleans after returning");
        fake.cancel = nullptr;
    }
}
void NativeChangesAndAllocationIsolation() {
    for (unsigned changed = 0; changed < 5; ++changed) {
        Reset(); WindowedRedControl control;
        if (changed == 0) fake.showOnRead = true;
        if (changed == 1) fake.moveOnRead = true;
        if (changed == 2) fake.wrongParent = true;
        if (changed == 3) fake.childVisible = true;
        if (changed == 4) fake.styleRead = false;
        Check(!Prepare(control) && !control.prepared() && !control.ownsResources(), "hidden host/child/geometry are rechecked after all native configuration reads");
    }
    Reset(); WindowedRedControl control; bool prepared = false, pending = false, released = false;
    { TestMemory::FailNextAllocation fail; prepared = Prepare(control); released = control.shutdown(); pending = TestMemory::AllocationFailurePending(); }
    Check(prepared && released && pending && fake.errorReads == 0 && fake.globalCalls == 0, "preparation/cleanup allocate no C++ memory and read no stale/undocumented extended error or global display state");
}
void CleanupReentryCannotDoubleRelease() {
    for (const auto call : {Call::Destroy, Call::Uninit}) {
        Reset(); WindowedRedControl control; Check(Prepare(control), "prepare callback-safe child cleanup");
        fake.cancel = &control; fake.cancelAt = call;
        Check(control.shutdown() && !control.ownsResources() && !control.prepared() && fake.destroys == 1 && fake.uninitializes == 1 && !fake.badParameters,
            "a parent notification or runtime callback during cleanup cannot recurse into destruction or release a reference twice");
        fake.cancel = nullptr;
    }
}
void ExistingOwnerAndDestructionLimits() {
    WindowedRedControl control; Check(Prepare(control), "prepare an existing hidden owner");
    const auto calls = fake.calls;
    Check(!Prepare(control, RedMode::StrictRed, LumaProfile::EqualDetail, 100) && control.prepared() &&
        control.failure() == ControlStage::AlreadyOwned && fake.calls == calls, "an already-owned child is not implicitly recolored, replaced, or reinitialized");
    Check(control.shutdown(), "explicit release precedes a new preparation");
    Check(Prepare(control) && control.shutdown() && fake.initializes == 2 && fake.uninitializes == 2 && fake.destroys == 2,
        "a released component can establish a new independent child/runtime lifetime");
    Reset();
    {
        WindowedRedControl dying; Check(Prepare(dying), "prepare the destructor's best-effort cleanup");
        fake.outcomes[Index(Call::Destroy)] = Outcome::Fail;
    }
    Check(fake.diagnostics == 1 && fake.child && fake.runtime && fake.uninitializes == 0,
        "destruction diagnoses failed cleanup and cannot promise recovery beyond the object's lifetime; future probe must retain it for retry");
}
}

BOOL IsWindow(HWND window) {
    const auto key = reinterpret_cast<std::uintptr_t>(window);
    return window == Host() ? fake.host : (window == Peer() ? fake.peer : (window == Child() ? fake.child : key >= 5 && key <= 19));
}
BOOL IsWindowVisible(HWND window) { return window == Host() ? fake.hostVisible : (window == Peer() ? fake.peerVisible : (window == Child() && fake.childVisible && fake.hostVisible)); }
LONG_PTR GetWindowLongPtrW(HWND window, int index) {
    if (window != Child() || index != GWL_STYLE) fake.badParameters = true;
    return fake.styleRead ? WS_CHILD | (fake.childVisible ? WS_VISIBLE : 0) : 0;
}
HWND GetParent(HWND window) { return window == Child() ? (fake.wrongParent ? Peer() : Host()) : nullptr; }
BOOL GetWindowRect(HWND window, RECT* value) { if (window != Host() || !value) fake.badParameters = true; *value = fake.bounds; return fake.geometryRead; }
BOOL GetClientRect(HWND window, RECT* value) { if (window != Host() || !value) fake.badParameters = true; *value = fake.client; return fake.geometryRead; }
HWND CreateWindowExW(DWORD ex, const wchar_t* name, const wchar_t*, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menu, HINSTANCE instance, void* data) {
    const auto result = Record(Call::Create);
    if (ex || std::wcscmp(name, WC_MAGNIFIERW) || style != WS_CHILD || x || y || width != 1920 || height != 1080 ||
        parent != Host() || menu || instance != Instance() || data || !fake.runtime || fake.child) fake.badParameters = true;
    if (!Succeeds(result)) return nullptr;
    fake.child = true; return Child();
}
BOOL DestroyWindow(HWND window) {
    ++fake.destroys; const auto result = Record(Call::Destroy, window);
    if (!fake.child || !fake.runtime) fake.badParameters = true;
    if (!Succeeds(result)) return FALSE;
    fake.child = false; return TRUE;
}
BOOL MagInitialize() {
    ++fake.initializes; const auto result = Record(Call::Init);
    if (fake.runtime) fake.badParameters = true;
    if (!Succeeds(result)) return FALSE;
    fake.runtime = true; return TRUE;
}
BOOL MagUninitialize() {
    ++fake.uninitializes; const auto result = Record(Call::Uninit);
    if (!fake.runtime || fake.child) fake.badParameters = true;
    if (!Succeeds(result)) return FALSE;
    fake.runtime = false; return TRUE;
}
BOOL MagSetColorEffect(HWND window, MAGCOLOREFFECT* effect) {
    const auto result = Record(Call::ColorSet, window); if (Applies(result)) fake.effect = *effect; return Succeeds(result);
}
BOOL MagGetColorEffect(HWND window, MAGCOLOREFFECT* effect) {
    const auto result = Record(Call::ColorGet, window); *effect = fake.effect;
    if (result == Outcome::Mismatch) effect->transform[0][0] += 0.01f;
    if (result == Outcome::Nonfinite) effect->transform[0][0] = (std::numeric_limits<float>::quiet_NaN)();
    if (result == Outcome::Green) effect->transform[0][1] = 0.00000001f;
    if (result == Outcome::Black) effect->transform[4][0] = 0.00000001f;
    return Succeeds(result);
}
BOOL MagSetWindowFilterList(HWND window, DWORD mode, int count, HWND* list) {
    const auto result = Record(Call::FiltersSet, window);
    if (mode != MW_FILTERMODE_EXCLUDE || count < 1 || count > 16 || !list) fake.badParameters = true;
    if (Applies(result)) { fake.mode = mode; fake.count = count; std::copy_n(list, count, fake.exclusions.begin()); }
    return Succeeds(result);
}
int MagGetWindowFilterList(HWND window, DWORD* mode, int count, HWND* list) {
    const auto result = Record(count ? Call::FiltersGet : Call::FilterCount, window);
    if ((count != 0 && count != fake.count) || (count && !list) || (!count && list) || !mode) fake.badParameters = true;
    if (!Succeeds(result)) return -1;
    *mode = fake.mode;
    if (result == Outcome::Mismatch) *mode = 1;
    if (count) std::copy_n(fake.exclusions.begin(), count, list);
    if (count && result == Outcome::Duplicate) list[1] = list[0];
    if (count && result == Outcome::Reverse) std::reverse(list, list + count);
    if (count && result == Outcome::UnknownHost) list[0] = reinterpret_cast<HWND>(std::uintptr_t{99});
    return fake.count + (result == Outcome::CountChanged ? 1 : 0);
}
BOOL MagSetWindowTransform(HWND window, MAGTRANSFORM* value) { const auto result = Record(Call::TransformSet, window); if (Applies(result)) fake.transform = *value; return Succeeds(result); }
BOOL MagGetWindowTransform(HWND window, MAGTRANSFORM* value) {
    const auto result = Record(Call::TransformGet, window); *value = fake.transform;
    if (result == Outcome::Mismatch) value->v[2][0] = 0.01f;
    if (result == Outcome::Nonfinite) value->v[0][0] = (std::numeric_limits<float>::infinity)();
    return Succeeds(result);
}
BOOL MagSetWindowSource(HWND window, RECT value) { const auto result = Record(Call::SourceSet, window); if (Applies(result)) fake.source = value; return Succeeds(result); }
BOOL MagGetWindowSource(HWND window, RECT* value) {
    const auto result = Record(Call::SourceGet, window); *value = fake.source;
    if (result == Outcome::Mismatch) ++value->left;
    if (fake.moveOnRead) ++fake.bounds.left;
    if (fake.showOnRead) fake.hostVisible = true;
    return Succeeds(result);
}
DWORD GetLastError() { ++fake.errorReads; return 0xbad; }
void OutputDebugStringA(const char*) { ++fake.diagnostics; }
BOOL MagGetFullscreenColorEffect(MAGCOLOREFFECT*) { ++fake.globalCalls; return FALSE; }
BOOL MagSetFullscreenColorEffect(MAGCOLOREFFECT*) { ++fake.globalCalls; return FALSE; }
BOOL GetDeviceGammaRamp(HDC, void*) { ++fake.globalCalls; return FALSE; }
BOOL SetDeviceGammaRamp(HDC, void*) { ++fake.globalCalls; return FALSE; }

int main() {
    const std::pair<const char*, void(*)()> tests[] = {
        {"Windowed hidden setup/order/cleanup", HiddenPreparationOrderingAndCleanup}, {"Windowed all brightness/mode/profile coefficients", EveryBrightnessModeAndProfile},
        {"Windowed invalid plans/options", InvalidPlansNeverInitialize}, {"Windowed hidden host/geometry gates", HiddenHostAndGeometryGates},
        {"Windowed exclusion bounds/membership", ExclusionBoundsAndCompleteMembership},
        {"Windowed every preparation failure", EveryNativePreparationFailure}, {"Windowed failed/ignored setters", FailedOrIgnoredSettersCannotConfirm},
        {"Windowed red-only read-back safety", UnsafeColorReadbackNeverPrepares}, {"Windowed spatial/exclusion read-backs", SourceTransformAndExclusionReadbacks},
        {"Windowed cleanup ownership/retry", CleanupFailureRetainsExactOwner}, {"Windowed causal/cleanup failure separation", ConfigurationAndCleanupFailuresStayDistinct},
        {"Windowed nested preparation cancellation", NestedCancellationCannotPublishPreparedState}, {"Windowed changes/allocation/API isolation", NativeChangesAndAllocationIsolation},
        {"Windowed cleanup reentry", CleanupReentryCannotDoubleRelease},
        {"Windowed existing owner/destruction limits", ExistingOwnerAndDestructionLimits}
    };
    unsigned failures = 0;
    for (const auto& [name, run] : tests) {
        Reset(); invalidDuringRun = false;
        try { run(); Check(!invalidDuringRun && !fake.badParameters && fake.errorReads == 0 && fake.globalCalls == 0, "fake native target/order/extended-error/global isolation violation"); std::printf("PASS %s\n", name); }
        catch (const std::exception& error) { ++failures; std::printf("FAIL %s: %s\n", name, error.what()); }
    }
    return failures ? 1 : 0;
}
