#pragma once
#include <cstdint>

enum class ShortcutApiOperation { Register, Unregister };
struct ShortcutApiFailure {
    ShortcutApiOperation operation;
    unsigned id;
    std::uint32_t errorCode;
    bool duringRollback = false;
    bool operator==(const ShortcutApiFailure&) const = default;
};
inline const char* ShortcutApiName(ShortcutApiOperation operation) noexcept {
    return operation == ShortcutApiOperation::Register ? "RegisterHotKey" : "UnregisterHotKey";
}
