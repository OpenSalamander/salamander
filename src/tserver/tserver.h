// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define TRACE_SERVER_VERSION 7 // aktualni verze serveru

#define HOT_KEY_ID 0x0001
#define HOT_KEYCLEAR_ID 0x0002

/// Nazev tridy MainWindow.
#define WC_MAINWINDOW L"MainWindowsTSClass"

// nazev aplikace
extern const WCHAR* MAINWINDOW_NAME;
// nazvy souboru
extern const WCHAR* CONFIG_FILENAME;

// texty v about dialogu
extern WCHAR AboutText1[]; // texty, ktere se zobrazi
extern WCHAR AboutText2[]; // v dialogu About

extern BOOL UseMaxMessagesCount;
extern int MaxMessagesCount;

extern BOOL WindowsVistaAndLater;

// prijde hlavnimu oknu v pripade, ze connecting thread skonci s chybou
#define WM_USER_CT_TERMINATED WM_USER + 100 // [0, 0]
// uvolni OpenConnectionMutex -> umozni komunikaci mezi clientem a serverem
#define WM_USER_CT_OPENCONNECTION WM_USER + 101 // [0, 0]
// ohlasi chybu - chybove konstanty viz nize
#define WM_USER_SHOWERROR WM_USER + 102 // [error code, 0]
// doslo ke zmene v Data.Processes
#define WM_USER_PROCESSES_CHANGE WM_USER + 103 // [0, 0]
// doslo ke zmene v Data.Threads
#define WM_USER_THREADS_CHANGE WM_USER + 104 // [0, 0]
// novy process se konektnul
#define WM_USER_PROCESS_CONNECTED WM_USER + 105 // [0, 0]
// ohlasi systemovou chybu
#define WM_USER_PROCESS_DISCONNECTED WM_USER + 107 // [processID, 0]
// ohlasi systemovou chybu
#define WM_USER_SHOWSYSTEMERROR WM_USER + 108 // [sysErrCode, 0]
// pokud se zaplnila message cache
#define WM_USER_FLUSH_MESSAGES_CACHE WM_USER + 109 // [0, 0]
// spatna verze clientu (clientName je alokovano -> volat free)
#define WM_USER_INCORRECT_VERSION WM_USER + 110 // [client, client PID]

// pokud dojde nad ikonou k nejake udalosti, prijde tato message
#define WM_USER_ICON_NOTIFY WM_USER + 111 // [0, 0]

// doslo ke zmene viditelnosti sloupcu v seznamu
#define WM_USER_HEADER_CHANGE WM_USER + 120 // [0, 0]

// doslo ke zmene poradi sloupcu v seznamu
#define WM_USER_HEADER_POS_CHANGE WM_USER + 121 // [0, 0]

// doslo ke zmene poradi sloupcu v seznamu
#define WM_USER_HEADER_DEL_ITEM WM_USER + 122 // [index, 0]

// doslo k aktivaci aplikace
#define WM_USER_ACTIVATE_APP WM_USER + 123 // [0, 0]

// uzivatel drazdi vodorovnou scrollbaru u listboxu
#define WM_USER_HSCROLL WM_USER + 124 // [pos, 0]

//// uzivatel provedl doubleclik v listboxu
//#define WM_USER_LISTBOX_DBLCLK       WM_USER + 114  // [0, 0]

// pokud prekroci pocet messagi tuto mez, je postnuta message na flushnuti
#define MESSAGES_CACHE_MAX 1000

// exit cody connecting threadu
#define CT_SUCCESS 0
#define CT_UNABLE_TO_CREATE_FILE_MAPPING 1
#define CT_UNABLE_TO_MAP_VIEW_OF_FILE 2

// chybove kody pro WM_USER_SHOWERROR
#define EC_CANNOT_CREATE_READ_PIPE_THREAD 10
#define EC_LOW_MEMORY 11
#define EC_UNKNOWN_MESSAGE_TYPE 12

// mutex - vlastni ho client proces, ktery zapisuje do sdilene pameti
extern HANDLE OpenConnectionMutex;
// event - signaled -> server prijal data ze sdilene pameti
extern HANDLE ConnectDataAcceptedEvent;
extern BOOL ConnectDataAcceptedEventMayBeSignaled;
// event - manual reset - nahodit po flushnuti message cache
extern HANDLE MessagesFlushDoneEvent;

// thread, ktery zajistuje pripojeni k serveru
extern HANDLE ConnectingThread;

// pokud je tato promenna FALSE, nelze program ovladat pomoci ikony
extern BOOL IconControlEnable;

inline BOOL TServerIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // nahrada za memset (nevyzaduje RTLko)
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
    DWORD ProcessID;     // PID processu, ze ktereho message pochazi
    DWORD ThreadID;      // pro upresneni jeste ID threadu
    C__MessageType Type; // typ zpravy
    SYSTEMTIME Time;     // cas vzniku message
    double Counter;      // presne pocitadlo v ms
    WCHAR* Message;      // text message (ukazatel do File bufferu za 1. 0)
    WCHAR* File;         // jmeno souboru
    DWORD Line;          // cislo radky
    DWORD Index;         // poradi, ve kterem byla message vlozena

    DWORD UniqueProcessID; // unikatni ID procesu
    DWORD UniqueThreadID;  // unikatni ID threadu

    static DWORD StaticIndex; // index pro dalsi message

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

    /// Otevre BOSSe s prislusnym souborem a radkou.
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
    // vola se z timeru a pri preplneni cache
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

// ukazatal na hlavni okno
extern CMainWindow* MainWindow;
