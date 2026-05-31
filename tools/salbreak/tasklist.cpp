// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>

#include "salbreak.h"
#include "tasklist.h"

#define NOHANDLES(function) function // protects against polluting the source with HANDLES macros via CheckHnd

CTaskList TaskList;

// the process list is shared across all Salamander instances in the local session
// since AS 3.0 we changed the meaning of the "Break" event: it triggers an exception
// in the target, so we get a full bug report, but the target also terminates
// therefore I changed the following constants from "AltapSalamander*" to
// "AltapSalamander3*" so that we stay separated from older versions
const char* AS_PROCESSLIST_NAME = "AltapSalamander3bProcessList";                               // shared memory for CProcessList
const char* AS_PROCESSLIST_MUTEX_NAME = "AltapSalamander3bProcessListMutex";                    // synchronization for access to shared memory
const char* AS_PROCESSLIST_EVENT_NAME = "AltapSalamander3bProcessListEvent";                    // event trigger (what to do is stored in shared memory)
const char* AS_PROCESSLIST_EVENT_PROCESSED_NAME = "AltapSalamander3bProcessListEventProcessed"; // the triggered event was processed

const char* FIRST_SALAMANDER_MUTEX_NAME = "AltapSalamanderFirstInstance";     // introduced in AS 2.52 beta 1
const char* LOADSAVE_REGISTRY_MUTEX_NAME = "AltapSalamanderLoadSaveRegistry"; // introduced in AS 2.52 beta 1

//
// ****************************************************************************
// CTaskList
//

// We want to receive SEH exceptions even on x64 Windows 7 SP1 and later
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

    //---  try to connect to the FMO mutex; this also tests whether some Salamander is already running
    FMOMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, AS_PROCESSLIST_MUTEX_NAME));
    if (FMOMutex == NULL) // no Salamander 3.0 or newer in the local session; we are done
    {
        return FALSE;
    }
    else // another instance; just connect to it ...
    {
        //---  take the FMO
        DWORD waitRet = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (waitRet == WAIT_TIMEOUT)
            return FALSE; // fail

        //---  connect to the other system objects used for communication
        FMO = NOHANDLES(OpenFileMapping(FILE_MAP_WRITE, FALSE, AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE;                                                                    // fail
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0)); // FIXME_X64 we do not pass x86/x64-incompatible data, do we?
        if (ProcessList == NULL)
            return FALSE; // fail
        // to be able to call SetEvent on the event, it must have EVENT_MODIFY_STATE;
        // Wait* needs SYNCHRONIZE
        Event = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // fail
        EventProcessed = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //---  add a record to shared memory
        // we are not Salamander, so we do not add anything

        //---  release the FMO
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
        //---  take the FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return 0; // fail
        }

        CProcessListItem* ptr = ProcessList->Items;

        //---  remove terminated processes
        RemoveKilledItems(&changed);

        //---  return values
        if (items != NULL)
            memcpy(items, ptr, ProcessList->ItemsCount * sizeof(CProcessListItem));
        if (changed)
            ProcessList->ItemsStateUID++;
        if (itemsStateUID != NULL)
            *itemsStateUID = ProcessList->ItemsStateUID;

        int count = ProcessList->ItemsCount;
        //---  release the FMO
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
        // take ProcessList
        DWORD waitRet = WaitForSingleObject(FMOMutex, 2000);
        if (waitRet == WAIT_FAILED)
            return FALSE;
        if (waitRet == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return FALSE; // fail
        }

        // set the transferred parameters
        ProcessList->Todo = todo;
        ProcessList->TodoUID++;
        ProcessList->TodoTimestamp = GetTickCount();
        ProcessList->PID = pid;

        // when breaking another Salamander instance, allow its Salmon to come to the foreground
        if (todo == TASKLIST_TODO_BREAK)
        {
            for (DWORD i = 0; i < ProcessList->ItemsCount; i++)
            {
                if (ProcessList->Items[i].PID == pid)
                {
                    AllowSetForegroundWindow(ProcessList->Items[i].PID);       // better allow its Salamander too, even though that is probably unnecessary...
                    AllowSetForegroundWindow(ProcessList->Items[i].SalmonPID); // we definitely have to allow its Salmon to come in front of us
                    break;
                }
            }
        }

        // release ProcessList
        ReleaseMutex(FMOMutex);

        // start the check in all Salamanders
        ResetEvent(EventProcessed);
        SetEvent(Event);

        //---  give it a moment to react (during this time someone should "catch" the task and complete it)
        BOOL ret = (WaitForSingleObject(EventProcessed, 1000) == WAIT_OBJECT_0);

        //---  tell all Salamanders to get ready for the next command
        ResetEvent(Event);

        //---  restore break-PID back
        //    ProcessList->Todo = 0;
        //    ProcessList->PID = 0;

        //---  release the FMO

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
                continue; // keep the process in the list
        }
        else
        {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_ACCESS_DENIED)
            {
                continue; // keep the process in the list
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
// does not work on XP if the processes in one session are started under different users
// we do not have the right to open a handle to another process
//---  remove terminated processes
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
        if (cont) continue;  // keep the process in the list
      }
//---  remove the process from the list
      memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CTLItem));
      c--;
      i--;
    }
    ((DWORD *)SharedMem)[0] = c;   // items-count
    memcpy(items, ptr, c * sizeof(CTLItem));
*/

    return TRUE;
}
