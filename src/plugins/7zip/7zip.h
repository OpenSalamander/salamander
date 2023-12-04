// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "7za/CPP/Common/MyString.h"

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;
// interface pro komfortni praci se soubory
extern CSalamanderSafeFileAbstract* SalamanderSafeFile;
// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
extern CSalamanderGUIAbstract* SalamanderGUI;
// aktualni hodnota konfiguracni promenne Salamandera SALCFG_SORTBYEXTDIRSASFILES
extern int SortByExtDirsAsFiles;

// globalni promenne, do kterych se ulozi ukazatele na globalni promenne v Salamanderovi
// pro archiv - promenne se sdileji
extern const CFileData** TransferFileData;
extern int* TransferIsDir;
extern char* TransferBuffer;
extern int* TransferLen;
extern DWORD* TransferRowData;
extern CPluginDataInterfaceAbstract** TransferPluginDataIface;
extern DWORD* TransferActCustomData;

class C7zClient;

/*
// ****************************************************************************
//
// CPluginInterface
//

class CPluginInterfaceForViewer: public CPluginInterfaceForViewerAbstract
{
  public:
    virtual BOOL WINAPI ViewFile(const char *name, int left, int top, int width, int height,
                                 UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE *lock,
                                 BOOL *lockOwner, CSalamanderPluginViewerData *viewerData,
                                 int enumFilesSourceUID, int enumFilesCurrentIndex);
    virtual BOOL WINAPI CanViewFile(const char *name);
};
*/

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
protected:
public:
    CPluginInterfaceForArchiver();

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
    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) { return FALSE; }
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) {}
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) { return FALSE; }

    // vlastni metody pluginu
public:
    BOOL Init();
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) { return 0; };
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent, int id, DWORD eventMask);
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

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver();
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}

protected:
    void SetDefaultConfiguration();
};

class CPluginDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    C7zClient* Client;
    UString Password;

    friend class CPluginInterfaceForArchiver; // For acessing Password

public:
    CPluginDataInterface(C7zClient* client);
    ~CPluginDataInterface();

    virtual BOOL WINAPI CallReleaseForFiles() { return TRUE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return TRUE; }
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir);

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) {}
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) { return TRUE; }

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) { return NULL; }
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return FALSE; }
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

    C7zClient* Get7zClient() { return Client; }
};

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

struct CCompressParams
{
    int CompressLevel; // compression level
    enum EMethod
    {
        LZMA = 0,
        PPMd = 1,
        LZMA2 = 2
    } Method; // method

    int DictSize;      // dictionary size (in KB)
    int WordSize;      // word size
    BOOL SolidArchive; // create solid archive
};

// konfigurace
struct CConfig
{
    BOOL ShowExtendedOptions;
    BOOL ExtendedListInfo;
    BOOL ListInfoPackedSize;
    BOOL ListInfoMethod;

    int ColumnPackedSizeFixedWidth; // LO/HI-WORD: levy/pravy panel: FixedWidth pro sloupec PackedSize
    int ColumnPackedSizeWidth;      // LO/HI-WORD: levy/pravy panel: Width pro sloupec PackedSize
    int ColumnMethodFixedWidth;     // LO/HI-WORD: levy/pravy panel: FixedWidth pro sloupec Method
    int ColumnMethodWidth;          // LO/HI-WORD: levy/pravy panel: Width pro sloupec Method

    // version 2
    CCompressParams CompressParams;
};

// konfigurace
extern CConfig Config;

char* LoadStr(int resID);
void GetInfo(char* buffer, const FILETIME* lastWrite, CQuadWord size);
void GetInfo(char* buffer, const FILETIME* lastWrite, UINT64 size);

BOOL Error(int resID, BOOL quiet = FALSE, ...);
BOOL Error(HWND hParent, int resID, BOOL quiet = FALSE, ...);
//BOOL Error(char *msg, DWORD err, BOOL quiet = FALSE);
BOOL SysError(int resID, DWORD err, BOOL quiet = FALSE, ...);

//BOOL SysError(int title, int error, ...);

BOOL Warning(int resID, BOOL quiet, ...);

BOOL SafeDeleteFile(const char* fileName, BOOL& silent);

extern CPluginInterface PluginInterface;

// constants for compression level
#define COMPRESS_LEVEL_STORE 0
#define COMPRESS_LEVEL_FASTEST 1
#define COMPRESS_LEVEL_FAST 3
#define COMPRESS_LEVEL_NORMAL 5
#define COMPRESS_LEVEL_MAXIMUM 7
#define COMPRESS_LEVEL_ULTRA 9

#define DUMP_MEM_OBJECTS

#if defined(DUMP_MEM_OBJECTS) && defined(_DEBUG)
#define CRT_MEM_CHECKPOINT \
    _CrtMemState ___CrtMemState; \
    _CrtMemCheckpoint(&___CrtMemState);
#define CRT_MEM_DUMP_ALL_OBJECTS_SINCE _CrtMemDumpAllObjectsSince(&___CrtMemState);

#else //DUMP_MEM_OBJECTS
#define CRT_MEM_CHECKPOINT ;
#define CRT_MEM_DUMP_ALL_OBJECTS_SINCE ;

#endif //DUMP_MEM_OBJECTS
