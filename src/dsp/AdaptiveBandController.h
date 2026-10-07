#pragma once

#include "Biquad.h"

#include <array>
#include <cstddef>

namespace HighGainGuitarFinisher::dsp {

enum class AdaptiveBandMode {
    LowTransient,
    BodyResonance,
    ArticulationSupport,
    Harshness,
    Fizz
};

class AdaptiveBandController {
public:
    static constexpr std::size_t kBandCount = 4;
    using Centers = std::array<double, kBandCount>;

    void prepare(double sampleRate,
                 AdaptiveBandMode mode,
                 const Centers& centers,
                 double q) noexcept;

    void reset() noexcept;
    void processFrame(double& left, double& right) noexcept;

    double currentReduction() const noexcept {
        return reduction_;
    }

    double selectedFrequency() const noexcept;

    double currentDominance() const noexcept {
        return dominance_;
    }

private:
    static double timeCoefficient(double sampleRate,
                                  double milliseconds) noexcept;

    void updateSelection() noexcept;

    std::array<std::array<Biquad, kBandCount>, 2> filters_ {};
    Centers centers_ {};
    std::array<double, kBandCount> fastEnergy_ {};
    std::array<double, kBandCount> slowEnergy_ {};
    std::array<double, kBandCount> weights_ {};

    AdaptiveBandMode mode_ {AdaptiveBandMode::BodyResonance};

    double sampleRate_ {44100.0};
    double wideFastEnergy_ {0.0};
    double wideSlowEnergy_ {0.0};
    double reduction_ {0.0};
    double dominance_ {0.0};

    double fastAttack_ {0.0};
    double fastRelease_ {0.0};
    double slowAttack_ {0.0};
    double slowRelease_ {0.0};
    double wideFastAttack_ {0.0};
    double wideFastRelease_ {0.0};
    double wideSlowAttack_ {0.0};
    double wideSlowRelease_ {0.0};
    double reductionAttack_ {0.0};
    double reductionRelease_ {0.0};
    double weightSmoothing_ {0.0};

    std::size_t selected_ {0};
    std::size_t selectionCounter_ {0};
};

} // namespace HighGainGuitarFinisher::dsp
