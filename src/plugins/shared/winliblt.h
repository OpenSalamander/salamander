// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

// "light" verze WinLibu

#pragma once

// makra pro potlaceni nepotrebnych casti WinLibLT (snazsi kompilace):
// ENABLE_PROPERTYDIALOG - je-li definovano, je mozne pouzivat property sheet dialog (CPropertyDialog)

// nastaveni vlastnich textu do WinLibu
void SetWinLibStrings(const char* invalidNumber, // "neni cislo" (u transferbufferu cisel)
                      const char* error);        // titulek "chyba" (u transferbufferu cisel)

// je potreba zavolat pred pouzitim WinLibu; 'pluginName' je jmeno pluginu (napr. "DEMOPLUG"),
// pouziva se pro odliseni jmen trid univerzalnich oken WinLibu (mezi pluginy se musi lisit,
// jinak nastane kolize jmen trid a WinLib nemuze fungovat - bude fungovat jen prvni spusteny
// plugin); 'dllInstance' je modul pluginu (pouziva se pri registraci univerzalnich trid WinLibu)
BOOL InitializeWinLib(const char* pluginName, HINSTANCE dllInstance);
// je potreba zavolat po pouziti WinLibu; 'dllInstance' je modul pluginu (pouziva se pri zruseni
// registrace univerzalnich trid WinLibu)
void ReleaseWinLib(HINSTANCE dllInstance);

// typ callbacku pro pripojeni na HTML help
typedef void(WINAPI* FWinLibLTHelpCallback)(HWND hWindow, UINT helpID);

// nastaveni callbacku pro pripojeni na HTML help
void SetupWinLibHelp(FWinLibLTHelpCallback helpCallback);

// konstanty pro stringy WinLibu (jen interni pouziti ve WinLibu)
enum CWLS
{
    WLS_INVALID_NUMBER,
    WLS_ERROR,

    WLS_COUNT
};

extern char CWINDOW_CLASSNAME[100];  // jmeno tridy universalniho okna
extern char CWINDOW_CLASSNAME2[100]; // jmeno tridy universalniho okna - nema CS_VREDRAW | CS_HREDRAW

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
#ifdef ENABLE_PROPERTYDIALOG
    otPropSheetPage,
#endif // ENABLE_PROPERTYDIALOG
    otLastWinLibObject
};

// ****************************************************************************

class CWindowsObject // predek vsech MS-Windows objektu
{
public:
    HWND HWindow;
    UINT HelpID; // -1 = empty value (do not use help)

    CWindowsObject(CObjectOrigin origin)
    {
        HWindow = NULL;
        ObjectOrigin = origin;
        HelpID = -1;
    }
    CWindowsObject(UINT helpID, CObjectOrigin origin)
    {
        HWindow = NULL;
        ObjectOrigin = origin;
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
            TRACE_E("CWindowsObject::SetHelpID(): helpID==-1, -1 is 'empty value', you should use another helpID! If you want to set HelpID to -1, use ClearHelpID().");
        HelpID = helpID;
    }
    void ClearHelpID() { HelpID = -1; }

protected:
    CObjectOrigin ObjectOrigin;
};

// ****************************************************************************

class CWindow : public CWindowsObject
{
public:
    CWindow(CObjectOrigin origin = ooAllocated) : CWindowsObject(origin) { DefWndProc = DefWindowProc; }
    CWindow(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated)
        : CWindowsObject(origin)
    {
        DefWndProc = DefWindowProc;
        AttachToControl(hDlg, ctrlID);
    }
    CWindow(HWND hDlg, int ctrlID, UINT helpID, CObjectOrigin origin = ooAllocated)
        : CWindowsObject(helpID, origin)
    {
        DefWndProc = DefWindowProc;
        AttachToControl(hDlg, ctrlID);
    }

    virtual BOOL Is(int type) { return type == otWindow; }
    virtual int GetObjectType() { return otWindow; }

    // registruje univerzalni tridy WinLibu, vola se automaticky (registraci rusi tez automaticky)
    static BOOL RegisterUniversalClass(HINSTANCE dllInstance);

    // registrace vlastni univerzalni tridy; POZOR: pri unloadu pluginu je nutne zrusit registraci tridy,
    // jinak pri opakovanem loadu pluginu dojde k chybe pri registraci (konflikt se starou tridou)
    static BOOL RegisterUniversalClass(UINT style,
                                       int cbClsExtra,
                                       int cbWndExtra,
                                       HINSTANCE dllInstance,
                                       HICON hIcon,
                                       HCURSOR hCursor,
                                       HBRUSH hbrBackground,
                                       LPCTSTR lpszMenuName,
                                       LPCTSTR lpszClassName,
                                       HICON hIconSm);

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

    void AttachToWindow(HWND hWnd);
    void AttachToControl(HWND dlg, int ctrlID);
    void DetachWindow();

    static LRESULT CALLBACK CWindowProc(HWND hwnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

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

    void EditLine(int ctrlID, char* buffer, DWORD bufferSize, BOOL select = TRUE);
    void RadioButton(int ctrlID, int ctrlValue, int& value);
    void CheckBox(int ctrlID, int& value); // 0-unchecked, 1-checked, 2-grayed

    // kontroluje double hodnotu (pokud to neni cislo, neprojde), oddelovac muze byt '.' i ',';
    // 'format' se pouziva v sprintf pri prevodu cisla na retezec (napr. "%.2f" nebo "%g")
    void EditLine(int ctrlID, double& value, char* format, BOOL select = TRUE);

    // kontroluje int hodnotu (pokud to neni cislo, neprojde)
    void EditLine(int ctrlID, int& value, BOOL select = TRUE);

protected:
    HWND HDialog; // handle dialogu, pro ktery se provadi transfer
};

// ****************************************************************************

class CDialog : public CWindowsObject
{
public:
#ifdef ENABLE_PROPERTYDIALOG
    CWindowsObject::HWindow;         // kvuli zkompilovatelnosti CPropSheetPage
    CWindowsObject::SetObjectOrigin; // kvuli zkompilovatelnosti CPropSheetPage
#endif                               // ENABLE_PROPERTYDIALOG

    CDialog(HINSTANCE modul, int resID, HWND parent,
            CObjectOrigin origin = ooStandard) : CWindowsObject(origin)
    {
        Modal = 0;
        Modul = modul;
        ResID = resID;
        Parent = parent;
    }
    CDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent,
            CObjectOrigin origin = ooStandard) : CWindowsObject(helpID, origin)
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

#ifdef ENABLE_PROPERTYDIALOG

class CPropertyDialog;

class CPropSheetPage : protected CDialog
{
public:
    CDialog::HWindow; // HWindow zustane pristupne

    CDialog::SetObjectOrigin; // zpristupneni povolenych metod predku
    CDialog::Transfer;

    // testovano s resourcem dialogu stranky se stylem:
    // DS_CONTROL | DS_3DLOOK | WS_CHILD | WS_CAPTION;
    // pokud chceme pouzit primo titulek z resourcu, staci dat 'title'==NULL a
    // 'flags'==0
    CPropSheetPage(char* title, HINSTANCE modul, int resID,
                   DWORD flags /* = PSP_USETITLE*/, HICON icon,
                   CObjectOrigin origin = ooStatic);
    CPropSheetPage(char* title, HINSTANCE modul, int resID, int helpID,
                   DWORD flags /* = PSP_USETITLE*/, HICON icon,
                   CObjectOrigin origin = ooStatic);
    ~CPropSheetPage();

    void Init(char* title, HINSTANCE modul, int resID,
              HICON icon, DWORD flags, CObjectOrigin origin);

    virtual BOOL ValidateData();
    virtual BOOL TransferData(CTransferType type);

    HPROPSHEETPAGE CreatePropSheetPage();
    virtual BOOL Is(int type) { return type == otPropSheetPage || CDialog::Is(type); }
    virtual int GetObjectType() { return otPropSheetPage; }
    virtual BOOL IsAllocated() { return ObjectOrigin == ooAllocated; }

    static INT_PTR CALLBACK CPropSheetPageProc(HWND hwndDlg, UINT uMsg,
                                               WPARAM wParam, LPARAM lParam);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    char* Title;
    DWORD Flags;
    HICON Icon;

    CPropertyDialog* ParentDialog; // vlastnik teto stranky

    friend class CPropertyDialog;
};

// ****************************************************************************

class CPropertyDialog : public TIndirectArray<CPropSheetPage>
{
public:
    // do tohoto objektu je idealni pridat objekty jednotlivych stranek
    // a pak je jako "staticke" (defaultni volba) napridavat pres metodu Add;
    // 'startPage' a 'lastPage' muze byt jen jedina promenna (hodnota in/odkaz out);
    // 'flags' viz help k 'PROPSHEETHEADER', pouzitelne hlavne konstanty
    // PSH_NOAPPLYNOW, PSH_USECALLBACK a PSH_HASHELP (jinak staci 'flags'==0)
    CPropertyDialog(HWND parent, HINSTANCE modul, char* caption,
                    int startPage, DWORD flags, HICON icon = NULL,
                    DWORD* lastPage = NULL, PFNPROPSHEETCALLBACK callback = NULL)
        : TIndirectArray<CPropSheetPage>(10, 5, dtNoDelete)
    {
        Parent = parent;
        HWindow = NULL;
        Modul = modul;
        Icon = icon;
        Caption = caption;
        StartPage = startPage;
        Flags = flags;
        LastPage = lastPage;
        Callback = callback;
    }

    virtual INT_PTR Execute();

    virtual int GetCurSel();

protected:
    HWND Parent; // parametry pro vytvareni dialogu
    HWND HWindow;
    HINSTANCE Modul;
    HICON Icon;
    char* Caption;
    int StartPage;
    DWORD Flags;
    PFNPROPSHEETCALLBACK Callback;

    DWORD* LastPage; // posledni zvolena stranka (muze byt NULL, pokud nezajima)

    friend class CPropSheetPage;
};

#endif // ENABLE_PROPERTYDIALOG

// ****************************************************************************

class CWindowsManager
{
public:
    int WindowsCount; // pocet oken obsluhovanych WinLibem (aktualni stav)

public:
    CWindowsManager() { WindowsCount = 0; }

    BOOL AddWindow(HWND hWnd, CWindowsObject* wnd);
    void DetachWindow(HWND hWnd);
    CWindowsObject* GetWindowPtr(HWND hWnd);
};

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
    const char* QueueName; // jmeno fronty (jen pro debugovaci ucely)
    CWindowQueueItem* Head;

    struct CCS // pristup z vice threadu -> nutna synchronizace
    {
        CRITICAL_SECTION cs;

        CCS() { InitializeCriticalSection(&cs); }
        ~CCS() { DeleteCriticalSection(&cs); }

        void Enter() { EnterCriticalSection(&cs); }
        void Leave() { LeaveCriticalSection(&cs); }
    } CS;

public:
    CWindowQueue(const char* queueName /* napr. "DemoPlug Viewers" */)
    {
        QueueName = queueName;
        Head = NULL;
    }
    ~CWindowQueue();

    BOOL Add(CWindowQueueItem* item); // prida polozku do fronty, vraci uspech
    void Remove(HWND hWindow);        // odstrani polozku z fronty
    BOOL Empty();                     // vraci TRUE pokud je fronta prazdna

    // posle (PostMessage - okna muzou byt v ruznych threadech) vsem oknum zpravu
    void BroadcastMessage(DWORD uMsg, WPARAM wParam, LPARAM lParam);

    // broadcastne WM_CLOSE, pak ceka na prazdnou frontu (max. cas dle 'force' bud 'forceWaitTime'
    // nebo 'waitTime'); vraci TRUE pokud je fronta prazdna (vsechna okna se zavrela)
    // nebo je 'force' TRUE; cas INFINITE = neomezene dlouhe cekani
    // Poznamka: pri 'force' TRUE vraci vzdy TRUE, nema smysl na nic cekat, proto forceWaitTime = 0
    BOOL CloseAllWindows(BOOL force, int waitTime = 1000, int forceWaitTime = 0);
};

// ****************************************************************************

extern CWindowsManager WindowsManager;
