// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/AudioPlatform.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <UIKit/UIKit.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <juce_core/juce_core.h>
#include <mutex>
#include <stdexcept>
namespace aw {
class VoiceProcessingAudioIODevice final : public AudioPlatform {
  public:
    ~VoiceProcessingAudioIODevice() override {
        stop();
    }
    void start(const AudioOptions& options, AudioCallback callback) override {
        stop();
        options_ = options;
        callback_ = std::move(callback);
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;
        hasMic_ = records(options.mode) && session.recordPermission == AVAudioSessionRecordPermissionGranted;
        const bool voice = options.mode == Mode::speakerphone && options.aec && hasMic_;
        AVAudioSessionCategoryOptions category =
            AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetooth;
        if (options.mix)
            category |= AVAudioSessionCategoryOptionMixWithOthers;
        if (!voice)
            category |= AVAudioSessionCategoryOptionAllowBluetoothA2DP;
        if (![session
                setCategory:hasMic_ ? AVAudioSessionCategoryPlayAndRecord : AVAudioSessionCategoryPlayback
                       mode:voice ? AVAudioSessionModeVoiceChat : AVAudioSessionModeDefault
                    options:hasMic_ ? category
                                    : (options.mix ? AVAudioSessionCategoryOptionMixWithOthers : 0)error
                           :&error])
            throw std::runtime_error(
                "iPhone audio could not start. Check microphone permission in Settings.");
        [session setPreferredSampleRate:options.force44100 ? 44100 : 48000 error:nil];
        [session setPreferredIOBufferDuration:0.01 error:nil];
        if (![session setActive:YES error:&error])
            throw std::runtime_error("iPhone audio is busy. Finish the current call and reconnect.");
        rate_ = session.sampleRate;
        AudioComponentDescription description{};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType =
            voice ? kAudioUnitSubType_VoiceProcessingIO : kAudioUnitSubType_RemoteIO;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        auto component = AudioComponentFindNext(nullptr, &description);
        if (!component || AudioComponentInstanceNew(component, &unit_) != noErr)
            throw std::runtime_error("iPhone speaker could not start.");
        auto require = [&](OSStatus status) {
            if (status != noErr) {
                stop();
                throw std::runtime_error("iPhone audio setup failed. Try reconnecting.");
            }
        };
        UInt32 one = 1, zero = 0, maximum = 4096;
        require(AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0,
                                     &one, sizeof(one)));
        require(AudioUnitSetProperty(unit_, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1,
                                     hasMic_ ? &one : &zero, sizeof(one)));
        AudioStreamBasicDescription output{};
        output.mSampleRate = rate_;
        output.mFormatID = kAudioFormatLinearPCM;
        output.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
        output.mBytesPerPacket = output.mBytesPerFrame = 4;
        output.mFramesPerPacket = 1;
        output.mChannelsPerFrame = 2;
        output.mBitsPerChannel = 32;
        require(AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                     &output, sizeof(output)));
        if (hasMic_) {
            auto input = output;
            input.mChannelsPerFrame = 1;
            require(AudioUnitSetProperty(unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1,
                                         &input, sizeof(input)));
            require(AudioUnitSetProperty(unit_, kAudioUnitProperty_ShouldAllocateBuffer,
                                         kAudioUnitScope_Output, 1, &zero, sizeof(zero)));
        }
        require(AudioUnitSetProperty(unit_, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
                                     0, &maximum, sizeof(maximum)));
        AURenderCallbackStruct render{renderCallback, this};
        require(AudioUnitSetProperty(unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                                     &render, sizeof(render)));
        require(AudioUnitInitialize(unit_));
        if (voice) {
            require(AudioUnitSetProperty(unit_, kAUVoiceIOProperty_BypassVoiceProcessing,
                                         kAudioUnitScope_Global, 0, &zero, sizeof(zero)));
            require(AudioUnitSetProperty(unit_, kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                                         kAudioUnitScope_Global, 0, &one, sizeof(one)));
        }
        running_ = true;
        require(AudioOutputUnitStart(unit_));
        updateRoute();
        auto* self = this;
        interruption_ = [[NSNotificationCenter defaultCenter]
            addObserverForName:AVAudioSessionInterruptionNotification
                        object:session
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification* notification) {
                      auto type =
                          [notification.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
                      if (type == AVAudioSessionInterruptionTypeBegan) {
                          self->interrupted_ = true;
                          if (self->unit_)
                              AudioOutputUnitStop(self->unit_);
                      } else {
                          auto flags = [notification.userInfo[AVAudioSessionInterruptionOptionKey]
                              unsignedIntegerValue];
                          if (flags & AVAudioSessionInterruptionOptionShouldResume) {
                              [[AVAudioSession sharedInstance] setActive:YES error:nil];
                              if (self->unit_)
                                  AudioOutputUnitStart(self->unit_);
                              self->interrupted_ = false;
                          }
                      }
                    }];
        routeObserver_ =
            [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionRouteChangeNotification
                                                              object:session
                                                               queue:[NSOperationQueue mainQueue]
                                                          usingBlock:^(NSNotification*) {
                                                            self->updateRoute();
                                                          }];
        lifecycle_ = [[NSNotificationCenter defaultCenter]
            addObserverForName:UIApplicationDidEnterBackgroundNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification*) {
                      if (!self->options_.background) {
                          self->interrupted_ = true;
                          if (self->unit_)
                              AudioOutputUnitStop(self->unit_);
                      }
                    }];
        foreground_ = [[NSNotificationCenter defaultCenter]
            addObserverForName:UIApplicationWillEnterForegroundNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification*) {
                      if (self->unit_ && self->interrupted_) {
                          [[AVAudioSession sharedInstance] setActive:YES error:nil];
                          AudioOutputUnitStart(self->unit_);
                          self->interrupted_ = false;
                      }
                    }];
        [UIDevice currentDevice].batteryMonitoringEnabled = YES;
    }
    void stop() override {
        running_ = false;
        for (id observer in @[
                 interruption_ ?: [NSNull null], routeObserver_ ?: [NSNull null], lifecycle_ ?: [NSNull null],
                 foreground_ ?: [NSNull null]
             ])
            if (observer != [NSNull null])
                [[NSNotificationCenter defaultCenter] removeObserver:observer];
        interruption_ = routeObserver_ = lifecycle_ = foreground_ = nil;
        if (unit_) {
            AudioOutputUnitStop(unit_);
            AudioUnitUninitialize(unit_);
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
        }
        [[AVAudioSession sharedInstance] setActive:NO
                                       withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                             error:nil];
    }
    double sampleRate() const override {
        return rate_;
    }
    bool microphoneAvailable() const override {
        return hasMic_;
    }
    bool interrupted() const override {
        return interrupted_;
    }
    bool lowPower() const override {
        return [NSProcessInfo processInfo].lowPowerModeEnabled ||
               [NSProcessInfo processInfo].thermalState >= NSProcessInfoThermalStateSerious;
    }
    double battery() const override {
        const float level = [UIDevice currentDevice].batteryLevel;
        return level < 0 ? -1 : level * 100;
    }
    std::string status() const override {
        return hasMic_ ? "iPhone speaker and microphone ready"
                       : "Speaker mode ready. Enable microphone access in iPhone Settings for calls.";
    }
    std::string route() const override {
        std::lock_guard<std::mutex> l(mutex_);
        return route_;
    }
    bool useAsDefault(bool) override {
        return false;
    }

  private:
    void updateRoute() {
        auto port = [AVAudioSession sharedInstance].currentRoute.outputs.firstObject;
        std::lock_guard<std::mutex> l(mutex_);
        route_ = port.portName ? port.portName.UTF8String : "iPhone Speaker";
    }
    static OSStatus renderCallback(void* context, AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* time, UInt32, UInt32 frames, AudioBufferList* data) {
        auto* self = static_cast<VoiceProcessingAudioIODevice*>(context);
        for (UInt32 i = 0; i < data->mNumberBuffers; ++i)
            if (data->mBuffers[i].mData)
                std::memset(data->mBuffers[i].mData, 0, data->mBuffers[i].mDataByteSize);
        if (!self->running_ || self->interrupted_ || frames > 4096)
            return noErr;
        std::fill_n(self->mic_.data(), frames, 0);
        if (self->hasMic_) {
            AudioBufferList mic{};
            mic.mNumberBuffers = 1;
            mic.mBuffers[0] = {1, frames * 4, self->mic_.data()};
            if (AudioUnitRender(self->unit_, flags, time, 1, frames, &mic) != noErr)
                std::fill_n(self->mic_.data(), frames, 0);
        }
        const float* input[] = {self->mic_.data()};
        float* output[] = {self->left_.data(), self->right_.data()};
        self->callback_(input, output, static_cast<int>(frames));
        for (UInt32 b = 0; b < data->mNumberBuffers; ++b) {
            auto& buffer = data->mBuffers[b];
            auto* out = static_cast<float*>(buffer.mData);
            if (!out)
                continue;
            const UInt32 capacity = buffer.mDataByteSize / (4 * std::max(1u, buffer.mNumberChannels)),
                         count = std::min(frames, capacity);
            for (UInt32 i = 0; i < count; ++i)
                for (UInt32 ch = 0; ch < buffer.mNumberChannels; ++ch)
                    out[i * buffer.mNumberChannels + ch] = (b + ch) == 0 ? self->left_[i] : self->right_[i];
        }
        return noErr;
    }
    AudioOptions options_;
    AudioCallback callback_;
    AudioUnit unit_ = nullptr;
    std::array<float, 4096> mic_{}, left_{}, right_{};
    std::atomic<bool> running_{false}, interrupted_{false};
    bool hasMic_ = false;
    double rate_ = 48000;
    mutable std::mutex mutex_;
    std::string route_ = "iPhone Speaker";
    id interruption_ = nil, routeObserver_ = nil, lifecycle_ = nil, foreground_ = nil;
};
std::unique_ptr<AudioPlatform> makeAudioPlatform() {
    return std::make_unique<VoiceProcessingAudioIODevice>();
}
void setAutostart(bool) {}
void openSoundSettings() {
    openPermissionSettings();
}
void openPermissionSettings() {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                                       options:@{}
                             completionHandler:nil];
}
} // namespace aw
