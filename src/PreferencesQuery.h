#pragma once

inline constexpr unsigned WM_REDLIGHT_PREFERENCES = 0x8004; // WM_APP + 4
inline constexpr unsigned REDLIGHT_PREFERENCES_PROTOCOL = 1;
// Cached existing-tray query only. 0 complete, 3 not running, 1 query/output error.
int QueryRunningPreferences() noexcept;
