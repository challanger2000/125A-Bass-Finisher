#include "TestSupport.h"
#include "HighGainGuitarFinisherProcessor.h"
#include "HighGainGuitarFinisherIDs.h"
#include "ToneMatchStateIO.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <array>
#include <cmath>
#include <iostream>

using HighGainGuitarFinisher::Processor;
using namespace HighGainGuitarFinisher;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr double kFs = 48000.0;
constexpr int32 kBlock = 256;

void addChange(ParameterChanges& changes, ParamID id, ParamValue value) {
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(id, queueIndex);
    BF_REQUIRE(queue != nullptr);
    int32 pointIndex = 0;
    BF_REQUIRE(queue->addPoint(0, value, pointIndex) == kResultTrue);
}

void rewind(MemoryStream& stream) {
    int64 position = 0;
    BF_REQUIRE(stream.seek(0, IBStream::kIBSeekSet, &position) == kResultOk);
    BF_REQUIRE(position == 0);
}

std::array<double, 6> readCoreState(Processor& p) {
    MemoryStream state;
    BF_REQUIRE(p.getState(&state) == kResultOk);
    rewind(state);
    IBStreamer r(&state, kLittleEndian);
    int32 version = 0;
    BF_REQUIRE(r.readInt32(version));
    BF_REQUIRE(version == kStateVersion);
    std::array<double, 6> values {};
    for (auto& v : values)
        BF_REQUIRE(r.readDouble(v));
    return values;
}

void verifyParameterFlush() {
    Processor p;
    BF_REQUIRE(p.initialize(nullptr) == kResultOk);

    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = kBlock;
    setup.sampleRate = kFs;
    BF_REQUIRE(p.setupProcessing(setup) == kResultOk);

    ParameterChanges changes(3);
    addChange(changes, kFinish, 0.73);
    addChange(changes, kOutput, 0.61);
    addChange(changes, kMass, 0.42);

    ProcessData flush {};
    flush.processMode = kRealtime;
    flush.symbolicSampleSize = kSample64;
    flush.numSamples = 0;
    flush.numInputs = 0;
    flush.numOutputs = 0;
    flush.inputParameterChanges = &changes;

    BF_REQUIRE(p.process(flush) == kResultOk);

    const auto state = readCoreState(p);
    BF_REQUIRE(std::abs(state[0] - 0.73) < 1.0e-12);
    BF_REQUIRE(std::abs(state[1] - 0.61) < 1.0e-12);
    BF_REQUIRE(std::abs(state[5] - 0.42) < 1.0e-12);

    BF_REQUIRE(p.terminate() == kResultOk);
}

void verifyLifecycleAndBusContracts() {
    Processor p;
    BF_REQUIRE(p.initialize(nullptr) == kResultOk);

    SpeakerArrangement monoIn[1] {SpeakerArr::kMono};
    SpeakerArrangement monoOut[1] {SpeakerArr::kMono};
    BF_REQUIRE(p.setBusArrangements(monoIn, 1, monoOut, 1) == kResultOk);

    SpeakerArrangement badOut[1] {SpeakerArr::kStereo};
    BF_REQUIRE(p.setBusArrangements(monoIn, 1, badOut, 1) == kResultFalse);

    BF_REQUIRE(p.canProcessSampleSize(kSample32) == kResultTrue);
    BF_REQUIRE(p.canProcessSampleSize(kSample64) == kResultTrue);
    BF_REQUIRE(p.getTailSamples() == 0u);

    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = kBlock;
    setup.sampleRate = kFs;
    BF_REQUIRE(p.setupProcessing(setup) == kResultOk);
    BF_REQUIRE(p.setActive(true) == kResultOk);
    BF_REQUIRE(p.setProcessing(true) == kResultOk);

    std::array<double, kBlock> in {};
    std::array<double, kBlock> out {};
    for (int i = 0; i < kBlock; ++i)
        in[static_cast<std::size_t>(i)] = 0.2 * std::sin(2.0 * 3.14159265358979323846 * 82.41 * i / kFs);

    double* inPtrs[1] {in.data()};
    double* outPtrs[1] {out.data()};

    AudioBusBuffers inBus {};
    inBus.numChannels = 1;
    inBus.channelBuffers64 = inPtrs;

    AudioBusBuffers outBus {};
    outBus.numChannels = 1;
    outBus.channelBuffers64 = outPtrs;

    ProcessData data {};
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample64;
    data.numSamples = kBlock;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inBus;
    data.outputs = &outBus;

    BF_REQUIRE(p.process(data) == kResultOk);
    for (double v : out)
        BF_REQUIRE(std::isfinite(v));

    BF_REQUIRE(p.setProcessing(false) == kResultOk);
    BF_REQUIRE(p.setProcessing(true) == kResultOk);

    std::array<double, kBlock> zeroIn {};
    std::array<double, kBlock> zeroOut {};
    inPtrs[0] = zeroIn.data();
    outPtrs[0] = zeroOut.data();
    BF_REQUIRE(p.process(data) == kResultOk);
    for (double v : zeroOut)
        BF_REQUIRE(v == 0.0);

    BF_REQUIRE(p.setProcessing(false) == kResultOk);
    BF_REQUIRE(p.setActive(false) == kResultOk);
    BF_REQUIRE(p.terminate() == kResultOk);
}

}

int main() {
    verifyParameterFlush();
    verifyLifecycleAndBusContracts();
    std::cout << "Bass Finisher processor contracts passed\n";
    return 0;
}
