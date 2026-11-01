// SPDX-License-Identifier: GPL-3.0-or-later
#include "Session.h"
#include <algorithm>
#include <stdexcept>
namespace aw {
void Session::beginPairing() {
    if (state_ == State::connected)
        throw std::logic_error("Disconnect before pairing another device");
    state_ = State::pairing;
}
void Session::connected(Clock::time_point now) {
    state_ = State::connected;
    attempts_ = 0;
    lastReceived_ = now;
}
void Session::received(Clock::time_point now) {
    lastReceived_ = now;
}
void Session::lost(Clock::time_point now) {
    if (state_ != State::connected)
        return;
    state_ = State::reconnecting;
    attempts_ = 0;
    retry_ = now + std::chrono::seconds(1);
}
void Session::disconnect() {
    state_ = State::idle;
    attempts_ = 0;
}
bool Session::retryDue(Clock::time_point now) const {
    return state_ == State::reconnecting && now >= retry_;
}
void Session::retryFailed(Clock::time_point now) {
    attempts_ = std::min(attempts_ + 1, 4);
    const int seconds = std::min(1 << attempts_, 15);
    retry_ = now + std::chrono::seconds(seconds);
}
bool Session::timedOut(Clock::time_point now) const {
    return state_ == State::connected && now - lastReceived_ > std::chrono::seconds(3);
}
} // namespace aw
