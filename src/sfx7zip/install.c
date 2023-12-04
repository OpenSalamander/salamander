// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>

#include "sfx7zip.h"

#include "extract.h"
#include "7zip.h"
#include "resource.h"

#pragma warning(disable : 4996)

const char* SETUP_NAME = "setup.exe";
const char* X64MARK_NAME = "x64"; // existence souboru rika, ze se jedna o x64 verzi, kterou nelze spustit pod x86

int terminate;
unsigned long ProgressPos;
unsigned char CurrentName[101];
unsigned char tmpName[MAX_PATH];
HWND DlgWin;
#ifdef FOR_SALAMANDER_SETUP
BOOL WaitingForExit = FALSE;
#endif // FOR_SALAMANDER_SETUP

int Help()
{
    char title[500];
    char message[500];
    LoadString(GetModuleHandle(NULL), ARC_TITLE, title, 500);
    LoadString(GetModuleHandle(NULL), ARC_HELP, message, 500);
    MessageBox(NULL, message, title, MB_OK | MB_ICONINFORMATION);
    return 0;
}

BOOL FileExists(const char* fileName)
{
    DWORD attr = GetFileAttributes(fileName);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;
BOOL Is64BitWindows()
{
    BOOL bIsWow64 = FALSE;
#ifdef _WIN64
    return TRUE; // 64-bit programs run only on Win64
#endif
    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
    if (NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
        {
            //handle error
        }
    }
    return bIsWow64;
}

void _RemoveTemporaryDir(const char* dir)
{
    char path[MAX_PATH];
    char* end;
    HANDLE find;
    WIN32_FIND_DATA file;

    lstrcpy(path, dir);
    end = path + lstrlen(path);
    if (*(end - 1) != '\\')
        *end++ = '\\';
    lstrcpy(end, "*.*");
    find = FindFirstFile(path, &file);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (file.cFileName[0] != '.' || file.cFileName[1] != '\0' &&
                                                (file.cFileName[1] != '.' || file.cFileName[2] != '\0'))
            {
                lstrcpy(end, file.cFileName);
                if (file.dwFileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                                             FILE_ATTRIBUTE_SYSTEM))
                    SetFileAttributes(path, FILE_ATTRIBUTE_ARCHIVE);
                if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    _RemoveTemporaryDir(path);
                else
                    DeleteFile(path);
            }
        } while (FindNextFile(find, &file));
        FindClose(find);
    }
    *(end - 1) = 0;
    RemoveDirectory(path);
}

void RemoveTemporaryDir(const char* dir)
{
    char buf[MAX_PATH];

    _RemoveTemporaryDir(dir);
    GetSystemDirectory(buf, MAX_PATH);
    SetCurrentDirectory(buf); // musime z adresare odejit, jinak nepujde smazat

    if (GetFileAttributes(dir) & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                                  FILE_ATTRIBUTE_SYSTEM))
        SetFileAttributes(dir, FILE_ATTRIBUTE_ARCHIVE);
    RemoveDirectory(dir);
}

BOOL CALLBACK DlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RECT dlg;
        long x, y;
        SetDlgItemText(hWindow, IDC_DIR, tmpName);
        SendDlgItemMessage(hWindow, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 200)); // 0..100 pro SzDecode(), ktery napred vse vybali do pameti, 100..200 pro nakopirovani na disk
        GetWindowRect(hWindow, &dlg);
        x = (GetSystemMetrics(SM_CXSCREEN) - (dlg.right - dlg.left)) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - (dlg.bottom - dlg.top)) / 2;
        SetWindowPos(hWindow, NULL, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }
#ifdef FOR_SALAMANDER_SETUP
#define WM_USER_SHOWACTSFX7ZIP (WM_APP + 666) // aktivace zaslana z instalaku (cislo je sdilene se SETUP.EXE)
    case WM_USER_SHOWACTSFX7ZIP:              // aktivace zaslana z instalaku
        ShowWindow(DlgWin, SW_SHOW);
        SetForegroundWindow(DlgWin);
        WaitingForExit = TRUE;
        return TRUE;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && WaitingForExit) // dialog se deaktivuje, konecne se spustila aplikace a aktivuje se (nas tedy deaktivuje)
        {
            DestroyWindow(DlgWin);
            DlgWin = NULL;
            return TRUE;
        }
        break;
#endif // FOR_SALAMANDER_SETUP
    case WM_USER_UPDATEPROGRESS:
    {
        HWND hChild = GetDlgItem(hWindow, IDC_PROGRESS);
        SendMessage(hChild, PBM_SETPOS, ProgressPos, 0);
        UpdateWindow(hChild);
        return TRUE;
    }
    case WM_USER_UPDATEFNAME:
        SetDlgItemText(hWindow, IDC_FILE, CurrentName);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            terminate = 1;
            return TRUE;
        }
    }
    return FALSE;
}

// for VS2008
#pragma intrinsic(memset) // abych mohli prekladat s optimalizaci na rychlost a prekladac nerval
#pragma function(memset)  // "error C2169: 'memset' : intrinsic function, cannot be defined"
void* memset(void* dest, int val, size_t len)
{
    register unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0)
        *ptr++ = val;
    return dest;
}

#ifdef FOR_SALAMANDER_SETUP

BOOL GetSpecialFolderPath(int folder, char* path)
{
    ITEMIDLIST* pidl; // vyber root-folderu
    *path = 0;
    if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, folder, &pidl)))
    {
        IMalloc* alloc;
        BOOL ret = SHGetPathFromIDList(pidl, path);
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->lpVtbl->DidAlloc(alloc, pidl) == 1)
                alloc->lpVtbl->Free(alloc, pidl);
            alloc->lpVtbl->Release(alloc);
        }
        return ret;
    }
    return FALSE;
}

/* dle http://vcfaq.mvps.org/sdk/21.htm */
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

BOOL CreateParamsFileIfNeeded(const char* path, char* name)
{
    BOOL ret = FALSE;
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os);
    if (os.dwPlatformId == VER_PLATFORM_WIN32_NT && os.dwMajorVersion >= 6 ||                 // Vista or later
        os.dwPlatformId == VER_PLATFORM_WIN32_NT && os.dwMajorVersion == 5 && !IsUserAdmin()) // W2K and XP: when started on limited/restricted account, system offers to run setup.exe as Admin, so params.txt file is needed too
    {
        int len;
        lstrcpyn(name, path, MAX_PATH);
        len = lstrlen(name);
        if (len > 0 && name[len - 1] != '\\')
            name[len++] = '\\';
        if (len + sizeof("params.txt") <= MAX_PATH)
        {
            HANDLE file;
            lstrcpy(name + len, "params.txt");
            file = CreateFile(name, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file != INVALID_HANDLE_VALUE)
            {
                char userPaths[7][MAX_PATH];
                ret = TRUE;
                ret &= GetSpecialFolderPath(CSIDL_COMMON_DESKTOPDIRECTORY, userPaths[0]);
                ret &= GetSpecialFolderPath(CSIDL_DESKTOPDIRECTORY, userPaths[1]);
                ret &= GetSpecialFolderPath(CSIDL_COMMON_STARTMENU, userPaths[2]);
                ret &= GetSpecialFolderPath(CSIDL_STARTMENU, userPaths[3]);
                ret &= GetSpecialFolderPath(CSIDL_COMMON_PROGRAMS, userPaths[4]);
                ret &= GetSpecialFolderPath(CSIDL_PROGRAMS, userPaths[5]);
                ret &= GetSpecialFolderPath(CSIDL_APPDATA, userPaths[6]);
                if (ret)
                {
                    DWORD written;
                    int i;
                    for (i = 0; i < 7; i++)
                    {
                        if (!WriteFile(file, userPaths[i], lstrlen(userPaths[i]), &written, NULL) || written != (DWORD)lstrlen(userPaths[i]) ||
                            !WriteFile(file, "\r\n", 2, &written, NULL) || written != 2)
                        {
                            ret = FALSE;
                        }
                    }
                    if (!WriteFile(file, "end-of-params", sizeof("end-of-params") - 1, &written, NULL) || written != sizeof("end-of-params") - 1 ||
                        !WriteFile(file, "\r\n", 2, &written, NULL) || written != 2)
                    {
                        ret = FALSE;
                    }
                }
                CloseHandle(file);
            }
        }
    }
    return ret;
}

void MyCreateProcess(const char* fileName, BOOL parseCurDir, BOOL addQuotes, const char* salReadmeFile)
{
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi;
    char buf[MAX_PATH];
    char buf2[2 * MAX_PATH + 50];

    if (lstrlen(fileName) >= MAX_PATH || salReadmeFile != NULL && lstrlen(salReadmeFile) >= MAX_PATH)
        return;

    if (parseCurDir)
    {
        char* p;
        lstrcpy(buf, fileName);
        p = buf + lstrlen(buf) - 1;
        while (p > buf && *p != '\\')
            p--;
        if (p >= buf && *p == '\\')
            *p = 0;
    }
    else
        GetSystemDirectory(buf, MAX_PATH); // dame mu systemovy adresar, at neblokuje mazani soucasneho pracovniho adresare

    if (addQuotes)
    {
        if (salReadmeFile != NULL)
            wsprintf(buf2, "\"%s\" -run_notepad \"%s\"", fileName, salReadmeFile);
        else
            wsprintf(buf2, "\"%s\"", fileName);
    }
    si.cb = sizeof(STARTUPINFO);
    CreateProcess(NULL, addQuotes ? buf2 : fileName, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL, buf, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void ProcessResultsInParamsFile(const char* name, BOOL* waitForExec)
{
    HANDLE file = CreateFile(name, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD sizeLo, sizeHi;
        sizeLo = GetFileSize(file, &sizeHi);
        if ((sizeLo != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) &&
            sizeHi == 0 && sizeLo <= 3000)
        {
            char buf[3001];
            DWORD read;
            if (ReadFile(file, buf, sizeLo, &read, NULL) && read == sizeLo)
            {
                int state;
                char readmeFile[MAX_PATH];
                char salamExeFile[MAX_PATH];
                BOOL execViewer;
                BOOL execSalam;
                char* end;
                char* line;
                char* s;

                state = 0; // 0 = pred "end-of-params"; 1 = cteme cmd-line pro viewer; 2 = cteme cmd-line pro Salama;
                           // 3 = cteme exec-viewer a/nebo exec-Salam
                execViewer = FALSE;
                execSalam = FALSE;
                end = buf + read;
                line = buf;
                s = line;
                while (s < end)
                {
                    while (s < end && *s != '\r' && *s != '\n')
                        s++;
                    *s++ = 0; // s == end je jeste platny znak

                    switch (state)
                    {
                    case 0:
                    {
                        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, line, -1, "end-of-params", -1) == CSTR_EQUAL)
                            state = 1;
                        break;
                    }

                    case 1:
                    {
                        lstrcpyn(readmeFile, line, MAX_PATH);
                        state = 2;
                        break;
                    }

                    case 2:
                    {
                        lstrcpyn(salamExeFile, line, MAX_PATH);
                        state = 3;
                        break;
                    }

                    case 3:
                    {
                        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, line, -1, "exec-viewer", -1) == CSTR_EQUAL)
                            execViewer = TRUE;
                        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, line, -1, "exec-salamander", -1) == CSTR_EQUAL)
                            execSalam = TRUE;
                        break;
                    }
                    }

                    while (s < end && (*s == '\r' || *s == '\n'))
                        s++;
                    line = s;
                }
                // pod Vistou (a Win7) mame problem se spustenim dvou softu najednou (notepad a Salamander): oba po
                // spusteni vypadaji aktivovane (cerveny krizek + blikajici kurzor), ale fokus ma prirozene jen jeden;
                // tento problem obchazime tak, ze pokud se maji pustit najednou oba, pusti se jen Salamander, ktery
                // az nabehne spusti teprve notepad (Salamander bude aktivni, tedy zadne problemy se spustenim
                // notepadu nebudou)
                if (execSalam)
                {
                    LoadString(GetModuleHandle(NULL), IDS_STATUS_STARTSALAM, buf, 200);
                    SetDlgItemText(DlgWin, IDC_STATUS, buf);
                }
                else
                {
                    if (execViewer)
                    {
                        LoadString(GetModuleHandle(NULL), IDS_STATUS_STARTNOTEPAD, buf, 200);
                        SetDlgItemText(DlgWin, IDC_STATUS, buf);
                    }
                }
                *waitForExec = execViewer || execSalam;
                if (execViewer)
                {
                    if (execSalam)
                    {
                        MyCreateProcess(salamExeFile, TRUE, TRUE, readmeFile);
                    }
                    else // spoustime jen notepad
                    {
                        wsprintf(buf, "notepad.exe \"%s\"", readmeFile);
                        MyCreateProcess(buf, FALSE, FALSE, NULL);
                    }
                }
                else
                {
                    if (execSalam) // spoustime jen Salamandera
                        MyCreateProcess(salamExeFile, TRUE, TRUE, NULL);
                }
            }
        }
        CloseHandle(file);
    }
}

#endif // FOR_SALAMANDER_SETUP

BOOL CheckWindows2000AndLater()
{
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os);
    if (os.dwPlatformId == VER_PLATFORM_WIN32s ||
        os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ||
        os.dwPlatformId == VER_PLATFORM_WIN32_NT && os.dwMajorVersion <= 4)
    {
        MessageBox(NULL, "This program cannot run on Windows 95/98/Me/NT.", "Information", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

int MyWinMain(struct SCabinet* cabinet)
{
    unsigned char tmpPath[2 * MAX_PATH];
    unsigned char tmpArgs[2 * MAX_PATH];
    unsigned char exeName[MAX_PATH];
    unsigned char setupParams[MAX_PATH];
    unsigned char instPrefix[4];
    unsigned char* ptr;
    unsigned long threadID, exitCode;
    unsigned int i;
    HANDLE hThread;
    MSG msg;
    SHELLEXECUTEINFO sei = {0};
#ifdef FOR_SALAMANDER_SETUP
    char paramsFileName[MAX_PATH];
#endif // FOR_SALAMANDER_SETUP

    if (!CheckWindows2000AndLater())
        return 1;

    tmpPath[0] = 0;
    setupParams[0] = 0;
    i = 0;
    ptr = GetCommandLine();
    // Extract program name from the command line
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;
    if (*ptr == '"')
    {
        ptr++;
        while (i < MAX_PATH && *ptr != '\0' && *ptr != '"')
            exeName[i++] = *ptr++;
    }
    else
        while (i < MAX_PATH && *ptr != '\0' && *ptr != ' ' && *ptr != '\t')
            exeName[i++] = *ptr++;
    exeName[i] = '\0';
    // If the extension is not present, add ".exe"
    if (i < 5 || exeName[i - 4] != '.')
    {
        lstrcpy(exeName, ".exe");
    }

    // if there remains any parameter on command line, parse it
    if (*ptr == '"')
        ptr++;
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;
    if (*ptr != '\0')
    {
        // if it is an option, it should be "help"
        if (*ptr == '/')
        {
            ptr++;
            if (*ptr == '?')
                return Help();

            if (*ptr == 't' || *ptr == 'T')
            {
                // path to temporary directory to hold extracted files
                ptr++;
                while (*ptr == ' ' || *ptr == '\t')
                    ptr++;

                i = 0;
                if (*ptr == '"')
                {
                    ptr++;
                    while (i < MAX_PATH - 1 && *ptr != '"' && *ptr != '\0')
                        tmpPath[i++] = *ptr++;
                    if (*ptr == '"')
                        ptr++;
                }
                else
                    while (i < MAX_PATH - 1 && *ptr != '\0' && *ptr != ' ' && *ptr != '\t')
                        tmpPath[i++] = *ptr++;
                tmpPath[i] = '\0';
                while (*ptr == ' ' || *ptr == '\t')
                    ptr++;
            }

            if (*ptr == '/')
                ptr++;

            if (*ptr == 'x' || *ptr == 'X')
            {
                // parameters we should pass to setup.exe
                ptr++;
                while (*ptr == ' ' || *ptr == '\t')
                    ptr++;

                i = 0;
                while (i < MAX_PATH - 1 && *ptr != '\0')
                    setupParams[i++] = *ptr++;
                setupParams[i] = '\0';
            }
            else
                return HandleError(ERROR_TITLE, ARC_BADPARAMS, 0, NULL);
        }
    }
    if (tmpPath[0] == 0)
    {
        if (GetTempPath(MAX_PATH, tmpPath) == 0)
            return HandleError(ERROR_TITLE, ERROR_TEMPPATH, GetLastError(), NULL);
    }
    lstrcpy(instPrefix, "SFX");
    if (GetTempFileName(tmpPath, instPrefix, 0, tmpName) == 0)
        return HandleError(ERROR_TITLE, ERROR_TEMPNAME, GetLastError(), NULL);
    if (DeleteFile(tmpName) == 0)
        return HandleError(ERROR_TITLE, ERROR_DELFILE, GetLastError(), tmpName);
    if (CreateDirectory(tmpName, NULL) == 0)
        return HandleError(ERROR_TITLE, ERROR_MKDIR, GetLastError(), tmpName);

    InitCommonControls();

#ifdef _DEBUG
    if (InitExtraction("X:\\PROTECH_SFX\\CD450\\PROTECH-450.exe", cabinet))
#else  // _DEBUG
    if (InitExtraction(exeName, cabinet))
#endif // _DEBUG
    {
        RemoveDirectory(tmpName);
        return 1;
    }

    // extracting to current dir - so switch there
    if (SetCurrentDirectory(tmpName) == 0)
    {
        HandleError(ERROR_TITLE, ERROR_MKDIR, GetLastError(), tmpName);
        RemoveDirectory(tmpName);
        return 1;
    }

    terminate = 0;
    DlgWin = NULL;
    ProgressPos = 0;
    CurrentName[0] = '\0';
    CurrentName[100] = '\0';

    hThread = CreateThread(NULL, 0, ExtractArchive, (void*)cabinet, 0, &threadID);
    if (hThread == NULL)
    {
        HandleError(ERROR_TITLE, ERROR_TCREATE, GetLastError(), NULL);
        RemoveTemporaryDir(tmpName);
        return 1;
    }

    DlgWin = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PROGRESS), NULL, DlgProc);
    if (DlgWin == NULL)
    {
        HandleError(ERROR_TITLE, ERROR_DLGCREATE, GetLastError(), NULL);
        RemoveTemporaryDir(tmpName);
        return 1;
    }
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_USER_THREADEXIT)
        {
            char text[200];
            if (WaitForSingleObject(hThread, INFINITE) == WAIT_FAILED)
            {
                DWORD err = GetLastError();
                DestroyWindow(DlgWin);
                DlgWin = NULL;
                HandleError(ERROR_TITLE, ERROR_ERRWAIT, err, NULL);
                RemoveTemporaryDir(tmpName);
                return 1;
            }
            if (!GetExitCodeThread(hThread, &exitCode))
            {
                DWORD err = GetLastError();
                DestroyWindow(DlgWin);
                DlgWin = NULL;
                HandleError(ERROR_TITLE, ERROR_EXITCODE, err, NULL);
                RemoveTemporaryDir(tmpName);
                return 1;
            }
            if (exitCode)
            {
                DestroyWindow(DlgWin);
                DlgWin = NULL;
                RemoveTemporaryDir(tmpName);
                return exitCode;
            }

            LoadString(GetModuleHandle(NULL), ARC_TITLE, text, 200);
            SetWindowText(DlgWin, text);
            ShowWindow(GetDlgItem(DlgWin, IDC_PROGRESS), SW_HIDE);
            ShowWindow(GetDlgItem(DlgWin, IDCANCEL), SW_HIDE);
            LoadString(GetModuleHandle(NULL), IDS_STATUS_STARTSETUP, text, 200);
            SetDlgItemText(DlgWin, IDC_STATUS, text);
            ShowWindow(GetDlgItem(DlgWin, IDC_STATUS), SW_SHOW);

            // jdeme zpracovat zpravy (aby se i pod XP ukazal text IDS_STATUS_STARTSETUP)
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (DlgWin == NULL || !IsWindow(DlgWin) || !IsDialogMessage(DlgWin, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            lstrcpy(tmpPath, tmpName);
            ptr = tmpPath + lstrlen(tmpPath) - 1;
            if (ptr >= tmpPath && *ptr != '\\')
                *++ptr = '\\';

            // pokud najdeme znacku pro x64 instalaci a nebezime na x64 systemu, nepustime instalaci dal
            lstrcpy(ptr + 1, X64MARK_NAME);
            if (FileExists(tmpPath) && !Is64BitWindows())
            {
                char title[500];
                char message[500];
                DestroyWindow(DlgWin);
                DlgWin = NULL;
                LoadString(GetModuleHandle(NULL), ARC_TITLE, title, 500);
                LoadString(GetModuleHandle(NULL), ERROR_X64, message, 500);
                MessageBox(NULL, message, title, MB_OK | MB_ICONEXCLAMATION);
                RemoveTemporaryDir(tmpName);
                return 1;
            }

            lstrcpy(ptr + 1, SETUP_NAME);

            tmpArgs[0] = 0;
            ptr = tmpArgs;

#ifdef FOR_SALAMANDER_SETUP
            paramsFileName[0] = 0;
            {
                // pod Vistou a novejsimi: setup.exe je eskalovany, musime mu predat cesty usera, ktery instalaci spustil (pokud nejde o admina, cesty ziskane po eskalaci budou admina a ne obyc. usera)
                // W2K and XP: when started on limited/restricted account, system offers to run setup.exe as Admin, so params.txt file is needed too
                if (CreateParamsFileIfNeeded(tmpName, paramsFileName))
                {
                    wsprintf(ptr, "/runbysfx 0x%X ", (DWORD)DlgWin);
                    ptr += lstrlen(ptr);
                }
                else
                    paramsFileName[0] = 0;
            }
#endif // FOR_SALAMANDER_SETUP

            lstrcpy(ptr, setupParams);

            sei.cbSize = sizeof(SHELLEXECUTEINFO);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.hwnd = DlgWin;
            sei.lpFile = tmpPath;
            sei.lpParameters = tmpArgs;
            sei.lpDirectory = tmpName;
            sei.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteEx(&sei))
            {
                DWORD err = GetLastError();
                DestroyWindow(DlgWin);
                DlgWin = NULL;
                if (err != ERROR_CANCELLED)
                    HandleError(ERROR_TITLE, ERROR_CREATEPROC, err, tmpPath);
                RemoveTemporaryDir(tmpName);
                return 1;
            }

            ShowWindow(DlgWin, SW_HIDE);

            if (sei.hProcess != NULL)
            {
                DWORD res;
                while (1)
                {
                    res = MsgWaitForMultipleObjects(1, &sei.hProcess, FALSE, INFINITE, QS_ALLINPUT);
                    if (res != WAIT_OBJECT_0 + 1)
                        break;
                    // jdeme zpracovat zpravy
                    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                    {
                        if (DlgWin == NULL || !IsWindow(DlgWin) || !IsDialogMessage(DlgWin, &msg))
                        {
                            TranslateMessage(&msg);
                            DispatchMessage(&msg);
                        }
                    }
                }
                if (res == WAIT_FAILED)
                {
                    DWORD err = GetLastError();
                    DestroyWindow(DlgWin);
                    DlgWin = NULL;
                    HandleError(ERROR_TITLE, ERROR_ERRWAIT, err, tmpPath);
                    CloseHandle(sei.hProcess);
                    RemoveTemporaryDir(tmpName);
                    return 1;
                }
            }
            if (sei.hProcess != NULL)
                CloseHandle(sei.hProcess);
#ifdef FOR_SALAMANDER_SETUP
            if (paramsFileName[0] != 0) // jen W2K/XP pokud user neni admin nebo Vista nebo novejsi + povedl se vytvorit soubor params.txt
            {
                BOOL waitForExec;
                ProcessResultsInParamsFile(paramsFileName, &waitForExec);
                if (waitForExec) // pokud se vubec neco spousti, jinak hned koncime
                {
                    // jdeme zpracovat zpravy az do doby, kdy se aktivuje spoustena aplikace, pak teprve koncime (jinak blbne aktivace spoustenych softu)
                    DWORD ti = GetTickCount();
                    if (!IsWindowVisible(DlgWin))
                        ShowWindow(DlgWin, SW_SHOW); // jen tak pro sychr
                    do
                    {
                        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                        {
                            if (DlgWin == NULL || !IsWindow(DlgWin) || !IsDialogMessage(DlgWin, &msg))
                            {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        }
                        Sleep(100);
                    } while (DlgWin != NULL && GetTickCount() - ti < 10000);
                }
            }
#endif // FOR_SALAMANDER_SETUP
            DestroyWindow(DlgWin);
            DlgWin = NULL;
            break;
        }

        if (DlgWin == NULL || !IsWindow(DlgWin) || !IsDialogMessage(DlgWin, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    RemoveTemporaryDir(tmpName);
    return 0;
}

// ****************************************************************************
// EnableExceptionsOn64
//

// Chceme se dozvedet o SEH Exceptions i na x64 Windows 7 SP1 a dal
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

void WinMainCRTStartup()
{
    int ret;
    struct SCabinet cabinet;

    EnableExceptionsOn64();

    cabinet.file = INVALID_HANDLE_VALUE;

    ret = MyWinMain(&cabinet);
    if (cabinet.file != INVALID_HANDLE_VALUE)
        CloseHandle(cabinet.file);

    ExitProcess(ret);
}
