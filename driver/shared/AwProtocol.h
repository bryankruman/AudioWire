// SPDX-License-Identifier: MIT
#pragma once
// Versioned, pointer-free ABI. Buffered I/O keeps untrusted memory out of the DPC.
// 10 ms mono PCM16 blocks keep kernel work bounded and require no user-page mapping.
#define AW_ABI_VERSION 1u
#define AW_SAMPLE_RATE 48000u
#define AW_BLOCK_SAMPLES 480u
#define AW_RING_SAMPLES 9600u
#define AW_DEVICE_PATH L"\\\\.\\AudioWireFeed"
#define IOCTL_AUDIOWIRE_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_AUDIOWIRE_STATS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_AUDIOWIRE_RESET CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_DATA)
typedef struct _AW_AUDIO_BLOCK {
    unsigned int version;
    unsigned int count;
    short samples[AW_BLOCK_SAMPLES];
} AW_AUDIO_BLOCK;
typedef struct _AW_STATS {
    unsigned int version;
    unsigned int queued;
    unsigned long long underruns;
    unsigned long long overruns;
} AW_STATS;
