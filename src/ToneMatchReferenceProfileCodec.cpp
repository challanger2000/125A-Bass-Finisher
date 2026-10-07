#include "ToneMatchReferenceProfileCodec.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace HighGainGuitarFinisher {

std::string
ToneMatchReferenceProfileCodec::encode(
    const dsp::ToneMatchSpectrumSnapshot& snapshot) {

    std::ostringstream out;
    out << "125A_REFERENCE_PROFILE\n";
    out << "version=" << kFileVersion << "\n";
    out << std::setprecision(17);
    out << "sampleRate=" << snapshot.sampleRate << "\n";
    out << "frameCount=" << snapshot.frameCount << "\n";
    out << "hasLogCurve="
        << (snapshot.hasLogCurve ? 1 : 0)
        << "\n";

    for (std::size_t i = 0;
         i < snapshot.meanPower.size();
         ++i) {

        out << "p" << i << "="
            << snapshot.meanPower[i]
            << "\n";
    }

    for (std::size_t i = 0;
         i < snapshot.meanDb.size();
         ++i) {

        out << "d" << i << "="
            << snapshot.meanDb[i]
            << "\n";
    }

    return out.str();
}

bool ToneMatchReferenceProfileCodec::decode(
    const std::string& text,
    dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    try {
        std::istringstream in(text);
        std::string line;

        if (!std::getline(in, line) ||
            line != "125A_REFERENCE_PROFILE") {
            return false;
        }

        dsp::ToneMatchSpectrumSnapshot next {};
        bool versionSeen = false;
        int decodedVersion = 0;
        bool sampleRateSeen = false;
        bool frameCountSeen = false;
        std::array<bool,
            dsp::ToneMatchAnalyzer::kSpectrumBins> powerSeen {};
        std::array<bool,
            dsp::ToneMatchAnalyzer::kCurveBins> dbSeen {};
        bool hasLogCurveSeen = false;

        while (std::getline(in, line)) {
            if (line.empty())
                continue;

            const auto equal =
                line.find('=');

            if (equal ==
                std::string::npos) {
                return false;
            }

            const auto key =
                line.substr(0, equal);

            const auto valueText =
                line.substr(equal + 1);

            if (key == "version") {
                const auto version =
                    std::stoi(valueText);

                if (version < 1 ||
                    version > kFileVersion)
                    return false;

                decodedVersion = version;
                versionSeen = true;
                continue;
            }

            if (key == "sampleRate") {
                next.sampleRate =
                    std::stod(valueText);

                if (!std::isfinite(
                        next.sampleRate) ||
                    next.sampleRate < 8000.0 ||
                    next.sampleRate > 768000.0) {
                    return false;
                }

                sampleRateSeen = true;
                continue;
            }

            if (key == "frameCount") {
                const auto frames =
                    std::stoull(valueText);

                if (frames < 4u)
                    return false;

                next.frameCount =
                    static_cast<std::uint64_t>(
                        frames);

                frameCountSeen = true;
                continue;
            }

            if (key == "hasLogCurve") {
                const int value =
                    std::stoi(valueText);

                if (value != 0 &&
                    value != 1) {
                    return false;
                }

                next.hasLogCurve =
                    value != 0;
                hasLogCurveSeen = true;
                continue;
            }

            if (key.size() >= 2 &&
                key[0] == 'd') {

                const auto index =
                    static_cast<std::size_t>(
                        std::stoull(
                            key.substr(1)));

                if (index >=
                    next.meanDb.size() ||
                    dbSeen[index]) {
                    return false;
                }

                const double db =
                    std::stod(valueText);

                if (!std::isfinite(db))
                    return false;

                next.meanDb[index] = db;
                dbSeen[index] = true;
                continue;
            }

            if (key.size() < 2 ||
                key[0] != 'p') {
                return false;
            }

            const auto index =
                static_cast<std::size_t>(
                    std::stoull(
                        key.substr(1)));

            if (index >=
                next.meanPower.size() ||
                powerSeen[index]) {
                return false;
            }

            const double power =
                std::stod(valueText);

            if (!std::isfinite(power) ||
                power < 0.0) {
                return false;
            }

            next.meanPower[index] =
                power;

            powerSeen[index] = true;
        }

        if (!versionSeen ||
            !sampleRateSeen ||
            !frameCountSeen) {
            return false;
        }

        for (const bool seen : powerSeen) {
            if (!seen)
                return false;
        }

        if (decodedVersion >= 2) {
            if (!hasLogCurveSeen)
                return false;

            for (const bool seen : dbSeen) {
                if (!seen)
                    return false;
            }
        } else {
            next.hasLogCurve = false;
            next.meanDb.fill(0.0);
        }

        snapshot = next;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace HighGainGuitarFinisher
