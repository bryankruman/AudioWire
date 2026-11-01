// SPDX-License-Identifier: GPL-3.0-or-later
// AOO integration adapted from SonoBus by Jesse Chappell (Sonosaurus LLC).
#pragma once
#include "Types.h"
#include <aoo/aoo.hpp>
#include <atomic>
#include <functional>
namespace aw {
class Transport {
  public:
    Transport(Role, Quality, double rate, int maxFrames, std::function<void(const char*, int)> send);
    ~Transport();
    void start();
    void stop();
    void setQuality(Quality);
    void setBufferMs(int);
    void process(const float* const* input, float* const* output, int frames) noexcept;
    void receive(const char*, int);
    void sendPending();
    std::uint64_t lostBlocks() const {
        return lost_.load();
    }
    int bufferMs() const {
        return buffer_.load();
    }

  private:
    static int32_t reply(void*, const char*, int32_t);
    static int32_t events(void*, const aoo_event**, int32_t);
    aoo::isource::pointer source_;
    aoo::isink::pointer sink_;
    Role role_;
    int channels_, rate_;
    std::function<void(const char*, int)> send_;
    std::atomic<std::uint64_t> lost_{0};
    std::atomic<int> buffer_{20};
    std::uint64_t lastLoss_ = 0;
    int stableTicks_ = 0, baselineMs_ = 20;
    bool automatic_ = true;
};
} // namespace aw
