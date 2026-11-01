// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/Types.h"
#include <functional>
#include <memory>
namespace aw {
using AudioCallback = std::function<void(const float* const*, float* const*, int)>;
struct AudioOptions {
    Mode mode = Mode::speakerphone;
    bool mix = false, aec = true, background = true, force44100 = false, mirror = false,
         feedMicrophone = true;
    std::string captureDevice;
};
class AudioPlatform {
  public:
    virtual ~AudioPlatform() = default;
    virtual void start(const AudioOptions&, AudioCallback) = 0;
    virtual void stop() = 0;
    virtual double sampleRate() const {
        return 48000;
    }
    virtual int maxFrames() const {
        return 4096;
    }
    virtual bool captureReady() const {
        return false;
    }
    virtual bool microphoneAvailable() const = 0;
    virtual std::string status() const = 0;
    virtual std::string route() const = 0;
    virtual bool interrupted() const {
        return false;
    }
    virtual bool lowPower() const {
        return false;
    }
    virtual double battery() const {
        return -1;
    }
    virtual std::vector<Device> captureDevices() const {
        return {};
    }
    virtual bool useAsDefault(bool) = 0;
};
std::unique_ptr<AudioPlatform> makeAudioPlatform();
int runAudioTest();
void setAutostart(bool);
void openSoundSettings();
void openPermissionSettings();
} // namespace aw
