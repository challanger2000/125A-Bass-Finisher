#pragma once

#include "ToneMatchProfile.h"

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>

namespace HighGainGuitarFinisher::dsp {

struct ToneMatchSpectrumSnapshot {
    double sampleRate {44100.0};
    std::uint64_t frameCount {0};
    std::array<double, 2049> meanPower {};
};

class ToneMatchAnalyzer {
public:
    static constexpr std::size_t kFftSize = 4096;
    static constexpr std::size_t kHopSize = kFftSize / 2;
    static constexpr std::size_t kSpectrumBins = kFftSize / 2 + 1;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void pushStereo(
        const double* left,
        const double* right,
        std::size_t count) noexcept;

    bool hasEnoughData() const noexcept {
        return frameCount_ >= 4;
    }

    std::size_t frameCount() const noexcept {
        return frameCount_;
    }

    ToneMatchSpectrumSnapshot snapshot() const noexcept;

    ToneMatchProfile makeProfileAgainst(
        const ToneMatchAnalyzer& target) const noexcept;

    static ToneMatchProfile makeProfile(
        const ToneMatchSpectrumSnapshot& reference,
        const ToneMatchSpectrumSnapshot& target) noexcept;

private:
    void processFrame() noexcept;
    static void fft(
        std::array<std::complex<double>, kFftSize>& data) noexcept;

    double interpolatedMagnitudeDb(
        double frequencyHz) const noexcept;

    double peakMagnitudeDb() const noexcept;

    double sampleRate_ {44100.0};
    std::array<double, kFftSize> fifo_ {};
    std::size_t fifoFill_ {0};

    std::array<long double, kSpectrumBins> powerSum_ {};
    std::size_t frameCount_ {0};
};

} // namespace HighGainGuitarFinisher::dsp
