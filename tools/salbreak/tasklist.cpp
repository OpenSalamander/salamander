// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "salbreak.h"
#include "tasklist.h"

#define NOHANDLES(function) function // obrana proti zanaseni maker HANDLES do zdrojaku pomoci CheckHnd

CTaskList TaskList;

// process list je sdileny skrz vsechny salamander v lokani session
// od AS 3.0 menime pojeti "Break" udalosti - vyvola v cili exception, takze mame "plnotucny" bug reprot, ale zaroven tim cil konci
// proto menim nasledujici konstanty "AltapSalamander*" -> "AltapSalamander3*", abychom byli oddeleni od starsich verzi
const char* AS_PROCESSLIST_NAME = "AltapSalamander3bProcessList";                               // sdilena pamet CProcessList
const char* AS_PROCESSLIST_MUTEX_NAME = "AltapSalamander3bProcessListMutex";                    // synchronizace pro pristup do sdilene pameti
const char* AS_PROCESSLIST_EVENT_NAME = "AltapSalamander3bProcessListEvent";                    // odpaleni udalosti (co se ma delat je ulozeno ve sdilene pameti)
const char* AS_PROCESSLIST_EVENT_PROCESSED_NAME = "AltapSalamander3bProcessListEventProcessed"; // odpalena udalost byla zpracovana

const char* FIRST_SALAMANDER_MUTEX_NAME = "AltapSalamanderFirstInstance";     // zavedeno od AS 2.52 beta 1
const char* LOADSAVE_REGISTRY_MUTEX_NAME = "AltapSalamanderLoadSaveRegistry"; // zavedeno od AS 2.52 beta 1

//
// ****************************************************************************
// CTaskList
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

CTaskList::CTaskList()
{
    EnableExceptionsOn64();

    OK = FALSE;
    FMO = NULL;
    ProcessList = NULL;
    FMOMutex = NULL;
    Event = NULL;
    EventProcessed = NULL;
}

BOOL CTaskList::Init()
{
    OK = FALSE;

    //---  pokusime se pripojit na FMO-mutex - zaroven test jestli uz nejaky Salamander bezi
    FMOMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, AS_PROCESSLIST_MUTEX_NAME));
    if (FMOMutex == NULL) // zadny Salamander 3.0 nebo novejsi v lokalni sessione = koncime
    {
        return FALSE;
    }
    else // dalsi instance, jen se pripojime ...
    {
        //---  zabereme FMO
        DWORD waitRet = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (waitRet == WAIT_TIMEOUT)
            return FALSE; // fail

        //---  pripojime se na ostatni systemove objekty pro komunikaci
        FMO = NOHANDLES(OpenFileMapping(FILE_MAP_WRITE, FALSE, AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE;                                                                    // fail
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0)); // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
        if (ProcessList == NULL)
            return FALSE; // fail
        // aby na event bylo mozne volat SetEvent(), musi mit nahozeny EVENT_MODIFY_STATE, pro Wait* potrebuje SYNCHRONIZE
        Event = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // fail
        EventProcessed = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //---  pridame zaznam do sdilene pameti
        // my nejsme Salamander, nic nepridavame

        //---  uvolnime FMO
        ReleaseMutex(FMOMutex);
    }

    OK = TRUE;
    return TRUE;
}

CTaskList::~CTaskList()
{
    if (ProcessList != NULL)
        NOHANDLES(UnmapViewOfFile(ProcessList));
    if (FMO != NULL)
        NOHANDLES(CloseHandle(FMO));
    if (FMOMutex != NULL)
        NOHANDLES(CloseHandle(FMOMutex));
    if (Event != NULL)
        NOHANDLES(CloseHandle(Event));
    if (EventProcessed != NULL)
        NOHANDLES(CloseHandle(EventProcessed));
}

int CTaskList::GetItems(CProcessListItem* items, DWORD* itemsStateUID, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;
    if (OK)
    {
        BOOL changed = FALSE;
        //---  zabereme FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return 0; // fail
        }

        CProcessListItem* ptr = ProcessList->Items;

        //---  vyhodime killnute procesy
        RemoveKilledItems(&changed);

        //---  navratove hodnoty
        if (items != NULL)
            memcpy(items, ptr, ProcessList->ItemsCount * sizeof(CProcessListItem));
        if (changed)
            ProcessList->ItemsStateUID++;
        if (itemsStateUID != NULL)
            *itemsStateUID = ProcessList->ItemsStateUID;

        int count = ProcessList->ItemsCount;
        //---  uvolnime FMO
        ReleaseMutex(FMOMutex);
        return count;
    }
    else
        return 0;
}

BOOL CTaskList::FireEvent(DWORD todo, DWORD pid, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;
    if (OK)
    {
        // zabereme ProcessList
        DWORD waitRet = WaitForSingleObject(FMOMutex, 2000);
        if (waitRet == WAIT_FAILED)
            return FALSE;
        if (waitRet == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return FALSE; // fail
        }

        // nastavime predavane parametry
        ProcessList->Todo = todo;
        ProcessList->TodoUID++;
        ProcessList->TodoTimestamp = GetTickCount();
        ProcessList->PID = pid;

        // pri breaknuti jine instance Salamandera pustime jeho Salmon nad nas
        if (todo == TASKLIST_TODO_BREAK)
        {
            for (DWORD i = 0; i < ProcessList->ItemsCount; i++)
            {
                if (ProcessList->Items[i].PID == pid)
                {
                    AllowSetForegroundWindow(ProcessList->Items[i].PID);       // radeji povolime i vlastniho Salamandera, i kdyz to je asi zbytecne...
                    AllowSetForegroundWindow(ProcessList->Items[i].SalmonPID); // rozhodne musime pustit nad nas jeho Salmon
                    break;
                }
            }
        }

        // uvolnime ProcessList
        ReleaseMutex(FMOMutex);

        // spustime kontrolu ve vsech Salamanderech
        ResetEvent(EventProcessed);
        SetEvent(Event);

        //---  dame chvilku na reakci (behem teto doby by se nekdo mel "chytnout" a ukol splnit)
        BOOL ret = (WaitForSingleObject(EventProcessed, 1000) == WAIT_OBJECT_0);

        //---  rekneme vsem Salamanderum, at se pripravi na dalsi command
        ResetEvent(Event);

        //---  nastavime zpatky break-PID
        //    ProcessList->Todo = 0;
        //    ProcessList->PID = 0;

        //---  uvolnime FMO

        return ret;
    }
    return FALSE;
}

BOOL CTaskList::RemoveKilledItems(BOOL* changed)
{
    if (!OK)
        return FALSE;

    if (changed != NULL)
        *changed = FALSE;
    CProcessListItem* ptr = ProcessList->Items;
    int c = ProcessList->ItemsCount;

    int i;
    for (i = 0; i < c; i++)
    {
        HANDLE h = NOHANDLES(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, ptr[i].PID));
        if (h != NULL)
        {
            BOOL cont = FALSE;
            DWORD exitcode;
            if (!GetExitCodeProcess(h, &exitcode) || exitcode == STILL_ACTIVE)
                cont = TRUE;
            NOHANDLES(CloseHandle(h));
            if (cont)
                continue; // nechame proces v seznamu
        }
        else
        {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_ACCESS_DENIED)
            {
                continue; // nechame proces v seznamu
            }
        }
        memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CProcessListItem));
        c--;
        i--;
        if (changed != NULL)
            *changed = TRUE;
    }
    ProcessList->ItemsCount = c;

    /*
// neslape pod XP pokud jsou procesy v ramci jedne session spusteny pod ruznymi uzivateli
// nemame pravo otevrit hande jineho procesu
//---  vyhodime killnuty procesy
int i;
    for (i = 0; i < c; i++)
    {
      HANDLE h = NOHANDLES(OpenProcess(PROCESS_TERMINATE, FALSE, ptr[i].PID));
      if (h != NULL)
      {
        BOOL cont = FALSE;
        DWORD exitcode;
        if (!GetExitCodeProcess(h, &exitcode) || exitcode == STILL_ACTIVE) cont = TRUE;
        NOHANDLES(CloseHandle(h));
        if (cont) continue;  // nechame proces v seznamu
      }
//---  vykopneme proces ze seznamu
      memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CTLItem));
      c--;
      i--;
    }
    ((DWORD *)SharedMem)[0] = c;   // items-count
    memcpy(items, ptr, c * sizeof(CTLItem));
*/

    return TRUE;
}