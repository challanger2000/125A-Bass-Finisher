#include "SteelPanelView.h"

#include "vstgui/lib/cdrawcontext.h"

#include <array>

namespace HighGainGuitarFinisher {

namespace {

constexpr VSTGUI::CColor kBase {15, 17, 20, 255};
constexpr VSTGUI::CColor kBaseLineA {20, 22, 26, 255};
constexpr VSTGUI::CColor kBaseLineB {11, 13, 16, 255};
constexpr VSTGUI::CColor kOuterFrame {66, 69, 75, 255};
constexpr VSTGUI::CColor kInnerFrame {31, 34, 39, 255};
constexpr VSTGUI::CColor kPlate {24, 27, 31, 255};
constexpr VSTGUI::CColor kPlateInner {18, 20, 24, 255};
constexpr VSTGUI::CColor kPlateTop {84, 87, 94, 150};
constexpr VSTGUI::CColor kPlateBottom {0, 0, 0, 145};
constexpr VSTGUI::CColor kAccent {118, 104, 255, 255};
constexpr VSTGUI::CColor kAccentSoft {118, 104, 255, 70};
constexpr VSTGUI::CColor kScrewOuter {8, 9, 11, 255};
constexpr VSTGUI::CColor kScrew {119, 123, 130, 255};
constexpr VSTGUI::CColor kScrewLight {204, 207, 212, 180};
constexpr VSTGUI::CColor kScrewSlot {45, 48, 53, 255};

void drawScrew(
    VSTGUI::CDrawContext* context,
    double x,
    double y) {

    auto shadow =
        VSTGUI::CRect(
            x - 6.0,
            y - 5.0,
            x + 6.0,
            y + 7.0);

    context->setFillColor(
        kScrewOuter);

    context->drawEllipse(
        shadow,
        VSTGUI::kDrawFilled);

    auto body =
        VSTGUI::CRect(
            x - 5.0,
            y - 5.0,
            x + 5.0,
            y + 5.0);

    context->setFillColor(kScrew);
    context->drawEllipse(
        body,
        VSTGUI::kDrawFilled);

    auto shine = body;
    shine.inset(1.8, 1.8);
    shine.bottom =
        shine.top +
        shine.getHeight() * 0.45;

    context->setFillColor(
        kScrewLight);

    context->drawEllipse(
        shine,
        VSTGUI::kDrawFilled);

    context->setFrameColor(
        kScrewSlot);

    context->setLineWidth(1.2);

    context->drawLine(
        VSTGUI::CPoint(
            x - 2.4,
            y + 2.4),
        VSTGUI::CPoint(
            x + 2.4,
            y - 2.4));
}

void drawPlate(
    VSTGUI::CDrawContext* context,
    const VSTGUI::CRect& rect) {

    context->setFillColor(
        kPlateBottom);

    auto shadow = rect;
    shadow.offset(0.0, 4.0);

    context->drawRect(
        shadow,
        VSTGUI::kDrawFilled);

    context->setFillColor(
        kPlate);

    context->setFrameColor(
        kOuterFrame);

    context->setLineWidth(1.0);

    context->drawRect(
        rect,
        VSTGUI::kDrawFilledAndStroked);

    auto inner = rect;
    inner.inset(5.0, 5.0);

    context->setFillColor(
        kPlateInner);

    context->setFrameColor(
        kInnerFrame);

    context->drawRect(
        inner,
        VSTGUI::kDrawFilledAndStroked);

    context->setFrameColor(
        kPlateTop);

    context->drawLine(
        VSTGUI::CPoint(
            rect.left + 1.0,
            rect.top + 1.0),
        VSTGUI::CPoint(
            rect.right - 1.0,
            rect.top + 1.0));
}

}

SteelPanelView::SteelPanelView(
    const VSTGUI::CRect& size)
: VSTGUI::CView(size) {

    setMouseEnabled(false);
    setTransparency(false);
}

void SteelPanelView::draw(
    VSTGUI::CDrawContext* context) {

    const auto r =
        getViewSize();

    context->setDrawMode(
        VSTGUI::kAntiAliasing);

    context->setFillColor(kBase);
    context->drawRect(
        r,
        VSTGUI::kDrawFilled);

    // Dense brushed-steel texture.
    for (double y =
             r.top + 1.0;
         y <
             r.bottom;
         y += 3.0) {

        context->setFrameColor(
            static_cast<int>(y) % 2 == 0
                ? kBaseLineA
                : kBaseLineB);

        context->setLineWidth(1.0);

        context->drawLine(
            VSTGUI::CPoint(
                r.left,
                y),
            VSTGUI::CPoint(
                r.right,
                y));
    }

    auto outer = r;
    outer.inset(8.0, 8.0);

    context->setFrameColor(kOuterFrame);
    context->setLineWidth(1.0);
    context->drawRect(
        outer,
        VSTGUI::kDrawStroked);

    auto inner = outer;
    inner.inset(7.0, 7.0);

    context->setFrameColor(kInnerFrame);
    context->drawRect(
        inner,
        VSTGUI::kDrawStroked);

    // Header and four workflow chassis sections. Coordinates intentionally
    // match the uidesc so every visual group has a fixed mathematical box.
    drawPlate(
        context,
        VSTGUI::CRect(
            24.0,
            22.0,
            908.0,
            116.0));

    drawPlate(
        context,
        VSTGUI::CRect(
            28.0,
            124.0,
            290.0,
            544.0));

    drawPlate(
        context,
        VSTGUI::CRect(
            302.0,
            124.0,
            530.0,
            544.0));

    drawPlate(
        context,
        VSTGUI::CRect(
            542.0,
            124.0,
            774.0,
            544.0));

    drawPlate(
        context,
        VSTGUI::CRect(
            786.0,
            124.0,
            904.0,
            544.0));

    // Signal-flow rails: MATCH -> FINISH -> MIX FIT.
    context->setFillColor(kAccentSoft);

    context->drawRect(
        VSTGUI::CRect(
            48.0,
            254.0,
            270.0,
            257.0),
        VSTGUI::kDrawFilled);

    context->drawRect(
        VSTGUI::CRect(
            318.0,
            184.0,
            514.0,
            187.0),
        VSTGUI::kDrawFilled);

    context->drawRect(
        VSTGUI::CRect(
            558.0,
            184.0,
            758.0,
            187.0),
        VSTGUI::kDrawFilled);

    // Stronger energy rail around the two main macro controls.
    context->setFillColor(kAccent);

    context->drawRect(
        VSTGUI::CRect(
            92.0,
            254.0,
            224.0,
            257.0),
        VSTGUI::kDrawFilled);

    context->drawRect(
        VSTGUI::CRect(
            347.0,
            184.0,
            485.0,
            187.0),
        VSTGUI::kDrawFilled);

    // Lower rails visually lock each section together.
    context->setFillColor(kAccentSoft);

    for (const auto& rail :
         std::array<VSTGUI::CRect, 3> {
            VSTGUI::CRect(
                48.0, 532.0,
                270.0, 535.0),
            VSTGUI::CRect(
                318.0, 520.0,
                514.0, 523.0),
            VSTGUI::CRect(
                558.0, 486.0,
                758.0, 489.0)
         }) {

        context->drawRect(
            rail,
            VSTGUI::kDrawFilled);
    }

    // Corner fasteners.
    drawScrew(context, 20.0, 20.0);
    drawScrew(context, 912.0, 20.0);
    drawScrew(context, 20.0, 540.0);
    drawScrew(context, 912.0, 540.0);

    setDirty(false);
}

} // namespace HighGainGuitarFinisher
