// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "wndframe.h"

#include <tlhelp32.h>

BOOL GetProcessPath(PROCESSENTRY32* pe32, LPTSTR szPath)
{
    HANDLE hModules;
    MODULEENTRY32 me32 = {0};

    hModules = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe32->th32ProcessID);
    me32.dwSize = sizeof(me32);
    szPath[0] = '\0';

    if (hModules && Module32First(hModules, &me32))
    {
        while (TRUE)
        {
            if (_stricmp(pe32->szExeFile, me32.szModule) == 0 ||
                _stricmp("", me32.szModule) == 0)
            {
                lstrcpy(szPath, me32.szExePath);
                break;
            }

            if (!Module32Next(hModules, &me32))
                break;
        }
    }

    if (hModules)
        CloseHandle(hModules);

    return (szPath[0] != '\0');
}

BOOL KillSalamandersAux(const char* path)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if (_stricmp(entry.szExeFile, "salamand.exe") == 0)
            {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION + PROCESS_VM_READ + PROCESS_TERMINATE, FALSE, entry.th32ProcessID);

                char processPath[MAX_PATH];
                GetProcessPath(&entry, processPath);
                if (lstrcmpi(processPath, path) == 0)
                {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (h != NULL)
                    {
                        TerminateProcess(h, 666);
                        CloseHandle(h);
                    }
                }

                CloseHandle(hProcess);
            }
        }
    }

    CloseHandle(snapshot);
    return TRUE;
}

BOOL StartSalamander(const char* path)
{
    SHELLEXECUTEINFO se;
    memset(&se, 0, sizeof(SHELLEXECUTEINFO));
    se.cbSize = sizeof(SHELLEXECUTEINFO);
    se.fMask = SEE_MASK_IDLIST;
    se.lpVerb = "open";
    se.hwnd = FrameWindow.HWindow;
    se.nShow = SW_SHOWNORMAL;
    se.lpFile = path;
    se.lpParameters = "-i 2"; // modra ikona
    ShellExecuteEx(&se);
    return TRUE;
}

BOOL KillSalamanders()
{
    KillSalamandersAux(Data.FullSalamanderExeFile);
    return TRUE;
}

BOOL RestartSalamander()
{
    KillSalamanders();
    FrameWindow.OnSave();
    StartSalamander(Data.FullSalamanderExeFile);
    return TRUE;
}
