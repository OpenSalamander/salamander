// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "config.h"
#include "memapi.h"

#define CALL_STACK_MESSAGE_NONE

__forceinline void* MemAlloc(long bytes, void* heapInfo)
{
    CALL_STACK_MESSAGE_NONE
    return HeapAlloc((HANDLE)heapInfo, 0, bytes);
}

__forceinline int MemFree(void* pointer, void* heapInfo)
{
    CALL_STACK_MESSAGE_NONE
    return HeapFree((HANDLE)heapInfo, 0, pointer);
}

__forceinline void MemCopy(void* destination, const void* source, long length)
{
    CALL_STACK_MESSAGE_NONE
    CopyMemory(destination, source, length);
}

__forceinline void MemZero(void* pointer, long length)
{
    CALL_STACK_MESSAGE_NONE
    ZeroMemory(pointer, length);
}
