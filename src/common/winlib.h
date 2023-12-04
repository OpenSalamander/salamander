// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// makro __DEBUG_WINLIB zapina nekolik testu zaludnych chyb WinLibu

// konstanty pro stringy WinLibu (jen interni pouziti ve WinLibu)
enum CWLS
{
    WLS_INVALID_NUMBER,
    WLS_ERROR,

    WLS_COUNT
};

// nastaveni vlastnich textu do WinLibu
void SetWinLibStrings(const TCHAR* invalidNumber, // "neni cislo" (u transferbufferu cisel)
                      const TCHAR* error);        // titulek "chyba" (u transferbufferu cisel)

extern HINSTANCE HInstance;
extern const TCHAR* CWINDOW_CLASSNAME;  // jmeno tridy universalniho okna
extern const TCHAR* CWINDOW_CLASSNAME2; // jmeno tridy universalniho okna - nema CS_VREDRAW | CS_HREDRAW

#ifndef _UNICODE
extern const WCHAR* CWINDOW_CLASSNAMEW;  // jmeno tridy unicodoveho universalniho okna
extern const WCHAR* CWINDOW_CLASSNAME2W; // jmeno tridy unicodoveho universalniho okna - nema CS_VREDRAW | CS_HREDRAW
#endif                                   // _UNICODE

class CWinLibHelp;

// je potreba zavolat pred pouzitim WinLibu
BOOL InitializeWinLib();
// je potreba zavolat po pouziti WinLibu
void ReleaseWinLib();
// je treba zavolat pred pouzivanim helpu
BOOL SetupWinLibHelp(CWinLibHelp* winLibHelp);

class CWinLibHelp
{
public:
    virtual void OnHelp(HWND /*hWindow*/, UINT /*helpID*/, HELPINFO* /*helpInfo*/,
                        BOOL /*ctrlPressed*/, BOOL /*shiftPressed*/) {}
    virtual void OnContextMenu(HWND /*hWindow*/, WORD /*xPos*/, WORD /*yPos*/) {}
};

// ****************************************************************************

enum CObjectOrigin // pouzito pri destrukci oken a dialogu
{
    ooAllocated, // pri WM_DESTROY se bude dealokovat
    ooStatic,    // pri WM_DESTROY se HWindow nastavi na NULL
    ooStandard   // pro modalni dlg =ooStatic, pro nemodalni dlg =ooAllocated
};

// ****************************************************************************

enum CObjectType // pro rozpoznani typu objektu
{
    otBase,
    otWindow,
    otDialog,
    otPropSheetPage,
    otLastWinLibObject
};

// ****************************************************************************

class CWindowsObject // predek vsech MS-Windows objektu
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

    virtual ~CWindowsObject() {} // aby se u potomku volal jejich destruktor

    virtual BOOL Is(int) { return FALSE; } // identifikace objektu
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
    // okna: create: TRUE = okno je unicodove, jinak je ANSI; attach: TRUE = nase window procedura
    // je unicodova, jinak je ANSI; dialogy: TRUE = dialog je unicodovy, jinak je ANSI
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
                LPVOID lpvParam);       // ukazatel na objekt vytvareneho okna

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
                  LPVOID lpvParam);       // ukazatel na objekt vytvareneho okna

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
                 LPVOID lpvParam);       // ukazatel na objekt vytvareneho okna

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
                   LPVOID lpvParam);       // ukazatel na objekt vytvareneho okna
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
    ttDataToWindow,  // data jdou do okna
    ttDataFromWindow // data jdou z okna
};

// ****************************************************************************

class CTransferInfo
{
public:
    int FailCtrlID; // INT_MAX - vse v poradku, jinak ID controlu s chybou
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
    void EditLine(int ctrlID, double& value, TCHAR* format, BOOL select = TRUE); // format napr. _T("%.2lf")
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
    HWND HDialog; // handle dialogu, pro ktery se provadi transfer
};

// ****************************************************************************

class CDialog : public CWindowsObject
{
public:
    CWindowsObject::SetObjectOrigin; // kvuli zkompilovatelnosti CPropSheetPage
    CWindowsObject::HWindow;         // kvuli zkompilovatelnosti CPropSheetPage

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
    INT_PTR Execute(); // modalni dialog
    HWND Create();     // nemodalni dialog

    static INT_PTR CALLBACK CDialogProc(HWND hwndDlg, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated() {}

    BOOL Modal; // kvuli zpusobu destrukce dialogu
    HINSTANCE Modul;
    int ResID;
    HWND Parent;
};

// ****************************************************************************
// ****************************************************************************

struct CWindowData
{
    // pokud jsou objekty oken (Wnd) umistene na stacku (typicky modalni dialogy, napr. SalMessageBox())
    // a dojde k terminovani threadu, stack se zneplatni a tim jiz objekty oken nejsou pristupne,
    // resime tak, ze na objekty (Wnd) sahame jen dokud jsou platne handly oken (HWnd)
    HWND HWnd;
    CWindowsObject* Wnd;
};

#define WNDMGR_CACHE_SIZE 256 // (2kB cache) musi byt v souladu s GetCacheIndex

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

    CWinLibCS CS; // je public, aby slo lokalne zamezit zmenam ve Windows Manageru

public:
    CWindowsManager();

    BOOL AddWindow(HWND hWnd, CWindowsObject* wnd);
    void DetachWindow(HWND hWnd);
    CWindowsObject* GetWindowPtr(HWND hWnd);
    int GetCount();

private:
    HWND LastHWnd[WNDMGR_CACHE_SIZE]; // posledni pozadavek - cache
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
        if (hw == hWnd) // nalezeno
        {
            index = m;
            CS.Leave();
            return TRUE;
        }
        else if (hw > hWnd)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                index = m; // mel by byt na teto pozici
                CS.Leave();
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                index = m + 1; // mel by byt az za touto pozici
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
    const TCHAR* QueueName; // jmeno fronty (jen pro debugovaci ucely)
    CWindowQueueItem* Head;
    int Count;
    CWinLibCS CS; // pristup z vice threadu -> nutna synchronizace

public:
    CWindowQueue(const TCHAR* queueName /* napr. "Find Dialogs" */)
    {
        QueueName = queueName;
        Head = NULL;
        Count = 0;
    }
    ~CWindowQueue();

    BOOL Add(CWindowQueueItem* item); // prida polozku do fronty, vraci uspech
    void Remove(HWND hWindow);        // odstrani polozku z fronty
    BOOL Empty();                     // vraci TRUE pokud je fronta prazdna
    int GetWindowCount();             // vraci pocet oken ve fronte

    // posle (PostMessage - okna muzou byt v ruznych threadech) vsem oknum zpravu
    void BroadcastMessage(DWORD uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************

extern CWindowsManager WindowsManager;
