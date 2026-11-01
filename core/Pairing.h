// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Types.h"
#include <chrono>
namespace aw {
void initializeCrypto();
Key randomKey();
std::string randomId();
std::string hex(const Key&);
Key unhex(const std::string&);
Key hash(const Bytes&, const Key* key = nullptr);
Key authenticate(const Key&, const std::string& domain, const Bytes&);
struct SessionKeys {
    Key transmit{}, receive{};
};
SessionKeys deriveSessionKeys(const Key& pairKey, const Key& pcNonce, const Key& phoneNonce, Role);
// Balanced SPAKE2-style exchange on libsodium's prime-order Ristretto255 group.
// Fixed independent M/N points are hash-to-group; no discrete logs are known.
// This implementation requires independent cryptographic review before release.
class Pairing {
  public:
    Pairing(Role, const std::string& code, const std::string& pcId, const std::string& phoneId);
    ~Pairing();
    Pairing(const Pairing&) = delete;
    Pairing& operator=(const Pairing&) = delete;
    const Key& message() const {
        return message_;
    }
    Key finish(const Key& peerMessage);
    Key confirmation(const Key& pairKey, Role sender) const;
    bool verifyConfirmation(const Key&, const Key& proof, Role sender) const;

  private:
    Role role_;
    Key secret_{}, password_{}, message_{};
    Bytes context_, transcript_;
    bool finished_ = false;
};
class PairingGate {
  public:
    using Clock = std::chrono::steady_clock;
    void open(Clock::time_point now);
    bool available(Clock::time_point now) const;
    void failure() {
        if (failures_ < 5)
            ++failures_;
    }
    void close() {
        open_ = false;
    }
    int triesLeft() const {
        return 5 - failures_;
    }

  private:
    bool open_ = false;
    int failures_ = 0;
    Clock::time_point expires_{};
};
} // namespace aw
