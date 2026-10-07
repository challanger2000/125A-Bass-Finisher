#pragma once

#include <atomic>
#include <cstddef>
#include <memory>

namespace HighGainGuitarFinisher::dsp {

class ToneMatchCaptureBuffer {
public:
    static constexpr std::size_t kCapacity = 1u << 21; // ~10.9 s at 192 kHz

    bool prepare() {
        try {
            left_ =
                std::make_unique<float[]>(
                    kCapacity);

            right_ =
                std::make_unique<float[]>(
                    kCapacity);
        } catch (...) {
            left_.reset();
            right_.reset();
            prepared_.store(
                false,
                std::memory_order_release);
            return false;
        }

        writeIndex_.store(
            0u,
            std::memory_order_relaxed);

        readIndex_.store(
            0u,
            std::memory_order_relaxed);

        overflow_.store(
            false,
            std::memory_order_relaxed);

        prepared_.store(
            true,
            std::memory_order_release);

        return true;
    }

    bool push(
        const double* left,
        const double* right,
        std::size_t count) noexcept {

        return pushBlock(
            left,
            right,
            count);
    }

    template <typename Sample>
    bool pushBlock(
        const Sample* left,
        const Sample* right,
        std::size_t count) noexcept {

        if (!prepared_.load(
                std::memory_order_acquire) ||
            !left_ ||
            !right_) {
            return false;
        }

        if (!left || count == 0)
            return true;

        auto write =
            writeIndex_.load(
                std::memory_order_relaxed);

        const auto read =
            readIndex_.load(
                std::memory_order_acquire);

        const auto used =
            distance(read, write);

        const auto free =
            kCapacity - 1u - used;

        if (count > free) {
            overflow_.store(
                true,
                std::memory_order_release);
            return false;
        }

        for (std::size_t i = 0;
             i < count;
             ++i) {

            const auto index =
                (write + i) &
                (kCapacity - 1u);

            left_[index] =
                static_cast<float>(
                    left[i]);

            right_[index] =
                right
                    ? static_cast<float>(
                        right[i])
                    : static_cast<float>(
                        left[i]);
        }

        writeIndex_.store(
            (write + count) &
                (kCapacity - 1u),
            std::memory_order_release);

        return true;
    }

    std::size_t pop(
        double* left,
        double* right,
        std::size_t maximum) noexcept {

        if (!prepared_.load(
                std::memory_order_acquire) ||
            !left_ ||
            !right_ ||
            !left ||
            !right ||
            maximum == 0) {
            return 0;
        }

        auto read =
            readIndex_.load(
                std::memory_order_relaxed);

        const auto write =
            writeIndex_.load(
                std::memory_order_acquire);

        const auto available =
            distance(read, write);

        const auto count =
            available < maximum
                ? available
                : maximum;

        for (std::size_t i = 0;
             i < count;
             ++i) {

            const auto index =
                (read + i) &
                (kCapacity - 1u);

            left[i] =
                left_[index];

            right[i] =
                right_[index];
        }

        readIndex_.store(
            (read + count) &
                (kCapacity - 1u),
            std::memory_order_release);

        return count;
    }

    void resetConsumerSide() noexcept {
        const auto write =
            writeIndex_.load(
                std::memory_order_acquire);

        readIndex_.store(
            write,
            std::memory_order_release);

        overflow_.store(
            false,
            std::memory_order_release);
    }

    bool overflowed() const noexcept {
        return overflow_.load(
            std::memory_order_acquire);
    }

    bool prepared() const noexcept {
        return prepared_.load(
            std::memory_order_acquire);
    }

    std::size_t writableFrames() const noexcept {
        const auto write =
            writeIndex_.load(
                std::memory_order_acquire);

        const auto read =
            readIndex_.load(
                std::memory_order_acquire);

        return
            kCapacity - 1u -
            distance(
                read,
                write);
    }

private:
    static_assert(
        (kCapacity &
         (kCapacity - 1u)) == 0u,
        "Capacity must be power of two");

    static constexpr std::size_t distance(
        std::size_t read,
        std::size_t write) noexcept {

        return
            (write - read) &
            (kCapacity - 1u);
    }

    std::unique_ptr<float[]> left_ {};
    std::unique_ptr<float[]> right_ {};

    std::atomic<std::size_t> writeIndex_ {0u};
    std::atomic<std::size_t> readIndex_ {0u};
    std::atomic<bool> overflow_ {false};
    std::atomic<bool> prepared_ {false};
};

} // namespace HighGainGuitarFinisher::dsp
