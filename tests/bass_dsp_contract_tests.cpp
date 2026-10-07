#include "TestSupport.h"
#include "MetalFinisherDSP.h"
#include "LowCutMapping.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

using HighGainGuitarFinisher::dsp::MetalFinisherDSP;
using HighGainGuitarFinisher::dsp::lowCutNormalizedFromFrequency;
using HighGainGuitarFinisher::dsp::lowCutFrequencyFromNormalized;

namespace {
constexpr double kFs = 48000.0;
constexpr double kPi = 3.141592653589793238462643383279502884;

double steadyRms(double frequency, double lowCut, double mass) {
    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setFinish(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.setLowCut(lowCut);
    dsp.setMass(mass);
    dsp.reset();

    const int total = static_cast<int>(kFs * 2.0);
    const int start = static_cast<int>(kFs * 1.0);
    long double power = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        const double x = 0.1 * std::sin(2.0 * kPi * frequency * static_cast<double>(i) / kFs);
        double l = x, r = x;
        dsp.processFrame(l, r);
        BF_REQUIRE(std::isfinite(l) && std::isfinite(r));
        if (i >= start) {
            power += 0.5L * (l*l + r*r);
            ++count;
        }
    }
    return std::sqrt(static_cast<double>(power / static_cast<long double>(count)));
}

double dbRatio(double a, double b) {
    BF_REQUIRE(a > 0.0 && b > 0.0);
    return 20.0 * std::log10(a / b);
}

double massDelta(double frequency, double lowCut) {
    return dbRatio(steadyRms(frequency, lowCut, 1.0),
                   steadyRms(frequency, lowCut, 0.0));
}

void verifyNeutralPathIsExact() {
    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setFinish(0.0);
    dsp.setMass(0.0);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    for (int i = 0; i < 20000; ++i) {
        const double l0 = 0.31 * std::sin(0.013 * i);
        const double r0 = 0.27 * std::cos(0.017 * i);
        double l = l0, r = r0;
        dsp.processFrame(l, r);
        BF_REQUIRE(l == l0);
        BF_REQUIRE(r == r0);
    }
}

void verifyLowCutMappingAndResponse() {
    BF_REQUIRE(std::abs(lowCutFrequencyFromNormalized(1.0) - 90.0) < 1.0e-12);
    BF_REQUIRE(std::abs(lowCutFrequencyFromNormalized(lowCutNormalizedFromFrequency(25.0)) - 25.0) < 1.0e-9);
    BF_REQUIRE(std::abs(lowCutFrequencyFromNormalized(lowCutNormalizedFromFrequency(90.0)) - 90.0) < 1.0e-9);

    const double cut90 = lowCutNormalizedFromFrequency(90.0);
    const double g30 = steadyRms(30.0, cut90, 0.0) / steadyRms(30.0, 0.0, 0.0);
    const double g120 = steadyRms(120.0, cut90, 0.0) / steadyRms(120.0, 0.0, 0.0);

    BF_REQUIRE(g30 < 0.20);
    BF_REQUIRE(g120 > 0.70);
    BF_REQUIRE(g30 < g120);
}

void verifyBassMassContract() {
    const double off75 = massDelta(75.0, 0.0);
    const double off220 = massDelta(220.0, 0.0);

    BF_REQUIRE(off75 > 3.0);
    BF_REQUIRE(off75 < 6.5);
    BF_REQUIRE(off220 < -2.0);

    const double cut90 = lowCutNormalizedFromFrequency(90.0);
    const double highCut50 = massDelta(50.0, cut90);
    const double highCut110 = massDelta(110.0, cut90);

    BF_REQUIRE(highCut110 > highCut50);
    BF_REQUIRE(highCut110 < off75);
}

void verifyFinishModesAreFiniteDistinctAndLevelBounded() {
    double signatures[3] {};
    const double modes[3] {0.0, 0.5, 1.0};

    for (int m = 0; m < 3; ++m) {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.setMode(modes[m]);
        dsp.setFinish(1.0);
        dsp.setMass(0.0);
        dsp.setLowCut(0.0);
        dsp.setToneMatchAmount(0.0);
        dsp.reset();

        long double diff = 0.0L;
        for (int i = 0; i < static_cast<int>(kFs * 3.0); ++i) {
            const double t = static_cast<double>(i) / kFs;
            const double pulse = std::fmod(t, 0.25) < 0.050 ? 1.0 : 0.22;
            const double x =
                pulse * (0.34 * std::sin(2.0*kPi*55.0*t) +
                         0.22 * std::sin(2.0*kPi*110.0*t)) +
                0.18 * std::sin(2.0*kPi*180.0*t) +
                0.12 * std::sin(2.0*kPi*900.0*t) +
                0.08 * std::sin(2.0*kPi*2200.0*t) +
                0.05 * std::sin(2.0*kPi*5200.0*t);

            double l = x;
            double r = 0.98*x + 0.01*std::sin(2.0*kPi*1450.0*t);
            dsp.processFrame(l, r);
            BF_REQUIRE(std::isfinite(l) && std::isfinite(r));
            const long double dl = static_cast<long double>(l - x);
            diff += dl * dl;
        }

        signatures[m] = static_cast<double>(diff);
        BF_REQUIRE(std::abs(dsp.currentAutoLevelGainDb()) <= 3.0001);
        BF_REQUIRE(signatures[m] > 1.0e-8);
    }

    BF_REQUIRE(std::abs(signatures[0] - signatures[1]) > 1.0e-6);
    BF_REQUIRE(std::abs(signatures[1] - signatures[2]) > 1.0e-6);
}

}


void verifyAutoInputAndFinalContract() {
    // INPUT AUTO must remain dormant on the exact neutral path.
    {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.setFinish(0.0);
        dsp.setMass(0.0);
        dsp.setLowCut(0.0);
        dsp.setToneMatchAmount(0.0);
        dsp.reset();

        for (int i = 0; i < 4096; ++i) {
            const double x =
                0.2 *
                std::sin(
                    2.0 * kPi * 82.0 *
                    static_cast<double>(i) /
                    kFs);
            double l = x;
            double r = x;
            dsp.processFrame(l, r);
            BF_REQUIRE(l == x);
            BF_REQUIRE(r == x);
        }
    }

    // With production processing active, very hot material must remain finite
    // and FINAL must enforce the linked -0.1 dBFS ceiling.
    {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.setFinish(1.0);
        dsp.setMass(1.0);
        dsp.setToneMatchAmount(0.0);
        dsp.reset();

        constexpr double ceiling =
            0.9885530946569389;

        for (int i = 0; i < 48000; ++i) {
            const double t =
                static_cast<double>(i) /
                kFs;

            double l =
                1.8 *
                std::sin(
                    2.0 * kPi * 55.0 * t);

            double r =
                1.6 *
                std::sin(
                    2.0 * kPi * 73.0 * t);

            dsp.processFrame(l, r);

            BF_REQUIRE(std::isfinite(l));
            BF_REQUIRE(std::isfinite(r));
            BF_REQUIRE(std::abs(l) <= ceiling + 1.0e-12);
            BF_REQUIRE(std::abs(r) <= ceiling + 1.0e-12);
        }
    }

    // FINAL must not touch ordinary sub-knee material by itself.
    {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.reset();

        for (int i = 0; i < 4096; ++i) {
            const double l0 =
                0.4 *
                std::sin(
                    0.011 * i);
            const double r0 =
                0.35 *
                std::cos(
                    0.013 * i);

            double l = l0;
            double r = r0;
            dsp.processFrame(l, r);

            BF_REQUIRE(l == l0);
            BF_REQUIRE(r == r0);
        }
    }
}


double thirdHarmonicAmplitude(double massAmount) {
    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setFinish(0.0);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.setMass(massAmount);
    dsp.reset();

    constexpr double fundamental = 80.0;
    constexpr double harmonic = 240.0;
    constexpr int total = static_cast<int>(kFs * 2.0);
    constexpr int start = static_cast<int>(kFs * 1.0);

    long double sinAcc = 0.0L;
    long double cosAcc = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        const double time =
            static_cast<double>(i) /
            kFs;

        const double x =
            0.45 *
            std::sin(
                2.0 * kPi *
                fundamental *
                time);

        double l = x;
        double r = x;
        dsp.processFrame(l, r);

        if (i >= start) {
            const double phase =
                2.0 * kPi *
                harmonic *
                time;

            sinAcc +=
                static_cast<long double>(
                    l *
                    std::sin(phase));

            cosAcc +=
                static_cast<long double>(
                    l *
                    std::cos(phase));

            ++count;
        }
    }

    return
        2.0 *
        std::sqrt(
            static_cast<double>(
                sinAcc * sinAcc +
                cosAcc * cosAcc)) /
        static_cast<double>(count);
}

void verifyLowControlAndDynamicMass() {
    const double cut90 =
        lowCutNormalizedFromFrequency(
            90.0);

    // The static boundary still strongly removes true sub content.
    const double subBefore =
        steadyRms(
            30.0,
            0.0,
            0.0);

    const double subAfter =
        steadyRms(
            30.0,
            cut90,
            0.0);

    BF_REQUIRE(
        subAfter <
        subBefore * 0.20);

    // MASS is no longer only a static EQ: the band-limited nonlinear residual
    // must create measurable third harmonic content from an 80 Hz sine.
    const double h3Off =
        thirdHarmonicAmplitude(0.0);

    const double h3On =
        thirdHarmonicAmplitude(1.0);

    BF_REQUIRE(
        h3On >
        h3Off + 1.0e-4);
}


double finishThirdHarmonicAmplitude(double finishAmount) {
    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setMode(0.5);
    dsp.setFinish(finishAmount);
    dsp.setMass(0.0);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    constexpr double fundamental = 300.0;
    constexpr double harmonic = 900.0;
    constexpr int total = static_cast<int>(kFs * 3.0);
    constexpr int start = static_cast<int>(kFs * 2.0);

    long double sinAcc = 0.0L;
    long double cosAcc = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        const double time =
            static_cast<double>(i) /
            kFs;

        const double x =
            0.22 *
            std::sin(
                2.0 * kPi *
                fundamental *
                time);

        double l = x;
        double r = x;
        dsp.processFrame(l, r);

        if (i >= start) {
            const double phase =
                2.0 * kPi *
                harmonic *
                time;

            sinAcc +=
                static_cast<long double>(
                    l * std::sin(phase));

            cosAcc +=
                static_cast<long double>(
                    l * std::cos(phase));

            ++count;
        }
    }

    return
        2.0 *
        std::sqrt(
            static_cast<double>(
                sinAcc * sinAcc +
                cosAcc * cosAcc)) /
        static_cast<double>(count);
}

void verifyFinishAddsControlledHarmonics() {
    const double h3Off =
        finishThirdHarmonicAmplitude(0.0);

    const double h3On =
        finishThirdHarmonicAmplitude(1.0);

    BF_REQUIRE(
        h3On >
        h3Off + 1.0e-5);

    // The added harmonic should remain controlled rather than becoming a
    // fuzz stage.
    BF_REQUIRE(
        h3On <
        0.08);
}


void verifySampleRatesExtremesAndStereoLink() {
    const double sampleRates[] {
        44100.0,
        48000.0,
        96000.0,
        192000.0
    };

    for (const double fs : sampleRates) {
        MetalFinisherDSP dsp;
        dsp.prepare(fs);
        dsp.setMode(0.5);
        dsp.setFinish(1.0);
        dsp.setMass(1.0);
        dsp.setLowCut(
            lowCutNormalizedFromFrequency(
                70.0));
        dsp.setToneMatchAmount(0.0);
        dsp.reset();

        const int total =
            static_cast<int>(
                std::min(
                    fs * 0.25,
                    48000.0));

        for (int i = 0; i < total; ++i) {
            const double time =
                static_cast<double>(i) /
                fs;

            // Deliberately hotter than normal production level to exercise
            // Auto Input, FINISH, LOW CONTROL, MASS and FINAL together.
            double l =
                2.5 *
                std::sin(
                    2.0 * kPi * 41.0 * time) +
                0.8 *
                std::sin(
                    2.0 * kPi * 820.0 * time);

            double r =
                2.1 *
                std::sin(
                    2.0 * kPi * 55.0 * time) +
                0.6 *
                std::sin(
                    2.0 * kPi * 1450.0 * time);

            dsp.processFrame(l, r);

            BF_REQUIRE(std::isfinite(l));
            BF_REQUIRE(std::isfinite(r));
            BF_REQUIRE(
                std::abs(l) <=
                0.9885530946569389 +
                1.0e-12);
            BF_REQUIRE(
                std::abs(r) <=
                0.9885530946569389 +
                1.0e-12);
        }

        dsp.reset();

        for (int i = 0; i < 4096; ++i) {
            double l = 0.0;
            double r = 0.0;
            dsp.processFrame(l, r);
            BF_REQUIRE(std::isfinite(l));
            BF_REQUIRE(std::isfinite(r));
        }
    }

    // FINAL gain reduction is linked between channels. A hot left channel
    // must not alter stereo balance by being limited independently.
    {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.setFinish(0.0);
        dsp.setMass(0.0);
        dsp.setLowCut(0.0);
        dsp.setToneMatchAmount(0.0);
        dsp.reset();

        double l = 1.5;
        double r = 0.3;

        dsp.processFrame(l, r);

        BF_REQUIRE(
            std::abs(l) <=
            0.9885530946569389 +
            1.0e-12);

        BF_REQUIRE(
            std::abs(
                (r / l) -
                0.2) <
            1.0e-12);
    }
}

void verifyPathologicalInputsRecover() {
    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setFinish(1.0);
    dsp.setMass(1.0);
    dsp.setLowCut(1.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    double l =
        std::numeric_limits<double>::infinity();

    double r =
        std::numeric_limits<double>::quiet_NaN();

    dsp.processFrame(l, r);

    BF_REQUIRE(std::isfinite(l));
    BF_REQUIRE(std::isfinite(r));

    for (int i = 0; i < 8192; ++i) {
        const double x =
            0.1 *
            std::sin(
                2.0 * kPi * 90.0 *
                static_cast<double>(i) /
                kFs);

        l = x;
        r = x;

        dsp.processFrame(l, r);

        BF_REQUIRE(std::isfinite(l));
        BF_REQUIRE(std::isfinite(r));
    }
}


double measuredHarmonic(
    double sampleRate,
    double fundamental,
    double harmonic,
    double inputAmplitude,
    double finish,
    double mass) {

    MetalFinisherDSP dsp;
    dsp.prepare(sampleRate);
    dsp.setMode(0.5);
    dsp.setFinish(finish);
    dsp.setMass(mass);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    const int total =
        static_cast<int>(
            sampleRate * 2.0);

    const int start =
        static_cast<int>(
            sampleRate * 1.0);

    long double sinAcc = 0.0L;
    long double cosAcc = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        const double time =
            static_cast<double>(i) /
            sampleRate;

        const double x =
            inputAmplitude *
            std::sin(
                2.0 * kPi *
                fundamental *
                time);

        double l = x;
        double r = x;
        dsp.processFrame(l, r);

        if (i >= start) {
            const double phase =
                2.0 * kPi *
                harmonic *
                time;

            sinAcc +=
                static_cast<long double>(
                    l * std::sin(phase));

            cosAcc +=
                static_cast<long double>(
                    l * std::cos(phase));

            ++count;
        }
    }

    return
        2.0 *
        std::sqrt(
            static_cast<double>(
                sinAcc * sinAcc +
                cosAcc * cosAcc)) /
        static_cast<double>(count);
}

double measuredDc(
    double sampleRate,
    double fundamental,
    double inputAmplitude,
    double finish,
    double mass) {

    MetalFinisherDSP dsp;
    dsp.prepare(sampleRate);
    dsp.setMode(0.5);
    dsp.setFinish(finish);
    dsp.setMass(mass);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    const int total =
        static_cast<int>(
            sampleRate * 2.0);

    const int start =
        static_cast<int>(
            sampleRate * 1.0);

    long double sum = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        const double time =
            static_cast<double>(i) /
            sampleRate;

        const double x =
            inputAmplitude *
            std::sin(
                2.0 * kPi *
                fundamental *
                time);

        double l = x;
        double r = x;
        dsp.processFrame(l, r);

        if (i >= start) {
            sum += l;
            ++count;
        }
    }

    return std::abs(
        static_cast<double>(
            sum /
            static_cast<long double>(
                count)));
}

void verifyNonlinearStagesRemainControlled() {
    // FINISH intentionally runs behind INPUT AUTO. Its nonlinear operating
    // level is therefore stabilized across ordinary source-level changes;
    // requiring monotonically more H3 for a hotter external input would test
    // against the product design rather than against the saturation itself.
    const double finishH3Low =
        measuredHarmonic(
            48000.0,
            300.0,
            900.0,
            0.08,
            1.0,
            0.0);

    const double finishH3High =
        measuredHarmonic(
            48000.0,
            300.0,
            900.0,
            0.22,
            1.0,
            0.0);

    BF_REQUIRE(finishH3Low > 1.0e-6);
    BF_REQUIRE(finishH3High > 1.0e-6);

    const double finishLevelRatio =
        finishH3High /
        finishH3Low;

    BF_REQUIRE(finishLevelRatio > 0.25);
    BF_REQUIRE(finishLevelRatio < 4.0);

    // MASS has no INPUT AUTO in front of it, so its harmonic residual must
    // remain genuinely level-dependent.
    const double massH3Low =
        measuredHarmonic(
            48000.0,
            80.0,
            240.0,
            0.15,
            0.0,
            1.0);

    const double massH3High =
        measuredHarmonic(
            48000.0,
            80.0,
            240.0,
            0.45,
            0.0,
            1.0);

    BF_REQUIRE(
        massH3High >
        massH3Low +
        1.0e-6);

    // tanh is intentionally symmetric; the surrounding filters and level
    // management must not introduce meaningful DC on a symmetric sine.
    BF_REQUIRE(
        measuredDc(
            48000.0,
            300.0,
            0.22,
            1.0,
            0.0) <
        1.0e-4);

    BF_REQUIRE(
        measuredDc(
            48000.0,
            80.0,
            0.45,
            0.0,
            1.0) <
        1.0e-4);

    // Harmonic generation should remain in the same order of magnitude at
    // common production sample rates. This is a regression guard, not an
    // aliasing verdict; aliasing gets its own measured decision before release.
    const double h3At44 =
        measuredHarmonic(
            44100.0,
            300.0,
            900.0,
            0.22,
            1.0,
            0.0);

    const double h3At96 =
        measuredHarmonic(
            96000.0,
            300.0,
            900.0,
            0.22,
            1.0,
            0.0);

    BF_REQUIRE(h3At44 > 0.0);
    BF_REQUIRE(h3At96 > 0.0);

    const double ratio =
        h3At44 / h3At96;

    BF_REQUIRE(ratio > 0.5);
    BF_REQUIRE(ratio < 2.0);
}

int main() {
    verifyNeutralPathIsExact();
    verifyLowCutMappingAndResponse();
    verifyBassMassContract();
    verifyFinishModesAreFiniteDistinctAndLevelBounded();
    verifyAutoInputAndFinalContract();
    verifyLowControlAndDynamicMass();
    verifyFinishAddsControlledHarmonics();
    verifySampleRatesExtremesAndStereoLink();
    verifyPathologicalInputsRecover();
    verifyNonlinearStagesRemainControlled();
    std::cout << "Bass Finisher DSP contract tests passed\n";
    return 0;
}
