// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

extern HWND ProgressDialogActivateDrop;

//
// ****************************************************************************
// CZIPUnpackProgress
//

#define ZIP_UNPACK_NUMLINES 5

class CProgressBar;
class CStaticText;

class CZIPUnpackProgress : public CCommonDialog
{
protected:
    const char* RemapNameFrom; // mapping of names from the tmp directory
    const char* RemapNameTo;   // to the name of the archive we are unpacking from
    BOOL FileProgress;         // for the single-progress variant: TRUE="File:", FALSE="Total:"

public:
    CZIPUnpackProgress();
    CZIPUnpackProgress(const char* title, HWND parent, const CQuadWord& totalSize, CITaskBarList3* taskBarList3);

    void Init();

    void Set(const char* title, HWND parent, const CQuadWord& totalSize, BOOL fileProgress);
    void Set(const char* title, HWND parent, const CQuadWord& totalSize1, const CQuadWord& totalSize2);
    void SetTotal(const CQuadWord& total1, const CQuadWord& total2); // CQuadWord(-1, -1) means do not set

    int AddSize(int size, BOOL delayedPaint);                                       // returns "continue?"
    int SetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint); // returns "continue?"; size == CQuadWord(-1, -1) means "do not set"

    void NewLine(const char* txt, BOOL delayedPaint);
    void EnableCancel(BOOL enable);

    void SetRemapNames(const char* nameFrom, const char* nameTo);
    void DoRemapNames(char* txt, int bufLen);

    void SetTaskBarList3(CITaskBarList3* taskBarList3);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL HasTwoProgress(); // TRUE if dual-progress; otherwise FALSE

    void DispatchMessages(); // dispatches queued messages, giving the UI a chance to repaint and handle button clicks

    void FlushDataToControls(); // pushes dirty data into the controls (texts, progress bars)

    const char* Title;   // caption - pointer inside the LoadStr buffer (do not keep for long)
    BOOL Cancel;         // user canceled the operation; the dialog should end as soon as possible
    DWORD LastTickCount; // used to detect when it is time to repaint the changed data

    // note: Summary, Summary2, and Lines will be NULL if memory is low; account for that in the dialog code
    CProgressBar* Summary;
    CProgressBar* Summary2;
    CStaticText* Lines[ZIP_UNPACK_NUMLINES];

    char LinesCache[ZIP_UNPACK_NUMLINES][300]; // queued texts waiting to be displayed later
    int CacheIndex;                            // index to LinesCache array to be filled by the next line
    BOOL CacheIsDirty;                         // does the cache need to be sent to the screen?

    CQuadWord TotalSize,
        ActualSize;
    CQuadWord TotalSize2,
        ActualSize2;
    BOOL SizeIsDirty;  // does Size need to be pushed to the screen?
    BOOL Size2IsDirty; // does Size2 need to be pushed to the screen?

    CITaskBarList3* TaskBarList3; // pointer to the interface owned by Salamander's main window
};

//
// ****************************************************************************
// CSalamanderForOperations
//

class CSalamanderForOperations : public CSalamanderForOperationsAbstract
{
protected:
    CFilesWindow* Panel;
    CZIPUnpackProgress UnpackProgress; // UnpackProgress dialog
    BOOL ProgressDialog2;              // TRUE = dual-progress, FALSE = single-progress dialog
    HWND FocusWnd;

    // control mechanism
    DWORD ThreadID; // they may call us only through the thread in which our pointer was passed
    BOOL Destroyed; // if TRUE, the object has already been destroyed

public:
    CSalamanderForOperations(CFilesWindow* panel);
    ~CSalamanderForOperations();

    // PROGRESS DIALOG: the dialog contains one or two progress meters (depending on 'twoProgressBars' FALSE/TRUE)
    // opens the progress dialog with the title 'title'; 'parent' is the parent window of the progress dialog (if
    // NULL, the main window is used); if it contains only one progress meter, it can be labeled
    // as "File" ('fileProgress' is TRUE) or "Total" ('fileProgress' is FALSE)
    virtual void WINAPI OpenProgressDialog(const char* title, BOOL twoProgressBars, HWND parent, BOOL fileProgress);
    // prints the text 'txt' (even multiple lines - splits to lines) into the progress dialog
    virtual void WINAPI ProgressDialogAddText(const char* txt, BOOL delayedPaint);
    // if 'totalSize1' is not CQuadWord(-1, -1), sets 'totalSize1' as 100 percent of the first progress meter,
    // if 'totalSize2' is not CQuadWord(-1, -1), sets 'totalSize2' as 100 percent of the second progress meter
    // (for a progress dialog with a single progress meter, 'totalSize2' must be CQuadWord(-1, -1))
    virtual void WINAPI ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2);
    // if 'size1' is not CQuadWord(-1, -1), sets the value of 'size1' (size1/total1*100 percent) on the first progress meter,
    // if 'size2' is not CQuadWord(-1, -1), sets the value of 'size2' (size2/total2*100 percent) on the second progress meter
    // (for a progress dialog with a single progress meter, 'size2' must be CQuadWord(-1, -1)); returns whether the action 
    // should continue (FALSE = end)
    virtual BOOL WINAPI ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint);
    // adds the size 'size' (optionally to both progress meters) (size/total*100 percent progress),
    // returns whether the action should continue (FALSE = end)
    virtual BOOL WINAPI ProgressAddSize(int size, BOOL delayedPaint);
    // enables/disables the Cancel button
    virtual void WINAPI ProgressEnableCancel(BOOL enable);
    // returns the progress dialog HWND (useful for displaying errors and prompts while the progress dialog is open)
    virtual HWND WINAPI ProgressGetHWND() { return UnpackProgress.HWindow; }
    // closes the progress dialog
    virtual void WINAPI CloseProgressDialog();

    // moves all files from the 'source' directory to the 'target' directory,
    // additionally remaps the prefixes of displayed names ('remapNameFrom' -> 'remapNameTo')
    // returns whether the operation succeeded
    virtual BOOL WINAPI MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                  const char* remapNameTo);
};

//
// ****************************************************************************
// CSalamanderDirectory
//

class CSalamanderDirectory;

// CSalamanderDirectoryAddCache is used to optimize adding files
// to CSalamanderDirectory (AddFile method)
struct CSalamanderDirectoryAddCache
{
    int PathLen;               // number of valid characters in 'Path'
    char Path[MAX_PATH];       // cached path
    CSalamanderDirectory* Dir; // pointer to the CSalamanderDirectory to which files and directories with the 'Path' path are being added
};

class CSalamanderDirectory : public CSalamanderDirectoryAbstract
{
protected:
    CFilesArray Dirs;                              // names of subdirectories (contents stored in SalamDirs at the same index)
    TDirectArray<CSalamanderDirectory*> SalamDirs; // pointers to CSalamanderDirectory (pointers are NULL until first access, then the objects are allocated)
    CFilesArray Files;                             // file names
    DWORD ValidData;                               // validity mask for data from CFileData
    DWORD Flags;                                   // object flags (see SALDIRFLAG_XXX)
    BOOL IsForFS;                                  // TRUE if this is a sal-dir for FS, FALSE if it is a sal-dir for archives
    CSalamanderDirectoryAddCache* AddCache;        // if not NULL, used to optimize adding files via AddFile; otherwise unused

public:
    CSalamanderDirectory(BOOL isForFS, DWORD validData = VALID_DATA_ALL_FS_ARC, DWORD flags = -1 /* set according to isForFS */);
    ~CSalamanderDirectory();

    // *********************************************************************************
    // methods of the CSalamanderDirectoryAbstract interface
    // *********************************************************************************
    virtual void WINAPI Clear(CPluginDataInterfaceAbstract* pluginData);
    virtual void WINAPI SetValidData(DWORD validData);
    virtual void WINAPI SetFlags(DWORD flags);
    virtual BOOL WINAPI AddFile(const char* path, CFileData& file, CPluginDataInterfaceAbstract* pluginData);
    virtual BOOL WINAPI AddDir(const char* path, CFileData& dir, CPluginDataInterfaceAbstract* pluginData);

    virtual int WINAPI GetFilesCount() const;
    virtual int WINAPI GetDirsCount() const;
    virtual CFileData const* WINAPI GetFile(int i) const;
    virtual CFileData const* WINAPI GetDir(int i) const;
    virtual CSalamanderDirectoryAbstract const* WINAPI GetSalDir(int i) const;
    virtual void WINAPI SetApproximateCount(int files, int dirs);

    // *********************************************************************************
    // helper methods (inaccessible from plugins)
    // *********************************************************************************

    // for optimizing the AddFile method
    void AllocAddCache();
    void FreeAddCache();

    // depending on Flags either StrICmp or strcmp (StrCmpEx) - selects case sensitive/insensitive comparison
    int SalDirStrCmp(const char* s1, const char* s2);
    int SalDirStrCmpEx(const char* s1, int l1, const char* s2, int l2);

    // calls 'pluginData'.ReleaseFilesOrDirs (releasing plug-in data) for all files (if 'releaseFiles' is TRUE)
    // and all directories (if 'releaseDirs' is TRUE)
    void ReleasePluginData(CPluginDataInterfaceEncapsulation& pluginData, BOOL releaseFiles,
                           BOOL releaseDirs);

    // returns directories from the specified path (relative to this Salamander directory)
    CFilesArray* GetDirs(const char* path);
    // returns files from the specified path (relative to this Salamander directory)
    CFilesArray* GetFiles(const char* path);

    // returns the parent directory for the path 'path' (returns NULL for root and unknown paths)
    const CFileData* GetUpperDir(const char* path);

    // returns the sum of sizes of all contained files; note: counters must be reset beforehand
    CQuadWord GetSize(int* dirsCount = NULL, int* filesCount = NULL, TDirectArray<CQuadWord>* sizes = NULL);
    // returns the directory size - sum of all files in it; note: counters must be reset beforehand
    CQuadWord GetDirSize(const char* path, const char* dirName, int* dirsCount = NULL,
                         int* filesCount = NULL, TDirectArray<CQuadWord>* sizes = NULL);
    // returns the salamander-dir for the specified directory; if 'readOnly' is TRUE, 
    // the returned salamander-dir object must not be modified
    CSalamanderDirectory* GetSalamanderDir(const char* path, BOOL readOnly);
    // returns the salamander-dir for the specified directory index; 
    // the returned salamander-dir object must not be modified
    CSalamanderDirectory* GetSalamanderDir(int i);
    // returns the index of the directory specified by name
    int GetIndex(const char* dir);
    // is there a directory at this index?
    BOOL IsDirectory(int i) { return i >= 0 && i < Dirs.Count; }
    // is there a file at this index?
    BOOL IsFile(int i) { return i >= Dirs.Count && i < Dirs.Count + Files.Count; }
    // returns the file for the specified index
    CFileData* GetFileEx(int i)
    {
        if (i >= Dirs.Count && i < Dirs.Count + Files.Count)
            return &Files[i - Dirs.Count];
        else
            return NULL;
    }
    // returns the directory for the specified index
    CFileData* GetDirEx(int i)
    {
        if (i >= 0 && i < Dirs.Count)
            return &Dirs[i];
        else
            return NULL;
    }

    DWORD GetValidData() { return ValidData; }

    DWORD GetFlags() { return Flags; }

protected:
    // helper method: allocates a salamander-dir object at index 'index' in the SalamDirs array,
    // returns a pointer to the object (or NULL on error)
    CSalamanderDirectory* AllocSalamDir(int index);

    BOOL FindDir(const char* path, const char*& s, int& i, const CFileData& file,
                 CPluginDataInterfaceAbstract* pluginData, const char* archivePath);

    // the AddFileInt and AddDirInt methods return a pointer to CSalamanderDirectory on success,
    // into which the item was added; otherwise they return NULL
    CSalamanderDirectory* AddFileInt(const char* path, CFileData& file,
                                     CPluginDataInterfaceAbstract* pluginData,
                                     const char* archivePath);
    CSalamanderDirectory* AddDirInt(const char* path, CFileData& dir,
                                    CPluginDataInterfaceAbstract* pluginData,
                                    const char* archivePath);
};

// checks the free space at path 'path' and, if it is >= totalSize, asks the user whether to continue
BOOL TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize, const char* messageTitle);

//
// ****************************************************************************
// CPackerConfig
//

// item type in the custom packers table
struct SPackCustomPacker
{
    const char* CopyArgs[2];
    const char* MoveArgs[2];
    int Title[2];
    const char* Ext;
    BOOL SupLN;
    BOOL Ansi;
    const char* Exe;
};

// item type in the custom unpackers table
struct SPackCustomUnpacker
{
    const char* Args;
    int Title;
    const char* Ext;
    BOOL SupLN;
    BOOL Ansi;
    const char* Exe;
};

// custom packer tables
extern SPackCustomPacker CustomPackers[];
extern SPackCustomUnpacker CustomUnpackers[];

#define CUSTOMPACKER_EXTERNAL 0

class CPackerConfigData
{
public:
    char* Title; // name shown to the user
    char* Ext;   // standard extension (without the dot)
    int Type;    // internal (-1, -2, ...; see CPlugins for details) / external (0; additional fields apply)
                 // note: see OldType below

    // data for external packers
    char* CmdExecCopy;
    char* CmdArgsCopy;
    BOOL SupportMove;
    char* CmdExecMove;
    char* CmdArgsMove;
    BOOL SupportLongNames;
    BOOL NeedANSIListFile;

    // helper flag to detect the data layout - TRUE = legacy -> 'Type' (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL OldType;

public:
    CPackerConfigData()
    {
        Empty();
    }

    ~CPackerConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Title != NULL)
            free(Title);
        if (Ext != NULL)
            free(Ext);
        if (OldType && Type == 1 ||
            !OldType && Type == CUSTOMPACKER_EXTERNAL)
        {
            if (CmdExecCopy != NULL)
                free(CmdExecCopy);
            if (CmdArgsCopy != NULL)
                free(CmdArgsCopy);
            if (CmdExecMove != NULL)
                free(CmdExecMove);
            if (CmdArgsMove != NULL)
                free(CmdArgsMove);
        }
        Empty();
    }

    void Empty()
    {
        OldType = FALSE;
        Title = NULL;
        Ext = NULL;
        Type = 1;
        CmdExecCopy = NULL;
        CmdArgsCopy = NULL;
        SupportMove = FALSE;
        CmdExecMove = NULL;
        CmdArgsMove = NULL;
        SupportLongNames = FALSE;
        NeedANSIListFile = FALSE;
    }

    BOOL IsValid()
    {
        if (Title == NULL || Ext == NULL)
            return FALSE;
        if ((OldType && Type == 1 || !OldType && Type == CUSTOMPACKER_EXTERNAL) &&
            (CmdExecCopy == NULL || CmdArgsCopy == NULL))
            return FALSE;
        if (SupportMove && (CmdExecMove == NULL || CmdArgsMove == NULL))
            return FALSE;
        return TRUE;
    }
};

class CPackerConfig
{
public:
    BOOL Move; // move or copy into the archive?

protected:
    int PreferedPacker;
    TIndirectArray<CPackerConfigData> Packers; // array of packer information, elements of type (CPackerConfigData *)

public:
    CPackerConfig(/*BOOL disableDefaultValues = FALSE*/);
    void InitializeDefaultValues(); // j.r. replaces the original constructor call
    BOOL Load(CPackerConfig& src);

    void DeleteAllPackers() { Packers.DestroyMembers(); }

    int AddPacker(BOOL toFirstIndex = FALSE); // returns the index of the created item or -1 on error
    void AddDefault(int SalamVersion);        // adds archivers introduced since SalamVersion

    // sets attributes; if something goes wrong, removes the item from the array, destroys it, and returns FALSE
    // old == TRUE -> 'type' uses the old convention (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL SetPacker(int index, int type, const char* title, const char* ext, BOOL old,
                   BOOL supportLongNames = FALSE, BOOL supportMove = FALSE,
                   const char* cmdExecCopy = NULL, const char* cmdArgsCopy = NULL,
                   const char* cmdExecMove = NULL, const char* cmdArgsMove = NULL,
                   BOOL needANSIListFile = FALSE);
    BOOL SetPackerTitle(int index, const char* title);
    void SetPackerType(int index, int type) { Packers[index]->Type = type; }
    void SetPackerOldType(int index, BOOL oldType) { Packers[index]->OldType = oldType; }
    void SetPackerSupMove(int index, BOOL supMove) { Packers[index]->SupportMove = supMove; }
    int GetPackersCount() { return Packers.Count; } // returns the number of items in the array
                                                    //    BOOL SwapPackers(int index1, int index2);         // swaps two items in the array
    BOOL MovePacker(int srcIndex, int dstIndex);    // moves an item
    void DeletePacker(int index);
    void SetPackerCmdExecCopy(int index, const char* cmd)
    {
        if (Packers[index]->CmdExecCopy)
            free(Packers[index]->CmdExecCopy);
        Packers[index]->CmdExecCopy = DupStr(cmd);
    }
    void SetPackerCmdExecMove(int index, const char* cmd)
    {
        if (Packers[index]->CmdExecMove)
            free(Packers[index]->CmdExecMove);
        Packers[index]->CmdExecMove = DupStr(cmd);
    }

    int GetPackerType(int index) { return Packers[index]->Type; }
    BOOL GetPackerOldType(int index) { return Packers[index]->OldType; }
    const char* GetPackerTitle(int index) { return Packers[index]->Title; }
    const char* GetPackerExt(int index) { return Packers[index]->Ext; }
    BOOL GetPackerSupLongNames(int index) { return Packers[index]->SupportLongNames; }
    BOOL GetPackerSupMove(int index) { return Packers[index]->SupportMove; }
    const char* GetPackerCmdExecCopy(int index) { return Packers[index]->CmdExecCopy; }
    const char* GetPackerCmdArgsCopy(int index) { return Packers[index]->CmdArgsCopy; }
    const char* GetPackerCmdExecMove(int index) { return Packers[index]->CmdExecMove; }
    const char* GetPackerCmdArgsMove(int index) { return Packers[index]->CmdArgsMove; }
    BOOL GetPackerNeedANSIListFile(int index) { return Packers[index]->NeedANSIListFile; }

    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);

    int GetPreferedPacker() // returns -1 if no preferred one exists
    {
        return (PreferedPacker < Packers.Count) ? PreferedPacker : -1;
    }
    void SetPreferedPacker(int i) { PreferedPacker = i; }

    BOOL ExecutePacker(CFilesWindow* panel, const char* zipFile, BOOL move,
                       const char* sourcePath, SalEnumSelection2 next, void* param);
};

//
// ****************************************************************************
// CUnpackerConfig
//

#define CUSTOMUNPACKER_EXTERNAL 0

class CUnpackerConfigData
{
public:
    char* Title; // name shown to the user
    char* Ext;   // list of standard extensions separated by semicolons
    int Type;    // internal (-1, -2, ...; see CPlugins for details) / external (0; additional fields apply)
                 // note: see OldType below

    // data for external packers
    char* CmdExecExtract;
    char* CmdArgsExtract;
    BOOL SupportLongNames;
    BOOL NeedANSIListFile;

    // helper flag to detect the data layout - TRUE = legacy -> 'Type' (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL OldType;

public:
    CUnpackerConfigData()
    {
        Empty();
    }

    ~CUnpackerConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Title != NULL)
            free(Title);
        if (Ext != NULL)
            free(Ext);
        if (OldType && Type == 1 ||
            !OldType && Type == CUSTOMUNPACKER_EXTERNAL)
        {
            if (CmdExecExtract != NULL)
                free(CmdExecExtract);
            if (CmdArgsExtract != NULL)
                free(CmdArgsExtract);
        }
        Empty();
    }

    void Empty()
    {
        OldType = FALSE;
        Title = NULL;
        Ext = NULL;
        Type = 1;
        CmdExecExtract = NULL;
        CmdArgsExtract = NULL;
        SupportLongNames = FALSE;
        NeedANSIListFile = FALSE;
    }

    BOOL IsValid()
    {
        if (Title == NULL || Ext == NULL)
            return FALSE;
        if ((OldType && Type == 1 || !OldType && Type == CUSTOMUNPACKER_EXTERNAL) &&
            (CmdExecExtract == NULL || CmdArgsExtract == NULL))
            return FALSE;
        return TRUE;
    }
};

class CUnpackerConfig
{
protected:
    int PreferedUnpacker;
    TIndirectArray<CUnpackerConfigData> Unpackers; // array of packer information, elements of type (CUnpackerConfigData *)

public:
    CUnpackerConfig(/*BOOL disableDefaultValues = FALSE*/);
    void InitializeDefaultValues(); // j.r. replaces the original constructor call
    BOOL Load(CUnpackerConfig& src);

    void DeleteAllUnpackers() { Unpackers.DestroyMembers(); }

    int AddUnpacker(BOOL toFirstIndex = FALSE); // returns the index of the created item or -1 on error
    void AddDefault(int SalamVersion);          // adds archivers introduced since SalamVersion

    // sets attributes; if something goes wrong, removes the item from the array, destroys it, and returns FALSE
    // old == TRUE -> 'type' uses the old convention (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL SetUnpacker(int index, int type, const char* title, const char* ext, BOOL old,
                     BOOL supportLongNames = FALSE,
                     const char* cmdExecExtract = NULL, const char* cmdArgsExtract = NULL,
                     BOOL needANSIListFile = FALSE);
    BOOL SetUnpackerTitle(int index, const char* title);
    void SetUnpackerType(int index, int type) { Unpackers[index]->Type = type; }
    void SetUnpackerOldType(int index, BOOL oldType) { Unpackers[index]->OldType = oldType; }
    int GetUnpackersCount() { return Unpackers.Count; } // returns the number of items in the array
                                                        //    BOOL SwapUnpackers(int index1, int index2);         // swaps two items in the array
    BOOL MoveUnpacker(int srcIndex, int dstIndex);      // moves an item

    void DeleteUnpacker(int index);

    int GetUnpackerType(int index) { return Unpackers[index]->Type; }
    BOOL GetUnpackerOldType(int index) { return Unpackers[index]->OldType; }
    const char* GetUnpackerTitle(int index) { return Unpackers[index]->Title; }
    const char* GetUnpackerExt(int index) { return Unpackers[index]->Ext; }
    BOOL GetUnpackerSupLongNames(int index) { return Unpackers[index]->SupportLongNames; }
    const char* GetUnpackerCmdExecExtract(int index) { return Unpackers[index]->CmdExecExtract; }
    const char* GetUnpackerCmdArgsExtract(int index) { return Unpackers[index]->CmdArgsExtract; }
    BOOL GetUnpackerNeedANSIListFile(int index) { return Unpackers[index]->NeedANSIListFile; }

    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);

    int GetPreferedUnpacker() // returns -1 if no preferred one exists
    {
        return (PreferedUnpacker < Unpackers.Count) ? PreferedUnpacker : -1;
    }
    void SetPreferedUnpacker(int i) { PreferedUnpacker = i; }

    BOOL ExecuteUnpacker(HWND parent, CFilesWindow* panel, const char* zipFile, const char* mask,
                         const char* targetDir, BOOL delArchiveWhenDone, CDynamicString* archiveVolumes);
};

extern CPackerConfig PackerConfig;
extern CUnpackerConfig UnpackerConfig;

int DialogError(HWND parent, DWORD flags, const char* fileName,
                const char* error, const char* title);
int DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                    const char* fileName2, const char* fileData2);
int DialogQuestion(HWND parent, DWORD flags, const char* fileName,
                   const char* question, const char* title);

BOOL ViewFileInPluginViewer(const char* pluginSPL,
                            CSalamanderPluginViewerData* pluginData,
                            BOOL useCache, const char* rootTmpPath,
                            const char* fileNameInCache, int& error);
