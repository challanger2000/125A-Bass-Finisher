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

    // Legacy linear-frequency power spectrum retained for V1/V2 state/profile
    // compatibility and synthetic fixtures.
    std::array<double, 2049> meanPower {};

    // V3 tonal signature: time-averaged, frame-level-normalized dB values on
    // a logarithmic 30 Hz .. 12 kHz grid. This is the primary MATCH curve.
    bool hasLogCurve {false};
    std::array<double, 512> meanDb {};
};

class ToneMatchAnalyzer {
public:
    // kFftSize/kSpectrumBins define the persisted legacy spectrum contract.
    // Analysis itself uses a 4x larger FFT so the low end is not quantized
    // into ~11.7 Hz bins at 48 kHz.
    static constexpr std::size_t kFftSize = 4096;
    static constexpr std::size_t kSpectrumBins = kFftSize / 2 + 1;
    static constexpr std::size_t kAnalysisFftSize = 16384;
    static constexpr std::size_t kHopSize = kAnalysisFftSize / 2;
    static constexpr std::size_t kCurveBins = 512;
    static constexpr double kCurveMinimumHz = 30.0;
    static constexpr double kCurveMaximumHz = 12000.0;

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
        std::array<std::complex<double>, kAnalysisFftSize>& data) noexcept;

    double interpolatedMagnitudeDb(
        double frequencyHz) const noexcept;

    double peakMagnitudeDb() const noexcept;

    double sampleRate_ {44100.0};
    std::array<double, kAnalysisFftSize> fifo_ {};
    std::size_t fifoFill_ {0};

    std::array<long double, kSpectrumBins> powerSum_ {};
    std::array<long double, kCurveBins> curvePowerSum_ {};
    std::array<double, kCurveBins> meanDb_ {};
    std::size_t frameCount_ {0};
};

} // namespace HighGainGuitarFinisher::dsp
