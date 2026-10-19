#pragma once
#include "DisplayFilter.h"
#include <span>
#include <string_view>

namespace MonitorPrototype {
struct ProbeOptions {
    unsigned monitor = 0, seconds = 0, brightness = 100;
    RedMode mode = RedMode::LumaRed;
    LumaProfile profile = LumaProfile::Classic;
    bool valid() const noexcept {
        return monitor >= 1 && monitor <= 8 && seconds >= 1 && seconds <= 30 && ValidRedBrightness(brightness) &&
            (mode == RedMode::StrictRed || mode == RedMode::LumaRed) && IsValidLumaProfile(static_cast<unsigned>(profile));
    }
};
enum class ProbeRequest { Help, Run, Invalid };
struct ProbeParseResult { ProbeRequest request = ProbeRequest::Invalid; ProbeOptions options; };
inline ProbeParseResult ParseProbeOptions(std::span<const std::wstring_view> arguments) noexcept {
    if (arguments.empty() || (arguments.size() == 1 && arguments[0] == L"--help")) return {ProbeRequest::Help};
    if (arguments.size() > 12 || arguments.size() % 2) return {};
    ProbeOptions options; unsigned seen = 0;
    const auto number = [](std::wstring_view text, unsigned maximum) -> unsigned {
        if (text.empty()) return 0;
        unsigned value = 0;
        for (const auto ch : text) {
            if (ch < L'0' || ch > L'9') return 0;
            value = value * 10 + static_cast<unsigned>(ch - L'0');
            if (value > maximum) return 0;
        }
        return value;
    };
    for (std::size_t index = 0; index < arguments.size(); index += 2) {
        const auto key = arguments[index], value = arguments[index + 1]; unsigned bit = 0;
        if (key == L"--monitor") { bit = 1; options.monitor = number(value, 8); }
        else if (key == L"--seconds") { bit = 2; options.seconds = number(value, 30); }
        else if (key == L"--brightness") { bit = 4; options.brightness = number(value, 100); }
        else if (key == L"--mode") {
            bit = 8;
            if (value == L"strict") options.mode = RedMode::StrictRed;
            else if (value == L"luma") options.mode = RedMode::LumaRed;
            else return {};
        } else if (key == L"--luma") {
            bit = 16;
            if (value == L"classic") options.profile = LumaProfile::Classic;
            else if (value == L"green") options.profile = LumaProfile::GreenDetail;
            else if (value == L"equal") options.profile = LumaProfile::EqualDetail;
            else return {};
        } else return {};
        if (seen & bit) return {};
        seen |= bit;
    }
    return (seen & 3) == 3 && options.valid() ? ProbeParseResult{ProbeRequest::Run, options} : ProbeParseResult{};
}
}
