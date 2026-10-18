#pragma once
#include <cstdint>

enum class CountdownTimerApi { Install, Remove };
struct CountdownTimerFailure {
    CountdownTimerApi operation = CountdownTimerApi::Install;
    std::uint32_t errorCode = 0;
    bool operator==(const CountdownTimerFailure&) const = default;
};
inline const char* CountdownTimerApiName(CountdownTimerApi operation) noexcept {
    switch (operation) {
    case CountdownTimerApi::Install: return "SetTimer";
    case CountdownTimerApi::Remove: return "KillTimer";
    }
    return "Unknown";
}
