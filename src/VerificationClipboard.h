#pragma once
#include "VerificationJson.h"
#include <windows.h>

bool CopyVerificationJsonToClipboard(HWND owner, const VerificationJsonSnapshot& snapshot) noexcept;
