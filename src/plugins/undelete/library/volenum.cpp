// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "..\undelete.rh2"
#include "snapshot.h"
#include "miscstr.h"
#include "os.h"

#include "volenum.h"

#pragma pack(1)
struct ExtGetDskFreSpcStruc
{
    WORD ExtFree_Size;
    WORD ExtFree_Level;
    DWORD ExtFree_SectorsPerCluster;
    DWORD ExtFree_BytesPerSector;
    DWORD ExtFree_AvailClusters;
    DWORD ExtFree_TotalClusters;
    DWORD ExtFree_AvailablePhysSectors;
    DWORD ExtFree_TotalPhysSectors;
    DWORD ExtFree_AvailableAllocationUnits;
    DWORD ExtFree_TotalAllocationUnits;
    DWORD ExtFree_Rsvd[2];
};
#pragma pack()

BOOL GetDiskFreeSpace95Aux(const wchar_t* path, LPDWORD lpSectorsPerCluster,
                           LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                           LPDWORD lpTotalNumberOfClusters)
{
    // no unicode support on Win95
    return FALSE;
}
BOOL GetDiskFreeSpace95(const wchar_t* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters)
{
    // no unicode support on Win95
    return FALSE;
}

BOOL GetDiskFreeSpace95Aux(const char* path, LPDWORD lpSectorsPerCluster,
                           LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                           LPDWORD lpTotalNumberOfClusters)
{
    return GetDiskFreeSpaceA(path, lpSectorsPerCluster, lpBytesPerSector,
                             lpNumberOfFreeClusters, lpTotalNumberOfClusters);
}

BOOL GetDiskFreeSpace95(const char* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters)
{
    if (sizeof(ExtGetDskFreSpcStruc) != 44)
        TRACE_E("Compiler error: Bad struct alignment!");
    else
    {
        /*
       On Windows 95, use the technique described in
       the Knowledge Base article Q125712 and in MSDN under
       "Windows 95 Guide to Programming", "Using Windows 95
       features", "Using VWIN32 to Carry Out MS-DOS Functions".
    */

        BOOL ok = FALSE;
        HANDLE h = NOHANDLES(CreateFileA("\\\\.\\VWIN32", 0, 0, 0, 0, FILE_FLAG_DELETE_ON_CLOSE, 0));
        if (h != INVALID_HANDLE_VALUE)
        {
            DIOC_REGISTERS reg;
            ExtGetDskFreSpcStruc gdfs;
            memset(&gdfs, 0, sizeof(gdfs));
            gdfs.ExtFree_Level = 0; // required

            reg.reg_EBX = 0;                      // jen tak pro formu
            reg.reg_EDX = (DWORD)(DWORD_PTR)path; // type cast is OK, we will not run it on x64
            reg.reg_ECX = sizeof(gdfs);
            reg.reg_EDI = (DWORD)(DWORD_PTR)&gdfs; // type cast is OK, we will not run it on x64
            reg.reg_ESI = 0;
            reg.reg_EAX = 0x7303;
            reg.reg_Flags = CARRY_FLAG; // assume failure

            // Make sure both DeviceIoControl and Int 21h succeeded.
            DWORD cb;
            if (DeviceIoControl(h, VWIN32_DIOC_DOS_IOCTL, &reg, sizeof(reg), &reg, sizeof(reg), &cb, 0) &&
                !(reg.reg_Flags & CARRY_FLAG))
            {
                if (lpSectorsPerCluster != NULL)
                    *lpSectorsPerCluster = gdfs.ExtFree_SectorsPerCluster;
                if (lpBytesPerSector != NULL)
                    *lpBytesPerSector = gdfs.ExtFree_BytesPerSector;
                if (lpNumberOfFreeClusters != NULL)
                    *lpNumberOfFreeClusters = gdfs.ExtFree_AvailClusters;
                if (lpTotalNumberOfClusters != NULL)
                    *lpTotalNumberOfClusters = gdfs.ExtFree_TotalClusters;
                ok = TRUE;
            }
            else
                TRACE_E("DeviceIoControl() or Int 21h error.");
            NOHANDLES(CloseHandle(h));
        }
        else
            TRACE_E("Error opening VWIN32.VXD.");

        if (!ok)
        {
            return GetDiskFreeSpaceA(path, lpSectorsPerCluster, lpBytesPerSector,
                                     lpNumberOfFreeClusters, lpTotalNumberOfClusters);
        }
        else
            return TRUE;
    }
}
