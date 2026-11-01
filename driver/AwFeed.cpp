// SPDX-License-Identifier: MIT
#include "AwFeed.h"
#include "shared/AwProtocol.h"
#include "shared/AwSample.h"
#include <wdmsec.h>
namespace {
PDEVICE_OBJECT device = nullptr;
UNICODE_STRING link;
PDRIVER_DISPATCH previous[IRP_MJ_MAXIMUM_FUNCTION + 1]{};
KSPIN_LOCK lock;
SHORT samples[AW_RING_SAMPLES]{};
ULONG readPosition = 0, writePosition = 0, queued = 0;
ULONGLONG underruns = 0, overruns = 0;
LONG owned = 0;
const GUID deviceClass = {0x8a509e6c, 0x42e1, 0x4b19, {0x9e, 0xcb, 0x74, 0xb1, 0x63, 0x14, 0x31, 0x51}};
void reset() {
    KIRQL old;
    KeAcquireSpinLock(&lock, &old);
    readPosition = writePosition = queued = 0;
    RtlZeroMemory(samples, sizeof(samples));
    KeReleaseSpinLock(&lock, old);
}
NTSTATUS complete(PIRP irp, NTSTATUS status, ULONG_PTR bytes = 0) {
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}
NTSTATUS dispatch(PDEVICE_OBJECT object, PIRP irp) {
    auto stack = IoGetCurrentIrpStackLocation(irp);
    if (object != device)
        return previous[stack->MajorFunction](object, irp);
    switch (stack->MajorFunction) {
    case IRP_MJ_CREATE:
        if (InterlockedCompareExchange(&owned, 1, 0))
            return complete(irp, STATUS_SHARING_VIOLATION);
        reset();
        return complete(irp, STATUS_SUCCESS);
    case IRP_MJ_CLEANUP:
        reset();
        return complete(irp, STATUS_SUCCESS);
    case IRP_MJ_CLOSE:
        reset();
        InterlockedExchange(&owned, 0);
        return complete(irp, STATUS_SUCCESS);
    case IRP_MJ_DEVICE_CONTROL: {
        const ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
        if (code == IOCTL_AUDIOWIRE_RESET) {
            reset();
            return complete(irp, STATUS_SUCCESS);
        }
        if (code == IOCTL_AUDIOWIRE_WRITE) {
            if (stack->Parameters.DeviceIoControl.InputBufferLength != sizeof(AW_AUDIO_BLOCK) ||
                !irp->AssociatedIrp.SystemBuffer)
                return complete(irp, STATUS_INVALID_PARAMETER);
            const auto block = static_cast<AW_AUDIO_BLOCK*>(irp->AssociatedIrp.SystemBuffer);
            if (block->version != AW_ABI_VERSION || block->count > AW_BLOCK_SAMPLES)
                return complete(irp, STATUS_INVALID_PARAMETER);
            KIRQL old;
            KeAcquireSpinLock(&lock, &old);
            if (block->count > AW_RING_SAMPLES - queued) {
                ++overruns;
                const ULONG drop = block->count - (AW_RING_SAMPLES - queued);
                readPosition = (readPosition + drop) % AW_RING_SAMPLES;
                queued -= drop;
            }
            for (ULONG i = 0; i < block->count; ++i) {
                samples[writePosition] = block->samples[i];
                writePosition = (writePosition + 1) % AW_RING_SAMPLES;
            }
            queued += block->count;
            KeReleaseSpinLock(&lock, old);
            return complete(irp, STATUS_SUCCESS);
        }
        if (code == IOCTL_AUDIOWIRE_STATS) {
            if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(AW_STATS) ||
                !irp->AssociatedIrp.SystemBuffer)
                return complete(irp, STATUS_BUFFER_TOO_SMALL);
            auto stats = static_cast<AW_STATS*>(irp->AssociatedIrp.SystemBuffer);
            RtlZeroMemory(stats, sizeof(*stats));
            KIRQL old;
            KeAcquireSpinLock(&lock, &old);
            stats->version = AW_ABI_VERSION;
            stats->queued = queued;
            stats->underruns = underruns;
            stats->overruns = overruns;
            KeReleaseSpinLock(&lock, old);
            return complete(irp, STATUS_SUCCESS, sizeof(*stats));
        }
        return complete(irp, STATUS_INVALID_DEVICE_REQUEST);
    }
    default:
        return complete(irp, STATUS_INVALID_DEVICE_REQUEST);
    }
}

} // namespace
NTSTATUS AwFeedInitialize(PDRIVER_OBJECT driver) {
    KeInitializeSpinLock(&lock);
    UNICODE_STRING name, sddl;
    RtlInitUnicodeString(&name, L"\\Device\\AudioWireFeed");
    RtlInitUnicodeString(&link, L"\\DosDevices\\AudioWireFeed");
    RtlInitUnicodeString(&sddl, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;IU)");
    NTSTATUS status = IoCreateDeviceSecure(driver, 0, &name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
                                           FALSE, &sddl, &deviceClass, &device);
    if (!NT_SUCCESS(status))
        return status;
    device->Flags |= DO_BUFFERED_IO;
    status = IoCreateSymbolicLink(&link, &name);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(device);
        device = nullptr;
        return status;
    }
    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
        previous[i] = driver->MajorFunction[i];
    const UCHAR majors[] = {IRP_MJ_CREATE, IRP_MJ_CLOSE, IRP_MJ_CLEANUP, IRP_MJ_DEVICE_CONTROL};
    for (auto major : majors)
        driver->MajorFunction[major] = dispatch;
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
void AwFeedShutdown() {
    if (device) {
        IoDeleteSymbolicLink(&link);
        IoDeleteDevice(device);
        device = nullptr;
    }
}
void AwFeedRead(PUCHAR output, ULONG bytes, PWAVEFORMATEXTENSIBLE format) {
    RtlZeroMemory(output, bytes);
    const ULONG block = format->Format.nBlockAlign, channels = format->Format.nChannels;
    if (!block || channels < 1 || channels > 2 || format->Format.nSamplesPerSec != AW_SAMPLE_RATE)
        return;
    const ULONG width = format->Format.wBitsPerSample / 8;
    const bool floating = IsEqualGUID(format->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != FALSE;
    if ((width != 2 && width != 4) || block != channels * width)
        return;
    KIRQL old;
    KeAcquireSpinLock(&lock, &old);
    bool missed = false;
    for (ULONG frame = 0; frame < bytes / block; ++frame) {
        SHORT value = 0;
        if (queued) {
            value = samples[readPosition];
            readPosition = (readPosition + 1) % AW_RING_SAMPLES;
            --queued;
        } else
            missed = true;
        for (ULONG ch = 0; ch < channels; ++ch) {
            auto target = output + frame * block + ch * width;
            if (width == 2)
                RtlCopyMemory(target, &value, 2);
            else {
                ULONG encoded = floating ? AwPcm16ToFloatBits(value)
                                         : static_cast<ULONG>(static_cast<LONG>(value) * 65536);
                RtlCopyMemory(target, &encoded, 4);
            }
        }
    }
    if (missed)
        ++underruns;
    KeReleaseSpinLock(&lock, old);
}
