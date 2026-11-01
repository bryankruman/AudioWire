// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace aw {
using Bytes = std::vector<std::uint8_t>;
using Key = std::array<std::uint8_t, 32>;
enum class Role { pc, phone };
enum class Mode { speakerphone, speaker, microphone };
enum class Quality { balanced, high, maximum, voice };
enum class State { idle, discovering, pairing, connected, reconnecting };
struct Device {
    std::string id, name, host;
    std::uint16_t port = 0;
};
struct AudioPreset {
    int speakerBitrate, micBitrate, bufferMs;
    bool pcm;
};
inline AudioPreset preset(Quality q) {
    switch (q) {
    case Quality::high:
        return {160000, 64000, 20, false};
    case Quality::maximum:
        return {0, 0, 20, true};
    case Quality::voice:
        return {32000, 32000, 30, false};
    default:
        return {96000, 48000, 20, false};
    }
}
inline std::string displayName(const std::string& value) {
    if (value.size() <= 64)
        return value;
    size_t end = 64;
    while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80)
        --end;
    return value.substr(0, end);
}
inline bool plays(Mode m) {
    return m != Mode::microphone;
}
inline bool records(Mode m) {
    return m != Mode::speaker;
}
inline const char* stateName(State s) {
    switch (s) {
    case State::idle:
        return "Waiting for a connection";
    case State::discovering:
        return "Looking for PCs";
    case State::pairing:
        return "Enter the code shown on your PC";
    case State::connected:
        return "Connected";
    case State::reconnecting:
        return "Reconnecting...";
    }
    return "Waiting";
}
} // namespace aw
