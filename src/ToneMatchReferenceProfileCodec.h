#pragma once

#include "dsp/ToneMatchAnalyzer.h"

#include <string>

namespace HighGainGuitarFinisher {

class ToneMatchReferenceProfileCodec {
public:
    static constexpr int kFileVersion = 1;

    static std::string encode(
        const dsp::ToneMatchSpectrumSnapshot& snapshot);

    static bool decode(
        const std::string& text,
        dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept;
};

} // namespace HighGainGuitarFinisher
