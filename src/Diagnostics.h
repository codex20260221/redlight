#pragma once
#include <string>

namespace Diagnostics {

std::wstring LogDirectory();
void Initialize();
void Log(const char* message);
void LogFormat(const char* format, ...);
void Shutdown();

}  // namespace Diagnostics
