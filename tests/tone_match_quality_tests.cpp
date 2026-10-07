#include "TestSupport.h"
#include "ToneMatchAnalyzer.h"
#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace HighGainGuitarFinisher::dsp;

namespace {

constexpr std::array<double, 22> kZones {
    35.0, 50.0, 65.0, 82.0,
    105.0, 135.0, 175.0, 225.0,
    300.0, 400.0, 550.0, 750.0,
    1050.0, 1500.0, 2200.0, 3200.0,
    4200.0, 5200.0, 6500.0, 7800.0,
    9000.0, 10000.0
};

constexpr std::array<double, 22> kWeights {
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 1.0, 1.0,
    1.0, 1.0
};

std::size_t binFor(double sampleRate, double frequency) {
    return std::min<std::size_t>(
        static_cast<std::size_t>(
            std::llround(
                frequency *
                ToneMatchAnalyzer::kFftSize /
                sampleRate)),
        ToneMatchAnalyzer::kSpectrumBins - 1);
}

ToneMatchSpectrumSnapshot makeTarget(double sampleRate) {
    ToneMatchSpectrumSnapshot s {};
    s.sampleRate = sampleRate;
    s.frameCount = 8;

    for (std::size_t i = 1; i < s.meanPower.size(); ++i) {
        const double frequency =
            static_cast<double>(i) *
            sampleRate /
            ToneMatchAnalyzer::kFftSize;

        s.meanPower[i] =
            std::max(
                1.0e-18,
                1.0 /
                (1.0 +
                 std::pow(
                     frequency / 190.0,
                     1.20)));
    }

    return s;
}

double gaussianLog(double frequency, double center, double width) {
    const double x =
        std::log(
            std::max(frequency, 1.0) /
            center) /
        width;

    return std::exp(-0.5 * x * x);
}

ToneMatchSpectrumSnapshot makeReference(double sampleRate) {
    auto s = makeTarget(sampleRate);

    for (std::size_t i = 1; i < s.meanPower.size(); ++i) {
        const double frequency =
            static_cast<double>(i) *
            sampleRate /
            ToneMatchAnalyzer::kFftSize;

        const double dbShape =
            4.0 * gaussianLog(frequency, 82.0, 0.24) -
            3.2 * gaussianLog(frequency, 330.0, 0.30) +
            3.6 * gaussianLog(frequency, 1100.0, 0.27) +
            3.2 * gaussianLog(frequency, 5200.0, 0.22) -
            2.4 * gaussianLog(frequency, 8500.0, 0.20);

        s.meanPower[i] *=
            std::pow(
                10.0,
                dbShape / 10.0);
    }

    return s;
}

double sampleDb(
    const ToneMatchSpectrumSnapshot& s,
    double frequency) {

    return 10.0 *
        std::log10(
            std::max(
                s.meanPower[
                    binFor(
                        s.sampleRate,
                        frequency)],
                1.0e-24));
}

double biquadMagnitudeDb(
    const BiquadCoefficients& coefficients,
    double sampleRate,
    double frequency) {

    constexpr double kPi =
        3.141592653589793238462643383279502884;

    const double omega =
        2.0 * kPi * frequency / sampleRate;

    const double cos1 = std::cos(omega);
    const double sin1 = std::sin(omega);
    const double cos2 = std::cos(2.0 * omega);
    const double sin2 = std::sin(2.0 * omega);

    const double numeratorReal =
        coefficients.b0 +
        coefficients.b1 * cos1 +
        coefficients.b2 * cos2;

    const double numeratorImag =
        -coefficients.b1 * sin1 -
        coefficients.b2 * sin2;

    const double denominatorReal =
        1.0 +
        coefficients.a1 * cos1 +
        coefficients.a2 * cos2;

    const double denominatorImag =
        -coefficients.a1 * sin1 -
        coefficients.a2 * sin2;

    const double numeratorPower =
        numeratorReal * numeratorReal +
        numeratorImag * numeratorImag;

    const double denominatorPower =
        denominatorReal * denominatorReal +
        denominatorImag * denominatorImag;

    const double magnitude =
        std::sqrt(
            std::max(
                numeratorPower /
                std::max(
                    denominatorPower,
                    1.0e-30),
                1.0e-30));

    return 20.0 *
        std::log10(magnitude);
}

double responseDb(
    const ToneMatchProfile& p,
    double sampleRate,
    double frequency) {

    double result =
        biquadMagnitudeDb(
            makeLowShelf(
                sampleRate,
                p.lowShelfFrequencyHz,
                p.lowShelfGainDb),
            sampleRate,
            frequency);

    for (const auto& peak : p.peaks) {
        result +=
            biquadMagnitudeDb(
                makePeaking(
                    sampleRate,
                    peak.frequencyHz,
                    peak.q,
                    peak.gainDb),
                sampleRate,
                frequency);
    }

    result +=
        biquadMagnitudeDb(
            makeHighShelf(
                sampleRate,
                p.highShelfFrequencyHz,
                p.highShelfGainDb),
            sampleRate,
            frequency);

    return result;
}

double distance(
    const ToneMatchSpectrumSnapshot& reference,
    const ToneMatchSpectrumSnapshot& target,
    const ToneMatchProfile* profile) {

    double offset = 0.0;
    double weightSum = 0.0;

    for (std::size_t i = 0; i < kZones.size(); ++i) {
        offset +=
            kWeights[i] *
            (sampleDb(reference, kZones[i]) -
             sampleDb(target, kZones[i]));
        weightSum += kWeights[i];
    }

    offset /= weightSum;

    double squared = 0.0;

    for (std::size_t i = 0; i < kZones.size(); ++i) {
        double residual =
            (sampleDb(reference, kZones[i]) -
             sampleDb(target, kZones[i])) -
            offset;

        if (profile) {
            residual -=
                responseDb(
                    *profile,
                    target.sampleRate,
                    kZones[i]);
        }

        squared +=
            kWeights[i] *
            residual *
            residual;
    }

    return
        std::sqrt(
            squared /
            weightSum);
}

void verifyDistanceImproves() {
    const auto target =
        makeTarget(48000.0);

    const auto reference =
        makeReference(48000.0);

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);

    const double before =
        distance(
            reference,
            target,
            nullptr);

    const double after =
        distance(
            reference,
            target,
            &profile);

    std::cout
        << "MATCH distance: before="
        << before
        << " dB, after="
        << after
        << " dB\n";

    // Improvement alone is not enough for a matcher. At 100% the protected
    // solver must land close to the synthetic reference curve.
    BF_REQUIRE(after < 0.25);
    BF_REQUIRE(after < before * 0.15);
}

void verifyNarrowSpikeIsRejected() {
    auto target =
        makeTarget(48000.0);

    auto reference =
        target;

    reference.meanPower[
        binFor(
            48000.0,
            1000.0)] *=
        100.0;

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);

    double maximumBoost = 0.0;

    for (const auto& peak : profile.peaks) {
        maximumBoost =
            std::max(
                maximumBoost,
                peak.gainDb);
    }

    BF_REQUIRE(maximumBoost < 3.0);
}

void verifySubBoostProtection() {
    auto target =
        makeTarget(48000.0);

    auto reference =
        target;

    for (std::size_t i = 1; i < reference.meanPower.size(); ++i) {
        const double frequency =
            static_cast<double>(i) *
            48000.0 /
            ToneMatchAnalyzer::kFftSize;

        if (frequency < 60.0)
            reference.meanPower[i] *=
                10.0;
    }

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);
    BF_REQUIRE(profile.lowShelfGainDb <= 1.500001);
    BF_REQUIRE(profile.peaks[0].gainDb <= 1.500001);
    BF_REQUIRE(profile.peaks[1].gainDb <= 2.000001);
}

}


void verifyAbsoluteLevelIsNotTone() {
    auto target =
        makeTarget(48000.0);

    auto reference =
        target;

    // Same spectral shape, reference 48 dB louder. MATCH must not turn that
    // absolute level difference into a broadband EQ curve.
    const double powerScale =
        std::pow(
            10.0,
            48.0 / 10.0);

    for (double& power :
         reference.meanPower) {
        power *= powerScale;
    }

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);

    double maximumMagnitude = 0.0;

    maximumMagnitude =
        std::max(
            maximumMagnitude,
            std::abs(
                profile.lowShelfGainDb));

    for (const auto& peak :
         profile.peaks) {

        maximumMagnitude =
            std::max(
                maximumMagnitude,
                std::abs(
                    peak.gainDb));
    }

    maximumMagnitude =
        std::max(
            maximumMagnitude,
            std::abs(
                profile.highShelfGainDb));

    BF_REQUIRE(
        maximumMagnitude <
        0.15);
}

void verifyAnalyzerHasNoUpperDbCeiling() {
    const auto target =
        makeTarget(48000.0);

    const auto reference =
        makeReference(48000.0);

    const auto baseline =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(baseline.valid);

    auto loudTarget =
        target;

    auto loudReference =
        reference;

    // Push both analysed spectra far above the historic +24 dB failure region.
    // A common level shift must not alter the resulting tonal solution.
    constexpr double hugePowerScale =
        1.0e8;

    for (double& power :
         loudTarget.meanPower) {
        power *= hugePowerScale;
    }

    for (double& power :
         loudReference.meanPower) {
        power *= hugePowerScale;
    }

    const auto loud =
        ToneMatchAnalyzer::makeProfile(
            loudReference,
            loudTarget);

    BF_REQUIRE(loud.valid);

    BF_REQUIRE(
        std::abs(
            loud.lowShelfGainDb -
            baseline.lowShelfGainDb) <
        1.0e-8);

    BF_REQUIRE(
        std::abs(
            loud.highShelfGainDb -
            baseline.highShelfGainDb) <
        1.0e-8);

    for (std::size_t i = 0;
         i < loud.peaks.size();
         ++i) {

        BF_REQUIRE(
            std::abs(
                loud.peaks[i].gainDb -
                baseline.peaks[i].gainDb) <
            1.0e-8);
    }
}

int main() {
    verifyDistanceImproves();
    verifyNarrowSpikeIsRejected();
    verifySubBoostProtection();
    verifyAbsoluteLevelIsNotTone();
    verifyAnalyzerHasNoUpperDbCeiling();

    std::cout
        << "Bass Finisher adaptive full-band MATCH quality passed\n";

    return 0;
}
