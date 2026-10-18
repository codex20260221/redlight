#pragma once
#include <array>

enum class LumaProfile : unsigned { Classic = 0, GreenDetail = 1, EqualDetail = 2 };
inline bool IsValidLumaProfile(unsigned value) { return value <= 2; }
inline const char* LumaProfileName(LumaProfile profile) {
    switch (profile) {
    case LumaProfile::Classic: return "Classic (BT.601)";
    case LumaProfile::GreenDetail: return "Green detail (BT.709)";
    case LumaProfile::EqualDetail: return "Equal color detail";
    }
    return "Unknown";
}
inline constexpr std::array<float, 3> LumaProfileWeights(LumaProfile profile) {
    switch (profile) {
    case LumaProfile::Classic: return {0.299f, 0.587f, 0.114f};
    case LumaProfile::GreenDetail: return {0.2126f, 0.7152f, 0.0722f};
    case LumaProfile::EqualDetail: return {1.0f / 3, 1.0f / 3, 1.0f / 3};
    }
    return {0.299f, 0.587f, 0.114f};
}
