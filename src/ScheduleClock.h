#pragma once
#include <array>
#include <cstdint>
#include <optional>

// Read-only scheduling capture. No display, timer, or persistence mutation.
struct ScheduleZoneIdentity {
    std::int32_t bias = 0, standardBias = 0, daylightBias = 0;
    std::array<unsigned, 8> standardDate{}, daylightDate{};
    std::array<wchar_t, 128> key{};
    bool dynamicDisabled = false;
    bool operator==(const ScheduleZoneIdentity&) const = default;
};
struct ScheduleClockReading {
    unsigned year = 0, month = 0, day = 0, minute = 0, millisecondsInMinute = 0;
    std::uint64_t utcMilliseconds = 0, awakeMilliseconds = 0, captureMilliseconds = 0, startedTick = 0;
    ScheduleZoneIdentity zone;
};
enum class ScheduleClockStage { None, AwakeClock, TimeZone, UtcConversion, LocalConversion, InvalidData, CaptureTooSlow };
struct ScheduleClockResult {
    std::optional<ScheduleClockReading> reading;
    ScheduleClockStage stage = ScheduleClockStage::None;
    // Only APIs documenting GetLastError supply this. AwakeClock has no such contract.
    std::uint32_t nativeError = 0;
};
ScheduleClockResult ReadScheduleClock() noexcept;
