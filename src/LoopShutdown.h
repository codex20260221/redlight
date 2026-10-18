#pragma once
#include "DisplayFilter.h"
#include "UiSafety.h"

enum class LoopShutdownDisposition { ExitSuccess, ExitFailure, ResumeControls };

// An ordinary quit with live controls can recover from failed shutdown. Native
// message-loop errors and missing/destroyed controls keep their failure exit path.
template<class ControlsAlive>
LoopShutdownDisposition TryLoopShutdown(DisplayFilter& filter, int messageResult, bool& busy, ControlsAlive&& controlsAlive) {
    if (busy) return messageResult == 0 && controlsAlive() ? LoopShutdownDisposition::ResumeControls : LoopShutdownDisposition::ExitFailure;
    bool completed = false;
    {
        ScopedFlag operation(busy);
        completed = filter.shutdown();
    }
    if (!completed && messageResult == 0 && controlsAlive()) return LoopShutdownDisposition::ResumeControls;
    return completed && messageResult == 0 ? LoopShutdownDisposition::ExitSuccess : LoopShutdownDisposition::ExitFailure;
}
