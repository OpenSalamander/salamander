// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "dialogs.h"
#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "zip.h"
#include "pack.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"

// mutex for access to shared memory
HANDLE SalShExtSharedMemMutex = NULL;
// shared memory — ee CSalShExtSharedMem
HANDLE SalShExtSharedMem = NULL;
// event used to send a request to perform Paste in the source Salamander (used only on Vista+)
HANDLE SalShExtDoPasteEvent = NULL;
// mapped shared memory — see the CSalShExtSharedMem structure
CSalShExtSharedMem* SalShExtSharedMemView = NULL;

// TRUE if SalShExt/SalamExt/SalExtX86/SalExtX64.DLL registered successfully or was already registered
// (also verifies the file)
BOOL SalShExtRegistered = FALSE;

// extreme hack: we need to find out which window Drop will target, we determine this
// in GetData based on the mouse position; this variable holds the last test result
HWND LastWndFromGetData = NULL;

// extreme hack: we need to find out which window Paste will target, we determine this
// in GetData based on the foreground window; this variable holds the last test result
HWND LastWndFromPasteGetData = NULL;

BOOL OurDataOnClipboard = FALSE; // TRUE = our data object is currently on the clipboard (copy & paste from the archive)

// data used for Paste from the clipboard stored inside the "source" Salamander
CSalShExtPastedData SalShExtPastedData;

//*****************************************************************************

void InitSalShLib()
{
    CALL_STACK_MESSAGE1("InitSalShLib()");
    PSID psidEveryone;
    PACL paclNewDacl;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES* saPtr = CreateAccessableSecurityAttributes(&sa, &sd, GENERIC_ALL, &psidEveryone, &paclNewDacl);

    SalShExtSharedMemMutex = HANDLES_Q(CreateMutex(saPtr, FALSE, SALSHEXT_SHAREDMEMMUTEXNAME));
    if (SalShExtSharedMemMutex == NULL)
        SalShExtSharedMemMutex = HANDLES_Q(OpenMutex(SYNCHRONIZE, FALSE, SALSHEXT_SHAREDMEMMUTEXNAME));
    if (SalShExtSharedMemMutex != NULL)
    {
        WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
        SalShExtSharedMem = HANDLES_Q(CreateFileMapping(INVALID_HANDLE_VALUE, saPtr, PAGE_READWRITE, // FIXME_X64 are we passing incompatible x86/x64 data?
                                                        0, sizeof(CSalShExtSharedMem),
                                                        SALSHEXT_SHAREDMEMNAME));
        BOOL created;
        if (SalShExtSharedMem == NULL)
        {
            SalShExtSharedMem = HANDLES_Q(OpenFileMapping(FILE_MAP_WRITE, FALSE, SALSHEXT_SHAREDMEMNAME));
            created = FALSE;
        }
        else
        {
            created = (GetLastError() != ERROR_ALREADY_EXISTS);
        }

        if (SalShExtSharedMem != NULL)
        {
            SalShExtSharedMemView = (CSalShExtSharedMem*)HANDLES(MapViewOfFile(SalShExtSharedMem, // FIXME_X64 are we passing incompatible x86/x64 data?
                                                                               FILE_MAP_WRITE, 0, 0, 0));
            if (SalShExtSharedMemView != NULL)
            {
                if (created)
                {
                    memset(SalShExtSharedMemView, 0, sizeof(CSalShExtSharedMem)); // it should already be zeroed, but we do not count on it
                    SalShExtSharedMemView->Size = sizeof(CSalShExtSharedMem);
                }
            }
            else
                TRACE_E("InitSalShLib(): unable to map view of shared memory!");
        }
        else
            TRACE_E("InitSalShLib(): unable to create shared memory!");
        ReleaseMutex(SalShExtSharedMemMutex);
    }
    else
        TRACE_E("InitSalShLib(): unable to create mutex object for access to shared memory!");

    if (psidEveryone != NULL)
        FreeSid(psidEveryone);
    if (paclNewDacl != NULL)
        LocalFree(paclNewDacl);
}

void ReleaseSalShLib()
{
    CALL_STACK_MESSAGE1("ReleaseSalShLib()");
    if (OurDataOnClipboard)
    {
        OleSetClipboard(NULL);      // remove our data object from the clipboard
        OurDataOnClipboard = FALSE; // theoretically redundant (it should be set in fakeDataObject’s Release())
    }
    if (SalShExtSharedMemView != NULL)
        HANDLES(UnmapViewOfFile(SalShExtSharedMemView));
    SalShExtSharedMemView = NULL;
    if (SalShExtSharedMem != NULL)
        HANDLES(CloseHandle(SalShExtSharedMem));
    SalShExtSharedMem = NULL;
    if (SalShExtSharedMemMutex != NULL)
        HANDLES(CloseHandle(SalShExtSharedMemMutex));
    SalShExtSharedMemMutex = NULL;
}

BOOL IsFakeDataObject(IDataObject* pDataObject, int* fakeType, char* srcFSPathBuf, int srcFSPathBufSize)
{
    CALL_STACK_MESSAGE1("IsFakeDataObject()");
    if (fakeType != NULL)
        *fakeType = 0;
    if (srcFSPathBuf != NULL && srcFSPathBufSize > 0)
        srcFSPathBuf[0] = 0;

    FORMATETC formatEtc;
    formatEtc.cfFormat = RegisterClipboardFormat(SALCF_FAKE_REALPATH);
    formatEtc.ptd = NULL;
    formatEtc.dwAspect = DVASPECT_CONTENT;
    formatEtc.lindex = -1;
    formatEtc.tymed = TYMED_HGLOBAL;

    STGMEDIUM stgMedium;
    stgMedium.tymed = TYMED_HGLOBAL;
    stgMedium.hGlobal = NULL;
    stgMedium.pUnkForRelease = NULL;

    if (pDataObject != NULL && pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
    {
        if (stgMedium.tymed != TYMED_HGLOBAL || stgMedium.hGlobal != NULL)
            ReleaseStgMedium(&stgMedium);

        if (fakeType != NULL || srcFSPathBuf != NULL && srcFSPathBufSize > 0)
        {
            formatEtc.cfFormat = RegisterClipboardFormat(SALCF_FAKE_SRCTYPE);
            formatEtc.ptd = NULL;
            formatEtc.dwAspect = DVASPECT_CONTENT;
            formatEtc.lindex = -1;
            formatEtc.tymed = TYMED_HGLOBAL;

            stgMedium.tymed = TYMED_HGLOBAL;
            stgMedium.hGlobal = NULL;
            stgMedium.pUnkForRelease = NULL;

            BOOL isFS = FALSE;
            if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
            {
                if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
                {
                    int* data = (int*)HANDLES(GlobalLock(stgMedium.hGlobal));
                    if (data != NULL)
                    {
                        isFS = *data == 2;
                        if (fakeType != NULL)
                            *fakeType = *data;
                        HANDLES(GlobalUnlock(stgMedium.hGlobal));
                    }
                }
                if (stgMedium.tymed != TYMED_HGLOBAL || stgMedium.hGlobal != NULL)
                    ReleaseStgMedium(&stgMedium);
            }

            if (isFS && srcFSPathBuf != NULL && srcFSPathBufSize > 0)
            {
                formatEtc.cfFormat = RegisterClipboardFormat(SALCF_FAKE_SRCFSPATH);
                formatEtc.ptd = NULL;
                formatEtc.dwAspect = DVASPECT_CONTENT;
                formatEtc.lindex = -1;
                formatEtc.tymed = TYMED_HGLOBAL;

                stgMedium.tymed = TYMED_HGLOBAL;
                stgMedium.hGlobal = NULL;
                stgMedium.pUnkForRelease = NULL;
                if (pDataObject->GetData(&formatEtc, &stgMedium) == S_OK)
                {
                    if (stgMedium.tymed == TYMED_HGLOBAL && stgMedium.hGlobal != NULL)
                    {
                        char* data = (char*)HANDLES(GlobalLock(stgMedium.hGlobal));
                        if (data != NULL)
                        {
                            lstrcpyn(srcFSPathBuf, data, srcFSPathBufSize);
                            HANDLES(GlobalUnlock(stgMedium.hGlobal));
                        }
                    }
                    if (stgMedium.tymed != TYMED_HGLOBAL || stgMedium.hGlobal != NULL)
                        ReleaseStgMedium(&stgMedium);
                }
            }
        }
        return TRUE;
    }
    return FALSE;
}

//
//*****************************************************************************
// CFakeDragDropDataObject
//

STDMETHODIMP CFakeDragDropDataObject::QueryInterface(REFIID iid, void** ppv)
{
    if (iid == IID_IUnknown || iid == IID_IDataObject)
    {
        *ppv = this;
        AddRef();
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP CFakeDragDropDataObject::GetData(FORMATETC* formatEtc, STGMEDIUM* medium)
{
    CALL_STACK_MESSAGE1("CFakeDragDropDataObject::GetData()");
    // TRACE_I("CFakeDragDropDataObject::GetData():" << formatEtc->cfFormat);
    if (formatEtc == NULL || medium == NULL)
        return E_INVALIDARG;

    POINT pt;
    GetCursorPos(&pt);
    LastWndFromGetData = WindowFromPoint(pt);

    if (formatEtc->cfFormat == CFSalFakeRealPath && (formatEtc->tymed & TYMED_HGLOBAL))
    {
        HGLOBAL dataDup = NULL; // create a copy of RealPath
        int size = (int)strlen(RealPath) + 1;
        dataDup = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size));
        if (dataDup != NULL)
        {
            void* ptr1 = HANDLES(GlobalLock(dataDup));
            if (ptr1 != NULL)
            {
                memcpy(ptr1, RealPath, size);
                HANDLES(GlobalUnlock(dataDup));
            }
            else
            {
                NOHANDLES(GlobalFree(dataDup));
                dataDup = NULL;
            }
        }
        if (dataDup != NULL) // we have data, store it on the medium and return it
        {
            medium->tymed = TYMED_HGLOBAL;
            medium->hGlobal = dataDup;
            medium->pUnkForRelease = NULL;
            return S_OK;
        }
        else
            return E_UNEXPECTED;
    }
    else
    {
        if (formatEtc->cfFormat == CFSalFakeSrcType && (formatEtc->tymed & TYMED_HGLOBAL))
        {
            HGLOBAL dataDup = NULL;
            dataDup = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(int)));
            if (dataDup != NULL)
            {
                BOOL ok = FALSE;
                int* ptr1 = (int*)HANDLES(GlobalLock(dataDup));
                if (ptr1 != NULL)
                {
                    *ptr1 = SrcType;
                    ok = TRUE;
                }
                if (ptr1 != NULL)
                    HANDLES(GlobalUnlock(dataDup));
                if (!ok)
                {
                    NOHANDLES(GlobalFree(dataDup));
                    dataDup = NULL;
                }
            }
            if (dataDup != NULL) // we have data, store it on the medium and return it
            {
                medium->tymed = TYMED_HGLOBAL;
                medium->hGlobal = dataDup;
                medium->pUnkForRelease = NULL;
                return S_OK;
            }
            else
                return E_UNEXPECTED;
        }
        else
        {
            if (formatEtc->cfFormat == CFSalFakeSrcFSPath && (formatEtc->tymed & TYMED_HGLOBAL))
            {
                HGLOBAL dataDup = NULL; // create a copy of SrcFSPath
                int size = (int)strlen(SrcFSPath) + 1;
                dataDup = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, size));
                if (dataDup != NULL)
                {
                    void* ptr1 = HANDLES(GlobalLock(dataDup));
                    if (ptr1 != NULL)
                    {
                        memcpy(ptr1, SrcFSPath, size);
                        HANDLES(GlobalUnlock(dataDup));
                    }
                    else
                    {
                        NOHANDLES(GlobalFree(dataDup));
                        dataDup = NULL;
                    }
                }
                if (dataDup != NULL) // we have data, store it on the medium and return it
                {
                    medium->tymed = TYMED_HGLOBAL;
                    medium->hGlobal = dataDup;
                    medium->pUnkForRelease = NULL;
                    return S_OK;
                }
                else
                    return E_UNEXPECTED;
            }
            else
                return WinDataObject->GetData(formatEtc, medium);
        }
    }
}

//
//*****************************************************************************
// CFakeCopyPasteDataObject
//

STDMETHODIMP CFakeCopyPasteDataObject::QueryInterface(REFIID iid, void** ppv)
{
    //  TRACE_I("QueryInterface");
    if (iid == IID_IUnknown || iid == IID_IDataObject)
    {
        *ppv = this;
        AddRef();
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG)
CFakeCopyPasteDataObject::Release(void)
{
    CALL_STACK_MESSAGE1("CFakeCopyPasteDataObject::Release()");
    //  TRACE_I("CFakeCopyPasteDataObject::Release(): " << RefCount - 1);
    if (--RefCount == 0)
    {
        OurDataOnClipboard = FALSE;

        if (CutOrCopyDone) // if an error occurred during cut/copy, waiting makes no sense and we perform the cleanup elsewhere
        {
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): deleting clipfake directory!");

            // now we can cancel the "paste" in shared memory, clean up the fake dir, and remove the data object
            if (SalShExtSharedMemView != NULL) // store the timestamp in shared memory (to distinguish between paste and another copy/move of the fake dir)
            {
                //        TRACE_I("CFakeCopyPasteDataObject::Release(): DoPasteFromSalamander = FALSE");
                WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                SalShExtSharedMemView->DoPasteFromSalamander = FALSE;
                SalShExtSharedMemView->PasteFakeDirName[0] = 0;
                ReleaseMutex(SalShExtSharedMemMutex);
            }
            char dir[MAX_PATH];
            lstrcpyn(dir, FakeDir, MAX_PATH);
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): removedir");
            char* cutDir;
            if (CutDirectory(dir, &cutDir) && cutDir != NULL && strcmp(cutDir, "CLIPFAKE") == 0)
            { // just to be sure, check that we really delete only the fake dir
                RemoveTemporaryDir(dir);
            }
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): posting WM_USER_SALSHEXT_TRYRELDATA");
            if (MainWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // we attempt to release the data (if it is neither locked nor blocked)
        }

        delete this;
        return 0; // we must not touch the object; it no longer exists
    }
    return RefCount;
}

STDMETHODIMP CFakeCopyPasteDataObject::GetData(FORMATETC* formatEtc, STGMEDIUM* medium)
{
    CALL_STACK_MESSAGE1("CFakeCopyPasteDataObject::GetData()");
    //  char buf[300];
    //  if (!GetClipboardFormatName(formatEtc->cfFormat, buf, 300)) buf[0] = 0;
    //  TRACE_I("CFakeCopyPasteDataObject::GetData():" << formatEtc->cfFormat << " (" << buf << ")");
    if (formatEtc == NULL || medium == NULL)
        return E_INVALIDARG;
    if (formatEtc->cfFormat == CFSalFakeRealPath && (formatEtc->tymed & TYMED_HGLOBAL))
    {
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = NULL;
        medium->pUnkForRelease = NULL;
        return S_OK; // return S_OK to satisfy the test in the IsFakeDataObject() function
    }
    else
    {
        if (formatEtc->cfFormat == CFIdList)
        { // Paste into Explorer uses this format; the others do not matter (they do not use the copy hook anyway)
            // solves a problem in Win98: when copying to the clipboard from Explorer, GetData is called on the existing object on the clipboard
            // only afterwards, it is released and replaced with a new object from Explorer (the problem is a 2-second
            // timeout due to waiting for the copy hook callback — we always expect it after GetData)
            DWORD ti = GetTickCount();
            if (ti - LastGetDataCallTime >= 100) // optimization: store a new time only if it changes by at least 100 ms
            {
                LastGetDataCallTime = ti;
                if (SalShExtSharedMemView != NULL) // store the timestamp in shared memory (to distinguish between paste and another copy/move of the fake dir)
                {
                    WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                    SalShExtSharedMemView->ClipDataObjLastGetDataTime = ti;
                    ReleaseMutex(SalShExtSharedMemMutex);
                }
            }

            LastWndFromPasteGetData = GetForegroundWindow();
        }
        return WinDataObject->GetData(formatEtc, medium);
    }
}

//
//*****************************************************************************
// CSalShExtPastedData
//

CSalShExtPastedData::CSalShExtPastedData()
{
    DataID = -1;
    Lock = FALSE;
    ArchiveFileName[0] = 0;
    PathInArchive[0] = 0;
    StoredArchiveDir = NULL;
    memset(&StoredArchiveDate, 0, sizeof(StoredArchiveDate));
    StoredArchiveSize.Set(0, 0);
}

CSalShExtPastedData::~CSalShExtPastedData()
{
    if (StoredArchiveDir != NULL)
        TRACE_E("CSalShExtPastedData::~CSalShExtPastedData(): unexpected situation: StoredArchiveDir is not empty!");
    Clear();
}

BOOL CSalShExtPastedData::SetData(const char* archiveFileName, const char* pathInArchive, CFilesArray* files,
                                  CFilesArray* dirs, BOOL namesAreCaseSensitive, int* selIndexes,
                                  int selIndexesCount)
{
    CALL_STACK_MESSAGE1("CSalShExtPastedData::SetData()");

    Clear();

    LastWndFromPasteGetData = NULL; // clear it here for the first Paste

    lstrcpyn(ArchiveFileName, archiveFileName, MAX_PATH);
    lstrcpyn(PathInArchive, pathInArchive, MAX_PATH);
    SelFilesAndDirs.SetCaseSensitive(namesAreCaseSensitive);
    int i;
    for (i = 0; i < selIndexesCount; i++)
    {
        int index = selIndexes[i];
        if (index < dirs->Count) // it is a directory
        {
            if (!SelFilesAndDirs.Add(TRUE, dirs->At(index).Name))
                break;
        }
        else // it is a file
        {
            if (!SelFilesAndDirs.Add(FALSE, files->At(index - dirs->Count).Name))
                break;
        }
    }
    if (i < selIndexesCount) // ran out of memory
    {
        Clear();
        return FALSE;
    }
    else
        return TRUE;
}

void CSalShExtPastedData::Clear()
{
    CALL_STACK_MESSAGE1("CSalShExtPastedData::Clear()");
    //  TRACE_I("CSalShExtPastedData::Clear()");
    DataID = -1;
    ArchiveFileName[0] = 0;
    PathInArchive[0] = 0;
    SelFilesAndDirs.Clear();
    ReleaseStoredArchiveData();
}

void CSalShExtPastedData::ReleaseStoredArchiveData()
{
    CALL_STACK_MESSAGE1("CSalShExtPastedData::ReleaseStoredArchiveData()");

    if (StoredArchiveDir != NULL)
    {
        if (StoredPluginData.NotEmpty())
        {
            // release the plug-in data for individual files and directories
            BOOL releaseFiles = StoredPluginData.CallReleaseForFiles();
            BOOL releaseDirs = StoredPluginData.CallReleaseForDirs();
            if (releaseFiles || releaseDirs)
                StoredArchiveDir->ReleasePluginData(StoredPluginData, releaseFiles, releaseDirs);

            // release the StoredPluginData interface
            CPluginInterfaceEncapsulation plugin(StoredPluginData.GetPluginInterface(), StoredPluginData.GetBuiltForVersion());
            plugin.ReleasePluginDataInterface(StoredPluginData.GetInterface());
        }
        StoredArchiveDir->Clear(NULL); // release the "standard" (Salamander) listing data
        delete StoredArchiveDir;
    }
    StoredArchiveDir = NULL;
    StoredPluginData.Init(NULL, NULL, NULL, NULL, 0);
}

BOOL CSalShExtPastedData::WantData(const char* archiveFileName, CSalamanderDirectory* archiveDir,
                                   CPluginDataInterfaceEncapsulation pluginData,
                                   FILETIME archiveDate, CQuadWord archiveSize)
{
    CALL_STACK_MESSAGE1("CSalShExtPastedData::WantData()");

    if (!Lock /* should not happen, but we play it safe */ &&
        StrICmp(ArchiveFileName, archiveFileName) == 0 &&
        archiveSize != CQuadWord(-1, -1) && // a corrupted date & time mark indicates an archive that must be reloaded
        (!pluginData.NotEmpty() || pluginData.CanBeCopiedToClipboard()))
    {
        ReleaseStoredArchiveData();
        StoredArchiveDir = archiveDir;
        StoredPluginData = pluginData;
        StoredArchiveDate = archiveDate;
        StoredArchiveSize = archiveSize;
        return TRUE;
    }
    return FALSE;
}

BOOL CSalShExtPastedData::CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE1("CSalShExtPastedData::CanUnloadPlugin()");

    BOOL used = FALSE;
    if (StoredPluginData.NotEmpty() && StoredPluginData.GetPluginInterface() == plugin)
        used = TRUE;
    else
    {
        if (ArchiveFileName[0] != 0)
        {
            // find out whether the unloaded plug-in has anything to do with our archive;
            // the plug-in could unload itself while the archiver is used (each archiver function
            // loads the plug-in itself), but better safe than sorry, so we cancel any pedning archive listing
            int format = PackerFormatConfig.PackIsArchive(ArchiveFileName);
            if (format != 0) // we found a supported archive
            {
                format--;
                CPluginData* data;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0) // view: is it processed internally (plug-in)?
                {
                    data = Plugins.Get(-index - 1);
                    if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                        used = TRUE;
                }
                if (PackerFormatConfig.GetUsePacker(format)) // does it have an editor?
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0) // is it processed internally (plug-in)?
                    {
                        data = Plugins.Get(-index - 1);
                        if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                            used = TRUE;
                    }
                }
            }
        }
    }

    if (used)
        ReleaseStoredArchiveData(); // we are using plug-in data, so we should release them
    return TRUE;                    // unloading the plug-in is possible
}

void CSalShExtPastedData::DoPasteOperation(BOOL copy, const char* tgtPath)
{
    CALL_STACK_MESSAGE1("CSalShExtPastedData::DoPasteOperation()");
    if (ArchiveFileName[0] == 0 || SelFilesAndDirs.GetCount() == 0)
    {
        TRACE_E("CSalShExtPastedData::DoPasteOperation(): empty data, nothing to do!");
        return;
    }
    if (MainWindow == NULL || MainWindow->LeftPanel == NULL || MainWindow->RightPanel == NULL)
    {
        TRACE_E("CSalShExtPastedData::DoPasteOperation(): unexpected situation!");
        return;
    }

    BeginStopRefresh(); // pause the snooper

    char text[1000];
    CSalamanderDirectory* archiveDir = NULL;
    CPluginDataInterfaceAbstract* pluginData = NULL;
    for (int j = 0; j < 2; j++)
    {
        CFilesWindow* panel = j == 0 ? MainWindow->GetActivePanel() : MainWindow->GetNonActivePanel();
        if (panel->Is(ptZIPArchive) && StrICmp(ArchiveFileName, panel->GetZIPArchive()) == 0)
        { // the panel contains our archive
            BOOL archMaybeUpdated;
            panel->OfferArchiveUpdateIfNeeded(MainWindow->HWindow, IDS_ARCHIVECLOSEEDIT2, &archMaybeUpdated);
            if (archMaybeUpdated)
            {
                EndStopRefresh(); // the snooper starts now
                return;
            }
            // reuse the data from the panel (we are in the main thread, the panel cannot change during the operation)
            archiveDir = panel->GetArchiveDir();
            pluginData = panel->PluginData.GetInterface();
            break;
        }
    }

    if (StoredArchiveDir != NULL) // if we have any archive data stored
    {
        if (archiveDir != NULL)
            ReleaseStoredArchiveData(); // the archive is open in a panel, discard the stored data
        else                            // try to use the stored data, check the archive file`s size and date
        {
            BOOL canUseData = FALSE;
            FILETIME archiveDate;  // archive file`s date & time
            CQuadWord archiveSize; // archive file`s size
            HANDLE file = HANDLES_Q(CreateFile(ArchiveFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileTime(file, NULL, NULL, &archiveDate);
                DWORD err = NO_ERROR;
                SalGetFileSize(file, archiveSize, err); // returns "success?" — ignore it, we test 'err' later
                HANDLES(CloseHandle(file));

                if (err == NO_ERROR &&                                        // size & date are obtained and
                    CompareFileTime(&archiveDate, &StoredArchiveDate) == 0 && // the date is identical and
                    archiveSize == StoredArchiveSize)                         // the size is identical as well
                {
                    canUseData = TRUE;
                }
            }
            if (canUseData)
            {
                archiveDir = StoredArchiveDir;
                pluginData = StoredPluginData.GetInterface();
            }
            else
                ReleaseStoredArchiveData(); // the archive file changed, discard the stored data
        }
    }

    if (archiveDir == NULL) // we have no data, we must list the archive again
    {
        CSalamanderDirectory* newArchiveDir = new CSalamanderDirectory(FALSE);
        if (newArchiveDir == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            // find information about the file (does it exist? size, date, and time)
            DWORD err = NO_ERROR;
            FILETIME archiveDate;  // archive file`s date & time
            CQuadWord archiveSize; // archive file`s size
            HANDLE file = HANDLES_Q(CreateFile(ArchiveFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileTime(file, NULL, NULL, &archiveDate);
                SalGetFileSize(file, archiveSize, err); // returns "success?" — ignore it, we test 'err' later
                HANDLES(CloseHandle(file));
            }
            else
                err = GetLastError();

            if (err != NO_ERROR)
            {
                sprintf(text, LoadStr(IDS_FILEERRORFORMAT), ArchiveFileName, GetErrorText(err));
                SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                // use optimized insertion into 'newArchiveDir'
                newArchiveDir->AllocAddCache();

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                CPluginDataInterfaceAbstract* pluginDataAbs = NULL;
                CPluginData* plugin = NULL;
                CreateSafeWaitWindow(LoadStr(IDS_LISTINGARCHIVE), NULL, 2000, FALSE, MainWindow->HWindow);
                BOOL haveList = PackList(MainWindow->GetActivePanel(), ArchiveFileName, *newArchiveDir, pluginDataAbs, plugin);
                DestroySafeWaitWindow();
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                if (haveList)
                {
                    // release the cache so it does not linger on the object unnecessarily
                    newArchiveDir->FreeAddCache();

                    StoredArchiveDir = newArchiveDir;
                    newArchiveDir = NULL; // prevent newArchiveDir from being released
                    if (plugin != NULL)
                    {
                        StoredPluginData.Init(pluginDataAbs, plugin->DLLName, plugin->Version,
                                              plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                    }
                    else
                        StoredPluginData.Init(NULL, NULL, NULL, NULL, 0); // used only by plug-ins, not by Salamander
                    StoredArchiveDate = archiveDate;
                    StoredArchiveSize = archiveSize;

                    archiveDir = StoredArchiveDir; // use the new listing for the Paste operation
                    pluginData = StoredPluginData.GetInterface();
                }
            }

            if (newArchiveDir != NULL)
                delete newArchiveDir;
        }
    }

    if (archiveDir != NULL) // if we have the archive data, perform the Paste
    {
        CPanelTmpEnumData data;
        SelFilesAndDirs.Sort();
        data.IndexesCount = SelFilesAndDirs.GetCount();
        data.Indexes = (int*)malloc(sizeof(int) * data.IndexesCount);
        BOOL* foundDirs = NULL;
        if (SelFilesAndDirs.GetDirsCount() > 0)
            foundDirs = (BOOL*)malloc(sizeof(BOOL) * SelFilesAndDirs.GetDirsCount());
        BOOL* foundFiles = NULL;
        if (SelFilesAndDirs.GetFilesCount() > 0)
            foundFiles = (BOOL*)malloc(sizeof(BOOL) * SelFilesAndDirs.GetFilesCount());
        if (data.Indexes == NULL ||
            SelFilesAndDirs.GetDirsCount() > 0 && foundDirs == NULL ||
            SelFilesAndDirs.GetFilesCount() > 0 && foundFiles == NULL)
        {
            TRACE_E(LOW_MEMORY);
        }
        else
        {
            CFilesArray* files = archiveDir->GetFiles(PathInArchive);
            CFilesArray* dirs = archiveDir->GetDirs(PathInArchive);
            int actIndex = 0;
            int foundOnIndex;
            if (dirs != NULL && SelFilesAndDirs.GetDirsCount() > 0)
            {
                memset(foundDirs, 0, SelFilesAndDirs.GetDirsCount() * sizeof(BOOL));
                int i;
                for (i = 0; i < dirs->Count; i++)
                {
                    if (SelFilesAndDirs.Contains(TRUE, dirs->At(i).Name, &foundOnIndex) &&
                        foundOnIndex >= 0 && foundOnIndex < SelFilesAndDirs.GetDirsCount() &&
                        !foundDirs[foundOnIndex]) // mark only the first instance of the name (if there are multiple identical names in SelFilesAndDirs, it does not work; halving in Contains always arrives at the same one)
                    {
                        foundDirs[foundOnIndex] = TRUE; // this name has just been found
                        data.Indexes[actIndex++] = i;
                    }
                }
            }
            if (files != NULL && SelFilesAndDirs.GetFilesCount() > 0)
            {
                memset(foundFiles, 0, SelFilesAndDirs.GetFilesCount() * sizeof(BOOL));
                int i;
                for (i = 0; i < files->Count; i++)
                {
                    if (SelFilesAndDirs.Contains(FALSE, files->At(i).Name, &foundOnIndex) &&
                        foundOnIndex >= 0 && foundOnIndex < SelFilesAndDirs.GetFilesCount() &&
                        !foundFiles[foundOnIndex]) // mark only the first instance of the name (if there are multiple identical names in SelFilesAndDirs, it does not work; halving in Contains always arrives at the same one)
                    {
                        foundFiles[foundOnIndex] = TRUE;            // this name has just been found
                        data.Indexes[actIndex++] = dirs->Count + i; // all files have their index shifted after directories, as is customary in the panel
                    }
                }
            }
            data.IndexesCount = actIndex;
            if (data.IndexesCount == 0) // our ZIP root vanished completely
            {
                SalMessageBox(MainWindow->HWindow, LoadStr(IDS_ARCFILESNOTFOUND),
                              LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                BOOL unpack = TRUE;
                if (data.IndexesCount != SelFilesAndDirs.GetCount()) // not all items selected on the clipboard were found (duplicate names or files deleted from the archive)
                {
                    unpack = SalMessageBox(MainWindow->HWindow, LoadStr(IDS_ARCFILESNOTFOUND2),
                                           LoadStr(IDS_ERRORUNPACK),
                                           MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) == IDYES;
                }
                if (unpack)
                {
                    data.CurrentIndex = 0;
                    data.ZIPPath = PathInArchive;
                    data.Dirs = dirs;
                    data.Files = files;
                    data.ArchiveDir = archiveDir;
                    data.EnumLastDir = NULL;
                    data.EnumLastIndex = -1;

                    char pathBuf[MAX_PATH];
                    lstrcpyn(pathBuf, tgtPath, MAX_PATH);
                    int l = (int)strlen(pathBuf);
                    if (l > 3 && pathBuf[l - 1] == '\\')
                        pathBuf[l - 1] = 0; // remove the trailing backslash except for "c:\"

                    // the actual unpacking
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    PackUncompress(MainWindow->HWindow, MainWindow->GetActivePanel(), ArchiveFileName,
                                   pluginData, pathBuf, PathInArchive, PanelSalEnumSelection, &data);
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    //if (GetForegroundWindow() == MainWindow->HWindow)  // for incomprehensible reasons the focus disappears from the panel during drag & drop to Explorer, so return it there
                    //  RestoreFocusInSourcePanel();

                    // refresh directories that are not automatically refreshed
                    // change on the target path and its subdirectories (creating new directories and unpacking
                    // files/directories)
                    MainWindow->PostChangeOnPathNotification(pathBuf, TRUE);
                    // change in the directory where the archive is located (should not happen during unpacking, but refresh it just in case)
                    lstrcpyn(pathBuf, ArchiveFileName, MAX_PATH);
                    CutDirectory(pathBuf);
                    MainWindow->PostChangeOnPathNotification(pathBuf, FALSE);

                    UpdateWindow(MainWindow->HWindow);
                }
            }
        }
        if (data.Indexes != NULL)
            free(data.Indexes);
        if (foundDirs != NULL)
            free(foundDirs);
        if (foundFiles != NULL)
            free(foundFiles);
    }

    EndStopRefresh(); // the snooper starts now
}
