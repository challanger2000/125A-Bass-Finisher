#pragma once

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher {

class BypassCrossfade {
public:
    void prepare(
        double sampleRate,
        bool bypassed) noexcept {

        const double fs =
            (std::isfinite(sampleRate) &&
             sampleRate > 1000.0)
                ? sampleRate
                : 44100.0;

        constexpr double
            transitionSeconds = 0.005;

        step_ =
            1.0 /
            std::max(
                1.0,
                fs * transitionSeconds);

        reset(bypassed);
    }

    void reset(bool bypassed) noexcept {
        target_ =
            bypassed ? 1.0 : 0.0;
        mix_ = target_;
    }

    void setBypassed(
        bool bypassed) noexcept {

        target_ =
            bypassed ? 1.0 : 0.0;
    }

    double advance() noexcept {
        if (mix_ < target_) {
            mix_ =
                std::min(
                    target_,
                    mix_ + step_);
        } else if (mix_ > target_) {
            mix_ =
                std::max(
                    target_,
                    mix_ - step_);
        }

        if (std::abs(
                mix_ - target_) <=
            step_ * 0.5) {
            mix_ = target_;
        }

        return mix_;
    }

    double mix() const noexcept {
        return mix_;
    }

    bool fullyBypassed() const noexcept {
        return mix_ >= 1.0;
    }

private:
    double step_ {1.0};
    double mix_ {0.0};
    double target_ {0.0};
};

} // namespace HighGainGuitarFinisher
