#pragma once
#ifndef REDLIGHT_FAKE_WIN32
#error Fake WTS header must never be used by RedLight
#endif
#include <windows.h>
constexpr DWORD NOTIFY_FOR_THIS_SESSION = 0;
BOOL WTSRegisterSessionNotification(HWND, DWORD);
BOOL WTSUnRegisterSessionNotification(HWND);
constexpr HANDLE WTS_CURRENT_SERVER_HANDLE = nullptr;
constexpr DWORD WTS_CURRENT_SESSION = 0xfffffffful;
enum WTS_INFO_CLASS { WTSSessionInfoEx = 25 };
enum WTS_CONNECTSTATE_CLASS { WTSActive, WTSConnected, WTSConnectQuery, WTSShadow, WTSDisconnected, WTSIdle, WTSListen, WTSReset, WTSDown, WTSInit };
constexpr long WTS_SESSIONSTATE_LOCK = 0, WTS_SESSIONSTATE_UNLOCK = 1;
struct WTSINFOEX_LEVEL1_W { DWORD SessionId; WTS_CONNECTSTATE_CLASS SessionState; long SessionFlags; };
union WTSINFOEX_LEVEL_W { WTSINFOEX_LEVEL1_W WTSInfoExLevel1; };
struct WTSINFOEXW { DWORD Level; WTSINFOEX_LEVEL_W Data; };
BOOL WTSQuerySessionInformationW(HANDLE, DWORD, WTS_INFO_CLASS, LPWSTR*, DWORD*);
void WTSFreeMemory(void*);
