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


constexpr std::array<double, 128> kRealReference128 {
    -12.597021, -9.330196, -4.958834, -1.445903, 1.604868, 3.905503, 6.228242, 9.451621,
    11.971102, 14.052376, 16.909374, 20.202958, 24.512635, 27.466840, 28.513017, 28.326752,
    29.001506, 29.265849, 29.492485, 32.023997, 32.477503, 35.112898, 37.709044, 36.922628,
    34.793018, 35.759885, 34.053940, 36.439624, 39.152587, 37.058044, 33.622929, 32.305653,
    32.828521, 32.719758, 31.268679, 28.052464, 29.306370, 26.869465, 22.715181, 21.751714,
    22.494062, 19.577136, 18.007466, 15.103258, 16.664181, 17.804976, 17.571588, 15.177114,
    13.688834, 10.631832, 10.957511, 12.821244, 8.345593, 5.921991, 4.208101, 2.365649,
    2.783983, 3.338652, -2.680939, -5.086792, -4.926216, -4.827603, -7.507258, -4.588165,
    0.817310, 3.133373, 2.763680, 1.732253, 1.559253, 2.337775, 7.201167, 6.847906,
    4.084484, 0.197133, -4.021089, -2.121203, 3.003149, 7.748352, 9.576081, 9.302245,
    10.278177, 9.163747, 11.981377, 14.602216, 13.598721, 6.051574, 6.969161, 8.789612,
    8.955854, 7.910541, 7.277778, 9.159186, 10.766976, 10.497303, 9.317817, 10.254004,
    5.115913, 3.629459, 4.698901, 5.833775, 1.581951, -0.454205, 1.702285, 1.872233,
    1.184198, -3.393956, -2.862807, -3.311581, -5.829475, -9.479733, -9.725375, -9.772560,
    -7.484180, -8.218369, -11.736930, -16.095853, -22.856669, -27.818913, -33.508962, -34.699214,
    -45.374240, -49.623682, -50.102121, -54.093554, -51.416938, -68.476507, -57.108864, -66.880285
};

constexpr std::array<double, 128> kRealTarget128 {
    3.455719, 5.188400, 8.251926, 11.220501, 12.679842, 13.816188, 15.170279, 18.151737,
    20.069445, 20.986466, 22.781184, 22.060030, 24.512672, 27.207944, 25.661384, 20.714237,
    20.310452, 22.197585, 27.138867, 31.667320, 28.872963, 26.551487, 33.193285, 35.757132,
    31.107708, 38.008968, 35.193854, 36.080924, 39.033699, 28.099560, 24.299600, 32.279967,
    26.617580, 22.590853, 31.834819, 20.313516, 25.665943, 22.142568, 17.111635, 14.633797,
    15.116136, 12.218228, 13.214251, 13.313569, 10.165920, 13.727184, 16.467768, 13.353620,
    6.781094, 9.599715, 5.959295, 9.823085, 6.684578, 2.588973, 1.014005, 2.009104,
    0.520663, 0.673010, -1.758936, 2.141258, 0.187981, 3.644845, 4.877665, 1.696452,
    -1.426565, 2.865602, 4.098842, 2.389970, 4.386084, 2.339812, 0.332406, -0.959103,
    -0.216946, 0.624082, 3.823505, 3.512597, 3.531162, 9.372874, 2.615706, 5.640691,
    0.690330, 1.744649, -3.053994, -1.558107, -0.582412, 4.836784, 0.171629, -0.447521,
    2.851422, -3.209642, -3.421432, 1.579790, 6.233158, 1.007970, -0.420139, -3.776666,
    -5.526654, -7.283553, -7.344590, -6.678543, -2.624437, -6.528122, -8.731911, -9.381103,
    -8.906035, -10.315419, -10.834050, -14.175152, -15.849374, -15.750561, -16.946537, -20.494452,
    -16.578192, -22.775496, -28.754322, -25.518139, -27.748802, -32.496420, -36.127816, -37.275894,
    -43.914388, -43.046770, -43.903971, -48.208933, -47.985098, -52.460382, -45.804331, -49.025004
};

constexpr std::array<double, 128> kRealMatch6_128 {
    -7.156229, -5.859653, -3.885986, -1.732673, -1.004360, -0.119310, 2.345588, 8.307067,
    12.443552, 15.766979, 19.519237, 20.309633, 25.157233, 28.425257, 27.065563, 22.652803,
    22.564232, 24.276450, 29.055768, 33.633624, 31.055626, 29.345301, 36.040504, 38.316596,
    31.665186, 34.811291, 30.558562, 34.727449, 39.187764, 29.555007, 26.118514, 34.591952,
    29.382498, 25.029792, 32.591406, 19.904469, 27.531640, 23.777402, 17.450146, 16.401913,
    18.355912, 13.714088, 10.513863, 11.120593, 9.962894, 12.200085, 13.904776, 10.881126,
    6.238896, 6.723248, 5.209844, 8.292423, 4.296471, 1.680543, 0.140933, -0.809559,
    -1.079219, -2.767620, -6.352102, -6.045282, -7.415496, -4.133447, -4.779383, -4.851574,
    -4.734944, 0.779142, 2.920890, 0.633708, 0.961222, -0.189538, 2.330241, 1.562456,
    -0.207699, -2.455841, -5.724785, -3.914991, 1.131834, 8.256245, 5.911075, 7.579940,
    7.119741, 7.972871, 8.271231, 10.473411, 8.677883, 6.465974, 3.556373, 4.749327,
    6.893937, 4.285268, 4.872078, 7.925496, 10.230109, 10.709228, 7.263002, 6.980909,
    3.564355, 1.969880, 2.637359, 3.606022, 0.732206, -2.174001, -0.351910, -0.529076,
    -1.227214, -5.684313, -5.328018, -7.306358, -10.496142, -13.574474, -14.641169, -17.408034,
    -12.261788, -18.168154, -26.785075, -25.998545, -29.620025, -34.618877, -38.296171, -39.492622,
    -46.157117, -45.297692, -46.160252, -50.454310, -50.253946, -54.697738, -48.076240, -51.300538
};

ToneMatchSpectrumSnapshot makeRealCurveSnapshot(
    const std::array<double, 128>& compact,
    std::uint64_t frames) {

    ToneMatchSpectrumSnapshot s {};
    s.sampleRate = 44100.0;
    s.frameCount = frames;
    s.hasLogCurve = true;

    for (std::size_t i = 0; i < s.meanDb.size(); ++i) {
        const double x =
            static_cast<double>(i) *
            static_cast<double>(compact.size() - 1u) /
            static_cast<double>(s.meanDb.size() - 1u);

        const std::size_t i0 =
            std::min<std::size_t>(
                static_cast<std::size_t>(std::floor(x)),
                compact.size() - 1u);

        const std::size_t i1 =
            std::min<std::size_t>(
                i0 + 1u,
                compact.size() - 1u);

        const double t = x - static_cast<double>(i0);
        s.meanDb[i] =
            compact[i0] +
            (compact[i1] - compact[i0]) * t;
    }

    return s;
}

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
    const auto reference =
        makeRealCurveSnapshot(
            kRealReference128,
            1474u);

    const auto target =
        makeRealCurveSnapshot(
            kRealTarget128,
            85u);

    const auto previousBest =
        makeRealCurveSnapshot(
            kRealMatch6_128,
            85u);

    const auto profile =
        ToneMatchAnalyzer::makeProfile(
            reference,
            target);

    BF_REQUIRE(profile.valid);
    BF_REQUIRE(profile.firValid);

    const double before =
        distance(reference, target, nullptr);

    const double previousBestDistance =
        distance(reference, previousBest, nullptr);

    const double after =
        distance(reference, target, &profile);

    std::cout
        << "REAL MATCH distance: before="
        << before
        << " dB, previous-best="
        << previousBestDistance
        << " dB, candidate="
        << after
        << " dB\n";

    BF_REQUIRE(before > previousBestDistance);
    BF_REQUIRE(after < previousBestDistance);
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
    verifyMeasuredAnalyzerSeparatesLevelFromTone();

    std::cout
        << "Bass Finisher adaptive full-band MATCH quality passed\n";

    return 0;
}
