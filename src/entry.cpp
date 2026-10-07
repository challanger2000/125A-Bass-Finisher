#include "public.sdk/source/main/pluginfactory.h"

#include "HighGainGuitarFinisherController.h"
#include "HighGainGuitarFinisherIDs.h"
#include "HighGainGuitarFinisherProcessor.h"

#define stringPluginName "125A High Gain Guitar Finisher V3"
#define stringPluginVersion "3.0.0"

BEGIN_FACTORY_DEF(
    "125A",
    "https://github.com/challanger2000/125A-High-Gain-Guitar-Finisher-Final",
    "")

DEF_CLASS2(
    INLINE_UID_FROM_FUID(HighGainGuitarFinisher::kProcessorUID),
    Steinberg::PClassInfo::kManyInstances,
    kVstAudioEffectClass,
    stringPluginName,
    Steinberg::Vst::kDistributable,
    "Fx|Dynamics",
    stringPluginVersion,
    kVstVersionString,
    HighGainGuitarFinisher::Processor::createInstance)

DEF_CLASS2(
    INLINE_UID_FROM_FUID(HighGainGuitarFinisher::kControllerUID),
    Steinberg::PClassInfo::kManyInstances,
    kVstComponentControllerClass,
    "125A High Gain Guitar Finisher V3 Controller",
    0,
    "",
    stringPluginVersion,
    kVstVersionString,
    HighGainGuitarFinisher::Controller::createInstance)

END_FACTORY
