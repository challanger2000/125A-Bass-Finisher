#include "AudioFileDecoder.h"

#include "third_party/dr_libs/dr_wav.h"
#include "third_party/dr_libs/dr_flac.h"
#include "third_party/dr_libs/dr_mp3.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace HighGainGuitarFinisher {

namespace {

std::string lowerExtension(
    const std::filesystem::path& path) {

    auto extension =
        path.extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return extension;
}

bool readWholeFile(
    const std::filesystem::path& path,
    std::vector<unsigned char>& bytes,
    std::string& error) {

    std::ifstream input(
        path,
        std::ios::binary |
        std::ios::ate);

    if (!input) {
        error = "Cannot open reference file";
        return false;
    }

    const auto end =
        input.tellg();

    if (end <= 0) {
        error = "Reference file is empty";
        return false;
    }

    const auto size =
        static_cast<std::uint64_t>(
            static_cast<std::streamoff>(
                end));

    constexpr std::uint64_t kMaximumBytes =
        1024ull * 1024ull * 1024ull;

    if (size > kMaximumBytes ||
        size >
            static_cast<std::uint64_t>(
                std::numeric_limits<
                    std::size_t>::max())) {
        error = "Reference file is too large";
        return false;
    }

    bytes.resize(
        static_cast<std::size_t>(
            size));

    input.seekg(
        0,
        std::ios::beg);

    if (!input.read(
            reinterpret_cast<char*>(
                bytes.data()),
            static_cast<std::streamsize>(
                bytes.size()))) {
        error = "Cannot read reference file";
        bytes.clear();
        return false;
    }

    return true;
}

bool assignDecoded(
    const float* samples,
    std::uint32_t channels,
    std::uint32_t sampleRate,
    std::uint64_t frameCount,
    DecodedAudioFile& audio,
    std::string& error) {

    if (!samples ||
        channels == 0 ||
        channels > 64 ||
        sampleRate < 8000 ||
        sampleRate > 768000 ||
        frameCount == 0) {

        error = "Unsupported or invalid audio file";
        return false;
    }

    if (frameCount >
        std::numeric_limits<std::size_t>::max() /
            channels) {

        error = "Decoded audio is too large";
        return false;
    }

    const auto sampleCount =
        static_cast<std::size_t>(
            frameCount) *
        static_cast<std::size_t>(
            channels);

    try {
        audio.interleaved.assign(
            samples,
            samples + sampleCount);
    } catch (...) {
        error = "Not enough memory for reference audio";
        audio = {};
        return false;
    }

    audio.sampleRate = sampleRate;
    audio.channels = channels;
    audio.frameCount = frameCount;

    return true;
}

}

bool AudioFileDecoder::supportedExtension(
    const std::filesystem::path& path) {

    const auto ext =
        lowerExtension(path);

    return ext == ".wav" ||
        ext == ".wave" ||
        ext == ".flac" ||
        ext == ".mp3";
}

bool AudioFileDecoder::decode(
    const std::filesystem::path& path,
    DecodedAudioFile& audio,
    std::string& error) {

    audio = {};
    error.clear();

    if (!supportedExtension(path)) {
        error = "Supported reference formats: WAV, FLAC, MP3";
        return false;
    }

    std::vector<unsigned char> bytes;

    if (!readWholeFile(
            path,
            bytes,
            error)) {
        return false;
    }

    const auto ext =
        lowerExtension(path);

    if (ext == ".wav" ||
        ext == ".wave") {

        unsigned int channels = 0;
        unsigned int sampleRate = 0;
        drwav_uint64 frames = 0;

        float* samples =
            drwav_open_memory_and_read_pcm_frames_f32(
                bytes.data(),
                bytes.size(),
                &channels,
                &sampleRate,
                &frames,
                nullptr);

        const bool ok =
            assignDecoded(
                samples,
                channels,
                sampleRate,
                frames,
                audio,
                error);

        drwav_free(
            samples,
            nullptr);

        return ok;
    }

    if (ext == ".flac") {
        unsigned int channels = 0;
        unsigned int sampleRate = 0;
        drflac_uint64 frames = 0;

        float* samples =
            drflac_open_memory_and_read_pcm_frames_f32(
                bytes.data(),
                bytes.size(),
                &channels,
                &sampleRate,
                &frames,
                nullptr);

        const bool ok =
            assignDecoded(
                samples,
                channels,
                sampleRate,
                frames,
                audio,
                error);

        drflac_free(
            samples,
            nullptr);

        return ok;
    }

    drmp3_config config {};
    drmp3_uint64 frames = 0;

    float* samples =
        drmp3_open_memory_and_read_pcm_frames_f32(
            bytes.data(),
            bytes.size(),
            &config,
            &frames,
            nullptr);

    const bool ok =
        assignDecoded(
            samples,
            config.channels,
            config.sampleRate,
            frames,
            audio,
            error);

    drmp3_free(
        samples,
        nullptr);

    return ok;
}

} // namespace HighGainGuitarFinisher
