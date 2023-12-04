// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define NUM_OF_CHECKTHREADS 30                   // max. pocet threadu pro "neblokujici" testy pristupnosti cest
#define ICONOVR_REFRESH_PERIOD 2000              // minimalni odstup refreshu icon-overlays v panelu (viz IconOverlaysChangedOnPath)
#define MIN_DELAY_BETWEENINACTIVEREFRESHES 2000  // minimalni odstup refreshu pri neaktivnim hl. okne
#define MAX_DELAY_BETWEENINACTIVEREFRESHES 10000 // maximalni odstup refreshu pri neaktivnim hl. okne

enum CActionType
{
    atCopy,
    atMove,
    atDelete,
    atCountSize,
    atChangeAttrs,
    atChangeCase,
    atRecursiveConvert, // convert including subdirectories
    atConvert           // convert not including subdirectories
};

enum CPluginFSActionType
{
    fsatCopy,
    fsatMove,
    fsatDelete,
    fsatCountSize,
    fsatChangeAttrs
};

enum CPanelType
{
    ptDisk,       // aktualni cesta je "c:\path" nebo UNC
    ptZIPArchive, // aktualni cesta je do archivu (obsluhuje bud plug-in nebo kod pro podporu externich archivatoru)
    ptPluginFS,   // aktualni cesta je na plug-inovy file-system (obsluhuje plug-in)
};

struct CAttrsData // data pro atChangeAttrs
{
    DWORD AttrAnd, AttrOr;
    BOOL SubDirs;
    BOOL ChangeCompression;
    BOOL ChangeEncryption;
};

struct CChangeCaseData // data pro atChangeCase
{
    int FileNameFormat; // cisla kompatibilni s funkci AlterFileName
    int Change;         // jaka cast jmena se ma menit  --||--
    BOOL SubDirs;       // vcetne podadresaru?
};

class CCopyMoveData;

struct CTmpDropData
{
    BOOL Copy;
    char TargetPath[MAX_PATH];
    CCopyMoveData* Data;
};

struct CTmpDragDropOperData
{
    BOOL Copy;      // copy/move
    BOOL ToArchive; // archive/FS
    char ArchiveOrFSName[MAX_PATH];
    char ArchivePathOrUserPart[MAX_PATH];
    CDragDropOperData* Data;
};

class CCriteriaData // data pro atCopy/atMove
{
public:
    BOOL OverwriteOlder;      // prepis starsich, skip novejsich
    BOOL StartOnIdle;         // ma se spustit az nic jineho nepobezi
    BOOL CopySecurity;        // zachova NTFS prava, FALSE = don't care = nic se nema extra resit, na vysledku nam nezalezi
    BOOL CopyAttrs;           // zachova Archive, Encrypt a Compress atributy, FALSE = don't care = nic se nema extra resit, na vysledku nam nezalezi
    BOOL PreserveDirTime;     // zachova datum+cas adresaru
    BOOL IgnoreADS;           // ignorujeme ADS (nehledame je ve zdroji kopirovani) - orez ADS + zrychleni na pomalych sitich (hlavne VPN)
    BOOL SkipEmptyDirs;       // prazdne adresare (nebo obsahujici pouze adresare) preskocime
    BOOL UseMasks;            // pokud je TRUE, je platna promenna 'Masks'; jinak zadne filtrovani
    CMaskGroup Masks;         // ktere soubory mame zpracovat (Masks museji byt prepared)
    BOOL UseAdvanced;         // pokud je TRUE, je platne promenna 'Advanced'; jinak zadne filtrovani
    CFilterCriteria Advanced; // advanced options
    BOOL UseSpeedLimit;       // TRUE = pouziva se speed-limit
    DWORD SpeedLimit;         // speed-limit v bytech za vterinu

public:
    CCriteriaData();

    void Reset();

    CCriteriaData& operator=(const CCriteriaData& s);

    // vraci TRUE, pokud je nektere kriterium nastaveno
    BOOL IsDirty();

    // pokud soubor 'file' odpovida kriteriim UseMasks/Masks a UseAdvanced/Advanced
    // vrati TRUE; pokud jim nevyhovuje, vrati FALSE
    // POZOR: masky museji byt pripravene
    // POZOR: advanced musi byt take pripraveno
    BOOL AgreeMasksAndAdvanced(const CFileData* file);
    BOOL AgreeMasksAndAdvanced(const WIN32_FIND_DATA* file);

    // save/load do/z Windows Registry
    // !!! POZOR: ukladani optimalizovano, ukladaji se pouze zmenene hodnoty; pred ulozenim do klice musi by tento klic napred promazan
    BOOL Save(HKEY hKey);
    BOOL Load(HKEY hKey);
};

// options pro Copy/Move dialog; zatim drzi/uklada pouze jednu polozku;
// pokud existuje, pouzije se jako default pro nove otevrene Copy/Move dialogy
// casem nas uzivatele pravdepodobne dotlaci k rozsireni na vice Options, jako ma Find
class CCopyMoveOptions
{
protected:
    TIndirectArray<CCriteriaData> Items;

public:
    CCopyMoveOptions() : Items(1, 1) {}

    void Set(const CCriteriaData* item); // ulozi polozku (pokud jiz v poli polozka je, bude prepsana); pokud je 'item' NULL, zahodi existujici polozku a pole zustane prazdne
    const CCriteriaData* Get();          // pokud drzi polozku, vrati na ni ukazatel, jinak vrati NULL

    BOOL Save(HKEY hKey);
    BOOL Load(HKEY hKey);
};

extern CCopyMoveOptions CopyMoveOptions; // globalni promenna drzici (default) options pro Copy/Move dialog

// flagy pro prekreslovani list boxu
#define DRAWFLAG_ICON_ONLY 0x00000001      // nakresli pouze cast s ikonou
#define DRAWFLAG_DIRTY_ONLY 0x00000002     // vyresli pouze polozky, ktere maji nastaven \
                                           // bit Dirty; nebudou se kreslit zadne plochy \
                                           // kolem, ani casti textu, ktere nemuzou byt \
                                           // ovlivneny selectionou
#define DRAWFLAG_KEEP_DIRTY 0x00000004     // po vykresleni nebude shozen bit Dirty
#define DRAWFLAG_SKIP_VISTEST 0x00000008   // nebude se provadet test viditelnosti jednotlivych casti polozky
#define DRAWFLAG_SELFOC_CHANGE 0x00000010  // staci vykreslit zmeny tykajici se focusu a selectu \
                                           // tento flag se pouziva k optimalizaci v pripade, \
                                           // ze FullRowSelect==FALSE => staci nakreslit sloupec Name
#define DRAWFLAG_MASK 0x00000020           // vykresli masku ikonky
#define DRAWFLAG_NO_STATE 0x00000040       // nepouzije state a vykresli ikonu v zakladnich barvach
#define DRAWFLAG_NO_FRAME 0x00000080       // potlaci zobrazovani ramecku kolem textu
#define DRAWFLAG_IGNORE_CLIPBOX 0x00000100 // paint se provede pro vsechny zobrazene polozky, bude \
                                           // se ignorovat clip box (GetClipBox) \
                                           // slouzi po prekresleni panelu v pripade, ze nad nim je zobrazen dialog \
                                           // s ulozenym starym pozadim; po jeho zavreni musi dojit k prekresleni
#define DRAWFLAG_DRAGDROP 0x00000200       // barvy pro drag&drop image
#define DRAWFLAG_SKIP_FRAME 0x00000400     // dirty hack pro thumbnails: nebude se malovat ramecek kolem thmb, abychom si nenakopli alfa kanal
#define DRAWFLAG_OVERLAY_ONLY 0x00000800   // nakresli pouze overlay (nekresli ikonu), slouzi pro kresleni overlay ikony na thumbnail

class CMainWindow;
class COperations;
class CFilesBox;
class CHeaderLine;
class CStatusWindow;
class CFilesArray;
struct CFileData;
class CIconCache;
class CSalamanderDirectory;
struct IContextMenu2;
class CPathHistory;
class CFilesWindow;
class CMenuNew;
class CMenuPopup;

// funkce pro uvolneni listingu panelu;
// zrusi oldPluginData a s nim i vsechny data pluginu (CFileData::PluginData v
// oldPluginFSDir nebo oldArchiveDir), oldFiles, oldDirs
// pokud je dealloc FALSE nerusi oldFiles, oldDirs, oldPluginFSDir, ani oldArchiveDir,
// vola jen Clear() a DestroyMembers(), v opacnem pripade na tyto objekty vola delete
void ReleaseListingBody(CPanelType oldPanelType, CSalamanderDirectory*& oldArchiveDir,
                        CSalamanderDirectory*& oldPluginFSDir,
                        CPluginDataInterfaceEncapsulation& oldPluginData,
                        CFilesArray*& oldFiles, CFilesArray*& oldDirs, BOOL dealloc);

//****************************************************************************
//
// CFilesMap
//
// Pri vyberu pomoci klece drzi umisteni adresaru a souboru v panelu.
//

struct CFilesMapItem
{
    WORD Width;
    CFileData* FileData;
    unsigned Selected : 1; // vybrany soubor ve fileboxu
};

class CFilesMap
{
protected:
    CFilesWindow* Panel; // nas panel

    int Rows; // pocet radku a sloupcu obsazenych v mape
    int Columns;
    CFilesMapItem* Map; // pole [Columns, Rows]
    int MapItemsCount;  // pocet polozek v mape

    int AnchorX; // vychozi bod, ze ktereho se tahne klec
    int AnchorY;
    int PointX; // druhy bod urcujici klec
    int PointY;

public:
    CFilesMap();
    ~CFilesMap();

    void SetPanel(CFilesWindow* panel) { Panel = panel; }

    BOOL CreateMap();  // naalokuje Mapu a naleje do ni delky adresaru a souboru
    void DestroyMap(); // uvolnim prostredky a nastavim promenne

    void SetAnchor(int x, int y); // nastavi bod, kde user zacal tahnout klec
    void SetPoint(int x, int y);  // nastavi druhy bod urcujici klec

    void DrawDragBox(POINT p); // vykresli klec z Anchor -> p

protected:
    CFilesMapItem* GetMapItem(int column, int row);
    BOOL PointInRect(int col, int row, RECT r);

    void UpdatePanel(); // naleje do panelu selection z Map

    // naplni RECT r; hodnoty jsou v indexech do pole Map
    void GetCROfRect(int newX, int newY, RECT& r);

    // vypocita column a row, ve kterych lezi bod [x, y]
    void GetCROfPoint(int x, int y, int& column, int& row);
};

inline BOOL CFilesMap::PointInRect(int col, int row, RECT r)
{
    return (col >= r.left && col <= r.right && row >= r.top && row <= r.bottom);
}

//****************************************************************************
//
// CScrollPanel
//
// Zapouzdruje metody pro posouvani obsahu panelu pri tazeni nebo drag & dropu,
// kdyz kurzor prekroci hranici panelu.
//

class CScrollPanel
{
protected:
    CFilesWindow* Panel; // nas panel
    BOOL ExistTimer;
    BOOL ScrollInside;      // rolujeme na vnitrnich hranach panelu
    POINT LastMousePoint;   // pokud je nahozen ScrollInside, zajisti presnejsi identifikaci pozadavku o rolovani
    int BeginScrollCounter; // druha forma jistice pred nechtenym rolovanim

public:
    CScrollPanel();
    ~CScrollPanel();

    void SetPanel(CFilesWindow* panel) { Panel = panel; }

    BOOL BeginScroll(BOOL scrollInside = FALSE); // tuto metodu zavolame pred zacatkem tazeni
    void EndScroll();                            // uklid

    void OnWMTimer(); // tato metoda je volana z panelu
};

//
// *****************************************************************************
// CFileTimeStamps
//

struct CFileTimeStampsItem
{
    char *ZIPRoot,
        *SourcePath,
        *FileName,
        *DosFileName;
    FILETIME LastWrite;
    CQuadWord FileSize;
    DWORD Attr;

    CFileTimeStampsItem();
    ~CFileTimeStampsItem();

    BOOL Set(const char* zipRoot, const char* sourcePath, const char* fileName,
             const char* dosFileName, const FILETIME& lastWrite, const CQuadWord& fileSize,
             DWORD attr);
};

class CFilesWindow;

class CFileTimeStamps
{
protected:
    char ZIPFile[MAX_PATH];                   // jmeno archivu, ve kterem jsou ulozeny vsechny sledovane soubory
    TIndirectArray<CFileTimeStampsItem> List; // seznam souboru a pro jejich update potrebnych dat
    CFilesWindow* Panel;                      // panel, pro ktery pracujeme

public:
    CFileTimeStamps() : List(10, 5)
    {
        ZIPFile[0] = 0;
        Panel = NULL;
    }
    ~CFileTimeStamps()
    {
        if (ZIPFile[0] != 0 ||
            List.Count > 0)
        {
            TRACE_E("Invalid work with CFileTimeStamps.");
        }
    }

    const char* GetZIPFile() { return ZIPFile; }

    void SetPanel(CFilesWindow* panel) { Panel = panel; }

    // pridava soubor + jeho znamku do seznamu, zaroven jsou zde vedeny informace potrebne
    // pro update souboru v archivu
    //
    // zipFile - plne jmeno archivu
    // zipRoot - cesta v archivu k danemu souboru
    // sourcePath - cesta k souboru na disku
    // fileName - jmeno souboru (jak v archivu, tak na disku)
    // dosFileName - DOS jmeno souboru (jak v archivu, tak na disku)
    // lastWrite - datum posledniho zapisu (po vypakovani z archivu - pro kontrolu zmeny)
    // fileSize - velikost souboru (po vypakovani z archivu - pro kontrolu zmeny)
    // attr - atributy souboru
    //
    // navratova hodnota TRUE - soubor byl pridan, FALSE - nebyl pridan (chyba nebo uz zde je)
    BOOL AddFile(const char* zipFile, const char* zipRoot, const char* sourcePath,
                 const char* fileName, const char* dosFileName,
                 const FILETIME& lastWrite, const CQuadWord& fileSize, DWORD attr);

    // overi casove znamky a pripadne provede update a pripravi objekt pro dalsi pouziti
    void CheckAndPackAndClear(HWND parent, BOOL* someFilesChanged = NULL, BOOL* archMaybeUpdated = NULL);

    // naplni listbox jmeny vsech obsazenych souboru
    void AddFilesToListBox(HWND list);

    // odstrani soubory ze vsech zadanych indexu, 'indexes' je pole indexu, 'count' je jejich pocet
    void Remove(int* indexes, int count);

    // umozni zkopirovat soubory ze vsech zadanych indexu, 'indexes' je pole indexu, 'count' je
    // jejich pocet; 'parent' je parent dialogu; 'initPath' je nabizena cilova cesta
    void CopyFilesTo(HWND parent, int* indexes, int count, const char* initPath);
};

//****************************************************************************
//
// CTopIndexMem
//

#define TOP_INDEX_MEM_SIZE 50 // pocet pamatovanych top-indexu (urovni), minimalne 1

class CTopIndexMem
{
protected:
    // cesta pro posledni zapamatovany top-index; nejdelsi je archive + archive-path, takze 2 * MAX_PATH
    char Path[2 * MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // zapamatovane top-indexy
    int TopIndexesCount;                // pocet zapamatovanych top-indexu

public:
    CTopIndexMem() { Clear(); }
    void Clear()
    {
        Path[0] = 0;
        TopIndexesCount = 0;
    }                                                 // vycisti pamet
    void Push(const char* path, int topIndex);        // uklada top-index pro danou cestu
    BOOL FindAndPop(const char* path, int& topIndex); // hleda top-index pro danou cestu, FALSE->nenalezeno
};

//******************************************************************************
//
// CDirectorySizes
//

class CDirectorySizes
{
protected:
    char* Path;                // plna cesta k adresari, pro ktery drzime jmena a velikosti podadresaru
    TDirectArray<char*> Names; // jmena podadresaru
    BOOL CaseSensitive;
    BOOL NeedSort; // hlidac spravneho pouzivani tridy

public:
    CDirectorySizes(const char* path, BOOL caseSensitive);
    ~CDirectorySizes();

    // destrukce vesech drzenych dat
    void Clean();

    BOOL IsGood() { return Path != NULL; }

    BOOL Add(const char* name, const CQuadWord* size);

    // pokud nalezne jmeno 'name' v poli Name, vraci ukazatel na jeho velikost
    // pokud nenalezne jmeno 'name', vraci NULL
    const CQuadWord* GetSize(const char* name);

    void Sort();

protected:
    int GetIndex(const char* name);

    friend class CDirectorySizesHolder;
};

//******************************************************************************
//
// CDirectorySizesHolder
//

#define DIRECOTRY_SIZES_COUNT 20

class CDirectorySizesHolder
{
protected:
    CDirectorySizes* Items[DIRECOTRY_SIZES_COUNT];
    int ItemsCount; // pocet platnych polozek v poli Items

public:
    CDirectorySizesHolder();
    ~CDirectorySizesHolder();

    // destrukce vsech drzenych dat mimo Path
    void Clean();

    BOOL Store(CFilesWindow* panel);

    void Restore(CFilesWindow* panel);

    // vraci NULL, pokud neni nalezena zadne polozka se stejnou cestou
    CDirectorySizes* Get(const char* path);

protected:
    // vraci index polozky, jejiz Path je stejna jako 'path'
    // pokud takovou polozku nenajde, vraci -1
    int GetIndex(const char* path);

    CDirectorySizes* Add(const char* path);
};

//****************************************************************************
//
// CQuickRenameWindow
//

class CQuickRenameWindow : public CWindow
{
protected:
    CFilesWindow* FilesWindow;
    BOOL CloseEnabled;
    BOOL SkipNextCharacter;

public:
    CQuickRenameWindow();

    void SetPanel(CFilesWindow* filesWindow);
    void SetCloseEnabled(BOOL closeEnabled);
    BOOL GetCloseEnabled();

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CFilesWindowAncestor
//

class CFilesWindowAncestor : public CWindow // prave objektove jadro - vsechno private ;-)
{
private:
    char Path[MAX_PATH];      // cesta pro panel typu ptDisk - normal ("c:\path") nebo UNC ("\\server\share\path")
    BOOL SuppressAutoRefresh; // TRUE = user dal cancel behem cteni listingu adresare a vybral docasne potlaceni autorefreshe

    CPanelType PanelType; // typ panelu (disk, archiv, plugin FS)

    BOOL MonitorChanges; // maji se monitorovat zmeny (autorefresh) ?
    UINT DriveType;      // disk+archiv: typ cesty Path (viz MyGetDriveType())

    // pokud jsme v archivu:
    CSalamanderDirectory* ArchiveDir; // obsah otevreneho archivu; zakladni data - pole CFileData
    char ZIPArchive[MAX_PATH];        // cesta k otevrenemu archivu
    char ZIPPath[MAX_PATH];           // cesta uvnitr otevreneho archivu
    FILETIME ZIPArchiveDate;          // datum archivu (pouzito pro datum ".." a pri refreshi)
    CQuadWord ZIPArchiveSize;         // velikost archivu - pouziva se pro test zmeny archivu

    // pokud jsme v plugin FS:
    CPluginFSInterfaceEncapsulation PluginFS; // ukazatel na otevreny FS
    CSalamanderDirectory* PluginFSDir;        // obsah otevreneho FS; zakladni data - pole CFileData
    int PluginIconsType;                      // typ ikon v panelu pri listovani FS

    // pokud jsme v archivu listovanem pluginem nebo v plugin FS
    CPluginInterfaceAbstract* PluginIface; // pouzivat vyhradne pro hledani pluginu v Plugins (neni tu pro volani metod, atd.)
    int PluginIfaceLastIndex;              // index PluginIface v Plugins pri poslednim hledani, pouzivat vyhradne pro hledani pluginu v Plugins

public:
    // obsahy vsech sloupcu zobrazovanych v panelu (jak zakladni data, tak data plug-inu pro archivy a FS)
    CFilesArray* Files; // filtrovany seznam souboru (melka kopie; zakladni data - struktura CFileData)
    CFilesArray* Dirs;  // filtrovany seznam adresaru (melka kopie; zakladni data - struktura CFileData)
    // interface pro ziskavani specifickych dat plug-inu; data pro sloupce plug-inu; definuje jak
    // pouzivat CFileData::PluginData; u plug-inu FS s ikonami typu pitFromPlugin slouzi i pro ziskani
    // ikony "na pozadi" v threadu icon-readra - pred zmenou je tedy nutne volat SleepIconCacheThread()
    CPluginDataInterfaceEncapsulation PluginData;

    CIconList* SimplePluginIcons; // jen FS + pitFromPlugin: image-list s jednoduchymi ikonami plug-inu

    // aktualni pocet vybranych polozek; musi byt nastavovan vsude tam, kde dochazi k zapisu do
    // promenne CFileData::Selected
    int SelectedCount;

    // pomocne promenne pouzite pro hladsi prubeh refreshe (bez "v panelu nejsou zadne polozky")
    // pro pluginove file-systemy:
    // TRUE = listing v panelu se nema standardne uvolnit, ale pouze odpojit (dale se budou
    // pouzivat objekty z NewFSXXX)
    BOOL OnlyDetachFSListing;
    CFilesArray* NewFSFiles;                // novy prazdny objekt pro Files
    CFilesArray* NewFSDirs;                 // novy prazdny objekt pro Dirs
    CSalamanderDirectory* NewFSPluginFSDir; // novy prazdny objekt pro PluginFSDir
    CIconCache* NewFSIconCache;             // neni-li NULL, novy prazdny objekt pro IconCache (neni zde, je v potomkovi)

public:
    CFilesWindowAncestor();
    ~CFilesWindowAncestor();

    // NULL -> Path; echo && err != ERROR_SUCCESS -> jen vypis chybu
    // 'parent' je parent messageboxu (NULL == HWindow)
    DWORD CheckPath(BOOL echo, const char* path = NULL, DWORD err = ERROR_SUCCESS,
                    BOOL postRefresh = TRUE, HWND parent = NULL);

    // zrusi PluginData a s nim i vsechny data pluginu (CFileData::PluginData v
    // PluginFSDir nebo ArchiveDir), Files, Dirs a vynuluje SelectedCount
    // 1. POZOR: nerusi Files, Dirs, PluginFSDir, ani ArchiveDir, vola jen Clear() a DestroyMembers()
    // 2. POZOR: pokud jde o FS s ikonami typu pitFromPlugin, je nutne pred zrusenim
    //           PluginFSDir a PluginData volat SleepIconCacheThread() - ikony se ziskavaji pomoci
    //           PluginFSDir a PluginData ...
    void ReleaseListing();

    // vraci cestu v panelu (vsechny typy panelu - disk, archiv i FS);
    // je-li convertFSPathToExternal TRUE a v panelu je FS cesta, zavola se
    // CPluginInterfaceForFSAbstract::ConvertPathToExternal();
    // vraci TRUE pokud se cesta komplet vejde do bufferu (jinak bude vracena textove oriznuta cesta)
    BOOL GetGeneralPath(char* buf, int bufSize, BOOL convertFSPathToExternal = FALSE);

    const char* GetPath() { return Path; }
    BOOL Is(CPanelType type) { return type == PanelType; }
    CPanelType GetPanelType() { return PanelType; }
    BOOL GetMonitorChanges() { return MonitorChanges; }
    BOOL GetNetworkDrive() { return DriveType == DRIVE_REMOTE; }
    UINT GetPathDriveType() { return DriveType; }
    CSalamanderDirectory* GetArchiveDir() { return ArchiveDir; }
    const char* GetZIPArchive() { return ZIPArchive; }
    const char* GetZIPPath() { return ZIPPath; }
    FILETIME GetZIPArchiveDate() { return ZIPArchiveDate; }
    BOOL IsSameZIPArchiveSize(const CQuadWord& size) { return ZIPArchiveSize == size; }
    CQuadWord GetZIPArchiveSize() { return ZIPArchiveSize; }
    BOOL GetSuppressAutoRefresh() { return SuppressAutoRefresh; }

    void SetPath(const char* path);
    void SetMonitorChanges(BOOL monitorChanges) { MonitorChanges = monitorChanges; }
    void SetPanelType(CPanelType type) { PanelType = type; }
    void SetZIPPath(const char* path);
    void SetZIPArchive(const char* archive);
    void SetArchiveDir(CSalamanderDirectory* dir) { ArchiveDir = dir; }
    void SetZIPArchiveDate(FILETIME& time) { ZIPArchiveDate = time; }
    void SetZIPArchiveSize(const CQuadWord& size) { ZIPArchiveSize = size; }
    void SetSuppressAutoRefresh(BOOL suppress) { SuppressAutoRefresh = suppress; }

    // pokud je parametr 'zipPath'==NULL, pouzije se cesta ZIPPath
    CFilesArray* GetArchiveDirFiles(const char* zipPath = NULL);
    CFilesArray* GetArchiveDirDirs(const char* zipPath = NULL);

    // porovnava Path tohoto objektu a objektu 'other', kvuli change-notify problemum (viz snooper)
    BOOL SamePath(CFilesWindowAncestor* other);

    // zjisti, jestli je mozne cestu 'fsName':'fsUserPart' otevrit v aktivnim FS, aneb jestli
    // je nutne zavrit FS nebo staci jen zmenit "adresar" v ramci otevreneho FS; je-li
    // 'convertPathToInternal' TRUE, dojde pred testem 'fsUserPart' (buffer aspon MAX_PATH znaku)
    // ke konverzi cesty na interni format a do 'convertPathToInternal' se zapise FALSE; pokud
    // vraci metoda TRUE, vraci i index 'fsNameIndex' jmena FS 'fsName' pluginu
    BOOL IsPathFromActiveFS(const char* fsName, char* fsUserPart, int& fsNameIndex,
                            BOOL& convertPathToInternal);

    CPluginFSInterfaceEncapsulation* GetPluginFS() { return &PluginFS; }
    CSalamanderDirectory* GetPluginFSDir() { return PluginFSDir; }
    int GetPluginIconsType() { return PluginIconsType; }

    CFilesArray* GetFSFiles();
    CFilesArray* GetFSDirs();

    void SetPluginFS(CPluginFSInterfaceAbstract* fsIface, const char* dllName,
                     const char* version, CPluginInterfaceForFSAbstract* ifaceForFS,
                     CPluginInterfaceAbstract* iface, const char* pluginFSName,
                     int pluginFSNameIndex, DWORD pluginFSCreateTime,
                     int chngDrvDuplicateItemIndex, int builtForVersion)
    {
        PluginFS.Init(fsIface, dllName, version, ifaceForFS, iface, pluginFSName,
                      pluginFSNameIndex, pluginFSCreateTime, chngDrvDuplicateItemIndex,
                      builtForVersion);
    }
    void SetPluginFSDir(CSalamanderDirectory* pluginFSDir) { PluginFSDir = pluginFSDir; }
    void SetPluginIconsType(int type) { PluginIconsType = type; }

    void SetPluginIface(CPluginInterfaceAbstract* pluginIface)
    {
        PluginIface = pluginIface;
        PluginIfaceLastIndex = -1;
    }

    // vraci CPluginData obsahujici iface 'PluginIface', pokud neexistuje vraci NULL
    // POZOR: ukazatel je platny jen do pristi zmeny poctu plug-inu (pole se roztahuje/smrstuje)
    CPluginData* GetPluginDataForPluginIface();
};

//****************************************************************************
//
// CFilesWindow
//

struct CViewerMasksItem;
struct CEditorMasksItem;
struct CHighlightMasksItem;
class CDrivesList;

// mode pro CFilesWindow::CopyFocusedNameToClipboard
enum CCopyFocusedNameModeEnum
{
    cfnmShort, // pouze kratky nazev (example.txt)
    cfnmFull,  // plna cesta (c:\example.txt)
    cfnmUNC    // plna UNC cesta (\\server\share\example.txt)
};

// pole pro priorizaci nacitani ikon a thumbnailu icon-readerem podle aktualniho stavu
// panelu a zobrazenych polozek
class CVisibleItemsArray
{
protected:
    CRITICAL_SECTION Monitor; // sekce pouzita pro synchronizaci tohoto objektu (chovani - monitor)

    BOOL SurroundArr; // TRUE/FALSE = pole polozek v okoli viditelne casti panelu / pole polozek ve viditelne casti panelu

    int ArrVersionNum; // verze pole
    BOOL ArrIsValid;   // je pole naplnene a platne?

    char** ArrNames;       // alokovane pole jmen viditelnych v panelu (jmena jsou jen odkazy do Files+Dirs v panelu (CFileData::Name))
    int ArrNamesCount;     // pocet jmen v poli ArrNames
    int ArrNamesAllocated; // pocet alokovanych jmen v poli ArrNames

    int FirstVisibleItem; // index prvni viditelne polozky
    int LastVisibleItem;  // index posledni viditelne polozky

public:
    CVisibleItemsArray(BOOL surroundArr);
    ~CVisibleItemsArray();

    // vraci TRUE pokud je pole naplnene a platne (odpovida aktualnimu stavu
    // panelu a zobrazenych polozek), zaroven vraci i cislo verze pole ve 'versionNum'
    // (neni-li NULL), jinak vraci FALSE ('versionNum' vraci i v tomto pripade)
    // vola jak panel, tak icon-reader
    BOOL IsArrValid(int* versionNum);

    // hlasi zmenu v panelu nebo v zobrazenych polozkach; zrusi platnost pole
    // vola jen panel
    void InvalidateArr();

    // slouzi k obnove pole podle aktualniho stavu panelu a zobrazenych polozek;
    // zvysuje cislo verze pole + oznaci ho za platne
    // vola jen panel v idle rezimu
    void RefreshArr(CFilesWindow* panel);

    // pokud je pole naplnene a platne a obsahuje 'name', vraci TRUE; navic vraci
    // v 'isArrValid' TRUE pokud je pole naplnene a platne a ve 'versionNum'
    // cislo verze pole
    // vola jen icon-reader
    BOOL ArrContains(const char* name, BOOL* isArrValid, int* versionNum);

    // pokud je pole naplnene a platne a obsahuje index 'index', vraci TRUE; navic
    // vraci v 'isArrValid' TRUE pokud je pole naplnene a platne a ve 'versionNum'
    // cislo verze pole
    // vola jen icon-reader
    BOOL ArrContainsIndex(int index, BOOL* isArrValid, int* versionNum);
};

enum CTargetPathState // stavy cilove cesty pri stavbe operacniho skriptu
{
    tpsUnknown, // pouziva se jen pro pocatecni zjisteni stavu existujici cilove cesty
    tpsEncryptedExisting,
    tpsEncryptedNotExisting,
    tpsNotEncryptedExisting,
    tpsNotEncryptedNotExisting,
};

// pomocna funkce pro urcovani stavu cilove cesty za zaklade stavu nadrazeneho adresare a cilove cesty
CTargetPathState GetTargetPathState(CTargetPathState upperDirState, const char* targetPath);

#ifndef HDEVNOTIFY
typedef PVOID HDEVNOTIFY;
#endif // HDEVNOTIFY

enum CViewModeEnum;

class CFilesWindow : public CFilesWindowAncestor
{
public:
    CViewTemplate* ViewTemplate;            // ukazatel na sablonu, ktera urcuje rezim, nazev a viditelnost
                                            // standardnich Salamanderovskych sloupcu VIEW_SHOW_xxxx
    BOOL NarrowedNameColumn;                // TRUE = zaply smart-mode sloupce Name + bylo potreba sloupec Name zuzit
    DWORD FullWidthOfNameCol;               // jen pri zaplem smart-mode sloupce Name: omerena sirka sloupce Name (sirka pred zuzenim)
    DWORD WidthOfMostOfNames;               // jen pri zaplem smart-mode sloupce Name: sirka vetsiny jmen ze sloupce Name (napr. 85% jmen je kratsich nebo rovno teto hodnote)
    TDirectArray<CColumn> Columns;          // Sloupce zobrazene v panelu. Standardne Salamander plni
                                            // toto pole na zaklade sablony, na kterou ukazuje promenna
                                            // 'ViewTemplate'. Pokud je do panelu pripojen filesystem,
                                            // jsou tyto sloupce modifikovany filesystemem. Muzou pribyt
                                            // nove sloupce a muzou byt docasne odstraneny nektere
                                            // z viditelnych sloupcu.
    TDirectArray<CColumn> ColumnsTemplate;  // predloha promenne Columns
                                            // Predne se sestavi tato predloha na zaklade aktualni sablony
                                            // 'ViewTemplate' a typu obsahu panelu (disk / archiv+FS). Tato
                                            // predloha se pak kopiruje do pole Columns.
                                            // Zavedeno pro rychlost - abychom porad nesestavovali pole.
    BOOL ColumnsTemplateIsForDisk;          // TRUE = ColumnsTemplate bylo postaveno pro disk, jinak pro archiv nebo FS
    FGetPluginIconIndex GetPluginIconIndex; // callback pro ziskani indexu jednoduche ikony pro plug-in
                                            // s vlastnimi ikonami (FS, pitFromPlugin)

    CFilesMap FilesMap;        // pro oznacovani pomoci tazeni klece
    CScrollPanel ScrollObject; // pro automaticke rolovani pri praci s mysi

    CIconCache* IconCache;                // cache na ikonky primo ze souboru
    BOOL IconCacheValid;                  // je uz nacteno ?
    BOOL InactWinOptimizedReading;        // TRUE = nacitaji se jen ikony/thumbnaily/overlaye z viditelne casti panelu (pouziva se pokud je hl. okno neaktivni a provede se refresh - snazime se zatizit masinu co mozna nejmene, protoze jsme "na pozadi")
    DWORD WaitBeforeReadingIcons;         // kolik milisekund se ma cekat pred tim, nez icon-reader zacne nacitat ikony (pouziva se pri refreshi, dobra prasarna, behem cekani v icon readeru se v RefreshDirectory() do icon cache stihnou vnutit stare ikony jako nove, coz eliminuje jejich opakovane cteni a nekonecny cyklus refreshu na sitovych discich, viz 'probablyUselessRefresh')
    DWORD WaitOneTimeBeforeReadingIcons;  // kolik milisekund se ma cekat pred tim, nez icon-reader zacne nacitat ikony, po cekani se tento udaj vynuluje (pouziva se pro pochytani balicku zmen od Tortoise SVN, viz IconOverlaysChangedOnPath())
    DWORD EndOfIconReadingTime;           // GetTickCount() z okamziku, kdy doslo k docteni vsech ikon v panelu
    HANDLE IconCacheThread;               // handle threadu - nacitace ikon
    HANDLE ICEventTerminate;              // signaled -> konec threadu
    HANDLE ICEventWork;                   // signaled -> zacni nacitat ikony
    BOOL ICSleep;                         // TRUE -> opustit smycku nacitani ikon
    BOOL ICWorking;                       // TRUE -> uvnitr smycky nacitani ikon
    BOOL ICStopWork;                      // TRUE -> ani nezacinat smycku pro nacitani ikon
    CRITICAL_SECTION ICSleepSection;      // kriticka sekce -> sleep-icon-thread ji musi projit
    CRITICAL_SECTION ICSectionUsingIcon;  // kriticka sekce -> uvnitr se pouziva image-list
    CRITICAL_SECTION ICSectionUsingThumb; // kriticka sekce -> uvnitr se pouziva thumbnail

    BOOL AutomaticRefresh;      // refreshuje se panel automaticky? (nebo manualne)
    BOOL FilesActionInProgress; // uz se pripravuje nebo vykonava prace Workrovi ?

    CDrivesList* OpenedDrivesList; // pokud je otevrene Alt+F1(2) menu, ukazuje tato hodnota na nej; jinak je NULL

    int LastFocus; // kvuli eliminaci zbytecneho prepisu status l.

    CFilesBox* ListBox;
    CStatusWindow *StatusLine,
        *DirectoryLine;

    BOOL StatusLineVisible;
    BOOL DirectoryLineVisible;
    BOOL HeaderLineVisible;

    CMainWindow* Parent;

    //CPanelViewModeEnum ViewMode;      // thumbnails / brieft / detail vzhled panelu
    DWORD ValidFileData; // urcuje, ktere promenne v ramci struktury CFileData maji vyznam, viz konstanty VALID_DATA_XXX; nastavuje se pres SetValidFileData()

    CSortType SortType;       // podle ceho radime
    BOOL ReverseSort;         // obratit poradi
    BOOL SortedWithRegSet;    // slouzi k ohlidani zmeny globalni promenne Configuration.SortUsesLocale
    BOOL SortedWithDetectNum; // slouzi k ohlidani zmeny globalni promenne Configuration.SortDetectNumbers

    char DropPath[2 * MAX_PATH];  // buffer pro aktualni adresar pro drop operaci
    char NextFocusName[MAX_PATH]; // jmeno, ktere pri dalsim refreshi focusne
    BOOL DontClearNextFocusName;  // TRUE = pri aktivaci hl. okna Salamandera se nema vymazat NextFocusName
    BOOL FocusFirstNewItem;       // refresh: ma se vybrat polozka, ktera pribyla? (pro systemove New)
    CTopIndexMem TopIndexMem;     // pamet top-indexu pro Execute()

    int LastRefreshTime; // kvuli chaosu notifikaci o zmene v adresari

    BOOL CanDrawItems;         // je mozne prekreslovat polozky v listboxu?
    BOOL FileBasedCompression; // file-based komprese podporovana?
    BOOL FileBasedEncryption;  // file-based sifrovani podporovano?
    BOOL SupportACLS;          // jde o FS podporujici prava (NTFS)?

    HDEVNOTIFY DeviceNotification; // POZOR: pristup jen v krit. sekci snoopera: pro ptDisk + jen removable media: handle registrace okna panelu jako prijemce hlaseni o udalostech na zarizeni (pouziva se pro opusteni media pred jeho odpojenim od pocitace)

    // maji se na tomto disku ziskavat ikonky ze souboru?
    // pro ptDisk je TRUE, pokud se ikony ziskavaji ze souboru; pro ptZIPArchive je TRUE, pokud se
    // ikony ziskavaji z registry; pro ptPluginFS je TRUE, pokud se ikony ziskavaji z registry
    // (pitFromRegistry) nebo primo z plug-inu (pitFromPlugin)
    BOOL UseSystemIcons;
    BOOL UseThumbnails; // TRUE pokud se thumbnaily zobrazuji v panelu a nacitaji v icon-readeru

    int DontDrawIndex; // index polozky, ktera se nema v listboxu vubec prekreslovat
    int DrawOnlyIndex; // index polozky, ktera jedina se ma v listboxu prekreslovat

    IContextMenu2* ContextMenu;  // aktualni context menu (kvuli owner-draw menu)
    CMenuNew* ContextSubmenuNew; // aktualni context submenu New (kvuli owner-draw menu)

    BOOL NeedRefreshAfterEndOfSM;         // bude po ukonceni suspend modu potreba refresh?
    int RefreshAfterEndOfSMTime;          // "cas" nejnovejsiho refreshe, ktery prisel po zacatku suspend modu
    BOOL PluginFSNeedRefreshAfterEndOfSM; // bude po ukonceni suspend modu potreba refresh plug-in FS?

    BOOL SmEndNotifyTimerSet;  // TRUE pokud bezi timer pro poslani WM_USER_SM_END_NOTIFY_DELAYED
    BOOL RefreshDirExTimerSet; // TRUE pokud bezi timer pro poslani WM_USER_REFRESH_DIR_EX_DELAYED
    LPARAM RefreshDirExLParam; // lParam pro poslani WM_USER_REFRESH_DIR_EX_DELAYED

    BOOL InactiveRefreshTimerSet;   // TRUE pokud bezi timer pro poslani WM_USER_INACTREFRESH_DIR
    LPARAM InactRefreshLParam;      // lParam pro poslani WM_USER_INACTREFRESH_DIR
    DWORD LastInactiveRefreshStart; // udaje o poslednim refreshi vyvolanem snooperem v neaktivnim okne: kdy zacal + shoda viz o radek nize...
    DWORD LastInactiveRefreshEnd;   // udaje o poslednim refreshi vyvolanem snooperem v neaktivnim okne: kdy skoncil + shoda s LastInactiveRefreshStart znamena, ze zadny takovy refresh od posledni deaktivace jeste nebyl

    BOOL NeedRefreshAfterIconsReading; // bude po ukonceni nacitani ikonek potreba refresh?
    int RefreshAfterIconsReadingTime;  // "cas" nejnovejsiho refreshe, ktery prisel behem nacitani ikonek

    CPathHistory* PathHistory; // historie prochazeni pro tento panel

    DWORD HiddenDirsFilesReason; // bitove pole, urcuje z jakeho duvodu jsou soubory/adresare skryty (HIDDEN_REASON_xxx)
    int HiddenDirsCount,         // pocet skrytych adresaru v panelu (pocet skipnutych)
        HiddenFilesCount;        // pocet skrytych souboru v panelu (pocet skipnutych)

    HANDLE ExecuteAssocEvent;           // event, ktery "spusti" mazani souboru vypakovanych v ExecuteFromArchive
    BOOL AssocUsed;                     // bylo pouzito vypakovavani pres asociace ?
    CFileTimeStamps UnpackedAssocFiles; // seznam vypakovanych souboru s casovymi razitky (last-write)

    CMaskGroup Filter;  // filter pro panel
    BOOL FilterEnabled; // je filter pouzity

    BOOL QuickSearchMode;           // rezim Quick Search ?
    short CaretHeight;              // nastavuje se pri mereni fontu v CFilesWindow
    char QuickSearch[MAX_PATH];     // jmeno souboru hledaneho pres Quick Search
    char QuickSearchMask[MAX_PATH]; // maska quick-searche (muze obsahovat '/' za lib. pocet znaku)
    int SearchIndex;                // kde je kurzor pri Quick Search

    int FocusedIndex;  // aktualni pozice caretu
    BOOL FocusVisible; // je focus zobrazeny?

    int DropTargetIndex;          // aktualni pozice drop targetu
    int SingleClickIndex;         // aktualni pozice kurzoru v rezimu SingleClick
    int SingleClickAnchorIndex;   // pozice kurzoru v rezimu SingleClick, kde user stisknul leve tlacitko
    POINT OldSingleClickMousePos; // stara pozice kurzoru

    POINT LButtonDown;     // pouzito pri "trhani" souboru a adr. pro tazeni
    DWORD LButtonDownTime; // pouzito pri "casovem trhani" (min cas mezi down a up)

    BOOL TrackingSingleClick; // "zvyraznujeme" polozku pod kurzorem?
    BOOL DragBoxVisible;      // selecion box je viditelny
    BOOL DragBox;             // tahneme selection box
    BOOL DragBoxLeft;         // 1 = left  0 = right button on mouse
    BOOL ScrollingWindow;     // je nastaveno na TRUE, pokud je rolovani okna vyvolano nama (osetreno zahsinani boxu)

    POINT OldBoxPoint; // selection box

    BOOL SkipCharacter; // preskakuje prelozene akceleratory
    BOOL SkipSysCharacter;

    //j.r.: ShiftSelect neni vubec potreba
    //    BOOL       ShiftSelect;           // oznacujeme pomoci klavesnice (SelectItems je stav)
    BOOL DragSelect;           // rezim oznacovani / odznacovani tazenim mysi
    BOOL BeginDragDrop;        // "trhame" soubor ?
    BOOL DragDropLeftMouseBtn; // TRUE = drag&drop levym tlacitkem mysi, FALSE = pravym
    BOOL BeginBoxSelect;       // "otevirame" selection box?
    BOOL PersistentTracking;   // pri WM_CAPTURECHANGED nedojde k vypnuti track modu
    BOOL SelectItems;          // pri Drag Selectu oznacovat ?
    BOOL FocusedSinceClick;    // polozka uz mela focus, kdyz jsme na ni klikli

    BOOL CutToClipChanged; // je nastaveny alespon jeden CutToClip flag u souboru/adresaru?

    BOOL PerformingDragDrop; // prave probiha drag&drop operace

    BOOL UserWorkedOnThisPath; // TRUE jen pokud na aktualni ceste user provedl nejakou cinnost (F3, F4, F5, ...)
    BOOL StopThumbnailLoading; // je-li TRUE, data o "thumbnail-loaderech" v icon-cache nelze pouzivat (probiha unload/remove pluginu)

    int EnumFileNamesSourceUID; // UID zdroje pro enumeraci jmen ve viewerech

    CNames OldSelection; // selection pred operaci, urcena pro Reselect command
    CNames HiddenNames;  // nazvy souboru a adresaru, na ktere uzivatel zavolal Hide funkci (nesouvisi s Hidden atributem)

    CQuickRenameWindow QuickRenameWindow;
    UINT_PTR QuickRenameTimer;
    int QuickRenameIndex;
    RECT QuickRenameRect;

    CVisibleItemsArray VisibleItemsArray;         // pole viditelnych polozek v panelu
    CVisibleItemsArray VisibleItemsArraySurround; // pole polozek sousedicich s viditelnou casti panelu

    BOOL TemporarilySimpleIcons; // az do pristiho ReadDirectory() pouzivat simple icons

    int NumberOfItemsInCurDir; // jen pro ptDisk: pocet polozek vracenych FindFirstFile+FindNextFile pro aktualni cestu (pouziva se pro detekci zmen na sitovych a nemonitorovanych cestach pro Drop do panelu, ktery provadi Explorer)

    BOOL NeedIconOvrRefreshAfterIconsReading; // bude po ukonceni nacitani ikonek potreba refresh icon-overlays?
    DWORD LastIconOvrRefreshTime;             // GetTickCount() posledniho refreshe icon-overlays (viz IconOverlaysChangedOnPath())
    BOOL IconOvrRefreshTimerSet;              // TRUE pokud bezi timer pro refresh icon-overlays (viz IconOverlaysChangedOnPath())
    DWORD NextIconOvrRefreshTime;             // cas, kdy ma smysl zase zacit sledovat notifikace o zmenach icon-overlays v tomto panelu (viz IconOverlaysChangedOnPath())

public:
    CFilesWindow(CMainWindow* parent);
    ~CFilesWindow();

    BOOL IsGood() { return DirectoryLine != NULL &&
                           StatusLine != NULL && ListBox != NULL && Files != NULL && Dirs != NULL &&
                           IconCache != NULL && PathHistory != NULL && ContextSubmenuNew != NULL &&
                           ExecuteAssocEvent != NULL; }

    // pro ptDisk vraci FALSE, pro ptZIPArchive a ptPluginFS vraci TRUE
    // pokud ma jejich CSalamanderDirectory nastaven flag SALDIRFLAG_CASESENSITIVE
    BOOL IsCaseSensitive();

    // promaze vsechny drzene historie
    void ClearHistory();

    // uspi resp. probudi icon-readera -> prestane sahat do IconCache resp. zacne nacitat
    // ikonky do IconCache; obe metody je mozne volat opakovane - system: posledni volani plati
    void SleepIconCacheThread();
    void WakeupIconCacheThread();

    // vola se pri aktivani/deaktivovani hl. okna Salamandera
    void Activate(BOOL shares);

    // vola se pro informovani panelu o zmenach na ceste 'path' (je-li 'includingSubdirs' TRUE,
    // muzou byt zmeny i v podadresarich cesty)
    void AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // vola se pro informovani panelu o zmenach icon-overlays na ceste 'path' (hlavne informace
    // od Tortoise SVN)
    void IconOverlaysChangedOnPath(const char* path);

    // zkusi jestli je cesta pristupna, prip. obnovi sitova spojeni pomoci funkci
    // CheckAndRestoreNetworkConnection a CheckAndConnectUNCNetworkPath;
    // vraci TRUE pokud je cesta pristupna
    BOOL CheckAndRestorePath(const char* path);

    // rozpozna o jaky typ cesty (FS/Windows/archiv) jde a postara se o rozdeleni na
    // jeji casti (u FS jde o fs-name a fs-user-part, u archivu jde o path-to-archive a
    // path-in-archive, u Windows cest jde o existujici cast a zbytek cesty), u FS cest
    // se nic nekontroluje, u Windows (normal/UNC) cest se kontroluje kam az cesta existuje
    // (prip. obnovit sitove spojeni), u archivu se kontroluje existence souboru archivu
    // (rozliseni archivu dle pripony);
    // 'path' je plna nebo relativni cesta (u relativnich cest se uvazuje cesta v aktivnim panelu
    // jako zaklad pro vyhodnoceni plne cesty), do 'path' se ulozi vysledna plna cesta (musi byt
    // min. 'pathBufSize' znaku); vraci TRUE pri uspesnem rozpoznani, pak 'type' je typ cesty (viz
    // PATH_TYPE_XXX) a 'pathPart' je nastavene:
    // - do 'path' na pozici za existujici cestu (za '\\' nebo na konci retezce; existuje-li
    //   v ceste soubor, ukazuje za cestu k tomuto souboru) (typ cesty windows), POZOR: neresi
    //   se delka vracene casti cesty (cela cesta muze byt delsi nez MAX_PATH)
    // - za soubor archivu (typ cesty archiv), POZOR: neresi se delka cesty v archivu (muze byt
    //   delsi nez MAX_PATH)
    // - za ':' za nazvem file-systemu - user-part cesty file-systemu (typ cesty FS), POZOR: neresi
    //   se delka user-part cesty (muze byt delsi nez MAX_PATH);
    // pokud vraci TRUE je jeste 'isDir' nastavena na:
    // - TRUE pokud prvni cast cesty (od zacatku do 'pathPart') je adresar, FALSE == soubor (typ
    //   cesty Windows)
    // - FALSE pro cesty typu archiv a FS;
    // pokud vrati FALSE, userovi byla vypsana chyba, ktera pri rozpoznavani nastala;
    // 'errorTitle' je titulek message boxu s chybou; pokud je 'nextFocus' != NULL a Windows/archiv
    // cesta neobsahuje '\\' nebo na '\\' jen konci, nakopiruje se cesta do 'nextFocus' (viz
    // SalGetFullName)
    BOOL ParsePath(char* path, int& type, BOOL& isDir, char*& secondPart, const char* errorTitle,
                   char* nextFocus, int* error, int pathBufSize);

    void Execute(int index); // enter + l_dblclk
    // soubor z archivu, execute (edit==FALSE) nebo edit (edit + editWithMenuParent != NULL znamena
    // "edit with")
    void ExecuteFromArchive(int index, BOOL edit = FALSE, HWND editWithMenuParent = NULL,
                            const POINT* editWithMenuPoint = NULL);

    void ChangeSortType(CSortType newType, BOOL reverse, BOOL force = FALSE);

    // zmena drivu na DefaultDir[drive], prip. i s nabidkou disku
    // pri 0 s dialogem, jinak hned zmena
    void ChangeDrive(char drive = 0);

    // najde si prvni pevny disk a na nej prepne;
    // 'parent' je parent message-boxu;
    // v 'noChange' (pokud neni NULL) vraci TRUE pokud data listingu v panelu nebyla znovu
    // vytvorena (Files + Dirs);
    // je-li 'refreshListBox' FALSE, nevola se RefreshListBox;
    // je-li 'canForce' TRUE, dostane user sanci zavrit tvrde i cestu, kterou plug-in uzavrit nechce;
    // je-li 'failReason' != NULL, nastavuje se na jednu z CHPPFR_XXX konstant;
    // jen pri FS v panelu: 'tryCloseReason' je duvod volani CPluginFSInterfaceAbstract::TryCloseOrDetach()
    // vraci TRUE pri uspechu
    BOOL ChangeToFixedDrive(HWND parent, BOOL* noChange = NULL, BOOL refreshListBox = TRUE,
                            BOOL canForce = FALSE, int* failReason = NULL,
                            int tryCloseReason = FSTRYCLOSE_CHANGEPATH);

    // zkusi zmenit cestu na Configuration.IfPathIsInaccessibleGoTo, kdyz neuspeje, zkusi
    // jeste zmenit cestu na fixed-drive (vola ChangeToFixedDrive); vraci TRUE pri uspechu
    BOOL ChangeToRescuePathOrFixedDrive(HWND parent, BOOL* noChange = NULL, BOOL refreshListBox = TRUE,
                                        BOOL canForce = FALSE, int tryCloseReason = FSTRYCLOSE_CHANGEPATH,
                                        int* failReason = NULL);

    // pomocna metoda:
    // slouzi jako priprava pro CloseCurrentPath, pripravi uzavreni/odlozeni soucasne cesty (update
    // editovanych souboru a volani CanCloseArchive v archivech; volani TryCloseOrDetach u FS);
    // vraci TRUE pokud bude cestu mozne uzavrit/odlozit nasledujicim volanim CloseCurrentPath,
    // vraci FALSE pokud se cesta neda uzavrit/odlozit (soucasna cesta se nebude menit, ani zavirat);
    // je-li 'canForce' TRUE, dostane user sanci zavrit tvrde i cestu, kterou plug-in uzavrit
    // nechce (nutne pri uzavirani Salamandera - je-li v plug-inu chyba, nesel by Salamander
    // uzavrit); je-li 'canForce' FALSE a 'canDetach' TRUE a jde o FS cestu, muze byt cesta uzavrena
    // (vraci v 'detachFS' FALSE) nebo odlozena (vraci v 'detachFS' TRUE), v ostatnich pripadech
    // muze byt cesta jen uzavrena (vraci v 'detachFS' FALSE); jen pri FS v panelu: 'tryCloseReason'
    // je duvod volani CPluginFSInterfaceAbstract::TryCloseOrDetach()
    // 'parent' je parent message-boxu
    BOOL PrepareCloseCurrentPath(HWND parent, BOOL canForce, BOOL canDetach, BOOL& detachFS,
                                 int tryCloseReason);
    // pomocna metoda:
    // dokonci zavirani/odkladani soucasne cesty zapocate pomoci PrepareCloseCurrentPath; je-li 'cancel'
    // TRUE, dojde k obnove soucasne cesty (cesta se nezavre, nezmeni; zajisti volani
    // Event(FSE_CLOSEORDETACHCANCELED) u FS); je-li 'cancel' FALSE dojde k uvolneni vsech dale
    // nepotrebnych zdroju soucasne cesty (podle typu cesty jde o: Files, Dirs, PluginData, ArchiveDir,
    // PluginFS a PluginFSDir); 'detachFS' je hodnota vracena z PrepareCloseCurrentPath (smysl jen pro FS,
    // je-li TRUE, nedojde k uvolneni PluginFS, ale k jeho pridani do DetachedFSList);
    // pokud se zavira FS s ikonami typu pitFromPlugin je nutne napred volat SleepIconCacheThread(),
    // aby nedoslo k uvolneni PluginData behem nacitani ikon jeho metodou;
    // 'parent' je parent message-boxu; 'newPathIsTheSame' je TRUE (ma smysl jen je-li 'cancel'
    // FALSE) pokud se po zavreni cesty do panelu dostane stejna cesta (napr. uspesny refresh
    // cesty); 'isRefresh' je TRUE pokud jde o tvrdy refresh cesty (Ctrl+R nebo change-notifikace);
    // je-li 'canChangeSourceUID' TRUE, je mozne zmenit EnumFileNamesSourceUID a tim zrusit
    // enumeraci souboru z panelu (napr. pro viewer), FALSE se pouziva pri zmene cesty na stejnou
    // cestu, jaka uz v panelu byla (obdoba refreshe, jen pres hot-path, focus-name, atd.)
    void CloseCurrentPath(HWND parent, BOOL cancel, BOOL detachFS, BOOL newPathIsTheSame,
                          BOOL isRefresh, BOOL canChangeSourceUID);

    // pomocna metoda:
    // na FS reprezentovanem 'pluginFS' zmeni cestu na 'fsName':'fsUserPart' nebo (pokud neni pristupna)
    // na nejblizsi kratsi pristupnou cestu; vraci FALSE pokud cesta (vcetne podcest) neni pristupna;
    // vraci TRUE pokud se podarilo cestu zmenit a vylistovat, pak 'dir' obsahuje vylistovane
    // soubory a adresare, 'pluginData' obsahuje interface k datum plug-inu o pridanych sloupcich,
    // 'shorterPath' je FALSE pokud je vylistovana presne pozadovana (nezkracena) cesta,
    // jinak je TRUE (zkracena cesta) a 'pluginIconsType' je typ ikon v panelu; je-li 'firstCall'
    // TRUE, zkontroluje se jestli neprobehlo zkraceni cesty na puvodni cestu ("access denied"
    // adresar - okamzity navrat) - pokud ano, zachova vsechna data a vraci *'cancel' TRUE, jinak
    // je *'cancel' FALSE; 'mode' je rezim zmeny cesty (popis hodnot viz ChangePathToPluginFS);
    // 'currentPath' (pokud neni NULL) je user-part puvodni cesty na FS (pro kontrolu
    // zkraceni cesty na puvodni cestu); je-li 'forceUpdate' TRUE, neoptimalizuje se pripad,
    // kdy je soucasna cesta shodna s novou (rusi ucel 'firstCall' TRUE); neni-li 'cutFileName'
    // NULL, jde o buffer MAX_PATH znaku a vraci se v nem jmeno souboru (bez cesty) vracene
    // pluginem v pripade, ze cesta obsahuje jmeno souboru (cesta byla zkracena, soubor bude
    // vyfokusen), pokud nejde o tento pripad, vraci se v 'cutFileName' prazdny retezec;
    // neni-li 'keepOldListing' NULL a ma-li hodnotu TRUE ('dir' musi byt v tomto pripade
    // GetPluginFSDir()), bude se listing ziskavat do tmp objektu (misto primo do 'dir') a
    // k presunu listingu (jen vymena ukazatelu) do 'dir' dojde az v okamziku jeho uspesneho
    // nacteni (pokud vraci chybu, puvodni listing je netknuty), pokud dojde k chybe alokace
    // tmp objektu, vraci se v 'keepOldListing' FALSE a puvodni listing se muze zrusit (pokud
    // se zmeni cesta na FS, jinak se puvodni listing pouzije beze zmeny)
    BOOL ChangeAndListPathOnFS(const char* fsName, int fsNameIndex, const char* fsUserPart,
                               CPluginFSInterfaceEncapsulation& pluginFS, CSalamanderDirectory* dir,
                               CPluginDataInterfaceAbstract*& pluginData, BOOL& shorterPath,
                               int& pluginIconsType, int mode, BOOL firstCall,
                               BOOL* cancel, const char* currentPath,
                               int currentPathFSNameIndex, BOOL forceUpdate,
                               char* cutFileName, BOOL* keepOldListing);

    // zmena cesty - relativni i absolutni cesty na windows cesty (UNC a C:\path), pripadne zkracuje cestu;
    // pokud jde o zmenu v ramci jednoho disku (vcetne archivu), najde platny adresar i za cenu
    // zmeny na "fixed-drive" (pokud je "aktualni disk" nepristupny);
    // 'parent' je parent message-boxu;
    // je-li suggestedTopIndex != -1, bude nastaven top-index;
    // je-li suggestedFocusName != NULL a bude v novem seznamu, bude vybrano;
    // v noChange (pokud neni NULL) vraci TRUE pokud data listingu v panelu nebyla znovu
    // vytvorena (Files + Dirs);
    // je-li refreshListBox FALSE nevola se RefreshListBox;
    // je-li canForce TRUE, dostane user sanci zavrit tvrde i cestu, kterou plug-in uzavrit nechce;
    // je-li isRefresh TRUE, jde o volani z RefreshDirectory (nevypisuje se chyba vedouci ke zkraceni cesty, nerusi se quick-search);
    // je-li failReason != NULL, nastavuje se na jednu z CHPPFR_XXX konstant;
    // je-li shorterPathWarning TRUE, otevre pri zkraceni cesty msg-box s chybou (jen pokud nejde o refresh)
    // jen pri FS v panelu: 'tryCloseReason' je duvod volani CPluginFSInterfaceAbstract::TryCloseOrDetach()
    // vraci TRUE pokud se podarilo vylistovat pozadovanou cestu
    BOOL ChangePathToDisk(HWND parent, const char* path, int suggestedTopIndex = -1,
                          const char* suggestedFocusName = NULL, BOOL* noChange = NULL,
                          BOOL refreshListBox = TRUE, BOOL canForce = FALSE, BOOL isRefresh = FALSE,
                          int* failReason = NULL, BOOL shorterPathWarning = TRUE,
                          int tryCloseReason = FSTRYCLOSE_CHANGEPATH);
    // zmena cesty do archivu, pouze absolutni windows cesty (archive je UNC nebo C:\path\archive);
    // je-li suggestedTopIndex != -1, bude nastaven top-index;
    // je-li suggestedFocusName != NULL a bude v novem seznamu, bude vybrano;
    // je-li forceUpdate TRUE, neoptimalizuje se pripad, kdy je soucasna cesta shodna s novou;
    // v noChange (pokud neni NULL) vraci TRUE pokud data listingu v panelu nebyla znovu
    //   vytvorena (ArchiveDir + PluginData);
    // je-li refreshListBox FALSE nevola se RefreshListBox;
    // je-li failReason != NULL, nastavuje se na jednu z CHPPFR_XXX konstant;
    // je-li isRefresh TRUE, jde o volani z RefreshDirectory (nerusi se quick-search);
    // v pripade, ze archivePath obsahuje jmeno souboru a canFocusFileName je TRUE, dojde
    //   k fokusu tohoto souboru (vraci FALSE - doslo ke zkraceni cesty);
    // je-li isHistory TRUE (pouziva se pri vyberu cesty z historie cest v panelu) a archiv nelze
    //   otevrit (nebo neexistuje), otevre v panelu aspon cestu k archivu (pripadne zkracenou, pri chybe
    //   cesty nechodi na fixed disk);
    // vraci TRUE pokud se podarilo vylistovat pozadovanou cestu
    BOOL ChangePathToArchive(const char* archive, const char* archivePath, int suggestedTopIndex = -1,
                             const char* suggestedFocusName = NULL, BOOL forceUpdate = FALSE,
                             BOOL* noChange = NULL, BOOL refreshListBox = TRUE, int* failReason = NULL,
                             BOOL isRefresh = FALSE, BOOL canFocusFileName = FALSE, BOOL isHistory = FALSE);
    // zmena cesty do plug-inoveho FS;
    // je-li suggestedTopIndex != -1, bude nastaven top-index;
    // je-li suggestedFocusName != NULL a bude v novem seznamu, bude vybrano;
    // je-li forceUpdate TRUE, neoptimalizuje se pripad, kdy je soucasna cesta shodna s novou;
    // 'mode' je rezim zmeny cesty:
    //   1 (refresh path) - zkracuje cestu, je-li treba; nehlasit neexistenci cesty (bez hlaseni
    //                      zkratit), hlasit soubor misto cesty, nepristupnost cesty a dalsi chyby
    //   2 (volani ChangePanelPathToPluginFS z pluginu, back/forward in history, etc.) - zkracuje cestu,
    //                      je-li treba; hlasit vsechny chyby cesty (soubor
    //                      misto cesty, neexistenci, nepristupnost a dalsi)
    //   3 (change-dir command) - zkracuje cestu jen jde-li o soubor nebo cestu nelze listovat
    //                      (ListCurrentPath pro ni vraci FALSE); nehlasit soubor misto cesty
    //                      (bez hlaseni zkratit a vratit jmeno souboru), hlasit vsechny ostatni
    //                      chyby cesty (neexistenci, nepristupnost a dalsi)
    // v noChange (pokud neni NULL) vraci TRUE pokud data listingu v panelu nebyla znovu
    // vytvorena (PluginFSDir + PluginData);
    // je-li refreshListBox FALSE nevola se RefreshListBox;
    // je-li failReason != NULL, nastavuje se na jednu z CHPPFR_XXX konstant;
    // je-li isRefresh TRUE, jde o volani z RefreshDirectory (nerusi se quick-search);
    // v pripade, ze fsUserPart obsahuje jmeno souboru a canFocusFileName je TRUE, dojde
    // k fokusu tohoto souboru (vraci FALSE - doslo ke zkraceni cesty);
    // je-li 'convertPathToInternal' TRUE, vola se CPluginInterfaceForFSAbstract::ConvertPathToInternal();
    // vraci TRUE pokud se podarilo vylistovat pozadovanou cestu
    BOOL ChangePathToPluginFS(const char* fsName, const char* fsUserPart, int suggestedTopIndex = -1,
                              const char* suggestedFocusName = NULL, BOOL forceUpdate = FALSE,
                              int mode = 2, BOOL* noChange = NULL, BOOL refreshListBox = TRUE,
                              int* failReason = NULL, BOOL isRefresh = FALSE,
                              BOOL canFocusFileName = FALSE, BOOL convertPathToInternal = FALSE);
    // zmena cesty do odpojeneho plug-inoveho FS (v MainWindow->DetachedFSList na indexu 'fsIndex');
    // je-li suggestedTopIndex != -1, bude nastaven top-index;
    // je-li suggestedFocusName != NULL a bude v novem seznamu, bude vybrano;
    // je-li refreshListBox FALSE nevola se RefreshListBox;
    // je-li failReason != NULL, nastavuje se na jednu z CHPPFR_XXX konstant;
    // neni-li newFSName a newUserPart NULL, ma se pred pripojenim FS zmenit cesta na 'newFSName':'newUserPart';
    // vyznam parametru 'mode' viz ChangePathToPluginFS + hodnota -1 znamena, ze mode=newUserPart==NULL?1:2;
    // v pripade, ze fsUserPart obsahuje jmeno souboru a canFocusFileName je TRUE, dojde
    // k fokusu tohoto souboru (vraci FALSE - doslo ke zkraceni cesty);
    // vraci TRUE pokud se podarilo vylistovat pozadovanou cestu
    BOOL ChangePathToDetachedFS(int fsIndex, int suggestedTopIndex = -1,
                                const char* suggestedFocusName = NULL, BOOL refreshListBox = TRUE,
                                int* failReason = NULL, const char* newFSName = NULL,
                                const char* newUserPart = NULL, int mode = -1,
                                BOOL canFocusFileName = FALSE);
    // meni cestu v panelu, vstupem muze byt absolutni i relativni cesta a to jak windows, tak do archivu
    // nebo cesta na FS (absolutni/relativni si resi primo plug-in); pokud je vstupem cesta k souboru,
    // dojde k fokusu tohoto souboru;
    // je-li suggestedTopIndex != -1, bude nastaven top-index;
    // je-li suggestedFocusName != NULL a bude v novem seznamu, bude vybrano;
    // 'mode' je rezim zmeny cesty pokud jde o FS cestu - viz ChangePathToPluginFS() - u archivu
    // a disku nema smysl;
    // je-li failReason != NULL, nastavuje se na jednu z CHPPFR_XXX konstant;
    // je-li 'convertFSPathToInternal' TRUE nebo 'newDir' NULL a jde o cestu na FS, vola se
    // CPluginInterfaceForFSAbstract::ConvertPathToInternal();
    // 'showNewDirPathInErrBoxes' jsem doplnil jen kvuli cestam vytazenym z linku (jen diskove cesty),
    // ma se ukazat cela cesta z linku, ne jen cast, na ktere se zjistila chyba (user jinak cestu
    // z linku nedostane);
    // vraci TRUE pokud se podarilo vylistovat pozadovanou cestu
    BOOL ChangeDir(const char* newDir = NULL, int suggestedTopIndex = -1,
                   const char* suggestedFocusName = NULL, int mode = 3 /*change-dir*/,
                   int* failReason = NULL, BOOL convertFSPathToInternal = TRUE,
                   BOOL showNewDirPathInErrBoxes = FALSE);

    // mene ortodoxni verze ChangeDir: vraci TRUE i pokud ChangeDir vraci FALSE a 'failReason'
    // je CHPPFR_SHORTERPATH nebo CHPPFR_FILENAMEFOCUSED
    BOOL ChangeDirLite(const char* newDir);

    BOOL ChangePathToDrvType(HWND parent, int driveType, const char* displayName = NULL);

    // vola se po ziskani noveho listingu ... hlaseni o zmenach posbiranych pro stary
    // listing je potreba zneplatnit
    void InvalidateChangesInPanelWeHaveNewListing();

    void SetAutomaticRefresh(BOOL value, BOOL force = FALSE);

    // nastavi ValidFileData; provadi kontroly jestli je mozne pouzit konstanty
    // VALID_DATA_PL_XXX (nesmi byt prazdne PluginData + nesmi se pouzivat prislusna
    // konstanta VALID_DATA_SIZE nebo VALID_DATA_DATE nebo VALID_DATA_TIME)
    void SetValidFileData(DWORD validFileData);

    // nastavi ikonu "disku" do dir-liny; pro kazdy panel-type jinak, viz kod...
    // je-li 'check' TRUE a jde o "ptDisk", nejprve se overi cesta pres CheckPath
    void UpdateDriveIcon(BOOL check);

    // obnovi udaj o volnem miste na disku; pro kazdy panel-type jinak, viz kod...
    // je-li 'check' TRUE a jde o "ptDisk", nejprve se overi cesta pres CheckPath;
    // je-li 'doNotRefreshOtherPanel' FALSE a v druhem panelu je diskova cesta
    // se stejnym rootem, provede se hned i refresh-disk-free-space v druhem panelu
    void RefreshDiskFreeSpace(BOOL check = TRUE, BOOL doNotRefreshOtherPanel = FALSE);

    void UpdateFilterSymbol(); // zajisti nastaveni statusbary

    // vola se pri zavirani FS - v historii jsou ulozene FS ifacy, ktere je po zavreni potreba
    // NULLovat (aby nahodou nedoslo ke shode jen diky alokaci FS ifacu na stejnou adresu)
    void ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs);

    // obnovi top-index a focused-name v aktualni polozce historie cest v tomto panelu
    void RefreshPathHistoryData();

    // odstrani aktualni cestu v panelu z historie cest v tomto panelu
    void RemoveCurrentPathFromHistory();

    // vraci TRUE, pokud jiz plug-in neni panelem pouzivan
    BOOL CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin);

    void ItemFocused(int index); // pri zmene focusu
    void RedrawIndex(int index);

    void SelectUnselect(BOOL forceIncludeDirs, BOOL select, BOOL showMaskDlg);
    void InvertSelection(BOOL forceIncludeDirs);

    // 'select' - TRUE: pujde o select FALSE: unselect
    // 'byName' - TRUE: vyberou se vsechny polozky se stejnym jmenem, jako ma polozka focusnuta
    //            FALSE: ridi se misto jmena priponou
    // Funkce cti promennou Configuration::IncludeDirs
    void SelectUnselectByFocusedItem(BOOL select, BOOL byName);

    // ulozi selection do globalni promenne GlobalSelection nebo na Clipboard
    void StoreGlobalSelection();

    // nastavi selection podle GlobalSelection nebo podle Clipboardu
    void RestoreGlobalSelection();

    // ulozi selection do OldSelection
    void StoreSelection();
    // obnovi selection podle stavu v OldSelection
    void Reselect();

    void ShowHideNames(int mode); // 0:show all 1:hide selected 2:hide unselected

    // nuluje flag cut-to-clip a je-li 'repaint' TRUE, vola RefreshListBox
    void ClearCutToClipFlag(BOOL repaint);

    // vrati index aktualniho pohledu
    int GetViewTemplateIndex();

    // vrati index dalsiho (pokud je 'forward' TRUE, jinak predesleho) pohledu
    // pokud je 'wrap' TRUE, za posledni/prvni polozkou pokracuje z druhe strany seznamu
    int GetNextTemplateIndex(BOOL forward, BOOL wrap);

    // vrati TRUE, pokud na int templateIndex lezi sablona, do ktere pujde prepnout pres
    // volani SelectViewTemplate
    BOOL IsViewTemplateValid(int templateIndex);

    // nastavi panelu aktualni sablonu
    // prvni sablony jsou pevne a dalsi muze uzivatel editovat
    // 'templateIndex' udava index sablony, ktera ma byt vybrana.
    // pokud je sablona na pozadovanem indexu neplatna, bude vybrana sablona s indexem 2 (detailed)
    // pokud je 'preserveTopIndex' TRUE, zachova se topIndex (nebude zajistena viditelnost focusu)
    BOOL SelectViewTemplate(int templateIndex, BOOL canRefreshPath,
                            BOOL calledFromPluginBeforeListing, DWORD columnValidMask = VALID_DATA_ALL,
                            BOOL preserveTopIndex = FALSE, BOOL salamanderIsStarting = FALSE);

    // vrati FALSE, pokud pripona neni definovana (viz VALID_DATA_EXTENSION) nebo je pripona
    // (CFileData::Ext) soucasti sloupce Name; jinak vrati TRUE (pro CFileData::Ext existuje
    // zvlastni sloupec)
    BOOL IsExtensionInSeparateColumn();

    void ToggleStatusLine();
    void ToggleDirectoryLine();
    void ToggleHeaderLine();

    void ConnectNet(BOOL readOnlyUNC, const char* netRootPath = NULL, BOOL changeToNewDrive = TRUE, char* newlyMappedDrive = NULL);
    void DisconnectNet();

    // v detailed rezimu vraci: min((sirka panelu) - (sirka vsech viditelnych sloupcu mimo sloupce NAME), (sirka sloupce NAME))
    // v brief rezimu vraci 0
    int GetResidualColumnWidth(int nameColWidth = 0);

    void ChangeAttr(BOOL setCompress = FALSE, BOOL compressed = FALSE,
                    BOOL setEncryption = FALSE, BOOL encrypted = FALSE);
    void Convert(); // konverze znakovych sad + koncu radek
    // handlerID udava, kterym viewerm/editorem se ma soubor otevrit; 0xFFFFFFFF = zadny preferovany
    void ViewFile(char* name, BOOL altView, DWORD handlerID, int enumFileNamesSourceUID,
                  int enumFileNamesLastFileIndex);           // name == NULL -> polozka pod kurzorem v panelu
    void EditFile(char* name, DWORD handlerID = 0xFFFFFFFF); // name == NULL -> polozka pod kurzorem v panelu
    void EditNewFile();
    // na zaklade dostupnych vieweru naplni popup
    void FillViewWithMenu(CMenuPopup* popup);
    // naplni pole ze ktereho se nasledne zobrazuje popup
    BOOL FillViewWithData(TDirectArray<CViewerMasksItem*>* items);
    // zobrazi focused soubor s pouzitim vieweru identifikovaneho promennou 'index'
    void OnViewFileWith(int index);
    // view-file-with: vybali menu s vyberem vieweru; name == NULL -> polozka pod kurzorem v panelu;
    // handlerID != NULL -> vraci jen cislo vybraneho handleru (neotevira viewer), pri chybe vraci 0xFFFFFFFF
    void ViewFileWith(char* name, HWND hMenuParent, const POINT* menuPos, DWORD* handlerID,
                      int enumFileNamesSourceUID, int enumFileNamesLastFileIndex);

    // na zaklade dostupnych editoru naplni popup
    void FillEditWithMenu(CMenuPopup* popup);
    // edituje focused soubor s pouzitim editoru identifikovaneho promennou 'index'
    void OnEditFileWith(int index);
    // edit-file-with: vybali menu s vyberem editoru; name == NULL -> polozka pod kurzorem v panelu;
    // handlerID != NULL -> vraci jen cislo vybraneho handleru (neotevira viewer), pri chybe vraci 0xFFFFFFFF
    void EditFileWith(char* name, HWND hMenuParent, const POINT* menuPos, DWORD* handlerID = NULL);
    void FindFile();
    void DriveInfo();
    void OpenActiveFolder();

    void GotoHotPath(int index);
    void SetUnescapedHotPath(int index);
    BOOL SetUnescapedHotPathToEmptyPos();
    void GotoRoot();

    void FilesAction(CActionType type, CFilesWindow* target = NULL, int countSizeMode = 0);
    void PluginFSFilesAction(CPluginFSActionType type);
    void CreateDir(CFilesWindow* target);
    void RenameFile(int specialIndex = -1);
    void RenameFileInternal(CFileData* f, const char* formatedFileName, BOOL* mayChange, BOOL* tryAgain);
    void DropCopyMove(BOOL copy, char* targetPath, CCopyMoveData* data);

    // provede vymaz pomoci API funkce SHFileOperation (jen pokud mazeme do kose)
    BOOL DeleteThroughRecycleBin(int* selection, int selCount, CFileData* oneFile);

    BOOL BuildScriptMain(COperations* script, CActionType type, char* targetPath,
                         char* mask, int selCount, int* selection,
                         CFileData* oneFile, CAttrsData* attrsData,
                         CChangeCaseData* chCaseData, BOOL onlySize,
                         CCriteriaData* filterCriteria);
    BOOL BuildScriptDir(COperations* script, CActionType type, char* sourcePath,
                        BOOL sourcePathSupADS, char* targetPath, CTargetPathState targetPathState,
                        BOOL targetPathSupADS, BOOL targetPathIsFAT32, char* mask, char* dirName,
                        char* dirDOSName, CAttrsData* attrsData, char* mapName,
                        DWORD sourceDirAttr, CChangeCaseData* chCaseData, BOOL firstLevelDir,
                        BOOL onlySize, BOOL fastDirectoryMove, CCriteriaData* filterCriteria,
                        BOOL* canDelUpperDirAfterMove, FILETIME* sourceDirTime,
                        DWORD srcAndTgtPathsFlags);
    BOOL BuildScriptFile(COperations* script, CActionType type, char* sourcePath,
                         BOOL sourcePathSupADS, char* targetPath, CTargetPathState targetPathState,
                         BOOL targetPathSupADS, BOOL targetPathIsFAT32, char* mask, char* fileName,
                         char* fileDOSName, const CQuadWord& fileSize, CAttrsData* attrsData,
                         char* mapName, DWORD sourceFileAttr, CChangeCaseData* chCaseData,
                         BOOL onlySize, FILETIME* fileLastWriteTime, DWORD srcAndTgtPathsFlags);
    BOOL BuildScriptMain2(COperations* script, BOOL copy, char* targetDir,
                          CCopyMoveData* data);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // nacte Files a Dirs podle typu panelu z disku, ArchiveDir nebo PluginFSDir;
    // u "ptDisk" vraci TRUE pokud byl adresar uspesne nacten z disku (FALSE pri chybe
    // nacitani nebo nedostatku pameti), u "ptZIPArchive" a "ptPluginFS" vraci FALSE jen
    // pokud dojde pamet nebo pri neexistujici ceste (to se ale kontroluje pred volanim
    // ReadDirectory -> nemelo by nastat); 'parent' je parent message-boxu
    // pokud vraci TRUE, provede take SortDirectory()
    BOOL ReadDirectory(HWND parent, BOOL isRefresh);

    // radi Files a Dirs podle aktualniho zpusobu razeni, vzhledem k tomu, ze meni poradi
    // Files a Dirs musi byt po dobu razeni pozastaveno nacitani ikon (viz SleepIconCacheThread())
    void SortDirectory(CFilesArray* files = NULL, CFilesArray* dirs = NULL);

    void RefreshListBox(int suggestedXOffset,         // je-li ruzny od -1 pouzije se
                        int suggestedTopIndex,        // je-li ruzny od -1 pouzije se
                        int suggestedFocusIndex,      // je-li ruzny od -1 pouzije se
                        BOOL ensureFocusIndexVisible, // zajisti alespon castecnou viditelnost focusu
                        BOOL wholeItemVisible);       // cela polozka musi byt zobrazena (ma vyznam pokud je ensureFocusIndexVisible TRUE)

    // 'probablyUselessRefresh' je TRUE, pokud jde nejspis jen o zbytecny refresh, ktery byl
    // vyvolany pouhym nactenim ikonek ze souboru na sitovem disku;
    // 'forceReloadThumbnails' je TRUE pokud potrebujeme znovu generovat vsechny thumbnaily
    // (nejen thumbnaily zmenenych souboru); 'isInactiveRefresh' je TRUE pokud je to refresh
    // v neaktivnim okne, budeme nacitat jen viditelne ikony/thumbnaily/overlaye, ostatni se
    // nactou az po aktivaci hl. okna
    void RefreshDirectory(BOOL probablyUselessRefresh = FALSE, BOOL forceReloadThumbnails = FALSE,
                          BOOL isInactiveRefresh = FALSE);

    // read-dir (archivy, FS, disk), sort
    // parent je parent message-boxu
    // je-li suggestedTopIndex != -1, bude nastaveno
    // je-li suggestedFocusName != NULL a bude pritomno v novem seznamu, bude vybrano
    // je-li refreshListBox FALSE nevola se RefreshListBox
    // je-li readDirectory FALSE, nevola se ReadDirectory
    // je-li isRefresh TRUE, provadi se timto refresh cesty v panelu
    // vraci TRUE pokud byl uspesny ReadDirectory
    BOOL CommonRefresh(HWND parent, int suggestedTopIndex = -1,
                       const char* suggestedFocusName = NULL, BOOL refreshListBox = TRUE,
                       BOOL readDirectory = TRUE, BOOL isRefresh = FALSE);

    // zajisti refresh panelu po zmene konfigurace (u archivu meni casovou znamku, aby doslo k refreshi)
    void RefreshForConfig();

    void RedrawFocusedIndex();

    void DirectoryLineSetText();

    // rozpakovavani nebo mazani z archivu (nejen ZIP); 'target' muze byt NULL
    // pokud 'tgtPath' neni NULL; neni-li 'tgtPath' NULL, dojde k vybaleni na
    // tuto cestu bez dotazu userovi
    void UnpackZIPArchive(CFilesWindow* target, BOOL deleteOp = FALSE, const char* tgtPath = NULL);
    // mazani z archivu (nejen ZIP) - jen vola UnpackZIPArchive
    void DeleteFromZIPArchive();
    // presune vsechny soubory ze source adresare do target adresare,
    // navic premapovava zobrazovana jmena
    BOOL MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                   const char* remapNameTo);

    // pomocna funkce: pred provedenim prikazu nebo drag&dropu nabidne update archivu
    void OfferArchiveUpdateIfNeeded(HWND parent, int textID, BOOL* archMaybeUpdated);
    void OfferArchiveUpdateIfNeededAux(HWND parent, int textID, BOOL* archMaybeUpdated); // pouziva se jen interne

    // do souboru hFile naleje seznam oznacenych souboru
    BOOL MakeFileList(HANDLE hFile);

    void Pack(CFilesWindow* target, int pluginIndex = -1, const char* pluginName = NULL, int delFilesAfterPacking = 0);
    void Unpack(CFilesWindow* target, int pluginIndex = -1, const char* pluginName = NULL, const char* unpackMask = NULL);

    void CalculateOccupiedZIPSpace(int countSizeMode = 0);

    // vrati bod, kde lze vybalit kontextove menu (v souradnicich obrazovky)
    void GetContextMenuPos(POINT* p);

    // pomocna funkce pro DrawBriefDetailedItem a DrawIconThumbnailItem
    void SetFontAndColors(HDC hDC, CHighlightMasksItem* highlightMasksItem,
                          CFileData* f, BOOL isItemFocusedOrEditMode,
                          int itemIndex);

    // pomocna funkce pro DrawBriefDetailedItem a DrawIconThumbnailItem
    // pokud je overlayRect ruzne od NULL, umisti se overlay do jeho leveho dolniho rohu
    void DrawIcon(HDC hDC, CFileData* f, BOOL isDir, BOOL isItemUpDir,
                  BOOL isItemFocusedOrEditMode, int x, int y, CIconSizeEnum iconSize,
                  const RECT* overlayRect, DWORD drawFlags);

    // kresli polozku pro Brief a Detailed rezim (ikonka 16x16 vlevo, za ni vpravo text)
    void DrawBriefDetailedItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags);
    // kresli polozku pro Icon a Thumbnails rezim (ikonka/thumbnail nahore, pod ni text)
    void DrawIconThumbnailItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags,
                               CIconSizeEnum iconSize);
    // kresli polozku pro Tiles rezim (ikonka vlevo, za ni vpravo multiline texty)
    void DrawTileItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags,
                      CIconSizeEnum iconSize);

    void CalculateDirSizes(); // vypocet velikosti vsech podadresaru

    void DragEnter();
    void DragLeave();

    void HandsOff(BOOL off); // off==TRUE - odpoji panel od disku (monitoring off), FALSE - opet pripoji

    // funkce pro obsluhu sloupcu
    BOOL BuildColumnsTemplate();                                           // na zaklade aktualni sablony pohledu 'ViewTemplate' a typu obsahu panelu (disk / archiv+FS)
                                                                           // naplni pole 'ColumnsTemplate'.
    BOOL CopyColumnsTemplateToColumns();                                   // prenese obsah pole 'ColumnsTemplate' do 'Columns'
    void DeleteColumnsWithoutData(DWORD columnValidMask = VALID_DATA_ALL); // vymaze sloupce, pro ktere nemame data (podle ValidFileData & columnValidMask)

    // Akce Kulovy Blesk: preneseno z FILESBOX.H
    void ClipboardCopy();
    void ClipboardCut();
    BOOL ClipboardPaste(BOOL onlyLinks = FALSE, BOOL onlyTest = FALSE, const char* pastePath = NULL);
    BOOL ClipboardPasteToArcOrFS(BOOL onlyTest, DWORD* pasteDefEffect); // 'pasteDefEffect' muze byt NULL
    BOOL ClipboardPasteLinks(BOOL onlyTest = FALSE);
    BOOL IsTextOnClipboard();
    void ClipboardPastePath(); // pro zmenu aktualniho adresare

    // postprocessing cesty zadane uzivatelem: odstrani white-spaces a uvozovky kolem cesty + odstrani file:// +
    // expanduje ENV promenne; vraci FALSE pri chybe (nema se pokracovat), 'parent' je parent pro error msgboxy
    BOOL PostProcessPathFromUser(HWND parent, char (&buff)[2 * MAX_PATH]);

    // pokud je disable==FALSE, otevre se dialog z moznosti volby
    // pokud je disable==TRUE, filtr ze vypne
    void ChangeFilter(BOOL disable = FALSE);

    void EndQuickSearch(); // ukonci rezim Quick Search

    // QuickRenameWindow
    void AdjustQuickRenameRect(const char* text, RECT* r); // upravi 'r' tak, aby nepresahoval panel a zaroven byl dostatecne veliky
    void AdjustQuickRenameWindow();
    //    void QuickRenameOnIndex(int index); // zavola QuickRenameBegin pro patricny index
    void QuickRenameBegin(int index, const RECT* labelRect); // otevre QuickRenameWindow
    void QuickRenameEnd();                                   // ukonci rezim Quick Rename
    BOOL IsQuickRenameActive();                              // vraci TRUE, pokud je otevrene QuickRenameWindow

    // vraci FALSE, pokud doslo k chybe behem prejmenovani a Edit okno ma zustat otevrene
    // jinak vraci TRUE
    BOOL HandeQuickRenameWindowKey(WPARAM wParam); // QuickRenameWindow vola tuto metodu pri VK_ESCAPE nebo VK_RETURN

    void KillQuickRenameTimer(); // pokud timer bezi, bude sestrelen

    // pokud bezi QuickSearch nebo QuickRename, ukonci je
    void CancelUI();

    // Hleda dalsi/prechozi polozku. Pri skip = TRUE preskoci polozku aktualni
    // pokud je newChar != 0, bude pripojen ke QuickSearchMask
    // pokud je wholeString == TRUE, musi odpovidat cela polozka, ne pouze jeji zacatek
    // vrati TRUE, pokud byl nalezen adresar/soubor a pak take nastavi index
    BOOL QSFindNext(int currentIndex, BOOL next, BOOL skip, BOOL wholeString, char newChar, int& index);

    // Hleda dalsi/prechozi vybranou polozku. Pri skip = TRUE preskoci polozku aktualni
    BOOL SelectFindNext(int currentIndex, BOOL next, BOOL skip, int& index);

    // vraci TRUE pokud je v panelu otevreny Nethood FS
    BOOL IsNethoodFS();

    void CtrlPageDnOrEnter(WPARAM key);
    void CtrlPageUpOrBackspace();

    // pokusi se otevrit focusnuty soubor jako shortcut, vytahnout z nej target
    // a ten focusnout v panelu 'panel'
    void FocusShortcutTarget(CFilesWindow* panel);

    // 'scroll' to partially (TRUE), full (FALSE) visible
    // forcePaint zajisti prekresleni i v pripade, ze se index nemeni
    void SetCaretIndex(int index, BOOL scroll, BOOL forcePaint = FALSE);
    int GetCaretIndex();

    void SetDropTarget(int index);       // oznaci kam se dropnou soubory
    void SetSingleClickIndex(int index); // zvirazni polozku a starou zhasne
    void SelectFocusedIndex();

    void DrawDragBox(POINT p);
    void EndDragBox(); // ukonci tazeni

    // !!! pozor - tyto metody nezajisti prekresleni polozek, pouze nastavuji bit Dirty
    // prekresleni je treba vyvolat explicitne pomoci:
    // RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    // pouze prvni metoda SetSel dokaze na explicitni zadost polozky prekreslit
    // prvni dve motody (SetSel a SetSelRange) metody neoznaci polozku ".." z directories
    void SetSel(BOOL select, int index, BOOL repaintDirtyItems = FALSE); // If index is -1 the selection is added to or removed from all strings
    // vraci TRUE, pokud se alespon jedne polozce zmenil stav, jinak vraci FALSE
    BOOL SetSelRange(BOOL select, int firstIndex, int lastIndex);
    void SetSel(BOOL select, CFileData* data); // data musi byt drzena prislusnym seznamem

    BOOL GetSel(int index);
    int GetSelItems(int itemsCountMax, int* items, BOOL focusedItemFirst = FALSE); // je-li 'focusedItemFirst' TRUE: od tohoto jsme ustoupili (viz telo GetSelItems): pro kontextova menu zaciname od fokusle polozky a koncime polozku pres fokusem (je tam mezilehle vraceni na zacatek seznamu jmen) (system to tak dela taky, viz Add To Windows Media Player List na MP3 souborech)

    // pokud je GetSelCount > 0, vrati se TRUE, je-li vybran alespon jeden adresar (nepocita se ".."); jinak se vrati FALSE
    // pokud je GetSelCount == 0, vrati se TRUE, je-li focusen adresar (nepocita se ".."); jinak se vrati FALSE
    BOOL SelectionContainsDirectory();
    BOOL SelectionContainsFile(); // to same pro soubory

    int GetSelCount(); // vrati pocet vybranych polozek

    void SelectFocusedItemAndGetName(char* name, int nameMax);
    void UnselectItemWithName(const char* name);

    // vraci PANEL_LEFT nebo PANEL_RIGHT podle toho, kde tento panel je
    int GetPanelCode();

    void SafeHandleMenuNewMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);
    void RegisterDragDrop();
    void RevokeDragDrop();

    HWND GetListBoxHWND(); // poze volaji listbox
    HWND GetHeaderLineHWND();
    int GetIndex(int x, int y, BOOL nearest = FALSE, RECT* labelRect = NULL);

    // funkce volane listboxem
    BOOL OnSysChar(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnChar(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnSysKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnSysKeyUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult);

    void GotoSelectedItem(BOOL next); // pokud je 'next' TRUE, posadi caret na dalsi vybranou polozku, jinak na predchozi

    void OnSetFocus(BOOL focusVisible = TRUE); // 'focusVisible'==FALSE pri prepinani z commandline do panelu v okamziku, kdy je user v nemodalnim dialogu (neni akt. hl. okno) - potrebuje plugin FTP - Welcome Message dialog
    void OnKillFocus(HWND hwndGetFocus);

    BOOL OnLButtonDown(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnLButtonUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnLButtonDblclk(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnRButtonDown(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnRButtonUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnMouseMove(WPARAM wParam, LPARAM lParam, LRESULT* lResult);

    BOOL IsDragDropSafe(int x, int y); // bezpecnejsi d&d: pokud bylo tazeni dostatecne dlouhe, vraci TRUE; x,y jsou relativni souradnice vuci pocatku panelu

    BOOL OnTimer(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnCaptureChanged(WPARAM wParam, LPARAM lParam, LRESULT* lResult);
    BOOL OnCancelMode(WPARAM wParam, LPARAM lParam, LRESULT* lResult);

    void LayoutListBoxChilds(); // po zmene fontu je treba provest layout
    void RepaintListBox(DWORD drawFlags);
    void RepaintIconOnly(int index);   // pro index == -1 se prekresli ikony vsech polozek
    void EnsureItemVisible(int index); // zajisti viditelnost polozky
    void SetQuickSearchCaretPos();     // nastavi pozici kurzoruv ramci FocusedIndex

    void SetupListBoxScrollBars();

    // vytvori imagelist s jednim prvkem, ktery bude pouzit pro zobrazovani prubehu tazeni
    // po ukonceni tazeni je treba tento imagelist uvolnit
    // vstupem je bod, ke kteremu se napocitaji offsety dxHotspot a dyHotspot
    // take je vracena velikost tazeneho obrazku
    HIMAGELIST CreateDragImage(int cursorX, int cursorY, int& dxHotspot, int& dyHotspot, int& imgWidth, int& imgHeight);

    // umisti na clipboard nazev vybraneho souboru nebo adresare (pripadne s plnou cestu nebo v UNC tvaru)
    // v pripade mode==cfnmUNC se funkce pokusi prevest cestu z tvaru "x:\" na UNC
    //                         a pokud to nedopadne, zobrazi chybu
    BOOL CopyFocusedNameToClipboard(CCopyFocusedNameModeEnum mode);

    // umisti na clipboard plnou cestu (aktivni v panelu)
    BOOL CopyCurrentPathToClipboard();

    void OpenDirHistory();

    void OpenStopFilterMenu(); // slouzi pro vypnuti jednotlivych filtru

    // omeri rozmery a vrati TRUE, pokud jsou dostatecne pro focus
    BOOL CanBeFocused();

    // na zaklade dostupnych sloupcu naplni popup
    BOOL FillSortByMenu(CMenuPopup* popup);

    // pokud uzivatel zmeni sirku sloupce, bude zavolana tato metoda (po ukonceni tazeni)
    void OnHeaderLineColWidthChanged();

    CHeaderLine* GetHeaderLine();

    // posle focuseny/oznacene soubory defaultnim postovnim clientem
    void EmailFiles();

    // doslo ke zmene barev nebo barevne hloubky obrazovky; uz jsou vytvorene nove imagelisty
    // pro tooblary a je treba je priradit controlum, ktere je pozuivaji
    void OnColorsChanged();

    // otevre v druhem panelu cestu slozenou z aktualni cesty a jmena na focusu
    // pokud je 'activate' TRUE, zaroven aktivuje druhy panel
    // vraci TRUE v pripade uspechu, jinak vraci FALSE
    BOOL OpenFocusedInOtherPanel(BOOL activate);

    // otevre v panelu cestu z druheho panelu
    void ChangePathToOtherPanelPath();

    // vola se v idle rezimu Salamandera, pokud je treba, uklada se zde pole
    // viditelnych polozek
    void RefreshVisibleItemsArray();

    // drag&drop z disku do archivu nebo plugin-FS
    void DragDropToArcOrFS(CTmpDragDropOperData* data);

    CViewModeEnum GetViewMode();
    CIconSizeEnum GetIconSizeForCurrentViewMode();

    void SetThumbnailSize(int size);
    int GetThumbnailSize();

    void SetFont();

    void LockUI(BOOL lock);
};

//****************************************************************************
//
// CPanelTmpEnumData
//

struct CPanelTmpEnumData
{
    int* Indexes;
    int CurrentIndex;
    int IndexesCount;
    const char* ZIPPath;              // archive root pro celou operaci
    CFilesArray* Dirs;                // aktualni seznam adresaru, do kterych vedou oznacene indexy z Indexes
    CFilesArray* Files;               // aktualni seznam souboru, do kterych vedou oznacene indexy z Indexes
    CSalamanderDirectory* ArchiveDir; // archive-dir aktualniho archivu

    // pro enum-zip-selection, enumFiles > 0
    CSalamanderDirectory* EnumLastDir;
    int EnumLastIndex;
    char EnumLastPath[MAX_PATH];
    char EnumTmpFileName[MAX_PATH];

    // pro enum-disk-selection, enumFiles > 0
    char WorkPath[MAX_PATH];                 // cesta, na ktere jsou Files a Dirs, vyuziva se jen pri prochazeni disku (ne archivu)
    CSalamanderDirectory* DiskDirectoryTree; // nahrazka za Panel->ArchiveDir
    char EnumLastDosPath[MAX_PATH];          // dos-name EnumLastPath
    char EnumTmpDosFileName[MAX_PATH];       // dos-name EnumTmpFileName
    int FilesCountReturnedFromWP;            // pocet souboru jiz vracenych enumeratorem primo z WorkPath (nebo-li z Files)

    CPanelTmpEnumData();
    ~CPanelTmpEnumData();

    void Reset(); // nastavi objekt do stavu zacatku enumerace
};

const char* WINAPI PanelEnumDiskSelection(HWND parent, int enumFiles, const char** dosName, BOOL* isDir,
                                          CQuadWord* size, DWORD* attr, FILETIME* lastWrite, void* param,
                                          int* errorOccured);

//****************************************************************************
//
// externals
//

extern CNames GlobalSelection; // ulozene oznaceni, sdilene obema panely

extern CDirectorySizesHolder DirectorySizesHolder; // drzi seznam jmen adresaru+velikosti se znamou velikosti

extern CFilesWindow* DropSourcePanel; // pro zamezeni drag&dropu z/do stejneho panelu
extern BOOL OurClipDataObject;        // TRUE pri "paste" naseho IDataObject
                                      // (detekce vlastni copy/move rutiny s cizimi daty)

// enumerace oznacenych souboru a adresaru z panelu
const char* WINAPI PanelSalEnumSelection(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                         const CFileData** fileData, void* param, int* errorOccured);

//****************************************************************************
//
// SplitText
//
// Vyuziva pole 'DrawItemAlpDx'
//
// text      [IN]  vstupni retezec, ktery budeme delit
// textLen   [IN]  pocet znaku v retezci 'text' (bez terminatoru)
// maxWidth  [IN]  maximalni pocet bodu, kolik delsi radek na sirku muze mit
//           [OUT] skutecna maximalni sirka sirka
// out1      [OUT] do tohoto retezce bude nakopirovan prvni radek vystupu bez terminatoru
// out1Len   [IN]  maximalni mozny pocet znaku, ktere lze zapsat do 'out1'
//           [OUT] pocet znaku nakopirovanych do 'out1'
// out1Width [OUT] sirka 'out1' v bodech
// out2            stejne jako out1, ale pro druhy radek
// out2Len
// out2Width
//

void SplitText(HDC hDC, const char* text, int textLen, int* maxWidth,
               char* out1, int* out1Len, int* out1Width,
               char* out2, int* out2Len, int* out2Width);

//
// Nakopiruje na clipboard UNC tvar cesty.
// 'path' je cesta bud v primem nebo UNC tvaru
// 'name' je vybrana polozka a 'isDir' urcuje, zda jde o soubor nebo o adresar
// 'hMessageParent' je okno, ke kteremu se zobrazi messagebox v pripade neuspechu
// 'nestingLevel' interni pocitadlo poctu zanoreni pri resolvovani SUBSTu (mohou byt cyklicke)
//
// Vraci TRUE v pripade uspechu a FALSE v pripade, ze se nepodarilo sestavit
// UNC cestu nebo ji nebylo mozne dostat na clipboard.
//
// Funkci ze volat z libovolneho threadu.
//

BOOL CopyUNCPathToClipboard(const char* path, const char* name, BOOL isDir, HWND hMessageParent, int nestingLevel = 0);

// z souboru/adresare 'f' vytvori tri radky textu a naplni out0/out0Len az out2/out2Len;
// 'validFileData' urcuje jake casti 'f' jsou platne
void GetTileTexts(CFileData* f, int isDir,
                  HDC hDC, int maxTextWidth, int* widthNeeded,
                  char* out0, int* out0Len,
                  char* out1, int* out1Len,
                  char* out2, int* out2Len,
                  DWORD validFileData,
                  CPluginDataInterfaceEncapsulation* pluginData,
                  BOOL isDisk);
