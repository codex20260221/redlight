#pragma once
#include "windows.h"
struct MAGCOLOREFFECT { float transform[5][5]; };
BOOL MagInitialize();
BOOL MagUninitialize();
BOOL MagGetFullscreenColorEffect(MAGCOLOREFFECT*);
BOOL MagSetFullscreenColorEffect(MAGCOLOREFFECT*);
struct MAGTRANSFORM { float v[3][3]; };
inline constexpr wchar_t WC_MAGNIFIERW[] = L"Magnifier";
constexpr DWORD MW_FILTERMODE_EXCLUDE = 0;
BOOL MagSetColorEffect(HWND, MAGCOLOREFFECT*);
BOOL MagGetColorEffect(HWND, MAGCOLOREFFECT*);
BOOL MagSetWindowFilterList(HWND, DWORD, int, HWND*);
int MagGetWindowFilterList(HWND, DWORD*, int, HWND*);
BOOL MagSetWindowTransform(HWND, MAGTRANSFORM*);
BOOL MagGetWindowTransform(HWND, MAGTRANSFORM*);
BOOL MagSetWindowSource(HWND, RECT);
BOOL MagGetWindowSource(HWND, RECT*);
