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

// je-li DEMOPLUG_QUIET definovano, setri se dotazy v message-boxech
#define DEMOPLUG_QUIET

// je-li ENABLE_DYNAMICMENUEXT definovano, nepridavaji se polozky do menu
// v CPluginInterface::Connect (vola se jen jednou pri loadu pluginu),
// ale opakovane pred kazdym otevrenim menu, viz FUNCTION_DYNAMICMENUEXT
// (umoznuje pluginu menit dynamicky menu podle aktualnich potreb)
//#define ENABLE_DYNAMICMENUEXT

// globalni data
extern const char* PluginNameEN; // neprekladane jmeno pluginu, pouziti pred loadem jazykoveho modulu + pro debug veci
extern HINSTANCE DLLInstance;    // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;      // handle k SLG-cku - jazykove zavisle resourcy

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
extern CSalamanderGUIAbstract* SalamanderGUI;

BOOL InitViewer();
void ReleaseViewer();

BOOL InitFS();
void ReleaseFS();

// FS-name pridelene Salamanderem po loadu pluginu
extern char AssignedFSName[MAX_PATH];
extern int AssignedFSNameLen;

// vyvolava se pro prvni instanci DEMOPLUGu: pripadny vymaz vlastniho tmp-diru
// s kopiemi souboru vybalenymi z archivu predchozimi instancemi DEMOPLUGu
void ClearTEMPIfNeeded(HWND parent);

// ukazatele na tabulky mapovani na mala/velka pismena
extern unsigned char* LowerCase;
extern unsigned char* UpperCase;

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
// pro archiv i pro FS - promenne se sdileji
extern const CFileData** TransferFileData;
extern int* TransferIsDir;
extern char* TransferBuffer;
extern int* TransferLen;
extern DWORD* TransferRowData;
extern CPluginDataInterfaceAbstract** TransferPluginDataIface;
extern DWORD* TransferActCustomData;

// globalni data
extern char Str[MAX_PATH];
extern int Number;
extern int Selection; // "second" in configuration dialog
extern BOOL CheckBox;
extern int RadioBox;                       // radio 2 in configuration dialog
extern BOOL CfgSavePosition;               // ukladat pozici okna/umistit dle hlavniho okna
extern WINDOWPLACEMENT CfgWindowPlacement; // neplatne, pokud CfgSavePosition != TRUE
extern int Size2FixedWidth;                // sloupec Size2 (archiver): LO/HI-WORD: levy/pravy panel: FixedWidth
extern int Size2Width;                     // sloupec Size2 (archiver): LO/HI-WORD: levy/pravy panel: Width
extern int CreatedFixedWidth;              // sloupec Created (FS): LO/HI-WORD: levy/pravy panel: FixedWidth
extern int CreatedWidth;                   // sloupec Created (FS): LO/HI-WORD: levy/pravy panel: Width
extern int ModifiedFixedWidth;             // sloupec Modified (FS): LO/HI-WORD: levy/pravy panel: FixedWidth
extern int ModifiedWidth;                  // sloupec Modified (FS): LO/HI-WORD: levy/pravy panel: Width
extern int AccessedFixedWidth;             // sloupec Accessed (FS): LO/HI-WORD: levy/pravy panel: FixedWidth
extern int AccessedWidth;                  // sloupec Accessed (FS): LO/HI-WORD: levy/pravy panel: Width
extern int DFSTypeFixedWidth;              // sloupec DFS Type (FS): LO/HI-WORD: levy/pravy panel: FixedWidth
extern int DFSTypeWidth;                   // sloupec DFS Type (FS): LO/HI-WORD: levy/pravy panel: Width

extern DWORD LastCfgPage; // start page (sheet) in configuration dialog

// image-list pro jednoduche ikony FS
extern HIMAGELIST DFSImageList;

// [0, 0] - pro otevrena okna viewru: konfigurace pluginu se zmenila
#define WM_USER_VIEWERCFGCHNG WM_APP + 3246
// [0, 0] - pro otevrena okna viewru: je treba podriznou historie
#define WM_USER_CLEARHISTORY WM_APP + 3247
// [0, 0] - pro otevrena okna vieweru: Salamander pregeneroval fonty, mame zavolat SetFont() listam
#define WM_USER_SETTINGCHANGE WM_APP + 3248

char* LoadStr(int resID);

// prikazy pluginoveho menu
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
#define MENUCMD_CHECKDEMOPLUGTMPDIR 15 // prvni instance DEMOPLUGu: pripadny vymaz vlastniho tmp-diru s kopiemi souboru vybalenymi z archivu predchozimi instancemi DEMOPLUGu
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
        // zadna vlastni data plugin v CFileData nema, takze neni potreba nic menit/alokovat
    }
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir)
    {
        // zadna vlastni data plugin v CFileData nema, takze neni potreba nic menit/alokovat
        return TRUE; // vracime uspech
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
    int ActiveFSCount; // pocet aktivnich FS interfacu (jen pro kontrolu dealokace)

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
    HANDLE Lock;                      // 'lock' objekt nebo NULL (do signaled stavu az zavreme soubor)
    char Name[MAX_PATH];              // jmeno souboru nebo ""
    CRendererWindow Renderer;         // vnitrni okno vieweru
    HIMAGELIST HGrayToolBarImageList; // toolbar a menu v sedivem provedeni (pocitano z barevneho)
    HIMAGELIST HHotToolBarImageList;  // toolbar a menu v barevnem provedeni

    DWORD Enablers[vweCount];

    HWND HRebar; // drzi MenuBar a ToolBar
    CGUIMenuPopupAbstract* MainMenu;
    CGUIMenuBarAbstract* MenuBar;
    CGUIToolBarAbstract* ToolBar;

    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index aktualniho souboru ve vieweru ve zdroji

public:
    CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);

    HANDLE GetLock();

    // je-li 'setLock' TRUE, dojde k nastaveni 'Lock' do signaled stavu (je
    // potreba po zavreni souboru)
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

extern CWindowQueue ViewerWindowQueue; // seznam vsech oken viewru
extern CThreadQueue ThreadQueue;       // seznam vsech threadu oken

//
// ****************************************************************************
// FILE-SYSTEM
//

//
// ****************************************************************************
// CConnectData
//
// struktura pro predani dat z "Connect" dialogu do nove vytvoreneho FS

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

// struktura pro predani dat z "Connect" dialogu do nove vytvoreneho FS
extern CConnectData ConnectData;

//
// ****************************************************************************
// CDeleteProgressDlg
//
// ukazka vlastniho progress dialogu pro operaci Delete na FS

class CDeleteProgressDlg : public CCommonDialog
{
protected:
    CGUIProgressBarAbstract* ProgressBar;
    BOOL WantCancel; // TRUE pokud uzivatel chce Cancel

    // protoze dialog nebezi ve vlastnim threadu, je zbytecne pouzivat WM_TIMER
    // metodu; stejna nas musi zavolat pro vypumpovani message queue
    DWORD LastTickCount; // pro detekci ze uz je treba prekreslit zmenena data

    char TextCache[MAX_PATH];
    BOOL TextCacheIsDirty;
    DWORD ProgressCache;
    BOOL ProgressCacheIsDirty;

public:
    CDeleteProgressDlg(HWND parent, CObjectOrigin origin = ooStandard);

    void Set(const char* fileName, DWORD progress, BOOL dalayedPaint);

    // vyprazdni message queue (volat dostatecne casto) a umozni prekreslit, stisknout Cancel...
    // vraci TRUE, pokud uzivatel chce prerusit operaci
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
    char Path[MAX_PATH]; // buffer pro plne jmeno souboru/adresare pouzivane pri nacitani ikon
    char* Name;          // ukazatel do Path za posledni backslash cesty (na jmeno)

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
// pamet top-indexu listboxu v panelu - pouziva CPluginFSInterface pro korektni
// chovani ExecuteOnFS (zachovani top-indexu po vstupu a vystupu z podadresare)

#define TOP_INDEX_MEM_SIZE 50 // pocet pamatovanych top-indexu (urovni), minimalne 1

class CTopIndexMem
{
protected:
    // cesta pro posledni zapamatovany top-index
    char Path[MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // zapamatovane top-indexy
    int TopIndexesCount;                // pocet zapamatovanych top-indexu

public:
    CTopIndexMem() { Clear(); }
    void Clear()
    {
        Path[0] = 0;
        TopIndexesCount = 0;
    }                                                 // vycisti pamet
    void Push(const char* path, int topIndex);        // uklada top-index pro danou cestu
    BOOL FindAndPop(const char* path, int& topIndex); // hleda top-index pro danou cestu, FALSE->nenalezeno
};

//
// ****************************************************************************
// CPluginFSInterface
//
// sada metod pluginu, ktere potrebuje Salamander pro praci s file systemem

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
public:
    char Path[MAX_PATH];             // aktualni cesta
    BOOL PathError;                  // TRUE pokud se nepovedla ListCurrentPath (path error), bude se volat ChangePath
    BOOL FatalError;                 // TRUE pokud se nepovedla ListCurrentPath (fatal error), bude se volat ChangePath
    CTopIndexMem TopIndexMem;        // pamet top-indexu pro ExecuteOnFS()
    BOOL CalledFromDisconnectDialog; // TRUE = user chce disconnectnout toto FS z Disconnect dialogu (klavesa F12)

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

// spolecny interface pro pluginova data archivatoru
extern CArcPluginDataInterface ArcPluginDataInterface;

// pomocna promenna pro testy
extern CPluginFSInterfaceAbstract* LastDetachedFS;

// rozhrani pluginu poskytnute Salamanderovi
extern CPluginInterface PluginInterface;

// otevre konfiguracni dialog; pokud jiz existuje, zobrazi hlasku a vrati se
void OnConfiguration(HWND hParent);

// otevre About okno
void OnAbout(HWND hParent);
