#pragma once

#include "dsp/ToneMatchProfile.h"
#include "dsp/ToneMatchAnalyzer.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace HighGainGuitarFinisher {

inline constexpr const char* kToneMatchProfileMessageID =
    "125A.ToneMatch.Profile.v1";

inline constexpr const char* kToneMatchProfileMessageKey =
    "profile";

inline constexpr const char* kToneMatchCaptureStartMessageID =
    "125A.ToneMatch.Capture.Start.v1";

inline constexpr const char* kToneMatchCaptureStopMessageID =
    "125A.ToneMatch.Capture.Stop.v1";

inline constexpr const char* kToneMatchTargetSpectrumMessageID =
    "125A.ToneMatch.TargetSpectrum.v1";

inline constexpr const char* kToneMatchTargetSpectrumMessageKey =
    "spectrum";

inline constexpr const char* kToneMatchReferenceSpectrumMessageID =
    "125A.ToneMatch.ReferenceSpectrum.v1";

inline constexpr const char* kToneMatchReferenceSpectrumMessageKey =
    "reference-spectrum";

struct ToneMatchSpectrumMessagePayload {
    std::uint32_t version {3u};
    double sampleRate {44100.0};
    std::uint64_t frameCount {0u};
    std::array<double, dsp::ToneMatchAnalyzer::kSpectrumBins> meanPower {};
    std::int32_t hasLogCurve {0};
    std::array<double, dsp::ToneMatchAnalyzer::kCurveBins> meanDb {};
};

inline ToneMatchSpectrumMessagePayload
makeToneMatchSpectrumMessage(
    const dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    ToneMatchSpectrumMessagePayload payload {};
    payload.sampleRate = snapshot.sampleRate;
    payload.frameCount = snapshot.frameCount;
    payload.meanPower = snapshot.meanPower;
    payload.hasLogCurve =
        snapshot.hasLogCurve ? 1 : 0;
    payload.meanDb = snapshot.meanDb;
    return payload;
}

inline bool parseToneMatchSpectrumMessage(
    const ToneMatchSpectrumMessagePayload& payload,
    dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    if (payload.version != 3u ||
        !std::isfinite(payload.sampleRate) ||
        payload.sampleRate <= 1000.0 ||
        payload.frameCount == 0u) {
        return false;
    }

    for (const double power : payload.meanPower) {
        if (!std::isfinite(power) ||
            power < 0.0) {
            return false;
        }
    }

    if ((payload.hasLogCurve != 0 &&
         payload.hasLogCurve != 1)) {
        return false;
    }

    for (const double db : payload.meanDb) {
        if (!std::isfinite(db))
            return false;
    }

    dsp::ToneMatchSpectrumSnapshot next {};
    next.sampleRate = payload.sampleRate;
    next.frameCount = payload.frameCount;
    next.meanPower = payload.meanPower;
    next.hasLogCurve =
        payload.hasLogCurve != 0;
    next.meanDb = payload.meanDb;
    snapshot = next;
    return true;
}

struct ToneMatchReferenceSpectrumMessagePayload {
    std::uint32_t version {2u};
    std::int32_t valid {0};
    double sampleRate {44100.0};
    std::array<double, dsp::ToneMatchAnalyzer::kSpectrumBins> meanPower {};
    std::int32_t hasLogCurve {0};
    std::array<double, dsp::ToneMatchAnalyzer::kCurveBins> meanDb {};
};

inline ToneMatchReferenceSpectrumMessagePayload
makeToneMatchReferenceSpectrumMessage(
    const dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    ToneMatchReferenceSpectrumMessagePayload payload {};

    const bool valid =
        snapshot.frameCount >= 4u &&
        std::isfinite(snapshot.sampleRate) &&
        snapshot.sampleRate > 1000.0;

    payload.valid = valid ? 1 : 0;

    if (valid) {
        payload.sampleRate = snapshot.sampleRate;
        payload.meanPower = snapshot.meanPower;
        payload.hasLogCurve =
            snapshot.hasLogCurve ? 1 : 0;
        payload.meanDb = snapshot.meanDb;
    }

    return payload;
}

inline bool parseToneMatchReferenceSpectrumMessage(
    const ToneMatchReferenceSpectrumMessagePayload& payload,
    dsp::ToneMatchSpectrumSnapshot& snapshot) noexcept {

    if (payload.version != 2u ||
        (payload.valid != 0 &&
         payload.valid != 1) ||
        (payload.hasLogCurve != 0 &&
         payload.hasLogCurve != 1)) {
        return false;
    }

    if (payload.valid == 0) {
        snapshot = {};
        return true;
    }

    if (!std::isfinite(payload.sampleRate) ||
        payload.sampleRate <= 1000.0) {
        return false;
    }

    for (const double power :
         payload.meanPower) {
        if (!std::isfinite(power) ||
            power < 0.0) {
            return false;
        }
    }

    for (const double db :
         payload.meanDb) {
        if (!std::isfinite(db))
            return false;
    }

    dsp::ToneMatchSpectrumSnapshot next {};
    next.sampleRate = payload.sampleRate;
    next.frameCount = 4u;
    next.meanPower = payload.meanPower;
    next.hasLogCurve =
        payload.hasLogCurve != 0;
    next.meanDb = payload.meanDb;
    snapshot = next;
    return true;
}

struct ToneMatchProfileMessagePayload {
    std::uint32_t version {2u};
    std::int32_t valid {0};
    std::int32_t firValid {0};
    std::array<double, 4 + 3 * dsp::kToneMatchPeakCount> values {};
    std::array<double, dsp::kToneMatchFirTapCount> firTaps {};
};

inline ToneMatchProfileMessagePayload
makeToneMatchProfileMessage(
    const dsp::ToneMatchProfile& profile) noexcept {

    ToneMatchProfileMessagePayload payload {};
    payload.valid = profile.valid ? 1 : 0;
    payload.firValid = profile.firValid ? 1 : 0;
    payload.firTaps = profile.firTaps;

    std::size_t i = 0;
    payload.values[i++] = profile.lowShelfFrequencyHz;
    payload.values[i++] = profile.lowShelfGainDb;

    for (const auto& peak : profile.peaks) {
        payload.values[i++] = peak.frequencyHz;
        payload.values[i++] = peak.q;
        payload.values[i++] = peak.gainDb;
    }

    payload.values[i++] = profile.highShelfFrequencyHz;
    payload.values[i++] = profile.highShelfGainDb;

    return payload;
}

inline bool parseToneMatchProfileMessage(
    const ToneMatchProfileMessagePayload& payload,
    dsp::ToneMatchProfile& profile) noexcept {

    if (payload.version != 2u ||
        (payload.valid != 0 &&
         payload.valid != 1) ||
        (payload.firValid != 0 &&
         payload.firValid != 1)) {
        return false;
    }

    for (const double value : payload.values) {
        if (!std::isfinite(value))
            return false;
    }

    for (const double tap :
         payload.firTaps) {
        if (!std::isfinite(tap))
            return false;
    }

    dsp::ToneMatchProfile next {};
    next.valid = payload.valid != 0;
    next.firValid = payload.firValid != 0;
    next.firTaps = payload.firTaps;

    std::size_t i = 0;
    next.lowShelfFrequencyHz = payload.values[i++];
    next.lowShelfGainDb = payload.values[i++];

    for (auto& peak : next.peaks) {
        peak.frequencyHz = payload.values[i++];
        peak.q = payload.values[i++];
        peak.gainDb = payload.values[i++];
    }

    next.highShelfFrequencyHz = payload.values[i++];
    next.highShelfGainDb = payload.values[i++];

    profile = next;
    return true;
}

} // namespace HighGainGuitarFinisher
