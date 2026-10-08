#include "TestSupport.h"
#include "ToneMatchDSP.h"

#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>

using namespace HighGainGuitarFinisher::dsp;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

ToneMatchProfile onePeakProfile() {
    ToneMatchProfile p {};
    p.valid = true;
    for (auto& peak : p.peaks) {
        peak.frequencyHz = 1000.0;
        peak.q = 1.0;
        peak.gainDb = 0.0;
    }
    p.peaks[0].frequencyHz = 1000.0;
    p.peaks[0].q = 1.0;
    p.peaks[0].gainDb = 6.0;
    return p;
}

double steadyRms(double fs, double frequency, double amount) {
    ToneMatchDSP dsp;
    dsp.prepare(fs);
    dsp.setProfile(onePeakProfile());
    dsp.setAmount(amount);
    dsp.reset();

    const int total = static_cast<int>(fs * 2.0);
    const int start = static_cast<int>(fs);
    long double power = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        double l = 0.05 * std::sin(2.0*kPi*frequency*static_cast<double>(i)/fs);
        double r = l;
        dsp.processFrame(l, r);
        BF_REQUIRE(std::isfinite(l) && std::isfinite(r));
        if (i >= start) {
            power += 0.5L*(l*l+r*r);
            ++count;
        }
    }
    return std::sqrt(static_cast<double>(power/static_cast<long double>(count)));
}

double deltaDb(double fs, double f, double amount) {
    return 20.0 * std::log10(steadyRms(fs,f,amount)/steadyRms(fs,f,0.0));
}

void verifyZeroExact() {
    ToneMatchDSP dsp;
    dsp.prepare(48000.0);
    dsp.setProfile(onePeakProfile());
    dsp.setAmount(0.0);
    dsp.reset();
    for (int i=0;i<100000;++i) {
        const double l0=0.37*std::sin(0.013*i);
        const double r0=0.29*std::cos(0.017*i);
        double l=l0,r=r0;
        dsp.processFrame(l,r);
        BF_REQUIRE(l==l0 && r==r0);
    }
}

void verifyAmountLawAndRates() {
    const double q=deltaDb(48000.0,1000.0,0.25);
    const double h=deltaDb(48000.0,1000.0,0.50);
    const double f=deltaDb(48000.0,1000.0,1.0);
    BF_REQUIRE(q>1.0 && q<2.1);
    BF_REQUIRE(h>2.4 && h<3.6);
    BF_REQUIRE(f>5.4 && f<6.6);
    BF_REQUIRE(q<h && h<f);
    BF_REQUIRE(std::abs(deltaDb(48000.0,100.0,1.0))<0.5);

    for (double fs : {44100.0,48000.0,96000.0,192000.0}) {
        const double g=deltaDb(fs,1000.0,1.0);
        BF_REQUIRE(g>5.35 && g<6.65);
    }
}


ToneMatchProfile bassPeakProfile() {
    ToneMatchProfile p {};
    p.valid = true;
    p.lowShelfFrequencyHz = 60.0;
    p.lowShelfGainDb = 2.5;

    for (auto& peak : p.peaks) {
        peak.frequencyHz = 1000.0;
        peak.q = 1.0;
        peak.gainDb = 0.0;
    }

    p.peaks[0].frequencyHz = 100.0;
    p.peaks[0].q = 0.85;
    p.peaks[0].gainDb = 4.0;
    return p;
}

double bassContextRms(
    double lowCutContext,
    double massContext) {

    ToneMatchDSP dsp;
    dsp.prepare(48000.0);
    dsp.setProfile(bassPeakProfile());
    dsp.setAmount(1.0);
    dsp.setBassContext(
        lowCutContext,
        massContext);
    dsp.reset();

    constexpr double fs = 48000.0;
    constexpr double f = 100.0;
    constexpr int total = 96000;
    constexpr int start = 48000;

    long double power = 0.0L;
    int count = 0;

    for (int i = 0; i < total; ++i) {
        double l =
            0.05 *
            std::sin(
                2.0 * kPi * f *
                static_cast<double>(i) /
                fs);
        double r = l;

        dsp.processFrame(l, r);

        if (i >= start) {
            power +=
                0.5L *
                (l * l + r * r);
            ++count;
        }
    }

    return std::sqrt(
        static_cast<double>(
            power /
            static_cast<long double>(
                count)));
}

void verifyBassContextDoesNotWeakenMatch() {
    const double neutral =
        bassContextRms(0.0, 0.0);

    const double downstreamMax =
        bassContextRms(1.0, 1.0);

    BF_REQUIRE(neutral > 0.0);
    BF_REQUIRE(
        std::abs(
            downstreamMax -
            neutral) <
        1.0e-12);
}

ToneMatchProfile firGainProfile() {
    ToneMatchProfile p {};
    p.valid = true;
    p.firValid = true;
    p.firTaps.fill(0.0);
    p.firTaps[0] = 2.0;
    return p;
}

void verifyFirPath() {
    ToneMatchDSP dsp;
    dsp.prepare(48000.0);
    dsp.setProfile(firGainProfile());
    dsp.setAmount(1.0);
    dsp.reset();

    for (int i = 0;
         i < 4096;
         ++i) {

        const double x =
            0.05 *
            std::sin(
                2.0 * kPi *
                440.0 *
                static_cast<double>(i) /
                48000.0);

        double l = x;
        double r = x;

        dsp.processFrame(l, r);

        BF_REQUIRE(
            std::abs(l - 2.0 * x) <
            1.0e-10);

        BF_REQUIRE(
            std::abs(r - 2.0 * x) <
            1.0e-10);
    }
}

void verifySanitization() {
    ToneMatchDSP dsp;
    dsp.prepare(48000.0);
    ToneMatchProfile p {};
    p.valid = true;
    p.lowShelfFrequencyHz = -1000.0;
    p.lowShelfGainDb = 200.0;
    p.peaks[0].frequencyHz = 1.0e20;
    p.peaks[0].q = -4.0;
    p.peaks[0].gainDb = -200.0;
    p.highShelfFrequencyHz = 1.0e20;
    p.highShelfGainDb = std::numeric_limits<double>::quiet_NaN();
    dsp.setProfile(p);
    const auto& safe=dsp.profile();
    BF_REQUIRE(safe.lowShelfFrequencyHz>=40.0);
    BF_REQUIRE(safe.lowShelfGainDb<=12.0);
    BF_REQUIRE(safe.peaks[0].frequencyHz<=48000.0*0.45);
    BF_REQUIRE(safe.peaks[0].q>=0.25);
    BF_REQUIRE(safe.peaks[0].gainDb>=-12.0);
    BF_REQUIRE(std::isfinite(safe.highShelfGainDb));
}

}


void verifyFirRealtimeBudget() {
    ToneMatchProfile profile {};
    profile.valid = true;
    profile.firValid = true;
    profile.firTaps.fill(0.0);

    // Dense non-zero kernel prevents the optimizer from turning this into
    // a trivial sparse/identity case.
    for (std::size_t i = 0;
         i < profile.firTaps.size();
         ++i) {

        profile.firTaps[i] =
            0.00001 *
            std::sin(
                0.013 *
                static_cast<double>(i + 1u));
    }

    profile.firTaps[0] += 1.0;

    ToneMatchDSP dsp;
    dsp.prepare(48000.0);
    dsp.setProfile(profile);
    dsp.setAmount(1.0);
    dsp.reset();

    constexpr std::size_t kFrames = 48000u;
    double left = 0.1;
    double right = -0.1;

    const auto begin =
        std::chrono::steady_clock::now();

    for (std::size_t i = 0;
         i < kFrames;
         ++i) {

        left += 1.0e-12;
        right -= 1.0e-12;
        dsp.processFrame(left, right);
    }

    const auto end =
        std::chrono::steady_clock::now();

    const double seconds =
        std::chrono::duration<double>(
            end - begin).count();

    std::cout
        << "FIR realtime benchmark: taps="
        << kToneMatchFirTapCount
        << ", 1s stereo processed in "
        << seconds
        << " s, realtime load="
        << (100.0 * seconds)
        << "% of one core\n";

    BF_REQUIRE(std::isfinite(left));
    BF_REQUIRE(std::isfinite(right));

    // A release build must have meaningful safety margin for the rest of the
    // plugin and host. CI timing is noisy, so only reject catastrophically
    // non-realtime direct convolution here.
    BF_REQUIRE(seconds < 0.50);
}

int main() {
    verifyFirRealtimeBudget();
    verifyZeroExact();
    verifyAmountLawAndRates();
    verifyBassContextDoesNotWeakenMatch();
    verifyFirPath();
    verifySanitization();
    std::cout << "Bass Finisher Tone Match core tests passed\n";
    return 0;
}
