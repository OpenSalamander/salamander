// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern HINSTANCE DLLInstance; // handle for SPL - language independent resources
extern HINSTANCE HLanguage;   // handle for SLG - language dependent resources

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern CSalamanderDebugAbstract* SalamanderDebug;
extern CSalamanderSafeFileAbstract* SalamanderSafeFile;
extern CSalamanderGUIAbstract* SalamanderGUI;

BOOL InitFS();
void ReleaseFS();

// FS-name assigned by Salamander after plugin is loaded
extern char AssignedFSName[MAX_PATH];

// image-list for simple FS icons
extern HIMAGELIST DFSImageList;

// configuration
extern BOOL ConfigAlwaysReuseScanInfo;
extern BOOL ConfigScanVacantClusters;
extern BOOL ConfigShowExistingFiles;
extern BOOL ConfigShowZeroFiles;
extern BOOL ConfigShowEmptyDirs;
extern BOOL ConfigShowMetafiles;
extern BOOL ConfigEstimateDamage;
extern char ConfigTempPath[MAX_PATH];
extern BOOL ConfigDontShowEncryptedWarning;
extern BOOL ConfigDontShowSamePartitionWarning;

// commands
#define CMD_UNDELETE 1
#define CMD_RESTORE_ENCRYPTED 2
#ifdef _DEBUG
#define CMD_VIEWINFO 3
#define CMD_DUMPFILES 4
#define CMD_COMPAREFILES 5
#define CMD_FRAGMENTFILE 6
#define CMD_SETVALIDDATAFILE 7
#define CMD_SETSPARSEFILE 8
#endif //_DEBUG
#define CMD_SALCMD_OFFSET 5000

extern CLUSTER_MAP_I cluster_map;

//
// ****************************************************************************
// CPluginFSDataInterface
//

struct CSnapshotState
{
    int RefCount; // references count to this object (together fro FS object and from listings)
    BOOL Valid;   // FALSE = FS already released this snapshot, data are invalid

    CSnapshotState()
    {
        Valid = TRUE;
        RefCount = 0;
    }
    ~CSnapshotState()
    {
        if (RefCount != 0)
            TRACE_E("~CSnapshotState(): RefCount != 0");
    }

    BOOL IsValid() { return Valid; }
    void Invalidate() { Valid = FALSE; }

    void AddRef() { RefCount++; }
    void Release()
    {
        if (--RefCount <= 0)
            delete this;
    }
};

class CPluginFSDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    CSnapshotState* SnapshotState;

public:
    CPluginFSDataInterface(CSnapshotState* snapshotState)
    {
        snapshotState->AddRef();
        SnapshotState = snapshotState;
    }
    ~CPluginFSDataInterface() { SnapshotState->Release(); }

    BOOL IsSnapshotValid() { return SnapshotState->IsValid(); }

    virtual BOOL WINAPI CallReleaseForFiles() { return FALSE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return FALSE; }
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir) {}

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir)
    {
        // plugin has no custom data in CFileData, nothing to change/allocate here
    }
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir)
    {
        // plugin has no custom data in CFileData, nothing to change/allocate here
        return TRUE; // return success
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

//****************************************************************************
//
// CTopIndexMem
//
// panel listing top index memory - using CPluginFSInterface for correct behavior
// of ExecuteOnFS (persistent top-index while entering / leaving directory)

#define TOP_INDEX_MEM_SIZE 50 // count of stored top-index (levels), minimal 1

class CTopIndexMem
{
protected:
    // path for last stored top-index
    char Path[MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // stored top-index list
    int TopIndexesCount;                // count of stored top-index

public:
    CTopIndexMem() { Clear(); }
    void Clear()
    {
        Path[0] = 0;
        TopIndexesCount = 0;
    }                                                 // clear memory
    void Push(const char* path, int topIndex);        // store top-index for given path
    BOOL FindAndPop(const char* path, int& topIndex); // search top-index for given path, FALSE->not found
};

class CPluginInterfaceForFS : public CPluginInterfaceForFSAbstract
{
protected:
    int ActiveFSCount; // number of active FS interfaces (just for deallocation check)

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

    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) {}

    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel);
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam) { return FALSE; }
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam) {}
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) { return 0; }
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
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

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

class CFileList;

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
protected:
    friend static BOOL WINAPI EncryptedProgress(int inc, void* ctx);
    friend void WINAPI CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                                          const char* pluginFSName, int pluginFSNameIndex,
                                                          CFileData& file, int isDir);

    char Path[MAX_PATH]; // current path
    char Root[MAX_PATH]; // root of volume
    FILE_RECORD_I<char>* CurrentDir;

    BOOL FatalError;          // TRUE when ListCurrentPath failed (fatal error), ChangePath will be called
    CTopIndexMem TopIndexMem; // top-index array for ExecuteOnFS()

    BOOL IsSnapshotValid;
    CVolume<char> Volume;
    CSnapshot<char>* Snapshot;
    CSnapshotState* SnapshotState;
    DWORD Flags;

    HWND hErrParent;
    BOOL SkipAllLongPaths;
    DWORD SilentMask;
    QWORD FileProgress, TotalProgress, FileTotal, GrandTotal;
    CCopyProgressDlg* Progress;
    char SourcePath[MAX_PATH];
    char AllSubstChar;
    BOOL BackupEncryptedFiles;

    BOOL RootPathFromFull(const char* pluginFullPath, char* rootPath, size_t bufferSize);
    BOOL AppendPath(char* buffer, char* path, char* name, BOOL* ret);
    BOOL CopyFile(FILE_RECORD_I<char>* record, char* filename, char* targetPath, BOOL view, int* incompleteFileAnswer);
    BOOL CopyDir(FILE_RECORD_I<char>* record, char* filename, char* targetPath);
    QWORD GetFileSize(FILE_RECORD_I<char>* record, BOOL* encrypted);
    QWORD GetDirSize(FILE_RECORD_I<char>* record, int plus, BOOL* encrypted);
    QWORD GetTotalProgress(int panel, BOOL focused, int plus, BOOL* encrypted);
    void UpdateProgress();
    void Replace0xE5(char* filename);
    char* FixDamagedName(char* name);
    BOOL CopyFileList(CFileList& list, char* targetPath);
    BOOL PrepareRawAPI(char* targetPath, BOOL allowBackup);

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

    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon) { return FALSE; }
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon);
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect) {}
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue) {}
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset);
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize) {}
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize) { return FALSE; }
    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent) {}
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo) { return FALSE; }
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                    char* newName, BOOL& cancel) { return FALSE; }
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs) {}
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel) { return FALSE; }
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file);
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError) { return FALSE; }
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget);
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel) { return FALSE; }
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs) { return FALSE; }
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs) {}
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs);
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel) { return FALSE; }
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent) {}
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) {}
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) { return FALSE; }
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) { return FALSE; }
    virtual void WINAPI ShowSecurityInfo(HWND parent) {}

    BOOL GetTempDirOutsideRoot(HWND parent, char* buffer, char** ret);
#if defined(_DEBUG) && _WIN32_WINNT >= 0x0501 // some debug feauters works from Windows XP (NTFS 5.0)
    // NOTE: following functions are just quitch, dirty, and minimal tests (no error handling, etc)
    // included only in debug build
    void DumpSpecifiedFiles(FILE* file, FILE_RECORD_I<char>* dir, char* path, int pathSize);
    void DumpDirItemInfo(FILE* file, const DIR_ITEM_I<char>* di);
    BOOL TestUndeleteOnExistingFile(FILE* file, FILE_RECORD_I<char>* record, char* path, int pathSize);
    void TestUndeleteOnExistingFiles(FILE* file, FILE_RECORD_I<char>* dir, char* path, int pathSize, DWORD* count);
    void DumpDebugInformation(HWND parent, const DIR_ITEM_I<char>* di, DWORD mode);
    void FragmentFile(HWND parent, const DIR_ITEM_I<char>* di, const char* fullPath);
    void SetFileValidData(HWND parent, const DIR_ITEM_I<char>* di, const char* fullPath);
    void SetFileSparse(HWND parent, const DIR_ITEM_I<char>* di, const char* fullPath);
#endif //_DEBUG
};

void UndeleteGetResolvedRootPath(const char* path, char* resolvedPath);
