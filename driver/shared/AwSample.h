// SPDX-License-Identifier: MIT
#pragma once
// Exact PCM16 / 32768 encoded as IEEE754 binary32, using no kernel floating-point instructions.
inline unsigned int AwPcm16ToFloatBits(short value) {
    if (!value)
        return 0;
    const unsigned int sign = value < 0 ? 0x80000000u : 0u;
    const unsigned int magnitude =
        value < 0 ? static_cast<unsigned int>(-static_cast<int>(value)) : static_cast<unsigned int>(value);
    unsigned int msb = 0;
    for (unsigned int n = magnitude; n > 1; n >>= 1)
        ++msb;
    return sign | ((112u + msb) << 23) | ((magnitude << (23 - msb)) & 0x7fffffu);
}
