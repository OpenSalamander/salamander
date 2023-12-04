// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <shlobj.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <stdio.h>

#include "resource.h"
#include "infinst.h"
#include "remove\ctxmenu.h"

#define COPY_FLAG_NOREMOVE 0x00000001 // nebude zarazeno do odnistlacniho logu
//#define COPY_FLAG_DELAY_ENABLED 0x00000002  // pokud bude cila otevren, dojde k delay copy
#define COPY_FLAG_FORCE_OVERWRITE 0x00000004 // nekompromisne prepise cil, at je jakej je
#define COPY_FLAG_DONT_OVERWRITE 0x00000008  // neprepise cil, pokud je novejsi
#define COPY_FLAG_TEST_CONFLICT 0x00000010   // testuje existenci minule (jine) verze
#define COPY_FLAG_SKIP_SAME 0x00000020       // preskocit stejny soubor bez ptani

DWORD InstallFinish(BOOL DoRunOnce);

// sections
const char* INF_PRIVATE_SECTION = "Private";
const char* INF_COPYSECTION = "CopyFiles";
const char* INF_SHORTCUTSECTION = "CreateShortcuts";
const char* INF_CREATEDIRSSECTION = "CreateDirs";
const char* INF_CREATEREGKEYS = "AddRegistryKeys";
const char* INF_CREATEREGVALUES = "AddRegistryValues";
const char* INF_GETREGVAR = "GetRegistryVar";

// items
const char* INF_APPNAME = "ApplicationName";
const char* INF_APPNAMEVER = "ApplicationNameVer";
const char* INF_DEFDIR = "DefaultDirectory";
//const char *INF_USELASTDIR = "UseLastDirectory";
const char* INF_LASTDIRS = "LastDirectories";
const char* INF_VIEWREADME = "ViewReadme";
const char* INF_RUNPROGRAM = "RunProgram";
const char* INF_RUNPROGRAMQUIET = "RunProgramQuiet";
const char* INF_INCREMENTFILECONTENTSRC = "IncrementFileContentSrc";
const char* INF_INCREMENTFILECONTENTDST = "IncrementFileContentDst";
const char* INF_ENSURESALAMANDER25DIR = "EnsureSalamander25Dir";
const char* INF_UNINSTALLRUNPROGRAMQUIET = "UninstallRunProgramQuiet";
const char* INF_SAVEREMOVELOG = "SaveRemoveLog";
const char* INF_LICENSEFILE = "LicenseFile";
const char* INF_LICENSEFILECZ = "LicenseFileCZ";
const char* INF_FIRSTREADME = "FirstReadmeFile";
const char* INF_SKIPCHOOSEDIR = "SkipChooseDirectory";
const char* INF_CHECKCOMMONCONTROLS = "CheckCommonControls";
const char* INF_CHECKRUNNINGAPPS = "CheckRunningApps";
const char* INF_DELREGVALUES = "DelRegistryValues";
const char* INF_DELREGKEYS = "DelRegistryKeys";
const char* INF_DELFILES = "DelFiles";
const char* INF_DELEMPTYDIRS = "DelEmptyDirs";
const char* INF_DELSHELLEXTS = "DelShellExts";
const char* INF_LOADOLDREMOVELOG = "LoadOldRemoveLog";
const char* INF_AUTOIMPORTCONFIG = "AutoImportConfig";
const char* INF_DISPLAYWELCOMEWARNING = "DisplayWelcomeWarning";
const char* INF_SLGLANGUAGES = "SLGLanguages";
const char* INF_WERLOCALDUMPS = "WERLocalDumps";

const char* INF_FILENAME = "\\setup.inf";

char ModulePath[MAX_PATH] = {0};
char WindowsDirectory[MAX_PATH] = {0};
char SystemDirectory[MAX_PATH] = {0};
char ProgramFilesDirectory[MAX_PATH] = {0};

char DesktopDirectory[MAX_PATH] = {0};
char StartMenuDirectory[MAX_PATH] = {0};
char StartMenuProgramDirectory[MAX_PATH] = {0};
char QuickLaunchDirectory[MAX_PATH] = {0};

char InfFileName[MAX_PATH];

DWORD CCMajorVer = 0;
DWORD CCMinorVer = 0;
DWORD CCMajorVerNeed = 0;
DWORD CCMinorVerNeed = 0;

char CmdLineDestination[MAX_PATH] = {0};

BOOL ContainsPathUpgradableVersion(const char* path);
BOOL IncrementFileContent();

//BOOL AfterRebootCopy(const char *sFileName, const char *tFileName);

//****************************************************************************
//
// MyStrToDWORD
//

DWORD
MyStrToDWORD(const char* num)
{
    const char* p = num;
    const char* iter;
    int len;
    DWORD ret = 0;
    int rad;

    while (*p == ' ')
        p++;

    len = lstrlen(p);
    if (len > 0)
    {
        rad = 1;
        iter = p + len - 1;
        while (iter >= p)
        {
            if (*iter < '0' || *iter > '9')
                return 0;
            ret += (*iter - '0') * rad;
            iter--;
            rad *= 10;
        }
    }
    return ret;
}

//****************************************************************************
//
// HandleError
//

int HandleError(int titleID, int messageID, unsigned long err)
{
    char title[500];
    char message[1000];
    LoadString(GetModuleHandle(NULL), titleID, title, 500);
    LoadString(GetModuleHandle(NULL), messageID, message, 1000);
    if (err)
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message + lstrlen(message),
                      1000 - lstrlen(message), NULL);
    if (!SetupInfo.Silent)
        MessageBox(HWizardDialog, message, title, MB_OK | MB_ICONEXCLAMATION);
    return 1;
}

int HandleErrorM(int titleID, const char* msg, unsigned long err)
{
    char title[500];
    char message[1000];
    lstrcpyn(message, msg, 1000);
    LoadString(GetModuleHandle(NULL), titleID, title, 500);
    if (err)
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message + lstrlen(message),
                      1000 - lstrlen(message), NULL);

    if (!SetupInfo.Silent)
        MessageBox(HWizardDialog, message, title, MB_OK | MB_ICONEXCLAMATION);
    return 1;
}

// ****************************************************************************

const char* MyLStrChr(const char* str, char chr)
{
    const char* iter;
    iter = str;
    while (*iter != 0 && *iter != chr)
        iter++;
    if (*iter != 0)
        return iter;
    return NULL;
}

const char* MyRStrChr(const char* str, char chr)
{
    const char* iter;
    iter = str + lstrlen(str);
    while (iter >= str && *iter != chr)
        iter--;
    if (iter >= str && *iter == chr)
        return iter;
    return NULL;
}

void MyCopyMemory(PVOID dst, CONST VOID* src, DWORD len)
{
    DWORD i;
    for (i = 0; i < len; i++)
    {
        *((BYTE*)dst) = *((BYTE*)src);
        ((BYTE*)dst)++;
        ((BYTE*)src)++;
    }
}

int MyMemCmp(CONST VOID* src1, CONST VOID* src2, DWORD len)
{
    DWORD i;
    for (i = 0; i < len; i++)
    {
        if (*((BYTE*)src1) != *((BYTE*)src2))
            return 1;
    }
    return 0;
}

BOOL CheckAndCreateDirectory(const char* dir)
{
    BOOL quiet = TRUE;
    DWORD attrs = GetFileAttributes(dir);
    char buf[MAX_PATH + 100];
    char name[MAX_PATH];
    if (attrs == INVALID_FILE_ATTRIBUTES) // asi neexistuje, umoznime jej vytvorit
    {
        char root[MAX_PATH];
        GetRootPath(root, dir);
        if (lstrlen(dir) <= lstrlen(root)) // dir je root adresar
        {
            if (!SetupInfo.Silent)
            {
                wsprintf(buf, LoadStr(IDS_CREATEDIRFAILED), dir);
                MessageBox(HWizardDialog, buf, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
            }
            return FALSE;
        }
        //    wsprintf(buf, LoadStr(IDS_CREATEDIRECTORY), dir);
        if (quiet)
        {
            char* s;
            const char* st;
            int len;

            lstrcpy(name, dir);
            while (1) // najdeme prvni existujici adresar
            {
                s = strrchr(name, '\\');
                if (s == NULL)
                {
                    if (!SetupInfo.Silent)
                    {
                        wsprintf(buf, LoadStr(IDS_CREATEDIRFAILED), dir);
                        MessageBox(HWizardDialog, buf, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
                    }
                    return FALSE;
                }
                if (s - name > (int)lstrlen(root))
                    *s = 0;
                else
                {
                    lstrcpy(name, root);
                    break; // uz jsme na root-adresari
                }
                attrs = GetFileAttributes(name);
                if (attrs != INVALID_FILE_ATTRIBUTES) // jmeno existuje
                {
                    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                        break; // budeme stavet od tohoto adresare
                    else       // je to soubor, to by neslo ...
                    {
                        if (!SetupInfo.Silent)
                        {
                            wsprintf(buf, LoadStr(IDS_NAMEUSEDFORFILE), name);
                            MessageBox(HWizardDialog, buf, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
                        }
                        return FALSE;
                    }
                }
            }
            s = name + lstrlen(name) - 1;
            if (*s != '\\')
            {
                *++s = '\\';
                *++s = 0;
            }
            st = dir + lstrlen(name);
            if (*st == '\\')
                st++;
            len = lstrlen(name);
            while (*st != 0)
            {
                const char* slash = MyLStrChr(st, '\\');
                if (slash == NULL)
                    slash = st + lstrlen(st);
                MyCopyMemory(name + len, st, (int)(slash - st));
                name[len += (int)(slash - st)] = 0;
                while (1)
                {
                    if (!CreateDirectory(name, NULL))
                    {
                        if (!SetupInfo.Silent)
                        {
                            wsprintf(buf, LoadStr(IDS_CREATEDIRFAILED), name);
                            if (MessageBox(HWizardDialog, buf, MAINWINDOW_TITLE, MB_RETRYCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                                return FALSE;
                        }
                        else
                            return FALSE;
                    }
                    else
                        break; // hotovo
                }
                name[len++] = '\\';
                if (*slash == '\\')
                    slash++;
                st = slash;
            }
            return TRUE;
        }
        return FALSE;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return TRUE;
    else // soubor, to by neslo ...
    {
        if (!SetupInfo.Silent)
        {
            wsprintf(buf, LoadStr(IDS_NAMEUSEDFORFILE), dir);
            MessageBox(HWizardDialog, buf, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }
}

//
// ****************************************************************************

struct VS_VERSIONINFO_HEADER_TAG
{
    WORD wLength;
    WORD wValueLength;
    WORD wType;
};

typedef struct VS_VERSIONINFO_HEADER_TAG VS_VERSIONINFO_HEADER;

BOOL GetModuleVersion(HINSTANCE hModule, DWORD* major, DWORD* minor)
{
    HRSRC hRes;
    HGLOBAL hVer;
    DWORD resSize;
    const BYTE* first;
    const BYTE* iterator;
    DWORD signature;
    VS_FIXEDFILEINFO* ffi;

    hRes = FindResource(hModule, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
    if (hRes == NULL)
        return FALSE;

    hVer = LoadResource(hModule, hRes);
    if (hVer == NULL)
        return FALSE;

    resSize = SizeofResource(hModule, hRes);
    first = (BYTE*)LockResource(hVer);
    if (resSize == 0 || first == 0)
        return FALSE;

    iterator = first + sizeof(VS_VERSIONINFO_HEADER);

    signature = 0xFEEF04BD;

    while (MyMemCmp(iterator, &signature, 4) != 0)
    {
        iterator++;
        if (iterator + 4 >= first + resSize)
            return FALSE;
    }

    ffi = (VS_FIXEDFILEINFO*)iterator;

    *major = ffi->dwFileVersionMS;
    *minor = ffi->dwFileVersionLS;

    return TRUE;
}

//****************************************************************************
//
// QueryOverwrite
//

// QueryOverwrite: dialog Overwrite pro copy file rutinu
// pokud uspeje, vrati v promenne result nasledujici hodnoty:
#define CFQO_YES 0
#define CFQO_YESALL 1
#define CFQO_SKIP 2
#define CFQO_SKIPALL 3
#define CFQO_CANCEL 4
/*
BOOL QueryOverwrite(const char *sFileName, const char *sourceAttr, 
                    const char *tFileName, const char *targetAttr, int *result);
INT_PTR CALLBACK QueryOverwriteDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam);
*/
struct CFQOInternalTag
{
    const char* sFileName;
    const char* sourceAttr;
    const char* tFileName;
    const char* targetAttr;
};

typedef struct CFQOInternalTag CFQOInternal;

INT_PTR CALLBACK QueryOverwriteDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CFQOInternal* data = (CFQOInternal*)lParam;

        // vycentruji dialog k obrazovce
        CenterWindow(hWindow);

        // nastavim retezce
        SetDlgItemText(hWindow, IDC_CF_SOURCEFILENAME, data->sFileName);
        SetDlgItemText(hWindow, IDC_CF_SOURCEFILEATTR, data->sourceAttr);
        SetDlgItemText(hWindow, IDC_CF_TARGETFILENAME, data->tFileName);
        SetDlgItemText(hWindow, IDC_CF_TARGETFILEATTR, data->targetAttr);
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
        case IDC_CF_YESALL:
        case IDC_CF_SKIP:
        case IDC_CF_SKIPALL:
        {
            EndDialog(hWindow, LOWORD(wParam));
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

BOOL QueryOverwrite(const char* sFileName, const char* sourceAttr,
                    const char* tFileName, const char* targetAttr, int* result)
{
    INT_PTR dlgRet;
    CFQOInternal data;
    data.sFileName = sFileName;
    data.sourceAttr = sourceAttr;
    data.tFileName = tFileName;
    data.targetAttr = targetAttr;

    dlgRet = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CF_OVERWRITE),
                            HProgressDialog, QueryOverwriteDlgProc, (LPARAM)&data);
    if (dlgRet == -1)
    {
        HandleError(IDS_MAINWINDOWTITLE, ERROR_DLGCREATE, GetLastError());
        return FALSE;
    }

    switch (dlgRet)
    {
    case IDOK:
        *result = CFQO_YES;
        break;
    case IDCANCEL:
        *result = CFQO_CANCEL;
        break;
    case IDC_CF_YESALL:
        *result = CFQO_YESALL;
        break;
    case IDC_CF_SKIP:
        *result = CFQO_SKIP;
        break;
    case IDC_CF_SKIPALL:
        *result = CFQO_SKIPALL;
        break;
    default:
        *result = CFQO_CANCEL;
    }
    return TRUE;
}

//****************************************************************************
//
// QueryRetry
//

struct CFQRInternalTag
{
    const char* sFileName;
    DWORD Error;
};

typedef struct CFQRInternalTag CFQRInternal;

INT_PTR CALLBACK QueryRetryDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        char buff[1024];
        CFQRInternal* data;
        data = (CFQRInternal*)lParam;

        // vycentruji dialog k obrazovce
        CenterWindow(hWindow);

        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, data->Error,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buff,
                      1024, NULL);

        // nastavim retezce
        SetDlgItemText(hWindow, IDC_CF_RETRYNAME, data->sFileName);
        SetDlgItemText(hWindow, IDC_CF_RETRYERROR, buff);
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
        case IDC_CF_SKIP:
        case IDC_CF_SKIPALL:
        {
            EndDialog(hWindow, LOWORD(wParam));
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

BOOL QueryRetry(const char* sFileName, DWORD error, int* result)
{
    INT_PTR dlgRet;
    CFQRInternal data;
    data.sFileName = sFileName;
    data.Error = error;

    dlgRet = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CF_RETRY),
                            HProgressDialog, QueryRetryDlgProc, (LPARAM)&data);
    if (dlgRet == -1)
    {
        HandleError(IDS_MAINWINDOWTITLE, ERROR_DLGCREATE, GetLastError());
        return FALSE;
    }

    switch (dlgRet)
    {
    case IDOK:
        *result = CFQO_YES;
        break;
    case IDCANCEL:
        *result = CFQO_CANCEL;
        break;
    case IDC_CF_SKIP:
        *result = CFQO_SKIP;
        break;
    case IDC_CF_SKIPALL:
        *result = CFQO_SKIPALL;
        break;
    default:
        *result = CFQO_CANCEL;
    }
    return TRUE;
}

//****************************************************************************
//
// GetComCtlVersion
//

// soubor z cesty sFileName nakopci do souboru tFileName
// pokud cilovy soubor uz existuje, dostavi se promenne overwrite
// a skip pokud je nastavena promenna overwrite, nepta se na prepis
// pokud je nastavena promenna skip, je soubor preskocen
// pri chybe vrati FALSE - pak options nemaji vyznam
// pri korektnim ukonceni vrati TRUE a promenna options
// je nastavena podle zpusobu provedeni operace

#define COPYFILEOPTIONS_NONE 0
#define COPYFILEOPTIONS_OVERWRITE 1
#define COPYFILEOPTIONS_OVERWRITEALL 2
#define COPYFILEOPTIONS_SKIP 3
#define COPYFILEOPTIONS_SKIPALL 4
#define COPYFILEOPTIONS_CANCEL 5
#define COPYFILEOPTIONS_SKIP_CREATE 6
#define COPYFILEOPTIONS_SKIPALL_CREATE 7
#define COPYFILEOPTIONS_SUCCESS 8

#define COPYBUFFER_MAX 50000

BOOL FileExist(const char* fileName)
{
    DWORD attr = GetFileAttributes(fileName);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return FALSE;
    return TRUE;
    /*
  // tohle nefacha na sitovem disku (zkouseno pod NT 4.0)
  HANDLE hFile = CreateFile(fileName, 0, 0,
                            NULL, OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    return FALSE;
  }
  else
  {
    CloseHandle(hFile);
    return TRUE;
  }
  */
}

BOOL IsSameFile(const char* src, const char* dst)
{
    DWORD srcMajor, srcMinor;
    DWORD dstMajor, dstMinor;
    HINSTANCE hSrc, hDst;

    hSrc = LoadLibraryEx(src, NULL, LOAD_LIBRARY_AS_DATAFILE);
    hDst = LoadLibraryEx(dst, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hSrc != NULL && hDst != NULL)
    {
        if (GetModuleVersion(hSrc, &srcMajor, &srcMinor) && GetModuleVersion(hDst, &dstMajor, &dstMinor))
        {
            if (srcMajor == dstMajor && srcMinor == dstMinor)
            {
                FreeLibrary(hSrc);
                FreeLibrary(hDst);
                return TRUE;
            }
        }
    }
    if (hSrc != NULL)
        FreeLibrary(hSrc);
    if (hDst != NULL)
        FreeLibrary(hDst);
    return FALSE;
}

BOOL MyCopyFile(const char* sFileName, const char* tFileName,
                BOOL overwrite, BOOL skip, BOOL skipCreate, int* options,
                DWORD flags)
{
    HANDLE hSourceFile = NULL;
    HANDLE hTargetFile = NULL;
    char buff[1000];                 // pro hlasky
    char copyBuffer[COPYBUFFER_MAX]; // pro vlastni kopirovani
    DWORD read;
    DWORD fileAtttr;

    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;

    *options = COPYFILEOPTIONS_NONE;

    // pokusim se otevrit zdrojovy soubor
    hSourceFile = CreateFile(sFileName, GENERIC_READ,
                             FILE_SHARE_READ, NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_SEQUENTIAL_SCAN,
                             NULL);
    if (hSourceFile == INVALID_HANDLE_VALUE)
    {
        // nepovedlo se - utecu pryc
        DWORD error = GetLastError();
        wsprintf(buff, LoadStr(ERROR_CF_OPENFILE), sFileName);
        HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
        return FALSE;
    }

    // pokud nemame force na overwrite a cilovy soubor existuje, musime se zeptat
    // na jeho prepsani
    if (!overwrite)
    {
        if (FileExist(tFileName))
        {
            BY_HANDLE_FILE_INFORMATION sourceInformation;
            BY_HANDLE_FILE_INFORMATION targetInformation;
            FILETIME ft;
            SYSTEMTIME st;
            int result;

            if (skip)
            {
                return TRUE;
            }

            hTargetFile = CreateFile(tFileName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hTargetFile == INVALID_HANDLE_VALUE)
            {
                // tohle by vubec nemelo nastat, pred chvilkou jsem ho otevrel v stejnem rezimu
                DWORD error = GetLastError();
                wsprintf(buff, LoadStr(ERROR_CF_OPENFILE), tFileName);
                HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
                CloseHandle(hSourceFile);
                return FALSE;
            }

            if (!GetFileInformationByHandle(hSourceFile, &sourceInformation))
            {
                DWORD error = GetLastError();
                wsprintf(buff, LoadStr(ERROR_CF_FILEINFORMATION), sFileName);
                HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
                CloseHandle(hSourceFile);
                CloseHandle(hTargetFile);
                return FALSE;
            }

            if (!GetFileInformationByHandle(hTargetFile, &targetInformation))
            {
                DWORD error = GetLastError();
                wsprintf(buff, LoadStr(ERROR_CF_FILEINFORMATION), tFileName);
                HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
                CloseHandle(hSourceFile);
                CloseHandle(hTargetFile);
                return FALSE;
            }
            CloseHandle(hTargetFile);

            if (!(flags & COPY_FLAG_FORCE_OVERWRITE))
            {
                BOOL testTime = TRUE;

                if (flags & COPY_FLAG_SKIP_SAME)
                {
                    // pokud ma soubor stejnou velikost a verzi, skipneme ho
                    // pokusim se oba moduly nacist
                    if (sourceInformation.nFileSizeLow == targetInformation.nFileSizeLow && IsSameFile(sFileName, tFileName))
                    {
                        result = CFQO_SKIP;
                        testTime = FALSE;
                    }
                }

                // pokud je prepisovany soubor novejsi a neni nastaven COPY_FLAG_FORCE_OVERWRITE,
                // doptame se uzivatele
                if (testTime)
                {
                    if (CompareFileTime(&sourceInformation.ftLastWriteTime, &targetInformation.ftLastWriteTime) == -1)
                    {
                        if (!(flags & COPY_FLAG_DONT_OVERWRITE))
                        {
                            char sourceAttr[200];
                            char targetAttr[200];
                            wsprintf(sourceAttr, "%d, ", sourceInformation.nFileSizeLow);
                            FileTimeToLocalFileTime(&sourceInformation.ftLastWriteTime, &ft);
                            FileTimeToSystemTime(&ft, &st);
                            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL,
                                              sourceAttr + lstrlen(sourceAttr), 50) == 0)
                                wsprintf(sourceAttr + lstrlen(sourceAttr), "%d.%d.%d", st.wDay, st.wMonth, st.wYear);
                            lstrcat(sourceAttr, ", ");
                            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL,
                                              sourceAttr + lstrlen(sourceAttr), 50) == 0)
                                wsprintf(sourceAttr + lstrlen(sourceAttr), "%d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

                            wsprintf(targetAttr, "%d, ", targetInformation.nFileSizeLow);
                            FileTimeToLocalFileTime(&targetInformation.ftLastWriteTime, &ft);
                            FileTimeToSystemTime(&ft, &st);
                            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL,
                                              targetAttr + lstrlen(targetAttr), 50) == 0)
                                wsprintf(targetAttr + lstrlen(targetAttr), "%d.%d.%d", st.wDay, st.wMonth, st.wYear);
                            lstrcat(targetAttr, ", ");
                            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL,
                                              targetAttr + lstrlen(targetAttr), 50) == 0)
                                wsprintf(targetAttr + lstrlen(targetAttr), "%d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
                            if (!QueryOverwrite(sFileName, sourceAttr, tFileName, targetAttr, &result))
                            {
                                CloseHandle(hSourceFile);
                                return FALSE;
                            }
                        }
                        else
                            result = CFQO_SKIP;
                    }
                    else
                        result = CFQO_YES;
                } //else result je jiz nastaven z na CFQO_SKIP
            }
            else
                result = CFQO_YES;

            switch (result)
            {
            case CFQO_YES:
                *options = COPYFILEOPTIONS_OVERWRITE;
                break;
            case CFQO_YESALL:
                *options = COPYFILEOPTIONS_OVERWRITEALL;
                break;
            case CFQO_SKIP:
                *options = COPYFILEOPTIONS_SKIP;
                break;
            case CFQO_SKIPALL:
                *options = COPYFILEOPTIONS_SKIPALL;
                break;
            case CFQO_CANCEL:
                *options = COPYFILEOPTIONS_CANCEL;
                break;
            default:
                *options = COPYFILEOPTIONS_CANCEL;
                break;
            }

            if (*options == COPYFILEOPTIONS_CANCEL ||
                *options == COPYFILEOPTIONS_SKIP || *options == COPYFILEOPTIONS_SKIPALL)
                return TRUE;

            // zbejva nam overwrite a overwrite all, takze jdem na vec
        }
    }

    if ((flags & COPY_FLAG_DONT_OVERWRITE) && (*options == COPYFILEOPTIONS_NONE))
    {
        *options = COPYFILEOPTIONS_SUCCESS;
        return TRUE;
    }

    if (FileExist(tFileName))
    {
        // prevalcujeme READONLY/SYSTEM/HIDDEN soubory
        if (!SetFileAttributes(tFileName, FILE_ATTRIBUTE_ARCHIVE))
        {
            DWORD error = GetLastError();
            wsprintf(buff, LoadStr(ERROR_CF_FILESETATTR), tFileName);
            HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
            CloseHandle(hSourceFile);
            return FALSE;
        }
    }

    fileAtttr = GetFileAttributes(sFileName);
    if (fileAtttr == INVALID_FILE_ATTRIBUTES)
    {
        HandleError(IDS_MAINWINDOWTITLE, ERROR_CF_GETATTR, GetLastError());
        CloseHandle(hSourceFile);
        return FALSE;
    }

    // otevreme si vystupni soubor pro zapis
CreateAgain:
    hTargetFile = CreateFile(tFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, fileAtttr, NULL);
    if (hTargetFile == INVALID_HANDLE_VALUE)
    {
        int result;
        DWORD error = GetLastError();
        /*
    if (flags & COPY_FLAG_DELAY_ENABLED && error == ERROR_SHARING_VIOLATION)
    {
      if (AfterRebootCopy(sFileName, tFileName))
        return TRUE;
    }
*/
        if (skipCreate)
        {
            CloseHandle(hSourceFile);
            return TRUE;
        }
        if (QueryRetry(tFileName, error, &result))
        {
            switch (result)
            {
            case CFQO_YES:
                goto CreateAgain;
                break;
            case CFQO_SKIP:
                *options = COPYFILEOPTIONS_SKIP_CREATE;
                break;
            case CFQO_SKIPALL:
                *options = COPYFILEOPTIONS_SKIPALL_CREATE;
                break;
            case CFQO_CANCEL:
                *options = COPYFILEOPTIONS_CANCEL;
                break;
            }
            CloseHandle(hSourceFile);
            return TRUE;
        }

        CloseHandle(hSourceFile);
        return FALSE;
    }

    // prekopcime obsah zdrojoveho souboru do ciloveho
    do
    {
        if (!ReadFile(hSourceFile, copyBuffer, COPYBUFFER_MAX, &read, NULL))
        {
            DWORD error = GetLastError();
            wsprintf(buff, LoadStr(ERROR_CF_READFILE), sFileName);
            HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
            CloseHandle(hTargetFile);
            CloseHandle(hSourceFile);
            return FALSE;
        }

        if (read > 0)
        {
            DWORD written;
            if (!WriteFile(hTargetFile, copyBuffer, read, &written, NULL) || read != written)
            {
                DWORD error = GetLastError();
                wsprintf(buff, LoadStr(ERROR_CF_WRITEFILE), tFileName);
                HandleErrorM(IDS_MAINWINDOWTITLE, buff, error);
                CloseHandle(hTargetFile);
                CloseHandle(hSourceFile);
                return FALSE;
            }
        }
    } while (read == COPYBUFFER_MAX);

    // nastavime cilovemu souboru stejny cas, jaky ma soubor zdrojovy
    GetFileTime(hSourceFile, &ftCreationTime, &ftLastAccessTime, &ftLastWriteTime);
    SetFileTime(hTargetFile, &ftCreationTime, &ftLastAccessTime, &ftLastWriteTime);

    CloseHandle(hTargetFile);
    CloseHandle(hSourceFile);

    if (*options == COPYFILEOPTIONS_NONE)
        *options = COPYFILEOPTIONS_SUCCESS;

    return TRUE;
}
/*
// presune soubor ciloveho adresare, ale pod pracovni nazev
// nastavi, aby po rebootu byl cil smazan a zdroj prejmenovan na cil
// nastavi promennou RebootNeeded

BOOL AfterRebootCopy(const char *sFileName, const char *tFileName)
{
  char tDirectory[MAX_PATH];
  char tTmpFileName[MAX_PATH];
  int options;
  lstrcpy(tDirectory, tFileName);

  *(strrchr(tDirectory, '\\')) = '\0';        // Strip file name
  if (GetTempFileName(tDirectory, "STP", 0, tTmpFileName) == 0)
    return FALSE;

  options = COPYFILEOPTIONS_NONE;
  if (!MyCopyFile(sFileName, tTmpFileName, TRUE, FALSE, FALSE, &options, 0))
    return FALSE;

  if (options != COPYFILEOPTIONS_SUCCESS)
    return FALSE;

  if (SystemWindowsNT)
  {
    if (MoveFileEx(tTmpFileName, tFileName, MOVEFILE_DELAY_UNTIL_REBOOT | MOVEFILE_REPLACE_EXISTING) == 0)
      return FALSE;
  }

  SetupInfo.RebootNeeded = TRUE;
  return TRUE;
}
*/
//****************************************************************************
//
// GetComCtlVersion
//

HRESULT GetComCtlVersion(LPDWORD pdwMajor, LPDWORD pdwMinor)
{
    HINSTANCE hComCtl;
    //load the DLL
    hComCtl = LoadLibrary(TEXT("comctl32.dll"));
    if (hComCtl)
    {
        HRESULT hr = S_OK;
        DLLGETVERSIONPROC pDllGetVersion;
        /*
     You must get this function explicitly because earlier versions of the DLL
     don't implement this function. That makes the lack of implementation of the
     function a version marker in itself.
    */
        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hComCtl, TEXT("DllGetVersion"));
        if (pDllGetVersion)
        {
            DLLVERSIONINFO dvi = {0};
            dvi.cbSize = sizeof(dvi);
            hr = (*pDllGetVersion)(&dvi);
            if (SUCCEEDED(hr))
            {
                *pdwMajor = dvi.dwMajorVersion;
                *pdwMinor = dvi.dwMinorVersion;
            }
            else
            {
                hr = E_FAIL;
            }
        }
        else
        {
            /*
      If GetProcAddress failed, then the DLL is a version previous to the one
      shipped with IE 3.x.
      */
            *pdwMajor = 4;
            *pdwMinor = 0;
        }
        FreeLibrary(hComCtl);
        return hr;
    }
    return E_FAIL;
}

//************************************************************************************
//
// CheckRunningApp
//
// zjisti, jestli nebezi nejaka aplikace s tridou okna appWindowClassName
//

void CheckRunningApp(const char* appWindowClassName)
{
    HWND hWnd;
    hWnd = FindWindow(appWindowClassName, NULL);
    if (hWnd != NULL)
    {
        char windowName[100];
        char buff[2000];

        GetWindowText(hWnd, windowName, 100);
        wsprintf(buff, LoadStr(IDS_APPRUNNING), windowName);

        if (!SetupInfo.Silent)
            MessageBox(NULL, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONINFORMATION);
    }
}

//************************************************************************************
//
// OpenInfFile
//

BOOL OpenInfFile()
{
    HANDLE hFile;
    DWORD read;
    BOOL bResult = FALSE;
    BOOL x64Mark;
    BOOL x64x86Conflit = FALSE;

    lstrcpy(InfFileName, ModulePath);
    lstrcat(InfFileName, "\\x64");
    x64Mark = FileExist(InfFileName);
#ifdef _WIN64
    if (!x64Mark)
        x64x86Conflit = TRUE;
#else
    if (x64Mark)
        x64x86Conflit = TRUE;
#endif
    if (x64x86Conflit)
    {
        MessageBox(NULL, "Internal x64/x86 conflict.", MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    lstrcpy(InfFileName, ModulePath);
    lstrcat(InfFileName, INF_FILENAME);

    // nacteme inf soubor
    hFile = CreateFile(InfFileName, GENERIC_READ, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        if (!SetupInfo.Silent)
        {
            char buff[MAX_PATH];
            wsprintf(buff, LoadStr(ERROR_LOADINF), InfFileName);
            MessageBox(NULL, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }

    if (!ReadFile(hFile, SetupInfo.InfFile, sizeof(SetupInfo.InfFile), &read, NULL))
    {
        if (!SetupInfo.Silent)
        {
            char buff[MAX_PATH];
            CloseHandle(hFile);
            wsprintf(buff, LoadStr(ERROR_LOADINF), InfFileName);
            MessageBox(NULL, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }
    CloseHandle(hFile);

    // vytahneme sekci GetRegistryVar
    if (!DoGetRegistryVarSection())
        return FALSE;

    // je-li treba, zkontroloujeme verzi common controls
    SetupInfo.TmpSection[0] = 0;
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_CHECKCOMMONCONTROLS, "",
                            SetupInfo.TmpSection, MAX_PATH, InfFileName);
    if (SetupInfo.TmpSection[0] != 0)
    {
        char* minorPtr;

        minorPtr = SetupInfo.TmpSection;
        while (*minorPtr != 0 && *minorPtr != '.')
            minorPtr++;

        if (*minorPtr == '.')
        {
            *minorPtr = 0;
            minorPtr++;
            CCMajorVerNeed = MyStrToDWORD(SetupInfo.TmpSection);
            CCMinorVerNeed = MyStrToDWORD(minorPtr);
        }

        GetComCtlVersion(&CCMajorVer, &CCMinorVer);
        if (CCMajorVer < CCMajorVerNeed || (CCMajorVer == CCMajorVerNeed && CCMinorVer < CCMinorVerNeed))
        {
            if (!SetupInfo.Silent)
                DialogBox(HInstance, MAKEINTRESOURCE(IDD_COMMONCONTROL), NULL, CommonControlsDlgProc);
        }
    }

    // je-li treba, zkontrolujeme bezici aplikace
    SetupInfo.TmpSection[0] = 0;
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_CHECKRUNNINGAPPS, "",
                            SetupInfo.CheckRunningApps, MAX_PATH, InfFileName);
    if (SetupInfo.CheckRunningApps[0] != 0 && !SetupInfo.Silent)
    {
        char appClass[1000];
        char* begin;
        char* end;

        begin = end = SetupInfo.CheckRunningApps;

        while (*begin != 0)
        {
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(appClass, begin, (int)(end - begin + 1));
            if (*end == ',')
                end++;
            begin = end;

            CheckRunningApp(appClass);
        }
    }

    // nacteme zakladni data
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_APPNAME, "",
                            SetupInfo.ApplicationName, MAX_PATH, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_APPNAMEVER, SetupInfo.ApplicationName,
                            SetupInfo.ApplicationNameVer, MAX_PATH, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_DEFDIR, "",
                            SetupInfo.DefaultDirectory, MAX_PATH, InfFileName);

    //  GetPrivateProfileString(INF_PRIVATE_SECTION, INF_USELASTDIR, "",
    //                          SetupInfo.UseLastDirectory, MAX_PATH, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_LASTDIRS, "",
                            SetupInfo.LastDirectories, 3000, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_SAVEREMOVELOG, "",
                            SetupInfo.SaveRemoveLog, MAX_PATH, InfFileName);

    if (GetCurDispLangID() == 1029 /* cestina */) // zkousime CZ a pokud neni, tak i EN verzi licence
    {
        GetPrivateProfileString(INF_PRIVATE_SECTION, INF_LICENSEFILECZ, "",
                                SetupInfo.LicenseFilePath, MAX_PATH, InfFileName);
        if (SetupInfo.LicenseFilePath[0] == 0)
        {
            GetPrivateProfileString(INF_PRIVATE_SECTION, INF_LICENSEFILE, "",
                                    SetupInfo.LicenseFilePath, MAX_PATH, InfFileName);
        }
    }
    else // zkousime jen EN verzi licence
    {
        GetPrivateProfileString(INF_PRIVATE_SECTION, INF_LICENSEFILE, "",
                                SetupInfo.LicenseFilePath, MAX_PATH, InfFileName);
    }

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_FIRSTREADME, "",
                            SetupInfo.FirstReadmeFilePath, MAX_PATH, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_UNINSTALLRUNPROGRAMQUIET, "",
                            SetupInfo.UnistallRunProgramQuietPath, MAX_PATH, InfFileName);

    SetupInfo.TmpSection[0] = 0;
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_SKIPCHOOSEDIR, "",
                            SetupInfo.TmpSection, MAX_PATH, InfFileName);
    SetupInfo.SkipChooseDir = SetupInfo.TmpSection[0] == '1';

    SetupInfo.TmpSection[0] = 0;
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_LOADOLDREMOVELOG, "",
                            SetupInfo.TmpSection, MAX_PATH, InfFileName);
    SetupInfo.LoadOldRemoveLog = SetupInfo.TmpSection[0] == '1';

    SetupInfo.TmpSection[0] = 0;
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_DISPLAYWELCOMEWARNING, "",
                            SetupInfo.TmpSection, MAX_PATH, InfFileName);
    SetupInfo.DisplayWelcomeWarning = SetupInfo.TmpSection[0] == 0 ? 0 : MyStrToDWORD(SetupInfo.TmpSection);

    ExpandPath(SetupInfo.DefaultDirectory);
    if (CmdLineDestination[0] != 0)
        lstrcpy(SetupInfo.DefaultDirectory, CmdLineDestination);
    ExpandPath(SetupInfo.LicenseFilePath);

    SetupInfo.UseFirstReadme = FALSE;
    if (SetupInfo.FirstReadmeFilePath[0] != 0)
    {
        ExpandPath(SetupInfo.FirstReadmeFilePath);
        if (LoadTextFile(SetupInfo.FirstReadmeFilePath, SetupInfo.FirstReadme, 100000))
            SetupInfo.UseFirstReadme = TRUE;
    }

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_INCREMENTFILECONTENTSRC, "",
                            SetupInfo.IncrementFileContentSrc, 5000, InfFileName);
    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_INCREMENTFILECONTENTDST, "",
                            SetupInfo.IncrementFileContentDst, 5000, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_ENSURESALAMANDER25DIR, "",
                            SetupInfo.EnsureSalamander25Dir, 5000, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_AUTOIMPORTCONFIG, "",
                            SetupInfo.AutoImportConfig, MAX_PATH, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_SLGLANGUAGES, "",
                            SetupInfo.SLGLanguages, MAX_PATH, InfFileName);

    GetPrivateProfileString(INF_PRIVATE_SECTION, INF_WERLOCALDUMPS, "",
                            SetupInfo.WERLocalDumps, MAX_PATH, InfFileName);

    SetupInfo.UseLicenseFile = lstrlen(SetupInfo.LicenseFilePath) > 0;

    ExtractCreateDirsSection();
    ExtractShortcutSection();
    ExtractCopySection();

    if (CmdLineDestination[0] == 0 && !SetupInfo.Silent)
        LoadLastDirectory(); // mame nastavene DefaultDirectory a UseLastDirectory - muzem jit na vec

    return TRUE;
}

BOOL MyGetPrivateProfileSection(const char* infFile, const char* section, char* buff, int buffMax)
{
    const char* p = infFile;
    char* dst = buff;
    while (*p != 0)
    {
        while (*p != '[' && *p != 0)
            p++;
        if (*p == '[')
        {
            const char* end = p + 1;
            while (*end != ']' && *end != 0)
                end++;
            if (*end == ']')
            {
                char tmp[1024];
                const char* src = p + 1;
                lstrcpyn(tmp, src, (int)(end - src + 1));
                if (lstrcmpi(tmp, section) == 0)
                {
                    char line[1024];
                    p = end + 1;

                    while (*p != '[' && *p != 0)
                    {
                        while (*p == '\r' || *p == '\n')
                            p++;
                        end = p;
                        while (*end != '\r' && *end != '\n' && *end != 0)
                            end++;

                        while (*p == ' ')
                            p++;

                        lstrcpyn(line, p, (int)(end - p + 1));

                        if (*line != ';' && *line != 0)
                        {
                            int len;
                            lstrcpy(dst, line);
                            len = lstrlen(line);
                            dst += len + 1;
                        }
                        while (*end == '\r' || *end == '\n')
                            end++;
                        p = end;
                    }
                    *dst = 0;
                    return TRUE;
                }
                else
                {
                    p = end + 1;
                }
            }
            else
                return FALSE;
        }
        else
            return FALSE;
    }
    return FALSE;
}

// pokud line uz v target neexistuje, pripoji ji na konec zakonci dvouma nulama
void RemoveAddLine(char* target, const char* line)
{
    char* p = target;
    while (*p != 0)
    {
        if (lstrcmpi(p, line) == 0)
            return;
        p += lstrlen(p) + 1;
    }

    lstrcpy(p, line);
    p += lstrlen(line) + 1;
    *p = 0; // konecny terminator
}

//************************************************************************************
//
// QueryFreeSpace
//

BOOL QueryFreeSpace(char* driveSpec, LONGLONG* spaceRequired)
{
    /*
  HDSKSPC hDskSpc;
  char root[MAX_PATH];

  lstrcpy(root, driveSpec);
  if (root[lstrlen(root) - 1] == '\\')
    root[lstrlen(root) - 1] = 0;


  hDskSpc = SetupCreateDiskSpaceList(NULL, 0, 0);
  if (hDskSpc == NULL)
    return FALSE;

  if (!SetupAddInstallSectionToDiskSpaceList(hDskSpc, HInfFile, NULL, "Install", 0, 0))
  {
    SetupDestroyDiskSpaceList(hDskSpc);
    return FALSE;
  }

  if (!SetupQuerySpaceRequiredOnDrive(hDskSpc, root, spaceRequired, NULL, 0))
  {
    SetupDestroyDiskSpaceList(hDskSpc);
    return FALSE;
  }
  SetupDestroyDiskSpaceList(hDskSpc);
  */
    return TRUE;
}

BOOL GetSpecialFolderPath(int folder, char* path)
{
    ITEMIDLIST* pidl; // vyber root-folderu
    *path = 0;
    if (SUCCEEDED(SHGetSpecialFolderLocation(HWizardDialog, folder, &pidl)))
    {
        IMalloc* alloc;
        SHGetPathFromIDList(pidl, path);
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->lpVtbl->DidAlloc(alloc, pidl) == 1)
                alloc->lpVtbl->Free(alloc, pidl);
            alloc->lpVtbl->Release(alloc);
        }
        return TRUE;
    }
    return FALSE;
}

// vraci ukazatel na zaverecne %SLG (pokud existuje), jinak vraci NULL
char* FindSLGEnding(char* buff)
{
    int len = lstrlen(buff);
    if (len < 4)
        return NULL;
    if (lstrcmp(buff + len - 4, "%SLG") != 0)
        return NULL;
    return buff + len - 4;
}

int GetSLGNameLen(const char* buff)
{
    const char* p = buff;
    while (*p != 0 && *p != ',')
        p++;
    return (int)(p - buff);
}

void ExtractCopySection()
{
    char* line;
    char src[MAX_PATH];
    char dst[MAX_PATH];
    //char size[100];
    char flags[100];
    char* from;
    char* to;
    char* srcSLGEnding;
    char* dstSLGEnding;
    BOOL expandLanguages;
    const char* slgLang;

    SetupInfo.CopyFrom[0] = 0;
    SetupInfo.CopyTo[0] = 0;
    SetupInfo.CopyCount = 0;
    //  SetupInfo.SpaceRequired = 0;

    from = SetupInfo.CopyFrom;
    to = SetupInfo.CopyTo;

    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_COPYSECTION, SetupInfo.CopySection, sizeof(SetupInfo.CopySection));
    line = SetupInfo.CopySection;
    while (*line != 0)
    {
        char* begin;
        char* end;
        int len = lstrlen(line);
        if (len > 0 && line[0] != ';')
        {
            BYTE flagsNum;
            src[0] = 0;
            dst[0] = 0;
            //size[0] = 0;
            flags[0] = 0;
            begin = end = line;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(src, begin, (int)(end - begin + 1));

            if (*end == ',')
                end++;

            begin = end;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(dst, begin, (int)(end - begin + 1));

            /* velikost uz neziskavame
      if (*end == ',')
        end++;
      begin = end;
      while (*end != 0 && *end != ',')
        end++;
      lstrcpyn(size, begin, end - begin + 1);
      */

            flagsNum = 0;
            if (*end == ',')
            {
                end++;

                begin = end;
                while (*end != 0 && *end != ',')
                    end++;
                lstrcpyn(flags, begin, (int)(end - begin + 1));
                flagsNum = MyStrToDWORD(flags) & 0xFF;
            }

            // overime, zda src i dst nekonci "%SLG"; pokud ano, udealme expanzi
            srcSLGEnding = FindSLGEnding(src);
            dstSLGEnding = FindSLGEnding(dst);
            expandLanguages = (srcSLGEnding != NULL && dstSLGEnding != NULL);
            slgLang = expandLanguages ? SetupInfo.SLGLanguages : "";

            do
            {
                SetupInfo.CopyFlags[SetupInfo.CopyCount] = flagsNum;

                if (expandLanguages)
                {
                    int slgNameLen = GetSLGNameLen(slgLang);
                    lstrcpyn(srcSLGEnding, slgLang, slgNameLen + 1);
                    lstrcpyn(dstSLGEnding, slgLang, slgNameLen + 1);
                    slgLang += slgNameLen;
                    if (*slgLang == ',')
                        slgLang++;
                }

                lstrcpy(from, src);
                lstrcpy(to, dst);
                from += lstrlen(src) + 1;
                to += lstrlen(dst) + 1;

                SetupInfo.CopyCount++;
            } while (*slgLang != 0);
        }
        line += len + 1;
    }
    //  SetupInfo.SpaceRequired += 10000; // uninstall log :-))) to je prasarna, co
    *from = 0;
    *to = 0;
}

#define LT_NONE 0
#define LT_DESKTOP 1
#define LT_STARTMENU 2
#define LT_QUICKLAUNCH 4

int GetLineType(const char* line)
{
    const char* p = line;
    while (*p != 0)
    {
        if (*p == '%')
        {
            if (*(p + 1) == '5')
                return LT_DESKTOP;
            if (*(p + 1) == '7')
                return LT_STARTMENU;
            if (*(p + 1) == '8')
                return LT_QUICKLAUNCH;
        }
        p++;
    }
    return LT_NONE;
}

void ExtractShortcutSection()
{
    char* line;

    SetupInfo.DesktopPresent = FALSE;
    SetupInfo.StartMenuPresent = FALSE;

    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_SHORTCUTSECTION, SetupInfo.ShortcutSection, sizeof(SetupInfo.ShortcutSection));
    line = SetupInfo.ShortcutSection;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0 && line[0] != ';')
        {
            int type = GetLineType(line);
            if (type == LT_DESKTOP)
                SetupInfo.DesktopPresent = TRUE;
            if (type == LT_STARTMENU)
                SetupInfo.StartMenuPresent = TRUE;
        }
        line += len + 1;
    }
}

HRESULT
CreateShortCut(LPCSTR pszShortcutFile, LPSTR pszLink, LPSTR pszDesc)
{
    HRESULT hres;
    IShellLink* psl;

    hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                            &IID_IShellLink, (void**)&psl);
    if (SUCCEEDED(hres))
    {
        IPersistFile* ppf;
        hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hres))
        {
            WORD wsz[MAX_PATH]; // buffer for Unicode string
            hres = psl->lpVtbl->SetPath(psl, pszShortcutFile);
            hres = psl->lpVtbl->SetDescription(psl, pszDesc);

            MultiByteToWideChar(CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);

            hres = ppf->lpVtbl->Save(ppf, wsz, TRUE);
            if (SUCCEEDED(hres))
                RemoveAddLine(SetupInfo.RemoveFiles, pszLink);

            if (!SUCCEEDED(hres))
            {
            }

            ppf->lpVtbl->Release(ppf);
        }
        psl->lpVtbl->Release(psl);
    }
    return hres;
}

//void
//PinToTaskbar(const char *pszLink)
//{
//  if (InvokeCmdFromContextMenu(HWizardDialog, pszLink, "taskbarpin"))
//    RemoveAddLine(SetupInfo.UnpinFromTaskbar, pszLink);
//}

BOOL CreateShortcuts()
{
    HRESULT hres;
    char* line;
    char src[MAX_PATH];
    char dst[MAX_PATH];
    char des[MAX_PATH];

    if (!SetupInfo.DesktopPresent && !SetupInfo.StartMenuPresent)
        return TRUE;

    OleInitialize(NULL);

    line = SetupInfo.ShortcutSection;
    while (*line != 0)
    {
        char* begin;
        char* end;

        int len = lstrlen(line);
        int type = GetLineType(line);

        if (!(type == LT_DESKTOP && !SetupInfo.ShortcutInDesktop ||
              type == LT_STARTMENU && !SetupInfo.ShortcutInStartMenu ||
              type == LT_QUICKLAUNCH))
        {
            src[0] = 0;
            dst[0] = 0;
            des[0] = 0;

            begin = end = line;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(src, begin, (int)(end - begin + 1));

            if (*end == ',')
                end++;

            begin = end;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(dst, begin, (int)(end - begin + 1));

            if (*end == ',')
                end++;

            lstrcpyn(des, end, MAX_PATH);

            ExpandPath(src);
            ExpandPath(dst);
            ExpandPath(des);

            hres = CreateShortCut(src, dst, des);
            if (hres != ERROR_SUCCESS)
            {
                char buff[200 + MAX_PATH];
                // nepovedlo se - ohlasime to uzivateli, at vi na cem je
                wsprintf(buff, LoadStr(ERROR_CREATESHORTCUT), hres, dst);
                HandleErrorM(IDS_MAINWINDOWTITLE, buff, hres == 0x80004005 ? ERROR_ACCESS_DENIED : hres);
            }
            else
            {
                //if (SetupInfo.PinToTaskbar && type == LT_STARTMENU)
                //  PinToTaskbar(dst);
            }
        }
        line += len + 1;
    }

    OleUninitialize();

    return TRUE;
}

void ExtractCreateDirsSection()
{
    char* line;

    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_CREATEDIRSSECTION, SetupInfo.CreateDirsSection, sizeof(SetupInfo.CreateDirsSection));
    line = SetupInfo.CreateDirsSection;

    SetupInfo.CreateDirsCount = 0;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0 && line[0] != ';')
        {
            SetupInfo.CreateDirsCount++;
        }
        line += len + 1;
    }
}

BOOL CreateDirs()
{
    char* line;
    char dir[MAX_PATH];

    line = SetupInfo.CreateDirsSection;
    while (*line != 0)
    {
        int len = lstrlen(line);

        int type = GetLineType(line);
        if (!(type == LT_DESKTOP && !SetupInfo.ShortcutInDesktop ||
              type == LT_STARTMENU && !SetupInfo.ShortcutInStartMenu ||
              type == LT_QUICKLAUNCH))
        {
            lstrcpy(dir, line);
            ExpandPath(dir);

            if (!CheckAndCreateDirectory(dir))
                return FALSE;
            RemoveAddLine(SetupInfo.RemoveDirs, dir);
        }

        line += len + 1;
    }

    return TRUE;
}

HKEY GetRootHandle(const char* root)
{
    if (lstrcmpi(root, "HKLM") == 0)
        return HKEY_LOCAL_MACHINE;
    if (lstrcmpi(root, "HKCU") == 0)
        return HKEY_CURRENT_USER;
    if (lstrcmpi(root, "HKCC") == 0)
        return HKEY_CURRENT_CONFIG;
    if (lstrcmpi(root, "HKCR") == 0)
        return HKEY_CLASSES_ROOT;
    return NULL;
}

BOOL GetRegistryDWORDValue(const char* line, DWORD* retVal)
{
    HKEY hRoot;
    HKEY hKey;
    const char* begin;
    const char* end;
    char root[MAX_PATH];
    char key[MAX_PATH];
    char value[MAX_PATH];
    BOOL ret = FALSE;

    begin = end = line;

    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(root, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(key, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(value, begin, (int)(end - begin + 1));

    hRoot = GetRootHandle(root);
    if (RegOpenKeyEx(hRoot, key, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD dwDataSize;
        DWORD dwType;
        dwDataSize = sizeof(DWORD);
        dwType = REG_DWORD;
        if (RegQueryValueEx(hKey, value, 0, &dwType, (LPBYTE)retVal, &dwDataSize) == ERROR_SUCCESS)
            ret = TRUE;
        RegDeleteValue(hKey, value);
        RegCloseKey(hKey);
    }
    return ret;
}

BOOL DelRegistryValue(const char* line)
{
    HKEY hRoot;
    HKEY hKey;
    const char* begin;
    const char* end;
    char root[MAX_PATH];
    char key[MAX_PATH];
    char value[MAX_PATH];

    begin = end = line;

    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(root, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(key, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(value, begin, (int)(end - begin + 1));

    hRoot = GetRootHandle(root);
    if (RegOpenKeyEx(hRoot, key, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
    {
        RegDeleteValue(hKey, value);
        RegCloseKey(hKey);
    }
    return TRUE;
}

BOOL DelRegistryValues()
{
    char* line;
    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_DELREGVALUES, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0)
            DelRegistryValue(line);
        line += len + 1;
    }
    return TRUE;
}

//************************************************************************************
//
// RemoveAllSubKeys
//

BOOL RemoveAllSubKeys(HKEY hKey)
{
    char buff[MAX_PATH];
    while (RegEnumKey(hKey, 0, buff, MAX_PATH) != ERROR_NO_MORE_ITEMS)
    {
        HKEY hSubKey;
        if (RegOpenKey(hKey, buff, &hSubKey) == ERROR_SUCCESS)
        {
            RemoveAllSubKeys(hSubKey);
            RegCloseKey(hSubKey);
            RegDeleteKey(hKey, buff);
        }
    }
    return TRUE;
}

//************************************************************************************
//
// DelRegistryKey
//

BOOL DelRegistryKey(const char* line)
{
    HKEY hSubKey;
    HKEY hRoot;
    const char* begin;
    const char* end;
    char root[MAX_PATH];
    char key[MAX_PATH];

    begin = end = line;

    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(root, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(key, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    hRoot = GetRootHandle(root);
    if (RegOpenKey(hRoot, key, &hSubKey) == ERROR_SUCCESS)
    {
        RemoveAllSubKeys(hSubKey);
        RegCloseKey(hSubKey);
    }
    RegDeleteKey(hRoot, key);
    return TRUE;
}

//************************************************************************************
//
// DelRegistryKeys
//

BOOL DelRegistryKeys()
{
    char* line;
    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_DELREGKEYS, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0)
            DelRegistryKey(line);
        line += len + 1;
    }
    return TRUE;
}

//************************************************************************************
//
// DelFiles
//

BOOL DelFiles()
{
    char* line;
    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_DELFILES, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0)
        {
            char buff[MAX_PATH];
            lstrcpyn(buff, line, MAX_PATH);
            ExpandPath(buff);
            DeleteFile(buff);
        }
        line += len + 1;
    }
    return TRUE;
}

//************************************************************************************
//
// DelEmptyDirs
//

BOOL DelEmptyDirs()
{
    char* line;
    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_DELEMPTYDIRS, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0)
        {
            char buff[MAX_PATH];
            lstrcpyn(buff, line, MAX_PATH);
            ExpandPath(buff);
            RemoveDirectory(buff);
        }
        line += len + 1;
    }
    return TRUE;
}

//************************************************************************************
//
// CreateRegistryKeys
//

BOOL CreateRegistryKeys()
{
    char* line;
    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_CREATEREGKEYS, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0)
            RemoveAddLine(SetupInfo.RemoveRegKeys, line);
        line += len + 1;
    }
    return TRUE;
}

BOOL AddRegistryValues()
{
    LONG ret;
    char* line;
    char root[MAX_PATH];
    char key[MAX_PATH];
    char valueName[MAX_PATH];
    char type[100];
    DWORD typeNum;
    char value[MAX_PATH];
    char log[1024];

    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_CREATEREGVALUES, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;
    while (*line != 0)
    {
        HKEY hRoot;
        HKEY hKey;
        DWORD dwType;
        char* begin;
        char* end;

        int len = lstrlen(line);
        if (len > 0 && line[0] != ';')
        {
            root[0] = 0;
            key[0] = 0;
            type[0] = 0;
            valueName[0] = 0;
            value[0] = 0;

            begin = end = line;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(root, begin, (int)(end - begin + 1));
            if (*end == ',')
                end++;

            begin = end;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(key, begin, (int)(end - begin + 1));
            if (*end == ',')
                end++;

            begin = end;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(valueName, begin, (int)(end - begin + 1));
            if (*end == ',')
                end++;

            begin = end;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(type, begin, (int)(end - begin + 1));
            typeNum = MyStrToDWORD(type);
            if (*end == ',')
                end++;

            begin = end;
            while (*end != 0 && *end != ',')
                end++;
            lstrcpyn(value, begin, (int)(end - begin + 1));

            if (typeNum == 1 || typeNum == 2 || typeNum == 101 || typeNum == 102)
            {
                // typy do 100 se zapisuji do remove logu, nad sto ne
                BOOL writeToRemoveLog = (typeNum < 100);
                if (typeNum >= 100)
                    typeNum -= 100;

                hRoot = GetRootHandle(root);
                ret = RegCreateKey(hRoot, key, &hKey);
                if (ret != ERROR_SUCCESS)
                {
                }

                ExpandPath(value);

                if (typeNum == 1) // umime pouze typ REG_SZ(1) a REG_DWORD(2)
                {
                    dwType = REG_SZ;
                    ret = RegSetValueEx(hKey, valueName, 0, dwType, value, lstrlen(value));
                }
                else
                {
                    DWORD valueNum = MyStrToDWORD(value);
                    dwType = REG_DWORD;
                    ret = RegSetValueEx(hKey, valueName, 0, dwType, (const BYTE*)&valueNum, sizeof(valueNum));
                }
                if (ret != ERROR_SUCCESS)
                {
                    RegCloseKey(hKey);
                    return FALSE;
                }
                if (writeToRemoveLog)
                {
                    wsprintf(log, "%s,%s,%s", root, key, valueName);
                    RemoveAddLine(SetupInfo.RemoveRegValues, log);
                    RegCloseKey(hKey);
                }
            }
        }
        line += len + 1;
    }

    return TRUE;
}

int GetEXENameLen(const char* buff)
{
    const char* p = buff;
    while (*p != 0 && *p != ',')
        p++;
    return (int)(p - buff);
}

BOOL IsWow64()
{
    typedef BOOL(WINAPI * LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process;
    BOOL bIsWow64 = FALSE;
    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process");
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
        {
            //handle error
        }
    }
    return bIsWow64;
}

void AddWERLocalDump(const char* exeName)
{
    // Pokud padne x86 aplikace pod x64 windows, pouziji se pro WER zaznamy z x64 Registry.
    // Klic HKEY_LOCAL_MACHINE\SOFTWARE podleha Registry Redirectoru, takze x86 Setup vidi x86 verzi,
    // zatimco x64 Setup vidi x64 verzi. My z x86 verze Setup.exe potrebujeme zapisovat do x64 vetve
    // Registry, coz lze zaridit pomoci specialniho klice KEY_WOW64_64KEY, viz MSDN:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129%28v=vs.85%29.aspx
    // Pozor, obchazeni redirectoru se tyka i modulu remove.c!
    HKEY hKey;
    DWORD dwDisposition;
    DWORD altapRefCount = 0;
    REGSAM samDesired = 0;
    if (IsWow64())
        samDesired = KEY_WOW64_64KEY;
    RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps", 0, NULL, REG_OPTION_NON_VOLATILE, samDesired | KEY_READ | KEY_WRITE, NULL, &hKey, NULL);
    if (hKey != NULL)
    {
        HKEY hExeKey;
        // otevreme nebo vytvorime klic s nazvem EXE
        if (RegCreateKeyEx(hKey, exeName, 0, NULL, REG_OPTION_NON_VOLATILE, samDesired | KEY_READ | KEY_WRITE, NULL, &hExeKey, &dwDisposition) == ERROR_SUCCESS)
        {
            DWORD dwType;
            DWORD dumpCount;
            DWORD dumpType;
            DWORD customDumpFlags;
            const char* dumpFolder = "%LOCALAPPDATA%\\Open Salamander";
            // pokud klic jiz existoval, nacteme pocet referenci
            if (dwDisposition == REG_OPENED_EXISTING_KEY)
            {
                DWORD size = sizeof(altapRefCount);
                dwType = REG_DWORD;
                if (RegQueryValueEx(hExeKey, "AltapRefCount", 0, &dwType, (BYTE*)&altapRefCount, &size) != ERROR_SUCCESS || dwType != REG_DWORD)
                    altapRefCount = 0;
            }
            altapRefCount++;

            dwType = REG_DWORD;
            RegSetValueEx(hExeKey, "AltapRefCount", 0, dwType, (const BYTE*)&altapRefCount, sizeof(altapRefCount));
            dumpCount = 50;
            RegSetValueEx(hExeKey, "DumpCount", 0, dwType, (const BYTE*)&dumpCount, sizeof(dumpCount));
            dumpType = 0; // custom dump type
            RegSetValueEx(hExeKey, "DumpType", 0, dwType, (const BYTE*)&dumpType, sizeof(dumpType));
            customDumpFlags = 0x21921; // flagy MINIDUMP_TYPE
            RegSetValueEx(hExeKey, "CustomDumpFlags", 0, dwType, (const BYTE*)&customDumpFlags, sizeof(customDumpFlags));
            dwType = REG_EXPAND_SZ;
            RegSetValueEx(hExeKey, "DumpFolder", 0, dwType, (const BYTE*)dumpFolder, (DWORD)lstrlen(dumpFolder));
            RegCloseKey(hExeKey);
        }
        RegCloseKey(hKey);
    }
}

void AddWERLocalDumps()
{
    // WER Local Dumps jsou podporovany od Windows Vista, viz
    // http://msdn.microsoft.com/en-us/library/windows/desktop/bb787181%28v=vs.85%29.aspx
    // x64 Salamander ma x64 instalak/odinstalak, takze se bude zapisovat do spravnych klicu
    char exeName[MAX_PATH];
    const char* names = SetupInfo.WERLocalDumps;
    while (*names != 0)
    {
        int exeNameLen = GetEXENameLen(names);
        if (exeNameLen > 0 && exeNameLen < MAX_PATH - 1)
        {
            lstrcpyn(exeName, names, exeNameLen + 1);
            AddWERLocalDump(exeName);
        }
        names += exeNameLen;
        if (*names == ',')
            names++;
    }
}

#define VALUE_LAST_DIRECTORY "LastDirectory"

// ulozi adresar, kam byl Salamander instalovan pro pristi instalaci
BOOL SaveLastDirectory()
{
    char root[MAX_PATH];
    char key[MAX_PATH];
    char value[MAX_PATH];
    HKEY hRoot;
    HKEY hKey;
    char* begin;
    char* end;
    DWORD dwType;
    LONG ret;

    if (SetupInfo.LastDirectories[0] == 0)
        return TRUE;

    root[0] = 0;
    key[0] = 0;

    begin = end = SetupInfo.LastDirectories;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(root, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(key, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    hRoot = GetRootHandle(root);
    ret = RegCreateKey(hRoot, key, &hKey);
    if (ret == ERROR_SUCCESS)
    {
        lstrcpy(value, SetupInfo.DefaultDirectory);

        dwType = REG_SZ; // umime pouze typ 1
        ret = RegSetValueEx(hKey, VALUE_LAST_DIRECTORY, 0, dwType, value, lstrlen(value));
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return FALSE;
        }
        RegCloseKey(hKey);
    }
    return TRUE;
}

// vytahne z registry LastDir
BOOL LoadLastDirectory()
{
    char root[MAX_PATH];
    char key[MAX_PATH];
    char* keyLastComponent;
    char value[MAX_PATH];
    HKEY hRoot;
    HKEY hKey;
    char* begin;
    char* end;
    DWORD dwType;
    DWORD dwDataSize;
    LONG ret;
    BOOL exit;

    if (SetupInfo.LastDirectories[0] == 0)
        return FALSE;

    root[0] = 0;
    key[0] = 0;
    value[0] = 0;

    // vytahnu root
    begin = end = SetupInfo.LastDirectories;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(root, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    // vytahnu cestu ke klici platnemu pro prave instalovanou verzi
    begin = end;
    keyLastComponent = NULL; // bude ukazovat za posledni zpetne lomitko, kam potom budeme vkladat starsi kandidaty ze seznamu, pokud ten aktualni selze
    while (*end != 0 && *end != ',')
    {
        if (*end == '\\')
            keyLastComponent = key + (end - begin + 1);
        end++;
    }
    lstrcpyn(key, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    // pokud jsme nenasli posledni komponentu, je neco spatne a je cas zmizet
    if (keyLastComponent == NULL)
        return FALSE;

    hRoot = GetRootHandle(root);

    exit = FALSE;
    do
    {
        // otevreme klic
        ret = RegOpenKeyEx(hRoot, key, 0, KEY_QUERY_VALUE, &hKey);
        if (ret == ERROR_SUCCESS)
        {
            // a v nem se pokusime nacist REG_SZ retezec "LastDirectory"
            dwType = REG_SZ; // umime pouze typ 1
            dwDataSize = MAX_PATH;
            ret = RegQueryValueEx(hKey, VALUE_LAST_DIRECTORY, 0, &dwType, value, &dwDataSize);
            RegCloseKey(hKey);
            if (ret == ERROR_SUCCESS && dwType == REG_SZ && value[0] != 0)
            {
                // mame kandidata na adresar, kam bychom se mohli tlacit
                // overime, zda splnuje dalsi podminky
                if (ContainsPathUpgradableVersion(value))
                {
                    lstrcpy(SetupInfo.DefaultDirectory, value);
                    return TRUE;
                }
            }
        }

        // pro aktualni verzi jsme nenasli vyhovujici adresar, takze zkusime
        // starsi verze dle seznamu
        begin = end;
        while (*end != 0 && *end != ',')
            end++;
        if (*begin != 0)
        {
            lstrcpyn(keyLastComponent, begin, (int)(end - begin + 1));
            if (*end == ',')
                end++;
        }
        else
            exit = TRUE;
    } while (!exit);

    return FALSE;
}

const char* REMOVE_OPTIONS = "[Options]\r\n";
const char* REMOVE_APPNAME = "UninstallApplication=";
const char* REMOVE_WERLOCALDUMPS = "UninstallWERLocalDumps=";
const char* REMOVE_RUNQUIET = "RunProgramQuiet=";
const char* REMOVE_CHECKRUNNING = "CheckRunningApps=";
const char* REMOVE_UNPINFROMTASKBAR = "[UnpinFromTaskbar]\r\n";
const char* REMOVE_DELREGKEYS = "[DelRegKeys]\r\n";
const char* REMOVE_DELREGVALS = "[DelRegValues]\r\n";
const char* REMOVE_DELFILES = "[DelFiles]\r\n";
const char* REMOVE_DELDIRS = "[DelDirs]\r\n";
const char* REMOVE_OPTIONS2 = "Options";
const char* REMOVE_DELREGKEYS2 = "DelRegKeys";
const char* REMOVE_DELREGVALS2 = "DelRegValues";
const char* REMOVE_DELFILES2 = "DelFiles";
const char* REMOVE_DELDIRS2 = "DelDirs";
const char* REMOVE_DELSHELLEXTS = "[DelShellExts]\r\n";
const char* REMOVE_NEXTLINE = "\r\n";

// zapise seznam retezcu oddelenych nulama a zakoncenych dvouma nulama
// do souboru; radek zakoncuje \r\n
void WriteList(HANDLE hFile, const char* list, BOOL expand)
{
    char buff[5000];
    DWORD written;
    const char* p = list;
    lstrcpy(buff, "\r\n");
    while (*p != 0)
    {
        lstrcpyn(buff, p, 4990);
        if (expand)
            ExpandPath(buff);
        lstrcat(buff, "\r\n");
        WriteFile(hFile, buff, lstrlen(buff), &written, NULL);
        p += lstrlen(p) + 1;
    }
}

BOOL SaveRemoveLog()
{
    HANDLE hFile;
    char buff[300];
    DWORD written;
    if (SetupInfo.SaveRemoveLog[0] == 0)
        return TRUE;

    hFile = CreateFile(SetupInfo.SaveRemoveLog, GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        if (!SetupInfo.Silent)
        {
            wsprintf(buff, LoadStr(IDS_ERRORCREATELOG), SetupInfo.SaveRemoveLog);
            MessageBox(HWizardDialog, buff, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }

    WriteFile(hFile, REMOVE_OPTIONS,
              lstrlen(REMOVE_OPTIONS), &written, NULL);

    if (SetupInfo.LoadOldRemoveLog)
    {
        char* p = SetupInfo.OldRemoveOptions;
        lstrcpy(buff, "\r\n");
        while (*p != 0)
        {
            int len = lstrlen(p);
            WriteFile(hFile, p, len, &written, NULL);
            WriteFile(hFile, buff, 2, &written, NULL);
            p += len + 1;
        }
    }
    else
    {
        lstrcpy(buff, REMOVE_APPNAME);
        lstrcat(buff, SetupInfo.ApplicationNameVer);
        lstrcat(buff, "\r\n");
        WriteFile(hFile, buff, lstrlen(buff), &written, NULL);
        if (SetupInfo.UnistallRunProgramQuietPath[0] != 0)
        {
            lstrcpy(buff, REMOVE_RUNQUIET);
            lstrcat(buff, SetupInfo.UnistallRunProgramQuietPath);
            lstrcat(buff, "\r\n");
            WriteFile(hFile, buff, lstrlen(buff), &written, NULL);
        }
        if (SetupInfo.CheckRunningApps[0] != 0)
        {
            lstrcpy(buff, REMOVE_CHECKRUNNING);
            lstrcat(buff, SetupInfo.CheckRunningApps);
            lstrcat(buff, "\r\n");
            WriteFile(hFile, buff, lstrlen(buff), &written, NULL);
        }
        if (SetupInfo.WERLocalDumps[0] != 0)
        {
            lstrcpy(buff, REMOVE_WERLOCALDUMPS);
            lstrcat(buff, SetupInfo.WERLocalDumps);
            lstrcat(buff, "\r\n");
            WriteFile(hFile, buff, lstrlen(buff), &written, NULL);
        }
    }

    WriteFile(hFile, REMOVE_NEXTLINE,
              lstrlen(REMOVE_NEXTLINE), &written, NULL);

    RemoveAddLine(SetupInfo.RemoveFiles, SetupInfo.SaveRemoveLog);

    // Delete Files
    if (SetupInfo.UnpinFromTaskbar[0] != 0)
    {
        WriteFile(hFile, REMOVE_UNPINFROMTASKBAR,
                  lstrlen(REMOVE_UNPINFROMTASKBAR), &written, NULL);
        WriteList(hFile, SetupInfo.UnpinFromTaskbar, FALSE);
        WriteFile(hFile, REMOVE_NEXTLINE,
                  lstrlen(REMOVE_NEXTLINE), &written, NULL);
    }

    // Delete Files
    if (SetupInfo.RemoveFiles[0] != 0)
    {
        WriteFile(hFile, REMOVE_DELFILES,
                  lstrlen(REMOVE_DELFILES), &written, NULL);
        WriteList(hFile, SetupInfo.RemoveFiles, FALSE);
        WriteFile(hFile, REMOVE_NEXTLINE,
                  lstrlen(REMOVE_NEXTLINE), &written, NULL);
    }

    // Delete Dirs
    if (SetupInfo.RemoveDirs[0] != 0)
    {
        WriteFile(hFile, REMOVE_DELDIRS,
                  lstrlen(REMOVE_DELDIRS), &written, NULL);
        WriteList(hFile, SetupInfo.RemoveDirs, FALSE);
        WriteFile(hFile, REMOVE_NEXTLINE,
                  lstrlen(REMOVE_NEXTLINE), &written, NULL);
    }

    // Delete Reg Values
    if (SetupInfo.RemoveRegValues[0] != 0)
    {
        WriteFile(hFile, REMOVE_DELREGVALS,
                  lstrlen(REMOVE_DELREGVALS), &written, NULL);
        WriteList(hFile, SetupInfo.RemoveRegValues, FALSE);
        WriteFile(hFile, REMOVE_NEXTLINE,
                  lstrlen(REMOVE_NEXTLINE), &written, NULL);
    }

    // Delete Reg Keys
    if (SetupInfo.RemoveRegKeys[0] != 0)
    {
        WriteFile(hFile, REMOVE_DELREGKEYS,
                  lstrlen(REMOVE_DELREGKEYS), &written, NULL);
        WriteList(hFile, SetupInfo.RemoveRegKeys, FALSE);
        WriteFile(hFile, REMOVE_NEXTLINE,
                  lstrlen(REMOVE_NEXTLINE), &written, NULL);
    }

    // Delete Shell Extensions
    if (SetupInfo.RemoveShellExts[0] != 0)
    {
        WriteFile(hFile, REMOVE_DELSHELLEXTS,
                  lstrlen(REMOVE_DELSHELLEXTS), &written, NULL);
        WriteList(hFile, SetupInfo.RemoveShellExts, TRUE);
        WriteFile(hFile, REMOVE_NEXTLINE,
                  lstrlen(REMOVE_NEXTLINE), &written, NULL);
    }

    CloseHandle(hFile);
    return TRUE;
}

BOOL LoadRemoveLog()
{
    HANDLE hFile;
    char removeLog[100000];
    DWORD read;
    if (SetupInfo.SaveRemoveLog[0] == 0)
        return TRUE;

    hFile = CreateFile(SetupInfo.SaveRemoveLog, GENERIC_READ, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    if (!ReadFile(hFile, removeLog, 100000, &read, NULL))
    {
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);

    if (read > 99999)
        read = 99999;
    removeLog[read] = 0;

    MyGetPrivateProfileSection(removeLog, REMOVE_OPTIONS2, SetupInfo.OldRemoveOptions, sizeof(SetupInfo.OldRemoveOptions));

    // nacucneme seznamy k podrezani
    MyGetPrivateProfileSection(removeLog, REMOVE_UNPINFROMTASKBAR, SetupInfo.UnpinFromTaskbar, sizeof(SetupInfo.UnpinFromTaskbar));
    MyGetPrivateProfileSection(removeLog, REMOVE_DELFILES2, SetupInfo.RemoveFiles, sizeof(SetupInfo.RemoveFiles));
    MyGetPrivateProfileSection(removeLog, REMOVE_DELDIRS2, SetupInfo.RemoveDirs, sizeof(SetupInfo.RemoveDirs));
    MyGetPrivateProfileSection(removeLog, REMOVE_DELREGVALS2, SetupInfo.RemoveRegValues, sizeof(SetupInfo.RemoveRegValues));
    MyGetPrivateProfileSection(removeLog, REMOVE_DELREGKEYS2, SetupInfo.RemoveRegKeys, sizeof(SetupInfo.RemoveRegKeys));
    MyGetPrivateProfileSection(removeLog, REMOVE_DELSHELLEXTS, SetupInfo.RemoveShellExts, sizeof(SetupInfo.RemoveShellExts));

    return TRUE;
}

BOOL DoGetRegistryVarSectionLine(char* line)
{
    char pathVar[MAX_PATH];
    char root[MAX_PATH];
    char key[MAX_PATH];
    char valueName[MAX_PATH];
    char value[MAX_PATH];
    char error[1024];
    char* begin;
    char* end;

    HKEY hRoot;
    HKEY hKey;
    DWORD dwType;
    DWORD dwDataSize;
    LONG ret;

    pathVar[0] = 0;
    root[0] = 0;
    key[0] = 0;
    valueName[0] = 0;
    value[0] = 0;
    error[0] = 0;

    begin = end = line;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(pathVar, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(root, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(key, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(valueName, begin, (int)(end - begin + 1));
    if (*end == ',')
        end++;

    begin = end;
    while (*end != 0 && *end != ',')
        end++;
    lstrcpyn(error, begin, (int)(end - begin + 1));

    hRoot = GetRootHandle(root);
    ret = RegOpenKeyEx(hRoot, key, 0, KEY_QUERY_VALUE, &hKey);
    if (ret == ERROR_SUCCESS)
    {
        dwType = REG_SZ; // umime pouze typ 1
        dwDataSize = MAX_PATH;
        ret = RegQueryValueEx(hKey, valueName, 0, &dwType, value, &dwDataSize);
        RegCloseKey(hKey);
        if (ret == ERROR_SUCCESS)
        {
            if (pathVar[1] >= 'a' && pathVar[1] <= 'z')
                pathVar[1] = 'A' + pathVar[1] - 'a';
            if (pathVar[0] != '%' || pathVar[1] < 'A' || pathVar[1] > 'Z')
            {
                if (!SetupInfo.Silent)
                    MessageBox(NULL, "Syntax error in [GetRegistryVar]", MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }

            // upalime zpetne lomitko na konci, pokud tam je
            if (lstrlen(value) > 1 && value[lstrlen(value) - 1] == '\\')
                value[lstrlen(value) - 1] = 0;

            lstrcpyn(SetupInfo.RegPathVal[pathVar[1] - 'A'], value, MAX_PATH);
            return TRUE;
        }
    }
    if (!SetupInfo.Silent)
        MessageBox(NULL, error, MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
    return FALSE;
}

BOOL DoGetRegistryVarSection()
{
    char* line;

    SetupInfo.TmpSection[0] = 0;
    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_GETREGVAR, SetupInfo.TmpSection, sizeof(SetupInfo.TmpSection));
    line = SetupInfo.TmpSection;

    while (*line != 0)
    {
        int len = lstrlen(line);
        if (len > 0 && line[0] != ';')
        {
            if (!DoGetRegistryVarSectionLine(line))
                return FALSE;
        }
        line += len + 1;
    }
    return TRUE;
}

BOOL FindConflictWithAnotherVersion(BOOL* sameOrOlderVersion, BOOL* sameVersion, /*BOOL *sameOrOlderVersionIgnoringBuild, BOOL *sameVersionIgnoringBuild, */ BOOL* foundRemoveLog)
{
    int i;
    char src[MAX_PATH];
    char dst[MAX_PATH];
    char* from;
    char* to;

    from = SetupInfo.CopyFrom;
    to = SetupInfo.CopyTo;
    for (i = 0; i < SetupInfo.CopyCount; i++)
    {
        DWORD flags;

        lstrcpy(src, from);
        lstrcpy(dst, to);

        ExpandPath(src);
        ExpandPath(dst);

        src[lstrlen(src) + 1] = 0;
        dst[lstrlen(dst) + 1] = 0;

        flags = SetupInfo.CopyFlags[i];
        if (flags & COPY_FLAG_TEST_CONFLICT) // mame modul testovat na existenci jine verze?
        {
            DWORD srcMajor, srcMinor;
            DWORD dstMajor, dstMinor;
            HINSTANCE hSrc, hDst;

            // pokusim se oba moduly nacist
            hSrc = LoadLibraryEx(src, NULL, LOAD_LIBRARY_AS_DATAFILE);
            hDst = LoadLibraryEx(dst, NULL, LOAD_LIBRARY_AS_DATAFILE);
            if (hSrc != NULL && hDst != NULL)
            {
                if (GetModuleVersion(hSrc, &srcMajor, &srcMinor) && GetModuleVersion(hDst, &dstMajor, &dstMinor))
                {
                    // including the build WORD
                    if (sameOrOlderVersion != NULL)
                    {
                        *sameOrOlderVersion = (dstMajor == srcMajor && dstMinor == srcMinor) ||
                                              (dstMajor < srcMajor) || (dstMajor == srcMajor && dstMinor < srcMinor);
                    }
                    if (sameVersion != NULL)
                    {
                        *sameVersion = (dstMajor == srcMajor && dstMinor == srcMinor);
                    }

                    /* pri releasu 2.52 beta 2 jsme zjistili, ze nemuzeme ignorovat build number, instalak nam pri upgradu z beta 1 hlasil, ze aplikace se stejnou verzi je jiz nainstalovana 
          // ignoring the build WORD
          dstMinor &= 0xffff0000;
          srcMinor &= 0xffff0000;
          if (sameOrOlderVersionIgnoringBuild != NULL)
          {
            *sameOrOlderVersionIgnoringBuild = (dstMajor == srcMajor && dstMinor == srcMinor) || 
                                               (dstMajor < srcMajor) || (dstMajor == srcMajor && dstMinor < srcMinor);
          }
          if (sameVersionIgnoringBuild != NULL)
          {
            *sameVersionIgnoringBuild = (dstMajor == srcMajor && dstMinor == srcMinor);
          }
*/
                    FreeLibrary(hSrc);
                    FreeLibrary(hDst);

                    if (foundRemoveLog != NULL)
                    {
                        char removeLog[MAX_PATH];
                        DWORD attrs;
                        lstrcpy(removeLog, SetupInfo.SaveRemoveLog);
                        ExpandPath(removeLog);
                        attrs = GetFileAttributes(removeLog);
                        *foundRemoveLog = (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
                    }

                    return TRUE;
                }
            }
            if (hSrc != NULL)
                FreeLibrary(hSrc);
            if (hDst != NULL)
                FreeLibrary(hDst);
        }

        from += lstrlen(from) + 1;
        to += lstrlen(to) + 1;
    }
    return FALSE;
}

// proveri danou cestu, zda obsahuje "salamand.exe" a "remove.rlg" -- dva soubory nezbytne pro pripadny upgrade
BOOL ContainsPathUpgradableVersion(const char* path)
{
    char backupDefaultDirectory[MAX_PATH];
    BOOL foundAnotherEXEVersion;
    BOOL foundRemoveLog;
    BOOL sameOrOlderVersion;

    // cesta musi existovat
    DWORD attrs = GetFileAttributes(path);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
        return FALSE;

    // zaloha globalky
    lstrcpy(backupDefaultDirectory, SetupInfo.DefaultDirectory);
    // globalku musim nastavit, aby se sprave expandovaly cesty
    lstrcpy(SetupInfo.DefaultDirectory, path);

    // overim, zda je v adresari pritomna starsi nebo shodna verze EXE
    foundAnotherEXEVersion = FindConflictWithAnotherVersion(&sameOrOlderVersion, NULL, &foundRemoveLog);

    // obnova ze zalohy
    lstrcpy(SetupInfo.DefaultDirectory, backupDefaultDirectory);

    if (foundAnotherEXEVersion && sameOrOlderVersion && foundRemoveLog)
        return TRUE;

    return FALSE;
}

BOOL RefreshDesktop(BOOL sleep)
{
    ITEMIDLIST root = {0};
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, &root, 0);
    if (sleep)
        Sleep(500); // dame systemu nejaky cas
    return TRUE;
}

BOOL DoInstallation()
{
    int progress = 0;
    int i;
    char src[MAX_PATH];
    char dst[MAX_PATH];
    char* from;
    char* to;
    int options;
    BOOL overwriteAll;
    BOOL skipAll;
    BOOL skipAllCreate;

    SetupInfo.OldRemoveOptions[0] = 0;
    SetupInfo.OldRemoveOptions[1] = 0;
    SetupInfo.UnpinFromTaskbar[0] = 0;
    SetupInfo.UnpinFromTaskbar[1] = 0;
    SetupInfo.RemoveFiles[0] = 0;
    SetupInfo.RemoveFiles[1] = 0;
    SetupInfo.RemoveDirs[0] = 0;
    SetupInfo.RemoveDirs[1] = 0;
    SetupInfo.RemoveRegValues[0] = 0;
    SetupInfo.RemoveRegValues[1] = 0;
    SetupInfo.RemoveRegKeys[0] = 0;
    SetupInfo.RemoveRegKeys[1] = 0;

    ExpandPath(SetupInfo.SaveRemoveLog);
    if (SetupInfo.LoadOldRemoveLog)
        LoadRemoveLog();

    SetProgressMax(SetupInfo.CopyCount - 1 + 11);
    SetProgressPos(progress++);

    if (!CreateDirs()) // 1
        return FALSE;
    SetProgressPos(progress++);

    from = SetupInfo.CopyFrom;
    to = SetupInfo.CopyTo;

    SetFromTo("", "");

    overwriteAll = FALSE;
    skipAll = FALSE;
    skipAllCreate = FALSE;

    for (i = 0; i < SetupInfo.CopyCount; i++)
    {
        char dir[MAX_PATH];
        char* iter;
        DWORD flags;

        lstrcpy(src, from);
        lstrcpy(dst, to);

        ExpandPath(src);
        ExpandPath(dst);

        src[lstrlen(src) + 1] = 0;
        dst[lstrlen(dst) + 1] = 0;

        SetFromTo(src, dst);
        if (!SetupInfo.Silent)
            UpdateWindow(HProgressDialog);

        lstrcpy(dir, dst);
        iter = (char*)MyRStrChr(dir, '\\');
        if (iter != NULL)
        {
            *iter = 0;
            if (!CheckAndCreateDirectory(dir))
                return FALSE;
        }

        flags = SetupInfo.CopyFlags[i];
        if (!MyCopyFile(src, dst, overwriteAll, skipAll, skipAllCreate, &options, flags))
        {
            return FALSE;
        }

        if (options == COPYFILEOPTIONS_CANCEL)
            return FALSE;

        if (options == COPYFILEOPTIONS_SKIPALL)
            skipAll = TRUE;

        if (options == COPYFILEOPTIONS_OVERWRITEALL)
            overwriteAll = TRUE;

        if (options == COPYFILEOPTIONS_SKIPALL_CREATE)
            skipAllCreate = TRUE;

        if (!(flags & COPY_FLAG_NOREMOVE) &&
            (options == COPYFILEOPTIONS_SUCCESS || options == COPYFILEOPTIONS_OVERWRITE ||
             options == COPYFILEOPTIONS_OVERWRITEALL))
            RemoveAddLine(SetupInfo.RemoveFiles, dst);

        from += lstrlen(from) + 1;
        to += lstrlen(to) + 1;

        SetProgressPos(progress++);
        Sleep(0);
    }

    SetProgressPos(progress++);

    SetFromTo("", "");
    DelRegistryValues(); // 3
    SetProgressPos(progress++);
    DelRegistryKeys(); // 4
    SetProgressPos(progress++);
    DelFiles(); // 5
    SetProgressPos(progress++);
    DelEmptyDirs(); // 6
    SetProgressPos(progress++);
    CreateShortcuts(); // 7
    SetProgressPos(progress++);
    CreateRegistryKeys(); // 8
    SetProgressPos(progress++);
    AddRegistryValues(); // 9
    AddWERLocalDumps();
    SetProgressPos(progress++);
    //  SaveLastDirectory();    // 10 // provedeme uz po spisteni instalace
    //  SetProgressPos(progress++);
    ExpandPath(SetupInfo.UnistallRunProgramQuietPath);

    MyGetPrivateProfileSection(SetupInfo.InfFile, INF_DELSHELLEXTS, SetupInfo.RemoveShellExts, sizeof(SetupInfo.RemoveShellExts));

    SaveRemoveLog(); // 10

    if (SetupInfo.IncrementFileContentSrc[0] != 0 && SetupInfo.IncrementFileContentDst[0] != 0)
        IncrementFileContent();

    SetProgressPos(progress++);

    // pod W2K dochazelo k tomu, ze po instalaci Sal2.51 zustala na plose take ikonka Sal2.5 a zmizela az na
    // manualni refresh (F5) desktopu; vlastni link uz byl smazanej, pouze desktop porad zobrazoval ikonu
    RefreshDesktop(FALSE);

    return TRUE;
}
