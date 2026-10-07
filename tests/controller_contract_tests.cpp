#include "TestSupport.h"
#include "HighGainGuitarFinisherController.h"
#include "HighGainGuitarFinisherIDs.h"
#include "ToneMatchStateIO.h"
#include "dsp/LowCutMapping.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"

#include <cmath>
#include <iostream>
#include <string>

using HighGainGuitarFinisher::Controller;
using namespace HighGainGuitarFinisher;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

void rewind(MemoryStream& stream) {
    int64 position = 0;
    BF_REQUIRE(stream.seek(0, IBStream::kIBSeekSet, &position) == kResultOk);
    BF_REQUIRE(position == 0);
}

std::string ascii(const String128 text) {
    std::string result;
    for (std::size_t i = 0; i < 127 && text[i] != 0; ++i)
        result.push_back(static_cast<char>(text[i]));
    return result;
}

void setAscii(String128 text, const char* source) {
    std::size_t i = 0;
    for (; source[i] != 0 && i < 127; ++i)
        text[i] = static_cast<TChar>(static_cast<unsigned char>(source[i]));
    text[i] = 0;
}

void verifyParameterContract() {
    Controller c;
    BF_REQUIRE(c.initialize(nullptr) == kResultOk);
    BF_REQUIRE(c.getParameterCount() == 7);

    const Steinberg::Vst::ParamID expectedIds[] {
        kFinish,
        kOutput,
        kBypass,
        kLowCut80,
        kMode,
        kMass,
        kToneMatchAmount
    };

    const ParamValue expectedDefaults[] {
        0.0,
        0.5,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0
    };

    for (int32 i = 0; i < 7; ++i) {
        ParameterInfo info {};
        BF_REQUIRE(c.getParameterInfo(i, info) == kResultTrue);
        BF_REQUIRE(info.id == expectedIds[i]);
        BF_REQUIRE(std::abs(info.defaultNormalizedValue - expectedDefaults[i]) < 1.0e-12);
    }

    String128 text {};
    BF_REQUIRE(c.getParamStringByValue(kLowCut80, 0.0, text) == kResultTrue);
    BF_REQUIRE(ascii(text) == "Off");

    const double low55 =
        dsp::lowCutNormalizedFromFrequency(55.0);

    BF_REQUIRE(c.getParamStringByValue(kLowCut80, low55, text) == kResultTrue);
    BF_REQUIRE(ascii(text) == "55 Hz");

    String128 input {};
    setAscii(input, "55");
    ParamValue parsed = 0.0;
    BF_REQUIRE(c.getParamValueByString(kLowCut80, input, parsed) == kResultTrue);
    BF_REQUIRE(std::abs(parsed - low55) < 1.0e-12);

    setAscii(input, "PUNCH");
    BF_REQUIRE(c.getParamValueByString(kMode, input, parsed) == kResultTrue);
    BF_REQUIRE(std::abs(parsed - 0.5) < 1.0e-12);

    setAscii(input, "42");
    BF_REQUIRE(c.getParamValueByString(kFinish, input, parsed) == kResultTrue);
    BF_REQUIRE(std::abs(parsed - 0.42) < 1.0e-12);

    BF_REQUIRE(c.terminate() == kResultOk);
}

void verifyControllerZoomState() {
    Controller c;
    BF_REQUIRE(c.initialize(nullptr) == kResultOk);

    MemoryStream incoming;
    IBStreamer writer(&incoming, kLittleEndian);
    BF_REQUIRE(writer.writeDouble(1.5));
    rewind(incoming);
    BF_REQUIRE(c.setState(&incoming) == kResultOk);

    MemoryStream outgoing;
    BF_REQUIRE(c.getState(&outgoing) == kResultOk);
    rewind(outgoing);

    IBStreamer reader(&outgoing, kLittleEndian);
    double zoom = 0.0;
    BF_REQUIRE(reader.readDouble(zoom));
    BF_REQUIRE(std::abs(zoom - 1.5) < 1.0e-12);

    BF_REQUIRE(c.terminate() == kResultOk);
}

void writeValidComponentState(MemoryStream& stream) {
    IBStreamer writer(&stream, kLittleEndian);

    BF_REQUIRE(writer.writeInt32(kStateVersion));

    const double values[6] {
        0.31,
        0.62,
        1.0,
        dsp::lowCutNormalizedFromFrequency(55.0),
        0.5,
        0.44
    };

    for (const double value : values)
        BF_REQUIRE(writer.writeDouble(value));

    ToneMatchStatePayload toneMatch {};
    toneMatch.amount = 0.67;
    BF_REQUIRE(writeToneMatchState(writer, toneMatch));

    dsp::ToneMatchSpectrumSnapshot reference {};
    BF_REQUIRE(writeToneMatchReferenceState(writer, reference));
}

void verifyComponentStateContract() {
    Controller c;
    BF_REQUIRE(c.initialize(nullptr) == kResultOk);

    MemoryStream valid;
    writeValidComponentState(valid);
    rewind(valid);

    BF_REQUIRE(c.setComponentState(&valid) == kResultOk);
    BF_REQUIRE(std::abs(c.getParamNormalized(kFinish) - 0.31) < 1.0e-12);
    BF_REQUIRE(std::abs(c.getParamNormalized(kOutput) - 0.62) < 1.0e-12);
    BF_REQUIRE(std::abs(c.getParamNormalized(kBypass) - 1.0) < 1.0e-12);
    BF_REQUIRE(std::abs(
        c.getParamNormalized(kLowCut80) -
        dsp::lowCutNormalizedFromFrequency(55.0)) < 1.0e-12);
    BF_REQUIRE(std::abs(c.getParamNormalized(kMode) - 0.5) < 1.0e-12);
    BF_REQUIRE(std::abs(c.getParamNormalized(kMass) - 0.44) < 1.0e-12);
    BF_REQUIRE(std::abs(c.getParamNormalized(kToneMatchAmount) - 0.67) < 1.0e-12);

    const auto finishBefore =
        c.getParamNormalized(kFinish);

    MemoryStream truncated;
    IBStreamer badWriter(&truncated, kLittleEndian);
    BF_REQUIRE(badWriter.writeInt32(kStateVersion));
    BF_REQUIRE(badWriter.writeDouble(0.99));
    rewind(truncated);

    BF_REQUIRE(c.setComponentState(&truncated) == kResultFalse);
    BF_REQUIRE(std::abs(c.getParamNormalized(kFinish) - finishBefore) < 1.0e-12);

    BF_REQUIRE(c.terminate() == kResultOk);
}

}

int main() {
    verifyParameterContract();
    verifyControllerZoomState();
    verifyComponentStateContract();

    std::cout << "Bass Finisher controller contracts passed\n";
    return 0;
}
