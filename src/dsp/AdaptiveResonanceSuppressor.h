#pragma once

#include "Biquad.h"

#include <array>
#include <cstddef>

namespace HighGainGuitarFinisher::dsp {

class AdaptiveResonanceSuppressor {
public:
    static constexpr std::size_t kBandCount = 10;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void processFrame(double& left, double& right) noexcept;

    double currentMaximumReduction() const noexcept;
    double primaryFrequency() const noexcept;

private:
    static double timeCoefficient(
        double sampleRate,
        double milliseconds) noexcept;

    void updateTargets() noexcept;

    static constexpr std::array<double, kBandCount>
        kCenters {
            1500.0, 1800.0, 2200.0, 2700.0, 3300.0,
            4000.0, 4800.0, 5800.0, 6800.0, 8000.0
        };

    std::array<std::array<Biquad, kBandCount>, 2> detectors_ {};
    std::array<double, kBandCount> slowEnergy_ {};
    std::array<double, kBandCount> reduction_ {};
    std::array<double, kBandCount> targetReduction_ {};

    double sampleRate_ {44100.0};
    double wideEnergy_ {0.0};

    double energyAttack_ {0.0};
    double energyRelease_ {0.0};
    double wideAttack_ {0.0};
    double wideRelease_ {0.0};
    double reductionAttack_ {0.0};
    double reductionRelease_ {0.0};

    std::size_t updateCounter_ {0};
};

} // namespace HighGainGuitarFinisher::dsp
