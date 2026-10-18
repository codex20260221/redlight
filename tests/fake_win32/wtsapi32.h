#pragma once
#ifndef REDLIGHT_FAKE_WIN32
#error Fake WTS header must never be used by RedLight
#endif
#include <windows.h>
constexpr DWORD NOTIFY_FOR_THIS_SESSION = 0;
BOOL WTSRegisterSessionNotification(HWND, DWORD);
BOOL WTSUnRegisterSessionNotification(HWND);
