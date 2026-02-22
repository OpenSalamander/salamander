// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define CREATE_DIR_SIZE CQuadWord(4096, 0) // operation cost estimates (uncached measurements based on worker thread runtimes)
#define MOVE_DIR_SIZE CQuadWord(5050, 0)
#define DELETE_DIR_SIZE CQuadWord(2400, 0)
#define DELETE_DIRLINK_SIZE CQuadWord(2400, 0)
#define MOVE_FILE_SIZE CQuadWord(6500, 0)
#define COPY_MIN_FILE_SIZE CQuadWord(4096, 0) // must be at least 1 (otherwise the DoCopyFile pre-copy space allocation test stops working)
#define CONVERT_MIN_FILE_SIZE CQuadWord(4096, 0)
#define COMPRESS_ENCRYPT_MIN_FILE_SIZE CQuadWord(4096, 0)
#define DELETE_FILE_SIZE CQuadWord(2300, 0)
#define CHATTRS_FILE_SIZE CQuadWord(500, 0)
#define MAX_OP_FILESIZE 6500 // WARNING: highest allowed value in this group

// 4/2012 - increased the buffer to ten times the old size; large files over the network now reach speeds 
// comparable to Total Commander and finish 2-3x faster than with the previous one-tenth buffer
// verified on local disks and across the network; I see no downside to the larger buffer
#define OPERATION_BUFFER (10 * 32768)          // 320KB buffer for Copy and Move
#define REMOVABLE_DISK_COPY_BUFFER 65536       // 64KB buffer for Copy and Move on removable media (floppy, ZIP)
#define ASYNC_COPY_BUF_SIZE_512KB (128 * 1024) // 128KB buffer for files up to 512KB
#define ASYNC_COPY_BUF_SIZE_2MB (256 * 1024)   // 256KB buffer for files up to 2MB
#define ASYNC_COPY_BUF_SIZE_8MB (512 * 1024)   // 512KB buffer for files up to 8MB
#define ASYNC_COPY_BUF_SIZE (1024 * 1024)      // maximum buffer size for asynchronous copy (Explorer caps it at 1MB); WARNING: must be >= RETRYCOPY_TAIL_MINSIZE
#define ASYNC_SLOW_COPY_BUF_SIZE (8 * 1024)    // 8KB buffer for slow copy (primarily network disks over VPN)
#define ASYNC_SLOW_COPY_BUF_MINBLOCKS 12

// WARNING: HIGH_SPEED_LIMIT must be >= the largest value in the previous group (OPERATION_BUFFER,
//        REMOVABLE_DISK_COPY_BUFFER, ASYNC_COPY_BUF_SIZE)
#define HIGH_SPEED_LIMIT (1024 * 1024) // when the speed-limit >= this number we throttle by inserting a braking \ Sleep after (speed-limit / HIGH_SPEED_LIMIT_BRAKE_DIV) bytes, if needed
#define HIGH_SPEED_LIMIT_BRAKE_DIV 10  // see HIGH_SPEED_LIMIT for details

void InitWorker();
void ReleaseWorker();

struct CChangeAttrsData
{
    BOOL ChangeCompression;
    BOOL ChangeEncryption;

    BOOL ChangeTimeModified;
    FILETIME TimeModified;
    BOOL ChangeTimeCreated;
    FILETIME TimeCreated;
    BOOL ChangeTimeAccessed;
    FILETIME TimeAccessed;
};

struct CConvertData // data used by ocConvert
{
    char CodeTable[256];
    int EOFType;
};

class COperations;
struct CProgressDlgArrItem;

struct CStartProgressDialogData
{
    COperations* Script;
    const char* Caption;
    CChangeAttrsData* AttrsData;
    CConvertData* ConvertData;
    CProgressDlgArrItem* NewDlg;
    BOOL OperationWasStarted;
    HANDLE ContEvent;
    RECT MainWndRectClipR; // coordinates used to center the parentless progress dialog (background operations)
    RECT MainWndRectByR;   // coordinates used to center the parentless progress dialog (background operations)
};

struct CProgressData
{
    const char *Operation,
        *Source,
        *Preposition,
        *Target;
};

//
// ****************************************************************************
// CTransferSpeedMeter
//
// measures data transfer speed (borrowed from the FTP plugin)

#define TRSPMETER_ACTSPEEDSTEP 200        // for calculating transfer speed: sample interval in milliseconds (must not be 0)
#define TRSPMETER_ACTSPEEDNUMOFSTEPS 25   // for calculating transfer speed: number of samples used (more samples make the drop smoother when the first entry falls out of the queue)
#define TRSPMETER_NUMOFSTOREDPACKETS 40   // for calculating transfer speed: number of recent "packets" to remember (for low-speed speed calculations) (must not be 0)
#define TRSPMETER_STPCKTSMININTERVAL 2000 // for calculating transfer speed: minimum time span between the first and last stored "packet" when using them for (low-speed) speed calculations

class CTransferSpeedMeter
{
protected:
    // WARNING: access to this object only in critical section COperations::StatusCS

    // transfer speed calculation:
    DWORD TransferedBytes[TRSPMETER_ACTSPEEDNUMOFSTEPS + 1]; // circular queue storing bytes transferred during the last N intervals (time intervals) + a working slot (accumulating the current interval)
    int ActIndexInTrBytes;                                   // index of the last (current) entry in TransferedBytes
    DWORD ActIndexInTrBytesTimeLim;                          // timestamp boundary (ms) for the last entry in TransferedBytes (bytes keep accumulating until this time)
    int CountOfTrBytesItems;                                 // number of slots in TransferedBytes (completed ones + the working slot)

    DWORD LastPacketsSize[TRSPMETER_NUMOFSTOREDPACKETS + 1]; // circular queue with sizes of the last N+1 "packets"
    DWORD LastPacketsTime[TRSPMETER_NUMOFSTOREDPACKETS + 1]; // circular queue with receive times of the last N+1 "packets"
    int ActIndexInLastPackets;                               // index in LastPacketsSize and LastPacketsTime for writing the next received "packet" (when full it also points to the oldest "packet")
    int CountOfLastPackets;                                  // number of "packets" in LastPacketsSize/LastPacketsTime (number of valid entries)
    DWORD MaxPacketSize;                                     // largest packet size we expect

public:
    BOOL ResetSpeed; // TRUE = the meter should be reset before the next speed measuring (call JustConnected) - the speed drop was so large we ended up displaying zero speed

public:
    CTransferSpeedMeter();

    // resets the object for reuse
    // may be called from any thread
    void Clear();

    // writes the transfer speed in bytes per second to 'speed' (must not be NULL)
    // may be called from any thread
    void GetSpeed(CQuadWord* speed);

    // call when speed measurement begins
    // may be called from any thread
    void JustConnected();

    // call after some of the data are transfered; report a data chunk: 'count' bytes
    // transferred in 'time'; 'maxPacketSize' is the largest amount expected
    // before the next BytesReceived() call
    void BytesReceived(DWORD count, DWORD time, DWORD maxPacketSize);

    // tunes 'progressBufferLimit' according to current received packets data;
    // 'lastFileBlockCount' is the limit we must not cross (we consider only continuous 
    // copying of a single file; the counter 'lastFileBlockCount' is overflow-safe and
    // values > 1000000 simply mean "a lot", the exact figure is irrelevant); 'lastFileStartTime' 
    // is the GetTickCount() captured when the most recent file copy started
    void AdjustProgressBufferLimit(DWORD* progressBufferLimit, DWORD lastFileBlockCount,
                                   DWORD lastFileStartTime);
};

//
// ****************************************************************************
// CProgressSpeedMeter
//
// object for the progress rate measuring - to estimate time remaining

#define PRSPMETER_ACTSPEEDSTEP 500         // for calculating progress speed: sample interval in milliseconds (must not be 0)
#define PRSPMETER_ACTSPEEDNUMOFSTEPS 60    // for calculating progress speed: number of samples (more samples make the drop smoother when the first entry falls out of the queue)
#define PRSPMETER_NUMOFSTOREDPACKETS 100   // for calculating progress speed: number of recent "packets" to remember (for low-speed speed calculations) (must not be 0)
#define PRSPMETER_STPCKTSMININTERVAL 10000 // for calculating progress speed: minimum time span between the first and last stored "packet" when using them for (low-speed) speed calculations

class CProgressSpeedMeter
{
protected:
    // WARNING: access this object only in critical section COperations::StatusCS

    // progress speed calculation:
    DWORD TransferedBytes[PRSPMETER_ACTSPEEDNUMOFSTEPS + 1]; // circular queue storing bytes transferred during the last N intervals (time intervals) + a working slot (accumulating the current interval)
    int ActIndexInTrBytes;                                   // index of the last (current) entry in TransferedBytes
    DWORD ActIndexInTrBytesTimeLim;                          // timestamp boundary (ms) for the last entry in TransferedBytes (bytes keep accumulating until this time)
    int CountOfTrBytesItems;                                 // number of slots in TransferedBytes (completed ones + the working slot)

    DWORD LastPacketsSize[PRSPMETER_NUMOFSTOREDPACKETS + 1]; // circular queue with sizes of the last N+1 "packets"
    DWORD LastPacketsTime[PRSPMETER_NUMOFSTOREDPACKETS + 1]; // circular queue with receive times of the last N+1 "packets"
    int ActIndexInLastPackets;                               // index in LastPacketsSize and LastPacketsTime for writing the next received "packet" (when full it also points to the oldest "packet")
    int CountOfLastPackets;                                  // number of "packets" in LastPacketsSize/LastPacketsTime (number of valid entries)
    DWORD MaxPacketSize;                                     // largest packet size we expect

public:
    CProgressSpeedMeter();

    // resets the object for reuse
    // may be called from any thread
    void Clear();

    // writes the progress speed in bytes per second to 'speed' (must not be NULL)
    // may be called from any thread
    void GetSpeed(CQuadWord* speed);

    // call when progress measurement begins
    // may be called from any thread
    void JustConnected();

    // call after some of the data are transfered; report a data chunk: 'count' bytes
    // transferred in 'time'; 'maxPacketSize' is the largest amount expected
    // before the next BytesReceived() call
    void BytesReceived(DWORD count, DWORD time, DWORD maxPacketSize);
};

enum COperationCode
{
    ocCopyFile,
    ocMoveFile,
    ocDeleteFile,
    ocCreateDir,
    ocMoveDir,
    ocDeleteDir,
    ocDeleteDirLink,
    ocChangeAttrs, // WARNING: requested attributes are stored in TargetName (applies to every type; treat it as a DWORD)
    ocCountSize,
    ocConvert,
    ocLabelForSkipOfCreateDir, // label to jump to when the script skips on ocCreateDirXXX; WARNING: SourceName and TargetName store the LO- and HI-DWORD of the total file sizes (including ADS) contained in the skipped directory; WARNING: Attr stores the ocCreateDirXXX index in the COperations array for that directory
    ocCopyDirTime,             // Move/Copy: when filterCriteria->PreserveDirTime==TRUE copy the directory timestamps; WARNING: lastWrite is stored in SourceName and Attr (applies to every type; just two DWORDs)
};

#define OPFL_OVERWROLDERALRTESTED 0x00000001 // the "overwrite older, skip other existing" test has already been performed
#define OPFL_AS_ENCRYPTED 0x00000002         // the target file/directory should have the Encrypted attribute set
#define OPFL_COPY_ADS 0x00000004             // copy the file's/directory's ADS as well
#define OPFL_SRCPATH_IS_NET 0x00000008       // the source path is a network path
#define OPFL_SRCPATH_IS_FAST 0x00000010      // the source path is a disk, USB disk, flash drive, flash-card reader, CD, DVD, or RAM disk (not a network or floppy)
#define OPFL_TGTPATH_IS_NET 0x00000020       // the target path is a network path
#define OPFL_TGTPATH_IS_FAST 0x00000040      // the target path is a disk, USB disk, flash drive, flash-card reader, CD, DVD, or RAM disk (not a network or floppy)
#define OPFL_IGNORE_INVALID_NAME 0x00000080  // skip the name validity test (for directories: unchanged name = do not flag as invalid)

struct COperation
{
    COperationCode Opcode;
    CQuadWord Size;
    CQuadWord FileSize; // file size, valid only for ocCopyFile and ocMoveFile
    char *SourceName,
        *TargetName;
    DWORD Attr;
    DWORD OpFlags; // combination of OPFL_xxx, see above
};

class COperations : public TDirectArray<COperation>
{
public:
    CQuadWord TotalSize;      // WARNING: not the byte size of the files (usable only for progress calculations)
    CQuadWord CompressedSize; // sum of file sizes after compression
    CQuadWord OccupiedSpace;  // space occupied on disk
    CQuadWord TotalFileSize;  // sum of file sizes on disk
    CQuadWord FreeSpace;      // free space on disk (Copy, Move) for verification
    DWORD BytesPerCluster;    // for calculating occupied space

    // sizes of individual files for estimation with the given cluster size
    TDirectArray<CQuadWord> Sizes;

    DWORD ClearReadonlyMask; // for automatically clearing the read-only flag from CD-ROMs
    BOOL InvertRecycleBin;   // invert Recycle Bin usage

    int FilesCount;
    int DirsCount;

    const char* RemapNameFrom; // for on-screen listings only:
    int RemapNameFromLen;      // name mapping for MoveFiles (From -> To)
    const char* RemapNameTo;
    int RemapNameToLen;

    BOOL RemovableTgtDisk;      // is this writing to removable media?
    BOOL RemovableSrcDisk;      // is this reading from removable media?
    BOOL CanUseRecycleBin;      // can we use the Recycle Bin? (only local fixed drives)
    BOOL SameRootButDiffVolume; // TRUE if this is a Move between paths with the same root but different volumes (at least one path contains a junction point)
    BOOL TargetPathSupADS;      // TRUE if the copy/move target supports ADS (delete the file's ADS (or the whole files) before overwriting)
                                //    BOOL TargetPathSupEFS;       // TRUE if the copy/move target supports EFS (or less generally: it is NTFS rather than FAT)

    // for Copy/Move operations
    BOOL IsCopyOrMoveOperation; // TRUE = this is a Copy/Move operation (add it to the queue of disk Copy/Move operations)
    BOOL OverwriteOlder;        // overwrite older items and skip newer ones without prompting
    BOOL CopySecurity;          // preserve NTFS permissions; FALSE = don't care = perform no extra handling and accept any result
    BOOL CopyAttrs;             // preserve the Archive, Encrypt, and Compress attributes; FALSE = don't care = perform no extra handling and accept any result
    BOOL PreserveDirTime;       // preserve directory timestamps (during Move we detect unintended changes and fix them manually; works e.g. on Samba)
    BOOL StartOnIdle;           // should start only when nothing else is running
    BOOL SourcePathIsNetwork;   // TRUE = the source path is a network path (UNC or mapped drive)

    // for the status line in the progress dialog (Copy and Move only)
    BOOL ShowStatus;       // should the operation status (copy speed, etc.) appear below the second progress bar?
    BOOL IsCopyOperation;  // TRUE = copy, FALSE = move
    BOOL FastMoveUsed;     // is at least one file or directory being "renamed"? (then the total moved data size would be misleading)
    BOOL ChangeSpeedLimit; // TRUE = the speed limit might change (the worker should reach a state where changing it is easy)

    BOOL SkipAllCountSizeErrors; // should all subsequent count-size errors be skipped?

    char WorkPath1[MAX_PATH];  // when non-empty string first path processed (used for change notifications)
    BOOL WorkPath1InclSubDirs; // TRUE/FALSE = with/without subdirectories (first path)
    char WorkPath2[MAX_PATH];  // when non-empty string second path processed (used for change notifications)
    BOOL WorkPath2InclSubDirs; // TRUE/FALSE = with/without subdirectories (second path)

    char* WaitInQueueSubject; // text for the "waiting in queue" state: dialog title
    char* WaitInQueueFrom;    // text for the "waiting in queue" state: top line (From)
    char* WaitInQueueTo;      // text for the "waiting in queue" state: bottom line (To)

private:
    // for the status line in the progress dialog (Copy and Move only)
    CRITICAL_SECTION StatusCS;              // critical section protecting TransferSpeedMeter, ProgressSpeedMeter, and
    CTransferSpeedMeter TransferSpeedMeter; // meter for data transfers (Read/WriteFile)
    CProgressSpeedMeter ProgressSpeedMeter; // meter for calculating "time left" (also tracks directory creation speed, empty copies, etc.; uses the same operation sizes as the progress meter)
    CQuadWord TransferredFileSize;          // bytes already copied/transferred (the final sum should match TotalFileSize unless on-disk data change)
    CQuadWord ProgressSize;                 // progress expressed in copied/transferred "bytes" (uses the same operation sizes as the progress meter)

    // data for the speed limit, used only inside StatusCS
    BOOL UseSpeedLimit;             // TRUE = the speed limit is in use
    DWORD SpeedLimit;               // speed limit value (in bytes per second), WARNING: must never be zero!
    DWORD SleepAfterWrite;          // how many ms to wait after a packet of size LastBufferLimit; -1 = the value must be computed (after the first packet)
    int LastBufferLimit;            // packet size, WARNING: must never be zero!
    DWORD LastSetupTime;            // GetTickCount() captured when we last computed the speed limit parameters + any braking
    CQuadWord BytesTrFromLastSetup; // bytes transferred since LastSetupTime

    // for asynchronous copying only: buffer limiter data (keeps progress updates flowing by preventing oversized buffers); used only inside StatusCS
    BOOL UseProgressBufferLimit;  // TRUE = use the buffer size limiter (asynchronous copying)
    DWORD ProgressBufferLimit;    // copy buffer size limit to keep progress updates reasonably frequent
    DWORD LastProgBufLimTestTime; // GetTickCount() from the last ProgressBufferLimit size evaluation
    DWORD LastFileBlockCount;     // blocks copied since the last file started (WARNING: overflow-protected; values > 1000000 mean "a lot", the exact amount doesn't matter)
    DWORD LastFileStartTime;      // GetTickCount() from when we started copying the last file

public:
    COperations(int base, int delta, char* waitInQueueSubject, char* waitInQueueFrom, char* waitInQueueTo);
    ~COperations() { HANDLES(DeleteCriticalSection(&StatusCS)); }

    void SetWorkPath1(const char* path, BOOL inclSubDirs)
    {
        lstrcpyn(WorkPath1, path, MAX_PATH);
        WorkPath1InclSubDirs = inclSubDirs;
    }

    void SetWorkPath2(const char* path, BOOL inclSubDirs)
    {
        lstrcpyn(WorkPath2, path, MAX_PATH);
        WorkPath2InclSubDirs = inclSubDirs;
    }

    void SetTFS(const CQuadWord& TFS);
    void SetTFSandProgressSize(const CQuadWord& TFS, const CQuadWord& pSize,
                               int* limitBufferSize = NULL, int bufferSize = 0);
    void AddBytesToSpeedMetersAndTFSandPS(DWORD bytesCount, BOOL onlyToProgressSpeedMeter,
                                          int bufferSize, int* limitBufferSize = NULL,
                                          DWORD maxPacketSize = 0);
    void GetNewBufSize(int* limitBufferSize, int bufferSize);
    void AddBytesToTFSandSetProgressSize(const CQuadWord& bytesCount, const CQuadWord& pSize);
    void AddBytesToTFS(const CQuadWord& bytesCount);
    void GetTFS(CQuadWord* TFS);
    void GetTFSandResetTrSpeedIfNeeded(CQuadWord* TFS);
    void SetProgressSize(const CQuadWord& pSize);
    void CalcLimitBufferSize(int* limitBufferSize, int bufferSize);

    void EnableProgressBufferLimit(BOOL useProgressBufferLimit);
    void SetFileStartParams();

    void GetStatus(CQuadWord* transferredFileSize, CQuadWord* transferSpeed,
                   CQuadWord* progressSize, CQuadWord* progressSpeed,
                   BOOL* useSpeedLimit, DWORD* speedLimit);
    void InitSpeedMeters(BOOL operInProgress);
    BOOL GetTFSandProgressSize(CQuadWord* transferredFileSize, CQuadWord* progressSize);

    void SetSpeedLimit(BOOL useSpeedLimit, DWORD speedLimit);
    void GetSpeedLimit(BOOL* useSpeedLimit, DWORD* speedLimit);
};

class COperationsQueue // queue of disk Copy/Move operations
{
protected:
    CRITICAL_SECTION QueueCritSect; // object's critical section

    // OperDlgs and OperPaused arrays have the same number of elements and share indices (each operation uses the same index in both arrays)
    TDirectArray<HWND> OperDlgs;    // array of HWND handles: dialogs of operations in the queue
    TDirectArray<DWORD> OperPaused; // int array describing queue operation state: 2/1/0 = "manually-paused"/"auto-paused"/"running"

public:
    COperationsQueue() : OperDlgs(5, 10), OperPaused(5, 10)
    {
        HANDLES(InitializeCriticalSection(&QueueCritSect));
    }
    ~COperationsQueue()
    {
        if (OperDlgs.Count > 0 || OperPaused.Count > 0)
            TRACE_E("~COperationsQueue(): unexpected situation: operation queue is not empty!");
        HANDLES(DeleteCriticalSection(&QueueCritSect));
    }

    // adds an operation to the queue; returns TRUE on success, otherwise the addition failed (not enough memory);
    // 'dlg' is the handle of the operation dialog window; 'startOnIdle' is TRUE if the operation should start
    // only when nothing else is running; in 'startPaused' (must not be NULL) it returns TRUE when
    // the added operation should start "paused", otherwise it starts "running"
    BOOL AddOperation(HWND dlg, BOOL startOnIdle, BOOL* startPaused);

    // removes the operation from the queue (the operation finished); if 'doNotResume' is FALSE, it posts
    // a "resume" of the first operation in the queue when every operation in the queue is "paused";
    // if 'foregroundWnd' is not NULL, it stores the handle of the dialog that should be activated
    // (if no activation is needed, the value remains unchanged)
    void OperationEnded(HWND dlg, BOOL doNotResume, HWND* foregroundWnd);

    // sets the state of operation 'dlg' to 'paused' (2/1/0 = "manually-paused"/"auto-paused"/"running")
    void SetPaused(HWND dlg, BOOL paused);

    // moves operation 'dlg' to the end of the list + sets its state to "auto-paused"
    void AutoPauseOperation(HWND dlg, HWND* foregroundWnd);

    // returns TRUE if there is no operation in the queue
    BOOL IsEmpty();

    // returns the current number of operations in the queue
    int GetNumOfOperations();
};

extern COperationsQueue OperationsQueue; // queue of disk Copy/Move operations

HANDLE StartWorker(COperations* script, HWND hDlg, CChangeAttrsData* attrsData,
                   CConvertData* convertData, HANDLE wContinue, HANDLE workerNotSuspended,
                   BOOL* cancelWorker, int* operationProgress, int* summaryProgress);

void FreeScript(COperations* script);

//
// File information classes and Io Status block (see NTDDK.H)
//
typedef enum _FILE_INFORMATION_CLASS
{
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation, // 2
    FileBothDirectoryInformation, // 3
    FileBasicInformation,         // 4  wdm
    FileStandardInformation,      // 5  wdm
    FileInternalInformation,      // 6
    FileEaInformation,            // 7
    FileAccessInformation,        // 8
    FileNameInformation,          // 9
    FileRenameInformation,        // 10
    FileLinkInformation,          // 11
    FileNamesInformation,         // 12
    FileDispositionInformation,   // 13
    FilePositionInformation,      // 14 wdm
    FileFullEaInformation,        // 15
    FileModeInformation,          // 16
    FileAlignmentInformation,     // 17
    FileAllInformation,           // 18
    FileAllocationInformation,    // 19
    FileEndOfFileInformation,     // 20 wdm
    FileAlternateNameInformation, // 21
    FileStreamInformation,        // 22
    FilePipeInformation,          // 23
    FilePipeLocalInformation,     // 24
    FilePipeRemoteInformation,    // 25
    FileMailslotQueryInformation, // 26
    FileMailslotSetInformation,   // 27
    FileCompressionInformation,   // 28
    FileObjectIdInformation,      // 29
    FileCompletionInformation,    // 30
    FileMoveClusterInformation,   // 31
    FileQuotaInformation,         // 32
    FileReparsePointInformation,  // 33
    FileNetworkOpenInformation,   // 34
    FileAttributeTagInformation,  // 35
    FileTrackingInformation,      // 36
    FileMaximumInformation
} FILE_INFORMATION_CLASS,
    *PFILE_INFORMATION_CLASS;

typedef struct _IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef NTSTATUS(__stdcall* NTQUERYINFORMATIONFILE)(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass);

extern NTQUERYINFORMATIONFILE DynNtQueryInformationFile;

typedef VOID(__stdcall* PIO_APC_ROUTINE)(
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved);

typedef NTSTATUS(__stdcall* NTFSCONTROLFILE)(
    _In_ HANDLE FileHandle,
    _In_opt_ HANDLE Event,
    _In_opt_ PIO_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcContext,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_ ULONG FsControlCode,
    _In_opt_ PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_opt_ PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength);

extern NTFSCONTROLFILE DynNtFsControlFile;

//
// MessageId: STATUS_BUFFER_OVERFLOW
//
// MessageText:
//
//  {Buffer Overflow}
//  The data was too large to fit into the specified buffer.
//
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005L)

#pragma pack(4)
typedef struct
{
    ULONG NextEntry;
    ULONG NameLength;
    LARGE_INTEGER Size;
    LARGE_INTEGER AllocationSize;
    USHORT Name[1];
} FILE_STREAM_INFORMATION, *PFILE_STREAM_INFORMATION;
#pragma pack()

// enumerates alternate data streams (ADS) of a file/directory ('isDir' is FALSE/TRUE)
// 'fileName'; meaningful only on NTFS disks; if 'adsSize' is not NULL it returns the
// sum of the sizes of all ADS; if 'streamNames' is not NULL it returns an allocated array
// of Unicode names of all ADS (except the default ADS) - the elements of the array are allocated 
// the caller must dealocate them and the array itself; the array of names is returned only
// if no error occurred (see 'lowMemory' and 'winError') and ADS were found (the function 
// returns TRUE); if 'streamNamesCount' is not NULL it returns the number of elements 
// in 'streamNames'; if 'lowMemory' is not NULL it returns TRUE when an out-of-memory 
// error occurs (only possible when 'streamNames' is not NULL); if 'winError' is not NULL 
// it returns the Windows error code (NO_ERROR if none occurred - if a Windows error occurs,
// the function always returns FALSE); the function returns TRUE if the file/directory 
// contains ADS, otherwise FALSE; 'bytesPerCluster' is the cluster size 
// used to compute disk space occupied by the ADS (0 = unknown size);
// in 'adsOccupiedSpace' (if not NULL) it returns the disk space occupied by the ADS;
// in 'onlyDiscardableStreams' (if not NULL) it returns TRUE if only ADS
// that can be discarded without prompting were found (currently only thumbnails from W2K)
BOOL CheckFileOrDirADS(const char* fileName, BOOL isDir, CQuadWord* adsSize, wchar_t*** streamNames,
                       int* streamNamesCount, BOOL* lowMemory, DWORD* winError,
                       DWORD bytesPerCluster, CQuadWord* adsOccupiedSpace,
                       BOOL* onlyDiscardableStreams);
