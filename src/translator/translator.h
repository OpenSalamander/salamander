// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int QuietValidate;                       // 0 = nebyl pouzit switch "-quiet-validate-???" prikazove radky, 1 = all, 2 = layout
extern int QuietTranslate;                      // 0 = nebyl pouzit switch "-quiet-translate", 1 = byl pouzit
extern int QuietMarkChAsTrl;                    // 0 = nebyl pouzit switch "-quiet-mark-changed-as-translated", 1 = byl pouzit
extern char QuietImport[MAX_PATH];              // prazdny retezec = nic neimportovat; jinak cesta k adresari, ze ktereho mame provest import stareho prekladu, dalsi info viz QuietImportOnlyDlgLayout (na dalsim radku)
extern int QuietImportOnlyDlgLayout;            // jen pri neprazdnem QuietImport: 0 = byl pouzit switch "-quiet-import", 1 = "-quiet-import-only-dialog-layout"
extern char QuietImportTrlProp[MAX_PATH];       // prazdny retezec = nic neimportovat; jinak nazev projektu, ze ktereho mame provest import translation properties
extern char QuietExportSLT[MAX_PATH];           // prazdny retezec = nic neexportovat; jinak adresar, do ktereho mame provest export SLT
extern char QuietExportSDC[MAX_PATH];           // prazdny retezec = nic neexportovat; jinak adresar, do ktereho mame provest export SDC
extern BOOL QuietExportSLTForDiff;              // TRUE = export do SLT bez version infa (pro porovnani dat mezi verzemi)
extern char QuietImportSLT[MAX_PATH];           // prazdny retezec = nic neimportovat; jinak adresar, ze ktereho mame provest import SLT
extern char QuietExportSpellChecker[MAX_PATH];  // prazdny retezec = nic neexportovat; jinak adresar, do ktereho mame provest export textu pro spell checker
extern char OpenLayoutEditorDialogID[MAX_PATH]; // prazdny retezec = LE neotevirat; jinak ID dialogu, pro ktery mame otevrit LE
extern BYTE* SharedMemoryCopy;                  // NULL = nedrzime kopii sdilene pameti

char* GetErrorText(DWORD error); // prevadi cislo chyby na string
char* LoadStr(int resID);        // taha string z resourcu

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

    SecureZeroMemory(&osvi, sizeof(osvi)); // nahrada za memset (nevyzaduje RTLko)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}
