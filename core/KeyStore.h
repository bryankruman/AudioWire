// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Types.h"
#include <memory>
#include <optional>
namespace aw {
class KeyStore {
  public:
    virtual ~KeyStore() = default;
    virtual void put(const std::string& id, const Key&) = 0;
    virtual std::optional<Key> get(const std::string& id) = 0;
    virtual void erase(const std::string& id) = 0;
};
std::unique_ptr<KeyStore> makeKeyStore();
} // namespace aw
