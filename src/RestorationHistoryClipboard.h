#pragma once
#include "RestorationHistoryJson.h"
#include <windows.h>

bool CopyRestorationHistoryJsonToClipboard(HWND owner, const RestorationHistory& snapshot, std::uint64_t now) noexcept;
