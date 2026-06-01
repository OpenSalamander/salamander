// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "tasklist.h"
#include "plugins.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"

#pragma warning(disable : 4074)
#pragma init_seg(compiler) // perform initialization as early as possible

#define NOHANDLES(function) function // guard against the CheckHnd macros inserting HANDLES into the source

CTaskList TaskList;

BOOL FirstInstance_3_or_later = FALSE;

// the process list is shared by all Salamanders in the local session
// starting with AS 3.0, the "Break" event behavior changes: it raises an exception in the target, giving us a full bug report, but it also terminates the target
// therefore the following constants are changed from "AltapSalamander*" to "AltapSalamander3*" so that they remain separate from older versions

// WARNING: if you change this, adjust salbreak.exe as well; just send me the info ... thanks, Petr

const char* AS_PROCESSLIST_NAME = "AltapSalamander3bProcessList";                               // shared memory CProcessList
const char* AS_PROCESSLIST_MUTEX_NAME = "AltapSalamander3bProcessListMutex";                    // synchronization for accessing shared memory
const char* AS_PROCESSLIST_EVENT_NAME = "AltapSalamander3bProcessListEvent";                    // firing the event (what should happen is stored in shared memory)
const char* AS_PROCESSLIST_EVENT_PROCESSED_NAME = "AltapSalamander3bProcessListEventProcessed"; // the fired event has been processed

const char* FIRST_SALAMANDER_MUTEX_NAME = "AltapSalamanderFirstInstance";     // introduced in AS 2.52 beta 1
const char* LOADSAVE_REGISTRY_MUTEX_NAME = "AltapSalamanderLoadSaveRegistry"; // introduced in AS 2.52 beta 1

// path used to save the bug report and minidump; later Salmon packs them into 7z and uploads them to the server
char BugReportPath[MAX_PATH] = "";

CRITICAL_SECTION CommandLineParamsCS;
CCommandLineParams CommandLineParams;
HANDLE CommandLineParamsProcessed;

// handle of the main window (the control thread should not access MainWindow because it may become NULL while we are using it)
HWND HSafeMainWindow = NULL;

void RaiseBreakException()
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif                                                   // CALLSTK_DISABLE
    RaiseException(OPENSAL_EXCEPTION_BREAK, 0, 0, NULL); // our own "break" exception
                                                         // the code never gets here
}

//
// ****************************************************************************
// CTaskList
//

DWORD WINAPI FControlThread(void* param)
{
    // this thread does not run with our CCallStack; when investigating
    // a leaked handle, Salamander crashed while trying to dump it during shutdown

    CTaskList* tasklist = (CTaskList*)param;

    SetThreadNameInVC("ControlThread");

    HANDLE arr[3];
    arr[0] = tasklist->TerminateEvent;
    arr[1] = tasklist->Event;
    arr[2] = SalShExtDoPasteEvent;

    DWORD lastTodoUID = 0;

    DWORD ourPID = GetCurrentProcessId();

    BOOL loop = TRUE;
    while (loop)
    {
        DWORD waitRet = WaitForMultipleObjects(arr[2] == NULL ? 2 : 3, arr, FALSE, INFINITE);
        switch (waitRet)
        {
        case WAIT_OBJECT_0 + 0: // tasklist->TerminateEvent
        {
            loop = FALSE;
            break;
        }

        case WAIT_OBJECT_0 + 1: // tasklist->Event
        {
            // lock ProcessList
            waitRet = WaitForSingleObject(tasklist->FMOMutex, TASKLIST_TODO_TIMEOUT);
            if (waitRet == WAIT_FAILED)
                Sleep(50); // so we do not hog the CPU
            if (waitRet == WAIT_FAILED || waitRet == WAIT_TIMEOUT)
                break;

            // prevent looping after executing the command
            if (tasklist->ProcessList->TodoUID <= lastTodoUID)
            {
                // release ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                Sleep(50); // give other processes a chance
                break;
            }
            else
                lastTodoUID = tasklist->ProcessList->TodoUID;

            // ProcessList is now locked by us
            DWORD pid = tasklist->ProcessList->PID;
            if (pid != ourPID) // if the event is not meant for us
            {
                // release ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                Sleep(50); // give other processes a chance
                break;
            }

            // now we are already running in the process that was supposed to receive the message; at the same time we are in a side thread,
            // so any communication with the main thread has to be synchronized separately

            // reset the Event, because we now know it belonged to us and it is pointless to keep the control threads of other processes running
            ResetEvent(tasklist->Event);

            // check the timestamp to see whether we already missed the window for handling the command
            DWORD tickCount = GetTickCount();
            if (tickCount - tasklist->ProcessList->TodoTimestamp >= TASKLIST_TODO_TIMEOUT)
            {
                // TIMEOUT
                // release the ProcessList mutex
                ReleaseMutex(tasklist->FMOMutex);
                break;
            }

            // make a copy of the locked ProcessList
            CProcessList processList;
            memcpy(&processList, tasklist->ProcessList, sizeof(CProcessList));
            // and release the shared memory
            ReleaseMutex(tasklist->FMOMutex);

            switch (processList.Todo)
            {
            case TASKLIST_TODO_HIGHLIGHT:
            {
                SetEvent(tasklist->EventProcessed); // notification to the requesting process: we're done
                if (HSafeMainWindow != NULL)
                    PostMessage(HSafeMainWindow, WM_USER_FLASHWINDOW, 0, 0);
                break;
            }

            case TASKLIST_TODO_BREAK:
            {
                SetEvent(tasklist->EventProcessed); // notification to the requesting process: done

                RaiseBreakException();
                // the code never gets here

                break;
            }

            case TASKLIST_TODO_TERMINATE:
            {
                SetEvent(tasklist->EventProcessed); // notify the requesting process that processing is complete

                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (h != NULL)
                {
                    TerminateProcess(h, 666);
                    CloseHandle(h);
                }
                break;
            }

            case TASKLIST_TODO_ACTIVATE:
            {
                // copy ProcessList into the global CommandLineParams variable,
                // which the main thread monitors when returning from idle;
                NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                memcpy(&CommandLineParams, &processList.CommandLineParams, sizeof(CCommandLineParams));
                ResetEvent(CommandLineParamsProcessed);
                NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));

                // if the main thread is idle, wake it up and force it to check CommandLineParams::RequestUID
                // if it is not idle, it is handling something else and will process the message when it next enters idle
                if (HSafeMainWindow != NULL)
                    PostMessage(HSafeMainWindow, WM_USER_WAKEUP_FROM_IDLE, 0, 0);

                // wait 5 seconds to see whether the main thread responds (we do not enter the critical section yet so it can do so)
                WaitForSingleObject(CommandLineParamsProcessed, TASKLIST_TODO_TIMEOUT);

                // now we can enter the critical section
                NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                CommandLineParams.RequestUID = 0;                             // prevent the main thread from taking further actions
                waitRet = WaitForSingleObject(CommandLineParamsProcessed, 0); // check the current state of the event
                if (waitRet == WAIT_OBJECT_0)
                    SetEvent(tasklist->EventProcessed); // notification to the requesting process: we're done
                NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));
                break;
            }

            default:
            {
                TRACE_E("FControlThread: unknown todo=" << processList.Todo);
                break;
            }
            }
            break;
        }

        case WAIT_OBJECT_0 + 2: // SalShExtDoPasteEvent
        {
            BOOL sleep = TRUE;
            if (SalShExtSharedMemMutex != NULL)
            {
                WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                if (HSafeMainWindow != NULL && SalShExtSharedMemView != NULL &&
                    SalShExtSharedMemView->SalamanderMainWnd == (UINT64)(DWORD_PTR)HSafeMainWindow)
                {
                    ResetEvent(SalShExtDoPasteEvent); // the "source" Salamander has been found, further searching is pointless
                    sleep = FALSE;
                    PostMessage(HSafeMainWindow, WM_USER_SALSHEXT_PASTE, SalShExtSharedMemView->PostMsgIndex, 0);
                }
                ReleaseMutex(SalShExtSharedMemMutex);
            }
            if (sleep)
                Sleep(50); // let other Salamanders have a chance
            break;
        }

        default: // this should not happen
        {
            Sleep(50); // so we do not hog the CPU
            break;
        }
        }
    }

    return 0;
}

CTaskList::CTaskList()
{
    // we run in the 'compiler' segment, so this happens before ms_init
    OK = FALSE;
    FMO = NULL;
    ProcessList = NULL;
    FMOMutex = NULL;
    Event = NULL;
    EventProcessed = NULL;
    TerminateEvent = NULL;
    ControlThread = NULL;
    // internal synchronization between the control thread and the main thread
    NOHANDLES(InitializeCriticalSection(&CommandLineParamsCS));
    CommandLineParamsProcessed = NULL;
}

BOOL CTaskList::Init()
{
    OK = FALSE;

    PSID psidEveryone;
    PACL paclNewDacl;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES* saPtr = CreateAccessableSecurityAttributes(&sa, &sd, GENERIC_ALL, &psidEveryone, &paclNewDacl);

    //---  first, a side note: on Vista and later, create an event for communication with the copy hook (the control thread waits for it)
    if (WindowsVistaAndLater)
    {
        SalShExtDoPasteEvent = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, SALSHEXT_DOPASTEEVENTNAME));
        if (SalShExtDoPasteEvent == NULL)
            SalShExtDoPasteEvent = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, SALSHEXT_DOPASTEEVENTNAME));
        if (SalShExtDoPasteEvent == NULL)
            TRACE_E("CTaskList::Init(): unable to create event object for communicating with copy-hook shell extension!");
    }

    //---  try to attach to the FMO mutex - also serves as a test whether any Salamander is already running
    FMOMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, AS_PROCESSLIST_MUTEX_NAME));
    if (FMOMutex == NULL) // we are the first instance of Salamander 3.0 or newer in the local session
    {
        //---  create system objects for communication, claim the FMO
        FMOMutex = NOHANDLES(CreateMutex(saPtr, TRUE, AS_PROCESSLIST_MUTEX_NAME)); // the task list is valid only for the given session; the mutex lives in the local namespace
        if (FMOMutex == NULL)
            return FALSE; // fail
        FMO = NOHANDLES(CreateFileMapping(INVALID_HANDLE_VALUE, saPtr, PAGE_READWRITE | SEC_COMMIT,
                                          0, sizeof(CProcessList), AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE; // fail
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0));
        if (ProcessList == NULL)
            return FALSE; // fail
        Event = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // failure
        EventProcessed = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //---  initialize the shared memory
        ZeroMemory(ProcessList, sizeof(CProcessList));
        ProcessList->Version = 1; // 3.0 beta 4

        ProcessList->ItemsCount = 1;
        ProcessList->ItemsStateUID++;
        ProcessList->Items[0] = CProcessListItem();

        //---  release the FMO
        ReleaseMutex(FMOMutex);
    }
    else // another instance, just connect ...
    {
        //---  claim the FMO
        DWORD waitRet = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (waitRet == WAIT_TIMEOUT)
            return FALSE; // failed

        //---  attach to the other system objects for communication
        FMO = NOHANDLES(OpenFileMapping(FILE_MAP_WRITE, FALSE, AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE; // failed
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0));
        if (ProcessList == NULL)
            return FALSE; // failed
        // to be able to call SetEvent() on the event, it must have EVENT_MODIFY_STATE set, Wait* requires SYNCHRONIZE
        Event = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // failed
        EventProcessed = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // failed

        //---  add a record to shared memory
        BOOL attempt = 0;
    AGAIN:
        int c = ProcessList->ItemsCount;
        if (c < MAX_TL_ITEMS) // if there are not too many, add this process
        {
            ProcessList->ItemsCount++;
            ProcessList->ItemsStateUID++;
            ProcessList->Items[c] = CProcessListItem();
        }
        else
        {
            if (attempt == 0)
            {
                // the array is full, try to shake it down (one of the processes might have died without informing us)
                RemoveKilledItems(NULL);
                attempt++;
                goto AGAIN;
            }
        }

        //---  release the FMO
        ReleaseMutex(FMOMutex);
    }

    // detect other Salamander instances
    LPTSTR sid = NULL;
    if (!GetStringSid(&sid))
        sid = NULL;
    char mutexName[1000];
    if (sid == NULL)
    {
        // failed to obtain the SID -- local namespace, without the SID appended
        _snprintf_s(mutexName, _TRUNCATE, "%s", FIRST_SALAMANDER_MUTEX_NAME);
    }
    else
    {
        _snprintf_s(mutexName, _TRUNCATE, "Global\\%s_%s", FIRST_SALAMANDER_MUTEX_NAME, sid);
        LocalFree(sid);
    }
    HANDLE hMutex = NOHANDLES(CreateMutex(saPtr, FALSE, mutexName));
    DWORD lastError = GetLastError();
    if (hMutex != NULL)
    {
        FirstInstance_3_or_later = (lastError != ERROR_ALREADY_EXISTS);
    }
    else
    {
        hMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, mutexName));
        lastError = GetLastError();
        if (hMutex != NULL)
            FirstInstance_3_or_later = FALSE;
    }

    if (psidEveryone != NULL)
        FreeSid(psidEveryone);
    if (paclNewDacl != NULL)
        LocalFree(paclNewDacl);

    TerminateEvent = NOHANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
    if (TerminateEvent == NULL)
        return FALSE; // failed

    // internal synchronization between the control thread and the main thread
    CommandLineParamsProcessed = CreateEvent(NULL, TRUE, FALSE, NULL); // manual, nonsignaled
    if (CommandLineParamsProcessed == NULL)
        return FALSE; // failed

    // cannot use _beginthreadex because the library might not be initialized yet
    DWORD id;
    ControlThread = NOHANDLES(CreateThread(NULL, 0, FControlThread, this, 0, &id));
    if (ControlThread == NULL)
        return FALSE; // failed
    // this thread must still receive CPU time even if resources are scarce ...
    SetThreadPriority(ControlThread, THREAD_PRIORITY_TIME_CRITICAL);

    OK = TRUE;
    return TRUE;
}

CTaskList::~CTaskList()
{
    if (ControlThread != NULL)
    {
        SetEvent(TerminateEvent);                     // terminate!
        WaitForSingleObject(ControlThread, INFINITE); // wait until the thread finishes
        NOHANDLES(CloseHandle(ControlThread));
    }
    if (TerminateEvent != NULL)
        NOHANDLES(CloseHandle(TerminateEvent));

    // remove ourselves from the list
    if (OK)
    {
        //---  claim the FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) != WAIT_TIMEOUT)
        {
            CProcessListItem* ptr = ProcessList->Items;
            int c = ProcessList->ItemsCount;

            //---  remove the current process, it is shutting down ...
            DWORD PID = GetCurrentProcessId();
            int i;
            for (i = 0; i < c; i++)
            {
                if (PID == ptr[i].PID)
                {
                    //---  delete the process from the list
                    memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CProcessListItem));
                    c--;
                    i--;
                }
            }
            ProcessList->ItemsCount = c;
            ProcessList->ItemsStateUID++;

            //---  release the FMO
            ReleaseMutex(FMOMutex);
        }
    }

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
    if (CommandLineParamsProcessed != NULL)
        NOHANDLES(CloseHandle(CommandLineParamsProcessed));
    NOHANDLES(DeleteCriticalSection(&CommandLineParamsCS));

    if (SalShExtDoPasteEvent != NULL)
        NOHANDLES(CloseHandle(SalShExtDoPasteEvent));
    SalShExtDoPasteEvent = NULL;
}

BOOL CTaskList::SetProcessState(DWORD processState, HWND hMainWindow, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;

    HSafeMainWindow = hMainWindow;

    if (OK)
    {
        DWORD ret = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (ret != WAIT_FAILED && ret != WAIT_TIMEOUT)
        {
            // find ourselves in the process list and set processState and hMainWindow
            CProcessListItem* ptr = ProcessList->Items;
            int c = ProcessList->ItemsCount;
            DWORD PID = GetCurrentProcessId();
            int i;
            for (i = 0; i < c; i++)
            {
                if (PID == ptr[i].PID)
                {
                    ptr[i].ProcessState = processState;
                    ptr[i].HMainWindow = (UINT64)(DWORD_PTR)hMainWindow; // 64b for x64/x86 compatibility
                    break;
                }
            }
            ReleaseMutex(FMOMutex);
            return TRUE;
        }
        else
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            TRACE_E("SetProcessState(): WaitForSingleObject failed!");
        }
    }
    return FALSE;
}

int CTaskList::GetItems(CProcessListItem* items, DWORD* itemsStateUID, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;
    if (OK)
    {
        BOOL changed = FALSE;
        //---  claim the FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return 0; // fail
        }

        CProcessListItem* ptr = ProcessList->Items;

        //---  remove killed processes
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
        // claim ProcessList
        DWORD waitRet = WaitForSingleObject(FMOMutex, 2000);
        if (waitRet == WAIT_FAILED)
            return FALSE;
        if (waitRet == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return FALSE; // fail
        }

        // set the parameters to pass
        ProcessList->Todo = todo;
        ProcessList->TodoUID++;
        ProcessList->TodoTimestamp = GetTickCount();
        ProcessList->PID = pid;

        // when breaking another Salamander instance, allow its Salmon to come to the foreground in front of us
        if (todo == TASKLIST_TODO_BREAK)
        {
            for (DWORD i = 0; i < ProcessList->ItemsCount; i++)
            {
                if (ProcessList->Items[i].PID == pid)
                {
                    AllowSetForegroundWindow(ProcessList->Items[i].PID);       // better allow our own Salamander too, even if it is probably unnecessary...
                    AllowSetForegroundWindow(ProcessList->Items[i].SalmonPID); // we definitely must allow its Salmon to come to the foreground before us
                    break;
                }
            }
        }

        // release ProcessList
        ReleaseMutex(FMOMutex);

        // trigger the check in all Salamanders
        ResetEvent(EventProcessed);
        SetEvent(Event);

        //---  give it a moment to react (during this time someone should "grab" it and complete the task)
        BOOL ret = (WaitForSingleObject(EventProcessed, 1000) == WAIT_OBJECT_0);

        //---  tell all Salamanders to prepare for the next command
        ResetEvent(Event);

        //---  restore the break PID
        //    ProcessList->Todo = 0;
        //    ProcessList->PID = 0;

        //---  release the FMO

        return ret;
    }
    return FALSE;
}

BOOL CTaskList::ActivateRunningInstance(const CCommandLineParams* cmdLineParams, BOOL* timeouted)
{
    if (timeouted != NULL)
        *timeouted = FALSE;

    if (!OK)
        return FALSE;

    CProcessListItem ourProcessInfo;

    // find a running process in our class, or possibly a starting one (wait a moment to see if it takes off)
    int firstStarting = -1; // index of a process from our class (matching Integrity Level and SID) that does not yet have a main window
    int firstRunnig = -1;   // index of a process from our class (matching Integrity Level and SID) that is already running (has a main window)
    DWORD timeStamp = GetTickCount();
    do
    {
        firstStarting = -1;
        firstRunnig = -1;
        DWORD ret = WaitForSingleObject(FMOMutex, 200);
        if (ret == WAIT_FAILED)
            return FALSE;
        if (ret != WAIT_TIMEOUT) // we obtained the mutex
        {
            int i;
            for (i = 0; i < (int)ProcessList->ItemsCount; i++)
            {
                CProcessListItem* item = &ProcessList->Items[i];
                // search only for processes in our class (matching IntegrityLevel and SID)
                if (item->PID != ourProcessInfo.PID &&
                    item->IntegrityLevel == ourProcessInfo.IntegrityLevel &&
                    memcmp(item->SID_MD5, ourProcessInfo.SID_MD5, 16) == 0)
                {
                    if (item->ProcessState == PROCESS_STATE_RUNNING)
                    {
                        firstRunnig = i;
                        break; // if we found a running instance, no need to look for a starting one
                    }
                    if (item->ProcessState == PROCESS_STATE_STARTING && firstStarting == -1)
                        firstStarting = i;
                }
            }

            if (firstRunnig == -1) // no process from our class has a main window yet
            {
                ReleaseMutex(FMOMutex); // so release the memory to others
                if (firstStarting == -1)
                    return FALSE; // no starting candidate found, give up
                else
                    Sleep(200); // found a starting candidate, pause for 200 ms to give it a chance to call SetProcessState()
            }
        }
    } while (firstRunnig == -1 && (GetTickCount() - timeStamp < TASKLIST_TODO_TIMEOUT)); // wait for a running instance for at most 5 s

    // if we did not find any instance of our class with a main window, or if waiting took 5 s, stop waiting
    if (firstRunnig == -1)
        return FALSE;

    CProcessListItem* item = &ProcessList->Items[firstRunnig];

    // set Todo, PID, and parameters
    ProcessList->Todo = TASKLIST_TODO_ACTIVATE;
    ProcessList->TodoUID++; // tell processes that a new command will be processed
    ProcessList->TodoTimestamp = GetTickCount();
    ProcessList->PID = item->PID;

    // copy parameters from the command line
    memcpy(&ProcessList->CommandLineParams, cmdLineParams, sizeof(CCommandLineParams));
    // and set our internal variables
    ProcessList->CommandLineParams.Version = 1;
    ProcessList->CommandLineParams.RequestUID = ProcessList->TodoUID;
    ProcessList->CommandLineParams.RequestTimestamp = ProcessList->TodoTimestamp;

    // allow the activated process to call SetForegroundWindow, otherwise it will not be able to come to the front
    AllowSetForegroundWindow(item->PID);

    // trigger the check in all Salamanders
    // release shared memory
    ReleaseMutex(FMOMutex);

    ResetEvent(EventProcessed);
    SetEvent(Event);

    // give it a moment to react (during this period someone should "grab" it and finish the task)
    // 500 ms is our cushion so we safely cover subordinate threads
    BOOL ret = (WaitForSingleObject(EventProcessed, TASKLIST_TODO_TIMEOUT + 500) == WAIT_OBJECT_0);

    // tell all Salamanders to prepare for the next command (it also resets in the control thread if a process is performing the todo)
    ResetEvent(Event);

    // reset todo
    // ProcessList->Todo = 0; // we should lock FMOMutex first, but in this case there is nothing to corrupt and we can zero the values
    // ProcessList->PID = 0;

    return ret;
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
            // on older Windows we obtain a handle even for a terminated process
            // therefore it is still necessary to query the exit code; probably unnecessary since W2K
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
// does not work on XP if processes within one session run under different users
// we do not have permission to open a handle to another process
//---  remove killed processes
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
