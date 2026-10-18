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

using LSTATUS = long;
using HKEY = void*;
constexpr HKEY HKEY_CURRENT_USER = nullptr;
constexpr DWORD ERROR_FILE_NOT_FOUND = 2;
constexpr DWORD ERROR_ACCESS_DENIED = 5;
constexpr DWORD ERROR_MORE_DATA = 234;
constexpr DWORD ERROR_UNSUPPORTED_TYPE = 1630;
constexpr DWORD RRF_RT_REG_BINARY = 8;
constexpr DWORD RRF_RT_REG_DWORD = 16;
constexpr DWORD REG_BINARY = 3;
constexpr DWORD REG_DWORD = 4;
LSTATUS RegGetValueW(HKEY, const wchar_t*, const wchar_t*, DWORD, DWORD*, void*, DWORD*);
LSTATUS RegSetKeyValueW(HKEY, const wchar_t*, const wchar_t*, DWORD, const void*, DWORD);

constexpr DWORD WC_ERR_INVALID_CHARS = 128;
HANDLE GetClipboardData(unsigned);
std::size_t GlobalSize(HGLOBAL);
int WideCharToMultiByte(unsigned, DWORD, const wchar_t*, int, char*, int, const char*, BOOL*);

constexpr unsigned MOD_NOREPEAT = 0x4000;
BOOL RegisterHotKey(HWND, int, unsigned, unsigned);
BOOL UnregisterHotKey(HWND, int);
