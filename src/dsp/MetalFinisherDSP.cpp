#include "MetalFinisherDSP.h"

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

namespace {
constexpr double kMassBoostHz = 75.0;
constexpr double kMassBoostDb = 6.0;
constexpr double kMassBoostQ = 0.80;
constexpr double kMassCleanupHz = 220.0;
constexpr double kMassCleanupDb = -4.5;
constexpr double kMassCleanupQ = 0.90;
constexpr double kMassTrimDb = -0.30;

// LOW CONTROL adds content-aware sub containment on top of the user-selected
// high-pass boundary. It reacts only when sub energy dominates the useful
// 60-140 Hz body region.
constexpr double kLowControlSubDetectorHz = 48.0;
constexpr double kLowControlBodyDetectorHz = 140.0;
constexpr double kLowControlDynamicShelfHz = 58.0;
constexpr double kLowControlRatioThreshold = 0.38;
constexpr double kLowControlMaximumDynamicCutDb = -3.0;

// MASS adds a band-limited odd-harmonic residual from roughly 35-180 Hz.
// Because only the nonlinear residual is mixed back, MASS gains audibility
// without simply stacking more sub energy.
constexpr double kMassHarmonicHighPassHz = 35.0;
constexpr double kMassHarmonicLowPassHz = 180.0;
constexpr double kMassHarmonicDrive = 2.0;
constexpr double kMassHarmonicMix = 0.35;

// FINISH saturation works only on the definition band. It is intentionally
// modest because adaptive correction and resonance control remain the primary
// FINISH mechanisms.
constexpr double kFinishSaturationHighPassHz = 110.0;
constexpr double kFinishSaturationLowPassHz = 3600.0;
constexpr double kFinishSaturationDrive = 1.85;

// At LOW CONTROL Off the Bass V1 MASS curve is preserved exactly. As the user moves
// the mix-placement high-pass upward, MASS shifts its positive weight above
// the cut and reduces the amount of deep boost. These endpoints are
// EMPIRICALLY TUNED and guarded by measured response tests.
constexpr double kMassBoostShiftHzAtMaxLowCut = 35.0;
constexpr double kMassBoostReductionDbAtMaxLowCut = 3.5;
constexpr double kMassCleanupShiftHzAtMaxLowCut = 100.0;
constexpr double kMassCleanupReductionDbAtMaxLowCut = 2.0;
constexpr double kMassTrimReductionDbAtMaxLowCut = 0.15;

// Emergency numerical guard only. +36.1 dBFS is far beyond the intended
// operating range but keeps hostile/invalid host input from poisoning state.
constexpr double kEmergencyInputLimit = 64.0;

// INPUT AUTO aims at a stable detector/processing operating level without
// behaving like a compressor. The bounds are deliberately modest.
constexpr double kAutoInputTargetRms = 0.12589254117941673; // -18 dBFS
constexpr double kAutoInputMinimumGain = 0.5011872336272722; // -6 dB
constexpr double kAutoInputMaximumGain = 1.9952623149688795; // +6 dB
constexpr double kAutoInputSilencePower = 1.0e-10;

// FINAL is transparent below the knee and linked across both channels.
constexpr double kFinalKnee = 0.8912509381337456; // -1 dBFS
constexpr double kFinalCeiling = 0.9885530946569389; // -0.1 dBFS
}

void MetalFinisherDSP::prepare(double sampleRate) {
    sampleRate_ = (std::isfinite(sampleRate) && sampleRate > 1000.0)
        ? sampleRate
        : 44100.0;

    lowCutMixSmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.005));

    lowCutFrequencySmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.020));

    lowControlEnvelopeSmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.080));

    modeSmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.030));

    inputPowerAttack_ =
        std::exp(-1.0 / (sampleRate_ * 0.050));

    inputPowerRelease_ =
        std::exp(-1.0 / (sampleRate_ * 0.500));

    inputGainSmoothing_ =
        std::exp(-1.0 / (sampleRate_ * 0.250));

    finalRelease_ =
        std::exp(-1.0 / (sampleRate_ * 0.080));

    lowEnd_.prepare(
        sampleRate_,
        AdaptiveBandMode::LowTransient,
        {45.0, 65.0, 85.0, 110.0},
        2.0);

    body_.prepare(
        sampleRate_,
        AdaptiveBandMode::BodyResonance,
        {120.0, 170.0, 240.0, 340.0},
        0.9);

    articulation_.prepare(
        sampleRate_,
        AdaptiveBandMode::ArticulationSupport,
        {650.0, 1000.0, 1500.0, 2200.0},
        0.90);

    harshness_.prepare(
        sampleRate_,
        AdaptiveBandMode::Harshness,
        {2500.0, 3200.0, 4000.0, 5000.0},
        1.10);

    fizz_.prepare(
        sampleRate_,
        AdaptiveBandMode::Fizz,
        {5200.0, 6500.0, 8000.0, 10000.0},
        1.0);

    resonanceSuppressor_.
        prepare(sampleRate_);

    const auto unityLowShelf =
        makeLowShelf(
            sampleRate_,
            70.0,
            0.0);

    const auto unityHighShelf =
        makeHighShelf(
            sampleRate_,
            6500.0,
            0.0);

    for (auto& filter : makeupLowShelf_)
        filter.setCoefficients(unityLowShelf);

    for (auto& filter : makeupHighShelf_)
        filter.setCoefficients(unityHighShelf);

    const auto subDetector =
        makeLowPass(
            sampleRate_,
            kLowControlSubDetectorHz,
            0.7071067811865476);

    const auto bodyDetector =
        makeLowPass(
            sampleRate_,
            kLowControlBodyDetectorHz,
            0.7071067811865476);

    const auto unityDynamicShelf =
        makeLowShelf(
            sampleRate_,
            kLowControlDynamicShelfHz,
            0.0);

    const auto harmonicHighPass =
        makeHighPass(
            sampleRate_,
            kMassHarmonicHighPassHz,
            0.7071067811865476);

    const auto harmonicLowPass =
        makeLowPass(
            sampleRate_,
            kMassHarmonicLowPassHz,
            0.7071067811865476);

    const auto finishSaturationHighPass =
        makeHighPass(
            sampleRate_,
            kFinishSaturationHighPassHz,
            0.7071067811865476);

    const auto finishSaturationLowPass =
        makeLowPass(
            sampleRate_,
            kFinishSaturationLowPassHz,
            0.7071067811865476);

    for (auto& filter : lowControlSubDetector_)
        filter.setCoefficients(subDetector);

    for (auto& filter : lowControlBodyDetector_)
        filter.setCoefficients(bodyDetector);

    for (auto& filter : lowControlDynamicShelf_)
        filter.setCoefficients(unityDynamicShelf);

    for (auto& filter : massHarmonicHighPass_)
        filter.setCoefficients(harmonicHighPass);

    for (auto& filter : massHarmonicLowPass_)
        filter.setCoefficients(harmonicLowPass);

    for (auto& filter : finishSaturationHighPass_)
        filter.setCoefficients(finishSaturationHighPass);

    for (auto& filter : finishSaturationLowPass_)
        filter.setCoefficients(finishSaturationLowPass);

    updateMassCoefficients();

    autoLevel_.prepare(sampleRate_);
    toneMatch_.prepare(sampleRate_);

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

    for (auto& filter : lowControlSubDetector_)
        filter.reset();

    for (auto& filter : lowControlBodyDetector_)
        filter.reset();

    for (auto& filter : lowControlDynamicShelf_)
        filter.reset();

    for (auto& filter : massHarmonicHighPass_)
        filter.reset();

    for (auto& filter : massHarmonicLowPass_)
        filter.reset();

    for (auto& filter : finishSaturationHighPass_)
        filter.reset();

    for (auto& filter : finishSaturationLowPass_)
        filter.reset();

    lowControlSubPower_ = 0.0;
    lowControlBodyPower_ = 0.0;
    lowControlDynamicGainDb_ = 0.0;
    lowControlCoefficientCountdown_ = 0;

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

    inputPower_ = 0.0;
    inputGain_ = 1.0;
    finalGain_ = 1.0;

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

    toneMatch_.setBassContext(
        lowCutTarget_,
        mass_);
}

void MetalFinisherDSP::setLowCut(double normalized) noexcept {
    lowCutTarget_ =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : 0.0,
            0.0,
            1.0);

    toneMatch_.setBassContext(
        lowCutTarget_,
        mass_);
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

void MetalFinisherDSP::updateLowControlCoefficients() noexcept {
    const auto coefficients =
        makeLowShelf(
            sampleRate_,
            kLowControlDynamicShelfHz,
            lowControlDynamicGainDb_);

    for (auto& filter : lowControlDynamicShelf_)
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
            70.0,
            cancellationDb);

    const auto highShelf =
        makeHighShelf(
            sampleRate_,
            6500.0,
            cancellationDb);

    for (auto& filter : makeupLowShelf_)
        filter.setCoefficients(lowShelf);

    for (auto& filter : makeupHighShelf_)
        filter.setCoefficients(highShelf);
}

void MetalFinisherDSP::setToneMatchAmount(double normalized) noexcept {
    toneMatchAmount_ =
        std::clamp(
            std::isfinite(normalized)
                ? normalized
                : 0.0,
            0.0,
            1.0);

    toneMatch_.setAmount(toneMatchAmount_);
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
        // CLEAN: restrained correction, retains natural DI/amp character.
        modeWeightTargets_ = {
            0.65,
            0.55,
            0.65,
            0.50,
            0.35
        };
    } else if (mode == 1) {
        // PUNCH: stronger transient control and midrange articulation so
        // bass remains readable against dense guitars and kick.
        modeWeightTargets_ = {
            1.15,
            0.75,
            1.15,
            0.45,
            0.55
        };
    } else {
        // DENSE: preserve low/body weight while smoothing aggressive clank
        // and upper harmonics.
        modeWeightTargets_ = {
            0.55,
            0.45,
            0.55,
            1.00,
            1.10
        };
    }
}

void MetalFinisherDSP::applyAutoInput(
    double& left,
    double& right) noexcept {

    // Only FINISH depends on a controlled operating level. MATCH, LOW CUT
    // and MASS are linear tonal stages and must not acquire an unrelated
    // level change merely because their amount is non-zero.
    const bool productionActive =
        finish_ > 0.0;

    if (!productionActive) {
        inputPower_ = 0.0;
        inputGain_ = 1.0;
        return;
    }

    const double instantaneousPower =
        0.5 *
        (left * left +
         right * right);

    const double powerCoefficient =
        instantaneousPower > inputPower_
            ? inputPowerAttack_
            : inputPowerRelease_;

    inputPower_ =
        powerCoefficient * inputPower_ +
        (1.0 - powerCoefficient) *
            instantaneousPower;

    double targetGain = 1.0;

    if (inputPower_ >
        kAutoInputSilencePower) {

        targetGain =
            std::clamp(
                kAutoInputTargetRms /
                    std::sqrt(inputPower_),
                kAutoInputMinimumGain,
                kAutoInputMaximumGain);
    }

    inputGain_ =
        inputGainSmoothing_ * inputGain_ +
        (1.0 - inputGainSmoothing_) *
            targetGain;

    if (!std::isfinite(inputGain_))
        inputGain_ = 1.0;

    left *= inputGain_;
    right *= inputGain_;
}

void MetalFinisherDSP::applyFinal(
    double& left,
    double& right) noexcept {

    const double peak =
        std::max(
            std::abs(left),
            std::abs(right));

    double requiredGain = 1.0;

    if (peak > kFinalKnee) {
        requiredGain =
            std::min(
                1.0,
                kFinalCeiling /
                    std::max(
                        peak,
                        1.0e-12));
    }

    if (requiredGain < finalGain_) {
        // Attack is instantaneous: a new peak cannot overshoot the ceiling.
        finalGain_ = requiredGain;
    } else {
        finalGain_ =
            finalRelease_ * finalGain_ +
            (1.0 - finalRelease_);
    }

    left *= finalGain_;
    right *= finalGain_;

    // The limiter calculation above is the normal path. This clamp is a
    // deterministic last line of defence against hostile discontinuities.
    left =
        std::clamp(
            left,
            -kFinalCeiling,
            kFinalCeiling);

    right =
        std::clamp(
            right,
            -kFinalCeiling,
            kFinalCeiling);
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

    // INPUT AUTO is an internal operating-level stage. It is dormant when
    // FINISH, MASS and MATCH are all neutral, preserving the exact neutral
    // contract while keeping active processing in a controlled level window.
    applyAutoInput(
        left,
        right);

    // TONE MATCH receives the level-conditioned bass signal.
    toneMatch_.processFrame(
        left,
        right);

    // FINISH deliberately sees the complete matched bass signal. LOW CONTROL is
    // a later mix-placement decision and must not remove low-end/transient information
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

        // Add controlled density/definition without saturating the deep bass.
        // CLEAN/PUNCH/DENSE progressively increase the nonlinear residual.
        const double saturationAmount =
            modeTarget_ < 0.25
                ? 0.12
                : (modeTarget_ < 0.75
                    ? 0.20
                    : 0.28);

        const double saturationBandLeft =
            finishSaturationLowPass_[0].process(
                finishSaturationHighPass_[0].process(
                    fullLeft));

        const double saturationBandRight =
            finishSaturationLowPass_[1].process(
                finishSaturationHighPass_[1].process(
                    fullRight));

        const double saturatedBandLeft =
            std::tanh(
                kFinishSaturationDrive *
                saturationBandLeft) /
            kFinishSaturationDrive;

        const double saturatedBandRight =
            std::tanh(
                kFinishSaturationDrive *
                saturationBandRight) /
            kFinishSaturationDrive;

        fullLeft +=
            (saturatedBandLeft -
             saturationBandLeft) *
            saturationAmount;

        fullRight +=
            (saturatedBandRight -
             saturationBandRight) *
            saturationAmount;

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

    // LOW CONTROL is a mix-placement stage after FINISH. It therefore cannot
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

    // LOW CONTROL: compare true sub energy with the broader low-body energy.
    // Only an excessive sub/body ratio produces extra attenuation, so normal
    // fundamentals are left alone.
    {
        const double subLeft =
            lowControlSubDetector_[0].process(
                processedLeft);

        const double subRight =
            lowControlSubDetector_[1].process(
                processedRight);

        const double bodyLeft =
            lowControlBodyDetector_[0].process(
                processedLeft);

        const double bodyRight =
            lowControlBodyDetector_[1].process(
                processedRight);

        const double subPower =
            0.5 *
            (subLeft * subLeft +
             subRight * subRight);

        const double bodyPower =
            0.5 *
            (bodyLeft * bodyLeft +
             bodyRight * bodyRight);

        lowControlSubPower_ =
            lowControlEnvelopeSmoothing_ *
                lowControlSubPower_ +
            (1.0 - lowControlEnvelopeSmoothing_) *
                subPower;

        lowControlBodyPower_ =
            lowControlEnvelopeSmoothing_ *
                lowControlBodyPower_ +
            (1.0 - lowControlEnvelopeSmoothing_) *
                bodyPower;

        const double ratio =
            lowControlSubPower_ /
            std::max(
                lowControlBodyPower_,
                1.0e-18);

        const double excess =
            std::clamp(
                (ratio -
                 kLowControlRatioThreshold) /
                    (1.0 -
                     kLowControlRatioThreshold),
                0.0,
                1.0);

        const double controlAmount =
            lowCutEnabled(lowCutTarget_)
                ? lowCutTarget_
                : 0.0;

        const double targetGainDb =
            kLowControlMaximumDynamicCutDb *
            excess *
            controlAmount;

        lowControlDynamicGainDb_ =
            0.95 * lowControlDynamicGainDb_ +
            0.05 * targetGainDb;

        if (--lowControlCoefficientCountdown_ <= 0) {
            updateLowControlCoefficients();
            lowControlCoefficientCountdown_ = 32;
        }

        processedLeft =
            lowControlDynamicShelf_[0].process(
                processedLeft);

        processedRight =
            lowControlDynamicShelf_[1].process(
                processedRight);
    }

    if (--massCoefficientCountdown_ <= 0) {
        updateMassCoefficients();
        massCoefficientCountdown_ = 16;
    }

    // MASS preserves the Bass V1 curve when LOW CONTROL is Off. With LOW CONTROL active,
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

    if (mass_ > 0.0) {
        const double harmonicInputLeft =
            massHarmonicLowPass_[0].process(
                massHarmonicHighPass_[0].process(
                    processedLeft));

        const double harmonicInputRight =
            massHarmonicLowPass_[1].process(
                massHarmonicHighPass_[1].process(
                    processedRight));

        const double saturatedLeft =
            std::tanh(
                kMassHarmonicDrive *
                harmonicInputLeft) /
            kMassHarmonicDrive;

        const double saturatedRight =
            std::tanh(
                kMassHarmonicDrive *
                harmonicInputRight) /
            kMassHarmonicDrive;

        const double residualLeft =
            harmonicInputLeft -
            saturatedLeft;

        const double residualRight =
            harmonicInputRight -
            saturatedRight;

        processedLeft +=
            residualLeft *
            kMassHarmonicMix *
            mass_;

        processedRight +=
            residualRight *
            kMassHarmonicMix *
            mass_;
    }

    // FINAL belongs to the active production path. With every user-facing
    // processing amount neutral, the plugin must remain truly transparent,
    // including near-full-scale peaks.
    const bool productionPathActive =
        finish_ > 0.0 ||
        mass_ > 0.0 ||
        lowCutEnabled(lowCutTarget_) ||
        toneMatchAmount_ > 0.0;

    if (productionPathActive) {
        applyFinal(
            processedLeft,
            processedRight);
    } else {
        finalGain_ = 1.0;
    }

    // Bass Finisher has no built-in SPACE stage. FINAL feeds OUT directly.
    left = processedLeft;
    right = processedRight;

    if (!std::isfinite(left))
        left = 0.0;
    if (!std::isfinite(right))
        right = 0.0;
}

} // namespace HighGainGuitarFinisher::dsp
