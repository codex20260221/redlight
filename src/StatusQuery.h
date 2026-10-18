#pragma once

inline constexpr unsigned WM_REDLIGHT_STATUS = 0x8003; // WM_APP + 3
inline constexpr unsigned REDLIGHT_STATUS_PROTOCOL = 1;
// 0: reported, 1: query/output failure, 3: no existing instance. Never starts one.
int QueryRunningStatus() noexcept;
