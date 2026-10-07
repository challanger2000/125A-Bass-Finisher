#include "BrandLogoView.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cresourcedescription.h"
#include "vstgui/uidescription/cstream.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>

namespace HighGainGuitarFinisher {

namespace {

constexpr double kMasterWidth =
    1774.0;

constexpr double kMasterHeight =
    887.0;

constexpr VSTGUI::CColor kSilver {
    217,
    217,
    217,
    255
};

constexpr VSTGUI::CColor kRed {
    215,
    25,
    32,
    255
};

void skipSeparators(
    const std::string& data,
    std::size_t& position) {

    while (position <
           data.size()) {

        const char c =
            data[position];

        if (std::isspace(
                static_cast<unsigned char>(c)) ||
            c == ',') {

            ++position;
            continue;
        }

        break;
    }
}

bool readNumber(
    const std::string& data,
    std::size_t& position,
    double& value) {

    skipSeparators(
        data,
        position);

    if (position >=
        data.size()) {
        return false;
    }

    const char* start =
        data.c_str() +
        position;

    char* end =
        nullptr;

    value =
        std::strtod(
            start,
            &end);

    if (end == start)
        return false;

    position =
        static_cast<std::size_t>(
            end -
            data.c_str());

    return true;
}

bool buildPath(
    VSTGUI::CGraphicsPath* path,
    const std::string& data,
    double scale,
    double offsetX,
    double offsetY) {

    std::size_t position = 0;
    char command = 0;
    bool hasGeometry = false;

    while (position <
           data.size()) {

        skipSeparators(
            data,
            position);

        if (position >=
            data.size()) {
            break;
        }

        const char current =
            data[position];

        if (std::isalpha(
                static_cast<unsigned char>(
                    current))) {

            command =
                current;

            ++position;

            if (command == 'Z' ||
                command == 'z') {

                path->closeSubpath();
                hasGeometry = true;
                command = 0;
            }

            continue;
        }

        if (command != 'M' &&
            command != 'L') {

            return false;
        }

        double x = 0.0;
        double y = 0.0;

        if (!readNumber(
                data,
                position,
                x) ||
            !readNumber(
                data,
                position,
                y)) {

            return false;
        }

        const VSTGUI::CPoint point(
            offsetX +
                x * scale,
            offsetY +
                y * scale);

        if (command == 'M') {
            path->beginSubpath(
                point);

            // SVG specifies that subsequent coordinate pairs
            // after M are implicit L commands.
            command = 'L';
        } else {
            path->addLine(
                point);
        }

        hasGeometry = true;
    }

    return hasGeometry;
}

}

BrandLogoView::BrandLogoView(
    const VSTGUI::CRect& size)
: VSTGUI::CView(size) {

    setTransparency(true);
    setMouseEnabled(false);
    loadMaster();
}

bool BrandLogoView::loadMaster() {
    VSTGUI::CResourceInputStream stream;

    if (!stream.open(
            VSTGUI::CResourceDescription(
                "125A_Logo_Master_FINAL.svg"))) {

        return false;
    }

    std::string xml;
    std::array<char, 4096>
        buffer {};

    for (;;) {
        const auto read =
            stream.readRaw(
                buffer.data(),
                static_cast<uint32_t>(
                    buffer.size()));

        if (read == 0 ||
            read ==
                VSTGUI::kStreamIOError) {
            break;
        }

        xml.append(
            buffer.data(),
            static_cast<std::size_t>(
                read));

        if (read <
            buffer.size()) {
            break;
        }
    }

    std::size_t position = 0;

    while (true) {
        const auto pathStart =
            xml.find(
                "<path",
                position);

        if (pathStart ==
            std::string::npos) {
            break;
        }

        const auto tagEnd =
            xml.find(
                "/>",
                pathStart);

        if (tagEnd ==
            std::string::npos) {
            break;
        }

        const auto dStart =
            xml.find(
                "d=\"",
                pathStart);

        if (dStart ==
                std::string::npos ||
            dStart >
                tagEnd) {

            position =
                tagEnd + 2;
            continue;
        }

        const auto dataStart =
            dStart + 3;

        const auto dataEnd =
            xml.find(
                '"',
                dataStart);

        if (dataEnd ==
                std::string::npos ||
            dataEnd >
                tagEnd) {

            position =
                tagEnd + 2;
            continue;
        }

        const auto fillStart =
            xml.find(
                "fill=\"",
                dataEnd);

        bool red = false;

        if (fillStart !=
                std::string::npos &&
            fillStart <
                tagEnd) {

            const auto colorStart =
                fillStart + 6;

            const auto colorEnd =
                xml.find(
                    '"',
                    colorStart);

            if (colorEnd !=
                    std::string::npos &&
                colorEnd <
                    tagEnd) {

                const auto color =
                    xml.substr(
                        colorStart,
                        colorEnd -
                            colorStart);

                red =
                    color ==
                    "#D71920";
            }
        }

        shapes_.push_back(
            {
                xml.substr(
                    dataStart,
                    dataEnd -
                        dataStart),
                red
            });

        position =
            tagEnd + 2;
    }

    return !shapes_.empty();
}

void BrandLogoView::draw(
    VSTGUI::CDrawContext* context) {

    if (shapes_.empty()) {
        setDirty(false);
        return;
    }

    const auto r =
        getViewSize();

    const double scale =
        std::min(
            r.getWidth() /
                kMasterWidth,
            r.getHeight() /
                kMasterHeight);

    const double width =
        kMasterWidth *
        scale;

    const double height =
        kMasterHeight *
        scale;

    const double offsetX =
        r.left +
        (r.getWidth() -
         width) *
            0.5;

    const double offsetY =
        r.top +
        (r.getHeight() -
         height) *
            0.5;

    context->setDrawMode(
        VSTGUI::kAntiAliasing);

    for (const auto& shape :
         shapes_) {

        auto* path =
            context->
                createGraphicsPath();

        if (!path)
            continue;

        if (buildPath(
                path,
                shape.path,
                scale,
                offsetX,
                offsetY)) {

            context->setFillColor(
                shape.red
                    ? kRed
                    : kSilver);

            context->drawGraphicsPath(
                path,
                VSTGUI::CDrawContext::
                    kPathFilledEvenOdd);
        }

        path->forget();
    }

    setDirty(false);
}

} // namespace HighGainGuitarFinisher
