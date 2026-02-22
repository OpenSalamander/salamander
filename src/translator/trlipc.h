// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(push)
#pragma pack(4)
struct CSharedMemory
{
    DWORD Version; // SHARED_MEMORY_VERSION
    BOOL Taken;    // Source sets this to FALSE; the destination switches it to TRUE after copying the data to its local buffer, letting the source dispose of the shared memory
    DWORD Size;    // Size of the shared memory block in bytes
                   // stream...
};
#pragma pack(pop)

BOOL ReadSharedMemory();
void FreeSharedMemory();
