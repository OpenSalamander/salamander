// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// main.cpp

#include <objbase.h>
#define INITGUID

//#include "StdAfx.h"

#include "../CPP/Common/MyInitGuid.h"
#include "../CPP/Common/ComTry.h"
#include "../CPP/Windows/PropVariant.h"
#if defined(_WIN32) && defined(_7ZIP_LARGE_PAGES)
extern "C"
{
#include "../C/Alloc.h"
}
#endif

#include "../CPP/7zip/Archive/IArchive.h"
#include "../CPP/7zip/ICoder.h"
#include "../CPP/7zip/IPassword.h"
#include "../CPP/7zip/MyVersion.h"

// JRYFIXME - overit jestli mame nejak nastavit?
// pokud je hodnota -1, pouziva se CP_OEMCP
int g_CodePage = -1;

struct version_string
{
    version_string() : version(MY_VERSION " " MY_DATE) {}
    const char* version;
};

// Forces inclusion of a version string
static version_string version;

HINSTANCE g_hInstance;
#ifndef _UNICODE
#ifdef _WIN32

// ****************************************************************************
// SalIsWindowsVersionOrGreater
//
// Based on SDK 8.1 VersionHelpers.h
// Indicates if the current OS version matches, or is greater than, the provided
// version information. This function is useful in confirming a version of Windows
// Server that doesn't share a version number with a client release.
// http://msdn.microsoft.com/en-us/library/windows/desktop/dn424964%28v=vs.85%29.aspx
//

BOOL SalIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // nahrada za memset (nevyzaduje RTLko)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

bool g_IsNT = false;
bool IsItWindowsNT()
{
    //  OSVERSIONINFO versionInfo;
    //  versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    //  if (!::GetVersionEx(&versionInfo))
    //    return false;
    //  return (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
    return SalIsWindowsVersionOrGreater(5, 0, 0) ? true : false; // Petr: jsou to aspon W2K?
}
#endif
#endif

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hInstance;
#ifndef _UNICODE
#ifdef _WIN32
        g_IsNT = IsItWindowsNT();
#endif
#endif
#if defined(_WIN32) && defined(_7ZIP_LARGE_PAGES)
        SetLargePageSize();
#endif
    }
    return TRUE;
}

DEFINE_GUID(CLSID_CArchiveHandler,
            0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00);

static const UInt16 kDecodeId = 0x2790;

DEFINE_GUID(CLSID_CCodec,
            0x23170F69, 0x40C1, kDecodeId, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

STDAPI CreateCoder(const GUID* clsid, const GUID* iid, void** outObject);
STDAPI CreateArchiver(const GUID* classID, const GUID* iid, void** outObject);

STDAPI CreateObject(const GUID* clsid, const GUID* iid, void** outObject)
{
    // COM_TRY_BEGIN
    *outObject = 0;
    if (*iid == IID_ICompressCoder || *iid == IID_ICompressCoder2 || *iid == IID_ICompressFilter)
    {
        return CreateCoder(clsid, iid, outObject);
    }
    else
    {
        return CreateArchiver(clsid, iid, outObject);
    }
    // COM_TRY_END
}
