// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Types.h"
#include <functional>
#include <memory>
namespace aw {
class Discovery {
  public:
    virtual ~Discovery() = default;
    virtual void advertise(const Device&, std::uint16_t audioPort) = 0;
    // A removal is reported with the known device ID and an empty host.
    virtual void browse(std::function<void(Device)> found) = 0;
    virtual void stop() = 0;
};
std::unique_ptr<Discovery> makeDiscovery();
} // namespace aw
