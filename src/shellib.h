// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// inicializace knihovny
BOOL InitializeShellib();

// uvolneni knihovny
void ReleaseShellib();

// bezpecne volani IContextMenu2::GetCommandString() ve kterem to MS obcas pada
HRESULT AuxGetCommandString(IContextMenu2* menu, UINT_PTR idCmd, UINT uType, UINT* pReserved, LPSTR pszName, UINT cchMax);

// callback, ktery vraci jmena oznacenych souboru pro vytvareni nasl. interfacu
typedef const char* (*CEnumFileNamesFunction)(int index, void* param);

// vytvori data object pro drag&drop operace nad oznacenymi soubory a adresari z rootDir
IDataObject* CreateIDataObject(HWND hOwnerWindow, const char* rootDir, int files,
                               CEnumFileNamesFunction nextFile, void* param);

// vytvori interface kontextoveho menu pro oznacene soubory a adresare z rootDir
IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* rootDir, int files,
                                   CEnumFileNamesFunction nextFile, void* param);

// vytvori interface kontextoveho menu pro dany adresar
IContextMenu2* CreateIContextMenu2(HWND hOwnerWindow, const char* dir);

// ma dany adresar nebo soubor drop-target?
BOOL HasDropTarget(const char* dir);

// vytvori drop-target pro drag&drop operace do daneho adresare nebo souboru
IDropTarget* CreateIDropTarget(HWND hOwnerWindow, const char* dir);

// otevre okno specialniho foldru
void OpenSpecFolder(HWND hOwnerWindow, int specFolder);

// otevre okno folderu 'dir' a postavi focus na 'item'
void OpenFolderAndFocusItem(HWND hOwnerWindow, const char* dir, const char* item);

// otevre browse dialog a vybere cestu (mozne omezit jen na sitovou cestu)
// hCenterWindow - okno, ke kteremu bude dialog centrovan
BOOL GetTargetDirectory(HWND parent, HWND hCenterWindow, const char* title, const char* comment,
                        char* path, BOOL onlyNet = FALSE, const char* initDir = NULL);

// rozpozna jestli se nejedna o cestu do NetHood (adresar se souborem target.lnk),
// pripadne zjisti kam vede target.lnk a cestu vraci v 'path'; 'path' je in/out cesta
// (min. MAX_PATH znaku)
void ResolveNetHoodPath(char* path);

class CMenuNew;

// vraci New menu - handle popup-menu a IContextMenu pres ktere se prikazy spousti
void GetNewOrBackgroundMenu(HWND hOwnerWindow, const char* dir, CMenuNew* menu,
                            int minCmd, int maxCmd, BOOL backgoundMenu);

struct CDragDropOperData
{
    char SrcPath[MAX_PATH];     // zdrojova cesta spolecna vsem souborum/adresarum z Names ("" == nepodarila se konverze cesty z Unicode)
    TIndirectArray<char> Names; // razena alokovana jmena souboru/adresaru (v CF_HDROP se nerozlisuje jestli jde o soubor nebo adresar) ("" == nepodarila se konverze cesty z Unicode)

    CDragDropOperData() : Names(200, 200) { SrcPath[0] = 0; }
};

// zjisti jestli jsou v 'pDataObject' soubory a adresare z disku a zaroven jen z jedine cesty,
// pripadne jejich jmena ulozi do 'namesList' (neni-li NULL)
BOOL IsSimpleSelection(IDataObject* pDataObject, CDragDropOperData* namesList);

// vytahne pres GetDisplayNameOf(flags) jmeno pro 'pidl' (ID-list o jedno ID zkrati, ziska od
// desktopu folder pro zkraceny ID-list a od tohoto folderu si vola GetDisplayNameOf pro posledni
// ID se zadanymi 'flags'); pri uspechu vraci TRUE + jmeno v 'name' (buffer o velikosti 'nameSize');
// 'pidl' nedealokuje; 'alloc' je iface ziskany pres CoGetMalloc
BOOL GetSHObjectName(ITEMIDLIST* pidl, DWORD flags, char* name, int nameSize, IMalloc* alloc);

// TRUE = effect drag&dropu se napocital v pluginovem FS, tedy neni potreba forcovat Copy
// v CImpIDropSource::GiveFeedback
extern BOOL DragFromPluginFSEffectIsFromPlugin;

//*****************************************************************************
//
// CImpIDropSource
//
// Zakladni verze objektu, chova se standardne (default kurzory atd.).
//
// Vyjimka: pri tazeni z pluginoveho FS (s moznymi efekty Copy i Move) do Explorera
// na disk s TEMP adresarem se defaultne nabizi Move misto Copy (coz je logicky
// nesmysl, useri cekaji Copy), proto je tento pripad znasilneny tak, ze ukazujeme
// jiny kurzor nez je dwEffect v GiveFeedback a vysledny efekt pak bereme podle
// posledniho tvaru kurzoru misto podle vysledku DoDragDrop.

class CImpIDropSource : public IDropSource
{
private:
    long RefCount;
    DWORD MouseButton; // -1 = neinicializovana hodnota, jinak MK_LBUTTON nebo MK_RBUTTON

public:
    // posledni efekt vraceny metodou GiveFeedback - zavadime, protoze
    // DoDragDrop nevraci dwEffect == DROPEFFECT_MOVE, pri MOVE vraci dwEffect == 0,
    // duvody viz "Handling Shell Data Transfer Scenarios" sekce "Handling Optimized Move Operations":
    // http://msdn.microsoft.com/en-us/library/windows/desktop/bb776904%28v=vs.85%29.aspx
    // (zkracene: dela se optimalizovany Move, coz znamena ze se nedela kopie do cile nasledovana mazanim
    //            originalu, aby zdroj nechtene nesmazal original (jeste nemusi byt presunuty), dostane
    //            vysledek operace DROPEFFECT_NONE nebo DROPEFFECT_COPY)
    DWORD LastEffect;

    BOOL DragFromPluginFSWithCopyAndMove; // tazeni z pluginoveho FS s moznym Copy i Move, detaily viz vyse

public:
    CImpIDropSource(BOOL dragFromPluginFSWithCopyAndMove)
    {
        RefCount = 1;
        MouseButton = -1;
        LastEffect = -1;
        DragFromPluginFSWithCopyAndMove = dragFromPluginFSWithCopyAndMove;
    }

    virtual ~CImpIDropSource()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
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

    STDMETHOD(GiveFeedback)
    (DWORD dwEffect)
    {
        if (DragFromPluginFSWithCopyAndMove && !DragFromPluginFSEffectIsFromPlugin)
        {
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if ((!shiftPressed || controlPressed) && (dwEffect & DROPEFFECT_MOVE))
            { // ma se delat Copy, ale nabizi se Move -> tuhle situaci znasilnime, ukazeme Copy kurzor a do LastEffect dame Copy
                LastEffect = DROPEFFECT_COPY;
                SetCursor(LoadCursor(HInstance, MAKEINTRESOURCE(IDC_DRAGCOPYEFFECT)));
                return S_OK;
            }
        }
        DragFromPluginFSEffectIsFromPlugin = FALSE;
        LastEffect = dwEffect;
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

    STDMETHOD(QueryContinueDrag)
    (BOOL fEscapePressed, DWORD grfKeyState)
    {
        DWORD mb = grfKeyState & (MK_LBUTTON | MK_RBUTTON);
        if (mb == 0)
            return DRAGDROP_S_DROP;
        if (MouseButton == -1)
            MouseButton = mb;
        if (fEscapePressed || MouseButton != mb)
            return DRAGDROP_S_CANCEL;
        return S_OK;
    }
};

//*****************************************************************************
//
// CImpDropTarget
//
// vola definovane callbacky pro ziskani drop targetu (adresare),
// oznameni dropnuti nebo ESC,
// zbytek operaci necha provest systemovy objekt IDropTarget z IShellFolder

// zaznam pouzity v datech pro copy a move callback
struct CCopyMoveRecord
{
    char* FileName;
    char* MapName;

    CCopyMoveRecord(const char* fileName, const char* mapName);
    CCopyMoveRecord(const wchar_t* fileName, const char* mapName);
    CCopyMoveRecord(const char* fileName, const wchar_t* mapName);
    CCopyMoveRecord(const wchar_t* fileName, const wchar_t* mapName);

    char* AllocChars(const char* name);
    char* AllocChars(const wchar_t* name);
};

// data pro copy a move callback
class CCopyMoveData : public TIndirectArray<CCopyMoveRecord>
{
public:
    BOOL MakeCopyOfName; // TRUE pokud se ma snazit o "Copy of..." pokud cil jiz existuje

public:
    CCopyMoveData(int base, int delta) : TIndirectArray<CCopyMoveRecord>(base, delta)
    {
        MakeCopyOfName = FALSE;
    }
};

// callback pro copy a move operace, stara se o destrukci 'data'
typedef BOOL (*CDoCopyMove)(BOOL copy, char* targetDir, CCopyMoveData* data,
                            void* param);

// callback pro drag&drop operace, 'copy' je TRUE/FALSE (copy/move), 'toArchive' je TRUE/FALSE
// (to archive/FS), 'archiveOrFSName' (muze byt NULL, pokud se informace ma ziskat z panelu)
// je jmeno souboru archivu nebo FS-name, 'archivePathOrUserPart' je cesta v archivu nebo
// user-part FS cesty, 'data' obsahuji popis zdrojovych souboru/adresaru, funkce se stara
// o destrukci objektu 'data', 'param' je parametr predany do konstruktoru CImpDropTarget
typedef void (*CDoDragDropOper)(BOOL copy, BOOL toArchive, const char* archiveOrFSName,
                                const char* archivePathOrUserPart, CDragDropOperData* data,
                                void* param);

// callback, ktery vraci cilovy adresar pro dany bod 'pt'
typedef const char* (*CGetCurDir)(POINTL& pt, void* param, DWORD* pdwEffect, BOOL rButton,
                                  BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType);

// callback oznamujici konec drop operace, drop == FALSE pri ESC
typedef void (*CDropEnd)(BOOL drop, BOOL shortcuts, void* param, BOOL ownRutine,
                         BOOL isFakeDataObject, int tgtType);

// callback pro dotaz pred dokoncenim operace (drop)
typedef BOOL (*CConfirmDrop)(DWORD& effect, DWORD& defEffect, DWORD& grfKeyState);

// callback oznamujici vstup a vystup mysi do targetu
typedef void (*CEnterLeaveDrop)(BOOL enter, void* param);

// callback, ktery povoluje pouziti nasich rutin pro copy/move
typedef BOOL (*CUseOwnRutine)(IDataObject* pDataObject);

// callback pro zjisteni default drop effectu pri tazeni z FS na FS
typedef void (*CGetFSToFSDropEffect)(const char* srcFSPath, const char* tgtFSPath,
                                     DWORD allowedEffects, DWORD keyState,
                                     DWORD* dropEffect, void* param);

enum CIDTTgtType
{
    idtttWindows,          // soubory/adresare z windowsove cesty na windowsovou cestu
    idtttArchive,          // soubory/adresare z windowsove cesty do archivu
    idtttPluginFS,         // soubory/adresare z windowsove cesty do FS
    idtttArchiveOnWinPath, // archiv na windowsove ceste (drop=pack to archive)
    idtttFullPluginFSPath, // FS to FS
};

class CImpDropTarget : public IDropTarget
{
private:
    long RefCount;
    HWND OwnerWindow;
    IDataObject* OldDataObject;
    BOOL OldDataObjectIsFake;
    int OldDataObjectIsSimple;                 // -1 (neznama hodnota), TRUE/FALSE = je/neni jednoduchy (vsechna jmena na jedne ceste)
    int OldDataObjectSrcType;                  // 0 (neznamy typ), 1/2 = archiv/FS
    char OldDataObjectSrcFSPath[2 * MAX_PATH]; // jen pro typ FS: zdrojova FS cesta

    CDoCopyMove DoCopyMove;
    void* DoCopyMoveParam;

    CDoDragDropOper DoDragDropOper;
    void* DoDragDropOperParam;

    CGetCurDir GetCurDir;
    void* GetCurDirParam;

    CDropEnd DropEnd;
    void* DropEndParam;

    CConfirmDrop ConfirmDrop;
    BOOL* ConfirmDropEnable;

    int TgtType; // hodnoty viz CIDTTgtType; idtttWindows i pro archivy a FS bez moznosti dropnuti aktualniho dataobjectu
    IDropTarget* CurDirDropTarget;
    char CurDir[2 * MAX_PATH];

    CEnterLeaveDrop EnterLeaveDrop;
    void* EnterLeaveDropParam;

    BOOL RButton; // akce pravym tlacitkem mysi?

    CUseOwnRutine UseOwnRutine;

    DWORD LastEffect; // posledni effect zjisteny v DragEnter nebo DragOver (-1 => neplatny)

    CGetFSToFSDropEffect GetFSToFSDropEffect;
    void* GetFSToFSDropEffectParam;

public:
    CImpDropTarget(HWND ownerWindow, CDoCopyMove doCopyMove, void* doCopyMoveParam,
                   CGetCurDir getCurDir, void* getCurDirParam, CDropEnd dropEnd,
                   void* dropEndParam, CConfirmDrop confirmDrop, BOOL* confirmDropEnable,
                   CEnterLeaveDrop enterLeaveDrop, void* enterLeaveDropParam,
                   CUseOwnRutine useOwnRutine, CDoDragDropOper doDragDropOper,
                   void* doDragDropOperParam, CGetFSToFSDropEffect getFSToFSDropEffect,
                   void* getFSToFSDropEffectParam)
    {
        RefCount = 1;
        OwnerWindow = ownerWindow;
        DoCopyMove = doCopyMove;
        DoCopyMoveParam = doCopyMoveParam;
        DoDragDropOper = doDragDropOper;
        DoDragDropOperParam = doDragDropOperParam;
        GetCurDir = getCurDir;
        GetCurDirParam = getCurDirParam;
        TgtType = idtttWindows;
        CurDirDropTarget = NULL;
        CurDir[0] = 0;
        DropEnd = dropEnd;
        DropEndParam = dropEndParam;
        OldDataObject = NULL;
        OldDataObjectIsFake = FALSE;
        OldDataObjectIsSimple = -1; // neznama hodnota
        OldDataObjectSrcType = 0;   // neznamy typ
        OldDataObjectSrcFSPath[0] = 0;
        ConfirmDrop = confirmDrop;
        ConfirmDropEnable = confirmDropEnable;
        RButton = FALSE;
        EnterLeaveDrop = enterLeaveDrop;
        EnterLeaveDropParam = enterLeaveDropParam;
        UseOwnRutine = useOwnRutine;
        LastEffect = -1;
        GetFSToFSDropEffect = getFSToFSDropEffect;
        GetFSToFSDropEffectParam = getFSToFSDropEffectParam;
    }
    virtual ~CImpDropTarget()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
        if (CurDirDropTarget != NULL)
            CurDirDropTarget->Release();
    }

    void SetDirectory(const char* path, DWORD grfKeyState, POINTL pt,
                      DWORD* effect, IDataObject* dataObject, BOOL tgtIsFile, int tgtType);
    BOOL TryCopyOrMove(BOOL copy, IDataObject* pDataObject, UINT CF_FileMapA,
                       UINT CF_FileMapW, BOOL cfFileMapA, BOOL cfFileMapW);
    BOOL ProcessClipboardData(BOOL copy, const DROPFILES* data, const char* mapA,
                              const wchar_t* mapW);

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

    STDMETHOD(DragEnter)
    (IDataObject* pDataObject, DWORD grfKeyState,
     POINTL pt, DWORD* pdwEffect);
    STDMETHOD(DragOver)
    (DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHOD(DragLeave)
    ();
    STDMETHOD(Drop)
    (IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
     DWORD* pdwEffect);
};

struct IShellFolder;
struct IContextMenu;
struct IContextMenu2;

class CMenuNew
{
protected:
    IContextMenu2* Menu2; // menu-interface 2
    HMENU Menu;           // submenu New

public:
    CMenuNew() { Init(); }
    ~CMenuNew() { Release(); }

    void Init()
    {
        Menu2 = NULL;
        Menu = NULL;
    }

    void Set(IContextMenu2* menu2, HMENU menu)
    {
        if (menu == NULL)
            return; // is-not-set
        Menu2 = menu2;
        Menu = menu;
    }

    BOOL MenuIsAssigned() { return Menu != NULL; }

    HMENU GetMenu() { return Menu; }
    IContextMenu2* GetMenu2() { return Menu2; }

    void Release();
    void ReleaseBody();
};

//
//*****************************************************************************
// CTextDataObject
//

class CTextDataObject : public IDataObject
{
private:
    long RefCount;
    HGLOBAL Data;

public:
    CTextDataObject(HGLOBAL data)
    {
        RefCount = 1;
        Data = data;
    }
    virtual ~CTextDataObject()
    {
        if (RefCount != 0)
            TRACE_E("Preliminary destruction of this object.");
        NOHANDLES(GlobalFree(Data));
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
        return E_NOTIMPL;
    }

    STDMETHOD(QueryGetData)
    (FORMATETC* formatEtc)
    {
        if (formatEtc == NULL)
            return E_INVALIDARG;
        if ((formatEtc->cfFormat == CF_TEXT || formatEtc->cfFormat == CF_UNICODETEXT) &&
            (formatEtc->tymed & TYMED_HGLOBAL))
        {
            return S_OK;
        }
        return (formatEtc->tymed & TYMED_HGLOBAL) ? DV_E_FORMATETC : DV_E_TYMED;
    }

    STDMETHOD(GetCanonicalFormatEtc)
    (FORMATETC* pFormatetcIn, FORMATETC* pFormatetcOut)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(SetData)
    (FORMATETC* pFormatetc, STGMEDIUM* pmedium, BOOL fRelease)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(EnumFormatEtc)
    (DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(DAdvise)
    (FORMATETC* pFormatetc, DWORD advf, IAdviseSink* pAdvSink,
     DWORD* pdwConnection)
    {
        return E_NOTIMPL;
    }

    STDMETHOD(DUnadvise)
    (DWORD dwConnection)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHOD(EnumDAdvise)
    (IEnumSTATDATA** ppenumAdvise)
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }
};

// uvolni CopyMoveData
void DestroyCopyMoveData(CCopyMoveData* data);
