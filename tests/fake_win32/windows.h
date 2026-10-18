#pragma once
#ifndef REDLIGHT_FAKE_WIN32
#error Fake Win32 headers must never be used by RedLight
#endif
#include <cstddef>
using BOOL = int;
using DWORD = unsigned long;
using WORD = unsigned short;
using HDC = void*;
using HWND = void*;
constexpr DWORD ERROR_INVALID_HANDLE = 6;
constexpr DWORD ERROR_INVALID_DATA = 13;
constexpr DWORD ERROR_INVALID_PARAMETER = 87;
constexpr DWORD ERROR_INVALID_STATE = 5023;
DWORD GetLastError();
HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
BOOL GetDeviceGammaRamp(HDC, void*);
BOOL SetDeviceGammaRamp(HDC, void*);
