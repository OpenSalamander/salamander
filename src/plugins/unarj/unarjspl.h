// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// silent flags
#define SF_DATA 0x00010000
#define SF_LONGNAMES 0x00020000
#define SF_IOERRORS 0x00040000
#define SF_ENCRYPTED 0x00080000
#define SF_TYPE 0x00100000
#define SF_VERSION 0x00200000
#define SF_METHOD 0x00400000

// general Salamander interface - valid from startup until the plugin is closed
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// ****************************************************************************
//
// CPluginInterface
//

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
protected:
    CSalamanderForOperationsAbstract* Salamander;
    char ArcFileName[MAX_PATH];
    BOOL List;
    DWORD Silent;
    BOOL Abort;
    CQuadWord ProgressTotal;
    const char* ArcRoot;
    DWORD RootLen;
    char TargetName[MAX_PATH];
    HANDLE TargetFile;
    BOOL UnPackWholeArchive;
    BOOL NotWholeArchListed;
    // when TRUE we add the size to the progress in ProcessDataProc,
    // set to FALSE when moving to the next volume so the progress bar does not overflow
    // ARJ does not contain information about the total size of the file,
    // only about the size of a single "portion" that is on the current volume
    BOOL UpdateProgress;
    BOOL AllocateWholeFile;
    BOOL TestAllocateWholeFile;

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

    BOOL WINAPI ChangeVolProc(char* volName, char* prevName, int mode);
    BOOL SafeSeek(DWORD position);
    BOOL WINAPI ProcessDataProc(const void* buffer, DWORD size);
    BOOL WINAPI ErrorProc(int error, BOOL flags);
    void SwitchToFirstVol(const char* arcName);
    BOOL MakeFilesList(TIndirectArray2<char>& files, SalEnumSelection next, void* nextParam, const char* targetDir);
    BOOL DoThisFile(CARJHeaderData* hdr, const char* arcName, const char* targetDir);
    BOOL ConstructMaskArray(TIndirectArray2<char>& maskArray, const char* masks);
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

extern HINSTANCE DLLInstance; // handle to the SPL - language-independent resources
extern HINSTANCE HLanguage;   // handle to the SLG - language-dependent resources

// for now this configuration placeholder is sufficient
#define OP_SKIPCONTINUED 0x01    // should we skip all files that start on the previous volume?
#define OP_NO_VOL_ATTENTION 0x02 // do not warn when the entire archive cannot be listed

extern DWORD Options; // configuration

char* LoadStr(int resID);
void GetInfo(char* buffer, FILETIME* lastWrite, unsigned size);

//***********************************************************************************
//
// Routines from SHLWAPI.DLL
//

//BOOL PathAppend(LPTSTR  pPath, LPCTSTR pMore);
//BOOL PathRemoveFileSpec(LPTSTR pszPath);
//LPTSTR PathAddBackslash(LPTSTR pszPath);
LPTSTR PathFindExtension(LPTSTR pszPath);
/*void PathRemoveExtension(LPTSTR pszPath);
BOOL PathRenameExtension(LPTSTR pszPath, LPCTSTR pszExt);*/
//LPTSTR PathStripPath(LPTSTR pszPath);

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
