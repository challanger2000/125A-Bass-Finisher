#pragma once

#include "AdaptiveBandController.h"
#include "AdaptiveResonanceSuppressor.h"
#include "AutoLevelCompensator.h"
#include "Biquad.h"
#include "IndustrialRoom.h"
#include "LeadDelay.h"
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
    void setRoomWet(double normalized) noexcept;
    void setRoomDecay(double normalized) noexcept;
    void setDelayWet(double normalized) noexcept;
    void setDelayFeedback(double normalized) noexcept;
    void setDelayDivision(double normalized) noexcept;
    void setTempo(double bpm) noexcept;
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

    AdaptiveBandController lowEnd_ {};
    AdaptiveBandController body_ {};
    AdaptiveBandController articulation_ {};
    AdaptiveBandController harshness_ {};
    AdaptiveBandController fizz_ {};
    AdaptiveResonanceSuppressor resonanceSuppressor_ {};
    AutoLevelCompensator autoLevel_ {};
    ToneMatchDSP toneMatch_ {};
    LeadDelay delay_ {};
    IndustrialRoom room_ {};

    double sampleRate_ {44100.0};
    double finish_ {0.0};
    double mass_ {0.0};
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
    int lowCutCoefficientCountdown_ {0};
    int massCoefficientCountdown_ {0};
    int makeupShelfCoefficientCountdown_ {0};

    void updateLowCutCoefficients() noexcept;
    void updateMassCoefficients() noexcept;
    void updateMakeupShelfCoefficients() noexcept;
    void updateModeTargets() noexcept;
};

} // namespace HighGainGuitarFinisher::dsp
