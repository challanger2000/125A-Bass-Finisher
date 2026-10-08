#include "HighGainGuitarFinisherController.h"
#include "HighGainGuitarFinisherIDs.h"
#include "BrandLogoView.h"
#include "SteelKnob.h"
#include "SteelPanelView.h"
#include "dsp/LowCutMapping.h"
#include "ToneMatchStateIO.h"
#include "ToneMatchMessage.h"
#include "ToneMatchReferenceService.h"
#include "pluginterfaces/vst/ivstmessage.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace HighGainGuitarFinisher {

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

constexpr int32 kGuiZoomTag =
    9000;

constexpr int32 kToneMatchLoadTag =
    9100;

constexpr int32 kToneMatchAnalyzeTag =
    9101;

constexpr int32 kToneMatchMenuTag =
    9102;

constexpr int32 kToneMatchStatusTag =
    9103;

void copyAscii(
    const std::string& text,
    String128 destination) {

    std::size_t i = 0;

    for (;
         i < text.size() &&
         i < 127;
         ++i) {

        destination[i] =
            static_cast<TChar>(
                static_cast<
                    unsigned char>(
                        text[i]));
    }

    destination[i] = 0;
}

bool asciiEqualsIgnoreCase(
    const TChar* text,
    const char* ascii) noexcept {

    if (!text ||
        !ascii) {
        return false;
    }

    std::size_t index = 0;

    for (;; ++index) {
        const TChar tc =
            text[index];

        const unsigned char ac =
            static_cast<unsigned char>(
                ascii[index]);

        if (tc == 0 ||
            ac == 0) {
            return tc == 0 &&
                ac == 0;
        }

        auto fold =
            [](unsigned int c) {
                return
                    c >= 'A' &&
                    c <= 'Z'
                        ? c + ('a' - 'A')
                        : c;
            };

        if (fold(
                static_cast<unsigned int>(
                    tc)) !=
            fold(ac)) {
            return false;
        }
    }
}

std::string percentText(
    ParamValue value) {

    char buffer[32] {};

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.0f %%",
        std::clamp(
            value,
            0.0,
            1.0) *
            100.0);

    return buffer;
}

std::string outputText(
    ParamValue value) {

    const double dB =
        std::clamp(
            value,
            0.0,
            1.0) *
            24.0 -
        12.0;

    char buffer[32] {};

    std::snprintf(
        buffer,
        sizeof(buffer),
        "%+.1f dB",
        dB);

    return buffer;
}

class LowCutParameter final :
    public Parameter {
public:
    LowCutParameter()
    : Parameter(
        STR16("Low Control"),
        kLowCut80,
        STR16("Hz"),
        0.0,
        kStepCountContinuous,
        ParameterInfo::kCanAutomate) {
    }

    void toString(
        ParamValue normalizedValue,
        String128 string) const
        SMTG_OVERRIDE {

        UString128 result;

        if (!dsp::lowCutEnabled(
                normalizedValue)) {

            result.fromAscii(
                "Off");
        } else {
            result.printFloat(
                dsp::
                    lowCutFrequencyFromNormalized(
                        normalizedValue),
                0);
            result.append(
                STR16(" Hz"));
        }

        result.copyTo(
            string,
            128);
    }

    bool fromString(
        const TChar* string,
        ParamValue& normalizedResult) const
        SMTG_OVERRIDE {

        if (!string)
            return false;

        if (asciiEqualsIgnoreCase(
                string,
                "Off")) {

            normalizedResult = 0.0;
            return true;
        }

        UString value(
            const_cast<TChar*>(
                string),
            strlen16(string));

        ParamValue frequency =
            0.0;

        if (!value.scanFloat(
                frequency)) {
            return false;
        }

        normalizedResult =
            dsp::
                lowCutNormalizedFromFrequency(
                    frequency);

        return true;
    }

    ParamValue toPlain(
        ParamValue normalizedValue) const
        SMTG_OVERRIDE {

        return dsp::lowCutEnabled(
                   normalizedValue)
            ? dsp::
                lowCutFrequencyFromNormalized(
                    normalizedValue)
            : 0.0;
    }

    ParamValue toNormalized(
        ParamValue plainValue) const
        SMTG_OVERRIDE {

        return dsp::
            lowCutNormalizedFromFrequency(
                plainValue);
    }

    OBJ_METHODS(
        LowCutParameter,
        Parameter)
};

}

tresult PLUGIN_API Controller::initialize(
    FUnknown* context) {

    const auto result =
        EditController::initialize(
            context);

    if (result !=
        kResultOk) {
        return result;
    }

    constexpr int32 automate =
        ParameterInfo::kCanAutomate;

    parameters.addParameter(
        STR16("Finish"),
        STR16("%"),
        0,
        0.0,
        automate,
        kFinish);

    parameters.addParameter(
        new RangeParameter(
            STR16("Output"),
            kOutput,
            STR16("dB"),
            -12.0,
            12.0,
            0.0));

    parameters.addParameter(
        STR16("Bypass"),
        STR16(""),
        1,
        0.0,
        ParameterInfo::kCanAutomate |
            ParameterInfo::kIsBypass,
        kBypass);

    parameters.addParameter(
        new LowCutParameter());

    parameters.addParameter(
        STR16("Profile"),
        STR16(""),
        2,
        0.0,
        automate,
        kMode);

    parameters.addParameter(
        STR16("Mass"),
        STR16("%"),
        0,
        0.0,
        automate,
        kMass);

    parameters.addParameter(
        STR16("Match"),
        STR16("%"),
        0,
        0.0,
        automate,
        kToneMatchAmount);

    return kResultOk;
}

tresult PLUGIN_API Controller::terminate() {
    invalidateToneMatchBuild();
    joinToneMatchBuildWorker();
    toneMatchBuildTimer_ = nullptr;
    return EditController::terminate();
}

tresult PLUGIN_API Controller::setState(
    IBStream* state) {

    if (!state)
        return kInvalidArgument;

    IBStreamer stream(
        state,
        kLittleEndian);

    double zoom = 1.0;

    if (!stream.readDouble(
            zoom)) {
        return kResultFalse;
    }

    guiZoom_ =
        zoom >= 1.25
            ? 1.5
            : 1.0;

    if (editor_)
        editor_->setZoomFactor(
            guiZoom_);

    return kResultOk;
}

tresult PLUGIN_API Controller::getState(
    IBStream* state) {

    if (!state)
        return kInvalidArgument;

    IBStreamer stream(
        state,
        kLittleEndian);

    return stream.writeDouble(
               guiZoom_)
        ? kResultOk
        : kResultFalse;
}

IPlugView* PLUGIN_API Controller::createView(
    FIDString name) {

    if (!name ||
        std::strcmp(
            name,
            ViewType::kEditor) != 0) {
        return nullptr;
    }

    auto* editor =
        new VSTGUI::VST3Editor(
            this,
            "view",
            "HighGainGuitarFinisher.uidesc");

    editor->setAllowedZoomFactors(
        std::vector<double> {
            1.0,
            1.5
        });

    editor->
        setEditorSizeConstrains(
            VSTGUI::CPoint(
                1320.0,
                560.0),
            VSTGUI::CPoint(
                1320.0,
                560.0));

    editor_ =
        editor;

    editor_->setZoomFactor(
        guiZoom_);

    return editor;
}

VSTGUI::CView*
Controller::createCustomView(
    VSTGUI::UTF8StringPtr name,
    const VSTGUI::UIAttributes&
        attributes,
    const VSTGUI::IUIDescription*,
    VSTGUI::VST3Editor* editor) {

    if (!name ||
        !editor) {
        return nullptr;
    }

    VSTGUI::CPoint origin {
        0.0,
        0.0
    };

    VSTGUI::CPoint size {
        80.0,
        80.0
    };

    attributes.getPointAttribute(
        "origin",
        origin);

    attributes.getPointAttribute(
        "size",
        size);

    const VSTGUI::CRect rect(
        origin.x,
        origin.y,
        origin.x + size.x,
        origin.y + size.y);

    if (std::strcmp(
            name,
            "HGGFPanel") == 0) {

        return new SteelPanelView(
            rect);
    }

    if (std::strcmp(
            name,
            "HGGFBrandLogo") == 0) {

        return new BrandLogoView(
            rect);
    }

    int32 tag = -1;
    auto style =
        SteelKnob::Style::Small;

    if (std::strcmp(
            name,
            "HGGFKnobMatch") == 0) {

        tag = kToneMatchAmount;
        style =
            SteelKnob::Style::Hero;
    } else if (std::strcmp(
                   name,
                   "HGGFKnobFinish") == 0) {

        tag = kFinish;
        style =
            SteelKnob::Style::Hero;
    } else if (std::strcmp(
                   name,
                   "HGGFKnobLowCut") == 0) {

        tag = kLowCut80;
    } else if (std::strcmp(
                   name,
                   "HGGFKnobOutput") == 0) {

        tag = kOutput;
    } else if (std::strcmp(
                   name,
                   "HGGFKnobMass") == 0) {

        tag = kMass;
    } else {
        return nullptr;
    }

    auto* knob = new SteelKnob(
        rect,
        editor,
        tag,
        style);

    // Custom views are created programmatically, so VSTGUI does not inherit
    // the VST3 parameter's default value automatically. CControl otherwise
    // defaults to 0.5, which makes Ctrl+left-click reset several knobs to the
    // wrong position. Keep the host-standard VSTGUI reset gesture and supply
    // the actual normalized parameter defaults explicitly.
    switch (tag) {
        case kFinish:
        case kLowCut80:
        case kMass:
        case kToneMatchAmount:
            knob->setDefaultValue(0.0f);
            break;

        case kOutput:
            knob->setDefaultValue(0.5f);
            break;

        default:
            break;
    }

    return knob;
}

VSTGUI::CView*
Controller::verifyView(
    VSTGUI::CView* view,
    const VSTGUI::UIAttributes&,
    const VSTGUI::IUIDescription*,
    VSTGUI::VST3Editor* editor) {

    auto* control =
        dynamic_cast<
            VSTGUI::CControl*>(
                view);

    if (!control)
        return view;

    const auto tag =
        control->getTag();

    if (tag == kGuiZoomTag) {
        editor_ = editor;

        control->setListener(
            this);

        control->setMin(0.f);
        control->setMax(1.f);

        control->setValueNormalized(
            guiZoom_ >= 1.25
                ? 1.f
                : 0.f);
    } else if (
        tag == kToneMatchLoadTag) {

        toneMatchLoadButton_ =
            dynamic_cast<
                VSTGUI::CTextButton*>(
                    control);

        control->setListener(this);
    } else if (
        tag == kToneMatchAnalyzeTag) {

        toneMatchAnalyzeButton_ =
            dynamic_cast<
                VSTGUI::CTextButton*>(
                    control);

        control->setListener(this);
    } else if (
        tag == kToneMatchMenuTag) {

        toneMatchMenu_ =
            dynamic_cast<
                VSTGUI::COptionMenu*>(
                    control);

        if (toneMatchMenu_) {
            toneMatchMenu_->
                setListener(this);

            if (toneMatchMenu_->
                    getNbEntries() == 0) {

                toneMatchMenu_->
                    addEntry("...");

                toneMatchMenu_->
                    addEntry(
                        "LOAD PROFILE...");

                toneMatchMenu_->
                    addEntry(
                        "SAVE PROFILE...");

                toneMatchMenu_->
                    addEntry(
                        "CLEAR TARGET");

                toneMatchMenu_->
                    addEntry(
                        "CLEAR REFERENCE");
            }

            toneMatchMenu_->
                setCurrent(0);
        }
    } else if (
        tag == kToneMatchStatusTag) {

        toneMatchStatusLabel_ =
            dynamic_cast<
                VSTGUI::CTextLabel*>(
                    control);
    }

    updateToneMatchGui();

    return view;
}

void Controller::valueChanged(
    VSTGUI::CControl* control) {

    if (!control)
        return;

    const auto tag =
        control->getTag();

    if (tag == kGuiZoomTag) {
        if (!editor_)
            return;

        guiZoom_ =
            control->
                getValueNormalized() >=
                    0.5f
                ? 1.5
                : 1.0;

        editor_->setZoomFactor(
            guiZoom_);

        return;
    }

    if (tag == kToneMatchLoadTag) {
        if (control->
                getValueNormalized() < 0.5f) {
            return;
        }

        chooseToneMatchReference(
            control->getFrame());

        updateToneMatchGui();
        return;
    }

    if (tag == kToneMatchAnalyzeTag) {
        if (control->
                getValueNormalized() < 0.5f) {
            return;
        }

        if (toneMatchStatus_ ==
            ToneMatchStatus::Analyzing) {

            stopToneMatchCapture();
        } else if (
            toneMatchStatus_ !=
                ToneMatchStatus::Matching) {
            startToneMatchCapture();
        }

        updateToneMatchGui();
        return;
    }

    if (tag == kToneMatchMenuTag) {
        auto* menu =
            dynamic_cast<
                VSTGUI::COptionMenu*>(
                    control);

        if (!menu)
            return;

        const int32 index =
            menu->getCurrentIndex();

        switch (index) {
            case 1:
                chooseToneMatchReferenceProfile(
                    control->getFrame());
                break;

            case 2:
                chooseToneMatchSaveProfile(
                    control->getFrame());
                break;

            case 3:
                clearToneMatchTarget();
                break;

            case 4:
                clearToneMatchReference();
                break;

            default:
                break;
        }

        menu->setCurrent(0);
        menu->setValue(0.f);
        menu->invalid();

        updateToneMatchGui();
    }
}

void Controller::willClose(
    VSTGUI::VST3Editor* editor) {

    if (editor_ == editor) {
        editor_ = nullptr;
        toneMatchLoadButton_ = nullptr;
        toneMatchAnalyzeButton_ = nullptr;
        toneMatchMenu_ = nullptr;
        toneMatchStatusLabel_ = nullptr;
    }
}

tresult PLUGIN_API
Controller::getParamStringByValue(
    Steinberg::Vst::ParamID id,
    ParamValue valueNormalized,
    String128 string) {

    switch (id) {
        case kFinish:
        case kMass:
        case kToneMatchAmount:
            copyAscii(
                percentText(
                    valueNormalized),
                string);
            return kResultTrue;

        case kOutput:
            copyAscii(
                outputText(
                    valueNormalized),
                string);
            return kResultTrue;

        case kBypass:
            copyAscii(
                valueNormalized >= 0.5
                    ? "BYPASS"
                    : "ACTIVE",
                string);
            return kResultTrue;

        case kMode:
            copyAscii(
                valueNormalized < 0.25
                    ? "CLEAN"
                    : (valueNormalized < 0.75
                        ? "PUNCH"
                        : "DENSE"),
                string);
            return kResultTrue;

        default:
            return EditController::
                getParamStringByValue(
                    id,
                    valueNormalized,
                    string);
    }
}

tresult PLUGIN_API
Controller::getParamValueByString(
    Steinberg::Vst::ParamID id,
    Steinberg::Vst::TChar* string,
    ParamValue& valueNormalized) {

    if (!string)
        return kInvalidArgument;

    if (id == kBypass) {
        if (asciiEqualsIgnoreCase(
                string,
                "ACTIVE")) {

            valueNormalized = 0.0;
            return kResultTrue;
        }

        if (asciiEqualsIgnoreCase(
                string,
                "BYPASS")) {

            valueNormalized = 1.0;
            return kResultTrue;
        }
    }

    if (id == kMode) {
        if (asciiEqualsIgnoreCase(string, "CLEAN")) {
            valueNormalized = 0.0;
            return kResultTrue;
        }

        if (asciiEqualsIgnoreCase(string, "PUNCH")) {
            valueNormalized = 0.5;
            return kResultTrue;
        }

        if (asciiEqualsIgnoreCase(string, "DENSE")) {
            valueNormalized = 1.0;
            return kResultTrue;
        }

        // Keep numeric 1/2/3 input compatible for hosts or old automation
        // text that may still submit the legacy labels.
        UString value(
            string,
            strlen16(string));

        ParamValue plain = 0.0;

        if (!value.scanFloat(plain))
            return kResultFalse;

        const int mode =
            std::clamp(
                static_cast<int>(
                    std::llround(plain)),
                1,
                3);

        valueNormalized =
            mode == 1
                ? 0.0
                : (mode == 2
                    ? 0.5
                    : 1.0);

        return kResultTrue;
    }

    if (id == kFinish ||
        id == kMass ||
        id == kToneMatchAmount) {

        UString value(
            string,
            strlen16(string));

        ParamValue percent = 0.0;

        if (!value.scanFloat(percent))
            return kResultFalse;

        valueNormalized =
            std::clamp(
                percent / 100.0,
                0.0,
                1.0);

        return kResultTrue;
    }

    return EditController::
        getParamValueByString(
            id,
            string,
            valueNormalized);
}

tresult PLUGIN_API
Controller::setComponentState(
    IBStream* state) {

    invalidateToneMatchBuild();

    if (!state)
        return kInvalidArgument;

    IBStreamer stream(state, kLittleEndian);

    int32 version = 0;
    if (!stream.readInt32(version) ||
        version < kFirstSupportedStateVersion ||
        version > kStateVersion) {
        return kResultFalse;
    }

    double values[6] {};
    for (double& value : values) {
        if (!stream.readDouble(value) ||
            !std::isfinite(value)) {
            return kResultFalse;
        }
        value = std::clamp(value, 0.0, 1.0);
    }

    ToneMatchStatePayload nextToneMatch {};
    if (!readToneMatchState(
            stream,
            nextToneMatch,
            version))
        return kResultFalse;

    dsp::ToneMatchSpectrumSnapshot nextReferenceSpectrum {};
    if (!readToneMatchReferenceState(
            stream,
            nextReferenceSpectrum,
            version)) {
        return kResultFalse;
    }

    setParamNormalized(kFinish, values[0]);
    setParamNormalized(kOutput, values[1]);
    setParamNormalized(kBypass, values[2]);
    setParamNormalized(kLowCut80, values[3]);
    setParamNormalized(kMode, values[4]);
    setParamNormalized(kMass, values[5]);
    setParamNormalized(kToneMatchAmount, nextToneMatch.amount);

    activeToneMatchProfile_ = nextToneMatch.profile;
    toneMatchReferenceSpectrum_ = nextReferenceSpectrum;
    toneMatchReferenceReady_ =
        toneMatchReferenceSpectrum_.frameCount >= 4u;

    // Target audio is intentionally not persisted. A restored reference can
    // be re-used, but TARGET must be analysed again for the current source.
    toneMatchTargetSpectrum_ = {};
    toneMatchTargetReady_ = false;

    if (activeToneMatchProfile_.valid) {
        toneMatchStatus_ = ToneMatchStatus::Ready;
        toneMatchLastError_.clear();
    } else if (toneMatchReferenceReady_) {
        toneMatchStatus_ = ToneMatchStatus::ReferenceReady;
        toneMatchLastError_.clear();
    } else {
        toneMatchStatus_ = ToneMatchStatus::Empty;
        toneMatchLastError_.clear();
    }

    updateToneMatchGui();
    return kResultOk;
}

} // namespace HighGainGuitarFinisher


namespace HighGainGuitarFinisher {

Steinberg::tresult
Controller::sendToneMatchReferenceSpectrum(
    const dsp::ToneMatchSpectrumSnapshot& snapshot) {

    using namespace Steinberg;

    auto message =
        owned(allocateMessage());

    if (!message)
        return kResultFalse;

    message->setMessageID(
        kToneMatchReferenceSpectrumMessageID);

    auto* attributes =
        message->getAttributes();

    if (!attributes)
        return kResultFalse;

    const auto payload =
        makeToneMatchReferenceSpectrumMessage(
            snapshot);

    if (attributes->setBinary(
            kToneMatchReferenceSpectrumMessageKey,
            &payload,
            static_cast<uint32>(
                sizeof(payload))) !=
        kResultOk) {
        return kResultFalse;
    }

    return sendMessage(message);
}


Steinberg::tresult
Controller::sendToneMatchProfile(
    const dsp::ToneMatchProfile& profile) {

    using namespace Steinberg;

    auto message =
        owned(allocateMessage());

    if (!message)
        return kResultFalse;

    message->setMessageID(
        kToneMatchProfileMessageID);

    auto* attributes =
        message->getAttributes();

    if (!attributes)
        return kResultFalse;

    const auto payload =
        makeToneMatchProfileMessage(
            profile);

    if (attributes->setBinary(
            kToneMatchProfileMessageKey,
            &payload,
            static_cast<uint32>(
                sizeof(payload))) !=
        kResultOk) {
        return kResultFalse;
    }

    const auto result =
        sendMessage(message);

    if (result == kResultOk) {
        activeToneMatchProfile_ =
            profile;

        toneMatchStatus_ =
            profile.valid
                ? ToneMatchStatus::Ready
                : (toneMatchReferenceReady_
                    ? ToneMatchStatus::ReferenceReady
                    : ToneMatchStatus::Empty);

        toneMatchLastError_.clear();
    }

    updateToneMatchGui();
    return result;
}


Steinberg::tresult
Controller::startToneMatchCapture() {

    if (!toneMatchReferenceReady_) {
        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Load a reference before analyzing the target";
        return Steinberg::kResultFalse;
    }

    auto message =
        Steinberg::owned(
            allocateMessage());

    if (!message) {
        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Cannot create analysis command";
        return Steinberg::kResultFalse;
    }

    message->setMessageID(
        kToneMatchCaptureStartMessageID);

    const auto result =
        sendMessage(message);

    if (result == Steinberg::kResultOk) {
        toneMatchTargetReady_ = false;
        toneMatchStatus_ =
            ToneMatchStatus::Analyzing;
        toneMatchLastError_.clear();
    } else {
        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Cannot start target analysis";
    }

    updateToneMatchGui();
    return result;
}

Steinberg::tresult
Controller::stopToneMatchCapture() {

    auto message =
        Steinberg::owned(
            allocateMessage());

    if (!message) {
        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Cannot create stop command";
        return Steinberg::kResultFalse;
    }

    message->setMessageID(
        kToneMatchCaptureStopMessageID);

    const auto result =
        sendMessage(message);

    if (result != Steinberg::kResultOk) {
        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Target analysis failed or was too long";
    }

    updateToneMatchGui();
    return result;
}


Steinberg::tresult PLUGIN_API
Controller::notify(
    Steinberg::Vst::IMessage* message) {

    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (!message ||
        !message->getMessageID()) {
        return kInvalidArgument;
    }

    if (std::strcmp(
            message->getMessageID(),
            kToneMatchTargetSpectrumMessageID) != 0) {
        return EditController::notify(message);
    }

    auto* attributes =
        message->getAttributes();

    if (!attributes)
        return kResultFalse;

    const void* data = nullptr;
    uint32 size = 0;

    if (attributes->getBinary(
            kToneMatchTargetSpectrumMessageKey,
            data,
            size) != kResultOk ||
        !data ||
        size !=
            sizeof(
                ToneMatchSpectrumMessagePayload)) {
        return kResultFalse;
    }

    const auto* payload =
        static_cast<
            const ToneMatchSpectrumMessagePayload*>(
                data);

    dsp::ToneMatchSpectrumSnapshot snapshot {};

    if (!parseToneMatchSpectrumMessage(
            *payload,
            snapshot)) {
        return kResultFalse;
    }

    setToneMatchTargetSpectrum(
        snapshot);

    if (!toneMatchTargetReady_)
        return kResultFalse;

    return toneMatchStatus_ ==
            ToneMatchStatus::Ready
        ? kResultOk
        : kResultFalse;
}

bool Controller::loadToneMatchReferenceAudio(
    const std::filesystem::path& path) {

    dsp::ToneMatchSpectrumSnapshot snapshot {};
    std::string error;

    if (!ToneMatchReferenceService::analyzeAudioFile(
            path,
            snapshot,
            error)) {

        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            error;
        updateToneMatchGui();
        return false;
    }

    setToneMatchReferenceSpectrum(
        snapshot);

    return true;
}

bool Controller::loadToneMatchReferenceProfile(
    const std::filesystem::path& path) {

    dsp::ToneMatchSpectrumSnapshot snapshot {};
    std::string error;

    if (!ToneMatchReferenceService::loadReferenceProfile(
            path,
            snapshot,
            error)) {

        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            error;
        updateToneMatchGui();
        return false;
    }

    setToneMatchReferenceSpectrum(
        snapshot);

    return true;
}

bool Controller::saveToneMatchReferenceProfile(
    const std::filesystem::path& path) {

    std::string error;

    if (!toneMatchReferenceReady_ ||
        !ToneMatchReferenceService::saveReferenceProfile(
            path,
            toneMatchReferenceSpectrum_,
            error)) {

        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            error.empty()
                ? "No analyzed reference to save"
                : error;
        updateToneMatchGui();
        return false;
    }

    toneMatchLastError_.clear();
    updateToneMatchGui();
    return true;
}

void Controller::clearToneMatchTarget() {
    invalidateToneMatchBuild();
    toneMatchTargetSpectrum_ = {};
    toneMatchTargetReady_ = false;
    activeToneMatchProfile_ = {};

    sendToneMatchProfile(
        activeToneMatchProfile_);

    toneMatchStatus_ =
        toneMatchReferenceReady_
            ? ToneMatchStatus::ReferenceReady
            : ToneMatchStatus::Empty;

    toneMatchLastError_.clear();
    updateToneMatchGui();
}

void Controller::clearToneMatchReference() {
    invalidateToneMatchBuild();
    toneMatchReferenceSpectrum_ = {};
    toneMatchReferenceReady_ = false;
    activeToneMatchProfile_ = {};
    toneMatchStatus_ =
        ToneMatchStatus::Empty;
    toneMatchLastError_.clear();

    sendToneMatchProfile(
        activeToneMatchProfile_);

    sendToneMatchReferenceSpectrum(
        toneMatchReferenceSpectrum_);

    updateToneMatchGui();
}

void Controller::setToneMatchReferenceSpectrum(
    const dsp::ToneMatchSpectrumSnapshot& snapshot) {

    toneMatchReferenceSpectrum_ =
        snapshot;

    toneMatchReferenceReady_ =
        snapshot.frameCount >= 4u;

    if (toneMatchReferenceReady_) {
        activeToneMatchProfile_ = {};
        sendToneMatchProfile(
            activeToneMatchProfile_);

        if (sendToneMatchReferenceSpectrum(
                toneMatchReferenceSpectrum_) !=
            Steinberg::kResultOk) {

            toneMatchStatus_ =
                ToneMatchStatus::Error;
            toneMatchLastError_ =
                "Cannot store reference in plugin state";
            updateToneMatchGui();
            return;
        }

        if (toneMatchTargetReady_) {
            tryBuildToneMatchProfile();
        } else {
            toneMatchStatus_ =
                ToneMatchStatus::ReferenceReady;
            toneMatchLastError_.clear();
        }
    } else {
        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Reference analysis did not contain enough audio";
    }

    updateToneMatchGui();
}

void Controller::setToneMatchTargetSpectrum(
    const dsp::ToneMatchSpectrumSnapshot& snapshot) {

    toneMatchTargetSpectrum_ =
        snapshot;

    toneMatchTargetReady_ =
        snapshot.frameCount >= 4u;

    if (!toneMatchTargetReady_) {
        activeToneMatchProfile_ = {};
        sendToneMatchProfile(
            activeToneMatchProfile_);

        toneMatchStatus_ =
            ToneMatchStatus::Error;
        toneMatchLastError_ =
            "Target analysis did not contain enough audio";
        updateToneMatchGui();
        return;
    }

    tryBuildToneMatchProfile();
    updateToneMatchGui();
}

void Controller::tryBuildToneMatchProfile() {
    if (!toneMatchReferenceReady_ ||
        !toneMatchTargetReady_) {
        updateToneMatchGui();
        return;
    }

    const auto generation =
        toneMatchBuildGeneration_.fetch_add(
            1,
            std::memory_order_acq_rel) + 1;

    {
        std::lock_guard<std::mutex> lock(
            toneMatchBuildMutex_);

        queuedToneMatchReference_ =
            toneMatchReferenceSpectrum_;

        queuedToneMatchTarget_ =
            toneMatchTargetSpectrum_;

        queuedToneMatchGeneration_ =
            generation;

        toneMatchBuildQueued_ =
            true;
    }

    toneMatchStatus_ =
        ToneMatchStatus::Matching;

    toneMatchLastError_.clear();
    updateToneMatchGui();

    launchQueuedToneMatchBuild();

    if (!toneMatchBuildTimer_) {
        toneMatchBuildTimer_ =
            VSTGUI::makeOwned<
                VSTGUI::CVSTGUITimer>(
                    [this](
                        VSTGUI::CVSTGUITimer*) {
                        pollToneMatchBuild();
                    },
                    50);
    }
}

void Controller::invalidateToneMatchBuild() noexcept {
    toneMatchBuildGeneration_.fetch_add(
        1,
        std::memory_order_acq_rel);

    std::lock_guard<std::mutex> lock(
        toneMatchBuildMutex_);

    toneMatchBuildQueued_ = false;
    pendingToneMatchProfile_ = {};
    pendingToneMatchProfileValid_ = false;
    pendingToneMatchProfileGeneration_ = 0;
}

void Controller::joinToneMatchBuildWorker() noexcept {
    if (toneMatchBuildWorker_.joinable())
        toneMatchBuildWorker_.join();

    toneMatchBuildRunning_.store(
        false,
        std::memory_order_release);
}

void Controller::launchQueuedToneMatchBuild() {
    if (toneMatchBuildRunning_.load(
            std::memory_order_acquire)) {
        return;
    }

    // A finished worker may still be joinable until the UI timer observes
    // completion. Joining it here is non-blocking because Running is already
    // false and the worker has reached its completion path.
    if (toneMatchBuildWorker_.joinable())
        toneMatchBuildWorker_.join();

    dsp::ToneMatchSpectrumSnapshot reference {};
    dsp::ToneMatchSpectrumSnapshot target {};
    std::uint64_t generation = 0;

    {
        std::lock_guard<std::mutex> lock(
            toneMatchBuildMutex_);

        if (!toneMatchBuildQueued_)
            return;

        reference = queuedToneMatchReference_;
        target = queuedToneMatchTarget_;
        generation = queuedToneMatchGeneration_;
        toneMatchBuildQueued_ = false;

        pendingToneMatchProfile_ = {};
        pendingToneMatchProfileValid_ = false;
        pendingToneMatchProfileGeneration_ = 0;
    }

    toneMatchBuildDone_.store(
        false,
        std::memory_order_release);

    toneMatchBuildRunning_.store(
        true,
        std::memory_order_release);

    try {
        toneMatchBuildWorker_ =
            std::thread(
                [this,
                 reference,
                 target,
                 generation]() noexcept {

                    dsp::ToneMatchProfile profile {};
                    bool valid = false;

                    try {
                        profile =
                            dsp::ToneMatchAnalyzer::
                                makeProfile(
                                    reference,
                                    target);

                        valid = profile.valid;
                    } catch (...) {
                        profile = {};
                        valid = false;
                    }

                    {
                        std::lock_guard<std::mutex>
                            lock(
                                toneMatchBuildMutex_);

                        pendingToneMatchProfile_ =
                            profile;

                        pendingToneMatchProfileValid_ =
                            valid;

                        pendingToneMatchProfileGeneration_ =
                            generation;
                    }

                    toneMatchBuildRunning_.store(
                        false,
                        std::memory_order_release);

                    toneMatchBuildDone_.store(
                        true,
                        std::memory_order_release);
                });
    } catch (...) {
        toneMatchBuildRunning_.store(
            false,
            std::memory_order_release);

        toneMatchStatus_ =
            ToneMatchStatus::Error;

        toneMatchLastError_ =
            "Cannot start Tone Match calculation";

        updateToneMatchGui();
    }
}

void Controller::pollToneMatchBuild() {
    if (!toneMatchBuildDone_.exchange(
            false,
            std::memory_order_acq_rel)) {
        return;
    }

    if (toneMatchBuildWorker_.joinable())
        toneMatchBuildWorker_.join();

    dsp::ToneMatchProfile profile {};
    bool valid = false;
    std::uint64_t completedGeneration = 0;
    bool queued = false;

    {
        std::lock_guard<std::mutex> lock(
            toneMatchBuildMutex_);

        profile =
            pendingToneMatchProfile_;

        valid =
            pendingToneMatchProfileValid_;

        completedGeneration =
            pendingToneMatchProfileGeneration_;

        queued =
            toneMatchBuildQueued_;
    }

    const auto currentGeneration =
        toneMatchBuildGeneration_.load(
            std::memory_order_acquire);

    if (completedGeneration !=
        currentGeneration) {

        if (queued) {
            toneMatchStatus_ =
                ToneMatchStatus::Matching;

            launchQueuedToneMatchBuild();
        }

        updateToneMatchGui();
        return;
    }

    if (!valid) {
        toneMatchStatus_ =
            ToneMatchStatus::Error;

        toneMatchLastError_ =
            "Reference and target could not be matched";

        updateToneMatchGui();
        return;
    }

    activeToneMatchProfile_ =
        profile;

    if (sendToneMatchProfile(profile) !=
        Steinberg::kResultOk) {

        toneMatchStatus_ =
            ToneMatchStatus::Error;

        toneMatchLastError_ =
            "Cannot apply Tone Match profile";
    } else {
        toneMatchStatus_ =
            ToneMatchStatus::Ready;

        toneMatchLastError_.clear();
    }

    updateToneMatchGui();
}

void Controller::updateToneMatchGui() {
    if (toneMatchStatusLabel_) {
        toneMatchStatusLabel_->
            setText(
                toneMatchStatusText(
                    toneMatchStatus_));

        toneMatchStatusLabel_->
            invalid();
    }

    if (toneMatchAnalyzeButton_) {
        toneMatchAnalyzeButton_->
            setTitle(
                toneMatchStatus_ ==
                    ToneMatchStatus::Analyzing
                    ? "STOP ANALYSIS"
                    : (toneMatchStatus_ ==
                           ToneMatchStatus::Matching
                       ? "MATCHING..."
                       : "ANALYZE TARGET"));

        toneMatchAnalyzeButton_->
            setMouseEnabled(
                toneMatchStatus_ !=
                    ToneMatchStatus::Matching);

        toneMatchAnalyzeButton_->
            invalid();
    }

    if (toneMatchMenu_) {
        if (auto* save =
                toneMatchMenu_->
                    getEntry(2)) {
            save->setEnabled(
                toneMatchReferenceReady_);
        }

        if (auto* clearTarget =
                toneMatchMenu_->
                    getEntry(3)) {
            clearTarget->setEnabled(
                toneMatchTargetReady_);
        }

        if (auto* clearReference =
                toneMatchMenu_->
                    getEntry(4)) {
            clearReference->setEnabled(
                toneMatchReferenceReady_);
        }

        toneMatchMenu_->invalid();
    }
}

void Controller::chooseToneMatchReference(
    VSTGUI::CFrame* frame) {

    if (!frame)
        return;

    auto selector =
        VSTGUI::owned(
            VSTGUI::CNewFileSelector::create(
                frame,
                VSTGUI::CNewFileSelector::
                    kSelectFile));

    if (!selector)
        return;

    selector->setTitle(
        "Load Tone Match Reference");

    selector->addFileExtension(
        VSTGUI::CFileExtension(
            "WAV Audio",
            "wav",
            "audio/wav"));

    selector->addFileExtension(
        VSTGUI::CFileExtension(
            "FLAC Audio",
            "flac",
            "audio/flac"));

    selector->addFileExtension(
        VSTGUI::CFileExtension(
            "MP3 Audio",
            "mp3",
            "audio/mpeg"));

    selector->addFileExtension(
        VSTGUI::CFileExtension(
            "125A Reference Profile",
            "125aref"));

    if (!selector->runModal() ||
        selector->getNumSelectedFiles() < 1) {
        return;
    }

    const auto* selected =
        selector->getSelectedFile(0);

    if (!selected)
        return;

    const auto path =
        std::filesystem::u8path(
            selected);

    auto extension =
        path.extension().u8string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    if (extension == ".125aref") {
        loadToneMatchReferenceProfile(
            path);
    } else {
        loadToneMatchReferenceAudio(
            path);
    }

    updateToneMatchGui();
}

void Controller::chooseToneMatchReferenceProfile(
    VSTGUI::CFrame* frame) {

    if (!frame)
        return;

    auto selector =
        VSTGUI::owned(
            VSTGUI::CNewFileSelector::create(
                frame,
                VSTGUI::CNewFileSelector::
                    kSelectFile));

    if (!selector)
        return;

    selector->setTitle(
        "Load 125A Reference Profile");

    selector->addFileExtension(
        VSTGUI::CFileExtension(
            "125A Reference Profile",
            "125aref"));

    selector->setDefaultExtension(
        VSTGUI::CFileExtension(
            "125A Reference Profile",
            "125aref"));

    if (!selector->runModal() ||
        selector->getNumSelectedFiles() < 1) {
        return;
    }

    const auto* selected =
        selector->getSelectedFile(0);

    if (!selected)
        return;

    loadToneMatchReferenceProfile(
        std::filesystem::u8path(
            selected));

    updateToneMatchGui();
}

void Controller::chooseToneMatchSaveProfile(
    VSTGUI::CFrame* frame) {

    if (!frame)
        return;

    auto selector =
        VSTGUI::owned(
            VSTGUI::CNewFileSelector::create(
                frame,
                VSTGUI::CNewFileSelector::
                    kSelectSaveFile));

    if (!selector)
        return;

    selector->setTitle(
        "Save 125A Reference Profile");

    selector->setDefaultExtension(
        VSTGUI::CFileExtension(
            "125A Reference Profile",
            "125aref"));

    selector->setDefaultSaveName(
        "ToneMatchReference.125aref");

    if (!selector->runModal() ||
        selector->getNumSelectedFiles() < 1) {
        return;
    }

    const auto* selected =
        selector->getSelectedFile(0);

    if (!selected)
        return;

    saveToneMatchReferenceProfile(
        std::filesystem::u8path(
            selected));

    updateToneMatchGui();
}

} // namespace HighGainGuitarFinisher
