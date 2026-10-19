#include "WindowedRedControl.h"
#include "BackendComparison.h"
#include <algorithm>
#include <array>

namespace MonitorPrototype {
namespace {
constexpr std::size_t MaxExclusions = 16;
bool Same(const RECT& a, const RECT& b) noexcept {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
bool Hidden(HWND window) noexcept { return window && IsWindow(window) && !IsWindowVisible(window); }
MAGCOLOREFFECT Effect(RedMode mode, LumaProfile profile, unsigned brightness) noexcept {
    MAGCOLOREFFECT effect = {};
    effect.transform[3][3] = effect.transform[4][4] = 1;
    const float scale = static_cast<float>(brightness) / 100.0f;
    if (mode == RedMode::StrictRed) effect.transform[0][0] = scale;
    else {
        const auto weights = LumaProfileWeights(profile);
        for (unsigned index = 0; index < 3; ++index) effect.transform[index][0] = weights[index] * scale;
    }
    return effect;
}
}

WindowedRedControl::~WindowedRedControl() {
    if (!shutdown()) OutputDebugStringA("WindowedRedControl destroyed with incomplete child/runtime cleanup; retain the owner for retry before destruction.\n");
}

bool WindowedRedControl::checked(bool success, ControlStage stage) noexcept {
    // These Magnification control APIs document success/failure, not an extended
    // GetLastError contract. Report the failed stage without inventing a cause.
    if (canceled_) { failure_ = ControlStage::Canceled; return false; }
    if (!success) { failure_ = stage; return false; }
    return true;
}

bool WindowedRedControl::prepare(HWND host, HINSTANCE instance, std::span<const Region> regions,
    const Context& context, std::span<const HWND> exclusions, RedMode mode, LumaProfile profile, unsigned brightness) noexcept {
    if (ownsResources() || preparing_) { failure_ = ControlStage::AlreadyOwned; return false; }
    failure_ = cleanupFailure_ = ControlStage::None;
    std::array<Slot, 1> slots;
    const auto plan = Preflight(regions, context, slots);
    if (!instance || plan.result != Result::Subset || plan.count != 1 ||
        (mode != RedMode::StrictRed && mode != RedMode::LumaRed) || !IsValidLumaProfile(static_cast<unsigned>(profile)) ||
        !ValidRedBrightness(brightness) || exclusions.empty() || exclusions.size() > MaxExclusions ||
        std::find(exclusions.begin(), exclusions.end(), host) == exclusions.end()) {
        failure_ = ControlStage::InvalidRequest; return false;
    }
    for (auto at = exclusions.begin(); at != exclusions.end(); ++at) {
        if (!*at || std::find(exclusions.begin(), at, *at) != at) { failure_ = ControlStage::InvalidRequest; return false; }
    }
    canceled_ = false; preparing_ = true;
    const bool configured = configure(host, instance, slots[0], exclusions, mode, profile, brightness);
    preparing_ = false;
    if (!configured || canceled_) { shutdown(); return false; }
    failure_ = ControlStage::None;
    prepared_ = true;
    return true;
}

bool WindowedRedControl::configure(HWND host, HINSTANCE instance, const Slot& slot, std::span<const HWND> exclusions,
    RedMode mode, LumaProfile profile, unsigned brightness) noexcept {
    for (const auto excluded : exclusions) if (!checked(Hidden(excluded), ControlStage::HostNotHidden)) return false;
    const RECT source{slot.source.left, slot.source.top, slot.source.right, slot.source.bottom};
    RECT bounds = {}, client = {};
    if (!checked(GetWindowRect(host, &bounds) && GetClientRect(host, &client) && Same(bounds, source) &&
        client.left == 0 && client.top == 0 && client.right == slot.width && client.bottom == slot.height, ControlStage::HostGeometry)) return false;
    runtime_ = MagInitialize() != FALSE;
    if (!checked(runtime_, ControlStage::Initialize)) return false;
    child_ = CreateWindowExW(0, WC_MAGNIFIERW, L"RedLight isolated control", WS_CHILD,
        0, 0, slot.width, slot.height, host, nullptr, instance, nullptr); // Deliberately no WS_VISIBLE.
    if (!checked(child_ != nullptr, ControlStage::Create)) return false;
    auto effect = Effect(mode, profile, brightness);
    if (!checked(MagSetColorEffect(child_, &effect) != FALSE, ControlStage::ColorWrite)) return false;
    std::array<HWND, MaxExclusions> requested = {};
    std::copy(exclusions.begin(), exclusions.end(), requested.begin());
    const int count = static_cast<int>(exclusions.size());
    if (!checked(MagSetWindowFilterList(child_, MW_FILTERMODE_EXCLUDE, count, requested.data()) != FALSE, ControlStage::ExclusionsWrite)) return false;
    MAGTRANSFORM transform = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    if (!checked(MagSetWindowTransform(child_, &transform) != FALSE, ControlStage::TransformWrite) ||
        !checked(MagSetWindowSource(child_, source) != FALSE, ControlStage::SourceWrite)) return false;
    MAGCOLOREFFECT actual = {};
    if (!checked(MagGetColorEffect(child_, &actual) != FALSE, ControlStage::ColorRead) ||
        !checked(BackendComparison::Matrix(actual.transform, effect.transform, BackendComparison::MatrixPurpose::RedOnly).mismatches == 0, ControlStage::ColorMismatch)) return false;
    DWORD filterMode = 0;
    const int reported = MagGetWindowFilterList(child_, &filterMode, 0, nullptr);
    if (!checked(reported >= 0, ControlStage::ExclusionsRead) ||
        !checked(reported == count && filterMode == MW_FILTERMODE_EXCLUDE, ControlStage::ExclusionsMismatch)) return false;
    std::array<HWND, MaxExclusions> found = {};
    const int received = MagGetWindowFilterList(child_, &filterMode, count, found.data());
    if (!checked(received >= 0, ControlStage::ExclusionsRead) ||
        !checked(received == count && filterMode == MW_FILTERMODE_EXCLUDE, ControlStage::ExclusionsMismatch)) return false;
    for (int index = 0; index < count; ++index) {
        const auto at = found.begin() + index;
        if (!checked(std::find(exclusions.begin(), exclusions.end(), *at) != exclusions.end() && std::find(found.begin(), at, *at) == at,
            ControlStage::ExclusionsMismatch)) return false;
    }
    MAGTRANSFORM actualTransform = {};
    if (!checked(MagGetWindowTransform(child_, &actualTransform) != FALSE, ControlStage::TransformRead)) return false;
    for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
        if (!checked(actualTransform.v[row][column] == transform.v[row][column], ControlStage::TransformMismatch)) return false;
    RECT actualSource = {};
    if (!checked(MagGetWindowSource(child_, &actualSource) != FALSE, ControlStage::SourceRead) ||
        !checked(Same(actualSource, source), ControlStage::SourceMismatch)) return false;
    for (const auto excluded : exclusions) if (!checked(Hidden(excluded), ControlStage::HostNotHidden)) return false;
    if (!checked(GetWindowRect(host, &bounds) && GetClientRect(host, &client) && Same(bounds, source) &&
        client.left == 0 && client.top == 0 && client.right == slot.width && client.bottom == slot.height, ControlStage::HostGeometry)) return false;
    // IsWindowVisible alone also returns false for a visible-style child under
    // a hidden parent. Check our child's own style before returning it.
    const auto style = GetWindowLongPtrW(child_, GWL_STYLE);
    return checked(Hidden(child_) && GetParent(child_) == host && (style & WS_CHILD) && !(style & WS_VISIBLE), ControlStage::HostNotHidden);
}

bool WindowedRedControl::shutdown() noexcept {
    prepared_ = false;
    if (preparing_) { canceled_ = true; return false; } // The outer preparation performs cleanup after the native call returns.
    if (stopping_) return false; // DestroyWindow may synchronously notify the borrowed parent.
    stopping_ = true;
    struct ResetFlag { bool& flag; ~ResetFlag() { flag = false; } } reset{stopping_};
    if (child_) {
        if (!DestroyWindow(child_)) { cleanupFailure_ = ControlStage::Destroy; return false; }
        child_ = nullptr;
    }
    if (runtime_) {
        if (!MagUninitialize()) { cleanupFailure_ = ControlStage::Uninitialize; return false; }
        runtime_ = false;
    }
    cleanupFailure_ = ControlStage::None;
    return true;
}
}
