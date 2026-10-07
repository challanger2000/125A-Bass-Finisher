#include "ToneMatchProfileCodec.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace HighGainGuitarFinisher {

namespace {

bool finite(double value) noexcept {
    return std::isfinite(value);
}

bool parseKeyValue(
    const std::string& line,
    std::string& key,
    double& value) {

    const auto pos =
        line.find('=');

    if (pos == std::string::npos)
        return false;

    key = line.substr(0, pos);

    try {
        std::size_t used = 0;
        value =
            std::stod(
                line.substr(pos + 1),
                &used);

        return used > 0 &&
            finite(value);
    } catch (...) {
        return false;
    }
}

}

std::string ToneMatchProfileCodec::encode(
    const dsp::ToneMatchProfile& profile) {

    std::ostringstream out;

    out << "125A_TONE_PROFILE\n";
    out << "version="
        << kFileVersion
        << "\n";

    out << std::setprecision(17);

    out << "valid="
        << (profile.valid ? 1 : 0)
        << "\n";

    out << "firValid="
        << (profile.firValid ? 1 : 0)
        << "\n";

    out << "lowShelfHz="
        << profile.lowShelfFrequencyHz
        << "\n";

    out << "lowShelfDb="
        << profile.lowShelfGainDb
        << "\n";

    for (std::size_t i = 0;
         i < profile.peaks.size();
         ++i) {

        const auto& peak =
            profile.peaks[i];

        out << "peak" << i << "Hz="
            << peak.frequencyHz
            << "\n";

        out << "peak" << i << "Q="
            << peak.q
            << "\n";

        out << "peak" << i << "Db="
            << peak.gainDb
            << "\n";
    }

    out << "highShelfHz="
        << profile.highShelfFrequencyHz
        << "\n";

    out << "highShelfDb="
        << profile.highShelfGainDb
        << "\n";

    for (std::size_t i = 0;
         i < profile.firTaps.size();
         ++i) {

        out << "fir" << i << "="
            << profile.firTaps[i]
            << "\n";
    }

    return out.str();
}

bool ToneMatchProfileCodec::decode(
    const std::string& text,
    dsp::ToneMatchProfile& profile) noexcept {

    try {
        std::istringstream input(text);
        std::string line;

        if (!std::getline(input, line) ||
            line != "125A_TONE_PROFILE") {
            return false;
        }

        dsp::ToneMatchProfile next {};
        bool sawVersion = false;
        int decodedVersion = 0;
        bool sawValid = false;
        bool sawFirValid = false;
        bool sawLowHz = false;
        bool sawLowDb = false;
        bool sawHighHz = false;
        bool sawHighDb = false;

        std::array<bool,
            dsp::kToneMatchPeakCount> sawPeakHz {};
        std::array<bool,
            dsp::kToneMatchPeakCount> sawPeakQ {};
        std::array<bool,
            dsp::kToneMatchPeakCount> sawPeakDb {};

        while (std::getline(input, line)) {
            if (line.empty())
                continue;

            std::string key;
            double value = 0.0;

            if (!parseKeyValue(
                    line,
                    key,
                    value)) {
                return false;
            }

            if (key == "version") {
                decodedVersion =
                    static_cast<int>(
                        std::llround(value));

                if (decodedVersion < 1 ||
                    decodedVersion > kFileVersion) {
                    return false;
                }

                sawVersion = true;
                continue;
            }

            if (key == "valid") {
                if (value != 0.0 &&
                    value != 1.0) {
                    return false;
                }

                next.valid =
                    value >= 0.5;
                sawValid = true;
                continue;
            }

            if (key == "firValid") {
                if (value != 0.0 &&
                    value != 1.0) {
                    return false;
                }

                next.firValid =
                    value >= 0.5;

                sawFirValid = true;
                continue;
            }

            if (key.rfind("fir", 0) == 0 &&
                key.size() > 3) {

                const auto index =
                    static_cast<std::size_t>(
                        std::stoull(
                            key.substr(3)));

                if (index >=
                    next.firTaps.size()) {
                    return false;
                }

                next.firTaps[index] =
                    value;
                continue;
            }

            if (key == "lowShelfHz") {
                next.lowShelfFrequencyHz =
                    value;
                sawLowHz = true;
                continue;
            }

            if (key == "lowShelfDb") {
                next.lowShelfGainDb =
                    value;
                sawLowDb = true;
                continue;
            }

            if (key == "highShelfHz") {
                next.highShelfFrequencyHz =
                    value;
                sawHighHz = true;
                continue;
            }

            if (key == "highShelfDb") {
                next.highShelfGainDb =
                    value;
                sawHighDb = true;
                continue;
            }

            bool matched = false;

            for (std::size_t i = 0;
                 i < next.peaks.size();
                 ++i) {

                const std::string prefix =
                    "peak" +
                    std::to_string(i);

                if (key == prefix + "Hz") {
                    next.peaks[i].frequencyHz =
                        value;
                    sawPeakHz[i] = true;
                    matched = true;
                    break;
                }

                if (key == prefix + "Q") {
                    next.peaks[i].q =
                        value;
                    sawPeakQ[i] = true;
                    matched = true;
                    break;
                }

                if (key == prefix + "Db") {
                    next.peaks[i].gainDb =
                        value;
                    sawPeakDb[i] = true;
                    matched = true;
                    break;
                }
            }

            if (!matched)
                return false;
        }

        if (!sawVersion ||
            !sawValid ||
            (decodedVersion >= 3 &&
             !sawFirValid) ||
            !sawLowHz ||
            !sawLowDb ||
            !sawHighHz ||
            !sawHighDb) {
            return false;
        }

        const std::size_t requiredPeakCount =
            decodedVersion >= 2
                ? next.peaks.size()
                : 16u;

        for (std::size_t i = 0;
             i < requiredPeakCount;
             ++i) {

            if (!sawPeakHz[i] ||
                !sawPeakQ[i] ||
                !sawPeakDb[i]) {
                return false;
            }
        }

        for (std::size_t i = requiredPeakCount;
             i < next.peaks.size();
             ++i) {

            next.peaks[i].frequencyHz = 1000.0;
            next.peaks[i].q = 1.0;
            next.peaks[i].gainDb = 0.0;
        }

        if (decodedVersion < 3) {
            next.firValid = false;
            next.firTaps.fill(0.0);
        }

        profile = next;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace HighGainGuitarFinisher
