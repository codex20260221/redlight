#include "GammaRampFilter.h"
#include "Diagnostics.h"
#include "BackendComparison.h"
#include "BackendTiming.h"

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
    RecordBackendOperation operation(statistics_.initialization, BackendMonotonicMilliseconds);
    if (initialized_) {
        return operation.success();
    }

    if ((hdc_ || ownershipMutex_) && !shutdown()) return false;
    SetLastError(ERROR_SUCCESS);
    hdc_ = GetDC(NULL);
    if (!hdc_) {
        SetError("GetDC", GetLastError());
        Diagnostics::LogFormat("%s initialization failed.", kFilterName);
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    if (!GetDeviceGammaRamp(hdc_, originalGammaRamp_)) {
        SetError("GetDeviceGammaRamp", GetLastError());
        SetLastError(ERROR_SUCCESS);
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
    return operation.success();
}

bool GammaRampFilter::enable() {
    RecordBackendOperation operation(statistics_.activation, BackendMonotonicMilliseconds);
    if (!initialized_) {
        SetError("enable before initialize", ERROR_INVALID_HANDLE);
        return false;
    }

    if (active_) {
        return operation.success();
    }

    if (needsRestore()) {
        SetError("enable while recovery pending", ERROR_INVALID_STATE);
        return false;
    }
    if (!AcquireOwnership()) return false;
    // Reacquire after topology changes and save the latest calibration on each enable.
    if (hdc_) {
        SetLastError(ERROR_SUCCESS);
        if (!ReleaseDC(NULL, hdc_)) {
            SetError("ReleaseDC before enable", GetLastError());
            ReleaseOwnership();
            return false;
        }
        hdc_ = NULL;
    }
    SetLastError(ERROR_SUCCESS);
    hdc_ = GetDC(NULL);
    if (!hdc_) {
        SetError("GetDC before enable", GetLastError());
        ReleaseOwnership();
        return false;
    }
    SetLastError(ERROR_SUCCESS);
    if (!GetDeviceGammaRamp(hdc_, originalGammaRamp_)) {
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
    return operation.success();
}

bool GammaRampFilter::disable() {
    RecordBackendOperation operation(statistics_.restoration, BackendMonotonicMilliseconds);
    if (!initialized_) {
        SetError("disable before initialize", ERROR_INVALID_HANDLE);
        return false;
    }

    if (active_ || restorePending_) {
        // Even a failed write can change the ramp; red output is no longer confirmed.
        active_ = false;
        restorePending_ = true;
        if (!SetGammaRamp(originalGammaRamp_, "SetDeviceGammaRamp(restore)")) return false;
        restorePending_ = false;
        Diagnostics::LogFormat("%s display restoration succeeded.", kFilterName);
    }
    if (!ReleaseOwnership()) return false;
    lastError_[0] = '\0';
    return operation.success();
}

bool GammaRampFilter::isActive() const {
    return active_;
}

bool GammaRampFilter::needsRestore() const { return active_ || restorePending_ || ownershipMutex_ != NULL; }

const char* GammaRampFilter::lastError() const { return lastError_; }

const char* GammaRampFilter::lifecycleDescription() const noexcept {
    if (active_) return "Active; red output confirmed at last operation";
    if (restorePending_) return "Restoration pending; red output unconfirmed";
    if (ownershipMutex_) {
        if (ownershipAbandoned_) return "Activation blocked; abandoned ownership cleanup pending";
        return ownershipAcquired_ ? "Filter off; ownership release pending" : "Filter off; ownership handle cleanup pending";
    }
    if (ownershipAbandoned_) return initialized_ ? "Ready/off; activation blocked after abandoned ownership" : "Released; activation blocked after abandoned ownership";
    if (initialized_) return "Ready; filter off";
    if (hdc_) return "Device context retained; initialization incomplete or cleanup pending";
    return "Released; backend not initialized";
}

BackendVerificationReport GammaRampFilter::inspectState() const {
    RecordBackendOperation operation(statistics_.verification, BackendMonotonicMilliseconds);
    if (!initialized_) return {BackendVerification::Unavailable, std::nullopt, ERROR_INVALID_HANDLE};
    if (!active_ && !restorePending_) { operation.success(); return {BackendVerification::Inactive}; }
    WORD actual[3][256] = {};
    SetLastError(ERROR_SUCCESS);
    if (!hdc_ || !GetDeviceGammaRamp(hdc_, actual)) {
        const auto error = static_cast<std::uint32_t>(hdc_ ? GetLastError() : ERROR_INVALID_HANDLE);
        if (error) Diagnostics::LogFormat("Gamma verification read failed (error %lu).", static_cast<unsigned long>(error));
        else Diagnostics::Log("Gamma verification read failed (no native error code).");
        return {BackendVerification::Unavailable, std::nullopt, error};
    }
    const auto details = BackendComparison::Gamma(actual, redGammaRamp_, true);
    Diagnostics::LogFormat("Gamma verification: %u/%u mismatches, maximum full-scale difference %.9g, unexpected G/B=%s, red black/white endpoint mismatch=%s/%s; no filter reapplied.",
        details.mismatches, details.values, details.maximumDifference, details.unexpectedGreenBlue ? "yes" : "no",
        details.redBlackEndpointMismatch ? "yes" : "no", details.redWhiteEndpointMismatch ? "yes" : "no");
    if (details.mismatches == 0) operation.success();
    return {details.mismatches == 0 ? BackendVerification::Matches : BackendVerification::Changed, details};
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

bool GammaRampFilter::setBrightnessPercent(unsigned percent) {
    if (percent == DefaultRedBrightness) return true; // No write or cleanup, including pending recovery.
    SetError("adjustable brightness requires Magnification; gamma stays at 100%", ERROR_INVALID_PARAMETER);
    return false;
}

bool GammaRampFilter::supportsRedMode(RedMode mode) const {
    return mode == RedMode::StrictRed;
}

bool GammaRampFilter::shutdown() {
    RecordBackendOperation operation(statistics_.shutdown, BackendMonotonicMilliseconds);
    if (needsRestore() && !disable()) {
        Diagnostics::Log("GammaRampFilter: recovery/ownership cleanup failed; retaining resources for retry.");
        return false;
    }
    if (hdc_) {
        SetLastError(ERROR_SUCCESS);
        if (!ReleaseDC(NULL, hdc_)) {
            SetError("ReleaseDC", GetLastError());
            return false;
        }
    }
    hdc_ = NULL;
    initialized_ = false;
    return operation.success();
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
    if (error) {
        std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (GetLastError=%lu)\n",
            kFilterName, action, static_cast<unsigned long>(error));
        Diagnostics::LogFormat("%s: %s failed (GetLastError=%lu)", kFilterName, action, static_cast<unsigned long>(error));
    } else {
        std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (no native error code)\n", kFilterName, action);
        Diagnostics::LogFormat("%s: %s failed (no native error code)", kFilterName, action);
    }
}

bool GammaRampFilter::SetGammaRamp(WORD (&ramp)[3][256], const char* action, bool requireStrictRed) {
    if (!ownershipAcquired_) {
        SetError("gamma write without exclusive ownership", ERROR_INVALID_STATE);
        return false;
    }
    if (!hdc_) {
        SetError(action, ERROR_INVALID_HANDLE);
        return false;
    }

    // These GDI APIs do not promise extended error information. Never report a
    // previous call's error when the driver supplies no error code of its own.
    SetLastError(ERROR_SUCCESS);
    if (!SetDeviceGammaRamp(hdc_, ramp)) {
        SetError(action, GetLastError());
        return false;
    }

    // Drivers can silently ignore a successful SetDeviceGammaRamp call.
    WORD applied[3][256] = {};
    SetLastError(ERROR_SUCCESS);
    if (!GetDeviceGammaRamp(hdc_, applied)) {
        SetError("gamma read-back", GetLastError());
        return false;
    }
    const auto comparison = BackendComparison::Gamma(applied, ramp, requireStrictRed);
    if (comparison.mismatches != 0) {
        Diagnostics::LogFormat("Gamma write confirmation mismatch: %u/%u entries, red black/white endpoints=%s/%s, unexpected G/B=%s.",
            comparison.mismatches, comparison.values, comparison.redBlackEndpointMismatch ? "changed" : "match",
            comparison.redWhiteEndpointMismatch ? "changed" : "match", comparison.unexpectedGreenBlue ? "yes" : "no");
        SetError("gamma read-back mismatch (unconfirmed ramp)", ERROR_INVALID_DATA);
        return false;
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
