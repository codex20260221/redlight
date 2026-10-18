#pragma once
#ifndef REDLIGHT_FAKE_WIN32
#error Fake shell API header must never be used by RedLight
#endif
#include <windows.h>
LPWSTR* CommandLineToArgvW(const wchar_t*, int*);
