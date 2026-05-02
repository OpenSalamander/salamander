// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern __forceinline void* MemAlloc(long bytes, void* heapInfo);

extern __forceinline int MemFree(void* pointer, void* heapInfo);

extern __forceinline void MemCopy(void* destination, const void* source, long length);

extern __forceinline void MemZero(void* pointer, long length);

#ifndef NULL
#define NULL 0
#endif
