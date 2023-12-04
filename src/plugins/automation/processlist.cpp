// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander

	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors

	processlist.cpp
	Windows process list.
*/

#include "precomp.h"
#include "processlist.h"

BOOL WindowBelongsToProcessID(HWND hWnd, DWORD dwProcessId)
{
    static HWND cachedWnd = 0;
    static DWORD cachedPID = 0;
    static BOOL cachedRet = TRUE; // safe value, toolbar will be visible

    DWORD dwPID;
    GetWindowThreadProcessId(hWnd, &dwPID);
    if (cachedWnd == hWnd && cachedPID == dwPID)
        return cachedRet;
    else
    {
        cachedWnd = hWnd;
        cachedPID = dwPID;
    }
    if (dwPID == dwProcessId)
    {
        cachedRet = TRUE;
        return TRUE;
    }

    BOOL belongsTo = FALSE;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != (HANDLE)-1)
    {
        TDirectArray<PROCESSENTRY32> processes(100, 100);
        PROCESSENTRY32 process;
        process.dwSize = sizeof(process);
        if (Process32First(hSnapshot, &process))
        {
            do
            {
                if (process.th32ProcessID == dwPID && process.th32ParentProcessID == dwProcessId)
                    belongsTo = TRUE;
                else
                    processes.Add(process);
            } while (!belongsTo && Process32Next(hSnapshot, &process));
        }
        CloseHandle(hSnapshot);

        for (int i = 0; !belongsTo && i < processes.Count; i++)
        {
            if (processes[i].th32ProcessID == dwPID)
            {
                if (processes[i].th32ParentProcessID == dwProcessId)
                {
                    belongsTo = TRUE;
                }
                else
                {
                    dwPID = processes[i].th32ParentProcessID;
                    i = 0; // search again
                }
            }
        }
    }
    cachedRet = belongsTo;
    return belongsTo;
}
