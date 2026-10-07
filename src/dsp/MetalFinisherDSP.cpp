#include "MetalFinisherDSP.h"

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

namespace {
constexpr double kMassBoostHz = 140.0;
constexpr double kMassBoostDb = 10.50;
constexpr double kMassBoostQ = 1.20;
constexpr double kMassCleanupHz = 220.0;
constexpr double kMassCleanupDb = -10.50;
constexpr double kMassCleanupQ = 1.00;
constexpr double kMassTrimDb = -0.60;

// At Low Cut Off the V2 MASS curve is preserved exactly. As the user moves
// the mix-placement high-pass upward, MASS shifts its positive weight above
// the cut and reduces the amount of deep boost. These endpoints are
// EMPIRICALLY TUNED and guarded by measured response tests.
constexpr double kMassBoostShiftHzAtMaxLowCut = 45.0;
constexpr double kMassBoostReductionDbAtMaxLowCut = 4.0;
constexpr double kMassCleanupShiftHzAtMaxLowCut = 80.0;
constexpr double kMassCleanupReductionDbAtMaxLowCut = 3.0;
constexpr double kMassTrimReductionDbAtMaxLowCut = 0.25;

// Emergency numerical guard only. +36.1 dBFS is far beyond the intended
// operating range but keeps hostile/invalid host input from poisoning state.
constexpr double kEmergencyInputLimit = 64.0;
}

void MetalFinisherDSP::prepare(double sampleRate) {
    sampleRate_ = (std::isfinite(sampleRate) && sampleRate > 1000.0)
        ? sampleRate
        : 44100.0;

    lowCutMixSmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.005));

    lowCutFrequencySmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.020));

    modeSmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.030));

    lowEnd_.prepare(
        sampleRate_,
        AdaptiveBandMode::LowTransient,
        {85.0, 110.0, 145.0, 180.0},
        2.5);

    body_.prepare(
        sampleRate_,
        AdaptiveBandMode::BodyResonance,
        {220.0, 300.0, 390.0, 500.0},
        0.9);

    articulation_.prepare(
        sampleRate_,
        AdaptiveBandMode::ArticulationSupport,
        {800.0, 1200.0, 1750.0, 2400.0},
        0.85);

    harshness_.prepare(
        sampleRate_,
        AdaptiveBandMode::Harshness,
        {2800.0, 3600.0, 4500.0, 5600.0},
        1.15);

    fizz_.prepare(
        sampleRate_,
        AdaptiveBandMode::Fizz,
        {6000.0, 7500.0, 9000.0, 11000.0},
        1.0);

    resonanceSuppressor_.
        prepare(sampleRate_);

    const auto unityLowShelf =
        makeLowShelf(
            sampleRate_,
            80.0,
            0.0);

    const auto unityHighShelf =
        makeHighShelf(
            sampleRate_,
            10000.0,
            0.0);

    for (auto& filter : makeupLowShelf_)
        filter.setCoefficients(unityLowShelf);

    for (auto& filter : makeupHighShelf_)
        filter.setCoefficients(unityHighShelf);

    updateMassCoefficients();

    autoLevel_.prepare(sampleRate_);
    toneMatch_.prepare(sampleRate_);
    delay_.prepare(sampleRate_);
    room_.prepare(sampleRate_);

    updateModeTargets();
    reset();
}

void MetalFinisherDSP::reset() noexcept {
    for (auto& filter : lowCut_)
        filter.reset();

    for (auto& filter : makeupLowShelf_)
        filter.reset();

    for (auto& filter : makeupHighShelf_)
        filter.reset();

    for (auto& filter : massBoost_)
        filter.reset();

    for (auto& filter : massCleanup_)
        filter.reset();

    massCoefficientCountdown_ = 0;
    makeupShelfCoefficientCountdown_ = 0;

    lowEnd_.reset();
    body_.reset();
    articulation_.reset();
    harshness_.reset();
    fizz_.reset();
    resonanceSuppressor_.reset();
    autoLevel_.reset();
    toneMatch_.reset();
    delay_.reset();
    room_.reset();

    updateModeTargets();
    modeWeights_ = modeWeightTargets_;

    const bool lowCutOn =
        lowCutEnabled(lowCutTarget_);

    lowCutMix_ =
        lowCutOn ? 1.0 : 0.0;

    lowCutFrequencyHz_ =
        lowCutOn
            ? lowCutFrequencyFromNormalized(
                lowCutTarget_)
            : kLowCutMinimumHz;

    updateLowCutCoefficients();
    updateMassCoefficients();
    lowCutCoefficientCountdown_ = 0;
    massCoefficientCountdown_ = 0;
}

void MetalFinisherDSP::setFinish(double normalized) noexcept {
    const double next =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : 0.0,
            0.0,
            1.0);

    const bool wasActive =
        finish_ > 0.0;

    finish_ = next;

    // Reset once at the transition to exact zero so a later re-enable cannot
    // revive stale detector, level-match or protection-filter history.
    if (wasActive &&
        finish_ <= 0.0) {

        lowEnd_.reset();
        body_.reset();
        articulation_.reset();
        harshness_.reset();
        fizz_.reset();
        resonanceSuppressor_.reset();
        autoLevel_.reset();

        for (auto& filter : makeupLowShelf_)
            filter.reset();
        for (auto& filter : makeupHighShelf_)
            filter.reset();
        makeupShelfCoefficientCountdown_ = 0;
    }
}

void MetalFinisherDSP::setMass(double normalized) noexcept {
    mass_ =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : 0.0,
            0.0,
            1.0);
}

void MetalFinisherDSP::setLowCut(double normalized) noexcept {
    lowCutTarget_ =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : 0.0,
            0.0,
            1.0);
}

void MetalFinisherDSP::updateLowCutCoefficients() noexcept {
    const auto coefficients =
        makeHighPass(
            sampleRate_,
            lowCutFrequencyHz_,
            0.7071067811865476);

    for (auto& filter : lowCut_)
        filter.setCoefficients(coefficients);
}

void MetalFinisherDSP::updateMassCoefficients() noexcept {
    double context = 0.0;

    if (lowCutEnabled(lowCutTarget_)) {
        context =
            std::clamp(
                (lowCutFrequencyHz_ - kLowCutMinimumHz) /
                    (kLowCutMaximumHz - kLowCutMinimumHz),
                0.0,
                1.0);
    }

    const double boostHz =
        kMassBoostHz +
        context *
            kMassBoostShiftHzAtMaxLowCut;

    const double boostDb =
        kMassBoostDb -
        context *
            kMassBoostReductionDbAtMaxLowCut;

    const double cleanupHz =
        kMassCleanupHz +
        context *
            kMassCleanupShiftHzAtMaxLowCut;

    const double cleanupDb =
        kMassCleanupDb +
        context *
            kMassCleanupReductionDbAtMaxLowCut;

    const double trimDb =
        kMassTrimDb +
        context *
            kMassTrimReductionDbAtMaxLowCut;

    const auto massBoostCoefficients =
        makePeaking(
            sampleRate_,
            boostHz,
            kMassBoostQ,
            boostDb);

    const auto massCleanupCoefficients =
        makePeaking(
            sampleRate_,
            cleanupHz,
            kMassCleanupQ,
            cleanupDb);

    for (auto& filter : massBoost_)
        filter.setCoefficients(
            massBoostCoefficients);

    for (auto& filter : massCleanup_)
        filter.setCoefficients(
            massCleanupCoefficients);

    massTrimGain_ =
        std::pow(
            10.0,
            trimDb / 20.0);
}

void MetalFinisherDSP::updateMakeupShelfCoefficients() noexcept {
    const double cancellationDb =
        -autoLevel_.currentGainDb();

    const auto lowShelf =
        makeLowShelf(
            sampleRate_,
            80.0,
            cancellationDb);

    const auto highShelf =
        makeHighShelf(
            sampleRate_,
            10000.0,
            cancellationDb);

    for (auto& filter : makeupLowShelf_)
        filter.setCoefficients(lowShelf);

    for (auto& filter : makeupHighShelf_)
        filter.setCoefficients(highShelf);
}

void MetalFinisherDSP::setRoomWet(double normalized) noexcept {
    room_.setWetDry(normalized);
}

void MetalFinisherDSP::setRoomDecay(double normalized) noexcept {
    room_.setDecay(normalized);
}

void MetalFinisherDSP::setDelayWet(double normalized) noexcept {
    delay_.setWet(normalized);
}

void MetalFinisherDSP::setDelayFeedback(double normalized) noexcept {
    delay_.setFeedback(normalized);
}

void MetalFinisherDSP::setDelayDivision(double normalized) noexcept {
    delay_.setDivision(normalized);
}

void MetalFinisherDSP::setTempo(double bpm) noexcept {
    delay_.setTempo(bpm);
}

void MetalFinisherDSP::setToneMatchAmount(double normalized) noexcept {
    toneMatch_.setAmount(normalized);
}

void MetalFinisherDSP::setToneMatchProfile(
    const ToneMatchProfile& profile) noexcept {

    toneMatch_.setProfile(profile);
}

void MetalFinisherDSP::clearToneMatchProfile() noexcept {
    toneMatch_.clearProfile();
}

void MetalFinisherDSP::setMode(double normalized) noexcept {
    modeTarget_ =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : 0.0,
            0.0,
            1.0);

    updateModeTargets();
}

void MetalFinisherDSP::updateModeTargets() noexcept {
    const int mode =
        modeTarget_ < 0.25
            ? 0
            : (modeTarget_ < 0.75 ? 1 : 2);

    if (mode == 0) {
        // Open / balanced: clearly more presence and articulation than the
        // dense profile, without reaching Mode 2's aggressive bite.
        modeWeightTargets_ = {
            1.00,
            0.90,
            1.10,
            0.80,
            0.70
        };
    } else if (mode == 1) {
        // Bite / industrial: keep the current user-preferred profile exactly.
        modeWeightTargets_ = {
            1.10,
            0.80,
            1.00,
            0.15,
            0.85
        };
    } else {
        // Dense / smooth: retain extra body, but avoid the previous overly
        // dark top-end suppression.
        modeWeightTargets_ = {
            0.75,
            0.65,
            0.55,
            0.95,
            1.05
        };
    }
}

void MetalFinisherDSP::processFrame(
    double& left,
    double& right) noexcept {

    if (!std::isfinite(left))
        left = 0.0;
    if (!std::isfinite(right))
        right = 0.0;

    left =
        std::clamp(
            left,
            -kEmergencyInputLimit,
            kEmergencyInputLimit);

    right =
        std::clamp(
            right,
            -kEmergencyInputLimit,
            kEmergencyInputLimit);

    const bool lowCutOn =
        lowCutEnabled(lowCutTarget_);

    const double targetMix =
        lowCutOn ? 1.0 : 0.0;

    lowCutMix_ =
        lowCutMixSmoothing_ * lowCutMix_ +
        (1.0 - lowCutMixSmoothing_) *
            targetMix;

    if (std::abs(lowCutMix_ - targetMix) < 1.0e-9)
        lowCutMix_ = targetMix;

    if (lowCutOn) {
        const double targetFrequency =
            lowCutFrequencyFromNormalized(
                lowCutTarget_);

        lowCutFrequencyHz_ =
            lowCutFrequencySmoothing_ *
                lowCutFrequencyHz_ +
            (1.0 - lowCutFrequencySmoothing_) *
                targetFrequency;

        if (--lowCutCoefficientCountdown_ <= 0) {
            updateLowCutCoefficients();
            lowCutCoefficientCountdown_ = 16;
        }
    } else {
        lowCutCoefficientCountdown_ = 0;
    }

    // TONE MATCH is the first production stage: it receives the untouched
    // post-amp/cab signal. FINISH must then analyse the fully matched signal.
    toneMatch_.processFrame(
        left,
        right);

    // FINISH deliberately sees the complete matched guitar signal. LOW CUT is
    // a later mix-placement decision and must not remove palm-mute information
    // from the adaptive FINISH detectors.
    const double baseLeft = left;
    const double baseRight = right;

    double processedLeft = baseLeft;
    double processedRight = baseRight;

    if (finish_ > 0.0) {
        double lowLeft = baseLeft;
        double lowRight = baseRight;
        double bodyLeft = baseLeft;
        double bodyRight = baseRight;
        double articulationLeft = baseLeft;
        double articulationRight = baseRight;
        double harshLeft = baseLeft;
        double harshRight = baseRight;
        double fizzLeft = baseLeft;
        double fizzRight = baseRight;

        lowEnd_.processFrame(lowLeft, lowRight);
        body_.processFrame(bodyLeft, bodyRight);
        articulation_.processFrame(
            articulationLeft,
            articulationRight);
        harshness_.processFrame(harshLeft, harshRight);
        fizz_.processFrame(fizzLeft, fizzRight);

        for (std::size_t i = 0;
             i < modeWeights_.size();
             ++i) {
            modeWeights_[i] =
                modeSmoothing_ * modeWeights_[i] +
                (1.0 - modeSmoothing_) *
                    modeWeightTargets_[i];
        }

        const double correctionLeft =
            modeWeights_[0] * (lowLeft - baseLeft) +
            modeWeights_[1] * (bodyLeft - baseLeft) +
            modeWeights_[2] * (articulationLeft - baseLeft) +
            modeWeights_[3] * (harshLeft - baseLeft) +
            modeWeights_[4] * (fizzLeft - baseLeft);

        const double correctionRight =
            modeWeights_[0] * (lowRight - baseRight) +
            modeWeights_[1] * (bodyRight - baseRight) +
            modeWeights_[2] * (articulationRight - baseRight) +
            modeWeights_[3] * (harshRight - baseRight) +
            modeWeights_[4] * (fizzRight - baseRight);

        // Build the complete 100% optimizer result first. FINISH is applied
        // only after adaptive correction, automatic level matching and edge
        // protection, making it a mathematically true linear amount control.
        double fullLeft =
            baseLeft + correctionLeft;

        double fullRight =
            baseRight + correctionRight;

        // Residual narrow resonances are handled after the broad adaptive
        // FINISH zones. This stage searches freely across the guitar range
        // and only reacts to persistent local spectral outliers, so normal
        // notes and broad tone balance are not treated as "bad resonance".
        resonanceSuppressor_.
            processFrame(
                fullLeft,
                fullRight);

        autoLevel_.processFrame(
            baseLeft,
            baseRight,
            fullLeft,
            fullRight);

        if (--makeupShelfCoefficientCountdown_ <= 0) {
            updateMakeupShelfCoefficients();
            makeupShelfCoefficientCountdown_ = 16;
        }

        fullLeft =
            makeupHighShelf_[0].process(
                makeupLowShelf_[0].process(
                    fullLeft));

        fullRight =
            makeupHighShelf_[1].process(
                makeupLowShelf_[1].process(
                    fullRight));

        processedLeft =
            baseLeft +
            (fullLeft - baseLeft) * finish_;

        processedRight =
            baseRight +
            (fullRight - baseRight) * finish_;
    }

    // LOW CUT is a mix-placement stage after FINISH. It therefore cannot
    // change what FINISH detects, but it defines the lower boundary that MASS
    // must respect.
    const double filteredLeft =
        lowCut_[0].process(processedLeft);

    const double filteredRight =
        lowCut_[1].process(processedRight);

    if (lowCutMix_ > 0.0) {
        processedLeft +=
            (filteredLeft - processedLeft) *
            lowCutMix_;

        processedRight +=
            (filteredRight - processedRight) *
            lowCutMix_;
    }

    if (--massCoefficientCountdown_ <= 0) {
        updateMassCoefficients();
        massCoefficientCountdown_ = 16;
    }

    // MASS preserves the V2 curve when LOW CUT is Off. With LOW CUT active,
    // its positive weight moves upward and weakens progressively so it cannot
    // simply restore the bass the user intentionally removed.
    const double massFullLeft =
        massTrimGain_ *
        massCleanup_[0].process(
            massBoost_[0].process(
                processedLeft));

    const double massFullRight =
        massTrimGain_ *
        massCleanup_[1].process(
            massBoost_[1].process(
                processedRight));

    processedLeft +=
        (massFullLeft - processedLeft) *
        mass_;

    processedRight +=
        (massFullRight - processedRight) *
        mass_;

    // Lead delay intentionally sits after MASS and before ROOM: the repeats
    // inherit the finished guitar tone and are then placed into the same room.
    double delayedLeft = processedLeft;
    double delayedRight = processedRight;

    delay_.processFrame(
        processedLeft,
        processedRight,
        delayedLeft,
        delayedRight);

    processedLeft = delayedLeft;
    processedRight = delayedRight;

    double roomLeft = 0.0;
    double roomRight = 0.0;

    room_.processFrame(
        processedLeft,
        processedRight,
        roomLeft,
        roomRight);

    const double roomMix =
        std::clamp(
            room_.currentWetDry(),
            0.0,
            1.0);

    left =
        processedLeft +
        (roomLeft - processedLeft) *
            roomMix;

    right =
        processedRight +
        (roomRight - processedRight) *
            roomMix;

    if (!std::isfinite(left))
        left = 0.0;
    if (!std::isfinite(right))
        right = 0.0;
}

} // namespace HighGainGuitarFinisher::dsp
