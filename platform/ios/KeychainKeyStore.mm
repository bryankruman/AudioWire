// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/KeyStore.h"
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#include <sodium.h>
#include <stdexcept>
namespace aw {
class KeychainKeyStore final : public KeyStore {
    NSMutableDictionary* query(const std::string& identity) {
        if (identity.size() != 16 || identity.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::invalid_argument("Invalid device identity");
        return [@{
            (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService : @"ai.rlab.audiowire.pairs",
            (__bridge id)kSecAttrAccount : [NSString stringWithUTF8String:identity.c_str()]
        } mutableCopy];
    }

  public:
    void put(const std::string& identity, const Key& key) override {
        auto q = query(identity);
        NSData* data = [NSData dataWithBytes:key.data() length:key.size()];
        OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)q,
                                        (__bridge CFDictionaryRef) @{(__bridge id)kSecValueData : data});
        if (status == errSecItemNotFound) {
            q[(__bridge id)kSecValueData] = data;
            q[(__bridge id)kSecAttrAccessible] =
                (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
            status = SecItemAdd((__bridge CFDictionaryRef)q, nullptr);
        }
        if (status != errSecSuccess)
            throw std::runtime_error("iOS could not save the pairing. Unlock your iPhone and try again.");
    }
    std::optional<Key> get(const std::string& identity) override {
        auto q = query(identity);
        q[(__bridge id)kSecReturnData] = @YES;
        q[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
        CFTypeRef result = nullptr;
        if (SecItemCopyMatching((__bridge CFDictionaryRef)q, &result) != errSecSuccess)
            return {};
        NSData* data = CFBridgingRelease(result);
        if (data.length != 32)
            return {};
        Key key{};
        [data getBytes:key.data() length:32];
        return key;
    }
    void erase(const std::string& identity) override {
        auto status = SecItemDelete((__bridge CFDictionaryRef)query(identity));
        if (status != errSecSuccess && status != errSecItemNotFound)
            throw std::runtime_error("iOS could not forget the pairing.");
    }
};
std::unique_ptr<KeyStore> makeKeyStore() {
    return std::make_unique<KeychainKeyStore>();
}
} // namespace aw
