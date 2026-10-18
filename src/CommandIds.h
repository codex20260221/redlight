#pragma once
#include "SessionTimer.h"

inline constexpr unsigned IDI_REDLIGHT_ICON = 100;
inline constexpr unsigned ID_TRAY_TOGGLE = 200;
inline constexpr unsigned ID_TRAY_ABOUT = 201;
inline constexpr unsigned ID_TRAY_EXIT = 202;
inline constexpr unsigned ID_TRAY_MODE_STRICT_RED = 203;
inline constexpr unsigned ID_TRAY_MODE_LUMA_RED = 204;
inline constexpr unsigned ID_TRAY_RESTORE = 205;
inline constexpr unsigned ID_TRAY_LOGS = 206;
inline constexpr unsigned ID_TRAY_COPY_REPORT = 207;
inline constexpr unsigned ID_TRAY_COPY_PROFILE = 208;
inline constexpr unsigned ID_TRAY_IMPORT_PROFILE = 209;
inline constexpr unsigned ID_TRAY_VERIFY_BACKEND = 210;
inline constexpr unsigned ID_TRAY_CUSTOM_SESSION = 211;
inline constexpr unsigned ID_TRAY_RETRY_LOGGING = 212;
inline constexpr unsigned ID_TRAY_RESET_PREFERENCES = 213;
inline constexpr unsigned ID_TRAY_RESET_BACKEND_COUNTS = 214;
inline constexpr unsigned ID_TRAY_SAVE_PREFERENCES = 215;
inline constexpr unsigned ID_TRAY_RETRY_SESSION = 216;
inline constexpr unsigned ID_TRAY_RESTART_COUNTDOWN = 217;
inline constexpr unsigned ID_TRAY_RETRY_SHORTCUTS = 218;
inline constexpr unsigned ID_TRAY_CLEAR_RESTORATION_HISTORY = 219;
inline constexpr unsigned ID_TRAY_COPY_STATUS_JSON = 220;
inline constexpr unsigned ID_TRAY_REFRESH_DISPLAY_CONTEXT = 221;
inline constexpr unsigned ID_TRAY_ENABLE = 222; // Explicit intent from an already displayed menu.
inline constexpr unsigned ID_TRAY_COPY_RECENT_DIAGNOSTICS = 223;
inline constexpr unsigned ID_HOTKEY_TOGGLE = 1;
inline constexpr unsigned ID_HOTKEY_RESTORE = 2;
inline constexpr unsigned ID_TRAY_RETRY_TIMER = 1;
inline constexpr unsigned ID_SESSION_TIMER = 2;
inline constexpr unsigned ID_SESSION_LIMIT_BASE = 1000;
inline constexpr unsigned ID_BACKEND_PREFERENCE_BASE = 1300;
inline constexpr unsigned ID_LUMA_PROFILE_BASE = 1400;
inline constexpr unsigned ID_SHORTCUT_PROFILE_BASE = 1500;
inline constexpr unsigned ID_EXPIRY_WARNING_BASE = 1600;

static_assert(ID_SESSION_LIMIT_BASE + SessionTimer::maxMinutes < ID_BACKEND_PREFERENCE_BASE);
static_assert(ID_BACKEND_PREFERENCE_BASE + 2 < ID_LUMA_PROFILE_BASE);

static_assert(ID_LUMA_PROFILE_BASE + 2 < ID_SHORTCUT_PROFILE_BASE);
static_assert(ID_SHORTCUT_PROFILE_BASE + 3 < ID_EXPIRY_WARNING_BASE);
