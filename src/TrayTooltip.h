#pragma once
#include "DisplayFilter.h"
#include "SessionTimer.h"
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

struct TrayTooltipState {
    bool active = false, needsRestore = false, hasError = false, restoreRequested = false, gammaBackend = false;
    BackendVerification verification = BackendVerification::NotChecked;
    RedMode mode = RedMode::LumaRed;
    LumaProfile luma = LumaProfile::Classic;
    unsigned brightness = 100;
    std::optional<unsigned> remainingSeconds, fadeTarget;
};
inline std::optional<std::string_view> FormatTrayTooltip(const TrayTooltipState& state, std::span<char> output) noexcept {
    if (output.empty() || !ValidRedBrightness(state.brightness) || !IsValidLumaProfile(static_cast<unsigned>(state.luma)) ||
        (state.mode != RedMode::StrictRed && state.mode != RedMode::LumaRed) || static_cast<unsigned>(state.verification) > 4 ||
        (state.active && !state.needsRestore) || (state.remainingSeconds && *state.remainingSeconds > SessionTimer::maxMinutes * 60) ||
        (state.gammaBackend && (state.mode != RedMode::StrictRed || state.brightness != 100 || state.fadeTarget)) ||
        (state.fadeTarget && (!state.active || !ValidRedBrightness(*state.fadeTarget) || *state.fadeTarget <= state.brightness))) return std::nullopt;
    char remaining[32] = {}, profile[48] = {}, brightness[32];
    if (state.remainingSeconds) std::snprintf(remaining, sizeof(remaining), " - %u:%02u left", *state.remainingSeconds / 60, *state.remainingSeconds % 60);
    if (state.mode == RedMode::LumaRed) std::snprintf(profile, sizeof(profile), " [%s]", LumaProfileName(state.luma));
    if (state.fadeTarget) std::snprintf(brightness, sizeof(brightness), "%u->%u%% red fade", state.brightness, *state.fadeTarget);
    else std::snprintf(brightness, sizeof(brightness), "%u%% red", state.brightness);
    const int length = std::snprintf(output.data(), output.size(), "RedLight %s - %s - %s%s - %s%s",
        FilterStatusDescription(state.active, state.needsRestore, state.hasError, state.verification, state.restoreRequested),
        state.gammaBackend ? "Gamma" : "Magnification", RedModeDisplayName(state.mode), profile, brightness, remaining);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size()) return std::nullopt;
    return std::string_view(output.data(), static_cast<std::size_t>(length));
}
