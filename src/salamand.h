// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// InitializeGraphics
//
// Inicializuje sdilene GDI objekty vyuzivane pro chod Salamandera.
// Vola se pred otevrenim hlavniho okna s firstRun==TRUE, colorsOnly==FALSE,
// a fonts==TRUE.
//
// Pokud behem behu aplikace dojde ke zmene barev nebo systemoveho nastaveni,
// je funkce volana s parametrem firstRun==FALSE
//

BOOL InitializeGraphics(BOOL colorsOnly);
void ReleaseGraphics(BOOL colorsOnly);

// inicializace objektu, ktere se nemeni se zmenou barev nebo rozliseni
BOOL InitializeConstGraphics();
void ReleaseConstGraphics();

class CMenuPopup;

//
// ****************************************************************************

class CFilesArray : public TDirectArray<CFileData>
{
protected:
    BOOL DeleteData; // ma volat destruktory rusenych prvku?

public:
    // j.r. zvetsuji deltu na 800, protoze pri vstupu do vetsich adresaru (nekolik tisic souboru)
    // zacina Enlarge() podle profileru celkem zrat CPU
    CFilesArray(int base = 200, int delta = 800) : TDirectArray<CFileData>(base, delta) { DeleteData = TRUE; }
    ~CFilesArray() { Destroy(); }

    void SetDeleteData(BOOL deleteData) { DeleteData = deleteData; }

    void DestroyMembers()
    {
        if (DeleteData)
            TDirectArray<CFileData>::DestroyMembers();
        else
            TDirectArray<CFileData>::DetachMembers();
    }

    void Destroy()
    {
        if (!DeleteData)
            DetachMembers();
        TDirectArray<CFileData>::Destroy();
    }

    void Delete(int index)
    {
        if (DeleteData)
            TDirectArray<CFileData>::Delete(index);
        else
            TDirectArray<CFileData>::Detach(index);
    }

    virtual void CallDestructor(CFileData& member)
    {
#ifdef _DEBUG
        if (!DeleteData)
            TRACE_E("Unexpected situation in CFilesArray::CallDestructor()");
#endif // _DEBUG
        free(member.Name);
        if (member.DosName != NULL)
            free(member.DosName);
    }
};

//****************************************************************************
//
// CNames
//
// pole alokovanych retezcu, lze je abecedne seradit, a pak v nich hledat retezec
// (pulenim intervalu)
//

class CNames
{
protected:
    TDirectArray<char*> Dirs;
    TDirectArray<char*> Files;
    BOOL CaseSensitive;
    BOOL NeedSort; // hlidac spravneho pouzivani tridy

public:
    CNames();
    ~CNames();

    // vyprazdni a dealokuje obe pole
    void Clear();

    // nastavuje chovani metod Sort a Contains; pokud je caseSensitive == TRUE,
    // budou rozlisovany jmena s lisici se pouze velikosti pismen
    void SetCaseSensitive(BOOL caseSensitive);

    // kopiiruje si obsah 'name' do vlastniho bufferu
    // prida ho do seznamu (do Dirs pokud je 'nameIsDir' TRUE, jinak do Files)
    // vrati TRUE v pripade uspechu, jinak FALSE
    BOOL Add(BOOL nameIsDir, const char* name);

    // seradi seznamy Dirs a Files, aby bylo mozne volat Contains();
    void Sort();

    // vraci TRUE, pokud nazev specifikovany pres 'nameIsDir' a 'name'; pokud 'foundOnIndex'
    // neni NULL, vraci se v nem index, na kterem byla polozka nalezena
    // je obsazen v jednom z poli
    BOOL Contains(BOOL nameIsDir, const char* name, int* foundOnIndex = NULL);

    // vraci celkovy pocet drzenych jmen
    int GetCount() { return Dirs.Count + Files.Count; }
    // vraci pocet drzenych adresaru
    int GetDirsCount() { return Dirs.Count; }
    // vraci pocet drzenych souboru
    int GetFilesCount() { return Files.Count; }

    // nacte seznam jmen z textu na clipboardu; Dirs budou prazdne, vse napada do Files
    // hWindow je pro OpenClipboard (netusim, jestli je nutny)
    BOOL LoadFromClipboard(HWND hWindow);
};

//
// ****************************************************************************

class CPathHistoryItem
{
protected:
    int Type;                             // typ: 0 je disk, 1 je archiv, 2 je FS
    char* PathOrArchiveOrFSName;          // diskova cesta nebo jmeno archivu nebo jmeno FS
    char* ArchivePathOrFSUserPart;        // cesta v archivu nebo user-part FS cesty
    HICON HIcon;                          // ikona odpovidajici ceste (muze byt NULL); v destruktoru bude ikona sestrelena
    CPluginFSInterfaceAbstract* PluginFS; // jen pro Type==2: posledni pouzivany interface pro FS cestu

    int TopIndex;      // top-index v dobe ulozeni stavu panelu
    char* FocusedName; // focus v dobe ulozeni stavu panelu

public:
    CPathHistoryItem(int type, const char* pathOrArchiveOrFSName,
                     const char* archivePathOrFSUserPart, HICON hIcon,
                     CPluginFSInterfaceAbstract* pluginFS);
    ~CPathHistoryItem();

    // zmena top-indexu a focused-name (opakovane pridani jedne cesty do historie)
    void ChangeData(int topIndex, const char* focusedName);

    void GetPath(char* buffer, int bufferSize);
    HICON GetIcon();
    BOOL Execute(CFilesWindow* panel); // vraci TRUE pokud zmena vysla (FALSE - zustava na miste)

    BOOL IsTheSamePath(CPathHistoryItem& item, CPluginFSInterfaceEncapsulation* curPluginFS); // vraci TRUE pri shode cest (kazdy typ se porovnava jinak)

    friend class CPathHistory;
};

class CPathHistory
{
protected:
    TIndirectArray<CPathHistoryItem> Paths;
    int ForwardIndex;            // -1 znamena 0 polozek pro forward, jinak od tohoto indexu
                                 // do konce pole Paths je forward
    BOOL Lock;                   // je objekt "uzamcen" (zmeny nejsou vitany - pouziva Execute - nase
                                 // zmeny cest v panelech neukladame ... (preruseni historie by nebylo na miste)
    BOOL DontChangeForwardIndex; // TRUE = ForwardIndex ma zustat na -1 (plne backward)
    CPathHistoryItem* NewItem;   // alokuje se, pokud behem AddPathUnique je nahozeny Lock (pro pozdejsi zpracovani)

public:
    CPathHistory(BOOL dontChangeForwardIndex = FALSE);
    ~CPathHistory();

    // vycisti vsechny polozky historie
    void ClearHistory();

    // pridani cesty do historie
    void AddPath(int type, const char* pathOrArchiveOrFSName, const char* archivePathOrFSUserPart,
                 CPluginFSInterfaceAbstract* pluginFS, CPluginFSInterfaceEncapsulation* curPluginFS);

    // pridani cesty do historie, jen pokud uz zde cesta neni (viz Alt+F12; u FS prepisuje pluginFS na nejnovejsi)
    void AddPathUnique(int type, const char* pathOrArchiveOrFSName, const char* archivePathOrFSUserPart,
                       HICON hIcon, CPluginFSInterfaceAbstract* pluginFS,
                       CPluginFSInterfaceEncapsulation* curPluginFS);

    // meni data (top-index a focused-name) aktualni cesty, a to jen pokud zadana cesta
    // odpovida aktualni ceste v historii
    void ChangeActualPathData(int type, const char* pathOrArchiveOrFSName,
                              const char* archivePathOrFSUserPart,
                              CPluginFSInterfaceAbstract* pluginFS,
                              CPluginFSInterfaceEncapsulation* curPluginFS,
                              int topIndex, const char* focusedName);

    // vymaze aktualni cestu z historie, a to jen pokud zadana cesta odpovida aktualni
    // ceste v historii
    void RemoveActualPath(int type, const char* pathOrArchiveOrFSName,
                          const char* archivePathOrFSUserPart,
                          CPluginFSInterfaceAbstract* pluginFS,
                          CPluginFSInterfaceEncapsulation* curPluginFS);

    // naplni menu polozkami
    // ID budou od jednicky a odpovidaji parameru index ve volani metody Execute()
    void FillBackForwardPopupMenu(CMenuPopup* popup, BOOL forward);

    // naplni menu polozkami
    // ID budou od firstID. Pri volani Execute je treba je posunout tak, aby prvni melo hodnotu 1.
    // maxCount - maximalni pocet pripojovanych polozek; -1 - vsechny dostupne (separator se nepocita)
    // separator - pokud menu obsahuje alespon jednu polozku, bude nad ni vlozen separator
    void FillHistoryPopupMenu(CMenuPopup* popup, DWORD firstID, int maxCount, BOOL separator);

    // vola se pri zavirani FS - v historii jsou ulozene FS ifacy, ktere je po zavreni potreba
    // NULLovat (aby nahodou nedoslo ke shode jen diky alokaci FS ifacu na stejnou adresu)
    void ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs);

    // index vybrane polozky v menu forward/backward (indexovano: u forward od jedne, backward od dvou)
    void Execute(int index, BOOL forward, CFilesWindow* panel, BOOL allItems = FALSE, BOOL removeItem = FALSE);

    BOOL HasForward() { return ForwardIndex != -1; }
    BOOL HasBackward()
    {
        int count = (ForwardIndex == -1) ? Paths.Count : ForwardIndex;
        return count > 1;
    }
    BOOL HasPaths() { return Paths.Count > 0; }

    void SaveToRegistry(HKEY hKey, const char* name, BOOL onlyClear);
    void LoadFromRegistry(HKEY hKey, const char* name);
};

//*****************************************************************************
//
// CFileHistoryItem, CFileHistory
//
// Drzi seznam souboru, na ktere uzivatel volal View nebo Edit.
//

enum CFileHistoryItemTypeEnum
{
    fhitView,
    fhitEdit,
    fhitOpen,
};

class CFileHistoryItem
{
protected:
    CFileHistoryItemTypeEnum Type; // jakym zpusobem bylo k souboru pristoupeno
    DWORD HandlerID;               // ID viewru/editoru pro opakovani akce
    HICON HIcon;                   // ikonka asociovana k souboru
    char* FileName;                // nazev souboru

public:
    CFileHistoryItem(CFileHistoryItemTypeEnum type, DWORD handlerID, const char* fileName);
    ~CFileHistoryItem();

    BOOL IsGood() { return FileName != NULL; }

    // vrati TRUE, pokud je objekt nakonstruovan z uvedenych dat
    BOOL Equal(CFileHistoryItemTypeEnum type, DWORD handlerID, const char* fileName);

    BOOL Execute();

    friend class CFileHistory;
};

class CFileHistory
{
protected:
    TIndirectArray<CFileHistoryItem> Files; // polozky s mensim indexem jsou mladsi

public:
    CFileHistory();

    // odstrani vsechny polozky historie
    void ClearHistory();

    // prohleda historii a pokud pridavanou polozku nenalezne, prida ji na vrchni pozici
    // pokud polozka uz exituje, bude vytazena na vrchni pozici
    BOOL AddFile(CFileHistoryItemTypeEnum type, DWORD handlerID, const char* fileName);

    // naplni menu polozkami
    // ID budou od jednicky a odpovidaji parameru index ve volani metody Execute()
    BOOL FillPopupMenu(CMenuPopup* popup);

    // index vybrane polozky v menu (indexovano od jedne)
    BOOL Execute(int index);

    // drzi historie nejakou polozku?
    BOOL HasItem();
};

//****************************************************************************
//
// CColumn
//

class CPluginDataInterfaceAbstract;

// tato sada promennych slouzi pro interni callbacky sloupcu Salamandera
// plugin dostane ukazatele na tyto ukazatele
extern const CFileData* TransferFileData;                     // ukazatel na data, podle kterych kreslime sloupec
extern int TransferIsDir;                                     // 0 (soubor), 1 (adresar), 2 (up-dir)
extern char TransferBuffer[TRANSFER_BUFFER_MAX];              // ukazatel na pole, ktere ma TRANSFER_BUFFER_MAX znaku a slouzi jako navratova hodnota
extern int TransferLen;                                       // pocet vracenych znaku
extern DWORD TransferRowData;                                 // uzivatelska data, bity 0x00000001 az 0x00000080 jsou vyhrazene pro Salamandera
extern CPluginDataInterfaceAbstract* TransferPluginDataIface; // plugin-data-interface panelu, do ktereho se polozka vykresluje (patri k TransferFileData->PluginData)
extern DWORD TransferActCustomData;                           // CustomData sloupce, pro ktery se ziskava text (pro ktery se vola callback) // FIXME_X64 - male pro ukazatel, neni nekdy potreba?

// pokud uz se hledala pripona v Associations, je zde vysledek hledani
extern int TransferAssocIndex; // -2 jeste se nehledala, -1 neni tam, >=0 platny index

// funkce pro plneni standardnich sloupcu Salamandera
void WINAPI InternalGetDosName();
void WINAPI InternalGetSize();
void WINAPI InternalGetType();
void WINAPI InternalGetDate();
void WINAPI InternalGetDateOnlyForDisk();
void WINAPI InternalGetTime();
void WINAPI InternalGetTimeOnlyForDisk();
void WINAPI InternalGetAttr();
void WINAPI InternalGetDescr();

// funkce pro ziskani indexu jednoduchych ikon pro FS s vlastnimi ikonami (pitFromPlugin)
int WINAPI InternalGetPluginIconIndex();

//****************************************************************************
//
// CViews
//

#define STANDARD_COLUMNS_COUNT 9 // pocet standardnich sloupcu pro rozsah
#define VIEW_TEMPLATES_COUNT 10
#define VIEW_NAME_MAX 30
// sloupec Name je vzdy viditelny a pokud neni nastaven flag VIEW_SHOW_EXTENSION, obsahuje i priponu
#define VIEW_SHOW_EXTENSION 0x00000001
#define VIEW_SHOW_DOSNAME 0x00000002
#define VIEW_SHOW_SIZE 0x00000004
#define VIEW_SHOW_TYPE 0x00000008
#define VIEW_SHOW_DATE 0x00000010
#define VIEW_SHOW_TIME 0x00000020
#define VIEW_SHOW_ATTRIBUTES 0x00000040
#define VIEW_SHOW_DESCRIPTION 0x00000080

// struktura pro definici jednoho standardniho sloupce
struct CColumDataItem
{
    DWORD Flag;
    int NameResID;
    int DescResID;
    FColumnGetText GetText;
    unsigned SupportSorting : 1;
    unsigned LeftAlignment : 1;
    unsigned ID : 4;
};

// definice standardnich sloupcu
CColumDataItem* GetStdColumn(int i, BOOL isDisk);

//****************************************************************************
//
// CViewTemplate, CViewTemplates
//
// Slouzi jako predloha pro pohledy v panelech. Urcuje viditelnost sloupcu
// v jednotlivych pohledech. Sablony jsou spolecne oboum panelum. Neobsahuji
// data, ktera jsou panelove zavisla (krome sirek a elasticity sloupcu).
//

struct CColumnConfig
{
    unsigned LeftWidth : 16;
    unsigned RightWidth : 16;
    unsigned LeftFixedWidth : 1;
    unsigned RightFixedWidth : 1;
};

struct CViewTemplate
{
    DWORD Mode;               // rezim zobrazovani pohledu (tree/brief/detailed)
    char Name[VIEW_NAME_MAX]; // nazev, pod ktery pohled bude pohled vystupovat v konfiguraci/menu;
                              // pokud je prazdny retezec, pohled neni definovan
    DWORD Flags;              // viditelnost standardnich Salamanderovskych sloupcu
                              // VIEW_SHOW_xxxx

    CColumnConfig Columns[STANDARD_COLUMNS_COUNT]; // drzi sirky a elasticitu sloupcu

    BOOL LeftSmartMode;  // smart mode pro levy panel (jen elasticky sloupec Name: sloupec se zuzuje, aby nebyla potreba horizontalni scrollbara)
    BOOL RightSmartMode; // smart mode pro pravy panel (jen elasticky sloupec Name: sloupec se zuzuje, aby nebyla potreba horizontalni scrollbara)
};

class CViewTemplates
{
public:
    // prvni pohledy nelze presouvat a mazat; lze je vsak prejmenovat
    // promenna Mode je pevne dana pro vsech deset pohledu a nelze ji menit
    CViewTemplate Items[VIEW_TEMPLATES_COUNT];

public:
    CViewTemplates();

    // nastavi atributy
    void Set(DWORD index, DWORD viewMode, const char* name, DWORD flags, BOOL leftSmartMode, BOOL rightSmartMode);
    void Set(DWORD index, const char* name, DWORD flags, BOOL leftSmartMode, BOOL rightSmartMode);

    BOOL SwapItems(int index1, int index2); // prohodi dve polozky v poli
    BOOL CleanName(char* name);             // oreze mezery a vrati TRUE, je-li name ok

    int SaveColumns(CColumnConfig* columns, char* buffer);  // konverze pole na retezec
    void LoadColumns(CColumnConfig* columns, char* buffer); // a zpet

    BOOL Save(HKEY hKey); // ulozi cele pole
    BOOL Load(HKEY hKey); // nacte cele pole

    void Load(CViewTemplates& source)
    {
        memcpy(Items, source.Items, sizeof(Items));
    }
};

//****************************************************************************
//
// CDynamicStringImp
//
// dynamicky vytvoreny string - sam se prialokovava podle potreby (aktualni potreba + 100 znaku)

class CDynamicStringImp : public CDynamicString
{
public:
    char* Text;
    int Allocated;
    int Length;

public:
    CDynamicStringImp()
    {
        Allocated = 0;
        Length = 0;
        Text = NULL;
    }
    ~CDynamicStringImp()
    {
        if (Text != NULL)
            free(Text);
    }

    // odpoji data od objektu (aby se v destruktoru objektu nedealokoval buffer)
    void DetachData();

    // vraci TRUE pokud se retezec 'str' o delce 'len' podarilo pridat; je-li 'len' -1,
    // urci se 'len' jako "strlen(str)" (pridani bez koncove nuly); je-li 'len' -2,
    // urci se 'len' jako "strlen(str)+1" (pridani vcetne koncove nuly)
    virtual BOOL WINAPI Add(const char* str, int len = -1);
};

//****************************************************************************
//
// CTruncatedString
//
// Retezec, ktery je konstruovan na zaklade str="xxxx "%s" xxxx" a subStr="data.txt".
// Substring bude pripadne oriznut na zaklade rozmeru dialogu/messageboxu.
//

class CTruncatedString
{
protected:
    char* Text;      // kompletni text
    int SubStrIndex; // index prvniho znaku zkracovatelneho podretezce; -1, pokud neexistuje
    int SubStrLen;   // pocet znaku podretezce

    char* TruncatedText; // zkracena forma textu (pokud bylo zkraceni potreba)

public:
    CTruncatedString();
    ~CTruncatedString();

    // provede kopii
    BOOL CopyFrom(const CTruncatedString* src);

    // obsah promenne str bude nakopirovan do alokovaneho bufferu Text
    // pokud je subStr ruzny od NULL, bude jeho obsah pres sprintf vlozen do str
    // predpokladem je, ze str obsahuje formatovaci retezec %s
    BOOL Set(const char* str, const char* subStr);

    // na zaklade rozmeru okna urceneho ctrlID bude retezec zkracen
    // pokud je nastavena promenna forMessageBox, bude zkracen substring tak,
    // a messagebox nepresahl hranice obrazovky
    BOOL TruncateText(HWND hWindow, BOOL forMessageBox = FALSE);

    // vrati zkracenou (pokud to bylo treba) verzi textu
    const char* Get();

    // vrati TRUE, pokud je mozne retezec zkratit
    BOOL NeedTruncate() { return SubStrIndex != -1; }
};

//****************************************************************************
//
// CShares
//
// seznam sharovanych adresaru

struct CSharesItem
{
    char* LocalPath;  // alokovana lokalni cesta pro sdileny prostredek
    char* LocalName;  // ukazuje do LocalPath a oznacuje nazev sdileneho adresare
                      // pokud jde o root cestu, je rovno LocalPath
    char* RemoteName; // nazev sdileneho prostredku
    char* Comment;    // nepovinny popis sdileneho prostredku

    CSharesItem(const char* localPath, const char* remoteName, const char* comment);
    ~CSharesItem();

    void Cleanup(); // inicializuje ukazatele na a promenne
    void Destroy(); // destrukce alokovanych dat

    BOOL IsGood() { return LocalPath != NULL; } // pokud je alokovana LocalPath, budou i ostatni
};

class CShares
{
protected:
    CRITICAL_SECTION CS;                // sekce pouzita pro synchronizaci dat objektu
    TIndirectArray<CSharesItem> Data;   // seznam sharu
    TIndirectArray<CSharesItem> Wanted; // seznam odkazu do Data atraktivnich pro hledani
    BOOL SubsetOnly;

public:
    CShares(BOOL subsetOnly = TRUE); // subsetOnly znamena, ze nebudou pridany "special" shary
                                     // jeste bychom mohli neplnit Comment, ale je to asi minimalni ujma
    ~CShares();

    void Refresh(); // znovunacteni sharu ze systemu

    // pripravi pouziti pro Search(), 'path' je cesta, na ktere nas zajimaji shary ("" = this_computer)
    void PrepareSearch(const char* path);

    // vraci TRUE, pokud je na 'path' z PrepareSearch sharovany podadresar (nebo root) 'name'
    BOOL Search(const char* name);

    // vraci TRUE, pokud je 'path' sdilenym adresarem nebo jeho podadresarem
    // pokud takove sdileni nebylo nalezeno, vraci FALSE
    // volat bez PrepareSearch; prohledava vsechny shary linearne
    // POZOR! neoptimalizovane na rychlost jako PrepareSearch/Search
    BOOL GetUNCPath(const char* path, char* uncPath, int uncPathMax);

    // vrati pocet sdilenych adresaru
    int GetCount() { return Data.Count; }

    // vrati informace o konkretni polozce; localPath, remoteName nebo comment
    // mohou obsahovat NULL a nebudou pak vraceny
    BOOL GetItem(int index, const char** localPath, const char** remoteName, const char** comment);

protected:
    // vraci TRUE pokud najde 'name' ve Wanted + jeho 'index', jinak FALSE + 'index' kam vlozit
    BOOL GetWantedIndex(const char* name, int& index);
};

class CSalamanderHelp : public CWinLibHelp
{
public:
    virtual void OnHelp(HWND hWindow, UINT helpID, HELPINFO* helpInfo,
                        BOOL ctrlPressed, BOOL shiftPressed);
    virtual void OnContextMenu(HWND hWindow, WORD xPos, WORD yPos);
};

extern CSalamanderHelp SalamanderHelp;

//****************************************************************************
//
// CLanguage
//

class CLanguage
{
public:
    // nazev SLG souboru (pouze nazev.spl)
    char* FileName;

    // udaje vytazene ze SLG souboru
    WORD LanguageID;
    WCHAR* AuthorW;
    char* Web;
    WCHAR* CommentW;
    char* HelpDir;

public:
    CLanguage();

    BOOL Init(const char* fileName, WORD languageID, const WCHAR* authorW,
              const char* web, const WCHAR* commentW, const char* helpdir);
    BOOL Init(const char* fileName, HINSTANCE modul);
    void Free();
    BOOL GetLanguageName(char* buffer, int bufferSize);
};

BOOL IsSLGFileValid(HINSTANCE hModule, HINSTANCE hSLG, WORD& slgLangID, char* isIncomplete);

//*****************************************************************************
//
// CSystemPolicies
//
// Infromace se nacitaji pri startu Salamandera a upravuji nektere vlastnosti
// a funkce programu.
//

class CSystemPolicies
{
private:
    DWORD NoRun;
    DWORD NoDrives;
    // DWORD NoViewOnDrive;
    DWORD NoFind;
    DWORD NoShellSearchButton;
    DWORD NoNetHood;
    // DWORD NoEntireNetwork;
    // DWORD NoComputersNearMe;
    DWORD NoNetConnectDisconnect;
    DWORD RestrictRun;
    DWORD DisallowRun;
    DWORD NoDotBreakInLogicalCompare;
    TDirectArray<char*> RestrictRunList;
    TDirectArray<char*> DisallowRunList;

public:
    CSystemPolicies();
    ~CSystemPolicies();

    // vytahne nastaveni z registry
    void LoadFromRegistry();

    // If your application has a "run" function that allows a user to start a program
    // by typing in its name and path in a dialog, then your application must disable
    // that functionality when this policy is enabled
    //
    // 0 - The policy is disabled or not configured. The Run command appears.
    // 1 - The policy is enabled. The Run command is removed.
    DWORD GetNoRun() { return NoRun; }

    // Your application must hide any drives that are hidden by the system when this
    // policy is enabled; this includes any buttons, menu options, icons, or any other
    // visual representation of drives in your application. This does not preclude the
    // user from accessing drives by manually entering drive letters in dialogs
    //
    // 0x00000000 - Do not restrict drives. All drives appear.
    // 0x00000001 - Restrict A drive only.
    // 0x00000002 - Restrict B drive only.
    // ....
    // 0x0000000F - Restrict A, B, C, and D drives only.
    // ....
    // 0x03FFFFFF - Restrict all drives.
    DWORD GetNoDrives() { return NoDrives; }

    // When a drive is represented in the value of this entry, users cannot view the
    // contents of the selected drives in Computer or Windows Explorer. Also, they
    // cannot use the Run dialog box, the Map Network Drive dialog box, or the Dir command
    // to view the directories on these drives.
    //
    // 0x00000000 - Do not restrict drives. All drives appear.
    // 0x00000001 - Restrict A drive only.
    // 0x00000002 - Restrict B drive only.
    // ....
    // 0x0000000F - Restrict A, B, C, and D drives only.
    // ....
    // 0x03FFFFFF - Restrict all drives.
    // DWORD GetNoViewOnDrive() {return NoViewOnDrive;}

    // When the value of this entry is 1, the following features are removed or disabled.
    // When the value is 0, these features operate normally.
    // The Search item is removed from the Start menu and from the context menu that
    // appears when you right-click the Start menu.
    // The system does not respond when users press F3 or the Application key (the key
    // with the Windows logo) + F.
    // In Windows Explorer, the Search item still appears on the Standard buttons toolbar,
    // but the system does not respond when the user presses Ctrl + F.
    // In Windows Explorer, the Search item does not appear in the context menu when you
    // right-click an icon representing a drive or a folder.
    //
    // 0 - The policy is disabled or not configured.
    // 1 - The policy is enabled.
    DWORD GetNoFind() { return NoFind; }

    // Removes the Search button from the Standard Buttons toolbar that appears in
    // Windows Explorer and other programs that use the Windows Explorer window,
    // such as Computer and Network.
    //
    // 0 - The policy is disabled or not configured. The Search button appears on the Windows Explorer toolbar.
    // 1 - The policy is enabled. The Search button is removed from the Windows Explorer toolbar.
    DWORD GetNoShellSearchButton() { return NoShellSearchButton; }

    // Removes the My Network Places icon from the desktop.
    //
    // 0 - The policy is disabled or not configured. The My Network Places icon appears on the desktop.
    // 1 - The policy is enabled. The My Network Places icon is hidden.
    DWORD GetNoNetHood() { return NoNetHood; }

    // Removes the Entire Network option and the icons representing networked computers
    // from My Network Places and from the Map Network Drive browser. As a result, users
    // cannot view computers outside of their workgroup or local domain in lists of network
    // resources in Windows Explorer and My Network Places.
    //
    // 0 - The policy is disabled or not configured. The Entire Network option appears.
    // 1 - The policy is enabled. The Entire Network option is removed.
    // DWORD GetNoEntireNetwork() {return NoEntireNetwork;}

    // Removes the Computers Near Me icon, and the icons representing computers in the
    // workgroup, from My Network Places and from the Map Network Drive browser window.
    //
    // 0 - The policy is disabled or not configured. The Computers Near Me icon appears
    //     when the computer is a member of a workgroup.
    // 1 - The policy is enabled.The Computers Near Me icon never appears.
    // DWORD GetNoComputersNearMe() {return NoComputersNearMe;}

    // Prevents users from using Windows Explorer or My Network Places to map or
    // disconnect network drives.
    //
    // 0 - The policy is disabled or not configured. Users can map and disconnect network drives.
    // 1 - The policy is enabled. Users can't map and disconnect network drives.
    DWORD GetNoNetConnectDisconnect() { return NoNetConnectDisconnect; }

    // jsou uvalene restrikce na spousteni aplikaci?
    BOOL GetMyRunRestricted() { return RestrictRun != 0 || DisallowRun != 0; }
    // je uvalena restrikce na soubor 'fileName' (muze byt i plna cesta)
    BOOL GetMyCanRun(const char* fileName);

    // 1 = nase StrCmpLogicalEx a systemova StrCmpLogicalW pod Vistou neberou tecku jako
    // oddelovac ve jmenech ("File.txt" je vetsi nez "File (4).txt")
    DWORD GetNoDotBreakInLogicalCompare() { return NoDotBreakInLogicalCompare; }

private:
    // nastavi vsechny hodnoty do povoleneho stavu a provede destrukci seznamu stringu
    void EnableAll();
    // nacte vsechny klice a prida je do seznamu
    // vraci FALSE, pokud bylo malo pameti pro alokaci seznamu
    BOOL LoadList(TDirectArray<char*>* list, HKEY hRootKey, const char* keyName);
    // vrati TRUE, pokud je 'name' v seznamu list
    BOOL FindNameInList(TDirectArray<char*>* list, const char* name);
};

extern CSystemPolicies SystemPolicies;

//
// ****************************************************************************
//
// horizontalne i vertikalne centrovany dialog
// predek vsech dialogu v Salamandrovi
// zajistuje volani ArrangeHorizontalLines pro vsechny dialogy
//
// Pokud je 'HCenterAgains' ruzny od NULL, je centrovano k nemu, jinak k Parentu.
// nastavuje msgbox parenta pro plug-iny na tento dialog (jen po dobu jeho platnosti)
//

class CCommonDialog : public CDialog
{
protected:
    HWND HCenterAgains;
    HWND HOldPluginMsgBoxParent;
    BOOL CallEndStopRefresh; // je treba zavolat EndStopRefresh

public:
    CCommonDialog(HINSTANCE modul, int resID, HWND parent,
                  CObjectOrigin origin = ooStandard, HWND hCenterAgains = NULL)
        : CDialog(modul, resID, parent, origin)
    {
        HCenterAgains = hCenterAgains;
        HOldPluginMsgBoxParent = NULL;
        CallEndStopRefresh = FALSE;
    }
    CCommonDialog(HINSTANCE modul, int resID, UINT helpID, HWND parent,
                  CObjectOrigin origin = ooStandard, HWND hCenterAgains = NULL)
        : CDialog(modul, resID, helpID, parent, origin)
    {
        HCenterAgains = hCenterAgains;
        HOldPluginMsgBoxParent = NULL;
        CallEndStopRefresh = FALSE;
    }
    ~CCommonDialog()
    {
        if (CallEndStopRefresh)
            TRACE_E("CCommonDialog::~CCommonDialog(): EndStopRefresh() was not called!");
    }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
};

//
// ****************************************************************************
//
// predek vsech property-sheet pages v Salamandrovi
// zajistuje volani ArrangeHorizontalLines pro vsechny pages
//

class CCommonPropSheetPage : public CPropSheetPage
{
public:
    CCommonPropSheetPage(TCHAR* title, HINSTANCE modul, int resID,
                         DWORD flags /* = PSP_USETITLE*/, HICON icon,
                         CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, flags, icon, origin) {}
    CCommonPropSheetPage(TCHAR* title, HINSTANCE modul, int resID, UINT helpID,
                         DWORD flags /* = PSP_USETITLE*/, HICON icon,
                         CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, helpID, flags, icon, origin) {}

protected:
    virtual void NotifDlgJustCreated();
};

//****************************************************************************
//
// CMessagesKeeper
//
// Kruhova fronta, ktera drzi X poslednich struktur MSG,
// ktere jsme odchytili v hooku
//
// V pripade padu Salamandera vkladame tento seznam do Bug Reporu
//

// pocet drzenych zprav
#define MESSAGES_KEEPER_COUNT 30

class CMessagesKeeper
{
private:
    MSG Messages[MESSAGES_KEEPER_COUNT]; // vlastni zpravy
    int Index;                           // index do pole Messages na volne misto
    int Count;                           // pocet validnich polozek

public:
    CMessagesKeeper();

    // vlozi do fronty zpravu
    void Add(const MSG* msg);

    // vrati pocet validnich zprav
    int GetCount() { return Count; }

    // do 'buffer' (velikost bufferu predat v 'buffMax') vlozi polozku dle indexu
    // 'index': pro hodnotu 0 pujde o nejstarsi polozku, pro hodnotu Count
    // o posledne pridavanou message
    // pokud jde index mimo pole, vlozi text "error"
    void Print(char* buffer, int buffMax, int index);
};

// pro hlavni aplikacni smycku
extern CMessagesKeeper MessagesKeeper;

//****************************************************************************
//
// CWayPointsKeeper
//
// Kruhova fronta, ktera drzi X poslednich waypointu (id a custom data a cas vlozeni),
// ktere jsme rozmistili po kodu
//
// V pripade padu Salamandera vkladame tento seznam do Bug Reporu
//

// pocet drzenych waypointu
#define WAYPOINTS_KEEPER_COUNT 100

struct CWayPoint
{
    DWORD WayPoint;     // hodnota definovavana v kodu
    WPARAM CustomData1; // uzivatelska hodnota
    LPARAM CustomData2; // uzivatelska hodnota
    DWORD Time;         // cas vlozeni
};

class CWayPointsKeeper
{
private:
    CWayPoint WayPoints[WAYPOINTS_KEEPER_COUNT]; // vlastni waypointy
    int Index;                                   // index do pole WayPoints na volne misto
    int Count;                                   // pocet validnich polozek
    BOOL Stopped;                                // TRUE/FALSE = ukladani waypointu povoleno/zakazano
    CRITICAL_SECTION CS;                         // sekce pouzita pro synchronizaci dat objektu

public:
    CWayPointsKeeper();
    ~CWayPointsKeeper();

    // vlozi do fronty waypoint
    void Add(DWORD waypoint, WPARAM customData1 = 0, LPARAM customData2 = 0);

    // je-li 'stop' TRUE, ukonci ukladani waypointu (volani metody Add se ignoruje);
    // je-li 'stop' FALSE, ukladani waypointu se opet povoli
    void StopStoring(BOOL stop);

    // vrati pocet validnich waypointu
    int GetCount()
    {
        HANDLES(EnterCriticalSection(&CS));
        int count = Count;
        HANDLES(LeaveCriticalSection(&CS));
        return count;
    }

    // do 'buffer' (velikost bufferu predat v 'buffMax') vlozi polozku dle indexu
    // 'index': pro hodnotu 0 pujde o nejstarsi polozku, pro hodnotu Count
    // posledne pridavany waypoint
    // pokud jde index mimo pole, vlozi text "error"
    void Print(char* buffer, int buffMax, int index);
};

//******************************************************************************
//
// CITaskBarList3
//
// Zapouzdreni ITaskBarList3 interface, ktery MS zavedli od Windows 7
//

class CITaskBarList3
{
public:
    ITaskbarList3* Iface;
    HWND HWindow;

public:
    CITaskBarList3()
    {
        Iface = NULL;
        HWindow = NULL;
    }

    ~CITaskBarList3()
    {
        if (Iface != NULL)
        {
            Iface->Release();
            Iface = NULL;
            CoUninitialize();
        }
    }

    BOOL Init(HWND hWindow) // volat po obdrzeni zpravy TaskbarBtnCreatedMsg
    {
        // Initialize COM for this thread...
        CoInitialize(NULL);
        CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (void**)&Iface);
        if (Iface == NULL)
        {
            TRACE_E("CoCreateInstance() failed for IID_ITaskbarList3!");
            CoUninitialize();
            return FALSE;
        }
        HWindow = hWindow;
        return TRUE;
    }

    void SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal)
    {
        // muze se stat, ze progressTotal je 1 a progressCurrent je velke cislo, pak je vypocet
        // nesmyslny (navic pada diky RTC) a je treba explicitne zadat 0% nebo 100% (hodnotu 1000)
        SetProgressValue(progressCurrent >= progressTotal ? (progressTotal.Value == 0 ? 0 : 1000) : (DWORD)((progressCurrent * CQuadWord(1000, 0)) / progressTotal).Value,
                         1000);
    }

    void SetProgressValue(ULONGLONG ullCompleted, ULONGLONG ullTotal)
    {
        if (Iface != NULL)
        {
            HRESULT hres = Iface->SetProgressValue(HWindow, ullCompleted, ullTotal);
            if (hres != S_OK)
                TRACE_E("SetProgressValue failed! hres=" << hres);
        }
    }

    void SetProgressState(TBPFLAG tbpFlags)
    {
        if (Iface != NULL)
        {
            HRESULT hres = Iface->SetProgressState(HWindow, tbpFlags);
            if (hres != S_OK)
                TRACE_E("SetProgressState failed! hres=" << hres);
        }
    }
};

//****************************************************************************
//
// CShellExecuteWnd
//
// okno slouzi jako parent pri volani InvokeCommand, SHFileOperation, atd.
// pokud pred volanim destruktoru nekdo zavola DestroyWindow na tento handle,
// zobrazi se MessageBox ze nas postrelila nektera shell extension a pozada
// se o poslani nasledujiciho Break-bug reporu; v nem je vide callstack
//
class CShellExecuteWnd : public CWindow
{
protected:
    BOOL CanClose;

public:
    CShellExecuteWnd();
    ~CShellExecuteWnd();

    // 'format' je formatovaci retezec pro sprintf; ukazateke na retezce mohou byt NULL, budou prelozeny na "(null)"
    // v pripade uspechu vraci handle vytvoreneho okna
    // POZOR: v pripade neuspechu vraci hParent
    HWND Create(HWND hParent, const char* format, ...);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// pro okno 'hParent' enumeruje vsechny childy a hleda okna CShellExecuteWnd
// jejich nazvy vytahne a vklada do bufferu 'text', oddelene koncema radku "\r\n"
// neprekroci velikost bufferu 'textMax' a konec terminuje znakem 0
// vraci pocet nalezenych oken
int EnumCShellExecuteWnd(HWND hParent, char* text, int textMax);

//
// ****************************************************************************

class CSalamanderSafeFile : public CSalamanderSafeFileAbstract
{
public:
    virtual BOOL WINAPI SafeFileOpen(SAFE_FILE* file,
                                     const char* fileName,
                                     DWORD dwDesiredAccess,
                                     DWORD dwShareMode,
                                     DWORD dwCreationDisposition,
                                     DWORD dwFlagsAndAttributes,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask);

    virtual HANDLE WINAPI SafeFileCreate(const char* fileName,
                                         DWORD dwDesiredAccess,
                                         DWORD dwShareMode,
                                         DWORD dwFlagsAndAttributes,
                                         BOOL isDir,
                                         HWND hParent,
                                         const char* srcFileName,
                                         const char* srcFileInfo,
                                         DWORD* silentMask,
                                         BOOL allowSkip,
                                         BOOL* skipped,
                                         char* skipPath,
                                         int skipPathMax,
                                         CQuadWord* allocateWholeFile,
                                         SAFE_FILE* file);

    virtual void WINAPI SafeFileClose(SAFE_FILE* file);

    virtual BOOL WINAPI SafeFileSeek(SAFE_FILE* file,
                                     CQuadWord* distance,
                                     DWORD moveMethod,
                                     DWORD* error);

    virtual BOOL WINAPI SafeFileSeekMsg(SAFE_FILE* file,
                                        CQuadWord* distance,
                                        DWORD moveMethod,
                                        HWND hParent,
                                        DWORD flags,
                                        DWORD* pressedButton,
                                        DWORD* silentMask,
                                        BOOL seekForRead);

    virtual BOOL WINAPI SafeFileGetSize(SAFE_FILE* file,
                                        CQuadWord* fileSize,
                                        DWORD* error);

    virtual BOOL WINAPI SafeFileRead(SAFE_FILE* file,
                                     LPVOID lpBuffer,
                                     DWORD nNumberOfBytesToRead,
                                     LPDWORD lpNumberOfBytesRead,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask);

    virtual BOOL WINAPI SafeFileWrite(SAFE_FILE* file,
                                      LPVOID lpBuffer,
                                      DWORD nNumberOfBytesToWrite,
                                      LPDWORD lpNumberOfBytesWritten,
                                      HWND hParent,
                                      DWORD flags,
                                      DWORD* pressedButton,
                                      DWORD* silentMask);
};

extern BOOL GotMouseWheelScrollLines;

// An OS independent method to retrieve the number of wheel scroll lines.
// Returns: Number of scroll lines where WHEEL_PAGESCROLL indicates to scroll a page at a time.
UINT GetMouseWheelScrollLines();
UINT GetMouseWheelScrollChars(); // pro horizontalni scroll

BOOL InitializeMenuWheelHook();
BOOL ReleaseMenuWheelHook();

#define BUG_REPORT_REASON_MAX 1000
extern char BugReportReasonBreak[BUG_REPORT_REASON_MAX]; // text zobrazeny pri breaku Salamandera do bug reportu (jako duvod)

extern CShares Shares; // zde jsou ulozeny nactene sharovane adresare

extern CSalamanderSafeFile SalSafeFile; // interface pro komfortni praci se soubory

extern const char* SalamanderConfigurationRoots[];                                                           // popis v mainwnd2.cpp
BOOL GetUpgradeInfo(BOOL* autoImportConfig, char* autoImportConfigFromKey, int autoImportConfigFromKeySize); // popis v mainwnd2.cpp
BOOL FindLatestConfiguration(BOOL* deleteConfigurations, const char*& loadConfiguration);                    // popis v mainwnd2.cpp
BOOL FindLanguageFromPrevVerOfSal(char* slgName);                                                            // popis v mainwnd2.cpp

// k editline/comboboxu 'ctrlID' vytvori a attachne specialni tridu, ktera umoznuje
// odchyceni klaves a zaslani zpravy WM_USER_KEYDOWN do dialogu 'hDialog'
// LOWORD(wParam) obsahuje ctrlID, a lParam obsahuje stistenou klavesu (wParam ze zpravy WM_KEYDOWN/WM_SYSKEYDOWN)
BOOL CreateKeyForwarder(HWND hDialog, int ctrlID);
// volat po doruceni zpravy WM_USER_KEYDOWN; vraci TRUE, pokud byla klavesa zpracovana
DWORD OnDirectoryKeyDown(DWORD keyCode, HWND hDialog, int editID, int editBufSize, int buttonID);
// volat po doruceni zpravy WM_USER_BUTTON, zajisti vybaleni menu za tlacitkem 'buttonID'
// a nasledne naplneni editline 'editID'
void OnDirectoryButton(HWND hDialog, int editID, int editBufSize, int buttonID, WPARAM wParam, LPARAM lParam);

// volat po doruceni zpravy WM_USER_BUTTON, zajisti fungovani Ctrl+A na systemech do Windows Vista, kde uz to funguje vsude
DWORD OnKeyDownHandleSelectAll(DWORD keyCode, HWND hDialog, int editID);

//  vraci TRUE, pokud hotKey patri Salamanderu
BOOL IsSalHotKey(WORD hotKey);

void GetNetworkDrives(DWORD& netDrives, char (*netRemotePath)[MAX_PATH]);
void GetNetworkDrivesBody(DWORD& netDrives, char (*netRemotePath)[MAX_PATH], char* buffer); // jen pro interni pouziti v bug-reportu

// vrati SID (jako retezec) pro nas proces
// vraceny SID je potreba uvolnit pomoci volani LocalFree
//   LPTSTR sid;
//   if (GetStringSid(&sid))
//     LocalFree(sid);
BOOL GetStringSid(LPTSTR* stringSid);

// vrati MD5 hash napocitany ze SID, cimz dostavame z variable-length SIDu pole 16 bajtu
// 'sidMD5' musi ukazovat do pole 16 bajtu
// v pripade uspechu vraci TRUE, jinak FALSE a nuluje cele pole 'sidMD5'
BOOL GetSidMD5(BYTE* sidMD5);

// pripravi SECURITY_ATTRIBUTES tak aby pomoci nich vytvareny objekt (mutex, mapovana pamet) byl zabezpecny
// znamena to, ze skupine Everyone je odebran pristup WRITE_DAC | WRITE_OWNER, jinak je vse povoleno
// jde o tridu lepsi zabezpecni nez "NULL DACL", kde je objekt uplne otevren vsem
// lze volat pod kazdym OS, ukazatel vrati od W2K vejs, jinak vraci NULL
// pokud vrati 'psidEveryone' nebo 'paclNewDacl' ruzne od NULL, je treba je destruovat
SECURITY_ATTRIBUTES* CreateAccessableSecurityAttributes(SECURITY_ATTRIBUTES* sa, SECURITY_DESCRIPTOR* sd,
                                                        DWORD allowedAccessMask, PSID* psidEveryone, PACL* paclNewDacl);

// V pripade uspechu vrati TRUE a naplni DWORD na ktery odkazuje 'integrityLevel'
// jinak (pri selhani nebo pod OS strasima nez Vista) vrati FALSE
BOOL GetProcessIntegrityLevel(DWORD* integrityLevel);

// stejna funkce jako API GetProcessId(), ale funkcni take pod W2K
DWORD SalGetProcessId(HANDLE hProcess);

// je treba volat po spusteni procesu, uklada diference v env. promennych,
// aby pozdeji fungovala fce RegenEnvironmentVariables()
void InitEnvironmentVariablesDifferences();

// nacte aktualni environment promenne a aplikuje diference
void RegenEnvironmentVariables();

// pokus o detekce SSD, vice viz CSalamanderGeneralAbstract::IsPathOnSSD()
BOOL IsPathOnSSD(const char* path);
