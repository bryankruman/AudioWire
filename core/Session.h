// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Types.h"
#include <chrono>
namespace aw {
class Session {
  public:
    using Clock = std::chrono::steady_clock;
    State state() const {
        return state_;
    }
    void discover() {
        if (state_ == State::idle)
            state_ = State::discovering;
    }
    void beginPairing();
    void connected(Clock::time_point);
    void received(Clock::time_point);
    void lost(Clock::time_point);
    void disconnect();
    bool retryDue(Clock::time_point) const;
    void retryFailed(Clock::time_point);
    void networkAvailable(Clock::time_point now) {
        if (state_ == State::reconnecting)
            retry_ = now;
    }
    bool timedOut(Clock::time_point) const;
    int attempts() const {
        return attempts_;
    }

  private:
    State state_ = State::idle;
    int attempts_ = 0;
    Clock::time_point retry_{}, lastReceived_{};
};
} // namespace aw
