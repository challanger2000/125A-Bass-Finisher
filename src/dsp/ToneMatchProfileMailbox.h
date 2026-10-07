#pragma once

#include "ToneMatchProfile.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace HighGainGuitarFinisher::dsp {

class ToneMatchProfileMailbox {
public:
    bool push(
        const ToneMatchProfile& profile) noexcept {

        const auto write =
            writeIndex_.load(
                std::memory_order_relaxed);

        const auto next =
            increment(write);

        const auto read =
            readIndex_.load(
                std::memory_order_acquire);

        if (next == read)
            return false;

        slots_[write] = profile;

        writeIndex_.store(
            next,
            std::memory_order_release);

        return true;
    }

    bool popLatest(
        ToneMatchProfile& profile) noexcept {

        auto read =
            readIndex_.load(
                std::memory_order_relaxed);

        const auto write =
            writeIndex_.load(
                std::memory_order_acquire);

        if (read == write)
            return false;

        std::size_t latest = read;

        for (std::size_t i = 0;
             i < kSlotCount - 1 &&
             read != write;
             ++i) {

            latest = read;
            read = increment(read);
        }

        profile = slots_[latest];

        readIndex_.store(
            write,
            std::memory_order_release);

        return true;
    }

    void clearConsumerSide() noexcept {
        const auto write =
            writeIndex_.load(
                std::memory_order_acquire);

        readIndex_.store(
            write,
            std::memory_order_release);
    }

private:
    static constexpr std::size_t kSlotCount = 4;

    static constexpr std::size_t
    increment(std::size_t index) noexcept {
        return (index + 1u) %
            kSlotCount;
    }

    std::array<
        ToneMatchProfile,
        kSlotCount> slots_ {};

    std::atomic<std::size_t>
        writeIndex_ {0u};

    std::atomic<std::size_t>
        readIndex_ {0u};
};

} // namespace HighGainGuitarFinisher::dsp
