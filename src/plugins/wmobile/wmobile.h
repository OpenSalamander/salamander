// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// global data
extern char Str[MAX_PATH];
extern DWORD DWord;
extern HINSTANCE DLLInstance; // handle to SPL - language-independent resources
extern HINSTANCE HLanguage;   // handle to SLG - language-dependent resources

// Salamander general interface - valid from startup until the plugin terminates
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// interface providing customized Windows controls used in Salamander
extern CSalamanderGUIAbstract* SalamanderGUI;

BOOL InitFS();
void ReleaseFS();

// FS name assigned by Salamander after the plugin loads
extern char AssignedFSName[MAX_PATH];

// pointers to lower/upper case mapping tables
extern unsigned char* LowerCase;
extern unsigned char* UpperCase;

// global variables storing pointers to Salamander's global variables
// shared by both the archiver and FS
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
extern int Selection; // "second" in configuration dialog
extern BOOL CheckBox;
extern int RadioBox;                       // radio 2 in configuration dialog
extern BOOL CfgSavePosition;               // save window position/align with the main window
extern WINDOWPLACEMENT CfgWindowPlacement; // invalid when CfgSavePosition != TRUE

extern DWORD LastCfgPage; // start page (sheet) in the configuration dialog

char* LoadStr(int resID);

extern char TitleWMobile[];
extern char TitleWMobileError[];
extern char TitleWMobileQuestion[];

//
// ****************************************************************************
// CPluginInterface
//

class CPluginInterfaceForFS : public CPluginInterfaceForFSAbstract
{
protected:
    int ActiveFSCount; // number of active FS interfaces (for deallocation checks only)

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
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam);
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam);
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
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; };
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; };
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; };

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

//
// ****************************************************************************
// FILE-SYSTEM
//

//****************************************************************************
//
// CTopIndexMem
//
// top-index memory for the panel list box - used by CPluginFSInterface to keep
// ExecuteOnFS behavior consistent (preserves the top index when entering and leaving a subdirectory)

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
    } // clear the memory
    void Push(const char* path, int topIndex);        // store the top index for the given path
    BOOL FindAndPop(const char* path, int& topIndex); // find the top index for the given path, FALSE -> not found
};

//
// ****************************************************************************
// CPluginFSInterface
//
// set of plugin methods that Salamander needs to work with the file system

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
public:
    char Path[MAX_PATH];      // current path
    BOOL PathError;           // TRUE if ListCurrentPath failed (path error); triggers ChangePath
    BOOL FatalError;          // TRUE if ListCurrentPath failed (fatal error); triggers ChangePath
    CTopIndexMem TopIndexMem; // stored top indexes for ExecuteOnFS()

public:
    CPluginFSInterface();
    ~CPluginFSInterface() {}

    virtual BOOL WINAPI GetCurrentPath(char* userPart);
    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize);
    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success);
    virtual BOOL WINAPI GetRootPath(char* userPart);

    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);

    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                   const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                   BOOL forceRefresh, int mode);
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

    static void EmptyCache();
};

// plugin interface provided to Salamander
extern CPluginInterface PluginInterface;

// opens the configuration dialog; if it already exists, shows a message and returns
void OnConfiguration(HWND hParent);

// opens the About window
void OnAbout(HWND hParent);

/////////////////////////////////////////////////////////////////////////////
// helper functions for RAPI

struct CFileInfo
{
    char cFileName[MAX_PATH];
    DWORD dwFileAttributes;
    DWORD size;
    int block;
};

typedef TDirectArray<CFileInfo> CFileInfoArray;

class CRAPI
{
public:
    static BOOL Init();
    static BOOL ReInit();
    static void UnInit();

    //RAPI
    static BOOL FindAllFiles(LPCTSTR szPath, DWORD dwFlags, LPDWORD lpdwFoundCount,
                             RapiNS::LPCE_FIND_DATA* ppFindDataArray, BOOL tryReinit = FALSE);
    static HANDLE FindFirstFile(LPCTSTR lpFileName, RapiNS::LPCE_FIND_DATA lpFindFileData, BOOL tryReinit = FALSE);
    static BOOL FindNextFile(HANDLE hFindFile, RapiNS::LPCE_FIND_DATA lpFindFileData);
    static BOOL FindClose(HANDLE hFindFile);

    static DWORD GetFileAttributes(LPCTSTR lpFileName, BOOL tryReinit = FALSE);
    static BOOL SetFileAttributes(LPCTSTR lpFileName, DWORD dwFileAttributes);
    static BOOL GetFileTime(HANDLE hFile, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime);
    static BOOL SetFileTime(HANDLE hFile, FILETIME* lpCreationTime, FILETIME* lpLastAccessTime, FILETIME* lpLastWriteTime);
    static DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
    static BOOL GetStoreInformation(RapiNS::LPSTORE_INFORMATION lpsi);

    static HANDLE CreateFile(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                             LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                             DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    static BOOL CloseHandle(HANDLE hObject);

    static BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                         LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
    static BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                          LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

    static BOOL CopyFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists);
    static BOOL MoveFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName);
    static BOOL DeleteFile(LPCTSTR lpFileName);

    static BOOL CreateDirectory(LPCTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
    static BOOL RemoveDirectory(LPCTSTR lpPathName);

    static BOOL CreateProcess(LPCTSTR lpApplicationName, LPCTSTR lpCommandLine);

    static BOOL SHGetShortcutTarget(LPCTSTR lpszShortcut, LPTSTR lpszTarget, int cbMax);

    static HRESULT FreeBuffer(LPVOID Buffer);

    static DWORD GetLastError(void);
    static HRESULT RapiGetError(void);

    //Helpers
    static BOOL PathAppend(char* path, const char* name, int pathSize);
    static BOOL FindAllFilesInTree(const char* rootPath, const char* fileName, CFileInfoArray& array, int block, BOOL dirFirst);

    static DWORD CopyFileToPC(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists, CProgressDlg* dlg, INT64 totalCopied, INT64 totalSize, LPCTSTR* errorFileName);
    static DWORD CopyFileToCE(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists, CProgressDlg* dlg, INT64 totalCopied, INT64 totalSize, LPCTSTR* errorFileName);
    static DWORD CopyFile(LPCTSTR lpExistingFileName, LPCTSTR lpNewFileName, BOOL bFailIfExists, CProgressDlg* dlg, INT64 totalCopied, INT64 totalSize, LPCTSTR* errorFileName);

    static DWORD SetFileTime(LPCTSTR lpFileName, const SYSTEMTIME* lpCreationTime,
                             const SYSTEMTIME* lpLastAccessTime, const SYSTEMTIME* lpLastWriteTime);

    static BOOL CheckAndCreateDirectory(const char* dir, HWND parent, BOOL quiet, char* errBuf, int errBufSize, char* newDir);

    static void GetFileData(const char* name, char (&buf)[100]);

    static BOOL CheckConnection();

    //Implementation
private:
    static DWORD WaitAndDispatch(DWORD nCount, HANDLE* phWait, DWORD dwTimeout, BOOL bOnlySendMessage);
    static HRESULT InitRapi(HANDLE hExit, DWORD dwTimeout);

    static BOOL FindAllFilesInTree(LPCTSTR rootPath, char (&path)[MAX_PATH], LPCTSTR fileName, CFileInfoArray& array, int block, BOOL dirFirst);

    static BOOL initialized;
};
