// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define WM_APP_ACTIVATEPARENT WM_APP + 1 // [0, 0] - activate parent (used by all wait-windows)
#define WM_APP_STATUSCHANGED WM_APP + 2  // [0, 0] - status change of "data connection" (used by list-wait-window)

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#define WAITWND_SHOWTIMER 1              // timer ID for showing the wait window
#define LISTWAITWND_AUTOUPDATETIMER 2    // timer ID for automatic update of the list-wait window (after 1 second)
#define LISTWAITWND_DELAYEDUPDATETIMER 3 // timer ID for delayed update of the list-wait window (after 1/10 second)
#define LOGSDLG_DELAYEDUPDATETIMER 4     // timer ID for delayed update of the logs window (after 1/10 second)

extern TIndirectArray<CDialog> ModelessDlgs; // array of modeless dialogs (Welcome Message)

extern ATOM AtomObject2; // atom for CSetWaitCursorWindow

// array of strings describing default behavior on errors during operations
extern int OperDefFileAlreadyExists[];
extern int OperDefDirAlreadyExists[];
extern int OperDefCannotCreateFileOrDir[];
extern int OperDefRetryOnCreatedFile[];
extern int OperDefRetryOnResumedFile[];
extern int OperDefAsciiTrModeForBinFile[];
extern int OperDefUnknownAttrs[];
extern int OperDefDeleteArr[];

// support for combo boxes with history
void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* text,
                     int textLen, int historySize, char* history[], BOOL secretValue);

// support for filling combo boxes with default behavior on errors during operations
void HandleOperationsCombo(int* value, CTransferInfo& ti, int resID, int arrValuesResID[]);

// support for combo boxes with Proxy servers
void ProxyComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, int& proxyUID, BOOL addDefault,
                   CFTPProxyServerList* proxyServerList);

// 'lastCheck' (in/out) stores the last state of the checkbox, 'lastCheck' initializes to -1;
// 'valueBuf' is a buffer for the value in the "checked" state with size of at least 31 characters,
// 'valueBuf' initializes to an empty string;
// 'checkedVal' is the initial value in the "checked" state;
// 'globValUsed'+'globVal' - value for the third state of the checkbox (is it used? + value)
void CheckboxEditLineInteger(HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                             int checkedVal, BOOL globValUsed, int globVal);

// 'lastCheck' (in/out) stores the last state of the checkbox, 'lastCheck' initializes to -1;
// 'valueBuf' is a buffer for the value in the "checked" state with size of at least 31 characters,
// 'valueBuf' initializes to an empty string;
// 'checkedVal' is the initial value in the "checked" state;
// 'globValUsed'+'globVal' - value for the third state of the checkbox (is it used? + value)
void CheckboxEditLineDouble(HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                            double checkedVal, BOOL globValUsed, double globVal);

// 'lastCheck' (in/out) stores the last state of the checkbox, 'lastCheck' initializes to -1;
// 'valueBuf' is a buffer for the value in the "checked" state (index in the combo box),
// 'valueBuf' initializes to -1;
// 'checkedVal' is the initial value in the "checked" state (index in the combo box);
// 'globValUsed'+'globVal' - value for the third state of the checkbox (is it used? + value)
void CheckboxCombo(HWND dlg, int checkboxID, int comboID, int* lastCheck, int* valueBuf,
                   int checkedVal, BOOL globValUsed, int globVal);

// helper function: enabling commands in a menu
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
    int LastTotSpeed;     // last value of the "total speed limit" checkbox
    char TotSpeedBuf[31]; // buffer to keep the contents of the "total speed limit" line

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
    int LastMaxCon;                             // last value of the "max concurrent connections" checkbox
    char MaxConBuf[31];                         // buffer to keep the contents of the "max concurrent connections" line
    int LastSrvSpeed;                           // last value of the "server speed limit" checkbox
    char SrvSpeedBuf[31];                       // buffer to keep the contents of the "server speed limit" line
    CFTPProxyServerList* TmpFTPProxyServerList; // temporary copy of the list of user-defined proxy servers

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
    int LastLogMaxSize;           // last value of the "log max size" checkbox
    char LogMaxSizeBuf[31];       // buffer to keep the contents of the "log max size" line
    int LastMaxClosedConLogs;     // last value of the "max disconnected connection logs" checkbox
    char MaxClosedConLogsBuf[31]; // buffer to keep the contents of the "max disconnected connection logs" line

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
    CServerTypeList* TmpServerTypeList; // temporary working copy of Config.ServerTypeList

public:
    CConfigPageServers();
    ~CConfigPageServers();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    // move an item in the listbox; parameters must already be validated
    void MoveItem(HWND list, int fromIndex, int toIndex);

    // refresh the listbox; if 'focusLast' is TRUE, focus the last item, otherwise keep the previous focus
    // from before; if 'focusIndex' is not -1, it is the index that should receive focus
    void RefreshList(BOOL focusLast, int focusIndex = -1); // if 'focusLast' is TRUE, focuses the last item, otherwise keeps the previous focus; if 'focusIndex' is not -1, that index gets focus

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
    BOOL OK;                                   // TRUE if initialization of the dialog object succeeded
    CFTPServerList TmpFTPServerList;           // temporary copy of bookmark data on FTP servers (to allow cancel)
    CFTPProxyServerList TmpFTPProxyServerList; // temporary copy of the proxy server list (to allow cancel)
    BOOL CanChangeFocus;                       // TRUE if focus may move to the Connect and Close buttons (protection against an endless loop)
    int DragIndex;                             // index used for drag&drop in the listbox
    BOOL ExtraDragDropItemAdded;               // TRUE if an empty listbox item is added (for drag&drop to the end of the list)
    int AddBookmarkMode;                       // 0 - connect, 1 - organize bookmarks, 2 - organize bookmarks + focus last bookmark

    char LastRawHostAddress[HOST_MAX_SIZE]; // last value entered into the "Address" edit box (after leaving the edit box it is split, so we keep it in this buffer)

public:
    CConnectDlg(HWND parent, int addBookmarkMode = 0);
    ~CConnectDlg()
    {
        memset(LastRawHostAddress, 0, HOST_MAX_SIZE); // wipe memory where the password appeared
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
    CFTPProxyServerList* SourceTmpFTPProxyServerList; // source for TmpFTPProxyServerList (after OK in the dialog we write the modified data back into it)
    CFTPProxyServerList* TmpFTPProxyServerList;       // temporary copy of the proxy server list (to allow cancel)
    int LastUseMaxCon;                                // last value of the "max. concurrent connections" checkbox
    char MaxConBuf[31];                               // buffer to keep the contents of the "max. concurrent connections" line
    int LastUseTotSpeed;                              // last value of the "total speed limit for this server" checkbox
    char TotSpeedBuf[31];                             // buffer to keep the contents of the "total speed limit for this server" line
    int LastKeepConnectionAlive;                      // last value of the "keep connection alive" checkbox
    int KASendCmd;                                    // value to keep the state of the "keep-alive send command" combo
    char KASendEveryBuf[31];                          // buffer to keep the contents of the "keep-alive send every" line
    char KAStopAfterBuf[31];                          // buffer to keep the contents of the "keep-alive stop after" line

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
// support for five dialogs: Connect/New, Rename and Add Bookmark
//                           Configuration/Servers/New and Rename

class CRenameDlg : public CCenteredDialog
{
public:
    BOOL CopyDataFromFocusedServer;

protected:
    char* Name;         // listbox item name (for edit line + checkbox)
    BOOL NewServer;     // TRUE/FALSE: dialog New/Rename
    BOOL AddBookmark;   // TRUE/FALSE: Connect:Add Bookmark dialog / applies to NewServer
    BOOL ServerTypes;   // FALSE/TRUE: Connect/Configuration:Servers dialog
    char* CopyFromName; // only if ServerTypes==TRUE: name for the checkbox (differs from the name for the edit line)

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
// modeless window (disabling the parent and starting the message loops is up to the caller);
// place the object on the stack (do not allocate - unnecessary);
// used to display information about an ongoing action, the text can have multiple lines separated
// by CRLF/LF characters, the text can be changed while it is displayed
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

    // returns TRUE if the user clicked the wait-window close button with the mouse; it returns TRUE
    // only once and the next TRUE comes only after another click of the close button
    BOOL GetWindowClosePressed()
    {
        BOOL ret = WindowClosePressed;
        if (ret)
            WindowClosePressed = FALSE;
        return ret;
    }

    // set caption; if not called, the caption will be LoadStr(IDS_FTPPLUGINTITLE)
    void SetCaption(const char* text);

    // set text (possible even while displayed - note: the window size does not change,
    // only minor changes are possible - e.g. countdown: 60s -> 50s -> 40s)
    void SetText(const char* text);

    // create the window, show it after 'showTime' milliseconds
    HWND Create(DWORD showTime);

    // if 'show' is FALSE and the window is open it hides it; if 'show' is TRUE and the window is
    // hidden it shows it
    void Show(BOOL show);

    // called after a change of window visibility and sets the Visible variable
    virtual void SetVisible(BOOL visible) { Visible = visible; }

    // destroy (close) the window
    void Destroy();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CListWaitWindow
//
// used to display information about browsing a path on the server;
// modeless window (disables the parent, but starting the message loops is up to the caller);
// place the object on the stack (do not allocate - unnecessary);
//

class CDataConnectionSocket;

class CListWaitWindow : public CWaitWindow
{
protected:
    CDataConnectionSocket* DataConnection; // monitored "data connection"
    BOOL* Aborted;                         // pointer to a BOOL that is TRUE after the "data connection" is aborted

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
    BOOL HasRefreshStatusTimer; // TRUE = timer LISTWAITWND_AUTOUPDATETIMER is running
    BOOL HasDelayedUpdateTimer; // TRUE = timer LISTWAITWND_DELAYEDUPDATETIMER is running
    BOOL NeedDelayedUpdate;     // TRUE = a delayed update is needed

    DWORD LastTimeEstimation;          // -1==invalid, otherwise rounded number of seconds until the end of the operation
    DWORD ElapsedTime;                 // time since the start of the operation in seconds
    DWORD LastTickCountForElapsedTime; // -1==invalid, otherwise GetTickCount of the last ElapsedTime update

public:
    // 'dataConnection' is the monitored "data connection"; 'aborted' (must not be NULL)
    // points to a BOOL that is TRUE after the "data connection" is aborted
    CListWaitWindow(HWND hParent, CDataConnectionSocket* dataConnection, BOOL* aborted);
    ~CListWaitWindow();

    // set the text in the first line of the window (possible even while displayed)
    void SetText(const char* text);

    // set the path (after the "path:" text) in the window (second line; possible even while displayed)
    void SetPath(const char* path, CFTPServerPathType pathType);

    // create the window, show it after 'showTime' milliseconds
    HWND Create(DWORD showTime);

    // called after a change of window visibility and sets the Visible variable
    virtual void SetVisible(BOOL visible);

    // retrieves new transfer data (size, speed, etc.) from the "data connection"
    // and refreshes them in the window; 'fromTimer' is TRUE if the call is caused by a timer
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
    BOOL ProxyUsed; // TRUE = a proxy server is used (editing proxy host/port/user/password)

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

// helper object for handling a control in a modeless dialog without IsDialogMessage
class CSimpleDlgControlWindow : public CWindow
{
protected:
    BOOL HandleKeys; // TRUE => handle Enter, ESC and Tab keys

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
    int TextSize;            // -1 = null-terminated string
    const char* SentCommand; // which command this is a response to (only if ServerReply==TRUE)
    HWND SizeBox;            // size-box window

    BOOL ServerReply; // TRUE => this is the "FTP Server Reply" dialog (response to a sent FTP command)
    BOOL RawListing;  // TRUE => this is the "Raw Listing" dialog (uses the Show Raw Listing command)

    // layout parameters
    int MinDlgHeight; // minimum dialog height
    int MinDlgWidth;  // minimum dialog width
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
                   int textSize = -1); // if 'textSize' is -1, 'text' is a null-terminated string

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnSaveTextAs();
};

//
// ****************************************************************************
// CLogsDlg
//

#define WM_APP_UPDATELISTOFLOGS WM_APP + 1 // [0, 0] - update list of logs
#define WM_APP_UPDATELOG WM_APP + 2        // [UID, 0] - update log with UID 'UID'
#define WM_APP_ACTIVATELOG WM_APP + 3      // [UID, 0] - activate log with UID 'UID'

class CLogsDlg : public CDialog
{
public:
    BOOL* SendWMClose; // writing TRUE ensures WM_CLOSE is sent to this dialog (see COperationDlgThread::Body())
    BOOL CloseDlg;     // TRUE = the dialog should close as soon as possible (used only when close is requested before the dialog opens)

protected:
    HWND CenterToWnd; // window the dialog centers to when opened (NULL = no centering)
    HWND SizeBox;     // size-box window
    int ShowLogUID;   // selects the log with UID 'ShowLogUID' when the dialog opens (if not -1)
    BOOL Empty;       // TRUE if no logs exist
    int LastLogUID;   // UID of the log displayed in the edit

    BOOL HasDelayedUpdateTimer; // TRUE = timer LOGSDLG_DELAYEDUPDATETIMER is running
    int DelayedUpdateLogUID;    // UID of the log for a delayed update (-1 = no log should be updated yet)

    // layout parameters
    int MinDlgHeight; // minimum dialog height
    int MinDlgWidth;  // minimum dialog width
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

    void LoadListOfLogs(BOOL update); // if 'update' is FALSE it initializes the combo, otherwise it updates it

    // if 'updateUID' is -1 it is a focus change, otherwise it updates the log with UID 'updateUID'
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

// helper object for the Rules for Parsing edit control (custom context menu + does not
// select-all when the control gets focus)
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
    HWND HListView;                             // listview with columns
    BOOL CanReadListViewChanges;                // TRUE = record checkbox changes in the list-view, FALSE = undesirable while filling the list-view
    CServerType* ServerType;                    // edited server type (changes may occur only after pressing the OK button)
    TIndirectArray<CSrvTypeColumn> ColumnsData; // columns shown in the dialog (data for the listview with columns)

    char* RawListing;       // listing for parser tests (text in the Test of Parser dialog stays persistent for convenience)
    BOOL RawListIncomplete; // checkbox "act as if listing is incomplete" for parser tests (its value stays persistent in the Test of Parser dialog for convenience)

public:
    CEditServerTypeDlg(HWND parent, CServerType* serverType);
    ~CEditServerTypeDlg();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();

    void InitColumns();     // add columns to the listview
    void SetColumnWidths(); // set optimal column widths
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
    TIndirectArray<CSrvTypeColumn>* ColumnsData; // all columns (modify only on OK)
    BOOL Edit;                                   // TRUE = editing, FALSE = adding a new column
    int* EditedColumn;                           // IN: index (in 'ColumnsData') of the edited column, OUT: focus index

    int LastUsedIndexForName;  // -1==invalid, otherwise index in the combo of the last Name selection
    int LastUsedIndexForDescr; // -1==invalid, otherwise index in the combo of the last Description selection

    BOOL FirstSelNotifyAfterTransfer; // TRUE = data were just set into the dialog, the posted CBN_SELCHANGE for combo Type must not change Alignment

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
    HWND HListView;                          // listview with columns
    CFTPParser* Parser;                      // parser of the listing being tested
    TIndirectArray<CSrvTypeColumn>* Columns; // tested column definition for parsing
    char** RawListing;                       // buffer for listing text (allocated string owned by the parent Edit Server Type dialog - the listing survives closing/reopening this dialog)
    int AllocatedSizeOfRawListing;           // currently allocated size of the *RawListing buffer
    BOOL* RawListIncomplete;                 // checkbox "act as if listing is incomplete" (owned by the parent Edit Server Type dialog - the setting survives closing/reopening this dialog)
    TDirectArray<DWORD> Offsets;             // pairs of offsets (start+end of the line that produced the item (file/directory) in the list-view
    int LastSelectedOffset;                  // index of the last selected offset pair (just an optimization for setting the edit boxes)

    HIMAGELIST SymbolsImageList; // image list for the listview

    // parameters for dialog layouting
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
    void InitColumns();            // add columns to the listview
    void SetColumnWidths();        // set optimal column widths
    void ParseListingToListView(); // parse *RawListing and put the results directly into the listview

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
    DWORD AttrAndMask;   // resulting attribute AND mask (clearing attributes)
    DWORD AttrOrMask;    // resulting attribute OR mask (enabling attributes)

public:
    CChangeAttrsDlg(HWND parent, const char* subject, DWORD attr, DWORD attrDiff,
                    BOOL selDirs);

    void RefreshNumValue(); // set the number according to the checkbox

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// COperationDlg
//

#define OPERDLG_UPDATEPERIOD 100 // shortest interval (in ms) between two refreshes of information in the operation dialog
#define OPERDLG_UPDATETIMER 1    // timer ID for delayed refresh of information in the dialog
#ifdef _DEBUG
#define OPERDLG_CHECKCOUNTERSTIMER 2    // timer ID for testing counters in the queue
#define OPERDLG_CHECKCOUNTERSPERIOD 300 // timer period for testing counters in the queue
#endif
#define OPERDLG_STATUSUPDATETIMER 3      // timer ID for delayed refresh of status/progress info in the dialog (too fast updates from workers (if more than one runs) are skipped)
#define OPERDLG_STATUSUPDATEPERIOD 1000  // timer period for refreshing status/progress info in the dialog (regular update, happens only when no worker updates are running)
#define OPERDLG_STATUSMINIDLETIME 950    // minimum time gap for the next update (periodic or from a worker) after the last worker update - WARNING: tied to WORKER_STATUSUPDATETIMEOUT
#define OPERDLG_GETDISKSPACEPERIOD 3000  // time after which free space on the target disk is fetched again
#define OPERDLG_SHOWERRMINIDLETIME 10000 // minimum idle time in the dialog before an error is shown automatically (see Config.OpenSolveErrIfIdle)
#define OPERDLG_AUTOSHOWERRTIMER 4       // timer ID for a periodic test whether a Show Error dialog should appear
#define OPERDLG_AUTOSHOWERRPERIOD 1000   // timer period for testing whether a Show Error dialog should appear
#define OPERDLG_AUTOSHOWERRTIMER2 5      // helper timer: ensures "immediate" delivery of OPERDLG_AUTOSHOWERRTIMER
#define OPERDLG_CORRECTBTNSTIMER 6       // timer ID for checking button states shortly after the dialog is activated (focus is unfortunately unknown at activation)

#define WM_APP_DISABLEDETAILED WM_APP + 1   // [0, 0] - disable the Detailed button after maximizing
#define WM_APP_ACTIVATEWORKERS WM_APP + 2   // [0, 0] - activate workers after opening the dialog
#define WM_APP_WORKERCHANGEREP WM_APP + 3   // [0, 0] - a worker change was reported, we must read from the operation where the change happened (see CFTPOperation::GetChangedWorker())
#define WM_APP_ITEMCHANGEREP WM_APP + 4     // [0, 0] - an item change was reported, we must read from the operation where the changes happened (see CFTPOperation::GetChangedItems())
#define WM_APP_OPERSTATECHANGE WM_APP + 5   // [0, 0] - the operation state changed (done/in progress/completed with errors), we must read the state from the operation (see CFTPOperation::GetOperationState())
#define WM_APP_HAVEDISKFREESPACE WM_APP + 6 // [0, 0] - the thread checking disk free space reports that it has a result
#define WM_APP_CLOSEDLG WM_APP + 7          // [0, 0] - the progress dialog should close (uses auto-close)

#define OPERDLG_CONSTEXTBUFSIZE 1000  // max text length in the Connections listview column
#define OPERDLG_ITEMSTEXTBUFSIZE 1000 // max text length in the Operations listview column

class CFTPQueue;
class CFTPWorkersList;
class COperationDlg;

class CGetDiskFreeSpaceThread : public CThread
{
protected:
    // critical section for accessing object data
    CRITICAL_SECTION GetFreeSpaceCritSect;

    char Path[MAX_PATH]; // path where we check free space
    CQuadWord FreeSpace; // detected free space; -1 = free space unknown
    HWND Dialog;         // handle of the operation dialog that should receive it

    HANDLE WorkOrTerminate; // "signaled" if the thread should check disk free space or terminate
    BOOL TerminateThread;   // TRUE = the thread should terminate

public:
    CGetDiskFreeSpaceThread(const char* path, HWND dialog);
    ~CGetDiskFreeSpaceThread();

    // call after the constructor; if it returns FALSE the object cannot be used further
    BOOL IsGood() { return WorkOrTerminate != NULL; }

    // the dialog schedules thread termination via this method
    void ScheduleTerminate();

    // the dialog schedules checking disk free space via this method; after the check it
    // receives WM_APP_HAVEDISKFREESPACE and can read the result
    void ScheduleGetDiskFreeSpace();

    // returns the last detected free space value
    CQuadWord GetResult();

    virtual unsigned Body();
};

class COperDlgListView : public CWindow
{
public:
    HWND HToolTip;
    COperationDlg* OperDlg; // dialog in which the listview exists
    BOOL ConsOrItems;       // TRUE/FALSE = take data from the Connections/Operations listview

    int LastItem;
    int LastSubItem;
    int LastWidth;

    BOOL Scrolling; // TRUE/FALSE = the user is currently using/not using the scrollbar

    DWORD LastLButtonDownTime;
    LPARAM LastLButtonDownLParam;

public:
    COperDlgListView();
    ~COperDlgListView();

    void Attach(HWND hListView, COperationDlg* operDlg, BOOL consOrItems);
    void HideToolTip(int onlyIfOnIndex = -1); // if 'onlyIfOnIndex' is not -1 it is the index being changed, so an open tooltip above it must be closed

protected:
    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class COperationDlg : public CDialog
{
public:
    CFTPOperation* Oper;               // operation for which the dialog is open (always exists longer than the dialog, so direct access is allowed instead of via operUID) (cannot be NULL)
    CFTPQueue* Queue;                  // queue of operation items (copy of Oper->Queue - the pointer to the queue does not change during the operation)
    CFTPWorkersList* WorkersList;      // list of operation workers (the pointer to the list does not change during the operation)
    BOOL* SendWMClose;                 // writing TRUE ensures WM_CLOSE is sent to this dialog (see COperationDlgThread::Body())
    BOOL CloseDlg;                     // TRUE = the dialog should close as soon as possible (used only when close is requested before the dialog opens)
    BOOL DlgWillCloseIfOpFinWithSkips; // TRUE = if the operation finishes with skipped items only, the operation window closes (and with it all workers) - used to decide which refresh to use (immediate or after activating the main window)

protected:
    HWND CenterToWnd;                    // window the dialog centers to when opened (NULL = no centering)
    HWND SizeBox;                        // size-box window
    CGUIStaticTextAbstract* Source;      // source path text (cannot be NULL)
    CGUIStaticTextAbstract* Target;      // target path text (cannot be NULL)
    CGUIStaticTextAbstract* TimeLeft;    // time left text (cannot be NULL)
    CGUIStaticTextAbstract* ElapsedTime; // elapsed time text (cannot be NULL)
    CGUIStaticTextAbstract* Status;      // status text (cannot be NULL)
    CGUIProgressBarAbstract* Progress;   // progress bar (cannot be NULL)
    DWORD ProgressValue;                 // last value set on the 'Progress' bar; -2 == progress is idle, showing "done", "errors", "stopped" or "paused"
    HWND ConsListView;                   // Connections listview
    COperDlgListView ConsListViewObj;    // Connections listview object (provides tooltip)
    HIMAGELIST ConsImageList;            // image list for the Connections listview
    HWND ItemsListView;                  // Operations listview
    COperDlgListView ItemsListViewObj;   // Operations listview object (provides tooltip)
    HIMAGELIST ItemsImageList;           // image list for the Operations listview

    char ConsTextBuf[3][OPERDLG_CONSTEXTBUFSIZE];   // buffers for LVN_GETDISPINFO text in the Connections listview
    int ConsActTextBuf;                             // which of the three buffers is currently free for LVN_GETDISPINFO in the Connections listview
    char ItemsTextBuf[3][OPERDLG_ITEMSTEXTBUFSIZE]; // buffers for LVN_GETDISPINFO text in the Operations listview
    int ItemsActTextBuf;                            // which of the three buffers is currently free for LVN_GETDISPINFO in the Operations listview

    BOOL SimpleLook; // TRUE/FALSE = simple (after the split bar) / detailed (complete) dialog look

    char* TitleText; // text for the dialog title (without the initial "(XX%) ")

    BOOL IsDirtyStatus;                 // TRUE = status/progress of the operation needs update (the dialog must redraw status/progress)
    BOOL IsDirtyProgress;               // TRUE = a worker changed; maybe (if the worker changed due to progress) status/progress needs update (the dialog must redraw status/progress)
    DWORD LastUpdateOfProgressByWorker; // GetTickCount at the last update of progress triggered by a worker (used to skip unnecessarily frequent updates with multiple workers + skip periodic updates)
    BOOL IsDirtyConsListView;           // TRUE = the Connections listview content changed (needs repaint in the dialog)
    BOOL IsDirtyItemsListView;          // TRUE = the Operations listview content changed (needs repaint in the dialog)
    BOOL HasDelayedUpdateTimer;         // TRUE = timer OPERDLG_UPDATETIMER is running

    int ConErrorIndex;           // Connections listview: index of the first error; -1 = no error exists
    BOOL EnableChangeFocusedCon; // TRUE = when focus changes in Connections activate the worker log (FALSE is used during listview refresh to avoid unwanted changes)

    BOOL ShowOnlyErrors;               // TRUE = the Operations listview contains only items in "wait for user" state (otherwise it contains all items)
    TDirectArray<DWORD> ErrorsIndexes; // Operations listview: array of indices of items in "wait for user" state in the Queue
    BOOL EnableShowOnlyErrors;         // TRUE = the "show only errors" checkbox should be enabled, otherwise disabled

    int FocusedItemUID;              // UID of the focused item in the Operations listview (-1 = unknown)
    BOOL EnableChangeFocusedItemUID; // TRUE = change FocusedItemUID when focus changes (FALSE is used during listview refresh to prevent unwanted changes to FocusedItemUID)

    BOOL UserWasActive;       // TRUE = prevent the window from closing automatically after the operation finishes successfully (the user was doing something and the dialog would vanish)
    BOOL DelayAfterCancel;    // TRUE = do not open another Solve Error dialog immediately after Cancel in the previous Solve Error dialog
    BOOL CloseDlgWhenOperFin; // FALSE = do not close the window after operation completion = the window may close only if Config.CloseOperationDlgIfSuccessfullyFinished==TRUE
    DWORD ClearChkboxTime;
    HWND LastFocusedControl; // last focused control in the dialog
    DWORD LastActivityTime;  // GetTickCount() from the time of the user's last activity

    DWORD LastTimeEstimation; // -1==invalid, otherwise rounded number of seconds until the operation finishes

    char* OperationsTextOrig;        // original text of the "Operations:" listview title
    int DisplayedDoneOrSkippedCount; // number of skipped+done items displayed after "Operations:" in the listview title (-1 = unknown)
    int DisplayedTotalCount;         // total number of items displayed after "Operations:" in the listview title (-1 = unknown)

    BOOL DisableAddWorkerButton; // TRUE = all items are "done", so adding more workers makes no sense

    BOOL ShowLowDiskWarning;       // TRUE = the "low disk space" icon+hint is shown, FALSE = the status stretches to the right edge and the icon with hint is hidden
    CQuadWord LastNeededDiskSpace; // value of the required space on the target disk (from the last scheduling of the disk free space check)
    DWORD LastGetDiskFreeSpace;    // GetTickCount() from the last scheduling of the disk free space check (in the GetDiskFreeSpaceThread)
    CGUIHyperLinkAbstract* LowDiskSpaceHint;
    CGetDiskFreeSpaceThread* GetDiskFreeSpaceThread;

    HWND CurrentFlashWnd; // if != NULL, window that should flash when the title changes (call FlashWindow; the flash is lost when the title changes so we must restore it)

    // layout parameters (suffix "1" = "simple", suffix "2" = "detailed")
    int MinDlgHeight1;  // minimum dialog height in simple look
    int MinDlgHeight2;  // minimum dialog height in detailed look
    int MinDlgWidth;    // minimum dialog width
    int LastDlgHeight1; // height of the last shown "detailed" variant
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

    int ConnectionsActWidth;       // current width of the Connections listview
    int ConnectionsActHeight;      // current height of the Connections listview
    int ConsAddActYOffset;         // current y-offset of the Add button below the Connections listview
    int ConnectionsActHeightLimit; // current height limit for the Connections listview
    BOOL InListViewSplit;          // TRUE = the mouse is in the area where dragging changes the Connections listview height against the Operations listview
    BOOL Captured;                 // TRUE = the mouse is captured by us
    int DragOriginY;               // Y coordinate of the left-button press (start of the drag)
    double ListviewSplitRatio;     // ratio between the Connections listview height and the total height for the listviews

    BOOL RestoreToMaximized; // TRUE if the user minimized while maximized -> restore performs maximize

    BOOL PauseButtonIsEnabled;        // Pause/Resume button above the Connections listview: TRUE = enabled, FALSE = disabled
    BOOL PauseButtonIsResume;         // current text of the Pause/Resume button above the Connections listview: TRUE = Resume, FALSE = Pause
    char PauseButtonPauseText[50];    // buffer for the "Pause" text from dialog resources (the "Resume" text is IDS_OPERDLGRESUMEBUTTON)
    BOOL ConPauseButtonIsResume;      // current text of the Pause/Resume button below the Connections listview: TRUE = Resume, FALSE = Pause
    char ConPauseButtonPauseText[50]; // buffer for the "Pause" text from dialog resources (the "Resume" text is IDS_OPERDLGRESUMECONBUTTON)

public:
    COperationDlg(HWND parent, HWND centerToWnd, CFTPOperation* oper, CFTPQueue* queue,
                  CFTPWorkersList* workersList);
    ~COperationDlg();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LayoutDialog(BOOL showSizeBox);

    // changes ShowLowDiskWarning and hides/shows the hint+icon (but does not perform layout)
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
    void RefreshConnections(BOOL init, int newFocusIndex = -1, int refreshOnlyIndex = -1); // 'init' is TRUE only when called from WM_INITDIALOG
    void RefreshItems(BOOL init, int refreshOnlyIndex1 = -1, int refreshOnlyIndex2 = -1);  // 'init' is TRUE only when called from WM_INITDIALOG
    void EnableErrorsButton();
    void EnablePauseButton();
    void EnableSolveConError(int index);  // if 'index' is -1, it uses the focus index from the Connections listview
    void EnableRetryItem(int index);      // if 'index' is -1, it uses the focus index from the Operations listview
    void EnablePauseConButton(int index); // if 'index' is -1, it uses the focus index from the Connections listview

    // ensures data update (redraw) in the dialog from the dialog's internal variables
    // (calls UpdateDataInDialog()); the shortest interval between two consecutive
    // updates is OPERDLG_UPDATEPERIOD milliseconds
    void ScheduleDelayedUpdate();

    // draws (shows) new values of all changed data displayed in the dialog
    // (used for delayed data refresh); returns TRUE if
    // something needed updating
    BOOL UpdateDataInDialog();

    // ensures setting the close button (Close/Cancel); 'flashTitle' is TRUE
    // when called from WM_APP_OPERSTATECHANGE handling (operation reports the change)
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

    BOOL DontTransferName; // TRUE = the name should not be obtained from the dialog (no validation or transfer)

    int UsedButtonID; // ID of the button the user used to close the dialog (used in Transfer())

    CSolveItemErrorDlgType DlgType; // type of dialog being shown

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
    int UsedButtonID; // ID of the button the user used to close the dialog (used in Transfer())

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
    int TitleID; // if not equal to -1, resource ID of the dialog title

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
    sisdtNone,           // only an initial value
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
    int UsedButtonID; // ID of the button the user used to close the dialog (used in Transfer())

    CSolveItemErrorSimpleDlgType DlgType; // type of dialog being shown

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
    siscdtSimple,     // no menu on the Retry button
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

    CSolveItemErrorSrvCmdDlgType DlgType; // type of dialog being shown

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
    siscdt2Simple,               // no menu on the Retry button
    siscdt2ResumeFile,           // Copy/Move: unable to resume file (menu offers overwrite/resume/etc.)
    siscdt2ResumeTestFailed,     // Copy/Move: resume test failed (menu offers reduce-file/overwrite/resume/etc.)
    siscdt2UploadUnableToStore,  // upload: unable to store file to server + resume not supported in ASCII transfer mode + unable to resume (server does not support resuming + target file size is unknown + target file is larger than source)
    siscdt2UploadTestIfFinished, // upload: cannot verify whether the file uploaded successfully
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

    CSolveItemErrorSrvCmdDlgType2 DlgType; // type of dialog being shown

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

// helper object for the Script edit control (custom context menu + does not
// select-all when the control gets focus)
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
    CFTPProxyServerList* TmpFTPProxyServerList; // proxy server list from the parent dialog
    CFTPProxyServer* Proxy;                     // proxy server data (write allowed only via CFTPProxyServerList::SetProxyServer(), reading without a section because writing happens only in the main thread we are currently in)
    BOOL Edit;                                  // TRUE = editing, FALSE = adding

public:
    CProxyServerDlg(HWND parent, CFTPProxyServerList* tmpFTPProxyServerList,
                    CFTPProxyServer* proxy, BOOL edit);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    // if 'initScriptText' is TRUE it sets the Script edit box contents, the "read-only flag"
    // is always adjusted; if 'initProxyPort' is TRUE it sets the default proxy server port;
    // afterwards it enables other controls in the dialog after changing the proxy server type
    void EnableControls(BOOL initScriptText, BOOL initProxyPort);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void AlignPasswordControls();
    void ShowHidePasswordControls(BOOL lockedPassword, BOOL focusEdit);
};

//
// ****************************************************************************
// CPasswordEditLine
//
// subclass for an edit line containing a password; on ctrl+right-click it posts
// command WM_APP_SHOWPASSWORD to the parent

#define WM_APP_SHOWPASSWORD WM_APP + 50 // [hWnd, lParam] - user ctrl+right-clicked in the edit line, hWnd is the edit window handle, lParam is the click position, see WM_RBUTTONDOWN/lParam

class CPasswordEditLine : public CWindow
{
public:
    CPasswordEditLine(HWND hDlg, int ctrlID);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
