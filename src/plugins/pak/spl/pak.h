// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

#define STATUS_OK 1

struct CFileInfo
{
    char* Name;
    DWORD Status;
    DWORD DirDepth;
    DWORD Size;

    CFileInfo(const char* name, DWORD status, DWORD dirDepth, DWORD size);
    ~CFileInfo();
};

struct COptDlgData
{
    DWORD PakSize;
    DWORD ValData;
};

// ****************************************************************************
//
// CPluginInterface
//

#define SF_LONGNAMES 0x00010000
#define SF_IOERRORS 0x00020000
#define SF_LARGE 0x00040000
#define SF_OVEWRITEALL 0x00080000
#define SF_SKIPALL 0x00100000

#define OPTIMIZE_MENUID 0x01

class CPluginInterfaceForArchiver : public CPluginInterfaceForArchiverAbstract
{
public:
    CSalamanderForOperationsAbstract* Salamander;
    CPakIfaceAbstract* PakIFace;
    const char* PakFileName;
    HANDLE IOFile;
    const char* IOFileName;
    DWORD Silent;
    BOOL Abort;
    CQuadWord ProgressTotal;

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
                                        BOOL force, int panel) { return TRUE; }
    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) { return FALSE; }
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) {}
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) { return FALSE; }

    /*void InitPlugin(CSalamanderForOperationsAbstract * salamander, CPakIfaceAbstract * pakIFace,
                    const char * pakFileName);*/

    //    HWND GetParentWindow();

    BOOL MakeFileList(TIndirectArray2<CFileInfo>& files, const char* archiveRoot,
                      SalEnumSelection next, void* nextParam);

    BOOL UnpackFiles(TIndirectArray2<CFileInfo>& files, const char* arcFile,
                     int rootLen, const char* targetDir);

    BOOL ConstructMaskArray(TIndirectArray2<char>& maskArray, const char* masks);

    BOOL MakeFileList2(TIndirectArray2<char>& masks, TIndirectArray2<CFileInfo>& files);

    BOOL DeleteFiles(const char* archiveRoot, SalEnumSelection next, void* nextParam);

    BOOL MakeFileList3(TIndirectArray2<CFileInfo>& files, BOOL* del, const char* archiveRoot, const char* sourcePath,
                       SalEnumSelection2 next, void* nextParam);

    BOOL DelFilesToBeOverwritten(unsigned* deleted);

    BOOL AddFiles(TIndirectArray2<CFileInfo>& files, unsigned deleted, const char* sourPath, const char* archiveRoot);

    void DeleteSourceFiles(TIndirectArray2<CFileInfo>& files, const char* sourcePath);
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);
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

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) {}
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) {}
    virtual void WINAPI Configuration(HWND parent) {}

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) { return; }

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver();
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() { return NULL; }
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

class CPakCallbacks : public CPakCallbacksAbstract
{
    BOOL UserBreak;

public:
    CPluginInterfaceForArchiver* Plugin;

    CPakCallbacks(CPluginInterfaceForArchiver* plugin);

    //vraci TRUE v pripade ze se ma pokracovat dal, nebo FALSE
    //ma-li oprace skoncit
    virtual BOOL HandleError(DWORD flags, int errorID, va_list arglist);

    //precte data z vystupniho souboru
    //volano behem baleni souboru
    //vraci TRUE kdyz ma operace pokracovat
    virtual BOOL Read(void* buffer, DWORD size);

    //zapise data do vystupniho souboru
    //volano behem vybalovani souboru
    //vraci TRUE kdyz ma operace pokracovat
    virtual BOOL Write(void* buffer, DWORD size);

    //informuje o prubehu zpracovani dat
    //prida velikost 'size'
    //vraci kdyz ma oprace pokracovat
    virtual BOOL AddProgress(unsigned size);

    //informuje o probihajicim mazani
    virtual BOOL DelNotify(const char* fileName, unsigned fileProgressTotal);

    BOOL SafeSeek(DWORD position);
};

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy
/*
extern FPAKGetIFace PAKGetIFace;
extern FPAKReleaseIFace PAKReleaseIFace;
*/

extern CPluginInterfaceForArchiver InterfaceForArchiver;
extern CPluginInterfaceForMenuExt InterfaceForMenuExt;

char* LoadStr(int resID);
void FreeFileInfo(void* file);
void GetInfo(char* buffer, FILETIME* lastWrite, unsigned size);
INT_PTR WINAPI OptimizeDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
