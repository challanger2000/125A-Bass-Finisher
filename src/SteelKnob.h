#pragma once

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/controls/cknob.h"

#include <cstdint>

namespace HighGainGuitarFinisher {

class SteelKnob final : public VSTGUI::CKnob {
public:
    enum class Style {
        Small,
        Hero
    };

    SteelKnob(const VSTGUI::CRect& size,
              VSTGUI::IControlListener* listener,
              int32_t tag,
              Style style);

    void draw(VSTGUI::CDrawContext* context) override;

private:
    Style style_ {Style::Small};
    VSTGUI::SharedPointer<VSTGUI::CBitmap> ring100_;
    VSTGUI::SharedPointer<VSTGUI::CBitmap> ring150_;
};

} // namespace HighGainGuitarFinisher
