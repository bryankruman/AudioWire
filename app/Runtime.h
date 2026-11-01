// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Settings.h"
#include "core/Control.h"
#include "core/Discovery.h"
#include "core/KeyStore.h"
#include "core/Pairing.h"
#include "core/SecureTransport.h"
#include "core/Session.h"
#include "core/Transport.h"
#include <deque>
#include <juce_events/juce_events.h>
#include <mutex>
namespace aw {
struct Snapshot {
    State state = State::idle;
    std::string peer, error, driver, route;
    std::vector<Device> devices;
    double latencyMs = 0, battery = -1;
    uint64_t packets = 0, rejected = 0, lost = 0;
    int bufferMs = 20;
    bool micAvailable = false, muted = false;
    float input = 0, output = 0, volume = 0.8f;
    int triesLeft = 5, pairSeconds = 0;
};
class Runtime : private juce::Thread {
  public:
    Runtime(Role, Settings, bool persist = true);
    ~Runtime() override;
    void start();
    void pairNew();
    void connect(Device, const std::string& code);
    void disconnect();
    void forget(const std::string& id);
    void setControl(Control);
    void updateSettings(const Settings&);
    Settings settings() const;
    Snapshot snapshot() const;
    std::string pairingCode() const;
    std::string pairingUrl() const;
    uint16_t tcpPort() const {
        return tcpPort_;
    }
    uint16_t udpPort() const {
        return udpPort_;
    }
    std::string diagnostics() const;
    void process(const float* const*, float* const*, int);
    void attachAudioInfo(double rate, int frames, bool microphone, std::string status, std::string route);
    void reportError(std::string error) {
        std::lock_guard<std::mutex> l(mutex_);
        snapshot_.error = std::move(error);
    }
    void gateMicrophone() {
        gateFrames_ = static_cast<int>(rate_.load() / 2);
    }

  private:
    friend int runNetworkTest();
    void run() override;
    void serverHandshake(std::unique_ptr<juce::StreamingSocket>);
    void clientHandshake(const Device&, const std::string&);
    void establish(std::unique_ptr<juce::StreamingSocket>, const Device&, uint16_t, SessionKeys);
    void teardown(bool retry);
    void handleControl(const Control&);
    void publish();
    void remember(Device);
    void sendJson(juce::StreamingSocket&, const juce::var&);
    juce::var readJson(juce::StreamingSocket&);
    Bytes readFrame(juce::StreamingSocket&);
    void writeFrame(juce::StreamingSocket&, const Bytes&);
    void exactRead(juce::StreamingSocket&, void*, int);
    void sendControl(const Control&);
    Role role_;
    bool persist_;
    mutable std::mutex mutex_;
    Settings settings_;
    Snapshot snapshot_;
    Session session_;
    PairingGate gate_;
    std::string code_;
    PairingGate::Clock::time_point codeExpires_{};
    std::unique_ptr<KeyStore> keys_;
    std::unique_ptr<Discovery> discovery_;
    std::unique_ptr<juce::StreamingSocket> listener_, control_;
    std::unique_ptr<juce::DatagramSocket> udp_;
    uint16_t tcpPort_ = 0, udpPort_ = 0, remotePort_ = 0;
    Device remote_;
    std::unique_ptr<Transport> transport_;
    std::unique_ptr<SecureTransport> audioSecure_, controlSecure_;
    std::atomic<Transport*> realtime_{nullptr};
    std::atomic<unsigned> callbacks_{0};
    std::atomic<int> mode_{0}, quality_{0};
    std::atomic<bool> mute_{false}, micAvailable_{false};
    std::atomic<float> volume_{0.8f}, inputMeter_{0}, outputMeter_{0};
    std::atomic<double> rate_{48000};
    std::atomic<int> frames_{4096};
    std::atomic<int> gateFrames_{24000};
    std::deque<std::function<void()>> commands_;
    bool disconnectRequested_ = false;
    double disconnectDeadline_ = 0;
    double lastPing_ = 0, lastBattery_ = -2;
    uint64_t packets_ = 0, rejected_ = 0;
    double latencyMs_ = 0;
    std::array<float, 4096> silence_{};
    std::array<float, 4096> micScratch_{};
};
std::string lanAddress();
int runNetworkTest();
} // namespace aw
