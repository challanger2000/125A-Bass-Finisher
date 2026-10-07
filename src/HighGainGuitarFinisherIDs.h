#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace HighGainGuitarFinisher {

static const Steinberg::FUID kProcessorUID(
    0x73A6D21F, 0x4C9B48E1, 0xB825F0A3, 0x1D6E97C4);

static const Steinberg::FUID kControllerUID(
    0xA4E17C62, 0x8F3D45B9, 0x96C20E71, 0xD53A8B4F);

enum ParamID : Steinberg::Vst::ParamID {
    kFinish = 100,
    kRoom,
    kOutput,
    kBypass,
    kLowCut80,
    kRoomDecay,
    kMode,
    kMass,
    kDelayWet,
    kDelayFeedback,
    kDelayDivision,
    kToneMatchAmount
};

constexpr Steinberg::int32 kStateVersion = 11;
constexpr Steinberg::int32 kFirstSupportedStateVersion = 1;

} // namespace HighGainGuitarFinisher
