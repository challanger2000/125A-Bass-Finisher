#pragma once

#include "vstgui/lib/cview.h"

#include <string>
#include <vector>

namespace HighGainGuitarFinisher {

class BrandLogoView final : public VSTGUI::CView {
public:
    explicit BrandLogoView(
        const VSTGUI::CRect& size);

    void draw(
        VSTGUI::CDrawContext* context) override;

private:
    struct Shape {
        std::string path;
        bool red {false};
    };

    bool loadMaster();
    std::vector<Shape> shapes_ {};
};

} // namespace HighGainGuitarFinisher
