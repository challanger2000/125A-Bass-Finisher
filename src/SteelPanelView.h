#pragma once

#include "vstgui/lib/cview.h"

namespace HighGainGuitarFinisher {

class SteelPanelView final :
    public VSTGUI::CView {
public:
    explicit SteelPanelView(
        const VSTGUI::CRect& size);

    SteelPanelView(
        const SteelPanelView& other);

    void draw(
        VSTGUI::CDrawContext* context) override;
};

} // namespace HighGainGuitarFinisher
