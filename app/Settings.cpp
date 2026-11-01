// SPDX-License-Identifier: GPL-3.0-or-later
#include "Settings.h"
#include "core/Pairing.h"
namespace aw {
juce::File settingsDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("AudioWire");
}
Settings Settings::load() {
    Settings s;
    auto root = juce::JSON::parse(settingsDirectory().getChildFile("settings.json"));
    s.id = root.getProperty("id", juce::String(randomId())).toString().toStdString();
    if (s.id.size() != 16 || s.id.find_first_not_of("0123456789abcdef") != std::string::npos)
        s.id = randomId();
    s.name = root.getProperty("name", juce::SystemStats::getComputerName())
                 .toString()
                 .substring(0, 64)
                 .toStdString();
    s.name = displayName(s.name);
    s.mode = static_cast<Mode>(juce::jlimit(0, 2, static_cast<int>(root.getProperty("mode", 0))));
    s.quality = static_cast<Quality>(juce::jlimit(0, 3, static_cast<int>(root.getProperty("quality", 0))));
    s.audio.mode = s.mode;
    s.audio.mix = root.getProperty("mix", false);
    s.audio.aec = root.getProperty("aec", true);
    s.audio.background = root.getProperty("background", true);
    s.audio.force44100 = root.getProperty("force44100", false);
    s.audio.mirror = root.getProperty("mirror", false);
    s.audio.captureDevice = root.getProperty("captureDevice", "").toString().toStdString();
    s.autostart = root.getProperty("autostart", true);
    s.notifications = root.getProperty("notifications", true);
    s.autoConnect = root.getProperty("autoConnect", true);
    s.autoRevert = root.getProperty("autoRevert", true);
    s.bufferMs = juce::jlimit(0, 100, static_cast<int>(root.getProperty("bufferMs", 0)));
    if (auto* list = root.getProperty("paired", juce::var()).getArray())
        for (auto& item : *list) {
            Device d{item["id"].toString().toStdString(), item["name"].toString().toStdString(),
                     item["host"].toString().toStdString(),
                     static_cast<uint16_t>(juce::jlimit(1, 65535, static_cast<int>(item["port"])))};
            if (d.id.size() == 16 && d.id.find_first_not_of("0123456789abcdef") == std::string::npos)
                s.paired.push_back(d);
        }
    return s;
}
void Settings::save() const {
    juce::DynamicObject::Ptr o = new juce::DynamicObject;
    o->setProperty("id", juce::String(id));
    o->setProperty("name", juce::String(name));
    o->setProperty("mode", static_cast<int>(mode));
    o->setProperty("quality", static_cast<int>(quality));
    o->setProperty("mix", audio.mix);
    o->setProperty("aec", audio.aec);
    o->setProperty("background", audio.background);
    o->setProperty("force44100", audio.force44100);
    o->setProperty("mirror", audio.mirror);
    o->setProperty("captureDevice", juce::String(audio.captureDevice));
    o->setProperty("autostart", autostart);
    o->setProperty("notifications", notifications);
    o->setProperty("autoConnect", autoConnect);
    o->setProperty("autoRevert", autoRevert);
    o->setProperty("bufferMs", bufferMs);
    juce::Array<juce::var> list;
    for (auto& d : paired) {
        juce::DynamicObject::Ptr p = new juce::DynamicObject;
        p->setProperty("id", juce::String(d.id));
        p->setProperty("name", juce::String(d.name));
        p->setProperty("host", juce::String(d.host));
        p->setProperty("port", d.port);
        list.add(juce::var(p.get()));
    }
    o->setProperty("paired", list);
    auto dir = settingsDirectory();
    if (dir.createDirectory().failed())
        throw std::runtime_error("Cannot save settings");
    juce::TemporaryFile temp(dir.getChildFile("settings.json"));
    if (!temp.getFile().replaceWithText(juce::JSON::toString(juce::var(o.get()))) ||
        !temp.overwriteTargetFileWithTemporary())
        throw std::runtime_error("Cannot save settings");
}
} // namespace aw
