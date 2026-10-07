#include "ToneMatchAnalyzer.h"
#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace HighGainGuitarFinisher::dsp {

namespace {

constexpr double kPi =
    3.141592653589793238462643383279502884;

constexpr double kMinimumDb = -120.0;
constexpr double kMaximumMatchDb = 12.0;

double clampFinite(
    double value,
    double fallback) noexcept {

    return std::isfinite(value)
        ? value
        : fallback;
}

}

void ToneMatchAnalyzer::prepare(
    double sampleRate) noexcept {

    sampleRate_ =
        std::isfinite(sampleRate) &&
        sampleRate > 1000.0
            ? sampleRate
            : 44100.0;

    reset();
}

void ToneMatchAnalyzer::reset() noexcept {
    fifo_.fill(0.0);
    powerSum_.fill(0.0L);
    fifoFill_ = 0;
    frameCount_ = 0;
}

void ToneMatchAnalyzer::pushStereo(
    const double* left,
    const double* right,
    std::size_t count) noexcept {

    if (!left || count == 0)
        return;

    for (std::size_t i = 0;
         i < count;
         ++i) {

        double l =
            std::isfinite(left[i])
                ? left[i]
                : 0.0;

        double r =
            right && std::isfinite(right[i])
                ? right[i]
                : l;

        fifo_[fifoFill_++] =
            0.5 * (l + r);

        if (fifoFill_ == kFftSize) {
            processFrame();

            for (std::size_t j = 0;
                 j < kFftSize - kHopSize;
                 ++j) {
                fifo_[j] =
                    fifo_[j + kHopSize];
            }

            fifoFill_ =
                kFftSize - kHopSize;
        }
    }
}

void ToneMatchAnalyzer::fft(
    std::array<
        std::complex<double>,
        kFftSize>& data) noexcept {

    std::size_t j = 0;

    for (std::size_t i = 1;
         i < kFftSize;
         ++i) {

        std::size_t bit =
            kFftSize >> 1;

        for (;
             j & bit;
             bit >>= 1) {
            j ^= bit;
        }

        j ^= bit;

        if (i < j)
            std::swap(
                data[i],
                data[j]);
    }

    for (std::size_t len = 2;
         len <= kFftSize;
         len <<= 1) {

        const double angle =
            -2.0 * kPi /
            static_cast<double>(len);

        const std::complex<double> wLen {
            std::cos(angle),
            std::sin(angle)
        };

        for (std::size_t i = 0;
             i < kFftSize;
             i += len) {

            std::complex<double> w {1.0, 0.0};

            for (std::size_t k = 0;
                 k < len / 2;
                 ++k) {

                const auto u =
                    data[i + k];

                const auto v =
                    data[i + k + len / 2] * w;

                data[i + k] =
                    u + v;

                data[i + k + len / 2] =
                    u - v;

                w *= wLen;
            }
        }
    }
}

void ToneMatchAnalyzer::processFrame() noexcept {
    std::array<
        std::complex<double>,
        kFftSize> spectrum {};

    long double mean = 0.0L;

    for (const double sample : fifo_)
        mean += sample;

    mean /=
        static_cast<long double>(
            kFftSize);

    for (std::size_t i = 0;
         i < kFftSize;
         ++i) {

        const double window =
            0.5 -
            0.5 *
                std::cos(
                    2.0 * kPi *
                    static_cast<double>(i) /
                    static_cast<double>(
                        kFftSize - 1));

        const double sample =
            fifo_[i] -
            static_cast<double>(mean);

        spectrum[i] =
            std::complex<double> {
                sample * window,
                0.0
            };
    }

    fft(spectrum);

    for (std::size_t bin = 0;
         bin < kSpectrumBins;
         ++bin) {

        const double magnitude =
            std::abs(
                spectrum[bin]);

        powerSum_[bin] +=
            static_cast<long double>(
                magnitude * magnitude);
    }

    ++frameCount_;
}

double ToneMatchAnalyzer::interpolatedMagnitudeDb(
    double frequencyHz) const noexcept {

    if (frameCount_ == 0)
        return kMinimumDb;

    const double nyquist =
        sampleRate_ * 0.5;

    const double frequency =
        std::clamp(
            clampFinite(
                frequencyHz,
                1000.0),
            0.0,
            nyquist);

    const double bin =
        frequency *
        static_cast<double>(
            kFftSize) /
        sampleRate_;

    const std::size_t index0 =
        std::min(
            static_cast<std::size_t>(
                std::floor(bin)),
            kSpectrumBins - 1);

    const std::size_t index1 =
        std::min(
            index0 + 1,
            kSpectrumBins - 1);

    const double fraction =
        bin -
        static_cast<double>(
            index0);

    const auto powerAt =
        [&](std::size_t index) noexcept {

            return
                static_cast<double>(
                    powerSum_[index] /
                    static_cast<long double>(
                        frameCount_));
        };

    const double p0 =
        std::max(
            powerAt(index0),
            1.0e-24);

    const double p1 =
        std::max(
            powerAt(index1),
            1.0e-24);

    const double db0 =
        10.0 *
        std::log10(p0);

    const double db1 =
        10.0 *
        std::log10(p1);

    return
        db0 +
        (db1 - db0) *
            fraction;
}

double ToneMatchAnalyzer::peakMagnitudeDb() const noexcept {
    if (frameCount_ == 0)
        return kMinimumDb;

    double maximum = kMinimumDb;

    for (std::size_t bin = 1;
         bin < kSpectrumBins;
         ++bin) {

        const double power =
            static_cast<double>(
                powerSum_[bin] /
                static_cast<long double>(
                    frameCount_));

        if (power <= 0.0)
            continue;

        maximum =
            std::max(
                maximum,
                10.0 *
                    std::log10(
                        std::max(
                            power,
                            1.0e-24)));
    }

    return maximum;
}

ToneMatchSpectrumSnapshot
ToneMatchAnalyzer::snapshot() const noexcept {

    ToneMatchSpectrumSnapshot result {};
    result.sampleRate = sampleRate_;
    result.frameCount =
        static_cast<std::uint64_t>(
            frameCount_);

    if (frameCount_ == 0)
        return result;

    const long double divisor =
        static_cast<long double>(
            frameCount_);

    for (std::size_t bin = 0;
         bin < kSpectrumBins;
         ++bin) {

        result.meanPower[bin] =
            static_cast<double>(
                powerSum_[bin] /
                divisor);
    }

    return result;
}

namespace {

double snapshotMagnitudeDb(
    const ToneMatchSpectrumSnapshot& snapshot,
    double frequencyHz) noexcept {

    if (snapshot.frameCount < 1 ||
        !std::isfinite(snapshot.sampleRate) ||
        snapshot.sampleRate <= 1000.0) {
        return kMinimumDb;
    }

    const double nyquist =
        snapshot.sampleRate * 0.5;

    const double frequency =
        std::clamp(
            clampFinite(
                frequencyHz,
                1000.0),
            0.0,
            nyquist);

    const double bin =
        frequency *
        static_cast<double>(
            ToneMatchAnalyzer::kFftSize) /
        snapshot.sampleRate;

    const std::size_t index0 =
        std::min(
            static_cast<std::size_t>(
                std::floor(bin)),
            ToneMatchAnalyzer::kSpectrumBins - 1);

    const std::size_t index1 =
        std::min(
            index0 + 1,
            ToneMatchAnalyzer::kSpectrumBins - 1);

    const double fraction =
        bin -
        static_cast<double>(
            index0);

    const double p0 =
        std::max(
            snapshot.meanPower[index0],
            1.0e-24);

    const double p1 =
        std::max(
            snapshot.meanPower[index1],
            1.0e-24);

    const double db0 =
        10.0 * std::log10(p0);

    const double db1 =
        10.0 * std::log10(p1);

    return db0 +
        (db1 - db0) * fraction;
}

double snapshotPeakDb(
    const ToneMatchSpectrumSnapshot& snapshot) noexcept {

    if (snapshot.frameCount < 1)
        return kMinimumDb;

    double maximum = kMinimumDb;

    for (std::size_t bin = 1;
         bin < snapshot.meanPower.size();
         ++bin) {

        const double power =
            snapshot.meanPower[bin];

        if (!std::isfinite(power) ||
            power <= 0.0) {
            continue;
        }

        maximum =
            std::max(
                maximum,
                10.0 *
                    std::log10(
                        std::max(
                            power,
                            1.0e-24)));
    }

    return maximum;
}

}

double snapshotBandPower(
    const ToneMatchSpectrumSnapshot& snapshot,
    double minimumFrequencyHz,
    double maximumFrequencyHz) noexcept {

    if (snapshot.frameCount < 1 ||
        !std::isfinite(snapshot.sampleRate) ||
        snapshot.sampleRate <= 1000.0) {
        return 0.0;
    }

    const double nyquist =
        snapshot.sampleRate * 0.5;

    const double minimum =
        std::clamp(
            minimumFrequencyHz,
            0.0,
            nyquist);

    const double maximum =
        std::clamp(
            maximumFrequencyHz,
            minimum,
            nyquist);

    const auto firstBin =
        std::min(
            static_cast<std::size_t>(
                std::ceil(
                    minimum *
                    static_cast<double>(
                        ToneMatchAnalyzer::kFftSize) /
                    snapshot.sampleRate)),
            ToneMatchAnalyzer::kSpectrumBins - 1);

    const auto lastBin =
        std::min(
            static_cast<std::size_t>(
                std::floor(
                    maximum *
                    static_cast<double>(
                        ToneMatchAnalyzer::kFftSize) /
                    snapshot.sampleRate)),
            ToneMatchAnalyzer::kSpectrumBins - 1);

    long double total = 0.0L;

    for (std::size_t bin = firstBin;
         bin <= lastBin;
         ++bin) {

        const double power =
            snapshot.meanPower[bin];

        if (!std::isfinite(power) ||
            power <= 0.0) {
            continue;
        }

        total +=
            static_cast<long double>(
                power);
    }

    return
        static_cast<double>(
            total);
}


double biquadMagnitudeDb(
    const BiquadCoefficients& coefficients,
    double sampleRate,
    double frequencyHz) noexcept {

    const double omega =
        2.0 * kPi *
        frequencyHz /
        sampleRate;

    const std::complex<double> z1 {
        std::cos(-omega),
        std::sin(-omega)
    };

    const std::complex<double> z2 =
        z1 * z1;

    const std::complex<double> numerator =
        coefficients.b0 +
        coefficients.b1 * z1 +
        coefficients.b2 * z2;

    const std::complex<double> denominator =
        1.0 +
        coefficients.a1 * z1 +
        coefficients.a2 * z2;

    const double magnitude =
        std::abs(numerator) /
        std::max(
            std::abs(denominator),
            1.0e-18);

    return
        20.0 *
        std::log10(
            std::max(
                magnitude,
                1.0e-12));
}

double profileResponseDb(
    const ToneMatchProfile& profile,
    double sampleRate,
    double frequencyHz) noexcept {

    double response =
        biquadMagnitudeDb(
            makeLowShelf(
                sampleRate,
                profile.lowShelfFrequencyHz,
                profile.lowShelfGainDb),
            sampleRate,
            frequencyHz);

    for (const auto& peak :
         profile.peaks) {

        response +=
            biquadMagnitudeDb(
                makePeaking(
                    sampleRate,
                    peak.frequencyHz,
                    peak.q,
                    peak.gainDb),
                sampleRate,
                frequencyHz);
    }

    response +=
        biquadMagnitudeDb(
            makeHighShelf(
                sampleRate,
                profile.highShelfFrequencyHz,
                profile.highShelfGainDb),
            sampleRate,
            frequencyHz);

    return response;
}

template <typename DesiredCurve>
double profileFitError(
    const ToneMatchProfile& profile,
    double sampleRate,
    const DesiredCurve& desiredAt) noexcept {

    constexpr std::size_t kFitPointCount = 96u;
    constexpr double kMinimumFitHz = 30.0;
    constexpr double kMaximumFitHz = 3500.0;

    const double maximumFrequency =
        std::min(
            kMaximumFitHz,
            sampleRate * 0.45);

    const double logMinimum =
        std::log(kMinimumFitHz);

    const double logMaximum =
        std::log(maximumFrequency);

    long double squaredError = 0.0L;

    for (std::size_t i = 0;
         i < kFitPointCount;
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                kFitPointCount - 1u);

        const double frequency =
            std::exp(
                logMinimum +
                (logMaximum - logMinimum) *
                    position);

        const double residual =
            profileResponseDb(
                profile,
                sampleRate,
                frequency) -
            desiredAt(frequency);

        squaredError +=
            residual * residual;
    }

    return
        static_cast<double>(
            squaredError /
            static_cast<long double>(
                kFitPointCount));
}

template <typename DesiredCurve>
void refineProfileGains(
    ToneMatchProfile& profile,
    double sampleRate,
    const DesiredCurve& desiredAt) noexcept {

    // Offline/controller-side coordinate descent. The realtime topology stays
    // exactly the same; this only chooses gains that account for the actual
    // summed response of the overlapping shelves and peaking filters.
    constexpr std::array<double, 5> steps {
        2.0, 1.0, 0.5, 0.25, 0.125
    };

    auto optimizeGain =
        [&](double& gain,
            double step) noexcept {

            const double original = gain;
            double bestGain = original;
            double bestError =
                profileFitError(
                    profile,
                    sampleRate,
                    desiredAt);

            for (const double direction :
                 {-1.0, 1.0}) {

                gain =
                    std::clamp(
                        original +
                            direction * step,
                        -kMaximumMatchDb,
                        kMaximumMatchDb);

                const double error =
                    profileFitError(
                        profile,
                        sampleRate,
                        desiredAt);

                if (error < bestError) {
                    bestError = error;
                    bestGain = gain;
                }
            }

            gain = bestGain;
        };

    const auto optimizeAllGains =
        [&]() noexcept {

            for (const double step :
                 steps) {

                optimizeGain(
                    profile.lowShelfGainDb,
                    step);

                for (auto& peak :
                     profile.peaks) {
                    optimizeGain(
                        peak.gainDb,
                        step);
                }

                optimizeGain(
                    profile.highShelfGainDb,
                    step);
            }
        };

    optimizeAllGains();

    // The broad shelf corners are part of the spectral fit, not a timbral
    // preference. Keep them within narrow guitar-safe ranges and optimize
    // them offline so edge mismatches do not have to be approximated by
    // neighbouring peaking bands.
    const auto optimizeFrequency =
        [&](double& frequency,
            double minimum,
            double maximum,
            const auto& frequencySteps) noexcept {

            for (const double step :
                 frequencySteps) {

                const double original =
                    frequency;

                double bestFrequency =
                    original;

                double bestError =
                    profileFitError(
                        profile,
                        sampleRate,
                        desiredAt);

                for (const double direction :
                     {-1.0, 1.0}) {

                    frequency =
                        std::clamp(
                            original +
                                direction * step,
                            minimum,
                            maximum);

                    const double error =
                        profileFitError(
                            profile,
                            sampleRate,
                            desiredAt);

                    if (error < bestError) {
                        bestError = error;
                        bestFrequency =
                            frequency;
                    }
                }

                frequency =
                    bestFrequency;
            }
        };

    static constexpr std::array<double, 4>
        lowShelfSteps {
            10.0, 5.0, 2.5, 1.25
        };

    static constexpr std::array<double, 4>
        highShelfSteps {
            500.0, 250.0, 125.0, 62.5
        };

    optimizeFrequency(
        profile.lowShelfFrequencyHz,
        40.0,
        75.0,
        lowShelfSteps);

    optimizeFrequency(
        profile.highShelfFrequencyHz,
        2500.0,
        4500.0,
        highShelfSteps);

    // Frequency moves change overlap with the adjacent bands, so give the
    // gain solver one final pass at the new shelf positions.
    optimizeAllGains();
}

double snapshotLevelOffsetDb(
    const ToneMatchSpectrumSnapshot& reference,
    const ToneMatchSpectrumSnapshot& target) noexcept {

    // Loudness is not timbre. Normalize the two analysed spectra to the same
    // broad guitar-band energy before deriving any EQ correction. This keeps
    // an otherwise identical reference recorded, for example, 6 dB louder
    // from turning into a broadband +6 dB Tone Match profile.
    constexpr double kMinimumFrequencyHz = 30.0;
    constexpr double kMaximumFrequencyHz = 3500.0;

    const double referencePower =
        snapshotBandPower(
            reference,
            kMinimumFrequencyHz,
            kMaximumFrequencyHz);

    const double targetPower =
        snapshotBandPower(
            target,
            kMinimumFrequencyHz,
            kMaximumFrequencyHz);

    if (referencePower <= 1.0e-24 ||
        targetPower <= 1.0e-24) {
        return 0.0;
    }

    const double offsetDb =
        10.0 *
        std::log10(
            referencePower /
            targetPower);

    return std::isfinite(offsetDb)
        ? offsetDb
        : 0.0;
}



ToneMatchProfile
ToneMatchAnalyzer::makeProfile(
    const ToneMatchSpectrumSnapshot& reference,
    const ToneMatchSpectrumSnapshot& target) noexcept {

    ToneMatchProfile profile {};

    if (reference.frameCount < 4 ||
        target.frameCount < 4 ||
        !std::isfinite(reference.sampleRate) ||
        !std::isfinite(target.sampleRate) ||
        reference.sampleRate <= 1000.0 ||
        target.sampleRate <= 1000.0) {
        return profile;
    }

    static constexpr std::array<double, kToneMatchPeakCount> centers {
        35.0, 50.0, 65.0, 82.0,
        105.0, 135.0, 175.0, 225.0,
        300.0, 400.0, 550.0, 750.0,
        1050.0, 1500.0, 2200.0, 3200.0
    };

    static constexpr std::array<double, kToneMatchPeakCount> qValues {
        0.70, 0.75, 0.80, 0.85,
        0.90, 0.95, 1.00, 1.00,
        1.00, 1.00, 1.00, 1.05,
        1.10, 1.10, 1.05, 0.95
    };

    static constexpr std::array<double, kToneMatchPeakCount> zoneWeight {
        0.80, 0.90, 1.00, 1.00,
        1.00, 1.00, 1.00, 1.00,
        0.95, 0.92, 0.92, 0.96,
        1.00, 0.92, 0.68, 0.42
    };

    static constexpr std::array<double, kToneMatchPeakCount> maximumBoostDb {
        1.5, 2.0, 2.6, 3.2,
        3.8, 4.2, 4.4, 4.4,
        4.2, 4.0, 4.0, 4.2,
        4.4, 4.0, 3.0, 2.0
    };

    static constexpr std::array<double, kToneMatchPeakCount> maximumCutDb {
        3.0, 3.4, 3.8, 4.2,
        4.6, 4.8, 5.0, 5.0,
        4.8, 4.6, 4.5, 4.5,
        4.5, 4.2, 3.6, 2.8
    };

    const double referencePeakDb =
        snapshotPeakDb(reference);

    const double targetPeakDb =
        snapshotPeakDb(target);

    const double levelOffsetDb =
        snapshotLevelOffsetDb(
            reference,
            target);

    constexpr double kActivityFloorDb = 48.0;

    const auto rawDifferenceAt =
        [&](double frequency) noexcept {

            const double referenceDb =
                snapshotMagnitudeDb(
                    reference,
                    frequency);

            const double targetDb =
                snapshotMagnitudeDb(
                    target,
                    frequency);

            const bool referenceActive =
                referenceDb >=
                referencePeakDb -
                    kActivityFloorDb;

            const bool targetActive =
                targetDb >=
                targetPeakDb -
                    kActivityFloorDb;

            if (!referenceActive &&
                !targetActive) {
                return 0.0;
            }

            const double difference =
                (referenceDb - targetDb) -
                levelOffsetDb;

            return std::isfinite(difference)
                ? difference
                : 0.0;
        };

    std::array<double, kToneMatchPeakCount>
        zoneDifference {};

    for (std::size_t i = 0;
         i < centers.size();
         ++i) {

        const double center =
            centers[i];

        // First reduce single-bin/narrow-feature influence by measuring a
        // local log-frequency neighbourhood. No dB clamp occurs here.
        zoneDifference[i] =
            0.15 * rawDifferenceAt(center / 1.30) +
            0.20 * rawDifferenceAt(center / 1.14) +
            0.30 * rawDifferenceAt(center) +
            0.20 * rawDifferenceAt(center * 1.14) +
            0.15 * rawDifferenceAt(center * 1.30);
    }

    // Explicit broad-curve smoothing before any protection limits. Two
    // [0.25, 0.50, 0.25] passes suppress narrow reference features while
    // preserving the large-scale bass tonal shape.
    for (int pass = 0; pass < 2; ++pass) {
        const auto previous =
            zoneDifference;

        for (std::size_t i = 0;
             i < zoneDifference.size();
             ++i) {

            const double left =
                previous[
                    i > 0
                        ? i - 1
                        : i];

            const double right =
                previous[
                    i + 1 <
                            previous.size()
                        ? i + 1
                        : i];

            zoneDifference[i] =
                0.25 * left +
                0.50 * previous[i] +
                0.25 * right;
        }
    }

    std::array<double, kToneMatchPeakCount>
        protectedDesired {};

    for (std::size_t i = 0;
         i < protectedDesired.size();
         ++i) {

        protectedDesired[i] =
            std::clamp(
                zoneDifference[i] *
                    zoneWeight[i],
                -maximumCutDb[i],
                maximumBoostDb[i]);
    }

    const auto desiredAt =
        [&](double frequency) noexcept {

            const double safeFrequency =
                std::max(
                    frequency,
                    centers.front());

            if (safeFrequency <=
                centers.front()) {
                return
                    protectedDesired.front();
            }

            if (safeFrequency >=
                centers.back()) {
                return
                    protectedDesired.back();
            }

            for (std::size_t i = 0;
                 i + 1 < centers.size();
                 ++i) {

                if (safeFrequency >
                    centers[i + 1]) {
                    continue;
                }

                const double logA =
                    std::log(
                        centers[i]);

                const double logB =
                    std::log(
                        centers[i + 1]);

                const double t =
                    std::clamp(
                        (std::log(
                             safeFrequency) -
                         logA) /
                            (logB - logA),
                        0.0,
                        1.0);

                return
                    protectedDesired[i] +
                    (protectedDesired[i + 1] -
                     protectedDesired[i]) *
                        t;
            }

            return
                protectedDesired.back();
        };

    profile.valid = true;
    profile.lowShelfFrequencyHz = 55.0;
    profile.lowShelfGainDb =
        std::clamp(
            0.65 * protectedDesired[0] +
            0.35 * protectedDesired[1],
            -3.0,
            1.5);

    for (std::size_t i = 0;
         i < centers.size();
         ++i) {

        auto& peak =
            profile.peaks[i];

        peak.frequencyHz =
            centers[i];

        peak.q =
            qValues[i];

        peak.gainDb =
            protectedDesired[i];
    }

    profile.highShelfFrequencyHz = 3200.0;
    profile.highShelfGainDb =
        std::clamp(
            0.35 * protectedDesired[
                protectedDesired.size() - 2] +
            0.65 * protectedDesired.back(),
            -2.5,
            1.5);

    // Solve against the protected smoothed target curve using the actual
    // combined response of every overlapping shelf and peaking filter.
    refineProfileGains(
        profile,
        target.sampleRate,
        desiredAt);

    profile.lowShelfGainDb =
        std::clamp(profile.lowShelfGainDb, -3.0, 1.5);

    for (std::size_t i = 0; i < profile.peaks.size(); ++i) {
        profile.peaks[i].gainDb =
            std::clamp(
                profile.peaks[i].gainDb,
                -maximumCutDb[i],
                maximumBoostDb[i]);
    }

    profile.highShelfGainDb =
        std::clamp(profile.highShelfGainDb, -2.5, 1.5);

    constexpr double kMaximumAdjacentStepDb = 1.6;

    for (std::size_t i = 1; i < profile.peaks.size(); ++i) {
        profile.peaks[i].gainDb =
            std::clamp(
                profile.peaks[i].gainDb,
                profile.peaks[i - 1].gainDb - kMaximumAdjacentStepDb,
                profile.peaks[i - 1].gainDb + kMaximumAdjacentStepDb);
    }

    for (std::size_t i = profile.peaks.size() - 1; i > 0; --i) {
        profile.peaks[i - 1].gainDb =
            std::clamp(
                profile.peaks[i - 1].gainDb,
                profile.peaks[i].gainDb - kMaximumAdjacentStepDb,
                profile.peaks[i].gainDb + kMaximumAdjacentStepDb);
    }

    return profile;
}

ToneMatchProfile
ToneMatchAnalyzer::makeProfileAgainst(
    const ToneMatchAnalyzer& target) const noexcept {

    return makeProfile(
        snapshot(),
        target.snapshot());
}

} // namespace HighGainGuitarFinisher::dsp
