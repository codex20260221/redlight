#pragma once
#include <optional>
#include <string_view>

enum class BackendPreference : unsigned { Automatic = 0, Magnification = 1, Gamma = 2 };
inline bool IsValidBackendPreference(unsigned value) { return value <= 2; }
inline const char* BackendPreferenceName(BackendPreference value) {
    switch (value) {
    case BackendPreference::Automatic: return "Automatic";
    case BackendPreference::Magnification: return "Magnification only";
    case BackendPreference::Gamma: return "Legacy gamma only";
    }
    return "Unknown";
}
inline std::optional<BackendPreference> ParseBackendPreference(std::wstring_view value) {
    if (value == L"auto") return BackendPreference::Automatic;
    if (value == L"magnification") return BackendPreference::Magnification;
    if (value == L"gamma") return BackendPreference::Gamma;
    return std::nullopt;
}
