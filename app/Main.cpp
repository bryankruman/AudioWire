// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainView.h"
#if JUCE_WINDOWS
#include "platform/windows/WindowsIntegration.h"
#endif
namespace aw {
class App final : public juce::JUCEApplication, private juce::Timer {
    class PhoneScreen final : public juce::Component {
        juce::Viewport viewport_;
        MainView* content_;

      public:
        explicit PhoneScreen(MainView* view) : content_(view) {
            addAndMakeVisible(viewport_);
            viewport_.setViewedComponent(view, true);
            viewport_.setScrollBarsShown(true, false);
        }
        void resized() override {
            auto bounds = getLocalBounds();
#if JUCE_IOS
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                bounds.removeFromTop(display->safeAreaInsets.getTop());
                bounds.removeFromBottom(display->safeAreaInsets.getBottom());
            }
#endif
            viewport_.setBounds(bounds);
            content_->setSize(std::max(300, bounds.getWidth() - 10), std::max(900, bounds.getHeight()));
        }
        MainView* view() const {
            return content_;
        }
    };
    class Window final : public juce::DocumentWindow {
      public:
        explicit Window(MainView* view) : DocumentWindow("AudioWire", juce::Colour(0xff10151c), allButtons) {
            setUsingNativeTitleBar(true);
#if JUCE_IOS
            setContentOwned(new PhoneScreen(view), true);
#else
            setContentOwned(view, true);
#endif
#if JUCE_IOS
            setFullScreen(true);
#else
            setResizable(true, false);
            setResizeLimits(500, 900, 760, 1200);
            centreWithSize(540, 900);
#endif
        }
        void closeButtonPressed() override {
#if JUCE_IOS
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
#else
            setVisible(false);
#endif
        }
    };
#if JUCE_WINDOWS
    class Tray final : public juce::SystemTrayIconComponent {
      public:
        explicit Tray(App& app) : app_(app) {
            update(false);
        }
        void update(bool connected) {
            juce::Image image(juce::Image::ARGB, 32, 32, true);
            juce::Graphics g(image);
            g.setColour(connected ? juce::Colour(0xff73dfbf) : juce::Colour(0xff98a8ba));
            g.drawRoundedRectangle(4, 7, 24, 18, 5, 3);
            g.drawLine(11, 13, 11, 19, 3);
            g.drawLine(16, 10, 16, 22, 3);
            g.drawLine(21, 13, 21, 19, 3);
            setIconImage(image, image);
        }
        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isPopupMenu()) {
                juce::PopupMenu menu;
                menu.addItem(1, "Open AudioWire");
                menu.addItem(2, "Pair a new phone");
                menu.addSeparator();
                menu.addItem(3, "Quit AudioWire");
                menu.showMenuAsync(juce::PopupMenu::Options(), [this](int id) {
                    if (id == 1)
                        app_.show();
                    if (id == 2) {
                        app_.runtime_->pairNew();
                        app_.show();
                    }
                    if (id == 3)
                        app_.quit();
                });
            } else
                app_.show();
        }

      private:
        App& app_;
    };
#endif
  public:
    const juce::String getApplicationName() override {
        return "AudioWire";
    }
    const juce::String getApplicationVersion() override {
        return AUDIOWIRE_VERSION;
    }
    bool moreThanOneInstanceAllowed() override {
        auto args = juce::StringArray::fromTokens(getCommandLineParameters(), true);
        if (args.isEmpty())
            return false;
        const auto first = args[0].unquoted();
        return first == "--network-test" || first == "--render-ui" || first == "--remove-user-startup" ||
               first == "--audio-test";
    }
    void initialise(const juce::String& command) override {
        const auto tokens = juce::StringArray::fromTokens(command, true);
        const auto first = tokens.isEmpty() ? juce::String() : tokens[0].unquoted();
        if (first == "--remove-user-startup") {
            setAutostart(false);
            quit();
            return;
        }
        if (first == "--render-ui") {
            auto args = juce::StringArray::fromTokens(command, true);
            if (args.size() != 2) {
                setApplicationReturnValue(2);
                quit();
                return;
            }
            setApplicationReturnValue(renderUi(juce::File(args[1].unquoted())));
            quit();
            return;
        }
#if JUCE_WINDOWS
        if (first == "--audio-test") {
            setApplicationReturnValue(runAudioTest());
            quit();
            return;
        }
#endif
        if (first == "--network-test") {
            setApplicationReturnValue(runNetworkTest());
            quit();
            return;
        }
#if JUCE_IOS
        constexpr bool phone = true;
#else
        constexpr bool phone = false;
#endif
        try {
            auto settings = Settings::load();
#if JUCE_WINDOWS
            recoverDefaultAudio();
#endif
            runtime_ = std::make_unique<Runtime>(phone ? Role::phone : Role::pc, settings);
            audio_ = makeAudioPlatform();
            audioOptions_ = settings.audio;
            if (!phone || !records(settings.mode))
                startAudio();
            runtime_->start();
            window_ = std::make_unique<Window>(new MainView(*runtime_, audio_.get(), phone));
#if JUCE_WINDOWS
            tray_ = std::make_unique<Tray>(*this);
            setAutostart(settings.autostart);
            window_->setVisible(first != "--tray");
            if (settings.notifications)
                tray_->showInfoBubble("AudioWire is ready", "Open AudioWire on your iPhone to connect.");
#else
            window_->setVisible(true);
#endif
            startTimerHz(5);
            if (first.startsWithIgnoreCase("audiowire://"))
                anotherInstanceStarted(command);
        } catch (const std::exception& e) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "AudioWire", e.what());
        }
    }
    void startAudio() {
        if (juce::Time::getMillisecondCounterHiRes() < audioRetryAfter_)
            return;
        try {
            audio_->start(audioOptions_, [this](auto input, auto output, int frames) {
                runtime_->process(input, output, frames);
            });
            runtime_->attachAudioInfo(audio_->sampleRate(), audio_->maxFrames(),
                                      audio_->microphoneAvailable(), audio_->status(), audio_->route());
            audioStarted_ = true;
            runtime_->reportError("");
        } catch (const std::exception& e) {
            audio_->stop();
            audioStarted_ = false;
            audioRetryAfter_ = juce::Time::getMillisecondCounterHiRes() + 3000;
            runtime_->reportError(e.what());
        }
    }
    void shutdown() override {
        stopTimer();
        if (audio_)
            audio_->stop();
        window_.reset();
#if JUCE_WINDOWS
        tray_.reset();
#endif
        runtime_.reset();
        audio_.reset();
    }
    void systemRequestedQuit() override {
        quit();
    }
    void anotherInstanceStarted(const juce::String& command) override {
        show();
        auto args = juce::StringArray::fromTokens(command, true);
        if (args.size() && args[0].unquoted().startsWithIgnoreCase("audiowire://") && window_) {
#if JUCE_IOS
            static_cast<PhoneScreen*>(window_->getContentComponent())
                ->view()
                ->handlePairLink(args[0].unquoted());
#else
            static_cast<MainView*>(window_->getContentComponent())->handlePairLink(args[0].unquoted());
#endif
        }
    }
    void show() {
        if (window_) {
            window_->setVisible(true);
            window_->toFront(true);
        }
    }

  private:
    void timerCallback() override {
        if (!runtime_ || !audio_)
            return;
        auto s = runtime_->snapshot();
        auto own = runtime_->settings();
#if JUCE_IOS
        if (permissionPending_)
            return;
        if (records(own.mode) &&
            !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
            if (s.state != State::connected)
                return;
            permissionPending_ = true;
            auto safe = juce::Component::SafePointer<juce::Component>(window_.get());
            juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [this,
                                                                                      safe](bool granted) {
                if (!safe)
                    return;
                permissionPending_ = false;
                if (!granted) {
                    auto settings = runtime_->settings();
                    settings.mode = settings.audio.mode = Mode::speaker;
                    runtime_->updateSettings(settings);
                    juce::AlertWindow::showOkCancelBox(juce::AlertWindow::InfoIcon, "Microphone access",
                                                       "Speaker mode still works. Enable AudioWire "
                                                       "microphone access in iPhone Settings to use calls.",
                                                       "Open Settings", "Keep speaker mode", nullptr,
                                                       juce::ModalCallbackFunction::create([](int result) {
                                                           if (result)
                                                               openPermissionSettings();
                                                       }));
                }
            });
            return;
        }
#endif
        if (!audioStarted_ && s.state == State::connected) {
            audioOptions_ = own.audio;
            startAudio();
        }
        const auto& desired = own.audio;
#if JUCE_IOS
        const bool needsRestart = desired.mode != audioOptions_.mode || desired.mix != audioOptions_.mix ||
                                  desired.aec != audioOptions_.aec ||
                                  desired.force44100 != audioOptions_.force44100 ||
                                  desired.background != audioOptions_.background;
#else
        const bool needsRestart =
            desired.mirror != audioOptions_.mirror || desired.captureDevice != audioOptions_.captureDevice;
#endif
        if (audioStarted_ && needsRestart) {
            audioOptions_ = desired;
            audio_->stop();
            startAudio();
            if (own.allSound)
                audio_->useAsDefault(true);
        }
        runtime_->attachAudioInfo(audio_->sampleRate(), audio_->maxFrames(), audio_->microphoneAvailable(),
                                  audio_->status(), audio_->route());
        if (audio_->lowPower() && !lowPower_) {
            qualityBeforeLowPower_ = own.quality;
            lowPower_ = true;
            runtime_->setControl({ControlKind::quality, 3, {}});
        } else if (!audio_->lowPower() && lowPower_) {
            lowPower_ = false;
            if (own.quality == Quality::voice)
                runtime_->setControl({ControlKind::quality,
                                      static_cast<double>(static_cast<int>(qualityBeforeLowPower_)),
                                      {}});
        }
        if (lastRoute_ != audio_->route() || lastInterrupted_ != audio_->interrupted()) {
            lastRoute_ = audio_->route();
            lastInterrupted_ = audio_->interrupted();
            runtime_->gateMicrophone();
        }
        if (audio_->battery() >= 0 && std::abs(audio_->battery() - lastBattery_) >= 1) {
            lastBattery_ = audio_->battery();
            runtime_->setControl({ControlKind::battery, lastBattery_, {}});
        }
#if JUCE_WINDOWS
        if (tray_) {
            tray_->update(s.state == State::connected);
            const char* mode = own.mode == Mode::speakerphone ? "Speakerphone"
                               : own.mode == Mode::speaker    ? "Speaker only"
                                                              : "Mic only";
            juce::String tooltip = "AudioWire - " + juce::String(stateName(s.state)) +
                                   (s.peer.empty() ? juce::String() : " | " + juce::String(s.peer));
            if (s.state == State::connected) {
                tooltip += " | " + juce::String(mode);
                if (s.battery >= 0)
                    tooltip += " | " + juce::String(static_cast<int>(s.battery)) + "%";
                if (s.latencyMs > 0)
                    tooltip += " | " + juce::String(static_cast<int>(s.latencyMs)) + " ms";
            }
            tray_->setIconTooltip(tooltip);
            if (s.state == State::connected && !connectedBefore_ && own.notifications)
                tray_->showInfoBubble("iPhone connected",
                                      "Use the AudioWire window to choose sound, microphone and volume.");
        }
#endif
        connectedBefore_ = s.state == State::connected;
    }
    std::unique_ptr<Runtime> runtime_;
    std::unique_ptr<AudioPlatform> audio_;
    std::unique_ptr<Window> window_;
#if JUCE_WINDOWS
    std::unique_ptr<Tray> tray_;
#endif
    bool connectedBefore_ = false;
    double audioRetryAfter_ = 0;
    AudioOptions audioOptions_;
    bool permissionPending_ = false;
    bool audioStarted_ = false, lowPower_ = false, lastInterrupted_ = false;
    Quality qualityBeforeLowPower_ = Quality::balanced;
    std::string lastRoute_;
    double lastBattery_ = -2;
};
} // namespace aw
START_JUCE_APPLICATION(aw::App)
