// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include "WindowsIntegration.h"
#include "core/AudioRing.h"
#include "driver/shared/AwProtocol.h"
#include "platform/AudioPlatform.h"
#include <audioclient.h>
#include <avrt.h>
#include <cmath>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <mmdeviceapi.h>
#include <mutex>
#include <thread>
#include <wrl/client.h>
namespace aw {
using Microsoft::WRL::ComPtr;
namespace {
const GUID floatSubtype = {3, 0, 0x10, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
std::string name(IMMDevice* device) {
    ComPtr<IPropertyStore> store;
    PROPVARIANT value;
    PropVariantInit(&value);
    std::string result;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) &&
        SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR)
        result = juce::String(value.pwszVal).toStdString();
    PropVariantClear(&value);
    return result;
}
std::wstring identity(IMMDevice* device) {
    LPWSTR p = nullptr;
    device->GetId(&p);
    std::wstring result = p ? p : L"";
    CoTaskMemFree(p);
    return result;
}
ComPtr<IMMDevice> find(IMMDeviceEnumerator* enumerator, EDataFlow flow, const std::string& query) {
    ComPtr<IMMDeviceCollection> devices;
    ComPtr<IMMDevice> result;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &devices))) {
        UINT count = 0;
        devices->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> d;
            devices->Item(i, &d);
            if (name(d.Get()).find(query) != std::string::npos) {
                result = d;
                break;
            }
        }
    }
    return result;
}
WAVEFORMATEXTENSIBLE floatFormat(int channels) {
    WAVEFORMATEXTENSIBLE f{};
    f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.Format.nChannels = static_cast<WORD>(channels);
    f.Format.nSamplesPerSec = 48000;
    f.Format.wBitsPerSample = 32;
    f.Format.nBlockAlign = static_cast<WORD>(channels * 4);
    f.Format.nAvgBytesPerSec = 48000 * f.Format.nBlockAlign;
    f.Format.cbSize = 22;
    f.Samples.wValidBitsPerSample = 32;
    f.dwChannelMask = channels == 1 ? SPEAKER_FRONT_CENTER : SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    f.SubFormat = floatSubtype;
    return f;
}
float sample(const BYTE* p, int bits, bool floating) {
    if (floating && bits == 32) {
        float v;
        std::memcpy(&v, p, 4);
        return std::isfinite(v) ? v : 0;
    }
    if (bits == 16) {
        short v;
        std::memcpy(&v, p, 2);
        return v / 32768.0f;
    }
    if (bits == 24) {
        int v = static_cast<int>(p[0] | (p[1] << 8) | (p[2] << 16));
        if (v & 0x800000)
            v |= static_cast<int>(0xff000000);
        return v / 8388608.0f;
    }
    if (bits == 32) {
        int32_t v;
        std::memcpy(&v, p, 4);
        return static_cast<float>(v / 2147483648.0);
    }
    return 0;
}
} // namespace
class WinAudioBridge final : public AudioPlatform {
  public:
    ~WinAudioBridge() override {
        stop();
    }
    void start(const AudioOptions& options, AudioCallback cb) override {
        stop();
        captureReady_ = false;
        captureError_.clear();
        hasMic_ = false;
        options_ = options;
        callback_ = std::move(cb);
        running_ = true;
        captureThread_ = std::thread([this] { capture(); });
        clockThread_ = std::thread([this] { clock(); });
    }
    void stop() override {
        running_ = false;
        if (captureThread_.joinable())
            captureThread_.join();
        if (clockThread_.joinable())
            clockThread_.join();
        useAsDefault(false);
    }
    int maxFrames() const override {
        return 480;
    }
    bool captureReady() const override {
        return captureReady_;
    }
    bool microphoneAvailable() const override {
        return hasMic_;
    }
    std::string status() const override {
        std::lock_guard<std::mutex> l(mutex_);
        return captureError_.empty() ? status_ : captureError_;
    }
    std::string route() const override {
        return "Your connected iPhone";
    }
    std::vector<Device> captureDevices() const override {
        std::vector<Device> result;
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IMMDeviceEnumerator> enumerator;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       IID_PPV_ARGS(&enumerator)))) {
            ComPtr<IMMDeviceCollection> devices;
            if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
                UINT count = 0;
                devices->GetCount(&count);
                for (UINT i = 0; i < count; ++i) {
                    ComPtr<IMMDevice> d;
                    devices->Item(i, &d);
                    auto n = name(d.Get());
                    result.push_back({n, n, "", 0});
                }
            }
        }
        if (SUCCEEDED(initialized))
            CoUninitialize();
        return result;
    }
    bool useAsDefault(bool enabled) override {
        std::lock_guard<std::mutex> l(mutex_);
        if (enabled && speakerId_.empty()) {
            status_ = "Install the AudioWire audio driver before selecting it for all PC sound.";
            return false;
        }
        std::string error;
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool ok = switchDefaultAudio(speakerId_, micId_, !enabled, error);
        if (SUCCEEDED(initialized))
            CoUninitialize();
        if (!error.empty())
            status_ = error;
        return ok;
    }

  private:
    void report(std::string s) {
        std::lock_guard<std::mutex> l(mutex_);
        status_ = std::move(s);
    }
    void captureReport(std::string s) {
        std::lock_guard<std::mutex> l(mutex_);
        captureError_ = std::move(s);
    }
    void capture() {
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        while (running_) {
            ComPtr<IMMDeviceEnumerator> enumerator;
            ComPtr<IMMDevice> device;
            ComPtr<IAudioClient> client;
            ComPtr<IAudioCaptureClient> reader;
            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                        IID_PPV_ARGS(&enumerator)))) {
                captureReport("Windows audio is unavailable. Open Windows Sound settings.");
                break;
            }
            device = options_.captureDevice.empty() ? find(enumerator.Get(), eRender, "AudioWire Speaker")
                                                    : find(enumerator.Get(), eRender, options_.captureDevice);
            if (device && name(device.Get()).find("AudioWire Speaker") != std::string::npos) {
                std::lock_guard<std::mutex> l(mutex_);
                speakerId_ = identity(device.Get());
            } else
                enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
            if (!device) {
                captureReport("No PC speakers found. Open Windows Sound settings.");
                juce::Thread::sleep(1000);
                continue;
            }
            if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                        reinterpret_cast<void**>(client.GetAddressOf())))) {
                juce::Thread::sleep(500);
                continue;
            }
            WAVEFORMATEX* format = nullptr;
            if (FAILED(client->GetMixFormat(&format))) {
                juce::Thread::sleep(500);
                continue;
            }
            bool floating =
                format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                 IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat, floatSubtype));
            const auto bits = format->wBitsPerSample, channels = format->nChannels;
            const double ratio = format->nSamplesPerSec / 48000.0;
            auto hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 200000, 0,
                                         format, nullptr);
            CoTaskMemFree(format);
            if (FAILED(hr) || FAILED(client->GetService(IID_PPV_ARGS(&reader))) || FAILED(client->Start())) {
                captureReport("PC sound could not start. Check Windows Sound settings.");
                juce::Thread::sleep(500);
                continue;
            }
            captureReport("");
            captureReady_ = true;
            juce::LagrangeInterpolator interpolate[2];
            std::array<float, 16384> raw[2];
            std::array<float, 16384> converted[2];
            bool healthy = true;
            while (running_ && healthy) {
                UINT frames = 0;
                hr = reader->GetNextPacketSize(&frames);
                if (FAILED(hr)) {
                    healthy = false;
                    break;
                }
                while (frames && running_) {
                    BYTE* data = nullptr;
                    DWORD flags = 0;
                    UINT count = 0;
                    hr = reader->GetBuffer(&data, &count, &flags, nullptr, nullptr);
                    if (FAILED(hr)) {
                        healthy = false;
                        break;
                    }
                    if (count <= 8192 && channels > 0) {
                        for (UINT i = 0; i < count; ++i)
                            for (int ch = 0; ch < 2; ++ch)
                                raw[ch][i] =
                                    (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                                        ? 0
                                        : sample(data + (i * channels +
                                                         std::min(ch, static_cast<int>(channels) - 1)) *
                                                            (bits / 8),
                                                 bits, floating);
                        if (std::abs(ratio - 1.0) < 0.0001) {
                            speaker_[0].write(raw[0].data(), count);
                            speaker_[1].write(raw[1].data(), count);
                        } else {
                            const int out = std::min(16380, static_cast<int>(count / ratio));
                            for (int ch = 0; ch < 2; ++ch) {
                                interpolate[ch].process(ratio, raw[ch].data(), converted[ch].data(), out,
                                                        static_cast<int>(count), 0);
                                speaker_[ch].write(converted[ch].data(), out);
                            }
                        }
                    }
                    reader->ReleaseBuffer(count);
                    if (FAILED(reader->GetNextPacketSize(&frames))) {
                        healthy = false;
                        break;
                    }
                }
                juce::Thread::sleep(2);
            }
            captureReady_ = false;
            client->Stop();
        }
        if (SUCCEEDED(initialized))
            CoUninitialize();
    }
    void clock() {
        DWORD task = 0;
        HANDLE scheduling = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
        HANDLE timer =
            CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        HANDLE driver = INVALID_HANDLE_VALUE;
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> cable, realSpeaker;
        ComPtr<IAudioClient> cableClient, mirrorClient;
        ComPtr<IAudioRenderClient> cableWriter, mirrorWriter;
        auto openOutput = [&](IMMDevice* d, ComPtr<IAudioClient>& client,
                              ComPtr<IAudioRenderClient>& writer) {
            if (!d)
                return false;
            auto f = floatFormat(2);
            return SUCCEEDED(d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                         reinterpret_cast<void**>(client.GetAddressOf()))) &&
                   SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                                    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                                400000, 0, &f.Format, nullptr)) &&
                   SUCCEEDED(client->GetService(IID_PPV_ARGS(&writer))) && SUCCEEDED(client->Start());
        };
        auto writeOutput = [&](ComPtr<IAudioClient>& client, ComPtr<IAudioRenderClient>& writer,
                               const float* mono, const float* l, const float* r) {
            if (!client || !writer)
                return;
            UINT size = 0, padding = 0;
            client->GetBufferSize(&size);
            if (FAILED(client->GetCurrentPadding(&padding)) || size - padding < 480)
                return;
            BYTE* data = nullptr;
            if (SUCCEEDED(writer->GetBuffer(480, &data))) {
                auto* out = reinterpret_cast<float*>(data);
                for (int i = 0; i < 480; ++i) {
                    out[i * 2] = mono ? mono[i] : l[i];
                    out[i * 2 + 1] = mono ? mono[i] : r[i];
                }
                writer->ReleaseBuffer(480, 0);
            }
        };
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (enumerator) {
            cable = find(enumerator.Get(), eRender, "CABLE Input");
            auto mic = find(enumerator.Get(), eCapture, "AudioWire Microphone");
            if (mic) {
                std::lock_guard<std::mutex> l(mutex_);
                micId_ = identity(mic.Get());
            }
            if (options_.feedMicrophone && cable)
                openOutput(cable.Get(), cableClient, cableWriter);
            enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &realSpeaker);
            if (options_.mirror && realSpeaker &&
                name(realSpeaker.Get()).find("AudioWire") == std::string::npos)
                openOutput(realSpeaker.Get(), mirrorClient, mirrorWriter);
        }
        std::array<float, 480> l{}, r{}, mic{};
        const float* input[] = {l.data(), r.data()};
        float* output[] = {mic.data()};
        auto deadline = std::chrono::steady_clock::now();
        int retry = 0;
        while (running_) {
            if (options_.feedMicrophone && driver == INVALID_HANDLE_VALUE && retry++ % 100 == 0) {
                driver = CreateFileW(AW_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                     0, nullptr);
                hasMic_ = driver != INVALID_HANDLE_VALUE || cableWriter;
                report(driver != INVALID_HANDLE_VALUE ? "AudioWire audio devices ready"
                       : cableWriter ? "Using your existing microphone audio connection"
                                     : "Speaker mode works. Install the AudioWire audio driver to use your "
                                       "iPhone microphone.");
            }
            speaker_[0].read(l.data(), 480);
            speaker_[1].read(r.data(), 480);
            callback_(input, output, 480);
            if (driver != INVALID_HANDLE_VALUE) {
                AW_AUDIO_BLOCK block{};
                block.version = AW_ABI_VERSION;
                block.count = 480;
                for (int i = 0; i < 480; ++i)
                    block.samples[i] = static_cast<short>(std::clamp(mic[i], -1.0f, 1.0f) * 32767);
                DWORD done = 0;
                if (!DeviceIoControl(driver, IOCTL_AUDIOWIRE_WRITE, &block, sizeof(block), nullptr, 0, &done,
                                     nullptr)) {
                    CloseHandle(driver);
                    driver = INVALID_HANDLE_VALUE;
                    hasMic_ = cableWriter != nullptr;
                }
            } else
                writeOutput(cableClient, cableWriter, mic.data(), nullptr, nullptr);
            writeOutput(mirrorClient, mirrorWriter, nullptr, l.data(), r.data());
            deadline += std::chrono::milliseconds(10);
            if (deadline < std::chrono::steady_clock::now() - std::chrono::milliseconds(30))
                deadline = std::chrono::steady_clock::now();
            if (timer) {
                auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     deadline - std::chrono::steady_clock::now())
                                     .count();
                if (remaining > 0) {
                    LARGE_INTEGER due;
                    due.QuadPart = -std::max<LONGLONG>(1, remaining / 100);
                    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                        WaitForSingleObject(timer, INFINITE);
                    else
                        std::this_thread::sleep_until(deadline);
                }
            } else
                std::this_thread::sleep_until(deadline);
        }
        if (timer)
            CloseHandle(timer);
        if (scheduling)
            AvRevertMmThreadCharacteristics(scheduling);
        if (driver != INVALID_HANDLE_VALUE)
            CloseHandle(driver);
        if (cableClient)
            cableClient->Stop();
        if (mirrorClient)
            mirrorClient->Stop();
        if (SUCCEEDED(initialized))
            CoUninitialize();
    }
    AudioOptions options_;
    AudioCallback callback_;
    std::atomic<bool> running_{false}, hasMic_{false}, captureReady_{false};
    std::thread captureThread_, clockThread_;
    AudioRing<48000> speaker_[2];
    mutable std::mutex mutex_;
    std::string captureError_;
    std::string status_ = "Checking Windows audio...";
    std::wstring speakerId_, micId_;
};
int runAudioTest() {
    auto audio = std::make_unique<WinAudioBridge>();
    AudioOptions options;
    options.feedMicrophone = false;
    options.mode = Mode::speaker;
    std::atomic<int> blocks{0};
    audio->start(options, [&](const float* const* input, float* const* output, int frames) {
        if (frames == 480 && input && input[0] && input[1])
            ++blocks;
        std::fill_n(output[0], frames, 0.0f);
    });
    for (int i = 0; i < 100 && (!audio->captureReady() || blocks < 100); ++i)
        juce::Thread::sleep(20);
    const bool ok = audio->captureReady() && blocks >= 100;
    const auto status = audio->status();
    audio->stop();
    if (ok) {
        std::cout << "PASS: WASAPI loopback capture opened and 10 ms audio callbacks ran; microphone "
                     "injection disabled\n";
        return 0;
    }
    std::cerr << "FAIL: Windows audio smoke check: " << status << " (" << blocks.load() << " callbacks)\n";
    return 1;
}
std::unique_ptr<AudioPlatform> makeAudioPlatform() {
    return std::make_unique<WinAudioBridge>();
}
} // namespace aw
