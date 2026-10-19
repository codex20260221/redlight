#pragma once
#include <windows.h>
#include <magnification.h>
#include "MonitorPlan.h"
#include "DisplayFilter.h"
#include <span>

// Research component only: no production caller and no executable entry point.
// The future probe owns the host, topology/DPI proof, input/close controls and
// deadline. Keep the host hidden during preparation and alive until shutdown
// succeeds. Its owner must check visibility/opacity and attempt hiding before cleanup. This object
// owns only its child and its successful MagInitialize reference, on one thread.
namespace MonitorPrototype {
enum class ControlStage {
    None, InvalidRequest, AlreadyOwned, HostNotHidden, HostGeometry, Initialize,
    Create, ColorWrite, ExclusionsWrite, TransformWrite, SourceWrite,
    ColorRead, ColorMismatch, ExclusionsRead, ExclusionsMismatch,
    TransformRead, TransformMismatch, SourceRead, SourceMismatch, Canceled,
    Destroy, Uninitialize
};

class WindowedRedControl {
public:
    WindowedRedControl() = default;
    ~WindowedRedControl();
    WindowedRedControl(const WindowedRedControl&) = delete;
    WindowedRedControl& operator=(const WindowedRedControl&) = delete;
    WindowedRedControl(WindowedRedControl&&) = delete;
    WindowedRedControl& operator=(WindowedRedControl&&) = delete;

    bool prepare(HWND hiddenHost, HINSTANCE instance, std::span<const Region> regions,
        const Context& context, std::span<const HWND> hiddenExcludedHosts,
        RedMode mode, LumaProfile profile, unsigned brightness) noexcept;
    bool shutdown() noexcept;
    bool prepared() const noexcept { return prepared_; }
    bool ownsResources() const noexcept { return runtime_ || child_; }
    HWND child() const noexcept { return child_; }
    ControlStage failure() const noexcept { return failure_; }
    ControlStage cleanupFailure() const noexcept { return cleanupFailure_; }
private:
    bool configure(HWND host, HINSTANCE instance, const Slot& slot, std::span<const HWND> exclusions,
        RedMode mode, LumaProfile profile, unsigned brightness) noexcept;
    bool checked(bool success, ControlStage stage) noexcept;
    HWND child_ = nullptr;
    bool runtime_ = false, preparing_ = false, stopping_ = false, canceled_ = false, prepared_ = false;
    ControlStage failure_ = ControlStage::None, cleanupFailure_ = ControlStage::None;
};
}
