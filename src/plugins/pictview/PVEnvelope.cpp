// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <stdio.h>

#include "PVMessage.h"
#include "PixelAccess.h"
#include "Thumbnailer.h"

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))

extern "C" WINUSERAPI PVCODE WINAPI PVSetParam(const char*(WINAPI* GetExtTextProc)(int msgID));

extern const char* Strings[450];
extern char* StringsBuf;
extern CSalamanderThumbnailMaker Thumbnailer;

TCHAR SalamanderMutex[64];
DWORD MainThreadID;

CPVWrapper PVWrapper;

DWORD WINAPI CheckThreadProc(LPVOID)
{
    for (;;)
    {
        HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, SalamanderMutex);
        if (!hMutex)
        {
            // Hmmm, Salamander died
            break;
        }
        CloseHandle(hMutex);
        Sleep(1000);
    }

    PostThreadMessage(MainThreadID, WM_QUIT, 0, 0);

    return TRUE;
}

// This callback provides strings coming from the Salamander process to PVW32Cnv.dll
// Salamander provides translated messages, this allows using single PVW32Cnv.dll
const char* WINAPI GetExtTextProc(int msgID)
{
    if ((msgID >= 0) && (msgID <= SizeOf(Strings)))
    {
        const char* ret = Strings[msgID];
        return ret ? ret : "";
    }
    return "";
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

#ifdef _CONSOLE
int _tmain(int argc, TCHAR* argv[])
#else
int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPTSTR lpCmdLine, int /*nCmdShow*/)
#endif
{
    DWORD CheckThreadID;
    HANDLE hCheckThread;
    HANDLE hEvent;
    TCHAR eventName[32];
    MSG msg;

    EnableExceptionsOn64();

    // nechci zadne kriticke chyby jako "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

#ifdef _CONSOLE
    if (argc != 2)
    {
#else
    if (*lpCmdLine == 0)
    {
#endif
        return -1;
    }
    /*  printf("Cmdline arg #: %d\n", argc);
  for (int i = 0; i < argc; i++)
    printf("Cmdline arg: %s\n", argv[i]);*/

#ifdef _CONSOLE
    _tcsncpy_s(SalamanderMutex, argv[1], _TRUNCATE);
#else
    _tcsncpy_s(SalamanderMutex, lpCmdLine, _TRUNCATE);
#endif

    // PVSetParam unlocks PVW32Cnv.dll for Salamander and install custom text provider
    memset(Strings, 0, sizeof(Strings));
    PVSetParam(GetExtTextProc);

    MainThreadID = GetCurrentThreadId();

    // Make thread to check whether Salamander is still alive
    hCheckThread = CreateThread(NULL, 0, CheckThreadProc, NULL, 0, &CheckThreadID);

    // Main message loop
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        switch (msg.message)
        {
        case WM_QUIT:
            return 0;

        case WM_USER:
#ifdef _CONSOLE
            printf("Received WM_USER\n");
#endif

            _sntprintf(eventName, SizeOf(eventName), _T("%s_ev%x"), SalamanderMutex, msg.lParam);
            hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, eventName);
            _ASSERTE(hEvent);
            PVMessage::HandleMessage(msg.wParam, msg.lParam, hEvent);

            SetEvent(hEvent);
            CloseHandle(hEvent);
            break;

        case WM_USER + 1:
            // Handle messages that don't need to send response, e.g. PVMSG_CloseHandle
#ifdef _CONSOLE
            printf("Received WM_USER+1\n");
#endif
            PVMessage::HandlePostedMessage(msg.wParam, msg.lParam);
            break;

        default:
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // This should be done when the application quits
    if (StringsBuf)
    {
        free(StringsBuf);
    }
    Thumbnailer.Clear(-1);
    return 0;
}
