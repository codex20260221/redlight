#pragma once
#include <windows.h>
#include <optional>
std::optional<unsigned> ShowSessionLimitDialog(HWND owner, unsigned currentMinutes);
