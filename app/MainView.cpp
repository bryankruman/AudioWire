// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainView.h"
#include <cmath>
namespace aw {
namespace {
juce::Colour bg{0xff10151c}, panel{0xff19212d}, line{0xff2b3544}, accent{0xff73dfbf}, text{0xffedf3f8},
    muted{0xff98a8ba};
void label(juce::Graphics& g, juce::String s, juce::Rectangle<int> r, float size, juce::Colour colour,
           bool bold = false) {
    g.setColour(colour);
    g.setFont(juce::Font(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawFittedText(s, r, juce::Justification::centredLeft, 2);
}
} // namespace
AwLookAndFeel::AwLookAndFeel() {
    setTheme(juce::Desktop::getInstance().isDarkModeActive());
}
void AwLookAndFeel::setTheme(bool dark) {
    bg = juce::Colour(dark ? 0xff10151c : 0xfff5f7fa);
    panel = juce::Colour(dark ? 0xff19212d : 0xffffffff);
    line = juce::Colour(dark ? 0xff2b3544 : 0xffd7dee8);
    accent = juce::Colour(dark ? 0xff73dfbf : 0xff167d63);
    text = juce::Colour(dark ? 0xffedf3f8 : 0xff172332);
    muted = juce::Colour(dark ? 0xff98a8ba : 0xff5e7187);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::outlineColourId, line);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::TextEditor::backgroundColourId, panel);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, line);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, accent);
    setColour(juce::ToggleButton::tickDisabledColourId, muted);
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, panel);
    setColour(juce::Slider::textBoxOutlineColourId, line);
}
void AwLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& colour,
                                         bool over, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(1);
    g.setColour(colour.brighter(down ? 0.12f : over ? 0.07f : 0.0f));
    g.fillRoundedRectangle(r, 12);
    g.setColour(line);
    g.drawRoundedRectangle(r, 12, 1);
}
MainView::MainView(Runtime& runtime, AudioPlatform* audio, bool phone)
    : runtime_(runtime), audio_(audio), phone_(phone) {
    setLookAndFeel(&look_);
    setSize(540, 900);
    auto add = [&](juce::Component& c, const char* description) {
        addAndMakeVisible(c);
        c.setTitle(description);
        c.setDescription(description);
    };
    add(home_, "Connection");
    add(settingsButton_, "Settings");
    add(diagnosticsButton_, "Diagnostics");
    add(action_, "Connect or disconnect");
    add(connect_, "Connect to selected PC");
    add(devices_, "Available PCs");
    add(mode_, "Speakerphone, speaker only or microphone only");
    add(mute_, "Mute microphone");
    add(volume_, "iPhone speaker volume");
    add(address_, "PC address or AudioWire pairing link");
    add(code_, "Six digit pairing code");
    add(allSound_, "Use iPhone for all PC sound");
    add(sound_, "Open Windows Sound settings");
    add(error_, "Connection help");
    add(subtitle_, "Connection status");
    add(pairHint_, "Pairing help");
    add(copy_, "Copy diagnostics");
    add(source_, "Open source and licence");
    add(viewport_, "Settings");
    viewport_.setViewedComponent(&settingsContent_, false);
    viewport_.setScrollBarsShown(true, false);
    for (auto* c : std::initializer_list<juce::Component*>{&name_, &quality_, &reserve_, &paired_, &forget_,
                                                           &capture_, &mix_, &background_, &aec_, &autostart_,
                                                           &mirror_, &autoRevert_, &force44100_,
                                                           &autoConnect_, &notifications_, &permission_}) {
        settingsContent_.addAndMakeVisible(c);
        c->setTitle(c == &name_                      ? "Display name"
                    : c == &quality_                 ? "Audio quality"
                    : c == &reserve_                 ? "Latency versus stability"
                    : c == &paired_                  ? "Saved devices"
                    : c == &capture_                 ? "PC speakers to capture"
                    : c == &forget_                  ? "Forget selected device"
                    : dynamic_cast<juce::Button*>(c) ? dynamic_cast<juce::Button*>(c)->getButtonText()
                                                     : c->getName());
    }
    mode_.addItem("Speakerphone", 1);
    mode_.addItem("Speaker only", 2);
    mode_.addItem("Mic only", 3);
    quality_.addItem("Balanced - everyday calls and listening", 1);
    quality_.addItem("High - clearer sound on strong Wi-Fi", 2);
    quality_.addItem("Max - uncompressed, needs strong Wi-Fi", 3);
    quality_.addItem("Voice - low power mode", 4);
    capture_.setTextWhenNothingSelected("Automatic - AudioWire or PC speakers");
    devices_.setTextWhenNothingSelected("Select your PC");
    paired_.setTextWhenNothingSelected("No saved devices");
    name_.setTextToShowWhenEmpty("Device display name", muted);
    address_.setTextToShowWhenEmpty("PC address, host:port or pairing link", muted);
    code_.setTextToShowWhenEmpty("6-digit code from your PC", muted);
    code_.setInputRestrictions(6, "0123456789");
    code_.setKeyboardType(juce::TextInputTarget::numericKeyboard);
    code_.setPasswordCharacter(0);
    volume_.setRange(0, 1, 0.01);
    volume_.setValue(0.8);
    volume_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    reserve_.setRange(0, 100, 1);
    reserve_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 30);
    reserve_.textFromValueFunction = [](double v) {
        return v == 0 ? juce::String("Auto") : juce::String(static_cast<int>(v)) + " ms";
    };
    auto s = runtime_.settings();
    loading_ = true;
    name_.setText(s.name, false);
    mode_.setSelectedId(static_cast<int>(s.mode) + 1);
    quality_.setSelectedId(static_cast<int>(s.quality) + 1);
    reserve_.setValue(s.bufferMs);
    reserve_.updateText();
    notifications_.setToggleState(s.notifications, juce::dontSendNotification);
    mix_.setToggleState(s.audio.mix, juce::dontSendNotification);
    aec_.setToggleState(s.audio.aec, juce::dontSendNotification);
    background_.setToggleState(s.audio.background, juce::dontSendNotification);
    autostart_.setToggleState(s.autostart, juce::dontSendNotification);
    mirror_.setToggleState(s.audio.mirror, juce::dontSendNotification);
    autoRevert_.setToggleState(s.autoRevert, juce::dontSendNotification);
    force44100_.setToggleState(s.audio.force44100, juce::dontSendNotification);
    autoConnect_.setToggleState(s.autoConnect, juce::dontSendNotification);
    loading_ = false;
    home_.onClick = [this] { page(0); };
    settingsButton_.onClick = [this] {
        refreshPaired();
        page(1);
    };
    diagnosticsButton_.onClick = [this] { page(2); };
    action_.onClick = [this] {
        auto s = runtime_.snapshot();
        if (phone_ && s.state == State::connected) {
            const auto safe = juce::Component::SafePointer<MainView>(this);
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::QuestionIcon, "Disconnect AudioWire?",
                "PC sound and your microphone will stop streaming.", "Disconnect", "Keep connected", nullptr,
                juce::ModalCallbackFunction::create(
                    [safe](int result) {
                        if (result && safe)
                            safe->runtime_.disconnect();
                    }));
        } else if (s.state == State::connected || s.state == State::reconnecting)
            runtime_.disconnect();
        else if (phone_)
            connect();
        else {
            runtime_.pairNew();
            showPairing();
        }
    };
    connect_.onClick = [this] { connect(); };
    code_.onReturnKey = [this] { connect(); };
    address_.onReturnKey = [this] { connect(); };
    mute_.onClick = [this] {
        runtime_.setControl({ControlKind::mute, mute_.getToggleState() ? 1.0 : 0.0, {}});
    };
    volume_.onValueChange = [this] { runtime_.setControl({ControlKind::volume, volume_.getValue(), {}}); };
    mode_.onChange = [this] {
        if (!loading_)
            saveSettings();
    };
    quality_.onChange = [this] {
        if (!loading_)
            saveSettings();
    };
    for (auto* b : {&mix_, &background_, &aec_, &autostart_, &mirror_, &autoRevert_, &force44100_,
                    &autoConnect_, &notifications_})
        b->onClick = [this] { saveSettings(); };
    reserve_.onDragEnd = [this] { saveSettings(); };
    name_.onReturnKey = [this] { saveSettings(); };
    name_.onFocusLost = [this] {
        if (name_.getText().toStdString() != runtime_.settings().name)
            saveSettings();
    };
    capture_.onChange = [this] {
        if (!loading_)
            saveSettings();
    };
    allSound_.onClick = [this] {
        if (audio_) {
            const bool enabled = allSound_.getToggleState();
            const bool ok = audio_->useAsDefault(enabled);
            allSoundApplied_ = enabled && ok;
            allSound_.setToggleState(allSoundApplied_, juce::dontSendNotification);
            auto s = runtime_.settings();
            s.allSound = allSoundApplied_;
            runtime_.updateSettings(s);
        }
    };
    sound_.onClick = [] { openSoundSettings(); };
    copy_.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard(runtime_.diagnostics());
        copy_.setButtonText("Copied");
    };
    permission_.onClick = [] { openPermissionSettings(); };
    source_.onClick = [] { juce::URL("https://github.com/bryankruman/AudioWire").launchInDefaultBrowser(); };
    forget_.onClick = [this] {
        auto s = runtime_.settings();
        int i = paired_.getSelectedId() - 1;
        if (i >= 0 && i < static_cast<int>(s.paired.size())) {
            runtime_.forget(s.paired[static_cast<size_t>(i)].id);
            const auto safe = juce::Component::SafePointer<MainView>(this);
            juce::Timer::callAfterDelay(150, [safe] {
                if (safe)
                    safe->refreshPaired();
            });
        }
    };
    if (audio_ && !phone_) {
        capture_.addItem("Automatic - AudioWire or PC speakers", 1);
        int i = 2;
        for (auto d : audio_->captureDevices())
            capture_.addItem(d.name, i++);
        capture_.setSelectedId(1);
    }
    refreshPaired();
    page(0);
    timerCallback();
    startTimerHz(15);
}
MainView::~MainView() {
    stopTimer();
    setLookAndFeel(nullptr);
}
void MainView::refreshPaired() {
    auto s = runtime_.settings();
    paired_.clear();
    int i = 1;
    for (auto& d : s.paired)
        paired_.addItem(d.name, i++);
    if (i > 1)
        paired_.setSelectedId(1);
}
void MainView::page(int p) {
    page_ = p;
    for (auto* c :
         std::initializer_list<juce::Component*>{&action_, &connect_, &devices_, &mode_, &mute_, &volume_,
                                                 &address_, &code_, &allSound_, &sound_, &pairHint_})
        c->setVisible(p == 0);
    devices_.setVisible(p == 0 && phone_);
    address_.setVisible(p == 0 && phone_);
    code_.setVisible(p == 0 && phone_);
    connect_.setVisible(p == 0 && phone_);
    allSound_.setVisible(p == 0 && !phone_);
    sound_.setVisible(p == 0 && !phone_);
    viewport_.setVisible(p == 1);
    copy_.setVisible(p == 2);
    source_.setVisible(p == 1 || p == 2);
    resized();
    repaint();
}
void MainView::resized() {
    const int margin = 24, w = getWidth() - margin * 2;
    home_.setBounds(margin, 88, w / 3 - 6, 44);
    settingsButton_.setBounds(margin + w / 3, 88, w / 3 - 6, 44);
    diagnosticsButton_.setBounds(margin + w * 2 / 3, 88, w / 3, 44);
    subtitle_.setBounds(margin, 145, w, 34);
    const int bottom = getHeight() - 68;
    error_.setBounds(margin, bottom - 58, w, 54);
    if (page_ == 0) {
        action_.setBounds((getWidth() - 180) / 2, 201, 180, 120);
        if (phone_) {
            devices_.setBounds(margin, 340, w, 44);
            address_.setBounds(margin, 395, w, 44);
            code_.setBounds(margin, 450, w - 116, 44);
            connect_.setBounds(getWidth() - margin - 106, 450, 106, 44);
        }
        pairHint_.setBounds(margin, phone_ ? 500 : 458, w, 42);
        mode_.setBounds(margin, 548, w, 44);
        mute_.setBounds(margin, 604, w / 2, 44);
        volume_.setBounds(margin + w / 2, 604, w / 2, 44);
        allSound_.setBounds(margin, 657, w, 44);
        sound_.setBounds(margin, bottom, w, 44);
    } else if (page_ == 1) {
        viewport_.setBounds(margin, 196, w, getHeight() - 286);
        settingsContent_.setSize(w - 14, phone_ ? 650 : 710);
        const int sw = w - 14;
        int y = 30;
        name_.setBounds(0, y, sw, 44);
        y += 86;
        quality_.setBounds(0, y, sw, 44);
        y += 90;
        reserve_.setBounds(0, y, sw, 44);
        y += 60;
        for (auto* b : {&autoConnect_, &aec_, &mix_, &background_, &force44100_, &autostart_, &notifications_,
                        &mirror_, &autoRevert_}) {
            bool visible =
                phone_ ? (b != &autostart_ && b != &mirror_ && b != &autoRevert_ && b != &notifications_)
                       : (b != &aec_ && b != &mix_ && b != &background_ && b != &force44100_);
            b->setVisible(visible);
            if (visible) {
                b->setBounds(0, y, sw, 44);
                y += 46;
            }
        }
        permission_.setVisible(phone_);
        if (phone_) {
            permission_.setBounds(0, y, sw, 44);
            y += 54;
        }
        capture_.setVisible(!phone_);
        if (!phone_) {
            capture_.setBounds(0, y, sw, 44);
            y += 60;
        }
        paired_.setBounds(0, y, sw, 44);
        y += 54;
        forget_.setBounds(0, y, sw, 44);
        settingsContent_.setSize(sw, y + 62);
        source_.setBounds(margin, bottom, w, 44);
    } else {
        copy_.setBounds(margin, bottom - 56, w, 44);
        source_.setBounds(margin, bottom, w, 44);
    }
}
void MainView::lookAndFeelChanged() {
    for (auto* editor : {&name_, &address_, &code_})
        editor->applyColourToAllText(text, true);
    name_.setTextToShowWhenEmpty("Device display name", muted);
    address_.setTextToShowWhenEmpty("PC address, host:port or pairing link", muted);
    code_.setTextToShowWhenEmpty("6-digit code from your PC", muted);
    const bool connected = runtime_.snapshot().state == State::connected;
    action_.setColour(juce::TextButton::textColourOffId, connected ? juce::Colours::white : text);
    action_.setColour(juce::TextButton::buttonColourId, connected ? juce::Colour(0xff235a4a) : panel);
}
void MainView::showPairing() {
    page(0);
}
void MainView::connect() {
    Device selected;
    int i = devices_.getSelectedId() - 1;
    if (i >= 0 && i < static_cast<int>(devicesCache_.size()))
        selected = devicesCache_[static_cast<size_t>(i)];
    auto manual = address_.getText().trim();
    if (manual.startsWithIgnoreCase("audiowire://")) {
        handlePairLink(manual);
        return;
    }
    if (manual.isNotEmpty()) {
        int colon = manual.lastIndexOfChar(':');
        if (colon < 1) {
            error_.setText("Enter the address and port shown on your PC, for example 192.168.1.20:51234.",
                           juce::dontSendNotification);
            return;
        }
        const int p = manual.substring(colon + 1).getIntValue();
        if (p < 1 || p > 65535) {
            error_.setText("Enter a port between 1 and 65535.", juce::dontSendNotification);
            return;
        }
        selected = {"", "PC", manual.substring(0, colon).toStdString(), static_cast<uint16_t>(p)};
    }
    if (selected.host.empty()) {
        error_.setText("Select a PC or enter its address from the Windows app.", juce::dontSendNotification);
        return;
    }
    runtime_.connect(selected, code_.getText().toStdString());
    code_.clear();
}
void MainView::handlePairLink(const juce::String& link) {
    if (!phone_) {
        runtime_.pairNew();
        showPairing();
        return;
    }
    juce::URL u(link);
    if (u.getScheme() != "audiowire")
        return;
    auto names = u.getParameterNames(), values = u.getParameterValues();
    auto value = [&](const char* key) {
        int i = names.indexOf(key);
        return i >= 0 ? values[i] : juce::String();
    };
    auto ip = value("ip"), code = value("code");
    int p = value("port").getIntValue();
    if (ip.isEmpty() || p < 1 || p > 65535 || code.length() != 6) {
        error_.setText("This pairing link is incomplete. Generate a new one on your PC.",
                       juce::dontSendNotification);
        return;
    }
    address_.setText(ip + ":" + juce::String(p));
    code_.setText(code);
    page(0);
    connect();
}
void MainView::saveSettings() {
    if (loading_)
        return;
    auto s = runtime_.settings();
    s.name = displayName(name_.getText().trim().toStdString());
    if (s.name.empty())
        return;
    s.mode = static_cast<Mode>(std::max(0, mode_.getSelectedId() - 1));
    s.audio.mode = s.mode;
    s.quality = static_cast<Quality>(std::max(0, quality_.getSelectedId() - 1));
    s.bufferMs = static_cast<int>(reserve_.getValue());
    s.audio.mix = mix_.getToggleState();
    s.audio.aec = aec_.getToggleState();
    s.audio.background = background_.getToggleState();
    s.audio.force44100 = force44100_.getToggleState();
    s.audio.mirror = mirror_.getToggleState();
    s.autostart = autostart_.getToggleState();
    s.notifications = notifications_.getToggleState();
    s.autoRevert = autoRevert_.getToggleState();
    s.autoConnect = autoConnect_.getToggleState();
    s.audio.captureDevice = capture_.getSelectedId() > 1 ? capture_.getText().toStdString() : "";
    try {
        runtime_.updateSettings(s);
        runtime_.setControl({ControlKind::rename, 0, s.name});
        if (!phone_)
            setAutostart(s.autostart);
    } catch (const std::exception& e) {
        error_.setText(e.what(), juce::dontSendNotification);
    }
}
void MainView::timerCallback() {
    auto s = runtime_.snapshot();
    subtitle_.setText(juce::String(stateName(s.state)) +
                          (s.peer.empty() ? juce::String() : " | " + juce::String(s.peer)),
                      juce::dontSendNotification);
    if (!s.error.empty() || s.state == State::connected)
        error_.setText(s.error, juce::dontSendNotification);
    mute_.setToggleState(s.muted, juce::dontSendNotification);
    volume_.setValue(s.volume, juce::dontSendNotification);
    allSound_.setEnabled(s.state == State::connected);
    bool dark = juce::Desktop::getInstance().isDarkModeActive();
    if (dark != dark_) {
        dark_ = dark;
        look_.setTheme(dark);
        sendLookAndFeelChange();
    }
    action_.setButtonText(s.state == State::connected      ? "Disconnect"
                          : s.state == State::reconnecting ? "Cancel reconnect"
                          : phone_                         ? "Connect"
                                                           : "Pair a new phone");
    action_.setTitle(juce::String(stateName(s.state)));
    action_.setColour(juce::TextButton::textColourOffId,
                      s.state == State::connected ? juce::Colours::white : text);
    action_.setColour(juce::TextButton::buttonColourId,
                      s.state == State::connected ? juce::Colour(0xff235a4a) : panel);
    if (phone_) {
        int selected = devices_.getSelectedId();
        devicesCache_ = s.devices;
        devices_.clear();
        int i = 1;
        for (auto& d : s.devices)
            devices_.addItem(d.name, i++);
        if (selected > 0 && selected < i)
            devices_.setSelectedId(selected, juce::dontSendNotification);
        else if (i == 2)
            devices_.setSelectedId(1);
    }
    if (phone_) {
        pairHint_.setText(s.devices.empty()
                              ? "Both devices need the same Wi-Fi. Enter the address shown on your PC if "
                                "discovery is blocked."
                              : "Enter the code from your PC. A saved PC reconnects without a code.",
                          juce::dontSendNotification);
    } else {
        pairHint_.setText(s.state == State::connected
                              ? s.driver
                              : "Open AudioWire on your iPhone, select this PC, then enter the code above.",
                          juce::dontSendNotification);
        auto url = runtime_.pairingUrl();
        if (url != lastQr_) {
            lastQr_ = url;
            qr_.reset();
            if (!url.empty())
                try {
                    qr_ = std::make_unique<qrcodegen::QrCode>(
                        qrcodegen::QrCode::encodeText(url.c_str(), qrcodegen::QrCode::Ecc::MEDIUM));
                } catch (...) {
                }
        }
        if (connectedBefore_ && s.state != State::connected && runtime_.settings().autoRevert && audio_) {
            audio_->useAsDefault(false);
            allSoundApplied_ = false;
            allSound_.setToggleState(false, juce::dontSendNotification);
            auto settings = runtime_.settings();
            settings.allSound = false;
            runtime_.updateSettings(settings);
        }
    }
    connectedBefore_ = s.state == State::connected;
    auto own = runtime_.settings();
    loading_ = true;
    mode_.setSelectedId(static_cast<int>(own.mode) + 1, juce::dontSendNotification);
    quality_.setSelectedId(static_cast<int>(own.quality) + 1, juce::dontSendNotification);
    loading_ = false;
    repaint();
}
void MainView::paintMeter(juce::Graphics& g, juce::Rectangle<float> r, float level,
                          const juce::String& title) {
    label(g, title, r.toNearestInt().withHeight(20), 12, muted);
    r.removeFromTop(25);
    g.setColour(line);
    g.fillRoundedRectangle(r, 3);
    g.setColour(accent);
    g.fillRoundedRectangle(r.withWidth(r.getWidth() * juce::jlimit(0.0f, 1.0f, std::sqrt(level))), 3);
}
void MainView::paint(juce::Graphics& g) {
    g.fillAll(bg);
    const int w = getWidth() - 48;
    auto s = runtime_.snapshot();
    label(g, "AudioWire", {24, 20, w, 36}, 28, text, true);
    label(g, phone_ ? "Your PC, on your iPhone." : "Your iPhone. Your sound. One connection.",
          {24, 56, w, 24}, 14, muted);
    g.setColour(s.state == State::connected      ? accent
                : s.state == State::reconnecting ? juce::Colours::orange
                                                 : muted);
    g.fillEllipse(static_cast<float>(getWidth() - 40), 32, 10, 10);
    if (page_ == 0) {
        if (!phone_ && s.state != State::connected) {
            auto code = runtime_.pairingCode();
            juce::String spaced;
            for (char c : code) {
                spaced += juce::String::charToString(c);
                spaced += " ";
            }
            label(g, code.empty() ? "Click Pair a new phone for a code" : spaced, {24, 334, w - 160, 54},
                  code.empty() ? 16 : 30, accent, true);
            label(g, code.empty() ? "" : juce::String(s.pairSeconds) + " seconds remaining",
                  {24, 389, w - 160, 26}, 13, muted);
            const auto ip = lanAddress();
            label(g,
                  runtime_.tcpPort() && !ip.empty()
                      ? "PC address: " + juce::String(ip) + ":" + juce::String(runtime_.tcpPort())
                      : "PC address available after launch",
                  {24, 417, w - 150, 28}, 13, muted);
            if (qr_) {
                int size = qr_->getSize(), scale = std::max(1, 128 / (size + 8));
                int x = getWidth() - 24 - (size + 8) * scale, y = 330;
                g.setColour(juce::Colours::white);
                g.fillRect(x, y, (size + 8) * scale, (size + 8) * scale);
                g.setColour(juce::Colours::black);
                for (int row = 0; row < size; ++row)
                    for (int col = 0; col < size; ++col)
                        if (qr_->getModule(col, row))
                            g.fillRect(x + (col + 4) * scale, y + (row + 4) * scale, scale, scale);
            }
        }
        if (!phone_ && s.state == State::connected) {
            label(g, juce::String(s.peer), {24, 354, w, 42}, 26, text, true);
            label(g,
                  s.battery >= 0 ? juce::String(static_cast<int>(s.battery)) + "% battery | Speakerphone"
                                 : "Your iPhone is connected",
                  {24, 402, w, 30}, 16, muted);
        }
        if (phone_)
            label(g,
                  juce::String(s.route.empty() ? "Playing through: iPhone Speaker"
                                               : "Playing through: " + s.route),
                  {24, 660, w, 30}, 13, muted);
        paintMeter(g, {24.0f, 707.0f, static_cast<float>(w / 2 - 12), 32}, s.input, "Sending sound");
        paintMeter(g, {static_cast<float>(getWidth() / 2 + 12), 707.0f, static_cast<float>(w / 2 - 12), 32},
                   s.output, "Receiving sound");
    } else if (page_ == 1) {
        // Section labels move with the settings viewport.
        const int y = viewport_.getY() - viewport_.getViewPositionY();
        if (y + 30 >= viewport_.getY())
            label(g, "DISPLAY NAME", {24, y, w, 24}, 12, muted, true);
        if (y + 110 >= viewport_.getY() && y + 110 < viewport_.getBottom())
            label(g, "AUDIO QUALITY", {24, y + 86, w, 24}, 12, muted, true);
        if (y + 200 >= viewport_.getY() && y + 200 < viewport_.getBottom())
            label(g, "LATENCY VS STABILITY | 0 = AUTO", {24, y + 176, w, 24}, 12, muted, true);
    } else {
        int y = 210;
        auto row = [&](juce::String title, juce::String value) {
            g.setColour(panel);
            g.fillRoundedRectangle(24.0f, static_cast<float>(y), static_cast<float>(w), 66, 12);
            label(g, title, {40, y + 8, w - 32, 22}, 13, muted);
            label(g, value, {40, y + 30, w - 32, 27}, 19, text, true);
            y += 78;
        };
        row("CONTROL ROUND-TRIP", s.latencyMs > 0 ? juce::String(static_cast<int>(s.latencyMs)) + " ms"
                                                  : "Connect a device to measure");
        row("AUDIO RESERVE",
            juce::String(s.bufferMs) +
                (runtime_.settings().bufferMs ? " ms | manual setting" : " ms | automatic adaptation"));
        row("AUDIO DELIVERY", juce::String(static_cast<juce::int64>(s.packets)) + " packets | " +
                                  juce::String(static_cast<juce::int64>(s.lost)) + " lost blocks");
        row("PC MICROPHONE", s.micAvailable ? "Ready"
                             : phone_       ? "Reported by your PC after connecting"
                                            : "Driver required - speaker mode available");
        label(g,
              "Round-trip measures the connection. Acoustic latency and echo cancellation require testing "
              "with your iPhone.",
              {24, y + 8, w, 54}, 13, muted);
        label(g,
              "AudioWire " AUDIOWIRE_VERSION
              " | GPLv3 + App Store permission\nBased on SonoBus by Jesse Chappell (Sonosaurus LLC)",
              {24, y + 76, w, 48}, 12, muted);
    }
}
} // namespace aw

namespace aw {
int renderUi(const juce::File& directory) {
    if (directory.createDirectory().failed())
        return 1;
    Settings settings;
    settings.id = randomId();
    settings.name = "AudioWire preview";
    settings.autoConnect = false;
    Runtime pc(Role::pc, settings, false);
    MainView windows(pc, nullptr, false);
    windows.setSize(540, 900);
    Runtime phone(Role::phone, settings, false);
    MainView mobile(phone, nullptr, true);
    mobile.setSize(375, 900);
    auto save = [&](MainView& view, const juce::String& filename) {
        auto image = view.createComponentSnapshot(view.getLocalBounds());
        juce::FileOutputStream out(directory.getChildFile(filename));
        out.setPosition(0);
        out.truncate();
        return out.openedOk() && juce::PNGImageFormat().writeImageToStream(image, out);
    };
    for (bool dark : {false, true}) {
        windows.previewTheme(dark);
        mobile.previewTheme(dark);
        for (int page = 0; page < 3; ++page) {
            windows.showPage(page);
            if (!save(windows,
                      juce::String(dark ? "windows-dark-" : "windows-") + juce::String(page) + ".png"))
                return 1;
        }
        if (!save(mobile, dark ? "phone-dark-home.png" : "phone-home.png"))
            return 1;
    }
    return 0;
}

} // namespace aw
