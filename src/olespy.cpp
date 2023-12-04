// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "olespy.h"

#ifdef _DEBUG

//-------------------------------------------------------------------------
//
// Poznamky k implentaci IMallocSpy
//
// 1) OLE cachuje (nejen) BSTR, takze pod beznym OS vraci IMallocSpy nesmyslne
//    leaky. Cachovani lze odstavit nastavenim ENV promenne OANOCACHE=1 a pouzitim
//    debug (Checked) verze OLE. Idealni je cely Checked Build W2K nebo vytahnout
//    patricna DLL ze Service Packu v Checked verzi, ktery je volne k mani.
//
// 2) Pokud v OLE zustanou nejake leaky, vrati pri pokusu o odregistraci spye
//    pomoci CoRevokeMallocSpy hodnotu E_ACCESSDENIED a odlozi volani metody
//    Release na pozdeji. V realu uz nam Release nezavola nikdy (W2K). Protoze
//    nektere alokace se uvolnuji az behem DLL_PROCESS_DETACH v OLEAUT32.DLL,
//    bude je nam spy hlasit jako leaky. Je sikovne hlasene leaky porovnat
//    s tim, co pod W2K Checked Build hlasi CairOLE system do debug okna.
//
// 3) Bylo treba predejit dvoum problemum: nechceme, aby nam CRT hlasil memory
//    leak, ktery by vzniknul alokovani instance CMallocSpy na heapu.
//    Zaroven potrebujeme zahovat instanci tohoto obejktu co nejdele, protoze
//    OLE bude volat nase metody (zejmena PredFree a PostFree s fSpy == FALSE)
//    i po zavolani CoRevokeMallocSpy. Resime to kopii objektu do staticke
//    pameti, viz OleSpyRegister(). Nebude tak nikdy zavolan destruktor a
//    kriticka sekce nebude zrusena.
//
// 4) Pokud testujeme pod W2K Checked Build, lze z OLE dostat vetsi mnozstvi
//    hlaseni pomoci pridani sekce do souboru win.ini:
//    [CairOLE InfoLevels]
//    cairole=7
//    heap=7
//
//    pokud chceme zobrazit detailni hlasky vcetne callstacku jednotlivych alokaci,
//    je treba do obou hodnot nastavit dekadicky ekvivalent k 0xffffffff:
//    [CairOLE InfoLevels]
//    cairole=4294967295
//    heap=4294967295
//
//    CairOLE casto hlasi vetsi pocet leaku nez ktery nam detekuje spy. Bezne take
//    je, ze CairOLE hlasi leaky, zatimco spy hlasi ze je vse OK. Podstatne je, aby
//    tyto leaky byly konstantni (napriklad na start/exit Salamandera hlasi CairOLE
//    13 leaku a ~800 bajtu pameti). Toto cislo vsak nijak neroste, takze to povazuji
//    za nezajimavou informaci.
//
// 5) Spy bude hlasit leaky z volani funkci jako je SHBrowseForFolder nebo
//    SHGetSpecialFolderLocation. Nejedna se o leaky, ale o cachovane PIDLy,
//    ktere shell32.dll uvolnuje az pri svem odpojeni od procesu. Rozpozname
//    je tak, ze pri opakovanem volani funkci se nebudou leaky mnozit.
//

//-------------------------------------------------------------------------

#define SHID_JUNCTION 0x80

#define SHID_GROUPMASK 0x70
#define SHID_TYPEMASK 0x7f
#define SHID_INGROUPMASK 0x0f

#define SHID_ROOT 0x10
#define SHID_ROOT_REGITEM 0x1f // Mail

#if ((DRIVE_REMOVABLE | DRIVE_FIXED | DRIVE_REMOTE | DRIVE_CDROM | DRIVE_RAMDISK) != 0x07)
#error Definitions of DRIVE_* are changed!
#endif

#define SHID_COMPUTER 0x20
#define SHID_COMPUTER_1 0x21                             // free
#define SHID_COMPUTER_REMOVABLE (0x20 | DRIVE_REMOVABLE) // 2
#define SHID_COMPUTER_FIXED (0x20 | DRIVE_FIXED)         // 3
#define SHID_COMPUTER_REMOTE (0x20 | DRIVE_REMOTE)       // 4
#define SHID_COMPUTER_CDROM (0x20 | DRIVE_CDROM)         // 5
#define SHID_COMPUTER_RAMDISK (0x20 | DRIVE_RAMDISK)     // 6
#define SHID_COMPUTER_7 0x27                             // free
#define SHID_COMPUTER_DRIVE525 0x28                      // 5.25 inch floppy disk drive
#define SHID_COMPUTER_DRIVE35 0x29                       // 3.5 inch floppy disk drive
#define SHID_COMPUTER_NETDRIVE 0x2a                      // Network drive
#define SHID_COMPUTER_NETUNAVAIL 0x2b                    // Network drive that is not restored.
#define SHID_COMPUTER_C 0x2c                             // free
#define SHID_COMPUTER_D 0x2d                             // free
#define SHID_COMPUTER_REGITEM 0x2e                       // Controls, Printers, ...
#define SHID_COMPUTER_MISC 0x2f                          // Unknown drive type

#define SHID_FS 0x30
#define SHID_FS_TYPEMASK 0x3F
#define SHID_FS_DIRECTORY 0x31   // CHICAGO
#define SHID_FS_FILE 0x32        // FOO.TXT
#define SHID_FS_UNICODE 0x34     // Is it unicode? (this is a bitmask)
#define SHID_FS_DIRUNICODE 0x35  // Folder with a unicode name
#define SHID_FS_FILEUNICODE 0x36 // File with a unicode name

#define SHID_NET 0x40
#define SHID_NET_DOMAIN (SHID_NET | RESOURCEDISPLAYTYPE_DOMAIN)
#define SHID_NET_SERVER (SHID_NET | RESOURCEDISPLAYTYPE_SERVER)
#define SHID_NET_SHARE (SHID_NET | RESOURCEDISPLAYTYPE_SHARE)
#define SHID_NET_FILE (SHID_NET | RESOURCEDISPLAYTYPE_FILE)
#define SHID_NET_GROUP (SHID_NET | RESOURCEDISPLAYTYPE_GROUP)
#define SHID_NET_NETWORK (SHID_NET | RESOURCEDISPLAYTYPE_NETWORK)
#define SHID_NET_RESTOFNET (SHID_NET | RESOURCEDISPLAYTYPE_ROOT)
#define SHID_NET_SHAREADMIN (SHID_NET | RESOURCEDISPLAYTYPE_SHAREADMIN)
#define SHID_NET_DIRECTORY (SHID_NET | RESOURCEDISPLAYTYPE_DIRECTORY)
#define SHID_NET_TREE (SHID_NET | RESOURCEDISPLAYTYPE_TREE)
#define SHID_NET_REGITEM 0x4e // Remote Computer items
#define SHID_NET_PRINTER 0x4f // \\PYREX\LASER1

#define SIL_GetType(pidl) (ILIsEmpty(pidl) ? 0 : (pidl)->mkid.abID[0])
#define FS_IsValidID(pidl) ((SIL_GetType(pidl) & SHID_GROUPMASK) == SHID_FS)
#define NET_IsValidID(pidl) ((SIL_GetType(pidl) & SHID_GROUPMASK) == SHID_NET)
#define ROOT_IsValidID(pidl) ((SIL_GetType(pidl) & SHID_GROUPMASK) == SHID_ROOT)

// unsafe macros
#define _ILSkip(pidl, cb) ((LPITEMIDLIST)(((BYTE*)(pidl)) + cb))
#define _ILNext(pidl) _ILSkip(pidl, (pidl)->mkid.cb)
/*
#define IS_VALID_READ_PTR(ptr, type) (!IsBadReadPtr((ptr), sizeof(type)))
#define IS_VALID_READ_BUFFER(ptr, type, len) (!IsBadReadPtr((ptr), sizeof(type)*(len)))
#define IS_VALID_PIDL(ptr) (IsValidPIDL(ptr))

// IsBadReadPtr hazi v debug verzi exceptions a to nechceme
BOOL
IsValidPIDL(LPCITEMIDLIST pidl)
{
  return (IS_VALID_READ_PTR(pidl, USHORT) &&
          IS_VALID_READ_BUFFER((LPBYTE)pidl+sizeof(USHORT), BYTE, pidl->mkid.cb) &&
          (0 == _ILNext(pidl)->mkid.cb || IS_VALID_PIDL(_ILNext(pidl))));
}
*/

// IsBadReadPtr hazi v debug verzi exceptions a to nechceme
BOOL IsGoodPIDL(LPCITEMIDLIST pidl, int cb)
{
    if (cb < sizeof(USHORT) || cb < (int)(pidl->mkid.cb + sizeof(USHORT)))
        return FALSE;

    if (pidl->mkid.cb > 512)
        return FALSE;

    if (_ILNext(pidl)->mkid.cb == 0) // terminator == validni
        return TRUE;

    cb -= pidl->mkid.cb;

    return IsGoodPIDL(_ILNext(pidl), cb);
}

const char* DumpPidl(LPCITEMIDLIST pidl)
{
    static TCHAR szBuf[MAX_PATH];
    TCHAR szTmp[MAX_PATH];
    USHORT cb;
    LPCTSTR pszT;

    szBuf[0] = 0;

    if (NULL == pidl)
    {
        StrNCat(szBuf, "Empty pidl", sizeof(szBuf));
        return szBuf;
    }

    while (!ILIsEmpty(pidl))
    {
        cb = pidl->mkid.cb;
        wsprintf(szTmp, "cb:%x id:", cb);
        StrNCat(szBuf, szTmp, sizeof(szBuf));

        switch (SIL_GetType(pidl) & SHID_TYPEMASK)
        {
        case SHID_ROOT:
            pszT = "SHID_ROOT";
            break;
        case SHID_ROOT_REGITEM:
            pszT = "SHID_ROOT_REGITEM";
            break;
        case SHID_COMPUTER:
            pszT = "SHID_COMPUTER";
            break;
        case SHID_COMPUTER_1:
            pszT = "SHID_COMPUTER_1";
            break;
        case SHID_COMPUTER_REMOVABLE:
            pszT = "SHID_COMPUTER_REMOVABLE";
            break;
        case SHID_COMPUTER_FIXED:
            pszT = "SHID_COMPUTER_FIXED";
            break;
        case SHID_COMPUTER_REMOTE:
            pszT = "SHID_COMPUTER_REMOTE";
            break;
        case SHID_COMPUTER_CDROM:
            pszT = "SHID_COMPUTER_CDROM";
            break;
        case SHID_COMPUTER_RAMDISK:
            pszT = "SHID_COMPUTER_RAMDISK";
            break;
        case SHID_COMPUTER_7:
            pszT = "SHID_COMPUTER_7";
            break;
        case SHID_COMPUTER_DRIVE525:
            pszT = "SHID_COMPUTER_DRIVE525";
            break;
        case SHID_COMPUTER_DRIVE35:
            pszT = "SHID_COMPUTER_DRIVE35";
            break;
        case SHID_COMPUTER_NETDRIVE:
            pszT = "SHID_COMPUTER_NETDRIVE";
            break;
        case SHID_COMPUTER_NETUNAVAIL:
            pszT = "SHID_COMPUTER_NETUNAVAIL";
            break;
        case SHID_COMPUTER_C:
            pszT = "SHID_COMPUTER_C";
            break;
        case SHID_COMPUTER_D:
            pszT = "SHID_COMPUTER_D";
            break;
        case SHID_COMPUTER_REGITEM:
            pszT = "SHID_COMPUTER_REGITEM";
            break;
        case SHID_COMPUTER_MISC:
            pszT = "SHID_COMPUTER_MISC";
            break;
        case SHID_FS:
            pszT = "SHID_FS";
            break;
        case SHID_FS_TYPEMASK:
            pszT = "SHID_FS_TYPEMASK";
            break;
        case SHID_FS_DIRECTORY:
            pszT = "SHID_FS_DIRECTORY";
            break;
        case SHID_FS_FILE:
            pszT = "SHID_FS_FILE";
            break;
        case SHID_FS_UNICODE:
            pszT = "SHID_FS_UNICODE";
            break;
        case SHID_FS_DIRUNICODE:
            pszT = "SHID_FS_DIRUNICODE";
            break;
        case SHID_FS_FILEUNICODE:
            pszT = "SHID_FS_FILEUNICODE";
            break;
        case SHID_NET:
            pszT = "SHID_NET";
            break;
        case SHID_NET_DOMAIN:
            pszT = "SHID_NET_DOMAIN";
            break;
        case SHID_NET_SERVER:
            pszT = "SHID_NET_SERVER";
            break;
        case SHID_NET_SHARE:
            pszT = "SHID_NET_SHARE";
            break;
        case SHID_NET_FILE:
            pszT = "SHID_NET_FILE";
            break;
        case SHID_NET_GROUP:
            pszT = "SHID_NET_GROUP";
            break;
        case SHID_NET_NETWORK:
            pszT = "SHID_NET_NETWORK";
            break;
        case SHID_NET_RESTOFNET:
            pszT = "SHID_NET_RESTOFNET";
            break;
        case SHID_NET_SHAREADMIN:
            pszT = "SHID_NET_SHAREADMIN";
            break;
        case SHID_NET_DIRECTORY:
            pszT = "SHID_NET_DIRECTORY";
            break;
        case SHID_NET_TREE:
            pszT = "SHID_NET_TREE";
            break;
        case SHID_NET_REGITEM:
            pszT = "SHID_NET_REGITEM";
            break;
        case SHID_NET_PRINTER:
            pszT = "SHID_NET_PRINTER";
            break;
        default:
            pszT = "unknown";
            break;
        }
        StrNCat(szBuf, pszT, sizeof(szBuf));

        if (SIL_GetType(pidl) & SHID_JUNCTION)
            StrNCat(szBuf, ", junction", sizeof(szBuf));

        pidl = _ILNext(pidl);

        if (!ILIsEmpty(pidl))
            StrNCat(szBuf, "; ", sizeof(szBuf));
    }

    return szBuf;
}

//-------------------------------------------------------------------------

void _OutputDebugString(BOOL useTServer, const char* text)
{
    if (useTServer)
        TRACE_I(text);
    OutputDebugString(text);
    OutputDebugString("\n");
}

//-------------------------------------------------------------------------

#define ASSERT(exp) (void)((exp) || (TRACE_E(#exp), DebugBreak(), 0))

#define SPYSIG 0x66FFAA55

// nas pomocny datovy blok umistovany pred vlastni alokovana data

#define SPYBLK_STACKLEN 100 // pocet znaku z callstacku, vcetne terminatoru

struct SPYBLK
{
    DWORD dwSig;                       // signatura
    SPYBLK* psbNext;                   // nasledujici alokovany blok
    SPYBLK* psbPrev;                   // predchozi alokovany blok
    SIZE_T cbRequest;                  // pozadovana velikost
    DWORD cRealloc;                    // kolikrat byla polozka realokovana
    DWORD cOrder;                      // kolikata alokace celkem to byla (pro break)
    DWORD dwThreadId;                  // ze ktereho threadu je leak?
    char szStackHead[SPYBLK_STACKLEN]; // prvni radek z call stacku
    char szStackTail[SPYBLK_STACKLEN]; // posledni radek z call stacku
};

class CMallocSpy : public IMallocSpy
{
private:
    ULONG _ulRef;      // pocet referenci objektu CMallocSpy
    SIZE_T _iAllocs;   // pocet alokaci
    SIZE_T _iBytes;    // aktualni alokovana velikost
    SPYBLK* _psbHead;  // prvni ze seznamu alokaci
    SIZE_T _cbRequest; // cache z PRE -> POST
    //void             *_pvRequest;   // cache z PRE -> POST
    LONG _iTotalAllocs;   // celkovy pocet alokaci
    SIZE_T _iTotalBytes;  // celkovy pocet bajtu
    SIZE_T _iPeakAllocs;  // maximalni pocet alokaci v jednu chvili
    SIZE_T _iPeakBytes;   // maximalni pocet bajtu v jedni chvili
    LONG _iBreakAlloc;    // alokace, na ktere mame vyvolat break (-1 == no break)
    CRITICAL_SECTION _CS; // kriticka sekce pro pristup k nasim datum (napriklad behem dumpu)

public:
    CMallocSpy();
    ~CMallocSpy();

    // IUnknown methods

    STDMETHOD(QueryInterface)
    (REFIID riid, void** ppv);
    STDMETHOD_(ULONG, AddRef)
    ();
    STDMETHOD_(ULONG, Release)
    ();

    // IMallocSpy methods

    STDMETHOD_(SIZE_T, PreAlloc)
    (SIZE_T cbRequest);
    STDMETHOD_(void*, PostAlloc)
    (void* pvActual);
    STDMETHOD_(void*, PreFree)
    (void* pvRequest, BOOL fSpyed);
    STDMETHOD_(void, PostFree)
    (BOOL fSpyed);
    STDMETHOD_(SIZE_T, PreRealloc)
    (void* pvRequest, SIZE_T cbRequest, void** ppvActual, BOOL fSpyed);
    STDMETHOD_(void*, PostRealloc)
    (void* pvActual, BOOL fSpyed);
    STDMETHOD_(void*, PreGetSize)
    (void* pvRequest, BOOL fSpyed);
    STDMETHOD_(SIZE_T, PostGetSize)
    (SIZE_T cbActual, BOOL fSpyed);
    STDMETHOD_(void*, PreDidAlloc)
    (void* pvRequest, BOOL fSpyed);
    STDMETHOD_(BOOL, PostDidAlloc)
    (void* pvRequest, BOOL fSpyed, BOOL fActual);
    STDMETHOD_(void, PreHeapMinimize)
    ();
    STDMETHOD_(void, PostHeapMinimize)
    ();

    // Other methods

    void SetBreakOnAlloc(int alloc);
    BOOL DumpLeaks();

private:
    // Helper Functions

    void EnterCS();
    void LeaveCS();

    void SpyEnqueue(SPYBLK* psb);
    void SpyDequeue(SPYBLK* psb);

    LPVOID SpyPostAlloc(SPYBLK* psb);
    LPVOID SpyPreFree(void* pvRequest);
    size_t SpyPreRealloc(void* pvRequest, size_t cbRequest, void** ppv);
    LPVOID SpyPostRealloc(SPYBLK* psb);
    void SpyStoreStack(SPYBLK* psb);
};

// ******************************************************************
// ******************************************************************
// Constructor/Destructor
// ******************************************************************
// ******************************************************************

CMallocSpy::CMallocSpy()
{
    _ulRef = 0;
    _iAllocs = 0;
    _iBytes = 0;
    _psbHead = NULL;
    _cbRequest = 0;
    //_pvRequest = NULL;
    _iTotalAllocs = 0;
    _iTotalBytes = 0;
    _iPeakAllocs = 0;
    _iPeakBytes = 0;
    _iBreakAlloc = -1;

    InitializeCriticalSection(&_CS);
}

CMallocSpy::~CMallocSpy()
{
    // POZOR, tento destruktor bude zavolan pro templateSpy, viz dole
    // nesmi zrusit kritickou sekci
    //  DeleteCriticalSection(&_CS);
}

// ******************************************************************
// ******************************************************************
// IUnknown support ...
// ******************************************************************
// ******************************************************************

STDMETHODIMP CMallocSpy::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IMallocSpy)
    {
        *ppv = (IMallocSpy*)this;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG)
CMallocSpy::AddRef()
{
    InterlockedIncrement((LONG*)&_ulRef);
    return 2;

#if 0
  return InterlockedIncrement((LONG *)&_ulRef);
#endif
}

STDMETHODIMP_(ULONG)
CMallocSpy::Release()
{
    InterlockedDecrement((LONG*)&_ulRef);
    return 1;
#if 0
  if (InterlockedDecrement((LONG *)&_ulRef))
    return _ulRef;
  else
  {
    delete this;
    return 0;
  }
#endif
}

// ******************************************************************
// ******************************************************************
// IMallocSpy methods ...
// ******************************************************************
// ******************************************************************

STDMETHODIMP_(SIZE_T)
CMallocSpy::PreAlloc(SIZE_T cbRequest)
{
    _cbRequest = cbRequest;

    return sizeof(SPYBLK) + cbRequest;
}

STDMETHODIMP_(void*)
CMallocSpy::PostAlloc(void* pvActual)
{
    void* pv;

    pv = SpyPostAlloc((SPYBLK*)pvActual);

    return pv;
}

STDMETHODIMP_(void*)
CMallocSpy::PreFree(void* pvRequest, BOOL fSpyed)
{
    void* pv;

    if (fSpyed)
        pv = SpyPreFree(pvRequest);
    else
        pv = pvRequest;

    return pv;
}

STDMETHODIMP_(void)
CMallocSpy::PostFree(BOOL fSpyed)
{

    // Tyto dealokace chodi pravidelne, nebudeme s nima spinit log
    //  if (!fSpyed)
    //  {
    //    char buff[200];
    //    sprintf(buff, "CMallocSpy freeing a block alloced before we started. Thread 0x%X", GetCurrentThreadId());
    //    _OutputDebugString(FALSE, buff);
    //  }
}

STDMETHODIMP_(SIZE_T)
CMallocSpy::PreRealloc(void* pvRequest, SIZE_T cbRequest, void** ppvActual, BOOL fSpyed)
{
    SIZE_T cb;

    if (fSpyed)
        cb = SpyPreRealloc(pvRequest, cbRequest, ppvActual);
    else
    {
        *ppvActual = pvRequest;
        cb = cbRequest;
    }

    return cb;
}

STDMETHODIMP_(void*)
CMallocSpy::PostRealloc(void* pvActual, BOOL fSpyed)
{
    void* pv;

    if (fSpyed)
        pv = SpyPostRealloc((SPYBLK*)pvActual);
    else
        pv = pvActual;

    return pv;
}

STDMETHODIMP_(void*)
CMallocSpy::PreGetSize(void* pvRequest, BOOL fSpyed)
{
    void* pv;

    if (fSpyed)
    {
        pv = (void*)(((BYTE*)pvRequest) - sizeof(SPYBLK));
    }
    else
        pv = pvRequest;

    return pv;
}

STDMETHODIMP_(SIZE_T)
CMallocSpy::PostGetSize(SIZE_T cbActual, BOOL fSpyed)
{
    SIZE_T cb;

    if (fSpyed)
    {
        return cbActual - sizeof(SPYBLK);
    }
    else
        cb = cbActual;

    return cb;
}

STDMETHODIMP_(void*)
CMallocSpy::PreDidAlloc(void* pvRequest, BOOL fSpyed)
{
    void* pv;

    if (fSpyed)
        pv = (((BYTE*)pvRequest) - sizeof(SPYBLK));
    else
        pv = pvRequest;

    return pv;
}

STDMETHODIMP_(BOOL)
CMallocSpy::PostDidAlloc(void* pvRequest, BOOL fSpyed, BOOL fActual)
{
    return fActual;
}

STDMETHODIMP_(void)
CMallocSpy::PreHeapMinimize()
{
}

STDMETHODIMP_(void)
CMallocSpy::PostHeapMinimize()
{
}

// ******************************************************************
// ******************************************************************
// Helper Functions ...
// ******************************************************************
// ******************************************************************

void CMallocSpy::EnterCS()
{
    EnterCriticalSection(&_CS);
}

void CMallocSpy::LeaveCS()
{
    LeaveCriticalSection(&_CS);
}

void CMallocSpy::SpyEnqueue(SPYBLK* psb)
{
    EnterCS();

    _iAllocs++;
    _iBytes += psb->cbRequest;

    _iTotalAllocs++;
    _iTotalBytes += psb->cbRequest;

    // pouze pro statisticke ucely
    if (_iBytes > _iPeakBytes)
        _iPeakBytes = _iBytes;
    if (_iAllocs > _iPeakAllocs)
        _iPeakAllocs = _iAllocs;

    // zapojime na zacatek
    psb->psbPrev = NULL;
    psb->psbNext = _psbHead;
    if (_psbHead != NULL)
    {
        ASSERT(_psbHead->psbPrev == NULL);
        _psbHead->psbPrev = psb;
    }
    _psbHead = psb;

    psb->dwSig = SPYSIG;
    psb->cOrder = _iTotalAllocs;

    SpyStoreStack(psb);

    if (_iBreakAlloc == _iTotalAllocs)
        DebugBreak();

    LeaveCS();
}

void CMallocSpy::SpyDequeue(SPYBLK* psb)
{
    EnterCS();

    if (psb->psbPrev != NULL)
    {
        ASSERT(psb->psbPrev->psbNext != NULL);
        psb->psbPrev->psbNext = psb->psbNext;
    }

    if (psb->psbNext != NULL)
    {
        ASSERT(psb->psbNext->psbPrev != NULL);
        psb->psbNext->psbPrev = psb->psbPrev;
    }

    if (psb == _psbHead)
        _psbHead = psb->psbNext;

    _iAllocs--;
    _iBytes -= psb->cbRequest;

    LeaveCS();
}

LPVOID
CMallocSpy::SpyPostAlloc(SPYBLK* psb)
{
    psb->cbRequest = _cbRequest;
    psb->cRealloc = 0;
    psb->dwThreadId = GetCurrentThreadId();

    if (psb != NULL)
        SpyEnqueue(psb);

    return (BYTE*)psb + sizeof(SPYBLK);
}

LPVOID
CMallocSpy::SpyPreFree(void* pvRequest)
{
    if (!pvRequest)
        return NULL;

    pvRequest = (void*)(((BYTE*)pvRequest) - sizeof(SPYBLK));
    SPYBLK* psb = (SPYBLK*)pvRequest;
    if (psb->dwSig != SPYSIG)
        _OutputDebugString(FALSE, "psb->dwSig != SPYSIG");
    SpyDequeue(psb);

    return (psb);
}

size_t
CMallocSpy::SpyPreRealloc(void* pvRequest, size_t cbRequest, void** ppv)
{
    ASSERT(pvRequest != NULL); // v tomto pripade vola OLE IMallocSpy::PreAlloc
    ASSERT(cbRequest != 0);    // v tomto pripade vola OLE IMallocSpy::PreFree

    size_t cb;

    _cbRequest = cbRequest;
    //_pvRequest = pvRequest;

    SPYBLK* psb = (SPYBLK*)(((BYTE*)pvRequest) - sizeof(SPYBLK));
    SpyDequeue(psb);
    ASSERT(psb->dwSig == SPYSIG);

    *ppv = psb;
    cb = sizeof(SPYBLK) + cbRequest;

    return cb;
}

void* CMallocSpy::SpyPostRealloc(SPYBLK* psb)
{
    ASSERT(psb != NULL);

    void* pvReturn;

    psb->cbRequest = _cbRequest;
    psb->cRealloc++;

    SpyEnqueue(psb);

    pvReturn = (BYTE*)psb + sizeof(SPYBLK);

    return pvReturn;
}

void CMallocSpy::SpyStoreStack(SPYBLK* psb)
{
    CCallStack* stack = CCallStack::GetThis();
    if (stack != NULL)
    {
        stack->Reset();
        lstrcpyn(psb->szStackHead, stack->GetNextLine(), SPYBLK_STACKLEN);
        const char* sOld = NULL;
        const char* sNew;
        do
        {
            sNew = stack->GetNextLine();
            if (sNew == NULL && sOld != NULL)
                lstrcpyn(psb->szStackTail, sOld, SPYBLK_STACKLEN);
            sOld = sNew;
        } while (sNew != NULL);
    }
    else
    {
        psb->szStackHead[0] = 0;
        psb->szStackTail[0] = 0;
    }
}

void CMallocSpy::SetBreakOnAlloc(int alloc)
{
    _iBreakAlloc = alloc;
}

BOOL IsAsciiString(LPSTR pv, int cb)
{
    BOOL bRet = TRUE;
    while (cb && *pv && bRet)
    {
        if (*pv > '~' || *pv < ' ')
            bRet = FALSE;
        cb--;
        pv++;
    }
    return (!cb && bRet && !*pv); // we've only had ascii characters, and we're null terminated
}

STDAPI _StrRetToBuf(STRRET* psr, LPCITEMIDLIST pidl, LPSTR pszBuf, UINT cchBuf)
{
    HRESULT hres = E_FAIL;

    if (cchBuf == 0)
    {
        TRACE_E("_StrRetToBuf cchBuf=0"); // na tohle nejsme staveni
        return hres;
    }

    switch (psr->uType)
    {
    case STRRET_WSTR:
    {
        LPWSTR pszStr = psr->pOleStr;
        if (pszStr != NULL)
        {
            int chars = WideCharToMultiByte(CP_ACP, 0, pszStr,
                                            -1, pszBuf, cchBuf, NULL, NULL);
            if (chars == 0)
            {
                DWORD err = GetLastError();
                if (err == ERROR_INSUFFICIENT_BUFFER)
                {
                    TRACE_E("_StrRetToBuf: buffer is short cchBuf=" << cchBuf);
                    pszBuf[cchBuf - 1] = 0;
                    hres = S_OK;
                }
                else
                {
                    TRACE_E("_StrRetToBuf: WideCharToMultiByte failed, error=" << err);
                    pszBuf[0] = 0;
                }
            }
            else
                hres = S_OK;
            CoTaskMemFree(pszStr); // SHFree function is deprecated.
        }
        break;
    }

    case STRRET_CSTR:
    {
        const char* str = (const char*)psr->cStr;
        if (strlen(str) + 1 > cchBuf)
            TRACE_E("_StrRetToBuf: buffer is short cchBuf=" << cchBuf);
        lstrcpyn(pszBuf, str, cchBuf);
        hres = S_OK;
        break;
    }

    case STRRET_OFFSET:
    {
        if (pidl != NULL)
        {
            const char* str = (const char*)pidl + psr->uOffset;
            if (strlen(str) + 1 > cchBuf)
                TRACE_E("_StrRetToBuf: buffer is short cchBuf=" << cchBuf);
            lstrcpyn(pszBuf, str, cchBuf);
            hres = S_OK;
        }
        break;
    }
    }

    if (FAILED(hres) && cchBuf != 0)
        *pszBuf = 0;

    return hres;
}

BOOL CMallocSpy::DumpLeaks()
{
    EnterCS();

    char buff[MAX_PATH + 200];
    _OutputDebugString(TRUE, "~~~~~~~~~~~~ CMallocSpy Report Begin ~~~~~~~~~~~~");
    sprintf(buff, "Memory Stats: %d allocations, %Iu bytes allocated", _iTotalAllocs, _iTotalBytes);
    _OutputDebugString(TRUE, buff);
    sprintf(buff, "Memory Peaks: %Iu allocations; %Iu bytes allocated", _iPeakAllocs, _iPeakBytes);
    _OutputDebugString(TRUE, buff);

    SIZE_T leakedBytes = 0;
    int leakedAllocs = 0;
    SPYBLK* psbWalk = _psbHead;

    // dojedem na konec a pojedem dopredu
    if (psbWalk != NULL)
        while (psbWalk->psbNext != NULL)
            psbWalk = psbWalk->psbNext;

    while (psbWalk != NULL)
    {
        // FIXME_X64 psbWalk->cbRequest je typu size_t, ale tiskneme ho jako %d, coz je zrejme chyba
        // projit zbytek kodu, kde by se chyba jeste mohla vyskytovat, zrejme by melo byt (%Id) - http://msdn.microsoft.com/en-us/library/tcxf1dw6.aspx
        sprintf(buff, "[%u] Leaked %Iu bytes at 0x%p, from thread 0x%X",
                psbWalk->cOrder, psbWalk->cbRequest,
                (BYTE*)psbWalk + sizeof(SPYBLK), psbWalk->dwThreadId);
        _OutputDebugString(TRUE, buff);
        if (psbWalk->cRealloc)
        {
            sprintf(buff, "  Data was re-alloced %u times", psbWalk->cRealloc);
            _OutputDebugString(TRUE, buff);
        }
        if (psbWalk->szStackHead[0] != 0)
        {
            sprintf(buff, "  Call Stack Head: '%s'", psbWalk->szStackHead);
            _OutputDebugString(TRUE, buff);
        }
        if (psbWalk->szStackTail[0] != 0)
        {
            sprintf(buff, "  Call Stack Tail: '%s'", psbWalk->szStackTail);
            _OutputDebugString(TRUE, buff);
        }
        __try
        {
            int iUniFlags = IS_TEXT_UNICODE_ASCII16 | IS_TEXT_UNICODE_STATISTICS;  // kazde volani IsTextUnicode musi mit svoji promennou, protoze ji prepise
            int iUniFlags2 = IS_TEXT_UNICODE_ASCII16 | IS_TEXT_UNICODE_STATISTICS; // --||--
            void* pvRequest = (BYTE*)psbWalk + sizeof(SPYBLK);
            if (psbWalk->cbRequest >= *((USHORT*)pvRequest) &&
                *((USHORT*)pvRequest) != 0 && // dve nuly na zacatku pameti nebudeme povazovat za PIDL
                IsGoodPIDL((LPCITEMIDLIST)pvRequest, (int)psbWalk->cbRequest))
            {
                // nejaky PIDL
                sprintf(buff, "  Data is pidl %s", DumpPidl((LPITEMIDLIST)pvRequest));
                _OutputDebugString(TRUE, buff);

                if (FS_IsValidID((LPITEMIDLIST)pvRequest))
                {
                    char szTemp[MAX_PATH];
                    SHGetPathFromIDList((LPCITEMIDLIST)pvRequest, szTemp);
                    if (szTemp[0])
                    {
                        sprintf(buff, "  Pidl for '%s'", szTemp);
                        _OutputDebugString(TRUE, buff);
                    }
                    else if (psbWalk->cbRequest > 16 && SIL_GetType((LPITEMIDLIST)pvRequest) == SHID_FS_FILE) // SHID_FS_FILE == 0x32
                    {
                        sprintf(buff, "  May be a relative pidl for '%s'", ((LPBYTE)pvRequest) + 14);
                        _OutputDebugString(TRUE, buff);
                    }
                }
                else if (ROOT_IsValidID((LPITEMIDLIST)pvRequest))
                {
                    IShellFolder* desktopFolder;
                    if (SUCCEEDED(SHGetDesktopFolder(&desktopFolder)))
                    {
                        char szTemp[MAX_PATH];
                        if (SHGetPathFromIDList((LPCITEMIDLIST)pvRequest, szTemp) && szTemp[0] != 0)
                        {
                            sprintf(buff, "  Pidl for '%s'", szTemp);
                            _OutputDebugString(TRUE, buff);
                        }
                        else
                        {
                            STRRET str;
                            if (SUCCEEDED(desktopFolder->GetDisplayNameOf((LPITEMIDLIST)pvRequest, SHGDN_NORMAL, &str)))
                            {
                                char name[MAX_PATH];
                                if (SUCCEEDED(_StrRetToBuf(&str, (LPITEMIDLIST)pvRequest, name, MAX_PATH)))
                                {
                                    sprintf(buff, "  Pidl for '%s'", name);
                                    _OutputDebugString(TRUE, buff);
                                }
                            }
                            desktopFolder->Release();
                        }
                    }
                }
            }
            else if (psbWalk->cbRequest > 8 &&
                     *((DWORD*)pvRequest) != 0 && // prvni 4 bajty jsou 0 -> to precni neni UNICODE string
                     IsTextUnicode((LPWSTR)pvRequest, (int)psbWalk->cbRequest - 2, &iUniFlags))
            {
                sprintf(buff, "  Data is UNICODE string '%ls'", (LPWSTR)pvRequest);
                _OutputDebugString(TRUE, buff);
            }
            else if (psbWalk->cbRequest > 8 && *((DWORD*)pvRequest) <= psbWalk->cbRequest - 4 && IsTextUnicode((LPWSTR)((BYTE*)pvRequest + 4), *((DWORD*)pvRequest), &iUniFlags2))
            {
                sprintf(buff, "  Data is BSTR string '%ls'", (LPWSTR)((BYTE*)pvRequest + 4));
                _OutputDebugString(TRUE, buff);
            }
            else if (IsAsciiString((LPSTR)pvRequest, (int)psbWalk->cbRequest))
            {
                sprintf(buff, "  Data is ASCII string '%s'", (LPSTR)pvRequest);
                _OutputDebugString(TRUE, buff);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            _OutputDebugString(TRUE, "Exception during memory analyzing");
        }

        leakedBytes += psbWalk->cbRequest;
        leakedAllocs++;
        psbWalk = psbWalk->psbPrev;
        if (psbWalk != NULL)
            _OutputDebugString(TRUE, "");
    }
    if (leakedAllocs != 0)
    {
        _OutputDebugString(TRUE, "-------------------------------------------------");
        sprintf(buff, "Memory Leaks Summary: %d allocations, %Iu bytes leaked", leakedAllocs, leakedBytes);
        _OutputDebugString(TRUE, buff);
        _OutputDebugString(TRUE, "Hint: to break on [n] allocation call OleSpySetBreak(n).");
    }
    else
    {
        _OutputDebugString(TRUE, "All allocations were deallocated.");
    }
    _OutputDebugString(TRUE, "~~~~~~~~~~~~~~~~~~ Report End ~~~~~~~~~~~~~~~~~~~");
    LeaveCS();
    return TRUE;
}

CMallocSpy* OleSpy = NULL;

#endif // _DEBUG

BOOL OleSpyRegister()
{
#ifdef _DEBUG
    if (OleSpy != NULL)
    {
        TRACE_E("OleSpyRegister: There is already a registered spy.");
        return FALSE;
    }

    // korekrni instance objetku
    CMallocSpy templateSpy;

    // cunarna: nechceme memory leaky v CRT, takze objekt nealokujeme na heapu
    // zaroven chceme korektne nakonstruovanou VMT, takze okopirujeme obsah predlohy
    static byte _Spy[sizeof(templateSpy)];
    memcpy(_Spy, &templateSpy, sizeof(templateSpy));

    // CRT nevi, ze OleSpy je objekt, takze nikdy nezavola jeho destruktor
    OleSpy = (CMallocSpy*)_Spy;
    HRESULT hr = CoRegisterMallocSpy(OleSpy);
    if (hr == CO_E_OBJISREG)
    {
        TRACE_E("CoRegisterMallocSpy: There is already a registered spy.");
        OleSpy = NULL;
        return FALSE;
    }
#endif // _DEBUG

    return TRUE;
}

void OleSpyRevoke()
{
#ifdef _DEBUG
    if (OleSpy == NULL)
    {
        TRACE_E("OleSpyRevoke: No spy is currently registered.");
        return;
    }

    HRESULT hr = CoRevokeMallocSpy();
    if (hr != S_OK)
    {
        if (hr == CO_E_OBJNOTREG)
            TRACE_E("CoRevokeMallocSpy: No spy is currently registered.");
        else if (hr == E_ACCESSDENIED)
            TRACE_I("CoRevokeMallocSpy: Spy is registered but there are outstanding allocations (not yet freed) made while this spy was active.");
    }
#endif // _DEBUG
}

void OleSpySetBreak(int alloc)
{
#ifdef _DEBUG
    if (OleSpy == NULL)
    {
        TRACE_E("OleSpyDump: No spy is currently registered.");
        return;
    }
    OleSpy->SetBreakOnAlloc(alloc);
#endif // _DEBUG
}

void OleSpyDump()
{
#ifdef _DEBUG
    if (OleSpy == NULL)
    {
        TRACE_E("OleSpyDump: No spy is currently registered.");
        return;
    }
    OleSpy->DumpLeaks();
#endif // _DEBUG
}

/*
unsigned OleSpyStressTest(void *param)
{
  CALL_STACK_MESSAGE1("OleSpyStressTest()");
  SetThreadNameInVCAndTrace("OleSpyStressTest");
  TRACE_I("Begin");

  IMalloc *alloc;
  if (SUCCEEDED(CoGetMalloc(1, &alloc)))
  {
  int i;
    for (i = 0; i < 10000; i++)
    {
      int size = 2 + rand();
      BYTE *p = (BYTE*)alloc->Alloc(size);
      
      BYTE pattern = 0;
      int j;
      for (j = 0; j < size; j++)
        p[j] = pattern++;

      size = size / 2;
      p = (BYTE*)alloc->Realloc(p, size);
      pattern = 0;
      for (j = 0; j < size; j++)
        if (p[j] != pattern++)
          TRACE_E("error");

      alloc->Free(p);

      //Sleep(1);
    }
    alloc->Release();
  }
    
  TRACE_I("End");
  return 0;
}

DWORD WINAPI OleSpyStressTestF(void *param)
{
  CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
  CCallStack stack;
#endif // CALLSTK_DISABLE
  return OleSpyStressTest(param);
}

void OleSpyStressTest()
{
  // nahodime thready, ve kterych potrapime IMalloc

  DWORD threadID;
  HANDLE thread1 = HANDLES(CreateThread(NULL, 0, OleSpyStressTestF, 0, 0, &threadID));
  HANDLE thread2 = HANDLES(CreateThread(NULL, 0, OleSpyStressTestF, 0, 0, &threadID));
  HANDLE thread3 = HANDLES(CreateThread(NULL, 0, OleSpyStressTestF, 0, 0, &threadID));
  HANDLE thread4 = HANDLES(CreateThread(NULL, 0, OleSpyStressTestF, 0, 0, &threadID));
  HANDLE thread5 = HANDLES(CreateThread(NULL, 0, OleSpyStressTestF, 0, 0, &threadID));

  if (thread1 != NULL && WaitForSingleObject(thread1, INFINITE) == WAIT_OBJECT_0) HANDLES(CloseHandle(thread1));
  if (thread2 != NULL && WaitForSingleObject(thread2, INFINITE) == WAIT_OBJECT_0) HANDLES(CloseHandle(thread2));
  if (thread3 != NULL && WaitForSingleObject(thread3, INFINITE) == WAIT_OBJECT_0) HANDLES(CloseHandle(thread3));
  if (thread4 != NULL && WaitForSingleObject(thread4, INFINITE) == WAIT_OBJECT_0) HANDLES(CloseHandle(thread4));
  if (thread5 != NULL && WaitForSingleObject(thread5, INFINITE) == WAIT_OBJECT_0) HANDLES(CloseHandle(thread5));
}
*/
