// SPDX-License-Identifier: MIT
#include <windows.h>
#include "driver/shared/AwProtocol.h"
#include "driver/shared/AwSample.h"
#include <cstring>
#include <iostream>
int main() {
    static_assert(sizeof(AW_AUDIO_BLOCK) == 968, "Stable audio block ABI");
    static_assert(sizeof(AW_STATS) == 24, "Stable statistics ABI");
    static_assert((IOCTL_AUDIOWIRE_WRITE & 3) == METHOD_BUFFERED, "No user pointers in feed");
    for (int i = -32768; i <= 32767; ++i) {
        unsigned int bits = AwPcm16ToFloatBits(static_cast<short>(i));
        float value;
        std::memcpy(&value, &bits, 4);
        if (value != static_cast<float>(i) / 32768.0f) {
            std::cerr << "FAIL: driver float conversion\n";
            return 1;
        }
    }
    std::cout << "PASS: driver ABI layout and all 65536 PCM16-to-float values\n";
    return 0;
}
