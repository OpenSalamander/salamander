// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// mutex for accessing the shared memory
extern HANDLE SalShExtSharedMemMutex;
// shared memory - see the CSalShExtSharedMem structure
extern HANDLE SalShExtSharedMem;
// event for sending a request to perform Paste in the source Salamander (used only on Vista+)
extern HANDLE SalShExtDoPasteEvent;
// mapped shared memory - see the CSalShExtSharedMem structure
extern CSalShExtSharedMem* SalShExtSharedMemView;

// TRUE if registering SalShExt/SalExten/SalamExt/SalExtX86/SalExtX64.DLL succeeded or it was already registered
extern BOOL SalShExtRegistered;

// utter hack: we need to find out into which window the Drop will happen,
// we detect it in GetData based on the mouse position; this variable stores the last test result
extern HWND LastWndFromGetData;

// utter hack: we need to find out into which window the Paste will happen
// we detect it in GetData based on the foreground window; this variable stores the last test result
extern HWND LastWndFromPasteGetData;

extern BOOL OurDataOnClipboard; // TRUE = our data object is on the clipboard (copy & paste from an archive)

//*****************************************************************************

// call before using the library
void InitSalShLib();

// call to release the library
void ReleaseSalShLib();

// returns TRUE if the data object contains only a "fake" directory; in 'fakeType' (if not NULL) it returns
// 1 if the source is an archive and 2 if the source is the filesystem; if the source is the filesystem and 'srcFSPathBuf' is not NULL,
// it returns the source filesystem path ('srcFSPathBufSize' is the size of the 'srcFSPathBuf' buffer)
BOOL IsFakeDataObject(IDataObject* pDataObject, int* fakeType, char* srcFSPathBuf, int srcFSPathBufSize);

//
//*****************************************************************************
// CFakeDragDropDataObject
//
// data object used to detect the target of a drag & drop operation (used during
// extraction from an archive and when copying from a plug-in file system),
// it encapsulates the Windows data object obtained for the "fake" directory and adds
// the SALCF_FAKE_REALPATH format (defines the path that should appear after the drop
// in the directory line, command line + blocks dropping to the usermenu toolbar),
// SALCF_FAKE_SRCTYPE (source type - 1 = archive, 2 = filesystem) and for the filesystem
// also SALCF_FAKE_SRCFSPATH (source filesystem path) into GetData()

class CFakeDragDropDataObject : public IDataObject
{
private:
    long RefCount;
    IDataObject* WinDataObject;   // encapsulated data object
    char RealPath[2 * MAX_PATH];  // path for dropping into the directory and command line
    int SrcType;                  // source type (1 = archive, 2 = filesystem)
    char SrcFSPath[2 * MAX_PATH]; // only for filesystem sources: source filesystem path
    UINT CFSalFakeRealPath;       // clipboard format for sal-fake-real-path
    UINT CFSalFakeSrcType;        // clipboard format for sal-fake-src-type
    UINT CFSalFakeSrcFSPath;      // clipboard format for sal-fake-src-fs-path

public:
    CFakeDragDropDataObject(IDataObject* winDataObject, const char* realPath, int srcType,
                            const char* srcFSPath)
    {
        RefCount = 1;
        WinDataObject = winDataObject;
        WinDataObject->AddRef();
        lstrcpyn(RealPath, realPath, 2 * MAX_PATH);
        if (srcFSPath != NULL && srcType == 2 /* FS */)
            lstrcpyn(SrcFSPath, srcFSPath, 2 * MAX_PATH);
        else
            SrcFSPath[0] = 0;
        SrcType = srcType;
        CFSalFakeRealPath = RegisterClipboardFormat(SALCF_FAKE_REALPATH);
        CFSalFakeSrcType = RegisterClipboardFormat(SALCF_FAKE_SRCTYPE);
        CFSalFakeSrcFSPath = RegisterClipboardFormat(SALCF_FAKE_SRCFSPATH);
    }

    virtual ~CFakeDragDropDataObject()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
        WinDataObject->Release();
    }

    STDMETHOD(QueryInterface)
    (REFIID, void FAR* FAR*);
    STDMETHOD_(ULONG, AddRef)
    (void) { return ++RefCount; }
    STDMETHOD_(ULONG, Release)
    (void)
    {
        if (--RefCount == 0)
        {
            delete this;
            return 0; // we must not touch the object, it no longer exists
        }
        return RefCount;
    }

    STDMETHOD(GetData)
    (FORMATETC* formatEtc, STGMEDIUM* medium);

    STDMETHOD(GetDataHere)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium)
    {
        return WinDataObject->GetDataHere(pFormatetc, pmedium);
    }

    STDMETHOD(QueryGetData)
    (FORMATETC* formatEtc)
    {
        if (formatEtc->cfFormat == CF_HDROP)
            return DV_E_FORMATETC; // this ensures a "NO" drop in simpler software (BOSS, WinCmd, SpeedCommander, MSIE, Word, etc.)
        return WinDataObject->QueryGetData(formatEtc);
    }

    STDMETHOD(GetCanonicalFormatEtc)
    (FORMATETC* pFormatetcIn, FORMATETC* pFormatetcOut)
    {
        return WinDataObject->GetCanonicalFormatEtc(pFormatetcIn, pFormatetcOut);
    }

    STDMETHOD(SetData)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium, BOOL fRelease)
    {
        return WinDataObject->SetData(pFormatetc, pmedium, fRelease);
    }

    STDMETHOD(EnumFormatEtc)
    (DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc)
    {
        return WinDataObject->EnumFormatEtc(dwDirection, ppenumFormatetc);
    }

    STDMETHOD(DAdvise)
    (FORMATETC* pFormatetc, DWORD advf, IAdviseSink* pAdvSink,
     DWORD* pdwConnection)
    {
        return WinDataObject->DAdvise(pFormatetc, advf, pAdvSink, pdwConnection);
    }

    STDMETHOD(DUnadvise)
    (DWORD dwConnection)
    {
        return WinDataObject->DUnadvise(dwConnection);
    }

    STDMETHOD(EnumDAdvise)
    (IEnumSTATDATA** ppenumAdvise)
    {
        return WinDataObject->EnumDAdvise(ppenumAdvise);
    }
};

//
//*****************************************************************************
// CSalShExtPastedData
//
// data for Paste from the clipboard stored inside the "source" Salamander

class CSalamanderDirectory;

class CSalShExtPastedData
{
protected:
    DWORD DataID; // version of data stored for Paste from the clipboard

    BOOL Lock; // TRUE = it is locked against deletion, FALSE = not locked

    char ArchiveFileName[MAX_PATH]; // full path to the archive
    char PathInArchive[MAX_PATH];   // path inside the archive where Copy to the clipboard happened
    CNames SelFilesAndDirs;         // names of files and directories from PathInArchive that will be extracted

    CSalamanderDirectory* StoredArchiveDir;             // stored archive structure (used if the archive is not open in the panel)
    CPluginDataInterfaceEncapsulation StoredPluginData; // stored plug-in data interface of the archive (used if the archive is not open in the panel)
    FILETIME StoredArchiveDate;                         // archive file`s date (for testing the validity of the archive listing)
    CQuadWord StoredArchiveSize;                        // archive file`s size (for testing the validity of the archive listing)

public:
    CSalShExtPastedData();
    ~CSalShExtPastedData();

    DWORD GetDataID() { return DataID; }
    void SetDataID(DWORD dataID) { DataID = dataID; }

    BOOL IsLocked() { return Lock; }
    void SetLock(BOOL lock) { Lock = lock; }

    // sets the object's data, returns TRUE on success; on failure leaves the object empty
    // and returns FALSE
    BOOL SetData(const char* archiveFileName, const char* pathInArchive, CFilesArray* files,
                 CFilesArray* dirs, BOOL namesAreCaseSensitive, int* selIndexes,
                 int selIndexesCount);

    // clears the data stored in StoredArchiveDir and StoredPluginData
    void ReleaseStoredArchiveData();

    // clears the object (removes all its data, the object remains ready for further use)
    void Clear();

    // performs the paste operation with the current data; 'copy' is TRUE if the data should be copied,
    // FALSE if they should be moved; 'tgtPath' is the target disk path of the operation
    void DoPasteOperation(BOOL copy, const char* tgtPath);

    // if the provided data suit the object, it keeps them and returns TRUE; otherwise it returns
    // FALSE (the provided data will then be released)
    BOOL WantData(const char* archiveFileName, CSalamanderDirectory* archiveDir,
                  CPluginDataInterfaceEncapsulation pluginData,
                  FILETIME archiveDate, CQuadWord archiveSize);

    // returns TRUE if it is possible to unload the plugin 'plugin'; if the object contains
    // data for the plug-in 'plugin', it tries to get rid of them so it can return TRUE
    BOOL CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin);
};

// data for Paste from the clipboard stored inside the "source" Salamander
extern CSalShExtPastedData SalShExtPastedData;

//
//*****************************************************************************
// CFakeCopyPasteDataObject
//
// data object used to detect the target of a copy & paste operation (used when
// extracting from an archive), it encapsulates the Windows data object obtained for the "fake"
// directory and ensures the "fake" directory is deleted from disk after the object is released from
// the clipboard

class CFakeCopyPasteDataObject : public IDataObject
{
private:
    long RefCount;
    IDataObject* WinDataObject; // encapsulated data object
    char FakeDir[MAX_PATH];     // "fake" directory
    UINT CFSalFakeRealPath;     // clipboard format for sal-fake-real-path
    UINT CFIdList;              // clipboard format for the shell ID list (Explorer uses it instead of the simpler CF_HDROP)

    DWORD LastGetDataCallTime; // time of the last GetData() call
    BOOL CutOrCopyDone;        // FALSE = the object is still being placed on the clipboard; Release does nothing until CutOrCopyDone is TRUE

public:
    CFakeCopyPasteDataObject(IDataObject* winDataObject, const char* fakeDir)
    {
        RefCount = 1;
        WinDataObject = winDataObject;
        WinDataObject->AddRef();
        lstrcpyn(FakeDir, fakeDir, MAX_PATH);
        CFSalFakeRealPath = RegisterClipboardFormat(SALCF_FAKE_REALPATH);
        CFIdList = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
        LastGetDataCallTime = GetTickCount() - 60000; // initialize to 1 minute before the object is created
        CutOrCopyDone = FALSE;
    }

    virtual ~CFakeCopyPasteDataObject()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
        WinDataObject->Release();
    }

    void SetCutOrCopyDone() { CutOrCopyDone = TRUE; }

    STDMETHOD(QueryInterface)
    (REFIID, void FAR* FAR*);
    STDMETHOD_(ULONG, AddRef)
    (void)
    {
        //      TRACE_I("AddRef");
        return ++RefCount;
    }
    STDMETHOD_(ULONG, Release)
    (void);

    STDMETHOD(GetData)
    (FORMATETC* formatEtc, STGMEDIUM* medium);

    STDMETHOD(GetDataHere)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium)
    {
        //      TRACE_I("GetDataHere");
        return WinDataObject->GetDataHere(pFormatetc, pmedium);
    }

    STDMETHOD(QueryGetData)
    (FORMATETC* formatEtc)
    {
        //      TRACE_I("QueryGetData");
        if (formatEtc->cfFormat == CF_HDROP)
            return DV_E_FORMATETC; // this enforces a "NO" drop in simpler softwares (BOSS, WinCmd, SpeedCommander, MSIE, Word, etc.)
        return WinDataObject->QueryGetData(formatEtc);
    }

    STDMETHOD(GetCanonicalFormatEtc)
    (FORMATETC* pFormatetcIn, FORMATETC* pFormatetcOut)
    {
        //      TRACE_I("GetCanonicalFormatEtc");
        return WinDataObject->GetCanonicalFormatEtc(pFormatetcIn, pFormatetcOut);
    }

    STDMETHOD(SetData)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium, BOOL fRelease)
    {
        //      TRACE_I("SetData");
        return WinDataObject->SetData(pFormatetc, pmedium, fRelease);
    }

    STDMETHOD(EnumFormatEtc)
    (DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc)
    {
        //      TRACE_I("EnumFormatEtc");
        return WinDataObject->EnumFormatEtc(dwDirection, ppenumFormatetc);
    }

    STDMETHOD(DAdvise)
    (FORMATETC* pFormatetc, DWORD advf, IAdviseSink* pAdvSink,
     DWORD* pdwConnection)
    {
        //      TRACE_I("DAdvise");
        return WinDataObject->DAdvise(pFormatetc, advf, pAdvSink, pdwConnection);
    }

    STDMETHOD(DUnadvise)
    (DWORD dwConnection)
    {
        //      TRACE_I("DUnadvise");
        return WinDataObject->DUnadvise(dwConnection);
    }

    STDMETHOD(EnumDAdvise)
    (IEnumSTATDATA** ppenumAdvise)
    {
        //      TRACE_I("EnumDAdvise");
        return WinDataObject->EnumDAdvise(ppenumAdvise);
    }
};
