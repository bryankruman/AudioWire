// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Types.h"
#include <optional>
namespace aw {
enum class ControlKind : std::uint8_t {
    mute = 1,
    volume,
    mode,
    quality,
    ping,
    pong,
    rename,
    battery,
    bye,
    driver
};
struct Control {
    ControlKind kind;
    double value = 0;
    std::string text;
};
Bytes encodeControl(const Control&);
std::optional<Control> decodeControl(const Bytes&);
} // namespace aw
