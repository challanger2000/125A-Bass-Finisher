#include "TestSupport.h"
#include "HighGainGuitarFinisherProcessor.h"
#include "HighGainGuitarFinisherIDs.h"
#include "ToneMatchStateIO.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace HighGainGuitarFinisher;
using namespace Steinberg;

namespace {
void rewind(MemoryStream& s){
    int64 pos=0;
    BF_REQUIRE(s.seek(0,IBStream::kIBSeekSet,&pos)==kResultOk);
    BF_REQUIRE(pos==0);
}
}


void verifyCoreValues(
    Processor& p,
    const double expected[6],
    double expectedMatchAmount) {

    MemoryStream saved;
    BF_REQUIRE(p.getState(&saved)==kResultOk);
    rewind(saved);

    IBStreamer r(&saved,kLittleEndian);

    int32 version=0;
    BF_REQUIRE(r.readInt32(version));
    BF_REQUIRE(version==kStateVersion);

    for(int i=0;i<6;++i){
        double value=0.0;
        BF_REQUIRE(r.readDouble(value));
        BF_REQUIRE(
            std::abs(
                value-
                expected[i])<
            1.0e-12);
    }

    ToneMatchStatePayload tm{};
    BF_REQUIRE(readToneMatchState(r,tm,kStateVersion));
    BF_REQUIRE(
        std::abs(
            tm.amount-
            expectedMatchAmount)<
        1.0e-12);
}

int main(){
    BF_REQUIRE(kStateVersion==4);
    BF_REQUIRE(kFirstSupportedStateVersion==1);

    MemoryStream state;
    IBStreamer w(&state,kLittleEndian);
    BF_REQUIRE(w.writeInt32(kStateVersion));
    const double values[6]{0.61,0.54,1.0,0.38,0.5,0.72};
    for(double v:values) BF_REQUIRE(w.writeDouble(v));

    ToneMatchStatePayload tm{};
    tm.amount=0.44;
    tm.profile.valid=true;
    tm.profile.firValid=true;
    tm.profile.firTaps.fill(0.0);
    tm.profile.firTaps[0]=1.0;
    tm.profile.lowShelfFrequencyHz=72.0;
    tm.profile.lowShelfGainDb=1.2;
    tm.profile.highShelfFrequencyHz=6200.0;
    tm.profile.highShelfGainDb=-1.1;
    for(std::size_t i=0;i<tm.profile.peaks.size();++i){
        tm.profile.peaks[i].frequencyHz=100.0+350.0*static_cast<double>(i);
        tm.profile.peaks[i].q=0.8;
        tm.profile.peaks[i].gainDb=-1.0+0.2*static_cast<double>(i);
    }
    BF_REQUIRE(writeToneMatchState(w,tm));

    dsp::ToneMatchSpectrumSnapshot ref{};
    ref.sampleRate=48000.0;
    ref.frameCount=12u;
    for(std::size_t i=0;i<ref.meanPower.size();++i) ref.meanPower[i]=1.0e-6+1.0e-9*static_cast<double>(i);
    BF_REQUIRE(writeToneMatchReferenceState(w,ref));

    rewind(state);

    Processor p;
    BF_REQUIRE(p.setState(&state)==kResultOk);

    MemoryStream saved;
    BF_REQUIRE(p.getState(&saved)==kResultOk);
    rewind(saved);

    IBStreamer r(&saved,kLittleEndian);
    int32 version=0;
    BF_REQUIRE(r.readInt32(version));
    BF_REQUIRE(version==4);

    double restored[6]{};
    for(double& v:restored) BF_REQUIRE(r.readDouble(v));
    for(int i=0;i<6;++i) BF_REQUIRE(std::abs(restored[i]-values[i])<1.0e-12);

    ToneMatchStatePayload tm2{};
    BF_REQUIRE(readToneMatchState(r,tm2,kStateVersion));
    BF_REQUIRE(std::abs(tm2.amount-tm.amount)<1.0e-12);
    BF_REQUIRE(tm2.profile.valid);
    BF_REQUIRE(tm2.profile.firValid);
    BF_REQUIRE(
        std::abs(
            tm2.profile.firTaps[0] - 1.0) <
        1.0e-12);

    dsp::ToneMatchSpectrumSnapshot ref2{};
    BF_REQUIRE(readToneMatchReferenceState(r,ref2,kStateVersion));
    // Reference-state IO persists the analyzed spectrum, not the original
    // analyzer frame counter. A restored valid snapshot is canonicalized to
    // the minimum-ready frame count.
    BF_REQUIRE(ref2.frameCount>=4u);
    BF_REQUIRE(ref2.sampleRate==ref.sampleRate);
    BF_REQUIRE(ref2.meanPower==ref.meanPower);

    // V1 legacy 16-band state remains readable after expanding the matcher
    // to 64 adaptive bands.
    {
        MemoryStream oldState;
        IBStreamer oldWriter(
            &oldState,
            kLittleEndian);

        BF_REQUIRE(oldWriter.writeInt32(1));

        for (double v : values)
            BF_REQUIRE(
                oldWriter.writeDouble(v));

        BF_REQUIRE(oldWriter.writeDouble(0.31));
        BF_REQUIRE(oldWriter.writeInt32(1));
        BF_REQUIRE(oldWriter.writeDouble(80.0));
        BF_REQUIRE(oldWriter.writeDouble(1.0));

        for (std::size_t i = 0; i < 16u; ++i) {
            BF_REQUIRE(oldWriter.writeDouble(
                100.0 + 200.0 * static_cast<double>(i)));
            BF_REQUIRE(oldWriter.writeDouble(1.0));
            BF_REQUIRE(oldWriter.writeDouble(0.0));
        }

        BF_REQUIRE(oldWriter.writeDouble(6500.0));
        BF_REQUIRE(oldWriter.writeDouble(0.0));

        dsp::ToneMatchSpectrumSnapshot emptyReference {};
        BF_REQUIRE(
            writeToneMatchReferenceState(
                oldWriter,
                emptyReference));

        rewind(oldState);

        Processor oldProject;
        BF_REQUIRE(
            oldProject.setState(
                &oldState) ==
            kResultOk);
    }

    MemoryStream bad;
    IBStreamer bw(&bad,kLittleEndian);
    BF_REQUIRE(bw.writeInt32(99));
    rewind(bad);
    BF_REQUIRE(p.setState(&bad)==kResultFalse);

    // State loading must be atomic. A malformed/truncated stream may fail,
    // but it must not partially overwrite an already valid plugin state.
    {
        MemoryStream truncated;
        IBStreamer tw(&truncated,kLittleEndian);
        BF_REQUIRE(tw.writeInt32(kStateVersion));
        BF_REQUIRE(tw.writeDouble(0.01));
        BF_REQUIRE(tw.writeDouble(0.02));
        rewind(truncated);

        BF_REQUIRE(
            p.setState(&truncated)==
            kResultFalse);

        verifyCoreValues(
            p,
            values,
            tm.amount);
    }

    {
        MemoryStream nonFinite;
        IBStreamer nw(&nonFinite,kLittleEndian);
        BF_REQUIRE(nw.writeInt32(kStateVersion));
        BF_REQUIRE(nw.writeDouble(0.10));
        BF_REQUIRE(
            nw.writeDouble(
                std::numeric_limits<double>::quiet_NaN()));
        rewind(nonFinite);

        BF_REQUIRE(
            p.setState(&nonFinite)==
            kResultFalse);

        verifyCoreValues(
            p,
            values,
            tm.amount);
    }

    std::cout<<"Bass Finisher V1 state round-trip passed\n";
    return 0;
}
