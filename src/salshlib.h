// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// mutex pro pristup do sdilene pameti
extern HANDLE SalShExtSharedMemMutex;
// sdilena pamet - viz struktura CSalShExtSharedMem
extern HANDLE SalShExtSharedMem;
// event pro zaslani zadosti o provedeni Paste ve zdrojovem Salamanderovi (pouziva se jen ve Vista+)
extern HANDLE SalShExtDoPasteEvent;
// namapovana sdilena pamet - viz struktura CSalShExtSharedMem
extern CSalShExtSharedMem* SalShExtSharedMemView;

// TRUE pokud se podarilo registrovat SalShExt/SalExten/SalamExt/SalExtX86/SalExtX64.DLL nebo uz registrovane bylo
extern BOOL SalShExtRegistered;

// maximalni prasarna: potrebujeme zjistit do ktereho okna probehne Drop, zjistujeme to
// v GetData podle pozice mysi, v tyhle promenny je posledni vysledek testu
extern HWND LastWndFromGetData;

// maximalni prasarna: potrebujeme zjistit do ktereho okna probehne Paste, zjistujeme to
// v GetData podle foreground window, v tyhle promenny je posledni vysledek testu
extern HWND LastWndFromPasteGetData;

extern BOOL OurDataOnClipboard; // TRUE = na clipboardu je nas data-object (copy&paste z archivu)

//*****************************************************************************

// volat pred pouzitim knihovny
void InitSalShLib();

// volat pro uvolneni knihovny
void ReleaseSalShLib();

// vraci TRUE pokud data objekt obsahuje jen "fake" adresar; ve 'fakeType' (neni-li NULL) vraci
// 1 pokud je zdroj archiv a 2 pokud je zdroj FS; je-li zdroj FS a 'srcFSPathBuf' neni NULL,
// vraci zdrojovou FS cestu ('srcFSPathBufSize' je velikost bufferu 'srcFSPathBuf')
BOOL IsFakeDataObject(IDataObject* pDataObject, int* fakeType, char* srcFSPathBuf, int srcFSPathBufSize);

//
//*****************************************************************************
// CFakeDragDropDataObject
//
// data object pouzivany pro zjistovani cile drag&drop operace (pouziva se pri
// vybalovani z archivu a pri kopirovani z pluginoveho file-systemu),
// zapouzdruje windowsovy data object ziskany pro "fake" adresar a pridava
// format SALCF_FAKE_REALPATH (urcuje cestu, ktera se ma po dropu objevit
// v directory-line, command-line + blokuje drop do usermenu-toolbar),
// SALCF_FAKE_SRCTYPE (typ zdroje - 1=archiv, 2=FS) a v pripade FS jeste
// SALCF_FAKE_SRCFSPATH (zdrojova FS cesta) do GetData()

class CFakeDragDropDataObject : public IDataObject
{
private:
    long RefCount;
    IDataObject* WinDataObject;   // zapouzdreny data object
    char RealPath[2 * MAX_PATH];  // cesta pro drop do directory a command line
    int SrcType;                  // typ zdroje (1=archiv, 2=FS)
    char SrcFSPath[2 * MAX_PATH]; // jen pro zdroj typu FS: zdrojova FS cesta
    UINT CFSalFakeRealPath;       // clipboard format pro sal-fake-real-path
    UINT CFSalFakeSrcType;        // clipboard format pro sal-fake-src-type
    UINT CFSalFakeSrcFSPath;      // clipboard format pro sal-fake-src-fs-path

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
            return 0; // nesmime sahnout do objektu, uz neexistuje
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
            return DV_E_FORMATETC; // timto zajistime "NO" drop u jednodussich softu (BOSS, WinCmd, SpeedCommander, MSIE, Word, atd.)
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
// data pro Paste z clipboardu ulozena uvnitr "zdrojoveho" Salamandera

class CSalamanderDirectory;

class CSalShExtPastedData
{
protected:
    DWORD DataID; // verze dat ulozenych pro Paste z clipboardu

    BOOL Lock; // TRUE = je zamknuty proti zruseni, FALSE = neni zamknuty

    char ArchiveFileName[MAX_PATH]; // plna cesta k archivu
    char PathInArchive[MAX_PATH];   // cesta uvnitr archivu, na ktere doslo ke Copy na clipboard
    CNames SelFilesAndDirs;         // jmena souboru a adresaru z PathInArchive, ktere se budou vypakovavat

    CSalamanderDirectory* StoredArchiveDir;             // ulozena struktura archivu (vyuziva se pokud archiv neni otevreny v panelu)
    CPluginDataInterfaceEncapsulation StoredPluginData; // ulozene rozhrani plugin-data archivu (vyuziva se pokud archiv neni otevreny v panelu)
    FILETIME StoredArchiveDate;                         // datum souboru archivu (pro testy platnosti listingu archivu)
    CQuadWord StoredArchiveSize;                        // velikost souboru archivu (pro testy platnosti listingu archivu)

public:
    CSalShExtPastedData();
    ~CSalShExtPastedData();

    DWORD GetDataID() { return DataID; }
    void SetDataID(DWORD dataID) { DataID = dataID; }

    BOOL IsLocked() { return Lock; }
    void SetLock(BOOL lock) { Lock = lock; }

    // nastavi data objektu, vraci TRUE pri uspechu, pri neuspechu necha objekt prazdny
    // a vraci FALSE
    BOOL SetData(const char* archiveFileName, const char* pathInArchive, CFilesArray* files,
                 CFilesArray* dirs, BOOL namesAreCaseSensitive, int* selIndexes,
                 int selIndexesCount);

    // vycisti data ulozena v StoredArchiveDir a StoredPluginData
    void ReleaseStoredArchiveData();

    // vycisti objekt (zrusi vsechna jeho data, objekt zustane pripraven pro dalsi pouziti)
    void Clear();

    // provede paste operaci se soucasnymi daty; 'copy' je TRUE pokud se maji data kopirovat,
    // FALSE pokud se maji presouvat; 'tgtPath' je cilova diskova cesta operace
    void DoPasteOperation(BOOL copy, const char* tgtPath);

    // pokud se objektu hodi poskytovana data, necha si je a vrati TRUE, jinak vraci
    // FALSE (poskytovana data budou nasledne uvolnena)
    BOOL WantData(const char* archiveFileName, CSalamanderDirectory* archiveDir,
                  CPluginDataInterfaceEncapsulation pluginData,
                  FILETIME archiveDate, CQuadWord archiveSize);

    // vraci TRUE, pokud je mozne unloadnout plugin 'plugin'; pokud objekt obsahuje
    // data pluginu 'plugin', pokusi se jich zbavit, aby mohl vratit TRUE
    BOOL CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin);
};

// data pro Paste z clipboardu ulozena uvnitr "zdrojoveho" Salamandera
extern CSalShExtPastedData SalShExtPastedData;

//
//*****************************************************************************
// CFakeCopyPasteDataObject
//
// data object pouzivany pro zjistovani cile copy&paste operace (pouziva se pri
// vybalovani z archivu), zapouzdruje windowsovy data object ziskany pro "fake"
// adresar a zajistuje vymaz "fake" adresare z disku po uvolneni objektu z
// clipboardu

class CFakeCopyPasteDataObject : public IDataObject
{
private:
    long RefCount;
    IDataObject* WinDataObject; // zapouzdreny data object
    char FakeDir[MAX_PATH];     // "fake" dir
    UINT CFSalFakeRealPath;     // clipboard format pro sal-fake-real-path
    UINT CFIdList;              // clipboard format pro shell id list (pouziva Explorer misto jednodussiho CF_HDROP)

    DWORD LastGetDataCallTime; // cas posledniho volani GetData()
    BOOL CutOrCopyDone;        // FALSE = objekt se teprve uklada na clipboard, Release nic nedela dokud CutOrCopyDone neni TRUE

public:
    CFakeCopyPasteDataObject(IDataObject* winDataObject, const char* fakeDir)
    {
        RefCount = 1;
        WinDataObject = winDataObject;
        WinDataObject->AddRef();
        lstrcpyn(FakeDir, fakeDir, MAX_PATH);
        CFSalFakeRealPath = RegisterClipboardFormat(SALCF_FAKE_REALPATH);
        CFIdList = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
        LastGetDataCallTime = GetTickCount() - 60000; // inicializujeme na 1 minutu pred zalozenim objektu
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
            return DV_E_FORMATETC; // timto zajistime "NO" drop u jednodussich softu (BOSS, WinCmd, SpeedCommander, MSIE, Word, atd.)
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
