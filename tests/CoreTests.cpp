// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/AudioRing.h"
#include "core/Control.h"
#include "core/Pairing.h"
#include "core/SecureTransport.h"
#include "core/Session.h"
#include "core/Transport.h"
#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <sodium.h>
#include <stdexcept>
#include <thread>
using namespace aw;
static int checks = 0;
void check(bool value, const char* name) {
    ++checks;
    if (!value)
        throw std::runtime_error(name);
}
template <class F> void rejects(F f, const char* name) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected, name);
}
void pairingTests() {
    Pairing pc(Role::pc, "012345", "pc", "phone"), phone(Role::phone, "012345", "pc", "phone");
    auto a = pc.finish(phone.message()), b = phone.finish(pc.message());
    check(a == b, "matching PAKE key");
    check(pc.verifyConfirmation(a, phone.confirmation(b, Role::phone), Role::phone), "mutual confirmation");
    check(!pc.verifyConfirmation(a, phone.confirmation(b, Role::pc), Role::phone),
          "confirmation role binding");
    rejects([&] { pc.finish(phone.message()); }, "exchange cannot be reused");
    Pairing wrong(Role::phone, "012346", "pc", "phone"), server(Role::pc, "012345", "pc", "phone");
    auto c = server.finish(wrong.message()), d = wrong.finish(server.message());
    check(c != d, "wrong code derives different keys");
    check(!server.verifyConfirmation(c, wrong.confirmation(d, Role::phone), Role::phone),
          "wrong code cannot confirm");
    Pairing id1(Role::pc, "012345", "pc", "phone"), id2(Role::phone, "012345", "other-pc", "phone");
    check(id1.finish(id2.message()) != id2.finish(id1.message()), "identity transcript binding");
    Pairing invalid(Role::pc, "012345", "pc", "phone");
    Key garbage{};
    garbage.fill(255);
    rejects([&] { invalid.finish(garbage); }, "invalid curve point");
    rejects([] { Pairing p(Role::pc, "12345", "pc", "phone"); }, "short code");
    rejects([] { Pairing p(Role::pc, "12345x", "pc", "phone"); }, "nonnumeric code");
    rejects([] { unhex("abcdef"); }, "short encoded key");
    rejects([] { unhex(std::string(64, 'z')); }, "invalid encoded key");
    check(unhex(hex(a)) == a, "hex round trip");
    PairingGate gate;
    auto now = PairingGate::Clock::now();
    check(!gate.available(now), "pairing closed by default");
    gate.open(now);
    check(gate.available(now), "pairing window opens");
    check(!gate.available(now + std::chrono::seconds(120)), "pairing window expires");
    for (int i = 0; i < 5; ++i)
        gate.failure();
    check(!gate.available(now) && gate.triesLeft() == 0, "five attempts lock pairing");
}
void secureTests() {
    auto key = randomKey(), a = randomKey(), b = randomKey();
    auto p = deriveSessionKeys(key, a, b, Role::pc), q = deriveSessionKeys(key, a, b, Role::phone);
    check(p.transmit == q.receive && p.receive == q.transmit && p.transmit != p.receive, "directional keys");
    check(deriveSessionKeys(key, a, randomKey(), Role::pc).transmit != p.transmit, "fresh session nonces");
    SecureTransport tx(p), rx(q), reflection(p);
    Bytes plain = {1, 2, 3, 4};
    auto packet = tx.seal(PacketKind::audio, plain.data(), plain.size());
    check(!reflection.open(packet.data(), packet.size()), "reflection rejected");
    auto opened = rx.open(packet.data(), packet.size());
    check(opened && opened->body == plain, "authenticated encryption round trip");
    check(!rx.open(packet.data(), packet.size()), "replay rejected");
    auto n1 = tx.seal(PacketKind::audio, plain.data(), plain.size()),
         n2 = tx.seal(PacketKind::control, plain.data(), plain.size());
    auto corrupt = n2;
    corrupt.back() ^= 1;
    check(!rx.open(corrupt.data(), corrupt.size()), "ciphertext tampering rejected");
    corrupt = n2;
    corrupt[6] = 255;
    check(!rx.open(corrupt.data(), corrupt.size()), "unauthenticated counter cannot advance window");
    check(rx.open(n2.data(), n2.size()).has_value(), "receive after forgery");
    check(rx.open(n1.data(), n1.size()).has_value(), "out of order within replay window");
    check(!rx.open(n1.data(), n1.size()), "out of order replay rejected");
    for (int i = 0; i < 70; ++i) {
        auto n = tx.seal(PacketKind::audio, plain.data(), plain.size());
        check(rx.open(n.data(), n.size()).has_value(), "window advance");
    }
    check(!rx.open(packet.data(), packet.size()), "old replay rejected");
    check(!rx.open(packet.data(), 7), "truncated header");
    auto empty = tx.seal(PacketKind::control, nullptr, 0);
    auto e = rx.open(empty.data(), empty.size());
    check(e && e->body.empty(), "empty payload");
}
void sessionTests() {
    Session s;
    auto t = Session::Clock::now();
    s.discover();
    check(s.state() == State::discovering, "discover state");
    s.beginPairing();
    s.connected(t);
    check(s.state() == State::connected, "connected state");
    rejects([&] { s.beginPairing(); }, "cannot replace connected peer");
    check(!s.timedOut(t + std::chrono::seconds(2)), "heartbeat healthy");
    check(s.timedOut(t + std::chrono::seconds(4)), "heartbeat timeout");
    s.lost(t);
    check(!s.retryDue(t) && s.retryDue(t + std::chrono::seconds(1)), "first reconnect one second");
    s.retryFailed(t);
    check(!s.retryDue(t + std::chrono::seconds(1)) && s.retryDue(t + std::chrono::seconds(2)),
          "backoff doubles");
    for (int i = 0; i < 10; ++i)
        s.retryFailed(t);
    check(!s.retryDue(t + std::chrono::seconds(14)) && s.retryDue(t + std::chrono::seconds(15)),
          "backoff caps at fifteen seconds");
    s.networkAvailable(t);
    check(s.retryDue(t), "network return retries immediately");
    s.connected(t);
    check(s.attempts() == 0, "successful reconnect resets retries");
    s.disconnect();
    check(s.state() == State::idle && !s.retryDue(t), "explicit disconnect stops retries");
}
void controlTests() {
    for (auto c : {Control{ControlKind::mute, 1, {}}, Control{ControlKind::volume, 0.5, {}},
                   Control{ControlKind::mode, 2, {}}, Control{ControlKind::quality, 3, {}},
                   Control{ControlKind::ping, 12345, {}}, Control{ControlKind::rename, 0, "My PC"},
                   Control{ControlKind::battery, 80, {}}, Control{ControlKind::bye, 0, {}}}) {
        auto d = decodeControl(encodeControl(c));
        check(d && d->kind == c.kind && d->value == c.value && d->text == c.text, "control round trip");
    }
    rejects([] { encodeControl({ControlKind::volume, 2, {}}); }, "volume bounds");
    rejects([] { encodeControl({ControlKind::mode, 0.5, {}}); }, "mode integer");
    rejects([] { encodeControl({ControlKind::volume, NAN, {}}); }, "NaN rejected");
    auto b = encodeControl({ControlKind::mute, 1, {}});
    b[0] = 255;
    check(!decodeControl(b), "unknown control");
    b = encodeControl({ControlKind::mute, 1, {}});
    b.push_back(0);
    check(!decodeControl(b), "trailing control bytes");
    check(!decodeControl({}), "empty control");
}
void ringTests() {
    AudioRing<8> ring;
    float in[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, out[10];
    check(ring.write(in, 10) == 8, "ring bounds overflow");
    check(ring.read(out, 5) == 5 && out[4] == 4, "ring read ordered");
    check(ring.write(in + 5, 5) == 5, "ring wrap write");
    check(ring.read(out, 10) == 8 && out[0] == 5 && out[7] == 9 && out[8] == 0, "ring wrap and silence");
    AudioRing<1024> concurrent;
    std::atomic<bool> good{true};
    std::thread producer([&] {
        for (int i = 1; i <= 100000; ++i) {
            float f = static_cast<float>(i);
            while (!concurrent.write(&f, 1))
                std::this_thread::yield();
        }
    });
    for (int i = 1; i <= 100000; ++i) {
        float f = 0;
        while (!concurrent.read(&f, 1))
            std::this_thread::yield();
        if (f != i)
            good = false;
    }
    producer.join();
    check(good, "ring acquire/release concurrency");
}
void audioTests(int phoneRate = 48000) {
    const int phoneFrames = phoneRate / 100;
    // A real AOO encoder/decoder test: packets traverse two encrypted sessions.
    auto key = randomKey(), n1 = randomKey(), n2 = randomKey();
    SecureTransport s1(deriveSessionKeys(key, n1, n2, Role::pc)),
        s2(deriveSessionKeys(key, n1, n2, Role::phone));
    std::vector<Bytes> pcPackets, phonePackets;
    Transport* pc = nullptr;
    Transport* phone = nullptr;
    Transport p(Role::pc, Quality::balanced, 48000, 480, [&](const char* b, int n) {
        pcPackets.push_back(s1.seal(PacketKind::audio, reinterpret_cast<const uint8_t*>(b), n));
    });
    Transport f(Role::phone, Quality::balanced, phoneRate, phoneFrames, [&](const char* b, int n) {
        phonePackets.push_back(s2.seal(PacketKind::audio, reinterpret_cast<const uint8_t*>(b), n));
    });
    pc = &p;
    phone = &f;
    p.start();
    f.start();
    float l[480], r[480], mic[480], toPc[480], toPhoneL[480], toPhoneR[480];
    bool speaker = false, microphone = false;
    const float* pi[] = {l, r};
    float* po[] = {toPc};
    const float* fi[] = {mic};
    float* fo[] = {toPhoneL, toPhoneR};
    for (int block = 0; block < 100; ++block) {
        for (int i = 0; i < 480; ++i) {
            l[i] = r[i] = 0.1f * std::sin((block * 480 + i) * 0.05f);
            mic[i] = 0.1f * std::sin((block * 480 + i) * 0.04f);
        }
        p.process(pi, po, 480);
        f.process(fi, fo, phoneFrames);
        p.sendPending();
        f.sendPending();
        for (auto& packet : pcPackets) {
            auto data = s2.open(packet.data(), packet.size());
            check(data.has_value(), "PC encrypted AOO packet");
            phone->receive(reinterpret_cast<const char*>(data->body.data()),
                           static_cast<int>(data->body.size()));
        }
        for (auto& packet : phonePackets) {
            auto data = s1.open(packet.data(), packet.size());
            check(data.has_value(), "phone encrypted AOO packet");
            pc->receive(reinterpret_cast<const char*>(data->body.data()),
                        static_cast<int>(data->body.size()));
        }
        pcPackets.clear();
        phonePackets.clear();
        for (int i = 0; i < 480; ++i)
            if (std::abs(toPc[i]) > 0.005f)
                microphone = true;
        for (int i = 0; i < phoneFrames; ++i)
            if (std::abs(toPhoneL[i]) > 0.005f)
                speaker = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    check(speaker, "real encrypted Opus speaker direction");
    check(microphone, "real encrypted Opus microphone direction");
    p.setQuality(Quality::high);
    f.setQuality(Quality::maximum);
    p.stop();
    f.stop();
}
int main() {
    try {
        initializeCrypto();
        check(displayName(std::string(63, 'a') + "\xc3\xa9") == std::string(63, 'a'), "UTF-8 name boundary");
        pairingTests();
        secureTests();
        sessionTests();
        controlTests();
        ringTests();
        audioTests();
        audioTests(44100);
        std::cout
            << "PASS: " << checks
            << " checks (pairing, encryption, replay, state, control, concurrent ring, bidirectional Opus)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << " (after " << checks << " checks)\n";
        return 1;
    }
}
