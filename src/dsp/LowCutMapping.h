#pragma once

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

constexpr double kLowCutMinimumHz = 45.0;
constexpr double kLowCutMaximumHz = 120.0;
constexpr double kLowCutOnsetNormalized = 0.01;

inline bool lowCutEnabled(double normalized) noexcept {
    return std::isfinite(normalized) &&
        normalized >= kLowCutOnsetNormalized;
}

inline double lowCutFrequencyFromNormalized(
    double normalized) noexcept {

    if (!lowCutEnabled(normalized))
        return kLowCutMinimumHz;

    const double active =
        std::clamp(
            (normalized - kLowCutOnsetNormalized) /
                (1.0 - kLowCutOnsetNormalized),
            0.0,
            1.0);

    return
        kLowCutMinimumHz +
        active *
            (kLowCutMaximumHz - kLowCutMinimumHz);
}

inline double lowCutNormalizedFromFrequency(
    double frequencyHz) noexcept {

    if (!std::isfinite(frequencyHz) ||
        frequencyHz <= 0.0) {
        return 0.0;
    }

    const double frequency =
        std::clamp(
            frequencyHz,
            kLowCutMinimumHz,
            kLowCutMaximumHz);

    const double active =
        (frequency - kLowCutMinimumHz) /
        (kLowCutMaximumHz - kLowCutMinimumHz);

    return
        kLowCutOnsetNormalized +
        active *
            (1.0 - kLowCutOnsetNormalized);
}

} // namespace HighGainGuitarFinisher::dsp
