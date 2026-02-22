// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#pragma pack(push, enter_include_operats_h_dt) // so that all structures are as small as possible (speed is not needed, we mainly save space)
#pragma pack(1)

// returns a pointer to the textual description of the 'error'; if 'error' is NO_ERROR, returns
// a text like "unknown, maybe insufficient system resources", otherwise returns the standard
// Windows description of the error (it uses the buffer 'errBuf'+'errBufSize' to store it)
const char* GetWorkerErrorTxt(int error, char* errBuf, int errBufSize);

//
// ****************************************************************************
// CFTPQueueItem
//
// common ancestor for all items in the "FTP Queue"

enum CFTPQueueItemType
{
    fqitNone, // empty item (must be set to one of the following types)

    fqitDeleteExploreDir,      // explore directory for delete (note: we delete links to directories as a whole, the goal of the operation is fulfilled and nothing "extra" gets deleted) (object of class CFTPQueueItemDelExplore)
    fqitCopyResolveLink,       // download: copy: find out whether it is a link to a file or a directory (object of class CFTPQueueItemCopyOrMove)
    fqitMoveResolveLink,       // download: move: find out whether it is a link to a file or a directory (object of class CFTPQueueItemCopyOrMove)
    fqitCopyExploreDir,        // download: explore a directory or a link to a directory for copying (object of class CFTPQueueItemCopyMoveExplore)
    fqitMoveExploreDir,        // download: explore a directory for moving (after finishing it deletes the directory) (object of class CFTPQueueItemCopyMoveExplore)
    fqitMoveExploreDirLink,    // download: explore a link to a directory for moving (after finishing it deletes the link to the directory) (object of class CFTPQueueItemCopyMoveExplore)
    fqitChAttrsExploreDir,     // explore a directory for attribute change (also adds an item for changing the directory attributes) (object of class CFTPQueueItemChAttrExplore)
    fqitChAttrsResolveLink,    // change attributes: find out whether it is a link to a directory (object of class CFTPQueueItem)
    fqitChAttrsExploreDirLink, // explore a link to a directory for attribute change (object of class CFTPQueueItem)
    fqitUploadCopyExploreDir,  // upload: explore a directory for copying (object of class CFTPQueueItemCopyMoveUploadExplore)
    fqitUploadMoveExploreDir,  // upload: explore a directory for moving (after finishing it deletes the directory) (object of class CFTPQueueItemCopyMoveUploadExplore)

    fqitLastResolveOrExploreItem, // numeric constant only for distinguishing types (processing priority) of items

    fqitDeleteLink,          // delete for a link (object of class CFTPQueueItemDel)
    fqitDeleteFile,          // delete for a file (object of class CFTPQueueItemDel)
    fqitDeleteDir,           // delete for a directory (object of class CFTPQueueItemDir)
    fqitCopyFileOrFileLink,  // download: copying a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
    fqitMoveFileOrFileLink,  // download: moving a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
    fqitMoveDeleteDir,       // download: delete a directory after moving its contents (object of class CFTPQueueItemDir)
    fqitMoveDeleteDirLink,   // download: delete a link to a directory after moving its contents (object of class CFTPQueueItemDir)
    fqitChAttrsFile,         // change file attributes (note: attributes cannot be changed on links) (object of class CFTPQueueItemChAttr)
    fqitChAttrsDir,          // change directory attributes (object of class CFTPQueueItemChAttrDir)
    fqitUploadCopyFile,      // upload: copy a file (object of class CFTPQueueItemCopyOrMoveUpload)
    fqitUploadMoveFile,      // upload: move a file (object of class CFTPQueueItemCopyOrMoveUpload)
    fqitUploadMoveDeleteDir, // upload: delete a directory after moving its contents (object of class CFTPQueueItemDir)
};

enum CFTPQueueItemState
{
    sqisNone,       // empty state (must be set to one of the following states)
    sqisDone,       // the item was completed
    sqisWaiting,    // waiting to be processed
    sqisProcessing, // the item is being processed
    sqisDelayed,    // item processing is postponed (waits until the "child" items are processed - e.g. deleting a directory only after removing all contained files and directories)

    // the remaining states are used for various expressions of an item error, WARNING: they must be at the end of the enum !!! (reason: conditions are used such as (item->GetItemState() >= sqisSkipped /* sqisSkipped, sqisFailed, sqisUserInputNeeded or sqisForcedToFail */))
    sqisSkipped,         // the item was skipped
    sqisFailed,          // the item did not finish due to an error (failed)
    sqisUserInputNeeded, // completing the item requires user intervention (we have a question for the user)
    sqisForcedToFail,    // item that entered the error state due to errors/skips of child items
};

// list of problems that may occur for items in the queue; for a new constant it is necessary to:
// - add handling to CFTPQueue::SolveErrorOnItem (operats1.cpp) or
//   if it cannot be resolved, add it to the condition in CFTPQueueItem::HasErrorToSolve (operats2.cpp)
// - add a textual description to CFTPQueueItem::GetProblemDescr (operats2.cpp)
#define ITEMPR_OK 0                          // no problem occurred
#define ITEMPR_LOWMEM 1                      // insufficient system resources (for example, memory)
#define ITEMPR_CANNOTCREATETGTFILE 2         // issue "the target file cannot be created or opened" (uses WinError)
#define ITEMPR_CANNOTCREATETGTDIR 3          // download: issue "the target directory cannot be created" (uses WinError)
#define ITEMPR_TGTFILEALREADYEXISTS 4        // issue "the target file already exists"
#define ITEMPR_TGTDIRALREADYEXISTS 5         // issue "the target directory already exists"
#define ITEMPR_RETRYONCREATFILE 6            // issue "retry on a file created or overwritten directly by the FTP client"
#define ITEMPR_RETRYONRESUMFILE 7            // issue "retry on a file resumed by the FTP client"
#define ITEMPR_ASCIITRFORBINFILE 8           // issue "ASCII transfer mode for a binary file"
#define ITEMPR_UNKNOWNATTRS 9                // issue "the file/directory has unknown attributes we cannot preserve (other permissions than 'r'+'w'+'x')"
#define ITEMPR_INVALIDPATHTODIR 10           // issue "the path to the directory is too long or syntactically incorrect" (occurs during explore-dir - both source and target directory) (unsolvable issue)
#define ITEMPR_UNABLETOCWD 11                // issue "error while changing the working directory on the server, server response: %s" - change of the path to Path+Name or TgtPath+TgtName (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UNABLETOPWD 12                // issue "error while querying the working directory on the server, server response: %s" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_DIREXPLENDLESSLOOP 13         // issue "exploring this directory would result in an endless loop" (occurs during explore-dir) (unsolvable issue)
#define ITEMPR_LISTENFAILURE 14              // issue "error while preparing to open an active data connection: %s" (uses WinError)
#define ITEMPR_INCOMPLETELISTING 15          // issue "unable to read the full list of files and directories from the server: %s" (uses WinError and ErrAllocDescr)
#define ITEMPR_UNABLETOPARSELISTING 16       // issue "unknown format of the file and directory listing from the server"
#define ITEMPR_DIRISHIDDEN 17                // issue "the directory is hidden"
#define ITEMPR_DIRISNOTEMPTY 18              // issue "the directory is not empty"
#define ITEMPR_FILEISHIDDEN 19               // issue "the file is hidden"
#define ITEMPR_INVALIDPATHTOLINK 20          // issue "the full name of the link is too long or syntactically incorrect" (occurs during resolve-link) (unsolvable issue)
#define ITEMPR_UNABLETORESOLVELNK 21         // issue "unable to determine whether it is a link to a directory or a file, server response: %s" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UNABLETODELETEFILE 22         // issue "unable to delete the file, server response: %s" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UNABLETODELETEDIR 23          // issue "unable to delete the directory, server response: %s" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UNABLETOCHATTRS 24            // issue "unable to change file/directory attributes, server response: %s" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UNABLETORESUME 25             // issue "unable to resume file transfer"
#define ITEMPR_RESUMETESTFAILED 26           // issue "unable to resume file transfer, unexpected tail of file (file has changed)"
#define ITEMPR_TGTFILEREADERROR 27           // issue "error reading the target file" (uses WinError)
#define ITEMPR_TGTFILEWRITEERROR 28          // issue "error writing the target file" (uses WinError)
#define ITEMPR_INCOMPLETEDOWNLOAD 29         // issue "unable to retrieve file from server: %s" (uses WinError and ErrAllocDescr)
#define ITEMPR_UNABLETODELSRCFILE 30         // issue "Move: unable to delete the source file, server response: %s" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UPLOADCANNOTCREATETGTDIR 31   // upload: issue "the target directory cannot be created" (if FTPMayBeValidNameComponent() returns FALSE, ErrAllocDescr==NULL and WinError==NO_ERROR; if a file/link is in the way, WinError==ERROR_ALREADY_EXISTS; otherwise it uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UPLOADCANNOTLISTTGTPATH 32    // upload: issue "unable to list the target path" (unable to detect possible name collisions) (uses WinError and ErrAllocDescr)
#define ITEMPR_UPLOADTGTDIRALREADYEXISTS 33  // upload: issue "the target directory or a link to the directory already exists"
#define ITEMPR_UPLOADCRDIRAUTORENFAILED 34   // upload: issue "unable to create the target directory under any name" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UPLOADCANNOTLISTSRCPATH 35    // upload: issue "unable to list the source path" (uses WinError)
#define ITEMPR_UNABLETOCWDONLYPATH 36        // issue "error while changing the working directory on the server, server response: %s" - change of the path to Path or TgtPath (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UNABLETODELETEDISKDIR 37      // issue "unable to delete directory on disk, error:" (uses WinError)
#define ITEMPR_UPLOADCANNOTCREATETGTFILE 38  // upload: issue "the target file cannot be created or opened" (if FTPMayBeValidNameComponent() returns FALSE, ErrAllocDescr==NULL and WinError==NO_ERROR; if a directory/link is in the way, WinError==ERROR_ALREADY_EXISTS; otherwise it uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_UPLOADCANNOTOPENSRCFILE 39    // issue "the source file cannot be opened" (uses WinError)
#define ITEMPR_UPLOADTGTFILEALREADYEXISTS 40 // upload: issue "the target file or a link to the file already exists"
#define ITEMPR_SRCFILEINUSE 41               // issue "the source file or link is locked by another operation"
#define ITEMPR_TGTFILEINUSE 42               // issue "the target file or link is locked by another operation"
#define ITEMPR_SRCFILEREADERROR 43           // issue "error reading the source file" (uses WinError)
#define ITEMPR_INCOMPLETEUPLOAD 44           // issue "unable to store file to server: %s" (uses WinError and ErrAllocDescr)
#define ITEMPR_UNABLETODELETEDISKFILE 45     // issue "unable to delete file on disk, error:" (uses WinError)
#define ITEMPR_UPLOADASCIIRESUMENOTSUP 46    // issue "resume in ASCII transfer mode is not supported (try resume in binary mode)"
#define ITEMPR_UPLOADUNABLETORESUMEUNKSIZ 47 // issue "unable to resume the file because the target file size is unknown"
#define ITEMPR_UPLOADUNABLETORESUMEBIGTGT 48 // issue "unable to resume the file because the target file is larger than the source file"
#define ITEMPR_UPLOADFILEAUTORENFAILED 49    // upload: issue "unable to create the target file under any name" (uses ErrAllocDescr to store the server response - it may span multiple lines)
#define ITEMPR_SKIPPEDBYUSER 50              // after pressing the Skip button on a waiting item in the operation dialog
#define ITEMPR_UPLOADTESTIFFINISHEDNOTSUP 51 // issue "unable to verify whether the file uploaded successfully" (we sent the entire file + the server "just" did not respond, most likely the file is OK, but we are unable to test it - reasons: ASCII transfer mode or we do not have the size in bytes (neither listing nor the SIZE command))

enum CFTPQueueItemAction
{
    fqiaNone,                     // no forced action (standard behavior)
    fqiaUseAutorename,            // autorename (use alternate name) should be used
    fqiaUseExistingDir,           // an existing directory should be used
    fqiaResume,                   // an existing file should be resumed
    fqiaResumeOrOverwrite,        // an existing file should be resumed or overwritten
    fqiaOverwrite,                // an existing file should be overwritten
    fqiaReduceFileSizeAndResume,  // the existing file should be truncated and then resumed
    fqiaUploadForceAutorename,    // upload: autorename (use alternate name) should be used no matter what
    fqiaUploadContinueAutorename, // upload: continuation of autorename (use alternate name)
    fqiaUploadTestIfFinished,     // upload: we sent the entire file + the server "just" did not respond, most likely the file is OK, we will test it
};

class CFTPQueueItemAncestor
{
private:
    CFTPQueueItemState State; // item state (private is used to ensure updating of parent counters when changing the state)

public:
    CFTPQueueItemAncestor() { State = sqisNone; }

    // returns State; call only from the queue critical section or before inserting into the queue!!!
    CFTPQueueItemState GetItemState() const { return State; }

    // internal State setup: use only before adding the item to the queue
    void SetStateInternal(CFTPQueueItemState state) { State = state; }

    // changes 'State' to 'state' and optionally adjusts parent item counters
    // CAUTION: can only be called from the QueueCritSect critical section!!!
    void ChangeStateAndCounters(CFTPQueueItemState state, CFTPOperation* oper,
                                CFTPQueue* queue);
};

class CFTPQueueItem : public CFTPQueueItemAncestor
{
public:
    // access to the object's data:
    //   - if it is not yet part of the queue (construction): without restrictions
    //   - if it is already in the queue: access only from the queue critical section

    static CRITICAL_SECTION NextItemUIDCritSect; // critical section for accessing NextItemUID
    static int NextItemUID;                      // global counter for item UIDs (access only inside the NextItemUIDCritSect section!)

    int UID;       // unique item number
    int ParentUID; // UID of the parent item (-1 = the parent is the operation)

    CFTPQueueItemType Type; // item type (for casting to the correct object type)
    DWORD ProblemID;        // complements State: see the ITEMPR_XXX constants
    DWORD WinError;         // may contain the code of the occurred Windows error (complements ProblemID)
    char* ErrAllocDescr;    // may contain an allocated text describing the error (complements ProblemID)

    DWORD ErrorOccurenceTime; // "time" when the error occurred (used to keep the order of error resolution according to their occurrence); -1 = no error occurred

    CFTPQueueItemAction ForceAction; // action forced by the user (for example an autorename entered from the Solve Error dialog)

    char* Path; // path to the processed file/directory (local path on the server or a Windows path)
    char* Name; // name of the processed file/directory (name without the path)

public:
    CFTPQueueItem();
    virtual ~CFTPQueueItem();

    // returns TRUE if this is an "explore" or "resolve" item
    // CAUTION: call only from the queue critical section
    BOOL IsExploreOrResolveItem() const { return Type < fqitLastResolveOrExploreItem; }

    // returns TRUE if the item is in state sqisFailed or sqisUserInputNeeded;
    // used instead of HasErrorToSolve() when storing ErrorOccurenceTime, because
    // at the point of use ProblemID is not set yet and therefore HasErrorToSolve()
    // cannot be used (in this way even unsolvable errors are marked for processing, which does not matter,
    // because they are skipped in SearchItemWithNewError)
    // CAUTION: call only from the queue critical section
    BOOL IsItemInSimpleErrorState() const { return GetItemState() == sqisFailed || GetItemState() == sqisUserInputNeeded; }

    // sets the basic item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItem(int parentUID, CFTPQueueItemType type, CFTPQueueItemState state,
                 DWORD problemID, const char* path, const char* name);

    // enabler for buttons in the operation dialog: returns TRUE if the "Solve Error" button makes sense;
    // in 'canSkip' (if not NULL) it returns TRUE if the "Skip" button makes sense;
    // in 'canRetry' (if not NULL) it returns TRUE if the "Retry" button makes sense
    // CAUTION: call only from the queue critical section
    BOOL HasErrorToSolve(BOOL* canSkip, BOOL* canRetry);

    // returns a textual description of the problem expressed by ProblemID + WinError + ErrAllocDescr
    // CAUTION: call only from the queue critical section
    void GetProblemDescr(char* buf, int bufSize);
};

//
// ****************************************************************************
// CFTPQueueItemDir
//

class CFTPQueueItemDir : public CFTPQueueItem
{
public:
    int ChildItemsNotDone;  // number of unfinished "child" items (except for type sqisDone)
    int ChildItemsSkipped;  // number of skipped "child" items (type sqisSkipped)
    int ChildItemsFailed;   // number of failed "child" items (type sqisFailed and sqisForcedToFail)
    int ChildItemsUINeeded; // number of user-input-needed "child" items (type sqisUserInputNeeded)

public:
    CFTPQueueItemDir();

    // sets the additional item parameters; returns TRUE on success
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    BOOL SetItemDir(int childItemsNotDone, int childItemsSkipped, int childItemsFailed,
                    int childItemsUINeeded);

    // sets the specified counts to the respective counters and optionally changes the item state
    // (only from sqisWaiting to sqisDelayed or sqisForcedToFail)
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue)
    void SetStateAndNotDoneSkippedFailed(int childItemsNotDone, int childItemsSkipped,
                                         int childItemsFailed, int childItemsUINeeded);

    // returns the item state determined by the counters (sqisWaiting, sqisDelayed or sqisForcedToFail)
    CFTPQueueItemState GetStateFromCounters();
};

//
// ****************************************************************************
// CFTPQueueItemDel
//

class CFTPQueueItemDel : public CFTPQueueItem
{
public:
    unsigned IsHiddenFile : 1; // TRUE/FALSE = is/is not a hidden file or link (possible delete confirmation, see CFTPOperation::ConfirmDelOnHiddenFile); CAUTION: after the user confirms the prompt it is set to FALSE

public:
    CFTPQueueItemDel();

    // sets the additional item parameters; returns TRUE on success
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    BOOL SetItemDel(int isHiddenFile);
};

//
// ****************************************************************************
// CFTPQueueItemDelExplore
//

class CFTPQueueItemDelExplore : public CFTPQueueItem
{
public:
    unsigned IsTopLevelDir : 1; // TRUE/FALSE = is/is not a directory selected directly in the panel - the "deleting non-empty directories" problem applies only to these directories, see CFTPOperation::ConfirmDelOnNonEmptyDir; CAUTION: after the user confirms the prompt it is set to FALSE
    unsigned IsHiddenDir : 1;   // TRUE/FALSE = is/is not a hidden directory (possible delete confirmation, see CFTPOperation::ConfirmDelOnHiddenDir); CAUTION: after the user confirms the prompt it is set to FALSE

public:
    CFTPQueueItemDelExplore();

    // sets the additional item parameters; returns TRUE on success
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    BOOL SetItemDelExplore(int isTopLevelDir, int isHiddenDir);
};

//
// ****************************************************************************
// CFTPQueueItemCopyOrMove
//

// target file state (constants for CFTPQueueItemCopyOrMove::TgtFileState)
#define TGTFILESTATE_UNKNOWN 0     // the file state has not been determined yet
#define TGTFILESTATE_TRANSFERRED 1 // the file was transferred successfully (useful if deleting the source file failed during move)
#define TGTFILESTATE_CREATED 2     // the file was created directly by the FTP client or resumed with the option to overwrite
#define TGTFILESTATE_RESUMED 3     // the file was resumed by the FTP client without the option to overwrite (overwriting can happen only if the already downloaded part of the file is too small, see Config.ResumeMinFileSize)

class CFTPQueueItemCopyOrMove : public CFTPQueueItem
{
public:
    char* TgtPath; // path to the target file (Windows path)
    char* TgtName; // name of the target file (name without the path)

    CQuadWord Size; // file size (CQuadWord(-1, -1) = size is unknown - e.g. links)

    CFTPDate Date; // date of the source file (after finishing the operation it gets set on the target file); if Date.Day==0, it is an "empty value" (the date should not be changed)
    CFTPTime Time; // time of the source file (after finishing the operation it gets set on the target file); if Time.Hour==24, it is an "empty value" (the time should not be changed)

    unsigned AsciiTransferMode : 1;           // TRUE/FALSE = ASCII/Binary transfer mode
    unsigned IgnoreAsciiTrModeForBinFile : 1; // TRUE/FALSE = ignore/check whether a binary file is being transferred in ASCII mode
    unsigned SizeInBytes : 1;                 // TRUE/FALSE = Size is in bytes/blocks
    unsigned TgtFileState : 2;                // see the TGTFILESTATE_XXX constants
    unsigned DateAndTimeValid : 1;            // TRUE/FALSE = Date+Time are valid and should be set on the target file after finishing the operation

public:
    CFTPQueueItemCopyOrMove();
    virtual ~CFTPQueueItemCopyOrMove();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItemCopyOrMove(const char* tgtPath, const char* tgtName, const CQuadWord& size,
                           int asciiTransferMode, int sizeInBytes, int tgtFileState,
                           BOOL dateAndTimeValid, const CFTPDate& date, const CFTPTime& time);
};

//
// ****************************************************************************
// CFTPQueueItemCopyOrMoveUpload
//

// target file state (constants for CFTPQueueItemCopyOrMoveUpload::TgtFileState)
#define UPLOADTGTFILESTATE_UNKNOWN 0     // the file state has not been determined yet
#define UPLOADTGTFILESTATE_TRANSFERRED 1 // the file was transferred successfully (useful if deleting the source file failed during move)
#define UPLOADTGTFILESTATE_CREATED 2     // the file was created directly by the FTP client or resumed with the option to overwrite
#define UPLOADTGTFILESTATE_RESUMED 3     // the file was resumed by the FTP client without the option to overwrite (overwriting can happen only if the already uploaded part of the file is too small, see Config.ResumeMinFileSize)

class CFTPQueueItemCopyOrMoveUpload : public CFTPQueueItem
{
public:
    char* TgtPath; // path to the target file (local path on the server)
    char* TgtName; // name of the target file (name without the path)

    CQuadWord Size;      // file size
    int AutorenamePhase; // when auto-rename is used this is the phase of generating names
    char* RenamedName;   // name created by auto-rename: not NULL only when an upload to the new name is in progress

    CQuadWord SizeWithCRLF_EOLs; // file size when using CRLF line endings (set only for fqiaUploadTestIfFinished)
    CQuadWord NumberOfEOLs;      // number of line endings (set only for fqiaUploadTestIfFinished)

    unsigned AsciiTransferMode : 1;           // TRUE/FALSE = ASCII/Binary transfer mode
    unsigned IgnoreAsciiTrModeForBinFile : 1; // TRUE/FALSE = ignore/check whether a binary file is being transferred in ASCII mode
    unsigned TgtFileState : 2;                // see the UPLOADTGTFILESTATE_XXX constants

public:
    CFTPQueueItemCopyOrMoveUpload();
    virtual ~CFTPQueueItemCopyOrMoveUpload();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItemCopyOrMoveUpload(const char* tgtPath, const char* tgtName, const CQuadWord& size,
                                 int asciiTransferMode, int tgtFileState);
};

//
// ****************************************************************************
// CFTPQueueItemCopyMoveExplore
//

// target directory state (constants for CFTPQueueItemCopyMoveExplore::TgtDirState)
#define TGTDIRSTATE_UNKNOWN 0 // the directory state has not been determined yet
#define TGTDIRSTATE_READY 1   // the target directory has already been created directly by the FTP client or the use of an existing directory has already been approved

class CFTPQueueItemCopyMoveExplore : public CFTPQueueItem
{
public:
    char* TgtPath; // path to the target directory (Windows path)
    char* TgtName; // name of the target directory (name without the path)

    unsigned TgtDirState : 1; // see the TGTDIRSTATE_XXX constants

public:
    CFTPQueueItemCopyMoveExplore();
    virtual ~CFTPQueueItemCopyMoveExplore();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItemCopyMoveExplore(const char* tgtPath, const char* tgtName, int tgtDirState);
};

//
// ****************************************************************************
// CFTPQueueItemCopyMoveUploadExplore
//

// target directory state (constants for CFTPQueueItemCopyMoveUploadExplore::TgtDirState)
#define UPLOADTGTDIRSTATE_UNKNOWN 0 // the directory state has not been determined yet
#define UPLOADTGTDIRSTATE_READY 1   // the target directory has already been created directly by the FTP client or the use of an existing directory has already been approved

class CFTPQueueItemCopyMoveUploadExplore : public CFTPQueueItem
{
public:
    char* TgtPath; // path to the target directory (local path on the server)
    char* TgtName; // name of the target directory (name without the path)

    unsigned TgtDirState : 1; // see the UPLOADTGTDIRSTATE_XXX constants

public:
    CFTPQueueItemCopyMoveUploadExplore();
    virtual ~CFTPQueueItemCopyMoveUploadExplore();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItemCopyMoveUploadExplore(const char* tgtPath, const char* tgtName, int tgtDirState);
};

//
// ****************************************************************************
// CFTPQueueItemChAttr
//

class CFTPQueueItemChAttr : public CFTPQueueItem
{
public:
    WORD Attr;        // requested mode (conversion to a string in octal digits is necessary)
    BYTE AttrErr;     // TRUE = an unknown attribute should be preserved, which we cannot do (FALSE = everything OK)
    char* OrigRights; // original file permissions (!=NULL only if they contain unknown permissions + if a permissions column was found at all)

public:
    CFTPQueueItemChAttr();
    virtual ~CFTPQueueItemChAttr();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItemChAttr(WORD attr, const char* origRights, BYTE attrErr);
};

//
// ****************************************************************************
// CFTPQueueItemChAttrDir
//
// not identical to CFTPQueueItemChAttr, it has a counter (inherited from CFTPQueueItemDir)

class CFTPQueueItemChAttrDir : public CFTPQueueItemDir
{
public:
    WORD Attr;        // requested mode (conversion to a string in octal digits is necessary)
    BYTE AttrErr;     // TRUE = an unknown attribute should be preserved, which we cannot do (FALSE = everything OK)
    char* OrigRights; // original directory permissions (!=NULL only if they contain unknown permissions + if a permissions column was found at all)

public:
    CFTPQueueItemChAttrDir();
    virtual ~CFTPQueueItemChAttrDir();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    // CAUTION: inherited from CFTPQueueItemDir - CALL CFTPQueueItemDir::SetItemDir() AS WELL!!!
    void SetItemChAttrDir(WORD attr, const char* origRights, BYTE attrErr);
};

//
// ****************************************************************************
// CFTPQueueItemChAttrExplore
//

class CFTPQueueItemChAttrExplore : public CFTPQueueItem
{
public:
    char* OrigRights; // original directory permissions for computing Attr of the explored directory (!=NULL if a permissions column was found)

public:
    CFTPQueueItemChAttrExplore();
    virtual ~CFTPQueueItemChAttrExplore();

    // sets the additional item parameters
    // CAUTION: does not use a critical section for data access (can only be called before
    //          adding the item to the queue) + it must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetItemChAttrExplore(const char* origRights);
};

//
// ****************************************************************************
// CFTPQueue
//

class CFTPQueue
{
protected:
    // critical section for accessing the object's data
    // CAUTION: do not enter any other critical sections from this section!!!
    CRITICAL_SECTION QueueCritSect;

    TIndirectArray<CFTPQueueItem> Items; // queue items

    // single-entry cache for speeding up FindItemWithUID() - beware: it may not be valid:
    int LastFoundUID;   // UID of the last found item
    int LastFoundIndex; // index of the last found item (always a non-negative number, even after initialization)

    int FirstWaitingItemIndex;          // first item in the queue (array Items) that can (but does not have to) be in state sqisWaiting and if GetOnlyExploreAndResolveItems==TRUE it can also be an "explore" or "resolve" item (there simply are none before this index)
    BOOL GetOnlyExploreAndResolveItems; // TRUE = for now we return only "explore" and "resolve" items from the queue (FALSE only when no such item is in the "waiting" state) (Type < fqitLastResolveOrExploreItem)

    int ExploreAndResolveItemsCount;            // number of queue items of type "explore" and "resolve" (Type < fqitLastResolveOrExploreItem)
    int DoneOrSkippedItemsCount;                // number of queue items in state sqisDone or sqisSkipped
    int WaitingOrProcessingOrDelayedItemsCount; // number of queue items in state sqisWaiting, sqisProcessing or sqisDelayed

    CQuadWord DoneOrSkippedByteSize;                  // sum of known file sizes in bytes of items of type fqitCopyFileOrFileLink and fqitMoveFileOrFileLink in state sqisDone or sqisSkipped
    CQuadWord DoneOrSkippedBlockSize;                 // sum of known file sizes in blocks of items of type fqitCopyFileOrFileLink and fqitMoveFileOrFileLink in state sqisDone or sqisSkipped
    CQuadWord WaitingOrProcessingOrDelayedByteSize;   // sum of known file sizes in bytes of items of type fqitCopyFileOrFileLink and fqitMoveFileOrFileLink in state sqisWaiting, sqisProcessing or sqisDelayed
    CQuadWord WaitingOrProcessingOrDelayedBlockSize;  // sum of known file sizes in blocks of items of type fqitCopyFileOrFileLink and fqitMoveFileOrFileLink in state sqisWaiting, sqisProcessing or sqisDelayed
    CQuadWord DoneOrSkippedUploadSize;                // sum of file sizes of items of type fqitUploadCopyFile and fqitUploadMoveFile in state sqisDone or sqisSkipped
    CQuadWord WaitingOrProcessingOrDelayedUploadSize; // sum of file sizes of items of type fqitUploadCopyFile and fqitUploadMoveFile in state sqisWaiting, sqisProcessing or sqisDelayed
    int CopyUnknownSizeCount;                         // number of items whose size is unknown during a Copy/Move operation (explore/delete-dir/resolve/copy-unknown-size-items)
    int CopyUnknownSizeCountIfUnknownBlockSize;       // number of items whose size is unknown if the block size in bytes is unknown during a Copy/Move operation (explore/delete-dir/resolve/copy-unknown-size-items)

    DWORD LastErrorOccurenceTime;      // "time" when the last error occurred (initialized to -1)
    DWORD LastFoundErrorOccurenceTime; // "time" of the last found item with an error or the "time" before which no item with an error exists anymore

public:
    CFTPQueue();
    ~CFTPQueue();

    // adds a new item to the queue; returns TRUE on success
    BOOL AddItem(CFTPQueueItem* newItem);

    // returns the number of items in the queue
    int GetCount();

    // helper method: adjusts the counters ExploreAndResolveItemsCount, DoneOrSkippedItemsCount
    // and WaitingOrProcessingOrDelayedItemsCount according to the item 'item'; 'add' is
    // TRUE/FALSE when the item 'item' is being added to/removed from the queue
    // called when adding an item to the queue and when changing the item's State
    // CAUTION: can only be called from the QueueCritSect critical section!!!
    void UpdateCounters(CFTPQueueItem* item, BOOL add);

    // methods for locking/unlocking the queue to call multiple operations "at once"
    // (this is just entering/leaving the QueueCritSect critical section)
    void LockForMoreOperations();
    void UnlockForMoreOperations();

    // replaces the item with UID 'itemUID' with the list of items 'items' (the number of items is 'itemsCount');
    // returns TRUE on success (in this case the item with UID 'itemUID' has already been released)
    // CAUTION: it is necessary to use LockForMoreOperations() and UnlockForMoreOperations(), because after calling
    //          ReplaceItemWithListOfItems the counters must be adjusted before other threads access them
    //          (to ensure data consistency)
    BOOL ReplaceItemWithListOfItems(int itemUID, CFTPQueueItem** items, int itemsCount);

    // adds the given counts to the counters ChildItemsNotDone+ChildItemsSkipped+ChildItemsFailed+ChildItemsUINeeded
    // of the dir-item 'itemDirUID' (the item must be a descendant of CFTPQueueItemDir); it can also
    // change the state of the dir-item (to sqisDelayed (child items are being processed), sqisWaiting (children
    // are done, the dir-item can be processed) or sqisForcedToFail (the children cannot be completed,
    // errors/skips occurred)), in that case it ensures updating the counters of the parent of the dir-item
    // (works recursively, so it will adjust the counters of the parent of the parent of the dir-item, etc.)
    void AddToNotDoneSkippedFailed(int itemDirUID, int notDone, int skipped, int failed,
                                   int uiNeeded, CFTPOperation* oper);

    // returns the number of items in state sqisUserInputNeeded, sqisSkipped, sqisFailed or sqisForcedToFail
    // in the queue; if 'onlyUINeeded' is TRUE, fills the array 'UINeededArr' with items in state
    // sqisUserInputNeeded, sqisSkipped, sqisFailed or sqisForcedToFail; if 'focusedItemUID' is not -1,
    // it returns in 'indexInAll' the index of the item with UID 'focusedItemUID' in the queue +
    // returns in 'indexInUIN' the index of the item with UID 'focusedItemUID' in the array of items in state
    // sqisUserInputNeeded, sqisSkipped, sqisFailed or sqisForcedToFail; if it does not find
    // the item with UID 'focusedItemUID', it returns both 'indexInAll' and 'indexInUIN' equal to -1
    int GetUserInputNeededCount(BOOL onlyUINeeded, TDirectArray<DWORD>* UINeededArr,
                                int focusedItemUID, int* indexInAll, int* indexInUIN);

    // returns the UID of the item at index 'index', returns -1 for an invalid index
    int GetItemUID(int index);

    // returns the index of the item with UID 'itemUID', returns -1 for an unknown UID
    int GetItemIndex(int itemUID);

    // returns data for displaying the operation item with index 'index' in the list view in the operation dialog;
    // 'buf'+'bufSize' is a buffer for the text returned in 'lvdi' (it changes three cyclically
    // to satisfy the requirements of LVN_GETDISPINFO); if the index is not
    // valid, it does nothing (the list view refresh is already on the way)
    void GetListViewDataFor(int index, NMLVDISPINFO* lvdi, char* buf, int bufSize);

    // button enabler in the operation dialog for the item at index 'index':
    // returns TRUE if "Solve Error" makes sense; returns FALSE for an invalid index;
    // in 'canSkip' (if not NULL) it returns TRUE if "Skip" makes sense;
    // in 'canRetry' (if not NULL) it returns TRUE if "Retry" makes sense
    BOOL IsItemWithErrorToSolve(int index, BOOL* canSkip, BOOL* canRetry);

    // performs Skip on the item with UID 'UID' (change to state "skipped"); returns the index
    // of the changed item or -1 on error
    int SkipItem(int UID, CFTPOperation* oper);

    // performs Retry on the item with UID 'UID' (change to state "waiting"); returns the index
    // of the changed item or -1 on error
    int RetryItem(int UID, CFTPOperation* oper);

    // performs Solve Error for the item with UID 'UID'; returns the index of the changed item or
    // -1 when multiple items changed or -2 when no change happened; 'oper' is the operation
    // to which the item (and the entire queue) belongs
    int SolveErrorOnItem(HWND parent, int UID, CFTPOperation* oper);

    // searches the queue for the first item in state sqisWaiting, returns a pointer to it (or NULL
    // if such item does not exist); switches the found item into state sqisProcessing
    // (so that it cannot be found again immediately)
    CFTPQueueItem* GetNextWaitingItem(CFTPOperation* oper);

    // returns the item 'item' to state sqisWaiting (the worker cannot process it, this
    // allows other workers to process the item)
    void ReturnToWaitingItems(CFTPQueueItem* item, CFTPOperation* oper);

    // sets the state of the item 'item'; 'errAllocDescr' is NULL or an allocated error description
    // (the caller passes a buffer allocated by malloc, after calling this method
    // the buffer is no longer valid for the caller)
    void UpdateItemState(CFTPQueueItem* item, CFTPQueueItemState state, DWORD problemID,
                         DWORD winError, char* errAllocDescr, CFTPOperation* oper);

    // assigns 'renamedName' to the RenamedName of item 'item' (releases the previous value of RenamedName)
    void UpdateRenamedName(CFTPQueueItemCopyOrMoveUpload* item, char* renamedName);

    // assigns 'autorenamePhase' to AutorenamePhase of item 'item'
    void UpdateAutorenamePhase(CFTPQueueItemCopyOrMoveUpload* item, int autorenamePhase);

    // assigns RenamedName to TgtName of item 'item' (releases the previous value of TgtName)
    void ChangeTgtNameToRenamedName(CFTPQueueItemCopyOrMoveUpload* item);

    // assigns 'tgtName' to TgtName of item 'item' (releases the previous value of TgtName)
    void UpdateTgtName(CFTPQueueItemCopyMoveExplore* item, char* tgtName);

    // assigns 'tgtName' to TgtName of item 'item' (releases the previous value of TgtName)
    void UpdateTgtName(CFTPQueueItemCopyMoveUploadExplore* item, char* tgtName);

    // assigns 'tgtName' to TgtName of item 'item' (releases the previous value of TgtName)
    void UpdateTgtName(CFTPQueueItemCopyOrMove* item, char* tgtName);

    // assigns 'tgtDirState' to TgtDirState of item 'item'
    void UpdateTgtDirState(CFTPQueueItemCopyMoveExplore* item, unsigned tgtDirState);

    // assigns 'tgtDirState' to TgtDirState of item 'item'
    void UpdateUploadTgtDirState(CFTPQueueItemCopyMoveUploadExplore* item, unsigned tgtDirState);

    // assigns 'forceAction' to ForceAction of item 'item'
    void UpdateForceAction(CFTPQueueItem* item, CFTPQueueItemAction forceAction);

    // assigns 'tgtFileState' to TgtFileState of item 'item'
    void UpdateTgtFileState(CFTPQueueItemCopyOrMove* item, unsigned tgtFileState);

    // assigns 'attrErr' to AttrErr of item 'item'
    void UpdateAttrErr(CFTPQueueItemChAttrDir* item, BYTE attrErr);

    // assigns 'attrErr' to AttrErr of item 'item'
    void UpdateAttrErr(CFTPQueueItemChAttr* item, BYTE attrErr);

    // assigns 'isHiddenDir' to IsHiddenDir of item 'item'
    void UpdateIsHiddenDir(CFTPQueueItemDelExplore* item, BOOL isHiddenDir);

    // assigns 'isHiddenFile' to IsHiddenFile of item 'item'
    void UpdateIsHiddenFile(CFTPQueueItemDel* item, BOOL isHiddenFile);

    // assigns 'size' to Size and 'sizeInBytes' to SizeInBytes of item 'item' + adjusts
    // TotalSizeInBytes and TotalSizeInBlocks in the operation ('oper')
    void UpdateFileSize(CFTPQueueItemCopyOrMove* item, CQuadWord const& size,
                        BOOL sizeInBytes, CFTPOperation* oper);

    // assigns 'asciiTransferMode' to AsciiTransferMode of item 'item'
    void UpdateAsciiTransferMode(CFTPQueueItemCopyOrMove* item, BOOL asciiTransferMode);

    // assigns 'ignoreAsciiTrModeForBinFile' to IgnoreAsciiTrModeForBinFile of item 'item'
    void UpdateIgnoreAsciiTrModeForBinFile(CFTPQueueItemCopyOrMove* item, BOOL ignoreAsciiTrModeForBinFile);

    // for upload: assigns 'asciiTransferMode' to AsciiTransferMode of item 'item'
    void UpdateAsciiTransferMode(CFTPQueueItemCopyOrMoveUpload* item, BOOL asciiTransferMode);

    // for upload: assigns 'ignoreAsciiTrModeForBinFile' to IgnoreAsciiTrModeForBinFile of item 'item'
    void UpdateIgnoreAsciiTrModeForBinFile(CFTPQueueItemCopyOrMoveUpload* item, BOOL ignoreAsciiTrModeForBinFile);

    // assigns 'tgtFileState' to TgtFileState of item 'item'
    void UpdateTgtFileState(CFTPQueueItemCopyOrMoveUpload* item, unsigned tgtFileState);

    // assigns 'size' to Size of item 'item' + adjusts TotalSizeInBytes in the operation ('oper')
    void UpdateFileSize(CFTPQueueItemCopyOrMoveUpload* item, CQuadWord const& size,
                        CFTPOperation* oper);

    // sets SizeWithCRLF_EOLs and NumberOfEOLs of item 'item'
    void UpdateTextFileSizes(CFTPQueueItemCopyOrMoveUpload* item, CQuadWord const& sizeWithCRLF_EOLs,
                             CQuadWord const& numberOfEOLs);

    // for debugging purposes only: verifies the counters ChildItemsNotDone+ChildItemsSkipped+
    // ChildItemsFailed+ChildItemsUINeeded in all items and also in the operation,
    // on error it throws TRACE_E
    void DebugCheckCounters(CFTPOperation* oper);

    // returns progress based on the ratio between completed ('doneOrSkippedCount') and all
    // items ('totalCount'); returns in 'unknownSizeCount' the number of unfinished items
    // with unknown size; returns in 'waitingCount' the number of items waiting for
    // processing (waiting + delayed + processing)
    int GetSimpleProgress(int* doneOrSkippedCount, int* totalCount, int* unknownSizeCount,
                          int* waitingCount);

    // returns information for download - copy/move progress; returns in 'downloaded' the sum of downloaded
    // sizes in bytes; returns in 'unknownSizeCount' the number of unfinished items with
    // unknown size; returns in 'totalWithoutErrors' the sum of sizes (in bytes)
    // of items that are not in an error state (a prompt to the user is also an error state)
    void GetCopyProgressInfo(CQuadWord* downloaded, int* unknownSizeCount,
                             CQuadWord* totalWithoutErrors, int* errorsCount,
                             int* doneOrSkippedCount, int* totalCount, CFTPOperation* oper);

    // returns information for upload - copy/move progress; returns in 'uploaded' the sum of uploaded
    // sizes in bytes; returns in 'unknownSizeCount' the number of unfinished items with
    // unknown size; returns in 'totalWithoutErrors' the sum of sizes (in bytes)
    // of items that are not in an error state (a prompt to the user is also an error state)
    void GetCopyUploadProgressInfo(CQuadWord* uploaded, int* unknownSizeCount,
                                   CQuadWord* totalWithoutErrors, int* errorsCount,
                                   int* doneOrSkippedCount, int* totalCount, CFTPOperation* oper);

    // returns TRUE if all items in the queue are in state sqisDone
    BOOL AllItemsDone();

    // increments LastErrorOccurenceTime by one and returns the new value of LastErrorOccurenceTime;
    // CAUTION: call only in the QueueCritSect critical section!!!
    DWORD GiveLastErrorOccurenceTime() { return ++LastErrorOccurenceTime; }

    // searches for the UID of the item that needs to open the Solve Error dialog (a
    // "new" error appeared in it (the user has not seen it yet)); returns TRUE if such an
    // item was found, its UID is returned in 'itemUID' + its index
    // in the queue in 'itemIndex' (the index may change immediately, so it should
    // be taken only as indicative)
    BOOL SearchItemWithNewError(int* itemUID, int* itemIndex);

protected:
    // searches for the item with UID 'UID'; returns NULL only if the item was not found
    // CAUTION: call only in the QueueCritSect critical section!!!
    CFTPQueueItem* FindItemWithUID(int UID);

    // updates GetOnlyExploreAndResolveItems and FirstWaitingItemIndex when changing an item
    // to state sqisWaiting; 'itemIndex' is the index of the item being changed; 'exploreOrResolveItem'
    // is the result of the item's IsExploreOrResolveItem() method
    // CAUTION: call only in the QueueCritSect critical section!!!
    void HandleFirstWaitingItemIndex(BOOL exploreOrResolveItem, int itemIndex);
};

//
// ****************************************************************************
// CFTPDiskThread
//

enum CFTPDiskWorkType
{
    fdwtNone,               // initialization value
    fdwtCreateDir,          // creating a directory
    fdwtCreateFile,         // creating/opening a file in a situation where the file state is unknown
    fdwtRetryCreatedFile,   // creating/opening a file in a situation where the file has already been created/overwritten/resumed_with_overwrite_option
    fdwtRetryResumedFile,   // creating/opening a file in a situation where the file has already been resumed
    fdwtCheckOrWriteFile,   // verifying contents or writing to a file (for "resume" it checks the end of the file, writing happens beyond the end of the file)
    fdwtCreateAndWriteFile, // if the file is not open, it creates it (overwrites an existing file if any) + writes flush data to the file (at the position of the current seek, so normally at the end of the file)
    fdwtListDir,            // lists a directory on disk
    fdwtDeleteDir,          // deleting a directory
    fdwtOpenFileForReading, // opening the source file on disk for reading (upload)
    fdwtReadFile,           // reading part of a file into a buffer (for upload)
    fdwtReadFileInASCII,    // reading part of a file for ASCII transfer mode (converting all EOLs to CRLF) into a buffer (for upload)
    fdwtDeleteFile,         // deleting a file on disk (source file for upload-Move)
};

struct CDiskListingItem
{
    char* Name;     // file/directory name
    BOOL IsDir;     // TRUE for a directory, FALSE for a file
    CQuadWord Size; // files only: size (in bytes)

    CDiskListingItem(const char* name, BOOL isDir, const CQuadWord& size);
    ~CDiskListingItem()
    {
        if (Name != NULL)
            free(Name);
    }
    BOOL IsGood() { return Name != NULL; }
};

struct CFTPDiskWork
{
    int SocketMsg; // parameters for posting the message when work is finished
    int SocketUID;
    DWORD MsgID;

    CFTPDiskWorkType Type; // type of disk work

    char Path[MAX_PATH]; // target path (e.g. fdwtCheckOrWriteFile and fdwtCreateAndWriteFile do not use it = "")
    char Name[MAX_PATH]; // IN/OUT target name (the name may change during autorename) (e.g. fdwtCheckOrWriteFile does not use it = "") (for fdwtCreateAndWriteFile this holds the tgt-full-file-name)

    CFTPQueueItemAction ForceAction; // action forced by the user (for example an autorename entered from the Solve Error dialog)

    BOOL AlreadyRenamedName; // TRUE if Name is already the renamed name (the name of the source file is different)

    unsigned CannotCreateDir : 2;    // see the CANNOTCREATENAME_XXX constants
    unsigned DirAlreadyExists : 2;   // see the DIRALREADYEXISTS_XXX constants
    unsigned CannotCreateFile : 2;   // see the CANNOTCREATENAME_XXX constants
    unsigned FileAlreadyExists : 3;  // see the FILEALREADYEXISTS_XXX constants
    unsigned RetryOnCreatedFile : 3; // see the RETRYONCREATFILE_XXX constants
    unsigned RetryOnResumedFile : 3; // see the RETRYONRESUMFILE_XXX constants

    // info for fdwtCheckOrWriteFile:
    // test the contents of the WorkFile file from offset CheckFromOffset to WriteOrReadFromOffset (if they match,
    // nothing is tested) to see whether it matches the contents of the FlushDataBuffer buffer (data start at the beginning);
    // starting at offset WriteOrReadFromOffset in the WorkFile file write FlushDataBuffer (the data start at
    // (WriteOrReadFromOffset - CheckFromOffset)); ValidBytesInFlushDataBuffer is the number of bytes in the FlushDataBuffer buffer
    //
    // info for fdwtReadFile + fdwtReadFileInASCII:
    // CheckFromOffset is not used, the file is read from offset WriteOrReadFromOffset, it is read into the FlushDataBuffer buffer
    // (for fdwtReadFileInASCII LF is additionally converted to CRLF), ValidBytesInFlushDataBuffer returns the number of bytes
    // placed into the FlushDataBuffer buffer (after reaching the end of the file this will be zero), additionally
    // WriteOrReadFromOffset returns the new offset in the file (for fdwtReadFileInASCII LF is converted to CRLF,
    // so the new offset cannot be computed from the previous offset and ValidBytesInFlushDataBuffer), for fdwtReadFileInASCII
    // EOLsInFlushDataBuffer returns the number of line endings in the FlushDataBuffer buffer
    CQuadWord CheckFromOffset;
    CQuadWord WriteOrReadFromOffset;
    char* FlushDataBuffer;
    int ValidBytesInFlushDataBuffer;
    int EOLsInFlushDataBuffer;
    HANDLE WorkFile;

    // the result of the disk operation is returned in the following variables:
    DWORD ProblemID;                               // if not ITEMPR_OK, this is the error that occurred
    DWORD WinError;                                // complements some ProblemID values (ignored when ITEMPR_OK)
    CFTPQueueItemState State;                      // if not sqisNone, this is the desired new state of the item
    char* NewTgtName;                              // if not NULL, this is an allocated new name (must be deallocated)
    HANDLE OpenedFile;                             // if not NULL, this is a newly opened file handle (must be closed)
    CQuadWord FileSize;                            // file size (for a new or overwritten file it is zero)
    BOOL CanOverwrite;                             // TRUE if the file can be overwritten (used to distinguish "resume" and "resume or overwrite")
    BOOL CanDeleteEmptyFile;                       // TRUE if an empty file can be deleted (used when canceling/on item error to decide whether to delete a zero-size file)
    TIndirectArray<CDiskListingItem>* DiskListing; // if not NULL (only when Type == fdwtListDir), this is an allocated listing

    void CopyFrom(CFTPDiskWork* work); // copies values from 'work' into 'this'
};

struct CFTPFileToClose
{
    char FileName[MAX_PATH]; // file name
    HANDLE File;             // file handle we should close
    BOOL DeleteIfEmpty;      // TRUE = if the file being closed is empty, delete it
    BOOL SetDateAndTime;     // TRUE = set 'Date'+'Time' before closing the file
    CFTPDate Date;           // date to set as the last write time of the file; if Date.Day==0, it is an "empty value"
    CFTPTime Time;           // time to set as the last write time of the file; if Time.Hour==24, it is an "empty value"
    BOOL AlwaysDeleteFile;   // TRUE = delete the file after closing
    CQuadWord EndOfFile;     // if not CQuadWord(-1, -1), this is the offset at which the file will be truncated

    CFTPFileToClose(const char* path, const char* name, HANDLE file, BOOL deleteIfEmpty,
                    BOOL setDateAndTime, const CFTPDate* date, const CFTPTime* time,
                    BOOL deleteFile, CQuadWord* setEndOfFile);
};

class CFTPDiskThread : public CThread
{
protected:
    HANDLE ContEvent; // "signaled" if there is work in the Work array or if the thread should terminate

    // critical section for accessing the data part of the object
    // CAUTION: consult access to critical sections in servers\critsect.txt!!!
    CRITICAL_SECTION DiskCritSect;

    TIndirectArray<CFTPDiskWork> Work;
    TIndirectArray<CFTPFileToClose> FilesToClose;
    BOOL ShouldTerminate;  // TRUE = the thread should terminate
    BOOL WorkIsInProgress; // TRUE = processing of item Work[0] is in progress

    int NextFileCloseIndex; // sequence number of the next file close operation
    int DoneFileCloseIndex; // sequence number of the last completed file close (-1 = none closed yet)

    // critical section without synchronization (access outside DiskCritSect)
    HANDLE FileClosedEvent; // pulsed after a file is closed (handles waiting for file closure)

public:
    CFTPDiskThread();
    ~CFTPDiskThread();

    BOOL IsGood() { return ContEvent != NULL; }

    void Terminate();

    // adds work to the thread, the pointer 'work' must remain valid until the result message is received
    // or until CancelWork() is called; returns success
    BOOL AddWork(CFTPDiskWork* work);

    // cancels the work 'work' added to the thread; returns TRUE if the work has not started yet
    // or if it can still be interrupted (it is in progress and after finishing the opposite
    // action will be performed); if the work is in progress, returns TRUE in 'workIsInProgress' (if not NULL); returns
    // FALSE if the work has already finished
    BOOL CancelWork(const CFTPDiskWork* work, BOOL* workIsInProgress);

    // adds a file to close to the thread; if 'deleteIfEmpty' is TRUE and the file has zero
    // size, it will also be deleted; 'path'+'name' is the full file name;
    // if 'setDateAndTime' is TRUE, before closing the file the write time
    // is set to 'date'+'time' (CAUTION: if date->Day==0 or time->Hour==24, these are
    // "empty values" for the date or time); if 'deleteFile' is TRUE, delete the file
    // immediately after closing it; if 'setEndOfFile' is not NULL, the file will be truncated
    // to the offset 'setEndOfFile' after closing; if 'fileCloseIndex' is not NULL, it returns
    // the sequence number of the file close (you can wait for this closure later,
    // see WaitForFileClose)
    BOOL AddFileToClose(const char* path, const char* name, HANDLE file, BOOL deleteIfEmpty,
                        BOOL setDateAndTime, const CFTPDate* date, const CFTPTime* time,
                        BOOL deleteFile, CQuadWord* setEndOfFile, int* fileCloseIndex);

    // waits for the file with sequence number 'fileCloseIndex' to be closed or for a timeout
    // ('timeout' in milliseconds or the value INFINITE = no timeout); returns TRUE
    // if the closure happened, FALSE on timeout
    BOOL WaitForFileClose(int fileCloseIndex, DWORD timeout);

    virtual unsigned Body();
};

//
// ****************************************************************************
// CFTPWorker
//
// "control connection" processing the selected operation item (selected
// from the queue of operation items)

class CFTPOperation;
struct CUploadWaitingWorker;

enum CFTPWorkerState
{
    fwsLookingForWork,      // has no work and should find some
    fwsSleeping,            // has no work, none was available, after the queue of operation items changes switch to fwsLookingForWork
    fwsPreparing,           // has work, verifies the feasibility of the operation item, checks the connection and switches to fwsWorking
    fwsConnecting,          // has work, has no connection, tries to connect to the server (timeout = switch to fwsWaitingForReconnect); if connection fails, returns the work and switches to fwsConnectionError; CAUTION: when ConnectAttemptNumber > 0 a reason for a new Connect must be set in ErrorDescr
    fwsWaitingForReconnect, // has work, has no connection, waits for the next attempt to connect to the server (switch to fwsConnecting); error description is in ErrorDescr
    fwsConnectionError,     // may or may not have work (tries to get rid of it), has no connection, waits for user intervention (entering username+password, instruction for another set of reconnects, etc.); error description is in ErrorDescr
    fwsWorking,             // has work, has a connection, performs the work (processes the operation item)
    fwsStopped,             // stopped, only waiting for deallocation (the worker must not take any more work)
};

enum CFTPWorkerSubState // substates for individual states from CFTPWorkerState
{
    fwssNone, // base substate of every state from CFTPWorkerState

    // substates of fwsLookingForWork:
    fwssLookFWQuitSent, // after sending the "QUIT" command

    // substates of fwsSleeping:
    fwssSleepingQuitSent, // after sending the "QUIT" command

    // substates of fwsPreparing:
    fwssPrepQuitSent,                 // after sending the "QUIT" command
    fwssPrepWaitForDisk,              // waiting for the disk operation to finish (part of verifying the feasibility of the item)
    fwssPrepWaitForDiskAfterQuitSent, // after sending the "QUIT" command + waiting for the disk operation to finish (part of verifying the feasibility of the item)

    // substates of fwsConnecting:
    // fwssNone,                // determine whether we need to obtain an IP address
    fwssConWaitingForIP,        // waiting to receive an IP address (resolving the host name)
    fwssConConnect,             // perform Connect()
    fwssConWaitForConRes,       // wait for the result of Connect()
    fwssConWaitForPrompt,       // wait for the login prompt from the server
    fwssConSendAUTH,            // send AUTH TLS before login script
    fwssConWaitForAUTHCmdRes,   // wait for response to AUTH TLS
    fwssConSendPBSZ,            // send PBSZ before login script
    fwssConWaitForPBSZCmdRes,   // wait for response to PBSZ
    fwssConSendPROT,            // send PROT before login script
    fwssConWaitForPROTCmdRes,   // wait for response to PROT
    fwssConSendNextScriptCmd,   // send the next command from the login script
    fwssConWaitForScriptCmdRes, // wait for the result of the command from the login script
    fwssConSendMODEZ,           // send MODE Z after login script to enable compression
    fwssConWaitForMODEZCmdRes,  // wait for response to MODE Z
    fwssConSendInitCmds,        // send another initialization command (user-defined, see CFTPOperation::InitFTPCommands) - CAUTION: set NextInitCmd=0 before the first call
    fwssConWaitForInitCmdRes,   // wait for the result of the executed initialization command
    fwssConSendSyst,            // determine the server system (send the "SYST" command)
    fwssConWaitForSystRes,      // wait for the server system information (result of the "SYST" command)
    fwssConReconnect,           // decide whether to perform reconnect or report a worker error (CAUTION: ErrorDescr must be set on input)

    // substates of fwsWorking:
    fwssWorkStopped, // work stopped (if the connection was open, the "QUIT" command was already sent), now only waiting for the worker to be released (the same for all work types)
    // fwssNone,                 // check ShouldStop + transition to substate fwssWorkStartWork (the same for all work types)
    fwssWorkStartWork,                         // start of work (the same for all work types)
    fwssWorkExplWaitForCWDRes,                 // explore-dir: waiting for the result of "CWD" (changing to the explored directory)
    fwssWorkExplWaitForPWDRes,                 // explore-dir: waiting for the result of "PWD" (obtaining the working path of the explored directory)
    fwssWorkExplWaitForPASVRes,                // explore-dir: waiting for the result of "PASV" (getting IP+port for the passive data connection)
    fwssWorkExplOpenActDataCon,                // explore-dir: open the active data connection
    fwssWorkExplWaitForListen,                 // explore-dir: waiting for the "listen" port to open (we open an active data connection) - local or on the proxy server
    fwssWorkExplSetTypeA,                      // explore-dir: set transfer mode to ASCII
    fwssWorkExplWaitForPORTRes,                // explore-dir: waiting for the result of "PORT" (passing IP+port to the server for the active data connection)
    fwssWorkExplWaitForTYPERes,                // explore-dir: waiting for the result of "TYPE" (switch to ASCII data transfer mode)
    fwssWorkExplSendListCmd,                   // explore-dir: send the LIST command
    fwssWorkExplActivateDataCon,               // explore-dir: activate the data connection (right after sending the LIST command)
    fwssWorkExplWaitForLISTRes,                // explore-dir: waiting for the result of "LIST" (waiting for the end of the listing transfer)
    fwssWorkExplWaitForDataConFinish,          // explore-dir: waiting for the "data connection" to end (the server response to "LIST" already arrived)
    fwssWorkExplProcessLISTRes,                // explore-dir: process the result of "LIST" (after ending the "data connection" and receiving the server response to "LIST")
    fwssWorkResLnkWaitForCWDRes,               // resolve-link: waiting for the result of "CWD" (changing to the explored link - if successful it is a directory link)
    fwssWorkSimpleCmdWaitForCWDRes,            // simple commands (everything except explore and resolve items): waiting for the result of "CWD" (changing to the directory of the processed file/link/subdirectory)
    fwssWorkSimpleCmdStartWork,                // simple commands (everything except explore and resolve items): start of work (the working directory is already set)
    fwssWorkDelFileWaitForDELERes,             // deleting file/link: waiting for the result of "DELE" (delete file/link)
    fwssWorkDelDirWaitForRMDRes,               // deleting directory/link: waiting for the result of "RMD" (delete directory/link)
    fwssWorkChAttrWaitForCHMODRes,             // change attributes: waiting for the result of "SITE CHMOD" (change file/directory mode, probably Unix only)
    fwssWorkChAttrWaitForCHMODQuotedRes,       // change attributes (name in quotes): waiting for the result of "SITE CHMOD" (change file/directory mode, probably Unix only)
    fwssWorkCopyWaitForPASVRes,                // copy/move file: waiting for the result of "PASV" (getting IP+port for the passive data connection)
    fwssWorkCopyOpenActDataCon,                // copy/move file: open the active data connection
    fwssWorkCopyWaitForListen,                 // copy/move file: waiting for the "listen" port to open (we open an active data connection) - local or on the proxy server
    fwssWorkCopySetType,                       // copy/move file: set the required transfer mode (ASCII / binary)
    fwssWorkCopyWaitForPORTRes,                // copy/move file: waiting for the result of "PORT" (passing IP+port to the server for the active data connection)
    fwssWorkCopyWaitForTYPERes,                // copy/move file: waiting for the result of "TYPE" (switch to ASCII / binary data transfer mode)
    fwssWorkCopyResumeFile,                    // copy/move file: optionally ensure resume (send the REST command)
    fwssWorkCopyWaitForResumeRes,              // copy/move file: waiting for the result of the "REST" command (resume file)
    fwssWorkCopyResumeError,                   // copy/move file: error of the "REST" command (not implemented, etc.) or we already know REST will fail
    fwssWorkCopySendRetrCmd,                   // copy/move file: send the RETR command (start reading the file, possibly from the offset specified by Resume)
    fwssWorkCopyActivateDataCon,               // copy/move file: activate the data connection (right after sending the RETR command)
    fwssWorkCopyWaitForRETRRes,                // copy/move file: waiting for the result of "RETR" (waiting for the file reading to finish)
    fwssWorkCopyWaitForDataConFinish,          // copy/move file: waiting for the "data connection" to end (the server response to "RETR" already arrived)
    fwssWorkCopyFinishFlushData,               // copy/move file: ensure flushing the data from the data connection is finished (the connection is already closed)
    fwssWorkCopyFinishFlushDataAfterQuitSent,  // copy/move file: after sending "QUIT" wait for the control connection to close + wait for the data flush to disk to finish
    fwssWorkCopyProcessRETRRes,                // copy/move file: process the result of "RETR" (after ending the "data connection", flushing data to disk and receiving the server response to "RETR")
    fwssWorkCopyDelayedAutoRetry,              // copy/move file: wait WORKER_DELAYEDAUTORETRYTIMEOUT milliseconds for auto-retry (so that all unexpected responses from the server can arrive)
    fwssWorkCopyTransferFinished,              // copy/move file: file transferred, in case of Move delete the source file
    fwssWorkCopyMoveWaitForDELERes,            // copy/move file: waiting for the result of "DELE" (Move: delete the source file/link after finishing the file transfer)
    fwssWorkCopyDone,                          // copy/move file: done, close the file and go to the next item
    fwssWorkUploadWaitForListing,              // upload copy/move file: wait for another worker to finish listing the target path on the server (to detect collisions)
    fwssWorkUploadResolveLink,                 // upload copy/move file: determine what the conflicting link is (file/directory) whose name collides with the target directory/file name on the server
    fwssWorkUploadResLnkWaitForCWDRes,         // upload copy/move file: waiting for the result of "CWD" (change to the explored link - if successful it is a directory link)
    fwssWorkUploadCreateDir,                   // upload copy/move file: create the target directory on the server - start by setting the target path
    fwssWorkUploadCrDirWaitForCWDRes,          // upload copy/move file: waiting for the result of "CWD" (setting the target path)
    fwssWorkUploadCrDirWaitForMKDRes,          // upload copy/move file: waiting for the result of "MKD" (creating the target directory)
    fwssWorkUploadCantCreateDirInvName,        // upload copy/move file: handle error "target directory cannot be created" (invalid name)
    fwssWorkUploadCantCreateDirFileEx,         // upload copy/move file: handle error "target directory cannot be created" (name already used for file or link to file)
    fwssWorkUploadDirExists,                   // upload copy/move file: handle error "target directory already exists"
    fwssWorkUploadDirExistsDirLink,            // same state as fwssWorkUploadDirExists: additionally CWD to the target directory was just performed (test whether the link is a directory or a file)
    fwssWorkUploadAutorenameDir,               // upload copy/move file: handle the error when creating the target directory - autorename - start by setting the target path
    fwssWorkUploadAutorenDirWaitForCWDRes,     // upload copy/move file: autorename - waiting for the result of "CWD" (setting the target path)
    fwssWorkUploadAutorenDirSendMKD,           // upload copy/move file: autorename - try to generate another new name for the target directory and try to create it
    fwssWorkUploadAutorenDirWaitForMKDRes,     // upload copy/move file: autorename - waiting for the result of "MKD" (creating the target directory under a new name)
    fwssWorkUploadGetTgtPath,                  // upload copy/move file: determine the path to the target directory on the server - start by changing to it
    fwssWorkUploadGetTgtPathWaitForCWDRes,     // upload copy/move file: waiting for the result of "CWD" (setting the path to the target directory)
    fwssWorkUploadGetTgtPathSendPWD,           // upload copy/move file: send "PWD" (getting the path to the target directory)
    fwssWorkUploadGetTgtPathWaitForPWDRes,     // upload copy/move file: waiting for the result of "PWD" (getting the path to the target directory)
    fwssWorkUploadListDiskDir,                 // upload copy/move file: list the directory being uploaded from disk
    fwssWorkUploadListDiskWaitForDisk,         // upload copy/move file: waiting for the disk operation to finish (listing the directory)
    fwssWorkUploadListDiskWaitForDiskAftQuit,  // upload copy/move file: after sending the "QUIT" command wait for the disk operation to finish (listing the directory)
    fwssWorkUploadCantCreateFileInvName,       // upload copy/move file: handle error "target file cannot be created" (invalid name)
    fwssWorkUploadCantCreateFileDirEx,         // upload copy/move file: handle error "target file cannot be created" (name already used for directory or link to directory)
    fwssWorkUploadFileExists,                  // upload copy/move file: handle error "target file already exists"
    fwssWorkUploadNewFile,                     // upload copy/move file: the target file does not exist, start uploading it
    fwssWorkUploadAutorenameFile,              // upload copy/move file: handle the error when creating the target file - autorename
    fwssWorkUploadResumeFile,                  // upload copy/move file: problem "target file exists" - resume
    fwssWorkUploadTestIfFinished,              // upload copy/move file: we sent the entire file + the server "just" did not respond, most likely the file is OK, we will test it
    fwssWorkUploadResumeOrOverwriteFile,       // upload copy/move file: problem "target file exists" - resume or overwrite
    fwssWorkUploadOverwriteFile,               // upload copy/move file: problem "target file exists" - overwrite
    fwssWorkUploadFileSetTgtPath,              // upload file: set the target path
    fwssWorkUploadFileSetTgtPathWaitForCWDRes, // upload file: waiting for the result of "CWD" (setting the target path)
    fwssWorkUploadGenNewName,                  // upload file: autorename: generate a new name
    fwssWorkUploadLockFile,                    // upload file: open the file in FTPOpenedFiles
    fwssWorkUploadDelForOverwrite,             // upload file: if this is an overwrite and delete should be used first, do it here
    fwssWorkUploadDelForOverWaitForDELERes,    // upload file: waiting for the DELE result before overwrite
    fwssWorkUploadFileAllocDataCon,            // upload file: allocate the data connection
    fwssWorkUploadGetFileSize,                 // upload file: resume: determine the file size (via the SIZE command or from the listing)
    fwssWorkUploadWaitForSIZERes,              // upload file: resume: waiting for the response to the SIZE command
    fwssWorkUploadGetFileSizeFromListing,      // upload file: resume: the SIZE command failed (or is not implemented), determine the file size from the listing
    fwssWorkUploadTestFileSizeOK,              // upload copy/move file: after an upload error the file size test succeeded
    fwssWorkUploadTestFileSizeFailed,          // upload copy/move file: after an upload error the file size test failed
    fwssWorkUploadWaitForPASVRes,              // upload copy/move file: waiting for the result of "PASV" (getting IP+port for the passive data connection)
    fwssWorkUploadOpenActDataCon,              // upload copy/move file: open the active data connection
    fwssWorkUploadWaitForListen,               // upload copy/move file: waiting for the "listen" port to open (we open an active data connection) - local or on the proxy server
    fwssWorkUploadWaitForPORTRes,              // upload copy/move file: waiting for the result of "PORT" (passing IP+port to the server for the active data connection)
    fwssWorkUploadSetType,                     // upload copy/move file: set the required transfer mode (ASCII / binary)
    fwssWorkUploadWaitForTYPERes,              // upload copy/move file: waiting for the result of "TYPE" (switch to ASCII / binary data transfer mode)
    fwssWorkUploadSendSTORCmd,                 // upload copy/move file: send the STOR/APPE command (start storing the file on the server)
    fwssWorkUploadActivateDataCon,             // upload copy/move file: activate the data connection (right after sending the STOR command)
    fwssWorkUploadWaitForSTORRes,              // upload copy/move file: waiting for the result of "STOR/APPE" (waiting for the file upload to finish)
    fwssWorkUploadCopyTransferFinished,        // upload copy/move file: the target file has already been uploaded, if it is a Move try to delete the source file on disk
    fwssWorkUploadDelFileWaitForDisk,          // upload copy/move file: waiting for the disk operation to finish (deleting the source file)
    fwssWorkUploadDelFileWaitForDiskAftQuit,   // upload copy/move file: after sending the "QUIT" command wait for the disk operation to finish (deleting the source file)
    fwssWorkUploadWaitForDELERes,              // upload copy/move file: waiting for the result of the DELE command (deleting the target file)
    fwssWorkUploadCopyDone,                    // upload copy/move file: done, move on to the next item
};

enum CFTPWorkerSocketEvent
{
    fwseConnect,       // [error, 0], connection to the server opened
    fwseClose,         // [error, 0], socket was closed
    fwseNewBytesRead,  // [error, 0], another block of data read into the socket buffer
    fwseWriteDone,     // [error, 0], buffer write finished (only if Write returned 'allBytesWritten'==FALSE)
    fwseIPReceived,    // [IP, error], we received the IP (while resolving the host name)
    fwseTimeout,       // [0, 0], timer delivery reports timeout while sending FTP commands (see WORKER_TIMEOUTTIMERID)
    fwseWaitForCmdErr, // [0, 0], sending a command to the server failed, waiting whether FD_CLOSE arrives, if not, close the socket "manually"
};

enum CFTPWorkerEvent
{
    fweActivate, // (re)activation of the worker (sent after creating the worker to start activity + after changing the worker state so that before processing the new state other sockets get their turn as well (thanks to the sockets thread message loop))

    fweWorkerShouldStop,   // notify the worker to terminate (sent when the worker has an open connection and is in an "idle" state (waiting for queue changes, user response, reconnect, etc.) - these conditions can be met by: state fwsSleeping or state fwsWorking:fwssWorkUploadWaitForListing or state fwsWorking:fwssWorkExplWaitForListen or state fwsWorking:fwssWorkCopyWaitForListen or state fwsWorking:fwssWorkUploadWaitForListen or state fwsLookingForWork when ShouldBePaused==TRUE)
    fweWorkerShouldPause,  // notify the worker to pause (sent to a resumed (running) worker)
    fweWorkerShouldResume, // notify the worker to resume (sent to a paused worker)

    fweCmdReplyReceived, // receive a complete reply (except type FTP_D1_MAYBESUCCESS) to the sent FTP command (arrives only after all bytes of the FTP command have been sent)
    fweCmdInfoReceived,  // receive a complete reply of type FTP_D1_MAYBESUCCESS to the sent FTP command (arrives only after all bytes of the FTP command have been sent)
    fweCmdConClosed,     // the connection was closed while executing the command (even during/before sending the command + also due to a timeout); error description is in ErrorDescr; whether it was a timeout is in CommandReplyTimeout

    fweIPReceived,   // we set the discovered IP in the operation object (while resolving the host name)
    fweIPRecFailure, // error while obtaining the IP (while resolving the host name); error description is in ErrorDescr

    fweConTimeout, // timeout for actions during the connection to the server (see WORKER_CONTIMEOUTTIMID)

    fweConnected,      // requested connection established (FD_CONNECT received)
    fweConnectFailure, // error while establishing the connection (FD_CONNECT received with an error); error description is in ErrorDescr

    fweReconTimeout, // timeout for the next connect attempt (see WORKER_RECONTIMEOUTTIMID)

    fweNewLoginParams, // notify the worker about new login parameters (password/account) (see WORKER_NEWLOGINPARAMS)

    fweWakeUp, // a "sleeping" worker should wake up and go look for work in the queue (see WORKER_WAKEUP)

    fweDiskWorkFinished,        // disk work finished (see WORKER_DISKWORKFINISHED)
    fweDiskWorkWriteFinished,   // disk work - write - finished (see WORKER_DISKWORKWRITEFINISHED)
    fweDiskWorkListFinished,    // disk work - directory listing - finished (see WORKER_DISKWORKLISTFINISHED)
    fweDiskWorkReadFinished,    // disk work - read - finished (see WORKER_DISKWORKREADFINISHED)
    fweDiskWorkDelFileFinished, // disk work - file deletion - finished (see WORKER_DISKWORKDELFILEFINISHED)

    fweDataConConnectedToServer, // data connection reports that it connected to the server (see WORKER_DATACON_CONNECTED)
    fweDataConConnectionClosed,  // data connection reports that the connection to the server was closed/interrupted (see WORKER_DATACON_CLOSED)
    fweDataConFlushData,         // data connection reports that data are ready in the flush buffer for verification/write to disk (see WORKER_DATACON_FLUSHDATA)
    fweDataConListeningForCon,   // data connection reports that the "listen" port was opened (see WORKER_DATACON_LISTENINGFORCON)

    fweDataConStartTimeout, // timeout for waiting for the data connection to open after receiving the server reply to RETR (see WORKER_DATACONSTARTTIMID)

    fweDelayedAutoRetry, // timer for delayed auto-retry (see WORKER_DELAYEDAUTORETRYTIMID)

    fweTgtPathListingFinished, // UploadListingCache obtained the listing of the target path (or learned about an error while downloading it) (see WORKER_TGTPATHLISTINGFINISHED)

    fweUplDataConConnectedToServer, // upload data connection reports that it connected to the server (see WORKER_UPLDATACON_CONNECTED)
    fweUplDataConConnectionClosed,  // upload data connection reports that the connection to the server was closed/interrupted (see WORKER_UPLDATACON_CLOSED)
    fweUplDataConPrepareData,       // upload data connection reports that more data need to be prepared in the flush buffer for sending to the server (see WORKER_UPLDATACON_PREPAREDATA)
    fweUplDataConListeningForCon,   // upload data connection reports that the "listen" port was opened (see WORKER_UPLDATACON_LISTENINGFORCON)

    fweDataConListenTimeout, // timeout for waiting for the "listen" port to open on the proxy server (when opening an active data connection) (see WORKER_LISTENTIMEOUTTIMID)
};

enum CFTPWorkerCmdState
{
    fwcsIdle, // no command is in progress (in this state we only receive unexpected messages from the server)

    // an FTP command is being sent, HandleSocketEvent() prepares for HandleEvent()
    // fweCmdReplyReceived (receiving a reply to the FTP command, except replies of type FTP_D1_MAYBESUCCESS),
    // fweCmdInfoReceived (receiving a reply of type FTP_D1_MAYBESUCCESS) and fweCmdConClosed (error while sending
    // the command or connection loss while sending/waiting for the reply or a timeout - connection closed
    // because we did not receive the server's reply to the FTP command)
    fwcsWaitForCmdReply,
    fwcsWaitForLoginPrompt, // same functionality as fwcsWaitForCmdReply, we distinguish only because of error messages

    // waiting for the reason of an error that occurred when sending a command (writing to the socket): waiting for fwseClose,
    // if it does not arrive we close the socket "manually" (on fwseWaitForCmdErr timeout); at the same time we capture an error
    // message from the socket or at least from fwseClose; if we capture nothing, we at least print WaitForCmdErrError
    fwcsWaitForCmdError,
};

enum CWorkerDataConnectionState
{
    wdcsDoesNotExist,         // does not exist (WorkerDataCon == NULL && WorkerUploadDataCon == NULL)
    wdcsOnlyAllocated,        // only allocated (not yet added to SocketsThread)
    wdcsWaitingForConnection, // in SocketsThread, waiting for connection to the server (active/passive)
    wdcsTransferingData,      // connection to the server established, data transfer possible
    wdcsTransferFinished,     // transfer completed (successful/unsuccessful), connection to the server is closed or could not be established at all
};

enum CWorkerStatusType // type of status information stored in the worker (displayed in the operation dialog in the Connections listview)
{
    wstNone,           // no status is displayed
    wstDownloadStatus, // download status is displayed (retrieving listings + copy & move from FTP to disk)
    wstUploadStatus,   // upload status is displayed (retrieving listings of target paths + copy & move from disk to FTP)
};

enum CFlushDataError // types of errors during data flushing (Copy and Move operations)
{
    fderNone,               // none
    fderASCIIForBinaryFile, // ASCII transfer mode used for a binary file
    fderLowMemory,          // insufficient memory to add work to FTPDiskThread
    fderWriteError,         // error detected only when writing to disk
};

enum CPrepareDataError // types of errors when preparing data to send to the server (upload: Copy and Move operations)
{
    pderNone,               // none
    pderASCIIForBinaryFile, // ASCII transfer mode used for a binary file
    pderLowMemory,          // insufficient memory to add work to FTPDiskThread
    pderReadError,          // error detected already when reading from disk
};

enum CUploadType
{
    utNone,                  // no file upload in progress
    utNewFile,               // upload to a new file (it did not exist before the upload)
    utAutorename,            // upload to a new file automatically named so that nothing is overwritten
    utResumeFile,            // resume an existing file on the server (append the part of the file not yet uploaded)
    utResumeOrOverwriteFile, // see utResumeFile + if the file cannot be resumed, overwrite it
    utOverwriteFile,         // overwrite an existing file on the server
    utOnlyTestFileSize,      // tests whether the file is fully uploaded (based on matching file size)
};

#define FTPWORKER_ERRDESCR_BUFSIZE 200             // buffer size of CFTPWorker::ErrorDescr
#define FTPWORKER_BYTESTOWRITEONSOCKETPREALLOC 512 // how many bytes to preallocate for writing (so that another write does not allocate unnecessarily due to a 1-byte overflow)
#define FTPWORKER_BYTESTOREADONSOCKET 1024         // minimum number of bytes to read from the socket at once (also allocate the buffer for read data)
#define FTPWORKER_BYTESTOREADONSOCKETPREALLOC 512  // how many bytes to preallocate for reading (so that another read does not immediately allocate again)

#define WORKER_ACTIVATE 10                   // message ID posted to the worker when it should activate
#define WORKER_SHOULDSTOP 11                 // message ID posted to the worker in the "idle" state (notification that it should stop)
#define WORKER_NEWLOGINPARAMS 12             // message ID posted to the worker when new login parameters (password/account) are available
#define WORKER_WAKEUP 13                     // message ID posted to the worker in the "idle" state when new items are available in the queue
#define WORKER_DISKWORKFINISHED 14           // message ID posted to the worker when FTPDiskThread finishes the requested disk work
#define WORKER_DATACON_CONNECTED 15          // message ID posted to the worker when the data connection connects to the server
#define WORKER_DATACON_CLOSED 16             // message ID posted to the worker when the data connection closes/interrupts the connection to the server
#define WORKER_DATACON_FLUSHDATA 17          // message ID posted to the worker when the data connection has data ready in the flush buffer for verification/write to disk
#define WORKER_DISKWORKWRITEFINISHED 18      // message ID posted to the worker when FTPDiskThread finishes the requested disk work - write
#define WORKER_TGTPATHLISTINGFINISHED 19     // message ID posted to the worker when UploadListingCache obtains the target path listing (or learns about an error while downloading it)
#define WORKER_DISKWORKLISTFINISHED 20       // message ID posted to the worker when FTPDiskThread finishes the requested disk work - directory listing
#define WORKER_UPLDATACON_CONNECTED 21       // message ID posted to the worker when the upload data connection connects to the server
#define WORKER_UPLDATACON_CLOSED 22          // message ID posted to the worker when the upload data connection closes/interrupts the connection to the server
#define WORKER_UPLDATACON_PREPAREDATA 23     // message ID posted to the worker when more data need to be prepared in the flush buffer for sending to the server (into the upload data connection)
#define WORKER_DISKWORKREADFINISHED 24       // message ID posted to the worker when FTPDiskThread finishes the requested disk work - read (for upload)
#define WORKER_DISKWORKDELFILEFINISHED 25    // message ID posted to the worker when FTPDiskThread finishes the requested disk work - file deletion
#define WORKER_SHOULDPAUSE 26                // message ID posted to a resumed worker (notification that it should pause)
#define WORKER_SHOULDRESUME 27               // message ID posted to a paused worker (notification that it should resume)
#define WORKER_DATACON_LISTENINGFORCON 28    // message ID posted to the worker when the data connection opens the "listen" port
#define WORKER_UPLDATACON_LISTENINGFORCON 29 // message ID posted to the worker when the upload data connection opens the "listen" port

#define WORKER_TIMEOUTTIMERID 30        // timer ID ensuring timeout detection when sending FTP commands to the server
#define WORKER_CMDERRORTIMERID 31       // timer ID enabling waiting for the type of error that occurred when sending a command to the server (see fwseWaitForCmdErr)
#define WORKER_CONTIMEOUTTIMID 32       // timer ID ensuring timeout detection for obtaining the IP address, connect and login prompt
#define WORKER_RECONTIMEOUTTIMID 33     // timer ID for waiting for another connect attempt (reconnect)
#define WORKER_STATUSUPDATETIMID 34     // timer ID for periodically updating the status
#define WORKER_DATACONSTARTTIMID 35     // timer ID for detecting a timeout while waiting for the data connection to open after receiving the server reply to RETR (WarFTPD illogically sends 226 even before our data connection accept)
#define WORKER_DELAYEDAUTORETRYTIMID 36 // timer ID for delaying auto-retry (e.g. Quick&Easy FTPD returns 426 for RETR and immediately 220 - if you retry immediately, responses shift (220 is taken as the response to the next command instead of this command and everything goes wrong))
#define WORKER_LISTENTIMEOUTTIMID 37    // timer ID ensuring timeout detection when opening the "listen" port on the proxy server (when opening an active data connection)

#define WORKER_STATUSUPDATETIMEOUT 1000     // time in milliseconds after which the status in the worker (and the worker progress and overall progress in the operation dialog) is updated - NOTE: linked to OPERDLG_STATUSMINIDLETIME
#define WORKER_DELAYEDAUTORETRYTIMEOUT 5000 // time in milliseconds after which auto-retry is performed (see WORKER_DELAYEDAUTORETRYTIMID)

class CUploadDataConnectionSocket;

class CFTPWorker : public CSocket
{
protected:
    // the critical section for accessing the socket part of the object is CSocket::SocketCritSect
    // WARNING: within this section do not nest into SocketsThread->CritSect (do not call SocketsThread methods)

    int ControlConnectionUID; // if the worker has a connection from the panel, this stores the UID of the panel socket object (otherwise -1)

    BOOL HaveWorkingPath;                     // TRUE if WorkingPath is valid
    char WorkingPath[FTP_MAX_PATH];           // current working path on the FTP server (it may be only the last string sent via CWD with a "success" return value - for performance reasons we do not run PWD after every CWD)
    CCurrentTransferMode CurrentTransferMode; // current transfer mode on the FTP server (only stores the last FTP command "TYPE")

    BOOL EventConnectSent; // TRUE only if the fwseConnect event has already been generated (handles FD_READ arriving before FD_CONNECT)

    char* BytesToWrite;            // buffer for bytes that were not written in Write (they are written after receiving FD_WRITE)
    int BytesToWriteCount;         // number of valid bytes in the 'BytesToWrite' buffer
    int BytesToWriteOffset;        // number of bytes already sent from the 'BytesToWrite' buffer
    int BytesToWriteAllocatedSize; // allocated size of the 'BytesToWrite' buffer

    char* ReadBytes;            // buffer for bytes read from the socket (they are read after receiving FD_READ)
    int ReadBytesCount;         // number of valid bytes in the 'ReadBytes' buffer
    int ReadBytesOffset;        // number of bytes already processed (skipped) in the 'ReadBytes' buffer
    int ReadBytesAllocatedSize; // allocated size of the 'ReadBytes' buffer

    CDataConnectionSocket* WorkerDataCon;             // NULL; otherwise, the data connection currently used by this worker (state see WorkerDataConState)
    CUploadDataConnectionSocket* WorkerUploadDataCon; // NULL; otherwise, the data connection currently used by this worker for upload (state see WorkerDataConState)

    // critical section for accessing the data part of the object (data for display in dialogs)
    // WARNING: consult access to critical sections in the file servers\critsect.txt !!!
    CRITICAL_SECTION WorkerCritSect;

    int CopyOfUID; // copy of CSocket::UID available in the WorkerCritSect critical section
    int CopyOfMsg; // copy of CSocket::Msg available in the WorkerCritSect critical section

    int ID;                                      // worker ID (shown in the ID column in the Connections list view in the operation dialog)
    int LogUID;                                  // log UID for this worker (-1 if the log is not created); NOTE: it is in WorkerCritSect and not SocketCritSect !!!
    CFTPWorkerState State;                       // worker state, see CFTPWorkerState
    CFTPWorkerSubState SubState;                 // state inside the worker state (substate for the processing steps of each State), see CFTPWorkerSubState
    CFTPQueueItem* CurItem;                      // read-only data: processed item (in state sqisProcessing), NULL=worker has no work; write via Queue and CurItem->UID
    char ErrorDescr[FTPWORKER_ERRDESCR_BUFSIZE]; // textual description of the error, contains no CR or LF and does not end with a period; ensuring these conditions see CorrectErrorDescr(); displayed for fwsWaitingForReconnect and fwsConnectionError, filled on errors see CFTPWorkerEvent
    int ConnectAttemptNumber;                    // number of the current attempt to establish the connection; before the very first attempt this is zero (set to one when the connection is established)
    CCertificate* UnverifiedCertificate;         // SSL: if the attempt to connect fails due to an unknown untrusted certificate, the connection is closed and the certificate is stored here (we show it to the user in the Solve Error dialog)

    DWORD ErrorOccurenceTime; // "time" the error occurred (used to keep the order of resolving errors according to when they arose); -1 = no error occurred

    BOOL ShouldStop;     // TRUE as soon as the worker is expected to finish (after calling InformAboutStop())
    BOOL SocketClosed;   // TRUE/FALSE: the worker "control connection" socket is closed/open
    BOOL ShouldBePaused; // TRUE if the user wants to pause this worker (after calling InformAboutPause(TRUE))

    CFTPWorkerCmdState CommandState; // worker state related to sending commands (see CFTPWorkerCmdState)
    BOOL CommandTransfersData;       // TRUE = the sent command is used to transfer data (timeout is adjusted based on data-connection activity - WorkerDataCon+WorkerUploadDataCon)
    BOOL CommandReplyTimeout;        // valid only for the currently sent fweCmdConClosed event: TRUE = timeout while waiting for a reply (forcibly closed connection), FALSE = the connection was not closed by the client (closed by the server or a connection error)

    DWORD WaitForCmdErrError; // error of the last Write(), more details see fwcsWaitForCmdError

    BOOL CanDeleteSocket;    // FALSE = the worker is still in the operation's WorkersList, it cannot be cancelled after handing the socket to the "control connection" panel
    BOOL ReturnToControlCon; // TRUE = the worker returns the socket to the "control connection" panel, it cannot be left for deletion in DeleteWorkers

    BOOL ReceivingWakeup; // TRUE = this worker has been posted the WORKER_WAKEUP message (after delivery it switches to FALSE)

    const char* ProxyScriptExecPoint; // current proxy script command (NULL = first command); WARNING: not intended for reading, only for passing to CFTPOperation::PrepareNextScriptCmd()
    int ProxyScriptLastCmdReply;      // reply to the last command sent from the proxy script (-1 = none)

    BOOL DiskWorkIsUsed; // TRUE if DiskWork is inserted in FTPDiskThread

    HANDLE OpenedFile;                   // target file for copy/move operations
    CQuadWord OpenedFileSize;            // current size of the 'OpenedFile'
    CQuadWord OpenedFileOriginalSize;    // original size of the 'OpenedFile'
    BOOL CanDeleteEmptyFile;             // TRUE if an empty file may be deleted (used when canceling/an item error to decide whether to delete a zero-sized file)
    CQuadWord OpenedFileCurOffset;       // current offset in the 'OpenedFile' (where data from the data connection should be written/checked)
    CQuadWord OpenedFileResumedAtOffset; // resume offset in the 'OpenedFile' (offset sent by the REST command - from where we check/write)
    BOOL ResumingOpenedFile;             // TRUE = performing Resume download (only verifying the existing part, then writing); FALSE = performing Overwrite download (only writing)

    HANDLE OpenedInFile;                     // source file for copy/move operations (upload)
    CQuadWord OpenedInFileSize;              // size of the 'OpenedInFile' determined when the file was opened
    CQuadWord OpenedInFileCurOffset;         // current offset in the 'OpenedInFile' (from where data should be read from the file for writing to the data connection)
    CQuadWord OpenedInFileSizeWithCRLF_EOLs; // size of the part of the file read from disk so far with CRLF line endings (upload of a text file)
    CQuadWord OpenedInFileNumberOfEOLs;      // number of line endings in the part of the file read from disk so far (upload of a text file)
    CQuadWord FileOnServerResumedAtOffset;   // resume offset from which the source file is read for the APPE command
    BOOL ResumingFileOnServer;               // TRUE = performing Resume upload; FALSE = performing Overwrite upload

    int LockedFileUID; // UID of the file locked (in FTPOpenedFiles) for working on the item in this worker (0 = no file is locked)

    CWorkerDataConnectionState WorkerDataConState; // state of the data connection (see WorkerDataCon+WorkerUploadDataCon)
    BOOL DataConAllDataTransferred;                // TRUE if the data connection has already been closed and all data have been received from or sent to the server

    SYSTEMTIME StartTimeOfListing; // time when we started retrieving the listing (just before allocating the data connection)
    DWORD StartLstTimeOfListing;   // IncListingCounter() from the moment we started retrieving the listing (just before allocating the data connection)
    int ListCmdReplyCode;          // result of the "LIST"/"RETR" command stored for later processing (see fwssWorkExplProcessLISTRes/fwssWorkCopyProcessRETRRes)
    char* ListCmdReplyText;        // result of the "LIST"/"RETR" command stored for later processing (see fwssWorkExplProcessLISTRes/fwssWorkCopyProcessRETRRes)

    CWorkerStatusType StatusType;   // type of status information stored in the worker (shown in the operation dialog in the Connections list view)
    DWORD StatusConnectionIdleTime; // for StatusType == wstDownloadStatus/wstUploadStatus: time in seconds since the last data reception
    DWORD StatusSpeed;              // for StatusType == wstDownloadStatus/wstUploadStatus: connection speed in bytes per second
    CQuadWord StatusTransferred;    // for StatusType == wstDownloadStatus/wstUploadStatus: number of bytes already downloaded/uploaded
    CQuadWord StatusTotal;          // for StatusType == wstDownloadStatus/wstUploadStatus: total download/upload size in bytes - if unknown, CQuadWord(-1, -1)

    DWORD LastTimeEstimation; // -1 == invalid, otherwise the rounded number of seconds until the work with the current item finishes

    CFlushDataError FlushDataError;     // error code that occurred while flushing data (Copy and Move operations)
    CPrepareDataError PrepareDataError; // error code that occurred while preparing data (upload: Copy and Move operations)

    BOOL UploadDirGetTgtPathListing; // only when processing upload-dir-explore or upload-file items: TRUE = the target path listing should be fetched

    int UploadAutorenamePhase;              // upload: current phase of generating names for the target directory/file (see FTPGenerateNewName()); 0 = beginning of the autorename process; -1 = it was the last generation phase, we simply cannot generate another name of that type
    char UploadAutorenameNewName[MAX_PATH]; // upload: buffer for the last generated name for the target directory/file

    CUploadType UploadType; // type of upload (according to the state of the target file): new, resume, resume or overwrite, overwrite, autorename

    BOOL UseDeleteForOverwrite; // upload: TRUE = call DELE before STOR (handles overwriting a file of another user on Unix - it cannot be written to, but it can be deleted and then created)

    // data without the need for a critical section (read-only + valid for the entire lifetime of the object):
    CFTPOperation* Oper; // operation to which the worker belongs (the worker has a shorter lifetime than the operation)
    CFTPQueue* Queue;    // queue of the operation to which the worker belongs (the worker has a shorter lifetime than the operation queue)

    // data without the need for a critical section (used only in the sockets thread):
    int IPRequestUID; // counter to distinguish individual requests for obtaining the IP address (may be called repeatedly; results would otherwise get mixed) (used only in the sockets thread, no synchronization needed)
    int NextInitCmd;  // ordinal number of the next command sent in the CFTPOperation::InitFTPCommands sequence - 0 = first command

    // data without the need for a critical section (written only in the sockets thread and disk thread, synchronization via FTPDiskThread and DiskWorkIsUsed):
    CFTPDiskWork DiskWork; // work data submitted to the FTPDiskThread object (thread performing disk operations)

public:
    CFTPWorker(CFTPOperation* oper, CFTPQueue* queue, const char* host, unsigned short port,
               const char* user);
    ~CFTPWorker();

    // returns ID (in the WorkerCritSect critical section)
    int GetID();

    // sets ID (in the WorkerCritSect critical section)
    void SetID(int id);

    // returns State (in the WorkerCritSect critical section)
    CFTPWorkerState GetState();

    // returns ShouldBePaused (in the WorkerCritSect critical section); in 'isWorking' (must not be
    // NULL) returns TRUE if the worker is working (not sleeping, not waiting for the user, and not terminated);
    BOOL IsPaused(BOOL* isWorking);

    // sets 'CopyOfUID' and 'CopyOfMsg' (in the CSocket::SocketCritSect and
    // WorkerCritSect critical sections); always returns TRUE
    BOOL RefreshCopiesOfUIDAndMsg();

    // returns a copy of CSocket::UID (in the WorkerCritSect critical section)
    int GetCopyOfUID();

    // returns a copy of CSocket::Msg (in the WorkerCritSect critical section)
    int GetCopyOfMsg();

    // returns LogUID (in the WorkerCritSect critical section)
    int GetLogUID();

    // returns ShouldStop (in the WorkerCritSect critical section)
    BOOL GetShouldStop();

    // returns data for the Connections list view in the operation dialog
    void GetListViewData(LVITEM* itemData, char* buf, int bufSize);

    // returns TRUE if it is possible that the worker needs the user to resolve an error (states
    // fwsConnectionError (error must be resolved) and fwsWaitingForReconnect (login parameters
    // might need to be changed - e.g. when "Always try to reconnect" is enabled
    // and an incorrect password is entered))
    BOOL HaveError();

    // returns TRUE if the worker needs the user to resolve an error (see HaveError());
    // if the worker is in the state fwsWaitingForReconnect, it changes to fwsConnectionError;
    // if it returns TRUE, it also returns the error text in 'buf' (buffer of size 'bufSize'), and if
    // the error is caused by an untrusted server certificate, it returns it
    // in 'unverifiedCertificate' (if not NULL; the caller is responsible for releasing
    // the certificate using its Release() method); if it is necessary to post
    // fweActivate after calling the method, it returns TRUE in 'postActivate';
    BOOL GetErrorDescr(char* buf, int bufSize, BOOL* postActivate,
                       CCertificate** unverifiedCertificate);

    // used to query whether the worker can be cancelled from the CReturningConnections methods (only
    // if we already attempted deletion via DeleteWorkers); returns TRUE if cancellation is possible
    BOOL CanDeleteFromRetCons();

    // used to query whether the worker can be cancelled from DeleteWorkers (only if the
    // connection from the worker is not being returned to the panel, or if we already attempted deletion
    // from the CReturningConnections methods); returns TRUE if cancellation is possible
    BOOL CanDeleteFromDelWorkers();

    // informs the worker that it should attempt to stop; returns TRUE if the worker
    // wants to close the data connection or post WORKER_SHOULDSTOP (fweWorkerShouldStop)
    // by calling CloseDataConnectionOrPostShouldStop() after this method finishes
    // WARNING: may be called for one worker several times in a row (check whether it has already been called; if so, do nothing and return FALSE)!
    BOOL InformAboutStop();

    // closes the data connection (called outside all critical sections so CloseSocket() + DeleteSocket() can be invoked without issues)
    // or posts WORKER_SHOULDSTOP (fweWorkerShouldStop)
    void CloseDataConnectionOrPostShouldStop();

    // informs the worker that it should attempt to pause/resume; returns TRUE if the worker
    // wants to post WORKER_SHOULDPAUSE or WORKER_SHOULDRESUME (fweWorkerShouldPause
    // or fweWorkerShouldResume) by calling PostShouldPauseOrResume() after this
    // method; 'pause' is TRUE/FALSE depending on pause/resume
    // WARNING: may be called for one worker several times in a row (check whether it has already been called; if so, do nothing and return FALSE)!
    BOOL InformAboutPause(BOOL pause);

    // posts WORKER_SHOULDPAUSE or WORKER_SHOULDRESUME (fweWorkerShouldPause
    // or fweWorkerShouldResume) according to the state ShouldBePaused; called outside all
    // critical sections
    void PostShouldPauseOrResume();

    // determines whether the worker "control connection" socket is closed and the worker
    // "data connection" does not exist; returns TRUE if the "control connection" socket is closed
    // and the "data connection" does not exist (the worker can be cancelled)
    BOOL SocketClosedAndDataConDoesntExist();

    // determines whether the worker has work in the disk thread; returns FALSE if it has no work
    // (the worker can be cancelled)
    BOOL HaveWorkInDiskThread();

    // prepares the worker for cancellation (force-closes both the "control connection" and the "data connection");
    // called outside all critical sections or in the single CSocketsThread::CritSect critical section
    // so CloseSocket() can be invoked without issues
    void ForceClose();

    // preparation of the worker for cancellation (cancels work in the disk thread)
    void ForceCloseDiskWork();

    // releases worker data (returns the processed operation item back to the queue);
    // updates the list of workers waiting for WORKER_TGTPATHLISTINGFINISHED
    // in 'uploadFirstWaitingWorker'
    void ReleaseData(CUploadWaitingWorker** uploadFirstWaitingWorker);

    // activates the worker - posts WORKER_ACTIVATE to the socket
    // WARNING: enters the CSocketsThread::CritSect and CSocket::SocketCritSect sections !!!
    void PostActivateMsg();

    // clears the read and write buffers (useful for example before reopening the connection)
    void ResetBuffersAndEvents();

    // writes to the socket (performs "send") bytes from the 'buffer' of length 'bytesToWrite'
    // (if 'bytesToWrite' is -1, writes strlen(buffer) bytes); on error returns FALSE and,
    // if the Windows error code is known, returns it in 'error' (if not NULL); if it returns TRUE,
    // at least part of the buffer was successfully written; in 'allBytesWritten' (must not be
    // NULL) returns TRUE if the entire buffer was written; if 'allBytesWritten'
    // returns FALSE, before the next call to Write you must wait for the fwseWriteDone event
    // (once it arrives, the write is complete)
    BOOL Write(const char* buffer, int bytesToWrite, DWORD* error, BOOL* allBytesWritten);

    // helper method to detect whether the entire reply from the FTP server is already in the 'ReadBytes' buffer;
    // returns TRUE on success - in 'reply' (must not be NULL) returns a pointer
    // to the start of the reply, in 'replySize' (must not be NULL) returns the reply length,
    // in 'replyCode' (if not NULL) returns the FTP reply code or -1 if the reply has no
    // code (does not start with a three-digit number); if the reply is not complete yet, returns
    // FALSE - calling ReadFTPReply again makes sense only after the fwseNewBytesRead event is received
    // WARNING: must be called from the SocketCritSect critical section (otherwise the 'ReadBytes' buffer
    // could change arbitrarily)
    BOOL ReadFTPReply(char** reply, int* replySize, int* replyCode = NULL);

    // helper method to release the FTP server reply (of length 'replySize') from the
    // 'ReadBytes' buffer
    // WARNING: must be called from the SocketCritSect critical section (otherwise the 'ReadBytes'
    // buffer could change arbitrarily)
    void SkipFTPReply(int replySize);

    // helper method for reading error messages from the ReadBytes buffer (the socket may
    // already be closed); it writes the messages to the Log and, if ErrorDescr is empty,
    // fills it with the first error message
    // WARNING: must be called from the SocketCritSect critical section (otherwise the 'ReadBytes'
    // buffer could change arbitrarily) and the WorkerCritSect critical section
    void ReadFTPErrorReplies();

    // determines whether the worker is in the "sleeping" state and, if so (returns TRUE),
    // also determines whether it has an open connection (returns TRUE in 'hasOpenedConnection')
    // and whether it is already waiting for the WORKER_WAKEUP message to be delivered, i.e.
    // someone is already waking it up (returns TRUE in 'receivingWakeup')
    BOOL IsSleeping(BOOL* hasOpenedConnection, BOOL* receivingWakeup);

    // sets ReceivingWakeup (inside the WorkerCritSect critical section)
    void SetReceivingWakeup(BOOL receivingWakeup);

    // passes an item from 'sourceWorker' to this worker and puts it into the fwsPreparing state,
    // 'sourceWorker' is put into the fwsLookingForWork state (from which it most likely moves to fwsSleeping)
    void GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker);

    // adds to 'downloaded' the size (in bytes) of the file currently being downloaded by this worker
    void AddCurrentDownloadSize(CQuadWord* downloaded);

    // adds to 'uploaded' the size (in bytes) of the file currently being uploaded by this worker
    void AddCurrentUploadSize(CQuadWord* uploaded);

    // if the worker is in the fwsConnectionError state, returns ErrorOccurenceTime (inside the
    // WorkerCritSect critical section); otherwise returns -1
    DWORD GetErrorOccurenceTime();

    // ******************************************************************************************
    // methods called in the "sockets" thread (based on receiving messages from the system or other threads)
    //
    // WARNING: called inside the SocketsThread->CritSect section; they should be executed as quickly
    //          as possible (no waiting for user input, etc.)
    // ******************************************************************************************

    // receipt of the result of calling GetHostByAddress; if 'ip' == INADDR_NONE, it's an error and 'err'
    // may contain the error code (if 'err' != 0)
    virtual void ReceiveHostByAddress(DWORD ip, int hostUID, int err);

    // reception of an event for this socket (FD_READ, FD_WRITE, FD_CLOSE, etc.); 'index' is
    // the socket index in the SocketsThread->Sockets array (used for repeated posting of
    // messages for the socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // reception of the result from ReceiveNetEvent(FD_CLOSE) - if 'error' is not NO_ERROR, it is
    // a Windows error code (arrived with FD_CLOSE or arose while processing FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // reception of a timer with ID 'id' and parameter 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // reception of a posted message with ID 'id' and parameter 'param'
    virtual void ReceivePostMessage(DWORD id, void* param);

protected:
    // processing of all event types on the worker socket; called only in the sockets thread, so
    // concurrent invocation is not a threat; called inside the single CSocketsThread::CritSect critical section
    void HandleSocketEvent(CFTPWorkerSocketEvent event, DWORD data1, DWORD data2);

    // processing of worker events (activation, sending commands, etc.); called only in the sockets
    // thread, so concurrent invocation is not a threat; called in the CSocketsThread::CritSect and
    // CSocket::SocketCritSect critical sections; 'reply'+'replySize'+'replyCode' contain the server
    // reply (only for fweCmdReplyReceived and fweCmdInfoReceived, otherwise they are NULL+0+0) - after
    // the HandleEvent() method finishes, SkipFTPReply(replySize) is called
    void HandleEvent(CFTPWorkerEvent event, char* reply, int replySize, int replyCode);

    // helper method solely to make HandleEvent() easier to follow
    void HandleEventInPreparingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                     BOOL& reportWorkerChange);

    // helper method solely to make HandleEvent() easier to follow
    void HandleEventInConnectingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                      BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                      int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                      int replyCode, BOOL& operStatusMaybeChanged);

    // helper method solely to make HandleEvent() easier to follow
    void HandleEventInWorkingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                   BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                   int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                   int replyCode);

    // helper method solely to make HandleEventInWorkingState and HandleEvent() easier to follow
    void HandleEventInWorkingState2(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                    int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                    int replyCode, char* ftpPath, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop, BOOL* listingNotAccessible);

    // helper method solely to make HandleEventInWorkingState and HandleEvent() easier to follow
    void HandleEventInWorkingState3(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    char* buf, char* errBuf, int& cmdLen, BOOL& sendCmd,
                                    char* reply, int replySize, int replyCode, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop);

    // helper method solely to make HandleEventInWorkingState and HandleEvent() easier to follow
    void HandleEventInWorkingState4(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                    int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                    int replyCode, char* ftpPath, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop, BOOL& quitCmdWasSent);

    // helper method solely to make HandleEventInWorkingState and HandleEvent() easier to follow
    void HandleEventInWorkingState5(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                    int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                    int replyCode, char* ftpPath, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop, BOOL& quitCmdWasSent);

    // helper method solely to make HandleEventInWorkingState() easier to follow
    BOOL ParseListingToFTPQueue(TIndirectArray<CFTPQueueItem>* ftpQueueItems,
                                const char* allocatedListing, int allocatedListingLen,
                                CServerType* serverType, BOOL* lowMem,
                                BOOL isVMS, BOOL isAS400, int transferMode, CQuadWord* totalSize,
                                BOOL* sizeInBytes, BOOL selFiles, BOOL selDirs,
                                BOOL includeSubdirs, DWORD attrAndMask,
                                DWORD attrOrMask, int operationsUnknownAttrs,
                                int operationsHiddenFileDel, int operationsHiddenDirDel);

    // helper method solely to make HandleEventInWorkingState2() easier to follow
    void OpenActDataCon(CFTPWorkerSubState waitForListen, char* errBuf,
                        BOOL& conClosedRetryItem, BOOL& lookForNewWork);

    // helper method solely to make HandleEventInWorkingState2() easier to follow
    void WaitForListen(CFTPWorkerEvent event, BOOL& handleShouldStop, char* errBuf,
                       char* buf, int& cmdLen, BOOL& sendCmd, BOOL& conClosedRetryItem,
                       CFTPWorkerSubState waitForPORTRes);

    // helper method solely to make HandleEventInWorkingState2() easier to follow
    void WaitForPASVRes(CFTPWorkerEvent event, char* reply, int replySize, int replyCode,
                        BOOL& handleShouldStop, BOOL& nextLoop, BOOL& conClosedRetryItem,
                        CFTPWorkerSubState setType, CFTPWorkerSubState openActDataCon);

    // helper method solely to make HandleEventInWorkingState2() easier to follow
    void WaitForPORTRes(CFTPWorkerEvent event, BOOL& nextLoop, BOOL& conClosedRetryItem,
                        CFTPWorkerSubState setType);

    // helper method solely to make HandleEventInWorkingState2() easier to follow
    void SetTypeA(BOOL& handleShouldStop, char* errBuf, char* buf, int& cmdLen,
                  BOOL& sendCmd, BOOL& nextLoop, CCurrentTransferMode trMode,
                  BOOL asciiTrMode, CFTPWorkerSubState waitForTYPERes,
                  CFTPWorkerSubState trModeAlreadySet);

    // helper method solely to make HandleEventInWorkingState2() easier to follow
    void WaitForTYPERes(CFTPWorkerEvent event, int replyCode, BOOL& nextLoop, BOOL& conClosedRetryItem,
                        CCurrentTransferMode trMode, CFTPWorkerSubState trModeAlreadySet);

    // adjusts the text in the ErrorDescr buffer so that it contains no CR or LF and has no period at the end
    // WARNING: call only inside the WorkerCritSect critical section !!!
    void CorrectErrorDescr();

    // initializes the items of the 'DiskWork' structure
    // WARNING: call only inside the CSocket::SocketCritSect critical section + enters the
    //          CFTPOperation::OperCritSect section !!!
    void InitDiskWork(DWORD msgID, CFTPDiskWorkType type, const char* path, const char* name,
                      CFTPQueueItemAction forceAction, BOOL alreadyRenamedName,
                      char* flushDataBuffer, CQuadWord const* checkFromOffset,
                      CQuadWord const* writeOrReadFromOffset, int validBytesInFlushDataBuffer,
                      HANDLE workFile);

    // returns the processed 'CurItem' back to the queue (returns it to the "waiting" state so another
    // worker can process it)
    // WARNING: call only inside the WorkerCritSect critical section !!!
    void ReturnCurItemToQueue();

    // closes the open file 'OpenedFile' (only if it is open, otherwise does nothing);
    // 'transferAborted' is TRUE if the file transfer was interrupted (in addition to closing,
    // an empty file may also be deleted), otherwise it is FALSE (the file was transferred successfully);
    // if 'setDateAndTime' is TRUE, the write time is set to 'date'+'time' before the file is closed
    // (WARNING: if date->Day==0 or time->Hour==24, these are "empty values" for the date or time);
    // if 'deleteFile' is TRUE, the file is deleted immediately after it is closed; if 'setEndOfFile'
    // is not NULL, the file is truncated at the 'setEndOfFile' offset after it is closed
    // WARNING: call only inside the WorkerCritSect critical section !!!
    void CloseOpenedFile(BOOL transferAborted, BOOL setDateAndTime, const CFTPDate* date,
                         const CFTPTime* time, BOOL deleteFile, CQuadWord* setEndOfFile);

    // closes the open file 'OpenedInFile' (only if it is open, otherwise does nothing);
    // WARNING: call only inside the WorkerCritSect critical section !!!
    void CloseOpenedInFile();

    // reports the worker socket closure or cancellation/completion of the worker's work in the disk thread,
    // see WorkerMayBeClosedEvent
    void ReportWorkerMayBeClosed();

    // processes the error (see FlushDataError) that occurred while flushing data
    // (Copy and Move operations) and resets FlushDataError; returns TRUE if an
    // error occurred and was processed; returns FALSE if no error occurred
    // and it is possible to continue normally
    // WARNING: call only inside the WorkerCritSect critical section !!!
    BOOL HandleFlushDataError(CFTPQueueItemCopyOrMove* curItem, BOOL& lookForNewWork);

    // processes the error (see PrepareDataError) that occurred while preparing data
    // (upload: Copy and Move operations) and resets PrepareDataError; returns TRUE if an
    // error occurred and was processed; returns FALSE if no error occurred
    // and it is possible to continue normally
    // WARNING: call only inside the WorkerCritSect critical section !!!
    BOOL HandlePrepareDataError(CFTPQueueItemCopyOrMoveUpload* curItem, BOOL& lookForNewWork);

    friend class CControlConnectionSocket;
};

//
// ****************************************************************************
// CFTPWorkersList
//

class CFTPWorkersList
{
protected:
    // critical section for accessing the object's data
    // WARNING: consult access to critical sections in the servers\critsect.txt file !!!
    CRITICAL_SECTION WorkersListCritSect;

    TIndirectArray<CFTPWorker> Workers; // array of workers
    int NextWorkerID;                   // counter for worker IDs (displayed in the Connections listview in the operation dialog)

    DWORD LastFoundErrorOccurenceTime; // "time" of the last worker found with an error or the "time" before which no such worker exists

public:
    CFTPWorkersList();
    ~CFTPWorkersList();

    // adds a new worker to the array; returns TRUE on success
    // WARNING: should not be called while ActivateWorkers(), PostLoginChanged(),
    //          PostNewWorkAvailable() are running
    BOOL AddWorker(CFTPWorker* newWorker);

    // informs workers that they should attempt to stop; operates on all workers
    // of the operation (if 'workerInd' is -1) or only on the worker at index 'workerInd'; must
    // be called repeatedly until it returns TRUE (processing is batched); 'victims'+'maxVictims'
    // is an array for workers that need to close the data connection or post WORKER_SHOULDSTOP
    // (fweWorkerShouldStop) by calling CloseDataConnectionOrPostShouldStop() after this method
    // finishes; 'foundVictims' receives the number of filled items in the 'victims' array on input and
    // returns the updated count of filled items
    BOOL InformWorkersAboutStop(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // informs workers that they should attempt to pause/resume; operates on all workers
    // of the operation (if 'workerInd' is -1) or only on the worker at index 'workerInd'; must
    // be called repeatedly until it returns TRUE (processing is batched); 'victims'+'maxVictims'
    // is an array for workers that need to post WORKER_SHOULDPAUSE or WORKER_SHOULDRESUME
    // (fweWorkerShouldPause or fweWorkerShouldResume) by calling PostShouldPauseOrResume() after
    // this method finishes; 'foundVictims' receives the number of filled items in the 'victims'
    // array on input and returns the updated count of filled items; 'pause' is TRUE if pause should
    // be performed, FALSE if resume should be performed
    BOOL InformWorkersAboutPause(int workerInd, CFTPWorker** victims, int maxVictims,
                                 int* foundVictims, BOOL pause);

    // determines whether all workers (if 'workerInd' is -1) or the worker at index
    // 'workerInd' already has the socket closed and no work in progress in the disk thread; returns
    // TRUE if the sockets are closed (disconnected) and the workers have no work
    // in the disk thread
    BOOL CanCloseWorkers(int workerInd);

    // forces all workers of the operation (if 'workerInd' is -1) or the worker at index
    // 'workerInd' to quickly close the socket and cancel the work in the disk thread (if the work is
    // already finished, it lets the worker process its results, it should not be a slowdown); must
    // be called repeatedly until it returns TRUE (processing is batched); 'victims'+'maxVictims'
    // is an array for workers for which the ForceClose() method should be called after this method
    // finishes; 'foundVictims' receives the number of filled items in the 'victims' array on input
    // and returns the updated count of filled items
    BOOL ForceCloseWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // gradually cancels all workers of the operation (if 'workerInd' is -1) or cancels
    // the worker at index 'workerInd'; must be called repeatedly until it returns TRUE
    // (processing is batched); 'victims'+'maxVictims' is an array for sockets
    // that should be closed by calling DeleteSocket() after this method finishes;
    // 'foundVictims' receives the number of filled items in the 'victims' array on input
    // and returns the updated count of filled items;
    // 'uploadFirstWaitingWorker' contains the updated list of workers waiting for
    // WORKER_TGTPATHLISTINGFINISHED
    BOOL DeleteWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims,
                       CUploadWaitingWorker** uploadFirstWaitingWorker);

    // returns the number of workers
    int GetCount();

    // returns the index of the first worker that reports an error (state fwsConnectionError);
    // if no worker reports an error, returns -1
    int GetFirstErrorIndex();

    // returns the index of the worker with ID 'workerID'; if not found, returns -1
    int GetWorkerIndex(int workerID);

    // returns the data for displaying the worker at index 'index' in the listview in the operation dialog;
    // 'buf'+'bufSize' is a buffer for the text returned in 'lvdi' (changes in three cycles
    // to meet the LVN_GETDISPINFO requirements); if the index is not
    // valid, does nothing (listview refresh is already on the way)
    void GetListViewDataFor(int index, NMLVDISPINFO* lvdi, char* buf, int bufSize);

    // returns the worker ID at index 'index'; -1 = invalid index
    int GetWorkerID(int index);

    // returns the log UID of the worker at index 'index'; -1 = invalid index or the worker has no log
    int GetLogUID(int index);

    // returns TRUE if it is possible that the worker at index 'index' needs the user to resolve
    // an error (see CFTPWorker::HaveError()); returns FALSE for an invalid index
    BOOL HaveError(int index);

    // returns TRUE if the worker at index 'index' is paused (see CFTPWorker::IsPaused());
    // in 'isWorking' (must not be NULL) returns TRUE if the worker is working (not sleeping, not waiting for
    // the user and not finished);
    // returns FALSE for an invalid index (also in 'isWorking')
    BOOL IsPaused(int index, BOOL* isWorking);

    // returns TRUE if at least one worker is working (not sleeping, not waiting for the user and not finished);
    // in 'someIsWorkingAndNotPaused' (must not be NULL) returns TRUE if at least
    // one worker is working (not sleeping, not waiting for the user and not finished) and not paused
    BOOL SomeWorkerIsWorking(BOOL* someIsWorkingAndNotPaused);

    // returns TRUE if the worker at index 'index' needs the user to resolve an error
    // (see HaveError()); if this worker is in the fwsWaitingForReconnect state, it changes
    // to the fwsConnectionError state; if it returns TRUE, it also returns the error text in 'buf' (buffer
    // of size 'bufSize') and, if the error is caused by an untrusted server certificate,
    // returns it in 'unverifiedCertificate' (if not NULL; the caller is responsible for releasing
    // the certificate using its Release() method); returns FALSE for an invalid index
    // WARNING: enters the CSocketsThread::CritSect section !!!
    BOOL GetErrorDescr(int index, char* buf, int bufSize, CCertificate** unverifiedCertificate);

    // activates all workers (posts WORKER_ACTIVATE to the worker socket)
    // WARNING: does not fully lock the WorkersListCritSect, it may not be executed for a worker
    //          added while the method is running (see AddWorker()) !!!
    // WARNING: enters the CSocketsThread::CritSect and CSocket::SocketCritSect sections !!!
    void ActivateWorkers();

    // informs selected workers with an error about changed login parameters (a new connection attempt should be made);
    // if 'workerID' is -1, informs all workers in the fwsConnectionError state;
    // if 'workerID' is not -1, informs the worker with ID 'workerID' (if it is in the fwsConnectionError state)
    // WARNING: does not fully lock the WorkersListCritSect, it may not be executed for a worker
    //          added while the method is running (see AddWorker()) !!!
    // WARNING: enters the CSocketsThread::CritSect section !!!
    void PostLoginChanged(int workerID);

    // informs workers in the "sleeping" state about the existence of new work (impulse to search
    // for work in the item queue); if 'onlyOneItem' is TRUE, there is just one item
    // (informing one worker is enough), otherwise there are more items (inform all
    // workers); informing the workers = posting WORKER_WAKEUP;
    // WARNING: does not fully lock the WorkersListCritSect, it may not be executed for a worker
    //          added while the method is running (see AddWorker()) !!!
    // WARNING: enters the CSocketsThread::CritSect section !!!
    void PostNewWorkAvailable(BOOL onlyOneItem);

    // attempts to find a "sleeping" worker with an open connection; on success,
    // returns TRUE and passes the item from 'sourceWorker' to it and puts it into the
    // fwsPreparing state (+ posts fweActivate to it); 'sourceWorker' is put into the
    // fwsSleeping state
    // WARNING: enters the CSocketsThread::CritSect section !!!
    BOOL GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker);

    // adds to 'downloaded' the size (in bytes) of the files currently being downloaded by all workers
    void AddCurrentDownloadSize(CQuadWord* downloaded);

    // adds to 'uploaded' the size (in bytes) of the files currently being uploaded by all workers
    void AddCurrentUploadSize(CQuadWord* uploaded);

    // searches for the index of a worker that needs to open the Solve Error dialog (a
    // "new" error appeared there (the user has not seen it yet)); 'lastErrorOccurenceTime' is the "time"
    // assigned to the last error (used for a quick test whether it even makes sense to look
    // for a "new" error); returns TRUE if such a worker was found,
    // its index is returned in 'index'
    BOOL SearchWorkerWithNewError(int* index, DWORD lastErrorOccurenceTime);

    // returns TRUE if the worker list is empty or all workers should stop
    BOOL EmptyOrAllShouldStop();

    // returns TRUE if at least one worker is waiting for the user
    BOOL AtLeastOneWorkerIsWaitingForUser();
};

//
// ****************************************************************************
// CTransferSpeedMeter
//
// object for calculating data transfer speed in the data connection

#define DATACON_ACTSPEEDSTEP 1000     // for computing the transfer speed: step size in milliseconds (must not be 0)
#define DATACON_ACTSPEEDNUMOFSTEPS 60 // for computing the transfer speed: number of steps used (more steps = smoother speed changes when the first step in the queue "drops out")

class CTransferSpeedMeter
{
protected:
    CRITICAL_SECTION TransferSpeedMeterCS; // critical section for accessing the object's data

    // transfer speed calculation:
    DWORD TransferedBytes[DATACON_ACTSPEEDNUMOFSTEPS + 1]; // circular queue with the number of bytes transferred in the last N steps (time intervals) + one extra "working" step (the value for the current interval is accumulated there)
    int ActIndexInTrBytes;                                 // index of the last (current) record in TransferedBytes
    DWORD ActIndexInTrBytesTimeLim;                        // time limit (in ms) of the last record in TransferedBytes (bytes are added to the last record up to this time)
    int CountOfTrBytesItems;                               // number of steps in TransferedBytes (closed ones + one "working" step)

    DWORD LastTransferTime; // GetTickCount from the moment of the last BytesReceived call

public:
    CTransferSpeedMeter();
    ~CTransferSpeedMeter();

    // resets the object (preparation for reuse)
    // can be called from any thread
    void Clear();

    // returns the connection speed in bytes per second; in 'transferIdleTime' (may be NULL)
    // returns the time in seconds since the last data reception
    // can be called from any thread
    DWORD GetSpeed(DWORD* transferIdleTime);

    // called at the moment the connection is established (active/passive)
    // can be called from any thread
    void JustConnected();

    // called after transferring a portion of data; 'count' contains how much data it was; 'time' is
    // the transfer duration
    void BytesReceived(DWORD count, DWORD time);
};

//
// ****************************************************************************
// CSynchronizedDWORD
//
// DWORD with synchronized access for use from multiple threads

class CSynchronizedDWORD
{
private:
    CRITICAL_SECTION ValueCS; // critical section for accessing the object's data
    DWORD Value;

public:
    CSynchronizedDWORD();
    ~CSynchronizedDWORD();

    void Set(DWORD value);
    DWORD Get();
};

//
// ****************************************************************************
// CFTPOperation
//
// object with information about the operation as a whole (connection parameters, operation type, etc.)

enum CFTPOperationType
{
    fotNone,         // empty operation (must be set to one of the following types)
    fotDelete,       // delete operation
    fotCopyDownload, // copy-from-server operation
    fotMoveDownload, // move-from-server operation
    fotChangeAttrs,  // attribute change operation
    fotCopyUpload,   // copy-to-server operation
    fotMoveUpload,   // move-to-server operation
};

// how to solve the problem "target file/directory cannot be created"
#define CANNOTCREATENAME_USERPROMPT 0 // ask the user
#define CANNOTCREATENAME_AUTORENAME 1 // use automatic renaming
#define CANNOTCREATENAME_SKIP 2       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the problem "target file already exists"
#define FILEALREADYEXISTS_USERPROMPT 0 // ask the user
#define FILEALREADYEXISTS_AUTORENAME 1 // use automatic renaming
#define FILEALREADYEXISTS_RESUME 2     // resume (append to the end of the file - the file can only grow)
#define FILEALREADYEXISTS_RES_OVRWR 3  // resume or overwrite (if resume is not possible, delete the file + create it again)
#define FILEALREADYEXISTS_OVERWRITE 4  // overwrite (delete + create again)
#define FILEALREADYEXISTS_SKIP 5       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the problem "target directory already exists"
#define DIRALREADYEXISTS_USERPROMPT 0 // ask the user
#define DIRALREADYEXISTS_AUTORENAME 1 // use automatic renaming
#define DIRALREADYEXISTS_JOIN 2       // use the existing directory as the target directory (merge directories)
#define DIRALREADYEXISTS_SKIP 3       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the problem "retry on a file created or overwritten directly by the FTP client"
#define RETRYONCREATFILE_USERPROMPT 0 // ask the user
#define RETRYONCREATFILE_AUTORENAME 1 // use automatic renaming
#define RETRYONCREATFILE_RESUME 2     // resume (append to the end of the file - the file can only grow)
#define RETRYONCREATFILE_RES_OVRWR 3  // resume or overwrite (if resume is not possible, delete the file + create it again)
#define RETRYONCREATFILE_OVERWRITE 4  // overwrite (delete + create again)
#define RETRYONCREATFILE_SKIP 5       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the problem "retry on a file resumed by the FTP client"
#define RETRYONRESUMFILE_USERPROMPT 0 // ask the user
#define RETRYONRESUMFILE_AUTORENAME 1 // use automatic renaming
#define RETRYONRESUMFILE_RESUME 2     // resume (append to the end of the file - the file can only grow)
#define RETRYONRESUMFILE_RES_OVRWR 3  // resume or overwrite (if resume is not possible, delete the file + create it again)
#define RETRYONRESUMFILE_OVERWRITE 4  // overwrite (delete + create again)
#define RETRYONRESUMFILE_SKIP 5       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the problem "ASCII transfer mode for a binary file"
#define ASCIITRFORBINFILE_USERPROMPT 0 // ask the user
#define ASCIITRFORBINFILE_IGNORE 1     // ignore it, the user knows what they are doing
#define ASCIITRFORBINFILE_INBINMODE 2  // change the transfer mode to binary and restart the transfer (assuming nothing has been written to the file yet)
#define ASCIITRFORBINFILE_SKIP 3       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the situation "deleting a non-empty directory"
#define NONEMPTYDIRDEL_USERPROMPT 0 // ask the user
#define NONEMPTYDIRDEL_DELETEIT 1   // delete it without asking
#define NONEMPTYDIRDEL_SKIP 2       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the situation "deleting a hidden file"
#define HIDDENFILEDEL_USERPROMPT 0 // ask the user
#define HIDDENFILEDEL_DELETEIT 1   // delete it without asking
#define HIDDENFILEDEL_SKIP 2       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the situation "deleting a hidden directory"
#define HIDDENDIRDEL_USERPROMPT 0 // ask the user
#define HIDDENDIRDEL_DELETEIT 1   // delete it without asking
#define HIDDENDIRDEL_SKIP 2       // skip (simply do not perform the operation)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

// how to solve the problem "file/directory has unknown attributes that we cannot preserve (permissions other than 'r'+'w'+'x')"
#define UNKNOWNATTRS_USERPROMPT 0 // ask the user
#define UNKNOWNATTRS_IGNORE 1     // ignore it, the user knows what they are doing (set attributes as close to the requested ones as possible)
#define UNKNOWNATTRS_SKIP 2       // we will not change attributes on this file/directory (skip the file/directory)
// WARNING: when adding a value, check the bit range in CFTPOperation !!!

class CExploredPaths
{
protected:
    TIndirectArray<char> Paths; // format of paths: short with the path length + the path text itself + null terminator

public:
    CExploredPaths() : Paths(100, 500) {}

    // returns TRUE if the path 'path' is already in 'Paths';
    // WARNING: assumes that 'path' is a path returned by the server, so paths
    // are compared only as case-sensitive strings (slashes/backslashes/dots, etc., are not trimmed)
    // - a cycle may be detected only on its second pass, which is sufficient for our purposes
    BOOL ContainsPath(const char* path);

    // stores the path 'path' in the 'Paths' list;
    // returns FALSE if 'path' is already in this list (out of memory is ignored in this function)
    // WARNING: assumes that 'path' is a path returned by the server, so paths
    // are compared only as case-sensitive strings (slashes/backslashes/dots, etc., are not trimmed)
    // - a cycle may be detected only on its second pass, which is sufficient for our purposes
    BOOL AddPath(const char* path);

protected:
    // returns "found?" and the index of the item or where it should be inserted (sorted array)
    BOOL GetPathIndex(const char* path, int pathLen, int& index);
};

enum COperationState
{
    opstNone,                 // empty value (variable initialization)
    opstInProgress,           // the operation is still in progress (there are items in the sqisWaiting, sqisProcessing, sqisDelayed or sqisUserInputNeeded states)
    opstSuccessfullyFinished, // the operation was successfully completed (all items are in the sqisDone state)
    opstFinishedWithSkips,    // the operation was successfully completed but with skips (items are in the sqisDone or sqisSkipped states)
    opstFinishedWithErrors    // the operation was completed but with errors (items are in the sqisDone, sqisSkipped, sqisFailed or sqisForcedToFail states)
};

#define SMPLCMD_APPROXBYTESIZE 1000 // approximate size of processing a single item in bytes for measuring the speed of Delete and ChangeAttrs operations

class CFTPOperation
{
public:
    static int NextOrdinalNumber;                // global counter for the operation's OrdinalNumber (access only within the NextOrdinalNumberCS section!)
    static CRITICAL_SECTION NextOrdinalNumberCS; // critical section for NextOrdinalNumber

protected:
    // critical section for accessing the object's data
    // WARNING: consult access to critical sections in the servers\critsect.txt file !!!
    CRITICAL_SECTION OperCritSect;

    int UID;           // unique operation number (index in the CFTPOperationsList::Operations array + link for items in the Queue)
    int OrdinalNumber; // sequential number of the operation (position in the CFTPOperationsList::Operations array is not by the operation creation time)

    CFTPQueue* Queue; // queue of operation items

    CFTPWorkersList WorkersList; // array of workers ("control connections" processing the operation's items from the queue)

    COperationDlg* OperationDlg; // operation dialog object (runs in its own thread)
    HANDLE OperationDlgThread;   // handle of the thread in which the last opened operation dialog ran/is running

    CFTPProxyServer* ProxyServer;          // NULL = "not used (direct connection)"
    const char* ProxyScriptText;           // proxy script text (exists even when the proxy server is not used); WARNING: may point to ProxyServer->ProxyScript (i.e. the text is valid only until ProxyServer is deallocated)
    const char* ProxyScriptStartExecPoint; // line with the first command (the line after the line with "connect to:")
    char* ConnectToHost;                   // "host" according to the proxy script
    unsigned short ConnectToPort;          // "port" according to the proxy script
    DWORD HostIP;                          // IP address of the FTP server 'Host' (==INADDR_NONE if the IP is unknown) (used only for SOCKS4 proxy server)

    char* Host;                   // host (FTP server) (NULL = unknown)
    unsigned short Port;          // port on which the FTP server runs (-1 = unknown)
    char* User;                   // user (NULL = unknown) WARNING: anonymous is already part of the string here
    char* Password;               // password (NULL = unknown) WARNING: anonymous password (email) is already part of the string here
    char* Account;                // account info (see FTP command "ACCT") (NULL = unknown)
    BOOL RetryLoginWithoutAsking; // TRUE = the worker should try to reconnect even for "error" server replies (code "5xx"); FALSE = reconnect only for "transient-error" replies (code "4xx"); set later by the user when resolving the worker connection problem
    char* InitFTPCommands;        // list of FTP commands to send to the server immediately after connecting (NULL = no commands)
    BOOL UsePassiveMode;          // TRUE/FALSE = passive/active data connection mode
    BOOL SizeCmdIsSupported;      // FALSE = the SIZE command received the server reply "not supported" (no point trying again)
    char* ListCommand;            // command to obtain a listing of a path on this server (NULL = "LIST")
    DWORD ServerIP;               // IP address of the FTP/Proxy server 'ConnectToHost' (==INADDR_NONE until the IP is known)
    char* ServerSystem;           // server system (reply to the SYST command) - may also be NULL
    char* ServerFirstReply;       // first server reply (often contains the FTP server version) - may also be NULL
    BOOL UseListingsCache;        // TRUE = the user wants to store listings in the cache for this connection
    char* ListingServerType;      // server type for parsing listings: NULL = autodetect; otherwise the server type name (without the optional leading '*'; if it stops existing, it switches to autodetect)
    BOOL EncryptControlConnection;
    BOOL EncryptDataConnection;
    int CompressData;
    CCertificate* pCertificate;

    CExploredPaths ExploredPaths; // list of explored paths (paths where workers have already performed "explore-dir")

    int ReportChangeInWorkerID; // -2 = no changes (the operation dialog needs to be posted WM_APP_WORKERCHANGEREP), -1 = change in more than one worker, otherwise the ID of the changed worker (type of changes see ReportWorkerChange())
    BOOL ReportProgressChange;  // TRUE = the worker status information was updated, the progress in the operation dialog will change (see ReportWorkerChange())
    int ReportChangeInItemUID;  // -3 = no changes (the operation dialog needs to be posted WM_APP_ITEMCHANGEREP), -2 = one change (stored in ReportChangeInItemUID2), -1 = change in more than two items, otherwise the UID of the second changed item (type of changes see ReportItemChange()) - the first changed item is in ReportChangeInItemUID2
    int ReportChangeInItemUID2; // UID of the first changed item (validity see ReportChangeInItemUID)

    BOOL OperStateChangedPosted;           // TRUE = we have already posted WM_APP_OPERSTATECHANGE to the operation dialog, waiting for it to react (call GetOperationState(TRUE))
    COperationState LastReportedOperState; // last operation state returned by GetOperationState(TRUE)

    DWORD OperationStart; // GetTickCount() from the moment the operation was started (when interrupted and restarted it shifts by the "idle" time so the total elapsed time fits)
    DWORD OperationEnd;   // GetTickCount() from the moment the operation finished (even with errors) (-1 = invalid - operation still running)

    CFTPOperationType Type; // operation type
    char* OperationSubject; // what the operation works with ("file "test.txt"", "3 files and 1 directory", etc.)

    int ChildItemsNotDone;  // number of unfinished "child" items (except type sqisDone)
    int ChildItemsSkipped;  // number of skipped "child" items (type sqisSkipped)
    int ChildItemsFailed;   // number of failed "child" items (types sqisFailed and sqisForcedToFail)
    int ChildItemsUINeeded; // number of user-input-needed "child" items (types sqisFailed and sqisForcedToFail)

    char* SourcePath;                 // operation source path (full path, possibly including fs-name)
    char SrcPathSeparator;            // most frequently used source path separator ('/', '.', etc.)
    BOOL SrcPathCanChange;            // TRUE if changes on the source path should be reported after the operation finishes
    BOOL SrcPathCanChangeInclSubdirs; // if SrcPathCanChange is TRUE, this stores whether the changes also cover subdirectories of the given path

    DWORD LastErrorOccurenceTime; // "time" when the last error occurred (initialized to -1)

    // ****************************************************************************
    // for Change Attributes (+ has another section):
    // ****************************************************************************

    WORD AttrAnd; // AND mask for calculating desired attributes (modes) of files/directories (new_attr = (cur_attr & AttrAnd) | AttrOr)
    WORD AttrOr;  // OR mask for calculating desired attributes (modes) of files/directories (new_attr = (cur_attr & AttrAnd) | AttrOr)

    // ****************************************************************************
    // for Copy and Move (download and upload):
    // ****************************************************************************

    char* TargetPath;                 // operation target path (full path, possibly including fs-name)
    char TgtPathSeparator;            // most frequently used target path separator ('/', '.', etc.)
    BOOL TgtPathCanChange;            // TRUE if changes on the target path should be reported after the operation finishes
    BOOL TgtPathCanChangeInclSubdirs; // if TgtPathCanChange is TRUE, this stores whether the changes also cover subdirectories of the given path

    CSalamanderMaskGroup* ASCIIFileMasks; // for automatic file transfer mode - masks for the "ascii" mode (others will be "binary") (NULL = unknown)

    CQuadWord TotalSizeInBytes; // sum of sizes of copied/moved files for which the size in bytes is known
    // valid only for download:
    CQuadWord TotalSizeInBlocks; // sum of sizes of copied/moved files for which the size in blocks is known

    // valid only for download:
    // data for estimating the block size (for VMS, MVS and other servers that use
    // blocks - ignored elsewhere)
    CQuadWord BlkSizeTotalInBytes;  // total size of files obtained so far in bytes (we take only files with a size of at least two blocks)
    CQuadWord BlkSizeTotalInBlocks; // total size of files obtained so far in blocks (we take only files with a size of at least two blocks)
    DWORD BlkSizeActualValue;       // current value of the real block size (for progress estimation) (-1 = unknown)

    BOOL ResumeIsNotSupported;         // TRUE = the FTP REST/APPE command returns a permanent error (e.g. "not implemented")
    BOOL DataConWasOpenedForAppendCmd; // TRUE = using the FTP APPE command led to opening the data connection with the server (append is 99.9% functional/implemented)

    unsigned AutodetectTrMode : 1;     // TRUE/FALSE = autodetect according to ASCIIFileMasks / choose based on UseAsciiTransferMode
    unsigned UseAsciiTransferMode : 1; // TRUE/FALSE = ASCII/Binary transfer mode (used only when AutodetectTrMode == FALSE)

    // valid only for download:
    // user-preferred ways of resolving the following problems
    unsigned CannotCreateFile : 2;      // see constants CANNOTCREATENAME_XXX
    unsigned CannotCreateDir : 2;       // see constants CANNOTCREATENAME_XXX
    unsigned FileAlreadyExists : 3;     // see constants FILEALREADYEXISTS_XXX
    unsigned DirAlreadyExists : 2;      // see constants DIRALREADYEXISTS_XXX
    unsigned RetryOnCreatedFile : 3;    // see constants RETRYONCREATFILE_XXX
    unsigned RetryOnResumedFile : 3;    // see constants RETRYONRESUMFILE_XXX
    unsigned AsciiTrModeButBinFile : 2; // see constants ASCIITRFORBINFILE_XXX

    // valid only for upload:
    // user-preferred ways of resolving the following problems
    unsigned UploadCannotCreateFile : 2;      // see constants CANNOTCREATENAME_XXX
    unsigned UploadCannotCreateDir : 2;       // see constants CANNOTCREATENAME_XXX
    unsigned UploadFileAlreadyExists : 3;     // see constants FILEALREADYEXISTS_XXX
    unsigned UploadDirAlreadyExists : 2;      // see constants DIRALREADYEXISTS_XXX
    unsigned UploadRetryOnCreatedFile : 3;    // see constants RETRYONCREATFILE_XXX
    unsigned UploadRetryOnResumedFile : 3;    // see constants RETRYONRESUMFILE_XXX
    unsigned UploadAsciiTrModeButBinFile : 2; // see constants ASCIITRFORBINFILE_XXX

    // ****************************************************************************
    // for Delete:
    // ****************************************************************************

    // user-preferred behaviour in the following situations
    unsigned ConfirmDelOnNonEmptyDir : 2; // see constants NONEMPTYDIRDEL_XXX
    unsigned ConfirmDelOnHiddenFile : 2;  // see constants HIDDENFILEDEL_XXX
    unsigned ConfirmDelOnHiddenDir : 2;   // see constants HIDDENDIRDEL_XXX

    // ****************************************************************************
    // more for Change Attributes:
    // ****************************************************************************

    unsigned ChAttrOfFiles : 1; // TRUE/FALSE = change/do not change file attributes
    unsigned ChAttrOfDirs : 1;  // TRUE/FALSE = change/do not change directory attributes

    // user-preferred ways of resolving the following problems
    unsigned UnknownAttrs : 2; // see constants UNKNOWNATTRS_XXX

    // data without needing a critical section (have their own synchronization + used only in shorter-lived objects):
    // global speed meter of the operation (measuring the total data-connection speed of all workers)
    CTransferSpeedMeter GlobalTransferSpeedMeter;

    // data without needing a critical section (have their own synchronization + used only in shorter-lived objects):
    // global object for storing the time of the last activity on the data-connection group (all
    // data-connection workers of the FTP operation)
    CSynchronizedDWORD GlobalLastActivityTime;

public:
    CFTPOperation();
    ~CFTPOperation();

    // gets the UID (in the critical section)
    int GetUID();

    // sets the UID (in the critical section)
    void SetUID(int uid);

    // sets the FTP server connection data - used by all operations; returns TRUE on success
    // WARNING: does not use the critical section for accessing data (can be called only before
    //          adding the operation to FTPOperationsList) + must not be called repeatedly (expects
    //          initialized attribute values of the object)
    BOOL SetConnection(CFTPProxyServer* proxyServer, const char* host, unsigned short port,
                       const char* user, const char* password, const char* account,
                       const char* initFTPCommands, BOOL usePassiveMode,
                       const char* listCommand, DWORD serverIP,
                       const char* serverSystem, const char* serverFirstReply,
                       BOOL useListingsCache, DWORD hostIP);

    // sets the basic operation data - used by all operations
    // WARNING: does not use the critical section for accessing data (can be called only before
    //          adding the operation to FTPOperationsList) + must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetBasicData(char* operationSubject, const char* listingServerType);

    // configures this object for the Delete operation (used only after calling SetConnection() and
    // SetBasicData())
    // WARNING: does not use the critical section for accessing data (can be called only before
    //          adding the operation to FTPOperationsList) + must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetOperationDelete(const char* sourcePath, char srcPathSeparator,
                            BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                            int confirmDelOnNonEmptyDir, int confirmDelOnHiddenFile,
                            int confirmDelOnHiddenDir);

    // configures this object for the Copy/Move-from-server operation (used only after calling
    // SetConnection() and SetBasicData()); returns TRUE on success
    // WARNING: does not use the critical section for accessing data (can be called only before
    //          adding the operation to FTPOperationsList) + must not be called repeatedly (expects
    //          initialized attribute values of the object)
    BOOL SetOperationCopyMoveDownload(BOOL isCopy, const char* sourcePath, char srcPathSeparator,
                                      BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                      const char* targetPath, char tgtPathSeparator,
                                      BOOL tgtPathCanChange, BOOL tgtPathCanChangeInclSubdirs,
                                      const char* asciiFileMasks, int autodetectTrMode,
                                      int useAsciiTransferMode, int cannotCreateFile, int cannotCreateDir,
                                      int fileAlreadyExists, int dirAlreadyExists, int retryOnCreatedFile,
                                      int retryOnResumedFile, int asciiTrModeButBinFile);

    // configures this object for the Change Attributes operation (used only after calling SetConnection()
    // and SetBasicData())
    // WARNING: does not use the critical section for accessing data (can be called only before
    //          adding the operation to FTPOperationsList) + must not be called repeatedly (expects
    //          initialized attribute values of the object)
    void SetOperationChAttr(const char* sourcePath, char srcPathSeparator,
                            BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                            WORD attrAnd, WORD attrOr, int chAttrOfFiles, int chAttrOfDirs,
                            int unknownAttrs);

    // configures this object for the Copy/Move-to-server operation (used only after calling
    // SetConnection() and SetBasicData()); returns TRUE on success
    // WARNING: does not use the critical section for accessing data (can be called only before
    //          adding the operation to FTPOperationsList) + must not be called repeatedly (expects
    //          initialized attribute values of the object)
    BOOL SetOperationCopyMoveUpload(BOOL isCopy, const char* sourcePath, char srcPathSeparator,
                                    BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                    const char* targetPath, char tgtPathSeparator,
                                    BOOL tgtPathCanChange, BOOL tgtPathCanChangeInclSubdirs,
                                    const char* asciiFileMasks, int autodetectTrMode,
                                    int useAsciiTransferMode, int uploadCannotCreateFile,
                                    int uploadCannotCreateDir, int uploadFileAlreadyExists,
                                    int uploadDirAlreadyExists, int uploadRetryOnCreatedFile,
                                    int uploadRetryOnResumedFile, int uploadAsciiTrModeButBinFile);

    // sets Queue (in the critical section)
    void SetQueue(CFTPQueue* queue);

    // returns the Queue value (in the critical section)
    CFTPQueue* GetQueue();

    // allocates a new worker (passes it the operation, queue, host+user+port); returns NULL if
    // memory is insufficient
    CFTPWorker* AllocNewWorker();

    // sends the operation header (operation description) to the log with UID 'logUID'
    void SendHeaderToLog(int logUID);

    // sets ChildItemsNotDone+ChildItemsSkipped+ChildItemsFailed+ChildItemsUINeeded (in the critical section)
    void SetChildItems(int notDone, int skipped, int failed, int uiNeeded);

    // adds the given counts to the ChildItemsNotDone+ChildItemsSkipped+ChildItemsFailed+ChildItemsUINeeded counters
    // (in the critical section); it may also change the operation state (running / finished / did not finish
    // due to errors+skips); 'onlyUINeededOrFailedToSkipped' is TRUE if this is just a state change
    // from sqisUserInputNeeded/sqisFailed to sqisSkipped (this change definitely does not require a listing refresh - the operation
    // remains in the "finished" state without needing a refresh)
    void AddToNotDoneSkippedFailed(int notDone, int skipped, int failed, int uiNeeded,
                                   BOOL onlyUINeededOrFailedToSkipped);

    // determines whether the name matches the aggregate ASCIIFileMasks mask; 'name'+'ext' are pointers
    // to the name and extension (or the end of the name), both placed in a single buffer; returns TRUE if
    // the name matches the aggregate mask
    BOOL IsASCIIFile(const char* name, const char* ext);

    // adds/subtracts 'size' to/from 'TotalSizeInBytes' (if 'sizeInBytes' is TRUE) or to/from 'TotalSizeInBlocks'
    // (if 'sizeInBytes' is FALSE); the total size of the operation is the sum of the sizes in bytes
    // and in blocks (one operation can theoretically have sizes in both bytes and blocks - each
    // "directory" can have a different listing format, e.g. MVS)
    void AddToTotalSize(const CQuadWord& size, BOOL sizeInBytes);
    void SubFromTotalSize(const CQuadWord& size, BOOL sizeInBytes);

    // sets OperationDlg (in the critical section)
    void SetOperationDlg(COperationDlg* operDlg);

    // activates or opens the operation dialog (see OperationDlg); returns success of the operation
    BOOL ActivateOperationDlg(HWND dropTargetWnd);

    // closes the operation dialog (if open) and returns the handle of the thread in which the
    // last dialog was open (NULL if the dialog was never open)
    void CloseOperationDlg(HANDLE* dlgThread);

    // called by the operation dialog from WM_INITDIALOG; 'dlg' is the dialog (the value in OperationDlg may not
    // be valid if CloseOperationDlg() has already been called); returns success (FALSE =
    // fatal error -> close the dialog)
    BOOL InitOperDlg(COperationDlg* dlg);

    // adds a new worker to WorkersList; returns TRUE on success
    BOOL AddWorker(CFTPWorker* newWorker);

    // called when the operation may have been paused, resumed, or stopped:
    // after adding a worker or after resuming a worker, after stopping a worker or pausing
    // a worker, or after sending "should-stop"; if the operation was resumed: resets
    // the speed meter and starts measuring the total operation time; if the operation was paused
    // or stopped (only waiting for the worker to finish): stops measuring the total
    // operation time
    void OperationStatusMaybeChanged();

    // see CFTPWorkersList::InformWorkersAboutStop
    BOOL InformWorkersAboutStop(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // see CFTPWorkersList::InformWorkersAboutPause
    BOOL InformWorkersAboutPause(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims, BOOL pause);

    // see CFTPWorkersList::CanCloseWorkers
    BOOL CanCloseWorkers(int workerInd);

    // see CFTPWorkersList::ForceCloseWorkers
    BOOL ForceCloseWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // see CFTPWorkersList::DeleteWorkers
    BOOL DeleteWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims,
                       CUploadWaitingWorker** uploadFirstWaitingWorker);

    // returns TRUE if the IP address of the FTP/proxy server is known (returned in 'serverIP'; 'host'+'hostBufSize'
    // are ignored in this case); returns FALSE if the server IP address is not known yet; in that
    // case it returns the hostname of the FTP/proxy server in the 'host' buffer of size 'hostBufSize'
    BOOL GetServerAddress(DWORD* serverIP, char* host, int hostBufSize);

    // sets 'ServerIP' (in the critical section)
    void SetServerIP(DWORD serverIP);

    // returns the FTP/proxy server IP in 'serverIP', the FTP/proxy server port in 'port', the hostname
    // of the FTP server in the 'host' buffer (minimum size HOST_MAX_SIZE), the proxy server type in 'proxyType',
    // the FTP server IP in 'hostIP' (used only for SOCKS4, otherwise INADDR_NONE), the FTP server port
    // in 'hostPort'; username and password for the proxy server in 'proxyUser' (minimum size USER_MAX_SIZE)
    // and 'proxyPassword' (minimum size PASSWORD_MAX_SIZE)
    void GetConnectInfo(DWORD* serverIP, unsigned short* port, char* host,
                        CFTPProxyServerType* proxyType, DWORD* hostIP, unsigned short* hostPort,
                        char* proxyUser, char* proxyPassword);

    // stores the log message in the 'buf' buffer (of size 'bufSize')
    void GetConnectLogMsg(BOOL isReconnect, char* buf, int bufSize, int attemptNumber, const char* dateTime);

    // if ServerSystem == NULL, sets ServerSystem to the string 'reply'
    // of length 'replySize'
    void SetServerSystem(const char* reply, int replySize);

    // if ServerFirstReply == NULL, sets ServerFirstReply to the string 'reply'
    // of length 'replySize'
    void SetServerFirstReply(const char* reply, int replySize);

    // prepares the text of the next command from the proxy script for the server and for the Log (wraps
    // ProcessProxyScript); 'errDescrBuf' is a 300-character buffer for the error description; return values:
    // - script error: the function returns FALSE + the error position is returned in '*proxyScriptExecPoint' + the
    //   error description is in 'errDescrBuf'
    // - missing variable value: the function returns TRUE and also TRUE in 'needUserInput'; the description
    //   of the missing variable is in 'errDescrBuf' (the value of '*proxyScriptExecPoint' does not change)
    // - successful determination of which command to send to the server: returns TRUE and 'buf' contains the command (including
    //   CRLF at the end), 'logBuf' contains the text for the log (the password is replaced with the word "(hidden)");
    //   '*proxyScriptExecPoint' points to the start of the next script line
    // - end of script: returns TRUE and 'buf' is an empty string, '*proxyScriptExecPoint' points
    //   to the end of the script
    BOOL PrepareNextScriptCmd(char* buf, int bufSize, char* logBuf, int logBufSize, int* cmdLen,
                              const char** proxyScriptExecPoint, int proxyScriptLastCmdReply,
                              char* errDescrBuf, BOOL* needUserInput);

    // creates the CFTPProxyForDataCon structure (is NULL if 'ProxyServer' == NULL); returns FALSE if
    // memory is insufficient
    BOOL AllocProxyForDataCon(CFTPProxyForDataCon** newDataConProxyServer);

    // returns the value of RetryLoginWithoutAsking (in the critical section)
    BOOL GetRetryLoginWithoutAsking();

    // returns the contents of the InitFTPCommands string in 'buf' of size 'bufSize' (in the critical section)
    void GetInitFTPCommands(char* buf, int bufSize);

    // gets info for the Login Error dialog (opened via the "Solve Error" button in the Connections listview)
    void GetLoginErrorDlgInfo(char* user, int userBufSize, char* password, int passwordBufSize,
                              char* account, int accountBufSize, BOOL* retryLoginWithoutAsking,
                              BOOL* proxyUsed, char* proxyUser, int proxyUserBufSize,
                              char* proxyPassword, int proxyPasswordBufSize);

    // stores new values from the Login Error dialog (opened via the "Solve Error" button in the Connections listview)
    void SetLoginErrorDlgInfo(const char* password, const char* account, BOOL retryLoginWithoutAsking,
                              BOOL proxyUsed, const char* proxyUser, const char* proxyPassword);

    // called to report a change in a worker (only changes that affect the worker display in the Connections listview
    // in the operation dialog are reported - the worker variables are ShouldStop, State and CurItem);
    // 'reportProgressChange' is TRUE if the worker change is associated with a change of progress (status)
    void ReportWorkerChange(int workerID, BOOL reportProgressChange);

    // called by the operation dialog after a change is reported via ReportWorkerChange(); returns the ID of the changed worker
    // or -1 if more than one changed; in 'reportProgressChange' (if not NULL) returns TRUE if
    // it was a worker change associated with a progress (status) change
    int GetChangedWorker(BOOL* reportProgressChange);

    // just calls CFTPWorkersList::PostNewWorkAvailable()
    // WARNING: enters the CSocketsThread::CritSect section !!!
    void PostNewWorkAvailable(BOOL onlyOneItem);

    // just calls CFTPWorkersList::GiveWorkToSleepingConWorker()
    // WARNING: enters the CSocketsThread::CritSect section !!!
    BOOL GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker);

    // called to report an item change (only changes that affect the item display in the Operations listview
    // in the operation dialog are reported - the item variables are Name, Path, Type, State, Size,
    // SizeInBytes, TgtName, TgtPath, AsciiTransferMode, Attr, ProblemID, WinError, ErrAllocDescr, OrigRights);
    // if 'itemUID' is -1, changes in multiple items are reported (all items are redrawn)
    void ReportItemChange(int itemUID);

    // called by the operation dialog after a change is reported via ReportItemChange(); returns the UIDs of the changed items
    // (for a single change 'secondUID' is -1) or the pair (-1, -1) if more than two items changed
    void GetChangedItems(int* firstUID, int* secondUID);

    // fills in the default behaviour values for disk operations defined for the operation
    void GetDiskOperDefaults(CFTPDiskWork* diskWork);

    // returns CannotCreateDir (in the critical section)
    int GetCannotCreateDir();

    // returns DirAlreadyExists (in the critical section)
    int GetDirAlreadyExists();

    // returns CannotCreateFile (in the critical section)
    int GetCannotCreateFile();

    // returns FileAlreadyExists (in the critical section)
    int GetFileAlreadyExists();

    // returns RetryOnCreatedFile (in the critical section)
    int GetRetryOnCreatedFile();

    // returns RetryOnResumedFile (in the critical section)
    int GetRetryOnResumedFile();

    // returns AsciiTrModeButBinFile (in the critical section)
    int GetAsciiTrModeButBinFile();

    // returns UploadCannotCreateDir (in the critical section)
    int GetUploadCannotCreateDir();

    // returns UploadDirAlreadyExists (in the critical section)
    int GetUploadDirAlreadyExists();

    // returns UploadCannotCreateFile (in the critical section)
    int GetUploadCannotCreateFile();

    // returns UploadFileAlreadyExists (in the critical section)
    int GetUploadFileAlreadyExists();

    // returns UploadRetryOnCreatedFile (in the critical section)
    int GetUploadRetryOnCreatedFile();

    // returns UploadRetryOnResumedFile (in the critical section)
    int GetUploadRetryOnResumedFile();

    // returns UploadAsciiTrModeButBinFile (in the critical section)
    int GetUploadAsciiTrModeButBinFile();

    // returns UnknownAttrs (in the critical section)
    int GetUnknownAttrs();

    // returns ResumeIsNotSupported (in the critical section)
    BOOL GetResumeIsNotSupported();

    // returns DataConWasOpenedForAppendCmd (in the critical section)
    BOOL GetDataConWasOpenedForAppendCmd();

    BOOL GetEncryptControlConnection() { return EncryptControlConnection; }
    BOOL GetEncryptDataConnection() { return EncryptDataConnection; }
    int GetCompressData() { return CompressData; }
    CCertificate* GetCertificate(); // WARNING: returns the certificate only after calling its AddRef(), so the caller is responsible for releasing it by calling Release()

    void SetEncryptDataConnection(BOOL encryptDataConnection) { EncryptDataConnection = encryptDataConnection; }
    void SetEncryptControlConnection(BOOL encryptControlConnection) { EncryptControlConnection = encryptControlConnection; }
    void SetCompressData(int compressData) { CompressData = compressData; }
    void SetCertificate(CCertificate* certificate);

    // sets CannotCreateDir (in the critical section)
    void SetCannotCreateDir(int value);

    // sets DirAlreadyExists (in the critical section)
    void SetDirAlreadyExists(int value);

    // sets CannotCreateFile (in the critical section)
    void SetCannotCreateFile(int value);

    // sets FileAlreadyExists (in the critical section)
    void SetFileAlreadyExists(int value);

    // sets RetryOnCreatedFile (in the critical section)
    void SetRetryOnCreatedFile(int value);

    // sets RetryOnResumedFile (in the critical section)
    void SetRetryOnResumedFile(int value);

    // sets AsciiTrModeButBinFile (in the critical section)
    void SetAsciiTrModeButBinFile(int value);

    // sets UploadCannotCreateDir (in the critical section)
    void SetUploadCannotCreateDir(int value);

    // sets UploadDirAlreadyExists (in the critical section)
    void SetUploadDirAlreadyExists(int value);

    // sets UploadCannotCreateFile (in the critical section)
    void SetUploadCannotCreateFile(int value);

    // sets UploadFileAlreadyExists (in the critical section)
    void SetUploadFileAlreadyExists(int value);

    // sets UploadRetryOnCreatedFile (in the critical section)
    void SetUploadRetryOnCreatedFile(int value);

    // sets UploadRetryOnResumedFile (in the critical section)
    void SetUploadRetryOnResumedFile(int value);

    // sets UploadAsciiTrModeButBinFile (in the critical section)
    void SetUploadAsciiTrModeButBinFile(int value);

    // sets UnknownAttrs (in the critical section)
    void SetUnknownAttrs(int value);

    // sets ResumeIsNotSupported (in the critical section)
    void SetResumeIsNotSupported(BOOL value);

    // sets DataConWasOpenedForAppendCmd (in the critical section)
    void SetDataConWasOpenedForAppendCmd(BOOL value);

    // determines the path type on the FTP server (calls ::GetFTPServerPathType() in the critical section
    // with parameters 'ServerSystem' and 'ServerFirstReply')
    CFTPServerPathType GetFTPServerPathType(const char* path);

    // determines whether 'ServerSystem' contains the name 'systemName'
    BOOL IsServerSystem(const char* systemName);

    // returns TRUE if the path 'path' is already in the list of explored paths (see 'ExploredPaths');
    // WARNING: assumes that 'path' is a path returned by the server, so paths
    // are compared only as case-sensitive strings (slashes/backslashes/dots, etc., are not trimmed)
    // - a cycle may be detected only on its second pass, which is sufficient for our purposes
    BOOL IsAlreadyExploredPath(const char* path);

    // stores the path 'path' in the list of explored paths (see 'ExploredPaths');
    // returns FALSE if 'path' is already in this list (out of memory is ignored in this
    // function)
    // WARNING: assumes that 'path' is a path returned by the server, so paths
    // are compared only as case-sensitive strings (slashes/backslashes/dots, etc., are not trimmed)
    // - a cycle may be detected only on its second pass, which is sufficient for our purposes
    BOOL AddToExploredPaths(const char* path);

    // returns UsePassiveMode (in the critical section)
    BOOL GetUsePassiveMode();

    // sets UsePassiveMode (in the critical section)
    void SetUsePassiveMode(BOOL usePassiveMode);

    // returns SizeCmdIsSupported (in the critical section)
    BOOL GetSizeCmdIsSupported();

    // sets SizeCmdIsSupported (in the critical section)
    void SetSizeCmdIsSupported(BOOL sizeCmdIsSupported);

    // returns the listing command (if ListCommand == NULL, returns "LIST\r\n"), the command already has
    // CRLF at the end; 'buf' (must not be NULL) is a buffer of size 'bufSize'
    void GetListCommand(char* buf, int bufSize);

    // returns UseListingsCache (in the critical section)
    BOOL GetUseListingsCache();

    // returns User (in the critical section); if User == NULL, FTP_ANONYMOUS is returned
    void GetUser(char* buf, int bufSize);

    // returns (in the critical section) an allocated string with the server's reply to the SYST command
    // (attribute 'ServerSystemReply'); returns NULL if allocation fails
    char* AllocServerSystemReply();

    // returns (in the critical section) an allocated string with the first reply from the server
    // (attribute 'ServerFirstReply'); returns NULL if allocation fails
    char* AllocServerFirstReply();

    // returns (in the critical section) the server type used for parsing listings: returns FALSE if
    // the server type should be autodetected (returns an empty string in 'buf'); if it returns TRUE,
    // 'buf' (with size at least SERVERTYPE_MAX_SIZE) contains the requested server type name
    BOOL GetListingServerType(char* buf);

    // returns (in the critical section) the transfer mode reconstructed from AutodetectTrMode
    // and UseAsciiTransferMode
    int GetTransferMode();

    // returns (in the critical section) the parameters of the Change Attributes operation
    void GetParamsForChAttrsOper(BOOL* selFiles, BOOL* selDirs, BOOL* includeSubdirs,
                                 DWORD* attrAndMask, DWORD* attrOrMask,
                                 int* operationsUnknownAttrs);

    // increases the counters in the item-dir item 'itemDir' (the item must be a descendant of CFTPQueueItemDir)
    // or in this operation (if 'itemDir' is -1); may change the state of the item-dir item or the operation;
    // if it changes the state of the item-dir item, it updates the counters in its parent (works recursively,
    // so it ensures all necessary state and counter changes); 'onlyUINeededOrFailedToSkipped'
    // is TRUE if it is only a state change from sqisUserInputNeeded/sqisFailed to sqisSkipped
    // (this change definitely does not require a listing refresh - the operation remains in the "finished" state
    // without needing a refresh)
    // WARNING: operates in the CFTPOperation::OperCritSect and CFTPQueue::QueueCritSect critical sections
    void AddToItemOrOperationCounters(int itemDir, int childItemsNotDone,
                                      int childItemsSkipped, int childItemsFailed,
                                      int childItemsUINeeded, BOOL onlyUINeededOrFailedToSkipped);

    // returns (in the critical section) the parameters of the Delete operation; ignores NULL parameters
    void GetParamsForDeleteOper(int* confirmDelOnNonEmptyDir, int* confirmDelOnHiddenFile,
                                int* confirmDelOnHiddenDir);

    // sets (in the critical section) the parameters of the Delete operation; ignores (does not set) NULL parameters
    void SetParamsForDeleteOper(int* confirmDelOnNonEmptyDir, int* confirmDelOnHiddenFile,
                                int* confirmDelOnHiddenDir);

    // called to report an operation state change
    void ReportOperationStateChange();

    // obtains the operation state; if 'calledFromSetupCloseButton' is TRUE, it was called from the
    // SetupCloseButton method (from the operation dialog), responding to a posted notification about a change
    // in the operation state - in this case posting another notification is allowed
    COperationState GetOperationState(BOOL calledFromSetupCloseButton);

    // debug only: returns the counter states (in the critical section)
    void DebugGetCounters(int* childItemsNotDone, int* childItemsSkipped,
                          int* childItemsFailed, int* childItemsUINeeded);

    // sends Salamander messages about changes on paths (source and possibly target);
    // called only after the operation finishes or is interrupted to avoid "unnecessary"
    // refreshes (would slow down the operation); 'softRefresh' is TRUE if the worker connections
    // should not be closed (connections are not returned to the panel, so we let the refresh happen
    // only when the user switches back to the Salamander main window - an immediate refresh would
    // necessarily trigger a reconnect prompt, which would be annoying)
    void PostChangeOnPathNotifications(BOOL softRefresh);

    // returns the operation's global speed meter (measuring the total data-connection speed of all workers)
    CTransferSpeedMeter* GetGlobalTransferSpeedMeter() { return &GlobalTransferSpeedMeter; }

    // returns the global object for storing the time of the last activity on the data connections of all workers
    CSynchronizedDWORD* GetGlobalLastActivityTime() { return &GlobalLastActivityTime; }

    // returns (in the critical section) the operation type (see 'Type')
    CFTPOperationType GetOperationType();

    // returns progress based on the sum of sizes of completed and skipped files in bytes and
    // the total size in bytes (otherwise returns -1); in 'unknownSizeCount' returns the number of
    // unfinished operations with an unknown size in bytes; in 'errorsCount' returns the number of
    // items with an error or query; in 'waiting' returns the sum of sizes (in bytes)
    // of items waiting to be processed (waiting+delayed+processing)
    int GetCopyProgress(CQuadWord* downloaded, CQuadWord* total, CQuadWord* waiting,
                        int* unknownSizeCount, int* errorsCount, int* doneOrSkippedCount,
                        int* totalCount, CFTPQueue* queue);

    // returns progress based on the sum of sizes of completed and skipped files in bytes and
    // the total size in bytes (otherwise returns -1); in 'unknownSizeCount' returns the number of
    // unfinished operations with an unknown size in bytes; in 'errorsCount' returns the number of
    // items with an error or query; in 'waiting' returns the sum of sizes (in bytes)
    // of items waiting to be processed (waiting+delayed+processing)
    int GetCopyUploadProgress(CQuadWord* uploaded, CQuadWord* total, CQuadWord* waiting,
                              int* unknownSizeCount, int* errorsCount, int* doneOrSkippedCount,
                              int* totalCount, CFTPQueue* queue);

    // adds a matching pair of sizes in bytes and blocks; the data is used to calculate
    // the approximate block size (for VMS, MVS and other servers using blocks it is used to
    // estimate the total download size in bytes based on the known size in blocks)
    void AddBlkSizeInfo(CQuadWord const& sizeInBytes, CQuadWord const& sizeInBlocks);

    // calculates the approximate size in bytes based on the size in blocks; returns FALSE if
    // the block size is not yet known and therefore the conversion to bytes cannot be performed
    BOOL GetApproxByteSize(CQuadWord* sizeInBytes, CQuadWord const& sizeInBlocks);

    // returns TRUE if the approximate block size in bytes is known
    BOOL IsBlkSizeKnown();

    // returns how long the operation has been running/ran
    DWORD GetElapsedSeconds();

    // returns TRUE if there was any activity on worker data connections during the last WORKER_STATUSUPDATETIMEOUT milliseconds (applies to listing and download)
    BOOL GetDataActivityInLastPeriod();

    // returns TargetPath in 'buf' of size 'bufSize'
    void GetTargetPath(char* buf, int bufSize);

    // increments LastErrorOccurenceTime by one and returns the new LastErrorOccurenceTime value
    DWORD GiveLastErrorOccurenceTime();

    // searches for the index of a worker that needs to open the Solve Error dialog (a
    // "new" error appeared there (the user has not seen it yet)); returns TRUE if such a worker
    // was found, its index is returned in 'index'
    BOOL SearchWorkerWithNewError(int* index);

    // determines whether this operation can make changes on path 'path' of type 'pathType' on
    // the server 'user'+'host'+'port'; 'userLength' is zero if we do not know how long the username is
    // or if it does not contain "forbidden" characters, otherwise it is the expected
    // username length
    BOOL CanMakeChangesOnPath(const char* user, const char* host, unsigned short port,
                              const char* path, CFTPServerPathType pathType,
                              int userLength);

    // determines whether among the operations there is an upload to the server 'user'+'host'+'port';
    // 'user' is NULL for anonymous connections; 'userLength' is zero if we do not know how long the username is
    // or if it does not contain "forbidden" characters, otherwise it is the expected username length
    BOOL IsUploadingToServer(const char* user, const char* host, unsigned short port,
                             int userLength);

    // returns 'Host'+'User'+'Port'; 'host' (if not NULL) is a buffer for 'Host' of size
    // HOST_MAX_SIZE; 'user' (if not NULL) is a buffer for 'User' of size USER_MAX_SIZE;
    // 'port' (if not NULL) returns 'Port'
    void GetUserHostPort(char* user, char* host, unsigned short* port);
};

//
// ****************************************************************************
// CFTPOperationsList
//

enum CWorkerWaitSatisfiedReason // events monitored in CFTPOperationsList::WaitForFinishOrESC
{
    wwsrEsc,                 // ESC
    wwsrTimeout,             // timeout
    wwsrWorkerSocketClosure, // a worker's socket was closed
};

class CFTPOperationsList
{
protected:
    // critical section for accessing the object's data
    // WARNING: consult access to critical sections in the servers\critsect.txt file !!!
    CRITICAL_SECTION OpListCritSect;

    // list of existing operations; the operation UID is the index into this array,
    // so: ARRAY ELEMENTS MUST NOT BE SHIFTED (deletion is handled by writing NULL to the index)
    TIndirectArray<CFTPOperation> Operations;
    int FirstFreeIndexInOperations; // lowest free index inside the Operations array (-1 = none)

    BOOL CallOK; // helper variable for StartCall() and EndCall()

public:
    CFTPOperationsList();
    ~CFTPOperationsList();

    // returns TRUE if there is no operation in the list
    BOOL IsEmpty();

    // adds a new operation to the array (works with FirstFreeIndexInOperations); in 'newuid' (if not NULL)
    // returns the UID of the added operation; returns TRUE on success
    BOOL AddOperation(CFTPOperation* newOper, int* newuid);

    // closes all open operation dialogs and waits for all dialog threads to finish
    void CloseAllOperationDlgs();

    // stops and cancels selected workers of operations; 'parent' is the thread's "foreground" window (after
    // pressing ESC it is used to determine whether ESC was pressed in this window and not
    // for example in another application; in the main thread it is SalamanderGeneral->GetMsgBoxParent()
    // or a dialog opened by the plugin); if 'operUID' is -1, it works with all workers of all
    // operations; if 'operUID' is not -1 and 'workerInd' is -1, it works with all workers
    // of the operation with UID 'operUID'; if neither 'operUID' nor 'workerInd' is -1, it works with the worker
    // at index 'workerInd' of the operation with UID 'operUID'
    // WARNING: enters the CSocketsThread::CritSect and CSocket::SocketCritSect sections !!!
    void StopWorkers(HWND parent, int operUID, int workerInd);

    // pauses or resumes selected workers of operations; 'parent' is the thread's "foreground" window (after
    // pressing ESC it is used to determine whether ESC was pressed in this window and not
    // for example in another application; in the main thread it is SalamanderGeneral->GetMsgBoxParent()
    // or a dialog opened by the plugin); if 'operUID' is -1, it works with all workers of all
    // operations; if 'operUID' is not -1 and 'workerInd' is -1, it works with all workers
    // of the operation with UID 'operUID'; if neither 'operUID' nor 'workerInd' is -1, it works with the worker
    // at index 'workerInd' of the operation with UID 'operUID'; if 'pause' is TRUE the selected
    // workers should pause, otherwise they should resume
    // WARNING: enters the CSocketsThread::CritSect and CSocket::SocketCritSect sections !!!
    void PauseWorkers(HWND parent, int operUID, int workerInd, BOOL pause);

    // removes the operation with UID 'uid' from the array (updates FirstFreeIndexInOperations);
    // the operation dialog must be closed in advance and all workers of the operation must be cancelled;
    // if 'doNotPostChangeOnPathNotifications' is TRUE, the method does not call
    // PostChangeOnPathNotifications (FALSE only in case of canceling the operation);
    // WARNING: also enters the CUploadListingCache::UploadLstCacheCritSect section !!!
    void DeleteOperation(int uid, BOOL doNotPostChangeOnPathNotifications);

    // determines whether any of the operations can make changes on path 'path' of type 'pathType' on
    // the server 'user'+'host'+'port'; if 'ignoreOperUID' is not -1, it is the UID of an operation that
    // should be ignored (an upload operation we have just created)
    BOOL CanMakeChangesOnPath(const char* user, const char* host, unsigned short port,
                              const char* path, CFTPServerPathType pathType, int ignoreOperUID);

    // determines whether there is an upload to the server 'user'+'host'+'port' among the operations
    BOOL IsUploadingToServer(const char* user, const char* host, unsigned short port);

    // ***************************************************************************************
    // helper methods for calling methods of CFTPOperation objects; the objects are identified
    // by their UIDs passed in the first parameter of the helper methods ('operUID'); the helper methods
    // return TRUE if the object's method was called ('operUID' is a valid operation UID)
    // ***************************************************************************************

    // helper methods to simplify implementing the following method section
    BOOL StartCall(int operUID);
    BOOL EndCall();

    BOOL ActivateOperationDlg(int operUID, BOOL& success, HWND dropTargetWnd);
    BOOL CloseOperationDlg(int operUID, HANDLE* dlgThread);

protected:
    // waits for some worker's socket to close (additionally: monitors ESC, has a timeout, and contains
    // a message loop); 'parent' is the thread's "foreground" window (after pressing ESC it is used to determine
    // whether ESC was pressed in this window and not for example in another application; in the main thread it is
    // SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'milliseconds' is
    // the time in ms to wait for the socket to close; after this time the method returns
    // 'reason' == wwsrTimeout; if 'milliseconds' is INFINITE, it waits without a time
    // limit; if 'waitWnd' is not NULL, it monitors pressing the close button in the 'waitWnd' wait window;
    // 'reason' returns why the method ended (one of wwsrXXX; wwsrEsc if the user pressed ESC);
    // if the Windows message WM_CLOSE arrives, 'postWM_CLOSE' is set to TRUE and the 'parent' window is activated
    // (so the user sees what they are waiting for); 'lastWorkerMayBeClosedState'
    // is a helper variable; initialize it to -1 before the loop and do not change it within the loop
    // may be called from any thread
    void WaitForFinishOrESC(HWND parent, int milliseconds, CWaitWindow* waitWnd,
                            CWorkerWaitSatisfiedReason& reason, BOOL& postWM_CLOSE,
                            int& lastWorkerMayBeClosedState);
};

//
// ****************************************************************************
// CUIDArray
//

class CUIDArray
{
protected:
    // critical section for accessing the object's data
    CRITICAL_SECTION UIDListCritSect;
    TDirectArray<DWORD> Data;

public:
    CUIDArray(int base, int delta);
    ~CUIDArray();

    // adds a UID to the array, returns success
    BOOL Add(int uid);

    // returns the first element from the array in 'uid'; returns success
    BOOL GetFirstUID(int& uid);
};

//
// ****************************************************************************
// CReturningConnections
//

struct CReturningConnectionData
{
    int ControlConUID;         // to which "control connection" in the panel we return the connection
    CFTPWorker* WorkerWithCon; // worker with the returned connection

    CReturningConnectionData(int controlConUID, CFTPWorker* workerWithCon)
    {
        ControlConUID = controlConUID;
        WorkerWithCon = workerWithCon;
    }
};

class CReturningConnections
{
protected:
    // critical section for accessing the object's data
    CRITICAL_SECTION RetConsCritSect;
    TIndirectArray<CReturningConnectionData> Data;

public:
    CReturningConnections(int base, int delta);
    ~CReturningConnections();

    // adds an element to the array, returns success
    BOOL Add(int controlConUID, CFTPWorker* workerWithCon);

    // returns the first element from the array (on error returns -1 and NULL); returns success
    BOOL GetFirstCon(int* controlConUID, CFTPWorker** workerWithCon);

    // cancels all workers that have not yet managed to hand the connection back to the panel (they are in the Data array)
    // call only without nesting into any critical sections (enters CSocketsThread::CritSect)
    void CloseData();
};

//
// ****************************************************************************
// CUploadListingCache
//
// cache of listings of paths on servers - used during upload to determine
// whether the target file/directory already exists

enum CUploadListingItemType
{
    ulitFile,      // file
    ulitDirectory, // directory
    ulitLink,      // link (to a directory or file)
};

#define UPLOADSIZE_UNKNOWN CQuadWord(-1, -1)    // unknown size (listing does not contain the size in bytes)
#define UPLOADSIZE_NEEDUPDATE CQuadWord(-2, -1) // to obtain the size a new listing is needed (e.g. during upload or after uploading in ASCII mode)

struct CUploadListingItem // data of a single file/directory/link in the listing
{
    // access to the object's data in the CUploadListingCache::UploadLstCacheCritSect critical section
    // NOTE: before adding to CUploadListingCache the access is without a critical section

    CUploadListingItemType ItemType; // file/directory/link
    char* Name;                      // item name
    CQuadWord ByteSize;              // for files only: item size in bytes, special values see UPLOADSIZE_XXX
};

enum CUploadListingChangeType
{
    ulctDelete,       // deleting a name
    ulctCreateDir,    // creating a directory
    ulctStoreFile,    // start uploading a file (may also overwrite a file/link)
    ulctFileUploaded, // upload finished (may also overwrite a file/link)
};

struct CUploadListingChange // change in the listing (so we do not fetch the listing after every change, we perform the expected listing change on the cached listing after each FTP command)
{
    CUploadListingChange* NextChange; // next change in the listing (used while downloading the listing - changes are applied after the listing is downloaded)
    CUploadListingChangeType Type;    // change type
    DWORD ChangeTime;                 // IncListingCounter() from the moment of the change (the moment the server reply to the listing-changing command was received)

    char* Name;         // ulctDelete: name of the deleted file/link/directory; ulctCreateDir: name of the created directory; ulctStoreFile+ulctFileUploaded: name of the uploaded file
    CQuadWord FileSize; // ulctFileUploaded: size of the uploaded file

    CUploadListingChange(DWORD changeTime, CUploadListingChangeType type, const char* name,
                         const CQuadWord* fileSize = NULL);
    ~CUploadListingChange(); // releases data, but WARNING: must not release NextChange
    BOOL IsGood() { return Name != NULL; }
};

struct CUploadWaitingWorker // list of workers waiting for a path listing to finish (or fail)
{
    CUploadWaitingWorker* NextWorker;
    int WorkerMsg;
    int WorkerUID;
};

enum CUploadListingState
{
    ulsReady,                      // listing is ready for use (from Salamander's perspective it is current)
    ulsInProgress,                 // some worker is currently downloading it from the server
    ulsInProgressButObsolete,      // some worker is currently downloading it from the server, but we already have a listing in the ulsReady state from another source (panel) (after ignoring the worker result the state switches to ulsReady)
    ulsInProgressButMayBeOutdated, // some worker is currently downloading it from the server, but we already know the listing may be outdated (an unknown change occurred in the directory or the connection broke during the change)
    ulsNotAccessible,              // the listing is "unobtainable" (e.g. the server reports "access denied" or the listing cannot be parsed)
};

struct CUploadPathListing // listing for a specific path on the server
{
public:
    // access to the object's data in the CUploadListingCache::UploadLstCacheCritSect critical section
    // NOTE: before adding to CUploadListingCache the access is without a critical section

    char* Path;                  // cached path (local on the server)
    CFTPServerPathType PathType; // type of the cached path

    CUploadListingState ListingState;         // listing state
    DWORD ListingStartTime;                   // IncListingCounter() from the moment the LIST command was sent to the server (listing started)
    CUploadListingChange* FirstChange;        // only in ulsInProgress: first change in the listing (changes are stored only while some worker is downloading the listing)
    CUploadListingChange* LastChange;         // only in ulsInProgress: last change in the listing (a new change is appended after this one)
    DWORD LatestChangeTime;                   // IncListingCounter() from the moment of the last listing change (used to check whether it is possible to update the listing with a new listing - only if LatestChangeTime is less than the start time of downloading the new listing)
    CUploadWaitingWorker* FirstWaitingWorker; // only in ulsInProgress* states: list of workers waiting for the path listing to finish (or fail)
    BOOL FromPanel;                           // TRUE = listing taken from the panel (may be outdated; if there is doubt about the listing freshness, refresh it)

    TIndirectArray<CUploadListingItem> ListingItem; // array of listing items

public:
    CUploadPathListing(const char* path, CFTPServerPathType pathType,
                       CUploadListingState listingState, DWORD listingStartTime,
                       BOOL fromPanel);
    ~CUploadPathListing();
    BOOL IsGood() { return Path != NULL; }

    // releases the listing stored in ListingItem
    void ClearListingItems();

    // releases the change list (see FirstChange, LastChange)
    void ClearListingChanges();

    // finds an item with CUploadListingItem::Name == 'name'; returns TRUE on success and 'index'
    // is the index of the found item; returns FALSE on failure and 'index' is the place where
    // a possible new item with CUploadListingItem::Name == 'name' should be inserted
    BOOL FindItem(const char* name, int& index);

    // parses the listing 'pathListing'+'pathListingLen'+'pathListingDate'; 'welcomeReply' (must not
    // be NULL) is the first server reply (often contains the FTP server version);
    // 'systReply' (must not be NULL) is the server system (reply to the SYST command);
    // 'suggestedListingServerType' is the server type used for parsing the listing: NULL = autodetect,
    // otherwise the server type name (without the optional leading '*'; if it stops existing, it switches
    // to autodetect); if 'lowMemory' is not NULL, it returns TRUE in it when memory is low;
    // returns TRUE if the entire listing was successfully parsed and the object is filled with new items
    BOOL ParseListing(const char* pathListing, int pathListingLen, const CFTPDate& pathListingDate,
                      CFTPServerPathType pathType, const char* welcomeReply, const char* systReply,
                      const char* suggestedListingServerType, BOOL* lowMemory);

    // reporting a change: creating a directory; 'newDir' is just a single directory name;
    // returns TRUE in 'lowMem' (must not be NULL) if memory is low (the listing becomes invalid);
    // returns TRUE in 'dirCreated' (must not be NULL) if the directory 'newDir'
    // did not exist in the listing and was successfully added
    // WARNING: do not call directly, called through CUploadListingCache::ReportCreateDirs()
    void ReportCreateDir(const char* newDir, BOOL* dirCreated, BOOL* lowMem);

    // see CUploadListingCache::ReportDelete(); returns TRUE in 'invalidateNameDir' (must not be NULL)
    // if the directory 'name' needs to be invalidated (it may be a directory or
    // a link to a directory); returns TRUE in 'lowMem' (must not be NULL) if there is not enough memory to
    // add the change to the change queue (only in the ulsInProgress state)
    // WARNING: do not call directly, called through CUploadListingCache::ReportDelete()
    void ReportDelete(const char* name, BOOL* invalidateNameDir, BOOL* lowMem);

    // see CUploadListingCache::ReportStoreFile(); returns TRUE in 'lowMem' (must not be NULL)
    // if memory is low (the listing becomes invalid)
    // WARNING: do not call directly, called through CUploadListingCache::ReportStoreFile()
    void ReportStoreFile(const char* name, BOOL* lowMem);

    // see CUploadListingCache::ReportFileUploaded(); returns TRUE in 'lowMem' (must not be NULL)
    // if there is not enough memory to add the change to the change queue (only in the
    // ulsInProgress state)
    // WARNING: do not call directly, called through CUploadListingCache::ReportFileUploaded()
    void ReportFileUploaded(const char* name, const CQuadWord& fileSize, BOOL* lowMem);

    // applies the change 'change' to the listing; returns success (failure = the entire listing must be invalidated)
    BOOL CommitChange(CUploadListingChange* change);

    // adds worker 'workerMsg'+'workerUID' to the queue of workers waiting for this path listing
    // to finish (or fail); returns FALSE only when memory is low
    BOOL AddWaitingWorker(int workerMsg, int workerUID);

    // to all workers from the FirstWaitingWorker list: if 'uploadFirstWaitingWorker' is NULL,
    // WORKER_TGTPATHLISTINGFINISHED messages are sent; otherwise 'uploadFirstWaitingWorker'
    // is the list where these workers should be added
    // WARNING: if 'uploadFirstWaitingWorker' is NULL, must be called inside CSocketsThread::CritSect!
    void InformWaitingWorkers(CUploadWaitingWorker** uploadFirstWaitingWorker);

private:
    // adds an item to the end of the array; WARNING: SortItems() must be called before searching the array
    BOOL AddItemDoNotSort(CUploadListingItemType itemType, const char* name, const CQuadWord& byteSize);

    // sorts the array by CUploadListingItem::Name
    void SortItems();

    // helper method for ParseListing()
    BOOL ParseListingToArray(const char* pathListing, int pathListingLen, const CFTPDate& pathListingDate,
                             CServerType* serverType, BOOL* lowMem, BOOL isVMS);

    // adds 'ch' to the list of changes
    void AddChange(CUploadListingChange* ch);

    // inserts an item at index 'index'
    BOOL InsertNewItem(int index, CUploadListingItemType itemType, const char* name,
                       const CQuadWord& byteSize);
};

struct CUploadListingsOnServer // listings of paths on a single server
{
public:
    // access to the object's data in the CUploadListingCache::UploadLstCacheCritSect critical section
    // NOTE: before adding to CUploadListingCache the access is without a critical section

    char* User;          // user name, NULL == anonymous
    char* Host;          // host address (must not be NULL)
    unsigned short Port; // port on which the FTP server runs

    TIndirectArray<CUploadPathListing> Listing; // array of path listings on the server

#ifdef _DEBUG
    static int FoundPathIndexesInCache; // how many requested paths were found in the cache
    static int FoundPathIndexesTotal;   // total number of searched paths
#endif

protected:
#define FOUND_PATH_IND_CACHE_SIZE 5 // must be at least 1
    int FoundPathIndexes[FOUND_PATH_IND_CACHE_SIZE];

public:
    CUploadListingsOnServer(const char* user, const char* host, unsigned short port);
    ~CUploadListingsOnServer();
    BOOL IsGood() { return Host != NULL; }

    // see CUploadListingCache::AddOrUpdateListing();
    // WARNING: do not call directly, called through CUploadListingCache::AddOrUpdateListing()
    BOOL AddOrUpdateListing(const char* path, CFTPServerPathType pathType,
                            const char* pathListing, int pathListingLen,
                            const CFTPDate& pathListingDate, DWORD listingStartTime,
                            BOOL onlyUpdate, const char* welcomeReply,
                            const char* systReply, const char* suggestedListingServerType);

    // see CUploadListingCache::RemoveNotAccessibleListings();
    void RemoveNotAccessibleListings();

    // see CUploadListingCache::InvalidatePathListing
    void InvalidatePathListing(const char* path, CFTPServerPathType pathType);

    // see CUploadListingCache::IsListingFromPanel
    BOOL IsListingFromPanel(const char* path, CFTPServerPathType pathType);

    // adds an empty listing for path 'path'+'dirName' of type 'pathType' with state 'listingState';
    // 'dirName' may also be NULL (a listing for the path 'path' is added); 'doNotCheckIfPathIsKnown'
    // is TRUE if we know the path is not in the 'Listing' array; returns a pointer to the new
    // listing on success, otherwise returns NULL
    CUploadPathListing* AddEmptyListing(const char* path, const char* dirName,
                                        CFTPServerPathType pathType,
                                        CUploadListingState listingState,
                                        BOOL doNotCheckIfPathIsKnown);

    // finds an item with CUploadPathListing::Path == 'path'; returns TRUE on success and 'index'
    // is the index of the found item; returns FALSE on failure
    BOOL FindPath(const char* path, CFTPServerPathType pathType, int& index);

    // see CUploadListingCache::ReportCreateDirs();
    // WARNING: do not call directly, called through CUploadListingCache::ReportCreateDirs()
    void ReportCreateDirs(const char* workPath, CFTPServerPathType pathType, const char* newDirs,
                          BOOL unknownResult);

    // see CUploadListingCache::ReportRename();
    // WARNING: do not call directly, called through CUploadListingCache::ReportRename()
    void ReportRename(const char* workPath, CFTPServerPathType pathType,
                      const char* fromName, const char* newName, BOOL unknownResult);

    // see CUploadListingCache::ReportDelete();
    // WARNING: do not call directly, called through CUploadListingCache::ReportDelete()
    void ReportDelete(const char* workPath, CFTPServerPathType pathType, const char* name,
                      BOOL unknownResult);

    // see CUploadListingCache::ReportStoreFile();
    // WARNING: do not call directly, called through CUploadListingCache::ReportStoreFile()
    void ReportStoreFile(const char* workPath, CFTPServerPathType pathType, const char* name);

    // see CUploadListingCache::ReportFileUploaded();
    // WARNING: do not call directly, called through CUploadListingCache::ReportFileUploaded()
    void ReportFileUploaded(const char* workPath, CFTPServerPathType pathType, const char* name,
                            const CQuadWord& fileSize, BOOL unknownResult);

    // see CUploadListingCache::ReportUnknownChange();
    // WARNING: do not call directly, called through CUploadListingCache::ReportUnknownChange()
    void ReportUnknownChange(const char* workPath, CFTPServerPathType pathType);

    // invalidates the listing at index 'index' (e.g. after an "unknown change" in this listing)
    void InvalidateListing(int index);

    // see CUploadListingCache::GetListing()
    // WARNING: do not call directly, called through CUploadListingCache::GetListing()
    BOOL GetListing(const char* path, CFTPServerPathType pathType, int workerMsg,
                    int workerUID, BOOL* listingInProgress, BOOL* notAccessible,
                    BOOL* getListing, const char* name, CUploadListingItem** existingItem,
                    BOOL* nameExists);

    // see CUploadListingCache::ListingFailed()
    // WARNING: do not call directly, called through CUploadListingCache::ListingFailed()
    // WARNING: if 'uploadFirstWaitingWorker' is NULL, must be called inside CSocketsThread::CritSect!
    void ListingFailed(const char* path, CFTPServerPathType pathType,
                       BOOL listingIsNotAccessible,
                       CUploadWaitingWorker** uploadFirstWaitingWorker,
                       BOOL* listingOKErrorIgnored);

    // see CUploadListingCache::ListingFinished()
    // WARNING: do not call directly, called through CUploadListingCache::ListingFinished()
    // WARNING: must be called inside CSocketsThread::CritSect!
    BOOL ListingFinished(const char* path, CFTPServerPathType pathType,
                         const char* pathListing, int pathListingLen,
                         const CFTPDate& pathListingDate, const char* welcomeReply,
                         const char* systReply, const char* suggestedListingServerType);
};

class CUploadListingCache
{
protected:
    CRITICAL_SECTION UploadLstCacheCritSect;                  // object critical section
    TIndirectArray<CUploadListingsOnServer> ListingsOnServer; // array of servers for which we have cached listings

public:
    CUploadListingCache();
    ~CUploadListingCache();

    // adds or updates a listing from the panel (before starting an upload operation and after a panel refresh
    // during the upload operation); 'user'+'host'+'port' describes the server; 'path' is the
    // local path on the server of type 'pathType'; 'pathListing'+'pathListingLen' is the listing text;
    // 'pathListingDate' is the listing timestamp (needed for "year_or_time");
    // 'listingStartTime' is IncListingCounter() from the moment the LIST command was sent to the server
    // (listing started); if 'onlyUpdate' is TRUE, only an update of a listing already in the cache is performed;
    // 'welcomeReply' (must not be NULL) is the first server reply (often contains the FTP server version);
    // 'systReply' (must not be NULL) is the server system (reply to the SYST command);
    // 'suggestedListingServerType' is the server type for parsing the listing:
    // NULL = autodetect, otherwise the server type name (without the optional leading '*'; if it stops existing,
    // it switches to autodetect); returns FALSE if the listing cannot be parsed or if memory is low or parameters are invalid;
    // returns TRUE if the listing was added or updated or was not updated because 'onlyUpdate'==TRUE or because
    // an update is not needed (i.e. returns TRUE if the cached path listing can be used)
    BOOL AddOrUpdateListing(const char* user, const char* host, unsigned short port,
                            const char* path, CFTPServerPathType pathType,
                            const char* pathListing, int pathListingLen,
                            const CFTPDate& pathListingDate, DWORD listingStartTime,
                            BOOL onlyUpdate, const char* welcomeReply,
                            const char* systReply, const char* suggestedListingServerType);

    // removes listings for server 'user'+'host'+'port' from the cache
    void RemoveServer(const char* user, const char* host, unsigned short port);

    // invalidates the listing of the path - on the next attempt to obtain the listing, the path will be listed
    // on the server; 'user'+'host'+'port' describes the server; 'path' is the path of type 'pathType' being invalidated
    void InvalidatePathListing(const char* user, const char* host, unsigned short port,
                               const char* path, CFTPServerPathType pathType);

    // determines whether the path listing was taken from the panel (returns TRUE in that case);
    // 'user'+'host'+'port' describes the server; 'path' is the sought path of type 'pathType'
    BOOL IsListingFromPanel(const char* user, const char* host, unsigned short port,
                            const char* path, CFTPServerPathType pathType);

    // removes "unobtainable" listings for server 'user'+'host'+'port' from the cache
    void RemoveNotAccessibleListings(const char* user, const char* host, unsigned short port);

    // change notification: creating directories (e.g. on VMS multiple directories can be created at once);
    // 'user'+'host'+'port' describes the server; 'workPath' is the working path of type 'pathType';
    // 'newDirs' is the command parameter for creating directories (may be one or more directories
    // relative or with an absolute path); 'unknownResult' is FALSE if the directories were created,
    // TRUE if the result is unknown (the relevant listings must be invalidated)
    void ReportCreateDirs(const char* user, const char* host, unsigned short port,
                          const char* workPath, CFTPServerPathType pathType, const char* newDirs,
                          BOOL unknownResult);

    // change notification: renaming (also moving) a file/directory;
    // 'user'+'host'+'port' describes the server; 'workPath' is the working path of type 'pathType';
    // 'fromName' is the name (without path) of the file/directory/link being renamed; 'newName'
    // is the target name (provided by the user - may be relative or include a path); 'unknownResult'
    // is FALSE if the rename completed normally, TRUE if the result is unknown (the relevant listings must be invalidated)
    void ReportRename(const char* user, const char* host, unsigned short port,
                      const char* workPath, CFTPServerPathType pathType,
                      const char* fromName, const char* newName, BOOL unknownResult);

    // change notification: deleting a file/link/directory;
    // 'user'+'host'+'port' describes the server; 'workPath' is the working path of type 'pathType';
    // 'name' is the delete command parameter (just the name without path); 'unknownResult' is FALSE
    // if the deletion succeeded, TRUE if the result is unknown (the relevant listing must be invalidated)
    void ReportDelete(const char* user, const char* host, unsigned short port,
                      const char* workPath, CFTPServerPathType pathType, const char* name,
                      BOOL unknownResult);

    // change notification: start of uploading a file (may also overwrite/append(resume) a file/link) - if the file
    // or link does not yet exist, a file with name 'name' is created; the file size is
    // set to UPLOADSIZE_NEEDUPDATE; 'user'+'host'+'port' describes the server; 'workPath' is
    // the working path of type 'pathType'; 'name' is the file/link name (just the name without path)
    void ReportStoreFile(const char* user, const char* host, unsigned short port,
                         const char* workPath, CFTPServerPathType pathType, const char* name);

    // change notification: file upload finished (may also overwrite/append(resume) a file/link) - sets
    // the uploaded file size (after ReportStoreFile the size equals UPLOADSIZE_NEEDUPDATE);
    // 'user'+'host'+'port' describes the server; 'workPath' is the working path of type 'pathType';
    // 'name' is the file/link name (just the name without path); 'fileSize' is the file size;
    // 'unknownResult' is FALSE if the upload succeeded, TRUE if the result is unknown
    // (the relevant listing must be invalidated)
    void ReportFileUploaded(const char* user, const char* host, unsigned short port,
                            const char* workPath, CFTPServerPathType pathType, const char* name,
                            const CQuadWord& fileSize, BOOL unknownResult);

    // change notification: unknown change (used after sending a custom command to the server), the working path listing must be invalidated;
    // 'user'+'host'+'port' describes the server; 'workPath' is the working path of type 'pathType'
    void ReportUnknownChange(const char* user, const char* host, unsigned short port,
                             const char* workPath, CFTPServerPathType pathType);

    // obtaining the listing of path 'path' (of type 'pathType') on server 'user'+'host'+'port'
    // from the cache - if it is not yet in the cache, it is added; if it is already being fetched, we wait;
    // if the path is not in the cache, it is added in the ulsInProgress state, returns TRUE in 'getListing'
    // and TRUE in 'listingInProgress'; if the path listing is "unobtainable", returns TRUE
    // in 'notAccessible' and FALSE in 'listingInProgress'; if the path is in the cache
    // in one of the ulsInProgress* states, returns FALSE in 'getListing', TRUE
    // in 'listingInProgress', and after the listing download finishes (or fails)
    // posts WORKER_TGTPATHLISTINGFINISHED to worker 'workerMsg'+'workerUID';
    // if the path is in the cache in the ulsReady state, returns FALSE in 'notAccessible',
    // FALSE in 'listingInProgress', returns the item named 'name' in the allocated
    // structure 'existingItem' (NULL if an item with that name was not found),
    // and TRUE/FALSE in 'nameExists' depending on whether an item named 'name' was found;
    // returns FALSE only when memory is low
    BOOL GetListing(const char* user, const char* host, unsigned short port,
                    const char* path, CFTPServerPathType pathType, int workerMsg,
                    int workerUID, BOOL* listingInProgress, BOOL* notAccessible,
                    BOOL* getListing, const char* name, CUploadListingItem** existingItem,
                    BOOL* nameExists);

    // reports to the cache an error while obtaining the listing of path 'path' (of type 'pathType') on
    // server 'user'+'host'+'port' from a worker; 'listingIsNotAccessible' is TRUE
    // if the listing is "unobtainable" (further attempts make no sense),
    // FALSE if it is just a connection error (further attempts make sense);
    // if 'uploadFirstWaitingWorker' is NULL, WORKER_TGTPATHLISTINGFINISHED messages are sent,
    // otherwise 'uploadFirstWaitingWorker' is the list where workers that should receive WORKER_TGTPATHLISTINGFINISHED are added;
    // 'listingOKErrorIgnored' (if not NULL) returns TRUE if the listing
    // was obtained in another way and this error can therefore be ignored
    // WARNING: if 'uploadFirstWaitingWorker' is NULL, must be called inside CSocketsThread::CritSect!
    void ListingFailed(const char* user, const char* host, unsigned short port,
                       const char* path, CFTPServerPathType pathType,
                       BOOL listingIsNotAccessible,
                       CUploadWaitingWorker** uploadFirstWaitingWorker,
                       BOOL* listingOKErrorIgnored);

    // reports to the cache that listing of path 'path' (of type 'pathType') on server
    // 'user'+'host'+'port' finished in the worker; 'pathListing'+'pathListingLen' is the listing text;
    // 'pathListingDate' is the listing timestamp (needed for "year_or_time");
    // 'welcomeReply' (must not be NULL) is the first server reply (often contains the FTP server version);
    // 'systReply' (must not be NULL) is the server system (reply to the SYST command);
    // 'suggestedListingServerType' is the server type for parsing the listing:
    // NULL = autodetect, otherwise the server type name (without the optional leading '*'; if it stops existing,
    // it switches to autodetect); returns FALSE only when memory is low
    // WARNING: must be called inside CSocketsThread::CritSect!
    BOOL ListingFinished(const char* user, const char* host, unsigned short port,
                         const char* path, CFTPServerPathType pathType,
                         const char* pathListing, int pathListingLen,
                         const CFTPDate& pathListingDate, const char* welcomeReply,
                         const char* systReply, const char* suggestedListingServerType);

protected:
    // call only from the UploadLstCacheCritSect critical section; returns the server from ListingsOnServer
    // or NULL if the server was not found; if 'index' is not NULL, the index
    // of the found server is returned (not found - returns -1)
    CUploadListingsOnServer* FindServer(const char* user, const char* host,
                                        unsigned short port, int* index);
};

//
// ****************************************************************************
// CFTPOpenedFiles
//
// because FTP servers do not care whether a file is already open (it is possible to
// upload/download/delete a single file from any number of connections at the same time—the result
// then depends heavily on the server implementation), we at least check whether the file is already
// open (locked by another operation) within this Salamander instance

enum CFTPFileAccessType
{
    ffatRead,   // during download
    ffatWrite,  // during upload (the file may also be deleted before overwriting or after a failed save)
    ffatDelete, // during deletion
    ffatRename, // during renaming (for both the old and new name)
};

class CFTPOpenedFile
{
protected:
    int UID; // UID of this opened file

    CFTPFileAccessType AccessType; // why the file is open (locked)

    char User[USER_MAX_SIZE];    // user-name
    char Host[HOST_MAX_SIZE];    // host-address
    unsigned short Port;         // port on which the FTP server runs
    char Path[FTP_MAX_PATH];     // path to the opened file (local on the server)
    CFTPServerPathType PathType; // type of the cached path
    char Name[MAX_PATH];         // name of the opened file

public:
    CFTPOpenedFile(int myUID, const char* user, const char* host, unsigned short port,
                   const char* path, CFTPServerPathType pathType, const char* name,
                   CFTPFileAccessType accessType);

    // sets data into the object
    void Set(int myUID, const char* user, const char* host, unsigned short port,
             const char* path, CFTPServerPathType pathType, const char* name,
             CFTPFileAccessType accessType);

    // compares this opened file with the file specified by the method parameters; returns TRUE if
    // it is the same file
    BOOL IsSameFile(const char* user, const char* host, unsigned short port,
                    const char* path, CFTPServerPathType pathType, const char* name);

    BOOL IsUID(int uid) { return uid == UID; }

    // returns TRUE if it is not possible to open this file with access 'accessType'
    // (determined by the current 'AccessType'); returns FALSE if
    // another opening of this file is possible (e.g. a second download)
    BOOL IsInConflictWith(CFTPFileAccessType accessType);
};

class CFTPOpenedFiles
{
protected:
    CRITICAL_SECTION FTPOpenedFilesCritSect; // object critical section

    TIndirectArray<CFTPOpenedFile> OpenedFiles;      // all files currently opened
    TIndirectArray<CFTPOpenedFile> AllocatedObjects; // storage of allocated unused CFTPOpenedFile objects (optimization to avoid constant allocation/deallocation)
    int NextOpenedFileUID;                           // UID for the next file to open

public:
    CFTPOpenedFiles();
    ~CFTPOpenedFiles();

    // checks whether the file can be opened with access 'accessType'; if so,
    // adds it among the opened files, returns TRUE, and stores the new file UID in 'newUID'; otherwise
    // (even when memory is low) returns FALSE
    BOOL OpenFile(const char* user, const char* host, unsigned short port,
                  const char* path, CFTPServerPathType pathType, const char* name,
                  int* newUID, CFTPFileAccessType accessType);

    // closes the opened file with UID 'UID'
    void CloseFile(int UID);
};

//
// ****************************************************************************

// creates an item for the Delete operation for file/directory 'f'
CFTPQueueItem* CreateItemForDeleteOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                            CFTPListingPluginDataInterface* dataIface,
                                            CFTPQueueItemType* type, BOOL* ok, BOOL isTopLevelDir,
                                            int hiddenFileDel, int hiddenDirDel,
                                            CFTPQueueItemState* state, DWORD* problemID,
                                            int* skippedItems, int* uiNeededItems);

// creates an item for the Copy or Move operation for file/directory 'f'
CFTPQueueItem* CreateItemForCopyOrMoveOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                                CFTPListingPluginDataInterface* dataIface,
                                                CFTPQueueItemType* type, int transferMode,
                                                CFTPOperation* oper, BOOL copy, const char* targetPath,
                                                const char* targetName, CQuadWord* size,
                                                BOOL* sizeInBytes, CQuadWord* totalSize);

// creates an item for the Change Attributes operation for file/directory 'f'
CFTPQueueItem* CreateItemForChangeAttrsOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                                 CFTPListingPluginDataInterface* dataIface,
                                                 CFTPQueueItemType* type, BOOL* ok,
                                                 CFTPQueueItemState* state, DWORD* problemID,
                                                 int* skippedItems, int* uiNeededItems,
                                                 BOOL* skip, BOOL selFiles,
                                                 BOOL selDirs, BOOL includeSubdirs,
                                                 DWORD attrAndMask, DWORD attrOrMask,
                                                 int operationsUnknownAttrs);

// creates an item for Copy or Move from disk to the file system for file/directory 'name'
CFTPQueueItem* CreateItemForCopyOrMoveUploadOperation(const char* name, BOOL isDir, const CQuadWord* size,
                                                      CFTPQueueItemType* type, int transferMode,
                                                      CFTPOperation* oper, BOOL copy, const char* targetPath,
                                                      const char* targetName, CQuadWord* totalSize,
                                                      BOOL isVMS);

//
// ****************************************************************************

extern HANDLE WorkerMayBeClosedEvent;             // generates a pulse when a worker's socket closes
extern int WorkerMayBeClosedState;                // incremented with each worker socket closure
extern CRITICAL_SECTION WorkerMayBeClosedStateCS; // critical section protecting access to WorkerMayBeClosedState

extern CFTPOperationsList FTPOperationsList; // all FTP operations
extern CUIDArray CanceledOperations;         // array of operation UIDs scheduled for cancellation (after receiving the FTPCMD_CANCELOPERATION command)

extern CReturningConnections ReturningConnections; // array with connections returned from workers to the panel

extern CFTPDiskThread* FTPDiskThread; // thread providing disk operations (reason: non-blocking calls)

extern CUploadListingCache UploadListingCache; // cache of path listings on servers - used during upload to detect whether the target file/directory already exists

extern CFTPOpenedFiles FTPOpenedFiles; // list of files currently opened from this Salamander on FTP servers

#pragma pack(pop, enter_include_operats_h_dt)
