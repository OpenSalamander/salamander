// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// __DEBUG_WINLIB enables several checks for tricky WinLib bugs

// WinLib string constants (for internal WinLib use only)
enum CWLS
{
    WLS_INVALID_NUMBER,
    WLS_ERROR,

    WLS_COUNT
};

// set custom WinLib strings
void SetWinLibStrings(const TCHAR* invalidNumber, // "not a number" (for numeric transfer buffers)
                      const TCHAR* error);        // title "error" (for numeric transfer buffers)

extern HINSTANCE HInstance;
extern const TCHAR* CWINDOW_CLASSNAME;  // universal window class name
extern const TCHAR* CWINDOW_CLASSNAME2; // universal window class name - no CS_VREDRAW | CS_HREDRAW

#ifndef _UNICODE
extern const WCHAR* CWINDOW_CLASSNAMEW;  // Unicode universal window class name
extern const WCHAR* CWINDOW_CLASSNAME2W; // Unicode universal window class name - no CS_VREDRAW | CS_HREDRAW
#endif                                   // _UNICODE

class CWinLibHelp;

// must be called before using WinLib
BOOL InitializeWinLib();
// must be called after using WinLib
void ReleaseWinLib();
// must be called before using help
BOOL SetupWinLibHelp(CWinLibHelp* winLibHelp);

class CWinLibHelp
{
public:
    virtual void OnHelp(HWND /*hWindow*/, UINT /*helpID*/, HELPINFO* /*helpInfo*/,
                        BOOL /*ctrlPressed*/, BOOL /*shiftPressed*/) {}
    virtual void OnContextMenu(HWND /*hWindow*/, WORD /*xPos*/, WORD /*yPos*/) {}
};

// ****************************************************************************

enum CObjectOrigin // used when destroying windows and dialogs
{
    ooAllocated, // deallocated on WM_DESTROY
    ooStatic,    // HWindow is set to NULL on WM_DESTROY
    ooStandard   // for modal dialogs = ooStatic, for modeless dialogs = ooAllocated
};

// ****************************************************************************

enum CObjectType // for object type identification
{
    otBase,
    otWindow,
    otDialog,
    otPropSheetPage,
    otLastWinLibObject
};

// ****************************************************************************

class CWindowsObject // base class for all Windows objects
{
public:
    HWND HWindow;
    UINT HelpID; // -1 = empty value (do not use help)

    CWindowsObject(CObjectOrigin origin
#ifndef _UNICODE
                   ,
                   BOOL unicodeWnd
#endif // _UNICODE
    )
    {
        HWindow = NULL;
        ObjectOrigin = origin;
#ifndef _UNICODE
        UnicodeWnd = unicodeWnd;
#endif // _UNICODE
        HelpID = -1;
    }

    CWindowsObject(UINT helpID, CObjectOrigin origin
#ifndef _UNICODE
                   ,
                   BOOL unicodeWnd
#endif // _UNICODE
    )
    {
        HWindow = NULL;
        ObjectOrigin = origin;
#ifndef _UNICODE
        UnicodeWnd = unicodeWnd;
#endif // _UNICODE
        SetHelpID(helpID);
    }

    virtual ~CWindowsObject() {} // so derived destructors are called

    virtual BOOL Is(int) { return FALSE; } // object type identification
    virtual int GetObjectType() { return otBase; }

    virtual BOOL IsAllocated() { return ObjectOrigin == ooAllocated; }
    void SetObjectOrigin(CObjectOrigin origin) { ObjectOrigin = origin; }

    void SetHelpID(UINT helpID)
    {
        if (helpID == -1)
            TRACE_ET(_T("CWindowsObject::SetHelpID(): helpID==-1, -1 is 'empty value', you should use another helpID! If you want to set HelpID to -1, use ClearHelpID()."));
        HelpID = helpID;
    }
    void ClearHelpID() { HelpID = -1; }

protected:
    CObjectOrigin ObjectOrigin;
#ifndef _UNICODE
    // for windows: on create, TRUE = the window is Unicode, otherwise ANSI; on attach, TRUE = our window procedure
    // is Unicode, otherwise ANSI; for dialogs: TRUE = the dialog is Unicode, otherwise ANSI
    BOOL UnicodeWnd;
#endif // _UNICODE
};

// ****************************************************************************

class CWindow : public CWindowsObject
{
public:
#ifdef _UNICODE
    CWindow(CObjectOrigin origin = ooAllocated) : CWindowsObject(origin)
#else  // _UNICODE
    CWindow(CObjectOrigin origin = ooAllocated,
            BOOL unicodeWnd = FALSE) : CWindowsObject(origin, unicodeWnd)
#endif // _UNICODE
    {
        DefWndProc = GetDefWindowProc();
    }

#ifdef _UNICODE
    CWindow(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated) : CWindowsObject(origin)
#else  // _UNICODE
    CWindow(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated,
            BOOL unicodeWnd = FALSE) : CWindowsObject(origin, unicodeWnd)
#endif // _UNICODE
    {
        DefWndProc = GetDefWindowProc();
        AttachToControl(hDlg, ctrlID);
    }

#ifdef _UNICODE
    CWindow(HWND hDlg, int ctrlID, UINT helpID,
            CObjectOrigin origin = ooAllocated) : CWindowsObject(helpID, origin)
#else  // _UNICODE
    CWindow(HWND hDlg, int ctrlID, UINT helpID, CObjectOrigin origin = ooAllocated,
            BOOL unicodeWnd = FALSE) : CWindowsObject(helpID, origin, unicodeWnd)
#endif // _UNICODE
    {
        DefWndProc = GetDefWindowProc();
        AttachToControl(hDlg, ctrlID);
    }

    virtual BOOL Is(int type) { return type == otWindow; }
    virtual int GetObjectType() { return otWindow; }

    static BOOL RegisterUniversalClass();
    static BOOL RegisterUniversalClass(UINT style,
                                       int cbClsExtra,
                                       int cbWndExtra,
                                       HICON hIcon,
                                       HCURSOR hCursor,
                                       HBRUSH hbrBackground,
                                       LPCTSTR lpszMenuName,
                                       LPCTSTR lpszClassName,
                                       HICON hIconSm);

#ifndef _UNICODE
    static BOOL RegisterUniversalClassW(UINT style,
                                        int cbClsExtra,
                                        int cbWndExtra,
                                        HICON hIcon,
                                        HCURSOR hCursor,
                                        HBRUSH hbrBackground,
                                        LPCWSTR lpszMenuName,
                                        LPCWSTR lpszClassName,
                                        HICON hIconSm);
#endif // _UNICODE

    HWND Create(LPCTSTR lpszClassName,  // address of registered class name
                LPCTSTR lpszWindowName, // address of window name
                DWORD dwStyle,          // window style
                int x,                  // horizontal position of window
                int y,                  // vertical position of window
                int nWidth,             // window width
                int nHeight,            // window height
                HWND hwndParent,        // handle of parent or owner window
                HMENU hmenu,            // handle of menu or child-window identifier
                HINSTANCE hinst,        // handle of application instance
                LPVOID lpvParam);       // pointer to the window object being created

    HWND CreateEx(DWORD dwExStyle,        // extended window style
                  LPCTSTR lpszClassName,  // address of registered class name
                  LPCTSTR lpszWindowName, // address of window name
                  DWORD dwStyle,          // window style
                  int x,                  // horizontal position of window
                  int y,                  // vertical position of window
                  int nWidth,             // window width
                  int nHeight,            // window height
                  HWND hwndParent,        // handle of parent or owner window
                  HMENU hmenu,            // handle of menu or child-window identifier
                  HINSTANCE hinst,        // handle of application instance
                  LPVOID lpvParam);       // pointer to the window object being created

#ifndef _UNICODE
    HWND CreateW(LPCWSTR lpszClassName,  // address of registered class name
                 LPCWSTR lpszWindowName, // address of window name
                 DWORD dwStyle,          // window style
                 int x,                  // horizontal position of window
                 int y,                  // vertical position of window
                 int nWidth,             // window width
                 int nHeight,            // window height
                 HWND hwndParent,        // handle of parent or owner window
                 HMENU hmenu,            // handle of menu or child-window identifier
                 HINSTANCE hinst,        // handle of application instance
                 LPVOID lpvParam);       // pointer to the window object being created

    HWND CreateExW(DWORD dwExStyle,        // extended window style
                   LPCWSTR lpszClassName,  // address of registered class name
                   LPCWSTR lpszWindowName, // address of window name
                   DWORD dwStyle,          // window style
                   int x,                  // horizontal position of window
                   int y,                  // vertical position of window
                   int nWidth,             // window width
                   int nHeight,            // window height
                   HWND hwndParent,        // handle of parent or owner window
                   HMENU hmenu,            // handle of menu or child-window identifier
                   HINSTANCE hinst,        // handle of application instance
                   LPVOID lpvParam);       // pointer to the window object being created
#endif                                     // _UNICODE

    void AttachToWindow(HWND hWnd);
    void AttachToControl(HWND dlg, int ctrlID);
    void DetachWindow();

    static LRESULT CALLBACK CWindowProc(HWND hwnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);
#ifndef _UNICODE
    static LRESULT CALLBACK CWindowProcW(HWND hwnd, UINT uMsg,
                                         WPARAM wParam, LPARAM lParam);
#endif // _UNICODE

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

#ifndef _UNICODE
    static LRESULT CALLBACK CWindowProcInt(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL unicode);
#endif // _UNICODE

    WNDPROC GetDefWindowProc()
    {
#ifndef _UNICODE
        if (UnicodeWnd)
            return DefWindowProcW;
#endif // _UNICODE
        return DefWindowProc;
    }

    WNDPROC DefWndProc;
};

// ****************************************************************************

enum CTransferType
{
    ttDataToWindow,  // data to the window
    ttDataFromWindow // data from the window
};

// ****************************************************************************

class CTransferInfo
{
public:
    int FailCtrlID; // INT_MAX = no error, otherwise the ID of the failing control
    CTransferType Type;

    CTransferInfo(HWND hDialog, CTransferType type)
    {
        HDialog = hDialog;
        FailCtrlID = INT_MAX;
        Type = type;
    }

    BOOL IsGood() { return FailCtrlID == INT_MAX; }
    void ErrorOn(int ctrlID) { FailCtrlID = ctrlID; }
    BOOL GetControl(HWND& ctrlHWnd, int ctrlID, BOOL ignoreIsGood = FALSE);
    void EnsureControlIsFocused(int ctrlID);

    void EditLine(int ctrlID, TCHAR* buffer, DWORD bufferSizeInChars, BOOL select = TRUE);
    void EditLine(int ctrlID, double& value, TCHAR* format, BOOL select = TRUE); // format, e.g. _T("%.2lf")
    void EditLine(int ctrlID, int& value, BOOL select = TRUE);
    void EditLine(int ctrlID, __int64& value, BOOL select = TRUE, BOOL unsignedNum = FALSE /* signed number */,
                  BOOL hexMode = FALSE /* decimal mode */, BOOL ignoreOverflow = FALSE, BOOL quiet = FALSE);
    void RadioButton(int ctrlID, int ctrlValue, int& value);
    void CheckBox(int ctrlID, int& value); // 0-unchecked, 1-checked, 2-grayed
    void TrackBar(int ctrlID, int& value);

#ifndef _UNICODE
    void EditLineW(int ctrlID, WCHAR* buffer, DWORD bufferSizeInChars, BOOL select = TRUE);
#endif // _UNICODE

protected:
    HWND HDialog; // handle of the dialog for which the transfer is performed
};

// ****************************************************************************

class CDialog : public CWindowsObject
{
public:
    CWindowsObject::SetObjectOrigin; // needed so CPropSheetPage compiles
    CWindowsObject::HWindow;         // needed so CPropSheetPage compiles

#ifdef _UNICODE
    CDialog(HINSTANCE modul, int resID, HWND parent, CObjectOrigin origin = ooStandard) : CWindowsObject(origin)
#else  // _UNICODE
    CDialog(HINSTANCE modul, int resID, HWND parent, CObjectOrigin origin = ooStandard,
            BOOL unicodeWnd = FALSE) : CWindowsObject(origin, unicodeWnd)
#endif // _UNICODE
    {
        Modal = 0;
        Modul = modul;
        ResID = resID;
        Parent = parent;
    }

#ifdef _UNICODE
    CDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent,
            CObjectOrigin origin = ooStandard) : CWindowsObject(helpID, origin)
#else  // _UNICODE
    CDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent, CObjectOrigin origin = ooStandard,
            BOOL unicodeWnd = FALSE) : CWindowsObject(helpID, origin, unicodeWnd)
#endif // _UNICODE
    {
        Modal = 0;
        Modul = modul;
        ResID = resID;
        Parent = parent;
    }

    virtual BOOL ValidateData();
    virtual void Validate(CTransferInfo& /*ti*/) {}
    virtual BOOL TransferData(CTransferType type);
    virtual void Transfer(CTransferInfo& /*ti*/) {}

    virtual BOOL Is(int type) { return type == otDialog; }
    virtual int GetObjectType() { return otDialog; }

    virtual BOOL IsAllocated() { return ObjectOrigin == ooAllocated ||
                                        (!Modal && ObjectOrigin == ooStandard); }

    void SetParent(HWND parent) { Parent = parent; }
    HWND GetParent() { return Parent; }
    INT_PTR Execute(); // modal dialog
    HWND Create();     // modeless dialog

    static INT_PTR CALLBACK CDialogProc(HWND hwndDlg, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated() {}

    BOOL Modal; // because of the way the dialog is destroyed
    HINSTANCE Modul;
    int ResID;
    HWND Parent;
};

// ****************************************************************************
// ****************************************************************************

struct CWindowData
{
    // if window objects (Wnd) are allocated on the stack (typically modal dialogs, e.g. SalMessageBox())
    // and the thread terminates, the stack becomes invalid and the window objects are no longer accessible,
    // so we only access the objects (Wnd) while the window handles (HWnd) are still valid
    HWND HWnd;
    CWindowsObject* Wnd;
};

#define WNDMGR_CACHE_SIZE 256 // (2kB cache) must be consistent with GetCacheIndex

inline int GetCacheIndex(HWND hWnd)
{
    return ((int)(INT_PTR)hWnd) & 0xff;
}

struct CWinLibCS
{
    CRITICAL_SECTION cs;

    CWinLibCS() { HANDLES(InitializeCriticalSection(&cs)); }
    ~CWinLibCS() { HANDLES(DeleteCriticalSection(&cs)); }

    void Enter() { HANDLES(EnterCriticalSection(&cs)); }
    void Leave() { HANDLES(LeaveCriticalSection(&cs)); }
};

class CWindowsManager : protected TDirectArray<CWindowData>
{
public:
#ifdef __DEBUG_WINLIB
    int search, cache, maxWndCount;
#endif

    CWinLibCS CS; // public so Windows Manager changes can be prevented locally

public:
    CWindowsManager();

    BOOL AddWindow(HWND hWnd, CWindowsObject* wnd);
    void DetachWindow(HWND hWnd);
    CWindowsObject* GetWindowPtr(HWND hWnd);
    int GetCount();

private:
    HWND LastHWnd[WNDMGR_CACHE_SIZE]; // last-request cache
    CWindowsObject* LastWnd[WNDMGR_CACHE_SIZE];

    inline BOOL GetIndex(HWND hWnd, int& index);

    friend void ReleaseWinLib();
};

// ****************************************************************************

BOOL CWindowsManager::GetIndex(HWND hWnd, int& index)
{
    CS.Enter();
    if (Count == 0)
    {
        index = 0;
        CS.Leave();
        return FALSE;
    }

    int l = 0, r = Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        HWND hw = At(m).HWnd;
        if (hw == hWnd) // found
        {
            index = m;
            CS.Leave();
            return TRUE;
        }
        else if (hw > hWnd)
        {
            if (l == r || l > m - 1) // not found
            {
                index = m; // should be at this position
                CS.Leave();
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // not found
            {
                index = m + 1; // should be after this position
                CS.Leave();
                return FALSE;
            }
            l = m + 1;
        }
    }
}

// ****************************************************************************

struct CWindowQueueItem
{
    HWND HWindow;
    CWindowQueueItem* Next;

    CWindowQueueItem(HWND hWindow)
    {
        HWindow = hWindow;
        Next = NULL;
    }
};

class CWindowQueue
{
protected:
    const TCHAR* QueueName; // queue name (for debugging only)
    CWindowQueueItem* Head;
    int Count;
    CWinLibCS CS; // access from multiple threads -> synchronization required

public:
    CWindowQueue(const TCHAR* queueName /* e.g. "Find Dialogs" */)
    {
        QueueName = queueName;
        Head = NULL;
        Count = 0;
    }
    ~CWindowQueue();

    BOOL Add(CWindowQueueItem* item); // adds an item to the queue, returns success status
    void Remove(HWND hWindow);        // removes an item from the queue
    BOOL Empty();                     // returns TRUE if the queue is empty
    int GetWindowCount();             // returns the number of windows in the queue

    // posts the message to all windows (PostMessage - windows may be in different threads)
    void BroadcastMessage(DWORD uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

extern CWindowsManager WindowsManager;
