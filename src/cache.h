// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// disck cache initialization, returns success
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
    BOOL OutOfDate;                            // TRUE => once possible, we acquire a new copy (as if it's not on disk)
    BOOL OwnDelete;                            // FALSE = delete the tmp-file using DeleteFile(), TRUE = delete using DeleteManager (the plugin OwnDeletePlugin deletes)
    CPluginInterfaceAbstract* OwnDeletePlugin; // plugin interface, which should delete the tmp-file (NULL = the plugin is unloaded, the tmp-file should not be deleted)

public:
    CCacheData(const char* name, const char* tmpName, BOOL ownDelete,
               CPluginInterfaceAbstract* ownDeletePlugin);
    ~CCacheData();

    // returns the set size of the tmp-file
    CQuadWord GetSize() { return Size; }

    // returns TRUE if the tmp-file is deleted by the plugin 'ownDeletePlugin'
    BOOL IsDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin)
    {
        return OwnDelete && OwnDeletePlugin == ownDeletePlugin;
    }

    // returns "time" of last access to the tmp-file
    int GetLastAccess() { return LastAccess; }

    // cancels tmp-file on disk, returns success (Name is not on disk anymore)
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

    // for description see CDiskCache::AssignName()
    //
    // handles - object for watching the 'lock' object
    BOOL AssignName(CCacheHandles* handles, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove);

    // for description see CDiskCache::ReleaseName()
    //
    // lastLock - pointer to BOOL, which is set to TRUE if there are no links to the tmp-file anymore
    BOOL ReleaseName(BOOL* lastLock, BOOL storeInCache);

    // returns the full name of the tmp-file
    const char* GetTmpName() { return TmpName; }

    // detaches the object 'lock' (in "signaled" state) from the tmp-file (detaches the link)
    //
    // returns success
    //
    // lock - object, which is in "signaled" state
    // lastLock - pointer to BOOL, which is set to TRUE if there are no links to the tmp-file anymore
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

    // performs premature deletion of the tmp-file, which is deleted by the plugin 'ownDeletePlugin';
    // used when unloading the plugin (the tmp-file is marked as deleted - once all links are closed,
    // deletion won't occur); if 'onlyDetach' is TRUE, the tmp-file is not deleted, it's only marked
    // as deleted (the plugin is detached from the tmp-file)
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

    // returns TRUE if the tmp-directory contains 'tmpName' (name of file/directory on disk)
    // rootTmpPath - path where to place the tmp-directory with the tmp-file (must not be NULL)
    // rootTmpPathLen - length of the string in rootTmpPath
    // canContainThisName - must not be NULL, returns TRUE in it if it's possible to place
    //                      the tmp-file into this tmp-directory (matches tmp-root + there's
    //                      no file with DOS-name equal to 'tmpName')
    BOOL ContainTmpName(const char* tmpName, const char* rootTmpPath, int rootTmpPathLen,
                        BOOL* canContainThisName);

    // seeks for 'name' in the tmp-directory; if it's found, returns TRUE and values 'name' and 'tmpPath',
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

    // seeks for 'name' in the tmp-directory; if it's found, returns TRUE and 'ret' is set to return value
    // CDiskCache::NamePrepared(name, size); if it's not found, returns FALSE
    // for description see CDiskCache::NamePrepared()
    BOOL NamePrepared(const char* name, const CQuadWord& size, BOOL* ret);

    // seeks for 'name' in the tmp-directory; if it's found, returns TRUE and 'ret' is set to return value
    // CDiskCache::AssignName(name, lock, lockOwner, remove); if it's not found, returns FALSE
    // for description see CDiskCache::AssignName()
    //
    // handles - object for watching the 'lock' object
    BOOL AssignName(CCacheHandles* handles, const char* name, HANDLE lock, BOOL lockOwner,
                    CCacheRemoveType remove, BOOL* ret);

    // seeks for 'name' in the tmp-directory; if it's found, returns TRUE and 'ret' is set to return value
    // CDiskCache::ReleaseName(name); if it's not found, returns FALSE
    // for description see CDiskCache::ReleaseName()
    //
    // lastCached - pointer to BOOL, which is set to TRUE if this is the last link to the cached tmp-file,
    //              or if it's necessary to decide about its further existence
    BOOL ReleaseName(const char* name, BOOL* ret, BOOL* lastCached, BOOL storeInCache);

    // seeks for 'data' in the tmp-directory; if it's found, returns TRUE and cancels tmp-file 'data';
    // if it's not found, returns FALSE
    //
    // data - tmp-file
    BOOL Release(CCacheData* data);

    // sum of sizes of tmp-files in the tmp-directory
    CQuadWord GetSizeOfFiles();

    // fills the array 'victArr' with cached tmp-files without any link (WARNING: it doesn't sort from the oldest to the newest)
    void AddVictimsToArray(TDirectArray<CCacheData*>& victArr);

    // if we change our mind about deleting the tmp-file on disk (e.g. it was not possible to pack it
    // so we leave it in temp, so that the users don't kill us)
    BOOL DetachTmpFile(const char* tmpName);

    // removes all cached tmp-files beginning with 'name' (e.g. all files from one archive)
    // opened files will be marked as out-of-date, so that they will be restored when used again
    // (the current copy remains, so that the viewers don't yell at us)
    void FlushCache(const char* name);

    // removes cached file 'name'; the opened file will be marked as out-of-date, so that it will be
    // restored when used again (the current copy remains, so that the viewers don't yell at us);
    // returns TRUE if the file was found and removed
    BOOL FlushOneFile(const char* name);

    // search for the name in the array Names; returns TRUE if 'name' was found (returns also where - 'index');
    // returns FALSE if 'name' is not in Names (returns also where it could be inserted - 'index')
    BOOL GetNameIndex(const char* name, int& index);

    // counts how many tmp-files are contained in tmp-directory, which are deleted by the plugin 'ownDeletePlugin'
    int CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin);

    // performs premature deletion of all tmp-files, which are deleted by the plugin 'ownDeletePlugin';
    // used when unloading the plugin (the tmp-file is marked as deleted - once all links are closed,
    // deletion won't occur); if 'onlyDetach' is TRUE, it is not deleted, it's only marked
    // as deleted (the plugin is detached from the tmp-file)
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

    int Idle; // this is used to determine if we found anything during one search
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
    CRITICAL_SECTION Monitor;          // section used for synchronization of this object (behavior - monitor)
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

    // returns success of object initialization
    BOOL IsGood() { return Handles.IsGood() && Dirs.IsGood(); }

    // tries to find 'name' in cache; if it's found, waits until the tmp-file is prepared
    // (e.g. downloaded from FTP), then returns the tmp-file name and sets 'exists' to TRUE;
    // if it's found, but the tmp-file was deleted from disk, returns the tmp-file name and
    // 'exists' to FALSE, in this case it's necessary to prepare the tmp-file again and then
    // call NamePrepared(); if 'tmpName' is NULL, only find 'name' in cache (don't create new
    // tmp-file); if 'onlyAdd' is TRUE, only add to cache (if the tmp-file exists, returns
    // error; if the tmp-file was deleted from disk, restoring it is considered as adding,
    // so in this case it doesn't return error); if it's not found and 'tmpName' is not NULL,
    // creates a new name for the tmp-file, returns it immediately and sets 'exists' to FALSE,
    // in this case it's necessary to call NamePrepared() when the tmp-file is prepared (e.g.
    // downloaded from FTP) and then the tmp-file will be available to other threads;
    // WARNING: it's necessary to call AssignName() or ReleaseName()
    //
    // return value NULL -> "fatal error" ('exists' is TRUE) or in case 'tmpName' is NULL
    //                     "not found" ('exists' is FALSE), and in case 'onlyAdd' is TRUE
    //                     "file already exists" ('exists' is FALSE)
    //
    // name - unique item identification
    // tmpName - required name of the tmp-file or directory, a tmp-directory will be selected
    //           for it
    // exists - pointer to BOOL, which is set per the description above
    // onlyAdd - if it is TRUE, it is possible to create only a new name (if the name exists,
    //           returns NULL) or restore a deleted tmp-file (the name exists, but the tmp-file
    //           is not prepared)
    //
    // rootTmpPath - if NULL, the tmp-directory with the tmp-file should be placed into TEMP, otherwise
    //               it's the path where to place the tmp-directory with the tmp-file
    // ownDelete - if FALSE, tmp-files should be deleted using DeleteFile(), otherwise using
    //             DeleteManager (deletion using plugin - see ownDeletePlugin)
    // ownDeletePlugin - if ownDelete is TRUE, contains iface of the plugin, which should delete
    //                   the tmp-file
    // errorCode - if not NULL and an error occurs, its code is returned in this variable (for codes
    //             see DCGNE_XXX)
    const char* GetName(const char* name, const char* tmpName, BOOL* exists, BOOL onlyAdd,
                        const char* rootTmpPath, BOOL ownDelete,
                        CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode);

    // selects tmp-file related to 'name' for a valid one, provides it to other threads,
    // can be called only after GetName() returns 'exists' == FALSE
    //
    // returns success
    //
    // name - unique item identification
    // size - number of bytes occupied by the tmp-file on disk
    BOOL NamePrepared(const char* name, const CQuadWord& size);

    // assigns system object to the acquired tmp-file, using 'lock' the minimal lifetime
    // of the tmp-file is controlled (depends on 'remove')
    // can be called only in pair with GetName()
    //
    // returns success
    //
    // name - unique item identification (later for searching)
    // lock - when this object is "signaled", it will be possible to "release" the tmp-file
    //        (depends on 'remove')
    // lockOwner - should the cache take care of calling CloseHandle(lock) ?
    // remove - when to delete the tmp-file
    BOOL AssignName(const char* name, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove);

    // only called when after calling GetName() it is not possible to call NamePrepared() or AssignName()
    // it's present for the case of error when acquiring tmp-file or system object 'lock'
    // (running application for which the tmp-file was acquired)
    // in case that it was necessary to call NamePrepared(), it gives other threads the possibility
    // to create tmp-file (which are waiting until the tmp-file is prepared), in case that it was
    // necessary to call AssignName(), it cancels the tmp-file "waiting" for the system object 'lock',
    // tmp-file cancellation can occur only if 'storeInCache' is TRUE, tmp-file is prepared and
    // not locked, the tmp-file is marked as cached (if the maximum capacity of cache allows it,
    // it won't be deleted)
    //
    // returns success
    //
    // name - unique item identification (later for searching)
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

    // performs premature deletion of all tmp-files, which are deleted by the plugin 'ownDeletePlugin';
    // used when unloading the plugin (the tmp-file is marked as deleted - once all links are closed,
    // deletion won't occur); if 'onlyDetach' is TRUE, it is not deleted, it's only marked
    // as deleted (the plugin is detached from the tmp-file)
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);

    // the TEMP directory clean-up from the rest of previous instances; called only by the first instance
    // if it finds subdirectories "SAL*.tmp", it asks the user if he wants to delete them and if so,
    // it deletes them
    void ClearTEMPIfNeeded(HWND parent, HWND hActivePanel);

protected:
    void Enter() { HANDLES(EnterCriticalSection(&Monitor)); } // called before entering methods
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

    // file data, which should be deleted (in the main thread by calling the method
    // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy of the plugin)
    TIndirectArray<CDeleteManagerItem> Data;
    BOOL WaitingForProcessing; // TRUE = message to the main window is on the way or the data
                               // processing is in progress (the added item will be processed immediately)
    BOOL BlockDataProcessing;  // TRUE = do not process data (ProcessData() does nothing)

public:
    CDeleteManager();
    ~CDeleteManager();

    // adds file for deletion and ensures the closest call of the method
    // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy for file deletion;
    // in case of error, the file won't be deleted - the plugin should delete its
    // directory when loading/unloading (TEMP will be cleaned by Salamander)
    // can be called from any thread
    void AddFile(const char* fileName, CPluginInterfaceAbstract* plugin);

    // called from the main thread (calls main window after receiving the message WM_USER_PROCESSDELETEMAN);
    // processing of new data - deleting files in plugins
    void ProcessData();

    // unloading of the plugin imminent: let the plugin which is being unloaded delete tmp-copies from disk
    // (see CPluginInterfaceForArchiverAbstract::PrematureDeleteTmpCopy)
    void PluginMayBeUnloaded(HWND parent, CPluginData* plugin);

    // unload of the plugin finished: detach the plugin from delete-manager and disk-cache;
    // 'unloadedPlugin' is already invalid plugin interface (backup before unload)
    void PluginWasUnloaded(CPluginData* plugin, CPluginInterfaceAbstract* unloadedPlugin);
};

extern CDiskCache DiskCache;         // global disk-cache
extern CDeleteManager DeleteManager; // global disk-cache delete-manager (deleting tmp-files in archivers plugins)
