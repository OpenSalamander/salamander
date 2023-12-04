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

// mutex pro pristup do sdilene pameti
HANDLE SalShExtSharedMemMutex = NULL;
// sdilena pamet - viz struktura CSalShExtSharedMem
HANDLE SalShExtSharedMem = NULL;
// event pro zaslani zadosti o provedeni Paste ve zdrojovem Salamanderovi (pouziva se jen ve Vista+)
HANDLE SalShExtDoPasteEvent = NULL;
// namapovana sdilena pamet - viz struktura CSalShExtSharedMem
CSalShExtSharedMem* SalShExtSharedMemView = NULL;

// TRUE pokud se podarilo registrovat SalShExt/SalamExt/SalExtX86/SalExtX64.DLL nebo uz registrovane
// bylo (kontroluje i soubor)
BOOL SalShExtRegistered = FALSE;

// maximalni prasarna: potrebujeme zjistit do ktereho okna probehne Drop, zjistujeme to
// v GetData podle pozice mysi, v tyhle promenny je posledni vysledek testu
HWND LastWndFromGetData = NULL;

// maximalni prasarna: potrebujeme zjistit do ktereho okna probehne Paste, zjistujeme to
// v GetData podle foreground window, v tyhle promenny je posledni vysledek testu
HWND LastWndFromPasteGetData = NULL;

BOOL OurDataOnClipboard = FALSE; // TRUE = na clipboardu je nas data-object (copy&paste z archivu)

// data pro Paste z clipboardu ulozena uvnitr "zdrojoveho" Salamandera
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
        SalShExtSharedMem = HANDLES_Q(CreateFileMapping(INVALID_HANDLE_VALUE, saPtr, PAGE_READWRITE, // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
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
            SalShExtSharedMemView = (CSalShExtSharedMem*)HANDLES(MapViewOfFile(SalShExtSharedMem, // FIXME_X64 nepredavame x86/x64 nekompatibilni data?
                                                                               FILE_MAP_WRITE, 0, 0, 0));
            if (SalShExtSharedMemView != NULL)
            {
                if (created)
                {
                    memset(SalShExtSharedMemView, 0, sizeof(CSalShExtSharedMem)); // sice ma byt nulovana, ale nespolehame na to
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
        OleSetClipboard(NULL);      // vyhodime nas data-object z clipboardu
        OurDataOnClipboard = FALSE; // teoreticky zbytecne (melo by se nastavit v Release() fakeDataObjectu)
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
        HGLOBAL dataDup = NULL; // vyrobime kopii RealPath
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
        if (dataDup != NULL) // mame data, ulozime je na medium a vratime
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
            if (dataDup != NULL) // mame data, ulozime je na medium a vratime
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
                HGLOBAL dataDup = NULL; // vyrobime kopii SrcFSPath
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
                if (dataDup != NULL) // mame data, ulozime je na medium a vratime
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

        if (CutOrCopyDone) // pokud doslo k chybe behem cut/copy, cekani nema smysl a cisteni provedeme jinde
        {
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): deleting clipfake directory!");

            // ted uz muzeme zrusit "paste" ve sdilene pameti, vycistit fake-dir a zrusit data-object
            if (SalShExtSharedMemView != NULL) // ulozime cas do sdilene pameti (pro rozliseni mezi paste a jinym copy/move fake-diru)
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
            { // pro jistotu zkontrolujeme, jestli skutecne smazeme jen fake-dir
                RemoveTemporaryDir(dir);
            }
            //      TRACE_I("CFakeCopyPasteDataObject::Release(): posting WM_USER_SALSHEXT_TRYRELDATA");
            if (MainWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // zkusime provest uvolneni dat (nejsou-li zamcena ani blokovana)
        }

        delete this;
        return 0; // nesmime sahnout do objektu, uz neexistuje
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
        return S_OK; // vracime S_OK, abysme vyhoveli testu ve funkci IsFakeDataObject()
    }
    else
    {
        if (formatEtc->cfFormat == CFIdList)
        { // Paste do Explorera pouziva tenhle format, ostatni nas nezajimaji (stejne nepouziji copy-hook)
            // resi problem ve Win98: pri Copy to clipboard z Exploreru se na stavajici objekt na clipboardu
            // vola GetData, az pak se uvolni a nahradi novym objektem z Exploreru (problem je 2 sekundovy
            // timeout z duvodu cekani na zavolani copy-hooku - po GetData ho vzdycky ocekavame)
            DWORD ti = GetTickCount();
            if (ti - LastGetDataCallTime >= 100) // optimalizace: ukladame novy cas jen pri zmene o minimalne 100ms
            {
                LastGetDataCallTime = ti;
                if (SalShExtSharedMemView != NULL) // ulozime cas do sdilene pameti (pro rozliseni mezi paste a jinym copy/move fake-diru)
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

    LastWndFromPasteGetData = NULL; // pro prvni Paste to budeme nulovat zde

    lstrcpyn(ArchiveFileName, archiveFileName, MAX_PATH);
    lstrcpyn(PathInArchive, pathInArchive, MAX_PATH);
    SelFilesAndDirs.SetCaseSensitive(namesAreCaseSensitive);
    int i;
    for (i = 0; i < selIndexesCount; i++)
    {
        int index = selIndexes[i];
        if (index < dirs->Count) // jde o adresar
        {
            if (!SelFilesAndDirs.Add(TRUE, dirs->At(index).Name))
                break;
        }
        else // jde o soubor
        {
            if (!SelFilesAndDirs.Add(FALSE, files->At(index - dirs->Count).Name))
                break;
        }
    }
    if (i < selIndexesCount) // chyba nedostatku pameti
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
            // uvolnime data plug-inu pro jednotlive soubory a adresare
            BOOL releaseFiles = StoredPluginData.CallReleaseForFiles();
            BOOL releaseDirs = StoredPluginData.CallReleaseForDirs();
            if (releaseFiles || releaseDirs)
                StoredArchiveDir->ReleasePluginData(StoredPluginData, releaseFiles, releaseDirs);

            // uvolnime interface StoredPluginData
            CPluginInterfaceEncapsulation plugin(StoredPluginData.GetPluginInterface(), StoredPluginData.GetBuiltForVersion());
            plugin.ReleasePluginDataInterface(StoredPluginData.GetInterface());
        }
        StoredArchiveDir->Clear(NULL); // uvolnime "standardni" (Salamanderovska) data listingu
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

    if (!Lock /* nemelo by nastat, ale sychrujeme se */ &&
        StrICmp(ArchiveFileName, archiveFileName) == 0 &&
        archiveSize != CQuadWord(-1, -1) && // porusena date&time znamka svedci o archivu, ktery je nutne znovu nacist
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
            // zjistime, jestli unloadeny plugin ma co docineni s nasim archivem,
            // plug-in by se klidne mohl unloadnout behem pouziti archivatoru (kazda funkce archivatoru
            // si plug-in naloadi), ale nic se nema prehanet, takze pripadny listing archivu zrusime
            int format = PackerFormatConfig.PackIsArchive(ArchiveFileName);
            if (format != 0) // nasli jsme podporovany archiv
            {
                format--;
                CPluginData* data;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0) // view: jde o interni zpracovani (plug-in)?
                {
                    data = Plugins.Get(-index - 1);
                    if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                        used = TRUE;
                }
                if (PackerFormatConfig.GetUsePacker(format)) // ma edit?
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0) // jde o interni zpracovani (plug-in)?
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
        ReleaseStoredArchiveData(); // pouzivame data pluginu, musime je (nebo jen bude lepsi je) uvolnit
    return TRUE;                    // unload pluginu je mozny
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

    BeginStopRefresh(); // cmuchal si da pohov

    char text[1000];
    CSalamanderDirectory* archiveDir = NULL;
    CPluginDataInterfaceAbstract* pluginData = NULL;
    for (int j = 0; j < 2; j++)
    {
        CFilesWindow* panel = j == 0 ? MainWindow->GetActivePanel() : MainWindow->GetNonActivePanel();
        if (panel->Is(ptZIPArchive) && StrICmp(ArchiveFileName, panel->GetZIPArchive()) == 0)
        { // panel obsahuje nas archiv
            BOOL archMaybeUpdated;
            panel->OfferArchiveUpdateIfNeeded(MainWindow->HWindow, IDS_ARCHIVECLOSEEDIT2, &archMaybeUpdated);
            if (archMaybeUpdated)
            {
                EndStopRefresh(); // ted uz zase cmuchal nastartuje
                return;
            }
            // vyuzijeme data z panelu (jsme v hl. threadu, panel se behem operace nemuze zmenit)
            archiveDir = panel->GetArchiveDir();
            pluginData = panel->PluginData.GetInterface();
            break;
        }
    }

    if (StoredArchiveDir != NULL) // pokud mame nejaka data archivu ulozena
    {
        if (archiveDir != NULL)
            ReleaseStoredArchiveData(); // archiv je otevreny v panelu, ulozena data zrusime
        else                            // zkusime ulozena data vyuzit, zkontrolujeme size&date souboru archivu
        {
            BOOL canUseData = FALSE;
            FILETIME archiveDate;  // datum&cas souboru archivu
            CQuadWord archiveSize; // velikost souboru archivu
            HANDLE file = HANDLES_Q(CreateFile(ArchiveFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileTime(file, NULL, NULL, &archiveDate);
                DWORD err = NO_ERROR;
                SalGetFileSize(file, archiveSize, err); // vraci "uspech?" - ignorujeme, testujeme pozdeji 'err'
                HANDLES(CloseHandle(file));

                if (err == NO_ERROR &&                                        // size&date jsme ziskali a
                    CompareFileTime(&archiveDate, &StoredArchiveDate) == 0 && // date se nelisi a
                    archiveSize == StoredArchiveSize)                         // size se take nelisi
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
                ReleaseStoredArchiveData(); // soubor archivu se zmenil, ulozena data zrusime
        }
    }

    if (archiveDir == NULL) // nemame data, musime archiv znovu vylistovat
    {
        CSalamanderDirectory* newArchiveDir = new CSalamanderDirectory(FALSE);
        if (newArchiveDir == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            // zjistime informace o souboru (existuje?, size, date&time)
            DWORD err = NO_ERROR;
            FILETIME archiveDate;  // datum&cas souboru archivu
            CQuadWord archiveSize; // velikost souboru archivu
            HANDLE file = HANDLES_Q(CreateFile(ArchiveFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (file != INVALID_HANDLE_VALUE)
            {
                GetFileTime(file, NULL, NULL, &archiveDate);
                SalGetFileSize(file, archiveSize, err); // vraci "uspech?" - ignorujeme, testujeme pozdeji 'err'
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
                // uplatnime optimalizovane pridavani do 'newArchiveDir'
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
                    // uvolnime cache, at v objektu zbytecne nestrasi
                    newArchiveDir->FreeAddCache();

                    StoredArchiveDir = newArchiveDir;
                    newArchiveDir = NULL; // aby se newArchiveDir neuvolnilo
                    if (plugin != NULL)
                    {
                        StoredPluginData.Init(pluginDataAbs, plugin->DLLName, plugin->Version,
                                              plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                    }
                    else
                        StoredPluginData.Init(NULL, NULL, NULL, NULL, 0); // pouzivaji jen plug-iny, Salamander ne
                    StoredArchiveDate = archiveDate;
                    StoredArchiveSize = archiveSize;

                    archiveDir = StoredArchiveDir; // pro Paste operaci vyuzijeme novy listing
                    pluginData = StoredPluginData.GetInterface();
                }
            }

            if (newArchiveDir != NULL)
                delete newArchiveDir;
        }
    }

    if (archiveDir != NULL) // pokud mame data archivu, provedeme Paste
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
                        !foundDirs[foundOnIndex]) // oznacujeme jen prvni instanci jmena (je-li vic shodnych jmen v SelFilesAndDirs, nefunguje to, pulenim (v Contains) dojdeme vzdy k tomu samemu)
                    {
                        foundDirs[foundOnIndex] = TRUE; // toto jmeno bylo prave nalezeno
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
                        !foundFiles[foundOnIndex]) // oznacujeme jen prvni instanci jmena (je-li vic shodnych jmen v SelFilesAndDirs, nefunguje to, pulenim (v Contains) dojdeme vzdy k tomu samemu)
                    {
                        foundFiles[foundOnIndex] = TRUE;            // toto jmeno bylo prave nalezeno
                        data.Indexes[actIndex++] = dirs->Count + i; // vsechny soubory maji index posunuty za adresare, zvyk z panelu
                    }
                }
            }
            data.IndexesCount = actIndex;
            if (data.IndexesCount == 0) // nas zip-root cely odesel do vecnych lovist
            {
                SalMessageBox(MainWindow->HWindow, LoadStr(IDS_ARCFILESNOTFOUND),
                              LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                BOOL unpack = TRUE;
                if (data.IndexesCount != SelFilesAndDirs.GetCount()) // nenaslo vsechny oznacene polozky z clipboardu (duplicity jmen nebo vymaz souboru z archivu)
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
                        pathBuf[l - 1] = 0; // krom "c:\" zrusime koncovy backslash

                    // vlastni rozpakovani
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    PackUncompress(MainWindow->HWindow, MainWindow->GetActivePanel(), ArchiveFileName,
                                   pluginData, pathBuf, PathInArchive, PanelSalEnumSelection, &data);
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    //if (GetForegroundWindow() == MainWindow->HWindow)  // z nepochopitelnych duvodu mizi fokus z panelu pri drag&dropu do Explorera, vratime ho tam
                    //  RestoreFocusInSourcePanel();

                    // refresh neautomaticky refreshovanych adresaru
                    // zmena na cilove ceste a jejich podadresarich (vytvareni novych adresaru a vypakovani
                    // souboru/adresaru)
                    MainWindow->PostChangeOnPathNotification(pathBuf, TRUE);
                    // zmena v adresari, kde je umisteny archiv (pri unpacku by nemelo nastat, ale radsi refreshneme)
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

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}
