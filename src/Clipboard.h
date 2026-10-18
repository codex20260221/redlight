#pragma once
#include <windows.h>
#include <string_view>
#include <string>
#include <optional>

bool CopyUtf8TextToClipboard(HWND owner, std::string_view text);

std::optional<std::string> ReadUtf8TextFromClipboard(HWND owner, std::size_t maxBytes = 4096);
