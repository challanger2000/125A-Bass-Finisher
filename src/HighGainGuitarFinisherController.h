#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "dsp/ToneMatchProfile.h"
#include "dsp/ToneMatchAnalyzer.h"
#include "ToneMatchStatus.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace VSTGUI {
class CControl;
class CFrame;
class COptionMenu;
class CTextButton;
class CTextLabel;
}

namespace HighGainGuitarFinisher {

class Controller :
    public Steinberg::Vst::EditController,
    public VSTGUI::VST3EditorDelegate,
    public VSTGUI::IControlListener {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<
            Steinberg::Vst::IEditController*>(
                new Controller());
    }

    Steinberg::tresult PLUGIN_API initialize(
        Steinberg::FUnknown* context) override;

    Steinberg::tresult PLUGIN_API terminate() override;

    Steinberg::tresult PLUGIN_API setComponentState(
        Steinberg::IBStream* state) override;

    Steinberg::tresult PLUGIN_API setState(
        Steinberg::IBStream* state) override;

    Steinberg::tresult PLUGIN_API getState(
        Steinberg::IBStream* state) override;

    Steinberg::tresult PLUGIN_API notify(
        Steinberg::Vst::IMessage* message) override;

    Steinberg::IPlugView* PLUGIN_API createView(
        Steinberg::FIDString name) override;

    Steinberg::tresult PLUGIN_API getParamStringByValue(
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::ParamValue valueNormalized,
        Steinberg::Vst::String128 string) override;

    Steinberg::tresult PLUGIN_API getParamValueByString(
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::TChar* string,
        Steinberg::Vst::ParamValue& valueNormalized) override;

    VSTGUI::CView* createCustomView(
        VSTGUI::UTF8StringPtr name,
        const VSTGUI::UIAttributes& attributes,
        const VSTGUI::IUIDescription* description,
        VSTGUI::VST3Editor* editor) override;

    VSTGUI::CView* verifyView(
        VSTGUI::CView* view,
        const VSTGUI::UIAttributes& attributes,
        const VSTGUI::IUIDescription* description,
        VSTGUI::VST3Editor* editor) override;

    void valueChanged(
        VSTGUI::CControl* control) override;

    void willClose(
        VSTGUI::VST3Editor* editor) override;

    Steinberg::tresult sendToneMatchProfile(
        const dsp::ToneMatchProfile& profile);

    Steinberg::tresult startToneMatchCapture();
    Steinberg::tresult stopToneMatchCapture();

    bool loadToneMatchReferenceAudio(
        const std::filesystem::path& path);

    bool loadToneMatchReferenceProfile(
        const std::filesystem::path& path);

    bool saveToneMatchReferenceProfile(
        const std::filesystem::path& path);

    void clearToneMatchTarget();
    void clearToneMatchReference();

    ToneMatchStatus toneMatchStatus() const noexcept {
        return toneMatchStatus_;
    }

    const std::string& toneMatchLastError() const noexcept {
        return toneMatchLastError_;
    }

    bool toneMatchReferenceReady() const noexcept {
        return toneMatchReferenceReady_;
    }

    bool toneMatchTargetReady() const noexcept {
        return toneMatchTargetReady_;
    }

    void setToneMatchReferenceSpectrum(
        const dsp::ToneMatchSpectrumSnapshot& snapshot);

    void setToneMatchTargetSpectrum(
        const dsp::ToneMatchSpectrumSnapshot& snapshot);

private:
    Steinberg::tresult sendToneMatchReferenceSpectrum(
        const dsp::ToneMatchSpectrumSnapshot& snapshot);
    void tryBuildToneMatchProfile();
    void invalidateToneMatchBuild() noexcept;
    void joinToneMatchBuildWorker() noexcept;
    void launchQueuedToneMatchBuild();
    void pollToneMatchBuild();
    void updateToneMatchGui();
    void chooseToneMatchReference(
        VSTGUI::CFrame* frame);
    void chooseToneMatchReferenceProfile(
        VSTGUI::CFrame* frame);
    void chooseToneMatchSaveProfile(
        VSTGUI::CFrame* frame);

    double guiZoom_ {1.0};
    dsp::ToneMatchSpectrumSnapshot toneMatchReferenceSpectrum_ {};
    dsp::ToneMatchSpectrumSnapshot toneMatchTargetSpectrum_ {};
    bool toneMatchReferenceReady_ {false};
    bool toneMatchTargetReady_ {false};
    dsp::ToneMatchProfile activeToneMatchProfile_ {};
    ToneMatchStatus toneMatchStatus_ {ToneMatchStatus::Empty};
    std::string toneMatchLastError_ {};

    std::thread toneMatchBuildWorker_ {};
    std::atomic<bool> toneMatchBuildDone_ {false};
    std::atomic<bool> toneMatchBuildRunning_ {false};
    std::atomic<std::uint64_t> toneMatchBuildGeneration_ {0};
    std::mutex toneMatchBuildMutex_ {};
    dsp::ToneMatchProfile pendingToneMatchProfile_ {};
    bool pendingToneMatchProfileValid_ {false};
    std::uint64_t pendingToneMatchProfileGeneration_ {0};
    dsp::ToneMatchSpectrumSnapshot queuedToneMatchReference_ {};
    dsp::ToneMatchSpectrumSnapshot queuedToneMatchTarget_ {};
    std::uint64_t queuedToneMatchGeneration_ {0};
    bool toneMatchBuildQueued_ {false};
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> toneMatchBuildTimer_ {};

    VSTGUI::VST3Editor* editor_ {nullptr};
    VSTGUI::CTextButton* toneMatchLoadButton_ {nullptr};
    VSTGUI::CTextButton* toneMatchAnalyzeButton_ {nullptr};
    VSTGUI::COptionMenu* toneMatchMenu_ {nullptr};
    VSTGUI::CTextLabel* toneMatchStatusLabel_ {nullptr};
};

} // namespace HighGainGuitarFinisher
