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
#pragma init_seg(compiler) // initialize as soon as possible

#define NOHANDLES(function) function // Defense against the introduction of HANDLES macros into the source code using CheckHnd

CTaskList TaskList;

BOOL FirstInstance_3_or_later = FALSE;

// process list is shared across all salamanders in the local session
// Starting from AS 3.0, we are changing the concept of the "Break" event - it will now throw an exception at the target, providing a "full-fledged" bug report, but at the same time, it will end the target.
// therefore I am changing the following constants "AltapSalamander*" -> "AltapSalamander3*", to separate us from older versions

// WARNING: when changing, salbreak.exe needs to be adjusted, just send me the info please ... thanks, Petr

const char* AS_PROCESSLIST_NAME = "AltapSalamander3bProcessList";                               // shared memory CProcessList
const char* AS_PROCESSLIST_MUTEX_NAME = "AltapSalamander3bProcessListMutex";                    // synchronization for accessing shared memory
const char* AS_PROCESSLIST_EVENT_NAME = "AltapSalamander3bProcessListEvent";                    // Event firing (what to do is stored in shared memory)
const char* AS_PROCESSLIST_EVENT_PROCESSED_NAME = "AltapSalamander3bProcessListEventProcessed"; // Event fired was processed

const char* FIRST_SALAMANDER_MUTEX_NAME = "AltapSalamanderFirstInstance";     // Introduced since AS 2.52 beta 1
const char* LOADSAVE_REGISTRY_MUTEX_NAME = "AltapSalamanderLoadSaveRegistry"; // Introduced since AS 2.52 beta 1

// path where to store bug report and minidump; later salmon will pack it into 7z and upload it to the server
char BugReportPath[MAX_PATH] = "";

CRITICAL_SECTION CommandLineParamsCS;
CCommandLineParams CommandLineParams;
HANDLE CommandLineParamsProcessed;

// Handle of the main window (it is not good to access MainWindow from the control thread, which can be set to NULL under our hands)
HWND HSafeMainWindow = NULL;

void RaiseBreakException()
{
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif                                                   // CALLSTK_DISABLE
    RaiseException(OPENSAL_EXCEPTION_BREAK, 0, 0, NULL); // our own "break" exception
                                                         // the code won't reach here anymore
}

//
// ****************************************************************************
// CTaskList
//

DWORD WINAPI FControlThread(void* param)
{
    // This thread is not called from our CCallStack - I encountered a leaked handle when trying to
    // print (when Salamander ends) Salam fell

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
            // we will take a snapshot of the ProcessList
            waitRet = WaitForSingleObject(tasklist->FMOMutex, TASKLIST_TODO_TIMEOUT);
            if (waitRet == WAIT_FAILED)
                Sleep(50); // to avoid eating CPU
            if (waitRet == WAIT_FAILED || waitRet == WAIT_TIMEOUT)
                break;

            // Protection against cycling after executing commands
            if (tasklist->ProcessList->TodoUID <= lastTodoUID)
            {
                // Release ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                Sleep(50); // give a chance to other processes
                break;
            }
            else
                lastTodoUID = tasklist->ProcessList->TodoUID;

            // we have the ProcessList locked
            DWORD pid = tasklist->ProcessList->PID;
            if (pid != ourPID) // if the event does not concern us
            {
                // Release ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                Sleep(50); // give a chance to other processes
                break;
            }

            // Now we are already running in the process that was supposed to receive the message; at the same time, we are in a separate thread, so
            // Any communication with the main thread needs to be resolved with additional synchronization

            // Reset the Event because now we know it belonged to us and there is no point in letting the control threads of other processes run
            ResetEvent(tasklist->Event);

            // Check the timestamp to verify if we have not exceeded the time we had available to process the command
            DWORD tickCount = GetTickCount();
            if (tickCount - tasklist->ProcessList->TodoTimestamp >= TASKLIST_TODO_TIMEOUT)
            {
                // TIMEOUT
                // Release ProcessList
                ReleaseMutex(tasklist->FMOMutex);
                break;
            }

            // we will get a copy of the captured ProcessList
            CProcessList processList;
            memcpy(&processList, tasklist->ProcessList, sizeof(CProcessList));
            // and release shared memory
            ReleaseMutex(tasklist->FMOMutex);

            switch (processList.Todo)
            {
            case TASKLIST_TODO_HIGHLIGHT:
            {
                SetEvent(tasklist->EventProcessed); // message for the requester process: we are done
                if (HSafeMainWindow != NULL)
                    PostMessage(HSafeMainWindow, WM_USER_FLASHWINDOW, 0, 0);
                break;
            }

            case TASKLIST_TODO_BREAK:
            {
                SetEvent(tasklist->EventProcessed); // message for the requester process: we are done

                RaiseBreakException();
                // the code won't reach here anymore

                break;
            }

            case TASKLIST_TODO_TERMINATE:
            {
                SetEvent(tasklist->EventProcessed); // message for the requester process: we are done

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
                // Copy ProcessList to the global variable CommandLineParams,
                // which is monitored by the main thread upon entering from idle;
                NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                memcpy(&CommandLineParams, &processList.CommandLineParams, sizeof(CCommandLineParams));
                ResetEvent(CommandLineParamsProcessed);
                NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));

                // in case the main thread is in IDLE, we push into it and force it to control CommandLineParams::RequestUID
                // if it is not in IDLE, it is currently processing something and will handle the message as soon as it enters IDLE (if we wait)
                if (HSafeMainWindow != NULL)
                    PostMessage(HSafeMainWindow, WM_USER_WAKEUP_FROM_IDLE, 0, 0);

                // Wait for 5 seconds to see if the main thread responds (do not enter the critical section yet, so that it can enter)
                WaitForSingleObject(CommandLineParamsProcessed, TASKLIST_TODO_TIMEOUT);

                // We can now enter the critical section.
                NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                CommandLineParams.RequestUID = 0;                             // Disable main thread and any further actions
                waitRet = WaitForSingleObject(CommandLineParamsProcessed, 0); // we ask what the current status of the event looks like
                if (waitRet == WAIT_OBJECT_0)
                    SetEvent(tasklist->EventProcessed); // message for the requester process: we are done
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
                    ResetEvent(SalShExtDoPasteEvent); // "Source" Salamander has been found, further searching is pointless
                    sleep = FALSE;
                    PostMessage(HSafeMainWindow, WM_USER_SALSHEXT_PASTE, SalShExtSharedMemView->PostMsgIndex, 0);
                }
                ReleaseMutex(SalShExtSharedMemMutex);
            }
            if (sleep)
                Sleep(50); // give a chance to other Salamanders
            break;
        }

        default: // this should not happen
        {
            Sleep(50); // to avoid eating CPU
            break;
        }
        }
    }

    return 0;
}

CTaskList::CTaskList()
{
    // we are running in the 'compiler' group, before ms_init
    OK = FALSE;
    FMO = NULL;
    ProcessList = NULL;
    FMOMutex = NULL;
    Event = NULL;
    EventProcessed = NULL;
    TerminateEvent = NULL;
    ControlThread = NULL;
    // Internal synchronization between ControlThread and the main thread
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

    //--- First, a side note: under Vista+ we will create an event for communication with the copy-hook (it is expected in the control thread)
    if (WindowsVistaAndLater)
    {
        SalShExtDoPasteEvent = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, SALSHEXT_DOPASTEEVENTNAME));
        if (SalShExtDoPasteEvent == NULL)
            SalShExtDoPasteEvent = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, SALSHEXT_DOPASTEEVENTNAME));
        if (SalShExtDoPasteEvent == NULL)
            TRACE_E("CTaskList::Init(): unable to create event object for communicating with copy-hook shell extension!");
    }

    //--- we will try to connect to the FMO mutex - at the same time test if any Salamander is already running
    FMOMutex = NOHANDLES(OpenMutex(SYNCHRONIZE, FALSE, AS_PROCESSLIST_MUTEX_NAME));
    if (FMOMutex == NULL) // we are the first Salamander 3.0 or newer in the local session
    {
        //--- creating system objects for communication, we will occupy FMO
        FMOMutex = NOHANDLES(CreateMutex(saPtr, TRUE, AS_PROCESSLIST_MUTEX_NAME)); // task list is valid only for the given session, mutex belongs to the local namespace
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
            return FALSE; // fail
        EventProcessed = NOHANDLES(CreateEvent(saPtr, TRUE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //--- shared memory initialization
        ZeroMemory(ProcessList, sizeof(CProcessList));
        ProcessList->Version = 1; // 3.0 beta 4

        ProcessList->ItemsCount = 1;
        ProcessList->ItemsStateUID++;
        ProcessList->Items[0] = CProcessListItem();

        //---  release FMO
        ReleaseMutex(FMOMutex);
    }
    else // another instance, just connect...
    {
        //---  occupy FMO
        DWORD waitRet = WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT);
        if (waitRet == WAIT_TIMEOUT)
            return FALSE; // fail

        //--- we will connect to other system objects for communication
        FMO = NOHANDLES(OpenFileMapping(FILE_MAP_WRITE, FALSE, AS_PROCESSLIST_NAME));
        if (FMO == NULL)
            return FALSE; // fail
        ProcessList = (CProcessList*)NOHANDLES(MapViewOfFile(FMO, FILE_MAP_WRITE, 0, 0, 0));
        if (ProcessList == NULL)
            return FALSE; // fail
        // For calling SetEvent() on an event, EVENT_MODIFY_STATE privilege must be enabled, SYNCHRONIZE is needed for Wait* functions
        Event = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_NAME));
        if (Event == NULL)
            return FALSE; // fail
        EventProcessed = NOHANDLES(OpenEvent(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, AS_PROCESSLIST_EVENT_PROCESSED_NAME));
        if (EventProcessed == NULL)
            return FALSE; // fail

        //--- add a record to shared memory
        BOOL attempt = 0;
    AGAIN:
        int c = ProcessList->ItemsCount;
        if (c < MAX_TL_ITEMS) // if there are not too many of them, we will add this process
        {
            ProcessList->ItemsCount++;
            ProcessList->ItemsStateUID++;
            ProcessList->Items[c] = CProcessListItem();
        }
        else
        {
            if (attempt == 0)
            {
                // the field is full, let's try to sift through it (some of the processes could have crashed and not let us know)
                RemoveKilledItems(NULL);
                attempt++;
                goto AGAIN;
            }
        }

        //---  release FMO
        ReleaseMutex(FMOMutex);
    }

    // detecting other instances of Salamander
    LPTSTR sid = NULL;
    if (!GetStringSid(&sid))
        sid = NULL;
    char mutexName[1000];
    if (sid == NULL)
    {
        // Error in obtaining SID -- local name space without connected SID
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
        return FALSE; // fail

    // Internal synchronization between ControlThread and the main thread
    CommandLineParamsProcessed = CreateEvent(NULL, TRUE, FALSE, NULL); // manual, nonsignaled
    if (CommandLineParamsProcessed == NULL)
        return FALSE; // failed

    // Cannot use _beginthreadex because the library may not be initialized yet
    DWORD id;
    ControlThread = NOHANDLES(CreateThread(NULL, 0, FControlThread, this, 0, &id));
    if (ControlThread == NULL)
        return FALSE; // fail
    // this thread must reach the finish line even if there is no bread ...
    SetThreadPriority(ControlThread, THREAD_PRIORITY_TIME_CRITICAL);

    OK = TRUE;
    return TRUE;
}

CTaskList::~CTaskList()
{
    if (ControlThread != NULL)
    {
        SetEvent(TerminateEvent);                     // Terminate yourself!
        WaitForSingleObject(ControlThread, INFINITE); // Wait until the thread finishes
        NOHANDLES(CloseHandle(ControlThread));
    }
    if (TerminateEvent != NULL)
        NOHANDLES(CloseHandle(TerminateEvent));

    // remove ourselves from the list
    if (OK)
    {
        //---  occupy FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) != WAIT_TIMEOUT)
        {
            CProcessListItem* ptr = ProcessList->Items;
            int c = ProcessList->ItemsCount;

            //--- we terminate the current process, ending ...
            DWORD PID = GetCurrentProcessId();
            int i;
            for (i = 0; i < c; i++)
            {
                if (PID == ptr[i].PID)
                {
                    //--- we kick the process out of the list
                    memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CProcessListItem));
                    c--;
                    i--;
                }
            }
            ProcessList->ItemsCount = c;
            ProcessList->ItemsStateUID++;

            //---  release FMO
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
            // We will search for the process in the list and set the processState and hMainWindow
            CProcessListItem* ptr = ProcessList->Items;
            int c = ProcessList->ItemsCount;
            DWORD PID = GetCurrentProcessId();
            int i;
            for (i = 0; i < c; i++)
            {
                if (PID == ptr[i].PID)
                {
                    ptr[i].ProcessState = processState;
                    ptr[i].HMainWindow = (UINT64)(DWORD_PTR)hMainWindow; // 64-bit for x64/x86 compatibility
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
        //---  occupy FMO
        if (WaitForSingleObject(FMOMutex, TASKLIST_TODO_TIMEOUT) == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return 0; // fail
        }

        CProcessListItem* ptr = ProcessList->Items;

        //--- we will kill the processes that are running
        RemoveKilledItems(&changed);

        //--- return values
        if (items != NULL)
            memcpy(items, ptr, ProcessList->ItemsCount * sizeof(CProcessListItem));
        if (changed)
            ProcessList->ItemsStateUID++;
        if (itemsStateUID != NULL)
            *itemsStateUID = ProcessList->ItemsStateUID;

        int count = ProcessList->ItemsCount;
        //---  release FMO
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
        // we will take a snapshot of the ProcessList
        DWORD waitRet = WaitForSingleObject(FMOMutex, 2000);
        if (waitRet == WAIT_FAILED)
            return FALSE;
        if (waitRet == WAIT_TIMEOUT)
        {
            if (timeouted != NULL)
                *timeouted = TRUE;
            return FALSE; // fail
        }

        // set the passed parameters
        ProcessList->Todo = todo;
        ProcessList->TodoUID++;
        ProcessList->TodoTimestamp = GetTickCount();
        ProcessList->PID = pid;

        // When another instance of Salamander breaks, we release its Salmon above us
        if (todo == TASKLIST_TODO_BREAK)
        {
            for (DWORD i = 0; i < ProcessList->ItemsCount; i++)
            {
                if (ProcessList->Items[i].PID == pid)
                {
                    AllowSetForegroundWindow(ProcessList->Items[i].PID);       // We'd rather allow our own Salamander, even though it's probably unnecessary...
                    AllowSetForegroundWindow(ProcessList->Items[i].SalmonPID); // we definitely have to let his Salmon over us
                    break;
                }
            }
        }

        // Release ProcessList
        ReleaseMutex(FMOMutex);

        // we will start the check in all Salamanders
        ResetEvent(EventProcessed);
        SetEvent(Event);

        //--- give a moment to react (during this time, someone should "catch on" and complete the task)
        BOOL ret = (WaitForSingleObject(EventProcessed, 1000) == WAIT_OBJECT_0);

        //--- let's tell all Salamanders to prepare for the next command
        ResetEvent(Event);

        //--- set back to break-PID
        //    ProcessList->Todo = 0;
        //    ProcessList->PID = 0;

        //---  release FMO

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

    // we will look for a running process in our class, possibly starting (we will wait a moment to see if it starts)
    int firstStarting = -1; // index of a process that is from our class (same Integrity Level and SID) but does not have a main window yet
    int firstRunnig = -1;   // index of a process that is from our class (same Integrity Level and SID) and is already running (has a main window)
    DWORD timeStamp = GetTickCount();
    do
    {
        firstStarting = -1;
        firstRunnig = -1;
        DWORD ret = WaitForSingleObject(FMOMutex, 200);
        if (ret == WAIT_FAILED)
            return FALSE;
        if (ret != WAIT_TIMEOUT) // we have received the mutex
        {
            int i;
            for (i = 0; i < (int)ProcessList->ItemsCount; i++)
            {
                CProcessListItem* item = &ProcessList->Items[i];
                // Searching only for processes in our class (with the same IntegrityLevel and SID)
                if (item->PID != ourProcessInfo.PID &&
                    item->IntegrityLevel == ourProcessInfo.IntegrityLevel &&
                    memcmp(item->SID_MD5, ourProcessInfo.SID_MD5, 16) == 0)
                {
                    if (item->ProcessState == PROCESS_STATE_RUNNING)
                    {
                        firstRunnig = i;
                        break; // if we have found a running instance, we don't need to search for a starting one anymore
                    }
                    if (item->ProcessState == PROCESS_STATE_STARTING && firstStarting == -1)
                        firstStarting = i;
                }
            }

            if (firstRunnig == -1) // no process from our class currently has a main window
            {
                ReleaseMutex(FMOMutex); // so let's free the memory for others
                if (firstStarting == -1)
                    return FALSE; // We did not find any qualified candidate, we will drop out
                else
                    Sleep(200); // We found a candidate who is starting, we will pause for 200ms to give them a chance to call SetProcessState()
            }
        }
    } while (firstRunnig == -1 && (GetTickCount() - timeStamp < TASKLIST_TODO_TIMEOUT)); // we are waiting for the running instance for a maximum of 5 seconds

    // if we haven't found any instance of our class that should have the main window, or if waiting took us 5s, we wrap it up
    if (firstRunnig == -1)
        return FALSE;

    CProcessListItem* item = &ProcessList->Items[firstRunnig];

    // Set up Todo, PID, and parameters
    ProcessList->Todo = TASKLIST_TODO_ACTIVATE;
    ProcessList->TodoUID++; // rekneme procesum, ze se bude zpracovavat novy command
    ProcessList->TodoTimestamp = GetTickCount();
    ProcessList->PID = item->PID;

    // we will take parameters from the command-line
    memcpy(&ProcessList->CommandLineParams, cmdLineParams, sizeof(CCommandLineParams));
    // and we will set our internal variables
    ProcessList->CommandLineParams.Version = 1;
    ProcessList->CommandLineParams.RequestUID = ProcessList->TodoUID;
    ProcessList->CommandLineParams.RequestTimestamp = ProcessList->TodoTimestamp;

    // Allow the activated process to call SetForegroundWindow, otherwise it will not be able to bring itself up
    AllowSetForegroundWindow(item->PID);

    // we will start the check in all Salamanders
    // release shared memory
    ReleaseMutex(FMOMutex);

    ResetEvent(EventProcessed);
    SetEvent(Event);

    // Give me a moment to react (during this time, someone should "catch on" and complete the task)
    // 500ms is our reserve to safely cover subordinate threads
    BOOL ret = (WaitForSingleObject(EventProcessed, TASKLIST_TODO_TIMEOUT + 500) == WAIT_OBJECT_0);

    // Let's tell all Salamanders to prepare for the next command (it will also reset in the control thread if any process is performing a todo).
    ResetEvent(Event);

    // reset todo
    // ProcessList->Todo = 0; // we should first acquire the FMOMutex, but in this case there is nothing to mess up and we can zero out the values
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
            // On older versions of Windows, we can obtain a handle even for a terminated process
            // it is therefore necessary to inquire about the exitcode; probably unnecessary from W2K onwards
            BOOL cont = FALSE;
            DWORD exitcode;
            if (!GetExitCodeProcess(h, &exitcode) || exitcode == STILL_ACTIVE)
                cont = TRUE;
            NOHANDLES(CloseHandle(h));
            if (cont)
                continue; // let the process in the list
        }
        else
        {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_ACCESS_DENIED)
            {
                continue; // let the process in the list
            }
        }
        memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CProcessListItem));
        c--;
        i--;
        if (changed != NULL)
            *changed = TRUE;
    }
    ProcessList->ItemsCount = c;

    /*  // does not work under XP if processes within one session are run by different users
// we do not have the right to open a handle of another process
//---  we terminate killed processes
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
        if (cont) continue;  // leave the process in the list
      }
//---  remove the process from the list
      memmove(ptr + i, ptr + i + 1, (c - i - 1) * sizeof(CTLItem));
      c--;
      i--;
    }
    ((DWORD *)SharedMem)[0] = c;   // items-count
    memcpy(items, ptr, c * sizeof(CTLItem));*/

    return TRUE;
}