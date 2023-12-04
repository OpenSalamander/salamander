// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* winnt.h
#define FILE_ATTRIBUTE_READONLY             0x00000001
#define FILE_ATTRIBUTE_HIDDEN               0x00000002
#define FILE_ATTRIBUTE_SYSTEM               0x00000004
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020
*/

#define FILE_ATTRIBUTE_MASK (0x37)

// silent flags
//#define SF_DATA       0x00010000
#define SF_LONGNAMES 0x00020000
#define SF_IOERRORS 0x00040000
#define SF_CONTINUED 0x00080000

#define FF_EXTRFILE 0x0001 //soubor, ktery je rozbalovan, ne CAB soubor
#define FF_SKIPFILE 0x0002 //soubor, ktery je preskakovan, ale musi se vybalit bez zapisu na disk

struct CFile
{
    HANDLE Handle;
    char FileName[MAX_PATH];
    DWORD Flags;
    DWORD cabOffset; // for sfx archives
};

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

//CAB action
#define CA_LIST 0
#define CA_UNPACK 1
#define CA_UNPACK_ONE_FILE 2
#define CA_UNPACK_WHOLE_ARCHIVE 3

//cab cache items
struct CCABCacheEntry
{
    char CABName[CB_MAX_CABINET_NAME];
    char CABPath[CB_MAX_CAB_PATH];

    CCABCacheEntry(char* name, char* path);
};

// ****************************************************************************
//
// CPluginInterface
//

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
protected:
    CSalamanderForOperationsAbstract* Salamander;
    BOOL Abort;
    DWORD Action;
    DWORD Silent;
    BOOL NotWholeArchListed;
    BOOL IOError;
    BOOL FirstCAB;
    BOOL FirstCABINET_INFO;
    char CurrentCAB[CB_MAX_CABINET_NAME];
    char CurrentCABPath[CB_MAX_CAB_PATH];
    char NextCAB[CB_MAX_CABINET_NAME];
    char NextDISK[CB_MAX_DISK_NAME];
    int NextCABIndex;
    int CurrentCABIndex;
    USHORT SetID;
    CSalamanderDirectoryAbstract* Dir; //used by ListFile
    DWORD Count;
    CQuadWord ProgressTotal;
    const char* ArcRoot;
    DWORD RootLen;
    TIndirectArray2<char> Files;
    const char* TargetDir;
    TIndirectArray2<char> Masks;
    TIndirectArray2<CCABCacheEntry> CABCache;
    BOOL AllocateWholeFile;
    BOOL TestAllocateWholeFile;

    const char* NameInArchive; //unpack one file
    BOOL OneFileSuccess;
    /*
    char ArcFileName[MAX_PATH];
    HANDLE ArcHandle;
    BOOL List;
    unsigned int Silent;
    BOOL Abort;
    CQuadWord ProgressTotal;
    const char * ArcRoot;
    DWORD RootLen;
    char TargetName[MAX_PATH];
    HANDLE TargetFile;
    BOOL Success;
    BOOL UnPackWholeArchive;
    char Password[MAX_PASSWORD];
    BOOL FirstFile;
    BOOL NotWholeArchListed;*/

public:
    CPluginInterfaceForArchiver() : Files(256), Masks(16), CABCache(16) { ; }

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

    BOOL FDIError(int erfOper);

    BOOL Init();

    //BOOL OpenArchive();
    //BOOL ReadHeader(CFileHeader * header);
    //BOOL ProcessFile(int operation, char * fileName);
    //int ChangeVolProc(char *arcName, int mode);
    //int ProcessDataProc(unsigned char *addr, int size);
    //void SwitchToFirstVol(const char * arcName);
    //BOOL MakeFilesList(TIndirectArray2<char> &files, SalEnumSelection next, void *nextParam, const char * targetDir);
    //BOOL DoThisFile(CFileHeader * header, const char * arcName, const char * targetDir);
    BOOL ConstructMaskArray(TIndirectArray2<char>& maskArray, const char* masks);
    BOOL UpdateCABCache(char* name, char* path);
    BOOL GetCachedCABPath(char* name, char* path);
    BOOL ListFile(char* fileName, DWORD size, WORD date, WORD time, DWORD attributes);
    BOOL DoThisFile(char* fileName);
    INT_PTR UnpackFile(char* fileName, DWORD size, WORD date, WORD time, DWORD attributes);
    BOOL MakeFilesList(TIndirectArray2<char>& files, SalEnumSelection next, void* nextParam, const char* targetDir);
    //BOOL DoThisFile(CFileHeader * header, const char * arcName, const char * targetDir);
    INT_PTR Open(char* pszFile, int oflag, int pmode);
    UINT Read(INT_PTR hf, void* pv, UINT cb);
    UINT Write(INT_PTR hf, void* pv, UINT cb);
    int Close(INT_PTR hf);
    long SafeSeek(CFile* file, DWORD distance, DWORD method);
    long Seek(INT_PTR hf, long dist, int seektype);
    INT_PTR Notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin);
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force) { return TRUE; }

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) { return; }

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

extern DWORD Options; // konfigurace

char* LoadStr(int resID);
void GetInfo(char* buffer, FILETIME* lastWrite, unsigned size);

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
