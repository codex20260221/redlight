#include "GammaRampFilter.h"
#include "Diagnostics.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kFilterName = "GammaRampFilter";

}

GammaRampFilter::GammaRampFilter()
    : hdc_(NULL),
      initialized_(false),
      active_(false),
      restorePending_(false),
      originalGammaRamp_{},
      redGammaRamp_{},
      lastError_{} {}

GammaRampFilter::~GammaRampFilter() {
    shutdown();
}

bool GammaRampFilter::initialize() {
    if (initialized_) {
        return true;
    }

    if (hdc_ && !shutdown()) return false;
    hdc_ = GetDC(NULL);
    if (!hdc_) {
        SetError("GetDC", GetLastError());
        Diagnostics::LogFormat("%s initialization failed.", kFilterName);
        return false;
    }

    if (!GetDeviceGammaRamp(hdc_, originalGammaRamp_)) {
        SetError("GetDeviceGammaRamp", GetLastError());
        if (!ReleaseDC(NULL, hdc_)) {
            SetError("ReleaseDC after capture failure", GetLastError());
        } else {
            hdc_ = NULL;
        }
        Diagnostics::LogFormat("%s initialization failed.", kFilterName);
        return false;
    }

    BuildRedRamp();
    initialized_ = true;
    active_ = false;
    lastError_[0] = '\0';
    Diagnostics::LogFormat("%s initialization succeeded.", kFilterName);
    return true;
}

bool GammaRampFilter::enable() {
    if (!initialized_) {
        SetError("enable before initialize", ERROR_INVALID_HANDLE);
        return false;
    }

    if (active_) {
        return true;
    }

    if (restorePending_) {
        SetError("enable while recovery pending", ERROR_INVALID_STATE);
        return false;
    }
    // Reacquire after topology changes and save the latest calibration on each enable.
    if (hdc_ && !ReleaseDC(NULL, hdc_)) {
        SetError("ReleaseDC before enable", GetLastError());
        return false;
    }
    hdc_ = GetDC(NULL);
    if (!hdc_ || !GetDeviceGammaRamp(hdc_, originalGammaRamp_)) {
        SetError("capture before enable", GetLastError());
        return false;
    }
    restorePending_ = true;
    if (!SetGammaRamp(redGammaRamp_, "SetDeviceGammaRamp(red)", true)) {
        char enableError[sizeof(lastError_)];
        std::memcpy(enableError, lastError_, sizeof(enableError));
        if (disable()) {
            std::memcpy(lastError_, enableError, sizeof(lastError_));
        }
        return false;
    }

    active_ = true;
    Diagnostics::LogFormat("%s enable succeeded.", kFilterName);
    return true;
}

bool GammaRampFilter::disable() {
    if (!initialized_) {
        SetError("disable before initialize", ERROR_INVALID_HANDLE);
        return false;
    }

    if (!needsRestore()) {
        return true;
    }

    if (!SetGammaRamp(originalGammaRamp_, "SetDeviceGammaRamp(restore)")) {
        return false;
    }

    active_ = false;
    restorePending_ = false;
    Diagnostics::LogFormat("%s disable succeeded.", kFilterName);
    return true;
}

bool GammaRampFilter::isActive() const {
    return active_;
}

bool GammaRampFilter::needsRestore() const { return active_ || restorePending_; }

const char* GammaRampFilter::lastError() const { return lastError_; }

bool GammaRampFilter::setRedMode(RedMode mode) {
    if (mode == RedMode::StrictRed) {
        return true;
    }

    SetError("unsupported mode (gamma supports Strict Red only)", ERROR_INVALID_PARAMETER);
    return false;
}

RedMode GammaRampFilter::redMode() const {
    return RedMode::StrictRed;
}

bool GammaRampFilter::supportsRedMode(RedMode mode) const {
    return mode == RedMode::StrictRed;
}

bool GammaRampFilter::shutdown() {
    if (needsRestore() && !disable()) {
        Diagnostics::Log("GammaRampFilter: restore failed; retaining DC and recovery state.");
        return false;
    }
    if (hdc_ && !ReleaseDC(NULL, hdc_)) {
        SetError("ReleaseDC", GetLastError());
        return false;
    }
    hdc_ = NULL;
    initialized_ = false;
    return true;
}

const char* GammaRampFilter::name() const {
    return kFilterName;
}

void GammaRampFilter::SetError(const char* action, DWORD error) {
    std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (GetLastError=%lu)\n",
        kFilterName,
        action,
        static_cast<unsigned long>(error));
    Diagnostics::LogFormat("%s: %s failed (GetLastError=%lu)", kFilterName, action, static_cast<unsigned long>(error));
}

bool GammaRampFilter::SetGammaRamp(WORD ramp[3][256], const char* action, bool requireZeroGreenBlue) {
    if (!hdc_) {
        SetError(action, ERROR_INVALID_HANDLE);
        return false;
    }

    if (!SetDeviceGammaRamp(hdc_, ramp)) {
        SetError(action, GetLastError());
        return false;
    }

    // Drivers can silently ignore a successful SetDeviceGammaRamp call.
    WORD applied[3][256] = {};
    if (!GetDeviceGammaRamp(hdc_, applied)) {
        SetError("gamma read-back", GetLastError());
        return false;
    }
    for (int channel = 0; channel < 3; ++channel) {
        for (int i = 0; i < 256; ++i) {
            // DAC precision varies; compare the significant byte.
            if ((requireZeroGreenBlue && channel > 0 && applied[channel][i] != 0) ||
                (applied[channel][i] >> 8) != (ramp[channel][i] >> 8)) {
                SetError("gamma read-back mismatch (driver rejected ramp)", ERROR_INVALID_DATA);
                return false;
            }
        }
    }
    lastError_[0] = '\0';
    return true;
}

void GammaRampFilter::BuildRedRamp() {
    for (int i = 0; i < 256; ++i) {
        redGammaRamp_[0][i] = static_cast<WORD>(i * 257);
        redGammaRamp_[1][i] = 0;
        redGammaRamp_[2][i] = 0;
    }
}
