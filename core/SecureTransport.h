// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Pairing.h"
#include <optional>
namespace aw {
enum class PacketKind : std::uint8_t { audio = 1, control = 2 };
struct Packet {
    PacketKind kind;
    Bytes body;
};
class SecureTransport {
  public:
    explicit SecureTransport(SessionKeys);
    ~SecureTransport();
    SecureTransport(const SecureTransport&) = delete;
    SecureTransport& operator=(const SecureTransport&) = delete;
    Bytes seal(PacketKind, const std::uint8_t*, std::size_t);
    std::optional<Packet> open(const std::uint8_t*, std::size_t);

  private:
    SessionKeys keys_;
    std::uint64_t counter_ = 0, highest_ = 0, seen_ = 0;
    bool received_ = false;
};
} // namespace aw
