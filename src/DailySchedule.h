#pragma once
#include "SchedulePreferences.h"
#include <chrono>
#include <cstdint>
#include <optional>

// Supplied-time policy; the caller provides a checked UTC/local clock snapshot.
namespace Scheduling {
struct Sample {
    unsigned year = 0, month = 0, day = 0, minute = 0, millisecondsInMinute = 0;
    std::uint64_t utcMilliseconds = 0; // Since 1601-01-01; must match the local conversion.
    std::uint64_t tickMilliseconds = 0;
};
enum class Event { None, Enable, Restore, SkippedEnable, Rebased, InvalidSample };

class DailySchedule {
public:
    bool configure(DailyTimes times) noexcept {
        if (!times.valid()) return false;
        times_ = times;
        previous_.reset(); enableDate_.reset(); restoreDate_.reset(); // Deliberate new observation lifetime.
        return true;
    }
    void invalidate() noexcept { previous_.reset(); } // Preserve consumption across sleep/clock trouble.
    bool rebase(const Sample& sample) noexcept {
        const auto now = normalize(sample);
        if (!now) { invalidate(); return false; }
        previous_ = now;
        skipPast(*now);
        return true;
    }
    Event observe(const Sample& sample, bool allowEnable, bool continuous = true) noexcept {
        const auto now = normalize(sample);
        if (!now) { invalidate(); return Event::InvalidSample; }
        const auto prior = previous_;
        previous_ = now;
        if (!times_.enabled) return Event::None;
        if (!continuous || !prior || !follows(*prior, *now)) {
            skipPast(*now);
            return Event::Rebased;
        }
        if (now->localMinute == prior->localMinute) return Event::None;
        const auto minute = static_cast<unsigned>(now->localMinute % 1440);
        const auto day = static_cast<std::uint32_t>(now->localMinute / 1440);
        if (minute == times_.enableMinute && consume(enableDate_, day)) return allowEnable ? Event::Enable : Event::SkippedEnable;
        if (minute == times_.restoreMinute && consume(restoreDate_, day)) return Event::Restore;
        return Event::None;
    }
private:
    struct Observation {
        std::uint64_t localMinute, utcMilliseconds, tickMilliseconds;
        std::int64_t offsetMilliseconds;
    };
    static std::optional<Observation> normalize(const Sample& value) noexcept {
        if (value.year < 1601 || value.year > 30827 || value.month < 1 || value.month > 12 || value.day < 1 || value.day > 31 ||
            value.minute >= 1440 || value.millisecondsInMinute >= 60000) return std::nullopt;
        using namespace std::chrono;
        const year_month_day date{year{static_cast<int>(value.year)}, month{value.month}, day{value.day}};
        if (!date.ok()) return std::nullopt;
        const auto days = (sys_days{date} - sys_days{year{1601}/January/1}).count();
        const auto localMinute = static_cast<std::uint64_t>(days) * 1440 + value.minute;
        const auto localMilliseconds = localMinute * 60000 + value.millisecondsInMinute;
        constexpr std::uint64_t dayMilliseconds = 86400000;
        if (value.utcMilliseconds > localMilliseconds + dayMilliseconds) return std::nullopt;
        const auto offset = static_cast<std::int64_t>(localMilliseconds) - static_cast<std::int64_t>(value.utcMilliseconds);
        // Windows time-zone biases use whole minutes. Bound a supplied conversion
        // conservatively without encoding a list of geopolitical time zones.
        if (offset < -static_cast<std::int64_t>(dayMilliseconds) || offset > static_cast<std::int64_t>(dayMilliseconds) || offset % 60000 != 0)
            return std::nullopt;
        return Observation{localMinute, value.utcMilliseconds, value.tickMilliseconds, offset};
    }
    static bool follows(const Observation& prior, const Observation& now) noexcept {
        const auto elapsed = now.tickMilliseconds - prior.tickMilliseconds; // Unsigned monotonic wrap.
        if (elapsed > 60000 || now.utcMilliseconds < prior.utcMilliseconds || now.offsetMilliseconds != prior.offsetMilliseconds ||
            now.localMinute < prior.localMinute || now.localMinute > prior.localMinute + 1) return false;
        const auto utcElapsed = now.utcMilliseconds - prior.utcMilliseconds;
        const auto difference = elapsed > utcElapsed ? elapsed - utcElapsed : utcElapsed - elapsed;
        return difference <= 2000; // Clock adjustment tolerance; large jumps rebase.
    }
    static bool consume(std::optional<std::uint32_t>& mark, std::uint32_t day) noexcept {
        if (mark && day <= *mark) return false;
        mark = day;
        return true;
    }
    void skipPast(const Observation& now) noexcept {
        const auto day = static_cast<std::uint32_t>(now.localMinute / 1440);
        const auto minute = static_cast<unsigned>(now.localMinute % 1440);
        const auto skip = [&](std::optional<std::uint32_t>& mark, unsigned boundary) {
            if (minute >= boundary) consume(mark, day);
            else if (day) consume(mark, day - 1);
        };
        skip(enableDate_, times_.enableMinute);
        skip(restoreDate_, times_.restoreMinute);
    }
    DailyTimes times_;
    std::optional<Observation> previous_;
    std::optional<std::uint32_t> enableDate_, restoreDate_;
};
} // namespace Scheduling
