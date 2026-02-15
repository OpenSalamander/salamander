// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <regstr.h>

#include "infinst.h"
#include "resource.h"

//************************************************************************************
//
// Globals
//

const char* MAINWINDOW_CLASS = "AltapInstallMW";
char MAINWINDOW_TITLE[100] = {0};

INSTALLINFO SetupInfo = {0}; // a structure containing the review information
HINSTANCE HInstance = NULL;

BOOL SfxDirectoriesValid = FALSE;
char SfxDirectories[7][MAX_PATH];

HWND SfxDlg = NULL;    // SFX7ZIP window that launched this setup.exe (before setup.exe exits we show and activate it so the installed readme and Salam can be launched from it)
BOOL RunBySfx = FALSE; // TRUE if setup.exe was started with the /runbysfx parameter

BYTE LowerCase[256];

int StrICmp(const char* s1, const char* s2)
{
    int res;
    while (1)
    {
        res = (unsigned)LowerCase[*s1] - (unsigned)LowerCase[*s2++];
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
        if (*s1++ == 0)
            return 0; // ==
    }
}

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

    //  HANDLES(EnterCriticalSection(&__StrCriticalSection.cs));

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

    //  HANDLES(LeaveCriticalSection(&__StrCriticalSection.cs));

    return ret;
}

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

    SystemParametersInfo(SPI_GETWORKAREA, 0, &masterRect, 0);

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
// InsertProgramName
//
// replaces %s in str with the name of the installed application
//

void InsertProgramName(char* str, BOOL version)
{
    char buff[1024];
    if (version)
        wsprintf(buff, str, SetupInfo.ApplicationNameVer);
    else
        wsprintf(buff, str, SetupInfo.ApplicationName);
    lstrcpy(str, buff);
}

//************************************************************************************
//
// InsertAppName
//

void InsertAppName(HWND hDlg, int resID, BOOL version)
{
    char buff[1024];
    HWND hText = GetDlgItem(hDlg, resID);
    GetWindowText(hText, buff, 1024);
    InsertProgramName(buff, version);
    SetWindowText(hText, buff);
}

//************************************************************************************
//
// LoadLTextFile
//

BOOL LoadTextFile(const char* fileName, char* buff, int buffMax)
{
    HANDLE hFile;
    DWORD read;

    hFile = CreateFile(fileName,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    if (!ReadFile(hFile,
                  buff,
                  buffMax,
                  &read,
                  NULL))
    {
        CloseHandle(hFile);
        return FALSE;
    }

    CloseHandle(hFile);
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

//************************************************************************************
//
// WinMain
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

void* mini_memcpy(void* out, const void* in, int len); // defined in the remove part of the code

BOOL GetStringFromDLL(const char* dllName, int resID, char* buff, int buffSize)
{
    BOOL ret = FALSE;
    HINSTANCE hDLL = LoadLibraryEx(dllName, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (hDLL != NULL)
    {
        if (LoadString(hDLL, resID, buff, buffSize))
            ret = TRUE;
        FreeLibrary(hDLL);
    }
    return ret;
}

void ExpandPath(char* path)
{
    char buff[MAX_PATH];
    char* src;
    char* dst;
    const char* mui = NULL;
    src = path;
    dst = buff;
    *dst = 0;
    while (*src != 0)
    {
        char srcVal = *(src + 1);
        if (*src == '%' &&
            (srcVal == '%' ||
             srcVal == '@' && mui == NULL || // only one MUI marker per line, ending with the end of the line
             srcVal >= '0' && srcVal <= '9' ||
             srcVal >= 'A' && srcVal <= 'Z' ||
             srcVal >= 'a' && srcVal <= 'z'))
        {
            int len;
            char* p = NULL;
            if (srcVal == '%')
            {
                p = "%"; // %% -> %
            }
            else if (srcVal == '@')
            {
                p = "@";
                mui = dst; // store the MUI marker for potential final expansion
            }
            else if (srcVal >= '0' && srcVal <= '9')
            {
                switch (srcVal)
                {
                case '0':
                    p = ModulePath;
                    break;
                case '1':
                    p = SetupInfo.DefaultDirectory;
                    break;
                case '2':
                    p = WindowsDirectory;
                    break;
                case '3':
                    p = SystemDirectory;
                    break;
                case '4':
                    p = ProgramFilesDirectory;
                    break;
                case '5':
                    p = DesktopDirectory;
                    break;
                case '6':
                    p = StartMenuDirectory;
                    break;
                case '7':
                    p = StartMenuProgramDirectory;
                    break;
                case '8':
                    p = QuickLaunchDirectory;
                    break;
                }
            }
            else
            {
                if (srcVal >= 'a' && srcVal <= 'z')
                    srcVal = 'A' + srcVal - 'a';
                p = SetupInfo.RegPathVal[srcVal - 'A'];
            }

            len = 0;
            if (p != NULL)
            {
                len = lstrlen(p);
                mini_memcpy(dst, p, len);
            }
            src += 2;
            dst += len;
        }
        else
        {
            *dst = *src;
            dst++;
            src++;
        }
    }
    *dst = 0;
    lstrcpy(path, buff);
}

// ****************************************************************************
//
// GetCmdLine - extract parameters from the command line
//
// buf + size - buffer for the parameters
// argv - array of pointers that will be filled with the parameters
// argCount - on input the number of elements in argv; on output contains the number of parameters
// cmdLine - command-line parameters (without the .exe file name - from WinMain)

BOOL GetCmdLine(char* buf, int size, char* argv[], int* argCount, const char* cmdLine)
{
    int space = *argCount;
    char* c = buf;
    char* end = buf + size;
    const char* s = cmdLine;
    char term;
    *argCount = 0;

    while (*s != 0)
    {
        if (*s == '"') // opening '"'
        {
            if (*++s == 0)
                break;
            term = '"';
        }
        else
            term = ' ';

        if (*argCount < space && c < end)
            argv[(*argCount)++] = c;
        else
            return c < end; // failure only if the buffer is too small

        while (1)
        {
            if (*s == term || *s == 0)
            {
                if (*s == 0 || term != '"' || *++s != '"') // unless this is the escaped "" -> " sequence
                {
                    if (*s != 0)
                        s++;
                    while (*s != 0 && *s == ' ')
                        s++;
                    if (c < end)
                    {
                        *c++ = 0;
                        break;
                    }
                    else
                        return FALSE;
                }
            }
            if (c < end)
                *c++ = *s++;
            else
                return FALSE;
        }
    }
    return TRUE;
}

BOOL LoadSfxDirectories(HANDLE* fileOut, BOOL openAndPrepareForWrite)
{
    char name[MAX_PATH];
    HANDLE file;

    if (!openAndPrepareForWrite)
        SfxDirectoriesValid = FALSE;

    GetModuleFileName(NULL, name, MAX_PATH);
    lstrcpy(strrchr(name, '\\'), "\\params.txt");

    file = CreateFile(name, GENERIC_READ | (openAndPrepareForWrite ? GENERIC_WRITE : 0),
                      0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD sizeLo, sizeHi;
        if (fileOut != NULL)
            *fileOut = file;
        sizeLo = GetFileSize(file, &sizeHi);
        if ((sizeLo != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) &&
            sizeHi == 0 && sizeLo <= 3000)
        {
            char buf[3001];
            DWORD read;
            if (ReadFile(file, buf, sizeLo, &read, NULL) && read == sizeLo)
            {
                int index;
                char* end;
                char* line;
                char* s;

                index = 0;
                end = buf + read;
                line = buf;
                s = line;
                while (s < end)
                {
                    while (s < end && *s != '\r' && *s != '\n')
                        s++;
                    *s++ = 0; // s == end is still a valid position

                    if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, line, -1, "end-of-params", -1) == CSTR_EQUAL &&
                        index == 7)
                    {
                        if (openAndPrepareForWrite)
                        {
                            SetFilePointer(file, (int)((s - 1) - buf), NULL, FILE_BEGIN);
                            SetEndOfFile(file);
                            return TRUE;
                        }
                        SfxDirectoriesValid = TRUE;
                        break;
                    }
                    if (index < 7 && !openAndPrepareForWrite)
                        lstrcpyn(SfxDirectories[index], line, MAX_PATH);
                    index++;

                    while (s < end && (*s == '\r' || *s == '\n'))
                        s++;
                    line = s;
                }
            }
        }
        CloseHandle(file);
    }
    return FALSE;
}

BOOL StoreExecuteInfo(const char* cmdLineViewer, const char* cmdLineProgram, BOOL execViewer, BOOL execProgram)
{
    HANDLE file;
    BOOL ret = FALSE;
    if (LoadSfxDirectories(&file, TRUE))
    {
        DWORD written;
        if (WriteFile(file, "\r\n", 2, &written, NULL) && written == 2 &&
            WriteFile(file, cmdLineViewer, lstrlen(cmdLineViewer), &written, NULL) && written == (DWORD)lstrlen(cmdLineViewer) &&
            WriteFile(file, "\r\n", 2, &written, NULL) && written == 2 &&
            WriteFile(file, cmdLineProgram, lstrlen(cmdLineProgram), &written, NULL) && written == (DWORD)lstrlen(cmdLineProgram) &&
            WriteFile(file, "\r\n", 2, &written, NULL) && written == 2)
        {
            if (!execViewer ||
                WriteFile(file, "exec-viewer", sizeof("exec-viewer") - 1, &written, NULL) && written == sizeof("exec-viewer") - 1 &&
                    WriteFile(file, "\r\n", 2, &written, NULL) && written == 2)
            {
                if (!execProgram ||
                    WriteFile(file, "exec-salamander", sizeof("exec-salamander") - 1, &written, NULL) && written == sizeof("exec-salamander") - 1 &&
                        WriteFile(file, "\r\n", 2, &written, NULL) && written == 2)
                {
                    ret = TRUE;
                }
            }
        }
        CloseHandle(file);
    }
    return ret;
}

DWORD Hex2DWORD(const char* s)
{
    DWORD ret = 0;
    if (*s == '0' && *(s + 1) == 'x')
    {
        s += 2;
        while (*s != 0)
        {
            if (*s >= '0' && *s <= '9')
                ret = ret * 16 + (*s++ - '0');
            else if (*s >= 'a' && *s <= 'f')
                ret = ret * 16 + 10 + (*s++ - 'a');
            else if (*s >= 'A' && *s <= 'F')
                ret = ret * 16 + 10 + (*s++ - 'A');
            else
                break; // not hexadecimal, stop
        }
    }
    return ret;
}

BOOL GetCmdLineOptions(const char* cmdLine)
{
    char buf[MAX_PATH];
    char* argv[30];
    int p = 30; // number of elements in the argv array
    BOOL ret = TRUE;
    if (GetCmdLine(buf, MAX_PATH, argv, &p, cmdLine))
    {
        int i;
        for (i = 0; i < p; i++)
        {
            if ((StrICmp(argv[i], "-runbysfx") == 0 || StrICmp(argv[i], "/runbysfx") == 0)) // setup.exe started from the non-elevated sfx7zip.exe; take paths from the non-elevated process and optionally have it open the Readme and launch Salamander
            {
                if (i + 1 < p)
                {
                    RunBySfx = TRUE;
                    SfxDlg = (HWND)(DWORD_PTR)Hex2DWORD(argv[i + 1]);
                    LoadSfxDirectories(NULL, FALSE);
                    i++;
                    continue;
                }
                // otherwise it's an error
            }

            if (StrICmp(argv[i], "-s") == 0 || StrICmp(argv[i], "/s") == 0) // silent mode
            {
                SetupInfo.Silent = TRUE;
                continue;
            }

            if (StrICmp(argv[i], "-d") == 0 || StrICmp(argv[i], "/d") == 0) // destination
            {
                if (i + 1 < p)
                {
                    lstrcpyn(CmdLineDestination, argv[i + 1], MAX_PATH);
                    i++;
                    continue;
                }
                // otherwise it's an error
            }

            if (StrICmp(argv[i], "-id") == 0 || StrICmp(argv[i], "/id") == 0) // desktop icon
            {
                if (i + 1 < p && StrICmp(argv[i + 1], "y") == 0 || StrICmp(argv[i + 1], "n") == 0)
                {
                    SetupInfo.ShortcutInDesktop = (StrICmp(argv[i + 1], "y") == 0);
                    i++;
                    continue;
                }
                // otherwise it's an error
            }

            if (StrICmp(argv[i], "-is") == 0 || StrICmp(argv[i], "/is") == 0) // start menu icon
            {
                if (i + 1 < p && StrICmp(argv[i + 1], "y") == 0 || StrICmp(argv[i + 1], "n") == 0)
                {
                    SetupInfo.ShortcutInStartMenu = (StrICmp(argv[i + 1], "y") == 0);
                    i++;
                    continue;
                }
                // otherwise it's an error
            }

            //if (StrICmp(argv[i], "-it") == 0 || StrICmp(argv[i], "/it") == 0) // pin to taskbar
            //{
            //  if (i + 1 < p && StrICmp(argv[i + 1], "y") == 0 || StrICmp(argv[i + 1], "n") == 0)
            //  {
            //    SetupInfo.PinToTaskbar = (StrICmp(argv[i + 1], "y") == 0);
            //    i++;
            //    continue;
            //  }
            //  // otherwise it's an error
            //}

            if (StrICmp(argv[i], "-au") == 0 || StrICmp(argv[i], "/au") == 0) // all users
            {
                if (i + 1 < p && StrICmp(argv[i + 1], "y") == 0 || StrICmp(argv[i + 1], "n") == 0)
                {
                    SetupInfo.CommonFolders = (StrICmp(argv[i + 1], "y") == 0);
                    i++;
                    continue;
                }
                // otherwise it's an error
            }

            ret = FALSE;
            break;
        }
    }
    return ret;
}

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    OSVERSIONINFO osVersion = {0};
    MSG msg;

    HInstance = hInstance;

    EnableExceptionsOn64();

    // we do not want critical errors such as "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    for (int i = 0; i < 256; i++)
        LowerCase[i] = (char)CharLower((LPTSTR)(DWORD_PTR)i);

    lstrcpy(MAINWINDOW_TITLE, LoadStr(IDS_MAINWINDOWTITLE));

    PreviousVerOfFileToIncrementContent[0] = 0;

    SetupInfo.ShortcutInDesktop = TRUE;
    SetupInfo.ShortcutInStartMenu = TRUE;
    //SetupInfo.PinToTaskbar = TRUE;
    SetupInfo.CommonFolders = TRUE;
    SetupInfo.RunProgram = TRUE;
    SetupInfo.ViewReadme = FALSE;
    SetupInfo.Silent = FALSE;

    GetModuleFileName(NULL, ModulePath, MAX_PATH);
    *(strrchr(ModulePath, '\\')) = '\0'; // Strip setup.exe off path
    GetFolderPath(CSIDL_WINDOWS, WindowsDirectory);
    GetFolderPath(CSIDL_SYSTEM, SystemDirectory);
    GetFolderPath(CSIDL_PROGRAM_FILES, ProgramFilesDirectory);

    if (!GetCmdLineOptions(lpCmdLine))
    {
        MessageBox(HWizardDialog, LoadStr(ERROR_CMDLINE), MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    if (!OpenInfFile())
    {
        return FALSE;
    }

    if (SetupInfo.UseLicenseFile && !LoadTextFile(SetupInfo.LicenseFilePath, SetupInfo.License, 100000) && !SetupInfo.Silent)
    {
        MessageBox(HWizardDialog, LoadStr(ERROR_LOADLICENSE), MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    if (SetupInfo.Silent)
    {
        GetFoldersPaths();                // based on SetupInfo.CommonFolders extract the folders
        if (FindOutIfThisIsUpgrade(NULL)) // returns FALSE only if it cannot continue (we cannot upgrade a newer version)
        {
            BOOL cont = TRUE;
            if (!SetupInfo.UninstallExistingVersion)
                DeleteAutoImportConfigMarker(SetupInfo.AutoImportConfig);
            else
                cont = DoUninstall(NULL, NULL); // returns FALSE if a restart is required or the uninstall failed
            if (cont)
                DoInstallation();
        }
    }
    else
    {
        if (!CreateWizard())
        {
            if (!SetupInfo.Silent)
                MessageBox(NULL, LoadStr(ERROR_CREATEWIZARD), MAINWINDOW_TITLE, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }

        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (HWizardDialog == NULL || !IsDialogMessage(HWizardDialog, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    return 0;
}

BOOL RegisterString(
    LPSTR pszKey,
    LPSTR pszValue,
    LPSTR pszData)
{

    HKEY hKey;
    DWORD dwDisposition;

    //
    // Create the key, if it exists it will be opened
    //

    if (ERROR_SUCCESS !=
        RegCreateKeyEx(
            HKEY_LOCAL_MACHINE,      // handle of an open key
            pszKey,                  // address of subkey name
            0,                       // reserved
            NULL,                    // address of class string
            REG_OPTION_NON_VOLATILE, // special options flag
            KEY_ALL_ACCESS,          // desired security access
            NULL,                    // address of key security structure
            &hKey,                   // address of buffer for opened handle
            &dwDisposition))         // address of disposition value buffer
    {
        return FALSE;
    }

    //
    // Write the value and it's data to the key
    //

    if (ERROR_SUCCESS !=
        RegSetValueEx(
            hKey,              // handle of key to set value for
            pszValue,          // address of value to set
            0,                 // reserved
            REG_SZ,            // flag for value type
            pszData,           // address of value data
            lstrlen(pszData))) // size of value data
    {

        RegCloseKey(hKey);
        return FALSE;
    }

    //
    // Close the key
    //

    RegCloseKey(hKey);

    return TRUE;
}

BOOL GetRegString(HKEY hRootKey, LPSTR pszKey, LPSTR pszValue, LPSTR pszData)
{
    HKEY hKey;
    BOOL ret = FALSE;
    if (RegOpenKeyEx(hRootKey, pszKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        DWORD dwDataSize = MAX_PATH - 1;
        DWORD dwValueType = REG_SZ;
        if (RegQueryValueEx(hKey, pszValue, 0, &dwValueType, pszData, &dwDataSize) == ERROR_SUCCESS)
        {
            ret = TRUE;
            if (pszData[dwDataSize] != 0)
                pszData[dwDataSize] = 0;
        }
        RegCloseKey(hKey);
    }
    if (!ret)
        pszData[0] = 0;
    return ret;
}
