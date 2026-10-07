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

    constexpr std::size_t kFitPointCount = 128u;
    constexpr double kMinimumFitHz = 30.0;
    constexpr double kMaximumFitHz = 10000.0;

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

double maximumBoostAt(double frequencyHz) noexcept {
    if (frequencyHz < 50.0)
        return 1.5;
    if (frequencyHz < 65.0)
        return 2.0;
    if (frequencyHz < 80.0)
        return 3.0;
    if (frequencyHz < 120.0)
        return 5.5;
    if (frequencyHz < 7000.0)
        return 8.0;
    return 6.0;
}

double maximumCutAt(double frequencyHz) noexcept {
    if (frequencyHz < 50.0)
        return 4.0;
    if (frequencyHz < 65.0)
        return 5.0;
    if (frequencyHz < 80.0)
        return 6.0;
    if (frequencyHz < 7000.0)
        return 8.0;
    return 7.0;
}

template <typename DesiredCurve>
void refineProfileGains(
    ToneMatchProfile& profile,
    double sampleRate,
    const DesiredCurve& desiredAt) noexcept {

    constexpr std::size_t kVariableCount =
        kToneMatchPeakCount + 2u;

    constexpr std::size_t kPointCount =
        128u;

    constexpr double kMinimumFitHz =
        30.0;

    const double maximumFitHz =
        std::min(
            10000.0,
            sampleRate * 0.45);

    const double logMinimum =
        std::log(kMinimumFitHz);

    const double logMaximum =
        std::log(maximumFitHz);

    const auto getGain =
        [&](std::size_t index) noexcept
            -> double& {

            if (index == 0u)
                return profile.lowShelfGainDb;

            if (index <=
                kToneMatchPeakCount) {
                return profile.peaks[
                    index - 1u].gainDb;
            }

            return profile.highShelfGainDb;
        };

    const auto limitsFor =
        [&](std::size_t index) noexcept {

            if (index == 0u)
                return std::pair<double, double> {
                    -6.0, 4.0
                };

            if (index <=
                kToneMatchPeakCount) {

                const double frequency =
                    profile.peaks[
                        index - 1u].
                            frequencyHz;

                return std::pair<double, double> {
                    -maximumCutAt(
                        frequency),
                    maximumBoostAt(
                        frequency)
                };
            }

            return std::pair<double, double> {
                -6.0, 6.0
            };
        };

    std::array<double, kPointCount>
        frequencies {};

    for (std::size_t i = 0;
         i < kPointCount;
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                kPointCount - 1u);

        frequencies[i] =
            std::exp(
                logMinimum +
                (logMaximum -
                 logMinimum) *
                    position);
    }

    // Gauss-Newton on the actual summed dB response. Unlike the old
    // coordinate descent this solves all overlapping gain parameters together
    // and therefore does not depend on band iteration order.
    for (int iteration = 0;
         iteration < 5;
         ++iteration) {

        std::array<
            std::array<double, kVariableCount>,
            kPointCount> jacobian {};

        std::array<double, kPointCount>
            residual {};

        for (std::size_t point = 0;
             point < kPointCount;
             ++point) {

            const double frequency =
                frequencies[point];

            residual[point] =
                desiredAt(frequency) -
                profileResponseDb(
                    profile,
                    sampleRate,
                    frequency);
        }

        constexpr double kProbeDb =
            0.20;

        for (std::size_t variable = 0;
             variable < kVariableCount;
             ++variable) {

            double& gain =
                getGain(variable);

            const double original =
                gain;

            const auto limits =
                limitsFor(variable);

            const double probe =
                std::clamp(
                    original + kProbeDb,
                    limits.first,
                    limits.second);

            const double delta =
                probe - original;

            if (std::abs(delta) <
                1.0e-12) {
                continue;
            }

            gain = probe;

            for (std::size_t point = 0;
                 point < kPointCount;
                 ++point) {

                const double changed =
                    profileResponseDb(
                        profile,
                        sampleRate,
                        frequencies[point]);

                gain = original;

                const double base =
                    profileResponseDb(
                        profile,
                        sampleRate,
                        frequencies[point]);

                gain = probe;

                jacobian[point][variable] =
                    (changed - base) /
                    delta;
            }

            gain = original;
        }

        std::array<
            std::array<double, kVariableCount>,
            kVariableCount> normal {};

        std::array<double, kVariableCount>
            rhs {};

        for (std::size_t row = 0;
             row < kVariableCount;
             ++row) {

            for (std::size_t point = 0;
                 point < kPointCount;
                 ++point) {

                rhs[row] +=
                    jacobian[point][row] *
                    residual[point];

                for (std::size_t column = 0;
                     column < kVariableCount;
                     ++column) {

                    normal[row][column] +=
                        jacobian[point][row] *
                        jacobian[point][column];
                }
            }

            // Small Tikhonov damping stabilizes overlapping/near-collinear
            // filters without intentionally shrinking the solution.
            normal[row][row] += 1.0e-3;
        }

        // Gaussian elimination with partial pivoting.
        for (std::size_t pivot = 0;
             pivot < kVariableCount;
             ++pivot) {

            std::size_t bestRow =
                pivot;

            double bestMagnitude =
                std::abs(
                    normal[pivot][pivot]);

            for (std::size_t row =
                     pivot + 1u;
                 row < kVariableCount;
                 ++row) {

                const double magnitude =
                    std::abs(
                        normal[row][pivot]);

                if (magnitude >
                    bestMagnitude) {
                    bestMagnitude =
                        magnitude;
                    bestRow = row;
                }
            }

            if (bestMagnitude <
                1.0e-12) {
                continue;
            }

            if (bestRow != pivot) {
                std::swap(
                    normal[bestRow],
                    normal[pivot]);

                std::swap(
                    rhs[bestRow],
                    rhs[pivot]);
            }

            const double divisor =
                normal[pivot][pivot];

            for (std::size_t column =
                     pivot;
                 column < kVariableCount;
                 ++column) {
                normal[pivot][column] /=
                    divisor;
            }

            rhs[pivot] /=
                divisor;

            for (std::size_t row = 0;
                 row < kVariableCount;
                 ++row) {

                if (row == pivot)
                    continue;

                const double factor =
                    normal[row][pivot];

                if (std::abs(factor) <
                    1.0e-18) {
                    continue;
                }

                for (std::size_t column =
                         pivot;
                     column < kVariableCount;
                     ++column) {

                    normal[row][column] -=
                        factor *
                        normal[pivot][column];
                }

                rhs[row] -=
                    factor *
                    rhs[pivot];
            }
        }

        double maximumStep = 0.0;

        for (std::size_t variable = 0;
             variable < kVariableCount;
             ++variable) {

            double& gain =
                getGain(variable);

            const auto limits =
                limitsFor(variable);

            const double step =
                std::clamp(
                    rhs[variable],
                    -2.0,
                    2.0);

            const double next =
                std::clamp(
                    gain + step,
                    limits.first,
                    limits.second);

            maximumStep =
                std::max(
                    maximumStep,
                    std::abs(
                        next - gain));

            gain = next;
        }

        if (maximumStep < 0.01)
            break;
    }
}

template <typename DesiredCurve>
void refineProfileShape(
    ToneMatchProfile& profile,
    double sampleRate,
    const DesiredCurve& desiredAt,
    double maximumFrequency) noexcept {

    constexpr std::array<double, 3> frequencyRatios {
        1.20, 1.10, 1.05
    };

    constexpr std::array<double, 3> qRatios {
        1.40, 1.20, 1.10
    };

    for (std::size_t pass = 0;
         pass < frequencyRatios.size();
         ++pass) {

        for (auto& peak :
             profile.peaks) {

            double bestError =
                profileFitError(
                    profile,
                    sampleRate,
                    desiredAt);

            const double originalFrequency =
                peak.frequencyHz;

            double bestFrequency =
                originalFrequency;

            const double ratio =
                frequencyRatios[pass];

            for (const double candidate :
                 {
                    originalFrequency / ratio,
                    originalFrequency * ratio
                 }) {

                peak.frequencyHz =
                    std::clamp(
                        candidate,
                        35.0,
                        maximumFrequency);

                const double error =
                    profileFitError(
                        profile,
                        sampleRate,
                        desiredAt);

                if (error < bestError) {
                    bestError = error;
                    bestFrequency =
                        peak.frequencyHz;
                }
            }

            peak.frequencyHz =
                bestFrequency;

            const double originalQ =
                peak.q;

            double bestQ =
                originalQ;

            const double qRatio =
                qRatios[pass];

            for (const double candidate :
                 {
                    originalQ / qRatio,
                    originalQ * qRatio
                 }) {

                peak.q =
                    std::clamp(
                        candidate,
                        0.35,
                        6.0);

                const double error =
                    profileFitError(
                        profile,
                        sampleRate,
                        desiredAt);

                if (error < bestError) {
                    bestError = error;
                    bestQ =
                        peak.q;
                }
            }

            peak.q =
                bestQ;

            peak.gainDb =
                std::clamp(
                    peak.gainDb,
                    -maximumCutAt(
                        peak.frequencyHz),
                    maximumBoostAt(
                        peak.frequencyHz));
        }

        refineProfileGains(
            profile,
            sampleRate,
            desiredAt);
    }
}

double snapshotLevelOffsetDb(
    const ToneMatchSpectrumSnapshot& reference,
    const ToneMatchSpectrumSnapshot& target) noexcept {

    // Loudness is not timbre. Normalize the two analysed spectra to the same
    // broad guitar-band energy before deriving any EQ correction. This keeps
    // an otherwise identical reference recorded, for example, 6 dB louder
    // from turning into a broadband +6 dB Tone Match profile.
    constexpr double kMinimumFrequencyHz = 30.0;
    constexpr double kMaximumFrequencyHz = 10000.0;

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

    const double sampleRate =
        target.sampleRate;

    const double maximumFrequency =
        std::min(
            10000.0,
            sampleRate * 0.45);

    const double referencePeakDb =
        snapshotPeakDb(reference);

    const double targetPeakDb =
        snapshotPeakDb(target);

    const double levelOffsetDb =
        snapshotLevelOffsetDb(
            reference,
            target);

    constexpr double kActivityFloorDb =
        48.0;

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

    // This is the actual MATCH target: reference spectrum minus target
    // spectrum. Only a small local log-frequency average is applied so a
    // single FFT bin or narrow accidental resonance is not copied literally.
    const auto desiredAt =
        [&](double frequency) noexcept {

            const double f =
                std::clamp(
                    frequency,
                    35.0,
                    maximumFrequency);

            const double raw =
                0.12 * rawDifferenceAt(
                    f / 1.18) +
                0.20 * rawDifferenceAt(
                    f / 1.08) +
                0.36 * rawDifferenceAt(f) +
                0.20 * rawDifferenceAt(
                    f * 1.08) +
                0.12 * rawDifferenceAt(
                    f * 1.18);

            return
                std::clamp(
                    raw,
                    -maximumCutAt(f),
                    maximumBoostAt(f));
        };

    profile.valid = true;

    // Broad edge correction first. The 16 peaking sections below then fit the
    // remaining difference wherever the measured curve actually needs them.
    profile.lowShelfFrequencyHz = 55.0;
    profile.lowShelfGainDb =
        std::clamp(
            0.60 * desiredAt(40.0) +
            0.40 * desiredAt(70.0),
            -6.0,
            4.0);

    profile.highShelfFrequencyHz =
        std::min(
            6500.0,
            maximumFrequency * 0.80);

    profile.highShelfGainDb =
        std::clamp(
            0.45 * desiredAt(
                maximumFrequency * 0.65) +
            0.55 * desiredAt(
                maximumFrequency * 0.92),
            -6.0,
            6.0);

    for (auto& peak :
         profile.peaks) {
        peak.frequencyHz = 1000.0;
        peak.q = 1.0;
        peak.gainDb = 0.0;
    }

    constexpr std::size_t kCandidateCount =
        96u;

    std::array<double, kCandidateCount>
        candidateFrequencies {};

    const double logMinimum =
        std::log(35.0);

    const double logMaximum =
        std::log(maximumFrequency);

    for (std::size_t i = 0;
         i < candidateFrequencies.size();
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                candidateFrequencies.size() -
                1u);

        candidateFrequencies[i] =
            std::exp(
                logMinimum +
                (logMaximum -
                 logMinimum) *
                    position);
    }

    constexpr std::array<double, 8>
        qCandidates {
            0.50, 0.70, 0.90, 1.20,
            1.60, 2.20, 3.20, 4.80
        };

    constexpr std::array<double, 3>
        gainScales {
            0.70, 1.00, 1.25
        };

    // Greedy residual fit. Each existing peaking section is placed at the
    // frequency/Q that most reduces the global reference-minus-target error.
    for (std::size_t band = 0;
         band < profile.peaks.size();
         ++band) {

        auto& peak =
            profile.peaks[band];

        double bestError =
            profileFitError(
                profile,
                sampleRate,
                desiredAt);

        ToneMatchPeak bestPeak =
            peak;

        std::array<
            std::pair<double, std::size_t>,
            24> strongest {};

        for (auto& item : strongest) {
            item.first = -1.0;
            item.second = 0u;
        }

        for (std::size_t i = 0;
             i < candidateFrequencies.size();
             ++i) {

            const double frequency =
                candidateFrequencies[i];

            const double residual =
                desiredAt(frequency) -
                profileResponseDb(
                    profile,
                    sampleRate,
                    frequency);

            const double magnitude =
                std::abs(residual);

            for (std::size_t slot = 0;
                 slot < strongest.size();
                 ++slot) {

                if (magnitude <=
                    strongest[slot].first) {
                    continue;
                }

                for (std::size_t move =
                         strongest.size() - 1u;
                     move > slot;
                     --move) {
                    strongest[move] =
                        strongest[move - 1u];
                }

                strongest[slot] = {
                    magnitude,
                    i
                };

                break;
            }
        }

        for (const auto& candidate :
             strongest) {

            if (candidate.first < 0.0)
                continue;

            const double frequency =
                candidateFrequencies[
                    candidate.second];

            const double residual =
                desiredAt(frequency) -
                profileResponseDb(
                    profile,
                    sampleRate,
                    frequency);

            for (const double q :
                 qCandidates) {

                for (const double scale :
                     gainScales) {

                    peak.frequencyHz =
                        frequency;

                    peak.q = q;

                    peak.gainDb =
                        std::clamp(
                            residual * scale,
                            -maximumCutAt(
                                frequency),
                            maximumBoostAt(
                                frequency));

                    const double error =
                        profileFitError(
                            profile,
                            sampleRate,
                            desiredAt);

                    if (error < bestError) {
                        bestError = error;
                        bestPeak = peak;
                    }
                }
            }
        }

        peak = bestPeak;
    }

    // Jointly solve overlapping filter gains and then allow each band to move
    // locally in frequency and Q. This retains the compact 16-band/state
    // format while fitting the measured difference curve rather than a set of
    // hard-coded EQ centres.
    refineProfileGains(
        profile,
        sampleRate,
        desiredAt);

    refineProfileShape(
        profile,
        sampleRate,
        desiredAt,
        maximumFrequency);

    // Iterative residual refinement: measure the actual summed response after
    // the first adaptive fit and run two additional solve passes only on the
    // remaining error. This keeps the proven IIR topology but no longer treats
    // the first approximation as exact.
    for (int residualPass = 0;
         residualPass < 2;
         ++residualPass) {

        double maximumResidual = 0.0;

        constexpr std::size_t kResidualProbeCount = 64u;
        const double logMinimum =
            std::log(35.0);
        const double logMaximum =
            std::log(maximumFrequency);

        for (std::size_t i = 0;
             i < kResidualProbeCount;
             ++i) {

            const double position =
                static_cast<double>(i) /
                static_cast<double>(
                    kResidualProbeCount - 1u);

            const double frequency =
                std::exp(
                    logMinimum +
                    (logMaximum -
                     logMinimum) *
                        position);

            const double residual =
                desiredAt(frequency) -
                profileResponseDb(
                    profile,
                    sampleRate,
                    frequency);

            maximumResidual =
                std::max(
                    maximumResidual,
                    std::abs(residual));
        }

        if (maximumResidual < 0.15)
            break;

        refineProfileGains(
            profile,
            sampleRate,
            desiredAt);

        refineProfileShape(
            profile,
            sampleRate,
            desiredAt,
            maximumFrequency);
    }

    profile.lowShelfGainDb =
        std::clamp(
            profile.lowShelfGainDb,
            -6.0,
            4.0);

    profile.highShelfGainDb =
        std::clamp(
            profile.highShelfGainDb,
            -6.0,
            6.0);

    for (auto& peak :
         profile.peaks) {

        peak.frequencyHz =
            std::clamp(
                peak.frequencyHz,
                35.0,
                maximumFrequency);

        peak.q =
            std::clamp(
                peak.q,
                0.35,
                6.0);

        peak.gainDb =
            std::clamp(
                peak.gainDb,
                -maximumCutAt(
                    peak.frequencyHz),
                maximumBoostAt(
                    peak.frequencyHz));
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
