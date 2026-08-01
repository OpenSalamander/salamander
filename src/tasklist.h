// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//
// ****************************************************************************

// TRUE = first running instance of version 3.0 or later
// Determined using a mutex in the global namespace, so it is visible across sessions
// (remote desktop, fast user switching)
extern BOOL FirstInstance_3_or_later;

// Shared memory contains:
//  DWORD                  - PID of the process that should be broken into
//  DWORD                  - number of items in the list
//  MAX_TL_ITEMS * CTLItem - list of items

#define MAX_TL_ITEMS 500 // maximum number of items in shared memory, cannot be changed!

#define TASKLIST_TODO_HIGHLIGHT 1 // window of the process in 'PID' is to be highlighted
#define TASKLIST_TODO_BREAK 2     // the process specified by 'PID' is to be broken into
#define TASKLIST_TODO_TERMINATE 3 // process given in 'PID' is to be terminated
#define TASKLIST_TODO_ACTIVATE 4  // process given in 'PID' is to be activated

#define TASKLIST_TODO_TIMEOUT 5000 // 5 seconds that processes have to process the todo entry

#define PROCESS_STATE_STARTING 1 // our process is starting, the main window does not exist yet
#define PROCESS_STATE_RUNNING 2  // our process is running, we have the main window
#define PROCESS_STATE_ENDING 3   // our process is ending, we no longer have the main window

#pragma pack(push, enter_include_tasklist) // make the structures independent of the current alignment setting
#pragma pack(4)

extern HANDLE HSalmonProcess;

// WARNING, x64 and x86 processes communicate through the structure; mind the types
// (e.g. HANDLE) that have different widths
struct CProcessListItem
{
    DWORD PID;            // ProcessID, unique for the lifetime of the process, after which it can be reused
    SYSTEMTIME StartTime; // When the process was started
    DWORD IntegrityLevel; // Integrity Level of the process, distinguishes processes running at different privilege levels
    BYTE SID_MD5[16];     // MD5 calculated from the process SID, distinguishes processes running under different users; SID has an unknown length, hence this workaround
    DWORD ProcessState;   // State Salamander is currently in; see PROCESS_STATE_xxx
    UINT64 HMainWindow;   // (x64 friendly) Handle of the main window, if it already/still exists (set during creation/destruction)
    DWORD SalmonPID;      // Salmon ProcessID, so the process that breaks in can guarantee it has
                          // permission to call SetForegroundWindow

    CProcessListItem()
    {
        PID = GetCurrentProcessId();
        GetLocalTime(&StartTime);
        GetProcessIntegrityLevel(&IntegrityLevel);
        GetSidMD5(SID_MD5);
        ProcessState = PROCESS_STATE_STARTING;
        HMainWindow = NULL;
        SalmonPID = 0;
        if (HSalmonProcess != NULL)
            SalmonPID = SalGetProcessId(HSalmonProcess); // at this moment Salmon is already running
    }
};

// WARNING, only new items can be added to the structure because older Salamander versions use it
// WARNING, x64 and x86 processes communicate using the structure; mind the types (e.g. HANDLE) that have different widths
// WARNING, increasing the version and expanding the structure probably makes no sense because
//        the added data would not always be available (if an older Salamander version is
//        launched first, the new items will not exist in shared memory)
//        => the proper solution is likely to change AS_PROCESSLIST_NAME and similar constants
//        and then adjust the data as desired (feel free to enlarge, prune, reorder, etc.)
struct CCommandLineParams
{
    DWORD Version;               // newer Salamander versions can increase 'Version' and start using ReservedX variables
    DWORD RequestUID;            // unique (monotonically increasing) ID of the activation request
    DWORD RequestTimestamp;      // GetTickCount() value from the moment the activation request was created
    char LeftPath[2 * MAX_PATH]; // panel paths (left, right, possibly active); if empty, leave them unset
    char RightPath[2 * MAX_PATH];
    char ActivePath[2 * MAX_PATH];
    DWORD ActivatePanel;         // which panel to activate: 0-none, 1-left, 2-right
    BOOL SetTitlePrefix;         // if TRUE, set the title prefix according to TitlePrefix
    char TitlePrefix[MAX_PATH];  // title prefix; if empty, leave unchanged; I prefer to declare the length as MAX_PATH instead of TITLE_PREFIX_MAX, which might change unexpectedly
    BOOL SetMainWindowIconIndex; // if TRUE, set the main window icon according to MainWindowIconIndex
    DWORD MainWindowIconIndex;   // 0: first icon, 1: second icon, ...
    // WARNING, the structure can only be expanded if it is still declared as the last one in CProcessList;
    // otherwise it is already too late and it must not be touched

    CCommandLineParams()
    {
        ZeroMemory(this, sizeof(CCommandLineParams));
    }
};

// Open Salamander Process List
// !!! WARNING, only new items can be added to the structure because older Salamander versions use it
struct CProcessList
{
    DWORD Version; // newer Salamander versions can increase 'Version' and start using ReservedX variables

    DWORD ItemsCount;    // number of valid items in the Items array
    DWORD ItemsStateUID; // "version" of the Items list; increases with every change; used by the Tasks dialog as a signal to refresh
    CProcessListItem Items[MAX_TL_ITEMS];

    DWORD Todo;                           // specifies what to do after FireEvent signals the event; contains one of the TASKLIST_TODO_* values
    DWORD TodoUID;                        // sequence number of the submitted request; increases with every subsequent request
    DWORD TodoTimestamp;                  // GetTickCount() value from the moment the Todo request was created
    DWORD PID;                            // PID for which the Todo action should be performed
    CCommandLineParams CommandLineParams; // panel paths and other parameters for activation
                                          // WARNING, if this structure needs to be extended, it makes sense to extend CCommandLineParams first, for example
                                          // reserve a few MAX_PATH buffers and some DWORDs if we want to pass new command-line parameters
};

#pragma pack(pop, enter_include_tasklist)

class CTaskList
{
protected:
    HANDLE FMO;                // file-mapping-object, shared memory
    CProcessList* ProcessList; // pointer into shared memory
    HANDLE FMOMutex;           // mutex used to control access to the FMO
    HANDLE Event;              // event; when it becomes signaled, other processes should check
                               // whether they should perform the activity specified in Todo
    HANDLE EventProcessed;     // if one of the processes performs the Todo activity, it sets this
                               // event to signaled to let the controlling process know it is finished
    HANDLE TerminateEvent;     // event for terminating the break thread
    HANDLE ControlThread;      // control thread (waits for events and services them immediately)
    BOOL OK;                   // did construction finish OK?

public:
    CTaskList();
    ~CTaskList();

    BOOL Init();

    // Fills the task-list items; 'items' is an array of at least MAX_TL_ITEMS CProcessListItem structures; returns the number of items
    // 'items' can be NULL if only 'itemsStateUID' is needed
    // 'itemsStateUID' returns the "version" of the process list; the version increases with every change to the list (when an item is added or removed)
    // Used by the dialog to know when to refresh the list; 'itemsStateUID' can be NULL
    // If 'timeouted' is not NULL, it is set to indicate whether the failure was caused by a timeout while waiting for shared memory
    int GetItems(CProcessListItem* items, DWORD* itemsStateUID, BOOL* timeouted = NULL);

    // Requests process 'pid' to perform the action defined by 'todo' (except TASKLIST_TODO_ACTIVATE)
    // if 'timeouted' is not NULL, it sets whether the failure was caused by a timeout when waiting for shared memory
    BOOL FireEvent(DWORD todo, DWORD pid, BOOL* timeouted = NULL);

    // If 'timeouted' is not NULL, it is set to indicate whether the failure was caused by a timeout while waiting for shared memory
    BOOL ActivateRunningInstance(const CCommandLineParams* cmdLineParams, BOOL* timeouted = NULL);

    // Searches the process list for our entry and sets 'ProcessState' and 'HMainWindow'; returns TRUE on success, otherwise FALSE
    // if 'timeouted' is not NULL, it sets whether the failure was caused by a timeout when waiting for shared memory
    BOOL SetProcessState(DWORD processState, HWND hMainWindow, BOOL* timeouted = NULL);

protected:
    // Walks the process list and removes entries whose processes no longer exist
    // Call only after successfully entering the 'FMOMutex' critical section!
    // Sets 'changed' to TRUE if it discarded any item, otherwise to FALSE
    BOOL RemoveKilledItems(BOOL* changed);

    friend DWORD WINAPI FControlThread(void* param);
};

extern CTaskList TaskList;

// protection for access to CommandLineParams
extern CRITICAL_SECTION CommandLineParamsCS;
// used to hand off activation parameters from the Control thread to the main thread
extern CCommandLineParams CommandLineParams;
// event becomes "signaled" as soon as the main thread takes over the parameters
extern HANDLE CommandLineParamsProcessed;
