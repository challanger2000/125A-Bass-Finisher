#include "ToneMatchReferenceService.h"

#include <algorithm>
#include <fstream>
#include <vector>

namespace HighGainGuitarFinisher {

bool ToneMatchReferenceService::analyzeAudioFile(
    const std::filesystem::path& path,
    dsp::ToneMatchSpectrumSnapshot& snapshot,
    std::string& error) {

    DecodedAudioFile audio;

    if (!AudioFileDecoder::decode(
            path,
            audio,
            error)) {
        return false;
    }

    dsp::ToneMatchAnalyzer analyzer;
    analyzer.prepare(
        static_cast<double>(
            audio.sampleRate));

    constexpr std::size_t kChunkFrames = 4096;
    std::vector<double> left(kChunkFrames);
    std::vector<double> right(kChunkFrames);

    std::uint64_t frame = 0;

    while (frame < audio.frameCount) {
        const auto remaining =
            audio.frameCount - frame;

        const auto count =
            static_cast<std::size_t>(
                std::min<std::uint64_t>(
                    remaining,
                    kChunkFrames));

        for (std::size_t i = 0;
             i < count;
             ++i) {

            const auto base =
                static_cast<std::size_t>(
                    frame + i) *
                static_cast<std::size_t>(
                    audio.channels);

            if (audio.channels == 1) {
                const double sample =
                    audio.interleaved[base];

                left[i] = sample;
                right[i] = sample;
            } else if (audio.channels == 2) {
                left[i] =
                    audio.interleaved[base];

                right[i] =
                    audio.interleaved[base + 1];
            } else {
                double sum = 0.0;

                for (std::uint32_t channel = 0;
                     channel < audio.channels;
                     ++channel) {
                    sum +=
                        audio.interleaved[
                            base + channel];
                }

                const double mono =
                    sum /
                    static_cast<double>(
                        audio.channels);

                left[i] = mono;
                right[i] = mono;
            }
        }

        analyzer.pushStereo(
            left.data(),
            right.data(),
            count);

        frame += count;
    }

    if (!analyzer.hasEnoughData()) {
        error =
            "Reference audio is too short for analysis";
        return false;
    }

    snapshot =
        analyzer.snapshot();

    error.clear();
    return true;
}

bool ToneMatchReferenceService::saveReferenceProfile(
    const std::filesystem::path& path,
    const dsp::ToneMatchSpectrumSnapshot& snapshot,
    std::string& error) {

    if (snapshot.frameCount < 4u) {
        error = "No valid reference profile to save";
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary |
        std::ios::trunc);

    if (!output) {
        error = "Cannot create reference profile";
        return false;
    }

    const auto text =
        ToneMatchReferenceProfileCodec::encode(
            snapshot);

    output.write(
        text.data(),
        static_cast<std::streamsize>(
            text.size()));

    if (!output) {
        error = "Cannot write reference profile";
        return false;
    }

    error.clear();
    return true;
}

bool ToneMatchReferenceService::loadReferenceProfile(
    const std::filesystem::path& path,
    dsp::ToneMatchSpectrumSnapshot& snapshot,
    std::string& error) {

    std::ifstream input(
        path,
        std::ios::binary |
        std::ios::ate);

    if (!input) {
        error = "Cannot open reference profile";
        return false;
    }

    const auto end =
        input.tellg();

    if (end <= 0 ||
        end >
            static_cast<std::streamoff>(
                16 * 1024 * 1024)) {
        error = "Invalid reference profile size";
        return false;
    }

    const auto size =
        static_cast<std::size_t>(
            static_cast<std::streamoff>(
                end));

    std::string text(
        size,
        '\0');

    input.seekg(
        0,
        std::ios::beg);

    if (!input.read(
            text.data(),
            static_cast<std::streamsize>(
                text.size()))) {
        error = "Cannot read reference profile";
        return false;
    }

    if (!ToneMatchReferenceProfileCodec::decode(
            text,
            snapshot)) {
        error = "Invalid or unsupported reference profile";
        return false;
    }

    error.clear();
    return true;
}

} // namespace HighGainGuitarFinisher
