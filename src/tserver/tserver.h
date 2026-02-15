// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define TRACE_SERVER_VERSION 7 // current server version

#define HOT_KEY_ID 0x0001
#define HOT_KEYCLEAR_ID 0x0002

/// Name of the MainWindow class.
#define WC_MAINWINDOW L"MainWindowsTSClass"

// application name
extern const WCHAR* MAINWINDOW_NAME;
// file names
extern const WCHAR* CONFIG_FILENAME;

// texts for the About dialog
extern WCHAR AboutText1[]; // texts that will be displayed
extern WCHAR AboutText2[]; // in the About dialog

extern BOOL UseMaxMessagesCount;
extern int MaxMessagesCount;

extern BOOL WindowsVistaAndLater;

// posted to the main window if the connecting thread ends with an error
#define WM_USER_CT_TERMINATED WM_USER + 100 // [0, 0]
// releases OpenConnectionMutex -> allows communication between client and server
#define WM_USER_CT_OPENCONNECTION WM_USER + 101 // [0, 0]
// reports an error - see error constants below
#define WM_USER_SHOWERROR WM_USER + 102 // [error code, 0]
// Data.Processes changed
#define WM_USER_PROCESSES_CHANGE WM_USER + 103 // [0, 0]
// Data.Threads changed
#define WM_USER_THREADS_CHANGE WM_USER + 104 // [0, 0]
// a new process connected
#define WM_USER_PROCESS_CONNECTED WM_USER + 105 // [0, 0]
// reports a system error
#define WM_USER_PROCESS_DISCONNECTED WM_USER + 107 // [processID, 0]
// reports a system error
#define WM_USER_SHOWSYSTEMERROR WM_USER + 108 // [sysErrCode, 0]
// message cache filled up
#define WM_USER_FLUSH_MESSAGES_CACHE WM_USER + 109 // [0, 0]
// incorrect client version (clientName is allocated -> call free)
#define WM_USER_INCORRECT_VERSION WM_USER + 110 // [client, client PID]

// message sent when something happens over the icon
#define WM_USER_ICON_NOTIFY WM_USER + 111 // [0, 0]

// column visibility in the list changed
#define WM_USER_HEADER_CHANGE WM_USER + 120 // [0, 0]

// column order in the list changed
#define WM_USER_HEADER_POS_CHANGE WM_USER + 121 // [0, 0]

// column order in the list changed
#define WM_USER_HEADER_DEL_ITEM WM_USER + 122 // [index, 0]

// application was activated
#define WM_USER_ACTIVATE_APP WM_USER + 123 // [0, 0]

// user is fiddling with the horizontal scrollbar of the list box
#define WM_USER_HSCROLL WM_USER + 124 // [pos, 0]

//// user double-clicked in the list box
//#define WM_USER_LISTBOX_DBLCLK       WM_USER + 114  // [0, 0]

// if the number of messages exceeds this limit, a flush message is posted
#define MESSAGES_CACHE_MAX 1000

// exit codes of the connecting thread
#define CT_SUCCESS 0
#define CT_UNABLE_TO_CREATE_FILE_MAPPING 1
#define CT_UNABLE_TO_MAP_VIEW_OF_FILE 2

// error codes for WM_USER_SHOWERROR
#define EC_CANNOT_CREATE_READ_PIPE_THREAD 10
#define EC_LOW_MEMORY 11
#define EC_UNKNOWN_MESSAGE_TYPE 12

// mutex owned by the client process that writes into shared memory
extern HANDLE OpenConnectionMutex;
// event - signaled -> the server accepted data from shared memory
extern HANDLE ConnectDataAcceptedEvent;
extern BOOL ConnectDataAcceptedEventMayBeSignaled;
// event - manual reset - set after the message cache is flushed
extern HANDLE MessagesFlushDoneEvent;

// thread that handles connecting to the server
extern HANDLE ConnectingThread;

// if this variable is FALSE, the program cannot be controlled through the icon
extern BOOL IconControlEnable;

inline BOOL TServerIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // replacement for memset (does not require RTL)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

inline BOOL IsErrorMsg(C__MessageType type) { return type == __mtError || type == __mtErrorW; }

//****************************************************************************
//
// TSynchronizedDirectArray
//

template <class DATA_TYPE>
class TSynchronizedDirectArray : protected TDirectArray<DATA_TYPE>
{
protected:
    CRITICAL_SECTION CriticalSection;
    BOOL ArrayBlocked;

public:
    TSynchronizedDirectArray<DATA_TYPE>(int base, int delta)
        : TDirectArray<DATA_TYPE>(base, delta)
    {
        HANDLES(InitializeCriticalSection(&CriticalSection));
        ArrayBlocked = FALSE;
    }

    ~TSynchronizedDirectArray<DATA_TYPE>()
    {
        HANDLES(DeleteCriticalSection(&CriticalSection));
    }

    void BlockArray();
    void UnBlockArray();

    BOOL CroakIfNotBlocked()
    {
#ifndef ARRAY_NODEBUG
        if (!ArrayBlocked)
            MESSAGE_TEW(L"Call to this method requires locking!", MB_OK);
#endif // ARRAY_NODEBUG
        return !ArrayBlocked;
    }

    BOOL IsGood();
    void ResetState();

    void Insert(DWORD index, const DATA_TYPE& member)
    {
        CroakIfNotBlocked();
        TDirectArray<DATA_TYPE>::Insert(index, member);
    }

    DWORD Add(const DATA_TYPE& member)
    {
        CroakIfNotBlocked();
        return TDirectArray<DATA_TYPE>::Add(member);
    }

    void Delete(DWORD index)
    {
        CroakIfNotBlocked();
        TDirectArray<DATA_TYPE>::Delete(index);
    }

    int GetCount()
    {
        CroakIfNotBlocked();
        return this->Count;
    }

    DATA_TYPE& At(DWORD index)
    {
        CroakIfNotBlocked();
        return TDirectArray<DATA_TYPE>::At(index);
    }

    DATA_TYPE& operator[](DWORD index)
    {
        CroakIfNotBlocked();
        return TDirectArray<DATA_TYPE>::At(index);
    }

    void DestroyMembers()
    {
        CroakIfNotBlocked();
        TDirectArray<DATA_TYPE>::DestroyMembers();
    }
};

//****************************************************************************
//
// CGlobalDataArray
//

class CGlobalDataMessage
{
public:
    DWORD ProcessID;     // PID of the process that produced the message
    DWORD ThreadID;      // thread ID for better precision
    C__MessageType Type; // message type
    SYSTEMTIME Time;     // time when the message originated
    double Counter;      // precise counter in ms
    WCHAR* Message;      // message text (pointer to the File buffer after the first null)
    WCHAR* File;         // file name
    DWORD Line;          // line number
    DWORD Index;         // order in which the message was inserted

    DWORD UniqueProcessID; // unique process ID
    DWORD UniqueThreadID;  // unique thread ID

    static DWORD StaticIndex; // index for the next message

public:
    BOOL operator<(const CGlobalDataMessage& message);
};

struct CProcessInformation
{
    DWORD UniqueProcessID;
    WCHAR* Name;
};

struct CThreadInformation
{
    DWORD UniqueProcessID;
    DWORD UniqueThreadID;
    WCHAR* Name;
};

class CGlobalData
{
public:
    TDirectArray<CGlobalDataMessage> Messages;
    TSynchronizedDirectArray<CGlobalDataMessage> MessagesCache;
    TSynchronizedDirectArray<CProcessInformation> Processes;
    TSynchronizedDirectArray<CThreadInformation> Threads;
    BOOL MessagesFlushInProgress;

protected:
    BOOL EditorConnected;

public:
    CGlobalData();
    ~CGlobalData();

    int FindProcessNameIndex(DWORD uniqueProcessID);
    int FindThreadNameIndex(DWORD uniqueProcessID, DWORD uniqueThreadID);

    void GetProcessName(DWORD uniqueProcessID, WCHAR* buff, int buffLen);
    void GetThreadName(DWORD uniqueProcessID, DWORD uniqueThreadID,
                       WCHAR* buff, int buffLen);

    /// Opens BOSS with the corresponding file and line.
    void GotoEditor(int index);
};

//*****************************************************************************
//
// BuildFonts
//

BOOL BuildFonts();

//*****************************************************************************
//
// CMainWindow
//

class CMainWindow : public CWindow
{
public:
    CTabList* TabList;

    BOOL HasHotKey;
    BOOL HasHotKeyClear;
    UINT TaskbarRestartMsg;

public:
    CMainWindow();

    BOOL TaskBarAddIcon();
    BOOL TaskBarRemoveIcon();
    void ClearAllMessages();
    void ExportAllMessages();
    void GetWindowPos();
    void Activate();
    void ShowMessageDetails();

protected:
    // called from the timer and when the cache overflows
    void FlushMessagesCache(BOOL& ErrorMessage);
    void OnErrorMessage();

    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// TSynchronizedDirectArray
//

template <class DATA_TYPE>
void TSynchronizedDirectArray<DATA_TYPE>::BlockArray()
{
    HANDLES(EnterCriticalSection(&CriticalSection));
    if (ArrayBlocked)
        MESSAGE_TEW(L"Recursive call to BlockArray() is not supported.", MB_OK);
    ArrayBlocked = TRUE;
}

template <class DATA_TYPE>
void TSynchronizedDirectArray<DATA_TYPE>::UnBlockArray()
{
    if (!ArrayBlocked)
        MESSAGE_TEW(L"Call to UnBlockArray() is possible only immediately after BlockArray().", MB_OK);
    ArrayBlocked = FALSE;
    HANDLES(LeaveCriticalSection(&CriticalSection));
}

template <class DATA_TYPE>
BOOL TSynchronizedDirectArray<DATA_TYPE>::IsGood()
{
    HANDLES(EnterCriticalSection(&CriticalSection));
    BOOL good = TDirectArray<DATA_TYPE>::IsGood();
    HANDLES(LeaveCriticalSection(&CriticalSection));
    return good;
}

template <class DATA_TYPE>
void TSynchronizedDirectArray<DATA_TYPE>::ResetState()
{
    HANDLES(EnterCriticalSection(&CriticalSection));
    TDirectArray<DATA_TYPE>::ResetState();
    HANDLES(LeaveCriticalSection(&CriticalSection));
}

//*****************************************************************************

extern CGlobalData Data;

// pointer to the main window
extern CMainWindow* MainWindow;
