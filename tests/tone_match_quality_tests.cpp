#include "TestSupport.h"
#include "ToneMatchAnalyzer.h"
#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>

using namespace HighGainGuitarFinisher::dsp;

namespace {


constexpr std::array<double, 512> kRealDifference512 {
    -16.052740, -15.492336, -15.083499, -14.770810, -14.523344, -14.322330, -14.155655, -13.609918,
    -13.221370, -12.996796, -12.850309, -12.747165, -12.670592, -12.611488, -12.027651, -11.513722,
    -11.106360, -10.774197, -10.497432, -10.262842, -9.949437, -9.621339, -9.358767, -9.143522,
    -8.963671, -8.811034, -8.741817, -8.718026, -8.702018, -8.690512, -8.681842, -8.596625,
    -8.166024, -7.807880, -7.504376, -7.243311, -7.015992, -6.630750, -6.343471, -6.121206,
    -5.943926, -5.638635, -4.302554, -3.149218, -2.108995, -1.139473, -0.463475, -0.194888,
    -0.031273, 0.078916, 0.157475, 0.210550, 0.249673, 0.279707, 0.303488, 1.196332,
    2.342515, 3.881990, 6.239640, 6.739464, 7.344625, 8.100669, 8.837021, 8.768698,
    8.710199, 8.659546, 8.307406, 7.782736, 7.306430, 6.713349, 5.107331, 3.910506,
    2.950802, 1.546313, 0.683564, 0.167533, 0.190097, 0.561249, 0.937861, 1.594620,
    2.810474, 4.491248, 6.377629, 7.525929, 8.545369, 8.577708, 8.127992, 7.851440,
    5.476968, 3.627369, 2.398548, 1.823602, 1.232918, 1.108823, 1.884393, 3.412751,
    3.697631, 3.675898, 1.951280, -0.328335, -1.577096, -2.714994, -3.753313, -3.540946,
    -2.278119, -0.424888, 1.201168, 2.496656, 0.855605, 0.076507, -0.493632, -0.769490,
    -0.326673, 0.346972, 1.903947, 5.672433, 7.753354, 9.512567, 9.240913, 8.156225,
    8.876119, 9.507183, 7.339596, 4.455783, 1.046652, -0.347570, -0.742126, 0.447445,
    3.802184, 6.988770, 10.822270, 12.118037, 11.640631, 9.701346, 3.771722, 1.088433,
    -0.629843, -0.550527, 0.467406, 3.869354, 6.770941, 7.941768, 6.864319, 5.898766,
    4.448414, 3.498281, 3.305663, 4.308195, 6.464161, 4.476481, 3.342951, 3.988631,
    5.923218, 5.567092, 3.915617, 4.420263, 5.544486, 7.252398, 8.356220, 9.324305,
    8.534245, 7.310474, 5.846942, 5.963154, 7.610205, 7.350736, 5.934146, 5.719312,
    5.543701, 4.787260, 2.764981, 1.282213, 0.866107, 1.744477, 4.615435, 7.783412,
    8.335356, 6.477648, 7.001214, 5.327173, 2.051574, 4.016023, 4.996617, 2.465233,
    1.022712, 1.011993, 2.072178, 3.664601, 4.567452, 1.996006, 0.431079, 1.766412,
    3.601113, 6.835176, 7.377263, 1.764908, -1.359863, 1.189560, 0.189795, 1.957737,
    5.422744, 5.223475, 3.979657, 5.642335, 9.426197, 3.396722, 1.449892, 4.400563,
    1.694543, 1.143564, 3.409641, 3.460598, 1.984167, 3.170831, 3.814511, 2.685349,
    2.529198, 2.828626, 4.154758, 4.024335, 4.080682, 0.530253, -0.050298, 1.317999,
    2.292233, 2.263215, 2.263541, 0.705407, -0.174543, 3.646858, 0.814712, -1.714541,
    -2.283239, -1.751739, 0.490314, -1.620578, -0.779439, -5.796602, -9.432480, -4.571210,
    -7.555368, -6.133870, -3.690502, -4.430076, -9.182347, -8.547321, -8.377520, -0.842457,
    -7.763161, -11.353418, -13.573776, -6.115000, -2.370571, -9.806926, -2.591873, 1.291709,
    -1.571517, 2.872838, 1.643941, -5.319058, 5.578857, 3.751901, -2.755223, -0.840412,
    2.507751, 3.864120, -5.436004, -3.986031, 3.498733, 4.184580, -4.125849, -0.155894,
    -6.524971, 3.288069, -6.797545, 3.370825, -2.528679, -6.978695, 4.096750, 8.643106,
    7.697822, 2.339427, 9.269852, 4.828271, 8.112493, 7.404544, 7.998882, 7.363873,
    9.206684, 7.084242, 3.113263, 6.707031, 5.514750, 3.060980, -1.753878, 3.696141,
    2.213457, -10.570910, -6.926256, -11.680789, -2.728489, -16.498791, -2.418649, 2.474052,
    -2.303353, -6.779411, 1.081258, 7.777820, -4.246232, 2.446904, -2.524934, 5.461347,
    6.277711, 0.863004, 8.100072, 3.268669, 7.227216, -4.197237, 4.876094, 11.152216,
    5.590563, 2.094563, 10.516218, 12.630320, 10.624810, 4.496776, 7.696215, 8.979710,
    14.231506, 17.189166, 14.890578, 17.327127, 17.007400, 18.240267, 16.075080, 17.610581,
    13.455601, 6.497525, 14.304070, 8.383399, 10.112174, 4.956066, 1.171353, 6.687949,
    0.223234, -2.141004, 6.781334, 7.295615, 7.241842, -0.401460, 9.881935, -1.816609,
    8.382643, 7.961282, 5.687465, 10.982942, 12.803913, 9.147093, 11.021223, 11.987991,
    9.990981, 12.616079, 10.553857, 11.707588, 8.984702, 9.147244, 7.539300, 7.807312,
    9.170943, 12.537310, 2.936713, 12.156362, 12.826529, 9.334064, 8.831529, 12.173173,
    13.784665, 6.593172, 9.772065, 9.617355, 11.009767, 8.805395, 15.435683, 9.679660,
    8.980556, 8.187292, 10.101306, 12.123073, 10.244726, 12.302898, 11.161096, 10.309566,
    8.640565, 6.998334, 11.439684, 13.356775, 14.037022, 9.585828, 12.750376, 12.047271,
    8.226927, 6.824103, 3.696107, 5.104928, 4.529280, 5.279428, 4.366837, 8.791312,
    8.785194, 10.623404, 10.053840, 10.982787, 10.461530, 10.776065, 10.463811, 12.286896,
    10.490304, 12.025267, 10.450268, 9.661915, 7.245378, 4.294484, 7.814292, 5.955449,
    2.971207, 6.934281, 7.555114, 8.380871, 10.638717, 10.109725, 9.246554, 12.311648,
    11.502855, 11.158484, 10.428638, 9.687068, 7.515588, 6.234045, 6.217672, 6.310148,
    7.611257, 6.752375, 7.783996, 6.843470, 7.176340, 6.329084, 8.541443, 12.046722,
    9.741794, 14.148009, 9.943498, 8.627830, 11.814205, 12.610431, 14.967380, 14.354414,
    13.386449, 14.229390, 16.284716, 17.342101, 13.720679, 11.186222, 9.377722, 9.439916,
    7.098438, 5.785832, 4.199637, 5.135243, 4.966188, 5.204340, 6.150148, 4.222051,
    4.684416, 3.670342, 3.693566, 2.328682, 2.118993, 0.287408, 2.196939, 2.665164,
    1.257213, 2.453673, 3.444929, -2.431554, -7.327439, -5.242149, -4.625776, -6.899119,
    -6.233919, -7.445000, -7.826263, -5.980099, -4.031774, -3.437115, -5.452354, -5.929726,
    -4.913009, -6.015907, -4.473799, -3.352368, -2.614008, -3.355447, -4.496350, -16.587353,
    -12.633690, -11.488396, -9.908313, -11.338312, -11.502837, -14.817122, -13.815794, -17.855281
};

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

    if (s.hasLogCurve) {
        const double maximumFrequency =
            std::min(
                ToneMatchAnalyzer::kCurveMaximumHz,
                s.sampleRate * 0.45);

        const double f =
            std::clamp(
                frequency,
                ToneMatchAnalyzer::kCurveMinimumHz,
                maximumFrequency);

        const double position =
            (std::log(f) -
             std::log(
                 ToneMatchAnalyzer::kCurveMinimumHz)) /
            (std::log(maximumFrequency) -
             std::log(
                 ToneMatchAnalyzer::kCurveMinimumHz));

        const double exactIndex =
            position *
            static_cast<double>(
                ToneMatchAnalyzer::kCurveBins - 1u);

        const std::size_t index0 =
            std::min(
                static_cast<std::size_t>(
                    std::floor(exactIndex)),
                ToneMatchAnalyzer::kCurveBins - 1u);

        const std::size_t index1 =
            std::min(
                index0 + 1u,
                ToneMatchAnalyzer::kCurveBins - 1u);

        const double fraction =
            exactIndex -
            static_cast<double>(index0);

        return
            s.meanDb[index0] +
            (s.meanDb[index1] -
             s.meanDb[index0]) *
                fraction;
    }

    const double exactBin =
        std::clamp(
            frequency *
                static_cast<double>(
                    ToneMatchAnalyzer::kFftSize) /
                s.sampleRate,
            0.0,
            static_cast<double>(
                ToneMatchAnalyzer::kSpectrumBins - 1u));

    const std::size_t bin0 =
        std::min(
            static_cast<std::size_t>(
                std::floor(exactBin)),
            ToneMatchAnalyzer::kSpectrumBins - 1u);

    const std::size_t bin1 =
        std::min(
            bin0 + 1u,
            ToneMatchAnalyzer::kSpectrumBins - 1u);

    const double fraction =
        exactBin -
        static_cast<double>(bin0);

    const double power =
        s.meanPower[bin0] +
        (s.meanPower[bin1] -
         s.meanPower[bin0]) *
            fraction;

    return 10.0 *
        std::log10(
            std::max(
                power,
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

    if (p.firValid) {
        constexpr double pi =
            3.14159265358979323846;

        double real = 0.0;
        double imag = 0.0;

        const double omega =
            2.0 * pi *
            frequency /
            sampleRate;

        for (std::size_t i = 0;
             i < p.firTaps.size();
             ++i) {

            const double phase =
                -omega *
                static_cast<double>(i);

            real +=
                p.firTaps[i] *
                std::cos(phase);

            imag +=
                p.firTaps[i] *
                std::sin(phase);
        }

        return 20.0 *
            std::log10(
                std::max(
                    std::sqrt(
                        real * real +
                        imag * imag),
                    1.0e-12));
    }

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

    double offset = 0.0;
    double weightSum = 0.0;

    for (std::size_t i = 0;
         i < kZones.size();
         ++i) {
        offset +=
            kWeights[i] *
            (sampleDb(reference, kZones[i]) -
             sampleDb(target, kZones[i]));
        weightSum += kWeights[i];
    }

    offset /= weightSum;

    std::cout << "MATCH residual zones:\n";

    for (std::size_t i = 0;
         i < kZones.size();
         ++i) {

        const double desired =
            (sampleDb(reference, kZones[i]) -
             sampleDb(target, kZones[i])) -
            offset;

        const double actual =
            responseDb(
                profile,
                target.sampleRate,
                kZones[i]);

        const double residual =
            desired - actual;

        std::cout
            << "  " << kZones[i]
            << " Hz: desired=" << desired
            << " dB actual=" << actual
            << " dB residual=" << residual
            << " dB\n";
    }

    // Improvement alone is not enough for a matcher. At 100% the protected
    // solver must land close to the synthetic reference curve.
    BF_REQUIRE(profile.firValid);
    BF_REQUIRE(after < 0.15);
    BF_REQUIRE(after < before * 0.08);
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

    BF_REQUIRE(profile.firValid);
    BF_REQUIRE(
        responseDb(
            profile,
            48000.0,
            1000.0) <
        3.0);
}

void verifyHighRangeCorrectionIsNotArtificiallyCapped() {
    auto target =
        makeTarget(48000.0);

    auto reference =
        target;

    for (std::size_t i = 1;
         i < reference.meanPower.size();
         ++i) {

        const double frequency =
            static_cast<double>(i) *
            48000.0 /
            ToneMatchAnalyzer::kFftSize;

        const double db =
            18.0 *
            gaussianLog(
                frequency,
                900.0,
                0.34) -
            17.0 *
            gaussianLog(
                frequency,
                4200.0,
                0.30);

        reference.meanPower[i] *=
            std::pow(
                10.0,
                db / 10.0);
    }

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);
    BF_REQUIRE(profile.firValid);

    BF_REQUIRE(
        responseDb(
            profile,
            48000.0,
            900.0) >
        14.0);

    BF_REQUIRE(
        responseDb(
            profile,
            48000.0,
            4200.0) <
        -13.0);
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
    BF_REQUIRE(profile.firValid);
    BF_REQUIRE(
        responseDb(
            profile,
            48000.0,
            40.0) <=
        24.000001);
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

    BF_REQUIRE(profile.firValid);

    double maximumMagnitude = 0.0;

    for (const double frequency :
         kZones) {

        maximumMagnitude =
            std::max(
                maximumMagnitude,
                std::abs(
                    responseDb(
                        profile,
                        48000.0,
                        frequency)));
    }

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

    BF_REQUIRE(loud.firValid);
    BF_REQUIRE(baseline.firValid);

    for (std::size_t i = 0;
         i < loud.firTaps.size();
         ++i) {

        BF_REQUIRE(
            std::abs(
                loud.firTaps[i] -
                baseline.firTaps[i]) <
            1.0e-8);
    }
}



void verifyRealProgramMaterialBeatsPreviousBest() {
    ToneMatchSpectrumSnapshot reference {};
    ToneMatchSpectrumSnapshot target {};

    reference.sampleRate = 44100.0;
    target.sampleRate = 44100.0;
    reference.frameCount = 1474u;
    target.frameCount = 85u;
    reference.hasLogCurve = true;
    target.hasLogCurve = true;

    reference.meanDb =
        kRealDifference512;

    target.meanDb.fill(0.0);

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);
    BF_REQUIRE(profile.firValid);

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

    constexpr double kPreviousBestRealDistance =
        3.6845373110158626;

    std::cout
        << "REAL MATCH distance: before="
        << before
        << " dB, previous-best="
        << kPreviousBestRealDistance
        << " dB, candidate="
        << after
        << " dB\n";

    BF_REQUIRE(
        before >
        kPreviousBestRealDistance);

    BF_REQUIRE(
        after <
        kPreviousBestRealDistance);
}


double shapedDb(
    double frequency,
    double lowShelfDb,
    double lowShelfHz,
    double highShelfDb,
    double highShelfHz,
    const std::array<std::array<double, 3>, 4>& peaks) {

    const double low =
        lowShelfDb /
        (1.0 +
         std::pow(
             frequency /
             std::max(lowShelfHz, 1.0),
             3.0));

    const double high =
        highShelfDb *
        (1.0 -
         1.0 /
         (1.0 +
          std::pow(
              frequency /
              std::max(highShelfHz, 1.0),
              3.0)));

    double result =
        low + high;

    for (const auto& p : peaks) {
        result +=
            p[1] *
            gaussianLog(
                frequency,
                p[0],
                p[2]);
    }

    return result;
}

ToneMatchSpectrumSnapshot makeGeneralizationReference(
    double sampleRate,
    double lowShelfDb,
    double lowShelfHz,
    double highShelfDb,
    double highShelfHz,
    const std::array<std::array<double, 3>, 4>& peaks) {

    auto s =
        makeTarget(sampleRate);

    for (std::size_t i = 1;
         i < s.meanPower.size();
         ++i) {

        const double frequency =
            static_cast<double>(i) *
            sampleRate /
            ToneMatchAnalyzer::kFftSize;

        const double db =
            shapedDb(
                frequency,
                lowShelfDb,
                lowShelfHz,
                highShelfDb,
                highShelfHz,
                peaks);

        s.meanPower[i] *=
            std::pow(
                10.0,
                db / 10.0);
    }

    return s;
}

void verifyGeneralizationSuite() {
    struct Case {
        double lowShelfDb;
        double lowShelfHz;
        double highShelfDb;
        double highShelfHz;
        std::array<std::array<double, 3>, 4> peaks;
    };

    constexpr std::array<Case, 12> cases {{
        { 8.0, 70.0, -6.0, 6500.0, {{{120.0, 5.0, 0.22},{420.0,-7.0,0.20},{1800.0,6.0,0.18},{5200.0,-5.0,0.14}}}},
        {-10.0,55.0, 7.0, 7200.0, {{{90.0,-6.0,0.18},{300.0,8.0,0.24},{1300.0,-9.0,0.16},{4100.0,7.0,0.18}}}},
        { 4.0, 95.0, 9.0, 5000.0, {{{160.0,-5.0,0.25},{650.0,10.0,0.13},{2400.0,-8.0,0.17},{8200.0,6.0,0.15}}}},
        {-7.0,80.0,-8.0, 8000.0, {{{70.0, 8.0,0.16},{520.0,-10.0,0.15},{1700.0,9.0,0.12},{6200.0,-7.0,0.20}}}},
        {12.0,60.0, 5.0, 9000.0, {{{110.0,-8.0,0.18},{900.0,11.0,0.10},{3100.0,-10.0,0.11},{7000.0,8.0,0.13}}}},
        {-12.0,90.0,10.0, 6000.0, {{{140.0,7.0,0.14},{760.0,-11.0,0.09},{2200.0,10.0,0.14},{4800.0,-9.0,0.16}}}},
        { 0.0,80.0, 0.0, 7000.0, {{{65.0,14.0,0.10},{260.0,-12.0,0.08},{1450.0,13.0,0.09},{3600.0,-11.0,0.10}}}},
        { 6.0,50.0,-11.0,5500.0, {{{100.0,5.0,0.28},{430.0,9.0,0.20},{1200.0,-12.0,0.12},{9300.0,8.0,0.11}}}},
        {-5.0,120.0,12.0,4500.0, {{{180.0,-7.0,0.17},{840.0,12.0,0.11},{2600.0,-10.0,0.13},{6100.0,10.0,0.10}}}},
        {10.0,75.0,-10.0,8500.0, {{{135.0,-9.0,0.12},{570.0,13.0,0.10},{1900.0,-12.0,0.08},{7500.0,9.0,0.12}}}},
        {-8.0,65.0, 6.0,5200.0, {{{85.0,10.0,0.14},{350.0,-9.0,0.18},{2800.0,11.0,0.10},{9800.0,-8.0,0.10}}}},
        {14.0,55.0,-12.0,6800.0, {{{115.0,-10.0,0.12},{720.0,14.0,0.09},{2100.0,-13.0,0.10},{5600.0,12.0,0.11}}}}
    }};

    double sumBefore = 0.0;
    double sumAfter = 0.0;
    double worstAfter = 0.0;
    double worstRatio = 0.0;

    for (std::size_t i = 0;
         i < cases.size();
         ++i) {

        const auto target =
            makeTarget(48000.0);

        const auto reference =
            makeGeneralizationReference(
                48000.0,
                cases[i].lowShelfDb,
                cases[i].lowShelfHz,
                cases[i].highShelfDb,
                cases[i].highShelfHz,
                cases[i].peaks);

        const auto profile =
            ToneMatchAnalyzer::makeProfile(
                reference,
                target);

        BF_REQUIRE(profile.valid);
        BF_REQUIRE(profile.firValid);

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

        const double ratio =
            after /
            std::max(
                before,
                1.0e-9);

        sumBefore += before;
        sumAfter += after;
        worstAfter =
            std::max(
                worstAfter,
                after);
        worstRatio =
            std::max(
                worstRatio,
                ratio);

        std::cout
            << "GENERAL MATCH case "
            << i
            << ": before="
            << before
            << " dB, after="
            << after
            << " dB, ratio="
            << ratio
            << "\n";

        BF_REQUIRE(after < before);
    }

    const double meanBefore =
        sumBefore /
        static_cast<double>(
            cases.size());

    const double meanAfter =
        sumAfter /
        static_cast<double>(
            cases.size());

    std::cout
        << "GENERAL MATCH summary: mean-before="
        << meanBefore
        << " dB, mean-after="
        << meanAfter
        << " dB, worst-after="
        << worstAfter
        << " dB, worst-ratio="
        << worstRatio
        << "\n";

    BF_REQUIRE(meanAfter < meanBefore * 0.20);
    BF_REQUIRE(worstRatio < 0.35);
    BF_REQUIRE(worstAfter < 1.75);
}


void verifyFirTracksMeasuredDifferenceCurve() {
    ToneMatchSpectrumSnapshot reference {};
    ToneMatchSpectrumSnapshot target {};

    reference.sampleRate = 44100.0;
    target.sampleRate = 44100.0;
    reference.frameCount = 1474u;
    target.frameCount = 85u;
    reference.hasLogCurve = true;
    target.hasLogCurve = true;

    reference.meanDb =
        kRealDifference512;

    target.meanDb.fill(0.0);

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);
    BF_REQUIRE(profile.firValid);

    constexpr std::size_t kPoints = 512u;

    long double squared = 0.0L;
    double maximumAbsoluteResidual = 0.0;
    double maximumResidualFrequencyHz = 0.0;
    std::size_t severeResidualCount = 0u;

    // Reproduce the production design target exactly: same 256-point
    // broadband offset, same log-frequency interpolation, same spike guard.
    constexpr std::size_t kOffsetPoints = 256u;
    long double offsetSum = 0.0L;
    std::size_t offsetCount = 0u;

    const auto sampleCurve =
        [&](const ToneMatchSpectrumSnapshot& s,
            double frequency) {

            const double maximumFrequency =
                std::min(
                    ToneMatchAnalyzer::kCurveMaximumHz,
                    s.sampleRate * 0.45);

            const double f =
                std::clamp(
                    frequency,
                    ToneMatchAnalyzer::kCurveMinimumHz,
                    maximumFrequency);

            const double position =
                (std::log(f) -
                 std::log(
                     ToneMatchAnalyzer::kCurveMinimumHz)) /
                (std::log(maximumFrequency) -
                 std::log(
                     ToneMatchAnalyzer::kCurveMinimumHz));

            const double exactIndex =
                position *
                static_cast<double>(
                    ToneMatchAnalyzer::kCurveBins - 1u);

            const std::size_t i0 =
                std::min(
                    static_cast<std::size_t>(
                        std::floor(exactIndex)),
                    ToneMatchAnalyzer::kCurveBins - 1u);

            const std::size_t i1 =
                std::min(
                    i0 + 1u,
                    ToneMatchAnalyzer::kCurveBins - 1u);

            const double t =
                exactIndex -
                static_cast<double>(i0);

            return
                s.meanDb[i0] +
                (s.meanDb[i1] -
                 s.meanDb[i0]) * t;
        };

    for (std::size_t i = 0;
         i < kOffsetPoints;
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                kOffsetPoints - 1u);

        const double frequency =
            std::exp(
                std::log(
                    ToneMatchAnalyzer::kCurveMinimumHz) +
                (std::log(
                     std::min(
                         ToneMatchAnalyzer::kCurveMaximumHz,
                         reference.sampleRate * 0.45)) -
                 std::log(
                     ToneMatchAnalyzer::kCurveMinimumHz)) *
                    position);

        const double d =
            sampleCurve(reference, frequency) -
            sampleCurve(target, frequency);

        if (std::isfinite(d)) {
            offsetSum += d;
            ++offsetCount;
        }
    }

    const double levelOffsetDb =
        offsetCount > 0u
            ? static_cast<double>(
                  offsetSum /
                  static_cast<long double>(
                      offsetCount))
            : 0.0;

    const double logMinimum =
        std::log(
            ToneMatchAnalyzer::kCurveMinimumHz);

    const double logMaximum =
        std::log(
            std::min(
                ToneMatchAnalyzer::kCurveMaximumHz,
                reference.sampleRate * 0.45));

    for (std::size_t i = 0;
         i < kPoints;
         ++i) {

        const double position =
            static_cast<double>(i) /
            static_cast<double>(
                kPoints - 1u);

        const double frequency =
            std::exp(
                logMinimum +
                (logMaximum -
                 logMinimum) *
                    position);

        constexpr double kMinimumMatchHz = 30.0;
        constexpr double kMaximumMatchHz = 12000.0;
        const double maximumMatchHz =
            std::min(
                kMaximumMatchHz,
                reference.sampleRate * 0.45);

        const auto differenceAt =
            [&](double f) {
                return
                    sampleCurve(reference, f) -
                    sampleCurve(target, f) -
                    levelOffsetDb;
            };

        double desired = 0.0;

        if (frequency >= kMinimumMatchHz &&
            frequency <= maximumMatchHz) {

            const double center =
                differenceAt(frequency);

            const double lower =
                differenceAt(
                    std::max(
                        kMinimumMatchHz,
                        frequency / 1.025));

            const double upper =
                differenceAt(
                    std::min(
                        maximumMatchHz,
                        frequency * 1.025));

            const double neighbourMean =
                0.5 * (lower + upper);

            const double localSpread =
                std::abs(lower - upper);

            const bool isolatedSpike =
                std::abs(
                    center -
                    neighbourMean) >
                std::max(
                    3.0,
                    2.5 * localSpread);

            desired =
                std::clamp(
                    isolatedSpike
                        ? neighbourMean
                        : center,
                    -24.0,
                    24.0);

        } else if (frequency < kMinimumMatchHz) {

            desired =
                std::clamp(
                    differenceAt(kMinimumMatchHz),
                    -24.0,
                    24.0);

        } else {

            const double nyquist =
                reference.sampleRate * 0.5;

            const double edge =
                std::clamp(
                    differenceAt(maximumMatchHz),
                    -24.0,
                    24.0);

            const double t =
                std::clamp(
                    (frequency - maximumMatchHz) /
                    (nyquist - maximumMatchHz),
                    0.0,
                    1.0);

            desired =
                edge * (1.0 - t);
        }

        const double actual =
            responseDb(
                profile,
                reference.sampleRate,
                frequency);

        const double residual =
            desired - actual;

        squared +=
            static_cast<long double>(
                residual * residual);

        const double absoluteResidual =
            std::abs(residual);

        if (absoluteResidual >
            maximumAbsoluteResidual) {

            maximumAbsoluteResidual =
                absoluteResidual;

            maximumResidualFrequencyHz =
                frequency;
        }

        if (absoluteResidual > 4.0)
            ++severeResidualCount;
    }

    const double rmsResidual =
        std::sqrt(
            static_cast<double>(
                squared /
                static_cast<long double>(
                    kPoints)));

    std::cout
        << "FIR CURVE residual: rms="
        << rmsResidual
        << " dB, max="
        << maximumAbsoluteResidual
        << " dB @ "
        << maximumResidualFrequencyHz
        << " Hz, severe-bins="
        << severeResidualCount
        << "/"
        << kPoints
        << "\n";

    BF_REQUIRE(rmsResidual < 1.0);
    BF_REQUIRE(
        severeResidualCount <=
        (kPoints / 100u + 1u));
}

void verifyMeasuredAnalyzerSeparatesLevelFromTone() {
    auto quiet =
        std::make_unique<ToneMatchAnalyzer>();

    auto loud =
        std::make_unique<ToneMatchAnalyzer>();

    quiet->prepare(48000.0);
    loud->prepare(48000.0);

    constexpr std::size_t sampleCount =
        ToneMatchAnalyzer::kAnalysisFftSize * 6u;

    std::array<double, 2048> q {};
    std::array<double, 2048> l {};

    std::size_t produced = 0u;

    while (produced < sampleCount) {
        const std::size_t count =
            std::min<std::size_t>(
                q.size(),
                sampleCount - produced);

        for (std::size_t i = 0;
             i < count;
             ++i) {

            const double t =
                static_cast<double>(
                    produced + i) /
                48000.0;

            const double x =
                0.45 * std::sin(
                    2.0 * 3.14159265358979323846 *
                    73.0 * t) +
                0.25 * std::sin(
                    2.0 * 3.14159265358979323846 *
                    293.0 * t) +
                0.15 * std::sin(
                    2.0 * 3.14159265358979323846 *
                    1171.0 * t);

            q[i] = 0.1 * x;
            l[i] = x;
        }

        quiet->pushStereo(
            q.data(),
            q.data(),
            count);

        loud->pushStereo(
            l.data(),
            l.data(),
            count);

        produced += count;
    }

    const auto quietSnapshot =
        quiet->snapshot();

    const auto loudSnapshot =
        loud->snapshot();

    BF_REQUIRE(quietSnapshot.hasLogCurve);
    BF_REQUIRE(loudSnapshot.hasLogCurve);

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            loudSnapshot,
            quietSnapshot);

    BF_REQUIRE(profile.valid);

    BF_REQUIRE(profile.firValid);

    double maximumMagnitude = 0.0;

    for (const double frequency :
         kZones) {

        maximumMagnitude =
            std::max(
                maximumMagnitude,
                std::abs(
                    responseDb(
                        profile,
                        48000.0,
                        frequency)));
    }

    BF_REQUIRE(maximumMagnitude < 0.25);
}

int main() {
    verifyDistanceImproves();
    verifyNarrowSpikeIsRejected();
    verifyHighRangeCorrectionIsNotArtificiallyCapped();
    verifySubBoostProtection();
    verifyAbsoluteLevelIsNotTone();
    verifyAnalyzerHasNoUpperDbCeiling();
    verifyRealProgramMaterialBeatsPreviousBest();
    verifyGeneralizationSuite();
    verifyFirTracksMeasuredDifferenceCurve();
    verifyMeasuredAnalyzerSeparatesLevelFromTone();

    std::cout
        << "Bass Finisher adaptive full-band MATCH quality passed\n";

    return 0;
}
