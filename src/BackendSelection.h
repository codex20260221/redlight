#pragma once
#include "BackendPreference.h"
#include "DisplayFilter.h"

// The effective startup policy stays separate from editable next-start preferences.
inline std::string_view StartupFallbackReason(BackendPreference startupPolicy, bool selectedGamma, std::string_view magnificationError) noexcept {
    if (startupPolicy != BackendPreference::Automatic || !selectedGamma) return "Not applicable";
    return magnificationError.empty() ? "Magnification initialization failed; reason not retained" : magnificationError;
}

// Selection only initializes backends; activation always remains explicit.
inline DisplayFilter* InitializePreferredBackend(BackendPreference preference,
    DisplayFilter& magnification, DisplayFilter& gamma) {
    switch (preference) {
    case BackendPreference::Automatic:
        if (magnification.initialize()) return &magnification;
        // A failed initializer may retain resources after an unsuccessful cleanup.
        // Do not select another backend until that failure is recoverable.
        if (!magnification.shutdown()) return nullptr;
        return gamma.initialize() ? &gamma : nullptr;
    case BackendPreference::Magnification:
        return magnification.initialize() ? &magnification : nullptr;
    case BackendPreference::Gamma:
        return gamma.initialize() ? &gamma : nullptr;
    }
    return nullptr;
}
