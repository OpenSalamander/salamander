// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <commctrl.h>
#include <shlobj.h>

#include "utils.h"
#include "process.h"
#include "resource.h"
#include "ctxmenu.h"

#pragma comment(lib, "comctl32.lib")

#ifdef INSIDE_SETUP
#include "..\infinst.h"
#endif //INSIDE_SETUP

extern HINSTANCE HInstance;
extern char ModulePath[MAX_PATH];

char ModuleName[MAX_PATH] = {0};
#ifndef INSIDE_SETUP
HINSTANCE HInstance = NULL;
char ModulePath[MAX_PATH] = {0};
#endif //INSIDE_SETUP
char RemoveInfFileName[MAX_PATH] = {0};
char RemoveAppName[1024] = {0};
char RemoveRunProgramQuiet[1024] = {0};
//char       CheckRunningApps[1024] = {0};
char RemoveWERLocalDumps[5000] = {0};
char InfFileBuffer[500000] = {0};

BOOL QuietMode = FALSE;
BOOL InsideSetupMode = FALSE;
//BOOL       InstallingSameVersion = FALSE;
HWND HParent = NULL; // when called as an EXE or from setup in silent mode, the parent is NULL; otherwise it is the setup dialog

// uninstall scripts
char UnpinFromTaskbar[100000] = {0};
char RemoveFiles[400000] = {0};
char RemoveDirs[100000] = {0};
char RemoveRegValues[100000] = {0};
char RemoveRegKeys[100000] = {0};
char RemoveShellExts[100000] = {0};

extern const char* INF_REMOVE_DELFILES;
extern const char* INF_REMOVE_DELREGKEYS;
extern const char* INF_REMOVE_DELSHELLEXTS;

const char* INF_REMOVE_DELDIRS = "DelDirs";
const char* INF_REMOVE_DELREGVALS = "DelRegValues";
const char* INF_REMOVE_UNPINFROMTASKBAR = "UnpinFromTaskbar";
const char* INF_REMOVE_DELFILES = "DelFiles";
const char* INF_REMOVE_DELREGKEYS = "DelRegKeys";
const char* INF_REMOVE_DELSHELLEXTS = "DelShellExts";

const char* INF_REMOVE_OPTIONS_SECTION = "Options";
const char* INF_REMOVE_OPTIONS_APPNAME = "UninstallApplication";
const char* INF_REMOVE_OPTIONS_RUNPROGRAMQUIET = "RunProgramQuiet";
//const char *INF_OPTIONS_CHECKRUNNINGAPPS = "CheckRunningApps";
const char* INF_REMOVE_OPTIONS_WERLOCALDUMPS = "UninstallWERLocalDumps";

#ifndef INSIDE_SETUP
const char* INF_REMOVE_FILENAME = "\\remove.rlg";
#endif //INSIDE_SETUP

const char* BATCH_FILE_BEGIN = "@echo off\r\n:Repeat\r\ndel \"%s\"\r\nif exist \"%s\" goto Repeat\r\n";
const char* BATCH_FILE_DELDIR = "rmdir \"%s\"\r\n";
const char* BATCH_FILE_DELFILE = "del \"%s\"\r\n";

typedef struct
{
    const char* AppName;
    BOOL RemoveConfiguration;
} CConfirmationParams;

CConfirmationParams ParamsGlobal;

#ifndef INSIDE_SETUP
//************************************************************************************
//
// LoadStr
//

char* LoadStr(int resID)
{
    int size;
    char* ret;
    static char buffer[5000]; // buffer for many strings
    static char* act = buffer;

    if (5000 - (act - buffer) < 200)
        act = buffer;
    size = LoadString(HInstance, resID, act, 5000 - (int)(act - buffer));
    if (size != 0 || GetLastError() == NO_ERROR)
    {
        ret = act;
        act += size + 1;
    }
    else
    {
        ret = "ERROR LOADING STRING";
    }

    return ret;
}
#endif // INSIDE_SETUP

char* GetErrorText(DWORD error)
{
    static char tempErrorText[MAX_PATH + 20];
    int l = wsprintf(tempErrorText, "(%d) ", error);
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      error,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      tempErrorText + l,
                      MAX_PATH + 20 - l,
                      NULL) == 0 ||
        *(tempErrorText + l) == 0)
    {
        wsprintf(tempErrorText, "System error %d, text description is not available.", error);
    }
    return tempErrorText;
}

//************************************************************************************
//
// RemoveOnReboot
//

BOOL RemoveOnReboot(LPCTSTR pszExisting /*, LPCTSTR pszNew*/)
{
    BOOL fOk = 0;
    HMODULE hLib = LoadLibrary("kernel32.dll");
    if (hLib)
    {
        typedef BOOL(WINAPI * mfea_t)(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);
        mfea_t mfea;
        mfea = (mfea_t)GetProcAddress(hLib, "MoveFileExA");
        if (mfea)
        {
            fOk = mfea(pszExisting, NULL /*pszNew*/, MOVEFILE_DELAY_UNTIL_REBOOT | MOVEFILE_REPLACE_EXISTING);
        }
        FreeLibrary(hLib);
    }

    if (!fOk)
    {
        static char szRenameLine[1024];
        static char wininit[1024];
        static char tmpbuf[1024];
        int cchRenameLine;
        char* szRenameSec = "[Rename]\r\n";
        HANDLE hfile, hfilemap;
        DWORD dwFileSize, dwRenameLinePos;

        *((int*)tmpbuf) = *((int*)"NUL");

        //    if (pszNew) {
        //      // create the file if it's not already there to prevent GetShortPathName from failing
        //      CloseHandle(myOpenFile(pszNew, 0, CREATE_NEW));
        //      GetShortPathName(pszNew,tmpbuf,1024);
        //    }
        // wininit is used as a temporary here
        GetShortPathName(pszExisting, wininit, 1024);
        cchRenameLine = wsprintf(szRenameLine, "%s=%s\r\n", tmpbuf, wininit);

        GetFolderPath(CSIDL_WINDOWS, wininit);
        lstrcat(wininit, "\\wininit.ini");
        hfile = CreateFile(wininit,
                           GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

        if (hfile != INVALID_HANDLE_VALUE)
        {
            dwFileSize = GetFileSize(hfile, NULL);
            hfilemap = CreateFileMapping(hfile, NULL, PAGE_READWRITE, 0, dwFileSize + cchRenameLine + 10, NULL);

            if (hfilemap != NULL)
            {
                LPSTR pszWinInit = (LPSTR)MapViewOfFile(hfilemap, FILE_MAP_WRITE, 0, 0, 0);

                if (pszWinInit != NULL)
                {
                    LPSTR pszRenameSecInFile = mystrstr(pszWinInit, szRenameSec);
                    if (pszRenameSecInFile == NULL)
                    {
                        lstrcpy(pszWinInit + dwFileSize, szRenameSec);
                        dwFileSize += 10;
                        dwRenameLinePos = dwFileSize;
                    }
                    else
                    {
                        char* pszFirstRenameLine = pszRenameSecInFile + 10;
                        char* pszNextSec = mystrstr(pszFirstRenameLine, "\n[");
                        if (pszNextSec)
                        {
                            int l = dwFileSize - (int)(pszNextSec - pszWinInit);
                            void* data = (void*)GlobalAlloc(GPTR, l);
                            mini_memcpy(data, pszNextSec, l);
                            mini_memcpy(pszNextSec + cchRenameLine, data, l);
                            GlobalFree((HGLOBAL)data);

                            dwRenameLinePos = (int)(pszNextSec - pszWinInit);
                        }
                        // rename section is last, stick item at end of file
                        else
                            dwRenameLinePos = dwFileSize;
                    }

                    mini_memcpy(&pszWinInit[dwRenameLinePos], szRenameLine, cchRenameLine);
                    dwFileSize += cchRenameLine;

                    UnmapViewOfFile(pszWinInit);

                    fOk++;
                }
                CloseHandle(hfilemap);
            }
            SetFilePointer(hfile, dwFileSize, NULL, FILE_BEGIN);
            SetEndOfFile(hfile);
            CloseHandle(hfile);
        }
    }
    return fOk;
}

//************************************************************************************
//
// RemoveGetPrivateProfileSection
//

BOOL RemoveGetPrivateProfileSection(const char* section, char* buff, int buffMax)
{
    char* p = InfFileBuffer;
    char* dst = buff;
    while (*p != 0)
    {
        while (*p != '[' && *p != 0)
            p++;
        if (*p == '[')
        {
            char* end = p + 1;
            while (*end != ']' && *end != 0)
                end++;
            if (*end == ']')
            {
                char tmp[1024];
                char* src = p + 1;
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

#ifndef INSIDE_SETUP

//************************************************************************************
//
// CenterWindow
//

void CenterWindow(HWND hWindow)
{
    RECT masterRect;
    RECT slaveRect;
    int x, y, w, h;
    int mw, mh;

    masterRect.left = 0;
    masterRect.top = 0;
    masterRect.right = GetSystemMetrics(SM_CXSCREEN);
    masterRect.bottom = GetSystemMetrics(SM_CYSCREEN);

    GetWindowRect(hWindow, &slaveRect);
    w = slaveRect.right - slaveRect.left;
    h = slaveRect.bottom - slaveRect.top;
    mw = masterRect.right - masterRect.left;
    mh = masterRect.bottom - masterRect.top;
    x = masterRect.left + (mw - w) / 2;
    y = masterRect.top + (mh - h) / 2;
    SetWindowPos(hWindow, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

//************************************************************************************
//
// strrchr
//

char* strrchr(const char* string, int c)
{
    char* iter;
    int len;
    if (string == NULL)
        return NULL;
    len = lstrlen(string);
    iter = (char*)string + len - 1;
    while (iter >= string)
    {
        if (*iter == c)
            return iter;
        iter--;
    }
    return NULL;
}

//************************************************************************************
//
// GetCurDispLangID
//

typedef LANGID(WINAPI* FGetUserDefaultUILanguage)();

WORD GetCurDispLangID()
{
    WORD langID;
    HMODULE KERNEL32DLL;

    langID = GetUserDefaultLangID();

    // adjust the langID on newer Windows versions
    KERNEL32DLL = LoadLibrary("kernel32.dll");
    if (KERNEL32DLL != NULL)
    {
        FGetUserDefaultUILanguage proc = (FGetUserDefaultUILanguage)GetProcAddress(KERNEL32DLL, "GetUserDefaultUILanguage");
        if (proc != NULL)
            langID = proc();
        FreeLibrary(KERNEL32DLL);
    }

    return langID;
}

typedef void(WINAPI* PGNSI)(LPSYSTEM_INFO);

INT_PTR CALLBACK ConfirmationDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HICON hIcon;
        char buff1[2 * MAX_PATH];
        char buff2[2 * MAX_PATH];

        // center the dialog on the screen
        CenterWindow(hWindow);

        hIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_QUESTION));
        if (hIcon != NULL)
            SendDlgItemMessage(hWindow, IDC_MYICON, STM_SETICON, (WPARAM)hIcon, 0);

        GetDlgItemText(hWindow, IDC_QUESTION, buff1, 2 * MAX_PATH);
        wsprintf(buff2, buff1, ParamsGlobal.AppName);
        SetDlgItemText(hWindow, IDC_QUESTION, buff2);

        CheckDlgButton(hWindow, IDC_REMOVECONFIGURATION, ParamsGlobal.RemoveConfiguration ? BST_CHECKED : BST_UNCHECKED);

        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDYES:
        {
            ParamsGlobal.RemoveConfiguration = IsDlgButtonChecked(hWindow, IDC_REMOVECONFIGURATION) == BST_CHECKED;
        }
        case IDNO:
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

#endif // INSIDE_SETUP

//************************************************************************************
//
// RemoveRemoveAllSubKeys
//

BOOL RemoveRemoveAllSubKeys(HKEY hKey)
{
    char buff[MAX_PATH];
    while (RegEnumKey(hKey, 0, buff, MAX_PATH) != ERROR_NO_MORE_ITEMS)
    {
        HKEY hSubKey;
        if (RegOpenKey(hKey, buff, &hSubKey) == ERROR_SUCCESS)
        {
            RemoveRemoveAllSubKeys(hSubKey);
            RegCloseKey(hSubKey);
            RegDeleteKey(hKey, buff);
        }
    }
    return TRUE;
}

//************************************************************************************
//
// RemoveKey
//

BOOL RemoveKey(HKEY hKey, const char* key)
{
    HKEY hSubKey;
    if (RegOpenKey(hKey, key, &hSubKey) == ERROR_SUCCESS)
    {
        RemoveRemoveAllSubKeys(hSubKey);
        RegCloseKey(hSubKey);
    }
    RegDeleteKey(hKey, key);
    return TRUE;
}

//************************************************************************************
//
// RemoveOpenInfFile
//

BOOL RemoveOpenInfFile()
{
    int ret;

    // load the basic data
    ret = GetPrivateProfileString(INF_REMOVE_OPTIONS_SECTION, INF_REMOVE_OPTIONS_APPNAME, "",
                                  RemoveAppName, 1024, RemoveInfFileName);

    if (ret == 0)
        return FALSE;

    GetPrivateProfileString(INF_REMOVE_OPTIONS_SECTION, INF_REMOVE_OPTIONS_RUNPROGRAMQUIET, "",
                            RemoveRunProgramQuiet, 1024, RemoveInfFileName);

    //  GetPrivateProfileString(INF_REMOVE_OPTIONS_SECTION, INF_REMOVE_OPTIONS_CHECKRUNNINGAPPS, "",
    //                          CheckRunningApps, 1024, RemoveInfFileName);

    GetPrivateProfileString(INF_REMOVE_OPTIONS_SECTION, INF_REMOVE_OPTIONS_WERLOCALDUMPS, "",
                            RemoveWERLocalDumps, sizeof(RemoveWERLocalDumps), RemoveInfFileName);

    return TRUE;
}

//************************************************************************************
//
// ExistFiles
//

BOOL ExistFiles(const char* files)
{
    const char* line;

    line = files;
    while (*line != 0)
    {
        int len = lstrlen(line);
        if (GetFileAttributes(line) != INVALID_FILE_ATTRIBUTES)
            return TRUE;
        line += len + 1;
    }
    return FALSE;
}

//************************************************************************************
//
// DoUnpinFromTaskbar
//

BOOL DoUnpinFromTaskbar(const char* unpinFiles)
{
    const char* line;
    line = unpinFiles;
    while (*line != 0)
    {
        int len = lstrlen(line);
        InvokeCmdFromContextMenu(HParent, line, "taskbarunpin");
        line += len + 1;
    }

    return TRUE;
}

//************************************************************************************
//
// DoRemoveFiles
//

BOOL DoRemoveFiles(const char* removeFiles)
{
    const char* line;
    char buff[2 * MAX_PATH];
    int ret;
    DWORD err;

    line = removeFiles;
    while (*line != 0)
    {
        int len = lstrlen(line);
    AGAIN:
        SetFileAttributes(line, FILE_ATTRIBUTE_ARCHIVE);
        if (!DeleteFile(line))
        {
            err = GetLastError();
            // skip already deleted/nonexisting files
            if (err != ERROR_PATH_NOT_FOUND && err != ERROR_FILE_NOT_FOUND)
            {
#ifdef INSIDE_SETUP
                if (SetupInfo.Silent)
                    return FALSE;
#endif //INSIDE_SETUP
                wsprintf(buff, LoadStr(IDS_REMOVE_DELETEFAILED), line, GetErrorText(err));
                ret = MessageBox(HParent, buff, LoadStr(IDS_REMOVE_TITLE), MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
                if (ret == IDABORT)
                    return FALSE;
                if (ret == IDRETRY)
                    goto AGAIN;
            }
        }
        line += len + 1;
    }

    return TRUE;
}

//************************************************************************************
//
// DoRemoveDirs
//

// we must be able to delete in a random order, so keep deleting while it succeeds

BOOL DoRemoveDirs()
{
    BOOL someDirDeleted;
    char* line;

    do
    {
        someDirDeleted = FALSE;
        line = RemoveDirs;
        while (*line != 0)
        {
            int len = lstrlen(line);
            SetFileAttributes(line, FILE_ATTRIBUTE_ARCHIVE);
            if (RemoveDirectory(line))
                someDirDeleted = TRUE;
            line += len + 1;
        }
    } while (someDirDeleted);

    return TRUE;
}

//************************************************************************************
//
// GetRootHandle
//

HKEY RemoveGetRootHandle(const char* root)
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

//************************************************************************************
//
// DoRemoveRegValues
//

BOOL DoRemoveRegValues()
{
    char* line;
    char root[MAX_PATH];
    char key[MAX_PATH];
    char valueName[MAX_PATH];
    BOOL ret;

    //  MessageBox(HParent, "begin", "remove values", MB_OK);
    line = RemoveRegValues;
    while (*line != 0)
    {
        char* begin;
        char* end;
        HKEY hRoot;
        HKEY hKey;

        int len = lstrlen(line);

        root[0] = 0;
        key[0] = 0;
        valueName[0] = 0;

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

        hRoot = RemoveGetRootHandle(root);

        //    MessageBox(HParent, key, "open key", MB_OK);
        ret = RegOpenKeyEx(hRoot, key, 0, KEY_ALL_ACCESS, &hKey);
        if (ret == ERROR_SUCCESS)
        {
            //      MessageBox(HParent, valueName, "delete value", MB_OK);
            RegDeleteValue(hKey, valueName);
            RegCloseKey(hKey);
        }

        line += len + 1;
    }

    return TRUE;
}

//************************************************************************************
//
// DoRemoveRegKeys
//

#ifdef INSIDE_SETUP
BOOL CreateAutoImportConfigMarker(const char* autoImportConfig, const char* autoImportConfigValue)
{
    if (autoImportConfig != NULL && autoImportConfig[0] != 0)
    {
        char root[MAX_PATH];
        char key[MAX_PATH];
        char value[MAX_PATH];

        const char* begin;
        const char* end;
        HKEY hRoot;
        HKEY hKey;

        int len = lstrlen(autoImportConfig);

        root[0] = 0;
        key[0] = 0;

        begin = end = autoImportConfig;
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

        hRoot = RemoveGetRootHandle(root);
        if (RegCreateKey(hRoot, key, &hKey) == ERROR_SUCCESS)
        {
            DWORD dwType = REG_SZ;
            if (RegSetValueEx(hKey, (const char*)value, 0, dwType, autoImportConfigValue, lstrlen(autoImportConfigValue)) != ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                return FALSE;
            }
            RegCloseKey(hKey);
        }
    }
    return TRUE;
}

BOOL IsRegistryKeyEmpty(HKEY hKey)
{
    char buff[MAX_PATH];
    if (RegEnumKey(hKey, 0, buff, MAX_PATH) != ERROR_NO_MORE_ITEMS)
        return FALSE;
    return TRUE;
}
BOOL DeleteAutoImportConfigMarker(const char* autoImportConfig)
{
    if (autoImportConfig != NULL && autoImportConfig[0] != 0)
    {
        char root[MAX_PATH];
        char key[MAX_PATH];
        char value[MAX_PATH];

        const char* begin;
        const char* end;
        HKEY hRoot;
        HKEY hKey;

        int len = lstrlen(autoImportConfig);

        root[0] = 0;
        key[0] = 0;

        begin = end = autoImportConfig;
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

        hRoot = RemoveGetRootHandle(root);

        if (RegOpenKeyEx(hRoot, key, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
        {
            BOOL deleteKey;

            // delete the value
            RegDeleteValue(hKey, value);

            // if the key is empty after deleting the value, delete the key as well
            deleteKey = IsRegistryKeyEmpty(hKey);
            RegCloseKey(hKey);

            if (deleteKey)
            {
                char* end2;
                end2 = key + lstrlen(key) - 1;
                while ((end2 > key) && (*end2 != '\\'))
                    end2--;
                if (end2 > key)
                {
                    *end2 = 0;
                    lstrcpy(value, end2 + 1);
                    if (RegOpenKeyEx(hRoot, key, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
                    {
                        // delete the key
                        RegDeleteKey(hKey, value); // under Win9x this removes subkeys too (see old MSDN), hence the check that the key is truly empty
                        RegCloseKey(hKey);
                    }
                }
            }
        }
    }
    return TRUE;
}
#endif //INSIDE_SETUP

BOOL ContainSubStrIgnoreCase(const char* str, const char* subStr)
{
    int len = lstrlen(subStr);
    // if it is shorter than the substring, bail out
    if (lstrlen(str) < len)
        return FALSE;

    if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, str, len, subStr, len) == CSTR_EQUAL)
        return TRUE;

    return FALSE;
}

BOOL DoRemoveRegKeys(BOOL removeConfiguration)
{
    char* line;
    char root[MAX_PATH];
    char key[MAX_PATH];
    char tmp[MAX_PATH];
    const char* UNINSTALL_KEY = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    const char* SALAMNDER_KEY_NEW = "Software\\Altap\\Altap Salamander";
    const char* SALAMNDER_KEY_OLD = "Software\\Altap\\Servant Salamander";

    line = RemoveRegKeys;
    while (*line != 0)
    {
        BOOL removeIt;
        char* begin;
        char* end;
        HKEY hRoot;

        int len = lstrlen(line);

        root[0] = 0;
        key[0] = 0;

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

        removeIt = removeConfiguration;
        lstrcpy(tmp, key);
        tmp[lstrlen(UNINSTALL_KEY)] = 0;       // we do not have strstr, so work around it
        if (lstrcmpi(tmp, UNINSTALL_KEY) == 0) // always cut off the uninstall record
            removeIt = TRUE;

        hRoot = RemoveGetRootHandle(root);
        if (removeIt)
        {
            RemoveKey(hRoot, key);
        }

#ifdef INSIDE_SETUP
        if (!SetupInfo.TheExistingVersionIsSame)
        {
            if (ContainSubStrIgnoreCase(key, SALAMNDER_KEY_NEW) || ContainSubStrIgnoreCase(key, SALAMNDER_KEY_OLD))
            {
                // store only the last component of the path
                end = key + lstrlen(key) - 1;
                while (end > key && *end != '\\')
                    end--;
                if (end > key)
                    CreateAutoImportConfigMarker(SetupInfo.AutoImportConfig, end + 1);
            }
        }
#endif //INSIDE_SETUP

        line += len + 1;
    }

    return TRUE;
}

//************************************************************************************
//
// DoRemoveWERLocalDumps
//

int MyGetEXENameLen(const char* buff)
{
    const char* p = buff;
    while (*p != 0 && *p != ',')
        p++;
    return (int)(p - buff);
}

BOOL RemoveIsWow64()
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

void DoRemoveWERLocalDump(const char* exeName)
{
    // If an x86 application crashes on x64 Windows, WER uses entries from the x64 registry.
    // The HKEY_LOCAL_MACHINE\SOFTWARE key is subject to the registry redirector, so the x86 Setup sees the x86 view,
    // while the x64 Setup sees the x64 view. From the x86 Setup.exe we need to write to the x64 branch of the
    // registry, which can be achieved using the special KEY_WOW64_64KEY flag, see MSDN:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129%28v=vs.85%29.aspx
    // Note that bypassing the redirector also applies to doinst.c!
    HKEY hKey;
    DWORD altapRefCount = 0xffffffff;
    BOOL delExeKey = FALSE;
    REGSAM samDesired = 0;
    if (RemoveIsWow64())
        samDesired = KEY_WOW64_64KEY;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps", 0, samDesired | KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        HKEY hExeKey;
        // open the key named after the EXE
        if (RegOpenKeyEx(hKey, exeName, 0, samDesired | KEY_READ | KEY_WRITE, &hExeKey) == ERROR_SUCCESS)
        {
            DWORD dwType;
            DWORD size = sizeof(altapRefCount);
            const char* dumpFolder = "%LOCALAPPDATA%\\Altap\\Open Salamander";
            dwType = REG_DWORD;
            if (RegQueryValueEx(hExeKey, "AltapRefCount", 0, &dwType, (BYTE*)&altapRefCount, &size) == ERROR_SUCCESS && dwType == REG_DWORD)
            {
                if (altapRefCount != 0xffffffff && altapRefCount > 0)
                    altapRefCount--;
                if (altapRefCount == 0)
                {
                    // delete the entire key after closing it
                    delExeKey = TRUE;
                }
                else
                {
                    // store the new RefCount
                    dwType = REG_DWORD;
                    RegSetValueEx(hExeKey, "AltapRefCount", 0, dwType, (const BYTE*)&altapRefCount, sizeof(altapRefCount));
                }
            }
            RegCloseKey(hExeKey);
            if (delExeKey)
                RemoveKey(hKey, exeName);
        }
        RegCloseKey(hKey);
    }
}

void DoRemoveWERLocalDumps()
{
    char exeName[MAX_PATH];
    const char* names = RemoveWERLocalDumps;
    while (*names != 0)
    {
        int exeNameLen = MyGetEXENameLen(names);
        if (exeNameLen > 0 && exeNameLen < MAX_PATH - 1)
        {
            lstrcpyn(exeName, names, exeNameLen + 1);
            DoRemoveWERLocalDump(exeName);
        }
        names += exeNameLen;
        if (*names == ',')
            names++;
    }
}

//************************************************************************************
//
// DoRemoveShellExts
//
/* removing via CLSID is no longer necessary; now we go by file name
BOOL MyGetValue(HKEY hKey, const char *name, DWORD type, void *buffer, DWORD bufferSize)
{
  DWORD gettedType;
  LONG res = RegQueryValueEx(hKey, name, 0, &gettedType, (BYTE *)buffer, &bufferSize);
  return res == ERROR_SUCCESS && gettedType == type;
}

typedef HRESULT (STDAPICALLTYPE *DLLUNREGPROC)() ;

BOOL
RemoveShellExt(const char *clsid)
{
  HKEY hKey;
  char key[MAX_PATH];
  char shellExtPath[MAX_PATH];
  BOOL registered;
  DWORD attrs;

  // find out whether the shell extension is registered and how the SALSHEXT.DLL is named in TEMP
  registered = FALSE;
  lstrcpy(key, "CLSID\\");
  lstrcat(key, clsid);
  lstrcat(key, "\\InProcServer32");
  if (RegOpenKeyEx(HKEY_CLASSES_ROOT, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
  {
    if (MyGetValue(hKey, NULL, REG_SZ, shellExtPath, MAX_PATH))
      registered = TRUE;
    RegCloseKey(hKey);
  }

  // it appears to be registered
  if (registered)
  {
    // try to load it and call the unregister function
    HMODULE hShellExt = LoadLibrary(shellExtPath);
    if (hShellExt != NULL)
    {
      DLLUNREGPROC DllUnregisterServer = (DLLUNREGPROC)GetProcAddress(hShellExt, "DllUnregisterServer");
      if (DllUnregisterServer != NULL)
        DllUnregisterServer();
      FreeLibrary(hShellExt);
    }

    // check whether the DLL exists and delete it
    attrs = GetFileAttributes(shellExtPath);
    if (attrs != INVALID_FILE_ATTRIBUTES)
    {
      if (attrs & FILE_ATTRIBUTE_READONLY)
        SetFileAttributes(shellExtPath, attrs ^ FILE_ATTRIBUTE_READONLY);
      // if it cannot be deleted normally, remove it after restart
      if (!DeleteFile(shellExtPath))
      {
        RemoveOnReboot(shellExtPath);
      }
    }
  }

  return TRUE;
}
*/

typedef HRESULT(STDAPICALLTYPE* DLLUNREGPROC)();

BOOL TryToRenameShellExt(char* name)
{
    char buff[MAX_PATH];
    char ext[MAX_PATH];
    char* p;
    int counter;
    BOOL exist;

    if (lstrlen(name) > MAX_PATH - 10)
        return FALSE; // nowhere to fit our number

    lstrcpy(buff, name);

    p = strrchr(buff, '.');
    if (p == NULL)
        return FALSE; // shell extension without an extension? strange

    lstrcpy(ext, p); // backup of the extension
    counter = 1;
    do
    {
        wsprintf(p, "_%d%s", counter, ext);
        exist = GetFileAttributes(buff) != INVALID_FILE_ATTRIBUTES;
        if (exist)
            counter++;
    } while (exist && counter < 1000);

    if (exist)
        return FALSE; // 1000 attempts must be enough; give up

    if (!MoveFile(name, buff))
        return FALSE; // on Win9x the rename may fail

    lstrcpy(name, buff);
    return TRUE;
}

BOOL RemoveShellExt(const char* shellExtPath, BOOL* delayedDelete, BOOL* renamingFailed)
{
    DWORD attrs;

    // try to load it and call the unregister function
    HMODULE hShellExt = LoadLibrary(shellExtPath);
    if (hShellExt != NULL)
    {
        DLLUNREGPROC DllUnregisterServer = (DLLUNREGPROC)GetProcAddress(hShellExt, "DllUnregisterServer");
        if (DllUnregisterServer != NULL)
            DllUnregisterServer();
        DllUnregisterServer = (DLLUNREGPROC)GetProcAddress(hShellExt, "DllUnregisterServerOtherPlatform");
        if (DllUnregisterServer != NULL)
            DllUnregisterServer();
        FreeLibrary(hShellExt);
    }

    // check whether the DLL exists and delete it
    attrs = GetFileAttributes(shellExtPath);
    if (attrs != INVALID_FILE_ATTRIBUTES)
    {
        // drop the read-only attribute if it is set
        if (attrs & FILE_ATTRIBUTE_READONLY)
            SetFileAttributes(shellExtPath, attrs ^ FILE_ATTRIBUTE_READONLY);

        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (*delayedDelete)
                RemoveOnReboot(shellExtPath);
            else
                RemoveDirectory(shellExtPath);
        }
        else
        {
            char newName[MAX_PATH];
            lstrcpyn(newName, shellExtPath, MAX_PATH);
            newName[MAX_PATH - 1] = 0;

            if (!DeleteFile(newName))
            {
                // try to rename the shell extension so that if the user immediately copies a new version of Salamander into the same directory after uninstalling,
                // we do not accidentally cut off their new shell extension
                if (!TryToRenameShellExt(newName))
                    *renamingFailed = TRUE;

                // if it cannot be deleted normally, remove it after restart
                RemoveOnReboot(newName);
                *delayedDelete = TRUE;
            }
        }
    }

    return TRUE;
}

BOOL DoRemoveShellExts()
{
    BOOL delayedDelete = FALSE;
    BOOL renamingFailed = FALSE;
    char* line;
    line = RemoveShellExts;
    while (*line != 0)
    {
        int len = lstrlen(line);
        RemoveShellExt(line, &delayedDelete, &renamingFailed);
        line += len + 1;
    }
    if (renamingFailed)
        return FALSE;
    else
        return TRUE;
}

//************************************************************************************
//
// CheckRunningApp
//
// checks whether an application with the window class appWindowClassName is running
//
/*
BOOL
CheckRunningApp(const char *appWindowClassName)
{
  HWND hWnd;
  BOOL again;
  do
  {
    again = FALSE;
    hWnd = FindWindow(appWindowClassName, NULL);
    if (hWnd != NULL)
    {
      HANDLE hProc;
      DWORD  dwID;
      DWORD  ret;
      char   windowName[100];
      char   buff[2000];

      GetWindowText(hWnd, windowName, 100);
      wsprintf(buff, LoadStr(IDS_APPRUNNING), windowName);

      GetWindowThreadProcessId(hWnd, &dwID);
      hProc = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, FALSE, dwID);
      do 
      {
#ifdef INSIDE_SETUP
        if (SetupInfo.Silent) return FALSE;
#endif //INSIDE_SETUP
        int ret = MessageBox(HParent, buff, LoadStr(IDS_TITLE), MB_OKCANCEL | MB_ICONINFORMATION);
        if (ret == IDCANCEL)
          return FALSE;
      } while (hProc != NULL && (ret = WaitForSingleObject(hProc, 10)) == WAIT_TIMEOUT);
    }
  } while (hWnd != NULL);
  return TRUE;
}
*/
//************************************************************************************
//
// DoRemove
//

// returns: TRUE for continue uninstall, FALSE for abort uninstall
BOOL LookupForRunningApps(const char* apps)
{
    const char* line = apps;
    EnumProcesses();
    while (*line != 0)
    {
        char shortName[MAX_PATH];
        int len = lstrlen(line);

        // when conversion to short name fails, ignore item
        if (GetShortPathName(line, shortName, MAX_PATH) != 0)
        {
            BOOL again;
            do
            {
                again = FALSE;
                if (FindProcess(shortName))
                {
                    char buff[2 * MAX_PATH];
                    int ret;
#ifdef INSIDE_SETUP
                    if (SetupInfo.Silent)
                        return FALSE;
#endif //INSIDE_SETUP
                    wsprintf(buff, LoadStr(IDS_REMOVE_APPRUNNING), line);
                    ret = MessageBox(HParent, buff, LoadStr(IDS_REMOVE_TITLE), MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION | MB_DEFBUTTON2);

                    if (ret == IDABORT)
                        return FALSE;

                    if (ret == IDRETRY)
                    {
                        EnumProcesses();
                        again = TRUE;
                    }
                }
            } while (again);
        }

        line += len + 1;
    }

    return TRUE;
}

BOOL DoRemove(BOOL* needRestart)
{
    HANDLE hFile;
    DWORD read;

    if (needRestart != NULL)
        *needRestart = FALSE;

    // load the INF file
    hFile = CreateFile(RemoveInfFileName, GENERIC_READ, 0, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_ARCHIVE, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    if (!ReadFile(hFile, InfFileBuffer, 100000, &read, NULL))
    {
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);

    /*
  if (CheckRunningApps[0] != 0)
  {
    // force the user to close applications from the list
    char appClass[1000];
    char *begin;
    char *end;
    begin = end = CheckRunningApps;
    while(*begin != 0)
    {
      while (*end != 0 && *end != ',')
        end++;
      lstrcpyn(appClass, begin, end - begin + 1);
      if (*end == ',')
        end++;
      begin = end;

      if (!CheckRunningApp(appClass))
        return FALSE; // user canceled
    }
  }
*/

    // load the lists slated for removal
    RemoveGetPrivateProfileSection(INF_REMOVE_UNPINFROMTASKBAR, UnpinFromTaskbar, sizeof(UnpinFromTaskbar));
    RemoveGetPrivateProfileSection(INF_REMOVE_DELFILES, RemoveFiles, sizeof(RemoveFiles));
    RemoveGetPrivateProfileSection(INF_REMOVE_DELDIRS, RemoveDirs, sizeof(RemoveDirs));
    RemoveGetPrivateProfileSection(INF_REMOVE_DELREGVALS, RemoveRegValues, sizeof(RemoveRegValues));
    RemoveGetPrivateProfileSection(INF_REMOVE_DELREGKEYS, RemoveRegKeys, sizeof(RemoveRegKeys));
    RemoveGetPrivateProfileSection(INF_REMOVE_DELSHELLEXTS, RemoveShellExts, sizeof(RemoveShellExts));

    ParamsGlobal.AppName = RemoveAppName;
    ParamsGlobal.RemoveConfiguration = FALSE; // default values can be set here (if called in "InsideSetupMode", it must be FALSE!)

#ifndef INSIDE_SETUP
    if (!QuietMode && !InsideSetupMode)
    {
        // do we really want to terminate the application?
        INT_PTR ret = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CONFIRMATION),
                                     HParent, ConfirmationDlgProc, (LPARAM)NULL);
        if (ret == IDNO)
            return FALSE; // cancel
    }
#endif // INSIDE_SETUP

    if (RemoveRunProgramQuiet[0] != 0)
    {
        // launch the tool and wait until it finishes
        STARTUPINFO si = {0};
        PROCESS_INFORMATION pi;
        char currentDir[MAX_PATH];
        char* p;
        lstrcpy(currentDir, RemoveRunProgramQuiet);
        p = currentDir + lstrlen(currentDir) - 1;
        while (*p != '\\' && p > currentDir)
            p--;
        if (*p == '\\')
            *p = 0;

        si.cb = sizeof(STARTUPINFO);
        if (!CreateProcess(NULL, RemoveRunProgramQuiet, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL, currentDir, &si, &pi))
        {
#ifdef INSIDE_SETUP
            if (!SetupInfo.Silent)
            {
#endif //INSIDE_SETUP
                char errbuff[100];
                DWORD le = GetLastError();
                wsprintf(errbuff, "CreateProcess failed, error=%d", le);
                MessageBox(HParent, errbuff, "Error", MB_OK);
#ifdef INSIDE_SETUP
            }
#endif //INSIDE_SETUP
        }
        else
            WaitForSingleObject(pi.hProcess, INFINITE); // wait for the tool to finish
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    if (!LookupForRunningApps(RemoveFiles))
        return FALSE; // cancel

    if (RemoveShellExts[0] != 0)
    {
        if (!DoRemoveShellExts())
        {
            // if we are running as part of installing a new version and the old shell extension could not be renamed (W9x),
            // we must ask the user to restart the machine before finishing the installation
            if (InsideSetupMode)
            {
                if (needRestart != NULL)
                    *needRestart = TRUE;
                return FALSE;
            }
        }
    }

    if (UnpinFromTaskbar[0] != 0)
        DoUnpinFromTaskbar(UnpinFromTaskbar);

    if (RemoveFiles[0] != 0)
        if (!DoRemoveFiles(RemoveFiles))
            return FALSE;

    if (RemoveDirs[0] != 0)
        DoRemoveDirs();

    if (RemoveRegValues[0] != 0)
        DoRemoveRegValues();

    if (RemoveRegKeys[0] != 0)
        DoRemoveRegKeys(ParamsGlobal.RemoveConfiguration);

    if (RemoveWERLocalDumps[0] != 0)
        DoRemoveWERLocalDumps();

    return TRUE;
}

//************************************************************************************
//
// CleanUp
//

BOOL CleanUp()
{
    HANDLE hFile;
    char tempPath[MAX_PATH];
    char fileName[MAX_PATH];
    const char* nameOnly;
    DWORD written;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi;
    char cmdLine[300];
    char* line;
    int ret, i;
    BOOL ok = TRUE;

    if (GetTempPath(MAX_PATH, tempPath) == 0)
        return FALSE;

    if (GetTempFileName(tempPath, "rmb", 1, fileName) == 0)
        return FALSE;

    *(strrchr(fileName, '.') + 1) = '\0'; // strip extension TMP
    lstrcat(fileName, "bat");

    nameOnly = strrchr(fileName, '\\') + 1;
    if (nameOnly == NULL || *nameOnly == 0)
        nameOnly = fileName;

    // create a batch file that deletes us and then itself
    hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    wsprintf(cmdLine, BATCH_FILE_BEGIN, ModuleName, ModuleName);
    WriteFile(hFile, cmdLine, lstrlen(cmdLine), &written, NULL);
    ok = (written == (DWORD)lstrlen(cmdLine));
    if (ok)
    {
        for (i = 0; i < 5; i++) // ugly hack, but it should wipe everything down to depth 5
        {
            line = RemoveDirs;
            while (*line != 0)
            {
                int len = lstrlen(line);
                wsprintf(cmdLine, BATCH_FILE_DELDIR, line);
                WriteFile(hFile, cmdLine, lstrlen(cmdLine), &written, NULL);
                ok = (written == (DWORD)lstrlen(cmdLine));
                line += len + 1;
            }
        }
    }
    if (ok)
    {
        wsprintf(cmdLine, BATCH_FILE_DELFILE, fileName);
        WriteFile(hFile, cmdLine, lstrlen(cmdLine), &written, NULL);
        ok = (written == (DWORD)lstrlen(cmdLine));
    }

    if (!ok)
    {
        DeleteFile(fileName);
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);

    // and run the batch file
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    GetEnvironmentVariable("COMSPEC", cmdLine, MAX_PATH);
    lstrcat(cmdLine, " /C "); // run the command and close after it finishes
    wsprintf(cmdLine + lstrlen(cmdLine), "\"%s\"", nameOnly);

    SetCurrentDirectory(tempPath);
    ret = CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED | NORMAL_PRIORITY_CLASS, NULL, tempPath, &si, &pi);
    if (ret == 0)
    {
        DeleteFile(fileName);
        return FALSE;
    }

    // Lower the batch file's priority even more.
    SetThreadPriority(pi.hThread, THREAD_PRIORITY_IDLE);

    // Raise our priority so that we terminate as quickly as possible.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    // Allow the batch file to run and clean-up our handles.
    CloseHandle(pi.hProcess);
    ResumeThread(pi.hThread);
    // We want to terminate right away now so that we can be deleted
    CloseHandle(pi.hThread);

    return TRUE;
}

BOOL Uninstall(BOOL* needRestart)
{
    BOOL ret = FALSE;
    char buff[1024];
    if (!RemoveOpenInfFile())
    {
#ifdef INSIDE_SETUP
        if (!QuietMode && !SetupInfo.Silent)
#else  //INSIDE_SETUP
        if (!QuietMode)
#endif //INSIDE_SETUP
        {
            wsprintf(buff, LoadStr(IDS_REMOVE_ERROR_LOADINF), RemoveInfFileName);
            MessageBox(HParent, buff, LoadStr(IDS_REMOVE_TITLE), MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }

    if (DoRemove(needRestart))
    {
        if (!QuietMode && !InsideSetupMode)
        {
            // notify that everything is OK
            wsprintf(buff, LoadStr(IDS_REMOVE_SUCCESS), RemoveAppName);
            MessageBox(HParent, buff, LoadStr(IDS_REMOVE_TITLE), MB_OK | MB_ICONINFORMATION | MB_TASKMODAL | MB_SETFOREGROUND);
        }
        // clean up our temporary files
        if (!InsideSetupMode)
            CleanUp();
        ret = TRUE;
    }

    FreeProcesses();
    return ret;
}

//************************************************************************************
//
// WinMain
//

#ifndef INSIDE_SETUP

/* according to http://vcfaq.mvps.org/sdk/21.htm */
#define BUFF_SIZE 1024
BOOL IsUserAdmin()
{
    HANDLE hToken = NULL;
    PSID pAdminSid = NULL;
    BYTE buffer[BUFF_SIZE];
    PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)buffer;
    DWORD dwSize; // buffer size
    DWORD i;
    BOOL bSuccess;
    SID_IDENTIFIER_AUTHORITY siaNtAuth = SECURITY_NT_AUTHORITY;

    // get token handle
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return FALSE;

    bSuccess = GetTokenInformation(hToken, TokenGroups, (LPVOID)pGroups, BUFF_SIZE, &dwSize);
    CloseHandle(hToken);
    if (!bSuccess)
        return FALSE;

    if (!AllocateAndInitializeSid(&siaNtAuth, 2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &pAdminSid))
        return FALSE;

    bSuccess = FALSE;
    for (i = 0; (i < pGroups->GroupCount) && !bSuccess; i++)
    {
        if (EqualSid(pAdminSid, pGroups->Groups[i].Sid))
            bSuccess = TRUE;
    }
    FreeSid(pAdminSid);

    return bSuccess;
}

BOOL RunAsAdminAndWait(HWND hWnd, LPTSTR lpFile, LPTSTR lpParameters, DWORD* exitCode)
{
    BOOL ret;
    SHELLEXECUTEINFO sei = {0};

    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.hwnd = hWnd;
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "runas";
    sei.lpFile = lpFile;
    sei.lpParameters = lpParameters;
    sei.nShow = SW_SHOWNORMAL;

    ret = FALSE;
    *exitCode = 0;
    if (ShellExecuteEx(&sei)) // FALSE can also mean just Cancel
    {
        ret = TRUE;
        if (sei.hProcess != NULL)
        {
            WaitForSingleObject(sei.hProcess, INFINITE);
            if (!GetExitCodeProcess(sei.hProcess, exitCode))
                *exitCode = 0;
            CloseHandle(sei.hProcess);
        }
    }
    return ret;
}

char* SkipExeName(char* cmdline)
{
    // skip leading spaces
    while (*cmdline == ' ' || *cmdline == '\t')
        cmdline++;
    // skip exe name
    if (*cmdline == '"')
    {
        cmdline++;
        while (*cmdline != '\0' && *cmdline != '"')
            cmdline++;
        if (*cmdline == '"')
            cmdline++;
    }
    else
        while (*cmdline != '\0' && *cmdline != ' ' && *cmdline != '\t')
            cmdline++;
    while (*cmdline == ' ' || *cmdline == '\t')
        cmdline++;
    return cmdline;
}

void RunRemoveExeAsAdminIfNeeded()
{
    if (!IsUserAdmin())
    {
        {
            char removeExe[MAX_PATH];
            char params[MAX_PATH];
            DWORD exitCode;
            char* p;
            BOOL runAsForbidden;
            int len;

            char* cmdline = SkipExeName(GetCommandLine());
            lstrcpyn(params, cmdline, MAX_PATH - 3);
            len = lstrlen(params);
            while (len > 0 && (params[len - 1] == ' ' || params[len - 1] == '\t'))
                len--;
            lstrcpy(params + len, " /a");
            runAsForbidden = FALSE;
            p = cmdline;
            while (*p != 0)
            {
                if ((*p == '/' || *p == '-') &&
                    (*(p + 1) == 'a' || *(p + 1) == 'A') &&
                    (*(p + 2) == ' ' || *(p + 2) == '\t' || *(p + 2) == 0))
                {
                    runAsForbidden = TRUE;
                }
                while (*p != 0 && *p != ' ' && *p != '\t')
                    p++;
                while (*p != 0 && (*p == ' ' || *p == '\t'))
                    p++;
            }

            exitCode = 0;
            if (!runAsForbidden && GetModuleFileName(NULL, removeExe, MAX_PATH))
            {
                MessageBox(NULL, LoadStr(IDS_REMOVE_XPRUNASADMIN), LoadStr(IDS_REMOVE_TITLE), MB_OK | MB_ICONINFORMATION);
                if (RunAsAdminAndWait(NULL, removeExe, params, &exitCode))
                { // if another remove.exe is started, terminate this one here
                    ExitProcess(exitCode);
                }
            }
        }
    }
}

BOOL RefreshDesktop(BOOL sleep)
{
    ITEMIDLIST root = {0};
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, &root, 0);
    if (sleep)
        Sleep(500); // give the system some time
    return TRUE;
}

// ****************************************************************************
// EnableExceptionsOn64
//

// We want to be notified about SEH exceptions even on x64 Windows 7 SP1 and later
// http://blog.paulbetts.org/index.php/2010/07/20/the-case-of-the-disappearing-onload-exception-user-mode-callback-exceptions-in-x64/
// http://connect.microsoft.com/VisualStudio/feedback/details/550944/hardware-exceptions-on-x64-machines-are-silently-caught-in-wndproc-messages
// http://support.microsoft.com/kb/976038
void EnableExceptionsOn64()
{
    typedef BOOL(WINAPI * FSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
    typedef BOOL(WINAPI * FGetProcessUserModeExceptionPolicy)(LPDWORD dwFlags);
    typedef BOOL(WINAPI * FIsWow64Process)(HANDLE, PBOOL);
#define PROCESS_CALLBACK_FILTER_ENABLED 0x1

    HINSTANCE hDLL = LoadLibrary("KERNEL32.DLL");
    if (hDLL != NULL)
    {
        FIsWow64Process isWow64 = (FIsWow64Process)GetProcAddress(hDLL, "IsWow64Process");
        FSetProcessUserModeExceptionPolicy set = (FSetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "SetProcessUserModeExceptionPolicy");
        FGetProcessUserModeExceptionPolicy get = (FGetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "GetProcessUserModeExceptionPolicy");
        if (isWow64 != NULL && set != NULL && get != NULL)
        {
            BOOL bIsWow64;
            if (isWow64(GetCurrentProcess(), &bIsWow64) && bIsWow64)
            {
                DWORD dwFlags;
                if (get(&dwFlags))
                    set(dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            }
        }
        FreeLibrary(hDLL);
    }
}

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    char* p;
    HInstance = hInstance;

    EnableExceptionsOn64();

    RunRemoveExeAsAdminIfNeeded();

    InitUtils();

    // we do not want critical errors such as "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    OleInitialize(NULL);

    GetModuleFileName(NULL, ModuleName, MAX_PATH);

    lstrcpy(ModulePath, ModuleName);
    *(strrchr(ModulePath, '\\')) = '\0'; // Strip setup.exe off path

    InitCommonControls();

    p = SkipExeName(lpCmdLine);

    if ((*p == '/' || *p == '-') &&
        (*(p + 1) == 'q' || *(p + 1) == 'Q') &&
        (*(p + 2) == ' ' || *(p + 2) == '\t' || *(p + 2) == 0))
    {
        QuietMode = TRUE;
    }

    lstrcpy(RemoveInfFileName, ModulePath);
    lstrcat(RemoveInfFileName, INF_REMOVE_FILENAME);

    Uninstall(NULL);

    RefreshDesktop(FALSE);

    OleUninitialize();

    return 0;
}
#endif //INSIDE_SETUP

#ifdef INSIDE_SETUP
BOOL DoUninstall(HWND hParent, BOOL* needRestart)
{
    BOOL ret;
    if (SetupInfo.TheExistingVersionIsSame)
        ReadPreviousVerOfFileToIncrementContent();
    InsideSetupMode = TRUE;
    HParent = hParent;
    ExpandPath(SetupInfo.SaveRemoveLog);
    lstrcpy(RemoveInfFileName, SetupInfo.SaveRemoveLog);
    ret = Uninstall(needRestart);
    RefreshDesktop(TRUE);
    return ret;
}
#endif //INSIDE_SETUP
