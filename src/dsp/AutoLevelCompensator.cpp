#include "AutoLevelCompensator.h"

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

namespace {
constexpr double kEpsilon = 1.0e-20;
constexpr double kMaximumEnergyMagnitude = 1.0e100;

double safeStereoEnergy(
    double left,
    double right) noexcept {

    const double boundedLeft =
        std::min(
            std::abs(left),
            kMaximumEnergyMagnitude);

    const double boundedRight =
        std::min(
            std::abs(right),
            kMaximumEnergyMagnitude);

    return 0.5 * (
        boundedLeft * boundedLeft +
        boundedRight * boundedRight);
}
}

double AutoLevelCompensator::timeCoefficient(
    double sampleRate,
    double milliseconds) noexcept {

    const double fs = std::max(sampleRate, 1000.0);
    const double ms = std::max(milliseconds, 0.01);

    return std::exp(
        -1.0 / (fs * ms * 0.001));
}

void AutoLevelCompensator::prepare(
    double sampleRate) noexcept {

    sampleRate_ =
        (std::isfinite(sampleRate) && sampleRate > 1000.0)
            ? sampleRate
            : 44100.0;

    // Programme energy is learned slowly enough to ignore individual
    // pick transients and palm-mute peaks.
    energyCoefficient_ =
        timeCoefficient(sampleRate_, 850.0);

    // Independent activity memory closes the gate much faster than the
    // programme-energy window. This prevents a previous phrase from keeping
    // stale makeup active through a real pause.
    activityReleaseCoefficient_ =
        timeCoefficient(sampleRate_, 80.0);

    // Makeup rises slowly so it cannot act like another compressor.
    // It returns to unity faster when the required compensation falls.
    gainUpCoefficient_ =
        timeCoefficient(sampleRate_, 900.0);

    gainDownCoefficient_ =
        timeCoefficient(sampleRate_, 450.0);

    // The first part of a phrase needs quicker compensation because the
    // optimizer starts correcting immediately. Afterwards the slower
    // programme coefficients prevent palm-mute/chord pumping.
    bootstrapCoefficient_ =
        timeCoefficient(sampleRate_, 110.0);

    bootstrapLengthSamples_ =
        static_cast<int>(
            std::llround(
                sampleRate_ * 0.35));

    minGain_ =
        std::pow(10.0, -3.0 / 20.0);

    maxGain_ =
        std::pow(10.0, 3.0 / 20.0);

    // Do not chase amp hiss or numerical residue during silence.
    gateEnergy_ =
        std::pow(10.0, -65.0 / 10.0);

    reset();
}

void AutoLevelCompensator::reset() noexcept {
    inputEnergy_ = 0.0;
    outputEnergy_ = 0.0;
    activityEnergy_ = 0.0;
    gain_ = 1.0;
    bootstrapSamplesRemaining_ =
        bootstrapLengthSamples_;
}

double AutoLevelCompensator::currentGainDb() const noexcept {
    return gain_ > 0.0
        ? 20.0 * std::log10(gain_)
        : 0.0;
}

void AutoLevelCompensator::processFrame(
    double referenceLeft,
    double referenceRight,
    double& processedLeft,
    double& processedRight) noexcept {

    if (!std::isfinite(referenceLeft))
        referenceLeft = 0.0;
    if (!std::isfinite(referenceRight))
        referenceRight = 0.0;
    if (!std::isfinite(processedLeft))
        processedLeft = 0.0;
    if (!std::isfinite(processedRight))
        processedRight = 0.0;

    const double inputInstant =
        safeStereoEnergy(
            referenceLeft,
            referenceRight);

    const double outputInstant =
        safeStereoEnergy(
            processedLeft,
            processedRight);

    inputEnergy_ =
        energyCoefficient_ * inputEnergy_ +
        (1.0 - energyCoefficient_) * inputInstant;

    outputEnergy_ =
        energyCoefficient_ * outputEnergy_ +
        (1.0 - energyCoefficient_) * outputInstant;

    activityEnergy_ = std::max(
        inputInstant,
        activityReleaseCoefficient_ * activityEnergy_);

    double targetGain = 1.0;

    if (activityEnergy_ > gateEnergy_) {
        const double ratio = std::sqrt(
            (inputEnergy_ + kEpsilon) /
            (outputEnergy_ + kEpsilon));

        // Optimizer processing may cut or boost. Level matching must therefore
        // work in both directions so louder processing is not rewarded in A/B.
        targetGain = std::clamp(
            ratio,
            minGain_,
            maxGain_);
    } else {
        // A genuine pause starts a fresh programme estimate for the next
        // phrase instead of carrying the previous tone's ratio forward.
        inputEnergy_ = 0.0;
        outputEnergy_ = 0.0;
        bootstrapSamplesRemaining_ =
            bootstrapLengthSamples_;
    }

    const bool bootstrapActive =
        activityEnergy_ > gateEnergy_ &&
        bootstrapSamplesRemaining_ > 0;

    if (bootstrapActive)
        --bootstrapSamplesRemaining_;

    const double coefficient =
        bootstrapActive
            ? bootstrapCoefficient_
            : (targetGain > gain_
                ? gainUpCoefficient_
                : gainDownCoefficient_);

    gain_ =
        coefficient * gain_ +
        (1.0 - coefficient) * targetGain;

    if (std::abs(gain_ - 1.0) < 1.0e-12)
        gain_ = 1.0;

    processedLeft *= gain_;
    processedRight *= gain_;
}

} // namespace HighGainGuitarFinisher::dsp
