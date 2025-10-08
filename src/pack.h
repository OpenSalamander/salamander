// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************
// Constants
//

// The system doesn't define the command line length for DOS applications, so
// we have to do it ourselves
#define DOS_MAX_PATH 80
// maximum possible command line length
#define PACK_CMDLINE_MAXLEN (MAX_PATH * 4)
// at what value do custom errors of salspawn.exe start
#define SPAWN_ERR_BASE 10000
// program for running 16-bit archivers (the -c parameter must match SPAWN_ERR_BASE)
extern const char* SPAWN_EXE_PARAMS;
// name of the salspawn program
extern const char* SPAWN_EXE_NAME;

// indexes of individual packers in the ArchiverConfig array
#define PACKJAR32INDEX 0
#define PACKJAR16INDEX 5
#define PACKRAR32INDEX 1
#define PACKRAR16INDEX 6
#define PACKARJ32INDEX 9
#define PACKARJ16INDEX 2
#define PACKLHA16INDEX 3
#define PACKUC216INDEX 4
#define PACKACE32INDEX 10
#define PACKACE16INDEX 11
#define PACKZIP32INDEX 7
#define PACKZIP16INDEX 8

// ****************************************************************************
// Types
//

// Program type
enum EPackExeType
{
    EXE_INVALID,
    EXE_32BIT,
    EXE_16BIT,
    EXE_END
};

// structure for holding the path to the packer
struct SPackLocation
{
    const char* Variable;
    const char* Executable;
    EPackExeType Type;
    const char* Value;
    BOOL Valid;
};

// Class storing information about found programs
class CPackACFound
{
public:
    char* FullName;     // name of the found program including the path
    CQuadWord Size;     // size
    FILETIME LastWrite; // date
    BOOL Selected;

    CPackACFound()
    {
        FullName = NULL;
        Selected = FALSE;
    }
    ~CPackACFound()
    {
        if (FullName != NULL)
            free(FullName);
    }
    BOOL Set(const char* fullName, const CQuadWord& size, FILETIME lastWrite);
    void InvertSelect() { Selected = !Selected; }
    void Select(BOOL newSelect) { Selected = newSelect; }
    BOOL IsSelected() { return Selected; }
    char* GetText(int column);
};

// array of found packers
class CPackACArray : public TIndirectArray<CPackACFound>
{
public:
    CPackACArray(int base, int delta, CDeleteType dt = dtDelete) : TIndirectArray<CPackACFound>(base, delta, dt) {}
    int AddAndCheck(CPackACFound* member);
    void InvertSelect(int index);
    const char* GetSelectedFullName();
};

enum EPackPackerType
{
    Packer_Standalone, // can pack and unpack
    Packer_Packer,     // can only pack
    Packer_Unpacker    // can only unpack
};

// Class holding the list of requested and later found packers
class CPackACPacker
{
protected:
    // source of data
    int Index;                // index into the ArchiversConfig array
    EPackPackerType Unpacker; // is it a packer, an unpacker or both?
    const char* Title;        // title describing the packer
    // what we want to find
    const char* Name;  // program name to search for
    EPackExeType Type; // should it be a 16 or 32 bit exe?
    // array of what we found
    CPackACArray Found;                        // the items we found
    CRITICAL_SECTION FoundDataCriticalSection; // critical section for accessing data

public:
    CPackACPacker(int index, EPackPackerType unpacker, const char* title,
                  const char* name, EPackExeType type) : Found(20, 10)
    {
        Index = index;
        Unpacker = unpacker;
        Title = title;
        Name = name;
        Type = type;
        HANDLES(InitializeCriticalSection(&FoundDataCriticalSection));
    }
    ~CPackACPacker()
    {
        HANDLES(DeleteCriticalSection(&FoundDataCriticalSection));
    }
    int CheckAndInsert(const char* path, const char* fileName, FILETIME lastWriteTime,
                       const CQuadWord& size, EPackExeType type);
    int GetCount();
    const char* GetText(int index, int column);
    BOOL IsTitle(int index) { return index < 0 ? TRUE : FALSE; }
    void InvertSelect(int index);
    int GetSelectState(int index);
    const char* GetSelectedFullName() { return Found.GetSelectedFullName(); }
    int GetArchiverIndex() { return Index; }
    EPackPackerType GetPackerType() { return Unpacker; }
    EPackExeType GetExeType() { return Type; }
};

// Type of table of found programs for autoconfig
typedef TIndirectArray<CPackACPacker> APackACPackersTable;

//*********************************************************************************
//
// CPackACListView
//
class CPackACDialog;

class CPackACListView : public CWindow
{
protected:
    const CPackACDialog* ACDialog;
    APackACPackersTable* PackersTable;

public:
    CPackACListView(const CPackACDialog* acDialog) : CWindow()
    {
        ACDialog = acDialog;
        PackersTable = NULL;
    }
    virtual ~CPackACListView()
    {
        if (PackersTable != NULL)
            delete PackersTable;
    }

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Initialize(APackACPackersTable* table);
    BOOL FindArchiver(unsigned int listViewIndex,
                      unsigned int* archiver, unsigned int* arcIndex);
    BOOL ConsiderItem(const char* path, const char* fileName, FILETIME lastWriteTime,
                      const CQuadWord& size, EPackExeType type);
    BOOL InitColumns();
    void SetColumnWidth();
    void InvertSelect(int index);
    int GetCount();
    int GetPackersCount() { return PackersTable->Count; }
    CPackACPacker* GetPacker(int item, int* index);
    CPackACPacker* GetPacker(int index)
    {
        return PackersTable != NULL ? PackersTable->At(index) : NULL;
    }
};

//*********************************************************************************
//
// CPackACDialog
//

// dialog for autoconfig
class CPackACDialog : public CCommonDialog
{
public:
    CPackACDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent, CArchiverConfig* archiverConfig,
                  char** drivesList, CObjectOrigin origin = ooStandard)
        : CCommonDialog(modul, resID, helpID, parent, origin)
    {
        ArchiverConfig = archiverConfig;
        SearchRunning = FALSE;
        WillExit = FALSE;
        DrivesList = drivesList;
        HSearchThread = NULL;
        StopSearch = NULL;
    }

    static DWORD WINAPI PackACDiskSearchThread(LPVOID instance);
    static unsigned int PackACDiskSearchThreadEH(LPVOID instance);
    DWORD DiskSearch();
    BOOL DirectorySearch(char* path);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Transfer(CTransferInfo& ti);
    void LayoutControls();
    void GetLayoutParams();
    void UpdateListViewItems(int index);
    void AddToExtensions(int foundIndex, int packerIndex, CPackACPacker* foundPacker);
    void RemoveFromExtensions(int foundIndex, int packerIndex, CPackACPacker* foundPacker);
    void AddToCustom(int foundIndex, int packerIndex, CPackACPacker* foundPacker);
    void RemoveFromCustom(int foundIndex, int packerIndex);
    BOOL MyGetBinaryType(LPCSTR filename, LPDWORD lpBinaryType);

protected:
    CArchiverConfig* ArchiverConfig;
    CPackACListView* ListView;
    char** DrivesList;
    HANDLE HSearchThread;
    HWND HStatusBar;
    BOOL SearchRunning;
    HANDLE StopSearch;
    BOOL WillExit;
    // data needed for laying out the dialog
    int HMargin;  // space left and right between the dialog frame and controls
    int VMargin;  // space at the bottom between buttons and the status bar
    int ButtonW1, // button dimensions (they need not all have the same width; e.g. the DE+HU+CHS version has them different)
        ButtonW2,
        ButtonW3,
        ButtonW4,
        ButtonW5;
    int ButtonMargin; // space between buttons
    int ButtonH;
    int StatusHeight; // height of the status bar
    int ListY;        // position of the result list
    int CheckH;       // height of the check box
    int MinDlgW;      // minimal dialog dimensions
    int MinDlgH;
};

//*********************************************************************************
//
// CPackACDrives
//

// dialog for specifying the disks searched in autoconfig
class CPackACDrives : public CCommonDialog
{
public:
    CPackACDrives(HINSTANCE modul, int resID, UINT helpID, HWND parent, char** drivesList,
                  CObjectOrigin origin = ooStandard)
        : CCommonDialog(modul, resID, helpID, parent, origin)
    {
        DrivesList = drivesList;
        EditLB = NULL;
    }

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    CEditListBox* EditLB;
    BOOL Dirty;
    char** DrivesList;
};

//*********************************************************************************
//
//
//

// Type of the error code table for external packers
typedef int TPackErrorTable[][2];

// Structure of the supported formats table
struct SPackFormat
{
    const char* FileExtension;  // archive file extension
    const char* MultiExtension; // extension format when using a split archive
                                // (the '?' character stands for any digit)
    int ArchiveBrowseIndex;     // format number; negative value is the index of a format
                                // processed internally (DLL), positive value is
                                // an index into the external packers table (non-modifying part)
    int ArchiveModifyIndex;     // format number; negative value is the index of a format
                                // processed internally (DLL), positive value is
                                // an index into the external packers table (modifying part)
};

// modified indirect array that calls delete[]
template <class DATA_TYPE>
class TPackIndirectArray : public TIndirectArray<DATA_TYPE>
{
public:
    TPackIndirectArray(int base, int delta, CDeleteType dt = dtDelete)
        : TIndirectArray<DATA_TYPE>(base, delta, dt) {}

    virtual ~TPackIndirectArray() { this->DestroyMembers(); }

protected:
    virtual void CallDestructor(void*& member)
    {
        if (this->DeleteType == dtDelete && (DATA_TYPE*)member != NULL)
            delete[] ((DATA_TYPE*)member);
    }
};

// type for an array of lines read from the pipe
typedef TPackIndirectArray<char> CPackLineArray;

// general function for parsing an archive listing
typedef BOOL (*FPackList)(const char* archiveFileName, CPackLineArray& lineArray,
                          CSalamanderDirectory& dir);

// constants used for SPackModifyTable::DelEmptyDir
#define PMT_EMPDIRS_DONOTDELETE 0        // no need to delete the empty directory separately
#define PMT_EMPDIRS_DELETE 1             // delete the empty directory explicitly - specify the path only
#define PMT_EMPDIRS_DELETEWITHASTERISK 2 // delete the empty directory explicitly - specify path with '*'

//
// Structure for the table of external packer definitions – modifying operations
//
struct SPackModifyTable
{
    //
    // general items
    //
    TPackErrorTable* ErrorTable; // pointer to the table of return codes
    BOOL SupportLongNames;       // TRUE if long file names are supported

    //
    // items for compression
    //
    const char* CompressInitDir; // directory in which the pack command will run
    const char* CompressCommand; // command used to pack the archive
    BOOL CanPackToDir;           // TRUE if the archiver program supports packing to a directory

    //
    // items for deletion from the archive
    //
    const char* DeleteInitDir; // directory in which the delete command will be executed
    const char* DeleteCommand; // command for deleting a file from the archive
    int DelEmptyDir;           // see constants PMT_EMPDIRS_XXX

    //
    // items for moving to the archive
    //
    const char* MoveInitDir; // directory in which the move command will be executed; NULL if unsupported
    const char* MoveCommand; // command for moving files into the archive; NULL if the program does not support move

    BOOL NeedANSIListFile; // should the list of files remain in ANSI (no conversion to OEM)
};

// Structure for the definition table of external packers - non-modifying operations
struct SPackBrowseTable
{
    //
    // general items
    //
    TPackErrorTable* ErrorTable; // pointer to the table of return codes
    BOOL SupportLongNames;       // TRUE if long file names are supported

    //
    // items for listing archive contents
    //
    const char* ListInitDir; // directory where the listing command is executed
    const char* ListCommand; // command for listing archive contents
    FPackList SpecialList;   // if it is not NULL, it is a function for parsing the listing
                             // in that case, the following items may be meaningless
    const char* StartString; // how the last header line begins
    int LinesToSkip;         // number of lines to ignore after StartString
    int AlwaysSkip;          // number of lines to always ignore after StartString (stop string not checked)
    int LinesPerFile;        // number of data lines in the listing for one file
    const char* StopString;  // how the first footer line begins
    unsigned char Separator; // if the archiver uses a special separator, it is stored here (otherwise e.g. a space)
    // indices of items in the listing; the number is the item's order on the line
    // (the first is 1); zero means the item does not exist
    short NameIdx;  // index of the name on the listing line
    short SizeIdx;  // index of the file size on the listing line
    short TimeIdx;  // index of the time on the listing line
    short DateIdx;  // index of the date on the listing line
    short AttrIdx;  // index of the attribute on the listing line
    short DateYIdx; // index of the year in the date (first, second or third)
    short DateMIdx; // index of the month in the date (first, second or third)

    //
    // items for decompression
    //
    const char* UncompressInitDir; // directory where unpacking the archive begins
    const char* UncompressCommand; // command for unpacking the archive

    //
    // items for extracting a single file without its path
    //
    const char* ExtractInitDir; // directory used when extracting a single file
    const char* ExtractCommand; // command for extracting a single file without path

    BOOL NeedANSIListFile; // should the list of files remain in ANSI (no conversion to OEM)
};

// configuration tables of predefined packers
extern const SPackBrowseTable PackBrowseTable[];
extern const SPackModifyTable PackModifyTable[];

#define ARC_UID_JAR32 1
#define ARC_UID_RAR32 2
#define ARC_UID_ARJ16 3
#define ARC_UID_LHA16 4
#define ARC_UID_UC216 5
#define ARC_UID_JAR16 6
#define ARC_UID_RAR16 7
#define ARC_UID_ZIP32 8
#define ARC_UID_ZIP16 9
#define ARC_UID_ARJ32 10
#define ARC_UID_ACE32 11
#define ARC_UID_ACE16 12

// class for storing data
class CArchiverConfigData
{
public:
    DWORD UID;                      // unique identifier of the archiver (see ARC_UID_XXX)
    char* Title;                    // name under which it appears in the configuration
    const char* PackerVariable;     // name of the variable expanded as the packer
    const char* UnpackerVariable;   // name of the variable expanded as the unpacker (NULL when it's a packer)
    const char* PackerExecutable;   // packer program name for searching on disk
    const char* UnpackerExecutable; // unpacker program name or NULL
    EPackExeType Type;              // archiver type (16bit, 32bit)
    BOOL ExesAreSame;               // true if PackExeFile is used for both pack and unpack
    char* PackExeFile;              // path to the pack program
    char* UnpackExeFile;            // path to the unpack program or NULL

public:
    CArchiverConfigData()
    {
        Empty();
    }

    ~CArchiverConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Title != NULL)
            free(Title);
        if (PackExeFile != NULL)
            free(PackExeFile);
        if (UnpackExeFile != NULL)
            free(UnpackExeFile);
        Empty();
    }

    void Empty()
    {
        UID = 0;
        Title = NULL;
        PackerVariable = NULL;
        UnpackerVariable = NULL;
        PackerExecutable = NULL;
        UnpackerExecutable = NULL;
        Type = EXE_END;
        ExesAreSame = FALSE;
        PackExeFile = NULL;
        UnpackExeFile = NULL;
    }

    BOOL IsValid()
    {
        if (Title == NULL || PackerVariable == NULL || PackerExecutable == NULL ||
            Type >= EXE_END || PackExeFile == NULL ||
            (!ExesAreSame && (UnpackerVariable == NULL || UnpackerExecutable == NULL ||
                              UnpackExeFile == NULL)))
            return FALSE;
        return TRUE;
    }
};

// configuration class
class CArchiverConfig
{
protected:
    TIndirectArray<CArchiverConfigData> Archivers;

public:
    CArchiverConfig(/*BOOL disableDefaultValues*/);
    void InitializeDefaultValues(); // replaces the original constructor call (j.r.)
    BOOL Load(CArchiverConfig& src);

    void DeleteAllArchivers() { Archivers.DestroyMembers(); }

    int AddArchiver();                 // returns the index of the created item or -1 on error
    void AddDefault(int SalamVersion); // adds archivers introduced since SalamVersion; when SalamVersion = -1 adds all of them (default configuration)

    // sets attributes; if something goes wrong, the item is removed from the array, destroyed and FALSE is returned
    BOOL SetArchiver(int index, DWORD uid, const char* title, EPackExeType type, BOOL exesAreSame,
                     const char* packerVariable, const char* unpackerVariable,
                     const char* packerExecutable, const char* unpackerExecutable,
                     const char* packExeFile, const char* unpackExeFile);

    int GetArchiversCount() { return Archivers.Count; } // returns the number of items in the array

    DWORD GetArchiverUID(int index) { return Archivers[index]->UID; }
    const char* GetArchiverTitle(int index) { return Archivers[index]->Title; }
    const char* GetPackerVariable(int index) { return Archivers[index]->PackerVariable; }
    const char* GetUnpackerVariable(int index) { return Archivers[index]->UnpackerVariable; }
    const char* GetPackerExecutable(int index) { return Archivers[index]->PackerExecutable; }
    const char* GetUnpackerExecutable(int index) { return Archivers[index]->UnpackerExecutable; }
    EPackExeType GetArchiverType(int index) { return Archivers[index]->Type; }
    void SetPackerExeFile(int index, const char* filename);
    void SetUnpackerExeFile(int index, const char* filename);
    const char* GetPackerExeFile(int index) { return Archivers[index]->PackExeFile; }
    const char* GetUnpackerExeFile(int index) { return Archivers[index]->UnpackExeFile; }
    const SPackModifyTable* GetPackerConfigTable(int index) { return &PackModifyTable[index]; }
    const SPackBrowseTable* GetUnpackerConfigTable(int index) { return &PackBrowseTable[index]; }
    BOOL ArchiverExesAreSame(int index) { return Archivers[index]->ExesAreSame; }
    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);
};

// Class holding the configuration of archive formats and the association of packers with them.
// Modified direct array that sorts unique strings alphabetically.
class CExtItem
{
public:
    CExtItem() { Ext = NULL; }
    ~CExtItem()
    {
        if (Ext != NULL)
            free(Ext);
    }
    char* GetExt() { return Ext; }
    int GetIndex() { return Index; }
    void Set(char* ext, int index)
    {
        Ext = ext;
        Index = index;
    }

protected:
    char* Ext;
    int Index;
};

class CStringArray : public TDirectArray<CExtItem>
{
public:
    CStringArray() : TDirectArray<CExtItem>(2, 3) {}

    BOOL SIns(CExtItem& item)
    {
        if (Count == 0)
        {
            Add(item);
            return TRUE;
        }
        int i = 0;
        while (i < Count && strcmp(At(i).GetExt(), item.GetExt()) > 0)
            i++;
        if (i == Count)
            Add(item);
        else if (!strcmp(At(i).GetExt(), item.GetExt()))
            return FALSE;
        else
            Insert(i, item);
        return TRUE;
    }
};

// data item
class CPackerFormatConfigData
{
public:
    char* Ext;         // list of extensions the archive can have
    BOOL UsePacker;    // true if PackerIndex is valid (we can also pack)
    int PackerIndex;   // reference to the packer table
    int UnpackerIndex; // reference to the unpacker table

    BOOL OldType; // indicates old data (version < 6) - assumes internal ZIP+TAR+PAK?

public:
    CPackerFormatConfigData()
    {
        Empty();
    }

    ~CPackerFormatConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Ext != NULL)
            free(Ext);
        Empty();
    }

    void Empty()
    {
        OldType = FALSE;
        Ext = NULL;
        UsePacker = FALSE;
        PackerIndex = 500;
        UnpackerIndex = 500;
    }

    BOOL IsValid()
    {
        if (Ext == NULL || UnpackerIndex == 500 || UsePacker && PackerIndex == 500)
            return FALSE;
        return TRUE;
    }
};

// configuration class
class CPackerFormatConfig
{
protected:
    TIndirectArray<CPackerFormatConfigData> Formats;
    CStringArray Extensions[256];

public:
    CPackerFormatConfig(/*BOOL disableDefaultValues*/);
    void InitializeDefaultValues(); // replaces the original constructor call (j.r.)
    BOOL Load(CPackerFormatConfig& src);

    void DeleteAllFormats() { Formats.DestroyMembers(); }

    int AddFormat();                   // returns the index of the created item or -1 on error
    void AddDefault(int SalamVersion); // adds archivers introduced since SalamVersion; when SalamVersion = -1 adds all of them (default configuration)

    // sets attributes; if something goes wrong, the item is removed from the array, destroyed and FALSE is returned
    BOOL SetFormat(int index, const char* ext, BOOL usePacker,
                   const int packerIndex, const int unpackerIndex, BOOL old);
    void SetOldType(int index, BOOL old) { Formats[index]->OldType = old; }
    void SetUnpackerIndex(int index, int unpackerIndex) { Formats[index]->UnpackerIndex = unpackerIndex; }
    void SetUsePacker(int index, BOOL usePacker) { Formats[index]->UsePacker = usePacker; }
    void SetPackerIndex(int index, int packerIndex) { Formats[index]->PackerIndex = packerIndex; }

    // creates search data; if anything goes wrong, it returns false and may also provide the error line and column
    BOOL BuildArray(int* line = NULL, int* column = NULL);

    // archiveNameLen serves only as an optimization; if it is -1, the function measures the string itself
    int PackIsArchive(const char* archiveName, int archiveNameLen = -1);

    int GetFormatsCount() { return Formats.Count; } // returns the number of items in the array

    //    BOOL SwapFormats(int index1, int index2);         // swaps two items in the array
    BOOL MoveFormat(int srcIndex, int dstIndex); // moves the item
    void DeleteFormat(int index);

    int GetUnpackerIndex(int index) { return Formats[index]->UnpackerIndex; }
    BOOL GetUsePacker(int index) { return Formats[index]->UsePacker; }
    int GetPackerIndex(int index) { return Formats[index]->PackerIndex; }
    const char* GetExt(int index) { return Formats[index]->Ext; }
    BOOL GetOldType(int index) { return Formats[index]->OldType; }

    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);
};

// ****************************************************************************
// Variables
//

extern char SpawnExe[MAX_PATH * 2];
extern BOOL SpawnExeInitialised;

struct CExecuteItem;
extern CExecuteItem ArgsCustomPackers[];
extern CExecuteItem CmdCustomPackers[];

extern CPackerFormatConfig PackerFormatConfig;
extern CArchiverConfig ArchiverConfig;

extern const TPackErrorTable JARErrors;
extern const TPackErrorTable RARErrors;
extern const TPackErrorTable ARJErrors;
extern const TPackErrorTable LHAErrors;
extern const TPackErrorTable UC2Errors;
extern const TPackErrorTable ZIP204Errors;
extern const TPackErrorTable UNZIP204Errors;
extern const TPackErrorTable ACEErrors;
extern BOOL (*PackErrorHandlerPtr)(HWND parent, const WORD errNum, ...);
extern const SPackFormat PackFormat[];

// ****************************************************************************
// Functions
//

// Initialization of the path to spawn.exe
BOOL InitSpawnName(HWND parent);

// setting error handling
void PackSetErrorHandler(BOOL (*handler)(HWND parent, const WORD errNum, ...));

// determine the contents of the archive
BOOL PackList(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
              CPluginDataInterfaceAbstract*& pluginData, CPluginData*& plugin);

// extract the selected files from the archive (calls UniversalUncompress)
BOOL PackUncompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* targetDir, const char* archiveRoot,
                    SalEnumSelection nextName, void* param);

// universal archive extraction (for unpacking the entire archive)
BOOL PackUniversalUncompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                             const char* initDir, BOOL expandInitDir, CFilesWindow* panel,
                             const BOOL supportLongNames, const char* archiveFileName,
                             const char* targetDir, const char* archiveRoot,
                             SalEnumSelection nextName, void* param, BOOL needANSIListFile);

// extract a single file from the archive (for viewer)
BOOL PackUnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       CFileData* fileData, const char* targetPath, const char* newFileName,
                       BOOL* renamingNotSupported);

// pack the selected files into the archive (calls UniversalCompress)
BOOL PackCompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                  const char* archiveRoot, BOOL move, const char* sourceDir,
                  SalEnumSelection2 nextName, void* param);

// universal archive packing (for creating a new archive)
BOOL PackUniversalCompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                           const char* initDir, BOOL expandInitDir, const BOOL supportLongNames,
                           const char* archiveFileName, const char* sourceDir,
                           const char* archiveRoot, SalEnumSelection2 nextName,
                           void* param, BOOL needANSIListFile);

// delete the selected files from the archive
BOOL PackDelFromArc(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* archiveRoot, SalEnumSelection nextName,
                    void* param);

// automatic configuration of packers
void PackAutoconfig(HWND parent);

// runs the external program cmdLine and interprets the return code according to errorTable
BOOL PackExecute(HWND parent, char* cmdLine, const char* currentDir, TPackErrorTable* const errorTable);

// callback for enumeration by mask (used to extract all files matching the mask)
const char* WINAPI PackEnumMask(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                const CFileData** fileData, void* param, int* errorOccured);

// performs variable substitution in the command line string
BOOL PackExpandCmdLine(const char* archiveName, const char* tgtDir, const char* lstName,
                       const char* extName, const char* varText, char* buffer,
                       const int bufferLen, char* DOSTmpName);

// performs variable substitution in the current directory string
BOOL PackExpandInitDir(const char* archiveName, const char* srcDir, const char* tgtDir,
                       const char* varText, char* buffer, const int bufferLen);

// default error handling function - only does TRACE_E
BOOL EmptyErrorHandler(HWND parent, const WORD err, ...);

// function for parsing the output from the UC2 packer
BOOL PackUC2List(const char* archiveFileName, CPackLineArray& lineArray,
                 CSalamanderDirectory& dir);
