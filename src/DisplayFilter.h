#pragma once
#include "LumaProfile.h"
#include "BackendStatistics.h"
#include "RedBrightness.h"
#include <optional>

enum class RedMode {
    StrictRed,
    LumaRed,
};

inline const char* RedModeDisplayName(RedMode mode) {
    switch (mode) {
    case RedMode::StrictRed:
        return "Strict Red";
    case RedMode::LumaRed:
        return "Luma Red";
    }

    return "Unknown";
}

enum class BackendVerification { NotChecked, Inactive, Matches, Changed, Unavailable };
struct BackendValueLocation {
    unsigned row = 0, column = 0; // Matrix row/column or gamma channel/entry; zero-based.
    bool operator==(const BackendValueLocation&) const = default;
};
struct BackendReadbackDetails {
    unsigned values = 0, mismatches = 0;
    double maximumDifference = 0;
    bool nonFiniteValues = false, unexpectedGreenBlue = false, gammaRamp = false;
    bool redBlackEndpointMismatch = false, redWhiteEndpointMismatch = false;
    std::optional<BackendValueLocation> firstMismatch, maximumDifferenceAt;
};
struct BackendVerificationReport {
    BackendVerification state = BackendVerification::NotChecked;
    std::optional<BackendReadbackDetails> readback;
    std::uint32_t errorCode = 0;
};
inline const char* BackendVerificationName(BackendVerification value) {
    switch (value) {
    case BackendVerification::NotChecked: return "Not checked since last filter operation";
    case BackendVerification::Inactive: return "Inactive (no filter owned)";
    case BackendVerification::Matches: return "Read-back matches requested transform";
    case BackendVerification::Changed: return "Read-back differs from requested transform";
    case BackendVerification::Unavailable: return "Read-back unavailable";
    }
    return "Unknown";
}

inline const char* FilterStatusDescription(bool active, bool needsRestore, bool hasError, BackendVerification verification, bool restoreRequested = false) {
    if (restoreRequested || (needsRestore && (!active || hasError))) return "Recovery needed";
    if (verification == BackendVerification::Changed) return "Backend changed";
    if (verification == BackendVerification::Unavailable) return "Verification unavailable";
    return active ? "ON" : "off";
}

class DisplayFilter {
public:
    virtual ~DisplayFilter() = default;

    virtual bool initialize() = 0;
    virtual bool enable() = 0;
    virtual bool disable() = 0;
    virtual bool isActive() const = 0;
    // A failed/partial write or pending ownership cleanup can require recovery while inactive.
    virtual bool needsRestore() const = 0;
    virtual const char* lastError() const = 0;
    virtual BackendVerification verifyState() const { return inspectState().state; }
    virtual BackendVerificationReport inspectState() const = 0;
    virtual bool setRedMode(RedMode mode) = 0;
    virtual RedMode redMode() const = 0;
    virtual bool setLumaProfile(LumaProfile profile) = 0;
    virtual LumaProfile lumaProfile() const = 0;
    virtual bool supportsRedMode(RedMode mode) const = 0;
    virtual bool supportsBrightness() const noexcept = 0;
    virtual unsigned brightnessPercent() const noexcept = 0;
    virtual bool setBrightnessPercent(unsigned percent) = 0;
    // Keep resources and restore data available when recovery fails.
    virtual bool shutdown() = 0;
    virtual const char* name() const = 0;
    virtual BackendStatistics statistics() const noexcept = 0;
    virtual bool resetStatistics() noexcept = 0;
};
