#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "dsp/MetalFinisherDSP.h"
#include "dsp/ToneMatchProfile.h"
#include "dsp/ToneMatchProfileMailbox.h"
#include "dsp/ToneMatchCaptureBuffer.h"
#include "dsp/ToneMatchCaptureWorker.h"
#include "BypassCrossfade.h"

#include <array>
#include <atomic>

namespace HighGainGuitarFinisher {

class Processor : public Steinberg::Vst::AudioEffect {
public:
    Processor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor());
    }

    Steinberg::tresult PLUGIN_API initialize(
        Steinberg::FUnknown* context) override;

    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs,
        Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs,
        Steinberg::int32 numOuts) override;

    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize) override;

    Steinberg::uint32 PLUGIN_API getTailSamples() override;

    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup& setup) override;

    Steinberg::tresult PLUGIN_API setActive(
        Steinberg::TBool state) override;

    Steinberg::tresult PLUGIN_API setProcessing(
        Steinberg::TBool state) override;

    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData& data) override;

    Steinberg::tresult PLUGIN_API setState(
        Steinberg::IBStream* state) override;

    Steinberg::tresult PLUGIN_API getState(
        Steinberg::IBStream* state) override;

    Steinberg::tresult PLUGIN_API notify(
        Steinberg::Vst::IMessage* message) override;

private:
    static constexpr std::size_t kAutomatedParameterCount = 7;

    struct AutomationCursor {
        Steinberg::Vst::IParamValueQueue* queue {nullptr};
        Steinberg::int32 pointIndex {0};
        Steinberg::int32 pointCount {0};
        Steinberg::int32 segmentStartOffset {-1};
        Steinberg::Vst::ParamValue segmentStartValue {0.0};
        Steinberg::int32 nextSampleOffset {-1};
        Steinberg::Vst::ParamValue nextValue {0.0};
        Steinberg::Vst::ParamID id {0};
        bool discrete {false};
        bool hasNext {false};
    };

    Steinberg::Vst::ParamValue currentParameterValue(
        Steinberg::Vst::ParamID id) const noexcept;

    void applyParameterValue(
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::ParamValue value) noexcept;

    void readLastParameterChanges(
        Steinberg::Vst::IParameterChanges* changes) noexcept;

    void initializeAutomationCursors(
        Steinberg::Vst::IParameterChanges* changes,
        std::array<AutomationCursor, kAutomatedParameterCount>& cursors) noexcept;

    bool applyAutomationAtSample(
        std::array<AutomationCursor, kAutomatedParameterCount>& cursors,
        Steinberg::int32 sampleOffset,
        bool& outputPathChanged) noexcept;

    void syncDSPParameters() noexcept;
    void stopToneMatchCaptureWorker() noexcept;

    template <typename Sample>
    Steinberg::uint64 processBlock(
        Sample** inputs,
        Sample** outputs,
        Steinberg::int32 numSamples,
        Steinberg::int32 numChannels,
        Steinberg::Vst::IParameterChanges* parameterChanges);

    dsp::MetalFinisherDSP finisher_ {};

    double sampleRate_ {44100.0};
    double finish_ {0.0};
    double output_ {0.5};
    double bypass_ {0.0};
    double lowCut_ {0.0};
    double mode_ {0.0};
    double mass_ {0.0};
    double toneMatchAmount_ {0.0};

    dsp::ToneMatchProfile toneMatchProfile_ {};
    dsp::ToneMatchSpectrumSnapshot toneMatchReferenceSpectrum_ {};
    dsp::ToneMatchProfileMailbox toneMatchMailbox_ {};
    dsp::ToneMatchCaptureBuffer toneMatchCapture_ {};
    dsp::ToneMatchCaptureWorker toneMatchCaptureWorker_ {};
    std::atomic<bool> toneMatchCaptureActive_ {false};
    std::atomic<unsigned int> toneMatchCaptureWriters_ {0u};

    BypassCrossfade bypassCrossfade_ {};
    bool lastBypassed_ {false};
    bool bypassDSPDormant_ {false};
};

} // namespace HighGainGuitarFinisher
