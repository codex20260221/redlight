#include "MagnificationFilter.h"
#include "Diagnostics.h"
#include "BackendComparison.h"
#include "BackendTiming.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kFilterName = "MagnificationFilter";

constexpr MAGCOLOREFFECT kIdentityEffect = {{
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
}};

MAGCOLOREFFECT ColorEffectForMode(RedMode mode, LumaProfile profile) {
    MAGCOLOREFFECT effect = {};
    effect.transform[3][3] = 1.0f;
    effect.transform[4][4] = 1.0f;
    if (mode == RedMode::StrictRed) {
        effect.transform[0][0] = 1.0f;
    } else {
        const auto weights = LumaProfileWeights(profile);
        // Input-channel rows, red output column. No green/blue output or black offset.
        for (int i = 0; i < 3; ++i) effect.transform[i][0] = weights[i];
    }
    return effect;
}

}  // namespace

MagnificationFilter::MagnificationFilter()
    : initialized_(false),
      active_(false),
      restorePending_(false),
      redMode_(RedMode::LumaRed),
      lumaProfile_(LumaProfile::Classic),
      restoreEffect_(kIdentityEffect),
      lastError_{} {}

MagnificationFilter::~MagnificationFilter() {
    shutdown();
}

bool MagnificationFilter::initialize() {
    RecordBackendOperation operation(statistics_.initialization, BackendMonotonicMilliseconds);
    if (initialized_) {
        return operation.success();
    }

    if (!MagInitialize()) {
        LogError("MagInitialize", GetLastError());
        Diagnostics::LogFormat("%s initialization failed.", kFilterName);
        return false;
    }

    initialized_ = true;
    MAGCOLOREFFECT capturedEffect = {};
    if (MagGetFullscreenColorEffect(&capturedEffect)) {
        if (!BackendComparison::FiniteMatrix(capturedEffect.transform)) {
            LogError("validate initial restore matrix (non-finite value)", ERROR_INVALID_DATA);
            if (!shutdown()) Diagnostics::Log("MagnificationFilter: initialization cleanup remains pending after invalid matrix capture.");
            return false;
        }
        restoreEffect_ = capturedEffect;
    } else {
        LogError("MagGetFullscreenColorEffect", GetLastError());
        Diagnostics::Log("MagnificationFilter: refusing to enable without a saved restore matrix.");
        shutdown();
        return false;
    }

    initialized_ = true;
    active_ = false;
    lastError_[0] = '\0';
    Diagnostics::LogFormat("%s initialization succeeded.", kFilterName);
    return operation.success();
}

bool MagnificationFilter::enable() {
    RecordBackendOperation operation(statistics_.activation, BackendMonotonicMilliseconds);
    if (!initialized_) {
        LogError("enable before initialize", ERROR_INVALID_HANDLE);
        return false;
    }

    if (active_) {
        return operation.success();
    }
    if (restorePending_) {
        LogError("enable while restoration is pending; restore the saved matrix first", ERROR_INVALID_STATE);
        return false;
    }

    // Capture immediately before each activation, not just at application startup.
    MAGCOLOREFFECT currentEffect = {};
    if (!MagGetFullscreenColorEffect(&currentEffect)) {
        LogError("capture before enable", GetLastError());
        return false;
    }
    if (!BackendComparison::FiniteMatrix(currentEffect.transform)) {
        LogError("validate restore matrix before enable (non-finite value)", ERROR_INVALID_DATA);
        return false;
    }
    restoreEffect_ = currentEffect;

    char action[128];
    sprintf_s(action, sizeof(action), "MagSetFullscreenColorEffect(%s)", RedModeDisplayName(redMode_));
    // A failed write is unconfirmed, rather than proof that display state was unchanged.
    restorePending_ = true;
    if (!SetColorEffect(ColorEffectForMode(redMode_, lumaProfile_), action)) {
        char applyError[sizeof(lastError_)];
        std::memcpy(applyError, lastError_, sizeof(applyError));
        if (disable()) {
            std::memcpy(lastError_, applyError, sizeof(lastError_));
            Diagnostics::Log("MagnificationFilter: failed activation rolled back to the captured matrix; filter remains off.");
        } else Diagnostics::Log("MagnificationFilter: activation and rollback failed; saved matrix and API resources retained for restoration retry.");
        return false;
    }

    active_ = true;
    restorePending_ = false;
    Diagnostics::LogFormat("%s enable succeeded using %s.", kFilterName, RedModeDisplayName(redMode_));
    return operation.success();
}

bool MagnificationFilter::disable() {
    RecordBackendOperation operation(statistics_.restoration, BackendMonotonicMilliseconds);
    if (!initialized_) {
        LogError("disable before initialize", ERROR_INVALID_HANDLE);
        return false;
    }

    if (!needsRestore()) {
        return operation.success();
    }

    if (!SetColorEffect(restoreEffect_, "MagSetFullscreenColorEffect(restore)")) {
        return false;
    }

    active_ = false;
    restorePending_ = false;
    Diagnostics::LogFormat("%s disable succeeded.", kFilterName);
    return operation.success();
}

bool MagnificationFilter::isActive() const {
    return active_;
}

bool MagnificationFilter::needsRestore() const { return active_ || restorePending_; }

const char* MagnificationFilter::lastError() const { return lastError_; }

BackendVerificationReport MagnificationFilter::inspectState() const {
    RecordBackendOperation operation(statistics_.verification, BackendMonotonicMilliseconds);
    if (!initialized_) return {BackendVerification::Unavailable, std::nullopt, ERROR_INVALID_HANDLE};
    if (!needsRestore()) { operation.success(); return {BackendVerification::Inactive}; }
    MAGCOLOREFFECT actual = {};
    if (!MagGetFullscreenColorEffect(&actual)) {
        const auto error = static_cast<std::uint32_t>(GetLastError());
        Diagnostics::LogFormat("Magnification verification read failed (error %lu).", static_cast<unsigned long>(error));
        return {BackendVerification::Unavailable, std::nullopt, error};
    }
    const auto expected = ColorEffectForMode(redMode_, lumaProfile_);
    const auto details = BackendComparison::Matrix(actual.transform, expected.transform);
    Diagnostics::LogFormat("Magnification verification: %u/%u mismatches, maximum finite coefficient difference %.9g, nonfinite=%s, unexpected G/B=%s; no filter reapplied.",
        details.mismatches, details.values, details.maximumDifference, details.nonFiniteValues ? "yes" : "no", details.unexpectedGreenBlue ? "yes" : "no");
    if (details.mismatches == 0) operation.success();
    return {details.mismatches == 0 ? BackendVerification::Matches : BackendVerification::Changed, details};
}

bool MagnificationFilter::setRedMode(RedMode mode) {
    if (!supportsRedMode(mode)) {
        LogError("unsupported red mode", ERROR_INVALID_PARAMETER);
        return false;
    }
    if (mode == redMode_) {
        return true;
    }
    if (restorePending_ && !active_) {
        LogError("change red mode while restoration is pending", ERROR_INVALID_STATE);
        return false;
    }

    if (active_) {
        char action[128];
        sprintf_s(action, sizeof(action), "MagSetFullscreenColorEffect(%s)", RedModeDisplayName(mode));
        if (!SetLiveColorEffect(ColorEffectForMode(mode, lumaProfile_), action)) {
            return false;
        }
    }

    redMode_ = mode;
    Diagnostics::LogFormat("%s red mode set to %s.", kFilterName, RedModeDisplayName(redMode_));
    return true;
}

RedMode MagnificationFilter::redMode() const {
    return redMode_;
}

bool MagnificationFilter::setLumaProfile(LumaProfile profile) {
    if (!IsValidLumaProfile(static_cast<unsigned>(profile))) {
        LogError("unsupported luma profile", ERROR_INVALID_PARAMETER);
        return false;
    }
    if (profile == lumaProfile_) return true;
    if (restorePending_ && !active_) {
        LogError("change luma profile while restoration is pending", ERROR_INVALID_STATE);
        return false;
    }
    if (active_ && redMode_ == RedMode::LumaRed &&
        !SetLiveColorEffect(ColorEffectForMode(redMode_, profile), "change luma profile")) return false;
    lumaProfile_ = profile;
    Diagnostics::LogFormat("MagnificationFilter luma profile: %s.", LumaProfileName(profile));
    return true;
}

LumaProfile MagnificationFilter::lumaProfile() const { return lumaProfile_; }

bool MagnificationFilter::supportsRedMode(RedMode mode) const {
    return mode == RedMode::StrictRed || mode == RedMode::LumaRed;
}

bool MagnificationFilter::shutdown() {
    RecordBackendOperation operation(statistics_.shutdown, BackendMonotonicMilliseconds);
    if (!initialized_) {
        return operation.success();
    }
    if (needsRestore() && !disable()) {
        Diagnostics::Log("MagnificationFilter: restore failed; retaining recovery state.");
        return false;
    }
    if (!MagUninitialize()) {
        LogError("MagUninitialize", GetLastError());
        return false;
    }
    initialized_ = false;
    Diagnostics::LogFormat("%s shutdown completed.", kFilterName);
    return operation.success();
}

const char* MagnificationFilter::name() const {
    return kFilterName;
}

void MagnificationFilter::LogError(const char* action, DWORD error) {
    std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (GetLastError=%lu)\n",
        kFilterName,
        action,
        static_cast<unsigned long>(error));
    Diagnostics::LogFormat("%s: %s failed (GetLastError=%lu)", kFilterName, action, static_cast<unsigned long>(error));
}

bool MagnificationFilter::SetColorEffect(const MAGCOLOREFFECT& effect, const char* action) {
    if (!initialized_) {
        LogError(action, ERROR_INVALID_HANDLE);
        return false;
    }

    MAGCOLOREFFECT mutableEffect = effect;
    if (!MagSetFullscreenColorEffect(&mutableEffect)) {
        LogError(action, GetLastError());
        return false;
    }

    lastError_[0] = '\0';
    return true;
}

bool MagnificationFilter::SetLiveColorEffect(const MAGCOLOREFFECT& effect, const char* action) {
    if (SetColorEffect(effect, action)) return true;
    char applyError[sizeof(lastError_)];
    std::memcpy(applyError, lastError_, sizeof(applyError));
    active_ = false;
    restorePending_ = true;
    if (SetColorEffect(ColorEffectForMode(redMode_, lumaProfile_), "rollback confirmed red transform")) {
        active_ = true;
        restorePending_ = false;
        std::memcpy(lastError_, applyError, sizeof(lastError_));
        Diagnostics::Log("MagnificationFilter: failed live change rolled back to the prior confirmed red transform; preference and original snapshot retained.");
    } else Diagnostics::Log("MagnificationFilter: live change and rollback failed; inactive recovery retains the original display snapshot.");
    return false;
}
