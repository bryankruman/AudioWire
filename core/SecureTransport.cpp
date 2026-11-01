// SPDX-License-Identifier: GPL-3.0-or-later
#include "SecureTransport.h"
#include <limits>
#include <sodium.h>
#include <stdexcept>
namespace aw {
namespace {
constexpr std::size_t header = 14, tag = 16;
std::array<unsigned char, 12> nonce(std::uint64_t n) {
    std::array<unsigned char, 12> b{};
    for (int i = 0; i < 8; ++i)
        b[4 + i] = static_cast<unsigned char>(n >> (56 - 8 * i));
    return b;
}
} // namespace
SecureTransport::SecureTransport(SessionKeys k) : keys_(k) {
    initializeCrypto();
}
SecureTransport::~SecureTransport() {
    sodium_memzero(&keys_, sizeof(keys_));
}
Bytes SecureTransport::seal(PacketKind kind, const std::uint8_t* data, std::size_t n) {
    if (n > 65500 || counter_ == std::numeric_limits<std::uint64_t>::max())
        throw std::length_error("Packet limit reached");
    Bytes b(header + n + tag);
    b[0] = 'A';
    b[1] = 'W';
    b[2] = 'I';
    b[3] = 'R';
    b[4] = 1;
    b[5] = static_cast<std::uint8_t>(kind);
    const auto iv = nonce(counter_++);
    std::copy(iv.begin() + 4, iv.end(), b.begin() + 6);
    unsigned long long length = 0;
    crypto_aead_chacha20poly1305_ietf_encrypt(b.data() + header, &length, data, n, b.data(), header, nullptr,
                                              iv.data(), keys_.transmit.data());
    b.resize(header + length);
    return b;
}
std::optional<Packet> SecureTransport::open(const std::uint8_t* data, std::size_t n) {
    if (n < header + tag || n > 65530 || data[0] != 'A' || data[1] != 'W' || data[2] != 'I' ||
        data[3] != 'R' || data[4] != 1 || (data[5] != 1 && data[5] != 2))
        return {};
    std::uint64_t count = 0;
    for (int i = 0; i < 8; ++i)
        count = (count << 8) | data[6 + i];
    if (received_ && count <= highest_ &&
        (highest_ - count >= 64 || (seen_ & (std::uint64_t{1} << (highest_ - count)))))
        return {};
    Packet packet{static_cast<PacketKind>(data[5]), Bytes(n - header - tag)};
    unsigned long long length = 0;
    const auto iv = nonce(count);
    if (crypto_aead_chacha20poly1305_ietf_decrypt(packet.body.data(), &length, nullptr, data + header,
                                                  n - header, data, header, iv.data(),
                                                  keys_.receive.data()) != 0)
        return {};
    if (!received_) {
        highest_ = count;
        seen_ = 1;
        received_ = true;
    } else if (count > highest_) {
        const auto d = count - highest_;
        seen_ = d >= 64 ? 1 : (seen_ << d) | 1;
        highest_ = count;
    } else
        seen_ |= std::uint64_t{1} << (highest_ - count);
    packet.body.resize(length);
    return packet;
}
} // namespace aw
