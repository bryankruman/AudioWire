// SPDX-License-Identifier: MIT
// For an isolated driver-test VM only. It opens the actual microphone feed.
#include <windows.h>
#include "driver/shared/AwProtocol.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
unsigned u32(const std::vector<unsigned char>& b, size_t p) {
    return unsigned(b[p]) | (unsigned(b[p + 1]) << 8) | (unsigned(b[p + 2]) << 16) |
           (unsigned(b[p + 3]) << 24);
}
unsigned u16(const std::vector<unsigned char>& b, size_t p) {
    return b[p] | (unsigned(b[p + 1]) << 8);
}
void stats(HANDLE h) {
    AW_STATS s{};
    DWORD n = 0;
    if (DeviceIoControl(h, IOCTL_AUDIOWIRE_STATS, nullptr, 0, &s, sizeof(s), &n, nullptr) && n == sizeof(s))
        std::cout << "Queued samples: " << s.queued << "; underruns: " << s.underruns
                  << "; overruns: " << s.overruns << "\n";
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage in driver-test VM: AudioWireDriverFeed --probe | mono-48000-pcm16.wav\n";
        return 2;
    }
    Handle h;
    h.value =
        CreateFileW(AW_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h.value == INVALID_HANDLE_VALUE) {
        std::cerr << "AudioWire feed unavailable (Windows error " << GetLastError()
                  << "). Install the VM's test-signed driver and close AudioWire.\n";
        return 1;
    }
    if (std::strcmp(argv[1], "--probe") == 0) {
        DWORD n = 0;
        AW_AUDIO_BLOCK b{};
        b.version = AW_ABI_VERSION;
        b.count = 1;
        auto rejected = [&](DWORD ioctl, DWORD length) {
            return !DeviceIoControl(h.value, ioctl, &b, length, nullptr, 0, &n, nullptr);
        };
        bool ok = rejected(IOCTL_AUDIOWIRE_WRITE, 1);
        b.version = 999;
        ok = ok && rejected(IOCTL_AUDIOWIRE_WRITE, sizeof(b));
        b.version = AW_ABI_VERSION;
        b.count = AW_BLOCK_SAMPLES + 1;
        ok = ok && rejected(IOCTL_AUDIOWIRE_WRITE, sizeof(b));
        ok = ok &&
             rejected(CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8ff, METHOD_BUFFERED, FILE_WRITE_ACCESS), sizeof(b));
        Handle second;
        second.value =
            CreateFileW(AW_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        ok = ok && second.value == INVALID_HANDLE_VALUE;
        DeviceIoControl(h.value, IOCTL_AUDIOWIRE_RESET, nullptr, 0, nullptr, 0, &n, nullptr);
        stats(h.value);
        std::cout << (ok ? "PASS: feed bounds, version, unsupported request, exclusive writer\n"
                         : "FAIL: feed validation\n");
        return ok ? 0 : 1;
    }
    std::ifstream f(argv[1], std::ios::binary | std::ios::ate);
    if (!f || f.tellg() < 12 || f.tellg() > 50 * 1024 * 1024) {
        std::cerr << "Use a WAV smaller than 50 MB.\n";
        return 2;
    }
    auto length = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<unsigned char> b(length);
    f.read(reinterpret_cast<char*>(b.data()), length);
    if (!f || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) {
        std::cerr << "Invalid WAV.\n";
        return 2;
    }
    bool format = false;
    size_t audio = 0, bytes = 0;
    for (size_t p = 12; p + 8 <= b.size();) {
        unsigned size = u32(b, p + 4);
        if (size > b.size() - p - 8) {
            std::cerr << "Truncated WAV.\n";
            return 2;
        }
        if (!std::memcmp(b.data() + p, "fmt ", 4) && size >= 16)
            format =
                u16(b, p + 8) == 1 && u16(b, p + 10) == 1 && u32(b, p + 12) == 48000 && u16(b, p + 22) == 16;
        if (!std::memcmp(b.data() + p, "data", 4)) {
            audio = p + 8;
            bytes = size;
        }
        p += 8 + size + (size & 1);
    }
    if (!format || !audio || bytes % 2) {
        std::cerr << "Use mono 48 kHz PCM16 WAV.\n";
        return 2;
    }
    auto deadline = std::chrono::steady_clock::now();
    for (size_t p = 0; p < bytes; p += AW_BLOCK_SAMPLES * 2) {
        AW_AUDIO_BLOCK block{};
        block.version = AW_ABI_VERSION;
        block.count = static_cast<unsigned>(std::min<size_t>(AW_BLOCK_SAMPLES, (bytes - p) / 2));
        std::memcpy(block.samples, b.data() + audio + p, block.count * 2);
        DWORD n = 0;
        if (!DeviceIoControl(h.value, IOCTL_AUDIOWIRE_WRITE, &block, sizeof(block), nullptr, 0, &n,
                             nullptr)) {
            std::cerr << "Feed write failed: " << GetLastError() << "\n";
            return 1;
        }
        deadline += std::chrono::milliseconds(10);
        std::this_thread::sleep_until(deadline);
    }
    stats(h.value);
    return 0;
}
