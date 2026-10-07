#pragma once

#include <array>
#include <cstddef>

namespace HighGainGuitarFinisher::dsp {

constexpr std::size_t kToneMatchPeakCount = 64;
constexpr std::size_t kToneMatchFirTapCount = 4096;

struct ToneMatchPeak {
    double frequencyHz {1000.0};
    double q {1.0};
    double gainDb {0.0};
};

struct ToneMatchProfile {
    bool valid {false};

    // Direct-curve MATCH path. Legacy IIR fields remain for backwards
    // compatibility with saved V1/V2/V3 projects and old profile files.
    bool firValid {false};
    std::array<double, kToneMatchFirTapCount> firTaps {};

    double lowShelfFrequencyHz {100.0};
    double lowShelfGainDb {0.0};

    std::array<ToneMatchPeak, kToneMatchPeakCount> peaks {};

    double highShelfFrequencyHz {9000.0};
    double highShelfGainDb {0.0};
};

} // namespace HighGainGuitarFinisher::dsp
