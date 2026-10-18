#pragma once
#include "DisplayFilter.h"
#include "UiSafety.h"

enum class LoopShutdownDisposition { ExitSuccess, ExitFailure, ResumeControls };

// Keep the static backend owner and live controls after any incomplete shutdown,
// including a queue error. Missing/destroyed controls retain best-effort failure exit.
template<class ControlsAlive>
LoopShutdownDisposition TryLoopShutdown(DisplayFilter& filter, int messageResult, bool& busy, ControlsAlive&& controlsAlive) {
    if (busy) return controlsAlive() ? LoopShutdownDisposition::ResumeControls : LoopShutdownDisposition::ExitFailure;
    bool completed = false;
    {
        ScopedFlag operation(busy);
        completed = filter.shutdown();
    }
    if (!completed && controlsAlive()) return LoopShutdownDisposition::ResumeControls;
    return completed && messageResult == 0 ? LoopShutdownDisposition::ExitSuccess : LoopShutdownDisposition::ExitFailure;
}
