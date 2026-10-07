#pragma once

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::automation {

inline double linearValueAtSample(
    int sampleOffset,
    int startOffset,
    double startValue,
    int endOffset,
    double endValue) noexcept {

    if (!std::isfinite(startValue))
        startValue = 0.0;

    if (!std::isfinite(endValue))
        endValue = startValue;

    if (endOffset <= startOffset)
        return endValue;

    const double fraction =
        std::clamp(
            static_cast<double>(
                sampleOffset - startOffset) /
                static_cast<double>(
                    endOffset - startOffset),
            0.0,
            1.0);

    return startValue +
        fraction *
            (endValue - startValue);
}

} // namespace HighGainGuitarFinisher::automation
