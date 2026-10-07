#pragma once

#include "AdaptiveBandController.h"
#include "AdaptiveResonanceSuppressor.h"
#include "AutoLevelCompensator.h"
#include "Biquad.h"
#include "LowCutMapping.h"
#include "ToneMatchDSP.h"
#include "ToneMatchProfile.h"

#include <array>

namespace HighGainGuitarFinisher::dsp {

class MetalFinisherDSP {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void setFinish(double normalized) noexcept;
    void setMass(double normalized) noexcept;
    void setLowCut(double normalized) noexcept;
    void setToneMatchAmount(double normalized) noexcept;
    void setToneMatchProfile(const ToneMatchProfile& profile) noexcept;
    void clearToneMatchProfile() noexcept;
    void setMode(double normalized) noexcept;

    void processFrame(double& left, double& right) noexcept;

    double currentDynamicLowEndReduction() const noexcept {
        return lowEnd_.currentReduction();
    }

    double currentBodyReduction() const noexcept {
        return body_.currentReduction();
    }

    double currentArticulationCorrection() const noexcept {
        return articulation_.currentReduction();
    }

    double currentArticulationDominance() const noexcept {
        return articulation_.currentDominance();
    }

    double currentHarshnessReduction() const noexcept {
        return harshness_.currentReduction();
    }

    double currentFizzReduction() const noexcept {
        return fizz_.currentReduction();
    }

    double currentResonanceReduction() const noexcept {
        return resonanceSuppressor_.
            currentMaximumReduction();
    }

    double detectedResonanceFrequency() const noexcept {
        return resonanceSuppressor_.
            primaryFrequency();
    }

    double currentAutoLevelGainDb() const noexcept {
        return autoLevel_.currentGainDb();
    }

    double detectedLowFrequency() const noexcept {
        return lowEnd_.selectedFrequency();
    }

    double detectedBodyFrequency() const noexcept {
        return body_.selectedFrequency();
    }

    double detectedArticulationFrequency() const noexcept {
        return articulation_.selectedFrequency();
    }

    double detectedHarshnessFrequency() const noexcept {
        return harshness_.selectedFrequency();
    }

    double detectedFizzFrequency() const noexcept {
        return fizz_.selectedFrequency();
    }

private:
    std::array<Biquad, 2> lowCut_ {};
    std::array<Biquad, 2> makeupLowShelf_ {};
    std::array<Biquad, 2> makeupHighShelf_ {};
    std::array<Biquad, 2> massBoost_ {};
    std::array<Biquad, 2> massCleanup_ {};

    // LOW CONTROL detector and dynamic sub-containment path.
    std::array<Biquad, 2> lowControlSubDetector_ {};
    std::array<Biquad, 2> lowControlBodyDetector_ {};
    std::array<Biquad, 2> lowControlDynamicShelf_ {};

    // MASS harmonic support is band-limited so it creates useful upper
    // harmonics from the bass body instead of distorting the whole spectrum.
    std::array<Biquad, 2> massHarmonicHighPass_ {};
    std::array<Biquad, 2> massHarmonicLowPass_ {};

    // FINISH saturation is restricted to the definition band so fundamental
    // weight is preserved and upper-bass harmonics become more readable.
    std::array<Biquad, 2> finishSaturationHighPass_ {};
    std::array<Biquad, 2> finishSaturationLowPass_ {};

    AdaptiveBandController lowEnd_ {};
    AdaptiveBandController body_ {};
    AdaptiveBandController articulation_ {};
    AdaptiveBandController harshness_ {};
    AdaptiveBandController fizz_ {};
    AdaptiveResonanceSuppressor resonanceSuppressor_ {};
    AutoLevelCompensator autoLevel_ {};
    ToneMatchDSP toneMatch_ {};

    double sampleRate_ {44100.0};
    double finish_ {0.0};
    double mass_ {0.0};
    double toneMatchAmount_ {0.0};

    // Automatic operating-level conditioning. It is dormant for the exact
    // neutral path and only engages when a production stage is active.
    double inputPower_ {0.0};
    double inputGain_ {1.0};
    double inputPowerAttack_ {0.0};
    double inputPowerRelease_ {0.0};
    double inputGainSmoothing_ {0.0};

    // Linked stereo FINAL peak manager. Below the knee it is mathematically
    // transparent; only peaks approaching full scale are controlled.
    double finalGain_ {1.0};
    double finalRelease_ {0.0};
    double massTrimGain_ {0.9332543007969910};

    double modeTarget_ {0.0};
    std::array<double, 5> modeWeights_ {1.0, 1.0, 1.0, 1.0, 1.0};
    std::array<double, 5> modeWeightTargets_ {1.0, 1.0, 1.0, 1.0, 1.0};
    double modeSmoothing_ {0.0};

    double lowCutTarget_ {0.0};
    double lowCutFrequencyHz_ {kLowCutMinimumHz};
    double lowCutMix_ {0.0};
    double lowCutMixSmoothing_ {0.0};
    double lowCutFrequencySmoothing_ {0.0};

    double lowControlSubPower_ {0.0};
    double lowControlBodyPower_ {0.0};
    double lowControlEnvelopeSmoothing_ {0.0};
    double lowControlDynamicGainDb_ {0.0};
    int lowControlCoefficientCountdown_ {0};

    int lowCutCoefficientCountdown_ {0};
    int massCoefficientCountdown_ {0};
    int makeupShelfCoefficientCountdown_ {0};

    void updateLowCutCoefficients() noexcept;
    void updateLowControlCoefficients() noexcept;
    void updateMassCoefficients() noexcept;
    void updateMakeupShelfCoefficients() noexcept;
    void updateModeTargets() noexcept;
    void applyAutoInput(double& left, double& right) noexcept;
    void applyFinal(double& left, double& right) noexcept;
};

} // namespace HighGainGuitarFinisher::dsp
