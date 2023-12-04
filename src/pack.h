// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
// Konstanty
//

// System nedefinuje delku komandlajny pro DOSove aplikace, musime to udelat sami
#define DOS_MAX_PATH 80
// maximalni mozna delka prikazove radky
#define PACK_CMDLINE_MAXLEN (MAX_PATH * 4)
// na jake hodnote zacinaji vlastni chyby salspawn.exe
#define SPAWN_ERR_BASE 10000
// program pro spousteni 16-ti bitovych archveru (parametr -c musi odpovidat SPAWN_ERR_BASE)
extern const char* SPAWN_EXE_PARAMS;
// nazev programu salspawn
extern const char* SPAWN_EXE_NAME;

// indexy jednotlivych pakovacu v poli ArchiverConfig
#define PACKJAR32INDEX 0
#define PACKJAR16INDEX 5
#define PACKRAR32INDEX 1
#define PACKRAR16INDEX 6
#define PACKARJ32INDEX 9
#define PACKARJ16INDEX 2
#define PACKLHA16INDEX 3
#define PACKUC216INDEX 4
#define PACKACE32INDEX 10
#define PACKACE16INDEX 11
#define PACKZIP32INDEX 7
#define PACKZIP16INDEX 8

// ****************************************************************************
// Typy
//

// Typ programu
enum EPackExeType
{
    EXE_INVALID,
    EXE_32BIT,
    EXE_16BIT,
    EXE_END
};

// struktura pro drzeni cesty k pakovaci
struct SPackLocation
{
    const char* Variable;
    const char* Executable;
    EPackExeType Type;
    const char* Value;
    BOOL Valid;
};

// Trida drzici informace o nalezenych programech
class CPackACFound
{
public:
    char* FullName;     // jmeno nalezeneho programu i s cestou
    CQuadWord Size;     // velikost
    FILETIME LastWrite; // datum
    BOOL Selected;

    CPackACFound()
    {
        FullName = NULL;
        Selected = FALSE;
    }
    ~CPackACFound()
    {
        if (FullName != NULL)
            free(FullName);
    }
    BOOL Set(const char* fullName, const CQuadWord& size, FILETIME lastWrite);
    void InvertSelect() { Selected = !Selected; }
    void Select(BOOL newSelect) { Selected = newSelect; }
    BOOL IsSelected() { return Selected; }
    char* GetText(int column);
};

// pole nalezenych pakovacu
class CPackACArray : public TIndirectArray<CPackACFound>
{
public:
    CPackACArray(int base, int delta, CDeleteType dt = dtDelete) : TIndirectArray<CPackACFound>(base, delta, dt) {}
    int AddAndCheck(CPackACFound* member);
    void InvertSelect(int index);
    const char* GetSelectedFullName();
};

enum EPackPackerType
{
    Packer_Standalone, // umi pakovat i rozpakovavat
    Packer_Packer,     // umi jen pakovat
    Packer_Unpacker    // umi jen rozpakovavat
};

// Trida drzici seznam pozadovanych a posleze i nalezenych pakovacu
class CPackACPacker
{
protected:
    // odkud jsou data
    int Index;                // index do pole ArchiversConfig
    EPackPackerType Unpacker; // jde o packer, unpacker, nebo oboje ?
    const char* Title;        // titulek, ktery popisuje pakovac
    // co chceme nalezt
    const char* Name;  // jmeno programu pro hledani
    EPackExeType Type; // ma to byt 16 nebo 32 bit exe ?
    // pole toho, co jsme nalezli
    CPackACArray Found;                        // a to, co jsme nasli
    CRITICAL_SECTION FoundDataCriticalSection; // kriticka sekce pro pristup k datum

public:
    CPackACPacker(int index, EPackPackerType unpacker, const char* title,
                  const char* name, EPackExeType type) : Found(20, 10)
    {
        Index = index;
        Unpacker = unpacker;
        Title = title;
        Name = name;
        Type = type;
        HANDLES(InitializeCriticalSection(&FoundDataCriticalSection));
    }
    ~CPackACPacker()
    {
        HANDLES(DeleteCriticalSection(&FoundDataCriticalSection));
    }
    int CheckAndInsert(const char* path, const char* fileName, FILETIME lastWriteTime,
                       const CQuadWord& size, EPackExeType type);
    int GetCount();
    const char* GetText(int index, int column);
    BOOL IsTitle(int index) { return index < 0 ? TRUE : FALSE; }
    void InvertSelect(int index);
    int GetSelectState(int index);
    const char* GetSelectedFullName() { return Found.GetSelectedFullName(); }
    int GetArchiverIndex() { return Index; }
    EPackPackerType GetPackerType() { return Unpacker; }
    EPackExeType GetExeType() { return Type; }
};

// Typ tabulky nalezenych programu pro autoconfig
typedef TIndirectArray<CPackACPacker> APackACPackersTable;

//*********************************************************************************
//
// CPackACListView
//
class CPackACDialog;

class CPackACListView : public CWindow
{
protected:
    const CPackACDialog* ACDialog;
    APackACPackersTable* PackersTable;

public:
    CPackACListView(const CPackACDialog* acDialog) : CWindow()
    {
        ACDialog = acDialog;
        PackersTable = NULL;
    }
    virtual ~CPackACListView()
    {
        if (PackersTable != NULL)
            delete PackersTable;
    }

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Initialize(APackACPackersTable* table);
    BOOL FindArchiver(unsigned int listViewIndex,
                      unsigned int* archiver, unsigned int* arcIndex);
    BOOL ConsiderItem(const char* path, const char* fileName, FILETIME lastWriteTime,
                      const CQuadWord& size, EPackExeType type);
    BOOL InitColumns();
    void SetColumnWidth();
    void InvertSelect(int index);
    int GetCount();
    int GetPackersCount() { return PackersTable->Count; }
    CPackACPacker* GetPacker(int item, int* index);
    CPackACPacker* GetPacker(int index)
    {
        return PackersTable != NULL ? PackersTable->At(index) : NULL;
    }
};

//*********************************************************************************
//
// CPackACDialog
//

// dialog pro autoconfig
class CPackACDialog : public CCommonDialog
{
public:
    CPackACDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent, CArchiverConfig* archiverConfig,
                  char** drivesList, CObjectOrigin origin = ooStandard)
        : CCommonDialog(modul, resID, helpID, parent, origin)
    {
        ArchiverConfig = archiverConfig;
        SearchRunning = FALSE;
        WillExit = FALSE;
        DrivesList = drivesList;
        HSearchThread = NULL;
        StopSearch = NULL;
    }

    static DWORD WINAPI PackACDiskSearchThread(LPVOID instance);
    static unsigned int PackACDiskSearchThreadEH(LPVOID instance);
    DWORD DiskSearch();
    BOOL DirectorySearch(char* path);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Transfer(CTransferInfo& ti);
    void LayoutControls();
    void GetLayoutParams();
    void UpdateListViewItems(int index);
    void AddToExtensions(int foundIndex, int packerIndex, CPackACPacker* foundPacker);
    void RemoveFromExtensions(int foundIndex, int packerIndex, CPackACPacker* foundPacker);
    void AddToCustom(int foundIndex, int packerIndex, CPackACPacker* foundPacker);
    void RemoveFromCustom(int foundIndex, int packerIndex);
    BOOL MyGetBinaryType(LPCSTR filename, LPDWORD lpBinaryType);

protected:
    CArchiverConfig* ArchiverConfig;
    CPackACListView* ListView;
    char** DrivesList;
    HANDLE HSearchThread;
    HWND HStatusBar;
    BOOL SearchRunning;
    HANDLE StopSearch;
    BOOL WillExit;
    // data potrebna pro layoutovani dialogu
    int HMargin;  // prostor vlevo a vpravo mezi rameckem dialogu a controly
    int VMargin;  // prostor dole mezi tlacitky a status barou
    int ButtonW1, // rozmery tlacitek (nemusi byt vsechny stejne siroke, napr. DE+HU+CHS verze je maji jinak siroke)
        ButtonW2,
        ButtonW3,
        ButtonW4,
        ButtonW5;
    int ButtonMargin; // prostor mezi tlacitky
    int ButtonH;
    int StatusHeight; // vyska status bary
    int ListY;        // umisteni seznamu vysledku
    int CheckH;       // vyska checkboxu
    int MinDlgW;      // minimalni rozmery dialogu
    int MinDlgH;
};

//*********************************************************************************
//
// CPackACDrives
//

// dialog pro specifikaci prohledavanych disku v autokonfigu
class CPackACDrives : public CCommonDialog
{
public:
    CPackACDrives(HINSTANCE modul, int resID, UINT helpID, HWND parent, char** drivesList,
                  CObjectOrigin origin = ooStandard)
        : CCommonDialog(modul, resID, helpID, parent, origin)
    {
        DrivesList = drivesList;
        EditLB = NULL;
    }

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    CEditListBox* EditLB;
    BOOL Dirty;
    char** DrivesList;
};

//*********************************************************************************
//
//
//

// Typ tabulky chybovych kodu externich pakovacu
typedef int TPackErrorTable[][2];

// Struktura tabulky podporovanych formatu
struct SPackFormat
{
    const char* FileExtension;  // Pripona souboru archivu
    const char* MultiExtension; // Format pripony pri pouziti deleneho archivu
                                // (znak '?' znamena libovolne cislo)
    int ArchiveBrowseIndex;     // Cislo formatu. Zaporne cislo je index formatu
                                // zpracovavaneho interne (DLL), kladne cislo je
                                // index do tabulky externich pakovacu (nemodifikujici cast)
    int ArchiveModifyIndex;     // Cislo formatu. Zaporne cislo je index formatu
                                // zpracovavaneho interne (DLL), kladne cislo je
                                // index do tabulky externich pakovacu (modifikujici cast)
};

// upraveny indirect array, ktery vola delete[]
template <class DATA_TYPE>
class TPackIndirectArray : public TIndirectArray<DATA_TYPE>
{
public:
    TPackIndirectArray(int base, int delta, CDeleteType dt = dtDelete)
        : TIndirectArray<DATA_TYPE>(base, delta, dt) {}

    virtual ~TPackIndirectArray() { this->DestroyMembers(); }

protected:
    virtual void CallDestructor(void*& member)
    {
        if (this->DeleteType == dtDelete && (DATA_TYPE*)member != NULL)
            delete[] ((DATA_TYPE*)member);
    }
};

// typ pro pole radek prectenych z pajpy
typedef TPackIndirectArray<char> CPackLineArray;

// Obecna funkce pro parsovani listingu archivu
typedef BOOL (*FPackList)(const char* archiveFileName, CPackLineArray& lineArray,
                          CSalamanderDirectory& dir);

// konstanty pouzite pro SPackModifyTable::DelEmptyDir
#define PMT_EMPDIRS_DONOTDELETE 0        // neni potreba zvlast mazat prazdny adresar
#define PMT_EMPDIRS_DELETE 1             // je potreba zvlast mazat prazdny adresar - zadani jen cestou
#define PMT_EMPDIRS_DELETEWITHASTERISK 2 // je potreba zvlast mazat prazdny adresar - zadani cestou + '*'

//
// Struktura pro tabulku definic externich pakovacu - modifikujici operace
//
struct SPackModifyTable
{
    //
    // obecne polozky
    //
    TPackErrorTable* ErrorTable; // Ukazatel na tabulku navratovych kodu
    BOOL SupportLongNames;       // TRUE, pokud podporuje dlouhe nazvy souboru

    //
    // polozky pro kompreseni
    //
    const char* CompressInitDir; // adresar, ve kterem bude pusten pakovaci prikaz
    const char* CompressCommand; // prikaz pro zabaleni archivu
    BOOL CanPackToDir;           // TRUE, pokud archivacni program podporuje baleni do adresare

    //
    // polozky pro vymazani z archivu
    //
    const char* DeleteInitDir; // adresar, ve kterem bude pusten mazaci prikaz
    const char* DeleteCommand; // prikaz pro vymazani souboru z archivu
    int DelEmptyDir;           // viz konstanty PMT_EMPDIRS_XXX

    //
    // polozky pro presun do archivu
    //
    const char* MoveInitDir; // adresar, ve kterem bude pusten presunovaci prikaz. Pokud ho neumi, NULL
    const char* MoveCommand; // prikaz pro presun souboru do archivu. Pokud NULL, program neumi move

    BOOL NeedANSIListFile; // ma se list souboru nechat v ANSI (nema se prevest do OEM)
};

// Struktura pro tabulku definic externich pakovacu - nemodifikujici operace
struct SPackBrowseTable
{
    //
    // obecne polozky
    //
    TPackErrorTable* ErrorTable; // Ukazatel na tabulku navratovych kodu
    BOOL SupportLongNames;       // TRUE, pokud podporuje dlouhe nazvy souboru

    //
    // polozky pro listovani archivu
    //
    const char* ListInitDir; // adresar, ve kterem se spusti listovaci prikaz
    const char* ListCommand; // prikaz pro vylistovani obsahu archivu
    FPackList SpecialList;   // pokud neni NULL, je to funkce pro parsovani listingu
                             // v tom pripade nemusi mit nasledujici polozky vyznam
    const char* StartString; // jak zacina posledni radek hlavicky
    int LinesToSkip;         // kolik radek za StartStringem jeste ignorovat
    int AlwaysSkip;          // kolik radek za StartStringem ignorovat vzdy (nehlida se stop string)
    int LinesPerFile;        // kolik radku dat je ve vypisu pro jeden soubor
    const char* StopString;  // jak zacina prvni radek paticky
    unsigned char Separator; // pokud ma archivator extra separator, bude zde (jinak treba mezera)
    // indexy k polozkam vypisu, cislo znamena poradi polozky v radku,
    // (prvni je 1), nula znamena, ze polozka neexistuje
    short NameIdx;  // index jmena na radku ve vypisu
    short SizeIdx;  // index velikosti souboru na radku ve vypisu
    short TimeIdx;  // index casu na radku ve vypisu
    short DateIdx;  // index datumu na radku ve vypisu
    short AttrIdx;  // index atributu na radku ve vypisu
    short DateYIdx; // index roku v datumu (prvni, druhy, nebo treti)
    short DateMIdx; // index mesice v datumu (prvni, druhy, nebo treti)

    //
    // polozky pro dekompreseni
    //
    const char* UncompressInitDir; // adresar, ve kterem se spousti vybalovani archivu
    const char* UncompressCommand; // prikaz pro vybaleni archivu

    //
    // polozky pro vybaleni jednoho souboru bez cesty
    //
    const char* ExtractInitDir; // adresar, ve kterem se spousti vybalovani jednoho souboru
    const char* ExtractCommand; // prikaz pro vybaleni jednoho souboru bez cesty

    BOOL NeedANSIListFile; // ma se list souboru nechat v ANSI (nema se prevest do OEM)
};

// tridy konfigurace preddefinovanych pakovavacu
extern const SPackBrowseTable PackBrowseTable[];
extern const SPackModifyTable PackModifyTable[];

#define ARC_UID_JAR32 1
#define ARC_UID_RAR32 2
#define ARC_UID_ARJ16 3
#define ARC_UID_LHA16 4
#define ARC_UID_UC216 5
#define ARC_UID_JAR16 6
#define ARC_UID_RAR16 7
#define ARC_UID_ZIP32 8
#define ARC_UID_ZIP16 9
#define ARC_UID_ARJ32 10
#define ARC_UID_ACE32 11
#define ARC_UID_ACE16 12

// trida pro drzeni dat
class CArchiverConfigData
{
public:
    DWORD UID;                      // unikatni identifikator archiveru (viz ARC_UID_XXX)
    char* Title;                    // jmeno, pod kterym se v konfiguraci objevi
    const char* PackerVariable;     // jmeno promenne, ktera je expandovana jako pakovac
    const char* UnpackerVariable;   // jmeno promenne, ktera je expandovana jako rozpakovavac (NULL, kdyz je to pakovac)
    const char* PackerExecutable;   // nazev programu pakovace, pro hledani na disku
    const char* UnpackerExecutable; // nazev programu rozpakovavace, nebo NULL
    EPackExeType Type;              // typ archiveru (16bit, 32bit)
    BOOL ExesAreSame;               // true, pokud se PackExeFile pouziva pro pack i unpack
    char* PackExeFile;              // cesta k balicimu programu
    char* UnpackExeFile;            // cesta k rozbalovacimu programu, nebo NULL

public:
    CArchiverConfigData()
    {
        Empty();
    }

    ~CArchiverConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Title != NULL)
            free(Title);
        if (PackExeFile != NULL)
            free(PackExeFile);
        if (UnpackExeFile != NULL)
            free(UnpackExeFile);
        Empty();
    }

    void Empty()
    {
        UID = 0;
        Title = NULL;
        PackerVariable = NULL;
        UnpackerVariable = NULL;
        PackerExecutable = NULL;
        UnpackerExecutable = NULL;
        Type = EXE_END;
        ExesAreSame = FALSE;
        PackExeFile = NULL;
        UnpackExeFile = NULL;
    }

    BOOL IsValid()
    {
        if (Title == NULL || PackerVariable == NULL || PackerExecutable == NULL ||
            Type >= EXE_END || PackExeFile == NULL ||
            (!ExesAreSame && (UnpackerVariable == NULL || UnpackerExecutable == NULL ||
                              UnpackExeFile == NULL)))
            return FALSE;
        return TRUE;
    }
};

// trida s konfiguraci
class CArchiverConfig
{
protected:
    TIndirectArray<CArchiverConfigData> Archivers;

public:
    CArchiverConfig(/*BOOL disableDefaultValues*/);
    void InitializeDefaultValues(); // j.r. nahrazuje puvodni volani konstruktoru
    BOOL Load(CArchiverConfig& src);

    void DeleteAllArchivers() { Archivers.DestroyMembers(); }

    int AddArchiver();                 // vrati index zalozene polozky nebo -1 pri chybe
    void AddDefault(int SalamVersion); // prida archivery nove od verze SalamVersion, pri SalamVersion = -1 prida vsechny (default config)

    // nastavi atributy; kdyz se neco posere, vyradi prvek z pole, zdestroji ho a vrati FALSE
    BOOL SetArchiver(int index, DWORD uid, const char* title, EPackExeType type, BOOL exesAreSame,
                     const char* packerVariable, const char* unpackerVariable,
                     const char* packerExecutable, const char* unpackerExecutable,
                     const char* packExeFile, const char* unpackExeFile);

    int GetArchiversCount() { return Archivers.Count; } // vrati pocet polozek v poli

    DWORD GetArchiverUID(int index) { return Archivers[index]->UID; }
    const char* GetArchiverTitle(int index) { return Archivers[index]->Title; }
    const char* GetPackerVariable(int index) { return Archivers[index]->PackerVariable; }
    const char* GetUnpackerVariable(int index) { return Archivers[index]->UnpackerVariable; }
    const char* GetPackerExecutable(int index) { return Archivers[index]->PackerExecutable; }
    const char* GetUnpackerExecutable(int index) { return Archivers[index]->UnpackerExecutable; }
    EPackExeType GetArchiverType(int index) { return Archivers[index]->Type; }
    void SetPackerExeFile(int index, const char* filename);
    void SetUnpackerExeFile(int index, const char* filename);
    const char* GetPackerExeFile(int index) { return Archivers[index]->PackExeFile; }
    const char* GetUnpackerExeFile(int index) { return Archivers[index]->UnpackExeFile; }
    const SPackModifyTable* GetPackerConfigTable(int index) { return &PackModifyTable[index]; }
    const SPackBrowseTable* GetUnpackerConfigTable(int index) { return &PackBrowseTable[index]; }
    BOOL ArchiverExesAreSame(int index) { return Archivers[index]->ExesAreSame; }
    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);
};

// Trida drzici konfiguraci formatu archivu a asociaci pakovacu k nim.
// Upraveny direct array, ktery zatriduje unikatni stringy podle abecedy.
class CExtItem
{
public:
    CExtItem() { Ext = NULL; }
    ~CExtItem()
    {
        if (Ext != NULL)
            free(Ext);
    }
    char* GetExt() { return Ext; }
    int GetIndex() { return Index; }
    void Set(char* ext, int index)
    {
        Ext = ext;
        Index = index;
    }

protected:
    char* Ext;
    int Index;
};

class CStringArray : public TDirectArray<CExtItem>
{
public:
    CStringArray() : TDirectArray<CExtItem>(2, 3) {}

    BOOL SIns(CExtItem& item)
    {
        if (Count == 0)
        {
            Add(item);
            return TRUE;
        }
        int i = 0;
        while (i < Count && strcmp(At(i).GetExt(), item.GetExt()) > 0)
            i++;
        if (i == Count)
            Add(item);
        else if (!strcmp(At(i).GetExt(), item.GetExt()))
            return FALSE;
        else
            Insert(i, item);
        return TRUE;
    }
};

// datovy prvek
class CPackerFormatConfigData
{
public:
    char* Ext;         // seznam pripon, ktere muze archiv mit
    BOOL UsePacker;    // true, pokud PackerIndex je platny (umime i pakovat)
    int PackerIndex;   // odkaz do tabulky pakovacu
    int UnpackerIndex; // odkaz do tabulky rozpakovavacu

    BOOL OldType; // jde o stara data (verze < 6) - predpoklada interni ZIP+TAR+PAK?

public:
    CPackerFormatConfigData()
    {
        Empty();
    }

    ~CPackerFormatConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Ext != NULL)
            free(Ext);
        Empty();
    }

    void Empty()
    {
        OldType = FALSE;
        Ext = NULL;
        UsePacker = FALSE;
        PackerIndex = 500;
        UnpackerIndex = 500;
    }

    BOOL IsValid()
    {
        if (Ext == NULL || UnpackerIndex == 500 || UsePacker && PackerIndex == 500)
            return FALSE;
        return TRUE;
    }
};

// trida s konfiguraci
class CPackerFormatConfig
{
protected:
    TIndirectArray<CPackerFormatConfigData> Formats;
    CStringArray Extensions[256];

public:
    CPackerFormatConfig(/*BOOL disableDefaultValues*/);
    void InitializeDefaultValues(); // j.r. nahrazuje puvodni volani konstruktoru
    BOOL Load(CPackerFormatConfig& src);

    void DeleteAllFormats() { Formats.DestroyMembers(); }

    int AddFormat();                   // vrati index zalozene polozky nebo -1 pri chybe
    void AddDefault(int SalamVersion); // prida archivery nove od verze SalamVersion, pri SalamVersion = -1 prida vsechny (default config)

    // nastavi atributy; kdyz se neco posere, vyradi prvek z pole, zdestroji ho a vrati FALSE
    BOOL SetFormat(int index, const char* ext, BOOL usePacker,
                   const int packerIndex, const int unpackerIndex, BOOL old);
    void SetOldType(int index, BOOL old) { Formats[index]->OldType = old; }
    void SetUnpackerIndex(int index, int unpackerIndex) { Formats[index]->UnpackerIndex = unpackerIndex; }
    void SetUsePacker(int index, BOOL usePacker) { Formats[index]->UsePacker = usePacker; }
    void SetPackerIndex(int index, int packerIndex) { Formats[index]->PackerIndex = packerIndex; }

    // vytvori data pro vyhledavani; pokud se neco posere, vraci false a muze i radek a sloupek chyby
    BOOL BuildArray(int* line = NULL, int* column = NULL);

    // archiveNameLen slouzi pouze jako optimalizace, pokud je -1, funkce si retezec omeri sama
    int PackIsArchive(const char* archiveName, int archiveNameLen = -1);

    int GetFormatsCount() { return Formats.Count; } // vrati pocet polozek v poli

    //    BOOL SwapFormats(int index1, int index2);         // prohodi dve polozky v poli
    BOOL MoveFormat(int srcIndex, int dstIndex); // posune polozku
    void DeleteFormat(int index);

    int GetUnpackerIndex(int index) { return Formats[index]->UnpackerIndex; }
    BOOL GetUsePacker(int index) { return Formats[index]->UsePacker; }
    int GetPackerIndex(int index) { return Formats[index]->PackerIndex; }
    const char* GetExt(int index) { return Formats[index]->Ext; }
    BOOL GetOldType(int index) { return Formats[index]->OldType; }

    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);
};

// ****************************************************************************
// Promenne
//

extern char SpawnExe[MAX_PATH * 2];
extern BOOL SpawnExeInitialised;

struct CExecuteItem;
extern CExecuteItem ArgsCustomPackers[];
extern CExecuteItem CmdCustomPackers[];

extern CPackerFormatConfig PackerFormatConfig;
extern CArchiverConfig ArchiverConfig;

extern const TPackErrorTable JARErrors;
extern const TPackErrorTable RARErrors;
extern const TPackErrorTable ARJErrors;
extern const TPackErrorTable LHAErrors;
extern const TPackErrorTable UC2Errors;
extern const TPackErrorTable ZIP204Errors;
extern const TPackErrorTable UNZIP204Errors;
extern const TPackErrorTable ACEErrors;
extern BOOL (*PackErrorHandlerPtr)(HWND parent, const WORD errNum, ...);
extern const SPackFormat PackFormat[];

// ****************************************************************************
// Funkce
//

// Inicializace cesty k spawn.exe
BOOL InitSpawnName(HWND parent);

// Nastaveni zpracovani chyb
void PackSetErrorHandler(BOOL (*handler)(HWND parent, const WORD errNum, ...));

// zjisteni obsahu archivu
BOOL PackList(CFilesWindow* panel, const char* archiveFileName, CSalamanderDirectory& dir,
              CPluginDataInterfaceAbstract*& pluginData, CPluginData*& plugin);

// vybaleni pozadovanych souboru z archivu (vola UniversalUncompress)
BOOL PackUncompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* targetDir, const char* archiveRoot,
                    SalEnumSelection nextName, void* param);

// Univerzalni rozbaleni archivu (pro rozbaleni celeho archivu)
BOOL PackUniversalUncompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                             const char* initDir, BOOL expandInitDir, CFilesWindow* panel,
                             const BOOL supportLongNames, const char* archiveFileName,
                             const char* targetDir, const char* archiveRoot,
                             SalEnumSelection nextName, void* param, BOOL needANSIListFile);

// Vybaleni jednoho souboru z archivu (pro viewer)
BOOL PackUnpackOneFile(CFilesWindow* panel, const char* archiveFileName,
                       CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                       CFileData* fileData, const char* targetPath, const char* newFileName,
                       BOOL* renamingNotSupported);

// Zabaleni pozadovanych souboru do archivu (vola UniversalCompress)
BOOL PackCompress(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                  const char* archiveRoot, BOOL move, const char* sourceDir,
                  SalEnumSelection2 nextName, void* param);

// Univerzalni zabaleni archivu (pro zabaleni noveho archivu)
BOOL PackUniversalCompress(HWND parent, const char* command, TPackErrorTable* const errorTable,
                           const char* initDir, BOOL expandInitDir, const BOOL supportLongNames,
                           const char* archiveFileName, const char* sourceDir,
                           const char* archiveRoot, SalEnumSelection2 nextName,
                           void* param, BOOL needANSIListFile);

// Vymazani pozadovanych souboru z archivu
BOOL PackDelFromArc(HWND parent, CFilesWindow* panel, const char* archiveFileName,
                    CPluginDataInterfaceAbstract* pluginData,
                    const char* archiveRoot, SalEnumSelection nextName,
                    void* param);

// Automaticka konfigurace pakovacu
void PackAutoconfig(HWND parent);

// Spousti externi program cmdLine a rozlisuje navratovy kod podle errorTable
BOOL PackExecute(HWND parent, char* cmdLine, const char* currentDir, TPackErrorTable* const errorTable);

// Callback pro enumeraci podle masky (kvuli vybaleni vsech souboru podle masky)
const char* WINAPI PackEnumMask(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                const CFileData** fileData, void* param, int* errorOccured);

// provadi nahradu promennych v retezci prikazove radky
BOOL PackExpandCmdLine(const char* archiveName, const char* tgtDir, const char* lstName,
                       const char* extName, const char* varText, char* buffer,
                       const int bufferLen, char* DOSTmpName);

// provadi nahradu promennych v retezci aktualniho adresare
BOOL PackExpandInitDir(const char* archiveName, const char* srcDir, const char* tgtDir,
                       const char* varText, char* buffer, const int bufferLen);

// Defaultni funkce pro zpracovani chyb - dela jen TRACE_E
BOOL EmptyErrorHandler(HWND parent, const WORD err, ...);

// Funkce pro parsovani vystupu z UC2 pakovace
BOOL PackUC2List(const char* archiveFileName, CPackLineArray& lineArray,
                 CSalamanderDirectory& dir);
