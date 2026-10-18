#include "ScheduleClock.h"
#include <windows.h>
#include <chrono>

namespace {
constexpr std::uint64_t MaxCaptureMilliseconds = 2000;
ScheduleClockResult Failure(ScheduleClockStage stage, DWORD error = 0) noexcept {
    return {std::nullopt, stage, static_cast<std::uint32_t>(error)};
}
std::optional<std::uint64_t> CalendarMilliseconds(const SYSTEMTIME& value) noexcept {
    if (value.wYear < 1601 || value.wYear > 30827 || value.wMonth < 1 || value.wMonth > 12 || value.wDay < 1 || value.wDay > 31 ||
        value.wDayOfWeek > 6 || value.wHour > 23 || value.wMinute > 59 || value.wSecond > 59 || value.wMilliseconds > 999) return std::nullopt;
    using namespace std::chrono;
    const year_month_day date{year{value.wYear}, month{value.wMonth}, day{value.wDay}};
    if (!date.ok()) return std::nullopt;
    const auto days = (sys_days{date} - sys_days{year{1601}/January/1}).count();
    return static_cast<std::uint64_t>(days) * 86400000 + value.wHour * 3600000ull + value.wMinute * 60000ull + value.wSecond * 1000ull + value.wMilliseconds;
}
std::optional<std::array<unsigned, 8>> Transition(const SYSTEMTIME& value) noexcept {
    if (!value.wMonth) return std::array<unsigned, 8>{}; // Remaining fields are unused.
    if (value.wMonth > 12 || value.wDayOfWeek > 6 || value.wHour > 23 || value.wMinute > 59 || value.wSecond > 59 || value.wMilliseconds > 999 ||
        (!value.wYear && (value.wDay < 1 || value.wDay > 5)) || (value.wYear && !CalendarMilliseconds(value))) return std::nullopt;
    return std::array<unsigned, 8>{value.wYear, value.wMonth, value.wDayOfWeek, value.wDay, value.wHour, value.wMinute, value.wSecond, value.wMilliseconds};
}
std::optional<ScheduleZoneIdentity> ZoneIdentity(const DYNAMIC_TIME_ZONE_INFORMATION& native) noexcept {
    const auto standard = Transition(native.StandardDate), daylight = Transition(native.DaylightDate);
    if (!standard || !daylight || ((native.StandardDate.wMonth == 0) != (native.DaylightDate.wMonth == 0))) return std::nullopt;
    ScheduleZoneIdentity result;
    result.bias = native.Bias;
    result.standardBias = native.StandardDate.wMonth ? native.StandardBias : 0;
    result.daylightBias = native.DaylightDate.wMonth ? native.DaylightBias : 0;
    result.standardDate = *standard; result.daylightDate = *daylight;
    result.dynamicDisabled = native.DynamicDaylightTimeDisabled != 0;
    for (std::size_t index = 0; index < result.key.size(); ++index) {
        result.key[index] = native.TimeZoneKeyName[index];
        if (!result.key[index]) return result; // Ignore trailing bytes and localized display names.
    }
    return std::nullopt;
}
}

ScheduleClockResult ReadScheduleClock() noexcept {
    const auto started = GetTickCount64();
    ULONGLONG awakeStart = 0;
    if (!QueryUnbiasedInterruptTime(&awakeStart)) return Failure(ScheduleClockStage::AwakeClock);
    FILETIME file = {};
    GetSystemTimeAsFileTime(&file); // No failure/last-error contract.
    const auto rawUtc = (static_cast<std::uint64_t>(file.dwHighDateTime) << 32) | file.dwLowDateTime;
    DYNAMIC_TIME_ZONE_INFORMATION zone = {};
    SetLastError(ERROR_SUCCESS);
    const auto zoneStatus = GetDynamicTimeZoneInformation(&zone);
    if (zoneStatus == TIME_ZONE_ID_INVALID) return Failure(ScheduleClockStage::TimeZone, GetLastError());
    const auto identity = ZoneIdentity(zone);
    if (zoneStatus > TIME_ZONE_ID_DAYLIGHT || !identity) return Failure(ScheduleClockStage::InvalidData);
    SYSTEMTIME utc = {}, local = {};
    SetLastError(ERROR_SUCCESS);
    if (!FileTimeToSystemTime(&file, &utc)) return Failure(ScheduleClockStage::UtcConversion, GetLastError());
    const auto utcCalendar = CalendarMilliseconds(utc);
    if (!utcCalendar || *utcCalendar != rawUtc / 10000) return Failure(ScheduleClockStage::InvalidData);
    SetLastError(ERROR_SUCCESS);
    if (!SystemTimeToTzSpecificLocalTimeEx(&zone, &utc, &local)) return Failure(ScheduleClockStage::LocalConversion, GetLastError());
    const auto localCalendar = CalendarMilliseconds(local);
    if (!localCalendar) return Failure(ScheduleClockStage::InvalidData);
    const auto offset = static_cast<std::int64_t>(*localCalendar) - static_cast<std::int64_t>(*utcCalendar);
    if (offset < -86400000 || offset > 86400000 || offset % 60000 != 0) return Failure(ScheduleClockStage::InvalidData);
    ULONGLONG awakeEnd = 0;
    if (!QueryUnbiasedInterruptTime(&awakeEnd)) return Failure(ScheduleClockStage::AwakeClock);
    const auto elapsed = GetTickCount64() - started;
    const auto awakeElapsed = awakeEnd - awakeStart;
    if (elapsed > MaxCaptureMilliseconds || awakeElapsed > MaxCaptureMilliseconds * 10000) return Failure(ScheduleClockStage::CaptureTooSlow);
    ScheduleClockReading result;
    result.year = local.wYear; result.month = local.wMonth; result.day = local.wDay;
    result.minute = local.wHour * 60u + local.wMinute;
    result.millisecondsInMinute = local.wSecond * 1000u + local.wMilliseconds;
    result.utcMilliseconds = *utcCalendar;
    result.awakeMilliseconds = (awakeStart + awakeElapsed / 2) / 10000;
    result.captureMilliseconds = elapsed;
    result.zone = *identity;
    return {result};
}
