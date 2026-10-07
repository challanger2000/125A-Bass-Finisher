#include "AdaptiveBandController.h"

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

namespace {
constexpr double kEpsilon = 1.0e-18;
constexpr double kMaximumEnergyMagnitude = 1.0e100;

double safeSquaredMagnitude(double magnitude) noexcept {
    const double bounded =
        std::min(
            std::abs(magnitude),
            kMaximumEnergyMagnitude);

    return bounded * bounded;
}
}

double AdaptiveBandController::timeCoefficient(
    double sampleRate,
    double milliseconds) noexcept {

    const double fs = std::max(sampleRate, 1000.0);
    const double ms = std::max(milliseconds, 0.01);
    return std::exp(-1.0 / (fs * ms * 0.001));
}

void AdaptiveBandController::prepare(
    double sampleRate,
    AdaptiveBandMode mode,
    const Centers& centers,
    double q) noexcept {

    sampleRate_ = (std::isfinite(sampleRate) && sampleRate > 1000.0)
        ? sampleRate
        : 44100.0;

    mode_ = mode;
    centers_ = centers;

    for (std::size_t channel = 0; channel < filters_.size(); ++channel) {
        for (std::size_t band = 0; band < kBandCount; ++band) {
            filters_[channel][band].setCoefficients(
                makeBandPass(sampleRate_, centers_[band], q));
        }
    }

    const double fastAttackMs =
        mode_ == AdaptiveBandMode::BodyResonance ? 30.0 : 3.0;
    const double fastReleaseMs =
        mode_ == AdaptiveBandMode::BodyResonance ? 200.0 : 80.0;

    fastAttack_ = timeCoefficient(sampleRate_, fastAttackMs);
    fastRelease_ = timeCoefficient(sampleRate_, fastReleaseMs);
    slowAttack_ = timeCoefficient(sampleRate_, 250.0);
    slowRelease_ = timeCoefficient(sampleRate_, 900.0);

    wideFastAttack_ = timeCoefficient(sampleRate_, 3.0);
    wideFastRelease_ = timeCoefficient(sampleRate_, 80.0);
    wideSlowAttack_ = timeCoefficient(sampleRate_, 250.0);
    wideSlowRelease_ = timeCoefficient(sampleRate_, 900.0);

    double reductionAttackMs = 100.0;
    double reductionReleaseMs = 500.0;

    if (mode_ == AdaptiveBandMode::LowTransient) {
        reductionAttackMs = 4.0;
        reductionReleaseMs = 150.0;
    } else if (mode_ == AdaptiveBandMode::ArticulationSupport) {
        reductionAttackMs = 35.0;
        reductionReleaseMs = 260.0;
    } else if (mode_ == AdaptiveBandMode::Harshness) {
        reductionAttackMs = 6.0;
        reductionReleaseMs = 180.0;
    } else if (mode_ == AdaptiveBandMode::Fizz) {
        reductionAttackMs = 10.0;
        reductionReleaseMs = 220.0;
    }

    reductionAttack_ =
        timeCoefficient(sampleRate_, reductionAttackMs);
    reductionRelease_ =
        timeCoefficient(sampleRate_, reductionReleaseMs);

    weightSmoothing_ = timeCoefficient(sampleRate_, 180.0);

    reset();
}

void AdaptiveBandController::reset() noexcept {
    for (auto& channel : filters_) {
        for (auto& filter : channel)
            filter.reset();
    }

    fastEnergy_.fill(0.0);
    slowEnergy_.fill(0.0);
    weights_.fill(0.0);
    weights_[0] = 1.0;

    wideFastEnergy_ = 0.0;
    wideSlowEnergy_ = 0.0;
    reduction_ = 0.0;
    dominance_ = 0.0;
    selected_ = 0;
    selectionCounter_ = 0;
}

void AdaptiveBandController::updateSelection() noexcept {
    std::size_t best = 0;

    for (std::size_t band = 1; band < kBandCount; ++band) {
        if (slowEnergy_[band] > slowEnergy_[best])
            best = band;
    }

    if (best != selected_ &&
        slowEnergy_[best] > slowEnergy_[selected_] * 1.08) {
        selected_ = best;
    }
}

double AdaptiveBandController::selectedFrequency() const noexcept {
    double sum = 0.0;
    double weighted = 0.0;

    for (std::size_t band = 0; band < kBandCount; ++band) {
        sum += weights_[band];
        weighted += weights_[band] * centers_[band];
    }

    return sum > kEpsilon
        ? weighted / sum
        : centers_[selected_];
}

void AdaptiveBandController::processFrame(
    double& left,
    double& right) noexcept {

    if (!std::isfinite(left))
        left = 0.0;
    if (!std::isfinite(right))
        right = 0.0;

    double bandLeft[kBandCount] {};
    double bandRight[kBandCount] {};

    for (std::size_t band = 0; band < kBandCount; ++band) {
        bandLeft[band] = filters_[0][band].process(left);
        bandRight[band] = filters_[1][band].process(right);

        const double magnitude = std::max(
            std::abs(bandLeft[band]),
            std::abs(bandRight[band]));

        const double target =
            safeSquaredMagnitude(magnitude);

        const double fastCoefficient =
            target > fastEnergy_[band]
                ? fastAttack_
                : fastRelease_;

        fastEnergy_[band] =
            fastCoefficient * fastEnergy_[band] +
            (1.0 - fastCoefficient) * target;

        const double slowCoefficient =
            target > slowEnergy_[band]
                ? slowAttack_
                : slowRelease_;

        slowEnergy_[band] =
            slowCoefficient * slowEnergy_[band] +
            (1.0 - slowCoefficient) * target;
    }

    const double wideMagnitude =
        std::max(std::abs(left), std::abs(right));
    const double wideTarget =
        safeSquaredMagnitude(wideMagnitude);

    const double wideFastCoefficient =
        wideTarget > wideFastEnergy_
            ? wideFastAttack_
            : wideFastRelease_;

    wideFastEnergy_ =
        wideFastCoefficient * wideFastEnergy_ +
        (1.0 - wideFastCoefficient) * wideTarget;

    const double wideSlowCoefficient =
        wideTarget > wideSlowEnergy_
            ? wideSlowAttack_
            : wideSlowRelease_;

    wideSlowEnergy_ =
        wideSlowCoefficient * wideSlowEnergy_ +
        (1.0 - wideSlowCoefficient) * wideTarget;

    if (++selectionCounter_ >= 128) {
        selectionCounter_ = 0;
        updateSelection();
    }

    for (std::size_t band = 0; band < kBandCount; ++band) {
        const double target =
            band == selected_ ? 1.0 : 0.0;

        weights_[band] =
            weightSmoothing_ * weights_[band] +
            (1.0 - weightSmoothing_) * target;
    }

    const double selectedSlow =
        std::max(slowEnergy_[selected_], kEpsilon);

    double slowSum = 0.0;
    for (const double energy : slowEnergy_)
        slowSum += energy;

    const double otherAverage = std::max(
        (slowSum - selectedSlow) /
            static_cast<double>(kBandCount - 1),
        kEpsilon);

    const double peakiness =
        selectedSlow / otherAverage;

    double targetReduction = 0.0;

    const double dominance = std::sqrt(
        selectedSlow /
        (wideSlowEnergy_ + kEpsilon));

    dominance_ = dominance;

    if (mode_ == AdaptiveBandMode::BodyResonance) {
        const double resonanceExcess =
            dominance > 0.080
                ? std::clamp(
                    (peakiness - 1.12) / 1.00,
                    0.0,
                    1.0)
                : 0.0;

        // A guitar can be boxy/body-heavy without one narrow resonance
        // dominating the four candidates. Treat sustained broad dominance
        // as a second, independent reason to reduce this zone.
        const double broadExcess =
            std::clamp(
                (dominance - 0.16) / 0.18,
                0.0,
                1.0);

        const double excess =
            std::max(
                resonanceExcess,
                broadExcess);

        const double deficit = std::clamp(
            (0.105 - dominance) / 0.060,
            0.0,
            1.0);

        targetReduction =
            excess > 0.08
                ? 0.40 * excess
                : -0.20 * deficit;
    } else if (mode_ == AdaptiveBandMode::LowTransient) {
        const double rise = std::sqrt(
            (fastEnergy_[selected_] + kEpsilon) /
            (selectedSlow + kEpsilon));

        const double activity =
            dominance > 0.065
                ? std::clamp(
                    (rise - 1.08) / 0.62,
                    0.0,
                    1.0)
                : 0.0;

        targetReduction = 0.55 * activity;
    } else if (mode_ == AdaptiveBandMode::ArticulationSupport) {
        // Measured fixture separation after the detector-bank audit:
        // thin ~0.40, balanced ~0.56, articulation-heavy ~0.77.
        // Use the gap between thin and balanced as the support corridor
        // instead of the old absolute threshold, which was far below the
        // detector's real operating scale because of neighbouring-band energy.
        const double deficit = std::clamp(
            (0.54 - dominance) / 0.18,
            0.0,
            1.0);

        const double excess =
            dominance > 0.68
                ? std::clamp(
                    (peakiness - 1.65) / 1.80,
                    0.0,
                    1.0)
                : 0.0;

        targetReduction =
            excess > 0.20
                ? 0.22 * excess
                : -0.28 * deficit;
    } else {
        const double rise = std::sqrt(
            (fastEnergy_[selected_] + kEpsilon) /
            (selectedSlow + kEpsilon));

        const double transient = std::clamp(
            (rise - 1.07) /
                (mode_ == AdaptiveBandMode::Fizz ? 0.75 : 0.55),
            0.0,
            1.0);

        const double persistent = std::clamp(
            (peakiness -
                (mode_ == AdaptiveBandMode::Fizz ? 1.12 : 1.15)) /
                (mode_ == AdaptiveBandMode::Fizz ? 1.45 : 1.30),
            0.0,
            1.0);

        const double authority =
            mode_ == AdaptiveBandMode::Fizz
                ? 0.40
                : 0.45;

        targetReduction =
            authority *
            std::max(transient, persistent);
    }

    const double reductionCoefficient =
        targetReduction > reduction_
            ? reductionAttack_
            : reductionRelease_;

    reduction_ =
        reductionCoefficient * reduction_ +
        (1.0 - reductionCoefficient) * targetReduction;

    double componentLeft = 0.0;
    double componentRight = 0.0;

    for (std::size_t band = 0; band < kBandCount; ++band) {
        componentLeft +=
            weights_[band] * bandLeft[band];
        componentRight +=
            weights_[band] * bandRight[band];
    }

    left -= componentLeft * reduction_;
    right -= componentRight * reduction_;
}

} // namespace HighGainGuitarFinisher::dsp
