// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include "app/Settings.h"
#include "core/KeyStore.h"
#include <sodium.h>
#include <wincrypt.h>
namespace aw {
class DpapiKeyStore final : public KeyStore {
    juce::File file(const std::string& id) {
        if (id.size() != 16 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::invalid_argument("Invalid device identity");
        return settingsDirectory().getChildFile("pairs").getChildFile(juce::String(id) + ".dpapi");
    }

  public:
    void put(const std::string& id, const Key& key) override {
        auto f = file(id);
        if (f.getParentDirectory().createDirectory().failed())
            throw std::runtime_error("Cannot save pairing");
        DATA_BLOB plain{static_cast<DWORD>(key.size()), const_cast<BYTE*>(key.data())}, sealed{};
        if (!CryptProtectData(&plain, L"AudioWire pair", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                              &sealed))
            throw std::runtime_error("Windows could not protect the pairing");
        juce::TemporaryFile temp(f);
        bool ok = temp.getFile().replaceWithData(sealed.pbData, sealed.cbData) &&
                  temp.overwriteTargetFileWithTemporary();
        sodium_memzero(sealed.pbData, sealed.cbData);
        LocalFree(sealed.pbData);
        if (!ok)
            throw std::runtime_error("Cannot save pairing");
    }
    std::optional<Key> get(const std::string& id) override {
        juce::MemoryBlock blob;
        if (!file(id).loadFileAsData(blob) || blob.getSize() > 4096)
            return {};
        DATA_BLOB sealed{static_cast<DWORD>(blob.getSize()), static_cast<BYTE*>(blob.getData())}, plain{};
        if (!CryptUnprotectData(&sealed, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                                &plain))
            return {};
        std::optional<Key> key;
        if (plain.cbData == 32) {
            Key k{};
            std::copy_n(plain.pbData, 32, k.begin());
            key = k;
            sodium_memzero(k.data(), 32);
        }
        sodium_memzero(plain.pbData, plain.cbData);
        LocalFree(plain.pbData);
        return key;
    }
    void erase(const std::string& id) override {
        auto f = file(id);
        if (f.existsAsFile() && !f.deleteFile())
            throw std::runtime_error("Windows could not forget the pairing");
    }
};
std::unique_ptr<KeyStore> makeKeyStore() {
    return std::make_unique<DpapiKeyStore>();
}
} // namespace aw
