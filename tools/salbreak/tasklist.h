// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************

// TRUE = the first running instance of version 3.0 or newer
// determined based on a mutex in the global namespace, so it is visible together
// with mutexes from other sessions (remote desktop, fast user switching)
extern BOOL FirstInstance_3_or_later;

// shared memory contains:
//  DWORD                  - PID of the process that should be broken
//  DWORD                  - number of items in the list
//  MAX_TL_ITEMS * CTLItem - item list

#define MAX_TL_ITEMS 500 // maximum number of items in shared memory, cannot be changed!

#define TASKLIST_TODO_HIGHLIGHT 1 // the window of the process given in 'PID' should be highlighted
#define TASKLIST_TODO_BREAK 2     // the process given in 'PID' should be broken
#define TASKLIST_TODO_TERMINATE 3 // the process given in 'PID' should be terminated
#define TASKLIST_TODO_ACTIVATE 4  // the process given in 'PID' should be activated

#define TASKLIST_TODO_TIMEOUT 5000 // 5 seconds that processes have to handle todo

#define PROCESS_STATE_STARTING 1 // our process is starting, the main window does not exist yet
#define PROCESS_STATE_RUNNING 2  // our process is running, we have the main window
#define PROCESS_STATE_ENDING 3   // our process is ending, we no longer have the main window

#pragma pack(push, enter_include_tasklist) // keep the structures independent of the current packing setting
#pragma pack(4)

//extern HANDLE HSalmonProcess;

// WARNING: x64 and x86 processes communicate through this structure, so watch out
// for types (for example HANDLE) that have different widths
struct CProcessListItem
{
    DWORD PID;            // ProcessID, unique for the lifetime of the process, then it may be reused
    SYSTEMTIME StartTime; // when the process was started
    DWORD IntegrityLevel; // process Integrity Level, used to distinguish processes running with different privilege levels
    BYTE SID_MD5[16];     // MD5 computed from the process SID, used to distinguish processes running under different users; the SID has unknown length, hence this workaround
    DWORD ProcessState;   // state Salamander is currently in, see PROCESS_STATE_xxx
    UINT64 HMainWindow;   // (x64 friendly) handle of the main window, if it already/still exists (set when it is created/destroyed)
    DWORD SalmonPID;      // Salmon ProcessID, so the breaking process can grant it the SetForegroundWindow right

    CProcessListItem()
    {
        PID = GetCurrentProcessId();
        GetLocalTime(&StartTime);
        GetProcessIntegrityLevel(&IntegrityLevel);
        GetSidMD5(SID_MD5);
        ProcessState = PROCESS_STATE_STARTING;
        HMainWindow = NULL;
        SalmonPID = 0;
        //    if (HSalmonProcess != NULL)
        //      SalmonPID = GetProcessId(HSalmonProcess); // Salmon is already running at this point
    }
};

// Open Salamander Process List
// !!! WARNING: items may only be added to this structure, because older Salamander versions also use it
struct CProcessList
{
    DWORD Version; // newer Salamander versions may increase 'Version' and start using ReservedX variables

    DWORD ItemsCount;    // number of valid items in the Items array
    DWORD ItemsStateUID; // "version" of the Items list; increments on every change; used by the Tasks dialog as a signal that it should refresh
    CProcessListItem Items[MAX_TL_ITEMS];

    DWORD Todo;          // determines what should be done after the event is triggered via FireEvent; contains one of the TASKLIST_TODO_* values
    DWORD TodoUID;       // sequence number of the sent request; increments for each further request
    DWORD TodoTimestamp; // GetTickCount() value from the moment the Todo request was created
    DWORD PID;           // PID for which the Todo action should be performed
                         //CCommandLineParams CommandLineParams;// paths for panels and other activation parameters
                         // WARNING: if this structure ever needs to be extended, it would be sensible to extend CCommandLineParams first, for example
                         // reserve some MAX_PATH buffers and a few DWORDs if we wanted to pass new command-line parameters
};

#pragma pack(pop, enter_include_tasklist)

class CTaskList
{
protected:
    HANDLE FMO;                // file-mapping object, shared memory
    CProcessList* ProcessList; // pointer into shared memory
    HANDLE FMOMutex;           // mutex for synchronizing access to the FMO
    HANDLE Event;              // if this event is signaled, the other processes should check
                               // whether they should perform the action given in Todo
    HANDLE EventProcessed;     // if one of the processes performs the action in Todo, it sets this
                               // event to signaled to inform the controlling process that it is done
    BOOL OK;                   // did construction complete successfully?

public:
    CTaskList();
    ~CTaskList();

    BOOL Init();

    // fills the task-list items; items is an array of at least MAX_TL_ITEMS CTLItem structures; returns the item count
    // 'items' may be NULL if we are interested only in 'itemsStateUID'
    // returns the process-list "version"; the version increases with every change in the list (when an item is added or removed)
    // serves as information for the dialog that it should refresh the list; 'itemsStateUID' may be NULL
    // if 'timeouted' is not NULL, sets whether the failure was caused by a timeout while waiting for shared memory
    int GetItems(CProcessListItem* items, DWORD* itemsStateUID, BOOL* timeouted = NULL);

    // asks process 'pid' to perform the action specified by 'todo' (except TASKLIST_TODO_ACTIVATE)
    // if 'timeouted' is not NULL, sets whether the failure was caused by a timeout while waiting for shared memory
    BOOL FireEvent(DWORD todo, DWORD pid, BOOL* timeouted = NULL);

protected:
    // walks the process list and filters out non-existing items
    // must be called only after successfully entering the 'FMOMutex' critical section!
    // sets 'changed' to TRUE if some item was discarded, otherwise FALSE
    BOOL RemoveKilledItems(BOOL* changed);
};

extern CTaskList TaskList;
