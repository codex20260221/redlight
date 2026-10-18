#pragma once

#include "DisplayFilter.h"

#include <windows.h>

class GammaRampFilter final : public DisplayFilter {
public:
    GammaRampFilter();
    ~GammaRampFilter() override;
    GammaRampFilter(const GammaRampFilter&) = delete;
    GammaRampFilter& operator=(const GammaRampFilter&) = delete;

    bool initialize() override;
    bool enable() override;
    bool disable() override;
    bool isActive() const override;
    bool needsRestore() const override;
    const char* lastError() const override;
    BackendVerification verifyState() const override;
    bool setRedMode(RedMode mode) override;
    RedMode redMode() const override;
    bool setLumaProfile(LumaProfile profile) override;
    LumaProfile lumaProfile() const override;
    bool supportsRedMode(RedMode mode) const override;
    bool shutdown() override;
    const char* name() const override;
    BackendStatistics statistics() const noexcept override { return statistics_; }
    bool resetStatistics() noexcept override { return ResetBackendStatistics(statistics_); }

private:
    bool AcquireOwnership();
    bool ReleaseOwnership();
    void SetError(const char* action, DWORD error);
    bool SetGammaRamp(WORD ramp[3][256], const char* action, bool requireZeroGreenBlue = false);
    void BuildRedRamp();
    static bool RampMatches(const WORD applied[3][256], const WORD expected[3][256], bool requireZeroGreenBlue);

    HDC hdc_;
    bool initialized_;
    bool active_;
    bool restorePending_;
    // Gamma operations and mutex ownership stay on the tray's UI thread.
    HANDLE ownershipMutex_;
    bool ownershipAcquired_;
    bool ownershipAbandoned_;
    WORD originalGammaRamp_[3][256];
    WORD redGammaRamp_[3][256];
    char lastError_[256];
    mutable BackendStatistics statistics_{};
};
