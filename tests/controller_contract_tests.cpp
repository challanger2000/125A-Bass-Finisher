#include "TestSupport.h"
#include "HighGainGuitarFinisherController.h"
#include "HighGainGuitarFinisherIDs.h"
#include "ToneMatchStateIO.h"
#include "dsp/LowCutMapping.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include "vstgui/lib/events.h"
#include "vstgui/uidescription/uiattributes.h"

#include <cmath>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif
#include <string>

using HighGainGuitarFinisher::Controller;
using namespace HighGainGuitarFinisher;
using namespace Steinberg;
using namespace Steinberg::Vst;

// VST3 SDK 3.8.1 moduleinit.cpp expects this process/module symbol.
// A standalone EXE test has no plugin dllmain.cpp, so provide the test module handle here.
void* moduleHandle = nullptr;

extern bool InitModule ();
extern bool DeinitModule ();

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


class NoopControlListener final :
    public VSTGUI::IControlListener {
public:
    void valueChanged(
        VSTGUI::CControl*) override {
    }
};

void verifyCustomKnobCtrlResetContract() {
#ifdef _WIN32
    moduleHandle = GetModuleHandleW(nullptr);
    BF_REQUIRE(moduleHandle != nullptr);
#endif
    BF_REQUIRE(InitModule());

    Controller controller;

    BF_REQUIRE(
        controller.initialize(nullptr) ==
        kResultOk);

    auto* editor =
        new VSTGUI::VST3Editor(
            &controller,
            "view",
            "HighGainGuitarFinisher.uidesc");

    BF_REQUIRE(editor != nullptr);

    struct ControlSpec {
        const char* name;
        Steinberg::Vst::ParamID tag;
        float defaultValue;
        float probeValue;
    };

    const ControlSpec controls[] {
        {
            "HGGFKnobMatch",
            HighGainGuitarFinisher::kToneMatchAmount,
            0.0f,
            0.73f
        },
        {
            "HGGFKnobFinish",
            HighGainGuitarFinisher::kFinish,
            0.0f,
            0.81f
        },
        {
            "HGGFKnobLowCut",
            HighGainGuitarFinisher::kLowCut80,
            0.0f,
            0.66f
        },
        {
            "HGGFKnobMass",
            HighGainGuitarFinisher::kMass,
            0.0f,
            0.59f
        },
        {
            "HGGFKnobOutput",
            HighGainGuitarFinisher::kOutput,
            0.5f,
            0.17f
        }
    };

    VSTGUI::UIAttributes attributes;
    NoopControlListener noopListener;

    for (const auto& spec : controls) {
        auto* view =
            controller.createCustomView(
                spec.name,
                attributes,
                nullptr,
                editor);

        BF_REQUIRE(view != nullptr);

        auto* control =
            dynamic_cast<
                VSTGUI::CControl*>(
                    view);

        BF_REQUIRE(control != nullptr);
        BF_REQUIRE(
            control->getTag() ==
            static_cast<int32>(
                spec.tag));

        BF_REQUIRE(
            std::abs(
                control->getDefaultValue() -
                spec.defaultValue) <
            1.0e-7f);

        // The default-reset gesture belongs to VSTGUI::CControl itself.
        // Keep the real controller-created SteelKnob and real Ctrl-click
        // event path, but decouple the gesture test from host automation
        // callbacks because this editor is intentionally not opened here.
        control->setListener(&noopListener);

        control->setValueNormalized(
            spec.probeValue);

        VSTGUI::MouseDownEvent event;
        event.buttonState.set(
            VSTGUI::MouseButton::Left);
        event.modifiers =
            VSTGUI::ModifierKey::Control;

        control->dispatchEvent(event);

        BF_REQUIRE(
            static_cast<bool>(
                event.consumed));

        BF_REQUIRE(
            std::abs(
                control->getValueNormalized() -
                spec.defaultValue) <
            1.0e-7f);

        view->forget();
    }

    editor->release();

    BF_REQUIRE(
        controller.terminate() ==
        kResultOk);

    BF_REQUIRE(DeinitModule());
    moduleHandle = nullptr;
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


#ifdef _WIN32
class TestPlugFrame final : public Steinberg::IPlugFrame {
public:
    explicit TestPlugFrame(HWND parent)
    : parent_(parent) {
    }

    void setView(Steinberg::IPlugView* view) noexcept {
        view_ = view;
    }

    Steinberg::tresult PLUGIN_API resizeView(
        Steinberg::IPlugView* view,
        Steinberg::ViewRect* newSize) override {

        if (!view ||
            !newSize ||
            view != view_ ||
            !parent_) {
            return Steinberg::kInvalidArgument;
        }

        const int width =
            newSize->right -
            newSize->left;

        const int height =
            newSize->bottom -
            newSize->top;

        if (width <= 0 ||
            height <= 0) {
            return Steinberg::kInvalidArgument;
        }

        SetWindowPos(
            parent_,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOMOVE |
                SWP_NOZORDER |
                SWP_NOACTIVATE);

        Steinberg::ViewRect current {};
        if (view->getSize(&current) ==
                Steinberg::kResultTrue &&
            (current.right != newSize->right ||
             current.bottom != newSize->bottom ||
             current.left != newSize->left ||
             current.top != newSize->top)) {

            return view->onSize(
                newSize);
        }

        return Steinberg::kResultTrue;
    }

    Steinberg::tresult PLUGIN_API queryInterface(
        const Steinberg::TUID iid,
        void** obj) override {

        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
                iid,
                Steinberg::IPlugFrame::iid) ||
            Steinberg::FUnknownPrivate::iidEqual(
                iid,
                Steinberg::FUnknown::iid)) {

            *obj =
                static_cast<
                    Steinberg::IPlugFrame*>(
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

private:
    HWND parent_ {};
    Steinberg::IPlugView* view_ {};
};

void verifyEditorLifecycle() {
    wchar_t originalDirectory[MAX_PATH] {};
    const DWORD originalLength =
        GetCurrentDirectoryW(
            MAX_PATH,
            originalDirectory);

    BF_REQUIRE(originalLength > 0);
    BF_REQUIRE(originalLength < MAX_PATH);
    BF_REQUIRE(
        SetCurrentDirectoryW(
            L"..\\..\\resource") != FALSE);

    moduleHandle =
        GetModuleHandleW(nullptr);

    BF_REQUIRE(moduleHandle != nullptr);
    BF_REQUIRE(InitModule());

    Controller controller;
    BF_REQUIRE(
        controller.initialize(nullptr) ==
        kResultOk);

    HWND parent =
        CreateWindowExW(
            0,
            L"STATIC",
            L"BassFinisherEditorLifecycleHost",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1600,
            900,
            nullptr,
            nullptr,
            static_cast<HINSTANCE>(
                moduleHandle),
            nullptr);

    BF_REQUIRE(parent != nullptr);

    TestPlugFrame plugFrame(parent);

    for (int pass = 0;
         pass < 2;
         ++pass) {

        auto* view =
            controller.createView(
                ViewType::kEditor);

        BF_REQUIRE(view != nullptr);

        ViewRect expected {};
        BF_REQUIRE(
            view->getSize(&expected) ==
            kResultOk);

        BF_REQUIRE(
            expected.getWidth() ==
            932);

        BF_REQUIRE(
            expected.getHeight() ==
            560);

        plugFrame.setView(view);

        BF_REQUIRE(
            view->setFrame(
                &plugFrame) ==
            kResultOk);

        BF_REQUIRE(
            view->attached(
                parent,
                kPlatformTypeHWND) ==
            kResultOk);

        ViewRect attachedSize {};
        BF_REQUIRE(
            view->getSize(
                &attachedSize) ==
            kResultOk);

        BF_REQUIRE(
            attachedSize.getWidth() ==
            expected.getWidth());

        BF_REQUIRE(
            attachedSize.getHeight() ==
            expected.getHeight());

        BF_REQUIRE(
            view->setFrame(nullptr) ==
            kResultOk);

        BF_REQUIRE(
            view->removed() ==
            kResultOk);

        plugFrame.setView(nullptr);
        view->release();
    }

    DestroyWindow(parent);

    BF_REQUIRE(
        controller.terminate() ==
        kResultOk);

    BF_REQUIRE(DeinitModule());
    moduleHandle = nullptr;

    BF_REQUIRE(
        SetCurrentDirectoryW(
            originalDirectory) != FALSE);
}
#endif

int main() {
#ifdef _WIN32
    verifyEditorLifecycle();
#endif
    verifyParameterContract();
    verifyCustomKnobCtrlResetContract();
    verifyControllerZoomState();
    verifyComponentStateContract();

    std::cout << "Bass Finisher controller contracts passed\n";
    return 0;
}
