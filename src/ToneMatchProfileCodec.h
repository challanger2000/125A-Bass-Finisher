#pragma once

#include "dsp/ToneMatchProfile.h"

#include <string>

namespace HighGainGuitarFinisher {

class ToneMatchProfileCodec {
public:
    static constexpr int kFileVersion = 2;

    static std::string encode(
        const dsp::ToneMatchProfile& profile);

    static bool decode(
        const std::string& text,
        dsp::ToneMatchProfile& profile) noexcept;
};

} // namespace HighGainGuitarFinisher
