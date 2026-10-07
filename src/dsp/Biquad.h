#pragma once

#include <algorithm>
#include <cmath>

namespace HighGainGuitarFinisher::dsp {

struct BiquadCoefficients {
    double b0 {1.0};
    double b1 {0.0};
    double b2 {0.0};
    double a1 {0.0};
    double a2 {0.0};
};

class Biquad {
public:
    void reset() noexcept {
        z1_ = 0.0;
        z2_ = 0.0;
    }

    void setCoefficients(const BiquadCoefficients& coefficients) noexcept {
        c_ = coefficients;
    }

    double process(double input) noexcept {
        if (!std::isfinite(input))
            input = 0.0;

        const double output =
            c_.b0 * input + z1_;

        if (!std::isfinite(output)) {
            reset();
            return 0.0;
        }

        double nextZ1 =
            c_.b1 * input -
            c_.a1 * output +
            z2_;

        double nextZ2 =
            c_.b2 * input -
            c_.a2 * output;

        if (!std::isfinite(nextZ1) ||
            !std::isfinite(nextZ2)) {
            reset();
            return 0.0;
        }

        // Explicitly collapse numerically irrelevant residual state so filter
        // tails cannot enter the subnormal range and create callback spikes.
        constexpr double kStateFloor = 1.0e-30;

        z1_ =
            std::abs(nextZ1) < kStateFloor
                ? 0.0
                : nextZ1;

        z2_ =
            std::abs(nextZ2) < kStateFloor
                ? 0.0
                : nextZ2;

        return output;
    }

private:
    BiquadCoefficients c_ {};
    double z1_ {0.0};
    double z2_ {0.0};
};

inline BiquadCoefficients makeHighPass(double sampleRate,
                                       double frequency,
                                       double q) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double fs = std::max(sampleRate, 1000.0);
    const double f = std::clamp(frequency, 10.0, fs * 0.45);
    const double qq = std::max(q, 0.05);
    const double w0 = 2.0 * pi * f / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * qq);

    const double a0 = 1.0 + alpha;
    return {
        (1.0 + cosW0) / (2.0 * a0),
        -(1.0 + cosW0) / a0,
        (1.0 + cosW0) / (2.0 * a0),
        -2.0 * cosW0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoefficients makeLowPass(double sampleRate,
                                      double frequency,
                                      double q) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double fs = std::max(sampleRate, 1000.0);
    const double f = std::clamp(frequency, 10.0, fs * 0.45);
    const double qq = std::max(q, 0.05);
    const double w0 = 2.0 * pi * f / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * qq);

    const double a0 = 1.0 + alpha;
    return {
        (1.0 - cosW0) / (2.0 * a0),
        (1.0 - cosW0) / a0,
        (1.0 - cosW0) / (2.0 * a0),
        -2.0 * cosW0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoefficients makeBandPass(double sampleRate,
                                       double frequency,
                                       double q) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double fs = std::max(sampleRate, 1000.0);
    const double f = std::clamp(frequency, 10.0, fs * 0.45);
    const double qq = std::max(q, 0.05);
    const double w0 = 2.0 * pi * f / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * qq);

    const double a0 = 1.0 + alpha;
    return {
        alpha / a0,
        0.0,
        -alpha / a0,
        -2.0 * cosW0 / a0,
        (1.0 - alpha) / a0
    };
}

inline BiquadCoefficients makeLowShelf(double sampleRate,
                                       double frequency,
                                       double gainDb) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double fs = std::max(sampleRate, 1000.0);
    const double f = std::clamp(frequency, 10.0, fs * 0.45);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * f / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha =
        0.5 * sinW0 *
        std::sqrt(2.0);
    const double beta =
        2.0 * std::sqrt(A) * alpha;

    const double a0 =
        (A + 1.0) +
        (A - 1.0) * cosW0 +
        beta;

    return {
        A * ((A + 1.0) -
             (A - 1.0) * cosW0 +
             beta) / a0,
        2.0 * A * ((A - 1.0) -
                   (A + 1.0) * cosW0) / a0,
        A * ((A + 1.0) -
             (A - 1.0) * cosW0 -
             beta) / a0,
        -2.0 * ((A - 1.0) +
                (A + 1.0) * cosW0) / a0,
        ((A + 1.0) +
         (A - 1.0) * cosW0 -
         beta) / a0
    };
}

inline BiquadCoefficients makeHighShelf(double sampleRate,
                                        double frequency,
                                        double gainDb) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double fs = std::max(sampleRate, 1000.0);
    const double f = std::clamp(frequency, 10.0, fs * 0.45);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * f / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha =
        0.5 * sinW0 *
        std::sqrt(2.0);
    const double beta =
        2.0 * std::sqrt(A) * alpha;

    const double a0 =
        (A + 1.0) -
        (A - 1.0) * cosW0 +
        beta;

    return {
        A * ((A + 1.0) +
             (A - 1.0) * cosW0 +
             beta) / a0,
        -2.0 * A * ((A - 1.0) +
                    (A + 1.0) * cosW0) / a0,
        A * ((A + 1.0) +
             (A - 1.0) * cosW0 -
             beta) / a0,
        2.0 * ((A - 1.0) -
               (A + 1.0) * cosW0) / a0,
        ((A + 1.0) -
         (A - 1.0) * cosW0 -
         beta) / a0
    };
}

inline BiquadCoefficients makePeaking(double sampleRate,
                                      double frequency,
                                      double q,
                                      double gainDb) noexcept {
    constexpr double pi = 3.141592653589793238462643383279502884;
    const double fs = std::max(sampleRate, 1000.0);
    const double f = std::clamp(frequency, 10.0, fs * 0.45);
    const double qq = std::max(q, 0.05);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * f / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * qq);

    const double a0 = 1.0 + alpha / A;
    return {
        (1.0 + alpha * A) / a0,
        (-2.0 * cosW0) / a0,
        (1.0 - alpha * A) / a0,
        (-2.0 * cosW0) / a0,
        (1.0 - alpha / A) / a0
    };
}

} // namespace HighGainGuitarFinisher::dsp
