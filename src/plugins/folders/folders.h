// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
extern CSalamanderGUIAbstract* SalamanderGUI;

// FS-name pridelene Salamanderem po loadu pluginu
extern char AssignedFSName[MAX_PATH];

BOOL InitFS();
void ReleaseFS();

// ****************************************************************************
//
// CPluginInterface
//

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

    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) {}

    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel);
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam)
    {
        return FALSE;
    }
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam) {}
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) {}
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) {}
    virtual void WINAPI Configuration(HWND parent){};

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData);

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; }
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

//
// ****************************************************************************
// CPluginDataInterface
//

// sloupec ziskany od aktualniho folderu; nemusi byt realne zobrazen
struct CShellColumn
{
    char Name[COLUMN_NAME_MAX];
    int Fmt;           // Alignment of the column heading and the subitem text in the column. (DETAILSINFO::fmt)
    int Char;          // Number of average-sized characters in the heading. (DETAILSINFO::cxChar)
    SHCOLSTATEF Flags; // Default column state (SHCOLSTATEF)
};

class CPluginDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    TDirectArray<CShellColumn> ShellColumns; // sloupce panelu dle shellu
    TDirectArray<DWORD> VisibleColumns;      // indexy do pole ShellColumn

    // specifikace cesty
    IShellFolder* Folder;    // interface folderu pro ziskavani ikon jeho sub-souboru/sub-folderu
    LPITEMIDLIST FolderPIDL; // absolutni (vztazeny k DesktopFolder) PIDL folderu (lokalni kopie)

    // pomocne promenne
    IShellFolder2* ShellFolder2; // IID_IShellFolder2: ziskano z Folder; muze byt NULL pokud neni implementovan
    IShellDetails* ShellDetails; // IID_IShellDeatils: muze byt NULL, pokud neni implementovan

public:
    CPluginDataInterface(IShellFolder* folder, LPCITEMIDLIST folderPIDL);
    ~CPluginDataInterface();

    BOOL IsGood();

    //************************************************
    // Implementace metod CPluginDataInterfaceAbstract
    //************************************************

    virtual BOOL WINAPI CallReleaseForFiles() { return TRUE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return TRUE; }

    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir);

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) {}
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) { return FALSE; }

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize);
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return FALSE; }
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon);
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2);

    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                  const CFileData* upperDir);
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth) {}
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth) {}

    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount);

    virtual BOOL WINAPI CanBeCopiedToClipboard() { return TRUE; }

    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) { return FALSE; }

    //************************************************
    // Nase metody
    //************************************************

    BOOL GetShellColumns(); // vytahne z shelu seznam sloupcu a naplni pole ShellColumns

    HRESULT GetDetailsHelper(int i, DETAILSINFO* di); // zastresuje vytazeni informace o sloupci pres ShellFolder2 nebo ShellDetails

    // zpristupnime nase private data callbackum ze Salamandera
    friend void WINAPI GetRowText();
};

//
// ****************************************************************************
// CPluginFSInterface
//
// sada metod pluginu, ktere potrebuje Salamander pro praci s file systemem

// kody chyb pri komunikaci mezi ListCurrentPath a ChangePath:
enum CFSErrorState
{
    fesOK,               // zadna chyba
    fesFatal,            // fatalni chyba pri listovani (ChangePath ma jen vratit FALSE)
    fesInaccessiblePath, // cesta nejde vylistovat, nutne zkratit cestu
};

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
public:
    LPITEMIDLIST CurrentPIDL;    // absolutni (vztazeny k DesktopFolder) PIDL aktualniho folderu
    IShellFolder* CurrentFolder; // IShellFolder pro PIDL
    IShellFolder* DesktopFolder; // Desktop folder, konstantni po celou existenci objektu
    IContextMenu2* ContextMenu2; // Context menu, pokud neni otevrenem, je NULL

    CFSErrorState ErrorState;

public:
    CPluginFSInterface();
    ~CPluginFSInterface();

    BOOL IsGood(); // vraci TRUE, pokud konstruktor probehl korektne

    // vytvori pole LPCITEMIDLIST pro oznacene/focused polozek
    // v pripade uspechu vraci TRUE a nastavuje promenne 'pidlArray' a 'itemsInArray'
    // 'pidlArray' je pak treba destruovat pomoci LocalFree (neuvolnovat vlastni polozky pole!)
    // v pripade nedostatku pameti vraci FALSE
    BOOL CreateIDListFromSelection(int panel, int selectedFiles, int selectedDirs,
                                   LPCITEMIDLIST** pidlArray, int* itemsInArray);

    IContextMenu* GetContextMenu(HWND hParent, LPCITEMIDLIST* pidlArray, int itemsInArray);
    IContextMenu2* GetContextMenu2(HWND hParent, LPCITEMIDLIST* pidlArray, int itemsInArray);

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
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent);
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) {}
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) { return FALSE; }
    virtual void WINAPI ShowSecurityInfo(HWND parent) {}
};

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

char* LoadStr(int resID);

#define SAL_ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
