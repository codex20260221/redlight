#pragma once
#include <array>
#include <optional>
#include <string_view>

enum class ShortcutProfile : unsigned { Default = 0, F9 = 1, F10 = 2, Disabled = 3 };
struct ShortcutBinding {
    unsigned modifiers, key;
    bool operator==(const ShortcutBinding&) const = default;
};
inline bool IsValidShortcutProfile(unsigned value) { return value <= 3; }
inline const char* ShortcutProfileName(ShortcutProfile profile) {
    switch (profile) {
    case ShortcutProfile::Default: return "Ctrl+Alt+R (default)";
    case ShortcutProfile::F9: return "Ctrl+Alt+F9";
    case ShortcutProfile::F10: return "Ctrl+Alt+F10";
    case ShortcutProfile::Disabled: return "Disabled";
    }
    return "Unknown";
}
inline const char* ShortcutProfileToken(ShortcutProfile profile) {
    switch (profile) {
    case ShortcutProfile::Default: return "default";
    case ShortcutProfile::F9: return "f9";
    case ShortcutProfile::F10: return "f10";
    case ShortcutProfile::Disabled: return "disabled";
    }
    return "invalid";
}
inline std::optional<ShortcutProfile> ParseShortcutProfile(std::string_view token) {
    if (token == "default") return ShortcutProfile::Default;
    if (token == "f9") return ShortcutProfile::F9;
    if (token == "f10") return ShortcutProfile::F10;
    if (token == "disabled") return ShortcutProfile::Disabled;
    return std::nullopt;
}
inline std::array<std::optional<ShortcutBinding>, 2> ShortcutBindings(ShortcutProfile profile) {
    if (!IsValidShortcutProfile(static_cast<unsigned>(profile)) || profile == ShortcutProfile::Disabled) return {};
    const unsigned key = profile == ShortcutProfile::Default ? 'R' : (profile == ShortcutProfile::F9 ? 0x78 : 0x79);
    return {ShortcutBinding{3, key}, ShortcutBinding{7, key}}; // Control+Alt, plus Shift for restore.
}
inline const char* ShortcutBindingName(const ShortcutBinding& binding) {
    if (binding.modifiers == 3) {
        if (binding.key == 'R') return "Ctrl+Alt+R";
        if (binding.key == 0x78) return "Ctrl+Alt+F9";
        if (binding.key == 0x79) return "Ctrl+Alt+F10";
    } else if (binding.modifiers == 7) {
        if (binding.key == 'R') return "Ctrl+Alt+Shift+R";
        if (binding.key == 0x78) return "Ctrl+Alt+Shift+F9";
        if (binding.key == 0x79) return "Ctrl+Alt+Shift+F10";
    }
    return "Unknown binding";
}
