// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// struktura pro pridavani hlasek do Find Logu, posila se jako parametr zpravy
// WM_USER_ADDLOG; parametry budou okopirovany do dat logu (po navratu je lze dealokovat)
#define FLI_INFO 0x00000000   // jde o INFORMATION
#define FLI_ERROR 0x00000001  // jde o ERROR
#define FLI_IGNORE 0x00000002 // povolit Ignore tlacitko na teto polozce
struct FIND_LOG_ITEM
{
    DWORD Flags;      // FLI_xxx
    const char* Text; // text zpravy, musi byt ruzny od NULL
    const char* Path; // cesta k souboru nebo adresari, muze byt NULL
};

#define WM_USER_ADDLOG WM_APP + 210     // prida polozku do logu [FIND_LOG_ITEM *item, 0]
#define WM_USER_ADDFILE WM_APP + 211    // [0, 0]
#define WM_USER_SETREADING WM_APP + 212 // [0, 0] - zadost o prekresleni status-bary ("Reading:")
#define WM_USER_BUTTONS WM_APP + 213    // zavolej si EnableButtons() [HWND hButton]
#define WM_USER_FLASHICON WM_APP + 214  // po aktivaci findu mame zablikat stavovou ikonkou

extern BOOL IsNotAlpha[256];

#define ITEMNAME_TEXT_LEN MAX_PATH + MAX_PATH + 10
#define NAMED_TEXT_LEN MAX_PATH  // maximalni velikost textu v comboboxu
#define LOOKIN_TEXT_LEN MAX_PATH // maximalni velikost textu v comboboxu
#define GREP_TEXT_LEN 201        // maximalni velikost textu v comboboxu; POZOR: melo by byt shodne s FIND_TEXT_LEN
#define GREP_LINE_LEN 10000      // max. delka radky pro reg. expr. (viewer ma jine makro)

// delka mapovaneho view souboru; musi byt vetsi nez delka radky pro regexp + EOL +
// AllocationGranularity
#define VOF_VIEW_SIZE 0x2800400 // 40 MB (vic radsi ne, nemusel by byt k dispozici virt. prostor) + 1 KB (rezerva pro rozumnou text. radku)

// historie pro combobox Named
#define FIND_NAMED_HISTORY_SIZE 30 // pocet pamatovanych stringu
extern char* FindNamedHistory[FIND_NAMED_HISTORY_SIZE];

// historie pro combobox LookIn
#define FIND_LOOKIN_HISTORY_SIZE 30 // pocet pamatovanych stringu
extern char* FindLookInHistory[FIND_LOOKIN_HISTORY_SIZE];

// historie pro combobox Containing
#define FIND_GREP_HISTORY_SIZE 30 // pocet pamatovanych stringu
extern char* FindGrepHistory[FIND_GREP_HISTORY_SIZE];

extern BOOL FindManageInUse; // je otevreny Manage dialog?
extern BOOL FindIgnoreInUse; // je otevreny Ignore dialog?

BOOL InitializeFind();
void ReleaseFind();

// promazne vsechny historie Findu; pokud je 'dataOnly' == TRUE, nebudou se
// promazavat combboxy otevrenych oken
void ClearFindHistory(BOOL dataOnly);

DWORD WINAPI GrepThreadF(void* ptr); // telo grep-threadu

extern HACCEL FindDialogAccelTable;

class CFoundFilesListView;
class CFindDialog;
class CMenuPopup;
class CMenuBar;

//*********************************************************************************
//
// CSearchForData
//

struct CSearchForData
{
    char Dir[MAX_PATH];
    CMaskGroup MasksGroup;
    BOOL IncludeSubDirs;

    CSearchForData(const char* dir, const char* masksGroup, BOOL includeSubDirs)
    {
        Set(dir, masksGroup, includeSubDirs);
    }

    void Set(const char* dir, const char* masksGroup, BOOL includeSubDirs);
    const char* GetText(int i)
    {
        switch (i)
        {
        case 0:
            return MasksGroup.GetMasksString();
        case 1:
            return Dir;
        default:
            return IncludeSubDirs ? LoadStr(IDS_INCLUDESUBDIRSYES) : LoadStr(IDS_INCLUDESUBDIRSNO);
        }
    }
};

//*********************************************************************************
//
// CSearchingString
//
// synchronizovany buffer pro "Searching" text ve status bare Find dialogu

class CSearchingString
{
protected:
    char Buffer[MAX_PATH + 50];
    int BaseLen;
    BOOL Dirty;
    CRITICAL_SECTION Section;

public:
    CSearchingString();
    ~CSearchingString();

    // nastavi zaklad, ke kteremu pomoci Set pripojuje dalsi text + dirty na FALSE
    void SetBase(const char* buf);
    // pripojovani k zakladu nastavenemu pres SetBase
    void Set(const char* buf);
    // vraci komplet string
    void Get(char* buf, int bufSize);

    // nastavuje dirty (ceka uz se na prekresleni?)
    void SetDirty(BOOL dirty);
    // vraci TRUE pokud uz se ceka na prekresleni
    BOOL GetDirty();
};

//*********************************************************************************
//
// CGrepData
//

// flagy pro hledeni identickych souboru
// alespon _NAME nebo _SIZE bude specifikovano
// _CONTENT muze byt nastaveno pouze je-li nastaveno take _SIZE
#define FIND_DUPLICATES_NAME 0x00000001    // same name
#define FIND_DUPLICATES_SIZE 0x00000002    // same size
#define FIND_DUPLICATES_CONTENT 0x00000004 // same content

struct CGrepData
{
    BOOL FindDuplicates; // hledame duplikaty?
    DWORD FindDupFlags;  // FIND_DUPLICATES_xxx; ma vyznam je-li 'FindDuplicates' TRUE
    int Refine;          // 0: hledat nova data, 1 & 2: hledat nad nalezenyma datama; 1: intersect with old data; 2: subtract from old data
    BOOL Grep;           // grepovat ?
    BOOL WholeWords;     // jen cela slova ?
    BOOL Regular;        // regularni vyraz ?
    BOOL EOL_CRLF,       // EOLy pri hledani regularnich vyrazu
        EOL_CR,
        EOL_LF;
    //       EOL_NULL;              // na to nemam regexp :(

    CSearchData SearchData;
    CRegularExpression RegExp;
    // advanced search
    DWORD AttributesMask;  // nejprve vymaskujem
    DWORD AttributesValue; // pak porovname
    CFilterCriteria Criteria;
    // rizeni + data
    BOOL StopSearch;    // nastavuje hl. thread pro ukonceni grep threadu
    BOOL SearchStopped; // byl terminovan nebo ne ?
    HWND HWindow;       // s kym grep thread komunikuje
    TIndirectArray<CSearchForData>* Data;
    CFoundFilesListView* FoundFilesListView; // sem ladujeme nalezene soubory
    // dve kriteria pro update listview
    int FoundVisibleCount;  // pocet polozek zobrazenych v listview
    DWORD FoundVisibleTick; // kdy bylo naposled zobrazovano
    BOOL NeedRefresh;       // je potreba refreshnout zobrazeni (pribyla polozka bez zobrazeni)

    CSearchingString* SearchingText;  // synchronizovany text "Searching" ve status-bare Findu
    CSearchingString* SearchingText2; // [optional] druhy text vpravo; pouzivame pro "Total: 35%"
};

//*********************************************************************************
//
// CFindOptionsItem
//

class CFindOptionsItem
{
public:
    // Internal
    char ItemName[ITEMNAME_TEXT_LEN];

    CFilterCriteria Criteria;

    // Find dialog
    int SubDirectories;
    int WholeWords;
    int CaseSensitive;
    int HexMode;
    int RegularExpresions;

    BOOL AutoLoad;

    char NamedText[NAMED_TEXT_LEN];
    char LookInText[LOOKIN_TEXT_LEN];
    char GrepText[GREP_TEXT_LEN];

public:
    CFindOptionsItem();
    // !POZOR! jakmile bude objekt obsahovat alokovana data, prestane fungovat
    // kod pro presouvani polozek v Options Manageru, kde dochazi k destrukci tmp prvku

    CFindOptionsItem& operator=(const CFindOptionsItem& s);

    // na zaklade NamedText a LookInText postavi nazev polozky (ItemName)
    void BuildItemName();

    // !!! POZOR: ukladani optimalizovano, ukladaji se pouze zmenene hodnoty; pred ulozenim do klice musi by tento klic napred promazan
    BOOL Save(HKEY hKey);                   // ulozi itemu
    BOOL Load(HKEY hKey, DWORD cfgVersion); // nacte itemu
};

//*********************************************************************************
//
// CFindOptions
//

class CFindOptions
{
protected:
    TIndirectArray<CFindOptionsItem> Items;

public:
    CFindOptions();

    BOOL Save(HKEY hKey);                   // ulozi cele pole
    BOOL Load(HKEY hKey, DWORD cfgVersion); // nacte cele pole

    BOOL Load(CFindOptions& source);

    int GetCount() { return Items.Count; }
    BOOL Add(CFindOptionsItem* item);
    CFindOptionsItem* At(int i) { return Items[i]; }
    void Delete(int i) { Items.Delete(i); }

    // vycisti predchozi vlozene polozky a naboucha tam nove
    void InitMenu(CMenuPopup* popup, BOOL enabled, int originalCount);
};

//*********************************************************************************
//
// CFindIgnore
//

enum CFindIgnoreItemType
{
    fiitUnknow,
    fiitFull,     // Plna cesta vcetne rootu: 'C:\' 'D:\TMP\' \\server\share\'
    fiitRooted,   // Cesta zacinajici v libovolnem rootu
    fiitRelative, // Cesta bez rootu: 'aaa' 'aaa\bbbb\ccc'
};

class CFindIgnoreItem
{
public:
    BOOL Enabled;
    char* Path;

    // nasledujici data se neukladaji, inicializuji se v Prepare()
    CFindIgnoreItemType Type;
    int Len;

public:
    CFindIgnoreItem();
    ~CFindIgnoreItem();
};

// Objekt CFindIgnore slouzi dvema zpusoby:
// 1. Globalni objekt drzici seznam cest, editovatelnyc z Find/Options/Ignore Directory List
// 2. Docasna kopie tohoto pole za ucelem hledani -- obsahuje pouze Enabled polozky, ty jsou
//    navic upraveny (pridany zpetna lomitka) a kvalifikovany (nastavena CFindIgnoreItem::Type)
class CFindIgnore
{
protected:
    TIndirectArray<CFindIgnoreItem> Items;

public:
    CFindIgnore();

    BOOL Save(HKEY hKey);                   // ulozi cele pole
    BOOL Load(HKEY hKey, DWORD cfgVersion); // nacte cele pole

    // vola se pro lokalni kopii objekt, ktera se nasledne pouziva pro hledani
    // je treba zavolat pred volanim Contains()
    // z 'source' nakopiruje polozky a pripravi pro hledani
    // vraci TRUE, pokud se zadarilo, jinak vraci FALSE
    BOOL Prepare(CFindIgnore* source);

    // vraci TRUE, pokud seznam obashuje polozku odpovidajici ceste 'path'
    // vyhodnocuji se pouze 'Enabled'==TRUE polozky
    // pokud takovou polozku nenajde, vraci FALSE
    // !pozor! do metody musi vstupovat plna cesta s lomitkem na konci
    BOOL Contains(const char* path, int startPathLen);

    // prida cestu pouze v pripade, ze jiz v seznamu neexistuje
    BOOL AddUnique(BOOL enabled, const char* path);

protected:
    void DeleteAll();
    void Reset(); // sestreli existujici polozky, prida default hodnoty

    BOOL Load(CFindIgnore* source);

    int GetCount() { return Items.Count; }
    BOOL Add(BOOL enabled, const char* path);
    BOOL Set(int index, BOOL enabled, const char* path);
    CFindIgnoreItem* At(int i) { return Items[i]; }
    void Delete(int i) { Items.Delete(i); }
    BOOL Move(int srcIndex, int dstIndex);

    friend class CFindIgnoreDialog;
};

//*********************************************************************************
//
// CFindAdvancedDialog
//
/*
class CFindAdvancedDialog: public CCommonDialog
{
  public:
    BOOL             SetDateAndTime;
    CFindOptionsItem *Data;

  public:
    CFindAdvancedDialog(CFindOptionsItem *data);

    int Execute();
    virtual void Validate(CTransferInfo &ti);
    virtual void Transfer(CTransferInfo &ti);
    void EnableControls();   // zajistuje disable/enable operace
    void LoadTime();

  protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/
//*********************************************************************************
//
// CFindManageDialog
//

class CFindManageDialog : public CCommonDialog
{
protected:
    CEditListBox* EditLB;
    CFindOptions* FO;
    const CFindOptionsItem* CurrenOptionsItem;

public:
    CFindManageDialog(HWND hParent, const CFindOptionsItem* currenOptionsItem);
    ~CFindManageDialog();

    virtual void Transfer(CTransferInfo& ti);
    void LoadControls();

    BOOL IsGood() { return FO != NULL; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*********************************************************************************
//
// CFindIgnoreDialog
//

class CFindIgnoreDialog : public CCommonDialog
{
protected:
    CEditListBox* EditLB;
    CFindIgnore* IgnoreList; // nase pracovni kopie dat
    CFindIgnore* GlobalIgnoreList;
    BOOL DisableNotification;
    HICON HChecked;
    HICON HUnchecked;

public:
    CFindIgnoreDialog(HWND hParent, CFindIgnore* globalIgnoreList);
    ~CFindIgnoreDialog();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    BOOL IsGood() { return IgnoreList != NULL; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void FillList();
};

//****************************************************************************
//
// CFindDuplicatesDialog
//

class CFindDuplicatesDialog : public CCommonDialog
{
public:
    // nastaveni si budeme pamatovat po dobu behu Salamandera
    static BOOL SameName;
    static BOOL SameSize;
    static BOOL SameContent;

public:
    CFindDuplicatesDialog(HWND hParent);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls(); // zajistuje disable/enable operace
};

//*********************************************************************************
//
// CFindLog
//
// Slouzi k drzeni chyb, ktere nastaly behem hledani.
//

struct CFindLogItem
{
    DWORD Flags;
    char* Text;
    char* Path;
};

class CFindLog
{
protected:
    TDirectArray<CFindLogItem> Items;
    int SkippedErrors; // pocet chyb, ktere jsme neukladali
    int ErrorCount;
    int InfoCount;

public:
    CFindLog();
    ~CFindLog();

    void Clean(); // uvolni vsechny drzene polozky

    BOOL Add(DWORD flags, const char* text, const char* path);
    int GetCount() { return Items.Count; }
    int GetSkippedCount() { return SkippedErrors; }
    const CFindLogItem* Get(int index);
    int GetErrorCount() { return ErrorCount; }
    int GetInfoCount() { return InfoCount; }
};

//*********************************************************************************
//
// CFindLogDialog
//
// Slouzi k zobrazni chyb, ktere nastaly behem hledani.
//

class CFindLogDialog : public CCommonDialog
{
protected:
    CFindLog* Log;
    HWND HListView;

public:
    CFindLogDialog(HWND hParent, CFindLog* log);

    virtual void Transfer(CTransferInfo& ti);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnFocusFile();
    void OnIgnore();
    void EnableControls();
    const CFindLogItem* GetSelectedItem();
};

//*********************************************************************************
//
// CFoundFilesData
// CFoundFilesListView
//

#define MD5_DIGEST_SIZE 16

struct CMD5Digest
{
    BYTE Digest[MD5_DIGEST_SIZE];
};

struct CFoundFilesData
{
    char* Name;
    char* Path;
    CQuadWord Size;
    DWORD Attr;
    FILETIME LastWrite;

    // 'Group' se vyuziva dvema zpusoby:
    // 1) behem hledani duplicitnich souboru v pripade, ze se porovana obsah,
    //    obsahuje ukazatel na CMD5Digest s napocitanou MD5 pro soubor
    // 2) pred predanim vysledku hledani duplicitnich souboru do ListView
    //    obsahuje cislo spojujici vice souboru do ekvivaletni skupiny
    DWORD_PTR Group;

    unsigned IsDir : 1; // 0 - polozka je soubor, 1 - polozka je adresar
    // Selected a Focused maji vyznam pouze lokalne pro StoreItemsState/RestoreItemsState
    unsigned Selected : 1; // 0 - polozka neoznacena, 1 - polozka oznacena
    unsigned Focused : 1;  // 0 - polozka je focused, 1 - polozka neni focused
    // Different se pouziva k rozliseni skupin souboru pri hledani duplicit
    unsigned Different : 1; // 0 - polozka ma klasicke bile pozadi, 1 - polozka me jine pozadi (pouziva se pri hledani diferenci)

    CFoundFilesData()
    {
        Path = NULL;
        Name = NULL;
        Attr = 0;
        ZeroMemory(&LastWrite, sizeof(LastWrite));
        Group = 0;
        IsDir = 0;
        Selected = 0;
        Different = 0;
    }
    ~CFoundFilesData()
    {
        if (Path != NULL)
            free(Path);
        if (Name != NULL)
            free(Name);
    }
    BOOL Set(const char* path, const char* name, const CQuadWord& size, DWORD attr,
             const FILETIME* lastWrite, BOOL isDir);
    // v pripade Name nebo Path vrati ukazatel na prislusnou promennou
    // jinak naplmi buffer 'text' (musi byt nejmene 50 znaku dlouhy) odpovidajici hodnotou
    // a vrati ukazatel na 'text'
    // 'fileNameFormat' urcuje formatovani jmena nalezenych polozek
    char* GetText(int i, char* text, int fileNameFormat);
};

class CFoundFilesListView : public CWindow
{
protected:
    TIndirectArray<CFoundFilesData> Data;
    CRITICAL_SECTION DataCriticalSection; // kriticka sekce pro pristup k datum
    CFindDialog* FindDialog;
    TIndirectArray<CFoundFilesData> DataForRefine;

public:
    int EnumFileNamesSourceUID; // UID zdroje pro enumeraci jmen ve viewerech

public:
    CFoundFilesListView(HWND dlg, int ctrlID, CFindDialog* findDialog);
    ~CFoundFilesListView();

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL InitColumns();

    void StoreItemsState();
    void RestoreItemsState();

    int CompareFunc(CFoundFilesData* f1, CFoundFilesData* f2, int sortBy);
    void QuickSort(int left, int right, int sortBy);
    void SortItems(int sortBy);

    void QuickSortDuplicates(int left, int right, BOOL byName);
    int CompareDuplicatesFunc(CFoundFilesData* f1, CFoundFilesData* f2, BOOL byName);
    void SetDifferentByGroup(); // nastavi bit Different podle Group tak, aby se bit Different stridal na rozhrani skupin

    // interface pro Data
    CFoundFilesData* At(int index);
    void DestroyMembers();
    int GetCount();
    int Add(CFoundFilesData* item);
    void Delete(int index);
    BOOL IsGood();
    void ResetState();

    // ze seznamu Data pretaha potrebne casti do seznamu DataForRefine
    // smi byt volano pouze pokud nebezi hledaci thread
    BOOL TakeDataForRefine();
    void DestroyDataForRefine();
    int GetDataForRefineCount();
    CFoundFilesData* GetDataForRefine(int index);

    DWORD GetSelectedListSize();                     // vrati pocet bajtu, ktery bude potreba pro ulozeni vsech vybranych
                                                     // polozek ve tvaru "c:\\bla\\bla.txt\\0c:\\bla\\bla2.txt\0\0"
                                                     // pokud neni vybrana zadna polozka, vraci 2 (dva terminatory)
    BOOL GetSelectedList(char* list, DWORD maxSize); // naplni list seznamem dle GetSelectedListSize
                                                     // neprekroci maxSize

    // probehne vsechny selected soubory a adresare a pokud uz neexistuji, vyradi je
    // je-li nastavena promenna 'forceRemove', budou vyrazeny vybrane polozky bez kontroly
    // 'lastFocusedIndex' rika index, ktery byl 'focused' nez doslo k "mazani"
    // 'lastFocusedItem' ukazuje na kopii dat 'focused' polozky, abychom se ji mohli pokusit dohledat polde nazvu a cesty
    void CheckAndRemoveSelectedItems(BOOL forceRemove, int lastFocusedIndex, const CFoundFilesData* lastFocusedItem);
};

//****************************************************************************
//
// CFindTBHeader
//

class CToolBar;

class CFindTBHeader : public CWindow
{
protected:
    CToolBar* ToolBar;
    CToolBar* LogToolBar;
    HWND HNotifyWindow; // kam posilam comandy
    char Text[200];
    int FoundCount;
    int ErrorsCount;
    int InfosCount;
    HICON HWarningIcon;
    HICON HInfoIcon;
    HICON HEmptyIcon;
    BOOL WarningDisplayed;
    BOOL InfoDisplayed;
    int FlashIconCounter;
    BOOL StopFlash;

public:
    CFindTBHeader(HWND hDlg, int ctrlID);

    void SetNotifyWindow(HWND hWnd) { HNotifyWindow = hWnd; }

    int GetNeededHeight();

    BOOL EnableItem(DWORD position, BOOL byPosition, BOOL enabled);

    void SetFoundCount(int foundCount);
    void SetErrorsInfosCount(int errorsCount, int infosCount);

    void OnColorsChange();

    BOOL CreateLogToolbar(BOOL errors, BOOL infos);

    void StartFlashIcon();
    void StopFlashIcon();

    void SetFont();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*********************************************************************************
//
// CFindDialog
//

enum CCopyNameToClipboardModeEnum
{
    cntcmFullName,
    cntcmName,
    cntcmFullPath,
    cntcmUNCName,
};

enum CStateOfFindCloseQueryEnum
{
    sofcqNotUsed,     // klid, nic se nedeje
    sofcqSentToFind,  // poslali jsme pozadavek, jeste nedorazil nebo user jeste neodpovedel na "stop searching?"
    sofcqCanClose,    // muzeme zavrit Find okno
    sofcqCannotClose, // nemuzeme zavrit Find okno
};

class CComboboxEdit;
class CButton;

class CFindDialog : public CCommonDialog
{
protected:
    // data potrebna pro layoutovani dialogu
    BOOL FirstWMSize;
    int VMargin; // prostor vlevo a vpravo mezi rameckem dialogu a controly
    int HMargin; // prostor dole mezi tlacitky a status barou
    int ButtonW; // rozmery tlacitka
    int ButtonH;
    int RegExpButtonW; // rozmery tlacitka RegExpBrowse
    int RegExpButtonY; // umisteni tlacitka RegExpBrowse
    int MenuBarHeight; // vyska menu bary
    int StatusHeight;  // vyska status bary
    int ResultsY;      // umisteni seznamu vysledku
    int AdvancedY;     // umisteni tlacitka Advanced
    int AdvancedTextY; // umisteni textu za tlacitkem Advanced
    int AdvancedTextX; // umisteni textu za tlacitkem Advanced
    int FindTextY;     // Umisteni headru nad vysledkama
    int FindTextH;     // vyska headru
    int CombosX;       // umisteni comboboxu
    int CombosH;       // vyska comboboxu
    int BrowseY;       // umisteni tlacitka Browse
    int Line2X;        // umisteni oddelovaci cary u Search file content
    int FindNowY;      // umisteni tlacitka Find now
    int SpacerH;       // vyska o kterou budeme zkracovat/prodluzovat dialog

    BOOL Expanded; // dialog je roztazeny - jsou zobrazeny polozky SearchFileContent

    int MinDlgW; // minimalni rozmery Find dialogu
    int MinDlgH;

    int FileNameFormat; // jak upravit filename po nacteni z disku, prebirame z globalni konfigurace, kvuli problemum se synchronizaci
    BOOL SkipCharacter; // zabrani pipnuti pri Alt+Enter ve findu

    // dalsi data
    BOOL DlgFailed;
    CMenuPopup* MainMenu;
    CMenuBar* MenuBar;
    HWND HStatusBar;
    HWND HProgressBar; // child okno status bary, zobrazene pro nektere operace ve zvlastnim policku
    BOOL TwoParts;     // ma status bar dva texty?
                       //    CFindAdvancedDialog FindAdvanced;
    CFoundFilesListView* FoundFilesListView;
    char FoundFilesDataTextBuffer[MAX_PATH]; // pro ziskavani textu z CFoundFilesData::GetText
    CFindTBHeader* TBHeader;
    BOOL SearchInProgress;
    BOOL CanClose; // je mozne zavrit okno (nejsme v metode tohoto objektu)
    HANDLE GrepThread;
    CGrepData GrepData;
    CSearchingString SearchingText;
    CSearchingString SearchingText2;
    CComboboxEdit* EditLine;
    BOOL UpdateStatusBar;
    IContextMenu2* ContextMenu;
    CFindDialog** ZeroOnDestroy; // hodnota, ukazatele bude pri destrukci nulovana
    CButton* OKButton;

    BOOL OleInitialized;

    TIndirectArray<CSearchForData> SearchForData; // seznam adresaru a masek, ktere budou prohledany

    // jedna polozka, nad kterou pracuji Find dialog a Advanced dialog
    CFindOptionsItem Data;

    BOOL ProcessingEscape; // message loop prave maka na ESCAPE -- pokud bude nagenerovano
                           // IDCANCEL, zobrazime konfirmaci

    CFindLog Log; // uloziste chyb a informaci

    CBitmap* CacheBitmap; // pro vykreslovani cesty

    BOOL FlashIconsOnActivation; // az nas aktivuji, nechame zablikat stavove ikonky

    char FindNowText[100];

public:
    CStateOfFindCloseQueryEnum StateOfFindCloseQuery; // hl. thread zjistuje v threadu Findu jestli muze zavrit okno, nesynchronizovane, pouziva se jen pri shutdownu, staci bohate...

public:
    CFindDialog(HWND hCenterAgainst, const char* initPath);
    ~CFindDialog();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    BOOL IsGood() { return EditLine != NULL; }

    void SetZeroOnDestroy(CFindDialog** zeroOnDestroy) { ZeroOnDestroy = zeroOnDestroy; }

    BOOL GetFocusedFile(char* buffer, int bufferLen, int* viewedIndex /* muze byt NULL */);
    const char* GetName(int index);
    const char* GetPath(int index);
    void UpdateInternalViewerData();

    BOOL IsSearchInProgress() { return SearchInProgress; }

    void OnEnterIdle();

    // If the message is translated, the return value is TRUE.
    BOOL IsMenuBarMessage(CONST MSG* lpMsg);

    // pro vybrane polozky alokuje prislusna data
    HGLOBAL CreateHDrop();
    HGLOBAL CreateShellIdList();

    // hlavni okno vola tuto metodu vsem Find dialogum - doslo ke zmene barev
    void OnColorsChange();

    void SetProcessingEscape(BOOL value) { ProcessingEscape = value; }

    // umoznuje zpracovat Alt+C a dalsi horke klavesy, ktere patri prave
    // schovanym controlum: pokud je dialog zabalen a jedna se o horkou
    // klavesu skrytych controlu, rozbali ho
    // vzdy vraci FALSE
    BOOL ManageHiddenShortcuts(const MSG* msg);

protected:
    void GetLayoutParams();
    void LayoutControls(); // rozlozi prvky do plochy okna

    void SetTwoStatusParts(BOOL two, BOOL force = FALSE); // nastavi jednu nebo dve casti pro status bar; rozmery nastavuje podle delky status bar

    void SetContentVisible(BOOL visible);
    void UpdateAdvancedText();

    void LoadControls(int index); // z pole Items vytahne prislusnou polozku a nacpe ji do dialogu

    void StartSearch(WORD command);
    void StopSearch();

    void BuildSerchForData(); // naplni seznam SearchForData

    void EnableControls(BOOL nextIsButton = FALSE);
    void EnableToolBar();

    void InsertDrives(HWND hEdit, BOOL network); // naleje do hEdit seznam fixed disku (pripadne i network disku)

    void UpdateListViewItems();

    void OnContextMenu(int x, int y);
    void OnFocusFile();
    void OnViewFile(BOOL alternate);
    void OnEditFile();
    void OnViewFileWith();
    void OnEditFileWith();
    void OnHideSelection();
    void OnHideDuplicateNames();
    void OnDelete(BOOL toRecycle);
    void OnSelectAll();
    void OnInvertSelection();
    void OnShowLog();
    void OnOpen(BOOL onlyFocused);
    void UpdateStatusText(); // pokud nejsme v search modu, informuje o poctu a velikosti vybranych polozek

    // OLE clipboard operations

    // pro vybrane polozky vytvori context menu a vola ContextMenuInvoke pro specifikovany lpVerb
    // vraci TRUE, pokud byla zavolana Invoke, jinak pokud neco selze vraci FALSE
    BOOL InvokeContextMenu(const char* lpVerb);

    void OnCutOrCopy(BOOL cut);
    void OnDrag(BOOL rightMouseButton);

    void OnProperties();

    void OnUserMenu();

    void OnCopyNameToClipboard(CCopyNameToClipboardModeEnum mode);

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // obehne vybrane polozky v listview a pokusi se najit spolecny nadadresar
    // pokud ho najde, nakopci hodo bufferu a vrati TRUE
    // pokud ho nenajde nebo je buffer maly, vrati FALSE
    BOOL GetCommonPrefixPath(char* buffer, int bufferMax, int& commonPrefixChars);

    BOOL InitializeOle();
    void UninitializeOle();

    BOOL CanCloseWindow();

    BOOL DoYouWantToStopSearching();

    void SetFullRowSelect(BOOL fullRow);

    friend class CFoundFilesListView;
};

class CFindDialogQueue : public CWindowQueue
{
public:
    CFindDialogQueue(const char* queueName) : CWindowQueue(queueName) {}

    void AddToArray(TDirectArray<HWND>& arr);
};

//*********************************************************************************
//
// externs
//

BOOL OpenFindDialog(HWND hCenterAgainst, const char* initPath);

extern CFindOptions FindOptions;
extern CFindIgnore FindIgnore;
extern CFindDialogQueue FindDialogQueue; // seznam vsech Find dialogu
