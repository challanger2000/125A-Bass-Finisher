#include "SteelKnob.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HighGainGuitarFinisher {
namespace {

constexpr VSTGUI::CColor kAccent {
    118, 104, 255, 255
};

constexpr VSTGUI::CColor kAccentGlow {
    118, 104, 255, 70
};

constexpr VSTGUI::CColor kMarker {
    236, 239, 244, 255
};

constexpr VSTGUI::CColor kTick {
    158, 165, 177, 150
};

constexpr double kPi =
    3.14159265358979323846;

// The exact shared master has transparent padding around the visible metal.
constexpr double kFinisherRingVisibleDiameterFraction =
    1682.0 / 2048.0;

struct RingAssetNames {
    const char* zoom100;
    const char* zoom150;
};

RingAssetNames ringAssetNames(
    double minDim,
    bool hero) {

    if (hero || minDim >= 145.0) {
        return {
            "125A_FinisherRing_H_128px_100pct.png",
            "125A_FinisherRing_H_192px_150pct.png"
        };
    }

    if (minDim >= 96.0) {
        return {
            "125A_FinisherRing_M_96px_100pct.png",
            "125A_FinisherRing_M_144px_150pct.png"
        };
    }

    return {
        "125A_FinisherRing_S_64px_100pct.png",
        "125A_FinisherRing_S_96px_150pct.png"
    };
}

VSTGUI::SharedPointer<VSTGUI::CBitmap>
loadRingBitmap(
    const char* resourceName,
    double logicalCanvasSize) {

    auto bitmap =
        VSTGUI::owned(
            new VSTGUI::CBitmap(
                VSTGUI::CResourceDescription(
                    resourceName)));

    if (!bitmap ||
        !bitmap->isLoaded())
        return nullptr;

    auto platform =
        bitmap->getPlatformBitmap();

    if (!platform ||
        logicalCanvasSize <= 0.0)
        return nullptr;

    const auto pixelWidth =
        platform->getSize().x;

    if (pixelWidth <= 0.0)
        return nullptr;

    platform->setScaleFactor(
        pixelWidth /
        logicalCanvasSize);

    return bitmap;
}

void fillRadialEllipse(
    VSTGUI::CDrawContext* context,
    const VSTGUI::CRect& rect,
    const VSTGUI::CColor& inner,
    const VSTGUI::CColor& outer,
    const VSTGUI::CPoint& offset) {

    auto* path =
        context->createGraphicsPath();

    if (!path)
        return;

    path->addEllipse(rect);

    auto* gradient =
        VSTGUI::CGradient::create(
            0.0,
            1.0,
            inner,
            outer);

    if (gradient) {
        context->fillRadialGradient(
            path,
            *gradient,
            rect.getCenter(),
            std::max(
                rect.getWidth(),
                rect.getHeight()) * 0.52,
            offset);

        gradient->forget();
    }

    path->forget();
}

} // namespace

SteelKnob::SteelKnob(
    const VSTGUI::CRect& size,
    VSTGUI::IControlListener* listener,
    std::int32_t tag,
    Style style)
: VSTGUI::CKnob(
      size,
      listener,
      tag,
      nullptr,
      nullptr),
  style_(style) {

    setStartAngle(
        static_cast<float>(
            135.0 / 180.0 *
            kPi));

    setRangeAngle(
        static_cast<float>(
            270.0 / 180.0 *
            kPi));

    setWantsFocus(true);
    setTransparency(true);

    const auto minDim =
        std::min(
            size.getWidth(),
            size.getHeight());

    const bool hero =
        style_ == Style::Hero;

    const auto radius =
        minDim *
        (hero ? 0.365 : 0.355);

    const auto bezelRadius =
        radius +
        (hero ? 8.0 : 6.0);

    const auto logicalCanvasSize =
        (bezelRadius * 2.0) /
        kFinisherRingVisibleDiameterFraction;

    const auto names =
        ringAssetNames(
            minDim,
            hero);

    ring100_ =
        loadRingBitmap(
            names.zoom100,
            logicalCanvasSize);

    ring150_ =
        loadRingBitmap(
            names.zoom150,
            logicalCanvasSize);
}

void SteelKnob::draw(
    VSTGUI::CDrawContext* context) {

    const auto r =
        getViewSize();

    const auto center =
        r.getCenter();

    const auto minDim =
        std::min(
            r.getWidth(),
            r.getHeight());

    const bool hero =
        style_ == Style::Hero;

    const auto normalized =
        std::clamp(
            static_cast<double>(
                getValueNormalized()),
            0.0,
            1.0);

    const auto radius =
        minDim *
        (hero ? 0.365 : 0.355);

    const auto angle =
        (135.0 +
         normalized * 270.0) *
        kPi / 180.0;

    context->setDrawMode(
        VSTGUI::kAntiAliasing);

    // Scale ticks sit outside the hardware body.
    const int ticks =
        hero ? 13 : 11;

    for (int i = 0;
         i < ticks;
         ++i) {

        const auto t =
            static_cast<double>(i) /
            static_cast<double>(
                ticks - 1);

        const auto a =
            (135.0 +
             270.0 * t) *
            kPi / 180.0;

        const bool datum =
            i == 0 ||
            i == ticks - 1 ||
            i == (ticks - 1) / 2;

        const auto ro =
            radius +
            (hero ? 17.0 : 12.0);

        const auto ri =
            radius +
            (datum
                ? (hero ? 8.0 : 6.0)
                : (hero ? 11.0 : 8.5));

        context->setFrameColor(
            datum
                ? VSTGUI::CColor{
                    224, 228, 235, 205
                  }
                : kTick);

        context->setLineWidth(
            datum
                ? (hero ? 1.7 : 1.4)
                : 1.0);

        context->drawLine(
            {
                center.x +
                    std::cos(a) * ri,
                center.y +
                    std::sin(a) * ri
            },
            {
                center.x +
                    std::cos(a) * ro,
                center.y +
                    std::sin(a) * ro
            });
    }

    // Contact shadow: gives real physical separation from the faceplate.
    context->setFillColor(
        {0, 0, 0, static_cast<uint8_t>(hero ? 145 : 120)});

    context->drawEllipse(
        {
            center.x - radius - 8.0,
            center.y - radius - 2.0,
            center.x + radius + 8.0,
            center.y + radius + 12.0
        },
        VSTGUI::kDrawFilled);

    // Hero arc has a controlled halo beneath the crisp value arc.
    const auto arcRadius =
        radius +
        (hero ? 12.0 : 8.0);

    const VSTGUI::CRect arcRect(
        center.x - arcRadius,
        center.y - arcRadius,
        center.x + arcRadius,
        center.y + arcRadius);

    if (hero &&
        normalized > 0.001) {

        context->setFrameColor(
            kAccentGlow);

        context->setLineWidth(10.0);

        context->drawArc(
            arcRect,
            135.f,
            static_cast<float>(
                135.0 +
                normalized * 270.0));
    }

    context->setFrameColor(
        kAccent);

    context->setLineWidth(
        hero ? 4.0 : 2.8);

    context->drawArc(
        arcRect,
        135.f,
        static_cast<float>(
            135.0 +
            normalized * 270.0));

    // Shared 125A Finisher gunmetal bezel. The asset replaces the previous
    // procedural outer ring but keeps the exact computed center/radius. If a
    // resource ever fails to load, retain the procedural bezel as a safe UI
    // fallback rather than leaving the control visually incomplete.
    const auto bezelRadius =
        radius +
        (hero ? 8.0 : 6.0);

    const VSTGUI::CRect bezel(
        center.x - bezelRadius,
        center.y - bezelRadius,
        center.x + bezelRadius,
        center.y + bezelRadius);

    // Choose the 100% or 150% raster explicitly from the actual draw
    // scale. Do not rely on VSTGUI's automatic multi-resolution bitmap
    // selection in these plugins.
    double effectiveScale =
        context->getScaleFactor();

    const auto transform =
        context->getCurrentTransform();

    if (transform.m11 > 0.0 &&
        std::abs(transform.m12) < 1.0e-9 &&
        std::abs(transform.m21) < 1.0e-9) {

        effectiveScale *=
            transform.m11;
    }

    auto* ring =
        effectiveScale >= 1.25
            ? ring150_.get()
            : ring100_.get();

    const bool ringDrawn =
        ring &&
        ring->isLoaded();

    if (ringDrawn) {

        const auto logicalSize =
            ring->getWidth();

        const VSTGUI::CRect ringRect(
            center.x - logicalSize * 0.5,
            center.y - logicalSize * 0.5,
            center.x + logicalSize * 0.5,
            center.y + logicalSize * 0.5);

        ring->draw(
            context,
            ringRect);
    } else {

        // Fallback only: if the shared ring resource is unavailable, draw
        // the complete legacy hardware (bezel + skirt). Never draw this
        // legacy skirt on top of a successfully loaded Finisher ring.
        fillRadialEllipse(
            context,
            bezel,
            {205, 209, 216, 255},
            {20, 23, 28, 255},
            {
                -bezelRadius * 0.24,
                -bezelRadius * 0.27
            });

        context->setFrameColor(
            {4, 5, 7, 255});

        context->setLineWidth(
            hero ? 2.0 : 1.5);

        context->drawEllipse(
            bezel,
            VSTGUI::kDrawStroked);

        auto bezelGroove =
            bezel;

        bezelGroove.inset(
            hero ? 3.5 : 3.0,
            hero ? 3.5 : 3.0);

        context->setFrameColor(
            {5, 7, 10, 220});

        context->setLineWidth(
            hero ? 2.2 : 1.7);

        context->drawEllipse(
            bezelGroove,
            VSTGUI::kDrawStroked);

        const VSTGUI::CRect skirt(
            center.x - radius - 4.0,
            center.y - radius - 4.0,
            center.x + radius + 4.0,
            center.y + radius + 4.0);

        fillRadialEllipse(
            context,
            skirt,
            {172, 178, 188, 255},
            {31, 35, 42, 255},
            {
                -radius * 0.28,
                -radius * 0.31
            });

        context->setFrameColor(
            {4, 5, 7, 255});

        context->setLineWidth(
            hero ? 1.6 : 1.2);

        context->drawEllipse(
            skirt,
            VSTGUI::kDrawStroked);

        auto skirtInner =
            skirt;

        skirtInner.inset(
            4.0,
            4.0);

        context->setFrameColor(
            {230, 233, 239, 48});

        context->setLineWidth(1.0);

        context->drawEllipse(
            skirtInner,
            VSTGUI::kDrawStroked);
    }

    // Deep graphite cap with off-axis highlight.
    const auto capRadius =
        radius *
        (hero ? 0.72 : 0.70);

    const VSTGUI::CRect cap(
        center.x - capRadius,
        center.y - capRadius,
        center.x + capRadius,
        center.y + capRadius);

    fillRadialEllipse(
        context,
        cap,
        hero
            ? VSTGUI::CColor{
                78, 82, 91, 255
              }
            : VSTGUI::CColor{
                70, 74, 82, 255
              },
        {11, 13, 17, 255},
        {
            -capRadius * 0.32,
            -capRadius * 0.36
        });

    context->setFrameColor(
        {3, 4, 6, 255});

    context->setLineWidth(1.0);

    context->drawEllipse(
        cap,
        VSTGUI::kDrawStroked);

    // Restrained specular crescent: material cue, not decoration.
    const auto highlightRadius =
        capRadius * 0.78;

    VSTGUI::CRect highlight(
        center.x - highlightRadius,
        center.y - highlightRadius,
        center.x + highlightRadius,
        center.y + highlightRadius);

    context->setFrameColor(
        {255, 255, 255, static_cast<uint8_t>(hero ? 58 : 42)});

    context->setLineWidth(
        hero ? 1.5 : 1.2);

    context->drawArc(
        highlight,
        205.f,
        315.f);

    // Ivory index line plus tiny illuminated tip.
    const auto p1Radius =
        capRadius * 0.20;

    const auto p2Radius =
        radius * 0.80;

    const VSTGUI::CPoint p1(
        center.x +
            std::cos(angle) *
            p1Radius,
        center.y +
            std::sin(angle) *
            p1Radius);

    const VSTGUI::CPoint p2(
        center.x +
            std::cos(angle) *
            p2Radius,
        center.y +
            std::sin(angle) *
            p2Radius);

    context->setFrameColor(
        kMarker);

    context->setLineWidth(
        hero ? 3.0 : 2.2);

    context->drawLine(
        p1,
        p2);

    const auto tip =
        hero ? 2.5 : 1.9;

    context->setFillColor(
        {184, 176, 255, 255});

    context->drawEllipse(
        {
            p2.x - tip,
            p2.y - tip,
            p2.x + tip,
            p2.y + tip
        },
        VSTGUI::kDrawFilled);

    // Small hub completes the physical control without a decorative cap.
    const auto hub =
        hero ? 4.6 : 3.6;

    context->setFillColor(
        {12, 14, 18, 255});

    context->setFrameColor(
        {128, 134, 144, 190});

    context->setLineWidth(1.0);

    context->drawEllipse(
        {
            center.x - hub,
            center.y - hub,
            center.x + hub,
            center.y + hub
        },
        VSTGUI::kDrawFilledAndStroked);

    setDirty(false);
}

} // namespace HighGainGuitarFinisher
