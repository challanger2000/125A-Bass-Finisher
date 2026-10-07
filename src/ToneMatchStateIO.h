#pragma once

#include "dsp/ToneMatchProfile.h"
#include "dsp/ToneMatchAnalyzer.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher {

struct ToneMatchStatePayload {
    double amount {0.0};
    dsp::ToneMatchProfile profile {};
};

inline bool readToneMatchState(
    Steinberg::IBStreamer& stream,
    ToneMatchStatePayload& payload) noexcept {

    ToneMatchStatePayload next {};

    if (!stream.readDouble(next.amount) ||
        !std::isfinite(next.amount)) {
        return false;
    }

    Steinberg::int32 valid = 0;

    if (!stream.readInt32(valid) ||
        (valid != 0 && valid != 1)) {
        return false;
    }

    next.profile.valid =
        valid != 0;

    auto readFinite =
        [&stream](double& value) noexcept {
            return
                stream.readDouble(value) &&
                std::isfinite(value);
        };

    if (!readFinite(
            next.profile.lowShelfFrequencyHz) ||
        !readFinite(
            next.profile.lowShelfGainDb)) {
        return false;
    }

    for (auto& peak :
         next.profile.peaks) {

        if (!readFinite(peak.frequencyHz) ||
            !readFinite(peak.q) ||
            !readFinite(peak.gainDb)) {
            return false;
        }
    }

    if (!readFinite(
            next.profile.highShelfFrequencyHz) ||
        !readFinite(
            next.profile.highShelfGainDb)) {
        return false;
    }

    next.amount =
        std::clamp(
            next.amount,
            0.0,
            1.0);

    payload = next;
    return true;
}

inline bool writeToneMatchState(
    Steinberg::IBStreamer& stream,
    const ToneMatchStatePayload& payload) noexcept {

    if (!stream.writeDouble(
            std::clamp(
                std::isfinite(payload.amount)
                    ? payload.amount
                    : 0.0,
                0.0,
                1.0))) {
        return false;
    }

    if (!stream.writeInt32(
            payload.profile.valid
                ? 1
                : 0)) {
        return false;
    }

    auto writeFinite =
        [&stream](double value) noexcept {
            return
                std::isfinite(value) &&
                stream.writeDouble(value);
        };

    if (!writeFinite(
            payload.profile.lowShelfFrequencyHz) ||
        !writeFinite(
            payload.profile.lowShelfGainDb)) {
        return false;
    }

    for (const auto& peak :
         payload.profile.peaks) {

        if (!writeFinite(peak.frequencyHz) ||
            !writeFinite(peak.q) ||
            !writeFinite(peak.gainDb)) {
            return false;
        }
    }

    return
        writeFinite(
            payload.profile.highShelfFrequencyHz) &&
        writeFinite(
            payload.profile.highShelfGainDb);
}

inline bool writeToneMatchReferenceState(
    Steinberg::IBStreamer& stream,
    const dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    const bool valid =
        snapshot.frameCount >= 4u &&
        std::isfinite(snapshot.sampleRate) &&
        snapshot.sampleRate > 1000.0;

    if (!stream.writeInt32(
            valid ? 1 : 0)) {
        return false;
    }

    if (!valid)
        return true;

    if (!stream.writeDouble(
            snapshot.sampleRate)) {
        return false;
    }

    for (const double power :
         snapshot.meanPower) {
        if (!std::isfinite(power) ||
            power < 0.0 ||
            !stream.writeDouble(power)) {
            return false;
        }
    }

    return true;
}

inline bool readToneMatchReferenceState(
    Steinberg::IBStreamer& stream,
    dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    Steinberg::int32 valid = 0;

    if (!stream.readInt32(valid) ||
        (valid != 0 && valid != 1)) {
        return false;
    }

    if (valid == 0) {
        snapshot = {};
        return true;
    }

    dsp::ToneMatchSpectrumSnapshot next {};

    if (!stream.readDouble(
            next.sampleRate) ||
        !std::isfinite(
            next.sampleRate) ||
        next.sampleRate <= 1000.0) {
        return false;
    }

    for (double& power :
         next.meanPower) {

        if (!stream.readDouble(power) ||
            !std::isfinite(power) ||
            power < 0.0) {
            return false;
        }
    }

    next.frameCount = 4u;
    snapshot = next;
    return true;
}

} // namespace HighGainGuitarFinisher
