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
            650.0, 850.0, 1100.0, 1450.0, 1900.0,
            2500.0, 3200.0, 4100.0, 5200.0, 6500.0
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
