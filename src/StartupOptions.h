#pragma once
#include "CommandLine.h"

// Own/free the native argument block even if constructing C++ views fails.
StartupOptions GetStartupOptions() noexcept;
