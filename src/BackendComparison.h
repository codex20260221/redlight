#pragma once
#include "DisplayFilter.h"
#include <cmath>

namespace BackendComparison {
inline bool FiniteMatrix(const float (&matrix)[5][5]) noexcept {
    for (const auto& row : matrix) for (const float value : row) if (!std::isfinite(value)) return false;
    return true;
}
inline BackendReadbackDetails Matrix(const float (&actual)[5][5], const float (&expected)[5][5]) noexcept {
    BackendReadbackDetails result;
    for (int row = 0; row < 5; ++row) for (int column = 0; column < 5; ++column) {
        ++result.values;
        const float found = actual[row][column], wanted = expected[row][column];
        if ((column == 1 || column == 2) && wanted == 0 && found != 0) result.unexpectedGreenBlue = true;
        if (!std::isfinite(found) || !std::isfinite(wanted)) {
            result.nonFiniteValues = true;
            ++result.mismatches;
            continue;
        }
        const double difference = std::abs(static_cast<double>(found) - static_cast<double>(wanted));
        if (difference > result.maximumDifference) result.maximumDifference = difference;
        // Preserve the original float tolerance; every expected zero must stay exactly zero.
        if (wanted == 0 ? found != 0 : std::abs(found - wanted) > 0.000001f) ++result.mismatches;
    }
    return result;
}
inline BackendReadbackDetails Gamma(const std::uint16_t (&actual)[3][256], const std::uint16_t (&expected)[3][256], bool requireZeroGreenBlue) noexcept {
    BackendReadbackDetails result;
    result.gammaRamp = true;
    for (int channel = 0; channel < 3; ++channel) for (int i = 0; i < 256; ++i) {
        ++result.values;
        const bool unexpected = requireZeroGreenBlue && channel > 0 && actual[channel][i] != 0;
        result.unexpectedGreenBlue = result.unexpectedGreenBlue || unexpected;
        if (unexpected || (actual[channel][i] >> 8) != (expected[channel][i] >> 8)) ++result.mismatches;
        const double difference = std::abs(static_cast<double>(actual[channel][i]) - static_cast<double>(expected[channel][i])) / 65535.0;
        if (difference > result.maximumDifference) result.maximumDifference = difference;
    }
    return result;
}
}
