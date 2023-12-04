// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CColorArrowButton;

//****************************************************************************
//
// CHighlightMasksItem
//

struct CHighlightMasksItem
{
    CMaskGroup* Masks;
    DWORD Attr;      // 1:include; 0:exclude
    DWORD ValidAttr; // bity v Attr, ktere jsou platne = 1; pokud na nich nezalezi = 0

    SALCOLOR NormalFg; // barvy v jednotlivych rezimech
    SALCOLOR NormalBk;
    SALCOLOR FocusedFg;
    SALCOLOR FocusedBk;
    SALCOLOR SelectedFg;
    SALCOLOR SelectedBk;
    SALCOLOR FocSelFg;
    SALCOLOR FocSelBk;
    SALCOLOR HighlightFg;
    SALCOLOR HighlightBk;

    CHighlightMasksItem();
    CHighlightMasksItem(CHighlightMasksItem& item);
    ~CHighlightMasksItem();

    BOOL Set(const char* masks);
    BOOL IsGood();
};

//****************************************************************************
//
// CHighlightMasks
//

class CHighlightMasks : public TIndirectArray<CHighlightMasksItem>
{
public:
    CHighlightMasks(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CHighlightMasksItem>(base, delta, dt) {}

    BOOL Load(CHighlightMasks& source);

    // prohleda vsechny masky, pokud najde odpovidajici polozku, vrati na ni ukazatel
    // jinak vrati NULL; 'fileExt' je u adresaru NULL (pripona se musi dohledat)
    inline CHighlightMasksItem* AgreeMasks(const char* fileName, const char* fileExt, DWORD fileAttr)
    {
        int i;
        for (i = 0; i < Count; i++)
        {
            CHighlightMasksItem* item = At(i);
            if (((item->Attr & item->ValidAttr) == (fileAttr & item->ValidAttr)) &&
                item->Masks->AgreeMasks(fileName, fileExt))
                return At(i);
        }
        return NULL;
    }
};

//****************************************************************************
//
// CViewerMasksItem
//

#define VIEWER_EXTERNAL 0
#define VIEWER_INTERNAL 1

struct CViewerMasksItem
{
    CMaskGroup* Masks;
    char *Command,
        *Arguments,
        *InitDir;

    int ViewerType;

    DWORD HandlerID; // unikatni ID (v ramci spusteni Salamandera)
                     // slouzi pro identifikaci editoru pri vyberu z historie souboru CFileHistory

    // pomocna promenna pro zjisteni typu udaju - TRUE = stare -> 'Type' (0 viewer, 1 IE viewer, 2 external)
    BOOL OldType;

    CViewerMasksItem(const char* masks, const char* command, const char* arguments, const char* initDir,
                     int viewerType, BOOL oldType);

    CViewerMasksItem();
    CViewerMasksItem(CViewerMasksItem& item);
    ~CViewerMasksItem();

    BOOL Set(const char* masks, const char* command, const char* arguments, const char* initDir);
    BOOL IsGood();
};

//****************************************************************************
//
// CViewerMasks
//

class CViewerMasks : public TIndirectArray<CViewerMasksItem>
{
public:
    CViewerMasks(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CViewerMasksItem>(base, delta, dt) {}

    BOOL Load(CViewerMasks& source);
};

//****************************************************************************
//
// CEditorMasksItem
//

struct CEditorMasksItem
{
    CMaskGroup* Masks;
    char *Command,
        *Arguments,
        *InitDir;

    DWORD HandlerID; // unikatni ID (v ramci spusteni Salamandera)
                     // slouzi pro identifikaci editoru pri vyberu z historie souboru CFileHistory

    CEditorMasksItem(char* masks, char* command, char* arguments, char* initDir);
    CEditorMasksItem();
    CEditorMasksItem(CEditorMasksItem& item);
    ~CEditorMasksItem();

    BOOL Set(const char* masks, const char* command, const char* arguments, const char* initDir);
    BOOL IsGood();
};

//****************************************************************************
//
// CEditorMasks
//

class CEditorMasks : public TIndirectArray<CEditorMasksItem>
{
public:
    CEditorMasks(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CEditorMasksItem>(base, delta, dt) {}

    BOOL Load(CEditorMasks& source);
};

//
// ****************************************************************************

extern const char* DefTopToolBar; // default hodnoty
extern const char* DefMiddleToolBar;
extern const char* DefLeftToolBar;
extern const char* DefRightToolBar;

#define TITLE_BAR_MODE_DIRECTORY 0 // musi korespondovat s polem {IDS_TITLEBAR_DIRECTORY, IDS_TITLEBAR_COMPOSITE, IDS_TITLEBAR_FULLPATH}
#define TITLE_BAR_MODE_COMPOSITE 1
#define TITLE_BAR_MODE_FULLPATH 2

#define TITLE_PREFIX_MAX 100 // velikost bufferu pro title prefix

typedef struct
{
    int IconResID;
    int TextResID;
} CMainWindowIconItem;
#define MAINWINDOWICONS_COUNT 4
extern CMainWindowIconItem MainWindowIcons[MAINWINDOWICONS_COUNT];

struct CConfiguration
{
    // ConfigVersion cislo verze nactene konfigurace - viz. komentar v mainwnd2.cpp
    DWORD ConfigVersion;

    int IncludeDirs,            // select/deselect (*, +, -) i adresaru
        AutoSave,               // save on exit
        CloseShell,             // zavirat shell po spusteni command-liny
        ShowGrepErrors,         // maji se ve Find Files dialogu ukazovat chybove hlasky
        FindFullRowSelect,      // full row select ve Find dialogu
        MinBeepWhenDone,        // ma beepnout po ukonceni prace v neaktivnim okne
        ClearReadOnly,          // odstranuje read-only flag pri operacich s CD-ROM
        PrimaryContextMenu,     // je na pravem tlacitku contexove menu ?
        NotHiddenSystemFiles,   // zobrazovat skryte a systemove soubory ?
        AlwaysOnTop,            // je hlavni okno Always On Top?
                                //      FastDirectoryMove,     // presun adresare prejmenovanim povolen?
        SortUsesLocale,         // radit podle lokalniho nastaveni (regional settings)
        SortDetectNumbers,      // detekovat pri razeni cisla v retezcich? (viz StrCmpLogicalW)
        SortNewerOnTop,         // novejsi polozky nahore -- chovani Sal 2.0
        SortDirsByName,         // radit adresare podle jmena
        SortDirsByExt,          // emulovat pripony u adresaru (sort by extension + show in separated Ext column)
        SaveHistory,            // ukladat historie do konfigurace?
        SaveWorkDirs,           // ukladat List of Working Directories?
        EnableCmdLineHistory,   // drzet historii prikazove radky?
        SaveCmdLineHistory,     // ukaladat  historii prikazove radky?
                                //      LantasticCheck,        // Lantastic 7.0 paranoid-check (porovnani velikosti po Copy)
        OnlyOneInstance,        // povolit jen jednu instanci
        ForceOnlyOneInstance,   // nastaveno z cmdline, Salamander se ma tvarit, ze je zapnuta volba OnlyOneInstance
        StatusArea,             // Salam bde v Trayi a v pripade minimalizace nebude v taskbare
        SingleClick,            // staci jeden klik pro vyber polozky
        TopToolBarVisible,      // viditelnost toolbary
        PluginsBarVisible,      // viditelnost toolbary
        MiddleToolBarVisible,   // viditelnost toolbary
        BottomToolBarVisible,   // viditelnost toolbary
        UserMenuToolBarVisible, // viditelnost toolbary
        HotPathsBarVisible,     // viditelnost toolbary
        DriveBarVisible,        // viditelnost drive bar
        DriveBar2Visible,       // viditelnost drive bar 2
        UseSalOpen,             // ma se pouzivat salopen.exe (jinak spousteni asociaci naprimo)
        NetwareFastDirMove,     // ma se na Novell Netware pouzivat fast-dir-move (rename adresaru)? (jinak prejmenovavame jen soubory, adresare se vytvari + stare prazdne mazou) (DUVOD: nekomu proste fast-dir-move na Novellu funguje a tak proste nechce cekat)
        UseAsyncCopyAlg,        // jen Win7+ (starsi OS: vzdy FALSE): ma se pouzivat asynchronni algoritmus kopirovani souboru na sitove disky?
        ReloadEnvVariables,     // mame pri zmene env promennych provadet regeneraci?
        QuickRenameSelectAll,   // Quick Rename/Pack ma vybrat vse (ne pouze jmeno) -- lide nadavali na foru po zavedeni noveho oznacovani
        EditNewSelectAll,       // EditNew ma vybrat vse (ne pouze jmeno) -- lide si vyzadali samostnou volbu, protoze nekdo zaklada vzdy .TXT (a vyhovuje mu ze prepise jen jmeno) a nekdo ruzne pripony a chce prepsat cely nazev
        ShiftForHotPaths,       // ma se pouzivat Shift+1..0 pro go to hot path?
        IconSpacingVert,        // vertikalni roztec v bodech mezi Icons/Thumbnails v panelu
        IconSpacingHorz,        // horizontalni roztec v bodech mezi Icons v panelu
        TileSpacingVert,        // vertikalni roztec v bodech mezi Tiles v panelu
        ThumbnailSpacingHorz,   // horizontalni roztec v bodech mezi Thumbnails v panelu
        ThumbnailSize,          // ctvercove rozmery thumbnailu v bodech
                                //      PanelTooltip,         // v panelu jsou tooltipovany zkracene texty
        KeepPluginsSorted,      // pluginy budou abecedne razeny (plugins manager, menu)
        ShowSLGIncomplete,      // TRUE = je-li IsSLGIncomplete neprazdne, ukazat hlasku o nekompletnim prekladu (ze shanime prekladatele)

        // Confirmation
        CnfrmFileDirDel,         // files or directory delete
        CnfrmNEDirDel,           // non-empty directory delete
        CnfrmFileOver,           // file overwrite
        CnfrmDirOver,            // directory overwrite (Copy/Move: join with existing directory if target directory already exists)
        CnfrmSHFileDel,          // system or hidden file delete
        CnfrmSHDirDel,           // system or hidden directory delete
        CnfrmSHFileOver,         // system or hidden file overwrite
        CnfrmNTFSPress,          // NTFS Compress, Uncompress
        CnfrmNTFSCrypt,          // NTFS Encrypt, Decrypt
        CnfrmDragDrop,           // Drag and Drop operations
        CnfrmCloseArchive,       // Show information before closing edited archive
        CnfrmCloseFind,          // Show information before closing Find dialog box
        CnfrmStopFind,           // Show information before stopping searching in Find dialog box
        CnfrmCreatePath,         // Show "do you want to create target path?" in Copy/Move operations
        CnfrmAlwaysOnTop,        // Show "Always on Top information"
        CnfrmOnSalClose,         // Show "do you want to close SS?"
        CnfrmSendEmail,          // Show "Do you want to Email selected files?"
        CnfrmAddToArchive,       // Show "Do you want to update existing archive?"
        CnfrmCreateDir,          // Show "The directory doesn't exist. Do you want to create it?"
        CnfrmChangeDirTC,        // uzivatel se pokousi menit cestu pomoci command line
        CnfrmShowNamesToCompare, // User Menu: zobrazovat dialog se zadanim jmen pro Compare i v pripade, ze jsou obe jmena zadana
        CnfrmDSTShiftsIgnored,   // Compare Directories: byly nalezeny pary souboru, kterych casy se lisi presne o jednu nebo dve hodiny, ukazat warning, ze tyto rozdily byly ignorovany?
        CnfrmDSTShiftsOccured,   // Compare Directories: byly nalezeny pary souboru, kterych casy se lisi presne o jednu nebo dve hodiny, ukazat hint, ze se tyto rozdily daji ignorovat?
        CnfrmCopyMoveOptionsNS,  // Copy/Move: jsou nastavene nejake "Options" a cil jsou FS/archiv; (options are NotSupported)

        // Drive specific
        DrvSpecFloppyMon,    // Use automatic refresh
        DrvSpecFloppySimple, // Use simple icons
        DrvSpecRemovableMon,
        DrvSpecRemovableSimple,
        DrvSpecFixedMon,
        DrvSpecFixedSimple,
        DrvSpecRemoteMon,
        DrvSpecRemoteSimple,
        DrvSpecRemoteDoNotRefreshOnAct, // remote drives/do not refresh on activate of Salamander
        DrvSpecCDROMMon,
        DrvSpecCDROMSimple;

    // options for Compare Directories dialog box / functions
    int CompareByTime;
    int CompareBySize;
    int CompareByContent;
    int CompareByAttr;
    int CompareSubdirs;
    int CompareSubdirsAttr;
    int CompareOnePanelDirs; // oznaci jmena adresaru existujici pouze v jednom panelu
    int CompareMoreOptions;  // je dialog zobrazen v rozsirenem stavu?
    int CompareIgnoreFiles;  // maji se ignorovat specifikovane nazvy souboru?
    int CompareIgnoreDirs;   // maji se ignorovat specifikovane nazvy adresaru?
    CMaskGroup CompareIgnoreFilesMasks;
    CMaskGroup CompareIgnoreDirsMasks;

    BOOL IfPathIsInaccessibleGoToIsMyDocs;   // TRUE = nepouzivat IfPathIsInaccessibleGoTo, tahat ze systemu rovnou Documents
    char IfPathIsInaccessibleGoTo[MAX_PATH]; // cesta na kterou jdeme pokud nelze zustat na aktualni ceste v panelu (vypadek sitove cesty, vyndani media z removable drivu, atd.)

    DWORD LastUsedSpeedLimit; // pamet na posledni pouzity speed-limit (useri obvykle zadavaji kolem dokola jedno cislo)

    BOOL QuickSearchEnterAlt; // je-li TRUE, bude vstup do QS pres Alt+pismeno

    // pro zobrazeni polozek v panelu
    int FullRowSelect;    // v detailed/brief view lze klikat kamkoliv
    int FullRowHighlight; // v detailed view je za focusem jeste podbarven zbytek radku
    int UseIconTincture;  // pro hidden/system/selected/focused polozky
    int ShowPanelCaption; // bude v directory line zobrazen barevne panel caption?
    int ShowPanelZoom;    // bude v directory line zobrazeno tlacitko Zoom?

    char InfoLineContent[200];

    int FileNameFormat; // jak upravit filename po nacteni z disku

    int SizeFormat; // jak zobrazovat hodnoty ve sloupci Size: SIZE_FORMAT_xxx

    int SpaceSelCalcSpace; // selecting with Space calculate occupied space

    int UseTimeResolution; // compare directories: use time resolution
    int TimeResolution;    // compare directories: time resolution <0..3600>s
    int IgnoreDSTShifts;   // compare directories: ignorovat casove rozdily presne jednu nebo dve hodiny? (reseni casovych posunu zpusobenych daylight-saving-time posuny a NTFS/FAT32/FTP/atd.)

    int UseDragDropMinTime;
    int DragDropMinTime; // minimalni doba v ms mezi mouse down a up (jinak nejde o drag&drop)

    int HotPathAutoConfig; // automaticky otevre konfig po prirazeni z panelu

    char TopToolBar[400]; // obsah ToolBar
    char MiddleToolBar[400];
    char LeftToolBar[200];
    char RightToolBar[200];

    int UseRecycleBin;       // 0 - do not use, 1 - for all, 2 - pro RecycleMasks
    CMaskGroup RecycleMasks; // pole masek pro rozliseni toho, co posilat do kose

    // na co se citi chudak uzivatel -- budeme redukovat menu
    BOOL SkillLevel; // SKILL_LEVEL_BEGINNER, SKILL_LEVEL_INTERMEDIATE, SKILL_LEVEL_ADVANCED

    // destrukce historii je zajistena v metode ClearHistory()
    char* SelectHistory[SELECT_HISTORY_SIZE];
    char* CopyHistory[COPY_HISTORY_SIZE];
    char* EditHistory[EDIT_HISTORY_SIZE];
    char* ChangeDirHistory[CHANGEDIR_HISTORY_SIZE];
    char* FileListHistory[FILELIST_HISTORY_SIZE];
    char* CreateDirHistory[CREATEDIR_HISTORY_SIZE];
    char* QuickRenameHistory[QUICKRENAME_HISTORY_SIZE];
    char* EditNewHistory[EDITNEW_HISTORY_SIZE];
    char* ConvertHistory[CONVERT_HISTORY_SIZE];
    char* FilterHistory[FILTER_HISTORY_SIZE];

    char FileListName[MAX_PATH]; // file name
    BOOL FileListAppend;
    int FileListDestination; // 0=Clipboard 1=Viewer 2=File

    // Internal Viewer:
    int CopyFindText; // po F3 z find files dialogu: kopirovat find text do viewru ?

    int EOL_CRLF, // ruzne konce radku (zapnuto/vypnuto)
        EOL_CR,
        EOL_LF,
        EOL_NULL;

    int DefViewMode,  // 0 = Auto-Select, 1 = Text, 2 = Hex
        TabSize,      // velikost (pocet mezer) tabelatoru
        SavePosition; // ukladat pozici okna/umistit dle hlavniho okna

    CMaskGroup TextModeMasks; // pole masek pro soubory zobrazovane vzdy v textovem rezimu
    CMaskGroup HexModeMasks;  // pole masek pro soubory zobrazovane vzdy v hexa rezimu

    WINDOWPLACEMENT WindowPlacement; // neplatne, pokud SavePosition != TRUE

    BOOL WrapText; // wrapovani textu, nastavuje se z menu (zde jen kvuli ukladani)

    BOOL CodePageAutoSelect;  // automaticky rozpoznat kodovou stranku
    char DefaultConvert[200]; // nazev kodovani, ktery chce user defaultne pouzivat

    BOOL AutoCopySelection; // automaticky kopirovat selection na clipboard

    BOOL GoToOffsetIsHex; // TRUE = offset se zadava hexa, jinak desitkove

    // rebar
    int MenuIndex; // poradi bandu v rebaru, cislovano od nuly
    int MenuBreak; // je band na novem radku?
    int MenuWidth; // delka bandu
    int TopToolbarIndex;
    int TopToolbarBreak;
    int TopToolbarWidth;
    int PluginsBarIndex;
    int PluginsBarBreak;
    int PluginsBarWidth;
    int UserMenuToolbarIndex;
    int UserMenuToolbarBreak;
    int UserMenuToolbarWidth;
    int UserMenuToolbarLabels;
    int HotPathsBarIndex;
    int HotPathsBarBreak;
    int HotPathsBarWidth;
    int DriveBarIndex;
    int DriveBarBreak;
    int DriveBarWidth;
    int GripsVisible;

    // Change drive
    int ChangeDriveShowMyDoc;    // zobrazovat polozky Documents
    int ChangeDriveShowAnother;  // zobrazovat polozku Another panel path
    int ChangeDriveShowNet;      // zobrazovat polozku Network
    int ChangeDriveCloudStorage; // zobrazovat polozky pro cloud storage (Google Drive, atd.)

    // Packers / Unpackers
    int UseAnotherPanelForPack;
    int UseAnotherPanelForUnpack;
    int UseSubdirNameByArchiveForUnpack;
    int UseSimpleIconsInArchives;

    BOOL UseEditNewFileDefault;        // ma se pouzivat EditNewFileDefault? (pokud ne, nacita se z resourcu, takze chodi prepinani jazyku)
    char EditNewFileDefault[MAX_PATH]; // pouziva se v EditNewFile prikazu jako default, pokud je zapnuto UseEditNewFileDefault

    // Tip of the Day
    //  int  ShowTipOfTheDay;         // zobrazovat pri spusteni programu Tip dne
    //  int  LastTipOfTheDay;         // index posledniho zobrazeneho tipu

    // plug-iny
    int LastPluginVer;   // ACTUAL_VERSION z plugins.ver (pro detekci doinstalovani plug-inu)
    int LastPluginVerOP; // ACTUAL_VERSION z plugins.ver pro druhou platformu (x86/x64) - musime ukladat, jinak se nedozvime o premazani konfigurace druhou verzi, priklad: start x64, auto-save x64 s pridanym pictview, start x86, auto-save x86 s pridanym winscp (x86 pictview nepridala, uz byl v konfigu od x64), exit/save x86, a POZOR: exit/save x64 zrusi zaznam o winscp (x64 o winscp nic nevi a zustala bezet)

    // globalky
    BOOL ConfigWasImported; // ze souboru config.reg

    // custom icon overlays
    BOOL EnableCustomIconOverlays;    // TRUE = pouzivame icon overlays (viz ShellIconOverlays)
    char* DisabledCustomIconOverlays; // alokovany seznam zakazanych icon overlay handleru (oddelovac je ';', escape-sekvence pro ';' je ";;")

#ifndef _WIN64
    // FIXME_X64_WINSCP - tohle reseni neni OK, vykoumat lepsi (oddelit x86 a x64 verze + udelat shared data)
    BOOL AddX86OnlyPlugins; // TRUE = jeste jsme nestartovali x86 verzi Salama aktualni verze (poprve se musi pridat pluginy, ktere v x64 verzi chybi)
#endif                      // _WIN64

    CConfiguration();
    ~CConfiguration();

    void ClearHistory(); // promazne drzene vsechny historie

    int GetMainWindowIconIndex(); // vrati validni index do pole MainWindowIcons

    BOOL PrepareRecycleMasks(int& errorPos); // pripravi pro pouziti recycle-bin masky
    BOOL AgreeRecycleMasks(const char* fileName, const char* fileExt);

    DWORD LastFocusedPage;          // posledni navstivena stranka v dlg
    DWORD ConfigurationHeight;      // vyska konfiguracniho dialogu v bodech
    BOOL ViewersAndEditorsExpanded; // rozbaleni polozek ve strome
    BOOL PackersAndUnpackersExpanded;

    // Find dialog
    BOOL SearchFileContent;
    WINDOWPLACEMENT FindDialogWindowPlacement;
    int FindColNameWidth; // sirka sloupcu Name ve Find dialogu

    // Language
    char LoadedSLGName[MAX_PATH];    // xxxxx.slg, ktere se naloadilo pri startu Salamandera
    char SLGName[MAX_PATH];          // xxxxx.slg, ktere se priste pouzije pri startu Salamandera
    int DoNotDispCantLoadPluginSLG;  // TRUE = nezobrazovat warning o tom, ze neni mozne loadnout stejne pojmenovane SLG do pluginu jako do Salama
    int DoNotDispCantLoadPluginSLG2; // TRUE = nezobrazovat warning o tom, ze neni mozne loadnout SLG pluginu, ktere se pouzivalo posledne (user si ho vybral nebo bylo vybrano automaticky)
    int UseAsAltSLGInOtherPlugins;   // TRUE = zkusit pouzit AltSLGName pro pluginy
    char AltPluginSLGName[MAX_PATH]; // jen pokud je AltPluginSLGName TRUE: nahradni SLG modul pro pluginy (pro pripad, ze LoadedSLGName pro plugin neexistuje)

    // Nazev adresare convert\\XXX\\convert.cfg, ze ktereho se nacita convert.cfg
    char ConversionTable[MAX_PATH];

    int TitleBarShowPath;                        // budeme v titulku zobrazovat cestu?
    int TitleBarMode;                            // rezim zobrazeni title bar (TITLE_BAR_MODE_xxx)
    int UseTitleBarPrefix;                       // zobrazovat prefix v title bar?
    char TitleBarPrefix[TITLE_PREFIX_MAX];       // prefix pro title bar
    int UseTitleBarPrefixForced;                 // cmdline varianta, ma prednost a neuklada se
    char TitleBarPrefixForced[TITLE_PREFIX_MAX]; // cmdline varianta, ma prednost a neuklada se
    int MainWindowIconIndex;                     // index ikonky v poli MainWindowIcons[], 0=default
    int MainWindowIconIndexForced;               // cmdline varianta, ma prednost a neuklada se; -1 -- nenastaveno

    int ClickQuickRename; // kliknuti na focused polozku vyvola Quick Rename

    // bitova pole, jednotlive bity reprezentuji disky a..z
    // povolene hodnoty jsou 0 az 0x03FFFFFF (DRIVES_MASK)
    DWORD VisibleDrives;   // disky zobrazovany Salamanderem (nesouvisi s NoDrives polocii)
    DWORD SeparatedDrives; // disky, za ktere zobrazime v Alt+F1/2 menu separatory (zprehledneni)

    BOOL ShowSplashScreen; // ukazat splash screen behem startu
};

//
// ****************************************************************************

class CLoadSaveToRegistryMutex
{
protected:
    HANDLE Mutex;
    int DebugCheck;

public:
    CLoadSaveToRegistryMutex();
    ~CLoadSaveToRegistryMutex();

    void Init();

    void Enter();
    void Leave();
};

extern CLoadSaveToRegistryMutex LoadSaveToRegistryMutex; // mutex pro synchronizaci load/save do Registry (dva procesy najednou nemuzou, ma to neblahe vysledky)

//
// ****************************************************************************

class CCfgPageGeneral : public CCommonPropSheetPage
{
public:
    CCfgPageGeneral();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//
// ****************************************************************************

class CCfgPageRegional : public CCommonPropSheetPage
{
public:
    char SLGName[MAX_PATH];
    char DirName[MAX_PATH];

public:
    CCfgPageRegional();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LoadControls();
};

//
// ****************************************************************************

class CToolbarHeader;

class CCfgPageView : public CCommonPropSheetPage
{
protected:
    BOOL Dirty;
    CToolbarHeader* Header;
    HWND HListView;
    CToolbarHeader* Header2;
    HWND HListView2;
    CViewTemplates Config;
    BOOL DisableNotification;
    BOOL LabelEdit;
    int SelectIndex;

public:
    CCfgPageView(int index);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void OnModify();
    void OnDelete();
    void OnMove(BOOL up);

    void LoadControls();
    void StoreControls();
    void EnableControls();
    void EnableHeader();
    DWORD GetEnabledFunctions();

    BOOL IsDirty();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageIconOvrls : public CCommonPropSheetPage
{
protected:
    HWND HListView;

public:
    CCfgPageIconOvrls();

    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageSecurity : public CCommonPropSheetPage
{
public:
    CCfgPageSecurity();

    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

//class CColorButton;

class CCfgPageViewer : public CCommonPropSheetPage
{
protected:
    HFONT HFont;
    BOOL LocalUseCustomViewerFont;
    LOGFONT LocalViewerLogFont;
    CColorArrowButton* NormalText;
    CColorArrowButton* SelectedText;
    SALCOLOR TmpColors[NUMBER_OF_VIEWERCOLORS];

public:
    CCfgPageViewer();
    ~CCfgPageViewer();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void LoadControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
//
// ****************************************************************************

class CUserMenuItems;
class CEditListBox;
/*
class CSmallIconWindow : public CWindow
{
  protected:
    HICON HIcon;

  public:
    CSmallIconWindow(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID)
    {
      HIcon = NULL;
    }

    void SetIcon(HICON hIcon);

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/

class CCfgPageUserMenu : public CCommonPropSheetPage
{
public:
    CCfgPageUserMenu(CUserMenuItems* userMenuItems);
    ~CCfgPageUserMenu();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableButtons();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DeleteSubmenuEnd(int index); // k vybranemu otevreni submenu ('index') smaze konec

    void RefreshGroupIconInUMItems(); // po zmene barev se zmeni HGroupIcon, musime ho zmenit i v UserMenuItems

    CUserMenuItems* UserMenuItems;
    CUserMenuItems* SourceUserMenuItems;
    CEditListBox* EditLB;
    //    CSmallIconWindow *SmallIcon;
    BOOL DisableNotification;
};

//
// ****************************************************************************

class CHotPathItems;

class CCfgPageHotPath : public CCommonPropSheetPage
{
protected:
    BOOL Dirty;
    CToolbarHeader* Header;
    HWND HListView;
    CHotPathItems* Config;
    BOOL DisableNotification;
    BOOL EditMode;
    int EditIndex;
    BOOL LabelEdit; // editujem label

public:
    CCfgPageHotPath(BOOL editMode, int editIndex);
    ~CCfgPageHotPath();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void OnModify();
    void OnDelete();
    void OnMove(BOOL up);

    void LoadControls();
    void StoreControls();
    void EnableControls();
    void EnableHeader();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageSystem : public CCommonPropSheetPage
{
public:
    CCfgPageSystem();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);
    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

#define CFG_COLORS_BUTTONS 5

class CCfgPageColors : public CCommonPropSheetPage
{
protected:
    CColorArrowButton* Items[CFG_COLORS_BUTTONS];
    CColorArrowButton* Masks[CFG_COLORS_BUTTONS];

    HWND HScheme;
    HWND HItem;
    COLORREF TmpColors[NUMBER_OF_COLORS];

    CEditListBox* EditLB;
    BOOL DisableNotification;
    CHighlightMasks HighlightMasks;
    CHighlightMasks* SourceHighlightMasks;

    BOOL Dirty;

public:
    CCfgPageColors();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LoadColors();
    void LoadMasks();
    void StoreMasks();
    void EnableControls();
};

//
// ****************************************************************************

struct CConfirmationItem
{
    HTREEITEM HTreeItem;
    int* Variable;
    int Checked;
};

class CCfgPageConfirmations : public CCommonPropSheetPage
{
protected:
    HWND HTreeView;
    HTREEITEM HConfirmOn;
    HTREEITEM HShowMessage;
    HIMAGELIST HImageList;
    TDirectArray<CConfirmationItem> List;
    BOOL DisableNotification;

public:
    CCfgPageConfirmations();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InitTree();
    HIMAGELIST CreateImageList();
    HTREEITEM AddItem(HTREEITEM hParent, int iImage, int textResID, int* value);
    void SetItemChecked(HTREEITEM hTreeItem, BOOL checked);
    //BOOL GetItemChecked(HTREEITEM hTreeItem);
    int FindInList(HTREEITEM hTreeItem);
    void ChangeSelected();
};

//
// ****************************************************************************

class CCfgPageDrives : public CCommonPropSheetPage
{
protected:
    BOOL IfPathIsInaccessibleGoToChanged; // TRUE = user editoval cestu IfPathIsInaccessibleGoTo
    BOOL FocusIfPathIsInaccessibleGoTo;   // TRUE = provest focus editboxu "If Path Is Inaccessible Go To"

public:
    CCfgPageDrives(BOOL focusIfPathIsInaccessibleGoTo);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageViewEdit : public CCommonPropSheetPage
{
public:
    CCfgPageViewEdit();
};

//
// ****************************************************************************

class CCfgPageViewers : public CCommonPropSheetPage
{
protected:
    BOOL Alternative;

public:
    CCfgPageViewers(BOOL alternative = FALSE);

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL Dirty;
    CViewerMasks ViewerMasks;
    CViewerMasks* SourceViewerMasks;
    CEditListBox* EditLB;
    BOOL DisableNotification;
};

//
// ****************************************************************************

class CCfgPageEditors : public CCommonPropSheetPage
{
public:
    CCfgPageEditors();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL Dirty;
    CEditorMasks EditorMasks;
    CEditorMasks* SourceEditorMasks;
    CEditListBox* EditLB;
    BOOL DisableNotification;
};

//
// ****************************************************************************

class CCfgPageKeyboard : public CCommonPropSheetPage
{
public:
    CCfgPageKeyboard();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CConfigurationPageStats : public CCommonPropSheetPage
{
public:
    CConfigurationPageStats();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageArchivers : public CCommonPropSheetPage
{
public:
    CCfgPageArchivers();

    virtual void Transfer(CTransferInfo& ti);
};

//
// ****************************************************************************

class CPackerConfig;

class CCfgPagePackers : public CCommonPropSheetPage
{
protected:
    CPackerConfig* Config;
    CEditListBox* EditLB;
    BOOL DisableNotification;

public:
    CCfgPagePackers();
    ~CCfgPagePackers();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CUnpackerConfig;

class CCfgPageUnpackers : public CCommonPropSheetPage
{
protected:
    CUnpackerConfig* Config;
    CEditListBox* EditLB;
    BOOL DisableNotification;

public:
    CCfgPageUnpackers();
    ~CCfgPageUnpackers();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CArchiverConfig;

class CCfgPageExternalArchivers : public CCommonPropSheetPage
{
protected:
    CArchiverConfig* Config;
    BOOL DisableNotification;
    HWND HListbox;

public:
    CCfgPageExternalArchivers();
    ~CCfgPageExternalArchivers();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CPackerFormatConfig;

class CCfgPageArchivesAssoc : public CCommonPropSheetPage
{
protected:
    CPackerFormatConfig* Config;
    CEditListBox* EditLB;
    BOOL DisableNotification;

public:
    CCfgPageArchivesAssoc();
    ~CCfgPageArchivesAssoc();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
/*
class CCfgPageShellExt: public CCommonPropSheetPage
{
  protected:
    CEditListBox        *EditLB;
    BOOL                DisableNotification;

  public:
    CCfgPageShellExt();
    ~CCfgPageShellExt();

    virtual void Transfer(CTransferInfo &ti);
    virtual void Validate(CTransferInfo &ti);

  protected:
    void LoadControls();
    void StoreControls();
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};
*/
//
// ****************************************************************************

class CCfgPageAppearance : public CCommonPropSheetPage
{
protected:
    HFONT HPanelFont;
    BOOL LocalUseCustomPanelFont;
    LOGFONT LocalPanelLogFont;
    BOOL NotificationEnabled;

public:
    CCfgPageAppearance();
    ~CCfgPageAppearance();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    void LoadControls();
    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageMainWindow : public CCommonPropSheetPage
{
protected:
    HIMAGELIST HIconsList;

public:
    CCfgPageMainWindow();
    ~CCfgPageMainWindow();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

    void LoadControls();
    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL InitIconCombobox();
};

//
// ****************************************************************************

class CCfgPageChangeDrive : public CCommonPropSheetPage
{
protected:
    SIZE CharSize;

public:
    CCfgPageChangeDrive();

    virtual void Transfer(CTransferInfo& ti);

protected:
    void SetDrivesToListbox(int redID, DWORD drives);
    DWORD GetDrivesFromListbox(int redID);
    void InitList(int resID); // setup listbox

    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPagePanels : public CCommonPropSheetPage
{
public:
    CCfgPagePanels();

    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CCfgPageHistory : public CCommonPropSheetPage
{
public:
    CCfgPageHistory();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();

    // uzivatel chce promazat historie
    void OnClearHistory();
};

//
// ****************************************************************************

class CConfigurationDlg : public CTreePropDialog
{
public:
    // mode: 0 - normalni
    //       1 - hot paths
    CConfigurationDlg(HWND parent, CUserMenuItems* userMenuItems, int mode = 0, int param = 0);

public:
    CCfgPageGeneral PageGeneral;
    CCfgPageView PageView;
    CCfgPageViewer PageViewer;
    CCfgPageUserMenu PageUserMenu;
    CCfgPageHotPath PageHotPath;
    CCfgPageSystem PageSystem;
    CCfgPageColors PageColors;
    CCfgPageConfirmations PageConfirmations;
    CCfgPageDrives PageDrives;
    CCfgPageViewEdit PageViewEdit;
    CCfgPageViewers Page13;
    CCfgPageViewers Page14;
    CCfgPageEditors Page15;
    CCfgPageIconOvrls PageIconOvrls;
    CCfgPageAppearance PageAppear;
    CCfgPageMainWindow PageMainWindow;
    CCfgPageRegional PageRegional;
    CCfgPageHistory PageHistory;
    CCfgPageChangeDrive PageChangeDrive;
    CCfgPagePanels PagePanels;
    CCfgPageKeyboard PageKeyboard;
    CCfgPageSecurity PageSecurity;

    //    CCfgPageShellExt              PageShellExtensions; // ShellExtensions

    CCfgPageArchivers PagePP;
    CCfgPagePackers PageP1;
    CCfgPageUnpackers PageP2;
    CCfgPageExternalArchivers PageP3;
    CCfgPageArchivesAssoc PageP4;

    HWND HOldPluginMsgBoxParent;

protected:
    virtual void DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

BOOL ValidatePathIsNotEmpty(HWND hParent, const char* path);

extern CConfiguration Configuration;
