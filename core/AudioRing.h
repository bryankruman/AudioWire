// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
namespace aw {
// One producer and one consumer. Overflow drops new samples; underrun emits silence.
template <std::size_t Capacity> class AudioRing {
  public:
    std::size_t write(const float* data, std::size_t n) noexcept {
        auto w = write_.load(std::memory_order_relaxed), r = read_.load(std::memory_order_acquire);
        const auto room = Capacity - (w - r);
        if (n > room)
            n = room;
        for (std::size_t i = 0; i < n; ++i)
            buffer_[(w + i) % Capacity] = data[i];
        write_.store(w + n, std::memory_order_release);
        return n;
    }
    std::size_t read(float* data, std::size_t n) noexcept {
        auto r = read_.load(std::memory_order_relaxed), w = write_.load(std::memory_order_acquire);
        auto count = n;
        if (count > w - r)
            count = w - r;
        for (std::size_t i = 0; i < count; ++i)
            data[i] = buffer_[(r + i) % Capacity];
        for (std::size_t i = count; i < n; ++i)
            data[i] = 0;
        read_.store(r + count, std::memory_order_release);
        return count;
    }

  private:
    std::array<float, Capacity> buffer_{};
    alignas(64) std::atomic<std::size_t> read_{0};
    alignas(64) std::atomic<std::size_t> write_{0};
};
} // namespace aw
