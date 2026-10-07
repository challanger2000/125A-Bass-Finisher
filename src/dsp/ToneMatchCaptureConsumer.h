#pragma once

#include "ToneMatchAnalyzer.h"
#include "ToneMatchCaptureBuffer.h"

#include <array>
#include <cstddef>

namespace HighGainGuitarFinisher::dsp {

class ToneMatchCaptureConsumer {
public:
    static constexpr std::size_t kChunkSize = 2048;

    void prepare(double sampleRate) noexcept {
        analyzer_.prepare(sampleRate);
        totalSamples_ = 0;
    }

    void reset() noexcept {
        analyzer_.reset();
        totalSamples_ = 0;
    }

    std::size_t drain(
        ToneMatchCaptureBuffer& buffer) noexcept {

        std::size_t drained = 0;

        for (;;) {
            const auto count =
                buffer.pop(
                    left_.data(),
                    right_.data(),
                    kChunkSize);

            if (count == 0)
                break;

            analyzer_.pushStereo(
                left_.data(),
                right_.data(),
                count);

            drained += count;
            totalSamples_ += count;
        }

        return drained;
    }

    const ToneMatchAnalyzer& analyzer() const noexcept {
        return analyzer_;
    }

    ToneMatchAnalyzer& analyzer() noexcept {
        return analyzer_;
    }

    std::size_t totalSamples() const noexcept {
        return totalSamples_;
    }

private:
    ToneMatchAnalyzer analyzer_ {};
    std::array<double, kChunkSize> left_ {};
    std::array<double, kChunkSize> right_ {};
    std::size_t totalSamples_ {0};
};

} // namespace HighGainGuitarFinisher::dsp
