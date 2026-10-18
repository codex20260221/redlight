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

using HANDLE = void*;
using HGLOBAL = void*;
constexpr BOOL FALSE = 0;
constexpr DWORD ERROR_SUCCESS = 0;
constexpr unsigned CP_UTF8 = 65001;
constexpr DWORD MB_ERR_INVALID_CHARS = 8;
constexpr unsigned GMEM_MOVEABLE = 2;
constexpr unsigned CF_UNICODETEXT = 13;
void SetLastError(DWORD);
int MultiByteToWideChar(unsigned, DWORD, const char*, int, wchar_t*, int);
HGLOBAL GlobalAlloc(unsigned, std::size_t);
void* GlobalLock(HGLOBAL);
BOOL GlobalUnlock(HGLOBAL);
HGLOBAL GlobalFree(HGLOBAL);
BOOL OpenClipboard(HWND);
BOOL EmptyClipboard();
HANDLE SetClipboardData(unsigned, HANDLE);
BOOL CloseClipboard();
