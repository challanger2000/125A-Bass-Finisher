#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace HighGainGuitarFinisher {

static const Steinberg::FUID kProcessorUID(
    0x31B748D2, 0xC82F4E6A, 0x9A5713F1, 0x6D2BC405);

static const Steinberg::FUID kControllerUID(
    0x8E04A91C, 0x57D34B82, 0xB6C1297A, 0xF043DE15);

enum ParamID : Steinberg::Vst::ParamID {
    kFinish = 100,
    kOutput = 102,
    kBypass = 103,
    kLowCut80 = 104,
    kMode = 106,
    kMass = 107,
    kToneMatchAmount = 111
};

// Bass Finisher V1 owns an independent state contract.
constexpr Steinberg::int32 kStateVersion = 4;
constexpr Steinberg::int32 kFirstSupportedStateVersion = 1;

} // namespace HighGainGuitarFinisher
