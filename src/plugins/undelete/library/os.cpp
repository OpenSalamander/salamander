// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "..\undelete.rh2"

#include "miscstr.h"
#include "os.h"

// ***************************************************************************
//
//  Global variables
//
static BOOL OSVersionDetected = FALSE;
BOOL IsWindowsNT = FALSE;
BOOL IsWindows2000AndLater = FALSE;
BOOL IsWindows95 = FALSE;
BOOL IsWindows95OSR2AndLater = FALSE;
BOOL IsWindowsVistaAndLater = FALSE;

static HMODULE KernelModule = NULL;
static HINSTANCE NTShell32DLLInstance = NULL;

// ***************************************************************************
//
//  Static members
//
OS<char>::TFindFirstVolumeMountPoint OS<char>::F_FindFirstVolumeMountPoint = NULL;
OS<char>::TFindNextVolumeMountPoint OS<char>::F_FindNextVolumeMountPoint = NULL;
OS<char>::TFindVolumeMountPointClose OS<char>::F_FindVolumeMountPointClose = NULL;
OS<char>::TGetVolumeNameForVolumeMountPoint OS<char>::F_GetVolumeNameForVolumeMountPoint = NULL;
OS<char>::TGetDiskFreeSpaceEx OS<char>::F_GetDiskFreeSpaceEx = NULL;
OS<char>::TFindFirstVolume OS<char>::F_FindFirstVolume = NULL;
OS<char>::TFindNextVolume OS<char>::F_FindNextVolume = NULL;
OS<char>::TFindVolumeClose OS<char>::F_FindVolumeClose = NULL;
OS<char>::TGetVolumePathNamesForVolumeName OS<char>::F_GetVolumePathNamesForVolumeName = NULL;
OS<char>::TGetLogicalDriveStrings OS<char>::F_GetLogicalDriveStrings = NULL;
OS<char>::TSHGetFileInfo OS<char>::F_SHGetFileInfo = NULL;
HMODULE OS<char>::ImageResDLL = NULL;

// ***************************************************************************
//
//  Functions
//
BOOL OS_InitOSVersion()
{
    if (!OSVersionDetected)
    {
        OSVersionDetected = TRUE;

        // To run under W9x we must use the A-version of GetVersionEx().
        OSVERSIONINFOA osvi;
        ZeroMemory(&osvi, sizeof(osvi));
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        if (!GetVersionExA(&osvi))
        {
            DWORD err = GetLastError();
            TRACE_E("GetVersionEx() failed, GetLastError()=" << err);
            return FALSE;
        }
        IsWindowsNT = (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT);
        IsWindows2000AndLater = (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 5);
        IsWindows95 = (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
                       osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0);
        IsWindows95OSR2AndLater = (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
                                       (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0 && LOWORD(osvi.dwBuildNumber) > 1080) || // W95OSR2
                                   (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion >= 1));                                           // W98 a WinME
        IsWindowsVistaAndLater = (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 6);
    }
    return TRUE;
}

// ***************************************************************************
//
//  Explicitly instantiated template methods
//

// ****************************************************************************
//
// ANSI versions
//

BOOL OS<char>::OS_GetVolumeNameForVolumeMountPointExists()
{
    static BOOL functionsDetected = FALSE;

    // check for required functions, but just once
    if (!functionsDetected)
    {
        if (!KernelModule)
            KernelModule = GetModuleHandleA("kernel32.dll");

        if (KernelModule)
            F_GetVolumeNameForVolumeMountPoint = (TGetVolumeNameForVolumeMountPoint)GetProcAddress(KernelModule, "GetVolumeNameForVolumeMountPointA");

        functionsDetected = TRUE;
    }

    return (F_GetVolumeNameForVolumeMountPoint != NULL);
}

BOOL OS<char>::OS_VolumeEnumExists()
{
    static BOOL functionsDetected = FALSE;

    // check for required functions, but just once
    if (!functionsDetected)
    {
        if (!KernelModule)
            KernelModule = GetModuleHandleA("kernel32.dll");

        if (KernelModule)
        {
            F_FindFirstVolume = (TFindFirstVolume)GetProcAddress(KernelModule, "FindFirstVolumeA");
            F_FindNextVolume = (TFindNextVolume)GetProcAddress(KernelModule, "FindNextVolumeA");
            F_FindVolumeClose = (TFindVolumeClose)GetProcAddress(KernelModule, "FindVolumeClose");
        }
        if (!F_FindFirstVolume || !F_FindNextVolume || !F_FindVolumeClose)
        {
            F_FindFirstVolume = NULL;
            F_FindNextVolume = NULL;
            F_FindVolumeClose = NULL;
        }
        functionsDetected = TRUE;
    }

    return (F_FindFirstVolume != NULL);
}

BOOL OS<char>::OS_VolumeMountPointEnumExists()
{
    static BOOL functionsDetected = FALSE;

    // check for required functions, but just once
    if (!functionsDetected)
    {
        if (!KernelModule)
            KernelModule = GetModuleHandleA("kernel32.dll");

        if (KernelModule)
        {
            F_FindFirstVolumeMountPoint = (TFindFirstVolumeMountPoint)GetProcAddress(KernelModule, "FindFirstVolumeMountPointA");
            F_FindNextVolumeMountPoint = (TFindNextVolumeMountPoint)GetProcAddress(KernelModule, "FindNextVolumeMountPointA");
            F_FindVolumeMountPointClose = (TFindVolumeMountPointClose)GetProcAddress(KernelModule, "FindVolumeMountPointClose");
        }
        if (!F_FindFirstVolumeMountPoint || !F_FindNextVolumeMountPoint || !F_FindVolumeMountPointClose)
        {
            F_FindFirstVolumeMountPoint = NULL;
            F_FindNextVolumeMountPoint = NULL;
            F_FindVolumeMountPointClose = NULL;
        }
        functionsDetected = TRUE;
    }

    return (F_FindFirstVolumeMountPoint != NULL);
}

BOOL OS<char>::OS_GetLogicalDriveStringsExists()
{
    static BOOL functionsDetected = FALSE;

    // check for required functions, but just once
    if (!functionsDetected)
    {
        if (!KernelModule)
            KernelModule = GetModuleHandleA("kernel32.dll");

        if (KernelModule)
            F_GetLogicalDriveStrings = (TGetLogicalDriveStrings)GetProcAddress(KernelModule, "GetLogicalDriveStringsA");

        functionsDetected = TRUE;
    }

    return (F_GetLogicalDriveStrings != NULL);
}

BOOL OS<char>::OS_GetDiskFreeSpaceExExists()
{
    static BOOL functionsDetected = FALSE;

    // check for required functions, but just once
    if (!functionsDetected)
    {
        if (!KernelModule)
            KernelModule = GetModuleHandleA("kernel32.dll");

        if (KernelModule)
            F_GetDiskFreeSpaceEx = (TGetDiskFreeSpaceEx)GetProcAddress(KernelModule, "GetDiskFreeSpaceExA");

        functionsDetected = TRUE;
    }

    return (F_GetDiskFreeSpaceEx != NULL);
}

BOOL OS<char>::OS_GetVolumePathNamesForVolumeNameExists()
{
    static BOOL functionsDetected = FALSE;

    // check for required functions, but just once
    if (!functionsDetected)
    {
        if (!KernelModule)
            KernelModule = GetModuleHandleA("kernel32.dll");

        if (KernelModule)
            F_GetVolumePathNamesForVolumeName = (TGetVolumePathNamesForVolumeName)GetProcAddress(KernelModule, "GetVolumePathNamesForVolumeNameA");

        functionsDetected = TRUE;
    }

    return (F_GetVolumePathNamesForVolumeName != NULL);
}

BOOL OS<char>::OS_InitShell32Bindings()
{
    return TRUE;
}

void OS<char>::OS_ReleaseShell32Bindings()
{
}

template <>
VolumeType OS<char>::OS_GetVolumeType(const char* root)
{
    return static_cast<VolumeType>(::GetDriveTypeA(root));
}

template <>
void OS<char>::OS_GetDisplayNameFromSystem(const char* root, char* volumeName, int volumeNameBufSize)
{
    CALL_STACK_MESSAGE2("GetDisplayNameFromSystem(%s)", root);

    SHFILEINFOA fi = {0};
    if (SHGetFileInfoA(root, 0, &fi, sizeof(fi), SHGFI_DISPLAYNAME))
    {
        lstrcpynA(volumeName, fi.szDisplayName, volumeNameBufSize);
        char* s = strrchr(volumeName, '(');
        if (s != NULL)
        {
            while (s > volumeName && *(s - 1) == ' ')
                s--;
            *s = 0;
        }
    }
    else
        volumeName[0] = 0;
}

template <>
BOOL OS<char>::OS_GetVolumeInfo(const char* rootPathName, char* volumeNameBuffer, DWORD volumeNameSize,
                                DWORD* volumeSerialNumber, DWORD* maximumComponentLength,
                                DWORD* fileSystemFlags, char* fileSystemNameBuffer, DWORD fileSystemNameSize)
{
    return ::GetVolumeInformationA(rootPathName, volumeNameBuffer, volumeNameSize, volumeSerialNumber,
                                   maximumComponentLength, fileSystemFlags, fileSystemNameBuffer, fileSystemNameSize);
}

template <>
HANDLE OS<char>::OS_CreateFile(const char* fileName, DWORD desiredAccess, DWORD shareMode,
                               SECURITY_ATTRIBUTES* securityAttributes, DWORD creationDisposition,
                               DWORD flagsAndAttributes, HANDLE templateFile)
{
    return HANDLES_Q(CreateFileA(fileName, desiredAccess, shareMode, securityAttributes,
                                 creationDisposition, flagsAndAttributes, templateFile));
}

// ****************************************************************************
//
// get drives icons
//

template <>
HICON OS<char>::OS_GetFileOrPathIconAux(const char* path, BOOL large)
{
    __try
    {
        SHFILEINFOA shi;
        shi.hIcon = NULL;
        SHGetFileInfoA(path, 0, &shi, sizeof(shi),
                       SHGFI_ICON | SHGFI_SHELLICONSIZE | (large ? 0 : SHGFI_SMALLICON));
        return shi.hIcon;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return NULL;
    }
}
