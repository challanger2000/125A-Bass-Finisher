#include "TestSupport.h"
#include "MetalFinisherDSP.h"
#include "LowCutMapping.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

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

struct LowControlPairMeasurement {
    double sub30 {0.0};
    double body120 {0.0};
};

LowControlPairMeasurement measureLowControlPair(
    double lowCut,
    bool adaptivePath) {

    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setFinish(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.setLowCut(lowCut);
    dsp.setMass(0.0);
    dsp.reset();

    HighGainGuitarFinisher::dsp::Biquad staticHighPass;
    staticHighPass.setCoefficients(
        HighGainGuitarFinisher::dsp::makeHighPass(
            kFs,
            lowCutFrequencyFromNormalized(lowCut),
            0.7071067811865476));

    constexpr double subFrequency = 30.0;
    constexpr double bodyFrequency = 120.0;
    constexpr double subAmplitude = 0.40;
    constexpr double bodyAmplitude = 0.04;
    constexpr int total = static_cast<int>(kFs * 4.0);
    constexpr int start = static_cast<int>(kFs * 3.0);

    long double subSin = 0.0L;
    long double subCos = 0.0L;
    long double bodySin = 0.0L;
    long double bodyCos = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        const double time =
            static_cast<double>(i) / kFs;

        const double x =
            subAmplitude *
                std::sin(
                    2.0 * kPi *
                    subFrequency *
                    time) +
            bodyAmplitude *
                std::sin(
                    2.0 * kPi *
                    bodyFrequency *
                    time);

        double y = 0.0;

        if (adaptivePath) {
            double l = x;
            double r = x;
            dsp.processFrame(l, r);
            BF_REQUIRE(std::isfinite(l));
            BF_REQUIRE(std::isfinite(r));
            y = l;
        } else {
            y = staticHighPass.process(x);
            BF_REQUIRE(std::isfinite(y));
        }

        if (i >= start) {
            const double subPhase =
                2.0 * kPi *
                subFrequency *
                time;

            const double bodyPhase =
                2.0 * kPi *
                bodyFrequency *
                time;

            subSin +=
                static_cast<long double>(
                    y * std::sin(subPhase));

            subCos +=
                static_cast<long double>(
                    y * std::cos(subPhase));

            bodySin +=
                static_cast<long double>(
                    y * std::sin(bodyPhase));

            bodyCos +=
                static_cast<long double>(
                    y * std::cos(bodyPhase));

            ++count;
        }
    }

    const auto amplitude =
        [count](long double sinAcc,
                long double cosAcc) {
            return
                2.0 *
                std::sqrt(
                    static_cast<double>(
                        sinAcc * sinAcc +
                        cosAcc * cosAcc)) /
                static_cast<double>(count);
        };

    return {
        amplitude(subSin, subCos),
        amplitude(bodySin, bodyCos)
    };
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


struct ModeMeasurement {
    double rms {0.0};
    double peak {0.0};
    double crestDb {0.0};
    std::array<double, 4> toneAmplitude {};
    std::vector<double> rendered {};
};

ModeMeasurement measureFinishMode(double mode) {
    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setMode(mode);
    dsp.setFinish(1.0);
    dsp.setMass(0.0);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    constexpr int total =
        static_cast<int>(kFs * 4.0);

    constexpr int start =
        static_cast<int>(kFs * 2.0);

    constexpr std::array<double, 4> probeHz {
        55.0,
        900.0,
        3200.0,
        6500.0
    };

    long double power = 0.0L;
    double peak = 0.0;
    std::array<long double, 4> sinAcc {};
    std::array<long double, 4> cosAcc {};
    int count = 0;

    ModeMeasurement result;
    result.rendered.reserve(
        static_cast<std::size_t>(
            total - start));

    for (int i = 0; i < total; ++i) {
        const double t =
            static_cast<double>(i) / kFs;

        const double pulse =
            std::fmod(t, 0.25) < 0.050
                ? 1.0
                : 0.22;

        const double x =
            pulse *
                (0.34 *
                     std::sin(
                         2.0 * kPi *
                         55.0 * t) +
                 0.22 *
                     std::sin(
                         2.0 * kPi *
                         110.0 * t)) +
            0.18 *
                std::sin(
                    2.0 * kPi *
                    180.0 * t) +
            0.12 *
                std::sin(
                    2.0 * kPi *
                    900.0 * t) +
            0.08 *
                std::sin(
                    2.0 * kPi *
                    3200.0 * t) +
            0.05 *
                std::sin(
                    2.0 * kPi *
                    6500.0 * t);

        double l = x;
        double r =
            0.98 * x +
            0.01 *
                std::sin(
                    2.0 * kPi *
                    1450.0 * t);

        dsp.processFrame(l, r);

        BF_REQUIRE(
            std::isfinite(l) &&
            std::isfinite(r));

        if (i >= start) {
            const double mono =
                0.5 * (l + r);

            result.rendered.push_back(
                mono);

            power +=
                static_cast<long double>(
                    mono * mono);

            peak =
                std::max(
                    peak,
                    std::abs(mono));

            for (std::size_t band = 0;
                 band < probeHz.size();
                 ++band) {

                const double phase =
                    2.0 * kPi *
                    probeHz[band] *
                    t;

                sinAcc[band] +=
                    static_cast<long double>(
                        mono *
                        std::sin(phase));

                cosAcc[band] +=
                    static_cast<long double>(
                        mono *
                        std::cos(phase));
            }

            ++count;
        }
    }

    result.rms =
        std::sqrt(
            static_cast<double>(
                power /
                static_cast<long double>(
                    count)));

    result.peak = peak;

    BF_REQUIRE(result.rms > 0.0);

    result.crestDb =
        20.0 *
        std::log10(
            result.peak /
            result.rms);

    for (std::size_t band = 0;
         band < probeHz.size();
         ++band) {

        result.toneAmplitude[band] =
            2.0 *
            std::sqrt(
                static_cast<double>(
                    sinAcc[band] *
                        sinAcc[band] +
                    cosAcc[band] *
                        cosAcc[band])) /
            static_cast<double>(
                count);
    }

    BF_REQUIRE(
        std::abs(
            dsp.currentAutoLevelGainDb()) <=
        3.0001);

    return result;
}

double normalizedRenderDifference(
    const ModeMeasurement& a,
    const ModeMeasurement& b) {

    BF_REQUIRE(
        a.rendered.size() ==
        b.rendered.size());

    long double diffPower = 0.0L;
    long double referencePower = 0.0L;

    for (std::size_t i = 0;
         i < a.rendered.size();
         ++i) {

        const long double d =
            static_cast<long double>(
                a.rendered[i] -
                b.rendered[i]);

        const long double ref =
            0.5L *
            static_cast<long double>(
                a.rendered[i] +
                b.rendered[i]);

        diffPower += d * d;
        referencePower += ref * ref;
    }

    BF_REQUIRE(referencePower > 0.0L);

    return
        std::sqrt(
            static_cast<double>(
                diffPower /
                referencePower));
}

double modeThirdHarmonicAmplitude(
    double mode) {

    MetalFinisherDSP dsp;
    dsp.prepare(kFs);
    dsp.setMode(mode);
    dsp.setFinish(1.0);
    dsp.setMass(0.0);
    dsp.setLowCut(0.0);
    dsp.setToneMatchAmount(0.0);
    dsp.reset();

    constexpr double fundamental =
        300.0;

    constexpr double harmonic =
        900.0;

    constexpr int total =
        static_cast<int>(
            kFs * 3.0);

    constexpr int start =
        static_cast<int>(
            kFs * 2.0);

    long double sinAcc = 0.0L;
    long double cosAcc = 0.0L;
    int count = 0;

    for (int i = 0;
         i < total;
         ++i) {

        const double t =
            static_cast<double>(i) /
            kFs;

        const double x =
            0.22 *
            std::sin(
                2.0 * kPi *
                fundamental *
                t);

        double l = x;
        double r = x;

        dsp.processFrame(l, r);

        if (i >= start) {
            const double phase =
                2.0 * kPi *
                harmonic *
                t;

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
        static_cast<double>(
            count);
}

void verifyFinishModesAreFiniteDistinctAndLevelBounded() {
    constexpr std::array<double, 3> modes {
        0.0,
        0.5,
        1.0
    };

    std::array<ModeMeasurement, 3> measured {};

    for (std::size_t i = 0;
         i < modes.size();
         ++i) {

        measured[i] =
            measureFinishMode(
                modes[i]);

        BF_REQUIRE(
            std::isfinite(
                measured[i].rms));

        BF_REQUIRE(
            std::isfinite(
                measured[i].crestDb));

        for (const double amplitude :
             measured[i].toneAmplitude) {

            BF_REQUIRE(
                std::isfinite(
                    amplitude));

            BF_REQUIRE(
                amplitude >= 0.0);
        }
    }

    const double cleanPunch =
        normalizedRenderDifference(
            measured[0],
            measured[1]);

    const double punchDense =
        normalizedRenderDifference(
            measured[1],
            measured[2]);

    const double cleanDense =
        normalizedRenderDifference(
            measured[0],
            measured[2]);

    // A mode is a product feature, not merely a different enum value.
    // Require at least 0.2% normalized waveform separation for adjacent
    // modes and a larger separation between the endpoints.
    BF_REQUIRE(cleanPunch > 0.0020);
    BF_REQUIRE(punchDense > 0.0020);
    BF_REQUIRE(cleanDense > 0.0030);

    // The spectral contract must not collapse to three level-shifted copies.
    // Across the bass/body, articulation, harshness and fizz probes, every
    // adjacent mode pair must differ by at least 0.05 dB in one measured
    // region.
    const auto maxBandDeltaDb =
        [](const ModeMeasurement& a,
           const ModeMeasurement& b) {

            double maximum = 0.0;

            for (std::size_t i = 0;
                 i < a.toneAmplitude.size();
                 ++i) {

                const double aa =
                    std::max(
                        a.toneAmplitude[i],
                        1.0e-12);

                const double bb =
                    std::max(
                        b.toneAmplitude[i],
                        1.0e-12);

                maximum =
                    std::max(
                        maximum,
                        std::abs(
                            20.0 *
                            std::log10(
                                aa / bb)));
            }

            return maximum;
        };

    BF_REQUIRE(
        maxBandDeltaDb(
            measured[0],
            measured[1]) >
        0.05);

    BF_REQUIRE(
        maxBandDeltaDb(
            measured[1],
            measured[2]) >
        0.05);

    // Transient shape must not be bit-for-bit identical between all modes.
    // This is intentionally a small floor because adaptive material can make
    // crest changes program-dependent; it still catches a disconnected mode
    // parameter.
    const double crestSpread =
        std::max({
            measured[0].crestDb,
            measured[1].crestDb,
            measured[2].crestDb
        }) -
        std::min({
            measured[0].crestDb,
            measured[1].crestDb,
            measured[2].crestDb
        });

    BF_REQUIRE(crestSpread > 0.01);

    // Nonlinear density is explicitly ordered by design:
    // CLEAN < PUNCH < DENSE.
    const double h3Clean =
        modeThirdHarmonicAmplitude(
            modes[0]);

    const double h3Punch =
        modeThirdHarmonicAmplitude(
            modes[1]);

    const double h3Dense =
        modeThirdHarmonicAmplitude(
            modes[2]);

    BF_REQUIRE(h3Clean > 0.0);
    BF_REQUIRE(h3Punch > h3Clean);
    BF_REQUIRE(h3Dense > h3Punch);

    std::cout
        << "Mode QA: "
        << "diff CP=" << cleanPunch
        << ", PD=" << punchDense
        << ", CD=" << cleanDense
        << " | crest="
        << measured[0].crestDb << "/"
        << measured[1].crestDb << "/"
        << measured[2].crestDb
        << " dB | H3="
        << h3Clean << "/"
        << h3Punch << "/"
        << h3Dense
        << "\n";
}

} // namespace

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

    // The exact neutral contract also covers hot near/full-scale material.
    // FINAL must not become a hidden limiter when all production controls are off.
    {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.setFinish(0.0);
        dsp.setMass(0.0);
        dsp.setLowCut(0.0);
        dsp.setToneMatchAmount(0.0);
        dsp.reset();

        for (int i = 0; i < 4096; ++i) {
            const double l0 =
                1.05 *
                std::sin(
                    0.011 * i);
            const double r0 =
                1.02 *
                std::cos(
                    0.013 * i);

            double l = l0;
            double r = r0;
            dsp.processFrame(l, r);

            BF_REQUIRE(l == l0);
            BF_REQUIRE(r == r0);
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

    // Regression for the adaptive LOW CONTROL stage through the real audio
    // path. A 10:1 30 Hz / 120 Hz fixture represents severe sub dominance.
    // Compare it against the exact same static high-pass boundary so the
    // additional delta can only come from the adaptive containment stage.
    //
    // Product contract at a 55 Hz boundary:
    // - dominant 30 Hz sub must receive at least 0.75 dB additional control;
    // - useful 120 Hz body must lose less than 0.10 dB from that adaptation.
    const double adaptiveCut =
        lowCutNormalizedFromFrequency(
            55.0);

    const auto staticOnly =
        measureLowControlPair(
            adaptiveCut,
            false);

    const auto adaptive =
        measureLowControlPair(
            adaptiveCut,
            true);

    const double adaptiveSubDeltaDb =
        dbRatio(
            adaptive.sub30,
            staticOnly.sub30);

    const double adaptiveBodyDeltaDb =
        dbRatio(
            adaptive.body120,
            staticOnly.body120);

    BF_REQUIRE(
        adaptiveSubDeltaDb <=
        -0.75);

    BF_REQUIRE(
        adaptiveBodyDeltaDb >
        -0.10);

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

    // FINAL gain reduction is linked between channels whenever a production
    // stage is active. A hot left channel must not alter stereo balance by
    // being limited independently.
    {
        MetalFinisherDSP dsp;
        dsp.prepare(kFs);
        dsp.setFinish(0.0);
        dsp.setMass(0.0);
        dsp.setLowCut(0.0);
        // Activate the production path without altering the signal before
        // FINAL: MATCH amount is non-zero, but no valid profile is loaded.
        dsp.setToneMatchAmount(1.0);
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


double measuredToneAmplitude(
    double sampleRate,
    double fundamental,
    double measureFrequency,
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
            sampleRate * 3.0);

    const int start =
        static_cast<int>(
            sampleRate * 2.0);

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
                measureFrequency *
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

void verifyFinishAliasRiskIsBounded() {
    // 3.5 kHz is near the top of the FINISH saturation band. At 48 kHz,
    // the 7th harmonic (24.5 kHz) folds to 23.5 kHz, so this is a useful
    // worst-case alias probe for deciding whether oversampling is warranted.
    constexpr double fundamental = 3500.0;
    constexpr double inputAmplitude = 0.22;

    const double fundamentalAmp =
        measuredToneAmplitude(
            48000.0,
            fundamental,
            fundamental,
            inputAmplitude,
            1.0,
            0.0);

    const double aliasAmp =
        measuredToneAmplitude(
            48000.0,
            fundamental,
            23500.0,
            inputAmplitude,
            1.0,
            0.0);

    BF_REQUIRE(fundamentalAmp > 1.0e-6);
    BF_REQUIRE(aliasAmp >= 0.0);

    const double aliasDbc =
        20.0 *
        std::log10(
            std::max(
                aliasAmp /
                    fundamentalAmp,
                1.0e-15));

    // This is intentionally a permissive release guard. If we exceed it,
    // oversampling or a narrower nonlinear band becomes justified by data.
    BF_REQUIRE(aliasDbc < -35.0);

    // MASS is confined to the low bass; a 180 Hz upper-band probe must have
    // negligible energy near Nyquist even without oversampling.
    const double massFundamental =
        measuredToneAmplitude(
            48000.0,
            180.0,
            180.0,
            0.45,
            0.0,
            1.0);

    const double massNearNyquist =
        measuredToneAmplitude(
            48000.0,
            180.0,
            23000.0,
            0.45,
            0.0,
            1.0);

    BF_REQUIRE(massFundamental > 1.0e-6);

    const double massNyquistDbc =
        20.0 *
        std::log10(
            std::max(
                massNearNyquist /
                    massFundamental,
                1.0e-15));

    BF_REQUIRE(massNyquistDbc < -70.0);
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
    verifyFinishAliasRiskIsBounded();
    std::cout << "Bass Finisher DSP contract tests passed\n";
    return 0;
}
