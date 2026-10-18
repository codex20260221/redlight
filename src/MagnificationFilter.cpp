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

MAGCOLOREFFECT ColorEffectForMode(RedMode mode, LumaProfile profile, unsigned brightness) {
    MAGCOLOREFFECT effect = {};
    effect.transform[3][3] = 1.0f;
    effect.transform[4][4] = 1.0f;
    if (mode == RedMode::StrictRed) {
        effect.transform[0][0] = static_cast<float>(brightness) / 100.0f;
    } else {
        const auto weights = LumaProfileWeights(profile);
        // Input-channel rows, red output column. No green/blue output or black offset.
        const float scale = static_cast<float>(brightness) / 100.0f;
        for (int i = 0; i < 3; ++i) effect.transform[i][0] = weights[i] * scale;
    }
    return effect;
}

}  // namespace

MagnificationFilter::MagnificationFilter()
    : initialized_(false),
      runtimeInitialized_(false),
      active_(false),
      restorePending_(false),
      redMode_(RedMode::LumaRed),
      lumaProfile_(LumaProfile::Classic),
      restoreEffect_(kIdentityEffect),
      lastError_{} {}

MagnificationFilter::~MagnificationFilter() {
    if (!shutdown()) Diagnostics::Log("MagnificationFilter destroyed with incomplete recovery/cleanup. The owner must retain the object for retry before destruction; process termination cannot preserve its snapshot.");
}

bool MagnificationFilter::initialize() {
    RecordBackendOperation operation(statistics_.initialization, BackendMonotonicMilliseconds);
    if (initialized_) {
        return operation.success();
    }

    // A failed initial capture may still own API resources after failed cleanup.
    // Finish that cleanup before a fresh initialization; ownership is not readiness.
    if (runtimeInitialized_ && !shutdown()) return false;
    SetLastError(ERROR_SUCCESS);
    if (!MagInitialize()) {
        LogError("MagInitialize", GetLastError());
        Diagnostics::LogFormat("%s initialization failed.", kFilterName);
        return false;
    }

    runtimeInitialized_ = true;
    MAGCOLOREFFECT capturedEffect = {};
    SetLastError(ERROR_SUCCESS);
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
        if (!shutdown()) Diagnostics::Log("MagnificationFilter: initialization cleanup remains pending after failed matrix capture.");
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
    SetLastError(ERROR_SUCCESS);
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
    if (!SetColorEffect(ColorEffectForMode(redMode_, lumaProfile_, brightnessPercent_), action, BackendComparison::MatrixPurpose::RedOnly)) {
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
    Diagnostics::LogFormat("%s enable succeeded using %s at %u%% red brightness.", kFilterName, RedModeDisplayName(redMode_), brightnessPercent_);
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

    // Any restoration attempt can change output even when its API reports failure.
    // Drop red-output confirmation before the write and block live edits until retry.
    active_ = false;
    restorePending_ = true;
    if (!SetColorEffect(restoreEffect_, "MagSetFullscreenColorEffect(restore)", BackendComparison::MatrixPurpose::OriginalRestoration)) {
        Diagnostics::Log("MagnificationFilter: restoration unconfirmed; inactive recovery retains the original matrix and API resources.");
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
    SetLastError(ERROR_SUCCESS);
    if (!MagGetFullscreenColorEffect(&actual)) {
        const auto error = static_cast<std::uint32_t>(GetLastError());
        if (error) Diagnostics::LogFormat("Magnification verification read failed (error %lu).", static_cast<unsigned long>(error));
        else Diagnostics::Log("Magnification verification read failed; no native error code was provided.");
        return {BackendVerification::Unavailable, std::nullopt, error};
    }
    const auto expected = ColorEffectForMode(redMode_, lumaProfile_, brightnessPercent_);
    const auto details = BackendComparison::Matrix(actual.transform, expected.transform, BackendComparison::MatrixPurpose::RedOnly);
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
        if (!SetLiveColorEffect(ColorEffectForMode(mode, lumaProfile_, brightnessPercent_), action)) {
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
        !SetLiveColorEffect(ColorEffectForMode(redMode_, profile, brightnessPercent_), "change luma profile")) return false;
    lumaProfile_ = profile;
    Diagnostics::LogFormat("MagnificationFilter luma profile: %s.", LumaProfileName(profile));
    return true;
}

LumaProfile MagnificationFilter::lumaProfile() const { return lumaProfile_; }

bool MagnificationFilter::setBrightnessPercent(unsigned percent) {
    if (!ValidRedBrightness(percent)) {
        LogError("red brightness must be from 1 to 100 percent", ERROR_INVALID_PARAMETER);
        return false;
    }
    if (percent == brightnessPercent_) return true;
    if (restorePending_ && !active_) {
        LogError("change brightness while restoration is pending", ERROR_INVALID_STATE);
        return false;
    }
    char action[96];
    sprintf_s(action, "change red brightness to %u%%", percent);
    if (active_ && !SetLiveColorEffect(ColorEffectForMode(redMode_, lumaProfile_, percent), action)) return false;
    brightnessPercent_ = percent; // Commit only after confirmed live application.
    Diagnostics::LogFormat("MagnificationFilter red brightness: %u%%.", brightnessPercent_);
    return true;
}

bool MagnificationFilter::supportsRedMode(RedMode mode) const {
    return mode == RedMode::StrictRed || mode == RedMode::LumaRed;
}

bool MagnificationFilter::shutdown() {
    RecordBackendOperation operation(statistics_.shutdown, BackendMonotonicMilliseconds);
    if (!runtimeInitialized_) {
        return operation.success();
    }
    if (needsRestore() && !disable()) {
        Diagnostics::Log("MagnificationFilter: restore failed; retaining recovery state.");
        return false;
    }
    initialized_ = false; // A failed teardown may be partial; retain ownership, not readiness.
    SetLastError(ERROR_SUCCESS);
    if (!MagUninitialize()) {
        LogError("MagUninitialize", GetLastError());
        Diagnostics::Log("MagnificationFilter: API cleanup unconfirmed; readiness invalidated and runtime ownership retained for retry.");
        return false;
    }
    runtimeInitialized_ = false;
    Diagnostics::LogFormat("%s shutdown completed.", kFilterName);
    return operation.success();
}

const char* MagnificationFilter::name() const {
    return kFilterName;
}
const char* MagnificationFilter::lifecycleDescription() const noexcept {
    if (restorePending_) return "Restoration pending; red output unconfirmed";
    if (active_) return "Active; red output confirmed at last operation";
    if (initialized_) return "Ready; filter off";
    if (runtimeInitialized_) return "Runtime retained; initialization incomplete or cleanup pending";
    return "Released; backend not initialized";
}

void MagnificationFilter::LogError(const char* action, DWORD error) {
    if (error == ERROR_SUCCESS) {
        std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (no native error code)\n", kFilterName, action);
        Diagnostics::LogFormat("%s: %s failed (no native error code)", kFilterName, action);
        return;
    }
    std::snprintf(lastError_, sizeof(lastError_), "%s: %s failed (GetLastError=%lu)\n",
        kFilterName,
        action,
        static_cast<unsigned long>(error));
    Diagnostics::LogFormat("%s: %s failed (GetLastError=%lu)", kFilterName, action, static_cast<unsigned long>(error));
}

bool MagnificationFilter::SetColorEffect(const MAGCOLOREFFECT& effect, const char* action, BackendComparison::MatrixPurpose purpose) {
    if (!initialized_) {
        LogError(action, ERROR_INVALID_HANDLE);
        return false;
    }

    MAGCOLOREFFECT mutableEffect = effect;
    SetLastError(ERROR_SUCCESS);
    if (!MagSetFullscreenColorEffect(&mutableEffect)) {
        LogError(action, GetLastError());
        return false;
    }

    // Confirm every successful write, including rollback and restoration. A
    // setter return alone cannot confirm that the requested matrix is retained.
    MAGCOLOREFFECT applied = {};
    SetLastError(ERROR_SUCCESS);
    if (!MagGetFullscreenColorEffect(&applied)) {
        const DWORD error = GetLastError();
        LogError("matrix read-back after write", error);
        return false;
    }
    const auto comparison = BackendComparison::Matrix(applied.transform, effect.transform, purpose);
    if (comparison.mismatches != 0) {
        const auto first = comparison.firstMismatch.value_or(BackendValueLocation{});
        Diagnostics::LogFormat("Magnification %s write confirmation mismatch: %u/%u coefficients, first row/column=%u/%u, maximum finite difference %.9g, nonfinite=%s, unexpected G/B=%s.",
            purpose == BackendComparison::MatrixPurpose::RedOnly ? "red-only" : "original-restoration", comparison.mismatches, comparison.values, first.row, first.column, comparison.maximumDifference,
            comparison.nonFiniteValues ? "yes" : "no", comparison.unexpectedGreenBlue ? "yes" : "no");
        LogError("matrix read-back mismatch (unconfirmed transform)", ERROR_INVALID_DATA);
        return false;
    }

    lastError_[0] = '\0';
    return true;
}

bool MagnificationFilter::SetLiveColorEffect(const MAGCOLOREFFECT& effect, const char* action) {
    if (SetColorEffect(effect, action, BackendComparison::MatrixPurpose::RedOnly)) return true;
    char applyError[sizeof(lastError_)];
    std::memcpy(applyError, lastError_, sizeof(applyError));
    active_ = false;
    restorePending_ = true;
    if (SetColorEffect(ColorEffectForMode(redMode_, lumaProfile_, brightnessPercent_), "rollback confirmed red transform", BackendComparison::MatrixPurpose::RedOnly)) {
        active_ = true;
        restorePending_ = false;
        std::memcpy(lastError_, applyError, sizeof(lastError_));
        Diagnostics::Log("MagnificationFilter: failed live change rolled back to the prior confirmed red transform; preference and original snapshot retained.");
    } else Diagnostics::Log("MagnificationFilter: live change and rollback failed; inactive recovery retains the original display snapshot.");
    return false;
}
