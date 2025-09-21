// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define NUM_OF_CHECKTHREADS 30                   // maximum number of threads for "non-blocking" path accessibility tests
#define ICONOVR_REFRESH_PERIOD 2000              // minimum interval between icon-overlay refreshes in the panel (see IconOverlaysChangedOnPath)
#define MIN_DELAY_BETWEENINACTIVEREFRESHES 2000  // minimum refresh interval when the main window is inactive
#define MAX_DELAY_BETWEENINACTIVEREFRESHES 10000 // maximum refresh interval when the main window is inactive

enum CActionType
{
    atCopy,
    atMove,
    atDelete,
    atCountSize,
    atChangeAttrs,
    atChangeCase,
    atRecursiveConvert, // convert including subdirectories
    atConvert           // convert not including subdirectories
};

enum CPluginFSActionType
{
    fsatCopy,
    fsatMove,
    fsatDelete,
    fsatCountSize,
    fsatChangeAttrs
};

enum CPanelType
{
    ptDisk,       // current path is "c:\path" or UNC
    ptZIPArchive, // current path is inside an archive (handled either by a plug-in or code supporting external archivers)
    ptPluginFS,   // current path is on a plug-in file system (handled by a plug-in)
};

struct CAttrsData // data for atChangeAttrs
{
    DWORD AttrAnd, AttrOr;
    BOOL SubDirs;
    BOOL ChangeCompression;
    BOOL ChangeEncryption;
};

struct CChangeCaseData // data for atChangeCase
{
    int FileNameFormat; // numbers compatible with the AlterFileName function
    int Change;         // which part of the name should change --||--
    BOOL SubDirs;       // include subdirectories?
};

class CCopyMoveData;

struct CTmpDropData
{
    BOOL Copy;
    char TargetPath[MAX_PATH];
    CCopyMoveData* Data;
};

struct CTmpDragDropOperData
{
    BOOL Copy;      // copy/move
    BOOL ToArchive; // archive/FS
    char ArchiveOrFSName[MAX_PATH];
    char ArchivePathOrUserPart[MAX_PATH];
    CDragDropOperData* Data;
};

class CCriteriaData // data pro atCopy/atMove
{
public:
    BOOL OverwriteOlder;      // overwrite older, skip newer ones
    BOOL StartOnIdle;         // start only when nothing else is running
    BOOL CopySecurity;        // preserve NTFS permissions, FALSE = don't care = no special handling, result doesn't matter
    BOOL CopyAttrs;           // preserve Archive, Encrypt and Compress attributes; FALSE = don't care = no special handling, result doesn't matter to us
    BOOL PreserveDirTime;     // preserve date and time of directories
    BOOL IgnoreADS;           // ignore ADS (do not search for them in the copy source) - strips ADS and speeds up on slow networks (especially VPN)
    BOOL SkipEmptyDirs;       // skip empty directories (or directories containing only directories)
    BOOL UseMasks;            // if TRUE, the 'Masks' variable applies; otherwise no filtering
    CMaskGroup Masks;         // which files to process (Masks must be prepared)
    BOOL UseAdvanced;         // if TRUE, the 'Advanced' variable applies; otherwise no filtering
    CFilterCriteria Advanced; // advanced options
    BOOL UseSpeedLimit;       // TRUE = use the speed limit
    DWORD SpeedLimit;         // speed limit in bytes per second

public:
    CCriteriaData();

    void Reset();

    CCriteriaData& operator=(const CCriteriaData& s);

    // it returns TRUE if any criterion is set
    BOOL IsDirty();

    // if the file 'file' matches UseMasks/Masks and UseAdvanced/Advanced
    // it returns TRUE; if it doesn't match, it returns FALSE
    // NOTE: masks must be prepared beforehand
    // NOTE: advanced criteria must also be prepared
    BOOL AgreeMasksAndAdvanced(const CFileData* file);
    BOOL AgreeMasksAndAdvanced(const WIN32_FIND_DATA* file);

    // save/load to/from the Windows Registry
    // NOTE: saving is optimized, only changed values are stored; the key must be cleared before saving
    BOOL Save(HKEY hKey);
    BOOL Load(HKEY hKey);
};

// options for the Copy/Move dialog; for now only one item is stored
// if it exists, it is used as the default for newly opened Copy/Move dialogs
// eventually users will likely push us to extend this to multiple options as Find has
class CCopyMoveOptions
{
protected:
    TIndirectArray<CCriteriaData> Items;

public:
    CCopyMoveOptions() : Items(1, 1) {}

    void Set(const CCriteriaData* item); // it stores the item (if the item already exists in the array, it will be overwritten); if 'item' is NULL, the current item is cleared and the array becomes empty
    const CCriteriaData* Get();          // if an item is held, it returns a pointer to it, otherwise it returns NULL

    BOOL Save(HKEY hKey);
    BOOL Load(HKEY hKey);
};

extern CCopyMoveOptions CopyMoveOptions; // global variable holding the default options for the Copy/Move dialog

// flags for redrawing the list box
#define DRAWFLAG_ICON_ONLY 0x00000001      // draw only the part with the icon
#define DRAWFLAG_DIRTY_ONLY 0x00000002     // redraw only items that have the Dirty bit set; \
                                           // no surrounding areas or parts of text that cannot \
                                           // be affected by the selection will be drawn
#define DRAWFLAG_KEEP_DIRTY 0x00000004     // after drawing, the Dirty bit will not be cleared
#define DRAWFLAG_SKIP_VISTEST 0x00000008   // do not perform a visibility test for individual parts of the item
#define DRAWFLAG_SELFOC_CHANGE 0x00000010  // draw only changes related to focus and selection; \
                                           // this flag is used for optimization when \
                                           // FullRowSelect==FALSE => it is sufficient to draw the Name column
#define DRAWFLAG_MASK 0x00000020           // draw the icon mask
#define DRAWFLAG_NO_STATE 0x00000040       // do not use state and draw the icon in its basic colors
#define DRAWFLAG_NO_FRAME 0x00000080       // suppress drawing the frame around the text
#define DRAWFLAG_IGNORE_CLIPBOX 0x00000100 // paint will be performed for all displayed items, \
                                           // ignoring the clip box (GetClipBox); \
                                           // it is used after repainting the panel that had a dialog with a stored \
                                           // old background displayed over it; after it is closed, a full repaint is needed
#define DRAWFLAG_DRAGDROP 0x00000200       // colors for the drag&drop image
#define DRAWFLAG_SKIP_FRAME 0x00000400     // dirty hack for thumbnails: do not paint a frame around the thumbnail to avoid corrupting the alpha channel
#define DRAWFLAG_OVERLAY_ONLY 0x00000800   // draw only the overlay (no icon); used for drawing overlay icons on thumbnails

class CMainWindow;
class COperations;
class CFilesBox;
class CHeaderLine;
class CStatusWindow;
class CFilesArray;
struct CFileData;
class CIconCache;
class CSalamanderDirectory;
struct IContextMenu2;
class CPathHistory;
class CFilesWindow;
class CMenuNew;
class CMenuPopup;

// function to release the panel listing;
// deletes oldPluginData and with it all plugin data (CFileData::PluginData in
// oldPluginFSDir or oldArchiveDir), oldFiles, oldDirs
// if dealloc is FALSE, it does not delete oldFiles, oldDirs, oldPluginFSDir or oldArchiveDir,
// it only calls Clear() and DestroyMembers(); otherwise it deletes these objects
void ReleaseListingBody(CPanelType oldPanelType, CSalamanderDirectory*& oldArchiveDir,
                        CSalamanderDirectory*& oldPluginFSDir,
                        CPluginDataInterfaceEncapsulation& oldPluginData,
                        CFilesArray*& oldFiles, CFilesArray*& oldDirs, BOOL dealloc);

//****************************************************************************
//
// CFilesMap
//
// When selecting using a drag box, it holds the positions of directories and files in the panel.
//

struct CFilesMapItem
{
    WORD Width;
    CFileData* FileData;
    unsigned Selected : 1; // file selected in the file box
};

class CFilesMap
{
protected:
    CFilesWindow* Panel; // our panel

    int Rows; // number of rows and columns occupied in the map
    int Columns;
    CFilesMapItem* Map; // array [Columns, Rows]
    int MapItemsCount;  // number of items in the map

    int AnchorX; // starting point from which the box is dragged
    int AnchorY;
    int PointX; // second point defining the box (X)
    int PointY;

public:
    CFilesMap();
    ~CFilesMap();

    void SetPanel(CFilesWindow* panel) { Panel = panel; }

    BOOL CreateMap();  // allocates the map and fills it with directory and file widths
    void DestroyMap(); // releases resources and resets variables

    void SetAnchor(int x, int y); // sets the point where the user started dragging the box
    void SetPoint(int x, int y);  // sets the second point defining the box

    void DrawDragBox(POINT p); // draws the box from Anchor to p

protected:
    CFilesMapItem* GetMapItem(int column, int row);
    BOOL PointInRect(int col, int row, RECT r);

    void UpdatePanel(); // loads the selection from the Map into the panel

    // fills RECT r; values are indices into the Map array
    void GetCROfRect(int newX, int newY, RECT& r);

    // calculates the column and row in which the point [x, y] lies
    void GetCROfPoint(int x, int y, int& column, int& row);
};

inline BOOL CFilesMap::PointInRect(int col, int row, RECT r)
{
    return (col >= r.left && col <= r.right && row >= r.top && row <= r.bottom);
}

//****************************************************************************
//
// CScrollPanel
//
// Wraps methods used to scroll panel content during a drag or drag & drop
// when the cursor crosses the panel border.
//

class CScrollPanel
{
protected:
    CFilesWindow* Panel; // our panel
    BOOL ExistTimer;
    BOOL ScrollInside;      // scrolling along the panel's inner edges
    POINT LastMousePoint;   // if ScrollInside is active, helps precisely detect scroll requests
    int BeginScrollCounter; // secondary safeguard against unwanted scrolling

public:
    CScrollPanel();
    ~CScrollPanel();

    void SetPanel(CFilesWindow* panel) { Panel = panel; }

    BOOL BeginScroll(BOOL scrollInside = FALSE); // this method is called before starting the drag operation
    void EndScroll();                            // cleanup

    void OnWMTimer(); // this method is called from the panel
};

//
// *****************************************************************************
// CFileTimeStamps
//

struct CFileTimeStampsItem
{
    char *ZIPRoot,
        *SourcePath,
        *FileName,
        *DosFileName;
    FILETIME LastWrite;
    CQuadWord FileSize;
    DWORD Attr;

    CFileTimeStampsItem();
    ~CFileTimeStampsItem();

    BOOL Set(const char* zipRoot, const char* sourcePath, const char* fileName,
             const char* dosFileName, const FILETIME& lastWrite, const CQuadWord& fileSize,
             DWORD attr);
};

class CFilesWindow;

class CFileTimeStamps
{
protected:
    char ZIPFile[MAX_PATH];                   // name of the archive that stores all monitored files
    TIndirectArray<CFileTimeStampsItem> List; // list of files with data needed for their update
    CFilesWindow* Panel;                      // panel we work for

public:
    CFileTimeStamps() : List(10, 5)
    {
        ZIPFile[0] = 0;
        Panel = NULL;
    }
    ~CFileTimeStamps()
    {
        if (ZIPFile[0] != 0 ||
            List.Count > 0)
        {
            TRACE_E("Invalid work with CFileTimeStamps.");
        }
    }

    const char* GetZIPFile() { return ZIPFile; }

    void SetPanel(CFilesWindow* panel) { Panel = panel; }

    // adds a file with its timestamp to the list and keeps information that is necessary
    // for updating the file in the archive
    //
    // zipFile     - full name of the archive
    // zipRoot     - path inside the archive to the file
    // sourcePath  - path to the file on disk
    // fileName    - name of the file (both in the archive and on disk)
    // dosFileName - DOS file name (both in the archive and on disk)
    // lastWrite   - timestamp after extracting from the archive (used to check for changes)
    // fileSize    - file size after extraction (used to check for changes)
    // attr        - file attributes
    //
    // return value TRUE - the file was added; FALSE - it was not added (an error occurred or it already exists)
    BOOL AddFile(const char* zipFile, const char* zipRoot, const char* sourcePath,
                 const char* fileName, const char* dosFileName,
                 const FILETIME& lastWrite, const CQuadWord& fileSize, DWORD attr);

    // it verifies time stamps, updates if necessary and prepares the object for further use
    void CheckAndPackAndClear(HWND parent, BOOL* someFilesChanged = NULL, BOOL* archMaybeUpdated = NULL);

    // it fills a list box with names of all stored files
    void AddFilesToListBox(HWND list);

    // it removes files from all given indexes; 'indexes' is an array of indexes, 'count' is their number
    void Remove(int* indexes, int count);

    // it allows copying files from all given indexes; 'indexes' is an array of indexes, 'count' is
    // the number of them; 'parent' is the parent dialog; 'initPath' is the suggested target path
    void CopyFilesTo(HWND parent, int* indexes, int count, const char* initPath);
};

//****************************************************************************
//
// CTopIndexMem
//

#define TOP_INDEX_MEM_SIZE 50 // number of remembered top indexes (levels), at least 1

class CTopIndexMem
{
protected:
    // path for the last remembered top index; the longest is archive + archive-path so 2 * MAX_PATH
    char Path[2 * MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // stored top indexes
    int TopIndexesCount;                // number of stored top indexes

public:
    CTopIndexMem() { Clear(); }
    void Clear()
    {
        Path[0] = 0;
        TopIndexesCount = 0;
    } // clears memory
    void Push(const char* path, int topIndex);        // stores the top index for the given path
    BOOL FindAndPop(const char* path, int& topIndex); // looks for the top index of the path, FALSE -> not found
};

//******************************************************************************
//
// CDirectorySizes
//

class CDirectorySizes
{
protected:
    char* Path;                // full path to the directory whose subdirectory names and sizes are stored by us
    TDirectArray<char*> Names; // names of subdirectories
    BOOL CaseSensitive;
    BOOL NeedSort; // guard ensuring that the class is used correctly

public:
    CDirectorySizes(const char* path, BOOL caseSensitive);
    ~CDirectorySizes();

    // destroys all held data
    void Clean();

    BOOL IsGood() { return Path != NULL; }

    BOOL Add(const char* name, const CQuadWord* size);

    // if it finds the name 'name' in the Name array, it returns a pointer to its size
    // if the name is not found it returns NULL
    const CQuadWord* GetSize(const char* name);

    void Sort();

protected:
    int GetIndex(const char* name);

    friend class CDirectorySizesHolder;
};

//******************************************************************************
//
// CDirectorySizesHolder
//

#define DIRECOTRY_SIZES_COUNT 20

class CDirectorySizesHolder
{
protected:
    CDirectorySizes* Items[DIRECOTRY_SIZES_COUNT];
    int ItemsCount; // number of valid items in the Items array

public:
    CDirectorySizesHolder();
    ~CDirectorySizesHolder();

    // destroys all held data except for Path
    void Clean();

    BOOL Store(CFilesWindow* panel);

    void Restore(CFilesWindow* panel);

    // returns NULL if no item with the same path is found
    CDirectorySizes* Get(const char* path);

protected:
    // returns the index of the item whose Path matches 'path'
    // returns -1 if no such item is found
    int GetIndex(const char* path);

    CDirectorySizes* Add(const char* path);
};

//****************************************************************************
//
// CQuickRenameWindow
//

class CQuickRenameWindow : public CWindow
{
protected:
    CFilesWindow* FilesWindow;
    BOOL CloseEnabled;
    BOOL SkipNextCharacter;

public:
    CQuickRenameWindow();

    void SetPanel(CFilesWindow* filesWindow);
    void SetCloseEnabled(BOOL closeEnabled);
    BOOL GetCloseEnabled();

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CFilesWindowAncestor
//

class CFilesWindowAncestor : public CWindow // the real object core - everything private ;-)
{
private:
    char Path[MAX_PATH];      // path for a ptDisk panel - normal ("c:\path") or UNC ("\\server\share\path")
    BOOL SuppressAutoRefresh; // TRUE if the user canceled directory listing during reading and chose temporary auto-refresh suppression

    CPanelType PanelType; // type of panel (disk, archive, plugin FS)

    BOOL MonitorChanges; // should changes be monitored (auto refresh)?
    UINT DriveType;      // disk+archive: drive type of Path (see MyGetDriveType())

    // when we are inside an archive:
    CSalamanderDirectory* ArchiveDir; // content of the open archive; basic data - array of CFileData
    char ZIPArchive[MAX_PATH];        // path to the open archive
    char ZIPPath[MAX_PATH];           // path inside the open archive
    FILETIME ZIPArchiveDate;          // archive date (used for the ".." date and during refresh)
    CQuadWord ZIPArchiveSize;         // archive size - used to detect archive changes

    // when browsing a plugin file system:
    CPluginFSInterfaceEncapsulation PluginFS; // pointer to the open FS
    CSalamanderDirectory* PluginFSDir;        // content of the open FS; basic data - array of CFileData
    int PluginIconsType;                      // icon type in the panel when listing a FS

    // when viewing an archive listed by a plugin or within a plugin FS
    CPluginInterfaceAbstract* PluginIface; // use exclusively for locating the plugin in Plugins (not for invoking methods)
    int PluginIfaceLastIndex;              // index of PluginIface in Plugins during the last search, use only to locate the plugin

public:
    // contents of all columns shown in the panel (both basic data and plug-in data for archives and FS)
    CFilesArray* Files; // filtered list of files (shallow copy; basic data - CFileData structure)
    CFilesArray* Dirs;  // filtered list of directories (shallow copy; basic data - CFileData structure)
    // interface used to obtain plug-in specific data; data for plug-in columns; defines how
    // to use CFileData::PluginData; for FS plug-ins with pitFromPlugin icons, it is also used to
    // retrieving icons "in the background" in the icon reader thread - before making any changes, call SleepIconCacheThread()
    CPluginDataInterfaceEncapsulation PluginData;

    CIconList* SimplePluginIcons; // FS + pitFromPlugin only: image list with simple plug-in icons

    // current number of selected items; must be updated everywhere the variable
    // CFileData::Selected is modified
    int SelectedCount;

    // helper variables used for smoother refresh (without "the panel contains no items")
    // for plug-in file systems:
    // TRUE = the listing should not be released in a standard way but only detached (objects from NewFSXXX will continue
    // to be used)
    BOOL OnlyDetachFSListing;
    CFilesArray* NewFSFiles;                // new empty object for Files
    CFilesArray* NewFSDirs;                 // new empty object for Dirs
    CSalamanderDirectory* NewFSPluginFSDir; // new empty object for PluginFSDir
    CIconCache* NewFSIconCache;             // if not NULL, new empty object for IconCache (not here, in the descendant)

public:
    CFilesWindowAncestor();
    ~CFilesWindowAncestor();

    // NULL -> Path; echo && err != ERROR_SUCCESS -> only report the error
    // 'parent' is the parent of the message box (NULL == HWindow)
    DWORD CheckPath(BOOL echo, const char* path = NULL, DWORD err = ERROR_SUCCESS,
                    BOOL postRefresh = TRUE, HWND parent = NULL);

    // destroys PluginData and with it all plugin data (CFileData::PluginData in
    // PluginFSDir or ArchiveDir), Files and Dirs and resets SelectedCount
    // 1. NOTE: does not destroy Files, Dirs, PluginFSDir or ArchiveDir; it only calls Clear() and DestroyMembers()
    // 2. NOTE: for FS with pitFromPlugin icons you must call SleepIconCacheThread()
    //          before destroying PluginFSDir and PluginData because icons are
    //          retrieved using PluginFSDir and PluginData ...
    void ReleaseListing();

    // returns the path shown in the panel (disk, archive, or FS);
    // if convertFSPathToExternal is TRUE and the panel holds an FS path,
    // CPluginInterfaceForFSAbstract::ConvertPathToExternal() is called
    // it returns TRUE if the path fits into the buffer completely, otherwise a truncated path is returned
    BOOL GetGeneralPath(char* buf, int bufSize, BOOL convertFSPathToExternal = FALSE);

    const char* GetPath() { return Path; }
    BOOL Is(CPanelType type) { return type == PanelType; }
    CPanelType GetPanelType() { return PanelType; }
    BOOL GetMonitorChanges() { return MonitorChanges; }
    BOOL GetNetworkDrive() { return DriveType == DRIVE_REMOTE; }
    UINT GetPathDriveType() { return DriveType; }
    CSalamanderDirectory* GetArchiveDir() { return ArchiveDir; }
    const char* GetZIPArchive() { return ZIPArchive; }
    const char* GetZIPPath() { return ZIPPath; }
    FILETIME GetZIPArchiveDate() { return ZIPArchiveDate; }
    BOOL IsSameZIPArchiveSize(const CQuadWord& size) { return ZIPArchiveSize == size; }
    CQuadWord GetZIPArchiveSize() { return ZIPArchiveSize; }
    BOOL GetSuppressAutoRefresh() { return SuppressAutoRefresh; }

    void SetPath(const char* path);
    void SetMonitorChanges(BOOL monitorChanges) { MonitorChanges = monitorChanges; }
    void SetPanelType(CPanelType type) { PanelType = type; }
    void SetZIPPath(const char* path);
    void SetZIPArchive(const char* archive);
    void SetArchiveDir(CSalamanderDirectory* dir) { ArchiveDir = dir; }
    void SetZIPArchiveDate(FILETIME& time) { ZIPArchiveDate = time; }
    void SetZIPArchiveSize(const CQuadWord& size) { ZIPArchiveSize = size; }
    void SetSuppressAutoRefresh(BOOL suppress) { SuppressAutoRefresh = suppress; }

    // if the 'zipPath' parameter is NULL, the path ZIPPath is used
    CFilesArray* GetArchiveDirFiles(const char* zipPath = NULL);
    CFilesArray* GetArchiveDirDirs(const char* zipPath = NULL);

    // compares this object's Path with that of 'other' to work around change-notify issues (see snooper)
    BOOL SamePath(CFilesWindowAncestor* other);

    // determines whether the path 'fsName':'fsUserPart' can be opened within the active FS
    // i.e. whether the FS must be closed or if it's enough to just change the "directory" within the currently open FS;
    // if 'convertPathToInternal' is TRUE,'fsUserPart' (buffer of at least MAX_PATH characters) is converted to the
    // internal format before testing and 'convertPathToInternal' is set to FALSE;
    // if the method returns TRUE, it also returns the index 'fsNameIndex' of the plugin FS name "fsName" of the plugin
    BOOL IsPathFromActiveFS(const char* fsName, char* fsUserPart, int& fsNameIndex,
                            BOOL& convertPathToInternal);

    CPluginFSInterfaceEncapsulation* GetPluginFS() { return &PluginFS; }
    CSalamanderDirectory* GetPluginFSDir() { return PluginFSDir; }
    int GetPluginIconsType() { return PluginIconsType; }

    CFilesArray* GetFSFiles();
    CFilesArray* GetFSDirs();

    void SetPluginFS(CPluginFSInterfaceAbstract* fsIface, const char* dllName,
                     const char* version, CPluginInterfaceForFSAbstract* ifaceForFS,
                     CPluginInterfaceAbstract* iface, const char* pluginFSName,
                     int pluginFSNameIndex, DWORD pluginFSCreateTime,
                     int chngDrvDuplicateItemIndex, int builtForVersion)
    {
        PluginFS.Init(fsIface, dllName, version, ifaceForFS, iface, pluginFSName,
                      pluginFSNameIndex, pluginFSCreateTime, chngDrvDuplicateItemIndex,
                      builtForVersion);
    }
    void SetPluginFSDir(CSalamanderDirectory* pluginFSDir) { PluginFSDir = pluginFSDir; }
    void SetPluginIconsType(int type) { PluginIconsType = type; }

    void SetPluginIface(CPluginInterfaceAbstract* pluginIface)
    {
        PluginIface = pluginIface;
        PluginIfaceLastIndex = -1;
    }

    // returns CPluginData containing the interface 'PluginIface'; returns NULL if it does not exist
    // NOTE: the pointer is valid only until the number of plugins changes (the array grows/shrinks)
    CPluginData* GetPluginDataForPluginIface();
};

//****************************************************************************
//
// CFilesWindow
//

struct CViewerMasksItem;
struct CEditorMasksItem;
struct CHighlightMasksItem;
class CDrivesList;

// modes for CFilesWindow::CopyFocusedNameToClipboard
enum CCopyFocusedNameModeEnum
{
    cfnmShort, // only the short name (example.txt)
    cfnmFull,  // full path (c:\example.txt)
    cfnmUNC    // full UNC path (\\server\share\example.txt)
};

// array for prioritizing icon and thumbnail loading by the icon reader according
// to the current state of the panel and displayed items
class CVisibleItemsArray
{
protected:
    CRITICAL_SECTION Monitor; // section used to synchronize this object (monitor behavior)

    BOOL SurroundArr; // TRUE/FALSE = array of items around the visible area / array ofitems only from the visible area

    int ArrVersionNum; // version of the array
    BOOL ArrIsValid;   // is the array filled and valid?

    char** ArrNames;       // allocated array of names that are currently visible in the panel (names are only references into Files+Dirs in the (CFileData: :Name) panel)
    int ArrNamesCount;     // number of names in ArrNames
    int ArrNamesAllocated; // number of allocated namesfor ArrNames

    int FirstVisibleItem; // index of the first visible item
    int LastVisibleItem;  // index of the last visible item

public:
    CVisibleItemsArray(BOOL surroundArr);
    ~CVisibleItemsArray();

    // Returns TRUE if the array is filled and valid (matches the current state of thepanel
    // and visible items), also returns the number of the array version in 'versionNum'
    // (if it is not NULL), otherwise it returns FALSE ('versionNum' is also returnedin this case)
    // Called by both - the panel and the icon reader
    BOOL IsArrValid(int* versionNum);

    // Reports a change in the panel or visible items; invalidates the array
    // Called only by the panel
    void InvalidateArr();

    // it is used to refresh the array based on the current state of the panel and displayed items
    // it increments the version number and marks it valid. Called only by the
    // panel while in idle mode
    void RefreshArr(CFilesWindow* panel);

    // If the array is filled and valid and contains 'name', it returns TRUE; it also
    // returns TRUE in 'isArrValid' if the array is filled and valid and in 'versionNum' the number of the array version
    // called only by the icon reader
    BOOL ArrContains(const char* name, BOOL* isArrValid, int* versionNum);

    // If the array is filled and valid and contains the given index 'index', it returns
    // TRUE; moreover, in 'isArrValid', it returns TRUE if the array is filled and valid and in 'versionNum' the number of the array version
    // Called only by the icon reader
    BOOL ArrContainsIndex(int index, BOOL* isArrValid, int* versionNum);
};

enum CTargetPathState // state of the target path when building the operation script
{
    tpsUnknown, // used only to detect the initial state of the target path
    tpsEncryptedExisting,
    tpsEncryptedNotExisting,
    tpsNotEncryptedExisting,
    tpsNotEncryptedNotExisting,
};

// helper function determining the state of the target path based on the state of the parent directory and the target path
CTargetPathState GetTargetPathState(CTargetPathState upperDirState, const char* targetPath);

#ifndef HDEVNOTIFY
typedef PVOID HDEVNOTIFY;
#endif // HDEVNOTIFY

enum CViewModeEnum;

class CFilesWindow : public CFilesWindowAncestor
{
public:
    CViewTemplate* ViewTemplate;            // pointer to the template defining mode, name and visibility
                                            // of the standard Salamander columns VIEW_SHOW_xxxx
    BOOL NarrowedNameColumn;                // TRUE = Name column smart mode is enabled and it had to be narrowed
    DWORD FullWidthOfNameCol;               // only with smart mode: measured width of the Name column (width before narrowing)
    DWORD WidthOfMostOfNames;               // only with smart mode: width of most names in the Name column (e.g. 85% of names are shorter than or equal to this value)
    TDirectArray<CColumn> Columns;          // columns displayed in the panel. Filled by Salamander by default
                                            // this array based on the template pointed to by a variable 'ViewTemplate'.
                                            // If a filesystem is attached, it may modify these columns: new
                                            // columns can appear and some of the visible ones may be temporarily removed
    TDirectArray<CColumn> ColumnsTemplate;  // template for the Columns variable
                                            // Firstly, this template is built from the current 'ViewTemplate'
                                            // and panel content type (disk / archive + FS). Then the template is
                                            // copied into Columns. Added for performance so we do not build the
                                            // array repeatedly.
    BOOL ColumnsTemplateIsForDisk;          // TRUE = if ColumnsTemplate was built for a disk, otherwise for archive or FS
    FGetPluginIconIndex GetPluginIconIndex; // callback for retrieving a simple icon index for plug-ins
                                            // with their own icons (FS, pitFromPlugin)

    CFilesMap FilesMap;        // used for selecting items by dragging a selection box
    CScrollPanel ScrollObject; // used for automatic scrolling while working with the mouse

    CIconCache* IconCache;                // cache containing icons directly from files
    BOOL IconCacheValid;                  // is the cache already loaded?
    BOOL InactWinOptimizedReading;        // TRUE = only icons/thumbnails/overlays from the visible part of the panel are being read (used when the main window is inactive and a refresh is triggered – we try to minimize system load as we're "in the background")
    DWORD WaitBeforeReadingIcons;         // how many milliseconds to wait before the icon reader starts reading icons (used on refresh; while waiting old icons can be pushed into the cache to avoid repeated reading and endless refreshes on network drives)
    DWORD WaitOneTimeBeforeReadingIcons;  // how many milliseconds to wait before starting to read icons, then this value resets (used to catch batches of changes from Tortoise SVN, see IconOverlaysChangedOnPath())
    DWORD EndOfIconReadingTime;           // GetTickCount() from the moment all icons were loaded in the panel
    HANDLE IconCacheThread;               // handle of the icon - reading the thread
    HANDLE ICEventTerminate;              // signaled -> terminate the thread
    HANDLE ICEventWork;                   // signaled -> start reading icons
    BOOL ICSleep;                         // TRUE -> leave the icon-reading loop
    BOOL ICWorking;                       // TRUE -> inside the icon-reading loop
    BOOL ICStopWork;                      // TRUE -> do not even start the icon-reading loop
    CRITICAL_SECTION ICSleepSection;      // critical section -> sleep-icon-thread must pass through it
    CRITICAL_SECTION ICSectionUsingIcon;  // critical section -> image-list is used inside
    CRITICAL_SECTION ICSectionUsingThumb; // critical section -> thumbnail is used inside

    BOOL AutomaticRefresh;      // is the panel refreshed automatically (or manually)?
    BOOL FilesActionInProgress; // is work already being prepared or executed for the Worker?

    CDrivesList* OpenedDrivesList; // if the Alt+F1(2) menu is open, this value points to it; otherwise it is NULL

    int LastFocus; // to avoid unnecessary overwriting of the status line

    CFilesBox* ListBox;
    CStatusWindow *StatusLine,
        *DirectoryLine;

    BOOL StatusLineVisible;
    BOOL DirectoryLineVisible;
    BOOL HeaderLineVisible;

    CMainWindow* Parent;

    //CPanelViewModeEnum ViewMode;      // thumbnails / brief / detailed look of the panel
    DWORD ValidFileData; // it determines which CFileData variables are valid, see VALID_DATA_XXX constants; set via SetValidFileData()

    CSortType SortType;       // criterion used for sorting
    BOOL ReverseSort;         // reverse order
    BOOL SortedWithRegSet;    // used to monitor changes of the global variable Configuration.SortUsesLocale
    BOOL SortedWithDetectNum; // used to monitor changes of the global variable Configuration.SortDetectNumbers

    char DropPath[2 * MAX_PATH];  // buffer for the current directory used in a drop operation
    char NextFocusName[MAX_PATH]; // the name that will receive focus on the next refresh
    BOOL DontClearNextFocusName;  // TRUE = do not clear NextFocusName when the main Salamander window is activated
    BOOL FocusFirstNewItem;       // refresh: should the newly added item be selected? (for system New)
    CTopIndexMem TopIndexMem;     // memory of top index for Execute()

    int LastRefreshTime; // used to handle the chaos of directory change notifications

    BOOL CanDrawItems;         // can items be redrawn in the list box?
    BOOL FileBasedCompression; // is file-based compression supported?
    BOOL FileBasedEncryption;  // is file-based encryption supported?
    BOOL SupportACLS;          // does the FS support permissions (NTFS)?

    HDEVNOTIFY DeviceNotification; // WARNING: access only in the snooper critical section: for ptDisk
                                   // and removable media only: handle for registering the panel window
                                   // as a recipient of device event notifications (used to detect the media before
                                   // it is disconnected from the computer)

    // should icons be retrieved from files on this disk??
    // For ptDisk: TRUE if icons are retrieved from files; for ptZIPArchive: TRUE
    // if icons are retrieved from the registry; for ptPluginFs: TRUE if icons are retrieved from the registry (pitFromRegistry) or
    // directly from the plug-in (pitFromPlugin)
    BOOL UseSystemIcons;
    BOOL UseThumbnails; // TRUE when thumbnails are displayed in the panel and loaded in the icon reader

    int DontDrawIndex; // index of the item that should never be redrawn in the list box
    int DrawOnlyIndex; // index of the item that should be redrawn alone in the list box

    IContextMenu2* ContextMenu;  // current context menu (for owner-draw menus)
    CMenuNew* ContextSubmenuNew; // current context submenu New (for owner-draw menus)

    BOOL NeedRefreshAfterEndOfSM;         // will a refresh be needed after exiting suspend mode?
    int RefreshAfterEndOfSMTime;          // "time" of the latest refresh that arrived after suspend mode started
    BOOL PluginFSNeedRefreshAfterEndOfSM; // will the plug-in FS need a refresh after leaving suspend mode?

    BOOL SmEndNotifyTimerSet;  // TRUE when the timer for sending WM_USER_SM_END_NOTIFY_DELAYED is running
    BOOL RefreshDirExTimerSet; // TRUE when the timer for sending WM_USER_REFRESH_DIR_EX_DELAYED is running
    LPARAM RefreshDirExLParam; // lParam for sending WM_USER_REFRESH_DIR_EX_DELAYED

    BOOL InactiveRefreshTimerSet;   // TRUE when the timer for sending WM_USER_INACTREFRESH_DIR is running
    LPARAM InactRefreshLParam;      // lParam for sending WM_USER_INACTREFRESH_DIR
    DWORD LastInactiveRefreshStart; // info about the last snooper-initiated refresh in an inactive window: when did it start + matching the line below...
    DWORD LastInactiveRefreshEnd;   // info about the last snooper-initiated refresh in an inactive window: when did it finish + equality with LastInactiveRefreshStart means that no such refresh has occurred since the last deactivation

    BOOL NeedRefreshAfterIconsReading; // is a refresh needed after icon reading finishes?
    int RefreshAfterIconsReadingTime;  // "time" of the latest refresh that arrived while icons were being read

    CPathHistory* PathHistory; // browsing history for this panel (for the panel)

    DWORD HiddenDirsFilesReason; // bit field indicating the reason why files/directories are hidden (HIDDEN_REASON_xxx)
    int HiddenDirsCount,         // number of hidden directories in the panel (number of skipped ones)
        HiddenFilesCount;        // number of hidden files in the panel (number of skipped ones)

    HANDLE ExecuteAssocEvent;           // event that "triggers" deletion of files unpacked in ExecuteFromArchive
    BOOL AssocUsed;                     // was unpacking via associations used?
    CFileTimeStamps UnpackedAssocFiles; // list of unpacked files with timestamps (last-write)

    CMaskGroup Filter;  // filter for the panel
    BOOL FilterEnabled; // is the filter enabled

    BOOL QuickSearchMode;           // Quick Search mode?
    short CaretHeight;              // it is set when measuring the font in CFilesWindow
    char QuickSearch[MAX_PATH];     // name of the file that was sought via Quick Search
    char QuickSearchMask[MAX_PATH]; // quick search mask (may contain '/' after any number of characters)
    int SearchIndex;                // position of the cursor during Quick Search

    int FocusedIndex;  // current caret position
    BOOL FocusVisible; // is focus displayed?

    int DropTargetIndex;          // current drop target position
    int SingleClickIndex;         // current cursor position in SingleClick mode
    int SingleClickAnchorIndex;   // cursor position in SingleClick mode where the user pressed the left button
    POINT OldSingleClickMousePos; // old cursor position

    POINT LButtonDown;     // used when "tearing" files or directories for dragging
    DWORD LButtonDownTime; // used for "timed tearing" (minimum time between down and up)

    BOOL TrackingSingleClick; // are we "highlighting" the item under the cursor?
    BOOL DragBoxVisible;      // selection box is visible
    BOOL DragBox;             // we are dragging the selection box
    BOOL DragBoxLeft;         // 1 = left, 0 = right mouse button
    BOOL ScrollingWindow;     // set to TRUE if window scrolling was initiated by us (handles hiding the box)

    POINT OldBoxPoint; // selection box

    BOOL SkipCharacter; // it skips translated accelerators
    BOOL SkipSysCharacter;

    //j.r.: ShiftSelect is not needed at all
    //    BOOL       ShiftSelect;           // selecting using the keyboard (SelectItems is the state)
    BOOL DragSelect;           // mode for marking/unmarking by mouse drag
    BOOL BeginDragDrop;        // are we dragging the file?
    BOOL DragDropLeftMouseBtn; // TRUE = drag&drop with left mouse button, FALSE = with the right one
    BOOL BeginBoxSelect;       // are we "opening" the selection box?
    BOOL PersistentTracking;   // during WM_CAPTURECHANGED, tracking mode will not be disabled
    BOOL SelectItems;          // during Drag Select, do we mark items?
    BOOL FocusedSinceClick;    // the item already had focus when we clicked it

    BOOL CutToClipChanged; // is at least one CutToClip flag set on files/directories?

    BOOL PerformingDragDrop; // a drag&drop operation is in progress right now

    BOOL UserWorkedOnThisPath; // TRUE only if the user performed an action on the current path (F3, F4, F5, ...)
    BOOL StopThumbnailLoading; // if it is TRUE, icon-cache data about "thumbnail loaders" cannot be used (plugin unload/remove in progress)

    int EnumFileNamesSourceUID; // source UID for enumerating names in viewers

    CNames OldSelection; // selection before the operation, intended for the Reselect command
    CNames HiddenNames;  // names of files and directories that the user has hidden via the Hide function (unrelated to the Hidden attribute)

    CQuickRenameWindow QuickRenameWindow;
    UINT_PTR QuickRenameTimer;
    int QuickRenameIndex;
    RECT QuickRenameRect;

    CVisibleItemsArray VisibleItemsArray;         // array of items visible in the panel
    CVisibleItemsArray VisibleItemsArraySurround; // array of items adjacent to the visible part of the panel

    BOOL TemporarilySimpleIcons; // use simple icons until the next ReadDirectory()

    int NumberOfItemsInCurDir; // only for ptDisk: number of items returned by FindFirstFile+FindNextFile for the current path (used to detect changes on network and unmonitored paths when dropping to the panel via Explorer)

    BOOL NeedIconOvrRefreshAfterIconsReading; // is icon overlay refresh required after icon loading finishes?
    DWORD LastIconOvrRefreshTime;             // GetTickCount() of the last icon-overlay refresh (see IconOverlaysChangedOnPath())
    BOOL IconOvrRefreshTimerSet;              // TRUE if the timer for icon-overlay refresh is running (see IconOverlaysChangedOnPath())
    DWORD NextIconOvrRefreshTime;             // time when tracking icon-overlay changes makes sense again for this panel (see IconOverlaysChangedOnPath())

public:
    CFilesWindow(CMainWindow* parent);
    ~CFilesWindow();

    BOOL IsGood() { return DirectoryLine != NULL &&
                           StatusLine != NULL && ListBox != NULL && Files != NULL && Dirs != NULL &&
                           IconCache != NULL && PathHistory != NULL && ContextSubmenuNew != NULL &&
                           ExecuteAssocEvent != NULL; }

    // returns FALSE for ptDisk, TRUE for ptZIPArchive and ptPluginFS
    // when their CSalamanderDirectory has the SALDIRFLAG_CASESENSITIVE flag set
    BOOL IsCaseSensitive();

    // clears all stored histories
    void ClearHistory();

    // suspends or resumes the icon reader-either stops accessing IconCache or starts loading
    // icons into IconCache; both methods can be called repeatedly and the last call wins
    void SleepIconCacheThread();
    void WakeupIconCacheThread();

    // called when Salamander's main window is activated/deactivated
    void Activate(BOOL shares);

    // called to inform the panel about changes on 'path'; when 'includingSubdirs' is TRUE,
    // changes may also occur in subdirectories
    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // called to notify the panel about icon overlay changes on 'path' (mainly from Tortoise SVN)
    void IconOverlaysChangedOnPath(const char* path);

    // tries whether the path is accessible, restoring network connections using
    // CheckAndRestoreNetworkConnection and CheckAndConnectUNCNetworkPath if needed;
    // returns TRUE if the path is accessible
    BOOL CheckAndRestorePath(const char* path);

    // recognizes the path type (FS/Windows/archive) and splits it into components:
    // for FS paths it's fs-name and fs-user-part; for archives it's path-to-archive and
    // path-in-archive; for Windows paths it's an existing part and the remaining path. for FS paths, nothing is checked,
    // for Windows (normal/UNC) paths, it checks how far the path exists (possibly restore network paths),
    // for archives, it checks whether the archive file exists (determined by extension);
    // 'path' is a full or relative path (for relative paths, the path in the active panel is used as the base for evaluating the full path). The resulting full path is
    // stored back into 'path' (buffer must be at least 'pathBufSize' characters). Returns TRUE
    // when recognized successfully, setting 'type' to PATH_TYPE_XXX and 'pathPart' as follows:
    // - for Windows paths, pointer just after the existing path (after '\\' or at the end of string);
    //   if a file exists in the path, it points after the path to this file, WARNING: the returned part length is not
    //   checked and may exceed MAX_PATH.
    // - for archive paths, pointer past the archive file; WARNING: again the length inside the archive is not checked and can
    //   exceed MAX_PATH.
    // - for FS paths, pointer after ':' following the file-system name (user - part of the path);
    //   WARNING: length of user - part path isn't checked and may exceed MAX_PATH.
    // On success, 'isDir' is TRUE if the first part of the path up to 'pathPart' is a directory,
    // FALSE if it's a file (Windows paths). For archive and FS paths, 'isDir' is FALSE.
    // If it returns FALSE, an error that occurred during recognition was already displayed to the user, 'errorTitle' is the message box title with the error.
    // If 'nextFocus' is not NULL and the Windows/archive path doesn't contain '\\' or ends with
    // it, the path is copied to 'nextFocus' (see SalGetFullName)
    BOOL ParsePath(char* path, int& type, BOOL& isDir, char*& secondPart, const char* errorTitle,
                   char* nextFocus, int* error, int pathBufSize);

    void Execute(int index); // enter + l_dblclk
    // file from archive: execute (edit==FALSE) or edit (edit + editWithMenuParent != NULL means
    // "edit with")
    void ExecuteFromArchive(int index, BOOL edit = FALSE, HWND editWithMenuParent = NULL,
                            const POINT* editWithMenuPoint = NULL);

    void ChangeSortType(CSortType newType, BOOL reverse, BOOL force = FALSE);

    // change drive to DefaultDir[drive], optionally offering a drive menu;
    // when 0, a dialog is shown, the change is applied immediately
    void ChangeDrive(char drive = 0);

    // it finds the first fixed drive and switches to it;
    // 'parent' is the parent of message boxes;
    // if 'noChange' is not NULL it returns TRUE if the panel listing data (Files + Dirs)
    // were not recreated again;
    // if 'refreshListBox' is FALSE, RefreshListBox is not called;
    // if 'canForce' is TRUE, the user can forcibly close even a path the plug-in refuses to close;
    // if 'failReason' != NULL, it is set to one of the CHPPFR_XXX constants;
    // only for FS in panel: 'tryCloseReason' is the reason passed to CPluginFSInterfaceAbstract::TryCloseOrDetach();
    // returns TRUE on success
    BOOL ChangeToFixedDrive(HWND parent, BOOL* noChange = NULL, BOOL refreshListBox = TRUE,
                            BOOL canForce = FALSE, int* failReason = NULL,
                            int tryCloseReason = FSTRYCLOSE_CHANGEPATH);

    // tries to change the path to Configuration.IfPathIsInaccessibleGoTo; if it fails,
    // attempts to switch to a fixed drive (calls ChangeToFixedDrive). Returns TRUE on success
    BOOL ChangeToRescuePathOrFixedDrive(HWND parent, BOOL* noChange = NULL, BOOL refreshListBox = TRUE,
                                        BOOL canForce = FALSE, int tryCloseReason = FSTRYCLOSE_CHANGEPATH,
                                        int* failReason = NULL);

    // helper method:
    // serves as preparation for CloseCurrentPath, prepares closing/detached the current path (updates
    // edited files and calls CanCloseArchive for archives; TryCloseOrDetach for FS);
    // returns TRUE if the path can be closed/detached by the upcoming CloseCurrentPath call,
    // returns FALSE if it cannot close/defer(the current path won't change or close);
    // when 'canForce' is TRUE, the user may forcibly close even a path the plug-in doesn't want
    // to close (necessary when closing Salamander – if there is a bug in the plug-in, Salamander would otherwise not close);
    // when 'canForce' is FALSE and 'canDetach' is TRUE and it's an FS path, the path may be
    // closed (returns 'detachFS' FALSE) or detached (returns 'detachFS' TRUE). In other cases the
    // path can only be closed (returns 'detachFS' FALSE). For FS in the panel only, 'tryCloseReason'
    // is the reason passed to CPluginFSInterfaceAbstract::TryCloseOrDetach()
    // 'parent' is the parent message box
    BOOL PrepareCloseCurrentPath(HWND parent, BOOL canForce, BOOL canDetach, BOOL& detachFS,
                                 int tryCloseReason);
    // helper method:
    // finishes closing/detaching the current path started by PrepareCloseCurrentPath; if 'cancel'
    // is TRUE the current path is restored (it is neither closed nor changed; triggers
    // Event(FSE_CLOSEORDETACHCANCELED) for FS). If 'cancel' is FALSE all no-longer-needed
    // resources of the current path are released (depending on path type: Files, Dirs, PluginData,
    // ArchiveDir, PluginFS and PluginFSDir). 'detachFS' is the value returned from
    // PrepareCloseCurrentPath (meaningful only for FS; if TRUE, PluginFS is added to DetachedFSList
    // instead of being freed). When closing an FS with pitFromPlugin icons you must first call
    // SleepIconCacheThread() so PluginData isn't released while its method loads icons.
    // 'parent' is the message box parent; 'newPathIsTheSame' is TRUE (meaningful only if 'cancel'
    // is FALSE) if the same path ends up in the panel again after closing the path (for example a successful path refresh);
    // 'isRefresh' is TRUE for a hard refresh (Ctrl+R or change notification);
    // if 'canChangeSourceUID' is TRUE you may change EnumFileNamesSourceUID and with that cancel enumeration
    // of files from the panel (e.g. for the viewer). FALSE is used when changing to the same path
    // that was already in the panel (similar to refresh via hot-path, focus-name, etc.)
    void CloseCurrentPath(HWND parent, BOOL cancel, BOOL detachFS, BOOL newPathIsTheSame,
                          BOOL isRefresh, BOOL canChangeSourceUID);

    // helper method:
    // On the FS represented by 'pluginFS' changes the path to 'fsName':'fsUserPart' or, if
    // inaccessible, to the nearest shorter accessible path. Returns FALSE when the path (including
    // subpaths) is inaccessible; returns TRUE if the path was successfully changed and listed. In that case
    // 'dir' holds listed files and directories, 'pluginData' contains the plug-in interface for
    // additional columns. 'shorterPath' is FALSE if the exact requested (non-shortened) path was
    // listed, otherwise TRUE (shortened path) and 'pluginIconsType' is the icon type used in the
    // panel. If 'firstCall' is TRUE the code checks whether shortening back to the original path
    // occurred ("access denied" directory-immediate return); if so, all data are preserved and
    // *'cancel' is set to TRUE, otherwise *'cancel' is FALSE. 'mode' is the path change mode
    // (see ChangePathToPluginFS). 'currentPath' (if it is not NULL) is the user part of the original FS path
    // for checking shortening back to the original path; if 'forceUpdate' is TRUE the case where the new path equals the current one
    // is not optimized (cancels the effect of 'firstCall' TRUE). If 'cutFileName' is not NULL it is a
    // MAX_PATH buffer that returns a file name (without path) returned by the plug-in when the path
    // contains a file name (the path was shortened and the file will be focused); otherwise
    // 'cutFileName' returns an empty string. If 'keepOldListing' is not NULL and it is TRUE ('dir' must be
    // GetPluginFSDir() in this case) the listing will be retrieved into a temporary object (instead of
    // directly into 'dir') and moved (just pointer swap) into 'dir' only after successful loading. If allocation of the
    // temporary object fails, 'keepOldListing' is set to FALSE and the original listing may be
    // deleted if the path changes to an FS; otherwise the original listing is kept unchanged.
    BOOL ChangeAndListPathOnFS(const char* fsName, int fsNameIndex, const char* fsUserPart,
                               CPluginFSInterfaceEncapsulation& pluginFS, CSalamanderDirectory* dir,
                               CPluginDataInterfaceAbstract*& pluginData, BOOL& shorterPath,
                               int& pluginIconsType, int mode, BOOL firstCall,
                               BOOL* cancel, const char* currentPath,
                               int currentPathFSNameIndex, BOOL forceUpdate,
                               char* cutFileName, BOOL* keepOldListing);

    // path change-handles both relative and absolute paths to Windows form (UNC and C:\path);
    // shortens the path if needed. When changing within the same drive (including archives)
    // it finds a valid directory even if that means switching to a fixed drive (when the current drive is inaccessible);
    // 'parent' is the parent of the message box;
    // if suggestedTopIndex != -1 the top index will be set;
    // if suggestedFocusName != NULL and present in the new list it will be focused;
    // if noChange (if not NULL) it returns TRUE when the listing data in the panel were not recreated
    // (Files + Dirs);
    // if refreshListBox is FALSE RefreshListBox is not called;
    // if canForce is TRUE, the user can forcibly close even a path the plug-in refuses to close;
    // if isRefresh is TRUE, this call comes from RefreshDirectory (no error leading to shortening is shown,
    // quick search is not canceled);
    // if failReason != NULL, it is set to one of the CHPPFR_XXX constants;
    // if shorterPathWarning is TRUE, a message box with an error is opened when the path is shortened
    // (only when it is not a refresh);
    // only for FS in the panel: 'tryCloseReason' is the reason passed to CPluginFSInterfaceAbstract::TryCloseOrDetach()
    // returns TRUE if the requested path was listed successfully
    BOOL ChangePathToDisk(HWND parent, const char* path, int suggestedTopIndex = -1,
                          const char* suggestedFocusName = NULL, BOOL* noChange = NULL,
                          BOOL refreshListBox = TRUE, BOOL canForce = FALSE, BOOL isRefresh = FALSE,
                          int* failReason = NULL, BOOL shorterPathWarning = TRUE,
                          int tryCloseReason = FSTRYCLOSE_CHANGEPATH);
    // changes to an archive path; only absolute Windows paths are allowed (archive is UNC or C:\path\archive)
    // if suggestedTopIndex != -1, the top index will be set;
    // if suggestedFocusName != NULL, and present in the new list, it will be focused;
    // if forceUpdate is TRUE, the case where the new path equals the current one is not optimized;
    // if noChange is not NULL, it returns TRUE if the listing data in the panel were not recreated
    //   (ArchiveDir + PluginData);
    // if refreshListBox is FALSE, RefreshListBox is not called;
    // if failReason != NULL it is set to one of the CHPPFR_XXX constants;
    // if isRefresh is TRUE, this call comes from RefreshDirectory (quick search is not canceled);
    // if archivePath contains a file name and canFocusFileName is TRUE, that file is focused
    //   (returns FALSE because the path was shortened);
    // if isHistory is TRUE (used when selecting a path from a path history) and the archive cannot
    //   be opened (or does not exist), the panel opens at least the path to the archive
    //   (optionally shortened, on path error it does not switch to a fixed drive);
    // returns TRUE if the requested path was uccessfully listed
    BOOL ChangePathToArchive(const char* archive, const char* archivePath, int suggestedTopIndex = -1,
                             const char* suggestedFocusName = NULL, BOOL forceUpdate = FALSE,
                             BOOL* noChange = NULL, BOOL refreshListBox = TRUE, int* failReason = NULL,
                             BOOL isRefresh = FALSE, BOOL canFocusFileName = FALSE, BOOL isHistory = FALSE);
    // change path to the plug-in FS;
    // if suggestedTopIndex != -1 the top index will be set;
    // if suggestedFocusName != NULL and present in the new list, it will be focused;
    // if forceUpdate is TRUE, the case where the new path equals the current one is not optimized;
    // 'mode' is the path change mode:
    //   1 (refresh path) - shortens the path, if needed; do not report path not found (just shorten them),
    //                      report a file instead of path, inaccessibility of path and other errors
    //   2 (called via ChangePanelPathToPluginFS from plugin, back/forward in history, etc.) - shortens the path, if needed;
    //                      report all path errors (file instead of path, not found, not accessible, ...)
    //   3 (change-dir command) - shortens the path only if it is a file or the path cannot be listed
    //                      (ListCurrentPath returns FALSE); do not report file instead of path
    //                      (silent shorten and return the file name), report all other path errors (not found, not accessible, ...)
    // if noChange is not NULL, it returns TRUE when the listing data in the panel were not recreated
    //   (PluginFSDir + PluginData);
    // if refreshListBox is FALSE, RefreshListBox is not called;
    // if failReason != NULL, it is set to one of the CHPPFR_XXX constants;
    // if isRefresh is TRUE, this call comes from RefreshDirectory (quick search is not canceled);
    // if fsUserPart contains a file name and canFocusFileName is TRUE, the file is focused
    //   (returns FALSE because the path was shortened);
    // if 'convertPathToInternal' is TRUE, CPluginInterfaceForFSAbstract::ConvertPathToInternal() is called;
    // returns TRUE if the requested path was listed successfully
    BOOL ChangePathToPluginFS(const char* fsName, const char* fsUserPart, int suggestedTopIndex = -1,
                              const char* suggestedFocusName = NULL, BOOL forceUpdate = FALSE,
                              int mode = 2, BOOL* noChange = NULL, BOOL refreshListBox = TRUE,
                              int* failReason = NULL, BOOL isRefresh = FALSE,
                              BOOL canFocusFileName = FALSE, BOOL convertPathToInternal = FALSE);
    // change path to a detached plug-in FS (in MainWindow->DetachedFSList at index 'fsIndex');
    // if suggestedTopIndex != -1, the top index will be set;
    // if suggestedFocusName != NULL and present in the new list, it will be selected;
    // if refreshListBox is FALSE, RefreshListBox is not called;
    // if failReason != NULL, it is set to one of the CHPPFR_XXX constants;
    // if both newFSName and newUserPart are not NULL, the path should be changed to
    //   'newFSName':'newUserPart' before attaching the FS;
    // the meaning of 'mode' parameter is the same as in ChangePathToPluginFS; the value -1 means
    //   mode = newUserPart == NULL ? 1 : 2;
    // if fsUserPart contains a file name and canFocusFileName is TRUE, the file is focused
    //   (returns FALSE because the path was shortened);
    // returns TRUE if the requested path was successfully listed
    BOOL ChangePathToDetachedFS(int fsIndex, int suggestedTopIndex = -1,
                                const char* suggestedFocusName = NULL, BOOL refreshListBox = TRUE,
                                int* failReason = NULL, const char* newFSName = NULL,
                                const char* newUserPart = NULL, int mode = -1,
                                BOOL canFocusFileName = FALSE);
    // changes the panel path; the input may be an absolute or relative Windows path or an archive path
    // or an FS path (absolute/relative is handled directly by the plug-in). If the input path points to a file,
    // that file is focused;
    // if suggestedTopIndex != -1, the top index will be set;
    // if suggestedFocusName != NULL and present in the new list, it will be selected;
    // 'mode' specifies the change mode for FS paths-see ChangePathToPluginFS(); it has no meaning
    // for archives or disks;
    // if failReason != NULL, it is set to one of the CHPPFR_XXX constants;
    // if 'convertFSPathToInternal' is TRUE or 'newDir' is NULL and it is an FS path,
    // CPluginInterfaceForFSAbstract::ConvertPathToInternal() is called;
    // 'showNewDirPathInErrBoxes' exists only for paths taken from links (disk paths only)
    // the entire path from the link should be shown, not just the part where the error was detected (otherwise the user won’t get the full path from the link);
    // returns TRUE if the requested path was successfully listed
    BOOL ChangeDir(const char* newDir = NULL, int suggestedTopIndex = -1,
                   const char* suggestedFocusName = NULL, int mode = 3 /*change-dir*/,
                   int* failReason = NULL, BOOL convertFSPathToInternal = TRUE,
                   BOOL showNewDirPathInErrBoxes = FALSE);

    // less orthodox version of ChangeDir: returns TRUE even when ChangeDir returns FALSE and
    // 'failReason' is CHPPFR_SHORTERPATH or CHPPFR_FILENAMEFOCUSED
    BOOL ChangeDirLite(const char* newDir);

    BOOL ChangePathToDrvType(HWND parent, int driveType, const char* displayName = NULL);

    // called after a new listing is obtained ... change notifications collected for the old
    // listing must be invalidated
    void InvalidateChangesInPanelWeHaveNewListing();

    void SetAutomaticRefresh(BOOL value, BOOL force = FALSE);

    // it sets ValidFileData; it checks if the VALID_DATA_PL_XXX constants can be used
    // (PluginData must not be empty and the corresponding constant VALID_DATA_SIZE,
    // VALID_DATA_DATE or VALID_DATA_TIME must not be used)
    void SetValidFileData(DWORD validFileData);

    // sets the "drive" icon in the directory line; differs by panel type, see code...
    // if 'check' is TRUE and the type is "ptDisk", the path is first verified via CheckPath
    void UpdateDriveIcon(BOOL check);

    // updates the information about free disk space; handled differently for each panel type, see the code
    // if 'check' is TRUE and the panel type is ptDisk, the path is first verified via CheckPath
    // if 'doNotRefreshOtherPanel' is FALSE and the other panel has a disk path with the same root,
    // a refresh of free space is performed there as well
    void RefreshDiskFreeSpace(BOOL check = TRUE, BOOL doNotRefreshOtherPanel = FALSE);

    void UpdateFilterSymbol(); // ensures the status bar is updated

    // called when closing an FS - the history stores FS interfaces which must be set to NULL
    // after closing (to avoid accidental matches caused by allocating FS interfaces at the same address)
    void ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs);

    // restores top index and focused name in the current path history item for this panel
    void RefreshPathHistoryData();

    // // removes the current path in the panel from the path history of this panel
    void RemoveCurrentPathFromHistory();

    // returns TRUE, if the plug-in is no longer used by the panel
    BOOL CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin);

    void ItemFocused(int index); // called when focus changes
    void RedrawIndex(int index);

    void SelectUnselect(BOOL forceIncludeDirs, BOOL select, BOOL showMaskDlg);
    void InvertSelection(BOOL forceIncludeDirs);

    // 'select' - TRUE: select, FALSE: unselect
    // 'byName' - TRUE: select all items with the same name as the focused item
    //            FALSE: operate by extension instead of name
    // The function respects Configuration::IncludeDirs variable
    void SelectUnselectByFocusedItem(BOOL select, BOOL byName);

    // saves the selection to the global GlobalSelection variable or to the clipboard
    void StoreGlobalSelection();

    // sets the selection based on GlobalSelection or based on Clipboard
    void RestoreGlobalSelection();

    // stores the selection in OldSelection
    void StoreSelection();
    // restores the selection based on the state in OldSelection
    void Reselect();

    void ShowHideNames(int mode); // 0:show all 1:hide selected 2:hide unselected

    // resets the cut-to-clip flag and calls RefreshListBox if 'repaint' is TRUE
    void ClearCutToClipFlag(BOOL repaint);

    // returns the index of the current view
    int GetViewTemplateIndex();

    // returns the index of the next (or previous if 'forward' is FALSE) view
    // if 'wrap' is TRUE it continues from the other end of the list after the last/first item
    int GetNextTemplateIndex(BOOL forward, BOOL wrap);

    // returns TRUE if int templateIndex points to a template that can be switched to via SelectViewTemplate
    BOOL IsViewTemplateValid(int templateIndex);

    // sets current template for the panel
    // the first templates are fixed and the others can be edited by the user
    // 'templateIndex' specifies the index of the template that should be selected
    // if the template at the requested index is invalid, the template at index 2 (detailed) will be selected
    // if 'preserveTopIndex' is TRUE, the topIndex is preserved (focus visibility is not guaranteed)
    BOOL SelectViewTemplate(int templateIndex, BOOL canRefreshPath,
                            BOOL calledFromPluginBeforeListing, DWORD columnValidMask = VALID_DATA_ALL,
                            BOOL preserveTopIndex = FALSE, BOOL salamanderIsStarting = FALSE);

    // returns FALSE if the extension is not defined (see VALID_DATA_EXTENSION) or the extension
    // (CFileData::Ext) is part of the Name column; otherwise returns TRUE (CFileData::Ext has
    // a dedicated column)
    BOOL IsExtensionInSeparateColumn();

    void ToggleStatusLine();
    void ToggleDirectoryLine();
    void ToggleHeaderLine();

    void ConnectNet(BOOL readOnlyUNC, const char* netRootPath = NULL, BOOL changeToNewDrive = TRUE, char* newlyMappedDrive = NULL);
    void DisconnectNet();

    // in detailed mode returns: min((panel width) - (width of all visible columns except NAME column), (width of the NAME column)
    // in brief mode returns 0
    int GetResidualColumnWidth(int nameColWidth = 0);

    void ChangeAttr(BOOL setCompress = FALSE, BOOL compressed = FALSE,
                    BOOL setEncryption = FALSE, BOOL encrypted = FALSE);
    void Convert(); // converts character sets and line endings
    // handlerID specifies which viewer/editor should open the file; 0xFFFFFFFF = no preference
    void ViewFile(char* name, BOOL altView, DWORD handlerID, int enumFileNamesSourceUID,
                  int enumFileNamesLastFileIndex);           // name == NULL -> item under the cursor in the panel
    void EditFile(char* name, DWORD handlerID = 0xFFFFFFFF); // name == NULL -> item under the cursor in the panel
    void EditNewFile();
    // fills a popup based on available viewers
    void FillViewWithMenu(CMenuPopup* popup);
    // fills the array from which the popup is subsequently displayed
    BOOL FillViewWithData(TDirectArray<CViewerMasksItem*>* items);
    // displays the focused file using the viewer identified by 'index'
    void OnViewFileWith(int index);
    // view-file-with: opens a menu to choose a viewer; name == NULL -> item under the cursor in the panel;
    // if handlerID != NULL, only the selected handler ID is returned (the viewer is not opened);
    // on error returns 0xFFFFFFFF
    void ViewFileWith(char* name, HWND hMenuParent, const POINT* menuPos, DWORD* handlerID,
                      int enumFileNamesSourceUID, int enumFileNamesLastFileIndex);

    // fills a popup based on available editors
    void FillEditWithMenu(CMenuPopup* popup);
    // edits the focused file using the editor identified by the variable 'index'
    void OnEditFileWith(int index);
    // edit-file-with: opens a menu to choose an editor; name == NULL -> item under the cursor in the panel;
    // if handlerID != NULL, only the selected handler ID is returned (the editor is not opened),
    // on error returns 0xFFFFFFFF
    void EditFileWith(char* name, HWND hMenuParent, const POINT* menuPos, DWORD* handlerID = NULL);
    void FindFile();
    void DriveInfo();
    void OpenActiveFolder();

    void GotoHotPath(int index);
    void SetUnescapedHotPath(int index);
    BOOL SetUnescapedHotPathToEmptyPos();
    void GotoRoot();

    void FilesAction(CActionType type, CFilesWindow* target = NULL, int countSizeMode = 0);
    void PluginFSFilesAction(CPluginFSActionType type);
    void CreateDir(CFilesWindow* target);
    void RenameFile(int specialIndex = -1);
    void RenameFileInternal(CFileData* f, const char* formatedFileName, BOOL* mayChange, BOOL* tryAgain);
    void DropCopyMove(BOOL copy, char* targetPath, CCopyMoveData* data);

    // performs deletion using the SHFileOperation API function (only when deleting to the Recycle Bin)
    BOOL DeleteThroughRecycleBin(int* selection, int selCount, CFileData* oneFile);

    BOOL BuildScriptMain(COperations* script, CActionType type, char* targetPath,
                         char* mask, int selCount, int* selection,
                         CFileData* oneFile, CAttrsData* attrsData,
                         CChangeCaseData* chCaseData, BOOL onlySize,
                         CCriteriaData* filterCriteria);
    BOOL BuildScriptDir(COperations* script, CActionType type, char* sourcePath,
                        BOOL sourcePathSupADS, char* targetPath, CTargetPathState targetPathState,
                        BOOL targetPathSupADS, BOOL targetPathIsFAT32, char* mask, char* dirName,
                        char* dirDOSName, CAttrsData* attrsData, char* mapName,
                        DWORD sourceDirAttr, CChangeCaseData* chCaseData, BOOL firstLevelDir,
                        BOOL onlySize, BOOL fastDirectoryMove, CCriteriaData* filterCriteria,
                        BOOL* canDelUpperDirAfterMove, FILETIME* sourceDirTime,
                        DWORD srcAndTgtPathsFlags);
    BOOL BuildScriptFile(COperations* script, CActionType type, char* sourcePath,
                         BOOL sourcePathSupADS, char* targetPath, CTargetPathState targetPathState,
                         BOOL targetPathSupADS, BOOL targetPathIsFAT32, char* mask, char* fileName,
                         char* fileDOSName, const CQuadWord& fileSize, CAttrsData* attrsData,
                         char* mapName, DWORD sourceFileAttr, CChangeCaseData* chCaseData,
                         BOOL onlySize, FILETIME* fileLastWriteTime, DWORD srcAndTgtPathsFlags);
    BOOL BuildScriptMain2(COperations* script, BOOL copy, char* targetDir,
                          CCopyMoveData* data);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // loads Files and Dirs according to panel type from disk, ArchiveDir or PluginFSDir;
    // for ptDisk it returns TRUE if the directory was successfully read from disk (FALSE on read
    // error or out of memory). For ptZIPArchive and ptPluginFS it returns FALSE only when memory
    // runs out or if the path does not exist (checked before calling ReadDirectory, should not happen);
    // 'parent' is the parent of message boxes;
    // if TRUE is returned, SortDirectory() is also called
    BOOL ReadDirectory(HWND parent, BOOL isRefresh);

    // sorts Files and Dirs using the current ordering method; because it reorders them,
    // icon loading for Files and Dirs must be paused during sorting (see SleepIconCacheThread())
    void SortDirectory(CFilesArray* files = NULL, CFilesArray* dirs = NULL);

    void RefreshListBox(int suggestedXOffset,         // if not -1 this value is used
                        int suggestedTopIndex,        // if not -1 this value is used
                        int suggestedFocusIndex,      // if not -1 this value is used
                        BOOL ensureFocusIndexVisible, // ensure at least partial focus visibility
                        BOOL wholeItemVisible);       // the entire item must be visible (relevant only when ensureFocusIndexVisible is TRUE)

    // 'probablyUselessRefresh' is TRUE when it is likely just a redundant refresh triggered only
    // by loading icons from a file on a network drive;
    // 'forceReloadThumbnails' is TRUE when all thumbnails need to be regenerated again (not only those
    // of changed files); 'isInactiveRefresh' is TRUE when refreshing an inactive window-we load
    // only visible icons/thumbnails/overlays; others are loaded once the main window becomes active
    void RefreshDirectory(BOOL probablyUselessRefresh = FALSE, BOOL forceReloadThumbnails = FALSE,
                          BOOL isInactiveRefresh = FALSE);

    // read-dir (archives, FS, disk), sort
    // parent is the parent message box
    // if suggestedTopIndex != -1, the top index will be set
    // if suggestedFocusName != NULL and present in the new list, it will be selected
    // if refreshListBox is FALSE, RefreshListBox is not called
    // if readDirectory is FALSE, ReadDirectory is not called
    // if isRefresh is TRUE, the path in the panel is refreshed by this
    // returns TRUE if ReadDirectory succeeded
    BOOL CommonRefresh(HWND parent, int suggestedTopIndex = -1,
                       const char* suggestedFocusName = NULL, BOOL refreshListBox = TRUE,
                       BOOL readDirectory = TRUE, BOOL isRefresh = FALSE);

    // ensures a panel refresh after configuration change (for archives updates the timestamp so a refresh occurs)
    void RefreshForConfig();

    void RedrawFocusedIndex();

    void DirectoryLineSetText();

    // unpacking or deleting from archives (not only ZIP); 'target' may be NULL
    // if 'tgtPath' is not NULL; when 'tgtPath' is not NULL, the unpacking is done
    // to that path without asking the user
    void UnpackZIPArchive(CFilesWindow* target, BOOL deleteOp = FALSE, const char* tgtPath = NULL);
    // deleting from archives (not only ZIP) - simply calls UnpackZIPArchive
    void DeleteFromZIPArchive();
    // moves all files from the source directory to the target directory,
    // remapping displayed names as well
    BOOL MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                   const char* remapNameTo);

    // helper function: before executing a command or drag&drop it offers archive update
    void OfferArchiveUpdateIfNeeded(HWND parent, int textID, BOOL* archMaybeUpdated);
    void OfferArchiveUpdateIfNeededAux(HWND parent, int textID, BOOL* archMaybeUpdated); // used internally only

    // writes the list of selected files to the file hFile
    BOOL MakeFileList(HANDLE hFile);

    void Pack(CFilesWindow* target, int pluginIndex = -1, const char* pluginName = NULL, int delFilesAfterPacking = 0);
    void Unpack(CFilesWindow* target, int pluginIndex = -1, const char* pluginName = NULL, const char* unpackMask = NULL);

    void CalculateOccupiedZIPSpace(int countSizeMode = 0);

    // returns the point where the context menu can be displayed (screen coordinates)
    void GetContextMenuPos(POINT* p);

    // helper function for DrawBriefDetailedItem and DrawIconThumbnailItem
    void SetFontAndColors(HDC hDC, CHighlightMasksItem* highlightMasksItem,
                          CFileData* f, BOOL isItemFocusedOrEditMode,
                          int itemIndex);

    // helper function for DrawBriefDetailedItem and DrawIconThumbnailItem
    // if overlayRect is not NULL, the overlay is placed in its bottom-left corner
    void DrawIcon(HDC hDC, CFileData* f, BOOL isDir, BOOL isItemUpDir,
                  BOOL isItemFocusedOrEditMode, int x, int y, CIconSizeEnum iconSize,
                  const RECT* overlayRect, DWORD drawFlags);

    // draws an item for Brief and Detailed modes (16x16 icon on the left, text on the right)
    void DrawBriefDetailedItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags);
    // draws an item for Icon and Thumbnails mode (icon/thumbnail above, text below)
    void DrawIconThumbnailItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags,
                               CIconSizeEnum iconSize);
    // draws an item for Tiles mode (icon left, multiline texts on the right)
    void DrawTileItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags,
                      CIconSizeEnum iconSize);

    void CalculateDirSizes(); // compute sizes of all subdirectories

    void DragEnter();
    void DragLeave();

    void HandsOff(BOOL off); // off==TRUE detaches the panel from the disk (monitoring off), FALSE reattaches it

    // column handling functions
    BOOL BuildColumnsTemplate();                                           // fills the 'ColumnsTemplate' array based on the current view template 'ViewTemplate' and panel content type (disk / archive + FS)
    BOOL CopyColumnsTemplateToColumns();                                   // transfers the contents of the 'ColumnsTemplate' array to 'Columns'
    void DeleteColumnsWithoutData(DWORD columnValidMask = VALID_DATA_ALL); // removes columns for which no data are available (according to ValidFileData & columnValidMask)

    // Operation Ball Lightning: moved from FILESBOX.H
    void ClipboardCopy();
    void ClipboardCut();
    BOOL ClipboardPaste(BOOL onlyLinks = FALSE, BOOL onlyTest = FALSE, const char* pastePath = NULL);
    BOOL ClipboardPasteToArcOrFS(BOOL onlyTest, DWORD* pasteDefEffect); // 'pasteDefEffect' may be NULL
    BOOL ClipboardPasteLinks(BOOL onlyTest = FALSE);
    BOOL IsTextOnClipboard();
    void ClipboardPastePath(); // for changing the current directory

    // postprocesses of the user provided path: trims surrounding white spaces and quotes, removes file:// and
    // expands environment variables; returns FALSE on error (processing should stop); 'parent' is
    // the parent for error message boxes
    BOOL PostProcessPathFromUser(HWND parent, char (&buff)[2 * MAX_PATH]);

    // if disable==FALSE, opens a dialog with selection options
    // if disable==TRUE, the filter is turned off
    void ChangeFilter(BOOL disable = FALSE);

    void EndQuickSearch(); // ends Quick Search mode

    // QuickRenameWindow
    void AdjustQuickRenameRect(const char* text, RECT* r); // adjusts 'r' so it doesn't exceed the panel and is large enough at the same time
    void AdjustQuickRenameWindow();
    //    void QuickRenameOnIndex(int index); // calls QuickRenameBegin for the given index
    void QuickRenameBegin(int index, const RECT* labelRect); // opens QuickRenameWindow
    void QuickRenameEnd();                                   // ends Quick Rename mode
    BOOL IsQuickRenameActive();                              // returns TRUE if QuickRenameWindow is open

    // returns FALSE if an error occurred during renaming and the Edit window should remain open
    // otherwise returns TRUE
    BOOL HandeQuickRenameWindowKey(WPARAM wParam); // QuickRenameWindow calls this method on VK_ESCAPE or VK_RETURN

    void KillQuickRenameTimer(); // if the timer is running, it will be stopped

    // if QuickSearch or QuickRename is active, it ends it
    void CancelUI();

    // Searches for the next/previous item. If skip = TRUE, the current item is skipped
    // if newChar != 0,it is appended to QuickSearchMask
    // if wholeString == TRUE, the entire item must match, not just its start
    // returns TRUE when a directory/file is found and also sets the index
    BOOL QSFindNext(int currentIndex, BOOL next, BOOL skip, BOOL wholeString, char newChar, int& index);

    // Searches for the next/previous selected item. If skip = TRUE, the current item is skipped
    BOOL SelectFindNext(int currentIndex, BOOL next, BOOL skip, int& index);

    // returns TRUE if the panel has the Nethood FS open
    BOOL IsNethoodFS();

    void CtrlPageDnOrEnter(WPARAM key);
    void CtrlPageUpOrBackspace();

    // attempts to open the focused file as a shortcut, extract its target
    // and focus that target in the panel 'panel'
    void FocusShortcutTarget(CFilesWindow* panel);

    // 'scroll' to partially (TRUE) or fully (FALSE) visible
    // forcePaint ensures repaint even when the index does not change
    void SetCaretIndex(int index, BOOL scroll, BOOL forcePaint = FALSE);
    int GetCaretIndex();

    void SetDropTarget(int index);       // marks where files will be dropped
    void SetSingleClickIndex(int index); // highlights the item and clears the old one
    void SelectFocusedIndex();

    void DrawDragBox(POINT p);
    void EndDragBox(); // ends dragging

    // !!! note - these methods do not ensure repainting the items, they only set the Dirty bit
    // repaint must be triggered explicitly by using:
    // RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    // only the first method SetSel can repaint items on explicit request
    // the first two methods (SetSel and SetSelRange) do not mark the ".." directory item
    void SetSel(BOOL select, int index, BOOL repaintDirtyItems = FALSE); // If index is -1 the selection is added to or removed from all strings
    // returns TRUE if the state of at least one item has changed, otherwise returns FALSE
    BOOL SetSelRange(BOOL select, int firstIndex, int lastIndex);
    void SetSel(BOOL select, CFileData* data); // data must be held by the corresponding list

    BOOL GetSel(int index);
    int GetSelItems(int itemsCountMax, int* items, BOOL focusedItemFirst = FALSE); // if 'focusedItemFirst' is TRUE (not used anymore; see GetSelItems body): for context menus, we start we start from the focused item and end with the item before the focus (there is intermediate wrapping back to the beginning of the name list) (the system does it in the same way, see Add To Windows Media Player List on MP3 files)

    // if GetSelCount > 0 returns TRUE, if at least one directory is selected (".." not counted); otherwise it returns FALSE
    // if GetSelCount == 0 returns TRUE, if a directory is focused (".." not counted); otherwise it returns FALSE
    BOOL SelectionContainsDirectory();
    BOOL SelectionContainsFile(); // same for files

    int GetSelCount(); // returns the number of selected items

    void SelectFocusedItemAndGetName(char* name, int nameMax);
    void UnselectItemWithName(const char* name);

    // returns PANEL_LEFT or PANEL_RIGHT depending on which side this panel is on
    int GetPanelCode();

    void SafeHandleMenuNewMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);
    void RegisterDragDrop();
    void RevokeDragDrop();

    HWND GetListBoxHWND(); // used only by the list box
    HWND GetHeaderLineHWND();
    int GetIndex(int x, int y, BOOL nearest = FALSE, RECT* labelRect = NULL);

    // functions called by the list box
    BOOL OnSysChar(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnChar(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnSysKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnSysKeyUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult);

    void GotoSelectedItem(BOOL next); // if 'next' is TRUE, it moves caret to the next selected item, otherwise to the previous one

    void OnSetFocus(BOOL focusVisible = TRUE); // 'focusVisible'==FALSE when switching from the command line to the panel while the user is in a modeless dialog (main window is inactive) - required by the FTP plugin Welcome Message dialog
    void OnKillFocus(HWND hwndGetFocus);

    BOOL OnLButtonDown(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnLButtonUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnLButtonDblclk(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnRButtonDown(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnRButtonUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnMouseMove(WPARAM wParam, LPARAM lParam, LRESULT* lResult);

    BOOL IsDragDropSafe(int x, int y); // safer drag&drop: returns TRUE if the drag was long enough; x,y are coordinates relative to the panel origin

    BOOL OnTimer(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnCaptureChanged(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnCancelMode(WPARAM wParam, LPARAM lParam, LRESULT* lResult);

    void LayoutListBoxChilds(); // after a font change layout must be updated
    void RepaintListBox(DWORD drawFlags);
    void RepaintIconOnly(int index);   // for index == -1 redraws icons of all items
    void EnsureItemVisible(int index); // ensures the item is visible
    void SetQuickSearchCaretPos();     // sets caret position within FocusedIndex

    void SetupListBoxScrollBars();

    // creates an image list with one item used to display dragging progress
    // the image list must be freed after dragging ends
    // input is the point from which dxHotspot and dyHotspot offsets are computed
    // the function also returns the size of the dragged image
    HIMAGELIST CreateDragImage(int cursorX, int cursorY, int& dxHotspot, int& dyHotspot, int& imgWidth, int& imgHeight);

    // places the name of the selected file or directory on the clipboard (optionally with full path or in UNC form)
    // in case mode==cfnmUNC, it tries to convert the path from "x:\" to UNC and if it fails, an error is displayed
    BOOL CopyFocusedNameToClipboard(CCopyFocusedNameModeEnum mode);

    // places the full current path on the clipboard (active in the panel)
    BOOL CopyCurrentPathToClipboard();

    void OpenDirHistory();

    void OpenStopFilterMenu(); // used to disable individual filters

    // measures dimensions and returns TRUE if they are large enough for focus
    BOOL CanBeFocused();

    // fills a popup based on available columns
    BOOL FillSortByMenu(CMenuPopup* popup);

    // if the user changes the column width, this method will be called (after the dragging ends)
    void OnHeaderLineColWidthChanged();

    CHeaderLine* GetHeaderLine();

    // sends focused/selected files using the default mail client
    void EmailFiles();

    // the screen colors or color depth changed; new toolbar imagelists are already created
    // and must be assigned to the controls that use them
    void OnColorsChanged();

    // opens in the other panel the path composed of the current path and the focused name
    // if 'activate' is TRUE, the other panel is activated as well
    // returns TRUE on success, otherwise FALSE
    BOOL OpenFocusedInOtherPanel(BOOL activate);

    // opens in this panel the path from the other panel
    void ChangePathToOtherPanelPath();

    // called while Salamander is in idle mode; stores the array of visible items when needed
    void RefreshVisibleItemsArray();

    // drag&drop from disk to an archive or plugin FS
    void DragDropToArcOrFS(CTmpDragDropOperData* data);

    CViewModeEnum GetViewMode();
    CIconSizeEnum GetIconSizeForCurrentViewMode();

    void SetThumbnailSize(int size);
    int GetThumbnailSize();

    void SetFont();

    void LockUI(BOOL lock);
};

//****************************************************************************
//
// CPanelTmpEnumData
//

struct CPanelTmpEnumData
{
    int* Indexes;
    int CurrentIndex;
    int IndexesCount;
    const char* ZIPPath;              // archive root for the entire operation
    CFilesArray* Dirs;                // current list of directories pointed to by the selected indexes from Indexes
    CFilesArray* Files;               // current list of files rpointed by the selected indexes
    CSalamanderDirectory* ArchiveDir; // archive directory of the current archive

    // for enum-zip-selection, enumFiles > 0
    CSalamanderDirectory* EnumLastDir;
    int EnumLastIndex;
    char EnumLastPath[MAX_PATH];
    char EnumTmpFileName[MAX_PATH];

    // for disk enumeration, enumFiles > 0
    char WorkPath[MAX_PATH];                 // path where Files and Dirs reside, used only when browsing disk (not archives)
    CSalamanderDirectory* DiskDirectoryTree; // replacement for Panel->ArchiveDir
    char EnumLastDosPath[MAX_PATH];          // DOS name of EnumLastPath
    char EnumTmpDosFileName[MAX_PATH];       // DOS name of EnumTmpFileName
    int FilesCountReturnedFromWP;            // number of files already returned by the enumerator directly from WorkPath (i.e. from Files)

    CPanelTmpEnumData();
    ~CPanelTmpEnumData();

    void Reset(); // sets the object to the initial enumeration state
};

const char* WINAPI PanelEnumDiskSelection(HWND parent, int enumFiles, const char** dosName, BOOL* isDir,
                                          CQuadWord* size, DWORD* attr, FILETIME* lastWrite, void* param,
                                          int* errorOccured);

//****************************************************************************
//
// externals
//

extern CNames GlobalSelection; // stored selection shared by both panels

extern CDirectorySizesHolder DirectorySizesHolder; // holds the list of directory names and sizes with known size

extern CFilesWindow* DropSourcePanel; // prevents drag&drop from/to the same panel
extern BOOL OurClipDataObject;        // TRUE when pasting our IDataObject
                                      // (detects our copy/move routine with foreign data)

// enumeration of selected files and directories from the panel
const char* WINAPI PanelSalEnumSelection(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                         const CFileData** fileData, void* param, int* errorOccured);

//****************************************************************************
//
// SplitText
//
// Uses the array 'DrawItemAlpDx'
//
// text      [IN]  input string that we will split
// textLen   [IN]  number of characters in 'text' (without terminator)
// maxWidth  [IN]  maximum number of pixels a longer line may have in width
//           [OUT] actual maximum width
// out1      [OUT] this string receives the first output line without the terminator
// out1Len   [IN]  maximum number of characters that can be written to 'out1'
//           [OUT] number of characters copied to 'out1'
// out1Width [OUT] width of 'out1' in pixels
// out2            same as out1 but for the second line
// out2Len
// out2Width
//

void SplitText(HDC hDC, const char* text, int textLen, int* maxWidth,
               char* out1, int* out1Len, int* out1Width,
               char* out2, int* out2Len, int* out2Width);

//
// Copies the UNC form of a path to the clipboard.
// 'path' is the path either in direct or UNC form
// 'name' is the selected item and 'isDir' determines whether it is a file or directory
// 'hMessageParent' is the window to which a message box will be displayed in case of failure
// 'nestingLevel' is an internal counter of nesting when resolving SUBSTs (may be cyclic)
//
// Returns TRUE on success and FALSE if the UNC path could not be constructed or placed on the clipboard.
//
// This function can be called from any thread.
//

BOOL CopyUNCPathToClipboard(const char* path, const char* name, BOOL isDir, HWND hMessageParent, int nestingLevel = 0);

// From the file/directory 'f' creates three lines of text and fills out0/out0Len to out2/out2Len;
// 'validFileData' specifies which parts of 'f' are valid
void GetTileTexts(CFileData* f, int isDir,
                  HDC hDC, int maxTextWidth, int* widthNeeded,
                  char* out0, int* out0Len,
                  char* out1, int* out1Len,
                  char* out2, int* out2Len,
                  DWORD validFileData,
                  CPluginDataInterfaceEncapsulation* pluginData,
                  BOOL isDisk);
