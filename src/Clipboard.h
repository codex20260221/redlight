#pragma once
#include <windows.h>
#include <string_view>

bool CopyUtf8TextToClipboard(HWND owner, std::string_view text);
