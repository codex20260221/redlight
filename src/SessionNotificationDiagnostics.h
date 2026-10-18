#pragma once
#include <cstdint>

enum class SessionNotificationApi { Register, Unregister };
struct SessionNotificationFailure {
    SessionNotificationApi operation = SessionNotificationApi::Register;
    std::uint32_t errorCode = 0;
    bool operator==(const SessionNotificationFailure&) const = default;
};
inline const char* SessionNotificationApiName(SessionNotificationApi operation) noexcept {
    switch (operation) {
    case SessionNotificationApi::Register: return "WTSRegisterSessionNotification";
    case SessionNotificationApi::Unregister: return "WTSUnRegisterSessionNotification";
    }
    return "Unknown";
}
