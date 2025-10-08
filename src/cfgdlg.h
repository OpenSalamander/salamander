// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

class CColorArrowButton;

//****************************************************************************
//
// CHighlightMasksItem
//

struct CHighlightMasksItem
{
    CMaskGroup* Masks;
    DWORD Attr;      // 1:include; 0:exclude
    DWORD ValidAttr; // bits in Attr that are valid =1; set to 0 when they are ignored

    SALCOLOR NormalFg; // colors in individual modes
    SALCOLOR NormalBk;
    SALCOLOR FocusedFg;
    SALCOLOR FocusedBk;
    SALCOLOR SelectedFg;
    SALCOLOR SelectedBk;
    SALCOLOR FocSelFg;
    SALCOLOR FocSelBk;
    SALCOLOR HighlightFg;
    SALCOLOR HighlightBk;

    CHighlightMasksItem();
    CHighlightMasksItem(CHighlightMasksItem& item);
    ~CHighlightMasksItem();

    BOOL Set(const char* masks);
    BOOL IsGood();
};

//****************************************************************************
//
// CHighlightMasks
//

class CHighlightMasks : public TIndirectArray<CHighlightMasksItem>
{
public:
    CHighlightMasks(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CHighlightMasksItem>(base, delta, dt) {}

    BOOL Load(CHighlightMasks& source);

    // searches all masks and if it finds a matching item, it returns a pointer to it
    // otherwise returns NULL; 'fileExt' is NULL for directories (the extension must be resolved)
    inline CHighlightMasksItem* AgreeMasks(const char* fileName, const char* fileExt, DWORD fileAttr)
    {
        int i;
        for (i = 0; i < Count; i++)
        {
            CHighlightMasksItem* item = At(i);
            if (((item->Attr & item->ValidAttr) == (fileAttr & item->ValidAttr)) &&
                item->Masks->AgreeMasks(fileName, fileExt))
                return At(i);
        }
        return NULL;
    }
};

//****************************************************************************
//
// CViewerMasksItem
//

#define VIEWER_EXTERNAL 0
#define VIEWER_INTERNAL 1

struct CViewerMasksItem
{
    CMaskGroup* Masks;
    char *Command,
        *Arguments,
        *InitDir;

    int ViewerType;

    DWORD HandlerID; // unique ID (valid during the Salamander session)
                     // used to identify the editor when selecting from the history of the file - CFileHistory

    // helper variable for determining the type of data - TRUE = old -> 'Type' (0 viewer, 1 IE viewer, 2 external)
    BOOL OldType;

    CViewerMasksItem(const char* masks, const char* command, const char* arguments, const char* initDir,
                     int viewerType, BOOL oldType);

    CViewerMasksItem();
    CViewerMasksItem(CViewerMasksItem& item);
    ~CViewerMasksItem();

    BOOL Set(const char* masks, const char* command, const char* arguments, const char* initDir);
    BOOL IsGood();
};

//****************************************************************************
//
// CViewerMasks
//

class CViewerMasks : public TIndirectArray<CViewerMasksItem>
{
public:
    CViewerMasks(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CViewerMasksItem>(base, delta, dt) {}

    BOOL Load(CViewerMasks& source);
};

//****************************************************************************
//
// CEditorMasksItem
//

struct CEditorMasksItem
{
    CMaskGroup* Masks;
    char *Command,
        *Arguments,
        *InitDir;

    DWORD HandlerID; // unique ID (valid during the Salamander session)
                     // used to identify the editor when selecting from the history of the file - CFileHistory

    CEditorMasksItem(char* masks, char* command, char* arguments, char* initDir);
    CEditorMasksItem();
    CEditorMasksItem(CEditorMasksItem& item);
    ~CEditorMasksItem();

    BOOL Set(const char* masks, const char* command, const char* arguments, const char* initDir);
    BOOL IsGood();
};

//****************************************************************************
//
// CEditorMasks
//

class CEditorMasks : public TIndirectArray<CEditorMasksItem>
{
public:
    CEditorMasks(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CEditorMasksItem>(base, delta, dt) {}

    BOOL Load(CEditorMasks& source);
};

//
// ****************************************************************************

extern const char* DefTopToolBar; // default values
extern const char* DefMiddleToolBar;
extern const char* DefLeftToolBar;
extern const char* DefRightToolBar;

#define TITLE_BAR_MODE_DIRECTORY 0 // must correspond to the array {IDS_TITLEBAR_DIRECTORY, IDS_TITLEBAR_COMPOSITE, IDS_TITLEBAR_FULLPATH}
#define TITLE_BAR_MODE_COMPOSITE 1
#define TITLE_BAR_MODE_FULLPATH 2

#define TITLE_PREFIX_MAX 100 // size of the buffer for the title prefix

typedef struct
{
    int IconResID;
    int TextResID;
} CMainWindowIconItem;
#define MAINWINDOWICONS_COUNT 4
extern CMainWindowIconItem MainWindowIcons[MAINWINDOWICONS_COUNT];

struct CConfiguration
{
    // ConfigVersion - version number of the loaded configuration (see comment in mainwnd2.cpp)
    DWORD ConfigVersion;

    int IncludeDirs,            // select/deselect (*, +, -) directories as well
        AutoSave,               // save on exit
        CloseShell,             // close the shell after launching the command line
        ShowGrepErrors,         // should the Find Files dialog show error messages?
        FindFullRowSelect,      // enable full row select in the Find dialog
        MinBeepWhenDone,        // beep when processing ends in an inactive window
        ClearReadOnly,          // remove the read-only flag during CD-ROM operations
        PrimaryContextMenu,     // is a context menu displayed on the right mouse button?
        NotHiddenSystemFiles,   // show hidden and system files?
        AlwaysOnTop,            // should the main window stay Always On Top?
                                //      FastDirectoryMove,     // is moving directories by renaming allowed?
        SortUsesLocale,         // sort according to regional settings
        SortDetectNumbers,      // detect numbers during sorting strings? (see StrCmpLogicalW)
        SortNewerOnTop,         // show newer items first -- Salamander 2.0 behavior
        SortDirsByName,         // sort directories by name
        SortDirsByExt,          // emulate extensions for directories (sort by extension + show in separated Ext column)
        SaveHistory,            // store histories into the configuration?
        SaveWorkDirs,           // store the List of Working Directories?
        EnableCmdLineHistory,   // keep history of the command line?
        SaveCmdLineHistory,     // store the command line history?
                                //      LantasticCheck,        // Lantastic 7.0 paranoid check (compare sizes after Copy)
        OnlyOneInstance,        // allow just a single instance
        ForceOnlyOneInstance,   // set from cmdline: Salamander should behave as if the OnlyOneInstance option is enabled
        StatusArea,             // Salamander lives in the tray and won't appear in the taskbar when minimized
        SingleClick,            // single click selects an item
        TopToolBarVisible,      // toolbar visibility
        PluginsBarVisible,      // toolbar visibility
        MiddleToolBarVisible,   // toolbar visibility
        BottomToolBarVisible,   // toolbar visibility
        UserMenuToolBarVisible, // toolbar visibility
        HotPathsBarVisible,     // toolbar visibility
        DriveBarVisible,        // drive bar visibility
        DriveBar2Visible,       // second drive bar visibility
        UseSalOpen,             // should salopen.exe be used (otherwise association runs directly)
        NetwareFastDirMove,     // should fast-dir-move (rename directories) be used on the Novell Netware? (otherwise rename files only, directories are created + old empty ones deleted) (REASON: for some users, fast-dir-move works on Novell and they don’t want to wait)
        UseAsyncCopyAlg,        // Win7+ only (older OS: always FALSE): should asynchronous file copy algorithm be used on network drives?
        ReloadEnvVariables,     // should we perform regeneration when environment variables change??
        QuickRenameSelectAll,   // Quick Rename/Pack selects everything (not just the name) (users disliked the new selection)
        EditNewSelectAll,       // EditNew should select everything (not just the name). users requested a separate option because some always create .TXT (and are fine with overwriting just the name) while others use different extensions and want to overwrite the entire filename
        ShiftForHotPaths,       // use Shift+1..0 for go to hot path?
        IconSpacingVert,        // vertical spacing in points between Icons/Thumbnails in the panel
        IconSpacingHorz,        // horizontal spacing in points between Icons in the panel
        TileSpacingVert,        // vertical spacing in points between Tiles in the panel
        ThumbnailSpacingHorz,   // horizontal spacing in points between Thumbnails in the panel
        ThumbnailSize,          // square dimensions of thumbnails in points
                                //      PanelTooltip,         // shortened texts in panels get tooltips
        KeepPluginsSorted,      // plugins will be sorted alphabetically (plugins manager, menu)
        ShowSLGIncomplete,      // TRUE = if IsSLGIncomplete is not empty, show message about incomplete translation (we are looking for a translator)

        // Confirmation
        CnfrmFileDirDel,         // files or directory delete
        CnfrmNEDirDel,           // non-empty directory delete
        CnfrmFileOver,           // file overwrite
        CnfrmDirOver,            // directory overwrite (Copy/Move: join with existing directory if target directory already exists)
        CnfrmSHFileDel,          // system or hidden file delete
        CnfrmSHDirDel,           // system or hidden directory delete
        CnfrmSHFileOver,         // system or hidden file overwrite
        CnfrmNTFSPress,          // NTFS Compress, Uncompress
        CnfrmNTFSCrypt,          // NTFS Encrypt, Decrypt
        CnfrmDragDrop,           // Drag and Drop operations
        CnfrmCloseArchive,       // Show information before closing edited archive
        CnfrmCloseFind,          // Show information before closing Find dialog box
        CnfrmStopFind,           // Show information before stopping searching in Find dialog box
        CnfrmCreatePath,         // Show "do you want to create target path?" in Copy/Move operations
        CnfrmAlwaysOnTop,        // Show "Always on Top information"
        CnfrmOnSalClose,         // Show "do you want to close SS?"
        CnfrmSendEmail,          // Show "Do you want to Email selected files?"
        CnfrmAddToArchive,       // Show "Do you want to update existing archive?"
        CnfrmCreateDir,          // Show "The directory doesn't exist. Do you want to create it?"
        CnfrmChangeDirTC,        // user is trying to change path via the command line
        CnfrmShowNamesToCompare, // User Menu: show the  dialog to enter names for Compare even if both names are already provided
        CnfrmDSTShiftsIgnored,   // Compare Directories: file pairs were found with timestamps differing by exactly one or two hours; show warning that these differences were ignored?
        CnfrmDSTShiftsOccured,   // Compare Directories: file pairs were found with timestamps differing by exactly one or two hours; show hint that these differences can be ignored?
        CnfrmCopyMoveOptionsNS,  // Copy/Move: some Options are set but the target is FS/archive (options are NotSupported)

        // Drive specific
        DrvSpecFloppyMon,    // Use automatic refresh
        DrvSpecFloppySimple, // Use simple icons
        DrvSpecRemovableMon,
        DrvSpecRemovableSimple,
        DrvSpecFixedMon,
        DrvSpecFixedSimple,
        DrvSpecRemoteMon,
        DrvSpecRemoteSimple,
        DrvSpecRemoteDoNotRefreshOnAct, // remote drives/do not refresh on activate of Salamander
        DrvSpecCDROMMon,
        DrvSpecCDROMSimple;

    // options for Compare Directories dialog box / functions
    int CompareByTime;
    int CompareBySize;
    int CompareByContent;
    int CompareByAttr;
    int CompareSubdirs;
    int CompareSubdirsAttr;
    int CompareOnePanelDirs; // mark names of directories that exist only in one panel
    int CompareMoreOptions;  // is the dialog displayed in extended mode?
    int CompareIgnoreFiles;  // should specified filenames be ignored?
    int CompareIgnoreDirs;   // should specified names of directories be ignored?
    CMaskGroup CompareIgnoreFilesMasks;
    CMaskGroup CompareIgnoreDirsMasks;

    BOOL IfPathIsInaccessibleGoToIsMyDocs;   // TRUE = ignore IfPathIsInaccessibleGoTo and fetch Documents from the system directly
    char IfPathIsInaccessibleGoTo[MAX_PATH]; // path used when the current one becomes inaccessible (network outage, media removed from the removable drive, ...)

    DWORD LastUsedSpeedLimit; // remembers the last used speed limit (users often repeat one number)

    BOOL QuickSearchEnterAlt; // if it is TRUE, Quick Search is activated via Alt+letter

    // for displaying the items in the panel
    int FullRowSelect;    // in detailed/brief view clicking anywhere selects the item
    int FullRowHighlight; // in detailed view highlight continues past the focused column
    int UseIconTincture;  // for hidden/system/selected/focused items
    int ShowPanelCaption; // should the panel caption be shown in color in the directory line?
    int ShowPanelZoom;    // should the Zoom button be shown in the directory line?

    char InfoLineContent[200];

    int FileNameFormat; // how to adjust filename after reading from disk

    int SizeFormat; // how to display values in the Size column: SIZE_FORMAT_xxx

    int SpaceSelCalcSpace; // when selecting with Space, calculate occupied space

    int UseTimeResolution; // compare directories: use time resolution
    int TimeResolution;    // compare directories: time resolution <0..3600>s
    int IgnoreDSTShifts;   // compare directories: ignore time differences of exactly one or two hours? (handles daylight-saving shifts and NTFS/FAT32/FTP etc.)

    int UseDragDropMinTime;
    int DragDropMinTime; // minimum delay in ms between mouse down and up (otherwise it's not a drag&drop)

    int HotPathAutoConfig; // automatically open configuration after assigning from a panel

    char TopToolBar[400]; // ToolBar contents
    char MiddleToolBar[400];
    char LeftToolBar[200];
    char RightToolBar[200];

    int UseRecycleBin;       // 0 - do not use, 1 - for all, 2 - for RecycleMasks
    CMaskGroup RecycleMasks; // mask array determining what is sent to the Recycle Bin

    // how experienced the user feels -- menus will be reduced accordingly
    BOOL SkillLevel; // SKILL_LEVEL_BEGINNER, SKILL_LEVEL_INTERMEDIATE, SKILL_LEVEL_ADVANCED

    // the history arrays are destroyed in the ClearHistory() method
    char* SelectHistory[SELECT_HISTORY_SIZE];
    char* CopyHistory[COPY_HISTORY_SIZE];
    char* EditHistory[EDIT_HISTORY_SIZE];
    char* ChangeDirHistory[CHANGEDIR_HISTORY_SIZE];
    char* FileListHistory[FILELIST_HISTORY_SIZE];
    char* CreateDirHistory[CREATEDIR_HISTORY_SIZE];
    char* QuickRenameHistory[QUICKRENAME_HISTORY_SIZE];
    char* EditNewHistory[EDITNEW_HISTORY_SIZE];
    char* ConvertHistory[CONVERT_HISTORY_SIZE];
    char* FilterHistory[FILTER_HISTORY_SIZE];

    char FileListName[MAX_PATH]; // file name
    BOOL FileListAppend;
    int FileListDestination; // 0=Clipboard 1=Viewer 2=File

    // Internal Viewer:
    int CopyFindText; // after F3 from Find Files dialog: copy find text to viewer?

    int EOL_CRLF, // various line endings (on/off)
        EOL_CR,
        EOL_LF,
        EOL_NULL;

    int DefViewMode,  // 0 = Auto-Select, 1 = Text, 2 = Hex
        TabSize,      // tab size (number of spaces per tab)
        SavePosition; // store window position / place relative to the main window

    CMaskGroup TextModeMasks; // mask array for files always shown in text mode
    CMaskGroup HexModeMasks;  // mask array for files always shown in hex mode

    WINDOWPLACEMENT WindowPlacement; // invalid unless SavePosition != TRUE

    BOOL WrapText; // text wrapping set via menu (here only for saving)

    BOOL CodePageAutoSelect;  // automatically detect the code page
    char DefaultConvert[200]; // encoding name the user wants to use by default

    BOOL AutoCopySelection; // automatically copy selection to the clipboard

    BOOL GoToOffsetIsHex; // TRUE = offset entered as hex, otherwise decimal

    // rebar
    int MenuIndex; // zero-based order of the band in the rebar
    int MenuBreak; // is the band on a new line?
    int MenuWidth; // band width
    int TopToolbarIndex;
    int TopToolbarBreak;
    int TopToolbarWidth;
    int PluginsBarIndex;
    int PluginsBarBreak;
    int PluginsBarWidth;
    int UserMenuToolbarIndex;
    int UserMenuToolbarBreak;
    int UserMenuToolbarWidth;
    int UserMenuToolbarLabels;
    int HotPathsBarIndex;
    int HotPathsBarBreak;
    int HotPathsBarWidth;
    int DriveBarIndex;
    int DriveBarBreak;
    int DriveBarWidth;
    int GripsVisible;

    // Change drive
    int ChangeDriveShowMyDoc;    // display Documents items
    int ChangeDriveShowAnother;  // display the Another panel path item
    int ChangeDriveShowNet;      // display the Network item
    int ChangeDriveCloudStorage; // display items for cloud storage (Google Drive, etc.)

    // Packers / Unpackers
    int UseAnotherPanelForPack;
    int UseAnotherPanelForUnpack;
    int UseSubdirNameByArchiveForUnpack;
    int UseSimpleIconsInArchives;

    BOOL UseEditNewFileDefault;        // should the EditNewFileDefault value be used? (if not, it is loaded from resources, thus language switching works)
    char EditNewFileDefault[MAX_PATH]; // used as the default for the EditNewFile command when UseEditNewFileDefault is enabled

    // Tip of the Day
    //  int  ShowTipOfTheDay;         // display Tip of the Day at program startup
    //  int  LastTipOfTheDay;         // index of the last displayed tip

    // plug-ins
    int LastPluginVer;   // ACTUAL_VERSION from plugins.ver (detect newly installed plugins)
    int LastPluginVerOP; // ACTUAL_VERSION from plugins.ver for the other platform (x86/x64); must be saved, otherwise we won't know if the configuration was overwritten by the other version.
                         // Example: start x64, auto-save x64 with added pictview, start x86, auto-save x86 with added winscp (x86 pictview not added because it was already in the x64 config), exit/save x86, WARNING: exit/save x64 would remove the record of winscp (x64 knows nothing about winscp and continues running)

    // globals
    BOOL ConfigWasImported; // from config.reg file

    // custom icon overlays
    BOOL EnableCustomIconOverlays;    // TRUE = icon overlays are used (see ShellIconOverlays)
    char* DisabledCustomIconOverlays; // allocated list of disabled icon overlay handlers (separator is ';', escape - sequence for ';' is ';;')

#ifndef _WIN64
    // FIXME_X64_WINSCP - this approach is not ideal. Find a better one (split x86 and x64 versions and share data)
    BOOL AddX86OnlyPlugins; // TRUE = the x86 version of Salamander has not yet been started; on first run add plugins missing in the x64 build
#endif                      // _WIN64

    CConfiguration();
    ~CConfiguration();

    void ClearHistory(); // clears all stored histories

    int GetMainWindowIconIndex(); // returns a valid index in the MainWindowIcons array

    BOOL PrepareRecycleMasks(int& errorPos); // prepare recycle-bin masks for use
    BOOL AgreeRecycleMasks(const char* fileName, const char* fileExt);

    DWORD LastFocusedPage;          // last visited page in the dialog
    DWORD ConfigurationHeight;      // height of the configuration dialog in points
    BOOL ViewersAndEditorsExpanded; // expanded items in the tree
    BOOL PackersAndUnpackersExpanded;

    // Find dialog
    BOOL SearchFileContent;
    WINDOWPLACEMENT FindDialogWindowPlacement;
    int FindColNameWidth; // width of the Name column in the Find dialog

    // Language
    char LoadedSLGName[MAX_PATH];    // xxxxx.slg that was loaded at Salamander start
    char SLGName[MAX_PATH];          // xxxxx.slg to use next time Salamander starts
    int DoNotDispCantLoadPluginSLG;  // TRUE = suppress warning that an SLG with the same name cannot be loaded into the plugin as in Salamander
    int DoNotDispCantLoadPluginSLG2; // TRUE = suppress warning that the SLG plugin used last time (either user-selected or auto-selected) cannot be loaded
    int UseAsAltSLGInOtherPlugins;   // TRUE = try to use AltSLGName for plugins
    char AltPluginSLGName[MAX_PATH]; // only if UseAsAltSLGInOtherPlugins is TRUE: fallback SLG module for plugins (if LoadedSLGName for plugin does not exist)

    // Directory name convert\\XXX\\convert.cfg from which convert.cfg is loaded
    char ConversionTable[MAX_PATH];

    int TitleBarShowPath;                        // will we display the path in the title bar?
    int TitleBarMode;                            // title bar display mode (TITLE_BAR_MODE_xxx)
    int UseTitleBarPrefix;                       // should prefix be shown in the title bar?
    char TitleBarPrefix[TITLE_PREFIX_MAX];       // prefix for the title bar
    int UseTitleBarPrefixForced;                 // command-line variant has priority and is not saved
    char TitleBarPrefixForced[TITLE_PREFIX_MAX]; // command-line variant has priority and is not saved
    int MainWindowIconIndex;                     // index of the icon in MainWindowIcons[], 0=default
    int MainWindowIconIndexForced;               // command-line variant, has priority and is not saved; -1 -- unset

    int ClickQuickRename; // clicking the focused item triggers Quick Rename

    // bit fields where individual bits represent drives A..Z
    // allowed values are 0 to 0x03FFFFFF (DRIVES_MASK)
    DWORD VisibleDrives;   // drives displayed by Salamander (independent from the NoDrives policy)
    DWORD SeparatedDrives; // drives after which separators are shown in the Alt+F1/F2 menu (for clarity)

    BOOL ShowSplashScreen; // show the splash screen during startup
};

//
// ****************************************************************************

class CLoadSaveToRegistryMutex
{
protected:
    HANDLE Mutex;
    int DebugCheck;

public:
    CLoadSaveToRegistryMutex();
    ~CLoadSaveToRegistryMutex();

    void Init();

    void Enter();
    void Leave();
};

extern CLoadSaveToRegistryMutex LoadSaveToRegistryMutex; // mutex for synchronizing load/save to the Registry (two processes at once cause problems)

//
// ****************************************************************************

class CCfgPageGeneral : public CCommonPropSheetPage
{
public:
    CCfgPageGeneral();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

class CCfgPageRegional : public CCommonPropSheetPage
{
public:
    char SLGName[MAX_PATH];
    char DirName[MAX_PATH];

public:
    CCfgPageRegional();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LoadControls();
};

//
// ****************************************************************************

class CToolbarHeader;

class CCfgPageView : public CCommonPropSheetPage
{
protected:
    BOOL Dirty;
    CToolbarHeader* Header;
    HWND HListView;
    CToolbarHeader* Header2;
    HWND HListView2;
    CViewTemplates Config;
    BOOL DisableNotification;
    BOOL LabelEdit;
    int SelectIndex;

public:
    CCfgPageView(int index);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void OnModify();
    void OnDelete();
    void OnMove(BOOL up);

    void LoadControls();
    void StoreControls();
    void EnableControls();
    void EnableHeader();
    DWORD GetEnabledFunctions();

    BOOL IsDirty();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageIconOvrls : public CCommonPropSheetPage
{
protected:
    HWND HListView;

public:
    CCfgPageIconOvrls();

    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageSecurity : public CCommonPropSheetPage
{
public:
    CCfgPageSecurity();

    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

//class CColorButton;

class CCfgPageViewer : public CCommonPropSheetPage
{
protected:
    HFONT HFont;
    BOOL LocalUseCustomViewerFont;
    LOGFONT LocalViewerLogFont;
    CColorArrowButton* NormalText;
    CColorArrowButton* SelectedText;
    SALCOLOR TmpColors[NUMBER_OF_VIEWERCOLORS];

public:
    CCfgPageViewer();
    ~CCfgPageViewer();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void LoadControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
//
// ****************************************************************************

class CUserMenuItems;
class CEditListBox;
/*
class CSmallIconWindow : public CWindow
{
  protected:
    HICON HIcon;

  public:
    CSmallIconWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID)
    {
      HIcon = NULL;
    }

    void SetIcon(HICON hIcon);

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/

class CCfgPageUserMenu : public CCommonPropSheetPage
{
public:
    CCfgPageUserMenu(CUserMenuItems* userMenuItems);
    ~CCfgPageUserMenu();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableButtons();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DeleteSubmenuEnd(int index); // for opening the selected submenu ('index') remove the closing item

    void RefreshGroupIconInUMItems(); // after changing colors, HGroupIcon changes; we must update it in UserMenuItems as well

    CUserMenuItems* UserMenuItems;
    CUserMenuItems* SourceUserMenuItems;
    CEditListBox* EditLB;
    //    CSmallIconWindow *SmallIcon;
    BOOL DisableNotification;
};

//
// ****************************************************************************

class CHotPathItems;

class CCfgPageHotPath : public CCommonPropSheetPage
{
protected:
    BOOL Dirty;
    CToolbarHeader* Header;
    HWND HListView;
    CHotPathItems* Config;
    BOOL DisableNotification;
    BOOL EditMode;
    int EditIndex;
    BOOL LabelEdit; // we are editing a label

public:
    CCfgPageHotPath(BOOL editMode, int editIndex);
    ~CCfgPageHotPath();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void OnModify();
    void OnDelete();
    void OnMove(BOOL up);

    void LoadControls();
    void StoreControls();
    void EnableControls();
    void EnableHeader();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageSystem : public CCommonPropSheetPage
{
public:
    CCfgPageSystem();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

#define CFG_COLORS_BUTTONS 5

class CCfgPageColors : public CCommonPropSheetPage
{
protected:
    CColorArrowButton* Items[CFG_COLORS_BUTTONS];
    CColorArrowButton* Masks[CFG_COLORS_BUTTONS];

    HWND HScheme;
    HWND HItem;
    COLORREF TmpColors[NUMBER_OF_COLORS];

    CEditListBox* EditLB;
    BOOL DisableNotification;
    CHighlightMasks HighlightMasks;
    CHighlightMasks* SourceHighlightMasks;

    BOOL Dirty;

public:
    CCfgPageColors();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LoadColors();
    void LoadMasks();
    void StoreMasks();
    void EnableControls();
};

//
// ****************************************************************************

struct CConfirmationItem
{
    HTREEITEM HTreeItem;
    int* Variable;
    int Checked;
};

class CCfgPageConfirmations : public CCommonPropSheetPage
{
protected:
    HWND HTreeView;
    HTREEITEM HConfirmOn;
    HTREEITEM HShowMessage;
    HIMAGELIST HImageList;
    TDirectArray<CConfirmationItem> List;
    BOOL DisableNotification;

public:
    CCfgPageConfirmations();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InitTree();
    HIMAGELIST CreateImageList();
    HTREEITEM AddItem(HTREEITEM hParent, int iImage, int textResID, int* value);
    void SetItemChecked(HTREEITEM hTreeItem, BOOL checked);
    //BOOL GetItemChecked(HTREEITEM hTreeItem);
    int FindInList(HTREEITEM hTreeItem);
    void ChangeSelected();
};

//
// ****************************************************************************

class CCfgPageDrives : public CCommonPropSheetPage
{
protected:
    BOOL IfPathIsInaccessibleGoToChanged; // TRUE = the user edited the IfPathIsInaccessibleGoTo path
    BOOL FocusIfPathIsInaccessibleGoTo;   // TRUE = to set focus to the "If Path Is Inaccessible Go To" edit box

public:
    CCfgPageDrives(BOOL focusIfPathIsInaccessibleGoTo);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageViewEdit : public CCommonPropSheetPage
{
public:
    CCfgPageViewEdit();
};

//
// ****************************************************************************

class CCfgPageViewers : public CCommonPropSheetPage
{
protected:
    BOOL Alternative;

public:
    CCfgPageViewers(BOOL alternative = FALSE);

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL Dirty;
    CViewerMasks ViewerMasks;
    CViewerMasks* SourceViewerMasks;
    CEditListBox* EditLB;
    BOOL DisableNotification;
};

//
// ****************************************************************************

class CCfgPageEditors : public CCommonPropSheetPage
{
public:
    CCfgPageEditors();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL Dirty;
    CEditorMasks EditorMasks;
    CEditorMasks* SourceEditorMasks;
    CEditListBox* EditLB;
    BOOL DisableNotification;
};

//
// ****************************************************************************

class CCfgPageKeyboard : public CCommonPropSheetPage
{
public:
    CCfgPageKeyboard();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CConfigurationPageStats : public CCommonPropSheetPage
{
public:
    CConfigurationPageStats();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageArchivers : public CCommonPropSheetPage
{
public:
    CCfgPageArchivers();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************

class CPackerConfig;

class CCfgPagePackers : public CCommonPropSheetPage
{
protected:
    CPackerConfig* Config;
    CEditListBox* EditLB;
    BOOL DisableNotification;

public:
    CCfgPagePackers();
    ~CCfgPagePackers();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CUnpackerConfig;

class CCfgPageUnpackers : public CCommonPropSheetPage
{
protected:
    CUnpackerConfig* Config;
    CEditListBox* EditLB;
    BOOL DisableNotification;

public:
    CCfgPageUnpackers();
    ~CCfgPageUnpackers();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CArchiverConfig;

class CCfgPageExternalArchivers : public CCommonPropSheetPage
{
protected:
    CArchiverConfig* Config;
    BOOL DisableNotification;
    HWND HListbox;

public:
    CCfgPageExternalArchivers();
    ~CCfgPageExternalArchivers();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CPackerFormatConfig;

class CCfgPageArchivesAssoc : public CCommonPropSheetPage
{
protected:
    CPackerFormatConfig* Config;
    CEditListBox* EditLB;
    BOOL DisableNotification;

public:
    CCfgPageArchivesAssoc();
    ~CCfgPageArchivesAssoc();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
/*
class CCfgPageShellExt: public CCommonPropSheetPage
{
  protected:
    CEditListBox        *EditLB;
    BOOL                DisableNotification;

  public:
    CCfgPageShellExt();
    ~CCfgPageShellExt();

    virtual void Transfer(CTransferInfo &ti);
    virtual void Validate(CTransferInfo &ti);

  protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/
//
// ****************************************************************************

class CCfgPageAppearance : public CCommonPropSheetPage
{
protected:
    HFONT HPanelFont;
    BOOL LocalUseCustomPanelFont;
    LOGFONT LocalPanelLogFont;
    BOOL NotificationEnabled;

public:
    CCfgPageAppearance();
    ~CCfgPageAppearance();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    void LoadControls();
    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageMainWindow : public CCommonPropSheetPage
{
protected:
    HIMAGELIST HIconsList;

public:
    CCfgPageMainWindow();
    ~CCfgPageMainWindow();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    void LoadControls();
    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL InitIconCombobox();
};

//
// ****************************************************************************

class CCfgPageChangeDrive : public CCommonPropSheetPage
{
protected:
    SIZE CharSize;

public:
    CCfgPageChangeDrive();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void SetDrivesToListbox(int redID, DWORD drives);
    DWORD GetDrivesFromListbox(int redID);
    void InitList(int resID); // setup listbox

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPagePanels : public CCommonPropSheetPage
{
public:
    CCfgPagePanels();

    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageHistory : public CCommonPropSheetPage
{
public:
    CCfgPageHistory();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();

    // the user wants to clear histories
    void OnClearHistory();
};

//
// ****************************************************************************

class CConfigurationDlg : public CTreePropDialog
{
public:
    // mode: 0 - normal
    //       1 - hot paths
    CConfigurationDlg(HWND parent, CUserMenuItems* userMenuItems, int mode = 0, int param = 0);

public:
    CCfgPageGeneral PageGeneral;
    CCfgPageView PageView;
    CCfgPageViewer PageViewer;
    CCfgPageUserMenu PageUserMenu;
    CCfgPageHotPath PageHotPath;
    CCfgPageSystem PageSystem;
    CCfgPageColors PageColors;
    CCfgPageConfirmations PageConfirmations;
    CCfgPageDrives PageDrives;
    CCfgPageViewEdit PageViewEdit;
    CCfgPageViewers Page13;
    CCfgPageViewers Page14;
    CCfgPageEditors Page15;
    CCfgPageIconOvrls PageIconOvrls;
    CCfgPageAppearance PageAppear;
    CCfgPageMainWindow PageMainWindow;
    CCfgPageRegional PageRegional;
    CCfgPageHistory PageHistory;
    CCfgPageChangeDrive PageChangeDrive;
    CCfgPagePanels PagePanels;
    CCfgPageKeyboard PageKeyboard;
    CCfgPageSecurity PageSecurity;

    //    CCfgPageShellExt              PageShellExtensions; // ShellExtensions

    CCfgPageArchivers PagePP;
    CCfgPagePackers PageP1;
    CCfgPageUnpackers PageP2;
    CCfgPageExternalArchivers PageP3;
    CCfgPageArchivesAssoc PageP4;

    HWND HOldPluginMsgBoxParent;

protected:
    virtual void DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

BOOL ValidatePathIsNotEmpty(HWND hParent, const char* path);

extern CConfiguration Configuration;
