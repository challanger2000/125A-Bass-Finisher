#pragma once

#include "ToneMatchCaptureBuffer.h"
#include "ToneMatchCaptureConsumer.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace HighGainGuitarFinisher::dsp {

class ToneMatchCaptureWorker {
public:
    ToneMatchCaptureWorker() = default;

    ~ToneMatchCaptureWorker() {
        stopAndDrain();
    }

    ToneMatchCaptureWorker(
        const ToneMatchCaptureWorker&) = delete;

    ToneMatchCaptureWorker& operator=(
        const ToneMatchCaptureWorker&) = delete;

    bool start(
        ToneMatchCaptureBuffer& buffer,
        double sampleRate) {

        stopAndDrain();

        buffer_ = &buffer;
        consumer_.prepare(sampleRate);
        running_.store(
            true,
            std::memory_order_release);

        try {
            worker_ =
                std::thread(
                    [this]() noexcept {
                        run();
                    });
        } catch (...) {
            running_.store(
                false,
                std::memory_order_release);
            buffer_ = nullptr;
            return false;
        }

        return true;
    }

    void stopAndDrain() noexcept {
        running_.store(
            false,
            std::memory_order_release);

        if (worker_.joinable())
            worker_.join();

        if (buffer_)
            consumer_.drain(*buffer_);
    }

    void reset() noexcept {
        stopAndDrain();
        consumer_.reset();
        buffer_ = nullptr;
    }

    bool running() const noexcept {
        return running_.load(
            std::memory_order_acquire);
    }

    const ToneMatchCaptureConsumer& consumer() const noexcept {
        return consumer_;
    }

    ToneMatchCaptureConsumer& consumer() noexcept {
        return consumer_;
    }

private:
    void run() noexcept {
        while (running_.load(
                   std::memory_order_acquire)) {

            const auto drained =
                buffer_
                    ? consumer_.drain(
                        *buffer_)
                    : 0u;

            if (drained == 0u) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(
                        2));
            } else {
                std::this_thread::yield();
            }
        }
    }

    ToneMatchCaptureBuffer* buffer_ {nullptr};
    ToneMatchCaptureConsumer consumer_ {};
    std::atomic<bool> running_ {false};
    std::thread worker_ {};
};

} // namespace HighGainGuitarFinisher::dsp
