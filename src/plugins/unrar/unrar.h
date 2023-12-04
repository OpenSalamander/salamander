// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "unrardll.h"

/* winnt.h
#define FILE_ATTRIBUTE_READONLY             0x00000001
#define FILE_ATTRIBUTE_HIDDEN               0x00000002
#define FILE_ATTRIBUTE_SYSTEM               0x00000004
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020
*/

#define FILE_ATTRIBUTE_MASK (0x37)

// silent flags
#define SF_DATA 0x00010000
#define SF_LONGNAMES 0x00020000
#define SF_IOERRORS 0x00040000
#define SF_ENCRYPTED 0x00080000
#define SF_ALLENRYPT 0x00100000
#define SF_PASSWD 0x00200000

// archive flags
#define AF_VOLUME 0x0001            // Volume attribute (archive volume)
#define AF_COMMENT 0x0002           // Archive comment present
#define AF_LOCK 0x0004              // Archive lock attribute
#define AF_SOLID 0x0008             // Solid attribute (solid archive)
#define AF_NEWNAMES 0x0010          // New volume naming scheme ('volname.partN.rar')
#define AF_AUTHENTICITY_INFO 0x0020 // Authenticity information present
#define AF_RECOVERY 0x0040          // Recovery record present
#define AF_BLOCK 0x0080             // Block headers are encrypted
#define AF_FIRST_VOLUME 0x0100      // First volume (set only by RAR 3.0 and later)

#define MAX_PASSWORD 256

#ifndef QWORD
#define QWORD unsigned __int64
#endif

struct CFileHeader
{
    char FileName[260];
    CQuadWord Size;
    CQuadWord CompSize;
    FILETIME Time;
    DWORD Attr;
    DWORD Flags;
};

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// used in CFileData.PluginData
class CRARFileData
{
public:
    CRARFileData(QWORD qwPackedSize, int nItem) : PackedSize(qwPackedSize), ItemNumber(nItem) {}

    QWORD PackedSize;
    int ItemNumber; // # of item in the archive, not offset
};

struct CRARExtractInfo
{
    int ItemNumber; // # of item in the archive, not offset
    TCHAR FileName[1];
};

// ****************************************************************************
//
// CPluginDataInterface
//

#define NUM_PASSWORDS 8

class CPluginDataInterface : public CPluginDataInterfaceAbstract
{
public:
    LPTSTR FirstArchiveVolume;
    DWORD Silent;
    unsigned int SolidEncrypted;
    char Password[MAX_PASSWORD];
    BOOL PasswordForOpenArchive;

    CPluginDataInterface(char* firstArchiveVolume = NULL)
    {
        FirstArchiveVolume = firstArchiveVolume;
        Silent = SolidEncrypted = 0;
        Password[0] = 0;
        PasswordForOpenArchive = FALSE;
    }
    virtual ~CPluginDataInterface()
    {
        if (FirstArchiveVolume)
            free(FirstArchiveVolume);
        memset(Password, 0, sizeof(Password));
    }
    LPCTSTR GetFirstVolume() { return FirstArchiveVolume; }

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
};

// ****************************************************************************
//
// CPluginInterface
//

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
protected:
    CSalamanderForOperationsAbstract* Salamander;
    char ArcFileName[MAX_PATH];
    HANDLE ArcHandle;
    unsigned ArcFlags;
    BOOL List;
    //unsigned int Silent;
    BOOL Abort;
    CQuadWord ProgressTotal;
    const char* ArcRoot;
    DWORD RootLen;
    char TargetName[MAX_PATH];
    HANDLE TargetFile;
    BOOL Success;
    //char Password[MAX_PASSWORD];
    //    BOOL FirstFile;
    BOOL NotWholeArchListed;
    CPluginDataInterface* PluginData;
    BOOL AllocateWholeFile;
    BOOL TestAllocateWholeFile;
    CDynamicString* ArchiveVolumes;

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
                                      SalEnumSelection2 next, void* nextParam) { return FALSE; }
    virtual BOOL WINAPI DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                          CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                          SalEnumSelection next, void* nextParam) { return FALSE; }
    virtual BOOL WINAPI UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                           const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                           CDynamicString* archiveVolumes);
    virtual BOOL WINAPI CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                        BOOL force, int panel) { return TRUE; }
    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) { return FALSE; }
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) {}
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) { return FALSE; }

    BOOL Error(int error, ...);

    BOOL Init();

    BOOL OpenArchive();
    BOOL ReadHeader(CFileHeader* header);
    BOOL ProcessFile(int operation, char* fileName);
    int ChangeVolProc(char* arcName, int mode);
    BOOL SafeSeek(CQuadWord position);
    int ProcessDataProc(unsigned char* addr, int size);
    int NeedPassword(char* password, int size);
    BOOL SetSolidPassword();
    BOOL SwitchToFirstVol(LPCTSTR arcName, BOOL* saveFirstVolume = NULL);
    BOOL MakeFilesList(TIndirectArray2<CRARExtractInfo>& files, SalEnumSelection next, void* nextParam, const char* targetDir);
    BOOL DoThisFile(CFileHeader* header, LPCTSTR arcName, LPCTSTR targetDir);
    BOOL ConstructMaskArray(TIndirectArray2<char>& maskArray, const char* masks);
    BOOL UnpackWholeArchiveCalculateProgress(TIndirectArray2<char>& masks);
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

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
    {
        if (pluginData)
            delete (CPluginDataInterface*)pluginData;
    }

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver();
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; }
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

// zatim staci tohleto misto konfigurace
#define OP_SKIPCONTINUED 0x01    // budeme preskakovat vsechny soubory, ktere zacinaji na predchozim volumu?
#define OP_NO_VOL_ATTENTION 0x02 // nebudeme upozornovat, kdyz nejde vylistovat cely archive

#define OP_SAVED_IN_REGISTRY (OP_SKIPCONTINUED | OP_NO_VOL_ATTENTION)

// These options are not to be saved in config and are local for the current archive only
#define OP_SKITHISFILE 0x80000000

struct CConfiguration
{
    DWORD Options;

    // Custom columns:
    BOOL ListInfoPackedSize;          // Show custom column Packed
    DWORD ColumnPackedSizeWidth;      // LO/HI-WORD: left/right panel: Width for Packed column
    DWORD ColumnPackedSizeFixedWidth; // LO/HI-WORD: left/right panel: FixedWidth for Packed column
};

extern struct CConfiguration Config;

LPCTSTR LoadStr(int resID);
void GetInfo(char* buffer, FILETIME* lastWrite, CQuadWord& size);

typedef int(PASCAL* FRARGetDllVersion)();
typedef HANDLE(PASCAL* FRAROpenArchiveEx)(struct RAROpenArchiveDataEx* ArchiveData);
typedef int(PASCAL* FRARCloseArchive)(HANDLE hArcData);
//typedef int (PASCAL *FRARReadHeader)(HANDLE hArcData,struct RARHeaderData *HeaderData);
typedef int(PASCAL* FRARReadHeaderEx)(HANDLE hArcData, struct RARHeaderDataEx* HeaderData);
typedef int(PASCAL* FRARProcessFile)(HANDLE hArcData, int Operation, const char* DestPath, char* DestName);
//typedef void (PASCAL *FRARSetChangeVolProc)(HANDLE hArcData,int (PASCAL *ChangeVolProc)(char *ArcName,int Mode));
//typedef void (PASCAL *FRARSetProcessDataProc)(HANDLE hArcData,int (PASCAL *ProcessDataProc)(unsigned char *Addr,int Size));
typedef void(PASCAL* FRARSetPassword)(HANDLE hArcData, char* Password);
typedef void(PASCAL* FRARSetCallback)(HANDLE hArcData, UNRARCALLBACK Callback, LPARAM UserData);

//***********************************************************************************
//
// Rutiny ze SHLWAPI.DLL
//

//BOOL PathAppend(LPTSTR  pPath, LPCTSTR pMore);
//BOOL PathRemoveFileSpec(LPTSTR pszPath);
//LPTSTR PathAddBackslash(LPTSTR pszPath);
LPTSTR PathFindExtension(LPTSTR pszPath);
/*void PathRemoveExtension(LPTSTR pszPath);
BOOL PathRenameExtension(LPTSTR pszPath, LPCTSTR pszExt);*/
//LPTSTR PathStripPath(LPTSTR pszPath);

#ifndef SetWindowLongPtr
// compiling on VC6 w/o reasonably new SDK
#define SetWindowLongPtr SetWindowLong
#define GetWindowLongPtr GetWindowLong
#define GWLP_WNDPROC GWL_WNDPROC
#endif

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
