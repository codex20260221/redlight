#pragma once
#include "windows.h"
// Minimal declarations for the isolated topology adapter. Real SDK compilation
// separately checks its use of native structures/signatures.
using LONG = long;
using UINT32 = std::uint32_t;
struct LUID { DWORD LowPart; LONG HighPart; };
struct POINTL { LONG x, y; };
using HMONITOR = void*;
using DPI_AWARENESS_CONTEXT = void*;
#define CALLBACK
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 reinterpret_cast<DPI_AWARENESS_CONTEXT>(std::intptr_t{-4})
DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext();
BOOL AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
constexpr DWORD MONITORINFOF_PRIMARY = 1;
struct MONITORINFO { DWORD cbSize; RECT rcMonitor, rcWork; DWORD dwFlags; };
struct MONITORINFOEXW : MONITORINFO { wchar_t szDevice[32]; };
using MONITORENUMPROC = BOOL (*)(HMONITOR, HDC, RECT*, LPARAM);
BOOL EnumDisplayMonitors(HDC, const RECT*, MONITORENUMPROC, LPARAM);
BOOL GetMonitorInfoW(HMONITOR, MONITORINFO*);
constexpr UINT32 QDC_ONLY_ACTIVE_PATHS = 2, QDC_VIRTUAL_MODE_AWARE = 0x10;
constexpr UINT32 DISPLAYCONFIG_PATH_ACTIVE = 1, DISPLAYCONFIG_PATH_SUPPORT_VIRTUAL_MODE = 8, DISPLAYCONFIG_PATH_VALID_FLAGS = 0x1d;
constexpr UINT32 DISPLAYCONFIG_SOURCE_IN_USE = 1, DISPLAYCONFIG_TARGET_IN_USE = 1;
constexpr LONG ERROR_INSUFFICIENT_BUFFER = 122;
enum DISPLAYCONFIG_MODE_INFO_TYPE { DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE = 1, DISPLAYCONFIG_MODE_INFO_TYPE_FORCE_UINT32 = 0xffffffffu };
struct DISPLAYCONFIG_SOURCE_MODE { UINT32 width, height; UINT32 pixelFormat; POINTL position; };
struct DISPLAYCONFIG_MODE_INFO { DISPLAYCONFIG_MODE_INFO_TYPE infoType; UINT32 id; LUID adapterId; DISPLAYCONFIG_SOURCE_MODE sourceMode; };
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201) // Match the SDK's anonymous union/bit-field accessors.
#endif
struct DISPLAYCONFIG_PATH_SOURCE_INFO {
    LUID adapterId; UINT32 id;
    union { UINT32 modeInfoIdx; struct { UINT32 cloneGroupId : 16; UINT32 sourceModeInfoIdx : 16; }; };
    UINT32 statusFlags;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
struct DISPLAYCONFIG_PATH_TARGET_INFO { LUID adapterId; UINT32 id, modeInfoIdx, rotation; BOOL targetAvailable; UINT32 statusFlags; };
struct DISPLAYCONFIG_PATH_INFO { DISPLAYCONFIG_PATH_SOURCE_INFO sourceInfo; DISPLAYCONFIG_PATH_TARGET_INFO targetInfo; UINT32 flags; };
enum DISPLAYCONFIG_TOPOLOGY_ID { DISPLAYCONFIG_TOPOLOGY_EXTEND = 4 };
enum DISPLAYCONFIG_DEVICE_INFO_TYPE { DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME = 1, DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME = 2 };
struct DISPLAYCONFIG_DEVICE_INFO_HEADER { DISPLAYCONFIG_DEVICE_INFO_TYPE type; UINT32 size; LUID adapterId; UINT32 id; };
struct DISPLAYCONFIG_SOURCE_DEVICE_NAME { DISPLAYCONFIG_DEVICE_INFO_HEADER header; wchar_t viewGdiDeviceName[32]; };
struct DISPLAYCONFIG_TARGET_DEVICE_NAME { DISPLAYCONFIG_DEVICE_INFO_HEADER header; UINT32 flags, outputTechnology; WORD edidManufactureId, edidProductCodeId; UINT32 connectorInstance; wchar_t monitorFriendlyDeviceName[64], monitorDevicePath[128]; };
LONG GetDisplayConfigBufferSizes(UINT32, UINT32*, UINT32*);
LONG QueryDisplayConfig(UINT32, UINT32*, DISPLAYCONFIG_PATH_INFO*, UINT32*, DISPLAYCONFIG_MODE_INFO*, DISPLAYCONFIG_TOPOLOGY_ID*);
LONG DisplayConfigGetDeviceInfo(DISPLAYCONFIG_DEVICE_INFO_HEADER*);
