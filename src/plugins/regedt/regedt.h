// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define CURRENT_CONFIG_VERSION 1

// definice IDs do menu
#define MID_FIND 1
#define MID_NEWKEY 2
#define MID_NEWVAL 3
#define MID_EXPORT 4
#define MID_FOCUS 5

#ifndef REG_QWORD
// viz. winnt.h
#define REG_QWORD (11) // 64-bit number
#endif

// viz. winreg.h
#ifndef HKEY_PERFORMANCE_TEXT
#define HKEY_PERFORMANCE_TEXT ((HKEY)0x80000050)
#endif

#ifndef HKEY_PERFORMANCE_NLSTEXT
#define HKEY_PERFORMANCE_NLSTEXT ((HKEY)0x80000060)
#endif

#define MAX_KEYNAME 512
#define MAX_PREDEF_KEYNAME 32
#define MAX_FULL_KEYNAME (MAX_KEYNAME + MAX_PREDEF_KEYNAME + 1)
#define MAX_VAL_DATA 32767

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
class CPluginInterface;
extern CPluginInterface PluginInterface;
class CPluginInterfaceForMenuExt;
extern CPluginInterfaceForMenuExt InterfaceForMenuExt;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SG;

extern CSalamanderGUIAbstract* SalGUI;

// FS-name pridelene Salamanderem po loadu pluginu
extern char AssignedFSName[MAX_PATH];

class CWindowQueueEx : public CWindowQueue
{
public:
    CWindowQueueEx() : CWindowQueue("RegEdt Find Dialogs") {}

    HWND GetLastWnd()
    {
        CS.Enter();
        HWND w = NULL;
        CWindowQueueItem* next = Head;
        if (next != NULL) // najdeme posledni okno
        {
            while (next->Next)
                next = next->Next;
            w = next->HWindow;
        }
        CS.Leave();
        return w;
    }
};

extern CThreadQueue ThreadQueue;
extern CWindowQueueEx WindowQueue;
extern BOOL AlwaysOnTop;

// ****************************************************************************

struct CCS
{
    CRITICAL_SECTION cs;

    CCS() { InitializeCriticalSection(&cs); }
    ~CCS() { DeleteCriticalSection(&cs); }

    void Enter() { EnterCriticalSection(&cs); }
    void Leave() { LeaveCriticalSection(&cs); }
};

// ****************************************************************************

//char * ErrorStr(char * buf, int error, ...);
BOOL ErrorHelper(HWND parent, const char* message, int lastError, va_list arglist);
BOOL Error(HWND parent, int error, ...);
BOOL Error(HWND parent, const char* error, ...);
BOOL Error(int error, ...);
BOOL ErrorL(int lastError, HWND parent, int error, ...);
BOOL ErrorL(int lastError, int error, ...);
int SalPrintf(char* buffer, unsigned count, const char* format, ...);
int SalPrintfW(LPWSTR buffer, unsigned count, LPCWSTR format, ...);

// ****************************************************************************
//
// Interface pluginu
//

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

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; };
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

// ****************************************************************************

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
    // pouzite pro fokuseni polozek z jineho nez hlavniho threadu
    char Path[MAX_FULL_KEYNAME + 1];
    char Name[MAX_KEYNAME];

public:
    BOOL PostFocusCommand(const char* path, const char* name);

    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
};

// ****************************************************************************

extern HIMAGELIST ImageList;

extern const char* Str_REG_BINARY;
extern const char* Str_REG_DWORD;
extern const char* Str_REG_DWORD_BIG_ENDIAN;
extern const char* Str_REG_EXPAND_SZ;
extern const char* Str_REG_LINK;
extern const char* Str_REG_MULTI_SZ;
extern const char* Str_REG_NONE;
extern const char* Str_REG_QWORD;
extern const char* Str_REG_RESOURCE_LIST;
extern const char* Str_REG_SZ;
extern const char* Str_REG_FULL_RESOURCE_DESCRIPTOR;
extern const char* Str_REG_RESOURCE_REQUIREMENTS_LIST;

extern int Len_REG_BINARY;
extern int Len_REG_DWORD;
extern int Len_REG_DWORD_BIG_ENDIAN;
extern int Len_REG_EXPAND_SZ;
extern int Len_REG_LINK;
extern int Len_REG_MULTI_SZ;
extern int Len_REG_NONE;
extern int Len_REG_QWORD;
extern int Len_REG_RESOURCE_LIST;
extern int Len_REG_SZ;
extern int Len_REG_FULL_RESOURCE_DESCRIPTOR;
extern int Len_REG_RESOURCE_REQUIREMENTS_LIST;

struct CRegTypeText
{
    const char* Text;
    DWORD Type;
    unsigned CanCreate : 1;
    unsigned CanEdit : 1;
};

extern CRegTypeText RegTypeTexts[];

extern char KeyText[10];
extern int KeyTextLen;

struct CPredefinedHKey
{
    HKEY HKey;
    const WCHAR* KeyName;
};

extern CPredefinedHKey PredefinedHKeys[];

extern WCHAR RecentFullPath[MAX_FULL_KEYNAME];

BOOL InitFS();
void ReleaseFS();

class CPluginInterfaceForFS : public CPluginInterfaceForFSAbstract
{
public:
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

//****************************************************************************
//
// CTopIndexMem
//
// pamet top-indexu listboxu v panelu - pouziva CPluginFSInterface pro korektni
// chovani ExecuteOnFS (zachovani top-indexu po vstupu a vystupu z podadresare)

#define TOP_INDEX_MEM_SIZE 50 // pocet pamatovanych top-indexu (urovni), minimalne 1

class CTopIndexMem
{
protected:
    // cesta pro posledni zapamatovany top-index
    WCHAR Path[MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // zapamatovane top-indexy
    int TopIndexesCount;                // pocet zapamatovanych top-indexu

public:
    CTopIndexMem() { Clear(); }
    void Clear()
    {
        Path[0] = L'\0';
        TopIndexesCount = 0;
    }                                             // vycisti pamet
    void Push(LPCWSTR path, int topIndex);        // uklada top-index pro danou cestu
    BOOL FindAndPop(LPCWSTR path, int& topIndex); // hleda top-index pro danou cestu, FALSE->nenalezeno
};

// ****************************************************************************

void EditValue(int root, LPWSTR key, LPWSTR name, BOOL rawEdit);

BOOL SafeOpenKey(int root, LPWSTR key, DWORD sam, HKEY& hKey,
                 int errorTitle, LPBOOL skip, LPBOOL skipAll);

BOOL SafeQueryInfoKey(HKEY hKey, int root, LPWSTR key, LPWSTR className,
                      LPDWORD maxData, FILETIME* time,
                      int errorTitle, BOOL& skip, BOOL& skipAll);

BOOL SafeEnumValue(HKEY hKey, int root, LPWSTR key, DWORD& index,
                   LPWSTR name, DWORD& type, void* data, DWORD& size,
                   int errorTitle, BOOL& skip, BOOL& skipAll, BOOL& noMoreItems);

BOOL SafeEnumKey(HKEY hKey, int root, LPWSTR key, DWORD& index,
                 LPWSTR name, FILETIME* time,
                 int errorTitle, BOOL& skip, BOOL& skipAll, BOOL& noMoreItems);

BOOL CopyOrMoveKey(int sourceRoot, LPWSTR source, int targetRoot, LPWSTR target,
                   BOOL move, BOOL& skip,
                   BOOL& skipAllErrors, BOOL& skipAllLongNames,
                   BOOL& skipAllOverwrites, BOOL& overwriteAll,
                   BOOL& skipAllClassNames, LPWSTR nameBuffer,
                   TIndirectArray<WCHAR>& stack);

BOOL CopyOrMoveValue(int sourceRoot, LPWSTR sourcePath, LPWSTR sourceName,
                     int targetRoot, LPWSTR targetPath, LPWSTR targetName,
                     BOOL move, LPBOOL skip,
                     LPBOOL skipAllErrors, LPBOOL skipAllOverwrites,
                     LPBOOL overwriteAll);

class CChangeMonitor;

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
public:
    int CurrentKeyRoot;
    WCHAR CurrentKeyName[MAX_KEYNAME];
    CTopIndexMem TopIndexMem; // pamet top-indexu pro ExecuteOnFS()
    BOOL FocusFirstNewItem;

protected:
    BOOL PathError;
    WCHAR NewPath[MAX_KEYNAME];
    BOOL NewPathValid;
    BOOL FirstChangePath; // pri prvnim volani ChangePath ignorujeme chyby

public:
    CPluginFSInterface();
    ~CPluginFSInterface();

    BOOL IsGood() { return TRUE; }

    BOOL SetNewPath(WCHAR* newPath);

    BOOL GetCurrentPathW(WCHAR* userPart, int size);
    virtual BOOL WINAPI GetCurrentPath(char* userPart);

    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize);

    BOOL GetFullFSPathW(WCHAR* path, int pathSize, BOOL& success);
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

    virtual void WINAPI ReleaseObject(HWND parent) {}

    virtual DWORD WINAPI GetSupportedServices()
    {
        return FS_SERVICE_GETFSICON | FS_SERVICE_CREATEDIR | FS_SERVICE_DELETE |
               FS_SERVICE_CONTEXTMENU | FS_SERVICE_GETNEXTDIRLINEHOTPATH |
               FS_SERVICE_ACCEPTSCHANGENOTIF | FS_SERVICE_QUICKRENAME |
               FS_SERVICE_COPYFROMFS | FS_SERVICE_MOVEFROMFS | FS_SERVICE_VIEWFILE |
               FS_SERVICE_GETPATHFORMAINWNDTITLE | FS_SERVICE_OPENFINDDLG |
               FS_SERVICE_OPENACTIVEFOLDER;
    }

    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon) { return FALSE; }

    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon);

    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect) {}

    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue) { *retValue = CQuadWord(-1, -1); }

    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset);
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize) {}
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize) { return FALSE; }

    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent) { ; }

    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo) { return FALSE; }

    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                    char* newName, BOOL& cancel);

    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs);

    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel);

    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file);

    BOOL DeleteKey(WCHAR* keyName, BOOL& skip, BOOL& skipAllErrors, TIndirectArray<WCHAR>& stack);

    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError);

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
                                       int selectedFiles, int selectedDirs) { ; }

    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs);

    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel);

    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent);

    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) {}

    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) { return FALSE; }

    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) { return FALSE; }

    virtual void WINAPI ShowSecurityInfo(HWND parent) {}

    BOOL EditNewFile();
};

// ****************************************************************************

#define MAX_DATASIZE 200

struct CPluginData
{
    WCHAR* Name; // jmeno v unicode
    DWORD Type;
    DWORD_PTR Data;
    unsigned Allocated : 1;
    unsigned char DataSize;

    CPluginData(WCHAR* name, DWORD type, DWORD_PTR data = NULL, unsigned allocated = 0,
                unsigned char dataSize = 4)
    {
        Name = name;
        Type = type;
        Data = data;
        Allocated = allocated > 0;
        DataSize = dataSize;
    }

    ~CPluginData()
    {
        if (Allocated && Data)
            free((void*)Data);
        if (Name)
            free(Name);
    }
};

class CPluginDataInterface : public CPluginDataInterfaceAbstract
{
    virtual BOOL WINAPI CallReleaseForFiles() { return TRUE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return TRUE; }

    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir)
    {
        delete ((CPluginData*)file.PluginData);
    }

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) { ; }
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) { return FALSE; }

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize);

    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return TRUE; }

    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) { return NULL; }

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

// ****************************************************************************

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy
extern BOOL WindowsVistaAndLater;

char* LoadStr(int resID);
LPWSTR LoadStrW(int resID);
