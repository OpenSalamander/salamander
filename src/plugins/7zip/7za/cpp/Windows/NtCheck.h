// Windows/NtCheck.h

#ifndef __WINDOWS_NT_CHECK_H
#define __WINDOWS_NT_CHECK_H

#ifdef _WIN32

#include "../Common/MyWindows.h"

#if !defined(_WIN64) && !defined(UNDER_CE)

// OPENSAL_7ZIP_PATCH BEGIN
// ****************************************************************************
// SalIsWindowsVersionOrGreater
//
// Based on SDK 8.1 VersionHelpers.h
// Indicates if the current OS version matches, or is greater than, the provided 
// version information. This function is useful in confirming a version of Windows 
// Server that doesn't share a version number with a client release.
// http://msdn.microsoft.com/en-us/library/windows/desktop/dn424964%28v=vs.85%29.aspx
// 

static inline bool SalIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
  OSVERSIONINFOEXW osvi;
  DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
    VER_MAJORVERSION, VER_GREATER_EQUAL),
    VER_MINORVERSION, VER_GREATER_EQUAL),
    VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

  SecureZeroMemory(&osvi, sizeof(osvi));     // nahrada za memset (nevyzaduje RTLko)
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  osvi.dwMajorVersion = wMajorVersion;
  osvi.dwMinorVersion = wMinorVersion;
  osvi.wServicePackMajor = wServicePackMajor;
  return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

static inline bool IsItWindowsNT()
{
//  OSVERSIONINFO vi;
//  vi.dwOSVersionInfoSize = sizeof(vi);
//  return (::GetVersionEx(&vi) && vi.dwPlatformId == VER_PLATFORM_WIN32_NT);
  return SalIsWindowsVersionOrGreater(5, 0, 0);   // Petr: jsou to aspon W2K?
}
// OPENSAL_7ZIP_PATCH END
#endif

#ifndef _UNICODE
  #if defined(_WIN64) || defined(UNDER_CE)
    bool g_IsNT = true;
    #define SET_IS_NT
  #else
    bool g_IsNT = false;
    #define SET_IS_NT g_IsNT = IsItWindowsNT();
  #endif
  #define NT_CHECK_ACTION
  // #define NT_CHECK_ACTION { NT_CHECK_FAIL_ACTION }
#else
  #if !defined(_WIN64) && !defined(UNDER_CE)
    #define NT_CHECK_ACTION if (!IsItWindowsNT()) { NT_CHECK_FAIL_ACTION }
  #else
    #define NT_CHECK_ACTION
  #endif
  #define SET_IS_NT
#endif

#define NT_CHECK  NT_CHECK_ACTION SET_IS_NT

#else

#define NT_CHECK

#endif

#endif
