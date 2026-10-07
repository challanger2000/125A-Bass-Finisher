#include "TestSupport.h"
#include "HighGainGuitarFinisherProcessor.h"
#include "HighGainGuitarFinisherIDs.h"
#include "ToneMatchStateIO.h"
#include "dsp/LowCutMapping.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

#if defined(_WIN32)
#include <malloc.h>
#endif


namespace {
bool gTrackProcessorAllocations = false;
std::size_t gProcessorAllocationCount = 0;

void noteProcessorAllocation() noexcept {
    if (gTrackProcessorAllocations)
        ++gProcessorAllocationCount;
}
}

void* operator new(std::size_t size) {
    noteProcessorAllocation();
    if (void* ptr = std::malloc(size))
        return ptr;
    throw std::bad_alloc {};
}

void* operator new[](std::size_t size) {
    noteProcessorAllocation();
    if (void* ptr = std::malloc(size))
        return ptr;
    throw std::bad_alloc {};
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

#if defined(__cpp_aligned_new)
void* operator new(std::size_t size, std::align_val_t alignment) {
    noteProcessorAllocation();
#if defined(_WIN32)
    if (void* ptr = _aligned_malloc(
            size,
            static_cast<std::size_t>(alignment))) {
        return ptr;
    }
#else
    void* ptr = nullptr;
    if (posix_memalign(
            &ptr,
            static_cast<std::size_t>(alignment),
            size) == 0) {
        return ptr;
    }
#endif
    throw std::bad_alloc {};
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void operator delete(void* ptr, std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept {
    ::operator delete(ptr, alignment);
}

void operator delete(
    void* ptr,
    std::size_t,
    std::align_val_t alignment) noexcept {
    ::operator delete(ptr, alignment);
}

void operator delete[](
    void* ptr,
    std::size_t,
    std::align_val_t alignment) noexcept {
    ::operator delete(ptr, alignment);
}
#endif

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


class EmptyEventList final :
    public Steinberg::Vst::IEventList {
public:
    Steinberg::int32 PLUGIN_API getEventCount() override {
        return 0;
    }

    Steinberg::tresult PLUGIN_API getEvent(
        Steinberg::int32,
        Steinberg::Vst::Event&) override {
        return Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API addEvent(
        Steinberg::Vst::Event&) override {
        return Steinberg::kResultFalse;
    }

    Steinberg::tresult PLUGIN_API queryInterface(
        const Steinberg::TUID iid,
        void** obj) override {

        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
                iid,
                Steinberg::Vst::IEventList::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(
                iid,
                Steinberg::FUnknown::iid)) {

            *obj =
                static_cast<
                    Steinberg::Vst::IEventList*>(
                        this);

            addRef();
            return Steinberg::kResultTrue;
        }

        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override {
        return 1000;
    }

    Steinberg::uint32 PLUGIN_API release() override {
        return 1000;
    }
};

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
    BF_REQUIRE(p.getLatencySamples() == 0u);

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
    auto* queue = changes.addParameterData(HighGainGuitarFinisher::kOutput, queueIndex);
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
    addChange(settings, HighGainGuitarFinisher::kFinish, 0.67);
    addChange(
        settings,
        HighGainGuitarFinisher::kLowCut80,
        dsp::lowCutNormalizedFromFrequency(55.0));
    addChange(settings, HighGainGuitarFinisher::kMode, 0.5);
    addChange(settings, HighGainGuitarFinisher::kMass, 0.72);

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


void verifyIoEventAndTortureContracts() {
    Processor p;
    BF_REQUIRE(p.initialize(nullptr) == kResultOk);

    BF_REQUIRE(
        p.getBusCount(
            kAudio,
            Steinberg::Vst::kInput) ==
        1);

    BF_REQUIRE(
        p.getBusCount(
            kAudio,
            Steinberg::Vst::kOutput) ==
        1);

    BF_REQUIRE(
        p.getBusCount(
            kEvent,
            Steinberg::Vst::kInput) ==
        0);

    BF_REQUIRE(
        p.getBusCount(
            kEvent,
            Steinberg::Vst::kOutput) ==
        0);

    SpeakerArrangement stereo[1] {
        SpeakerArr::kStereo
    };

    BF_REQUIRE(
        p.setBusArrangements(
            stereo,
            1,
            stereo,
            1) ==
        kResultOk);

    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = 1024;
    setup.sampleRate = kFs;

    BF_REQUIRE(
        p.setupProcessing(setup) ==
        kResultOk);

    BF_REQUIRE(
        p.setActive(true) ==
        kResultOk);

    BF_REQUIRE(
        p.setProcessing(true) ==
        kResultOk);

    EmptyEventList inputEvents;
    EmptyEventList outputEvents;

    constexpr int32 maxSamples = 1024;

    std::array<double, maxSamples> inL {};
    std::array<double, maxSamples> inR {};
    std::array<double, maxSamples> outL {};
    std::array<double, maxSamples> outR {};

    double* inPtrs[2] {
        inL.data(),
        inR.data()
    };

    double* outPtrs[2] {
        outL.data(),
        outR.data()
    };

    AudioBusBuffers inBus {};
    inBus.numChannels = 2;
    inBus.channelBuffers64 = inPtrs;

    AudioBusBuffers outBus {};
    outBus.numChannels = 2;
    outBus.channelBuffers64 = outPtrs;

    const int32 blockSizes[] {
        1,
        3,
        17,
        64,
        255,
        1024
    };

    for (int cycle = 0;
         cycle < 24;
         ++cycle) {

        const int32 block =
            blockSizes[
                cycle %
                static_cast<int>(
                    std::size(
                        blockSizes))];

        const double tiny =
            std::numeric_limits<
                double>::denorm_min() *
            512.0;

        for (int32 i = 0;
             i < block;
             ++i) {

            const double t =
                static_cast<double>(
                    cycle * maxSamples + i) /
                kFs;

            const bool subnormalCycle =
                (cycle % 5) == 0;

            inL[static_cast<std::size_t>(i)] =
                subnormalCycle
                    ? tiny
                    : 0.19 *
                        std::sin(
                            2.0 *
                            3.14159265358979323846 *
                            73.0 *
                            t +
                            0.21);

            inR[static_cast<std::size_t>(i)] =
                subnormalCycle
                    ? -tiny
                    : 0.17 *
                        std::sin(
                            2.0 *
                            3.14159265358979323846 *
                            109.0 *
                            t +
                            0.47);

            outL[static_cast<std::size_t>(i)] = 99.0;
            outR[static_cast<std::size_t>(i)] = 99.0;
        }

        ParameterChanges changes(2);

        addChange(
            changes,
            HighGainGuitarFinisher::kFinish,
            (cycle % 3) == 0
                ? 0.0
                : 0.71);

        addChange(
            changes,
            HighGainGuitarFinisher::kMass,
            (cycle % 4) == 0
                ? 0.0
                : 0.63);

        ProcessData data {};
        data.processMode =
            (cycle % 2) == 0
                ? kRealtime
                : kOffline;

        data.symbolicSampleSize = kSample64;
        data.numSamples = block;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;
        data.inputParameterChanges = &changes;
        data.inputEvents = &inputEvents;
        data.outputEvents = &outputEvents;

        BF_REQUIRE(
            p.process(data) ==
            kResultOk);

        BF_REQUIRE(
            inputEvents.getEventCount() ==
            0);

        BF_REQUIRE(
            outputEvents.getEventCount() ==
            0);

        for (int32 i = 0;
             i < block;
             ++i) {

            BF_REQUIRE(
                std::isfinite(
                    outL[
                        static_cast<
                            std::size_t>(i)]));

            BF_REQUIRE(
                std::isfinite(
                    outR[
                        static_cast<
                            std::size_t>(i)]));
        }

        if ((cycle % 6) == 5) {
            BF_REQUIRE(
                p.setProcessing(false) ==
                kResultOk);

            BF_REQUIRE(
                p.setActive(false) ==
                kResultOk);

            BF_REQUIRE(
                p.setActive(true) ==
                kResultOk);

            BF_REQUIRE(
                p.setProcessing(true) ==
                kResultOk);
        }
    }

    ParameterChanges flushChanges(1);

    addChange(
        flushChanges,
        HighGainGuitarFinisher::kOutput,
        0.58);

    ProcessData noAudio {};
    noAudio.processMode = kRealtime;
    noAudio.symbolicSampleSize = kSample64;
    noAudio.numSamples = 0;
    noAudio.numInputs = 0;
    noAudio.numOutputs = 0;
    noAudio.inputParameterChanges =
        &flushChanges;
    noAudio.inputEvents =
        &inputEvents;
    noAudio.outputEvents =
        &outputEvents;

    BF_REQUIRE(
        p.process(noAudio) ==
        kResultOk);

    const auto state =
        readCoreState(p);

    BF_REQUIRE(
        std::abs(
            state[1] -
            0.58) <
        1.0e-12);

    BF_REQUIRE(
        p.setProcessing(false) ==
        kResultOk);

    BF_REQUIRE(
        p.setActive(false) ==
        kResultOk);

    BF_REQUIRE(
        p.terminate() ==
        kResultOk);
}


void writeAudioRecallState(MemoryStream& stream) {
    IBStreamer writer(&stream, kLittleEndian);

    BF_REQUIRE(
        writer.writeInt32(
            kStateVersion));

    const double values[6] {
        0.63,
        0.57,
        0.0,
        dsp::lowCutNormalizedFromFrequency(
            55.0),
        0.5,
        0.48
    };

    for (const double value : values)
        BF_REQUIRE(
            writer.writeDouble(
                value));

    ToneMatchStatePayload match {};
    match.amount = 0.72;
    match.profile.valid = true;
    match.profile.lowShelfFrequencyHz = 72.0;
    match.profile.lowShelfGainDb = 1.1;
    match.profile.highShelfFrequencyHz = 6200.0;
    match.profile.highShelfGainDb = -0.9;

    for (std::size_t i = 0;
         i < match.profile.peaks.size();
         ++i) {
        match.profile.peaks[i].frequencyHz =
            120.0 +
            310.0 *
                static_cast<double>(i);
        match.profile.peaks[i].q =
            0.85 +
            0.03 *
                static_cast<double>(i);
        match.profile.peaks[i].gainDb =
            -0.8 +
            0.12 *
                static_cast<double>(i);
    }

    BF_REQUIRE(
        writeToneMatchState(
            writer,
            match));

    dsp::ToneMatchSpectrumSnapshot reference {};
    reference.sampleRate = kFs;
    reference.frameCount = 8u;

    for (std::size_t i = 0;
         i < reference.meanPower.size();
         ++i) {
        reference.meanPower[i] =
            1.0e-6 +
            1.0e-9 *
                static_cast<double>(i);
    }

    BF_REQUIRE(
        writeToneMatchReferenceState(
            writer,
            reference));
}

std::array<std::vector<double>, 2>
renderStateRecallProcessor(
    Processor& processor) {

    constexpr int32 block = 256;
    constexpr int32 total = 4096;

    SpeakerArrangement stereo[1] {
        SpeakerArr::kStereo
    };

    BF_REQUIRE(
        processor.setBusArrangements(
            stereo,
            1,
            stereo,
            1) ==
        kResultOk);

    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = block;
    setup.sampleRate = kFs;

    BF_REQUIRE(
        processor.setupProcessing(
            setup) ==
        kResultOk);

    BF_REQUIRE(
        processor.setActive(true) ==
        kResultOk);

    BF_REQUIRE(
        processor.setProcessing(true) ==
        kResultOk);

    std::array<std::vector<double>, 2> rendered {
        std::vector<double>(
            static_cast<std::size_t>(total)),
        std::vector<double>(
            static_cast<std::size_t>(total))
    };

    std::array<double, block> inL {};
    std::array<double, block> inR {};
    std::array<double, block> outL {};
    std::array<double, block> outR {};

    for (int32 offset = 0;
         offset < total;
         offset += block) {

        for (int32 i = 0;
             i < block;
             ++i) {
            const double t =
                static_cast<double>(
                    offset + i) /
                kFs;

            inL[static_cast<std::size_t>(i)] =
                0.24 *
                    std::sin(
                        2.0 *
                        3.14159265358979323846 *
                        55.0 * t +
                        0.23) +
                0.08 *
                    std::sin(
                        2.0 *
                        3.14159265358979323846 *
                        880.0 * t +
                        0.41);

            inR[static_cast<std::size_t>(i)] =
                0.22 *
                    std::sin(
                        2.0 *
                        3.14159265358979323846 *
                        61.74 * t +
                        0.37) +
                0.07 *
                    std::sin(
                        2.0 *
                        3.14159265358979323846 *
                        1450.0 * t +
                        0.67);
        }

        double* inputs[2] {
            inL.data(),
            inR.data()
        };

        double* outputs[2] {
            outL.data(),
            outR.data()
        };

        AudioBusBuffers inBus {};
        inBus.numChannels = 2;
        inBus.channelBuffers64 = inputs;

        AudioBusBuffers outBus {};
        outBus.numChannels = 2;
        outBus.channelBuffers64 = outputs;

        ProcessData data {};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample64;
        data.numSamples = block;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;

        BF_REQUIRE(
            processor.process(data) ==
            kResultOk);

        for (int32 i = 0;
             i < block;
             ++i) {
            rendered[0][
                static_cast<std::size_t>(
                    offset + i)] =
                outL[
                    static_cast<std::size_t>(i)];

            rendered[1][
                static_cast<std::size_t>(
                    offset + i)] =
                outR[
                    static_cast<std::size_t>(i)];
        }
    }

    BF_REQUIRE(
        processor.setProcessing(false) ==
        kResultOk);

    BF_REQUIRE(
        processor.setActive(false) ==
        kResultOk);

    return rendered;
}

void verifyStateRecallRestoresAudioResult() {
    Processor source;
    Processor restored;

    BF_REQUIRE(
        source.initialize(nullptr) ==
        kResultOk);

    BF_REQUIRE(
        restored.initialize(nullptr) ==
        kResultOk);

    MemoryStream initial;
    writeAudioRecallState(initial);
    rewind(initial);

    BF_REQUIRE(
        source.setState(&initial) ==
        kResultOk);

    MemoryStream saved;

    BF_REQUIRE(
        source.getState(&saved) ==
        kResultOk);

    rewind(saved);

    BF_REQUIRE(
        restored.setState(&saved) ==
        kResultOk);

    const auto sourceAudio =
        renderStateRecallProcessor(
            source);

    const auto restoredAudio =
        renderStateRecallProcessor(
            restored);

    for (std::size_t channel = 0;
         channel < sourceAudio.size();
         ++channel) {

        BF_REQUIRE(
            sourceAudio[channel].size() ==
            restoredAudio[channel].size());

        for (std::size_t i = 0;
             i < sourceAudio[channel].size();
             ++i) {

            BF_REQUIRE(
                std::abs(
                    sourceAudio[channel][i] -
                    restoredAudio[channel][i]) <
                1.0e-12);
        }
    }

    BF_REQUIRE(
        source.terminate() ==
        kResultOk);

    BF_REQUIRE(
        restored.terminate() ==
        kResultOk);
}

void verifyProcessorCallbackHasNoAllocations() {
    Processor processor;

    BF_REQUIRE(
        processor.initialize(nullptr) ==
        kResultOk);

    SpeakerArrangement stereo[1] {
        SpeakerArr::kStereo
    };

    BF_REQUIRE(
        processor.setBusArrangements(
            stereo,
            1,
            stereo,
            1) ==
        kResultOk);

    ProcessSetup setup {};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample64;
    setup.maxSamplesPerBlock = kBlock;
    setup.sampleRate = kFs;

    BF_REQUIRE(
        processor.setupProcessing(
            setup) ==
        kResultOk);

    BF_REQUIRE(
        processor.setActive(true) ==
        kResultOk);

    BF_REQUIRE(
        processor.setProcessing(true) ==
        kResultOk);

    ParameterChanges settings(5);
    addChange(
        settings,
        HighGainGuitarFinisher::kFinish,
        1.0);
    addChange(
        settings,
        HighGainGuitarFinisher::kLowCut80,
        dsp::lowCutNormalizedFromFrequency(
            55.0));
    addChange(
        settings,
        HighGainGuitarFinisher::kMode,
        0.5);
    addChange(
        settings,
        HighGainGuitarFinisher::kMass,
        0.72);
    addChange(
        settings,
        HighGainGuitarFinisher::kOutput,
        0.5);

    ProcessData flush {};
    flush.processMode = kRealtime;
    flush.symbolicSampleSize = kSample64;
    flush.numSamples = 0;
    flush.inputParameterChanges = &settings;

    BF_REQUIRE(
        processor.process(flush) ==
        kResultOk);

    std::array<double, kBlock> inL {};
    std::array<double, kBlock> inR {};
    std::array<double, kBlock> outL {};
    std::array<double, kBlock> outR {};

    double* inputs[2] {
        inL.data(),
        inR.data()
    };

    double* outputs[2] {
        outL.data(),
        outR.data()
    };

    AudioBusBuffers inBus {};
    inBus.numChannels = 2;
    inBus.channelBuffers64 = inputs;

    AudioBusBuffers outBus {};
    outBus.numChannels = 2;
    outBus.channelBuffers64 = outputs;

    ProcessData data {};
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample64;
    data.numSamples = kBlock;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inBus;
    data.outputs = &outBus;

    auto fillBlock =
        [&](int blockIndex) {
            for (int32 i = 0;
                 i < kBlock;
                 ++i) {
                const double t =
                    static_cast<double>(
                        blockIndex * kBlock + i) /
                    kFs;

                inL[static_cast<std::size_t>(i)] =
                    0.27 *
                        std::sin(
                            2.0 *
                            3.14159265358979323846 *
                            55.0 * t +
                            0.19) +
                    0.09 *
                        std::sin(
                            2.0 *
                            3.14159265358979323846 *
                            1800.0 * t +
                            0.43);

                inR[static_cast<std::size_t>(i)] =
                    0.25 *
                        std::sin(
                            2.0 *
                            3.14159265358979323846 *
                            61.74 * t +
                            0.31) +
                    0.08 *
                        std::sin(
                            2.0 *
                            3.14159265358979323846 *
                            2300.0 * t +
                            0.71);
            }
        };

    for (int blockIndex = 0;
         blockIndex < 16;
         ++blockIndex) {
        fillBlock(blockIndex);
        BF_REQUIRE(
            processor.process(data) ==
            kResultOk);
    }

    gProcessorAllocationCount = 0;
    gTrackProcessorAllocations = true;

    bool processOk = true;
    double checksum = 0.0;

    for (int blockIndex = 16;
         blockIndex < 144;
         ++blockIndex) {
        fillBlock(blockIndex);

        if (processor.process(data) !=
            kResultOk) {
            processOk = false;
            break;
        }

        checksum +=
            outL[0] +
            outR[kBlock - 1];
    }

    gTrackProcessorAllocations = false;

    BF_REQUIRE(processOk);
    BF_REQUIRE(std::isfinite(checksum));
    BF_REQUIRE(
        gProcessorAllocationCount ==
        0u);

    BF_REQUIRE(
        processor.setProcessing(false) ==
        kResultOk);

    BF_REQUIRE(
        processor.setActive(false) ==
        kResultOk);

    BF_REQUIRE(
        processor.terminate() ==
        kResultOk);
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
    verifyIoEventAndTortureContracts();
    verifyStateRecallRestoresAudioResult();
    verifyProcessorCallbackHasNoAllocations();
    verifyProcessingMatrix();
    std::cout << "Bass Finisher processor contracts passed\n";
    return 0;
}
