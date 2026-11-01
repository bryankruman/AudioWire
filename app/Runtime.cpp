// SPDX-License-Identifier: GPL-3.0-or-later
#include "Runtime.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <sodium.h>
namespace aw {
namespace {
class MemoryKeyStore final : public KeyStore {
    std::map<std::string, Key> keys;

  public:
    ~MemoryKeyStore() {
        for (auto& pair : keys)
            sodium_memzero(pair.second.data(), pair.second.size());
    }
    void put(const std::string& id, const Key& k) override {
        keys[id] = k;
    }
    std::optional<Key> get(const std::string& id) override {
        auto i = keys.find(id);
        return i == keys.end() ? std::optional<Key>{} : i->second;
    }
    void erase(const std::string& id) override {
        auto it = keys.find(id);
        if (it != keys.end()) {
            sodium_memzero(it->second.data(), it->second.size());
            keys.erase(it);
        }
    }
};
struct WipeKey {
    Key& key;
    ~WipeKey() {
        sodium_memzero(key.data(), key.size());
    }
};
juce::var object(std::initializer_list<std::pair<const char*, juce::var>> values) {
    juce::DynamicObject::Ptr o = new juce::DynamicObject;
    for (auto& v : values)
        o->setProperty(v.first, v.second);
    return juce::var(o.get());
}
juce::String str(const std::string& s) {
    return juce::String(s);
}
std::string string(const juce::var& v, const char* field) {
    return v[field].toString().toStdString();
}
bool validId(const std::string& id) {
    return id.size() == 16 && id.find_first_not_of("0123456789abcdef") == std::string::npos;
}
Bytes resumeTranscript(const std::string& pc, const std::string& phone, const Key& p, const Key& f) {
    Bytes b(pc.begin(), pc.end());
    b.insert(b.end(), phone.begin(), phone.end());
    b.insert(b.end(), p.begin(), p.end());
    b.insert(b.end(), f.begin(), f.end());
    return b;
}
SessionKeys controlKeys(SessionKeys k) {
    return {authenticate(k.transmit, "AudioWire v1 control", {}),
            authenticate(k.receive, "AudioWire v1 control", {})};
}
uint16_t port(const juce::var& v, const char* f) {
    auto n = static_cast<int>(v[f]);
    if (n < 1 || n > 65535)
        throw std::runtime_error("Invalid connection address");
    return static_cast<uint16_t>(n);
}
} // namespace
std::string lanAddress() {
    std::string fallback;
    for (auto a : juce::IPAddress::getAllAddresses()) {
        const auto ip = a.toString();
        if (a.isIPv6 || ip.startsWith("127.") || ip.startsWith("169.254.") || ip == "0.0.0.0")
            continue;
        if (fallback.empty())
            fallback = ip.toStdString();
        if (ip.startsWith("192.168.") || ip.startsWith("10.") ||
            (a.address[0] == 172 && a.address[1] >= 16 && a.address[1] <= 31))
            return ip.toStdString();
    }
    return fallback;
}
Runtime::Runtime(Role r, Settings s, bool persist)
    : juce::Thread("AudioWire network"), role_(r), persist_(persist), settings_(std::move(s)) {
    initializeCrypto();
    keys_ = persist_ ? makeKeyStore() : std::make_unique<MemoryKeyStore>();
    discovery_ = makeDiscovery();
    mode_ = static_cast<int>(settings_.mode);
    quality_ = static_cast<int>(settings_.quality);
}
Runtime::~Runtime() {
    discovery_->stop();
    signalThreadShouldExit();
    notify();
    stopThread(-1);
    realtime_ = nullptr;
}
void Runtime::start() {
    udp_ = std::make_unique<juce::DatagramSocket>(false);
    if (!udp_->bindToPort(0))
        throw std::runtime_error("AudioWire could not open the Wi-Fi connection");
    udpPort_ = static_cast<uint16_t>(udp_->getBoundPort());
    if (role_ == Role::pc) {
        listener_ = std::make_unique<juce::StreamingSocket>();
        if (!listener_->createListener(0))
            throw std::runtime_error("AudioWire could not listen for your iPhone");
        tcpPort_ = static_cast<uint16_t>(listener_->getBoundPort());
        pairNew();
        if (persist_)
            discovery_->advertise({settings_.id, settings_.name, "", tcpPort_}, udpPort_);
    } else {
        session_.discover();
        if (persist_)
            discovery_->browse([this](Device d) {
                std::lock_guard<std::mutex> lock(mutex_);
                auto& devices = snapshot_.devices;
                if (d.host.empty()) {
                    devices.erase(std::remove_if(devices.begin(), devices.end(),
                                                 [&](auto& old) { return old.id == d.id; }),
                                  devices.end());
                    return;
                }
                auto i =
                    std::find_if(devices.begin(), devices.end(), [&](auto& old) { return old.id == d.id; });
                if (i == devices.end())
                    devices.push_back(d);
                else
                    *i = d;
                for (auto& saved : settings_.paired)
                    if (saved.id == d.id) {
                        saved.name = d.name;
                        saved.host = d.host;
                        saved.port = d.port;
                    }
                commands_.push_back([this, d] {
                    if (remote_.id == d.id) {
                        remote_ = d;
                        session_.networkAvailable(Session::Clock::now());
                    }
                });
                notify();
            });
    }
    startThread();
    if (role_ == Role::phone && settings_.autoConnect && !settings_.paired.empty())
        connect(settings_.paired.front(), "");
}
void Runtime::pairNew() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state == State::connected)
        return;
    // The code is displayed only in the local pairing UI, never included in logs or advertisements.
    code_ = juce::String(static_cast<int>(randombytes_uniform(1000000))).paddedLeft('0', 6).toStdString();
    ++pairingGeneration_;
    gate_.open(PairingGate::Clock::now());
    codeExpires_ = PairingGate::Clock::now() + std::chrono::minutes(2);
    snapshot_.error.clear();
}
void Runtime::connect(Device d, const std::string& code) {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back([this, d, code] {
        if (session_.state() == State::connected)
            teardown(false);
        remote_ = d;
        session_.beginPairing();
        publish();
        try {
            clientHandshake(d, code);
        } catch (const std::exception& e) {
            session_.disconnect();
            {
                std::lock_guard<std::mutex> l(mutex_);
                snapshot_.error = e.what();
            }
            if (code.empty() && !d.id.empty() && settings().autoConnect) {
                session_.connected(Session::Clock::now());
                session_.lost(Session::Clock::now());
            }
        }
    });
    notify();
}
void Runtime::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back([this] {
        if (!control_) {
            teardown(false);
            return;
        }
        disconnectRequested_ = true;
        disconnectDeadline_ = juce::Time::getMillisecondCounterHiRes() + 1000;
        realtime_ = nullptr;
        inputMeter_ = 0;
        outputMeter_ = 0;
        try {
            sendControl({ControlKind::bye, 0, {}});
        } catch (...) {
            teardown(false);
        }
    });
    notify();
}
void Runtime::forget(const std::string& id) {
    if (!validId(id))
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back([this, id] {
        if (remote_.id == id)
            teardown(false);
        keys_->erase(id);
        std::lock_guard<std::mutex> l(mutex_);
        auto& list = settings_.paired;
        list.erase(std::remove_if(list.begin(), list.end(), [&](auto& d) { return d.id == id; }), list.end());
        if (persist_)
            settings_.save();
    });
    notify();
}
void Runtime::setControl(Control c) {
    encodeControl(c);
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back([this, c] {
        handleControl(c);
        if (control_)
            sendControl(c);
    });
    notify();
}
void Runtime::updateSettings(const Settings& s) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool renamed = settings_.name != s.name;
    settings_ = s;
    settings_.audio.mode = s.mode;
    if (persist_)
        settings_.save();
    commands_.push_back([this, s, renamed] {
        if (renamed && persist_ && role_ == Role::pc)
            discovery_->advertise({s.id, s.name, "", tcpPort_}, udpPort_);
        if (!s.autoConnect && session_.state() == State::reconnecting)
            teardown(false);
        handleControl({ControlKind::mode, static_cast<double>(static_cast<int>(s.mode)), {}});
        quality_ = static_cast<int>(s.quality);
        if (transport_) {
            transport_->setQuality(s.quality);
            transport_->setBufferMs(s.bufferMs);
        }
        if (control_) {
            sendControl({ControlKind::mode, static_cast<double>(mode_.load()), {}});
            sendControl({ControlKind::quality, static_cast<double>(static_cast<int>(s.quality)), {}});
        }
    });
    notify();
}
Settings Runtime::settings() const {
    std::lock_guard<std::mutex> l(mutex_);
    return settings_;
}
Snapshot Runtime::snapshot() const {
    std::lock_guard<std::mutex> l(mutex_);
    auto s = snapshot_;
    s.input = inputMeter_;
    s.output = outputMeter_;
    s.muted = mute_;
    s.volume = volume_;
    s.pairSeconds = std::max(0, static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                                                     codeExpires_ - PairingGate::Clock::now())
                                                     .count()));
    s.triesLeft = gate_.triesLeft();
    return s;
}
std::string Runtime::pairingCode() const {
    std::lock_guard<std::mutex> l(mutex_);
    return gate_.available(PairingGate::Clock::now()) ? code_ : "";
}
std::string Runtime::pairingUrl() const {
    std::lock_guard<std::mutex> l(mutex_);
    if (!gate_.available(PairingGate::Clock::now()))
        return {};
    const auto ip = lanAddress();
    if (ip.empty() || !tcpPort_)
        return {};
    return "audiowire://pair?ip=" + ip + "&port=" + std::to_string(tcpPort_) + "&code=" + code_;
}
void Runtime::attachAudioInfo(double rate, int frames, bool mic, std::string status, std::string route) {
    const bool changed = std::abs(rate_.exchange(rate) - rate) > 1;
    frames_ = std::clamp(frames, 1, 4096);
    const bool micChanged = micAvailable_.exchange(mic) != mic;
    std::lock_guard<std::mutex> l(mutex_);
    snapshot_.driver = std::move(status);
    snapshot_.route = std::move(route);
    snapshot_.micAvailable = mic;
    if (micChanged && role_ == Role::pc)
        commands_.push_back([this, mic] {
            if (control_) {
                handleControl({ControlKind::driver, mic ? 1.0 : 0.0, {}});
                sendControl({ControlKind::driver, mic ? 1.0 : 0.0, {}});
            }
        });
    if (changed)
        commands_.push_back([this] {
            if (session_.state() == State::connected)
                teardown(true);
        });
}
void Runtime::exactRead(juce::StreamingSocket& socket, void* target, int length) {
    auto* out = static_cast<char*>(target);
    int done = 0;
    auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000;
    while (done < length) {
        if (threadShouldExit() || juce::Time::getMillisecondCounterHiRes() > deadline)
            throw std::runtime_error("Connection timed out. Check that both devices are on the same Wi-Fi.");
        int ready = socket.waitUntilReady(true, 50);
        if (ready < 0)
            throw std::runtime_error("Connection closed");
        if (!ready)
            continue;
        int n = socket.read(out + done, length - done, false);
        if (n <= 0)
            throw std::runtime_error("Connection closed");
        done += n;
    }
}
Bytes Runtime::readFrame(juce::StreamingSocket& socket) {
    unsigned char h[4];
    exactRead(socket, h, 4);
    uint32_t n = 0;
    for (auto b : h)
        n = (n << 8) | b;
    if (n == 0 || n > 8192)
        throw std::runtime_error("Invalid connection message");
    Bytes b(n);
    exactRead(socket, b.data(), static_cast<int>(n));
    return b;
}
void Runtime::writeFrame(juce::StreamingSocket& socket, const Bytes& b) {
    if (b.empty() || b.size() > 8192)
        throw std::runtime_error("Invalid connection message");
    Bytes frame(4 + b.size());
    for (int i = 0; i < 4; ++i)
        frame[i] = static_cast<uint8_t>(b.size() >> (24 - 8 * i));
    std::copy(b.begin(), b.end(), frame.begin() + 4);
    size_t done = 0;
    auto deadline = juce::Time::getMillisecondCounterHiRes() + 3000;
    while (done < frame.size()) {
        if (threadShouldExit() || juce::Time::getMillisecondCounterHiRes() > deadline)
            throw std::runtime_error("Connection timed out");
        if (socket.waitUntilReady(false, 50) <= 0)
            continue;
        int n = socket.write(frame.data() + done, static_cast<int>(frame.size() - done));
        if (n <= 0)
            throw std::runtime_error("Connection closed");
        done += n;
    }
}
void Runtime::sendJson(juce::StreamingSocket& socket, const juce::var& v) {
    auto s = juce::JSON::toString(v, true).toStdString();
    writeFrame(socket, Bytes(s.begin(), s.end()));
}
juce::var Runtime::readJson(juce::StreamingSocket& socket) {
    auto b = readFrame(socket);
    auto v = juce::JSON::parse(
        juce::String::fromUTF8(reinterpret_cast<const char*>(b.data()), static_cast<int>(b.size())));
    if (!v.isObject())
        throw std::runtime_error("Invalid connection message");
    if (v.hasProperty("error"))
        throw std::runtime_error(string(v, "error"));
    return v;
}
void Runtime::remember(Device d) {
    std::lock_guard<std::mutex> l(mutex_);
    auto& list = settings_.paired;
    auto i = std::find_if(list.begin(), list.end(), [&](auto& old) { return old.id == d.id; });
    if (i == list.end())
        list.push_back(d);
    else
        *i = d;
    if (persist_)
        settings_.save();
}
void Runtime::serverHandshake(std::unique_ptr<juce::StreamingSocket> socket) {
    if (session_.state() == State::connected) {
        sendJson(*socket,
                 object({{"error", "This PC is already connected to a phone. Disconnect it first."}}));
        return;
    }
    const auto hello = readJson(*socket);
    const auto id = string(hello, "id");
    if (!validId(id) || static_cast<int>(hello["v"]) != 1)
        throw std::runtime_error("Unsupported AudioWire version");
    Device peer{id, displayName(string(hello, "name")), socket->getHostName().toStdString(), 0};
    const auto remoteUdp = port(hello, "udp");
    const auto phoneNonce = unhex(string(hello, "nonce")), pcNonce = randomKey();
    auto own = settings();
    const bool pairing = string(hello, "type") == "pair";
    Key pairKey{};
    WipeKey wipe{pairKey};
    std::string code;
    uint64_t pairingGeneration = 0;
    if (pairing) {
        std::lock_guard<std::mutex> l(mutex_);
        if (!gate_.available(PairingGate::Clock::now())) {
            sendJson(*socket, object({{"error", "Click Pair a new phone on your PC to get a fresh code."}}));
            return;
        }
        code = code_;
        pairingGeneration = pairingGeneration_;
        gate_.failure();
    } else {
        auto stored = keys_->get(id);
        if (!stored) {
            sendJson(*socket,
                     object({{"error", "This phone is no longer paired. Enter a new code from your PC."}}));
            return;
        }
        pairKey = *stored;
    }
    const auto transcript = resumeTranscript(own.id, id, pcNonce, phoneNonce);
    sendJson(*socket,
             object({{"id", str(own.id)},
                     {"name", str(own.name)},
                     {"nonce", str(hex(pcNonce))},
                     {"proof", pairing ? juce::String()
                                       : str(hex(authenticate(pairKey, "PC resumes", transcript)))}}));
    if (pairing) {
        Pairing exchange(Role::pc, code, own.id, id);
        sodium_memzero(code.data(), code.size());
        auto response = readJson(*socket);
        pairKey = exchange.finish(unhex(string(response, "message")));
        sendJson(*socket, object({{"message", str(hex(exchange.message()))},
                                  {"proof", str(hex(exchange.confirmation(pairKey, Role::pc)))}}));
        auto proof = unhex(string(readJson(*socket), "proof"));
        if (!exchange.verifyConfirmation(pairKey, proof, Role::phone)) {
            std::lock_guard<std::mutex> l(mutex_);
            sendJson(*socket, object({{"error", juce::String("That code didn't match. ") +
                                                    juce::String(gate_.triesLeft()) + " tries left."}}));
            return;
        }
    } else {
        auto proof = unhex(string(readJson(*socket), "proof"));
        auto expected = authenticate(pairKey, "Phone resumes", transcript);
        if (sodium_memcmp(proof.data(), expected.data(), 32))
            throw std::runtime_error("Pairing verification failed. Pair again with a new code.");
    }
    if (pairing) {
        std::lock_guard<std::mutex> l(mutex_);
        // Commit authentication only for the same local code window that began the exchange.
        if (pairingGeneration != pairingGeneration_ || PairingGate::Clock::now() >= codeExpires_)
            throw std::runtime_error("Pairing code expired or changed. Generate a new one.");
    }
    if (pairing)
        keys_->put(id, pairKey);
    remember(peer);
    sendJson(*socket, object({{"ready", true}, {"udp", udpPort_}, {"mic", micAvailable_.load()}}));
    establish(std::move(socket), peer, remoteUdp, deriveSessionKeys(pairKey, pcNonce, phoneNonce, Role::pc));
    sodium_memzero(pairKey.data(), 32);
    {
        std::lock_guard<std::mutex> l(mutex_);
        gate_.close();
        code_.clear();
    }
}
void Runtime::clientHandshake(const Device& pc, const std::string& code) {
    auto socket = std::make_unique<juce::StreamingSocket>();
    if (!socket->connect(str(pc.host), pc.port, 2000))
        throw std::runtime_error(
            "PC not found. Make sure AudioWire is running and both devices are on the same Wi-Fi.");
    auto own = settings();
    const auto phoneNonce = randomKey();
    const bool pairing = !code.empty();
    sendJson(*socket, object({{"v", 1},
                              {"type", pairing ? "pair" : "resume"},
                              {"id", str(own.id)},
                              {"name", str(own.name)},
                              {"udp", udpPort_},
                              {"nonce", str(hex(phoneNonce))}}));
    const auto response = readJson(*socket);
    auto pcId = string(response, "id");
    if (!validId(pcId) || (!pc.id.empty() && pcId != pc.id))
        throw std::runtime_error("The PC identity changed. Select your PC again.");
    Device peer{pcId, displayName(string(response, "name")), pc.host, pc.port};
    const auto pcNonce = unhex(string(response, "nonce"));
    Key pairKey{};
    WipeKey wipe{pairKey};
    if (pairing) {
        Pairing exchange(Role::phone, code, pcId, own.id);
        sendJson(*socket, object({{"message", str(hex(exchange.message()))}}));
        const auto message = readJson(*socket);
        pairKey = exchange.finish(unhex(string(message, "message")));
        if (!exchange.verifyConfirmation(pairKey, unhex(string(message, "proof")), Role::pc)) {
            // Sending the phone proof still lets the PC count this online guess.
            sendJson(*socket, object({{"proof", str(hex(exchange.confirmation(pairKey, Role::phone)))}}));
            readJson(*socket);
            throw std::runtime_error("That code didn't match. Enter the code shown on your PC.");
        }
        sendJson(*socket, object({{"proof", str(hex(exchange.confirmation(pairKey, Role::phone)))}}));
    } else {
        auto stored = keys_->get(pcId);
        if (!stored)
            throw std::runtime_error("Enter the code shown on your PC to pair.");
        pairKey = *stored;
        auto transcript = resumeTranscript(pcId, own.id, pcNonce, phoneNonce);
        auto expected = authenticate(pairKey, "PC resumes", transcript);
        if (sodium_memcmp(expected.data(), unhex(string(response, "proof")).data(), 32))
            throw std::runtime_error("The PC could not verify its identity. Pair again.");
        sendJson(*socket, object({{"proof", str(hex(authenticate(pairKey, "Phone resumes", transcript)))}}));
    }
    const auto ready = readJson(*socket);
    if (!static_cast<bool>(ready["ready"]))
        throw std::runtime_error("PC did not complete the connection");
    if (pairing)
        keys_->put(pcId, pairKey);
    remember(peer);
    if (!static_cast<bool>(ready["mic"])) {
        mode_ = static_cast<int>(Mode::speaker);
        std::lock_guard<std::mutex> l(mutex_);
        settings_.mode = Mode::speaker;
        settings_.audio.mode = Mode::speaker;
    }
    establish(std::move(socket), peer, port(ready, "udp"),
              deriveSessionKeys(pairKey, pcNonce, phoneNonce, Role::phone));
    sodium_memzero(pairKey.data(), 32);
}
void Runtime::establish(std::unique_ptr<juce::StreamingSocket> socket, const Device& d, uint16_t p,
                        SessionKeys k) {
    teardown(false);
    control_ = std::move(socket);
    remote_ = d;
    remotePort_ = p;
    if (role_ == Role::pc && !micAvailable_) {
        mode_ = static_cast<int>(Mode::speaker);
        std::lock_guard<std::mutex> l(mutex_);
        settings_.mode = settings_.audio.mode = Mode::speaker;
    }
    audioSecure_ = std::make_unique<SecureTransport>(k);
    controlSecure_ = std::make_unique<SecureTransport>(controlKeys(k));
    transport_ = std::make_unique<Transport>(
        role_, static_cast<Quality>(quality_.load()), rate_, frames_, [this](const char* data, int n) {
            auto b = audioSecure_->seal(PacketKind::audio, reinterpret_cast<const uint8_t*>(data),
                                        static_cast<size_t>(n));
            udp_->write(str(remote_.host), remotePort_, b.data(), static_cast<int>(b.size()));
        });
    transport_->setBufferMs(settings().bufferMs);
    transport_->start();
    gateFrames_ = static_cast<int>(rate_.load() / 2);
    realtime_.store(transport_.get());
    session_.connected(Session::Clock::now());
    {
        std::lock_guard<std::mutex> l(mutex_);
        snapshot_.error.clear();
        snapshot_.peer = d.name;
    }
    lastPing_ = 0;
    // The phone owns the initial mode and microphone privacy state. After setup, either UI can change
    // controls.
    if (role_ == Role::phone) {
        sendControl({ControlKind::mode, static_cast<double>(mode_.load()), {}});
        sendControl({ControlKind::quality, static_cast<double>(quality_.load()), {}});
        sendControl({ControlKind::mute, mute_ ? 1.0 : 0.0, {}});
        sendControl({ControlKind::volume, volume_.load(), {}});
    } else
        sendControl({ControlKind::driver, micAvailable_ ? 1.0 : 0.0, {}});
}
void Runtime::teardown(bool retry) {
    realtime_.store(nullptr);
    while (callbacks_.load())
        juce::Thread::sleep(1);
    disconnectRequested_ = false;
    transport_.reset();
    audioSecure_.reset();
    controlSecure_.reset();
    control_.reset();
    if (retry && (role_ == Role::pc || settings().autoConnect))
        session_.lost(Session::Clock::now());
    else
        session_.disconnect();
    inputMeter_ = 0;
    outputMeter_ = 0;
}
void Runtime::sendControl(const Control& c) {
    auto b = encodeControl(c);
    writeFrame(*control_, controlSecure_->seal(PacketKind::control, b.data(), b.size()));
}
void Runtime::handleControl(const Control& c) {
    switch (c.kind) {
    case ControlKind::mute:
        mute_ = c.value != 0;
        break;
    case ControlKind::volume:
        volume_ = static_cast<float>(c.value);
        break;
    case ControlKind::mode:
        mode_ = static_cast<int>(c.value);
        if (role_ == Role::pc && !micAvailable_ && records(static_cast<Mode>(mode_.load()))) {
            mode_ = static_cast<int>(Mode::speaker);
            if (control_)
                sendControl({ControlKind::mode, 1, {}});
        }
        gateFrames_ = static_cast<int>(rate_.load() / 2);
        break;
    case ControlKind::quality:
        quality_ = static_cast<int>(c.value);
        if (transport_)
            transport_->setQuality(static_cast<Quality>(quality_.load()));
        break;
    case ControlKind::ping:
        if (control_)
            sendControl({ControlKind::pong, c.value, {}});
        break;
    case ControlKind::pong:
        latencyMs_ = juce::Time::getMillisecondCounterHiRes() - c.value;
        break;
    case ControlKind::battery: {
        std::lock_guard<std::mutex> l(mutex_);
        snapshot_.battery = c.value;
    } break;
    case ControlKind::rename: {
        std::lock_guard<std::mutex> l(mutex_);
        snapshot_.peer = displayName(c.text);
        for (auto& d : settings_.paired)
            if (d.id == remote_.id)
                d.name = snapshot_.peer;
        if (persist_)
            settings_.save();
    } break;
    case ControlKind::bye:
        if (!disconnectRequested_ && control_)
            try {
                sendControl({ControlKind::bye, 0, {}});
            } catch (...) {
            }
        teardown(false);
        break;
    case ControlKind::driver:
        if (c.value == 0 && role_ == Role::phone) {
            mode_ = static_cast<int>(Mode::speaker);
            std::lock_guard<std::mutex> l(mutex_);
            settings_.mode = settings_.audio.mode = Mode::speaker;
            snapshot_.error =
                "Your PC needs the AudioWire audio driver for microphone mode. Speaker mode still works.";
        }
        break;
    }
    if (c.kind == ControlKind::mode || c.kind == ControlKind::quality) {
        std::lock_guard<std::mutex> l(mutex_);
        settings_.mode = static_cast<Mode>(mode_.load());
        settings_.quality = static_cast<Quality>(quality_.load());
        settings_.audio.mode = settings_.mode;
    }
}
void Runtime::publish() {
    std::lock_guard<std::mutex> l(mutex_);
    snapshot_.state = session_.state();
    snapshot_.latencyMs = latencyMs_;
    snapshot_.packets = packets_;
    snapshot_.rejected = rejected_;
    snapshot_.lost = transport_ ? transport_->lostBlocks() : 0;
    snapshot_.bufferMs = transport_ ? transport_->bufferMs() : 20;
}
void Runtime::run() {
    std::array<uint8_t, 65536> receiveBuffer{};
    while (!threadShouldExit()) {
        std::deque<std::function<void()>> jobs;
        {
            std::lock_guard<std::mutex> l(mutex_);
            jobs.swap(commands_);
        }
        for (auto& job : jobs)
            try {
                job();
            } catch (const std::exception& e) {
                teardown(session_.state() == State::connected && !disconnectRequested_);
                std::lock_guard<std::mutex> l(mutex_);
                snapshot_.error = e.what();
            }
        if (listener_ && listener_->waitUntilReady(true, 0) > 0) {
            std::unique_ptr<juce::StreamingSocket> accepted(listener_->waitForNextConnection());
            if (accepted)
                try {
                    serverHandshake(std::move(accepted));
                } catch (const std::exception&) { /* Never log pairing secrets or attacker-controlled data. */
                }
        }
        try {
            if (control_ && control_->waitUntilReady(true, 0) > 0) {
                auto b = readFrame(*control_);
                auto p = controlSecure_->open(b.data(), b.size());
                if (!p || p->kind != PacketKind::control)
                    throw std::runtime_error("Connection verification failed");
                auto c = decodeControl(p->body);
                if (!c)
                    throw std::runtime_error("Invalid connection message");
                session_.received(Session::Clock::now());
                handleControl(*c);
            }
            int drained = 0;
            while (udp_->waitUntilReady(true, 0) > 0 && drained++ < 128) {
                juce::String host;
                int port = 0;
                int n = udp_->read(receiveBuffer.data(), static_cast<int>(receiveBuffer.size()), false, host,
                                   port);
                if (n <= 0)
                    break;
                if (audioSecure_ && port == remotePort_) {
                    auto p = audioSecure_->open(receiveBuffer.data(), static_cast<size_t>(n));
                    if (p && p->kind == PacketKind::audio) {
                        ++packets_;
                        transport_->receive(reinterpret_cast<const char*>(p->body.data()),
                                            static_cast<int>(p->body.size()));
                        session_.received(Session::Clock::now());
                    } else
                        ++rejected_;
                } else
                    ++rejected_;
            }
            if (transport_)
                transport_->sendPending();
            if (control_ && juce::Time::getMillisecondCounterHiRes() - lastPing_ >= 1000) {
                lastPing_ = juce::Time::getMillisecondCounterHiRes();
                sendControl({ControlKind::ping, lastPing_, {}});
            }
            if (disconnectRequested_ && juce::Time::getMillisecondCounterHiRes() >= disconnectDeadline_)
                teardown(false);
            else if (session_.timedOut(Session::Clock::now()))
                teardown(true);
        } catch (const std::exception&) {
            teardown(!disconnectRequested_);
        }
        if (role_ == Role::phone && session_.retryDue(Session::Clock::now())) {
            Device d = remote_;
            auto s = settings();
            for (auto& saved : s.paired)
                if (saved.id == d.id) {
                    d.host = saved.host;
                    d.port = saved.port;
                }
            try {
                clientHandshake(d, "");
            } catch (const std::exception&) {
                session_.retryFailed(Session::Clock::now());
            }
        }
        publish();
        wait(2);
    }
    teardown(false);
}
void Runtime::process(const float* const* in, float* const* out, int n) {
    if (n <= 0 || n > 4096 || !out)
        return;
    const int outputs = role_ == Role::pc ? 1 : 2;
    for (int c = 0; c < outputs; ++c)
        if (out[c])
            std::fill_n(out[c], n, 0.0f);
        else
            return;
    callbacks_.fetch_add(1);
    auto* transport = realtime_.load();
    if (transport) {
        const Mode mode = static_cast<Mode>(mode_.load());
        const bool send = role_ == Role::pc ? plays(mode) : (records(mode) && !mute_);
        const float* input[2] = {silence_.data(), silence_.data()};
        if (send) {
            input[0] = in && in[0] ? in[0] : silence_.data();
            if (role_ == Role::pc)
                input[1] = in && in[1] ? in[1] : input[0];
        }
        if (role_ == Role::phone && mode == Mode::speakerphone && gateFrames_ > 0) {
            input[0] = silence_.data();
            int remaining = gateFrames_.load();
            while (remaining > 0 &&
                   !gateFrames_.compare_exchange_weak(remaining, std::max(0, remaining - n))) {
            }
        }
        transport->process(input, out, n);
        float a = 0, b = 0;
        for (int i = 0; i < n; ++i) {
            a = std::max(a, std::abs(input[0][i]));
            for (int ch = 0; ch < outputs; ++ch) {
                if (!out[ch])
                    continue;
                const bool receive = role_ == Role::pc ? (records(mode) && !mute_) : plays(mode);
                out[ch][i] *= receive ? (role_ == Role::phone ? volume_.load() : 1.0f) : 0.0f;
                b = std::max(b, std::abs(out[ch][i]));
            }
        }
        inputMeter_ = a;
        outputMeter_ = b;
    }
    callbacks_.fetch_sub(1);
}
std::string Runtime::diagnostics() const {
    const auto s = snapshot();
    const auto own = settings();
    return "AudioWire " AUDIOWIRE_VERSION "\nState: " + std::string(stateName(s.state)) +
           "\nAudio: " + s.driver + "\nRoute: " + s.route +
           "\nControl round-trip: " + std::to_string(static_cast<int>(s.latencyMs)) +
           " ms (not acoustic latency)\nAudio packets: " + std::to_string(s.packets) +
           "\nRejected packets: " + std::to_string(s.rejected) +
           "\nLost audio blocks: " + std::to_string(s.lost) +
           "\nAudio reserve: " + std::to_string(s.bufferMs) +
           " ms\nQuality: " + std::to_string(static_cast<int>(own.quality)) +
           "\nNo pairing codes or stored keys are included.\n";
}
int runNetworkTest() {
    try {
        auto require = [](bool ok, const char* what) {
            if (!ok)
                throw std::runtime_error(what);
        };
        auto waitFor = [](auto predicate, int ms = 5000) {
            for (int elapsed = 0; elapsed < ms; elapsed += 10) {
                if (predicate())
                    return true;
                juce::Thread::sleep(10);
            }
            return predicate();
        };
        Settings settings;
        settings.id = randomId();
        settings.name = "Network test PC";
        settings.mode = settings.audio.mode = Mode::speaker;
        settings.paired.clear();
        settings.autoConnect = false;
        Runtime pc(Role::pc, settings, false);
        pc.attachAudioInfo(48000, 480, true, "Test microphone", "Test route");
        pc.start();
        Settings client = settings;
        client.id = randomId();
        client.name = "Network test phone";
        client.mode = client.audio.mode = Mode::speakerphone;
        Runtime phone(Role::phone, client, false);
        phone.attachAudioInfo(48000, 480, true, "Test audio", "Test route");
        phone.start();
        Device host{settings.id, settings.name, "127.0.0.1", pc.tcpPort()};
        // An attacker can read the PC's PAKE proof and abandon the exchange. Each one must consume a guess.
        auto frame = [&](juce::StreamingSocket& socket, const juce::var& v) {
            auto text = juce::JSON::toString(v, true).toStdString();
            Bytes b(4 + text.size());
            for (int i = 0; i < 4; ++i)
                b[i] = static_cast<uint8_t>(text.size() >> (24 - 8 * i));
            std::copy(text.begin(), text.end(), b.begin() + 4);
            require(socket.write(b.data(), static_cast<int>(b.size())) == static_cast<int>(b.size()),
                    "test handshake write");
        };
        auto read = [&](juce::StreamingSocket& socket) {
            unsigned char h[4];
            require(socket.waitUntilReady(true, 3000) > 0 && socket.read(h, 4, true) == 4,
                    "test handshake header");
            uint32_t n = 0;
            for (auto byte : h)
                n = (n << 8) | byte;
            require(n > 0 && n < 8192, "test handshake bound");
            Bytes b(n);
            require(socket.read(b.data(), static_cast<int>(n), true) == static_cast<int>(n),
                    "test handshake body");
            return juce::JSON::parse(
                juce::String::fromUTF8(reinterpret_cast<const char*>(b.data()), static_cast<int>(n)));
        };
        const auto originalCode = pc.pairingCode();
        for (int attempt = 0; attempt < 5; ++attempt) {
            juce::StreamingSocket attacker;
            require(attacker.connect("127.0.0.1", pc.tcpPort(), 1000), "test attacker connect");
            auto id = randomId();
            frame(attacker, object({{"v", 1},
                                    {"type", "pair"},
                                    {"id", str(id)},
                                    {"name", "Abort test"},
                                    {"udp", phone.udpPort()},
                                    {"nonce", str(hex(randomKey()))}}));
            const auto hello = read(attacker);
            require(!hello.hasProperty("error"), "pairing unexpectedly rejected");
            Pairing exchange(Role::phone, originalCode, settings.id, id);
            frame(attacker, object({{"message", str(hex(exchange.message()))}}));
            auto proof = read(attacker);
            require(proof.hasProperty("proof"), "test proof missing");
            attacker.close();
            require(waitFor([&] { return pc.snapshot().triesLeft == 4 - attempt; }),
                    "abandoned exchange did not consume attempt");
        }
        require(pc.pairingCode().empty() && pc.settings().paired.empty(),
                "pairing gate exhaustion or unconfirmed key persistence");
        // A previously correct in-flight proof must not survive code replacement or expiry.
        auto rejectStaleExchange = [&](bool replaceCode) {
            pc.pairNew();
            const auto id = randomId();
            Pairing exchange(Role::phone, pc.pairingCode(), settings.id, id);
            juce::StreamingSocket pending;
            require(pending.connect("127.0.0.1", pc.tcpPort(), 1000), "stale test connect");
            frame(pending, object({{"v", 1}, {"type", "pair"}, {"id", str(id)},
                                   {"name", "Stale exchange test"}, {"udp", phone.udpPort()},
                                   {"nonce", str(hex(randomKey()))}}));
            require(!read(pending).hasProperty("error"), "stale test hello");
            frame(pending, object({{"message", str(hex(exchange.message()))}}));
            const auto response = read(pending);
            const auto key = exchange.finish(unhex(string(response, "message")));
            require(exchange.verifyConfirmation(key, unhex(string(response, "proof")), Role::pc),
                    "stale test PC proof");
            if (replaceCode)
                pc.pairNew();
            else {
                std::lock_guard<std::mutex> lock(pc.mutex_);
                pc.codeExpires_ = PairingGate::Clock::now();
            }
            frame(pending, object({{"proof", str(hex(exchange.confirmation(key, Role::phone)))}}));
            bool rejected = false;
            try {
                rejected = read(pending).hasProperty("error");
            } catch (const std::exception&) {
                rejected = true; // An invalidated exchange may be closed without an error frame.
            }
            require(rejected && pc.settings().paired.empty() && pc.snapshot().state != State::connected,
                    replaceCode ? "replaced code accepted an in-flight proof"
                                : "expired code accepted an in-flight proof");
        };
        rejectStaleExchange(true);
        rejectStaleExchange(false);
        pc.pairNew();
        const auto correctCode = pc.pairingCode();
        auto wrongCode = correctCode == "000000" ? "000001" : "000000";
        phone.connect(host, wrongCode);
        require(waitFor([&] { return !phone.snapshot().error.empty(); }), "wrong code did not fail");
        require(pc.settings().paired.empty() && phone.settings().paired.empty() &&
                    pc.snapshot().triesLeft == 4,
                "wrong code accepted or persisted");
        phone.connect(host, correctCode);
        auto connected = [&] {
            return pc.snapshot().state == State::connected && phone.snapshot().state == State::connected;
        };
        require(waitFor(connected), "pairing connection failed");
        require(waitFor([&] {
                    return pc.settings().mode == Mode::speakerphone &&
                           phone.settings().mode == Mode::speakerphone;
                }),
                "initial mode reconciliation failed");
        require(pc.settings().paired.size() == 1 && phone.settings().paired.size() == 1,
                "confirmed pair not remembered");
        Settings third = client;
        third.id = randomId();
        Runtime second(Role::phone, third, false);
        second.start();
        second.connect(host, correctCode);
        require(waitFor([&] { return !second.snapshot().error.empty(); }) && connected(),
                "second phone displaced active connection");
        phone.setControl({ControlKind::volume, 0.4, {}});
        require(waitFor([&] { return std::abs(pc.snapshot().volume - 0.4f) < 0.001f; }),
                "encrypted volume control missing");
        std::array<float, 480> l{}, r{}, mic{}, pcOut{}, phoneL{}, phoneR{};
        const float* pi[] = {l.data(), r.data()};
        float* po[] = {pcOut.data()};
        const float* fi[] = {mic.data()};
        float* fo[] = {phoneL.data(), phoneR.data()};
        auto audio = [&](int blocks) {
            double fromPc = 0, fromPhone = 0;
            for (int block = 0; block < blocks; ++block) {
                for (int i = 0; i < 480; ++i) {
                    l[i] = r[i] = 0.1f * std::sin((block * 480 + i) * 0.05f);
                    mic[i] = 0.1f * std::sin((block * 480 + i) * 0.04f);
                }
                pc.process(pi, po, 480);
                phone.process(fi, fo, 480);
                if (block > blocks / 2)
                    for (int i = 0; i < 480; ++i) {
                        fromPhone += std::abs(pcOut[i]);
                        fromPc += std::abs(phoneL[i]);
                    }
                juce::Thread::sleep(10);
            }
            return std::make_pair(fromPc, fromPhone);
        };
        auto playing = audio(110);
        require(playing.first > 1 && playing.second > 1, "encrypted UDP audio failed a direction");
        phone.setControl({ControlKind::mute, 1, {}});
        require(waitFor([&] { return pc.snapshot().muted && phone.snapshot().muted; }),
                "mute control missing");
        auto muted = audio(60);
        require(muted.first > 1 && muted.second < playing.second * 0.001,
                "mute did not silence decoded microphone audio");
        phone.setControl({ControlKind::mute, 0, {}});
        require(waitFor([&] { return !pc.snapshot().muted && !phone.snapshot().muted; }),
                "unmute control missing");
        auto unmuted = audio(60);
        require(unmuted.second > 1, "unmute did not restore microphone audio");
        juce::DatagramSocket spoof(false);
        require(spoof.bindToPort(0), "spoof socket bind");
        Bytes garbage(48, 0x42);
        auto rejected = pc.snapshot().rejected;
        spoof.write("127.0.0.1", pc.udpPort(), garbage.data(), static_cast<int>(garbage.size()));
        require(waitFor([&] { return pc.snapshot().rejected > rejected; }) && connected(),
                "unauthenticated UDP accepted or disconnected session");
        auto reconnectSettings = phone.settings();
        reconnectSettings.autoConnect = true;
        phone.updateSettings(reconnectSettings);
        require(waitFor([&] { return phone.settings().autoConnect; }), "auto reconnect setting");
        {
            std::lock_guard<std::mutex> l(pc.mutex_);
            pc.commands_.push_back([&pc] { pc.teardown(true); });
            pc.notify();
        }
        require(waitFor([&] { return phone.snapshot().state == State::reconnecting; }),
                "loss did not enter reconnect state");
        pc.process(pi, po, 480);
        require(std::all_of(pcOut.begin(), pcOut.end(), [](float v) { return v == 0; }),
                "disconnected audio not silent");
        require(waitFor(connected), "automatic stored-key reconnect failed");
        phone.disconnect();
        require(waitFor([&] {
                    return pc.snapshot().state == State::idle && phone.snapshot().state == State::idle;
                }),
                "explicit disconnect failed");
        phone.connect(host, "");
        require(waitFor(connected), "authenticated reconnect failed");
        phone.disconnect();
        require(waitFor([&] {
                    return pc.snapshot().state == State::idle && phone.snapshot().state == State::idle;
                }),
                "second disconnect failed");
        pc.forget(client.id);
        phone.forget(settings.id);
        require(waitFor([&] { return pc.settings().paired.empty() && phone.settings().paired.empty(); }),
                "forget failed");
        std::cout << "PASS: aborted PAKE attempt limit, wrong code, replaced/expired in-flight codes, single phone, mutual pairing, encrypted "
                     "controls and bidirectional UDP Opus, live mute/unmute, spoof rejection, disconnect, "
                     "stored-key and automatic reconnect, forget\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: network test: " << e.what() << "\n";
        return 1;
    }
}
} // namespace aw
