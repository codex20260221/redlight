#pragma once
#include "TrayStatus.h"
#include <windows.h>

// Export a supplied cached snapshot; never reads/changes a display backend.
bool CopyTrayStatusJsonToClipboard(HWND owner, const TrayStatus& snapshot) noexcept;
