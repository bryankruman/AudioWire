// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Runtime.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <qrcodegen.hpp>
namespace aw {
class AwLookAndFeel : public juce::LookAndFeel_V4 {
  public:
    AwLookAndFeel();
    void setTheme(bool dark);
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};
class MainView : public juce::Component, private juce::Timer {
  public:
    MainView(Runtime&, AudioPlatform*, bool phone);
    ~MainView() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void handlePairLink(const juce::String&);
    void previewTheme(bool dark) {
        dark_ = dark;
        look_.setTheme(dark);
        sendLookAndFeelChange();
    }
    void showPairing();
    void showPage(int p) {
        page(p);
    }

  private:
    void timerCallback() override;
    void page(int);
    void connect();
    void saveSettings();
    void refreshPaired();
    void paintMeter(juce::Graphics&, juce::Rectangle<float>, float, const juce::String&);
    Runtime& runtime_;
    AudioPlatform* audio_;
    bool phone_;
    int page_ = 0;
    AwLookAndFeel look_;
    juce::TextButton home_{"Connection"}, settingsButton_{"Settings"}, diagnosticsButton_{"Diagnostics"},
        action_{"Pair a new phone"};
    juce::TextButton connect_{"Connect"}, sound_{"Windows Sound settings"}, copy_{"Copy diagnostics"},
        forget_{"Forget selected device"}, source_{"Source & licence"},
        permission_{"iPhone permission settings"};
    juce::ComboBox devices_, mode_, quality_, paired_, capture_;
    juce::TextEditor address_, code_, name_;
    juce::Slider volume_, reserve_;
    juce::ToggleButton mute_{"Mute microphone"}, mix_{"Mix with other phone audio"},
        background_{"Keep playing when I switch apps"}, aec_{"Echo cancellation"},
        notifications_{"Show connection notifications"}, autostart_{"Start with Windows"},
        allSound_{"Use iPhone for all PC sound"}, mirror_{"Also play on this PC's speakers"},
        autoRevert_{"Restore PC sound on disconnect"}, force44100_{"Use 44.1 kHz"},
        autoConnect_{"Reconnect automatically"};
    juce::Label error_, subtitle_, pairHint_;
    juce::Viewport viewport_;
    juce::Component settingsContent_;
    std::string lastQr_;
    std::unique_ptr<qrcodegen::QrCode> qr_;
    std::vector<Device> devicesCache_;
    bool connectedBefore_ = false, loading_ = false, allSoundApplied_ = false;
    bool dark_ = true;
};
} // namespace aw

namespace aw {
int renderUi(const juce::File&);
}
