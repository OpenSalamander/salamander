// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

// if DEMOPLUG_QUIET is defined, keep message-box prompts to a minimum
#define DEMOPLUG_QUIET

// if ENABLE_DYNAMICMENUEXT is defined, menu items are not added
// in CPluginInterface::Connect (called only once when the plugin loads),
// but repeatedly before each menu is opened, see FUNCTION_DYNAMICMENUEXT
// (lets the plugin change the menu dynamically based on current needs)
//#define ENABLE_DYNAMICMENUEXT

// global data
extern const char* PluginNameEN; // untranslated plugin name, used before the language module loads and for debug purposes
extern HINSTANCE DLLInstance;    // handle to the SPL - language-independent resources
extern HINSTANCE HLanguage;      // handle to the SLG - language-dependent resources

// general Salamander interface - valid from startup until the plugin terminates
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// interface providing customized Windows controls used in Salamander
extern CSalamanderGUIAbstract* SalamanderGUI;

BOOL InitViewer();
void ReleaseViewer();

BOOL InitFS();
void ReleaseFS();

// FS name assigned by Salamander after the plugin loads
extern char AssignedFSName[MAX_PATH];
extern int AssignedFSNameLen;

// invoked for the first instance of DEMOPLUG: optional cleanup of its own temp directory
// with file copies extracted from archives by previous DEMOPLUG instances
void ClearTEMPIfNeeded(HWND parent);

// pointers to tables for lower/upper case mapping
extern unsigned char* LowerCase;
extern unsigned char* UpperCase;

// global variables used to store pointers to Salamander's global variables
// shared between the archiver and the FS
extern const CFileData** TransferFileData;
extern int* TransferIsDir;
extern char* TransferBuffer;
extern int* TransferLen;
extern DWORD* TransferRowData;
extern CPluginDataInterfaceAbstract** TransferPluginDataIface;
extern DWORD* TransferActCustomData;

// global data
extern char Str[MAX_PATH];
extern int Number;
extern int Selection; // "second" option in the configuration dialog
extern BOOL CheckBox;
extern int RadioBox;                       // radio 2 in configuration dialog
extern BOOL CfgSavePosition;               // save the window position/place it according to the main window
extern WINDOWPLACEMENT CfgWindowPlacement; // invalid when CfgSavePosition != TRUE
extern int Size2FixedWidth;                // Size2 column (archiver): LOW/HIGH WORD: left/right panel: FixedWidth
extern int Size2Width;                     // Size2 column (archiver): LOW/HIGH WORD: left/right panel: Width
extern int CreatedFixedWidth;              // Created column (FS): LOW/HIGH WORD: left/right panel: FixedWidth
extern int CreatedWidth;                   // Created column (FS): LOW/HIGH WORD: left/right panel: Width
extern int ModifiedFixedWidth;             // Modified column (FS): LOW/HIGH WORD: left/right panel: FixedWidth
extern int ModifiedWidth;                  // Modified column (FS): LOW/HIGH WORD: left/right panel: Width
extern int AccessedFixedWidth;             // Accessed column (FS): LOW/HIGH WORD: left/right panel: FixedWidth
extern int AccessedWidth;                  // Accessed column (FS): LOW/HIGH WORD: left/right panel: Width
extern int DFSTypeFixedWidth;              // DFS Type column (FS): LOW/HIGH WORD: left/right panel: FixedWidth
extern int DFSTypeWidth;                   // DFS Type column (FS): LOW/HIGH WORD: left/right panel: Width

extern DWORD LastCfgPage; // start page (sheet) in the configuration dialog

// image list for simple FS icons
extern HIMAGELIST DFSImageList;

// [0, 0] - for open viewer windows: plugin configuration has changed
#define WM_USER_VIEWERCFGCHNG WM_APP + 3246
// [0, 0] - for open viewer windows: history needs to be trimmed
#define WM_USER_CLEARHISTORY WM_APP + 3247
// [0, 0] - for open viewer windows: Salamander regenerated fonts, call SetFont() for the lists
#define WM_USER_SETTINGCHANGE WM_APP + 3248

char* LoadStr(int resID);

// plugin menu commands
#define MENUCMD_ALWAYS 1
#define MENUCMD_DIR 2
#define MENUCMD_ARCFILE 3
#define MENUCMD_FILEONDISK 4
#define MENUCMD_ARCFILEONDISK 5
#define MENUCMD_DOPFILES 6
#define MENUCMD_FILESDIRSINARC 7
#define MENUCMD_ENTERDISKPATH 8
#define MENUCMD_ALLUSERS 9
#define MENUCMD_INTADVUSERS 10
#define MENUCMD_ADVUSERS 11
#define MENUCMD_SHOWCONTROLS 12
#define MENUCMD_SEP 13
#define MENUCMD_HIDDENITEM 14
#define MENUCMD_CHECKDEMOPLUGTMPDIR 15 // first DEMOPLUG instance: optionally clean its temp dir with files unpacked by previous instances
#define MENUCMD_DISCONNECT_LEFT 16
#define MENUCMD_DISCONNECT_RIGHT 17
#define MENUCMD_DISCONNECT_ACTIVE 18

//
// ****************************************************************************
// CArcPluginDataInterface
//

class CArcPluginDataInterface : public CPluginDataInterfaceAbstract
{
public:
    virtual BOOL WINAPI CallReleaseForFiles() { return FALSE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return FALSE; }
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir) {}

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir)
    {
        // the plugin stores no custom data in CFileData, so nothing needs to change or be allocated
    }
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir)
    {
        // the plugin stores no custom data in CFileData, so nothing needs to change or be allocated
        return TRUE; // report success
    }

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) { return NULL; }
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return TRUE; }
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) { return NULL; }
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) { return 0; }

    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                  const CFileData* upperDir);
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth);
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth);

    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount) { return FALSE; }

    virtual BOOL WINAPI CanBeCopiedToClipboard() { return TRUE; }

    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) { return FALSE; }
};

//
// ****************************************************************************
// CPluginInterface
//

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
public:
    virtual BOOL WINAPI ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                    CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData);
    virtual BOOL WINAPI UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                      const char* archiveRoot, SalEnumSelection next, void* nextParam);
    virtual BOOL WINAPI UnpackOneFile(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                                      const CFileData* fileData, const char* targetDir,
                                      const char* newFileName, BOOL* renamingNotSupported);
    virtual BOOL WINAPI PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      const char* archiveRoot, BOOL move, const char* sourcePath,
                                      SalEnumSelection2 next, void* nextParam);
    virtual BOOL WINAPI DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                          CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                          SalEnumSelection next, void* nextParam);
    virtual BOOL WINAPI UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                           const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                           CDynamicString* archiveVolumes);
    virtual BOOL WINAPI CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                        BOOL force, int panel);

    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies);
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile);
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount);
};

class CPluginInterfaceForViewer : public CPluginInterfaceForViewerAbstract
{
public:
    virtual BOOL WINAPI ViewFile(const char* name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                 BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex);
    virtual BOOL WINAPI CanViewFile(const char* name) { return TRUE; }
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander);
};

class CPluginInterfaceForThumbLoader : public CPluginInterfaceForThumbLoaderAbstract
{
public:
    virtual BOOL WINAPI LoadThumbnail(const char* filename, int thumbWidth, int thumbHeight,
                                      CSalamanderThumbnailMakerAbstract* thumbMaker,
                                      BOOL fastThumbnail);
};

class CPluginInterfaceForFS : public CPluginInterfaceForFSAbstract
{
protected:
    int ActiveFSCount; // number of active FS interfaces (only to verify deallocation)

public:
    CPluginInterfaceForFS() { ActiveFSCount = 0; }
    int GetActiveFSCount() { return ActiveFSCount; }

    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(const char* fsName, int fsNameIndex);
    virtual void WINAPI CloseFS(CPluginFSInterfaceAbstract* fs);

    virtual void WINAPI ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex,
                                    CFileData& file, int isDir);
    virtual BOOL WINAPI DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                     CPluginFSInterfaceAbstract* pluginFS,
                                     const char* pluginFSName, int pluginFSNameIndex);

    virtual void WINAPI ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) {}
    virtual void WINAPI ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) {}

    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel);
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam);
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam);

    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) {}
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData);

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver();
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer();
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader();

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event);
};

//
// ****************************************************************************
// VIEWER
//

//
// ****************************************************************************
// CViewerWindow
//

#define BANDID_MENU 1
#define BANDID_TOOLBAR 2

enum CViewerWindowEnablerEnum
{
    vweAlwaysEnabled, // zero index is reserved
    vweCut,
    vwePaste,
    vweCount
};

class CViewerWindow;

class CRendererWindow : public CWindow
{
public:
    CViewerWindow* Viewer;

public:
    CRendererWindow();
    ~CRendererWindow();

    void OnContextMenu(const POINT* p);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CViewerWindow : public CWindow
{
public:
    HANDLE Lock;                      // 'lock' object or NULL (set to the signaled state after the file is closed)
    char Name[MAX_PATH];              // file name or ""
    CRendererWindow Renderer;         // inner viewer window
    HIMAGELIST HGrayToolBarImageList; // toolbar and menu in the gray variant (computed from the colored one)
    HIMAGELIST HHotToolBarImageList;  // toolbar and menu in the colored variant

    DWORD Enablers[vweCount];

    HWND HRebar; // holds the menu bar and toolbar
    CGUIMenuPopupAbstract* MainMenu;
    CGUIMenuBarAbstract* MenuBar;
    CGUIToolBarAbstract* ToolBar;

    int EnumFilesSourceUID;    // source UID for enumerating files in the viewer
    int EnumFilesCurrentIndex; // index of the current viewer file within the source

public:
    CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);

    HANDLE GetLock();

    // if 'setLock' is TRUE, set 'Lock' to the signaled state (it is
    // needed after closing the file)
    void OpenFile(const char* name, BOOL setLock = TRUE);

    BOOL IsMenuBarMessage(CONST MSG* lpMsg);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL FillToolBar();

    BOOL InitializeGraphics();
    BOOL ReleaseGraphics();

    BOOL InsertMenuBand();
    BOOL InsertToolBarBand();

    void LayoutWindows();

    void UpdateEnablers();
};

extern CWindowQueue ViewerWindowQueue; // list of all viewer windows
extern CThreadQueue ThreadQueue;       // list of all window threads

//
// ****************************************************************************
// FILE-SYSTEM
//

//
// ****************************************************************************
// CConnectData
//
// structure for passing data from the "Connect" dialog to a newly created FS

struct CConnectData
{
    BOOL UseConnectData;
    char UserPart[MAX_PATH];

    CConnectData()
    {
        UseConnectData = FALSE;
        UserPart[0] = 0;
    }
};

// structure for passing data from the "Connect" dialog to a newly created FS
extern CConnectData ConnectData;

//
// ****************************************************************************
// CDeleteProgressDlg
//
// example of a custom progress dialog for the Delete operation on the FS

class CDeleteProgressDlg : public CCommonDialog
{
protected:
    CGUIProgressBarAbstract* ProgressBar;
    BOOL WantCancel; // TRUE if the user wants to cancel

    // because the dialog does not run in its own thread, using a WM_TIMER method is pointless
    // the caller must invoke us anyway to pump the message queue
    DWORD LastTickCount; // to detect when changed data needs to be repainted

    char TextCache[MAX_PATH];
    BOOL TextCacheIsDirty;
    DWORD ProgressCache;
    BOOL ProgressCacheIsDirty;

public:
    CDeleteProgressDlg(HWND parent, CObjectOrigin origin = ooStandard);

    void Set(const char* fileName, DWORD progress, BOOL dalayedPaint);

    // empties the message queue (call often enough) and allows repainting, handling Cancel, ...
    // returns TRUE if the user wants to interrupt the operation
    BOOL GetWantCancel();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableCancel(BOOL enable);

    void FlushDataToControls();
};

//
// ****************************************************************************
// CPluginFSDataInterface
//

struct CFSData
{
    FILETIME CreationTime;
    FILETIME LastAccessTime;
    char* TypeName;

    CFSData(const FILETIME& creationTime, const FILETIME& lastAccessTime, const char* type);
    ~CFSData()
    {
        if (TypeName != NULL)
            SalamanderGeneral->Free(TypeName);
    }
    BOOL IsGood() { return TypeName != NULL; }
};

class CPluginFSDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    char Path[MAX_PATH]; // buffer for the full file/directory name used when loading icons
    char* Name;          // pointer within Path after the last backslash (to the name)

public:
    CPluginFSDataInterface(const char* path);

    virtual BOOL WINAPI CallReleaseForFiles() { return TRUE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return TRUE; }

    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir)
    {
        delete ((CFSData*)file.PluginData);
    }

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) {}
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) { return FALSE; }

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize);
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return FALSE; }
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon);

    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2)
    {
        return strcmp(file1->Name, file2->Name);
    }

    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                  const CFileData* upperDir);
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth);
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth);

    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount);

    virtual BOOL WINAPI CanBeCopiedToClipboard() { return TRUE; }

    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) { return FALSE; }
};

//****************************************************************************
//
// CTopIndexMem
//
// memory for the listbox top index in the panel - used by CPluginFSInterface for proper
// ExecuteOnFS behavior (preserves the top index when entering and leaving subdirectories)

#define TOP_INDEX_MEM_SIZE 50 // number of remembered top indexes (levels), at least 1

class CTopIndexMem
{
protected:
    // path for the last remembered top index
    char Path[MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // stored top indexes
    int TopIndexesCount;                // number of stored top indexes

public:
    CTopIndexMem() { Clear(); }
    void Clear()
    {
        Path[0] = 0;
        TopIndexesCount = 0;
    } // clears the memory
    void Push(const char* path, int topIndex);        // stores the top index for the given path
    BOOL FindAndPop(const char* path, int& topIndex); // finds the top index for the given path, FALSE -> not found
};

//
// ****************************************************************************
// CPluginFSInterface
//
// set of plugin methods required by Salamander to work with the file system

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
public:
    char Path[MAX_PATH];             // current path
    BOOL PathError;                  // TRUE if ListCurrentPath failed (path error); ChangePath will be called
    BOOL FatalError;                 // TRUE if ListCurrentPath failed (fatal error); ChangePath will be called
    CTopIndexMem TopIndexMem;        // top-index cache used by ExecuteOnFS()
    BOOL CalledFromDisconnectDialog; // TRUE = the user wants to disconnect this FS from the Disconnect dialog (F12)

public:
    CPluginFSInterface();
    ~CPluginFSInterface() {}

    virtual BOOL WINAPI GetCurrentPath(char* userPart);
    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize);
    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success);
    virtual BOOL WINAPI GetRootPath(char* userPart);

    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);

    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex, const char* userPart,
                                   char* cutFileName, BOOL* pathWasCut, BOOL forceRefresh, int mode);
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                        CPluginDataInterfaceAbstract*& pluginData,
                                        int& iconsType, BOOL forceRefresh);

    virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason);

    virtual void WINAPI Event(int event, DWORD param);

    virtual void WINAPI ReleaseObject(HWND parent);

    virtual DWORD WINAPI GetSupportedServices();

    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon);
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon);
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect);
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue);
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset);
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize) {}
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize) { return FALSE; }
    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent);
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo);
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                    char* newName, BOOL& cancel);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs);
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel);
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file);
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError);
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget);
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel);
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs);
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs);
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs);
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel) { return FALSE; }
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent) {}
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) {}
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) { return FALSE; }
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) { return FALSE; }
    virtual void WINAPI ShowSecurityInfo(HWND parent) {}
};

// common interface for archiver plugin data
extern CArcPluginDataInterface ArcPluginDataInterface;

// helper variable for tests
extern CPluginFSInterfaceAbstract* LastDetachedFS;

// plugin interface provided to Salamander
extern CPluginInterface PluginInterface;

// opens the configuration dialog; if it already exists, shows a message and returns
void OnConfiguration(HWND hParent);

// opens the About window
void OnAbout(HWND hParent);
