// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <shlobj.h>

#include "lstrfix.h"

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

BYTE LowerCase[256];
HANDLE Heap = NULL;

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// callback, ktery vraci jmena oznacenych souboru pro vytvareni nasl. interfacu
typedef const char* (*CEnumFileNamesFunction)(int index, void* param);

void my_memcpy(void* dst, const void* src, int len)
{
    char* d = (char*)dst;
    const char* s = (char*)src;
    while (len--)
        *d++ = *s++;
}

// for VS2019
#pragma intrinsic(memcpy) // abych mohli prekladat s optimalizaci na rychlost a prekladac nerval
#pragma function(memcpy)  // error C2169:  'memcpy': intrinsic function, cannot be defined
void* memcpy(void* dst, const void* src, size_t len)
{
    char* d = (char*)dst;
    const char* s = (char*)src;
    while (len--)
        *d++ = *s++;
    return dst;
}

#pragma optimize("", off)
void my_zeromem(void* dst, int len)
{
    char* d = (char*)dst;
    while (len--)
        *d++ = 0;
}
#pragma optimize("", on)

int StrNICmp(const char* s1, const char* s2, int n)
{
    int res;
    while (n--)
    {
        res = (unsigned)LowerCase[*s1++] - (unsigned)LowerCase[*s2++];
        if (res != 0)
            return (res < 0) ? -1 : 1; // < a >
    }
    return 0;
}

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

int GetRootPath(char* root, const char* path)
{
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        int len = (int)(s - path);
        my_memcpy(root, path, len);
        root[len] = '\\';
        root[len + 1] = 0;
        return len + 1;
    }
    else
    {
        root[0] = path[0];
        root[1] = ':';
        root[2] = '\\';
        root[3] = 0;
        return 3;
    }
}

LPITEMIDLIST GetItemIdListForFileName(LPSHELLFOLDER folder, const char* fileName)
{
    OLECHAR olePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fileName, -1, olePath, MAX_PATH);
    olePath[MAX_PATH - 1] = 0;

    LPITEMIDLIST pidl;
    ULONG chEaten;
    HRESULT ret;
    if (SUCCEEDED((ret = folder->ParseDisplayName(NULL, NULL, olePath, &chEaten,
                                                  &pidl, NULL))))
    {
        return pidl;
    }
    else
    {
        //    TRACE_E("ParseDisplayName error: " << hex << ret);
        return NULL;
    }
}

void DestroyItemIdList(LPITEMIDLIST* list, int itemsInList)
{
    IMalloc* alloc;
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        int i;
        for (i = 0; i < itemsInList; i++)
        {
            if (list[i] != NULL && alloc->DidAlloc(list[i]) == 1)
            {
                alloc->Free(list[i]);
            }
        }
        alloc->Release();
    }
    HeapFree(Heap, 0, list);
}

LPITEMIDLIST* CreateItemIdList(LPSHELLFOLDER folder, int files,
                               CEnumFileNamesFunction nextFile, void* param,
                               UINT& itemsInList)
{
    if (files <= 0)
        return NULL;

    LPITEMIDLIST* list = (LPITEMIDLIST*)HeapAlloc(Heap, 0, sizeof(LPITEMIDLIST) * files);
    if (list == NULL)
    {
        //    TRACE_E(LOW_MEMORY);
        return NULL;
    }
    my_zeromem(list, sizeof(ITEMIDLIST*) * files);

    LPITEMIDLIST pidl = NULL;
    int i;
    for (i = 0; i < files; i++)
    {
        pidl = GetItemIdListForFileName(folder, nextFile(i, param));
        if (pidl != NULL)
            list[i] = pidl;
        else
            break; // nejaka chyba
    }

    if (pidl == NULL)
    {
        DestroyItemIdList(list, files);
        itemsInList = 0;
        return NULL;
    }
    else
    {
        itemsInList = files;
        return list;
    }
}

BOOL GetShellFolder(const char* dir, IShellFolder*& shellFolderObj, LPITEMIDLIST& pidlFolder)
{
    shellFolderObj = NULL;
    pidlFolder = NULL;
    HRESULT ret;
    LPSHELLFOLDER desktop;
    if (SUCCEEDED((ret = SHGetDesktopFolder(&desktop))))
    {
        int rootFolder;
        if (dir[0] != '\\')
            rootFolder = CSIDL_DRIVES; // normalni cesta
        else
            rootFolder = CSIDL_NETWORK; // UNC - sitove zdroje
        LPITEMIDLIST rootFolderID;
        if (SUCCEEDED((ret = SHGetSpecialFolderLocation(NULL, rootFolder, &rootFolderID))))
        {
            if (SUCCEEDED((ret = desktop->BindToObject(rootFolderID, NULL,
                                                       IID_IShellFolder, (LPVOID*)&shellFolderObj))))
            {
                char root[MAX_PATH];
                GetRootPath(root, dir);
                if (lstrlen(root) < lstrlen(dir)) // neni to root cesta
                {
                    lstrcpy(root, dir);
                    char* name = root + lstrlen(root);
                    if (*--name == '\\')
                        *name = 0;
                    else
                        name++;
                    while (*--name != '\\')
                        ;
                    char c = *++name;
                    *name = 0;
                    LPITEMIDLIST pidlUpperDir = GetItemIdListForFileName(shellFolderObj, root);
                    LPSHELLFOLDER folder2;
                    if (pidlUpperDir != NULL &&
                        SUCCEEDED((ret = shellFolderObj->BindToObject(pidlUpperDir, NULL,
                                                                      IID_IShellFolder, (LPVOID*)&folder2))))
                    {
                        shellFolderObj->Release();
                        shellFolderObj = folder2;
                        *name = c;
                        dir = name;
                    }
                    //          else TRACE_E("BindToObject error: " << hex << ret); // dir zustava
                    IMalloc* alloc;
                    if (pidlUpperDir != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
                    {
                        if (alloc->DidAlloc(pidlUpperDir) == 1)
                            alloc->Free(pidlUpperDir);
                        alloc->Release();
                    }
                }
                else
                {
                    if (rootFolder == CSIDL_DRIVES)
                    {
                        LPENUMIDLIST enumIDList;
                        if (SUCCEEDED((ret = shellFolderObj->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_INCLUDEHIDDEN,
                                                                         &enumIDList))))
                        {
                            ULONG celt;
                            LPITEMIDLIST idList;
                            STRRET str;
                            enumIDList->Reset();
                            IMalloc* alloc;
                            if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                            {
                                while (1)
                                {
                                    ret = enumIDList->Next(1, &idList, &celt);
                                    if (ret == NOERROR)
                                    {
                                        ret = shellFolderObj->GetDisplayNameOf(idList, SHGDN_FORPARSING, &str);
                                        if (ret == NOERROR)
                                        {
                                            char buf[MAX_PATH];
                                            char* name;
                                            switch (str.uType)
                                            {
                                            case STRRET_CSTR:
                                                name = str.cStr;
                                                break;
                                            case STRRET_OFFSET:
                                                name = (char*)idList + str.uOffset;
                                                break;
                                            case STRRET_WSTR:
                                            {
                                                WideCharToMultiByte(CP_ACP, 0, str.pOleStr,
                                                                    -1, buf, MAX_PATH, NULL, NULL);
                                                buf[MAX_PATH - 1] = 0;
                                                name = buf;
                                                if (alloc->DidAlloc(str.pOleStr) == 1)
                                                    alloc->Free(str.pOleStr);
                                                break;
                                            }
                                            default:
                                                name = NULL;
                                            }

                                            if (name != NULL)
                                            {
                                                if (lstrlen(name) <= 3 && StrNICmp(name, root, 2) == 0) // name = "c:" nebo "c:\"
                                                {
                                                    pidlFolder = idList;
                                                    break; // pidl nalezen (ziskan)
                                                }
                                            }
                                        }
                                        if (alloc->DidAlloc(idList) == 1)
                                            alloc->Free(idList);
                                    }
                                    else
                                        break;
                                }
                                alloc->Release();
                            }
                            enumIDList->Release();
                        }
                    }
                    else
                    {
                        if (rootFolder == CSIDL_NETWORK) // musime ziskat slozite pidl, jinak nechodi mapovani
                        {
                            BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // ceka uz ?
                            HCURSOR oldCur;
                            if (setWait)
                                oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                            *(root + lstrlen(root) - 1) = 0;
                            dir = root;
                            char* s = root + 2;
                            while (*s != '\\')
                                s++;
                            *s = 0;
                            LPITEMIDLIST pidl = GetItemIdListForFileName(shellFolderObj, root);
                            *s = '\\';
                            LPSHELLFOLDER folder2;
                            if (pidl != NULL &&
                                SUCCEEDED((ret = shellFolderObj->BindToObject(pidl, NULL,
                                                                              IID_IShellFolder, (LPVOID*)&folder2))))
                            {
                                LPENUMIDLIST enumIDList;
                                if (SUCCEEDED((ret = folder2->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN,
                                                                          &enumIDList))))
                                {
                                    ULONG celt;
                                    LPITEMIDLIST idList;
                                    STRRET str;
                                    enumIDList->Reset();
                                    IMalloc* alloc;
                                    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                                    {
                                        while (1)
                                        {
                                            ret = enumIDList->Next(1, &idList, &celt);
                                            if (ret == NOERROR)
                                            {
                                                ret = folder2->GetDisplayNameOf(idList, SHGDN_FORPARSING, &str);
                                                if (ret == NOERROR)
                                                {
                                                    char buf[MAX_PATH];
                                                    char* name;
                                                    switch (str.uType)
                                                    {
                                                    case STRRET_CSTR:
                                                        name = str.cStr;
                                                        break;
                                                    case STRRET_OFFSET:
                                                        name = (char*)idList + str.uOffset;
                                                        break;
                                                    case STRRET_WSTR:
                                                    {
                                                        WideCharToMultiByte(CP_ACP, 0, str.pOleStr,
                                                                            -1, buf, MAX_PATH, NULL, NULL);
                                                        buf[MAX_PATH - 1] = 0;
                                                        name = buf;
                                                        if (alloc->DidAlloc(str.pOleStr) == 1)
                                                            alloc->Free(str.pOleStr);
                                                        break;
                                                    }
                                                    default:
                                                        name = NULL;
                                                    }

                                                    if (name != NULL)
                                                    {
                                                        if (*(name + lstrlen(name) - 1) == '\\')
                                                            *(name + lstrlen(name) - 1) = 0;
                                                        if (StrICmp(name, root) == 0)
                                                        {
                                                            pidlFolder = idList;
                                                            LPSHELLFOLDER swap = shellFolderObj;
                                                            shellFolderObj = folder2;
                                                            folder2 = swap;
                                                            break; // pidl nalezen (ziskan)
                                                        }
                                                    }
                                                }
                                                if (alloc->DidAlloc(idList) == 1)
                                                    alloc->Free(idList);
                                            }
                                            else
                                                break;
                                        }
                                        alloc->Release();
                                    }
                                    enumIDList->Release();
                                }
                                folder2->Release();
                            }
                            IMalloc* alloc;
                            if (pidl != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
                            {
                                if (alloc->DidAlloc(pidl) == 1)
                                    alloc->Free(pidl);
                                alloc->Release();
                            }
                            if (setWait)
                                SetCursor(oldCur);
                        }
                    }
                }
                if (pidlFolder == NULL)
                    pidlFolder = GetItemIdListForFileName(shellFolderObj, dir);

                // shellFolderObj + pidlFolder  -> dohromady "dir" folder
            }
            //      else TRACE_E("BindToObject error: " << hex << ret);
            IMalloc* alloc;
            if (SUCCEEDED(CoGetMalloc(1, &alloc)))
            {
                if (alloc->DidAlloc(rootFolderID) == 1)
                    alloc->Free(rootFolderID);
                alloc->Release();
            }
        }
        //    else TRACE_E("SHGetSpecialFolderLocation error: " << hex << ret);
        desktop->Release();
    }
    //  else TRACE_E("SHGetDesktopFolder error: " << hex << ret);
    if (shellFolderObj != NULL && pidlFolder != NULL)
        return TRUE;
    else
    {
        if (shellFolderObj != NULL)
            shellFolderObj->Release();
        if (pidlFolder != NULL)
        {
            IMalloc* alloc;
            if (SUCCEEDED(CoGetMalloc(1, &alloc)))
            {
                if (alloc->DidAlloc(pidlFolder) == 1)
                    alloc->Free(pidlFolder);
                alloc->Release();
            }
        }
        return FALSE;
    }
}

IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* rootDir, int files,
                                   CEnumFileNamesFunction nextFile, void* param)
{
    IContextMenu2* contextMenu2Obj = NULL;
    IShellFolder* shellFolderObj;
    LPITEMIDLIST pidlFolder;
    if (GetShellFolder(rootDir, shellFolderObj, pidlFolder))
    {
        HRESULT ret;
        LPSHELLFOLDER folder;
        if (SUCCEEDED((ret = shellFolderObj->BindToObject(pidlFolder, NULL,
                                                          IID_IShellFolder, (LPVOID*)&folder))))
        {
            UINT itemsInList;
            LPITEMIDLIST* list;

            list = CreateItemIdList(folder, files, nextFile, param, itemsInList);
            if (list != NULL)
            {
                IContextMenu* contextMenuObj;
                if (SUCCEEDED((ret = folder->GetUIObjectOf(hOwnerWindow, itemsInList, (LPCITEMIDLIST*)list,
                                                           IID_IContextMenu, NULL,
                                                           (LPVOID*)&contextMenuObj))))
                {
                    if (!SUCCEEDED((ret = contextMenuObj->QueryInterface(IID_IContextMenu2,
                                                                         (void**)&contextMenu2Obj))))
                    {
                        //            TRACE_E("QueryInterface error: " << hex << ret);
                    }
                    contextMenuObj->Release();
                }
                //        else TRACE_E("GetUIObjectOf error: " << hex << ret);
                DestroyItemIdList(list, itemsInList);
            }
            folder->Release();
        }
        //    else TRACE_E("BindToObject error: " << hex << ret);

        IMalloc* alloc;
        if (pidlFolder != NULL && SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            if (alloc->DidAlloc(pidlFolder) == 1)
                alloc->Free(pidlFolder);
            alloc->Release();
        }
        shellFolderObj->Release();
    }
    return contextMenu2Obj;
}

const char* ReturnNameFromParam(int, void* param)
{
    return (const char*)param;
}

void DoExecuteAssociation(HWND hWindow, const char* path, const char* name)
{
    IContextMenu2* menu = CreateIContextMenu2(hWindow, path, 1,
                                              ReturnNameFromParam, (void*)name);
    if (menu != NULL)
    {
        HMENU h = CreatePopupMenu();
        if (h != NULL)
        {
            menu->QueryContextMenu(h, 0, 0, -1, CMF_NORMAL | CMF_EXPLORE);

            UINT cmd = GetMenuDefaultItem(h, FALSE, GMDI_GOINTOPOPUPS);
            if (cmd == -1) // nenasli jsme default item -> zkusime hledat jen mezi verby
            {
                DestroyMenu(h);
                h = CreatePopupMenu();
                if (h != NULL)
                {
                    menu->QueryContextMenu(h, 0, 0, -1, CMF_VERBSONLY | CMF_DEFAULTONLY);

                    cmd = GetMenuDefaultItem(h, FALSE, GMDI_GOINTOPOPUPS);
                    if (cmd == -1)
                        cmd = 0; // zkusime "default verb" (index 0)
                }
            }
            if (cmd != -1)
            {
                CMINVOKECOMMANDINFO ici;
                ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
                ici.fMask = 0;
                ici.hwnd = hWindow;
                ici.lpVerb = MAKEINTRESOURCE(cmd);
                ici.lpParameters = NULL;
                ici.lpDirectory = path;
                ici.nShow = SW_SHOWNORMAL;
                ici.dwHotKey = 0;
                ici.hIcon = 0;

                menu->InvokeCommand(&ici);
            }
            DestroyMenu(h);
        }
        menu->Release();
    }
}

#pragma optimize("", off)
void ExecuteAssociation(HWND hWindow, const char* fileName)
{
    char defDir[MAX_PATH + 200];
    const char* s = fileName;
    while (*s != 0)
        s++;
    while (s > fileName && *s != '\\')
        s--;
    if (*s == '\\') // should be always true (full file name is specified)
    {
        const char* src = fileName;
        char* dst = defDir;
        while (src < s)
            *dst++ = *src++;
        if (dst - defDir == 2 && *(dst - 1) == ':')
            *dst++ = '\\'; // not-UNC root paths ends with '\\'
        *dst = 0;

        DoExecuteAssociation(hWindow, defDir, s + 1);
    }
}
#pragma optimize("", on)

// ExitProcess on any exception
LONG __stdcall MyUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
{
    //  DWORD written;
    //  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
    //            "SALOPEN: An exception has occured!\n",
    //            34, &written, NULL);
    //  ExitProcess(666);
    TerminateProcess(GetCurrentProcess(), 666); // tvrdsi exit (tenhle jeste neco vola)
    return EXCEPTION_EXECUTE_HANDLER;
}

const char* MY_WINDOW_CLASSNAME = "SalOpen";

char FileName[MAX_PATH + 200]; // + 200 is reserve for MS filenames (MAX_PATH limit is sometimes broken)

LRESULT CALLBACK MyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        PostMessage(hwnd, WM_APP + 1, 0, 0);
        break;
    }

    case WM_APP + 1:
    {
        ExecuteAssociation(hwnd, FileName);
        if (SetTimer(hwnd, 666, 10000, NULL) == 0)
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;
    }

    case WM_TIMER:
    {
        if (wParam == 666)
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        break;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        break;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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
        FIsWow64Process isWow64 = (FIsWow64Process)GetProcAddress(hDLL, "IsWow64Process");                                                      // Min: XP SP2
        FSetProcessUserModeExceptionPolicy set = (FSetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "SetProcessUserModeExceptionPolicy"); // Min: Vista with hotfix
        FGetProcessUserModeExceptionPolicy get = (FGetProcessUserModeExceptionPolicy)GetProcAddress(hDLL, "GetProcessUserModeExceptionPolicy"); // Min: Vista with hotfix
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
    EnableExceptionsOn64();
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL); // to run as fast as Explorer
    SetErrorMode(SEM_FAILCRITICALERRORS);
    SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);

    if (OleInitialize(NULL) != S_OK)
        ExitProcess(2);

    if ((Heap = GetProcessHeap()) == NULL)
        ExitProcess(2);

    int i;
    for (i = 0; i < 256; i++)
        LowerCase[i] = (char)(UINT_PTR)CharLower((LPTSTR)(UINT_PTR)i);

    char* cmdline = GetCommandLine();
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
    // get params
    DWORD PID, pos;
    DWORD_PTR fileMappingHandle;
    BOOL validPID = FALSE;
    BOOL validFileMappingHandle = FALSE;
    BOOL validPos = FALSE;
    while (1)
    {
        // skip spaces
        while (*cmdline == ' ' || *cmdline == '\t')
            cmdline++;
        if (*cmdline == '\0')
            break;
        // read PID and file-mapping-handle
        if (*cmdline > '9' || *cmdline < '0')
            break;
        DWORD_PTR retBase = 0;
        while (*cmdline <= '9' && *cmdline >= '0')
            retBase = retBase * 10 + *cmdline++ - '0';
        if (*cmdline != ' ' && *cmdline != '\t' && *cmdline != '\0')
            break;
        // set PID and then file-mapping-handle
        if (!validPID)
        {
            validPID = TRUE;
            PID = (DWORD)retBase;
        }
        else
        {
            if (!validFileMappingHandle)
            {
                validFileMappingHandle = TRUE;
                fileMappingHandle = retBase;
            }
            else
            {
                if (!validPos)
                {
                    validPos = TRUE;
                    pos = (DWORD)retBase;
                }
                else
                    break;
            }
        }
    }

    if (!validPID || !validFileMappingHandle || !validPos || *cmdline != 0)
    {
        //    DWORD written;
        //    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
        //              "SALOPEN: Opener for Open Salamander, (C) 2023 Open Salamander Authors\nUsage: salopen <PID> <file-mapping-handle>\n",
        //              98, &written, NULL);
        OleUninitialize();
        ExitProcess(1);
    }

    // get file-name from file-mapping (shared memory)
    FileName[0] = 0;
    HANDLE sendingProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, PID);
    HANDLE sendingFM = (HGLOBAL)fileMappingHandle;

    HANDLE fm;
    if (sendingProcess != NULL &&
        DuplicateHandle(sendingProcess, sendingFM, // sending-process file-mapping
                        GetCurrentProcess(), &fm,  // this process file-mapping
                        0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        void* ptr = MapViewOfFile(fm, FILE_MAP_READ, 0, 0, 0); // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
        if (ptr != NULL)
        {
            // lets copy file name from file-mapping (shared memory)
            char* s = (char*)ptr;
            char* d = FileName;
            while (*s != 0 && d - FileName < sizeof(FileName) + 1)
                *d++ = *s++;
            *d = 0; // file name is null terminated
            UnmapViewOfFile(ptr);
        }
        CloseHandle(fm);
    }
    CloseHandle(sendingProcess);

    if (FileName[0] == 0) // error reading shared memory
    {
        OleUninitialize();
        ExitProcess(1);
    }

    // vytvorime skryte okno a z nej spustime menu
    // pod W2K+ uz asi neni potreba
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS CWindowClass;
    CWindowClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    CWindowClass.lpfnWndProc = MyWindowProc;
    CWindowClass.cbClsExtra = 0;
    CWindowClass.cbWndExtra = 0;
    CWindowClass.hInstance = hInstance;
    CWindowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    CWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    CWindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    CWindowClass.lpszMenuName = NULL;
    CWindowClass.lpszClassName = MY_WINDOW_CLASSNAME;
    RegisterClass(&CWindowClass);

    HWND HWindow = CreateWindow(MY_WINDOW_CLASSNAME,
                                "SalOpen",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                GET_X_LPARAM(pos), GET_Y_LPARAM(pos), 1, 1,
                                NULL,
                                NULL,
                                hInstance,
                                NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    OleUninitialize();
    ExitProcess(0);
}
