#pragma once

namespace HighGainGuitarFinisher {

enum class ToneMatchStatus {
    Empty,
    ReferenceReady,
    Analyzing,
    Matching,
    Ready,
    Error
};

inline const char* toneMatchStatusText(
    ToneMatchStatus status) noexcept {

    switch (status) {
        case ToneMatchStatus::ReferenceReady:
            return "REFERENCE";
        case ToneMatchStatus::Analyzing:
            return "ANALYZING";
        case ToneMatchStatus::Matching:
            return "MATCHING";
        case ToneMatchStatus::Ready:
            return "READY";
        case ToneMatchStatus::Error:
            return "ERROR";
        case ToneMatchStatus::Empty:
        default:
            return "EMPTY";
    }
}

} // namespace HighGainGuitarFinisher
