// SPDX-License-Identifier: MIT
#pragma once
#include <ntddk.h>
#include <portcls.h>
NTSTATUS AwFeedInitialize(PDRIVER_OBJECT);
void AwFeedShutdown();
void AwFeedRead(PUCHAR output, ULONG bytes, PWAVEFORMATEXTENSIBLE format);
