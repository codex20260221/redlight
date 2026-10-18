#pragma once
#include <windows.h>
#include <optional>

std::optional<unsigned> ShowRedBrightnessDialog(HWND owner, unsigned currentPercent);
