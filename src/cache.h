// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// inicializace diskove cache, vraci uspech
BOOL InitializeDiskCache();

// jak dlouho se ceka mezi overenimi stavu sledovanych objektu
#define CACHE_HANDLES_WAIT 500
// (100 MB) max. velikost disk-cache v bytech na disku, potreba prenest do konfigurace
#define MAX_CACHE_SIZE CQuadWord(104857600, 0)

// kody chybovych stavu pro metodu CDiskCache::GetName():
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

enum CCacheRemoveType // pokud je mozne zrusit polozku v cache, kdy se tak stane?
{
    crtCache, // az si rekne cache (prekroceni limitu velikosti cache, ...) (FTP - rychlostni cache)
    crtDirect // hned (neni treba vubec cachovat, zrusit hned jak neni potreba)
};

class CDiskCache;
class CCacheHandles;

class CCacheData // tmp-jmeno, informace o souboru nebo adresari na disku, interni pouziti
{
protected:
    char* Name;       // identifikace polozky (cesta k originalu)
    char* TmpName;    // jmeno tmp-souboru na disku (plna cesta)
    HANDLE Preparing; // mutex, ktery "drzi" thread, ktery pripravuje tmp-soubor

    // systemove objekty - pole typu (HANDLE): stav "signaled" -> zrusit tento 'lock'
    TDirectArray<HANDLE> LockObject;
    // vlastnictvi objektu - pole typu (BOOL): TRUE -> volat CloseHandle('lock')
    TDirectArray<BOOL> LockObjOwner;

    BOOL Cached;                               // jde o cachovany tmp-soubor? (prislo uz crtCache?)
    BOOL Prepared;                             // je tmp-soubor pripraveny k pouziti? (napr. stazeny z FTP?)
    int NewCount;                              // pocet novych zadosti o tmp-soubor
    CQuadWord Size;                            // velikost tmp-souboru (v bytech)
    int LastAccess;                            // "cas" posledniho pristupu k tmp-souboru (pro cache - vyhazujeme nejstarsi)
    BOOL Detached;                             // TRUE => tmp-soubor se nema mazat
    BOOL OutOfDate;                            // TRUE => jakmile to pujde, ziskame novou kopii (jakoby na disku nebyl)
    BOOL OwnDelete;                            // FALSE = tmp-soubor mazat pres DeleteFile(), TRUE = mazat pres DeleteManager (maze plugin OwnDeletePlugin)
    CPluginInterfaceAbstract* OwnDeletePlugin; // iface pluginu, ktery ma smazat tmp-soubor (NULL = plugin je unloaded, tmp-soubor se nema mazat)

public:
    CCacheData(const char* name, const char* tmpName, BOOL ownDelete,
               CPluginInterfaceAbstract* ownDeletePlugin);
    ~CCacheData();

    // vraci nastavenou velikost tmp-souboru
    CQuadWord GetSize() { return Size; }

    // vraci TRUE, pokud se tento tmp-soubor maze pluginem 'ownDeletePlugin'
    BOOL IsDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin)
    {
        return OwnDelete && OwnDeletePlugin == ownDeletePlugin;
    }

    // vraci "cas" posledniho pristupu k tmp-souboru
    int GetLastAccess() { return LastAccess; }

    // zrusi tmp-soubor na disku, vraci uspech (Name jiz neni na disku)
    BOOL CleanFromDisk();

    // povedla se inicializace objektu ?
    BOOL IsGood() { return Name != NULL; }

    // ma byt tmp-soubor cachovany?
    BOOL IsCached() { return Cached; }

    // je tmp-soubor bezprizorny? (nema uz/jeste zadny link?)
    BOOL IsLocked() { return LockObject.Count == 0 && NewCount == 0; }

    BOOL NameEqual(const char* name) { return StrICmp(Name, name) == 0; }
    BOOL TmpNameEqual(const char* tmpName) { return StrICmp(TmpName, tmpName) == 0; }

    // ceka az bude tmp-soubor pripraven nebo bude zavolana ReleaseName(),
    // pak nastavi 'exists' a navratovou hodnotu v souladu s CDiskCache::GetName()
    // NULL -> fatalni chyba nebo "soubor nepripraven" (viz dale)
    //
    // je-li 'onlyAdd' TRUE, lze jen obnovit smazany tmp-soubor - pokud je tmp-soubor pripraveny,
    // vrati NULL a 'exists' FALSE (dale chapano jako chyba "soubor jiz existuje");
    // 'canBlock' je TRUE pokud ma dojit k cekani na pripravenost tmp-souboru v pripade, ze
    // neni pripraveny, je-li 'canBlock' FALSE a tmp-soubor neni pripraven, vraci NULL a
    // 'exists' FALSE ("nenalezeno"); neni-li 'errorCode' NULL, vraci se v nem kod chyby,
    // ktera nastala (viz DCGNE_XXX)
    const char* GetName(CDiskCache* monitor, BOOL* exists, BOOL canBlock, BOOL onlyAdd,
                        int* errorCode);

    // popis viz CDiskCache::NamePrepared()
    BOOL NamePrepared(const CQuadWord& size);

    // popis viz CDiskCache::AssignName()
    //
    // handles - objekt pro sledovani 'lock' objektu
    BOOL AssignName(CCacheHandles* handles, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove);

    // popis viz CDiskCache::ReleaseName()
    //
    // lastLock - ukazatel na BOOL, ve kterem metoda vraci TRUE pokud jiz nejsou zadne linky na tmp-soubor
    BOOL ReleaseName(BOOL* lastLock, BOOL storeInCache);

    // vraci plne jmeno tmp-souboru
    const char* GetTmpName() { return TmpName; }

    // odpoji objekt 'lock' (v "signaled" stavu) od tmp-souboru (odpoji link)
    //
    // vraci uspech
    //
    // lock - objekt, ktery presel do "signaled" stavu
    // lastLock - ukazatel na BOOL, ve kterem metoda vraci TRUE pokud jiz nejsou zadne linky na tmp-soubor
    BOOL WaitSatisfied(HANDLE lock, BOOL* lastLock);

    // pokud si rozmyslime smazani souboru na disku (napr. nepodaril se zapakovat, nechame
    // ho tedy radsi v tempu, aby nas useri nezabili)
    void DetachTmpFile() { Detached = TRUE; }

    // meni typ tmp-souboru na crtDirect (prime smazani po pouziti)
    void SetOutOfDate()
    {
        Cached = FALSE;
        OutOfDate = TRUE;
    }

    // vraci identifikaci polozky (cesta k originalu)
    const char* GetName() { return Name; }

    // provede predcasne smazani tmp-souboru, pokud se maze pomoci pluginu
    // 'ownDeletePlugin'; pouziva se pri unloadu pluginu (u tmp-souboru se nastavi,
    // ze uz byl smazan - az se na nej zavrou vsechny odkazy, k mazani uz nedojde);
    // je-li 'onlyDetach' TRUE, nedojde ke smazani, dojde jen k nastaveni, ze uz byl
    // smazany (odpojeni pluginu od tmp-souboru)
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);
};

//****************************************************************************
//
// CCacheDirData
//

class CCacheDirData // tmp-adresar, obsahuje unikatni tmp-jmena, interni pouziti
{
protected:
    char Path[MAX_PATH];             // reprezentace tmp-adresare na disku
    int PathLength;                  // delka retezce v Path
    TDirectArray<CCacheData*> Names; // seznam zaznamu, typ polozky (CCacheData *)

public:
    CCacheDirData(const char* path);
    ~CCacheDirData();

    int GetNamesCount() { return Names.Count; }

    // pokud v nasem tmp-adresari na disku neni zadny soubor, vymazeme ho z disku
    // (docisteni TEMPu - viz CDiskCache::RemoveEmptyTmpDirsOnlyFromDisk())
    void RemoveEmptyTmpDirsOnlyFromDisk();

    // vraci TRUE pokud tmp-adresar obsahuje tmpName (jmeno souboru/adresare na disku)
    // rootTmpPath - cesta kam umistit tmp-adresar s tmp-souborem (nesmi byt NULL)
    // rootTmpPathLen - delka retezce rootTmpPath
    // canContainThisName - nesmi byt NULL, vraci se v nem TRUE pokud je mozne umistit
    //                      tmp-soubor do tohoto tmp-adresare (odpovida tmp-root + nelezi
    //                      v nem zadny soubor s DOS-name rovnym 'tmpName')
    BOOL ContainTmpName(const char* tmpName, const char* rootTmpPath, int rootTmpPathLen,
                        BOOL* canContainThisName);

    // hleda 'name' v tmp-adresari; pokud najde, vraci TRUE a hodnoty 'name' a 'tmpPath'
    // tak, aby odpovidaly predpokladanym hodnotam z CDiskCache::GetName(); pokud nenajde
    // vraci FALSE
    //
    // name - jednoznacna identifikace polozky
    // exists - ukazatel na BOOL, ktery se nastavi dle vyse uvedeneho
    // tmpPath - navratova hodnota CDiskCache::GetName() (NULL && 'exists'==TRUE -> fatalni chyba)
    //
    // je-li 'onlyAdd' TRUE, lze vytvorit jen nove jmeno nebo obnovit smazany tmp-soubor
    // (jmeno existuje, ale tmp-soubor neni pripraveny) - pokud jiz jmeno existuje,
    // vrati TRUE, 'exists' FALSE a 'tmpPath' NULL ("soubor jiz existuje");
    // 'canBlock' je TRUE pokud ma dojit k cekani na pripravenost tmp-souboru v pripade, ze
    // 'name' v cache je, ale neni pripravene, je-li 'canBlock' FALSE a tmp-soubor neni
    // pripraven, vraci TRUE, 'exists' FALSE a 'tmpPath' NULL ("nenalezeno");
    // neni-li 'errorCode' NULL, vraci se v nem kod chyby, ktera nastala (viz DCGNE_XXX)
    BOOL GetName(CDiskCache* monitor, const char* name, BOOL* exists, const char** tmpPath,
                 BOOL canBlock, BOOL onlyAdd, int* errorCode);

    // popis viz CDiskCache::GetName() - pridavani noveho 'name'
    const char* GetName(const char* name, const char* tmpName, BOOL* exists, BOOL ownDelete,
                        CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode);

    // hleda 'name' v tmp-adresari; pokud najde, vraci TRUE a 'ret' je navratova hodnota
    // CDiskCache::NamePrepared(name, size); pokud nenajde, vraci FALSE
    // popis viz CDiskCache::NamePrepared()
    BOOL NamePrepared(const char* name, const CQuadWord& size, BOOL* ret);

    // hleda 'name' v tmp-adresari; pokud najde, vraci TRUE a 'ret' je navratova hodnota
    // CDiskCache::AssignName(name, lock, lockOwner, remove); pokud nenajde, vraci FALSE
    // popis viz CDiskCache::AssignName()
    //
    // handles - objekt pro sledovani 'lock' objektu
    BOOL AssignName(CCacheHandles* handles, const char* name, HANDLE lock, BOOL lockOwner,
                    CCacheRemoveType remove, BOOL* ret);

    // hleda 'name' v tmp-adresari; pokud najde, vraci TRUE a 'ret' je navratova hodnota
    // CDiskCache::ReleaseName(name); pokud nenajde, vraci FALSE
    // popis viz CDiskCache::ReleaseName()
    //
    // lastCached - ukazatel na BOOL, ktery obsahuje TRUE pokud jde o posledni link na cachovany tmp-soubor,
    //              nebo-li je potreba rozhodnout o jeho dalsi existenci
    BOOL ReleaseName(const char* name, BOOL* ret, BOOL* lastCached, BOOL storeInCache);

    // hleda 'data' v tmp-adresari; pokud najde vraci TRUE a zrusi tmp-soubor 'data';
    // pokud nenajde, vraci FALSE
    //
    // data - tmp-soubor
    BOOL Release(CCacheData* data);

    // soucet celikosti tmp-souboru v tmp-adresari
    CQuadWord GetSizeOfFiles();

    // naplni pole 'victArr' cachovanymi bezprizornymi tmp-soubory (POZOR: neradi od nejstarsiho po nejmladsi)
    void AddVictimsToArray(TDirectArray<CCacheData*>& victArr);

    // pokud si rozmyslime smazani souboru na disku (napr. nepodaril se zapakovat, nechame
    // ho tedy radsi v tempu, aby nas useri nezabili), vraci uspech
    BOOL DetachTmpFile(const char* tmpName);

    // odstrani vsechny cachovane soubory, ktere zacinaji na "name" (napr. vsechny
    // soubory z jednoho archivu), otevrene soubory oznaci out-of-date, takze se pri
    // dalsim pouziti obnovi (soucasna kopie zustava, aby viewry nervaly)
    void FlushCache(const char* name);

    // odstrani cachovany soubor 'name', otevreny soubor oznaci out-of-date, takze se pri
    // dalsim pouziti obnovi (soucasna kopie zustava, aby viewry nervali); vraci TRUE
    // pokud byl soubor nalezen a odstranen
    BOOL FlushOneFile(const char* name);

    // hledani jmena v poli Names; vraci TRUE pokud bylo 'name' nalezeno (vraci i kde - 'index');
    // vraci FALSE pokud 'name' neni v Names (vraci i kam by se mohlo zaradit - 'index')
    BOOL GetNameIndex(const char* name, int& index);

    // spocita kolik tmp-adresar obsahuje tmp-souboru mazanych pluginem 'ownDeletePlugin'
    int CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin);

    // provede predcasne smazani vsech tmp-souboru, ktere se mazou pomoci pluginu
    // 'ownDeletePlugin'; pouziva se pri unloadu pluginu (u tmp-souboru se nastavi,
    // ze uz byly smazany - az se na ne zavrou vsechny odkazy, k mazani uz nedojde);
    // je-li 'onlyDetach' TRUE, nedojde ke smazani, dojde jen k nastaveni, ze uz byl
    // smazany (odpojeni pluginu od tmp-souboru)
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);
};

//****************************************************************************
//
// CCacheHandles
//

class CCacheHandles
{
protected:
    TDirectArray<HANDLE> Handles;     // pole typu (HANDLE), pole pro WaitForXXX
    TDirectArray<CCacheData*> Owners; // pole typu (CCacheData *), pole vlastniku handlu pro hledani

    HANDLE HandleInBox; // box pro predavani dat - handle+owner
    CCacheData* OwnerInBox;
    HANDLE Box;       // event pro box - "signaled" znamena "box je volny"
    HANDLE BoxFull;   // event pro vyber boxu - "signaled" znamena "vyber box"
    HANDLE Terminate; // event pro ukonceni threadu - "signaled" znamena "koncime"
    HANDLE TestIdle;  // event pro spusteni testu idle-stavu - "signaled" znamena "testuj"
    HANDLE IsIdle;    // event pro oznaceni idle-stavu - "signaled" znamena "jsme v idle-stavu"

    HANDLE Thread; // sledovaci thread

    CDiskCache* DiskCache; // disk-cache, ke ktere tento objekt nalezi

    int Idle; // slouzi ke zjisteni, jestli jsme behem jednoho kola hledani nic nenasli
              // 0 - prazny (a odpoved ne), 1 - dotaz, 2 - zjistuje odpoved, 3 - odpoved ano

public:
    CCacheHandles();
    void SetDiskCache(CDiskCache* diskCache) { DiskCache = diskCache; }

    // destructor objektu
    void Destroy();

    // vraci uspech konstrukce objektu
    BOOL IsGood() { return Box != NULL; }

    // ceka az bude box volny (pokud je box plny, ceka az si sledovaci thread cache prevezme data)
    void WaitForBox();

    // nastavi data v boxu a oznami sledovacimu threadu, ze si ma box vybrat
    void SetBox(HANDLE handle, CCacheData* owner);

    // uvolneni boxu, nastala chyba, box neobsahuje zadna data
    void ReleaseBox();

    // ceka na okamzik, kdy bude mit cache-handles "volno", hodi se pri hromadnem ruseni
    // souboru pomoci jednoho lock objektu (pocka, az budou vsechny zavisle soubory
    // smazane)
    void WaitForIdle();

protected:
    // postupne (maximalne po MAXIMUM_WAIT_OBJECTS) ceka na zadane Handles,
    // vraci trojici (handle, owner, index), pro kterou je handle "signaled" nebo "abandoned"
    // a 'index' je index dvojice (handle, owner) v poli Handles
    void WaitForObjects(HANDLE* handle, CCacheData** owner, int* index);

    // vybere data z boxu a vlozi je do poli
    BOOL ReceiveBox();

    // reaguje na prechod nektereho z 'handle' do "signaled" stavu (tmp-souboru ubyva link),
    void WaitSatisfied(HANDLE handle, CCacheData* owner, int index);

    friend unsigned ThreadCacheHandlesBody(void* param); // nas sledovaci thread
};

//****************************************************************************
//
// CDiskCache
//

class CDiskCache // prideluje jmena pro tmp-soubory
{                // objekt je synchronizovany - monitor
protected:
    CRITICAL_SECTION Monitor;          // sekce pouzita pro synchronizaci tohoto objektu (chovani - monitor)
    CRITICAL_SECTION WaitForIdleCS;    // sekce pouzita pro synchronizaci volani WaitForIdle()
    TDirectArray<CCacheDirData*> Dirs; // seznam tmp-adresaru, typ polozky (CCacheDirData *)
    CCacheHandles Handles;             // objekt, ktery se stara o sledovani 'lock' objektu

public:
    CDiskCache();
    ~CDiskCache();

    // priprava objektu na ukonceni Salamandera (shutdown, log off, nebo jen obycejny exit)
    void PrepareForShutdown();

    // projde tmp-adresare na disku a vymaze ty, ktere jsou prazdne (docisteni vsech TEMPu
    // (napr. Encrypt plugin ma svuj vlastni) po predchozim volani PrematureDeleteByPlugin())
    void RemoveEmptyTmpDirsOnlyFromDisk();

    // vraci uspech inicializace objektu
    BOOL IsGood() { return Handles.IsGood() && Dirs.IsGood(); }

    // pokusi se najit 'name' v cache; pokud najde, ceka az bude tmp-soubor pripraven
    // (napr. dotazen z FTP), pak vraci jmeno tmp-souboru a nastavi 'exists' na TRUE;
    // pokud najde, ale tmp-soubor nekdo smazal z disku, vraci jmeno tmp-souboru a
    // 'exists' FALSE, v tomto pripade je nutne znovu pripravit tmp-soubor, a pak zavolat
    // NamePrepared(); je-li 'tmpName' NULL, ma se 'name' v cache jen najit (nezaklada novy
    // tmp-soubor); je-li 'onlyAdd' TRUE, ma se do cache jen pridavat (pokud jiz tmp-soubor
    // existuje, vraci chybu; pokud nekdo smazal tmp-soubor primo z disku, chape se obnoveni
    // tmp-souboru jako pridani, a proto v tomto pripade nevraci chybu); pokud nenajde a
    // 'tmpName' neni NULL, vytvori nove jmeno pro tmp-soubor, ktere ihned vraci a 'exists'
    // nastavi na FALSE, v tomto pripade je nutne v okamziku pripravenosti tmp-souboru (napr.
    // po jeho dotazeni z FTP) volat metodu NamePrepared() cimz bude soubor k dispozici
    // ostatnim threadum;
    // POZOR: nutne volat AssignName() nebo ReleaseName()
    //
    // navratova hodnota NULL -> "fatalni chyba" ('exists' je TRUE) nebo v pripade,
    //                           ze 'tmpName' je NULL "nenalezeno" ('exists' je FALSE),
    //                           a v pripade, ze 'onlyAdd' je TRUE "soubor jiz existuje"
    //                           ('exists' je FALSE)
    //
    // name - jednoznacna identifikace polozky
    // tmpName - pozadovane jmeno docasneho souboru nebo adresare, bude pro nej vybran tmp-adresar
    // exists - ukazatel na BOOL, ktery se nastavi dle vyse uvedeneho
    // onlyAdd - je-li TRUE, lze vytvorit jen nove jmeno (pokud jiz jmeno existuje, vrati NULL) nebo
    //           obnovit smazany tmp-soubor (jmeno existuje, ale tmp-soubor neni pripraveny)
    // rootTmpPath - je-li NULL, ma se tmp-adresar s tmp-souborem umistit do TEMPu, jinak je to
    //               cesta kam umistit tmp-adresar s tmp-souborem
    // ownDelete - je-li FALSE, maji se tmp-soubory mazat pres DeleteFile(), jinak pres
    //             DeleteManagera (mazani pres plugin - viz ownDeletePlugin)
    // ownDeletePlugin - je-li ownDelete TRUE, obsahuje iface pluginu, ktery provede
    //                   mazani tmp-souboru
    // errorCode - neni-li NULL a nastane-li chyba, jeji kod se vraci v teto promenne (kody viz DCGNE_XXX)
    const char* GetName(const char* name, const char* tmpName, BOOL* exists, BOOL onlyAdd,
                        const char* rootTmpPath, BOOL ownDelete,
                        CPluginInterfaceAbstract* ownDeletePlugin, int* errorCode);

    // oznaci tmp-soubor prislusici 'name' za platny, poskytne ho ostatnim threadum,
    // vola se jedine pote, co metoda GetName() vrati 'exists' == FALSE
    //
    // vraci uspech
    //
    // name - jednoznacna identifikace polozky
    // size - pocet bytu zabranych tmp-souborem na disku
    BOOL NamePrepared(const char* name, const CQuadWord& size);

    // priradi systemovy objekt k ziskanemu tmp-souboru, pomoci 'lock' se ovlada
    // minimalni zivotnost tmp-souboru (zavisi na 'remove')
    // vola se jedine do paru k GetName()
    //
    // vraci uspech
    //
    // name - identifikace polozky (pozdejsi pro hledani)
    // lock - az bude tento objekt "signaled" bude mozne tmp-soubor "uvolnit" (zalezi na 'remove')
    // lockOwner - ma se cache postarat o volani CloseHandle(lock) ?
    // remove - kdy mazat tmp-soubor
    BOOL AssignName(const char* name, HANDLE lock, BOOL lockOwner, CCacheRemoveType remove);

    // vola se pouze v pripade, ze po volani GetName() nelze zavolat NamePrepared() nebo AssignName(),
    // je zde pro pripad chyby pri ziskavani tmp-souboru nebo objektu 'lock' (spousteni
    // aplikace pro ktery se ziskaval tmp-soubor)
    // v pripade, ze bylo potreba volat NamePrepared() da moznost vytvoreni tmp-souboru ostatnim
    // threadum (ktere cekaji az bude tmp-soubor pripraven), v pripade, ze bylo potreba volat
    // AssignName() zrusi "cekani" tmp-souboru na prirazeni objektu 'lock', muze dojit ke
    // zruseni tmp-souboru; je-li 'storeInCache' TRUE, tmp-soubor je pripraven a neni zamceny,
    // oznaci se tmp-soubor za cachovany (pokud to dovoli max. kapacita cache, nebude zrusen)
    //
    // vraci uspech
    //
    // name - identifikace polozky (pozdejsi pro hledani)
    BOOL ReleaseName(const char* name, BOOL storeInCache);

    // ceka na okamzik, kdy bude mit cache-handles "volno", hodi se pri hromadnem ruseni
    // souboru pomoci jednoho lock objektu (pocka, az budou vsechny zavisle soubory
    // smazane)
    void WaitForIdle()
    {
        HANDLES(EnterCriticalSection(&WaitForIdleCS));
        Handles.WaitForIdle();
        HANDLES(LeaveCriticalSection(&WaitForIdleCS));
    }

    // pokud si rozmyslime smazani souboru na disku (napr. nepodaril se zapakovat, nechame
    // ho tedy radsi v tempu, aby nas useri nezabili), vraci uspech
    BOOL DetachTmpFile(const char* tmpName);

    // odstrani vsechny cachovane soubory, ktere zacinaji na "name" (napr. vsechny
    // soubory z jednoho archivu)
    void FlushCache(const char* name);

    // odstrani cachovany soubor 'name'; vraci TRUE pokud byl soubor nalezen a odstranen
    BOOL FlushOneFile(const char* name);

    // spocita kolik disk-cache obsahuje tmp-souboru mazanych pluginem 'ownDeletePlugin'
    int CountNamesDeletedByPlugin(CPluginInterfaceAbstract* ownDeletePlugin);

    // provede predcasne smazani vsech tmp-souboru, ktere se mazou pomoci pluginu
    // 'ownDeletePlugin'; pouziva se pri unloadu pluginu (u tmp-souboru se nastavi,
    // ze uz byly smazany - az se na ne zavrou vsechny odkazy, k mazani uz nedojde);
    // je-li 'onlyDetach' TRUE, nedojde ke smazani, dojde jen k nastaveni, ze uz byl
    // smazany (odpojeni pluginu od tmp-souboru)
    void PrematureDeleteByPlugin(CPluginInterfaceAbstract* ownDeletePlugin, BOOL onlyDetach);

    // cisteni adresare TEMP od zbytku predchozich instanci; spousti jen prvni instance;
    // najde-li podadresare "SAL*.tmp", zepta se usera na vymaz a pripadne ho provede
    void ClearTEMPIfNeeded(HWND parent, HWND hActivePanel);

protected:
    void Enter() { HANDLES(EnterCriticalSection(&Monitor)); } // vola se po vstupu do metod
    void Leave() { HANDLES(LeaveCriticalSection(&Monitor)); } // vola se pred opustenim metod

    // zkontroluje podminky na disku, pripadne uvolni nektere volne cachovane tmp-soubory
    void CheckCachedFiles();

    // reaguje na prechod nektereho z 'lock' do "signaled" stavu (tmp-souboru ubyva link),
    //
    // lock - handle sledovaneho objektu, ktery presel do "signaled" stavu
    // owner - objekt obsahujici tento 'lock'
    void WaitSatisfied(HANDLE lock, CCacheData* owner);

    friend class CCacheData;    // vola Enter() a Leave()
    friend class CCacheHandles; // vola WaitSatisfied()
};

//****************************************************************************
//
// CDeleteManager
//

struct CPluginData;

struct CDeleteManagerItem
{
    char* FileName;                   // jmeno souboru, ktery se ma smazat pres plugin
    CPluginInterfaceAbstract* Plugin; // plugin, ktery bude mazat soubor pres CPluginInterfaceForArchiverAbstract::DeleteTmpCopy

    CDeleteManagerItem(const char* fileName, CPluginInterfaceAbstract* plugin);
    ~CDeleteManagerItem();
    BOOL IsGood() { return FileName != NULL; }
};

class CDeleteManager
{
protected:
    CRITICAL_SECTION CS; // sekce pouzita pro synchronizaci dat objektu

    // data o souborech, ktere se maji smazat (v hl. threadu volanim metody
    // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy pluginu)
    TIndirectArray<CDeleteManagerItem> Data;
    BOOL WaitingForProcessing; // TRUE = message hl. oknu je na ceste nebo prave probiha zpracovani dat (pridana polozka se ihned zpracuje)
    BOOL BlockDataProcessing;  // TRUE = nemaji se procesit data (ProcessData() nic nedela)

public:
    CDeleteManager();
    ~CDeleteManager();

    // prida soubor pro smazani a zajisti co nejblizsi vyvolani metody pluginu
    // CPluginInterfaceForArchiverAbstract::DeleteTmpCopy pro vymaz souboru;
    // pri chybe nedojde k smazani souboru - plugin by mel pri unloadu/loadu
    // promazavat svuj adresar (TEMP bude promazavat Salamander)
    // mozne volat z libovolneho threadu
    void AddFile(const char* fileName, CPluginInterfaceAbstract* plugin);

    // vola se v hl. threadu (vola po prijmu zpravy WM_USER_PROCESSDELETEMAN hl. okno);
    // zpracovani novych dat - mazani souboru v pluginu
    void ProcessData();

    // hrozi unload pluginu 'plugin': nechame unloadovany plugin zrusit tmp-kopie z disku
    // (viz CPluginInterfaceForArchiverAbstract::PrematureDeleteTmpCopy)
    void PluginMayBeUnloaded(HWND parent, CPluginData* plugin);

    // probehl unload pluginu 'plugin', odpojime plugin od delete-manageru i disk-cache;
    // 'unloadedPlugin' je jiz neplatne rozhrani pluginu (zaloha pred unloadem)
    void PluginWasUnloaded(CPluginData* plugin, CPluginInterfaceAbstract* unloadedPlugin);
};

extern CDiskCache DiskCache;         // globalni disk-cache
extern CDeleteManager DeleteManager; // globalni disk-cache delete-manager (mazani tmp-souboru v pluginech archivatoru)
