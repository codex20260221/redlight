#include "VerificationJson.h"
#include <charconv>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace {
constexpr const char* states[] = {"not-checked", "inactive", "matches", "changed", "unavailable"};
static_assert(std::size(states) == static_cast<unsigned>(BackendVerification::Unavailable) + 1);
bool ValidLocation(const std::optional<BackendValueLocation>& location, bool gamma) noexcept {
    return !location || (gamma ? location->row < 3 && location->column < 256 : location->row < 5 && location->column < 5);
}
bool ValidSnapshot(const VerificationJsonSnapshot& snapshot) noexcept {
    const auto& report = snapshot.report;
    if (static_cast<unsigned>(report.state) >= std::size(states)) return false;
    if (report.state == BackendVerification::NotChecked)
        return !snapshot.ageSeconds && !snapshot.durationMilliseconds && !report.readback && report.errorCode == 0;
    if (!snapshot.ageSeconds || !snapshot.durationMilliseconds) return false;
    if (report.state == BackendVerification::Unavailable) return !report.readback;
    if (report.errorCode) return false;
    if (report.state == BackendVerification::Inactive) return !report.readback;
    if (!report.readback) return false;
    const auto& detail = *report.readback;
    if (detail.gammaRamp != snapshot.gammaBackend || detail.values != (snapshot.gammaBackend ? 768u : 25u) || detail.mismatches > detail.values ||
        !std::isfinite(detail.maximumDifference) || detail.maximumDifference < 0 || (detail.gammaRamp && detail.maximumDifference > 1) ||
        (report.state == BackendVerification::Matches) != (detail.mismatches == 0) ||
        (detail.mismatches != 0) != detail.firstMismatch.has_value() || (detail.maximumDifference > 0) != detail.maximumDifferenceAt.has_value() ||
        !ValidLocation(detail.firstMismatch, detail.gammaRamp) || !ValidLocation(detail.maximumDifferenceAt, detail.gammaRamp)) return false;
    if (detail.gammaRamp ? detail.nonFiniteValues : (detail.redBlackEndpointMismatch || detail.redWhiteEndpointMismatch)) return false;
    if ((detail.nonFiniteValues || detail.unexpectedGreenBlue || detail.redBlackEndpointMismatch || detail.redWhiteEndpointMismatch) && !detail.mismatches) return false;
    return !detail.mismatches || detail.maximumDifference > 0 || detail.nonFiniteValues;
}
bool FormatUnsigned(std::optional<std::uint64_t> value, char (&output)[32]) noexcept {
    if (!value) { output[0] = 'n'; output[1] = 'u'; output[2] = 'l'; output[3] = 'l'; output[4] = '\0'; return true; }
    const auto result = std::to_chars(output, output + sizeof(output) - 1, *value);
    if (result.ec != std::errc{}) return false;
    *result.ptr = '\0'; return true;
}
bool FormatLocation(const std::optional<BackendValueLocation>& location, bool gamma, char (&output)[64]) noexcept {
    const int length = !location ? std::snprintf(output, sizeof(output), "null") :
        std::snprintf(output, sizeof(output), gamma ? "{\"channel\":%u,\"entry\":%u}" : "{\"row\":%u,\"column\":%u}", location->row, location->column);
    return length >= 0 && static_cast<std::size_t>(length) < sizeof(output);
}
}

std::optional<std::string_view> FormatVerificationJson(const VerificationJsonSnapshot& snapshot, std::span<char> output) noexcept {
    if (output.empty() || !ValidSnapshot(snapshot)) return std::nullopt;
    char age[32], duration[32];
    if (!FormatUnsigned(snapshot.ageSeconds, age) || !FormatUnsigned(snapshot.durationMilliseconds, duration)) return std::nullopt;
    std::size_t length = 0;
    const auto append = [&](const char* format, auto... args) {
        const int written = std::snprintf(output.data() + length, output.size() - length, format, args...);
        if (written < 0 || static_cast<std::size_t>(written) >= output.size() - length) return false;
        length += static_cast<std::size_t>(written); return true;
    };
    if (!append("{\"protocol\":%u,\"status\":\"verification\",\"backend\":\"%s\",\"scope\":\"last-explicit-check\",\"verification\":\"%s\","
        "\"age_seconds\":%s,\"duration_ms\":%s,\"read_error\":%u,\"readback\":", VerificationJsonProtocolVersion, snapshot.gammaBackend ? "gamma" : "magnification",
        states[static_cast<unsigned>(snapshot.report.state)], age, duration, snapshot.report.errorCode)) return std::nullopt;
    if (snapshot.report.readback) {
        const auto& detail = *snapshot.report.readback;
        char maximum[64], first[64], maximumAt[64];
        // to_chars is locale independent and preserves the finite double's round-trip value.
        const auto number = std::to_chars(maximum, maximum + sizeof(maximum) - 1, detail.maximumDifference == 0 ? 0.0 : detail.maximumDifference);
        if (number.ec != std::errc{} || !FormatLocation(detail.firstMismatch, detail.gammaRamp, first) ||
            !FormatLocation(detail.maximumDifferenceAt, detail.gammaRamp, maximumAt)) return std::nullopt;
        *number.ptr = '\0';
        if (!append("{\"values\":%u,\"mismatches\":%u,\"maximum_difference\":%s,\"difference_unit\":\"%s\",\"nonfinite_values\":%s,"
            "\"unexpected_green_blue\":%s,\"red_black_endpoint_mismatch\":%s,\"red_white_endpoint_mismatch\":%s,\"first_mismatch\":%s,\"maximum_difference_at\":%s}",
            detail.values, detail.mismatches, maximum, detail.gammaRamp ? "gamma-full-scale" : "matrix-coefficient", detail.nonFiniteValues ? "true" : "false",
            detail.unexpectedGreenBlue ? "true" : "false", detail.redBlackEndpointMismatch ? "true" : "false", detail.redWhiteEndpointMismatch ? "true" : "false", first, maximumAt)) return std::nullopt;
    } else if (!append("null")) return std::nullopt;
    if (!append("}\n")) return std::nullopt;
    return std::string_view(output.data(), length);
}
