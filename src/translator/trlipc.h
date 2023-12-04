// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(push)
#pragma pack(4)
struct CSharedMemory
{
    DWORD Version; // SHARED_MEMORY_VERSION
    BOOL Taken;    // zdrojovy proces nastavi na FALSE, cilovy na TRUE po te, co si vytvori kopii dat do lokalniho bufferu; zdrojovy pak sdilenou pamet zahodi
    DWORD Size;    // velikost sdilene pameti v bajtech
                   // stream...
};
#pragma pack(pop)

BOOL ReadSharedMemory();
void FreeSharedMemory();
