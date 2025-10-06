// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int QuietValidate;                       // 0 = the "-quiet-validate-???" command-line switch was not used; 1 = all, 2 = layout only
extern int QuietTranslate;                      // 0 = the "-quiet-translate" switch was not used, 1 = it was supplied
extern int QuietMarkChAsTrl;                    // 0 = the "-quiet-mark-changed-as-translated" switch was not used, 1 = it was supplied
extern char QuietImport[MAX_PATH];              // empty string = do not import anything; otherwise the directory with the legacy translation to import (see QuietImportOnlyDlgLayout below)
extern int QuietImportOnlyDlgLayout;            // applies only when QuietImport is non-empty: 0 = "-quiet-import", 1 = "-quiet-import-only-dialog-layout"
extern char QuietImportTrlProp[MAX_PATH];       // empty string = do not import; otherwise the project name whose translation properties should be imported
extern char QuietExportSLT[MAX_PATH];           // empty string = do not export; otherwise the directory where the SLT should be exported
extern char QuietExportSDC[MAX_PATH];           // empty string = do not export; otherwise the directory where the SDC should be exported
extern BOOL QuietExportSLTForDiff;              // TRUE = export the SLT without version info (for comparing data between builds)
extern char QuietImportSLT[MAX_PATH];           // empty string = do not import; otherwise the directory from which the SLT should be imported
extern char QuietExportSpellChecker[MAX_PATH];  // empty string = do not export; otherwise the directory for exporting spell-checker text
extern char OpenLayoutEditorDialogID[MAX_PATH]; // empty string = do not open the layout editor; otherwise ID of the dialog to open
extern BYTE* SharedMemoryCopy;                  // NULL = we do not keep a copy of the shared memory block

char* GetErrorText(DWORD error); // converts an error code to a human-readable string
char* LoadStr(int resID);        // retrieves a string from resources

BOOL GetTargetDirectory(HWND parent, const char* title, const char* comment,
                        char* path, const char* initDir);

BOOL GetFileCRC(const char* fileName, DWORD* crc);

DWORD UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal);

void GetFixedLogFont(LOGFONT* lf);

extern BOOL Windows7AndLater;

// ****************************************************************************
// SalIsWindowsVersionOrGreater
//
// Based on SDK 8.1 VersionHelpers.h
// Indicates if the current OS version matches, or is greater than, the provided
// version information. This function is useful in confirming a version of Windows
// Server that doesn't share a version number with a client release.
// http://msdn.microsoft.com/en-us/library/windows/desktop/dn424964%28v=vs.85%29.aspx
//

inline BOOL SalIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // replacement for memset (does not require the CRT)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}
