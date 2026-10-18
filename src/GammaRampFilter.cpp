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
      ownershipMutex_(NULL),
      ownershipAcquired_(false),
      ownershipAbandoned_(false),
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

    if ((hdc_ || ownershipMutex_) && !shutdown()) return false;
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

    if (needsRestore()) {
        SetError("enable while recovery pending", ERROR_INVALID_STATE);
        return false;
    }
    if (!AcquireOwnership()) return false;
    // Reacquire after topology changes and save the latest calibration on each enable.
    if (hdc_ && !ReleaseDC(NULL, hdc_)) {
        SetError("ReleaseDC before enable", GetLastError());
        ReleaseOwnership();
        return false;
    }
    hdc_ = GetDC(NULL);
    if (!hdc_ || !GetDeviceGammaRamp(hdc_, originalGammaRamp_)) {
        SetError("capture before enable", GetLastError());
        ReleaseOwnership();
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

    if (active_ || restorePending_) {
        if (!SetGammaRamp(originalGammaRamp_, "SetDeviceGammaRamp(restore)")) return false;
        active_ = false;
        restorePending_ = false;
        Diagnostics::LogFormat("%s display restoration succeeded.", kFilterName);
    }
    if (!ReleaseOwnership()) return false;
    lastError_[0] = '\0';
    return true;
}

bool GammaRampFilter::isActive() const {
    return active_;
}

bool GammaRampFilter::needsRestore() const { return active_ || restorePending_ || ownershipMutex_ != NULL; }

const char* GammaRampFilter::lastError() const { return lastError_; }

BackendVerification GammaRampFilter::verifyState() const {
    if (!initialized_) return BackendVerification::Unavailable;
    if (!active_ && !restorePending_) return BackendVerification::Inactive;
    WORD actual[3][256] = {};
    if (!hdc_ || !GetDeviceGammaRamp(hdc_, actual)) {
        Diagnostics::LogFormat("Gamma verification read failed (error %lu).", GetLastError());
        return BackendVerification::Unavailable;
    }
    const bool matches = RampMatches(actual, redGammaRamp_, true);
    Diagnostics::Log(matches ? "Gamma read-back matches requested ramp." :
        "Gamma read-back differs from requested ramp; no filter was reapplied.");
    return matches ? BackendVerification::Matches : BackendVerification::Changed;
}

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

bool GammaRampFilter::setLumaProfile(LumaProfile) {
    SetError("luma profiles require cross-channel mixing", ERROR_INVALID_PARAMETER);
    return false;
}

LumaProfile GammaRampFilter::lumaProfile() const { return LumaProfile::Classic; }

bool GammaRampFilter::supportsRedMode(RedMode mode) const {
    return mode == RedMode::StrictRed;
}

bool GammaRampFilter::shutdown() {
    if (needsRestore() && !disable()) {
        Diagnostics::Log("GammaRampFilter: recovery/ownership cleanup failed; retaining resources for retry.");
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

bool GammaRampFilter::AcquireOwnership() {
    if (ownershipAbandoned_) {
        SetError("gamma ownership was abandoned; recover display calibration before restarting RedLight", ERROR_ABANDONED_WAIT_0);
        return false;
    }
    if (ownershipAcquired_) return true; // Never recursively acquire the named mutex.
    ownershipMutex_ = CreateMutexW(NULL, FALSE, L"Global\\RedLight.GammaRampOwner.v1");
    if (!ownershipMutex_) {
        SetError("CreateMutex(gamma ownership; unavailable or access denied)", GetLastError());
        return false;
    }
    const DWORD result = WaitForSingleObject(ownershipMutex_, 0); // Never block the tray or wait out another session.
    if (result == WAIT_OBJECT_0) {
        ownershipAcquired_ = true;
        Diagnostics::Log("GammaRampFilter: acquired global gamma ownership before calibration capture.");
        return true;
    }
    if (result == WAIT_ABANDONED) {
        ownershipAcquired_ = true; // Windows grants ownership even on abandonment.
        ownershipAbandoned_ = true;
        SetError("abandoned gamma ownership; calibration recovery required before restarting RedLight", ERROR_ABANDONED_WAIT_0);
    } else if (result == WAIT_TIMEOUT) {
        SetError("gamma is owned by another RedLight session; restore it there first", ERROR_BUSY);
    } else {
        SetError("WaitForSingleObject(gamma ownership)", result == WAIT_FAILED ? GetLastError() : ERROR_INVALID_DATA);
    }
    ReleaseOwnership();
    return false;
}

bool GammaRampFilter::ReleaseOwnership() {
    if (ownershipAcquired_) {
        if (!ReleaseMutex(ownershipMutex_)) {
            SetError("ReleaseMutex(gamma ownership)", GetLastError());
            return false;
        }
        ownershipAcquired_ = false;
        Diagnostics::Log("GammaRampFilter: released global gamma ownership.");
    }
    if (ownershipMutex_) {
        if (!CloseHandle(ownershipMutex_)) {
            SetError("CloseHandle(gamma ownership)", GetLastError());
            return false;
        }
        ownershipMutex_ = NULL;
    }
    return true;
}

void GammaRampFilter::SetError(const char* action, DWORD error) {
    std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (GetLastError=%lu)\n",
        kFilterName,
        action,
        static_cast<unsigned long>(error));
    Diagnostics::LogFormat("%s: %s failed (GetLastError=%lu)", kFilterName, action, static_cast<unsigned long>(error));
}

bool GammaRampFilter::SetGammaRamp(WORD ramp[3][256], const char* action, bool requireZeroGreenBlue) {
    if (!ownershipAcquired_) {
        SetError("gamma write without exclusive ownership", ERROR_INVALID_STATE);
        return false;
    }
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
    if (!RampMatches(applied, ramp, requireZeroGreenBlue)) {
        SetError("gamma read-back mismatch (driver rejected ramp)", ERROR_INVALID_DATA);
        return false;
    }
    lastError_[0] = '\0';
    return true;
}

bool GammaRampFilter::RampMatches(const WORD applied[3][256], const WORD expected[3][256], bool requireZeroGreenBlue) {
    for (int channel = 0; channel < 3; ++channel) for (int i = 0; i < 256; ++i) {
        if ((requireZeroGreenBlue && channel > 0 && applied[channel][i] != 0) ||
            (applied[channel][i] >> 8) != (expected[channel][i] >> 8)) return false;
    }
    return true;
}

void GammaRampFilter::BuildRedRamp() {
    for (int i = 0; i < 256; ++i) {
        redGammaRamp_[0][i] = static_cast<WORD>(i * 257);
        redGammaRamp_[1][i] = 0;
        redGammaRamp_[2][i] = 0;
    }
}
