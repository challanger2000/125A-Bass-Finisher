#pragma once

#include "AudioFileDecoder.h"
#include "ToneMatchReferenceProfileCodec.h"
#include "dsp/ToneMatchAnalyzer.h"

#include <filesystem>
#include <string>

namespace HighGainGuitarFinisher {

class ToneMatchReferenceService {
public:
    static bool analyzeAudioFile(
        const std::filesystem::path& path,
        dsp::ToneMatchSpectrumSnapshot& snapshot,
        std::string& error);

    static bool saveReferenceProfile(
        const std::filesystem::path& path,
        const dsp::ToneMatchSpectrumSnapshot& snapshot,
        std::string& error);

    static bool loadReferenceProfile(
        const std::filesystem::path& path,
        dsp::ToneMatchSpectrumSnapshot& snapshot,
        std::string& error);
};

} // namespace HighGainGuitarFinisher
