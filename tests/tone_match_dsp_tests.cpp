#include "TestSupport.h"
#include "ToneMatchDSP.h"

#include <cmath>
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

int main() {
    verifyZeroExact();
    verifyAmountLawAndRates();
    verifySanitization();
    std::cout << "Bass Finisher Tone Match core tests passed\n";
    return 0;
}
