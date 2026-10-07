#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace HighGainGuitarFinisher {

struct DecodedAudioFile {
    std::uint32_t sampleRate {0};
    std::uint32_t channels {0};
    std::uint64_t frameCount {0};
    std::vector<float> interleaved {};
};

class AudioFileDecoder {
public:
    static bool decode(
        const std::filesystem::path& path,
        DecodedAudioFile& audio,
        std::string& error);

    static bool supportedExtension(
        const std::filesystem::path& path);
};

} // namespace HighGainGuitarFinisher
