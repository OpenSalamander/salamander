// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WM_APP_ACTIVATEPARENT WM_APP + 1 // [0, 0] - aktivace parenta (pouzivaji vsechna wait-window)
#define WM_APP_STATUSCHANGED WM_APP + 2  // [0, 0] - zmena stavu "data connection" (pouziva list-wait-window)

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#define WAITWND_SHOWTIMER 1              // ID timeru pro zobrazeni wait-okna
#define LISTWAITWND_AUTOUPDATETIMER 2    // ID timeru pro automaticky update list-wait-okna (po 1 sekunde)
#define LISTWAITWND_DELAYEDUPDATETIMER 3 // ID timeru pro zpozdeny update list-wait-okna (po 1/10 sekundy)
#define LOGSDLG_DELAYEDUPDATETIMER 4     // ID timeru pro zpozdeny update logs-okna (po 1/10 sekundy)

extern TIndirectArray<CDialog> ModelessDlgs; // pole nemodalnich dialogu (Welcome Message)

extern ATOM AtomObject2; // atom pro CSetWaitCursorWindow

// pole retezcu popisujicich defaultni chovani pri chybach behem operaci
extern int OperDefFileAlreadyExists[];
extern int OperDefDirAlreadyExists[];
extern int OperDefCannotCreateFileOrDir[];
extern int OperDefRetryOnCreatedFile[];
extern int OperDefRetryOnResumedFile[];
extern int OperDefAsciiTrModeForBinFile[];
extern int OperDefUnknownAttrs[];
extern int OperDefDeleteArr[];

// podpora pro comboboxy s historii
void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* text,
                     int textLen, int historySize, char* history[], BOOL secretValue);

// podpora pro plneni comboboxu s defaultnim chovanim pri chybach behem operaci
void HandleOperationsCombo(int* value, CTransferInfo& ti, int resID, int arrValuesResID[]);

// podpora pro comboboxy s Proxy servery
void ProxyComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, int& proxyUID, BOOL addDefault,
                   CFTPProxyServerList* proxyServerList);

// 'lastCheck' (in/out) obsahuje posledni hodnotu stavu checkboxu, 'lastCheck' se inicializuje na -1;
// 'valueBuf' je buffer pro hodnotu ve stavu "checked" o velikosti aspon 31 znaku,
// 'valueBuf' se inicializuje na prazdny retezec;
// 'checkedVal' je pocatecni hodnota ve stavu "checked";
// 'globValUsed'+'globVal' - hodnota pro treti stav checkboxu (pouziva se? + hodnota)
void CheckboxEditLineInteger(HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                             int checkedVal, BOOL globValUsed, int globVal);

// 'lastCheck' (in/out) obsahuje posledni hodnotu stavu checkboxu, 'lastCheck' se inicializuje na -1;
// 'valueBuf' je buffer pro hodnotu ve stavu "checked" o velikosti aspon 31 znaku,
// 'valueBuf' se inicializuje na prazdny retezec;
// 'checkedVal' je pocatecni hodnota ve stavu "checked";
// 'globValUsed'+'globVal' - hodnota pro treti stav checkboxu (pouziva se? + hodnota)
void CheckboxEditLineDouble(HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                            double checkedVal, BOOL globValUsed, double globVal);

// 'lastCheck' (in/out) obsahuje posledni hodnotu stavu checkboxu, 'lastCheck' se inicializuje na -1;
// 'valueBuf' je buffer pro hodnotu ve stavu "checked" (index v comboboxu),
// 'valueBuf' se inicializuje na -1;
// 'checkedVal' je pocatecni hodnota ve stavu "checked" (index v comboboxu);
// 'globValUsed'+'globVal' - hodnota pro treti stav checkboxu (pouziva se? + hodnota)
void CheckboxCombo(HWND dlg, int checkboxID, int comboID, int* lastCheck, int* valueBuf,
                   int checkedVal, BOOL globValUsed, int globVal);

// pomocna funkce: enablovani prikazu v menu
void MyEnableMenuItem(HMENU subMenu, int cmd, BOOL enable);

//
// ****************************************************************************
// CCenteredDialog
//

class CCenteredDialog : public CDialog
{
public:
    CCenteredDialog(HINSTANCE modul, int resID, HWND parent,
                    CObjectOrigin origin = ooStandard) : CDialog(modul, resID, parent, origin) {}
    CCenteredDialog(HINSTANCE modul, int resID, int helpID, HWND parent,
                    CObjectOrigin origin = ooStandard) : CDialog(modul, resID, helpID, parent, origin) {}

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

//
// ****************************************************************************
// CCommonPropSheetPage
//

class CCommonPropSheetPage : public CPropSheetPage
{
public:
    CCommonPropSheetPage(TCHAR* title, HINSTANCE modul, int resID,
                         DWORD flags /* = PSP_USETITLE*/, HICON icon,
                         CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, flags, icon, origin) {}
    CCommonPropSheetPage(TCHAR* title, HINSTANCE modul, int resID, UINT helpID,
                         DWORD flags /* = PSP_USETITLE*/, HICON icon,
                         CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, helpID, flags, icon, origin) {}

protected:
    virtual void NotifDlgJustCreated();
};

//
// ****************************************************************************
// CConfigPageGeneral
//

class CConfigPageGeneral : public CCommonPropSheetPage
{
protected:
    int LastTotSpeed;     // posledni hodnota checkboxu "total speed limit"
    char TotSpeedBuf[31]; // buffer pro zachovani obsahu radky "total speed limit"

public:
    CConfigPageGeneral();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CConfigPageDefaults
//

class CConfigPageDefaults : public CCommonPropSheetPage
{
protected:
    int LastMaxCon;                             // posledni hodnota checkboxu "max concurrent connections"
    char MaxConBuf[31];                         // buffer pro zachovani obsahu radky "max concurrent connections"
    int LastSrvSpeed;                           // posledni hodnota checkboxu "server speed limit"
    char SrvSpeedBuf[31];                       // buffer pro zachovani obsahu radky "server speed limit"
    CFTPProxyServerList* TmpFTPProxyServerList; // docasna kopie seznamu uzivatelem nadefinovanych proxy serveru

public:
    CConfigPageDefaults();
    ~CConfigPageDefaults();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CConfigPageConfirmations
//

class CConfigPageConfirmations : public CCommonPropSheetPage
{
public:
    CConfigPageConfirmations();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************
// CConfigPageAdvanced
//

class CConfigPageAdvanced : public CCommonPropSheetPage
{
public:
    CConfigPageAdvanced();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************
// CConfigPageLogs
//

class CConfigPageLogs : public CCommonPropSheetPage
{
protected:
    int LastLogMaxSize;           // posledni hodnota checkboxu "log max size"
    char LogMaxSizeBuf[31];       // buffer pro zachovani obsahu radky "log max size"
    int LastMaxClosedConLogs;     // posledni hodnota checkboxu "max disconnected connection logs"
    char MaxClosedConLogsBuf[31]; // buffer pro zachovani obsahu radky "max disconnected connection logs"

public:
    CConfigPageLogs();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CConfigPageServers
//

class CConfigPageServers;

class CServersListbox : public CWindow
{
protected:
    CConfigPageServers* ParentDlg;

public:
    CServersListbox(CConfigPageServers* dlg, int ctrlID);

protected:
    void OpenContextMenu(int curSel, int menuX, int menuY);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CConfigPageServers : public CCommonPropSheetPage
{
protected:
    CServerTypeList* TmpServerTypeList; // docasna pracovni kopie Config.ServerTypeList

public:
    CConfigPageServers();
    ~CConfigPageServers();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    // posun polozky v listboxu, parametry musi byt overene
    void MoveItem(HWND list, int fromIndex, int toIndex);

    // obnova listboxu; je-li 'focusLast' TRUE, fokusne posledni polozku, jinak drzi fokus
    // od minula; neni-li 'focusIndex' -1, jde o index, ktery se ma fokusnout
    void RefreshList(BOOL focusLast, int focusIndex = -1);

    void OnExportServer(CServerType* serverType);
    void OnImportServer();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend CServersListbox;
};

//
// ****************************************************************************
// CConfigPageOperations
//

class CConfigPageOperations : public CCommonPropSheetPage
{
public:
    CConfigPageOperations();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************
// CConfigPageOperations2
//

class CConfigPageOperations2 : public CCommonPropSheetPage
{
public:
    CConfigPageOperations2();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************
// CConfigDlg
//

class CConfigDlg : public CPropertyDialog
{
protected:
    CConfigPageGeneral PageGeneral;
    CConfigPageDefaults PageDefaults;
    CConfigPageConfirmations PageConfirmations;
    CConfigPageOperations PageOperations;
    CConfigPageOperations2 PageOperations2;
    CConfigPageAdvanced PageAdvanced;
    CConfigPageLogs PageLogs;
    CConfigPageServers PageServers;

public:
    CConfigDlg(HWND parent);
};

//
// ****************************************************************************
// CConnectDlg
//

class CConnectDlg;

class CBookmarksListbox : public CWindow
{
protected:
    CConnectDlg* ParentDlg;

public:
    CBookmarksListbox(CConnectDlg* dlg, int ctrlID);

protected:
    void MoveUpDown(BOOL moveUp);
    void OpenContextMenu(int curSel, int menuX, int menuY);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CConnectDlg : public CCenteredDialog
{
protected:
    BOOL OK;                                   // TRUE pokud se podarila inicializace objektu dialogu
    CFTPServerList TmpFTPServerList;           // docasna kopie dat bookmark na FTP serverech (aby sel cancel)
    CFTPProxyServerList TmpFTPProxyServerList; // docasna kopie seznamu proxy serveru (aby sel cancel)
    BOOL CanChangeFocus;                       // TRUE pokud se ma menit fokus na tlacitka Connect a Close (obrana proti nekonecnemu cyklu)
    int DragIndex;                             // index pro drag&drop v listboxu
    BOOL ExtraDragDropItemAdded;               // TRUE pokud je pridana prazdna polozka v listboxu (pro drag&drop na konec seznamu)
    int AddBookmarkMode;                       // 0 - connect, 1 - organize bookmarks, 2 - org. bookmarks + focus last bookmark

    char LastRawHostAddress[HOST_MAX_SIZE]; // posledni hodnota zadana do editboxu "Address" (po opusteni editboxu se splitne, proto ji drzime v tomto bufferu)

public:
    CConnectDlg(HWND parent, int addBookmarkMode = 0);
    ~CConnectDlg()
    {
        memset(LastRawHostAddress, 0, HOST_MAX_SIZE); // mazeme pamet, ve ktere se objevil password
    }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    BOOL IsGood() { return OK; }

protected:
    void SelChanged();
    void EnableControls();
    void RefreshList(BOOL focusLast = FALSE);
    void AlignPasswordControls();
    void ShowHidePasswordControls(BOOL lockedPassword, BOOL focusEdit);

    void MoveItem(HWND list, int fromIndex, int toIndex, int topIndex = -1);
    BOOL GetCurSelServer(CFTPServer** server, int* index);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend CBookmarksListbox;
};

//
// ****************************************************************************
// CConnectAdvancedDlg
//

class CConnectAdvancedDlg : public CCenteredDialog
{
protected:
    CFTPServer* Server;
    CFTPProxyServerList* SourceTmpFTPProxyServerList; // zdroj pro TmpFTPProxyServerList (zaroven po OK v dialogu do nej ulozime zpet zmenena data)
    CFTPProxyServerList* TmpFTPProxyServerList;       // docasna kopie seznamu proxy serveru (aby sel cancel)
    int LastUseMaxCon;                                // posledni hodnota checkboxu "max. concurrent connections"
    char MaxConBuf[31];                               // buffer pro zachovani obsahu radky "max. concurrent connections"
    int LastUseTotSpeed;                              // posledni hodnota checkboxu "total speed limit for this server"
    char TotSpeedBuf[31];                             // buffer pro zachovani obsahu radky "total speed limit for this server"
    int LastKeepConnectionAlive;                      // posledni hodnota checkboxu "keep connection alive"
    int KASendCmd;                                    // hodnota pro zachovani stavu comba "keep-alive send command"
    char KASendEveryBuf[31];                          // buffer pro zachovani obsahu radky "keep-alive send every"
    char KAStopAfterBuf[31];                          // buffer pro zachovani obsahu radky "keep-alive stop after"

public:
    CConnectAdvancedDlg(HWND parent, CFTPServer* server,
                        CFTPProxyServerList* sourceTmpFTPProxyServerList);
    ~CConnectAdvancedDlg();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CRenameDlg
//
// podpora pro pet dialogu: Connect/New, Rename a Add Bookmark
//                          Configuration/Servers/New a Rename

class CRenameDlg : public CCenteredDialog
{
public:
    BOOL CopyDataFromFocusedServer;

protected:
    char* Name;         // jmeno polozky z listboxu (pro editline+checkbox)
    BOOL NewServer;     // TRUE/FALSE: dialog New/Rename
    BOOL AddBookmark;   // TRUE/FALSE: dialog Connect:Add Bookmark / plati NewServer
    BOOL ServerTypes;   // FALSE/TRUE: dialog Connect/Configuration:Servers
    char* CopyFromName; // jen pro ServerTypes==TRUE: jmeno pro checkbox (lisi se od jmena pro editline)

public:
    CRenameDlg(HWND parent, char* name, BOOL newServer, BOOL addBookmark = FALSE,
               BOOL serverTypes = FALSE, char* copyFromName = NULL);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSetWaitCursorWindow
//

class CSetWaitCursorWindow
{
public:
    HWND HWindow;
    WNDPROC DefWndProc;

public:
    void AttachToWindow(HWND hWnd);
    void DetachWindow();

    static LRESULT CALLBACK CWindowProc(HWND hwnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CWaitWindow
//
// nemodalni okenko (disablovani parenta a spousteni message-loopy je na volajicim);
// umisteni objektu na stacku (nealokovat - zbytecne);
// slouzi k zobrazeni informace o probihajicim deji, text muze byt viceradkovy, oddeleny
// znaky CRLF/LF, text je mozne behem zobrazeni menit
//

class CWaitWindow : public CWindow
{
protected:
    char* Caption;
    char* Text;
    SIZE TextSize;
    HWND HParent;
    BOOL ShowCloseButton;
    BOOL WindowClosePressed;
    BOOL HasTimer;
    BOOL Visible;
    BOOL NeedWrap;

public:
    CWaitWindow(HWND hParent, BOOL showCloseButton);
    ~CWaitWindow();

    // vraci TRUE pokud user stiskl mysi close button wait-okenka; TRUE vrati jen
    // jednou, dalsi TRUE vrati az po dalsim stisku close buttonu
    BOOL GetWindowClosePressed()
    {
        BOOL ret = WindowClosePressed;
        if (ret)
            WindowClosePressed = FALSE;
        return ret;
    }

    // nastaveni captionu; pokud nebude zavolano, caption bude LoadStr(IDS_FTPPLUGINTITLE)
    void SetCaption(const char* text);

    // nastaveni textu (mozne i behem zobrazeni - pozor: nedochazi ke zmene velikosti
    // okenka, mozne jen drobne zmeny - napr. countdouwn: 60s -> 50s -> 40s)
    void SetText(const char* text);

    // vytvoreni okenka, zobrazeni za 'showTime' milisekund
    HWND Create(DWORD showTime);

    // je-li 'show' FALSE a okenko je otevrene, schova ho; je-li 'show' TRUE a okenko je
    // schovane, ukaze ho
    void Show(BOOL show);

    // vola se po zmene viditelnosti okna a nastavuje promennou Visible
    virtual void SetVisible(BOOL visible) { Visible = visible; }

    // zrusi (zavre) okno
    void Destroy();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CListWaitWindow
//
// slouzi k zobrazeni informace o probihajicim listovani cesty na serveru;
// nemodalni okenko (disabluje parenta, ale spousteni message-loopy je na volajicim);
// umisteni objektu na stacku (nealokovat - zbytecne);
//

class CDataConnectionSocket;

class CListWaitWindow : public CWaitWindow
{
protected:
    CDataConnectionSocket* DataConnection; // sledovana "data connection"
    BOOL* Aborted;                         // ukazatel na BOOL, ve kterem je po abortnuti "data connection" TRUE

    CGUIStaticTextAbstract* PathOnFTPText;
    CGUIStaticTextAbstract* EstimatedTimeText;
    CGUIStaticTextAbstract* ElapsedTimeText;
    CGUIStaticTextAbstract* OperStatusText;
    CGUIProgressBarAbstract* OperProgressBar;

    char* Path;
    CFTPServerPathType PathType;
    char Status[100];
    char TimeLeft[20];
    char TimeElapsed[20];
    BOOL HasRefreshStatusTimer; // TRUE = nahozeny timer LISTWAITWND_AUTOUPDATETIMER
    BOOL HasDelayedUpdateTimer; // TRUE = nahozeny timer LISTWAITWND_DELAYEDUPDATETIMER
    BOOL NeedDelayedUpdate;     // TRUE = zpozdeny update je potreba

    DWORD LastTimeEstimation;          // -1==neplatny, jinak zaokrouhleny pocet sekund do konce operace
    DWORD ElapsedTime;                 // cas od zacatku operace v sekundach
    DWORD LastTickCountForElapsedTime; // -1==neplatny, jinak GetTickCount posledniho updatu ElapsedTime

public:
    // 'dataConnection' je sledovana "data connection"; 'aborted' (nesmi byt NULL) ukazuje
    // na BOOL, ve kterem je po abortnuti "data connection" TRUE
    CListWaitWindow(HWND hParent, CDataConnectionSocket* dataConnection, BOOL* aborted);
    ~CListWaitWindow();

    // nastaveni textu v prvni radce okna (mozne i behem zobrazeni)
    void SetText(const char* text);

    // nastaveni cesty (za textem "path:") v okne (druhy radek; mozne i behem zobrazeni)
    void SetPath(const char* path, CFTPServerPathType pathType);

    // vytvoreni okenka, zobrazeni za 'showTime' milisekund
    HWND Create(DWORD showTime);

    // vola se po zmene viditelnosti okna a nastavuje promennou Visible
    virtual void SetVisible(BOOL visible);

    // vytahne nove udaje o prenosu (velikost, rychlost, atd.) z "data connection"
    // a obnovi je v okne; 'fromTimer' je TRUE pokud je volani zpusobeno timerem
    void RefreshTimeAndStatusAndProgress(BOOL fromTimer);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CEnterStrDlg
//

class CEnterStrDlg : public CCenteredDialog
{
public:
    const char* Title;
    const char* Text;
    char* Data;
    int DataSize;
    BOOL HideChars;
    const char* ConnectingToAs;
    BOOL AllowEmpty;

public:
    CEnterStrDlg(HWND parent, const char* title, const char* text, char* data, int dataSize,
                 BOOL hideChars, const char* connectingToAs, BOOL allowEmpty);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CLoginErrorDlg
//

class CLoginErrorDlg : public CCenteredDialog
{
public:
    const char* ServerReply;
    CProxyScriptParams* ProxyScriptParams;
    BOOL RetryWithoutAsking;
    BOOL LoginChanged;

    const char* ConnectingTo;
    const char* Title;
    const char* RetryWithoutAskingText;
    const char* ErrorTitle;
    BOOL DisableUser;
    BOOL HideApplyToAll;
    BOOL ApplyToAll;
    BOOL ProxyUsed; // TRUE = pouziva se proxy server (editace proxy host/port/user/password)

public:
    CLoginErrorDlg(HWND parent, const char* serverReply, CProxyScriptParams* proxyScriptParams,
                   const char* connectingTo, const char* title, const char* retryWithoutAskingText,
                   const char* errorTitle, BOOL disableUser, BOOL hideApplyToAll,
                   BOOL proxyUsed);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CWelcomeMsgDlg
//

// pomocny objekt pro ovladani controlu nemodalniho dialogu bez IsDialogMessage
class CSimpleDlgControlWindow : public CWindow
{
protected:
    BOOL HandleKeys; // TRUE => zpracovani Enter, ESC a Tab klaves

public:
    CSimpleDlgControlWindow(HWND hDlg, int ctrlID, BOOL handleKeys = TRUE) : CWindow(hDlg, ctrlID)
    {
        HandleKeys = handleKeys;
    }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CWelcomeMsgDlg : public CCenteredDialog
{
public:
    const char* Text;
    int TextSize;            // -1 = null-terminated retezec
    const char* SentCommand; // na jaky prikaz je toto odpoved (jen u ServerReply==TRUE)
    HWND SizeBox;            // okno size-boxu

    BOOL ServerReply; // TRUE => jde o dialog "FTP Server Reply" (odpoved na poslany FTP command)
    BOOL RawListing;  // TRUE => jde o dialog "Raw Listing" (pouziva prikaz Show Raw Listing)

    // layoutovaci parametry
    int MinDlgHeight; // min. vyska dialogu
    int MinDlgWidth;  // min. sirka dialogu
    int EditBorderWidth;
    int EditBorderHeight;
    int ButtonWidth;
    int ButtonBottomBorder;
    int SizeBoxWidth;
    int SizeBoxHeight;
    int SaveAsButtonWidth;
    int SaveAsButtonOffset;

public:
    CWelcomeMsgDlg(HWND parent, const char* text, BOOL serverReply = FALSE,
                   const char* sentCommand = NULL, BOOL rawListing = FALSE,
                   int textSize = -1); // je-li 'textSize' -1 je 'text' null-terminated retezec

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnSaveTextAs();
};

//
// ****************************************************************************
// CLogsDlg
//

#define WM_APP_UPDATELISTOFLOGS WM_APP + 1 // [0, 0] - update seznamu logu
#define WM_APP_UPDATELOG WM_APP + 2        // [UID, 0] - update logu s UID 'UID'
#define WM_APP_ACTIVATELOG WM_APP + 3      // [UID, 0] - aktivace logu s UID 'UID'

class CLogsDlg : public CDialog
{
public:
    BOOL* SendWMClose; // zapis TRUE zajisti poslani WM_CLOSE tomuto dialogu (popis viz COperationDlgThread::Body())
    BOOL CloseDlg;     // TRUE = ma se co nejdrive zavrit dialog (vyuziva se jen pri zadosti o zavreni jeste pred otevrenim dialogu)

protected:
    HWND CenterToWnd; // okno, ke kteremu se na zacatku dialog centruje (NULL = necentruje se)
    HWND SizeBox;     // okno size-boxu
    int ShowLogUID;   // pri otevreni dialogu vybira log s UID 'ShowLogUID' (neni-li -1)
    BOOL Empty;       // TRUE pokud neexistuji zadne logy
    int LastLogUID;   // UID logu zobrazeneho v editu

    BOOL HasDelayedUpdateTimer; // TRUE = nahozeny timer LOGSDLG_DELAYEDUPDATETIMER
    int DelayedUpdateLogUID;    // UID logu pro zpozdeny update (-1 = zatim se nema updatovat zadny log)

    // layoutovaci parametry
    int MinDlgHeight; // min. vyska dialogu
    int MinDlgWidth;  // min. sirka dialogu
    int ComboBorderWidth;
    int ComboHeight;
    int EditBorderWidth;
    int EditBorderHeight;
    int ButtonWidth;
    int ButtonBottomBorder;
    int SizeBoxWidth;
    int SizeBoxHeight;
    int LineHeight;

public:
    CLogsDlg(HWND parent, HWND centerToWnd, int showLogUID);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LoadListOfLogs(BOOL update); // je-li 'update' FALSE jde o init comba, jinak jde o update

    // je-li 'updateUID' -1, jde o zmenu fokusu, jinak jde o update logu s UID 'updateUID'
    void LoadLog(int updateUID);
};

//
// ****************************************************************************
// CSendFTPCommandDlg
//

class CSendFTPCommandDlg : public CCenteredDialog
{
public:
    char Command[FTPCOMMAND_MAX_SIZE];
    BOOL ChangePathInPanel;
    BOOL RefreshWorkingPath;

public:
    CSendFTPCommandDlg(HWND parent);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CEditServerTypeDlg
//

// pomocny objekt pro edit control Rules for Parsing (vlastni kontextove menu + nedela
// pri fokusu controlu select-all)
class CEditRulesControlWindow : public CWindow
{
public:
    CEditRulesControlWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CEditServerTypeDlg : public CCenteredDialog
{
protected:
    HWND HListView;                             // listview se sloupcema
    BOOL CanReadListViewChanges;                // TRUE = zaznamenavat zmeny checkboxu v list-view, FALSE=pri plneni list-view je to nezadouci
    CServerType* ServerType;                    // editovany server type (ke zmenam muze dojit az pri stisku tlacitka OK)
    TIndirectArray<CSrvTypeColumn> ColumnsData; // sloupce zobrazene v dialogu (data pro listview se sloupcema)

    char* RawListing;       // listing pro testy parseru (text v dialogu Test of Parser je pro vetsi pohodli persistentni)
    BOOL RawListIncomplete; // checkbox "act as if listing is incomplete" pro testy parseru (hodnota v dialogu Test of Parser je pro vetsi pohodli persistentni)

public:
    CEditServerTypeDlg(HWND parent, CServerType* serverType);
    ~CEditServerTypeDlg();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    void InitColumns();     // do listview prida sloupce
    void SetColumnWidths(); // nastavi optimalni sirky sloupcu
    void RefreshListView(BOOL onlySet, int selIndex = -1);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CEditSrvTypeColumnDlg
//

class CEditSrvTypeColumnDlg : public CCenteredDialog
{
protected:
    TIndirectArray<CSrvTypeColumn>* ColumnsData; // vsechny sloupce (menit az na OK)
    BOOL Edit;                                   // TRUE = editace, FALSE = pridani noveho sloupce
    int* EditedColumn;                           // IN: index (v 'ColumnsData') editovaneho sloupce, OUT: focus index

    int LastUsedIndexForName;  // -1==neplatne, jinak index v combu posledni volby Name
    int LastUsedIndexForDescr; // -1==neplatne, jinak index v combu posledni volby Description

    BOOL FirstSelNotifyAfterTransfer; // TRUE = prave se do dialogu nastavila data, postnuty CBN_SELCHANGE pro combo Type nesmi zmenit hodnotu Alignment

public:
    CEditSrvTypeColumnDlg(HWND parent, TIndirectArray<CSrvTypeColumn>* columnsData,
                          int* editedColumn, BOOL edit);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSrvTypeTestParserDlg
//

class CFTPParser;

class CSrvTypeTestParserDlg : public CCenteredDialog
{
protected:
    HWND HListView;                          // listview se sloupcema
    CFTPParser* Parser;                      // testovany parser listingu
    TIndirectArray<CSrvTypeColumn>* Columns; // testovana definice sloupcu pro parsovani
    char** RawListing;                       // buffer pro text listingu (alokovany retezec vlastni nadrazeny dialog Edit Server Type - listing prezije zavreni/otevreni tohoto dialogu)
    int AllocatedSizeOfRawListing;           // aktualni alokovana velikost bufferu *RawListing
    BOOL* RawListIncomplete;                 // checkbox "act as if listing is incomplete" (vlastni nadrazeny dialog Edit Server Type - nastaveni prezije zavreni/otevreni tohoto dialogu)
    TDirectArray<DWORD> Offsets;             // dvojice offsetu (zacatek+konec radky, ze ktere vznikla polozka (soubor/adresar) v list-view
    int LastSelectedOffset;                  // index posledni zvolene dvojice offsetu (jen optimalizace nastavovani editboxu)

    HIMAGELIST SymbolsImageList; // image list pro listview

    // parametry pro layoutovani dialogu
    int MinDlgHeight;
    int MinDlgWidth;
    int ListingHeight;
    int ListingSpacing;
    int ButtonsY;
    int ParseBorderX;
    int ReadBorderX;
    int CloseBorderX;
    int HelpBorderX;
    int CloseBorderY;
    int ResultsSpacingX;
    int ResultsSpacingY;
    int SizeBoxWidth;
    int SizeBoxHeight;

    HWND SizeBox;

public:
    CSrvTypeTestParserDlg(HWND parent, CFTPParser* parser,
                          TIndirectArray<CSrvTypeColumn>* columns,
                          char** rawListing, BOOL* rawListIncomplete);
    ~CSrvTypeTestParserDlg();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void InitColumns();            // do listview prida sloupce
    void SetColumnWidths();        // nastavi optimalni sirky sloupcu
    void ParseListingToListView(); // parsuje *RawListing a dava vysledky primo do listview

    void LoadTextFromFile();

    void OnWMSize(int width, int height, BOOL notInitDlg, WPARAM wParam);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CCopyMoveDlg
//

class CCopyMoveDlg : public CCenteredDialog
{
protected:
    const char* Title;
    const char* Subject;
    char* Path;
    int PathBufSize;
    char** History;
    int HistoryCount;

public:
    CCopyMoveDlg(HWND parent, char* path, int pathBufSize, const char* title,
                 const char* subject, char* history[], int historyCount, int helpID);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CConfirmDeleteDlg
//

class CConfirmDeleteDlg : public CCenteredDialog
{
protected:
    const char* Subject;
    HICON Icon;

public:
    CConfirmDeleteDlg(HWND parent, const char* subject, HICON icon);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CChangeAttrsDlg
//

class CChangeAttrsDlg : public CCenteredDialog
{
protected:
    const char* Subject;
    DWORD Attr;
    DWORD AttrDiff;
    BOOL EnableNotification;

public:
    BOOL SelFiles;       // change attributes of files
    BOOL SelDirs;        // change attributes of dirs
    BOOL IncludeSubdirs; // include subdirs
    DWORD AttrAndMask;   // vysledna AND maska atributu (nulovani atributu)
    DWORD AttrOrMask;    // vysledna OR maska atributu (zapinani atributu)

public:
    CChangeAttrsDlg(HWND parent, const char* subject, DWORD attr, DWORD attrDiff,
                    BOOL selDirs);

    void RefreshNumValue(); // podle checkboxu nastavi cislo

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// COperationDlg
//

#define OPERDLG_UPDATEPERIOD 100 // nejkratsi interval (v ms) mezi dvemi obnovami informaci v dialogu operace
#define OPERDLG_UPDATETIMER 1    // ID timeru pro zpozdenou obnovu informaci v dialogu
#ifdef _DEBUG
#define OPERDLG_CHECKCOUNTERSTIMER 2    // ID timeru pro test pocitadel ve fronte
#define OPERDLG_CHECKCOUNTERSPERIOD 300 // perioda timeru pro test pocitadel ve fronte
#endif
#define OPERDLG_STATUSUPDATETIMER 3      // ID timeru pro zpozdenou obnovu status/progres informaci v dialogu (prilis rychle updaty od workeru (v pripade ze jich bezi vic nez jeden) se vynechavaji)
#define OPERDLG_STATUSUPDATEPERIOD 1000  // perioda timeru pro obnovu status/progres informaci v dialogu (pravidelny update, probiha jen tehdy, kdyz neprobihaji updaty od workeru)
#define OPERDLG_STATUSMINIDLETIME 950    // minimalni casovy odstup dalsiho updatu (periodickeho i od workera) od posledniho updatu od workera - POZOR: vazba na WORKER_STATUSUPDATETIMEOUT
#define OPERDLG_GETDISKSPACEPERIOD 3000  // cas, po kterem se znovu ziska volne misto na cilovem disku
#define OPERDLG_SHOWERRMINIDLETIME 10000 // minimalni doba bez aktivity v dialogu, aby se ukazala chyba samocinne (viz Config.OpenSolveErrIfIdle)
#define OPERDLG_AUTOSHOWERRTIMER 4       // ID timeru pro periodicky test jestli se nema ukazat nejaky Show Error dialog
#define OPERDLG_AUTOSHOWERRPERIOD 1000   // perioda timeru pro test jestli se nema ukazat nejaky Show Error dialog
#define OPERDLG_AUTOSHOWERRTIMER2 5      // pomocny timer: zajisti "okamzite" doruceni OPERDLG_AUTOSHOWERRTIMER
#define OPERDLG_CORRECTBTNSTIMER 6       // ID timeru pro kontrolu stavu buttonu v dialogu kratce po aktivaci dialogu (pri aktivaci jeste bohuzel neni znamy focus)

#define WM_APP_DISABLEDETAILED WM_APP + 1   // [0, 0] - disablovani butonu Detailed po maximalizaci
#define WM_APP_ACTIVATEWORKERS WM_APP + 2   // [0, 0] - aktivace workeru po otevreni dialogu
#define WM_APP_WORKERCHANGEREP WM_APP + 3   // [0, 0] - doslo ke hlaseni zmeny ve workerovi, mame si precist z operace kde ke zmene doslo (viz CFTPOperation::GetChangedWorker())
#define WM_APP_ITEMCHANGEREP WM_APP + 4     // [0, 0] - doslo ke hlaseni zmeny v polozkach, mame si precist z operace kde ke zmenam doslo (viz CFTPOperation::GetChangedItems())
#define WM_APP_OPERSTATECHANGE WM_APP + 5   // [0, 0] - doslo ke zmene stavu operace (hotovo/provadi se/dokonceno s chybami), mame si precist z operace stav (viz CFTPOperation::GetOperationState())
#define WM_APP_HAVEDISKFREESPACE WM_APP + 6 // [0, 0] - thread zjistujici volne misto na disku hlasi, ze ma vysledek
#define WM_APP_CLOSEDLG WM_APP + 7          // [0, 0] - ma dojit k zavreni progress dialogu (vyuziva auto-close)

#define OPERDLG_CONSTEXTBUFSIZE 1000  // max. delka textu ve sloupci listview Connections
#define OPERDLG_ITEMSTEXTBUFSIZE 1000 // max. delka textu ve sloupci listview Operations

class CFTPQueue;
class CFTPWorkersList;
class COperationDlg;

class CGetDiskFreeSpaceThread : public CThread
{
protected:
    // kriticka sekce pro pristup k datum objektu
    CRITICAL_SECTION GetFreeSpaceCritSect;

    char Path[MAX_PATH]; // cesta, na ktere zjistujeme volne misto
    CQuadWord FreeSpace; // zjistene volne misto; -1 = nezname volne misto
    HWND Dialog;         // handle dialogu operace, ktery ma prijmout

    HANDLE WorkOrTerminate; // "signaled" pokud ma thread zjistit volne misto na disku nebo pokud se ma ukoncit
    BOOL TerminateThread;   // TRUE = thread se ma terminovat

public:
    CGetDiskFreeSpaceThread(const char* path, HWND dialog);
    ~CGetDiskFreeSpaceThread();

    // volat po konstruktoru, pokud vrati FALSE, neni mozne dale objekt pouzivat
    BOOL IsGood() { return WorkOrTerminate != NULL; }

    // dialog touto metodou naplanuje ukonceni threadu
    void ScheduleTerminate();

    // dialog touto metodou naplanuje zjisteni volneho mista na disku; po zjisteni
    // mu prijde WM_APP_HAVEDISKFREESPACE, pak si muze precist vysledek
    void ScheduleGetDiskFreeSpace();

    // vraci posledni zjisteny udaj o volnem miste
    CQuadWord GetResult();

    virtual unsigned Body();
};

class COperDlgListView : public CWindow
{
public:
    HWND HToolTip;
    COperationDlg* OperDlg; // dialog, ve kterem listview existuje
    BOOL ConsOrItems;       // TRUE/FALSE = data brat z Connections/Operations listview

    int LastItem;
    int LastSubItem;
    int LastWidth;

    BOOL Scrolling; // TRUE/FALSE = user prave pouziva/nepouziva scrollbaru

    DWORD LastLButtonDownTime;
    LPARAM LastLButtonDownLParam;

public:
    COperDlgListView();
    ~COperDlgListView();

    void Attach(HWND hListView, COperationDlg* operDlg, BOOL consOrItems);
    void HideToolTip(int onlyIfOnIndex = -1); // neni-li 'onlyIfOnIndex' -1, jde o index, ktery se meni a tudiz je-li nad nim otevreny tooltip, musime ho zavrit

protected:
    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class COperationDlg : public CDialog
{
public:
    CFTPOperation* Oper;               // operace, pro kterou je dialog otevreny (existuje vzdy dele nez dialog, proto si dovolime primi pristup a ne pres operUID) (nemuze byt NULL)
    CFTPQueue* Queue;                  // fronta polozek operace (kopie Oper->Queue - ukazatel na frontu se behem operace nemeni)
    CFTPWorkersList* WorkersList;      // seznam workeru operace (ukazatel na seznam se behem operace nemeni)
    BOOL* SendWMClose;                 // zapis TRUE zajisti poslani WM_CLOSE tomuto dialogu (popis viz COperationDlgThread::Body())
    BOOL CloseDlg;                     // TRUE = ma se co nejdrive zavrit dialog (vyuziva se jen pri zadosti o zavreni jeste pred otevrenim dialogu)
    BOOL DlgWillCloseIfOpFinWithSkips; // TRUE = pokud se operace dokonci jen se skipnutymi polozkami, okno operace se zavre (a s nim i vsichni workeri) - pouziva se pro rozhodnuti jaky refresh pouzit (okamzity nebo az po aktivaci hlavniho okna)

protected:
    HWND CenterToWnd;                    // okno, ke kteremu se na zacatku dialog centruje (NULL = necentruje se)
    HWND SizeBox;                        // okno size-boxu
    CGUIStaticTextAbstract* Source;      // text zdrojove cesty (nemuze byt NULL)
    CGUIStaticTextAbstract* Target;      // text cilove cesty (nemuze byt NULL)
    CGUIStaticTextAbstract* TimeLeft;    // time left text (nemuze byt NULL)
    CGUIStaticTextAbstract* ElapsedTime; // elapsed time text (nemuze byt NULL)
    CGUIStaticTextAbstract* Status;      // status text (nemuze byt NULL)
    CGUIProgressBarAbstract* Progress;   // progress-bar (nemuze byt NULL)
    DWORD ProgressValue;                 // posledni nastavena hodnota na progresu 'Progress'; -2 == progres je v klidu, ma zobrazeny napis "done", "errors", "stopped" nebo "paused"
    HWND ConsListView;                   // listview Connections
    COperDlgListView ConsListViewObj;    // objekt listview Connections (dela tooltip)
    HIMAGELIST ConsImageList;            // image list pro listview Connections
    HWND ItemsListView;                  // listview Operations
    COperDlgListView ItemsListViewObj;   // objekt listview Operations (dela tooltip)
    HIMAGELIST ItemsImageList;           // image list pro listview Operations

    char ConsTextBuf[3][OPERDLG_CONSTEXTBUFSIZE];   // buffery pro text pro LVN_GETDISPINFO pro listview Connections
    int ConsActTextBuf;                             // ktery ze tri bufferu je prave mozne pouzit pro LVN_GETDISPINFO pro listview Connections
    char ItemsTextBuf[3][OPERDLG_ITEMSTEXTBUFSIZE]; // buffery pro text pro LVN_GETDISPINFO pro listview Operations
    int ItemsActTextBuf;                            // ktery ze tri bufferu je prave mozne pouzit pro LVN_GETDISPINFO pro listview Operations

    BOOL SimpleLook; // TRUE/FALSE = simple (po split-baru) / detailed (kompletni) vzhled dialogu

    char* TitleText; // text pro titulek dialogu (bez uvodniho "(XX%) ")

    BOOL IsDirtyStatus;                 // TRUE = je potreba updatnout status/progres operace (je treba prekreslit status/progres v dialogu)
    BOOL IsDirtyProgress;               // TRUE = doslo ke zmene workera, mozna (pokud se worker menil z duvodu zmeny progresu) je potreba updatnout status/progres operace (je treba prekreslit status/progres v dialogu)
    DWORD LastUpdateOfProgressByWorker; // GetTickCount z okamziku posledniho updatu progresu z popudu workera (pouziva se pro vynechani zbytecne castych updatu pri vice spustenych workerech + vynechani pravidelnych updatu)
    BOOL IsDirtyConsListView;           // TRUE = obsah listview Connections se zmenil (je treba ho prekreslit v dialogu)
    BOOL IsDirtyItemsListView;          // TRUE = obsah listview Operations se zmenil (je treba ho prekreslit v dialogu)
    BOOL HasDelayedUpdateTimer;         // TRUE = je nahozeny timer OPERDLG_UPDATETIMER

    int ConErrorIndex;           // listview Connections: index prvni chyby; -1=zadna chyba neexistuje
    BOOL EnableChangeFocusedCon; // TRUE = pri zmene fokusu v Connections aktivovat Log workera (FALSE se pouziva pri refreshi listviewevu pro zabraneni nechtenych zmen)

    BOOL ShowOnlyErrors;               // TRUE = listview Operations obsahuje jen polozky ve stavu "wait for user" (jinak obsahuje vsechny polozky)
    TDirectArray<DWORD> ErrorsIndexes; // listview Operations: pole indexu polozek ve stavu "wait for user" v Queue
    BOOL EnableShowOnlyErrors;         // TRUE = checkbox "show only errors" ma byt enaled, jinak disabled

    int FocusedItemUID;              // UID polozky s fokusem v listview Operations (-1 = nezname)
    BOOL EnableChangeFocusedItemUID; // TRUE = pri zmene fokusu menit i FocusedItemUID (FALSE se pouziva pri refreshi listviewevu pro zabraneni nechtenych zmen FocusedItemUID)

    BOOL UserWasActive;       // TRUE = nedovolime oknu, aby se automaticky zavrelo po uspesnem dokonceni operace (user neco delal, zmizel by mu dialog pod rukama)
    BOOL DelayAfterCancel;    // TRUE = nenechame okamzite otevrit dalsi Solve Error dialog po Cancelu v predchozim Solve Error dialogu
    BOOL CloseDlgWhenOperFin; // FALSE = nemame zavirat okno po dokonceni operace = okno se muze zavrit je-li Config.CloseOperationDlgIfSuccessfullyFinished==TRUE
    DWORD ClearChkboxTime;
    HWND LastFocusedControl; // posledni vyfocusenej control v dialogu
    DWORD LastActivityTime;  // GetTickCount() z okamziku posledni aktivity usera

    DWORD LastTimeEstimation; // -1==neplatny, jinak zaokrouhleny pocet sekund do konce operace

    char* OperationsTextOrig;        // puvodni text titulku listview "Operations:"
    int DisplayedDoneOrSkippedCount; // pocet skip+done polozek zobrazeny v titulku listview za "Operations:" (-1 = neznamy)
    int DisplayedTotalCount;         // celkovy pocet polozek zobrazeny v titulku listview za "Operations:" (-1 = neznamy)

    BOOL DisableAddWorkerButton; // TRUE = vsechny polozky jsou "done", takze pridavani dalsich workeru nema smysl

    BOOL ShowLowDiskWarning;       // TRUE = ukazuje se ikona+hint "low disk space", FALSE = status je az do praveho okraje + ikona s hintem jsou schovane
    CQuadWord LastNeededDiskSpace; // hodnota velikosti potrebneho mista na cilovem disku (z okamziku posledniho naplanovani ziskani volneho mista na disku)
    DWORD LastGetDiskFreeSpace;    // GetTickCount() z posledniho naplanovani ziskani volneho mista na disku (v threadu GetDiskFreeSpaceThread)
    CGUIHyperLinkAbstract* LowDiskSpaceHint;
    CGetDiskFreeSpaceThread* GetDiskFreeSpaceThread;

    HWND CurrentFlashWnd; // je-li != NULL, jde o okno, ktere se ma pri zmene titulku flashnout (volat FlashWindow; pri zmene titulku se totiz flash ztraci, musime ho obnovovat)

    // layoutovaci parametry (suffix "1" = "simple", suffix "2" = "detailed")
    int MinDlgHeight1;  // minimalni vyse dialogu pri simple vzhledu
    int MinDlgHeight2;  // minimalni vyse dialogu pri detailed vzhledu
    int MinDlgWidth;    // minimalni sire dialogu
    int LastDlgHeight1; // vyska posledne zobrazene "detailed" varianty
    int MinClientHeight;
    int SizeBoxWidth;
    int SizeBoxHeight;
    int SourceBorderWidth;
    int SourceHeight;
    int ProgressBorderWidth;
    int ProgressHeight;
    int DetailsXROffset;
    int DetailsYOffset;
    int NextErrXROffset;
    int HideXROffset;
    int PauseXROffset;
    int CancelXROffset;
    int HelpXROffset;
    int ErrIconXROffset;
    int ErrIconYOffset;
    int ErrHintXROffset;
    int ErrHintYOffset;
    int ErrIconToHintWidth;
    int SplitBorderWidth;
    int SplitHeight;
    int ConnectionsBorderWidth;
    int ConnectionsHeight;
    int ConnectionsYOffset;
    int ConsAddXROffset;
    int ConsAddYOffset;
    int ConsShowErrXROffset;
    int ConsStopXROffset;
    int ConsPauseXROffset;
    int OperTxtXOffset;
    int OperTxtYOffset;
    int OperationsBorderWidth;
    int OperationsHeight;
    int OperationsXOffset;
    int OperationsYOffset;
    int OperationsEdges;
    int OpersShowErrXROffset;
    int OpersShowErrYOffset;
    int OpersRetryXROffset;
    int OpersSkipXROffset;
    int ShowOnlyErrXOffset;
    int ShowOnlyErrYOffset;

    int ConnectionsActWidth;       // aktualni sirka listview Connections
    int ConnectionsActHeight;      // aktualni vyska listview Connections
    int ConsAddActYOffset;         // aktualni y-offset tlacitka Add pod listviewem Connections
    int ConnectionsActHeightLimit; // aktualni limit pro vysku listviewu Connections
    BOOL InListViewSplit;          // TRUE = mys je v oblasti, kde se da tazenim menit vyska listview Connections proti vysce listview Operations
    BOOL Captured;                 // TRUE = mys je nase (captured)
    int DragOriginY;               // y-ova souradnice stisknuti l-buttonu mysi (pocatek tazeni)
    double ListviewSplitRatio;     // pomer mezi vyskou listview Connections a celkovou vyskou pro listviewy

    BOOL RestoreToMaximized; // TRUE pokud user dal minimize pri maximized stavu -> restore provede maximize

    BOOL PauseButtonIsEnabled;        // tlacitko Pause/Resume nad Connections listviewem: TRUE = enablovano, FALSE = disablovano
    BOOL PauseButtonIsResume;         // aktualni text tlacitka Pause/Resume nad Connections listviewem: TRUE = Resume, FALSE = Pause
    char PauseButtonPauseText[50];    // buffer pro text "Pause" z resourcu dialogu (text "Resume" je IDS_OPERDLGRESUMEBUTTON)
    BOOL ConPauseButtonIsResume;      // aktualni text tlacitka Pause/Resume pod Connections listviewem: TRUE = Resume, FALSE = Pause
    char ConPauseButtonPauseText[50]; // buffer pro text "Pause" z resourcu dialogu (text "Resume" je IDS_OPERDLGRESUMECONBUTTON)

public:
    COperationDlg(HWND parent, HWND centerToWnd, CFTPOperation* oper, CFTPQueue* queue,
                  CFTPWorkersList* workersList);
    ~COperationDlg();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LayoutDialog(BOOL showSizeBox);

    // zmeni promennou ShowLowDiskWarning + schova/ukaze hint+ikonu (ale nedela layoutovani)
    void SetShowLowDiskWarning(BOOL show);

    void ToggleSimpleLook();
    void ShowControlsAndChangeSize(BOOL simple);

    void SetDlgTitle(int progressValue, const char* state);

    void CorrectLookOfPrevFocusedDisabledButton(HWND prevFocus);

    void SolveErrorOnConnection(int index);
    void SolveErrorOnItem(int itemUID);

    void SetUserWasActive();

    void InitColumns();
    void SetColumnWidths();
    void RefreshConnections(BOOL init, int newFocusIndex = -1, int refreshOnlyIndex = -1); // 'init' je TRUE jen pri volani z WM_INITDIALOG
    void RefreshItems(BOOL init, int refreshOnlyIndex1 = -1, int refreshOnlyIndex2 = -1);  // 'init' je TRUE jen pri volani z WM_INITDIALOG
    void EnableErrorsButton();
    void EnablePauseButton();
    void EnableSolveConError(int index);  // je-li 'index' -1, provede se pro focus index z listview Connections
    void EnableRetryItem(int index);      // je-li 'index' -1, provede se pro focus index z listview Operations
    void EnablePauseConButton(int index); // je-li 'index' -1, provede se pro focus index z listview Connections

    // zajisti update (prekresleni) dat dialogu z vnitrnich promennych dialogu
    // (vola na to metodu UpdateDataInDialog()); nejkratsi perioda dvou po sobe
    // jdoucich updatu je OPERDLG_UPDATEPERIOD milisekund
    void ScheduleDelayedUpdate();

    // vykresli (ukaze) nove hodnoty vsech zmenenych dat zobrazenych v dialogu
    // (pouziva se pri zpozdenem refreshovani udaju); vraci TRUE pokud bylo
    // potreba neco updatnout
    BOOL UpdateDataInDialog();

    // zajisti nastaveni tlacitka pro zavreni dialogu (Close/Cancel); 'flashTitle' je TRUE
    // pri volani ze zpracovani WM_APP_OPERSTATECHANGE (zmenu hlasi operace)
    void SetupCloseButton(BOOL flashTitle);

    friend class CFTPOperation;
    friend class COperDlgListView;
};

//
// ****************************************************************************
// CSolveItemErrorDlg
//

enum CSolveItemErrorDlgType
{
    sidtCannotCreateTgtDir,
    sidtTgtDirAlreadyExists,
    sidtCannotCreateTgtFile,
    sidtTgtFileAlreadyExists,
    sidtTransferFailedOnCreatedFile,
    sidtTransferFailedOnResumedFile,
    sidtASCIITrModeForBinFile,
    sidtUploadCannotCreateTgtDir,
    sidtUploadTgtDirAlreadyExists,
    sidtUploadCrDirAutoRenFailed,
    sidtUploadCannotCreateTgtFile,
    sidtUploadTgtFileAlreadyExists,
    sidtUploadASCIITrModeForBinFile,
    sidtUploadTransferFailedOnCreatedFile,
    sidtUploadTransferFailedOnResumedFile,
    sidtUploadFileAutoRenFailed,
    sidtUploadStoreFileFailed,
};

class CSolveItemErrorDlg : public CCenteredDialog
{
protected:
    CFTPOperation* Oper;
    DWORD WinError;
    const char* ErrDescription;
    const char* FtpPath;
    const char* FtpName;
    const char* DiskPath;
    const char* DiskName;
    BOOL* ApplyToAll;
    char** NewName;

    BOOL DontTransferName; // TRUE = nema se z dialogu ziskavat jmeno (ani validovat, ani transfer)

    int UsedButtonID; // ID tlacitka, kterym user zavira dialog (pouziva se v Transfer())

    CSolveItemErrorDlgType DlgType; // typ zobrazovaneho dialogu

public:
    CSolveItemErrorDlg(HWND parent, CFTPOperation* oper, DWORD winError,
                       const char* errDescription,
                       const char* ftpPath, const char* ftpName,
                       const char* diskPath, const char* diskName,
                       BOOL* applyToAll, char** newName, CSolveItemErrorDlgType dlgType);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSolveItemErrUnkAttrDlg
//

class CSolveItemErrUnkAttrDlg : public CCenteredDialog
{
protected:
    CFTPOperation* Oper;
    const char* Path;
    const char* Name;
    const char* OrigRights;
    WORD NewAttr;
    BOOL* ApplyToAll;
    int UsedButtonID; // ID tlacitka, kterym user zavira dialog (pouziva se v Transfer())

public:
    CSolveItemErrUnkAttrDlg(HWND parent, CFTPOperation* oper, const char* path, const char* name,
                            const char* origRights, WORD newAttr, BOOL* applyToAll);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSolveItemSetNewAttrDlg
//

class CSolveItemSetNewAttrDlg : public CCenteredDialog
{
protected:
    CFTPOperation* Oper;
    const char* Path;
    const char* Name;
    const char* OrigRights;
    WORD* Attr;
    BOOL* ApplyToAll;

    BOOL EnableNotification;

public:
    CSolveItemSetNewAttrDlg(HWND parent, CFTPOperation* oper, const char* path, const char* name,
                            const char* origRights, WORD* attr, BOOL* applyToAll);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void RefreshNumValue();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSolveLowMemoryErr
//

class CSolveLowMemoryErr : public CCenteredDialog
{
protected:
    const char* FtpPath;
    const char* FtpName;
    BOOL* ApplyToAll;
    int TitleID; // je-li ruzne od -1, jde o resource ID titulku dialogu

public:
    CSolveLowMemoryErr(HWND parent, const char* ftpPath, const char* ftpName, BOOL* applyToAll,
                       int titleID = -1);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSolveItemErrorSimpleDlg
//

enum CSolveItemErrorSimpleDlgType
{
    sisdtNone,           // jen inicializacni hodnota
    sisdtDelHiddenDir,   // Delete: directory is hidden
    sisdtDelHiddenFile,  // Delete: file or link is hidden
    sisdtDelNonEmptyDir, // Delete: directory is non-empty
};

class CSolveItemErrorSimpleDlg : public CCenteredDialog
{
protected:
    CFTPOperation* Oper;
    const char* FtpPath;
    const char* FtpName;
    BOOL* ApplyToAll;
    int UsedButtonID; // ID tlacitka, kterym user zavira dialog (pouziva se v Transfer())

    CSolveItemErrorSimpleDlgType DlgType; // typ zobrazovaneho dialogu

public:
    CSolveItemErrorSimpleDlg(HWND parent, CFTPOperation* oper,
                             const char* ftpPath, const char* ftpName,
                             BOOL* applyToAll, CSolveItemErrorSimpleDlgType dlgType);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSolveServerCmdErr
//

enum CSolveItemErrorSrvCmdDlgType
{
    siscdtSimple,     // zadne menu na Retry buttonu
    siscdtDeleteFile, // Delete: unable to delete file
    siscdtDeleteDir,  // Delete: unable to delete directory
};

class CSolveServerCmdErr : public CCenteredDialog
{
protected:
    int TitleID;
    const char* FtpPath;
    const char* FtpName;
    const char* ErrorDescr;
    BOOL* ApplyToAll;

    CSolveItemErrorSrvCmdDlgType DlgType; // typ zobrazovaneho dialogu

public:
    CSolveServerCmdErr(HWND parent, int titleID, const char* ftpPath,
                       const char* ftpName, const char* errorDescr,
                       BOOL* applyToAll, CSolveItemErrorSrvCmdDlgType dlgType);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CSolveServerCmdErr2
//

enum CSolveItemErrorSrvCmdDlgType2
{
    siscdt2Simple,               // zadne menu na Retry buttonu
    siscdt2ResumeFile,           // Copy/Move: unable to resume file (nabidka overwrite/resume/atd.)
    siscdt2ResumeTestFailed,     // Copy/Move: resume test fail (nabidka reduce-file/overwrite/resume/atd.)
    siscdt2UploadUnableToStore,  // upload: unable to store file to server + resume not supported in ASCII transfer mode + unable to resume (server does not support resuming + neni znama velikost ciloveho souboru + cilovy soubor je vetsi nez zdrojovy soubor)
    siscdt2UploadTestIfFinished, // upload: nelze overit jestli se soubor uspesne uploadnul
};

class CSolveServerCmdErr2 : public CCenteredDialog
{
protected:
    int TitleID;
    const char* FtpPath;
    const char* FtpName;
    const char* DiskPath;
    const char* DiskName;
    const char* ErrorDescr;
    BOOL* ApplyToAll;

    CSolveItemErrorSrvCmdDlgType2 DlgType; // typ zobrazovaneho dialogu

public:
    CSolveServerCmdErr2(HWND parent, int titleID, const char* ftpPath,
                        const char* ftpName, const char* diskPath,
                        const char* diskName, const char* errorDescr,
                        BOOL* applyToAll, CSolveItemErrorSrvCmdDlgType2 dlgType);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CViewErrAsciiTrForBinFileDlg
//

class CViewErrAsciiTrForBinFileDlg : public CCenteredDialog
{
public:
    CViewErrAsciiTrForBinFileDlg(HWND parent);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// CProxyServerDlg
//

// pomocny objekt pro edit control Script (vlastni kontextove menu + nedela
// pri fokusu controlu select-all)
class CProxyScriptControlWindow : public CWindow
{
public:
    CProxyScriptControlWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CProxyServerDlg : public CCenteredDialog
{
public:
    CFTPProxyServerList* TmpFTPProxyServerList; // seznam proxy serveru z parent dialogu
    CFTPProxyServer* Proxy;                     // data proxy serveru (zapis mozny jen pres CFTPProxyServerList::SetProxyServer(), cteni mozne bez sekce, protoze se zapisuje jen v hl. threadu, ve kterem zrovna jsme)
    BOOL Edit;                                  // TRUE = editujeme, FALSE = pridavame

public:
    CProxyServerDlg(HWND parent, CFTPProxyServerList* tmpFTPProxyServerList,
                    CFTPProxyServer* proxy, BOOL edit);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    // je-li 'initScriptText' TRUE, nastavi obsah editboxu Script, "read-only-flag"
    // nastavuje vzdy; je-li 'initProxyPort' TRUE, nastavi defaultni port proxy serveru;
    // dale enabluje dalsi controly v dialogu po zmene typu proxy serveru
    void EnableControls(BOOL initScriptText, BOOL initProxyPort);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void AlignPasswordControls();
    void ShowHidePasswordControls(BOOL lockedPassword, BOOL focusEdit);
};

//
// ****************************************************************************
// CPasswordEditLine
//
// subclass pro edit line obsahujici heslo; na ctrl+rclick postne do parenta
// command WM_APP_SHOWPASSWORD

#define WM_APP_SHOWPASSWORD WM_APP + 50 // [hWnd, lParam] - uzivatel cltrl+rclicknul do edit line, hWnd je handle okna edit line, lParam jsou souradnice kliknuti, viz WM_RBUTTONDOWN/lParam

class CPasswordEditLine : public CWindow
{
public:
    CPasswordEditLine(HWND hDlg, int ctrlID);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
