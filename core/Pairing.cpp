// SPDX-License-Identifier: GPL-3.0-or-later
#include "Pairing.h"
#include <algorithm>
#include <mutex>
#include <sodium.h>
#include <stdexcept>
namespace aw {
namespace {
Bytes bytes(const std::string& s) {
    return Bytes(s.begin(), s.end());
}
void append(Bytes& b, const Key& k) {
    b.insert(b.end(), k.begin(), k.end());
}
void stringField(Bytes& b, const std::string& s) {
    if (s.size() > 255)
        throw std::invalid_argument("Identity too long");
    b.push_back(static_cast<std::uint8_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}
Key point(const char* domain) {
    std::array<unsigned char, 64> h{};
    Key p{};
    auto b = bytes(domain);
    crypto_generichash(h.data(), h.size(), b.data(), b.size(), nullptr, 0);
    crypto_core_ristretto255_from_hash(p.data(), h.data());
    return p;
}
const Key& M() {
    static const Key p = point("AudioWire v1 SPAKE2 Ristretto255 M");
    return p;
}
const Key& N() {
    static const Key p = point("AudioWire v1 SPAKE2 Ristretto255 N");
    return p;
}
} // namespace
void initializeCrypto() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        if (sodium_init() < 0)
            throw std::runtime_error("Cryptography unavailable");
    });
}
Key randomKey() {
    initializeCrypto();
    Key k{};
    randombytes_buf(k.data(), k.size());
    return k;
}
std::string randomId() {
    return hex(randomKey()).substr(0, 16);
}
std::string hex(const Key& k) {
    char out[65]{};
    sodium_bin2hex(out, sizeof(out), k.data(), k.size());
    return out;
}
Key unhex(const std::string& s) {
    Key k{};
    size_t n = 0;
    if (s.size() != 64 || sodium_hex2bin(k.data(), k.size(), s.data(), s.size(), nullptr, &n, nullptr) != 0 ||
        n != 32)
        throw std::invalid_argument("Invalid key encoding");
    return k;
}
Key hash(const Bytes& b, const Key* key) {
    Key out{};
    crypto_generichash(out.data(), out.size(), b.data(), b.size(), key ? key->data() : nullptr,
                       key ? key->size() : 0);
    return out;
}
Key authenticate(const Key& key, const std::string& domain, const Bytes& data) {
    Bytes b;
    stringField(b, domain);
    b.insert(b.end(), data.begin(), data.end());
    Key out{};
    crypto_auth_hmacsha256(out.data(), b.data(), b.size(), key.data());
    return out;
}
SessionKeys deriveSessionKeys(const Key& key, const Key& pc, const Key& phone, Role r) {
    Bytes b;
    append(b, pc);
    append(b, phone);
    const auto a = authenticate(key, "AudioWire v1 PC to phone", b);
    const auto c = authenticate(key, "AudioWire v1 phone to PC", b);
    return r == Role::pc ? SessionKeys{a, c} : SessionKeys{c, a};
}
Pairing::Pairing(Role r, const std::string& code, const std::string& pc, const std::string& phone)
    : role_(r) {
    initializeCrypto();
    if (code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](char c) { return c >= '0' && c <= '9'; }))
        throw std::invalid_argument("Enter six digits");
    stringField(context_, "AudioWire v1 pairing");
    stringField(context_, pc);
    stringField(context_, phone);
    Bytes pw = context_;
    stringField(pw, code);
    std::array<unsigned char, 64> h{};
    crypto_generichash(h.data(), h.size(), pw.data(), pw.size(), nullptr, 0);
    crypto_core_ristretto255_scalar_reduce(password_.data(), h.data());
    sodium_memzero(h.data(), h.size());
    sodium_memzero(pw.data(), pw.size());
    crypto_core_ristretto255_scalar_random(secret_.data());
    Key base{}, mask{};
    if (crypto_scalarmult_ristretto255_base(base.data(), secret_.data()) != 0 ||
        crypto_scalarmult_ristretto255(mask.data(), password_.data(), (r == Role::pc ? M() : N()).data()) !=
            0)
        throw std::runtime_error("Pairing failed");
    crypto_core_ristretto255_add(message_.data(), base.data(), mask.data());
}
Pairing::~Pairing() {
    sodium_memzero(secret_.data(), 32);
    sodium_memzero(password_.data(), 32);
}
Key Pairing::finish(const Key& peer) {
    if (finished_)
        throw std::logic_error("Pairing exchange already used");
    finished_ = true;
    if (!crypto_core_ristretto255_is_valid_point(peer.data()))
        throw std::invalid_argument("Invalid pairing message");
    Key mask{}, base{}, shared{};
    if (crypto_scalarmult_ristretto255(mask.data(), password_.data(),
                                       (role_ == Role::pc ? N() : M()).data()) != 0 ||
        crypto_core_ristretto255_sub(base.data(), peer.data(), mask.data()) != 0 ||
        crypto_scalarmult_ristretto255(shared.data(), secret_.data(), base.data()) != 0)
        throw std::invalid_argument("Invalid pairing message");
    transcript_ = context_;
    append(transcript_, role_ == Role::pc ? message_ : peer);
    append(transcript_, role_ == Role::pc ? peer : message_);
    Bytes b = transcript_;
    append(b, shared);
    Key result = hash(b);
    sodium_memzero(shared.data(), 32);
    sodium_memzero(b.data(), b.size());
    sodium_memzero(secret_.data(), 32);
    sodium_memzero(password_.data(), 32);
    return result;
}
Key Pairing::confirmation(const Key& k, Role r) const {
    if (!finished_)
        throw std::logic_error("Pairing exchange unfinished");
    return authenticate(k, r == Role::pc ? "PC confirms pairing" : "Phone confirms pairing", transcript_);
}
bool Pairing::verifyConfirmation(const Key& k, const Key& proof, Role r) const {
    auto expected = confirmation(k, r);
    return sodium_memcmp(expected.data(), proof.data(), 32) == 0;
}
void PairingGate::open(Clock::time_point now) {
    open_ = true;
    failures_ = 0;
    expires_ = now + std::chrono::minutes(2);
}
bool PairingGate::available(Clock::time_point now) const {
    return open_ && failures_ < 5 && now < expires_;
}
} // namespace aw
