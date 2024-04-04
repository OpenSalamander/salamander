// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

// mutex for accessing shared memory
HANDLE SalShExtSharedMemMutex = NULL;
// shared memory - see the structure CSalShExtSharedMem
HANDLE SalShExtSharedMem = NULL;
// event for sending a request to perform Paste in the source Salamander (used only in Vista+)
HANDLE SalShExtDoPasteEvent = NULL;
// mapped shared memory - see the structure CSalShExtSharedMem
CSalShExtSharedMem* SalShExtSharedMemView = NULL;

// TRUE if SalShExt/SalamExt/SalExtX86/SalExtX64.DLL was successfully registered or already registered
// was (also checks the file)
BOOL SalShExtRegistered = FALSE;

// maximum nonsense: we need to find out which window the Drop will run into, we are finding out
// in GetData according to the mouse position, the last test result is stored in these variables
HWND LastWndFromGetData = NULL;

// maximum nonsense: we need to find out which window Paste will run into, we are investigating this
// in GetData according to the foreground window, the last test result is stored in these variables
HWND LastWndFromPasteGetData = NULL;

BOOL OurDataOnClipboard = FALSE; // TRUE = there is our data-object on the clipboard (copy & paste from the archive)

// data for Paste from clipboard saved inside the "source" of Salamander
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
        SalShExtSharedMem = HANDLES_Q(CreateFileMapping(INVALID_HANDLE_VALUE, saPtr, PAGE_READWRITE, // FIXME_X64 are we passing x86/x64 incompatible data?
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
            SalShExtSharedMemView = (CSalShExtSharedMem*)HANDLES(MapViewOfFile(SalShExtSharedMem, // FIXME_X64 are we passing x86/x64 incompatible data?
                                                                               FILE_MAP_WRITE, 0, 0, 0));
            if (SalShExtSharedMemView != NULL)
            {
                if (created)
                {
                    memset(SalShExtSharedMemView, 0, sizeof(CSalShExtSharedMem)); // should be zeroed out, but we don't rely on it
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
        OleSetClipboard(NULL);      // we remove our data-object from the clipboard
        OurDataOnClipboard = FALSE; // theoretically unnecessary (should be set in the Release() of the fakeDataObject)
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
        if (dataDup != NULL) // we have the data, we will save it to the medium and return
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
            if (dataDup != NULL) // we have the data, we will save it to the medium and return
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
                HGLOBAL dataDup = NULL; // make a copy of SrcFSPath
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
                if (dataDup != NULL) // we have the data, we will save it to the medium and return
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

        if (CutOrCopyDone) // if an error occurred during cut/copy, waiting is pointless and we will perform the cleanup elsewhere
        {
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): deleting clipfake directory!");

            // Now we can remove "paste" from shared memory, clean up the fake-dir, and delete the data-object
            if (SalShExtSharedMemView != NULL) // store the time in shared memory (to distinguish between paste and other copy/move fake-dirs)
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
            { // to make sure, let's check if we are really deleting only the fake-dir
                RemoveTemporaryDir(dir);
            }
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): posting WM_USER_SALSHEXT_TRYRELDATA");
            if (MainWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // Let's try to release the data (if they are neither locked nor blocked)
        }

        delete this;
        return 0; // We must not access the object, it no longer exists
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
        return S_OK; // we return S_OK to pass the test in the IsFakeDataObject() function
    }
    else
    {
        if (formatEtc->cfFormat == CFIdList)
        { // Explorer uses this format for paste, we are not interested in the others (we will not use the copy-hook anyway)
            // resolves an issue in Win98: when copying to clipboard from Explorer, the existing object on the clipboard
            // calls GetData, then releases and replaces it with a new object from Explorer (the problem is 2 seconds
            // timeout due to waiting for the copy-hook call - we always expect it after GetData
            DWORD ti = GetTickCount();
            if (ti - LastGetDataCallTime >= 100) // optimization: we only save the new time when it changes by at least 100ms
            {
                LastGetDataCallTime = ti;
                if (SalShExtSharedMemView != NULL) // store the time in shared memory (to distinguish between paste and other copy/move fake-dirs)
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

    LastWndFromPasteGetData = NULL; // for the first Paste we will zero out here

    lstrcpyn(ArchiveFileName, archiveFileName, MAX_PATH);
    lstrcpyn(PathInArchive, pathInArchive, MAX_PATH);
    SelFilesAndDirs.SetCaseSensitive(namesAreCaseSensitive);
    int i;
    for (i = 0; i < selIndexesCount; i++)
    {
        int index = selIndexes[i];
        if (index < dirs->Count) // it's about the directory
        {
            if (!SelFilesAndDirs.Add(TRUE, dirs->At(index).Name))
                break;
        }
        else // it's about the file
        {
            if (!SelFilesAndDirs.Add(FALSE, files->At(index - dirs->Count).Name))
                break;
        }
    }
    if (i < selIndexesCount) // out of memory error
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
            // Release plugin data for individual files and directories
            BOOL releaseFiles = StoredPluginData.CallReleaseForFiles();
            BOOL releaseDirs = StoredPluginData.CallReleaseForDirs();
            if (releaseFiles || releaseDirs)
                StoredArchiveDir->ReleasePluginData(StoredPluginData, releaseFiles, releaseDirs);

            // Release the interface StoredPluginData
            CPluginInterfaceEncapsulation plugin(StoredPluginData.GetPluginInterface(), StoredPluginData.GetBuiltForVersion());
            plugin.ReleasePluginDataInterface(StoredPluginData.GetInterface());
        }
        StoredArchiveDir->Clear(NULL); // let's release the "standard" (Salamander-style) data listing
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

    if (!Lock /* It shouldn't happen, but we are preparing for it*/ &&
        StrICmp(ArchiveFileName, archiveFileName) == 0 &&
        archiveSize != CQuadWord(-1, -1) && // Corrupted date & time stamp indicates an archive that needs to be reloaded
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
            // Let's find out if the unloaded plugin has anything to do with our archive,
            // the plug-in could easily be unloaded while using the archiver (each archiver function
            // the plug-in does not load), but there is no need to rush, so we will cancel any potential archive listing
            int format = PackerFormatConfig.PackIsArchive(ArchiveFileName);
            if (format != 0) // We found a supported archive
            {
                format--;
                CPluginData* data;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0) // view: is it internal processing (plug-in)?
                {
                    data = Plugins.Get(-index - 1);
                    if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                        used = TRUE;
                }
                if (PackerFormatConfig.GetUsePacker(format)) // edit?
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0) // Is it about internal processing (plug-in)?
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
        ReleaseStoredArchiveData(); // we are using data from the plugin, we need to release it (or it will be better to release it)
    return TRUE;                    // unloading the plugin is possible
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

    BeginStopRefresh(); // He was snoring in his sleep

    char text[1000];
    CSalamanderDirectory* archiveDir = NULL;
    CPluginDataInterfaceAbstract* pluginData = NULL;
    for (int j = 0; j < 2; j++)
    {
        CFilesWindow* panel = j == 0 ? MainWindow->GetActivePanel() : MainWindow->GetNonActivePanel();
        if (panel->Is(ptZIPArchive) && StrICmp(ArchiveFileName, panel->GetZIPArchive()) == 0)
        { // panel contains our archive
            BOOL archMaybeUpdated;
            panel->OfferArchiveUpdateIfNeeded(MainWindow->HWindow, IDS_ARCHIVECLOSEEDIT2, &archMaybeUpdated);
            if (archMaybeUpdated)
            {
                EndStopRefresh(); // now he's sniffling again, he'll start up
                return;
            }
            // we will use data from the panel (we are in the main thread, the panel cannot change during the operation)
            archiveDir = panel->GetArchiveDir();
            pluginData = panel->PluginData.GetInterface();
            break;
        }
    }

    if (StoredArchiveDir != NULL) // if we have some archive data stored
    {
        if (archiveDir != NULL)
            ReleaseStoredArchiveData(); // archive is open in the panel, saved data will be deleted
        else                            // Let's try to use the saved data, check the size and date of the archive file
        {
            BOOL canUseData = FALSE;
            FILETIME archiveDate;  // date and time of the archive file
            CQuadWord archiveSize; // size of the archive file
            HANDLE file = HANDLES_Q(CreateFile(ArchiveFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileTime(file, NULL, NULL, &archiveDate);
                DWORD err = NO_ERROR;
                SalGetFileSize(file, archiveSize, err); // returns "success?" - ignore, test later 'err'
                HANDLES(CloseHandle(file));

                if (err == NO_ERROR &&                                        // size&date we have obtained and
                    CompareFileTime(&archiveDate, &StoredArchiveDate) == 0 && // dates do not differ and
                    archiveSize == StoredArchiveSize)                         // the size does not differ either
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
                ReleaseStoredArchiveData(); // The archive file has changed, saved data will be deleted
        }
    }

    if (archiveDir == NULL) // We don't have the data, we need to list the archive again.
    {
        CSalamanderDirectory* newArchiveDir = new CSalamanderDirectory(FALSE);
        if (newArchiveDir == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            // retrieve information about the file (exists?, size, date&time)
            DWORD err = NO_ERROR;
            FILETIME archiveDate;  // date and time of the archive file
            CQuadWord archiveSize; // size of the archive file
            HANDLE file = HANDLES_Q(CreateFile(ArchiveFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileTime(file, NULL, NULL, &archiveDate);
                SalGetFileSize(file, archiveSize, err); // returns "success?" - ignore, test later 'err'
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
                // apply optimized adding to 'newArchiveDir'
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
                    // Let's clear the cache so the object doesn't unnecessarily shake
                    newArchiveDir->FreeAddCache();

                    StoredArchiveDir = newArchiveDir;
                    newArchiveDir = NULL; // so that newArchiveDir is not released
                    if (plugin != NULL)
                    {
                        StoredPluginData.Init(pluginDataAbs, plugin->DLLName, plugin->Version,
                                              plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                    }
                    else
                        StoredPluginData.Init(NULL, NULL, NULL, NULL, 0); // only plugins are used, Salamander does not
                    StoredArchiveDate = archiveDate;
                    StoredArchiveSize = archiveSize;

                    archiveDir = StoredArchiveDir; // For the Paste operation, we will use a new listing
                    pluginData = StoredPluginData.GetInterface();
                }
            }

            if (newArchiveDir != NULL)
                delete newArchiveDir;
        }
    }

    if (archiveDir != NULL) // if we have archive data, we will perform Paste
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
                        !foundDirs[foundOnIndex]) // We only mark the first instance of the name (if there are multiple identical names in SelFilesAndDirs, it doesn't work, by pulling (in Contains) we always get to the same one)
                    {
                        foundDirs[foundOnIndex] = TRUE; // This name has just been found
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
                        !foundFiles[foundOnIndex]) // We only mark the first instance of the name (if there are multiple identical names in SelFilesAndDirs, it doesn't work, by pulling (in Contains) we always get to the same one)
                    {
                        foundFiles[foundOnIndex] = TRUE;            // This name has just been found
                        data.Indexes[actIndex++] = dirs->Count + i; // all files have their index shifted behind the directories, a habit from the panel
                    }
                }
            }
            data.IndexesCount = actIndex;
            if (data.IndexesCount == 0) // Our zip-root has departed to the eternal hunting grounds
            {
                SalMessageBox(MainWindow->HWindow, LoadStr(IDS_ARCFILESNOTFOUND),
                              LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                BOOL unpack = TRUE;
                if (data.IndexesCount != SelFilesAndDirs.GetCount()) // did not find all marked items from the clipboard (duplicate names or delete files from the archive)
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
                        pathBuf[l - 1] = 0; // Except for "c:\" we will remove the trailing backslash

                    // custom unpacking
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    PackUncompress(MainWindow->HWindow, MainWindow->GetActivePanel(), ArchiveFileName,
                                   pluginData, pathBuf, PathInArchive, PanelSalEnumSelection, &data);
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    //if (GetForegroundWindow() == MainWindow->HWindow)  // for incomprehensible reasons, the focus disappears from the panel during drag&drop to Explorer, let's return it there
                    //  RestoreFocusInSourcePanel();

                    // refresh non-automatically refreshed directories
                    // change in the target path and its subdirectories (creating new directories and extracting
                    // file/directory)
                    MainWindow->PostChangeOnPathNotification(pathBuf, TRUE);
                    // change in the directory where the archive is located (unpacking should not occur, but let's refresh just in case)
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

    EndStopRefresh(); // now he's sniffling again, he'll start up
}
