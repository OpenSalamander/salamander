// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// initializes the disk cache, returns success
BOOL InitializeDiskCache();

// how long time to wait between checking the state of watched objects
#define CACHE_HANDLES_WAIT 500
// (100 MB) max . size of disk-cache in bytes, should be moved to configuration
#define MAX_CACHE_SIZE CQuadWord(104857600, 0)

// error state codes for method CDiskCache::GetName()
#define DCGNE_SUCCESS 0
#define DCGNE_LOWMEMORY 1
#define DCGNE_NOTFOUND 2
#define DCGNE_TOOLONGNAME 3
#define DCGNE_ERRCREATINGTMPDIR 4
#define DCGNE_ALREADYEXISTS 5

//****************************************************************************
//
// CCacheData
//

enum CCacheRemoveType // if it's possible to remove the item from cache, when it will happen?
{
    crtCache, // once the cache asks for it (cache size limit exceeded, ...) (FTP - speed cache)
    crtDirect // immediately (no need to cache, remove it once it's not needed)
};

class CDiskCache;
class CCacheHandles;

class CCacheData // tmp-name, info about file or directory on disk, internal use
{
protected:
    char* Name;       // the item identification (path to original)
    char* TmpName;    // the tmp-file name on disk (full path)
    HANDLE Preparing; // mutex, which "holds" the thread, which prepares the tmp-file

    // system objects - array of (HANDLE): state "signaled" -> remove this 'lock'
    TDirectArray<HANDLE> LockObject;
    // the object ownership - array of (BOOL): TRUE -> call CloseHandle('lock')
    TDirectArray<BOOL> LockObjOwner;

    BOOL Cached;                               // is it a cached tmp-file? (did CrtCache already arrive?)
    BOOL Prepared;                             // is the tmp-file prepared for use? (e.g. downloaded from FTP?)
    int NewCount;                              // the count of new requests for the tmp-file
    CQuadWord Size;                            // tmp-file size (in bytes)
    int LastAccess;                            // "time" of last access to the tmp-file (for cache - remove the oldest)
    BOOL Detached;                             // TRUE => the tmp-file should not be deleted
    BOOL OutOfDate;                            // TRUE => obtain a new copy as soon as possible (as if it were not on disk)
    BOOL OwnDelete;                            // FALSE = delete the tmp-file using DeleteFile(), TRUE = delete using DeleteManager (the plugin OwnDeletePlugin deletes)
    CPluginInterfaceAbstract* OwnDeletePlugin; // plugin interface, which should delete the tmp-file (NULL = the plugin is unloaded, the tmp-file should not be deleted)

public:
    CCacheData(const char* name, const char* tmpName, BOOL ownDelete,
               CPluginInterfaceAbstract* ownDeletePlugin);
    ~CCacheData();

    // returns the set size of the tmp-file
    CQuadWord GetSize() { return Size; }

    // returns TRUE if this tmp-file should be deleted by plugin 'ownDeletePlugin'
    BOOL IsDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin)
    {
        return OwnDelete && OwnDeletePlugin == ownDeletePlugin;
    }

    // returns "time" of last access to the tmp-file
    int GetLastAccess() { return LastAccess; }

    // removes the temporary file from disk, returns success (Name is no longer on disk)
    BOOL CleanFromDisk();

    // did the object initialization succeeded?
    BOOL IsGood() { return Name != NULL; }

    // should the tmp-file be cached?
    BOOL IsCached() { return Cached; }

    // is the tmp-file without any link? (it still has no link/it has no link anymore?)
    BOOL IsLocked() { return LockObject.Count == 0 && NewCount == 0; }

    BOOL NameEqual(const char* name) { return StrICmp(Name, name) == 0; }
    BOOL TmpNameEqual(const char* tmpName) { return StrICmp(TmpName, tmpName) == 0; }

    // waits until the tmp-file is prepared or until the method ReleaseName() is called
    // then 'exists' is set to return value matching CDiskCache::GetName()
    // NULL -> fatal error or "file not prepared" (see below)
    //
    // if 'onlyAdd' is TRUE, only the deleted tmp-file can be restored - if the tmp-file is prepared,
    // NULL is returned and 'exists' is set to FALSE (further interpreted as "file already exists" error);
    // 'canBlock' is TRUE if waiting for readiness of the tmp-file is expected, in case it's not
    // prepared, if 'canBlock' is FALSE and the tmp-file is not prepared, NULL is returned and
    // 'exists' is set to FALSE ("not found"); if 'errorCode' is not NULL, the error code is returned
    // in it (see DCGNE_XXX)
    const char* GetName(CDiskCache* monitor, BOOL* exists, BOOL canBlock, BOOL onlyAdd,
                        int* errorCode);

    // for description see CDiskCache::NamePrepared()
    BOOL NamePrepared(const CQuadWord& size);

    // see CDiskCache::AssignName() for a description
    //
    // handles - object for tracking 'lock' objects
    BOOL AssignName(CCacheHandles* handles, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove);

    // see CDiskCache::ReleaseName() for a description
    //
    // lastLock - pointer to a BOOL that is set to TRUE if there are no more links to the tmp-file
    BOOL ReleaseName(BOOL* lastLock, BOOL storeInCache);

    // returns the full name of the tmp-file
    const char* GetTmpName() { return TmpName; }

    // detaches the 'lock' object (in the "signaled" state) from the tmp-file (detaches the link)
    //
    // returns success
    //
    // lock - object that has entered the "signaled" state
    // lastLock - pointer to a BOOL that is set to TRUE if there are no more links to the tmp-file
    BOOL WaitSatisfied(HANDLE lock, BOOL* lastLock);

    // if we change our mind about deleting the tmp-file on disk (e.g. it was not possible to pack it,
    // so we leave it in temp, so that the users don't kill us)
    void DetachTmpFile() { Detached = TRUE; }

    // changes type of tmp-file to crtDirect (direct deletion after use)
    void SetOutOfDate()
    {
        Cached = FALSE;
        OutOfDate = TRUE;
    }

    // returns item identification (path to original)
    const char* GetName() { return Name; }

    // performs premature deletion of the tmp-file if it is deleted by the plugin
    // 'ownDeletePlugin'; used when unloading the plugin (the tmp-file is marked as deleted,
    // so once all references to it are closed, no deletion occurs); if 'onlyDetach' is TRUE,
    // the tmp-file is not deleted and is only marked as deleted (the plugin is detached from the tmp-file)
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);
};

//****************************************************************************
//
// CCacheDirData
//

class CCacheDirData // tmp-directory, contains unique tmp-names, internal use
{
protected:
    char Path[MAX_PATH];             // tmp-directory representation on disk
    int PathLength;                  // length of the string in Path
    TDirectArray<CCacheData*> Names; // the list of records, type of item (CCacheData *)

public:
    CCacheDirData(const char* path);
    ~CCacheDirData();

    int GetNamesCount() { return Names.Count; }

    // if there's no file in the tmp-directory, it's deleted from disk
    // (finishing of TEMP cleaning - see CDiskCache::RemoveEmptyTmpDirsOnlyFromDisk())
    void RemoveEmptyTmpDirsOnlyFromDisk();

    // returns TRUE if the tmp-directory contains 'tmpName' (the name of a file/directory on disk)
    // rootTmpPath - path where to place the tmp-directory with the tmp-file (must not be NULL)
    // rootTmpPathLen - length of the rootTmpPath string
    // canContainThisName - must not be NULL; set to TRUE if it is possible to place
    //                      the tmp-file into this tmp-directory (it matches tmp-root and
    //                      contains no file with a DOS name equal to 'tmpName')
    BOOL ContainTmpName(const char* tmpName, const char* rootTmpPath, int rootTmpPathLen,
                        BOOL* canContainThisName);

    // searches for 'name' in the tmp-directory; if it's found, returns TRUE and values 'name' and 'tmpPath',
    // so that they match expected values from CDiskCache::GetName(); if it's not found,
    // returns FALSE
    //
    // name - unique item identification
    // exists - pointer to BOOL, which is set per the description above
    // tmpPath - return value of CDiskCache::GetName() (NULL && 'exists'==TRUE -> fatal error)
    //
    // if 'onlyAdd' is TRUE, it is possible to create only a new name or restore a deleted tmp-file
    // (the name exists, but the tmp-file is not prepared) - if the name exists, returns TRUE,
    // 'exists' FALSE and 'tmpPath' NULL ("file already exists");
    // 'canBlock' is TRUE if waiting for readiness of the tmp-file is expected, in case 'name'
    // is in cache, but it's not prepared, if 'canBlock' is FALSE and the tmp-file is not prepared,
    // returns TRUE, 'exists' FALSE and 'tmpPath' NULL ("not found");
    // if 'errorCode' is not NULL, the error code is returned in it (see DCGNE_XXX)
    BOOL GetName(CDiskCache* monitor, const char* name, BOOL* exists, const char** tmpPath,
                 BOOL canBlock, BOOL onlyAdd, int* errorCode);

    // for description see CDiskCache::GetName() - adding a new 'name'
    const char* GetName(const char* name, const char* tmpName, BOOL* exists, BOOL ownDelete,
                        CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode);

    // searches for 'name' in the tmp-directory; if found, returns TRUE and 'ret' is set to the return value of
    // CDiskCache::NamePrepared(name, size); if not found, returns FALSE
    // see CDiskCache::NamePrepared() for a description
    BOOL NamePrepared(const char* name, const CQuadWord& size, BOOL* ret);

    // searches for 'name' in the tmp-directory; if found, returns TRUE and 'ret' is set to the return value of
    // CDiskCache::AssignName(name, lock, lockOwner, remove); if not found, returns FALSE
    // see CDiskCache::AssignName() for a description
    //
    // handles - object for tracking 'lock' objects
    BOOL AssignName(CCacheHandles* handles, const char* name, HANDLE lock, BOOL lockOwner,
                    CCacheRemoveType remove, BOOL* ret);

    // searches for 'name' in the tmp directory; if found, returns TRUE and 'ret' is set to the return value of
    // CDiskCache::ReleaseName(name); if not found, returns FALSE
    // for a description, see CDiskCache::ReleaseName()
    //
    // lastCached - pointer to BOOL that is set to TRUE if this is the last link to the cached tmp file,
    //              i.e. if its further existence must be decided
    BOOL ReleaseName(const char* name, BOOL* ret, BOOL* lastCached, BOOL storeInCache);

    // searches for 'data' in the tmp directory; if found, returns TRUE and removes the tmp file 'data';
    // if not found, returns FALSE
    //
    // data - tmp file
    BOOL Release(CCacheData* data);

    // sum of sizes of tmp-files in the tmp-directory
    CQuadWord GetSizeOfFiles();

    // fills the array 'victArr' with cached orphaned tmp files (WARNING: not sorted from oldest to newest)
    void AddVictimsToArray(TDirectArray<CCacheData*>& victArr);

    // if we change our mind about deleting the tmp-file on disk (e.g. it was not possible to pack it
    // so we leave it in temp, so that the users don't kill us)
    BOOL DetachTmpFile(const char* tmpName);

    // removes all cached files whose names start with 'name' (e.g. all files from one archive)
    // open files are marked as out-of-date so they are refreshed on next use
    // (the current copy remains so viewers do not complain)
    void FlushCache(const char* name);

    // removes cached file 'name'; the open file is marked as out-of-date so it is refreshed on next use
    // (the current copy remains so viewers are not disrupted); returns TRUE
    // if the file was found and removed
    BOOL FlushOneFile(const char* name);

    // searches for the name in the Names array; returns TRUE if 'name' is found (and also returns its position in 'index');
    // returns FALSE if 'name' is not in Names (and also returns the insertion position in 'index')
    BOOL GetNameIndex(const char* name, int& index);

    // counts how many tmp-files are contained in tmp-directory, which are deleted by the plugin 'ownDeletePlugin'
    int CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin);

    // performs premature deletion of all tmp-files deleted by the plugin
    // 'ownDeletePlugin'; used when unloading the plugin (the tmp-files are marked as deleted,
    // so once all references to them are closed, no deletion occurs); if 'onlyDetach' is TRUE,
    // they are not deleted and are only marked as deleted (the plugin is detached from the tmp-files)
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);
};

//****************************************************************************
//
// CCacheHandles
//

class CCacheHandles
{
protected:
    TDirectArray<HANDLE> Handles;     // array of (HANDLE), array for WaitForXXX
    TDirectArray<CCacheData*> Owners; // array of (CCacheData *), array of owners for searching handles

    HANDLE HandleInBox; // box for data transfer - handle+owner
    CCacheData* OwnerInBox;
    HANDLE Box;       // event for box - "signaled" means "box is free"
    HANDLE BoxFull;   // event for box selection - "signaled" means "select box"
    HANDLE Terminate; // event for thread termination - "signaled" means "terminate"
    HANDLE TestIdle;  // event for running idle-state test - "signaled" means "test"
    HANDLE IsIdle;    // event for marking idle-state - "signaled" means "we are in idle-state"

    HANDLE Thread; // watching thread

    CDiskCache* DiskCache; // disk-cache, to which this object belongs

    int Idle; // this is used to determine if we found anything during one round of searching
              // 0 - empty (and answer is no), 1 - query, 2 - checking answer, 3 - answer yes

public:
    CCacheHandles();
    void SetDiskCache(CDiskCache* diskCache) { DiskCache = diskCache; }

    // object destructor
    void Destroy();

    // returns object construction success
    BOOL IsGood() { return Box != NULL; }

    // waits until the box is free (if the box is full, waits until the watcher cache thread takes the data)
    void WaitForBox();

    // set data in the box and notify the watching thread to select the box
    void SetBox(HANDLE handle, CCacheData* owner);

    // release the box, an error occurred, the box contains no data
    void ReleaseBox();

    // waits until the moment when the cache-handles are "free", it's useful when mass
    // deleting files with  the only lock object (waits until all dependent files are
    // deleted)
    void WaitForIdle();

protected:
    // gradually (max. MAXIMUM_WAIT_OBJECTS) waits for the given Handles,
    // returns the triple (handle, owner, index), for which the handle is "signaled" or "abandoned"
    // and 'index' is the index of the pair (handle, owner) in the array Handles
    void WaitForObjects(HANDLE* handle, CCacheData** owner, int* index);

    // selects data from the box and inserts them into the arrays
    BOOL ReceiveBox();

    // reacts to the transition of one of the 'handle' to the "signaled" state (the tmp-file loses a link)
    void WaitSatisfied(HANDLE handle, CCacheData* owner, int index);

    friend unsigned ThreadCacheHandlesBody(void* param); // our watching thread
};

//****************************************************************************
//
// CDiskCache
//

class CDiskCache // assigns names for tmp-files
{                // object is synchronized - monitor
protected:
    CRITICAL_SECTION Monitor;          // critical section used to synchronize this object (monitor behavior)
    CRITICAL_SECTION WaitForIdleCS;    // section used for synchronization of calling WaitForIdle()
    TDirectArray<CCacheDirData*> Dirs; // list of tmp-directories, type of item (CCacheDirData *)
    CCacheHandles Handles;             // object, which watches the 'lock' objects

public:
    CDiskCache();
    ~CDiskCache();

    // preparation of the object for Salamander shutdown (shutdown, log off, or just exit)
    void PrepareForShutdown();

    // browses tmp-directories on disk and deletes those, which are empty (finishing of TEMP cleaning)
    // (e.g. Encrypt plugin has its own) after previous call of PrematureDeleteByPlugin())
    void RemoveEmptyTmpDirsOnlyFromDisk();

    // returns whether object initialization succeeded
    BOOL IsGood() { return Handles.IsGood() && Dirs.IsGood(); }

    // tries to find 'name' in the cache; if found, waits until the tmp-file is prepared
    // (e.g. downloaded from FTP), then returns the tmp-file name and sets 'exists' to TRUE;
    // if found but the tmp-file was deleted from disk, returns the tmp-file name and sets
    // 'exists' to FALSE; in that case the tmp-file must be prepared again and then
    // NamePrepared() must be called; if 'tmpName' is NULL, only look up 'name' in the cache
    // (do not create a new tmp-file); if 'onlyAdd' is TRUE, only add to the cache (if the
    // tmp-file already exists, the call fails; if someone deleted the tmp-file directly
    // from disk, restoring it is treated as adding, so in that case no error is returned);
    // if not found and 'tmpName' is not NULL, creates a new name for the tmp-file,
    // returns it immediately and sets 'exists' to FALSE; in that case, when the tmp-file is
    // prepared (e.g. downloaded from FTP), NamePrepared() must be called so that the file
    // becomes available to other threads;
    // WARNING: AssignName() or ReleaseName() must be called
    //
    // return value NULL -> "fatal error" ('exists' is TRUE) or, if 'tmpName' is NULL,
    //                      "not found" ('exists' is FALSE), and if 'onlyAdd' is TRUE,
    //                      "file already exists" ('exists' is FALSE)
    //
    // name - unique item identifier
    // tmpName - requested name of the tmp-file or directory; a tmp-directory will be selected
    //           for it
    // exists - pointer to a BOOL set as described above
    // onlyAdd - if TRUE, only a new name can be created (if the name already exists,
    //           returns NULL) or a deleted tmp-file can be restored (the name exists, but the
    //           tmp-file is not prepared)
    // rootTmpPath - if NULL, the tmp-directory with the tmp-file should be placed in TEMP;
    //               otherwise this is the path where the tmp-directory with the tmp-file
    //               should be placed
    // ownDelete - if FALSE, tmp-files should be deleted using DeleteFile(); otherwise they
    //             should be deleted by DeleteManager (plugin-based deletion - see
    //             ownDeletePlugin)
    // ownDeletePlugin - if ownDelete is TRUE, contains the plugin interface that deletes the
    //                   tmp-file
    // errorCode - if not NULL and an error occurs, its code is returned in this variable (for
    //             codes see DCGNE_XXX)
    const char* GetName(const char* name, const char* tmpName, BOOL* exists, BOOL onlyAdd,
                        const char* rootTmpPath, BOOL ownDelete,
                        CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode);

    // marks the tmp-file corresponding to 'name' as valid and makes it available to other threads;
    // can be called only after GetName() returns 'exists' == FALSE
    //
    // returns success
    //
    // name - unique item identifier
    // size - number of bytes occupied by the tmp-file on disk
    BOOL NamePrepared(const char* name, const CQuadWord& size);

    // assigns a system object to the acquired tmp-file; 'lock' controls the minimum
    // lifetime of the tmp-file (depends on 'remove')
    // can only be called together with GetName()
    //
    // returns success
    //
    // name - unique item identifier (used later for searching)
    // lock - when this object is "signaled", the tmp-file can be "released"
    //        (depends on 'remove')
    // lockOwner - should the cache take care of calling CloseHandle(lock)?
    // remove - when to delete the tmp-file
    BOOL AssignName(const char* name, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove);

    // called only when NamePrepared() or AssignName() cannot be called after GetName();
    // used for errors while acquiring the tmp-file or the 'lock' object (the launched
    // application for which the tmp-file was being acquired)
    // if NamePrepared() would have to be called, gives other threads a chance to create the
    // tmp-file (those waiting until the tmp-file is prepared); if AssignName() would have to
    // be called, cancels the tmp-file's waiting state for assignment of the 'lock' object, and the
    // tmp-file may be deleted; if 'storeInCache' is TRUE and the tmp-file is prepared and not
    // locked, it is marked as cached (if the maximum cache capacity allows it, it will not be
    // deleted)
    //
    // returns success
    //
    // name - item identifier (used later for lookup)
    BOOL ReleaseName(const char* name, BOOL storeInCache);

    // waits for the moment when the cache-handles are "free", it's useful when mass
    // deleting files with  the only lock object (waits until all dependent files are
    // deleted)
    void WaitForIdle()
    {
        HANDLES(EnterCriticalSection(&WaitForIdleCS));
        Handles.WaitForIdle();
        HANDLES(LeaveCriticalSection(&WaitForIdleCS));
    }

    // when we change our mind about deleting the tmp-file on disk (e.g. it was not possible to pack it,
    // so we leave it in temp, so that the users don't kill us), it returns success
    BOOL DetachTmpFile(const char* tmpName);

    // removes all cached tmp-files beginning with 'name' (e.g. all files from one archive)
    void FlushCache(const char* name);

    // removes cached file 'name'; returns TRUE if the file was found and removed
    BOOL FlushOneFile(const char* name);

    // counts how many tmp-files are contained in disk-cache, which are deleted by the plugin 'ownDeletePlugin'
    int CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin);

    // Performs early deletion of all temporary files deleted by the 'ownDeletePlugin'
    // plugin; used when unloading the plugin (the temporary files are marked as
    // already deleted, so once all references to them are closed, no further deletion
    // occurs); if 'onlyDetach' is TRUE, they are not deleted and are only marked as
    // already deleted (the plugin is detached from the temporary files).
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);

    // the TEMP directory clean-up from the rest of previous instances; called only by the first instance
    // if it finds subdirectories "SAL*.tmp", it asks the user if he wants to delete them and if so,
    // it deletes them
    void ClearTEMPIfNeeded(HWND parent, HWND hActivePanel);

protected:
    void Enter() { HANDLES(EnterCriticalSection(&Monitor)); } // called after entering methods
    void Leave() { HANDLES(LeaveCriticalSection(&Monitor)); } // called before leaving methods

    // checks conditions on disk, if necessary, releases some free cached tmp-files
    void CheckCachedFiles();

    // reacts to the transition of one of the 'lock' to the "signaled" state (the tmp-file loses a link)
    //
    // lock - watched object handle, which turned to "signaled" state
    // owner - an object containing this 'lock'
    void WaitSatisfied(HANDLE lock, CCacheData* owner);

    friend class CCacheData;    // calls Enter() and Leave()
    friend class CCacheHandles; // calls WaitSatisfied()
};

//****************************************************************************
//
// CDeleteManager
//

struct CPluginData;

struct CDeleteManagerItem
{
    char* FileName;                   // the name of file, which should be deleted by the plugin
    CPluginInterfaceAbstract* Plugin; // a plugin, which will delete the file via the method
                                      // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy

    CDeleteManagerItem(const char* fileName, CPluginInterfaceAbstract* plugin);
    ~CDeleteManagerItem();
    BOOL IsGood() { return FileName != NULL; }
};

class CDeleteManager
{
protected:
    CRITICAL_SECTION CS; // section used for synchronization of this object

    // data about the files that are to be deleted (in the main thread by calling the method
    // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy of the plugin)
    TIndirectArray<CDeleteManagerItem> Data;
    BOOL WaitingForProcessing; // TRUE = a message to the main window is pending or data
                               // processing is already in progress (the added item is processed immediately)
    BOOL BlockDataProcessing;  // TRUE = do not process data (ProcessData() does nothing)

public:
    CDeleteManager();
    ~CDeleteManager();

    // adds a file for deletion and ensures that the plugin method
    // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy is called as soon as possible
    // to delete the file; if an error occurs, the file is not deleted; the plugin should clean up its
    // directory on unload/load (TEMP will be cleaned by Salamander)
    // can be called from any thread
    void AddFile(const char* fileName, CPluginInterfaceAbstract* plugin);

    // called from the main thread (calls main window after receiving the message WM_USER_PROCESSDELETEMAN);
    // processing of new data - deleting files in plugins
    void ProcessData();

    // plugin 'plugin' may be unloaded: let it delete temporary copies from disk
    // (see CPluginInterfaceForArchiverAbstract::PrematureDeleteTmpCopy)
    void PluginMayBeUnloaded(HWND parent, CPluginData* plugin);

    // plugin 'plugin' has been unloaded: detach it from the delete-manager and disk-cache;
    // 'unloadedPlugin' is already an invalid plugin interface saved before unload
    void PluginWasUnloaded(CPluginData* plugin, CPluginInterfaceAbstract* unloadedPlugin);
};

extern CDiskCache DiskCache;         // global disk-cache
extern CDeleteManager DeleteManager; // global disk-cache delete-manager (for deleting tmp-files in archiver plugins)
