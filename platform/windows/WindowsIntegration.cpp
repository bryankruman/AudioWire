// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include "WindowsIntegration.h"
#include "platform/AudioPlatform.h"
#include "app/Settings.h"
#include <audioclient.h>
#include <juce_core/juce_core.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
namespace aw {
using Microsoft::WRL::ComPtr;
// Windows has no documented API for the default-device convenience toggle.
// Isolate the established PolicyConfig interface and fail with a Sound settings action.
struct IPolicyConfig : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, INT64*, INT64*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, INT64*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
static const GUID policyClass = {
    0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};
static const GUID policyInterface = {
    0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};
namespace {
struct DefaultsJournal {
    std::wstring previous[2][3], installed[2];
    bool enabled = false;
    juce::File file() const {
        return settingsDirectory().getChildFile("windows-defaults.json");
    }
    bool save() const {
        if (!enabled)
            return true; // Safe developer modes never read/write the user's journal.
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("version", 1);
        for (int flow = 0; flow < 2; ++flow) {
            const auto key = juce::String(flow);
            object->setProperty("installed" + key, juce::String(installed[flow].c_str()));
            for (int role = 0; role < 3; ++role)
                object->setProperty("previous" + key + juce::String(role),
                                    juce::String(previous[flow][role].c_str()));
        }
        if (file().getParentDirectory().createDirectory().failed())
            return false;
        juce::TemporaryFile temporary(file());
        return temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(object.release()))) &&
               temporary.overwriteTargetFileWithTemporary();
    }
    void load() {
        const auto journal = file();
        if (!journal.existsAsFile() || journal.getSize() > 32768)
            return;
        const auto value = juce::JSON::parse(journal);
        if (!value.isObject() || static_cast<int>(value["version"]) != 1)
            return;
        for (int flow = 0; flow < 2; ++flow) {
            const auto key = juce::String(flow);
            auto read = [&](juce::String property) {
                const auto text = value[juce::Identifier(property)].toString();
                return text.length() <= 2048 ? std::wstring(text.toWideCharPointer()) : std::wstring();
            };
            installed[flow] = read("installed" + key);
            for (int role = 0; role < 3; ++role)
                previous[flow][role] = read("previous" + key + juce::String(role));
        }
    }
};
DefaultsJournal defaults;
} // namespace
bool switchDefaultAudio(const std::wstring& out, const std::wstring& in, bool restore, std::string& error) {
    if (restore && defaults.installed[0].empty() && defaults.installed[1].empty())
        return true;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IPolicyConfig> policy;
    if (FAILED(
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))) ||
        FAILED(CoCreateInstance(policyClass, nullptr, CLSCTX_ALL, policyInterface,
                                reinterpret_cast<void**>(policy.GetAddressOf())))) {
        error = "Open Windows Sound settings to select AudioWire.";
        return false;
    }
    bool ok = true;
    for (int flow = 0; flow < 2; ++flow) {
        const auto& id = flow == 0 ? out : in;
        if (!restore && id.empty())
            continue;
        if (!restore) {
            // Capture every role before the first switch, including roles that
            // a previous partial attempt never reached. A manual change becomes
            // the new baseline when the user explicitly opts in again.
            for (int role = 0; role < 3; ++role) {
                ComPtr<IMMDevice> current;
                LPWSTR currentId = nullptr;
                if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow == 0 ? eRender : eCapture,
                                                                  static_cast<ERole>(role), &current)))
                    current->GetId(&currentId);
                const std::wstring now = currentId ? currentId : L"";
                CoTaskMemFree(currentId);
                if (defaults.installed[flow].empty() || now != defaults.installed[flow])
                    defaults.previous[flow][role] = now;
            }
            defaults.installed[flow] = id;
            if (!defaults.save()) {
                std::string rollbackError;
                switchDefaultAudio({}, {}, true, rollbackError);
                error = "PC sound could not be backed up. Check your AudioWire settings folder permissions.";
                return false;
            }
        }
        for (int role = 0; role < 3; ++role) {
            ComPtr<IMMDevice> current;
            LPWSTR currentId = nullptr;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow == 0 ? eRender : eCapture,
                                                              static_cast<ERole>(role), &current)))
                current->GetId(&currentId);
            const std::wstring now = currentId ? currentId : L"";
            CoTaskMemFree(currentId);
            if (restore) {
                auto& previous = defaults.previous[flow][role];
                if (!previous.empty() && now == defaults.installed[flow] &&
                    FAILED(policy->SetDefaultEndpoint(previous.c_str(), static_cast<ERole>(role)))) {
                    ok = false; // Keep this role's backup for a later retry.
                    continue;
                }
                previous.clear(); // A user's manual switch always wins.
            } else {
                if (FAILED(policy->SetDefaultEndpoint(id.c_str(), static_cast<ERole>(role)))) {
                    std::string rollbackError;
                    switchDefaultAudio({}, {}, true, rollbackError);
                    error = "Open Windows Sound settings to select AudioWire.";
                    return false;
                }
            }
        }
        if (restore) {
            bool pending = false;
            for (const auto& old : defaults.previous[flow])
                pending = pending || !old.empty();
            if (!pending)
                defaults.installed[flow].clear();
        }
    }
    if (!defaults.save())
        ok = false;
    if (!ok)
        error = "Open Windows Sound settings to restore your previous speakers or microphone.";
    return ok;
}
void recoverDefaultAudio() {
    defaults.enabled = true;
    defaults.load();
    if (defaults.installed[0].empty() && defaults.installed[1].empty())
        return;
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::string error;
    switchDefaultAudio({}, {}, true, error);
    if (SUCCEEDED(initialized))
        CoUninitialize();
}

void setAutostart(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                      KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (enabled) {
        auto path = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName();
        auto command = juce::String("\"") + path + "\" --tray";
        auto wide = command.toWideCharPointer();
        RegSetValueExW(key, L"AudioWire", 0, REG_SZ, reinterpret_cast<const BYTE*>(wide),
                       static_cast<DWORD>((wcslen(wide) + 1) * sizeof(wchar_t)));
    } else
        RegDeleteValueW(key, L"AudioWire");
    RegCloseKey(key);
}
void openSoundSettings() {
    juce::URL("ms-settings:sound").launchInDefaultBrowser();
}
void openPermissionSettings() {
    openSoundSettings();
}
} // namespace aw
