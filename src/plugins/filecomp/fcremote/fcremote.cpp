// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

HINSTANCE DLLInstance = NULL;

void my_memcpy(void* dst, const void* src, int len)
{
    char* d = (char*)dst;
    const char* s = (char*)src;
    while (len--)
        *d++ = *s++;
}

#pragma optimize("", off)
void my_zeromem(void* dst, int len)
{
    char* d = (char*)dst;
    while (len--)
        *d++ = 0;
}
#pragma optimize("", on)

const char*
Concatenate(const char* string1, const char* string2)
{
    static char buffer[5120];
    static int iterator = 0;

    int len1 = lstrlen(string1);
    int len2 = lstrlen(string2);

    if (len1 + len2 >= 5120)
        return "STRING TOO LONG";
    if (iterator + len1 + len2 >= 5120)
        iterator = 0;

    const char* ret = buffer + iterator;
    my_memcpy(buffer + iterator, string1, len1);
    iterator += len1;
    my_memcpy(buffer + iterator, string2, len2);
    iterator += len2;

    buffer[iterator++] = 0;
    return ret;
}

const char*
LoadStr(int resID)
{
    const char* ret;
    switch (resID)
    {
    case IDS_SPLERROR:
        ret = "File Comparator - Error";
        break;
    case IDS_INVALIDARGS:
        ret = "Invalid arguments. Usage:\n\tfcremote.exe [options] first second\nOptions are:\n\t-w\tWait until File Comparator closes.\n\t--wait\tWait until File Comparator closes.";
        break;
    case IDS_MSGERR:
        ret = "Cannot send message to File Comparator plugin.";
        break;
    case IDS_LAUNCHSAL:
        ret = "Unable to launch Open Salamander.";
        break;
    case IDS_MSGERR2:
        ret = "Cannot send message to File Comparator plugin. Ensure 'Load plugin on Open Salamander start' option is set in File Comparator configuration.";
        break;
    default:
        ret = "ERROR LOADING STRING";
    }
    return ret;
}

inline int IsSpace(char c) { return c == ' ' || c == '\t'; }

int RemoveQuotes(char* dest, const char* source, int len)
{
    int d = 0, s = 0;
    while (s < len)
    {
        if (source[s] == '"')
            s++;
        else
            dest[d++] = source[s++];
    }
    return d;
}

BOOL MakeArgv(const char* commandLine, char argv[2][MAX_PATH], int& argc,
              int maxlen, int maxarg)
{
    argc = 0;
    const char* start = commandLine;
    while (*start)
    {
        // orizneme white-space na zacatku
        while (*start && IsSpace(*start))
            start++;
        if (!*start || argc >= maxarg)
            break;
        const char* end = start;
        // najdeme konec tokenu
        while (*end && !IsSpace(*end))
        {
            if (*end++ == '"')
            {
                while (*end && *end != '"')
                    end++;
                if (end)
                    end++;
                else
                    end = start + lstrlen(start);
            }
        }
        // pridame token do pole
        int len = RemoveQuotes(argv[argc], start, min((int)(end - start), maxlen - 1));
        argv[argc++][len] = 0;
        start = end;
    }
    return *start == 0;
}

BOOL PathAppend(LPTSTR pPath, LPCTSTR pMore)
{
    if (pPath == NULL || pMore == NULL)
    {
        TRACE_E("pPath == NULL || pMore == NULL");
        return FALSE;
    }
    if (pMore[0] == 0)
    {
        TRACE_E("pMore[0] == 0");
        return TRUE;
    }
    int len = lstrlen(pPath);
    // ostrim zpetne lomitko pred pripojenim
    if (len > 1 && pPath[len - 1] != '\\' && pMore[0] != '\\')
    {
        pPath[len] = '\\';
        len++;
    }
    lstrcpy(pPath + len, pMore);
    return TRUE;
}

BOOL PathRemoveFileSpec(LPTSTR pszPath)
{
    if (pszPath == NULL)
    {
        TRACE_E("pszPath == NULL");
        return FALSE;
    }
    int len = lstrlen(pszPath);
    char* iterator = pszPath + len - 1;
    while (iterator >= pszPath)
    {
        if (*iterator == '\\')
        {
            if (iterator - 1 < pszPath || *(iterator - 1) == ':')
                iterator++;
            *iterator = 0;
            break;
        }
        iterator--;
    }
    return TRUE;
}

#ifndef ASFW_ANY
#define ASFW_ANY ((DWORD)-1)
#endif

int RemoteCompareFiles(HINSTANCE hInstance, LPTSTR lpCmdLine)
{
    /*
  char spl[MAX_PATH];
  GetModuleFileName(hInstance, spl, MAX_PATH);
  PathRemoveFileSpec(spl); // odeberem fcremote.exe
  PathAppend(spl, "filecomp.spl");
  DLLInstance = LoadLibraryEx(spl, NULL, LOAD_LIBRARY_AS_DATAFILE);
*/
    char argv[4][MAX_PATH];
    int argc;
    int first, second;
    BOOL wait = FALSE;

    // pripavime argv
    BOOL argOK = MakeArgv(lpCmdLine, argv, argc, MAX_PATH, 4) &&
                 3 <= argc && argc <= 4;
    if (argOK)
    {
        if (argc == 3)
        {
            first = 1;
            second = 2;
        }
        else
        {
            if (lstrcmp(argv[1], "-w") == 0 || lstrcmp(argv[1], "--wait") == 0)
                wait = TRUE;
            else
                argOK = FALSE;
            first = 2;
            second = 3;
        }
    }

    if (!argOK)
    {
        MessageBox(NULL, LoadStr(IDS_INVALIDARGS), LoadStr(IDS_SPLERROR), MB_OK | MB_ICONERROR);
        if (DLLInstance)
            FreeLibrary(DLLInstance);
        return -1;
    }

    HANDLE releaseEvent = NULL;
    BOOL firstTry = TRUE;
    BOOL ret = -1;
    while (1)
    {
        CMessageCenter mc(MessageCenterName, TRUE);
        if (!mc.IsGood())
        {
            if (firstTry)
            {
                // zkusime pustit salamandera
                char sal[MAX_PATH];
                GetModuleFileName(hInstance, sal, MAX_PATH);
                PathRemoveFileSpec(sal); // fcremote.exe
                PathRemoveFileSpec(sal); // filecomp
                PathRemoveFileSpec(sal); // plugins
                PathAppend(sal, "salamand.exe");

                STARTUPINFO si;
                PROCESS_INFORMATION pi;
                my_zeromem(&si, sizeof(STARTUPINFO));
                si.cb = sizeof(STARTUPINFO);
                si.lpTitle = NULL;
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_SHOWNORMAL;
                if (!CreateProcess(sal, NULL, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
                {
                    MessageBox(NULL, LoadStr(IDS_LAUNCHSAL), LoadStr(IDS_SPLERROR), MB_OK | MB_ICONERROR);
                    break;
                }
                HANDLE started =
                    CreateEvent(NULL, TRUE, FALSE, StartedEventName);
                WaitForSingleObject(started, 5000);
                CloseHandle(started);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                firstTry = FALSE;
                continue; // zkusime znova s nastartovanym salamanderem
            }
            MessageBox(NULL, LoadStr(IDS_MSGERR2), LoadStr(IDS_SPLERROR), MB_OK | MB_ICONERROR);
            break;
        }

        CRCMessage msg;
        msg.Size = sizeof(msg);
        lstrcpyn(msg.Path1, argv[first], MAX_PATH);
        lstrcpyn(msg.Path2, argv[second], MAX_PATH);
        msg.Path1[MAX_PATH - 1];
        msg.Path2[MAX_PATH - 1];
        GetCurrentDirectory(MAX_PATH, msg.CurrentDirectory);

        if (wait)
        {
            wsprintf(msg.ReleaseEvent, "FCREMOTE%X", GetCurrentProcessId());
            releaseEvent = CreateEvent(NULL, TRUE, FALSE, msg.ReleaseEvent);
        }
        else
            *msg.ReleaseEvent = 0;

        AllowSetForegroundWindow(ASFW_ANY);
        if (!mc.SendMessage(&msg, 5000))
        {
            MessageBox(NULL, LoadStr(IDS_MSGERR), LoadStr(IDS_SPLERROR), MB_OK | MB_ICONERROR);
            break;
        }
        ret = 0;
        break;
    }
    if (DLLInstance)
        FreeLibrary(DLLInstance);
    if (releaseEvent)
    {
        WaitForSingleObject(releaseEvent, INFINITE);
        CloseHandle(releaseEvent);
    }
    return ret;
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

// vyzaduje VC2008
void* __cdecl operator new(size_t size)
{
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void __cdecl operator delete(void* ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}

// vyzaduje VC2015
void* __cdecl operator new[](size_t size)
{
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void __cdecl operator delete[](void* ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}

void WinMainCRTStartup()
{
    EnableExceptionsOn64();
    // nechci zadne kriticke chyby jako "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    int ret = RemoteCompareFiles(GetModuleHandle(NULL), GetCommandLine());
    ExitProcess(ret);
}
