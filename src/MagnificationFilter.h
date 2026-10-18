#pragma once

#include "DisplayFilter.h"

#include <windows.h>
#include <magnification.h>

class MagnificationFilter final : public DisplayFilter {
public:
    MagnificationFilter();
    ~MagnificationFilter() override;
    MagnificationFilter(const MagnificationFilter&) = delete;
    MagnificationFilter& operator=(const MagnificationFilter&) = delete;

    bool initialize() override;
    bool enable() override;
    bool disable() override;
    bool isActive() const override;
    bool needsRestore() const override;
    const char* lastError() const override;
    BackendVerificationReport inspectState() const override;
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
    void LogError(const char* action, DWORD error);
    bool SetColorEffect(const MAGCOLOREFFECT& effect, const char* action);

    bool initialized_;
    bool active_;
    bool restorePending_;
    RedMode redMode_;
    LumaProfile lumaProfile_;
    MAGCOLOREFFECT restoreEffect_;
    char lastError_[256];
    mutable BackendStatistics statistics_{};
};
