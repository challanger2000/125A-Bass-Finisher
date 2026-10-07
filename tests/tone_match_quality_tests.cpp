#include "TestSupport.h"
#include "ToneMatchAnalyzer.h"
#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace HighGainGuitarFinisher::dsp;

namespace {

constexpr std::array<double, 16> kZones {
    35.0, 50.0, 65.0, 82.0,
    105.0, 135.0, 175.0, 225.0,
    300.0, 400.0, 550.0, 750.0,
    1050.0, 1500.0, 2200.0, 3200.0
};

constexpr std::array<double, 16> kWeights {
    0.80, 0.90, 1.00, 1.00,
    1.00, 1.00, 1.00, 1.00,
    0.95, 0.92, 0.92, 0.96,
    1.00, 0.92, 0.68, 0.42
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
            3.6 * gaussianLog(frequency, 1100.0, 0.27);

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

    BF_REQUIRE(
        after <
        before * 0.80);
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

int main() {
    verifyDistanceImproves();
    verifyNarrowSpikeIsRejected();
    verifySubBoostProtection();

    std::cout
        << "Bass Finisher protected 16-zone MATCH quality passed\n";

    return 0;
}
