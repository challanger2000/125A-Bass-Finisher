#include "ToneMatchDSP.h"

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

namespace {

constexpr double kMinimumFrequencyHz = 40.0;
constexpr double kMaximumGainDb = 12.0;
constexpr double kMinimumQ = 0.25;
constexpr double kMaximumQ = 5.0;
constexpr int kCoefficientUpdateInterval = 32;

double finiteOr(
    double value,
    double fallback) noexcept {

    return std::isfinite(value)
        ? value
        : fallback;
}

}

void ToneMatchDSP::prepare(double sampleRate) {
    sampleRate_ =
        std::isfinite(sampleRate) &&
        sampleRate > 1000.0
            ? sampleRate
            : 44100.0;

    amountSmoothing_ =
        std::exp(
            -1.0 /
            (sampleRate_ * 0.010));

    prepared_ = true;
    sanitizeProfile(profile_);
    reset();
}

void ToneMatchDSP::resetFilters() noexcept {
    for (auto& filter : lowShelf_)
        filter.reset();

    for (auto& band : peaks_)
        for (auto& filter : band)
            filter.reset();

    for (auto& filter : highShelf_)
        filter.reset();

    firHistoryLeft_.fill(0.0);
    firHistoryRight_.fill(0.0);
    firWriteIndex_ = 0;
}

void ToneMatchDSP::reset() noexcept {
    amountSmoothed_ =
        std::clamp(
            finiteOr(
                amountTarget_,
                0.0),
            0.0,
            1.0);

    lastCoefficientAmount_ = -1.0;
    coefficientCountdown_ = 0;
    bypassState_ =
        !profile_.valid ||
        amountSmoothed_ <= 0.0;

    resetFilters();

    if (!bypassState_)
        updateCoefficients(
            amountSmoothed_);
}

void ToneMatchDSP::setAmount(
    double normalized) noexcept {

    amountTarget_ =
        std::clamp(
            finiteOr(
                normalized,
                0.0),
            0.0,
            1.0);
}

void ToneMatchDSP::setBassContext(
    double lowCutNormalized,
    double massNormalized) noexcept {

    // MATCH is intentionally independent from downstream mix-placement
    // stages. LOW CONTROL and MASS process the already matched signal later
    // in MetalFinisherDSP; they must not make 100% MATCH stop short of the
    // reference curve.
    (void)lowCutNormalized;
    (void)massNormalized;
}

void ToneMatchDSP::sanitizeProfile(
    const ToneMatchProfile& source) noexcept {

    profile_ = source;

    const double nyquistMargin =
        std::max(
            kMinimumFrequencyHz,
            sampleRate_ * 0.45);

    profile_.lowShelfFrequencyHz =
        std::clamp(
            finiteOr(
                profile_.
                    lowShelfFrequencyHz,
                100.0),
            kMinimumFrequencyHz,
            nyquistMargin);

    profile_.lowShelfGainDb =
        std::clamp(
            finiteOr(
                profile_.
                    lowShelfGainDb,
                0.0),
            -kMaximumGainDb,
            kMaximumGainDb);

    for (auto& peak :
         profile_.peaks) {

        peak.frequencyHz =
            std::clamp(
                finiteOr(
                    peak.frequencyHz,
                    1000.0),
                kMinimumFrequencyHz,
                nyquistMargin);

        peak.q =
            std::clamp(
                finiteOr(
                    peak.q,
                    1.0),
                kMinimumQ,
                kMaximumQ);

        peak.gainDb =
            std::clamp(
                finiteOr(
                    peak.gainDb,
                    0.0),
                -kMaximumGainDb,
                kMaximumGainDb);
    }

    profile_.highShelfFrequencyHz =
        std::clamp(
            finiteOr(
                profile_.
                    highShelfFrequencyHz,
                9000.0),
            kMinimumFrequencyHz,
            nyquistMargin);

    profile_.highShelfGainDb =
        std::clamp(
            finiteOr(
                profile_.
                    highShelfGainDb,
                0.0),
            -kMaximumGainDb,
            kMaximumGainDb);

    if (profile_.firValid) {
        bool anyEnergy = false;

        for (double& tap :
             profile_.firTaps) {

            tap =
                std::clamp(
                    finiteOr(tap, 0.0),
                    -8.0,
                    8.0);

            anyEnergy =
                anyEnergy ||
                std::abs(tap) > 1.0e-15;
        }

        if (!anyEnergy)
            profile_.firValid = false;
    }
}

void ToneMatchDSP::setProfile(
    const ToneMatchProfile& profile) noexcept {

    sanitizeProfile(profile);

    resetFilters();
    lastCoefficientAmount_ = -1.0;
    coefficientCountdown_ = 0;

    if (prepared_ &&
        profile_.valid &&
        amountSmoothed_ > 0.0) {

        updateCoefficients(
            amountSmoothed_);
        bypassState_ = false;
    } else {
        bypassState_ = true;
    }
}

void ToneMatchDSP::clearProfile() noexcept {
    ToneMatchProfile cleared {};
    cleared.valid = false;
    setProfile(cleared);
}

void ToneMatchDSP::updateCoefficients(
    double amount) noexcept {

    const double scaledAmount =
        std::clamp(
            finiteOr(amount, 0.0),
            0.0,
            1.0);

    const double lowShelfGainDb =
        profile_.lowShelfGainDb;

    const auto low =
        makeLowShelf(
            sampleRate_,
            profile_.
                lowShelfFrequencyHz,
            lowShelfGainDb *
                scaledAmount);

    const auto high =
        makeHighShelf(
            sampleRate_,
            profile_.
                highShelfFrequencyHz,
            profile_.
                highShelfGainDb *
                scaledAmount);

    for (auto& filter : lowShelf_)
        filter.setCoefficients(low);

    for (std::size_t band = 0;
         band < peaks_.size();
         ++band) {

        double peakGainDb =
            profile_.peaks[band].gainDb;

        const double peakFrequencyHz =
            profile_.peaks[band].frequencyHz;

        const auto coefficients =
            makePeaking(
                sampleRate_,
                peakFrequencyHz,
                profile_.peaks[band].q,
                peakGainDb *
                    scaledAmount);

        for (auto& filter :
             peaks_[band]) {
            filter.setCoefficients(
                coefficients);
        }
    }

    for (auto& filter : highShelf_)
        filter.setCoefficients(high);

    lastCoefficientAmount_ =
        scaledAmount;

    coefficientCountdown_ =
        kCoefficientUpdateInterval;
}

double ToneMatchDSP::processFirSample(
    double input,
    std::array<double, 2 * kToneMatchFirTapCount>& history) noexcept {

    history[firWriteIndex_] = input;
    history[firWriteIndex_ + kToneMatchFirTapCount] = input;

    const double* samples =
        history.data() + firWriteIndex_;

    double sum = 0.0;

    for (std::size_t i = 0;
         i < kToneMatchFirTapCount;
         ++i) {

        sum +=
            profile_.firTaps[i] *
            samples[i];
    }

    return std::isfinite(sum)
        ? sum
        : 0.0;
}

void ToneMatchDSP::processFrame(
    double& left,
    double& right) noexcept {

    if (!std::isfinite(left))
        left = 0.0;

    if (!std::isfinite(right))
        right = 0.0;

    if (!prepared_ ||
        !profile_.valid) {
        return;
    }

    amountSmoothed_ =
        amountSmoothing_ *
            amountSmoothed_ +
        (1.0 - amountSmoothing_) *
            amountTarget_;

    if (std::abs(
            amountSmoothed_ -
            amountTarget_) <
        1.0e-10) {

        amountSmoothed_ =
            amountTarget_;
    }

    if (amountTarget_ <= 0.0 &&
        amountSmoothed_ <= 1.0e-9) {

        if (!bypassState_) {
            resetFilters();
            bypassState_ = true;
            lastCoefficientAmount_ = 0.0;
        }

        // Hard neutral endpoint: no filter is touched.
        return;
    }

    bypassState_ = false;

    if (profile_.firValid) {
        firWriteIndex_ =
            (firWriteIndex_ +
             kToneMatchFirTapCount - 1u) %
            kToneMatchFirTapCount;

        const double dryLeft = left;
        const double dryRight = right;

        const double wetLeft =
            processFirSample(
                dryLeft,
                firHistoryLeft_);

        const double wetRight =
            processFirSample(
                dryRight,
                firHistoryRight_);

        left =
            dryLeft +
            (wetLeft - dryLeft) *
                amountSmoothed_;

        right =
            dryRight +
            (wetRight - dryRight) *
                amountSmoothed_;
    } else {
        if (--coefficientCountdown_ <= 0 ||
            std::abs(
                amountSmoothed_ -
                lastCoefficientAmount_) >
                0.005) {

            updateCoefficients(
                amountSmoothed_);
        }

        left =
            lowShelf_[0].process(left);

        right =
            lowShelf_[1].process(right);

        for (auto& band : peaks_) {
            left =
                band[0].process(left);

            right =
                band[1].process(right);
        }

        left =
            highShelf_[0].process(left);

        right =
            highShelf_[1].process(right);
    }

    if (!std::isfinite(left))
        left = 0.0;

    if (!std::isfinite(right))
        right = 0.0;
}

} // namespace HighGainGuitarFinisher::dsp
