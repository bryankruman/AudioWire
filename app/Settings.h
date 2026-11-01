// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/Types.h"
#include "platform/AudioPlatform.h"
#include <juce_core/juce_core.h>
namespace aw {
struct Settings {
    std::string id, name;
    Mode mode = Mode::speakerphone;
    Quality quality = Quality::balanced;
    AudioOptions audio;
    bool autostart = true, notifications = true, autoConnect = true, autoRevert = true, allSound = false;
    int bufferMs = 0;
    std::vector<Device> paired;
    static Settings load();
    void save() const;
};
juce::File settingsDirectory();
} // namespace aw
