#pragma once
#include "SchedulePreferences.h"
#include <windows.h>
#include <optional>
std::optional<Scheduling::DailyTimes> ShowScheduleDialog(HWND owner, Scheduling::DailyTimes current);
