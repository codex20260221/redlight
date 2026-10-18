#pragma once
#include "CurrentPreferences.h"
#include <windows.h>

bool CopyCurrentPreferencesJsonToClipboard(HWND owner, const CurrentPreferencesSnapshot& snapshot) noexcept;
