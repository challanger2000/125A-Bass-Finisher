#include "TestSupport.h"
#include "HighGainGuitarFinisherProcessor.h"
#include "HighGainGuitarFinisherIDs.h"
#include "ToneMatchStateIO.h"
#include "dsp/LowCutMapping.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

using HighGainGuitarFinisher::Processor;
using namespace HighGainGuitarFinisher;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr double kFs = 48000.0;
constexpr int32 kBlock = 256;

void addChange(ParameterChanges& changes, Steinberg::Vst::ParamID id, ParamValue value) {
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
    addChange(changes, HighGainGuitarFinisher::kFinish, 0.73);
    addChange(changes, HighGainGuitarFinisher::kOutput, 0.61);
    addChange(changes, HighGainGuitarFinisher::kMass, 0.42);

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


template <typename Sample>
void runNeutralProcessingCase(
    int32 symbolicSampleSize,
    ProcessModes processMode,
    bool stereo,
    int32 blockSize,
    double sampleRate) {

    Processor p;
    BF_REQUIRE(p.initialize(nullptr) == kResultOk);

    SpeakerArrangement inputArrangement[1] {
        stereo ? SpeakerArr::kStereo : SpeakerArr::kMono
    };

    SpeakerArrangement outputArrangement[1] {
        stereo ? SpeakerArr::kStereo : SpeakerArr::kMono
    };

    BF_REQUIRE(
        p.setBusArrangements(
            inputArrangement,
            1,
            outputArrangement,
            1) ==
        kResultOk);

    ProcessSetup setup {};
    setup.processMode = processMode;
    setup.symbolicSampleSize = symbolicSampleSize;
    setup.maxSamplesPerBlock = 1024;
    setup.sampleRate = sampleRate;

    BF_REQUIRE(p.setupProcessing(setup) == kResultOk);
    BF_REQUIRE(p.setActive(true) == kResultOk);
    BF_REQUIRE(p.setProcessing(true) == kResultOk);

    std::vector<Sample> inputLeft(
        static_cast<std::size_t>(blockSize));

    std::vector<Sample> inputRight(
        static_cast<std::size_t>(blockSize));

    std::vector<Sample> outputLeft(
        static_cast<std::size_t>(blockSize),
        static_cast<Sample>(0));

    std::vector<Sample> outputRight(
        static_cast<std::size_t>(blockSize),
        static_cast<Sample>(0));

    for (int32 i = 0; i < blockSize; ++i) {
        const double t =
            static_cast<double>(i) /
            sampleRate;

        inputLeft[static_cast<std::size_t>(i)] =
            static_cast<Sample>(
                0.21 *
                std::sin(
                    2.0 *
                    3.14159265358979323846 *
                    82.41 *
                    t +
                    0.37));

        inputRight[static_cast<std::size_t>(i)] =
            static_cast<Sample>(
                0.17 *
                std::sin(
                    2.0 *
                    3.14159265358979323846 *
                    123.47 *
                    t +
                    0.61));
    }

    Sample* inputPointers[2] {
        inputLeft.data(),
        stereo
            ? inputRight.data()
            : inputLeft.data()
    };

    Sample* outputPointers[2] {
        outputLeft.data(),
        stereo
            ? outputRight.data()
            : outputLeft.data()
    };

    AudioBusBuffers inputBus {};
    inputBus.numChannels = stereo ? 2 : 1;

    AudioBusBuffers outputBus {};
    outputBus.numChannels = stereo ? 2 : 1;

    if constexpr (std::is_same_v<Sample, float>) {
        inputBus.channelBuffers32 = inputPointers;
        outputBus.channelBuffers32 = outputPointers;
    } else {
        inputBus.channelBuffers64 = inputPointers;
        outputBus.channelBuffers64 = outputPointers;
    }

    ProcessData data {};
    data.processMode = processMode;
    data.symbolicSampleSize = symbolicSampleSize;
    data.numSamples = blockSize;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inputBus;
    data.outputs = &outputBus;

    BF_REQUIRE(p.process(data) == kResultOk);
    BF_REQUIRE(outputBus.silenceFlags == 0u);

    for (int32 i = 0; i < blockSize; ++i) {
        const auto index =
            static_cast<std::size_t>(i);

        BF_REQUIRE(std::isfinite(
            static_cast<double>(
                outputLeft[index])));

        BF_REQUIRE(
            outputLeft[index] ==
            inputLeft[index]);

        if (stereo) {
            BF_REQUIRE(std::isfinite(
                static_cast<double>(
                    outputRight[index])));

            BF_REQUIRE(
                outputRight[index] ==
                inputRight[index]);
        }
    }

    std::fill(
        inputLeft.begin(),
        inputLeft.end(),
        static_cast<Sample>(0));

    std::fill(
        inputRight.begin(),
        inputRight.end(),
        static_cast<Sample>(0));

    std::fill(
        outputLeft.begin(),
        outputLeft.end(),
        static_cast<Sample>(1));

    std::fill(
        outputRight.begin(),
        outputRight.end(),
        static_cast<Sample>(1));

    outputBus.silenceFlags = 0u;

    BF_REQUIRE(p.process(data) == kResultOk);

    const uint64 expectedSilenceFlags =
        stereo ? uint64 {3} : uint64 {1};

    BF_REQUIRE(
        outputBus.silenceFlags ==
        expectedSilenceFlags);

    for (const auto value : outputLeft)
        BF_REQUIRE(value == static_cast<Sample>(0));

    if (stereo) {
        for (const auto value : outputRight)
            BF_REQUIRE(value == static_cast<Sample>(0));
    }

    BF_REQUIRE(p.setProcessing(false) == kResultOk);
    BF_REQUIRE(p.setActive(false) == kResultOk);
    BF_REQUIRE(p.terminate() == kResultOk);
}


void verifySampleAccurateOutputAutomation() {
    Processor p;
    BF_REQUIRE(p.initialize(nullptr) == kResultOk);

    SpeakerArrangement mono[1] {SpeakerArr::kMono};
    BF_REQUIRE(p.setBusArrangements(mono, 1, mono, 1) == kResultOk);

    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = 16;
    setup.sampleRate = kFs;
    BF_REQUIRE(p.setupProcessing(setup) == kResultOk);
    BF_REQUIRE(p.setActive(true) == kResultOk);
    BF_REQUIRE(p.setProcessing(true) == kResultOk);

    constexpr int32 n = 16;
    std::array<double, n> input {};
    std::array<double, n> output {};
    input.fill(0.1);

    double* inPtrs[1] {input.data()};
    double* outPtrs[1] {output.data()};

    AudioBusBuffers inBus {};
    inBus.numChannels = 1;
    inBus.channelBuffers64 = inPtrs;

    AudioBusBuffers outBus {};
    outBus.numChannels = 1;
    outBus.channelBuffers64 = outPtrs;

    ParameterChanges changes(1);
    int32 queueIndex = 0;
    auto* queue = changes.addParameterData(kOutput, queueIndex);
    BF_REQUIRE(queue != nullptr);

    int32 pointIndex = 0;
    BF_REQUIRE(queue->addPoint(7, 0.75, pointIndex) == kResultTrue);
    BF_REQUIRE(queue->addPoint(15, 0.50, pointIndex) == kResultTrue);

    ProcessData data {};
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample64;
    data.numSamples = n;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inBus;
    data.outputs = &outBus;
    data.inputParameterChanges = &changes;

    BF_REQUIRE(p.process(data) == kResultOk);

    for (int32 i = 0; i < n; ++i) {
        double normalized = 0.0;

        if (i <= 7) {
            normalized =
                0.5 +
                (static_cast<double>(i + 1) / 8.0) *
                    0.25;
        } else {
            normalized =
                0.75 +
                (static_cast<double>(i - 7) / 8.0) *
                    (-0.25);
        }

        const double gain =
            std::pow(
                10.0,
                ((normalized * 24.0) - 12.0) /
                    20.0);

        const double expected =
            input[static_cast<std::size_t>(i)] *
            gain;

        BF_REQUIRE(
            std::abs(
                output[static_cast<std::size_t>(i)] -
                expected) <
            1.0e-12);
    }

    BF_REQUIRE(p.setProcessing(false) == kResultOk);
    BF_REQUIRE(p.setActive(false) == kResultOk);
    BF_REQUIRE(p.terminate() == kResultOk);
}

std::array<std::vector<double>, 2> renderActivePath(
    ProcessModes mode,
    const std::vector<int32>& chunks) {

    constexpr int32 totalSamples = 1024;

    Processor p;
    BF_REQUIRE(p.initialize(nullptr) == kResultOk);

    SpeakerArrangement stereo[1] {SpeakerArr::kStereo};
    BF_REQUIRE(p.setBusArrangements(stereo, 1, stereo, 1) == kResultOk);

    ProcessSetup setup {};
    setup.processMode = mode;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = totalSamples;
    setup.sampleRate = kFs;

    BF_REQUIRE(p.setupProcessing(setup) == kResultOk);
    BF_REQUIRE(p.setActive(true) == kResultOk);
    BF_REQUIRE(p.setProcessing(true) == kResultOk);

    ParameterChanges settings(4);
    addChange(settings, kFinish, 0.67);
    addChange(
        settings,
        kLowCut80,
        dsp::lowCutNormalizedFromFrequency(55.0));
    addChange(settings, kMode, 0.5);
    addChange(settings, kMass, 0.72);

    ProcessData flush {};
    flush.processMode = mode;
    flush.symbolicSampleSize = kSample64;
    flush.numSamples = 0;
    flush.inputParameterChanges = &settings;
    BF_REQUIRE(p.process(flush) == kResultOk);

    std::vector<double> left(totalSamples);
    std::vector<double> right(totalSamples);
    std::array<std::vector<double>, 2> output {
        std::vector<double>(totalSamples, 0.0),
        std::vector<double>(totalSamples, 0.0)
    };

    for (int32 i = 0; i < totalSamples; ++i) {
        const double t =
            static_cast<double>(i) /
            kFs;

        left[static_cast<std::size_t>(i)] =
            0.18 * std::sin(
                2.0 * 3.14159265358979323846 * 41.2 * t + 0.23) +
            0.06 * std::sin(
                2.0 * 3.14159265358979323846 * 123.5 * t + 0.61);

        right[static_cast<std::size_t>(i)] =
            0.16 * std::sin(
                2.0 * 3.14159265358979323846 * 55.0 * t + 0.41) +
            0.05 * std::sin(
                2.0 * 3.14159265358979323846 * 185.0 * t + 0.79);
    }

    int32 offset = 0;

    for (const int32 chunk : chunks) {
        BF_REQUIRE(chunk > 0);
        BF_REQUIRE(offset + chunk <= totalSamples);

        double* inPtrs[2] {
            left.data() + offset,
            right.data() + offset
        };

        double* outPtrs[2] {
            output[0].data() + offset,
            output[1].data() + offset
        };

        AudioBusBuffers inBus {};
        inBus.numChannels = 2;
        inBus.channelBuffers64 = inPtrs;

        AudioBusBuffers outBus {};
        outBus.numChannels = 2;
        outBus.channelBuffers64 = outPtrs;

        ProcessData data {};
        data.processMode = mode;
        data.symbolicSampleSize = kSample64;
        data.numSamples = chunk;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;

        BF_REQUIRE(p.process(data) == kResultOk);
        offset += chunk;
    }

    BF_REQUIRE(offset == totalSamples);

    BF_REQUIRE(p.setProcessing(false) == kResultOk);
    BF_REQUIRE(p.setActive(false) == kResultOk);
    BF_REQUIRE(p.terminate() == kResultOk);

    return output;
}

void verifyActivePathBlockAndModeInvariance() {
    const auto whole =
        renderActivePath(
            kRealtime,
            std::vector<int32> {1024});

    const auto chunked =
        renderActivePath(
            kRealtime,
            std::vector<int32> {
                17, 31, 64, 7, 113, 5, 256, 19, 101, 211, 200
            });

    const auto offline =
        renderActivePath(
            kOffline,
            std::vector<int32> {1024});

    for (std::size_t channel = 0; channel < 2; ++channel) {
        BF_REQUIRE(
            whole[channel].size() ==
            chunked[channel].size());

        BF_REQUIRE(
            whole[channel].size() ==
            offline[channel].size());

        for (std::size_t i = 0; i < whole[channel].size(); ++i) {
            BF_REQUIRE(
                whole[channel][i] ==
                chunked[channel][i]);

            BF_REQUIRE(
                whole[channel][i] ==
                offline[channel][i]);
        }
    }
}

void verifyProcessingMatrix() {
    const int32 blockSizes[] {
        1,
        17,
        256,
        1024
    };

    const double sampleRates[] {
        44100.0,
        96000.0
    };

    const ProcessModes processModes[] {
        kRealtime,
        kOffline
    };

    for (const auto processMode : processModes) {
        for (const bool stereo : {false, true}) {
            for (const auto blockSize : blockSizes) {
                for (const auto sampleRate : sampleRates) {
                    runNeutralProcessingCase<float>(
                        kSample32,
                        processMode,
                        stereo,
                        blockSize,
                        sampleRate);

                    runNeutralProcessingCase<double>(
                        kSample64,
                        processMode,
                        stereo,
                        blockSize,
                        sampleRate);
                }
            }
        }
    }
}

}

int main() {
    verifyParameterFlush();
    verifyLifecycleAndBusContracts();
    verifySampleAccurateOutputAutomation();
    verifyActivePathBlockAndModeInvariance();
    verifyProcessingMatrix();
    std::cout << "Bass Finisher processor contracts passed\n";
    return 0;
}
