#pragma once
#include "DisplayFilter.h"
#include <cstdint>
#include <optional>

// One explicit check and its monotonic timing, cleared together before output changes.
class BackendVerificationCache {
public:
    const BackendVerificationReport& result() const noexcept { return report_; }
    void beforeModeSelection(const DisplayFilter& filter, RedMode mode) {
        if (filter.isActive() && filter.supportsRedMode(mode) && filter.redMode() != mode) *this = {};
    }
    void beforeLumaSelection(const DisplayFilter& filter, LumaProfile profile) {
        if (IsValidLumaProfile(static_cast<unsigned>(profile)) && filter.isActive() &&
            filter.supportsRedMode(RedMode::LumaRed) && filter.redMode() == RedMode::LumaRed &&
            filter.lumaProfile() != profile) *this = {};
    }
    void beforeBrightnessSelection(const DisplayFilter& filter, unsigned percent) {
        if (ValidRedBrightness(percent) && filter.isActive() && filter.supportsBrightness() &&
            filter.brightnessPercent() != percent) *this = {};
    }
    void record(const BackendVerificationReport& report, std::uint64_t started, std::uint64_t completed) noexcept {
        if (report.state == BackendVerification::NotChecked) {
            *this = {};
            return;
        }
        report_ = report;
        completed_ = completed;
        duration_ = completed - started;
    }
    std::optional<std::uint64_t> ageSeconds(std::uint64_t now) const noexcept {
        if (!completed_) return std::nullopt;
        return (now - *completed_) / 1000;
    }
    std::optional<std::uint64_t> durationMilliseconds() const noexcept {
        if (!completed_) return std::nullopt;
        return duration_;
    }
private:
    BackendVerificationReport report_;
    std::optional<std::uint64_t> completed_;
    std::uint64_t duration_ = 0;
};
