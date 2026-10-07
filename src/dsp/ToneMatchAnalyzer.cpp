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
    meanDb_.fill(0.0);
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

        if (fifoFill_ == kAnalysisFftSize) {
            processFrame();

            for (std::size_t j = 0;
                 j < kAnalysisFftSize - kHopSize;
                 ++j) {
                fifo_[j] =
                    fifo_[j + kHopSize];
            }

            fifoFill_ =
                kAnalysisFftSize - kHopSize;
        }
    }
}

void ToneMatchAnalyzer::fft(
    std::array<
        std::complex<double>,
        kAnalysisFftSize>& data) noexcept {

    std::size_t j = 0;

    for (std::size_t i = 1;
         i < kAnalysisFftSize;
         ++i) {

        std::size_t bit =
            kAnalysisFftSize >> 1;

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
         len <= kAnalysisFftSize;
         len <<= 1) {

        const double angle =
            -2.0 * kPi /
            static_cast<double>(len);

        const std::complex<double> wLen {
            std::cos(angle),
            std::sin(angle)
        };

        for (std::size_t i = 0;
             i < kAnalysisFftSize;
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
        kAnalysisFftSize> spectrum {};

    long double mean = 0.0L;
    long double squareSum = 0.0L;

    for (const double sample : fifo_) {
        mean += sample;
        squareSum +=
            static_cast<long double>(sample) *
            static_cast<long double>(sample);
    }

    mean /=
        static_cast<long double>(
            kAnalysisFftSize);

    const double rms =
        std::sqrt(
            std::max(
                static_cast<double>(
                    squareSum /
                    static_cast<long double>(
                        kAnalysisFftSize)),
                0.0));

    // Ignore effectively silent windows. Normalizing a noise-only/silent
    // frame would otherwise turn its floor into a tonal signature.
    if (rms < 1.0e-7)
        return;

    for (std::size_t i = 0;
         i < kAnalysisFftSize;
         ++i) {

        const double window =
            0.5 -
            0.5 *
                std::cos(
                    2.0 * kPi *
                    static_cast<double>(i) /
                    static_cast<double>(
                        kAnalysisFftSize - 1));

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

    // Keep the old 4096-grid power spectrum for backwards-compatible state
    // and fixtures. Because 16384 is exactly 4x 4096, these frequencies land
    // on exact high-resolution FFT bins.
    constexpr std::size_t kFftRatio =
        kAnalysisFftSize / kFftSize;

    for (std::size_t bin = 0;
         bin < kSpectrumBins;
         ++bin) {

        const std::size_t analysisBin =
            std::min(
                bin * kFftRatio,
                kAnalysisFftSize / 2);

        const double magnitude =
            std::abs(
                spectrum[analysisBin]);

        powerSum_[bin] +=
            static_cast<long double>(
                magnitude * magnitude);
    }

    std::array<double, kCurveBins>
        frameDb {};

    const double logMinimum =
        std::log(kCurveMinimumHz);

    const double logMaximum =
        std::log(
            std::min(
                kCurveMaximumHz,
                sampleRate_ * 0.45));

    double framePeakDb = kMinimumDb;

    for (std::size_t i = 0;
         i < kCurveBins;
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                kCurveBins - 1u);

        const double frequency =
            std::exp(
                logMinimum +
                (logMaximum -
                 logMinimum) *
                    position);

        const double exactBin =
            frequency *
            static_cast<double>(
                kAnalysisFftSize) /
            sampleRate_;

        const std::size_t index0 =
            std::min(
                static_cast<std::size_t>(
                    std::floor(exactBin)),
                kAnalysisFftSize / 2);

        const std::size_t index1 =
            std::min(
                index0 + 1u,
                kAnalysisFftSize / 2);

        const double fraction =
            exactBin -
            static_cast<double>(index0);

        const double m0 =
            std::max(
                std::abs(spectrum[index0]),
                1.0e-12);

        const double m1 =
            std::max(
                std::abs(spectrum[index1]),
                1.0e-12);

        const double db0 =
            20.0 * std::log10(m0);

        const double db1 =
            20.0 * std::log10(m1);

        const double db =
            db0 +
            (db1 - db0) *
                fraction;

        frameDb[i] = db;
        framePeakDb =
            std::max(
                framePeakDb,
                db);
    }

    // Separate level from tone on every frame before temporal averaging.
    // This prevents loud attacks/notes from dominating the reference shape.
    constexpr double kFrameActivityRangeDb = 60.0;

    long double activeSum = 0.0L;
    std::size_t activeCount = 0u;

    for (const double db : frameDb) {
        if (db >=
            framePeakDb -
                kFrameActivityRangeDb) {
            activeSum += db;
            ++activeCount;
        }
    }

    if (activeCount == 0u)
        return;

    const double frameLevelDb =
        static_cast<double>(
            activeSum /
            static_cast<long double>(
                activeCount));

    const std::size_t nextCount =
        frameCount_ + 1u;

    for (std::size_t i = 0;
         i < kCurveBins;
         ++i) {

        // Do not let the very-low floor of a single frame dominate the mean.
        const double normalized =
            std::max(
                frameDb[i],
                framePeakDb -
                    kFrameActivityRangeDb) -
            frameLevelDb;

        double sample =
            normalized;

        // Online robustification after a few observations. This is a
        // winsorized mean: broad persistent tonal differences survive, while
        // one-off note/resonance spikes cannot dominate a curve bin.
        if (frameCount_ >= 4u) {
            sample =
                std::clamp(
                    sample,
                    meanDb_[i] - 12.0,
                    meanDb_[i] + 12.0);
        }

        meanDb_[i] +=
            (sample - meanDb_[i]) /
            static_cast<double>(
                nextCount);
    }

    ++frameCount_;
}

double ToneMatchAnalyzer::interpolatedMagnitudeDb(
    double frequencyHz) const noexcept {

    if (frameCount_ == 0)
        return kMinimumDb;

    const double maximumFrequency =
        std::min(
            kCurveMaximumHz,
            sampleRate_ * 0.45);

    const double frequency =
        std::clamp(
            clampFinite(
                frequencyHz,
                1000.0),
            kCurveMinimumHz,
            maximumFrequency);

    const double position =
        (std::log(frequency) -
         std::log(kCurveMinimumHz)) /
        (std::log(maximumFrequency) -
         std::log(kCurveMinimumHz));

    const double bin =
        position *
        static_cast<double>(
            kCurveBins - 1u);

    const std::size_t index0 =
        std::min(
            static_cast<std::size_t>(
                std::floor(bin)),
            kCurveBins - 1u);

    const std::size_t index1 =
        std::min(
            index0 + 1u,
            kCurveBins - 1u);

    const double fraction =
        bin -
        static_cast<double>(
            index0);

    return
        meanDb_[index0] +
        (meanDb_[index1] -
         meanDb_[index0]) *
            fraction;
}

double ToneMatchAnalyzer::peakMagnitudeDb() const noexcept {
    if (frameCount_ == 0)
        return kMinimumDb;

    return *std::max_element(
        meanDb_.begin(),
        meanDb_.end());
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

    result.hasLogCurve = true;
    result.meanDb = meanDb_;

    return result;
}

namespace {

double snapshotMagnitudeDb(
    const ToneMatchSpectrumSnapshot& snapshot,
    double frequencyHz) noexcept {

    if (snapshot.hasLogCurve) {
        const double maximumFrequency =
            std::min(
                ToneMatchAnalyzer::kCurveMaximumHz,
                snapshot.sampleRate * 0.45);

        const double frequency =
            std::clamp(
                clampFinite(
                    frequencyHz,
                    1000.0),
                ToneMatchAnalyzer::kCurveMinimumHz,
                maximumFrequency);

        const double position =
            (std::log(frequency) -
             std::log(
                 ToneMatchAnalyzer::kCurveMinimumHz)) /
            (std::log(maximumFrequency) -
             std::log(
                 ToneMatchAnalyzer::kCurveMinimumHz));

        const double bin =
            position *
            static_cast<double>(
                ToneMatchAnalyzer::kCurveBins - 1u);

        const std::size_t index0 =
            std::min(
                static_cast<std::size_t>(
                    std::floor(bin)),
                ToneMatchAnalyzer::kCurveBins - 1u);

        const std::size_t index1 =
            std::min(
                index0 + 1u,
                ToneMatchAnalyzer::kCurveBins - 1u);

        const double fraction =
            bin -
            static_cast<double>(index0);

        return
            snapshot.meanDb[index0] +
            (snapshot.meanDb[index1] -
             snapshot.meanDb[index0]) *
                fraction;
    }


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

    if (snapshot.hasLogCurve) {
        return *std::max_element(
            snapshot.meanDb.begin(),
            snapshot.meanDb.end());
    }


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

double interpolateLimit(
    double frequencyHz,
    double f0,
    double v0,
    double f1,
    double v1) noexcept {

    if (frequencyHz <= f0)
        return v0;

    if (frequencyHz >= f1)
        return v1;

    const double t =
        (frequencyHz - f0) /
        (f1 - f0);

    return
        v0 +
        (v1 - v0) * t;
}

double maximumBoostAt(double frequencyHz) noexcept {
    if (frequencyHz <= 50.0)
        return 1.5;

    if (frequencyHz < 65.0)
        return interpolateLimit(
            frequencyHz,
            50.0, 1.5,
            65.0, 3.0);

    if (frequencyHz < 80.0)
        return interpolateLimit(
            frequencyHz,
            65.0, 3.0,
            80.0, 4.0);

    if (frequencyHz < 120.0)
        return interpolateLimit(
            frequencyHz,
            80.0, 4.0,
            120.0, 5.5);

    if (frequencyHz < 7000.0)
        return 8.0;

    return 6.0;
}

double maximumCutAt(double frequencyHz) noexcept {
    if (frequencyHz <= 50.0)
        return 4.0;

    if (frequencyHz < 65.0)
        return interpolateLimit(
            frequencyHz,
            50.0, 4.0,
            65.0, 6.0);

    if (frequencyHz < 80.0)
        return interpolateLimit(
            frequencyHz,
            65.0, 6.0,
            80.0, 7.0);

    if (frequencyHz < 7000.0)
        return 8.0;

    return 7.0;
}

template <typename DesiredCurve>
void refineProfileGains(
    ToneMatchProfile& profile,
    double sampleRate,
    const DesiredCurve& desiredAt) noexcept {

    constexpr std::array<double, 6> steps {
        2.0, 1.0, 0.5, 0.25, 0.125, 0.0625
    };

    auto optimizeGain =
        [&](double& gain,
            double step,
            double minimum,
            double maximum) noexcept {

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
                        minimum,
                        maximum);

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

    for (const double step : steps) {
        optimizeGain(
            profile.lowShelfGainDb,
            step,
            -6.0,
            4.0);

        for (auto& peak :
             profile.peaks) {
            optimizeGain(
                peak.gainDb,
                step,
                -maximumCutAt(
                    peak.frequencyHz),
                maximumBoostAt(
                    peak.frequencyHz));
        }

        optimizeGain(
            profile.highShelfGainDb,
            step,
            -6.0,
            6.0);
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

    // Tone matching must separate broadband level from spectral shape.
    // Use an equal-log-frequency dB offset rather than total band energy:
    // energy normalization overweights the bass and leaves a constant shape
    // error when reference and target have different low-frequency balance.
    constexpr std::size_t kPointCount = 96u;
    constexpr double kMinimumFrequencyHz = 35.0;
    constexpr double kMaximumFrequencyHz = 10000.0;
    constexpr double kActivityFloorDb = 48.0;

    const double referencePeakDb =
        snapshotPeakDb(reference);

    const double targetPeakDb =
        snapshotPeakDb(target);

    std::array<double, kPointCount>
        differences {};

    std::size_t count = 0u;

    const double logMinimum =
        std::log(kMinimumFrequencyHz);

    const double logMaximum =
        std::log(kMaximumFrequencyHz);

    for (std::size_t i = 0;
         i < kPointCount;
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                kPointCount - 1u);

        const double frequency =
            std::exp(
                logMinimum +
                (logMaximum -
                 logMinimum) *
                    position);

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
            continue;
        }

        const double difference =
            referenceDb -
            targetDb;

        if (std::isfinite(difference)) {
            differences[count++] =
                difference;
        }
    }

    if (count == 0u)
        return 0.0;

    // Equal-log-frequency mean. Narrow spectral outliers are already
    // handled later by desiredAt(), so the level normalization itself should
    // not bias the tonal reference by trimming valid broad-band differences.
    long double sum = 0.0L;

    for (std::size_t i = 0;
         i < count;
         ++i) {
        sum += differences[i];
    }

    const double offsetDb =
        static_cast<double>(
            sum /
            static_cast<long double>(
                count));

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

            const double center =
                rawDifferenceAt(f);

            std::array<double, 4> neighbours {
                rawDifferenceAt(f / 1.18),
                rawDifferenceAt(f / 1.08),
                rawDifferenceAt(f * 1.08),
                rawDifferenceAt(f * 1.18)
            };

            std::sort(
                neighbours.begin(),
                neighbours.end());

            const double neighbourMedian =
                0.5 *
                (neighbours[1] +
                 neighbours[2]);

            const double localSpread =
                std::max(
                    0.5,
                    neighbours[3] -
                    neighbours[0]);

            // Preserve the measured reference-target difference exactly for
            // normal broad spectral structure. Replace only a narrow centre
            // outlier that is inconsistent with its log-frequency
            // neighbours; this protects against isolated FFT/resonance spikes
            // without blurring the curve before the solver even sees it.
            const bool narrowOutlier =
                std::abs(
                    center -
                    neighbourMedian) >
                std::max(
                    2.5,
                    1.5 * localSpread);

            const double correction =
                narrowOutlier
                    ? neighbourMedian
                    : center;

            return
                std::clamp(
                    correction,
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
        256u;

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
            64> strongest {};

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
