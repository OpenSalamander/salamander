// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// __DEBUG_WINLIB macro enables some tests of tricky WinLib errors

// constants for WinLib strings (internal use in WinLib only)
// konstanty pro stringy WinLibu (jen interni pouziti ve WinLibu)
enum CWLS
{
    WLS_INVALID_NUMBER,
    WLS_ERROR,

    WLS_COUNT
};

// setting custom strings for WinLib
void SetWinLibStrings(const TCHAR* invalidNumber, // "not a number" (in transferbuffer of numbers)
                      const TCHAR* error);        // "error" title (in transferbuffer of numbers)

extern HINSTANCE HInstance;
extern const TCHAR* CWINDOW_CLASSNAME;  // universal window class name
extern const TCHAR* CWINDOW_CLASSNAME2; // universal window class name - does not have CS_VREDRAW | CS_HREDRAW

#ifndef _UNICODE
extern const WCHAR* CWINDOW_CLASSNAMEW;  // unicode universal window class name
extern const WCHAR* CWINDOW_CLASSNAME2W; // unicode universal window class name - does not have CS_VREDRAW | CS_HREDRAW
#endif                                   // _UNICODE

class CWinLibHelp;

// needed to call before using WinLib
BOOL InitializeWinLib();
// needed to call after using WinLib
void ReleaseWinLib();
// needed to call before using help
BOOL SetupWinLibHelp(CWinLibHelp* winLibHelp);

class CWinLibHelp
{
public:
    virtual void OnHelp(HWND /*hWindow*/, UINT /*helpID*/, HELPINFO* /*helpInfo*/,
                        BOOL /*ctrlPressed*/, BOOL /*shiftPressed*/) {}
    virtual void OnContextMenu(HWND /*hWindow*/, WORD /*xPos*/, WORD /*yPos*/) {}
};

// ****************************************************************************

enum CObjectOrigin // used to destroy windows and dialogs
{
    ooAllocated, // will be deallocated in WM_DESTROY
    ooStatic,    // HWindows will be set to NULL in WM_DESTROY
    ooStandard   // for modal dlg ==ooStatic, for non-modal dlg ==ooAllocated
};

// ****************************************************************************

enum CObjectType // to recognize type of object
{
    otBase,
    otWindow,
    otDialog,
    otPropSheetPage,
    otLastWinLibObject
};

// ****************************************************************************

class CWindowsObject // parent of all MS-Windows objects
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

    virtual ~CWindowsObject() {} // so that destructor is called for derived objects

    virtual BOOL Is(int) { return FALSE; } // object identification
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
    // windows: create: TRUE = window is unicode, otherwise ANSI; attach: TRUE = our window procedure
    // is unicode, otherwise ANSI; dialogs: TRUE = dialog is unicode, otherwise ANSI
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
                LPVOID lpvParam);       // pointer to object of the created window

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
                  LPVOID lpvParam);       // pointer to object of the created window

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
                 LPVOID lpvParam);       // pointer to object of the created window

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
                   LPVOID lpvParam);       // pointer to object of the created window
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
    ttDataToWindow,  // data come to window
    ttDataFromWindow // data come from window
};

// ****************************************************************************

class CTransferInfo
{
public:
    int FailCtrlID; // INT_MAX - everything is OK, otherwise ID of control with error
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
    void EditLine(int ctrlID, double& value, TCHAR* format, BOOL select = TRUE); // format e.g. _T("%.2lf")
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
    HWND HDialog; // handle of dialog for which the transfer is performed
};

// ****************************************************************************

class CDialog : public CWindowsObject
{
public:
    CWindowsObject::SetObjectOrigin; // for compilability CPropSheetPage
    CWindowsObject::HWindow;         // for compilability CPropSheetPage

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
    HWND Create();     // non-modal dialog

    static INT_PTR CALLBACK CDialogProc(HWND hwndDlg, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated() {}

    BOOL Modal; // due to the way of dialog destruction
    HINSTANCE Modul;
    int ResID;
    HWND Parent;
};

// ****************************************************************************
// ****************************************************************************

struct CWindowData
{
    // if all window objects (Wnd) are placed on stack (typically modal dialogs, e.g. SalMessageBox())
    // and thread is terminated, stack is invalidated and window objects are not accessible anymore,
    // we solve this by accessing objects (Wnd) only while window handles (HWnd) are valid
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

    CWinLibCS CS; // is public so that it can be used locally to prevent changes in Windows Manager

public:
    CWindowsManager();

    BOOL AddWindow(HWND hWnd, CWindowsObject* wnd);
    void DetachWindow(HWND hWnd);
    CWindowsObject* GetWindowPtr(HWND hWnd);
    int GetCount();

private:
    HWND LastHWnd[WNDMGR_CACHE_SIZE]; // the last request - cache
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
    const TCHAR* QueueName; // name of queue (for debug purposes only)
    CWindowQueueItem* Head;
    int Count;
    CWinLibCS CS; // access from more threads -> synchronization needed

public:
    CWindowQueue(const TCHAR* queueName /* e.g. "Find Dialogs" */)
    {
        QueueName = queueName;
        Head = NULL;
        Count = 0;
    }
    ~CWindowQueue();

    BOOL Add(CWindowQueueItem* item); // adds item into queue, returns success
    void Remove(HWND hWindow);        // removes item from queue
    BOOL Empty();                     // returns TRUE if queue is empty
    int GetWindowCount();             // returns number of windows in queue

    // sends (PostMessage - windows can be in different threads) message to all windows
    // posle (PostMessage - okna muzou byt v ruznych threadech) vsem oknum zpravu
    void BroadcastMessage(DWORD uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

extern CWindowsManager WindowsManager;
