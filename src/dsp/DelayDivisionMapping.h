#pragma once

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

constexpr int kDelayDivisionCount = 6;
constexpr double kDefaultDelayDivisionNormalized = 3.0 / 5.0;

inline int delayDivisionIndexFromNormalized(double normalized) noexcept {
    const double value =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : kDefaultDelayDivisionNormalized,
            0.0,
            1.0);

    return std::clamp(
        static_cast<int>(
            std::llround(
                value *
                static_cast<double>(
                    kDelayDivisionCount - 1))),
        0,
        kDelayDivisionCount - 1);
}

inline double delayDivisionNormalizedFromIndex(int index) noexcept {
    const int safe =
        std::clamp(
            index,
            0,
            kDelayDivisionCount - 1);

    return
        static_cast<double>(safe) /
        static_cast<double>(
            kDelayDivisionCount - 1);
}

inline double migrateLegacyDelayDivisionNormalized(
    double legacyNormalized) noexcept {

    const double value =
        std::clamp(
            std::isfinite(legacyNormalized)
                ? legacyNormalized
                : 1.0 / 3.0,
            0.0,
            1.0);

    const int legacyIndex =
        std::clamp(
            static_cast<int>(
                std::llround(
                    value * 3.0)),
            0,
            3);

    return delayDivisionNormalizedFromIndex(
        legacyIndex + 2);
}

} // namespace HighGainGuitarFinisher::dsp
