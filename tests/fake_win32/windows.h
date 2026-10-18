#pragma once
#ifndef REDLIGHT_FAKE_WIN32
#error Fake Win32 headers must never be used by RedLight
#endif
#include <cstddef>
#include <cstdint>
#include <mutex>
using BOOL = int;
using DWORD = unsigned long;
using WORD = unsigned short;
using HDC = void*;
using HWND = void*;
using HMENU = void*;
using UINT = unsigned;
using UINT_PTR = std::uintptr_t;
constexpr UINT MF_STRING = 0, MF_ENABLED = 0, MF_CHECKED = 8, MF_UNCHECKED = 0, MF_GRAYED = 1, MF_POPUP = 16, MF_SEPARATOR = 0x800;
HMENU CreatePopupMenu();
BOOL AppendMenuA(HMENU, UINT, UINT_PTR, const char*);
BOOL DestroyMenu(HMENU);
constexpr DWORD ERROR_INVALID_HANDLE = 6;
constexpr DWORD ERROR_INVALID_DATA = 13;
constexpr DWORD ERROR_INVALID_PARAMETER = 87;
constexpr DWORD ERROR_INVALID_STATE = 5023;
DWORD GetLastError();
std::uint64_t GetTickCount64();
constexpr int SM_CMONITORS = 80, SM_CXVIRTUALSCREEN = 78, SM_CYVIRTUALSCREEN = 79, SM_REMOTESESSION = 0x1000;
int GetSystemMetrics(int);
HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
BOOL GetDeviceGammaRamp(HDC, void*);
BOOL SetDeviceGammaRamp(HDC, void*);

using HANDLE = void*;
using HGLOBAL = void*;
constexpr BOOL FALSE = 0;
constexpr BOOL TRUE = 1;
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

using ULONGLONG = unsigned long long;
struct LARGE_INTEGER { long long QuadPart; };
struct SYSTEMTIME { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; };
struct SRWLOCK { std::mutex mutex; };
#define SRWLOCK_INIT {}
#define INVALID_HANDLE_VALUE reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))
constexpr DWORD ERROR_PATH_NOT_FOUND = 3;
constexpr DWORD ERROR_NOT_ENOUGH_MEMORY = 8;
constexpr DWORD ERROR_WRITE_FAULT = 29;
constexpr DWORD ERROR_DISK_FULL = 112;
constexpr DWORD ERROR_ALREADY_EXISTS = 183;
constexpr DWORD ERROR_ENVVAR_NOT_FOUND = 203;
constexpr DWORD ERROR_FILENAME_EXCED_RANGE = 206;
constexpr DWORD ERROR_DIRECTORY = 267;
constexpr DWORD FILE_APPEND_DATA = 4;
constexpr DWORD FILE_READ_ATTRIBUTES = 128;
constexpr DWORD FILE_SHARE_READ = 1, FILE_SHARE_WRITE = 2, FILE_SHARE_DELETE = 4;
constexpr DWORD OPEN_ALWAYS = 4;
constexpr DWORD FILE_ATTRIBUTE_NORMAL = 128, FILE_ATTRIBUTE_DIRECTORY = 16;
constexpr DWORD INVALID_FILE_ATTRIBUTES = 0xfffffffful;
constexpr DWORD MOVEFILE_REPLACE_EXISTING = 1;
void AcquireSRWLockExclusive(SRWLOCK*);
void ReleaseSRWLockExclusive(SRWLOCK*);
DWORD GetEnvironmentVariableW(const wchar_t*, wchar_t*, DWORD);
BOOL CreateDirectoryW(const wchar_t*, void*);
DWORD GetFileAttributesW(const wchar_t*);
HANDLE CreateFileW(const wchar_t*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
BOOL MoveFileExW(const wchar_t*, const wchar_t*, DWORD);
BOOL CloseHandle(HANDLE);
BOOL GetFileSizeEx(HANDLE, LARGE_INTEGER*);
BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
BOOL FlushFileBuffers(HANDLE);
void GetLocalTime(SYSTEMTIME*);
void OutputDebugStringA(const char*);

constexpr DWORD STD_OUTPUT_HANDLE = static_cast<DWORD>(-11);
constexpr DWORD ATTACH_PARENT_PROCESS = static_cast<DWORD>(-1);
HANDLE GetStdHandle(DWORD);
BOOL AttachConsole(DWORD);
BOOL FreeConsole();
using DWORD_PTR = std::uintptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
constexpr unsigned SMTO_BLOCK = 1, SMTO_ABORTIFHUNG = 2;
HWND FindWindowA(const char*, const char*);
LRESULT SendMessageTimeoutA(HWND, unsigned, WPARAM, LPARAM, unsigned, unsigned, DWORD_PTR*);
constexpr unsigned WM_COMMAND = 0x0111;
constexpr DWORD ERROR_INVALID_WINDOW_HANDLE = 1400;
BOOL PostMessageA(HWND, unsigned, WPARAM, LPARAM);

constexpr DWORD WAIT_OBJECT_0 = 0, WAIT_ABANDONED = 128, WAIT_TIMEOUT = 258, WAIT_FAILED = 0xfffffffful;
constexpr DWORD ERROR_BUSY = 170, ERROR_ABANDONED_WAIT_0 = 735;
HANDLE CreateMutexW(void*, BOOL, const wchar_t*);
DWORD WaitForSingleObject(HANDLE, DWORD);
BOOL ReleaseMutex(HANDLE);
using LPWSTR = wchar_t*;
using HLOCAL = void*;
LPWSTR GetCommandLineW();
HLOCAL LocalFree(HLOCAL);
constexpr DWORD GENERIC_READ = 0x80000000ul, OPEN_EXISTING = 3, FILE_FLAG_SEQUENTIAL_SCAN = 0x08000000ul;
constexpr DWORD FILE_TYPE_UNKNOWN = 0, FILE_TYPE_DISK = 1, FILE_TYPE_CHAR = 2, FILE_TYPE_PIPE = 3;
constexpr DWORD ERROR_HANDLE_EOF = 38, SEM_FAILCRITICALERRORS = 1;
DWORD GetFileType(HANDLE);
BOOL ReadFile(HANDLE, void*, DWORD, DWORD*, void*);
DWORD GetThreadErrorMode();
BOOL SetThreadErrorMode(DWORD, DWORD*);
