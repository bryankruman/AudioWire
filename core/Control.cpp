// SPDX-License-Identifier: GPL-3.0-or-later
#include "Control.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace aw {
namespace {
bool valid(const Control& c) {
    if (!std::isfinite(c.value) || c.text.size() > 128)
        return false;
    switch (c.kind) {
    case ControlKind::mute:
    case ControlKind::driver:
        return c.value == 0 || c.value == 1;
    case ControlKind::volume:
        return c.value >= 0 && c.value <= 1;
    case ControlKind::mode:
        return c.value >= 0 && c.value <= 2 && std::floor(c.value) == c.value;
    case ControlKind::quality:
        return c.value >= 0 && c.value <= 3 && std::floor(c.value) == c.value;
    case ControlKind::battery:
        return c.value >= -1 && c.value <= 100;
    case ControlKind::ping:
    case ControlKind::pong:
        return c.value >= 0;
    case ControlKind::rename:
        return !c.text.empty();
    case ControlKind::bye:
        return true;
    }
    return false;
}
} // namespace
Bytes encodeControl(const Control& c) {
    if (!valid(c))
        throw std::invalid_argument("Invalid control value");
    Bytes b(10 + c.text.size());
    b[0] = static_cast<std::uint8_t>(c.kind);
    std::uint64_t bits = 0;
    std::memcpy(&bits, &c.value, 8);
    for (int i = 0; i < 8; ++i)
        b[1 + i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    b[9] = static_cast<std::uint8_t>(c.text.size());
    std::copy(c.text.begin(), c.text.end(), b.begin() + 10);
    return b;
}
std::optional<Control> decodeControl(const Bytes& b) {
    if (b.size() < 10 || b.size() != 10 + static_cast<std::size_t>(b[9]))
        return {};
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i)
        bits = (bits << 8) | b[1 + i];
    Control c{static_cast<ControlKind>(b[0]), 0, std::string(b.begin() + 10, b.end())};
    std::memcpy(&c.value, &bits, 8);
    if (!valid(c))
        return {};
    return c;
}
} // namespace aw
