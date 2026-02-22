// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define HOT_PATHS_COUNT 30

#define TASKBAR_ICON_ID 0x0000

extern const int SPLIT_LINE_WIDTH;
extern const int MIN_WIN_WIDTH;

struct CCommandLineParams;

// if the user disallows multiple instances, just activate the previous one
BOOL CheckOnlyOneInstance(const CCommandLineParams* cmdLineParams);

// sends the WM_USER_CFGCHANGED message to open internal viewer and find windows
void BroadcastConfigChanged();

// universal callback for message boxes
void CALLBACK MessageBoxHelpCallback(LPHELPINFO helpInfo);

//
// ****************************************************************************

class CEditWindow;
class CFilesWindow;
struct CUserMenuItem;
class CUserMenuItems;
class CViewerMasks;
class CEditorMasks;
class CHighlightMasks;
class CMainToolBar;
class CPluginsBar;
class CBottomToolBar;
class CUserMenuBar;
class CHotPathsBar;
class CDriveBar;
class CMenuPopup;
class CMenuBar;
class CMenuNew;
class CToolTip;
class CAnimate;

//****************************************************************************
//
// CToolTipWindow
//

class CToolTipWindow : public CWindow
{ // displays the tooltip independently of the cursor position
public:
    CToolTipWindow() : CWindow(ooStatic) { ToolWindow = NULL; }

    void SetToolWindow(HWND toolWindow) { ToolWindow = toolWindow; }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND ToolWindow; // TTM_WINDOWFROMPOINT always returns this window
};

//****************************************************************************
//
// CHotPathItem
//

// used in the configuration dialog and specifies the maximum allowed length of CHotPathItem::Path
// we keep some margin because the path may contain long variables that will "shrink" to just a few characters after expansion,
// for example $[SystemDrive] -> C:
#define HOTPATHITEM_MAXPATH (4 * MAX_PATH)

struct CHotPathItem
{
    // Name and Path are allocated to save memory (people expect unlimited hot paths)
    // moreover, in the case of Path, MAX_PATH would be too small (escaping + variables)
    char* Name;   // name under which the path appears in the menu
    char* Path;   // path escaped (double '$' characters) for variables like $(SalDir), etc.
    BOOL Visible; // is the path present in the ChangeDrive menu

    CHotPathItem()
    {
        Name = NULL;
        Path = NULL;
        Visible = TRUE;
    }

    void CopyFrom(const CHotPathItem* src)
    {
        Empty();
        Name = DupStr(src->Name);
        Path = DupStr(src->Path);
        Visible = src->Visible;
    }

    void Empty()
    {
        if (Name != NULL)
        {
            free(Name);
            Name = NULL;
        }
        if (Path != NULL)
        {
            free(Path);
            Path = NULL;
        }
        Visible = TRUE;
    }
};

//****************************************************************************
//
// CHotPathItems
//

class CHotPathItems
{
protected:
    CHotPathItem Items[HOT_PATHS_COUNT];

public:
    ~CHotPathItems()
    {
        Empty();
    }

    // deallocates held data
    void Empty()
    {
        int i;
        for (i = 0; i < HOT_PATHS_COUNT; i++)
            Items[i].Empty();
    }

    // sets the attributes
    void Set(DWORD index, const char* name, const char* path)
    {
        if (Items[index].Name != NULL)
            free(Items[index].Name);
        if (Items[index].Path != NULL)
            free(Items[index].Path);
        Items[index].Name = DupStr(name);
        Items[index].Path = DupStr(path);
    }

    void Set(DWORD index, const char* name, const char* path, BOOL visible)
    {
        Set(index, name, path);
        Items[index].Visible = visible;
    }

    void SetPath(DWORD index, const char* path)
    {
        if (Items[index].Path != NULL)
            free(Items[index].Path);
        Items[index].Path = DupStr(path);
    }

    void SetVisible(DWORD index, BOOL visible)
    {
        Items[index].Visible = visible;
    }

    void GetName(int index, char* buffer, int bufferSize)
    {
        if (index < 0 || index >= HOT_PATHS_COUNT || Items[index].Name == NULL)
        {
            if (bufferSize > 0)
                *buffer = 0;
        }
        else
        {
            if (bufferSize > 0)
                lstrcpyn(buffer, Items[index].Name, bufferSize);
        }
    }

    void GetPath(int index, char* buffer, int bufferSize)
    {
        if (index < 0 || index >= HOT_PATHS_COUNT || Items[index].Path == NULL)
        {
            if (bufferSize > 0)
                *buffer = 0;
        }
        else
        {
            if (bufferSize > 0)
                lstrcpyn(buffer, Items[index].Path, bufferSize);
        }
    }

    int GetNameLen(int index)
    {
        if (index >= 0 && index < HOT_PATHS_COUNT && Items[index].Name != NULL)
            return lstrlen(Items[index].Name);
        else
            return 0;
    }

    int GetPathLen(int index)
    {
        if (index >= 0 && index < HOT_PATHS_COUNT && Items[index].Path != NULL)
            return lstrlen(Items[index].Path);
        else
            return 0;
    }

    // returns the index of an unassigned hot path or -1 if all are assigned
    int GetUnassignedHotPathIndex();

    BOOL GetVisible(int index) { return Items[index].Visible; }
    BOOL CleanName(char* name); // trims spaces and returns TRUE if the name is valid

    BOOL SwapItems(int index1, int index2); // swaps two items in the array

    // 'emptyItems' - also add unassigned paths (for assignment)
    // 'emptyEcho' - if the menu is empty, it outputs information that it is empty
    // 'customize' - append a separator and customize option
    // 'topSeparator' - if it inserts some path, it puts a separator above it
    // 'forAssign' - called from Assign Hot Path in the directory line context menu
    void FillHotPathsMenu(CMenuPopup* menu, int minCommand, BOOL emptyItems = FALSE, BOOL emptyEcho = TRUE,
                          BOOL customize = TRUE, BOOL topSeparator = FALSE, BOOL forAssign = FALSE);

    BOOL Save(HKEY hKey);     // saves the entire array
    BOOL Load(HKEY hKey);     // loads the entire array
    BOOL Load1_52(HKEY hKey); // loads the entire array from version 1.52

    void Load(CHotPathItems& source)
    {
        int i;
        for (i = 0; i < HOT_PATHS_COUNT; i++)
            Items[i].CopyFrom(&source.Items[i]);
    }
};

//*****************************************************************************
//
// UM_GetNextFileName - function type that gradually returns file names for U.M.
//
// index - order of the next name (starting from zero and increasing by one)
// path  - buffer for the path [MAX_PATH]
// name  - buffer for the file name [MAX_PATH]
// param - helper pointer for user data
//
// returns success - continue retrieving more names? (returns FALSE - ends the enumeration)

typedef BOOL (*UM_GetNextFileName)(int index, char* path, char* name, void* param);

struct CUMDataFromPanel
{
    CFilesWindow* Window;
    int* Index;
    int Count;

    CUMDataFromPanel(CFilesWindow* window)
    {
        Count = -1;
        Index = NULL;
        Window = window;
    }
    ~CUMDataFromPanel()
    {
        if (Index != NULL)
            delete[] (Index);
    }
};

BOOL GetNextFileFromPanel(int index, char* path, char* name, void* param);

struct CUserMenuAdvancedData;
struct IContextMenu2;

class CMainWindowAncestor : public CWindow
{
private:
    CFilesWindow* ActivePanel; // either LeftPanel or RightPanel

public:
    // NOTE: call this only when the request cannot be handled via FocusPanel
    // and ChangePanel (useful when EditMode is TRUE). When calling, verify that
    // the panel is wide enough and so on (see FocusPanel)
    void SetActivePanel(CFilesWindow* active)
    {
        ActivePanel = active;
    }
    CFilesWindow* GetActivePanel()
    {
        return ActivePanel;
    }
};

//class CTipOfTheDayDialog;
class CDetachedFSList;
class CPathHistory;

enum CMainWindowsHitTestEnum
{
    mwhteNone,
    mwhteTopRebar,      // it's in the rebar but on no band
    mwhteMenu,          // in the rebar on the Menu band
    mwhteTopToolbar,    // in the rebar on the TopToolbar band
    mwhtePluginsBar,    // in the rebar on the PluginsBar band
    mwhteMiddleToolbar, // Middle Bar on the split bar
    mwhteUMToolbar,     // in the rebar on the UserMenuBar band
    mwhteHPToolbar,     // in the rebar on the HotPathsBar band
    mwhteDriveBar,      // in the rebar on the Drive Bar band
    mwhteWorker,        // in the rebar on the Worker band
    mwhteCmdLine,
    mwhteBottomToolbar,
    mwhteSplitLine,
    mwhteLeftDirLine,
    mwhteLeftHeaderLine,
    mwhteLeftStatusLine,
    mwhteLeftWorkingArea,
    mwhteRightDirLine,
    mwhteRightHeaderLine,
    mwhteRightStatusLine,
    mwhteRightWorkingArea,
};

struct CChangeNotifData
{
    char Path[MAX_PATH];
    BOOL IncludingSubdirs;
};

typedef TDirectArray<CChangeNotifData> CChangeNotifArray;

//
// ****************************************************************************
// CDynString
//
// helper object for a dynamically allocated string

struct CDynString
{
    char* Buffer;
    int Length;
    int Allocated;

    CDynString()
    {
        Buffer = NULL;
        Length = 0;
        Allocated = 0;
    }

    ~CDynString()
    {
        if (Buffer != NULL)
            free(Buffer);
    }

    BOOL Append(const char* str, int len); // returns TRUE on success; if 'len' is -1 the length is calculated using "len = strlen(str)"

    const char* GetString() const { return Buffer; }
};

// flags for CompareDirectories methods
#define COMPARE_DIRECTORIES_BYTIME 0x00000001       // compare by date and time
#define COMPARE_DIRECTORIES_BYCONTENT 0x00000002    // compare file contents
#define COMPARE_DIRECTORIES_BYATTR 0x00000004       // compare attributes
#define COMPARE_DIRECTORIES_SUBDIRS 0x00000008      // include subdirectories
#define COMPARE_DIRECTORIES_SUBDIRS_ATTR 0x00000010 // subdirectory attributes
#define COMPARE_DIRECTORIES_BYSIZE 0x00000020       // compare file sizes
#define COMPARE_DIRECTORIES_ONEPANELDIRS 0x00000040 // mark directories that exist only in one panel (does not check subdirectories)
#define COMPARE_DIRECTORIES_IGNFILENAMES 0x00000080 // ignore file names matching Configuration.CompareIgnoreFilesMasks
#define COMPARE_DIRECTORIES_IGNDIRNAMES 0x00000100  // ignore directory names matching Configuration.CompareIgnoreDirsMasks

class CMainWindow : public CMainWindowAncestor
{
public:
    BOOL EditMode;             // the edit window is active, the rest just simulates
    BOOL EditPermanentVisible; // the edit window is always visible
    BOOL HelpMode;             // if TRUE, then Shift+F1 help mode is active

    CFilesWindow *LeftPanel,
        *RightPanel;
    CEditWindow* EditWindow;
    CMainToolBar* TopToolBar;
    CPluginsBar* PluginsBar;
    CMainToolBar* MiddleToolBar;
    CUserMenuBar* UMToolBar;
    CHotPathsBar* HPToolBar;
    CDriveBar* DriveBar;
    CDriveBar* DriveBar2;
    CBottomToolBar* BottomToolBar;
    //CAnimate       *AnimateBar;

    HWND HTopRebar;
    CMenuBar* MenuBar;
    UINT TaskbarRestartMsg; // sent by Explorer when taskbar icons need to be restored

    CToolTip* ToolTip; // always exists and is created-all controls use this single tooltip

    BOOL Created;

    CHotPathItems HotPaths;
    CViewTemplates ViewTemplates;

    CUserMenuItems* UserMenuItems;
    CViewerMasks* ViewerMasks;
    CRITICAL_SECTION ViewerMasksCS; // section used only for synchronizing access to 'ViewerMasks' (writes anywhere and reads outside the main thread)
    CViewerMasks* AltViewerMasks;
    CEditorMasks* EditorMasks;
    CHighlightMasks* HighlightMasks;

    CDetachedFSList* DetachedFSList; // list of "detached" FS for Alt+F1/F2 (for reattachment)

    CFileHistory* FileHistory; // history of files that were viewed or edited

    CPathHistory* DirHistory; // history of visited directories
    BOOL CanAddToDirHistory;  // TRUE only after Salamander starts (to avoid registering path changes while loading the configuration)

    CMenuNew* ContextMenuNew;          // handles commands from the New menu
    IContextMenu2* ContextMenuChngDrv; // handles commands from the Change Drive Menu

    char SelectionMask[MAX_PATH]; // mask for select/deselect

    BOOL CanClose;                    // can the main window be closed? (has the application fully started?)
    BOOL CanCloseButInEndSuspendMode; // TRUE if CanClose was TRUE but is temporarily FALSE because a message loop is running while processing WM_USER_END_SUSPMODE
    BOOL SaveCfgInEndSession;         // TRUE = configuration should be saved in WM_ENDSESSION
    BOOL WaitInEndSession;            // TRUE = WM_ENDSESSION should wait for disk operations to finish
    BOOL DisableIdleProcessing;       // TRUE = skip idle processing (the app is shutting down and it would only slow things down)
                                      //    CTipOfTheDayDialog *TipOfTheDayDialog;

    BOOL DragMode;
    int DragSplitX;

    // this function is disabled for now-the implementation would require modifying the displayed menu
    //    HWND           DrivesControlHWnd;   // handle of the Alt+F1/F2 window if displayed, otherwise NULL

    HWND HDisabledKeyboard; // handle of the window that disabled keyboard processing
                            // if the Escape key is pressed this window
                            // receives the WM_CANCELMODE message

    int CmdShow; // value received in WinMain

    int ActivateSuspMode; // counter of WM_ACTIVATEAPP activations/deactivations; some messages may get lost

    RECT WindowRect; // current window position

    BOOL CaptionIsActive; // is the main window caption active?

    // variables related to sending notifications about path changes (both FS and disk paths)
    CChangeNotifArray ChangeNotifArray;    // array of messages about path changes
    CRITICAL_SECTION DispachChangeNotifCS; // critical section for work with ChangeNotifArray
    int LastDispachChangeNotifTime;        // time of the last message dispatch
    BOOL NeedToResentDispachChangeNotif;   // TRUE = WM_USER_DISPACHCHANGENOTIF must be posted again

    BOOL DoNotLoadAnyPlugins; // TRUE = do not load any plugins (e.g. thumbnail loaders); WARNING: modify via SetDoNotLoadAnyPlugins()

    DWORD SHChangeNotifyRegisterID; // returned by SHChangeNotifyRegister
    BOOL IgnoreWM_SETTINGCHANGE;

    BOOL LockedUI;
    HWND LockedUIToolWnd;
    char* LockedUIReason;

    CITaskBarList3 TaskBarList3; // controls progress on the taskbar since Windows 7

protected:
    int WindowWidth, // due to split change
        WindowHeight,
        TopRebarHeight,
        BottomToolBarHeight,
        EditHeight,
        PanelsHeight,
        SplitPositionPix,
        DragAnchorX;
    double SplitPosition,        // current split position (0..1)
        BeforeZoomSplitPosition, // split position before panel zoom
        DragSplitPosition;       // shown in the tooltip
    CToolTipWindow ToolTipWindow;

    BOOL FirstActivateApp; // WM_ACTIVATEAPP uses this variable during startup

    BOOL IdleStatesChanged;    // set by the CheckAndSet() method
    BOOL PluginsStatesChanged; // the plugin bar needs to be rebuilt

public:
    CMainWindow();
    ~CMainWindow();

    void EnterViewerMasksCS() { HANDLES(EnterCriticalSection(&ViewerMasksCS)); }
    void LeaveViewerMasksCS() { HANDLES(LeaveCriticalSection(&ViewerMasksCS)); }
    BOOL GetViewersAssoc(int wantedViewerType, CDynString* strViewerMasks); // helper: collects all masks associated with the given viewer type "wantedViewerType"; returns TRUE on success (when enough memory for the string)

    void ClearHistory(); // clears all histories

    void GetSplitRect(RECT& r);

    BOOL IsGood();

    // sends information about a change on the path 'path' (if 'includingSubdirs' is TRUE,
    // changes may occur in subdirectories as well); the information is distributed to panels
    // and to all opened FS from plugins (both panels and FS can respond by refreshing their content);
    // can be called from any thread
    void PostChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // these functions have no effect if CFilesWindow::CanBeFocused is not satisfied
    void ChangePanel(BOOL force = FALSE);                                   // respects EditMode; activates the inactive panel; (ignores ZOOM if force is TRUE)
    void FocusPanel(CFilesWindow* focus, BOOL testIfMainWndActive = FALSE); // clears EditMode because focus is put into the panel
    void FocusLeftPanel();                                                  // calls FocusPanel for the left panel

    // compares directories in the left and right panels
    void CompareDirectories(DWORD flags); // flags are a combination of COMPARE_DIRECTORIES_xxx

    // ensures DirHistory->AddPathUnique is called and correctly updates the panel's SetHistory
    void DirHistoryAddPathUnique(int type, const char* pathOrArchiveOrFSName,
                                 const char* archivePathOrFSUserPart, HICON hIcon,
                                 CPluginFSInterfaceAbstract* pluginFS,
                                 CPluginFSInterfaceEncapsulation* curPluginFS);

    // ensures DirHistory->RemoveActualPath is called and correctly updates the panel's SetHistory
    void DirHistoryRemoveActualPath(CFilesWindow* panel);

    // returns TRUE if the detached FS was successfully closed (calls TryCloseOrDetach, ReleaseObject, then CloseFS)
    // if the plugin does not want to close the FS, asks the user whether to close forcibly (as if canForce==TRUE)
    BOOL CloseDetachedFS(HWND parent, CPluginFSInterfaceEncapsulation* detachedFS);

    // returns TRUE if the plugin is no longer used by Salamander -> it can be unloaded
    BOOL CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin);

    // called when closing a file system; the directory history stores FS
    // interfaces that must be set to NULL after closing (to prevent accidental match just because FS interfaces were allocated at the same address)
    void ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs);

    void SaveConfig(HWND parent = NULL); // parent: NULL = MainWindow->HWindow
    BOOL LoadConfig(BOOL importingOldConfig, const CCommandLineParams* cmdLineParams);
    void SavePanelConfig(CFilesWindow* panel, HKEY hSalamander, const char* reg);
    void LoadPanelConfig(char* panelPath, CFilesWindow* panel, HKEY hSalamander, const char* reg);
    void DeleteOldConfigurations(BOOL* deleteConfigurations, BOOL autoImportConfig,
                                 const char* autoImportConfigFromKey, BOOL doNotDeleteImportedCfg);

    void UserMenu(HWND parent, int itemIndex, UM_GetNextFileName getNextFile, void* data,
                  CUserMenuAdvancedData* userMenuAdvancedData);

    // sets the hot path 'path' with index 'index'; receives a valid path without doubled '$' or variables
    void SetUnescapedHotPath(int index, const char* path);

    // expands the hot path with index 'index' into 'buffer' of size 'bufferSize'
    // 'hParent' -- errors during path expansion will be shown for this window; if NULL, errors are suppressed
    // returns TRUE if the path was successfully obtained, otherwise FALSE
    BOOL GetExpandedHotPath(HWND hParent, int index, char* buffer, int bufferSize);

    // returns the index of an unassigned hot path or -1 if all are assigned
    int GetUnassignedHotPathIndex();

    void SetFont();
    void SetEnvFont();

    void RefreshDiskFreeSpace();
    void RefreshDirs();

    // restores DefaultDir based on the panel paths; if 'activePrefered' is TRUE the
    // active panel path is preferred (and later written to DefaultDir), otherwise
    // the non-active panel path has priority
    void UpdateDefaultDir(BOOL activePrefered);
    void SetDefaultDirectories(const char* curPath = NULL);

    HWND GetActivePanelHWND();
    int GetDirectoryLineHeight();

    CFilesWindow* GetOtherPanel(CFilesWindow* panel)
    {
        return panel == LeftPanel ? RightPanel : LeftPanel;
    }

    BOOL EditWindowKnowHWND(HWND hwnd);
    void EditWindowSetDirectory(); // sets the text before the command line and enables/disables it at the same time
    HWND GetEditLineHWND(BOOL disableSkip = FALSE);

    // returns TRUE if the key was handled
    BOOL HandleCtrlLetter(char c); // Ctrl+letter hotkeys

    void LayoutWindows();
    BOOL ToggleTopToolBar(BOOL storePos = TRUE);
    BOOL TogglePluginsBar(BOOL storePos = TRUE);
    BOOL ToggleMiddleToolBar();
    BOOL ToggleBottomToolBar();
    BOOL ToggleUserMenuToolBar(BOOL storePos = TRUE);
    BOOL ToggleHotPathsBar(BOOL storePos = TRUE);
    // If 'twoDriveBars' is TRUE, the user wants two drive lists; otherwise only one
    BOOL ToggleDriveBar(BOOL twoDriveBars, BOOL storePos = TRUE);

    void ToggleToolBarGrips();

    BOOL InsertMenuBand();
    BOOL CreateAndInsertWorkerBand();
    BOOL InsertTopToolbarBand();
    BOOL InsertPluginsBarBand();
    BOOL InsertUMToolbarBand();
    BOOL InsertHPToolbarBand();
    BOOL InsertDriveBarBand(BOOL twoDriveBars);
    void StoreBandsPos();

    //    void ReleaseMenuNew();

    void UpdateBottomToolBar();

    void AddTrayIcon(BOOL updateIcon = FALSE);
    void RemoveTrayIcon();
    void SetTrayIconText(const char* text);

    // fills the menu with UserMenuItems items
    // customize specifies whether the configuration option should be added
    void FillUserMenu(CMenuPopup* menu, BOOL customize = TRUE);
    // internal recursive function used for filling
    void FillUserMenu2(CMenuPopup* menu, int* iterator, int max);

    // fills the View menu with modes
    // 'popup' is the popup we are going to be filling
    // 'firstIndex' index in 'popup' from which the items should be inserted
    // 'type' 0=TopToolbar||MiddleToolBar, 1=LeftMenu/LeftToolbar, 2=RightMenu/RightToolbar
    void FillViewModeMenu(CMenuPopup* popup, int firstIndex, int type);

    void MakeFileList();

    // helper method for SetTitle; 'text' must be at least 2 * MAX_PATH characters long
    void GetFormatedPathForTitle(char* text);

    // if 'text' == NULL the default content will be set
    void SetWindowTitle(const char* text = NULL);

    // sets the main window icon according to MainWindowIconIndex
    void SetWindowIcon();

    void ShowCommandLine();
    void HideCommandLine(BOOL storeContent = FALSE, BOOL focusPanel = TRUE);

    void GetWindowSplitRect(RECT& r);
    BOOL PtInChild(HWND hChild, POINT p);
    void OnWmContextMenu(HWND hWnd, int xPos, int yPos);

    CFilesWindow* GetNonActivePanel()
    {
        return (GetActivePanel() == LeftPanel) ? RightPanel : LeftPanel;
    }

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void SafeHandleMenuNewMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);

    void OnEnterIdle();
    void RefreshCommandStates();
    inline void CheckAndSet(DWORD* target, DWORD source)
    {
        if (*target != source)
        {
            *target = source;
            IdleStatesChanged = TRUE;
        }
    }

    // The color palette or color depth changed; new image lists for toolbars
    // have already been created and must be assigned to the controls that use them.
    // reloadUMIcons determines whether the UserMenu icons are reloaded. WARNING:
    // if the items are on a network drive, this can be extremely slow
    // (for example about 1500 ms per icon).
    void OnColorsChanged(BOOL reloadUMIcons);

    // Notifies the main window that plugins have changed (loading, adding or
    // removing plugins, or any action that modifies the Plugin Bar). The method
    // only marks the variable; the toolbar is rebuilt later in the idle handler.
    void OnPluginsStateChanged()
    {
        PluginsStatesChanged = TRUE;
    }

    // Context Help (Shift+F1) support
    BOOL CanEnterHelpMode();
    void OnContextHelp();
    HWND SetHelpCapture(POINT point, BOOL* pbDescendant);
    BOOL ProcessHelpMsg(MSG& msg, DWORD* pContext, HWND* hDirtyWindow); // hDirtyWindow: returns the window to which we sent WM_USER_HELP_MOUSEMOVE and that needs to receive WM_USER_HELP_MOUSELEAVE
    void ExitHelpMode();
    DWORD MapClientArea(POINT point);
    DWORD MapNonClientArea(int iHit);

    CMainWindowsHitTestEnum HitTest(int xPos, int yPos); // screen coordinates

    // Presses drive bar buttons according to the panel paths
    void UpdateDriveBars();

    // if the drive bar has a different disk bitmask than 'drivesMask' or if 'checkCloudStorages' is TRUE
    // and the drive bar has a different cloud bitmask than 'cloudStoragesMask', it will be regenerated
    void RebuildDriveBarsIfNeeded(BOOL useDrivesMask, DWORD drivesMask, BOOL checkCloudStorages,
                                  DWORD cloudStoragesMask);

    // it sets DoNotLoadAnyPlugins and when it is FALSE, it sends refreshes to panels loading thumbnails
    void SetDoNotLoadAnyPlugins(BOOL doNotLoad);

    // based on 'show', shows or hides two drive bars
    void ShowHideTwoDriveBarsInternal(BOOL show);

    // returns the width of the split bar in pixels (expanded if the Middle Bar is visible)
    int GetSplitBarWidth();

    void StartAnimate();
    void StopAnimate();

    void CancelPanelsUI();          // cancels any QuickSearch or QuickRename
    BOOL QuickRenameWindowActive(); // returns TRUE if QuickRenameWindow is active in any panel

    // rename according to QuickRenameWindow; returns TRUE on success or when no renaming is in progress
    // if it returns FALSE, do not close the window (do not call CancelPanelsUI) and do not change focus (focus is in the edit line)
    BOOL DoQuickRename();

    // returns TRUE if a panel is maximized at the expense of the other panel
    // and the Zoom command would set both panels to a 50/50 ratio
    BOOL IsPanelZoomed(BOOL leftPanel);

    // toggles Smart Column mode for the given panel
    void ToggleSmartColumnMode(CFilesWindow* panel);
    // returns Smart Column mode (TRUE/FALSE) for the given panel
    BOOL GetSmartColumnMode(CFilesWindow* panel);

    // attach or detach the main window from shell change notifications (adding/removing drives)
    BOOL SHChangeNotifyInitialize();
    BOOL SHChangeNotifyRelease();
    BOOL OnAssociationsChangedNotification(BOOL showWaitWnd);

    void SafeHandleMenuChngDrvMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);

    void ApplyCommandLineParams(const CCommandLineParams* params, BOOL setActivePanelAndPanelPaths = TRUE);

    void LockUI(BOOL lock, HWND hToolWnd, const char* lockReason);
    BOOL HasLockedUI() { return LockedUI; }
    void BringLockedUIToolWnd();

    CFilesWindow* GetPanel(int panel);
    void PostFocusNameInPanel(int panel, const char* path, const char* name);

    friend void CMainWindow_RefreshCommandStates(CMainWindow* obj);
};

//
// ****************************************************************************
// C__MainWindowCS
//
// ensures access to the MainWindow variable outside the main thread, usage:
// if (MainWindowCS.LockIfNotClosed())
// {
//
//   we can work with MainWindow here (no blocking actions-preferably just grab the data and leave)...
//
//   MainWindowCS.Unlock();
// }

class C__MainWindowCS
{
protected:
    CRITICAL_SECTION cs;
    BOOL IsClosed;

public:
    C__MainWindowCS()
    {
        HANDLES(InitializeCriticalSection(&cs));
        IsClosed = FALSE;
    }
    ~C__MainWindowCS() { HANDLES(DeleteCriticalSection(&cs)); }

    void SetClosed()
    {
        HANDLES(EnterCriticalSection(&cs));
        IsClosed = TRUE;
        HANDLES(LeaveCriticalSection(&cs));
    }

    BOOL LockIfNotClosed()
    {
        HANDLES(EnterCriticalSection(&cs));
        if (!IsClosed)
            return TRUE;
        HANDLES(LeaveCriticalSection(&cs));
        return FALSE;
    }

    void Unlock() { HANDLES(LeaveCriticalSection(&cs)); }
};

extern C__MainWindowCS MainWindowCS;

//
// ****************************************************************************

// Protection against ShellExtensions that try to destroy MainWindow via DestroyWindow
// if CanDestroyMainWindow==FALSE and MainWindow receives WM_DESTROY, the reporting mechanism starts.
extern BOOL CanDestroyMainWindow;

extern CMainWindow* MainWindow;
