// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_fs) // so that the structures are independent of the current alignment setting
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

struct CFileData;
class CPluginFSInterfaceAbstract;
class CSalamanderDirectoryAbstract;
class CPluginDataInterfaceAbstract;

//
// ****************************************************************************
// CSalamanderForViewFileOnFSAbstract
//
// sada metod ze Salamandera pro podporu provedeni ViewFile v CPluginFSInterfaceAbstract,
// platnost interfacu je omezena na metodu, ktere je interface predan jako parametr

class CSalamanderForViewFileOnFSAbstract
{
public:
    // najde v disk-cache existujici kopii souboru nebo pokud jeste kopie souboru neni
    // v disk-cache, rezervuje pro ni jmeno (cilovy soubor napr. pro download z FTP);
    // 'uniqueFileName' je unikatni nazev originalniho souboru (podle tohoto nazvu
    // se prohledava disk-cache; melo by stacit plne jmeno souboru v salamanderovske
    // forme - "fs-name:fs-user-part"; POZOR: nazev se porovnava "case-sensitive", pokud
    // plugin vyzaduje "case-insensitive", musi vsechny nazvy prevadet napr. na mala
    // pismena - viz CSalamanderGeneralAbstract::ToLowerCase); 'nameInCache' je jmeno
    // kopie souboru, ktera je umistena v disk-cache (ocekava se zde posledni
    // cast jmena originalniho souboru, aby pozdeji v titulku viewru uzivateli pripominala
    // originalni soubor); je-li 'rootTmpPath' NULL, je diskova cache ve Windows TEMP
    // adresari, jinak je cesta do disk-cache v 'rootTmpPath'; pri systemove chybe vraci
    // NULL (nemelo by vubec nastat), jinak vraci plne jmeno kopie souboru v disk-cache
    // a ve 'fileExists' vraci TRUE pokud soubor v disk-cache existuje (napr. uz probehl
    // download z FTP) nebo FALSE pokud je soubor teprve potreba pripravit (napr. provest
    // jeho download); 'parent' je parent messageboxu s chybami (napriklad prilis dlouhy
    // nazev souboru)
    // POZOR: pokud nevratila NULL (nenastala systemova chyba), je nutne pozdeji zavolat
    //        FreeFileNameInCache (pro stejne 'uniqueFileName')
    // POZN.: pokud FS vyuziva disk-cache, mel by alespon pri unloadu pluginu zavolat
    //        CSalamanderGeneralAbstract::RemoveFilesFromCache("fs-name:"), jinak budou
    //        jeho kopie souboru zbytecne prekazet v disk-cache
    virtual const char* WINAPI AllocFileNameInCache(HWND parent, const char* uniqueFileName, const char* nameInCache,
                                                    const char* rootTmpPath, BOOL& fileExists) = 0;

    // otevre soubor 'fileName' z windowsove cesty v uzivatelem pozadovanem prohlizeci (bud
    // pomoci asociace viewru nebo pres prikaz View With); 'parent' je parent messageboxu
    // s chybami; neni-li 'fileLock' a 'fileLockOwner' NULL, vraci se v nich vazba na
    // otevreny viewer (pouzije se jako parametr metody FreeFileNameInCache); vraci TRUE
    // pokud byl viewer otevren
    virtual BOOL WINAPI OpenViewer(HWND parent, const char* fileName, HANDLE* fileLock,
                                   BOOL* fileLockOwner) = 0;

    // musi parovat s AllocFileNameInCache, vola se az po otevreni vieweru (nebo po chybe pri
    // priprave kopie souboru nebo otevirani vieweru); 'uniqueFileName' je unikatni nazev
    // originalniho souboru (pouzivat stejny retezec jako pri volani AllocFileNameInCache);
    // 'fileExists' je FALSE pokud kopie souboru v disk-cache neexistovala a TRUE pokud
    // jiz existovala (shodna hodnota s vystupnim parametrem 'fileExists' metody AllocFileNameInCache);
    // je-li 'fileExists' TRUE, 'newFileOK' a 'newFileSize' se ignoruji, jinak 'newFileOK' je
    // TRUE pokud byla kopie souboru uspesne pripravena (napr. download probehl uspesne) a
    // v 'newFileSize' je velikost pripravene kopie souboru; je-li 'newFileOK' FALSE, je
    // 'newFileSize' ignorovana; 'fileLock' a 'fileLockOwner' provazuji otevreny viewer
    // s kopiemi souboru v disk-cache (po zavreni vieweru disk-cache dovoli zrusit kopii
    // souboru - kdy dojde ke zruseni kopie zalezi na velikosti disk-cache na disku), oba
    // tyto parametry lze ziskat pri volani metody OpenViewer; pokud se viewer
    // nepodarilo otevrit (nebo se nepodarilo pripravit kopii souboru do disk-cache nebo viewer
    // nema vazbu s disk-cache), nastavi se 'fileLock' na NULL a 'fileLockOwner' na FALSE;
    // je-li 'fileExists' TRUE (kopie souboru existovala), hodnota 'removeAsSoonAsPossible'
    // se ignoruje, jinak: je-li 'removeAsSoonAsPossible' TRUE, kopie souboru se v disk-cache
    // nebude skladovat dele nez je nutne (po zavreni vieweru dojde hned k vymazu; pokud se
    // viewer vubec neotevrel ('fileLock' je NULL), nedojde ke vlozeni souboru do disk-cache,
    // ale k vymazu)
    virtual void WINAPI FreeFileNameInCache(const char* uniqueFileName, BOOL fileExists, BOOL newFileOK,
                                            const CQuadWord& newFileSize, HANDLE fileLock,
                                            BOOL fileLockOwner, BOOL removeAsSoonAsPossible) = 0;
};

//
// ****************************************************************************
// CPluginFSInterfaceAbstract
//
// sada metod pluginu, ktere potrebuje Salamander pro praci s file systemem

// typ ikon v panelu pri listovani FS (pouziva se v CPluginFSInterfaceAbstract::ListCurrentPath())
#define pitSimple 0       // simple icons for files and directories - by extension (association)
#define pitFromRegistry 1 // icons loaded from the registry according to the file/directory extension
#define pitFromPlugin 2   // the plugin provides the icons (obtained via CPluginDataInterfaceAbstract)

// FS event codes (and the meaning of parameter 'param'), received by CPluginFSInterfaceAbstract::Event():
// CPluginFSInterfaceAbstract::TryCloseOrDetach returned TRUE, but the new path could not be opened, so we stay on the current path (the FS that receives this message);
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_CLOSEORDETACHCANCELED 0

// successful attachment of a new FS to the panel (after the path is changed and listed)
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_OPENED 1

// successful addition to the list of detached FSs (end of "panel" FS mode, start of "detached" FS mode);
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_DETACHED 2

// successful attachment of a detached FS (end of "detached" FS mode, start of "panel" FS mode);
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_ATTACHED 3

// activation of Salamander's main window (when the window is minimized, this event is sent only after restore/maximize so any error windows appear above Salamander),
// it is delivered only to an FS that is in a panel (not detached); if changes on the FS are not monitored automatically,
// this event indicates a suitable time to refresh;
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_ACTIVATEREFRESH 4

// vyprsel timeout jednoho z timeru tohoto FS, 'param' je parametr tohoto timeru;
// POZOR: metoda CPluginFSInterfaceAbstract::Event() s kodem FSE_TIMER se vola
// z hlavniho threadu po doruceni zpravy WM_TIMER hlavnimu oknu (tedy napr. muze
// byt zrovna otevreny libovolny modalni dialog), takze reakce na timer by mela
// probehnout v tichosti (neotevirat zadna okna, atd.); k volani metody
// CPluginFSInterfaceAbstract::Event() s kodem FSE_TIMER muze dojit hned po
// volani metody CPluginInterfaceForFS::OpenFS (pokud se v ni prida timer pro
// nove vytvoreny objekt FS)
#define FSE_TIMER 5

// prave probehla zmena cesty (nebo refresh) v tomto FS v panelu nebo pripojeni
// tohoto odpojeneho FS do panelu (tato udalost se posila po zmene cesty a jejim
// vylistovani); FSE_PATHCHANGED chodi po kazdem uspesnem volani ListCurrentPath
// POZNAMKA: FSE_PATHCHANGED tesne nasleduje za vsemi FSE_OPENED a FSE_ATTACHED
// 'param' je panel obsahujici tento FS (PANEL_LEFT nebo PANEL_RIGHT)
#define FSE_PATHCHANGED 6

// konstanty oznacujici duvod volani CPluginFSInterfaceAbstract::TryCloseOrDetach();
// v zavorce jsou vzdy uvedeny mozne hodnoty forceClose ("FALSE->TRUE" znamena "nejdriv
// to zkusi bez force, kdyz FS odmitne, zepta se usera a pripadne to da s force") a canDetach:
//
// (FALSE, TRUE) pri zmene cesty mimo FS otevrene v panelu
#define FSTRYCLOSE_CHANGEPATH 1
// (FALSE->TRUE, FALSE) for an FS open in a panel during plugin unload (user-requested unload + Salamander shutdown + before removing the plugin + unload requested by the plugin)
#define FSTRYCLOSE_UNLOADCLOSEFS 2
// (FALSE, TRUE) when changing the path or refreshing (Ctrl+R) an FS open in a panel, it was found that no accessible path on the FS remains - Salamander tries to change the panel path to a fixed drive (if the FS does not allow it, the FS remains in the panel with no files or directories)
#define FSTRYCLOSE_CHANGEPATHFAILURE 3
// (FALSE, FALSE) when reattaching a detached FS to a panel, it was found that no path on this FS is accessible any more - Salamander tries to close this detached FS (if the FS refuses, it remains in the list of detached FSs - e.g. in the Alt+F1/F2 menu)
#define FSTRYCLOSE_ATTACHFAILURE 4
// (FALSE->TRUE, FALSE) for a detached FS during plugin unload (user-requested unload + Salamander shutdown + before removing the plugin + unload requested by the plugin)
#define FSTRYCLOSE_UNLOADCLOSEDETACHEDFS 5
// (FALSE, FALSE) the plugin called CSalamanderGeneral::CloseDetachedFS() for a detached FS
#define FSTRYCLOSE_PLUGINCLOSEDETACHEDFS 6

// flags indicating which file-system services the plugin provides - which methods of
// CPluginFSInterfaceAbstract are defined:
// copy from FS (F5 on FS)
#define FS_SERVICE_COPYFROMFS 0x00000001
// move from FS + rename on FS (F6 on FS)
#define FS_SERVICE_MOVEFROMFS 0x00000002
// copy from disk to FS (F5 on disk)
#define FS_SERVICE_COPYFROMDISKTOFS 0x00000004
// move from disk to FS (F6 on disk)
#define FS_SERVICE_MOVEFROMDISKTOFS 0x00000008
// delete on FS (F8)
#define FS_SERVICE_DELETE 0x00000010
// quick rename on FS (F2)
#define FS_SERVICE_QUICKRENAME 0x00000020
// view from FS (F3)
#define FS_SERVICE_VIEWFILE 0x00000040
// edit from FS (F4)
#define FS_SERVICE_EDITFILE 0x00000080
// edit new file from FS (Shift+F4)
#define FS_SERVICE_EDITNEWFILE 0x00000100
// change attributes on FS (Ctrl+F2)
#define FS_SERVICE_CHANGEATTRS 0x00000200
// create directory on FS (F7)
#define FS_SERVICE_CREATEDIR 0x00000400
// show info about FS (Ctrl+F1)
#define FS_SERVICE_SHOWINFO 0x00000800
// show properties on FS (Alt+Enter)
#define FS_SERVICE_SHOWPROPERTIES 0x00001000
// calculate occupied space on FS (Alt+F10 + Ctrl+Shift+F10 + calc. needed space + spacebar key in panel)
#define FS_SERVICE_CALCULATEOCCUPIEDSPACE 0x00002000
// command line for FS (otherwise command line is disabled)
#define FS_SERVICE_COMMANDLINE 0x00008000
// get free space on FS (number in directory line)
#define FS_SERVICE_GETFREESPACE 0x00010000
// get icon of FS (icon in directory line or Disconnect dialog)
#define FS_SERVICE_GETFSICON 0x00020000
// get next directory-line FS hot-path (for shortening of current FS path in panel)
#define FS_SERVICE_GETNEXTDIRLINEHOTPATH 0x00040000
// context menu on FS (Shift+F10)
#define FS_SERVICE_CONTEXTMENU 0x00080000
// get item for change drive menu or Disconnect dialog (item for active/detached FS in Alt+F1/F2 or Disconnect dialog)
#define FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM 0x00100000
// accepts change on path notifications from Salamander (see PostChangeOnPathNotification)
#define FS_SERVICE_ACCEPTSCHANGENOTIF 0x00200000
// get path for main-window title (text in window caption) (see Configuration/Appearance/Display current path...)
// if it's not defined, full path is displayed in window caption in all display modes
#define FS_SERVICE_GETPATHFORMAINWNDTITLE 0x00400000
// Find (Alt+F7 on FS) - if it's not defined, standard Find Files and Directories dialog
// is opened even if FS is opened in panel
#define FS_SERVICE_OPENFINDDLG 0x00800000
// open active folder (Shift+F3)
#define FS_SERVICE_OPENACTIVEFOLDER 0x01000000
// show security information (click on security icon in Directory Line, see CSalamanderGeneralAbstract::ShowSecurityIcon)
#define FS_SERVICE_SHOWSECURITYINFO 0x02000000

// Missing: Change Case, Convert, Properties, Make File List

// types of context menus for CPluginFSInterfaceAbstract::ContextMenu()
#define fscmItemsInPanel 0 // context menu for items in the panel (selected/focused files and directories)
#define fscmPathInPanel 1  // context menu for the current path in the panel
#define fscmPanel 2        // context menu for the panel

#define SALCMDLINE_MAXLEN 8192 // maximum command length from the Salamander command line

class CPluginFSInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginFSInterfaceEncapsulation)
    friend class CPluginFSInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns the user-part of the current path in this FS; 'userPart' is a buffer of size MAX_PATH
    // for the path; returns TRUE on success
    virtual BOOL WINAPI GetCurrentPath(char* userPart) = 0;

    // returns the user-part of the full name of file/directory/up-dir 'file' ('isDir' is 0/1/2) on the current
    // path in this FS; for the up-dir entry (first in the directory list and also named ".."),
    // 'isDir'==2 and the method should return the current path shortened by the last component; 'buf'
    // is a buffer of size 'bufSize' for the resulting full name; returns TRUE on success
    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize) = 0;

    // vraci absolutni cestu (vcetne fs-name) odpovidajici relativni ceste 'path' na tomto FS;
    // vraci FALSE pokud tato metoda neni implementovana (dalsi navratove hodnoty se pak ignoruji);
    // 'parent' je parent pripadnych messageboxu; 'fsName' je aktualni jmeno FS; 'path' je buffer
    // o velikosti 'pathSize' znaku, na vstupu je v nem relativni cesta na FS, na vystupu je v nem
    // odpovidajici absolutni cesta na FS; v 'success' vraci TRUE, pokud byla cesta uspesne prelozena
    // (ma se pouzit retezec v 'path' - jinak se ignoruje) - nasleduje zmena cesty (jde-li
    // o cestu na toto FS, vola se ChangePath()); pokud vrati v 'success' FALSE, predpoklada
    // se, ze uzivatel jiz videl chybove hlaseni
    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize,
                                      BOOL& success) = 0;

    // returns the user-part of the root of the current path in this FS; 'userPart' is a buffer of size MAX_PATH
    // for the path (used by the "goto root" function); returns TRUE on success
    virtual BOOL WINAPI GetRootPath(char* userPart) = 0;

    // compares the current path in this FS with the path specified by 'fsNameIndex' and 'userPart'
    // (the FS name in the path belongs to this plugin and is identified by 'fsNameIndex'); returns TRUE
    // if the paths are identical; 'currentFSNameIndex' is the index of the current FS name
    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart) = 0;

    // vraci TRUE, pokud je cesta z tohoto FS (coz znamena, ze Salamander muze cestu pustit
    // do ChangePath tohoto FS); cesta je vzdy na jeden z FS tohoto pluginu (napr. windows
    // cesty a cesty do archivu sem vubec neprijdou); 'fsNameIndex' je index jmena FS
    // v ceste (index je nula pro fs-name zadane v CSalamanderPluginEntryAbstract::SetBasicPluginData,
    // u ostatnich fs-name index vraci CSalamanderPluginEntryAbstract::AddFSName); user-part
    // cesty je 'userPart'; 'currentFSNameIndex' je index aktualniho jmena FS
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart) = 0;

    // zmeni aktualni cestu v tomto FS na cestu zadanou pres 'fsName' a 'userPart' (presne
    // nebo na nejblizsi pristupnou podcestu 'userPart' - viz hodnota 'mode'); v pripade, ze
    // se cesta zkracuje z duvodu, ze jde o cestu k souboru (staci domenka, ze by mohlo jit
    // o cestu k souboru - po vylistovani cesty se overuje jestli soubor existuje, pripadne
    // se zobrazi uzivateli chyba) a 'cutFileName' neni NULL (mozne jen v 'mode' 3), vraci
    // v bufferu 'cutFileName' (o velikosti MAX_PATH znaku) jmeno tohoto souboru (bez cesty),
    // jinak v bufferu 'cutFileName' vraci prazdny retezec; 'currentFSNameIndex' je index
    // aktualniho jmena FS; 'fsName' je buffer o velikosti MAX_PATH, na vstupu je v nem jmeno
    // FS v ceste, ktere je z tohoto pluginu (ale nemusi se shodovat s aktualnim jmenem FS
    // v tomto objektu, staci kdyz pro nej IsOurPath() vraci TRUE), na vystupu je v 'fsName'
    // aktualni jmeno FS v tomto objektu (musi byt z tohoto pluginu); 'fsNameIndex' je index
    // jmena FS 'fsName' v pluginu (pro snazsi detekci o jake jmeno FS jde); neni-li
    // 'pathWasCut' NULL, vraci se v nem TRUE pokud doslo ke zkraceni cesty; Salamander
    // pouziva 'cutFileName' a 'pathWasCut' u prikazu Change Directory (Shift+F7) pri zadani
    // jmena souboru - dochazi k fokusu tohoto souboru; je-li 'forceRefresh' TRUE, jde o
    // tvrdy refresh (Ctrl+R) a plugin by mel menit cestu bez pouziti informaci z cache
    // (je nutne overit jestli nova cesta existuje); 'mode' je rezim zmeny cesty:
    //   1 (refresh path) - zkracuje cestu, je-li treba; nehlasit neexistenci cesty (bez hlaseni
    //                      zkratit), hlasit soubor misto cesty, nepristupnost cesty a dalsi chyby
    //   2 (volani ChangePanelPathToPluginFS, back/forward in history, etc.) - zkracuje cestu,
    //                      je-li treba; hlasit vsechny chyby cesty (soubor
    //                      misto cesty, neexistenci, nepristupnost a dalsi)
    //   3 (change-dir command) - zkracuje cestu jen jde-li o soubor nebo cestu nelze listovat
    //                      (ListCurrentPath pro ni vraci FALSE); nehlasit soubor misto cesty
    //                      (bez hlaseni zkratit a vratit jmeno souboru), hlasit vsechny ostatni
    //                      chyby cesty (neexistenci, nepristupnost a dalsi)
    // je-li 'mode' 1 nebo 2, vraci FALSE jen pokud na tomto FS zadna cesta neni pristupna
    // (napr. pri vypadku spojeni); je-li 'mode' 3, vraci FALSE pokud neni pristupna
    // pozadovana cesta nebo soubor (ke zkracovani cesty dojde jen v pripade, ze jde o soubor);
    // v pripade, ze je otevreni FS casove narocne (napr. pripojeni na FTP server) a 'mode'
    // je 3, je mozne upravit chovani jako u archivu - zkracovat cestu, je-li treba a vracet FALSE
    // jen pokud na FS neni zadna cesta pristupna, hlaseni chyb se nemeni
    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                   const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                   BOOL forceRefresh, int mode) = 0;

    // nacita soubory a adresare z aktualni cesty, uklada je do objektu 'dir' (na cestu NULL nebo
    // "", soubory a adresare na jinych cestach jsou ignorovany; je-li pridan adresar se jmenem
    // "..", vykresluje se jako "up-dir" symbol; jmena souboru a adresaru jsou plne
    // zavisla na pluginu, Salamander je jen zobrazuje); Salamander zjisti obsah
    // pluginem pridanych sloupcu pomoci interfacu 'pluginData' (pokud plugin sloupce nepridava
    // a nema vlastni ikony, vraci 'pluginData'==NULL); v 'iconsType' vraci pozadovany zpusob
    // ziskavani ikon souboru a adresaru do panelu, pitFromPlugin degraduje na pitSimple pokud
    // je 'pluginData' NULL (bez 'pluginData' nelze zajistit pitFromPlugin); je-li 'forceRefresh'
    // TRUE, jde o tvrdy refresh (Ctrl+R) a plugin by mel nacitat soubory a adresare bez pouziti
    // cache; vraci TRUE pri uspesnem nacteni, pokud vrati FALSE jde o chybu a bude se volat
    // ChangePath na aktualni cestu, ocekava se, ze ChangePath vybere pristupnou podcestu
    // nebo vrati FALSE, po uspesnem volani ChangePath se bude opakovat volani ListCurrentPath;
    // pokud vrati FALSE, navratova hodnota 'pluginData' se ignoruje (data v 'dir' je potreba
    // uvolnit pomoci 'dir.Clear(pluginData)', jinak se uvolni jen Salamanderovska cast dat);
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                        CPluginDataInterfaceAbstract*& pluginData,
                                        int& iconsType, BOOL forceRefresh) = 0;

    // priprava FS na zavreni/odpojeni od panelu nebo zavreni odpojeneho FS; je-li 'forceClose'
    // TRUE, dojde k zavreni FS bez ohledu na navratove hodnoty, akci vynutil user nebo probiha
    // critical shutdown (vice viz CSalamanderGeneralAbstract::IsCriticalShutdown), kazdopadne
    // nema smysl se usera na cokoliv ptat, FS se ma proste hned zavrit (uz neotvirat zadna okna);
    // je-li 'forceClose' FALSE, muze se FS zavrit nebo odpojit ('canDetach' TRUE) a nebo jen
    // zavrit ('canDetach' FALSE); v 'detach' vraci TRUE pokud se chce jen odpojit, FALSE znamena
    // zavrit; 'reason' obsahuje duvod volani teto metody (jedna z FSTRYCLOSE_XXX); vraci TRUE
    // pokud lze zavrit/odpojit, jinak vraci FALSE
    virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason) = 0;

    // receives an event on this FS; see the FSE_XXX event codes; 'param' is the event parameter
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // releases all FS resources except the listing data (the listing
    // may still be displayed in the panel while this method is running); called immediately before the listing in the panel
    // is destroyed
    // (the listing is destroyed only for active FSs; detached FSs have no listing) and before CloseFS is called for this FS;
    // 'parent' is the parent for any message boxes; if critical shutdown is in progress (see
    // CSalamanderGeneralAbstract::IsCriticalShutdown), do not show any windows
    virtual void WINAPI ReleaseObject(HWND parent) = 0;

    // returns the set of supported FS services (see the FS_SERVICE_XXX constants); returns the bitwise
    // OR of the constants; called after this FS is opened (see CPluginInterfaceForFSAbstract::OpenFS),
    // and after each call to ChangePath and ListCurrentPath on this FS
    virtual DWORD WINAPI GetSupportedServices() = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM:
    // ziskani polozky pro tento FS (aktivni nebo odpojeny) do Change Drive menu (Alt+F1/F2)
    // nebo Disconnect dialogu (hotkey: F12; pripadny disconnect tohoto FS zajisti metoda
    // CPluginInterfaceForFSAbstract::DisconnectFS; pokud GetChangeDriveOrDisconnectItem vrati
    // FALSE a FS je v panelu, prida se polozka s ikonou ziskanou pres GetFSIcon a root cestou);
    // je-li navratova hodnota TRUE, prida se polozka s ikonou 'icon' a textem 'title';
    // 'fsName' je aktualni jmeno FS; je-li 'icon' NULL, nema polozka ikonu; je-li
    // 'destroyIcon' TRUE a 'icon' neni NULL, uvolni se 'icon' po pouziti pres Win32 API
    // funkci DestroyIcon; 'title' je text alokovany na heapu Salamandera a muze obsahovat
    // az tri sloupce vzajemne oddelene '\t' (viz Alt+F1/F2 menu), v Disconnect dialogu se
    // pouziva jen druhy sloupec; je-li navratova hodnota FALSE, jsou navratove hodnoty
    // 'title', 'icon' a 'destroyIcon' ignorovany (neprida se polozka)
    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title,
                                                       HICON& icon, BOOL& destroyIcon) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_GETFSICON:
    // ziskani ikony FS pro directory-line toolbaru nebo pripadne pro Disconnect dialog (F12);
    // ikona pro Disconnect dialog se zde ziskava jen pokud pro tento FS metoda
    // GetChangeDriveOrDisconnectItem nevraci polozku (napr. RegEdit a WMobile);
    // vraci ikonu nebo NULL pokud se ma pouzit standardni ikona; je-li 'destroyIcon' TRUE
    // a vraci ikonu (ne NULL), uvolni se vracena ikona po pouziti pres Win32 API
    // funkci DestroyIcon
    // Pozor: pokud je resource ikonky nacitane pomoci LoadIcon v rozmerech 16x16, vrati LoadIcon
    //        ikonku 32x32. Pri jejim naslednem kresleni do 16x16 vzniknou kolem ikonky barevne
    //        kontury. Konverzi 16->32->16 lze predejit pouzitim LoadImage:
    //        (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    //
    // v teto metode se nesmi zobrazovat zadna okna (obsah panelu neni konzistentni, nesmi
    // se distribuovat zpravy - prekresleni, atd.)
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon) = 0;

    // returns the requested drop effect for a drag&drop operation from an FS (possibly this FS) to this FS;
    // 'srcFSPath' is the source path; 'tgtFSPath' is the target path (it belongs to this FS); 'allowedEffects'
    // contains the allowed drop effects; 'keyState' is the key state (a combination of MK_CONTROL,
    // MK_SHIFT, MK_ALT, MK_BUTTON, MK_LBUTTON, MK_MBUTTON, and MK_RBUTTON flags; see IDropTarget::Drop);
    // 'dropEffect' contains the recommended drop effects (equal to 'allowedEffects' or limited to
    // DROPEFFECT_COPY or DROPEFFECT_MOVE if the user holds Ctrl or Shift) and
    // returns the selected drop effect (DROPEFFECT_COPY, DROPEFFECT_MOVE, or DROPEFFECT_NONE);
    // if the method does not change 'dropEffect' and it contains more than one effect,
    // Copy is preferred
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_GETFREESPACE:
    // vraci v 'retValue' (nesmi byt NULL) velikost volneho mista na FS (zobrazuje se
    // vpravo na directory-line); pokud volne misto nelze zjistit, vraci
    // CQuadWord(-1, -1) (udaj se nezobrazuje)
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_GETNEXTDIRLINEHOTPATH:
    // nalezeni delicich bodu v textu Directory Line (pro zkracovani cesty pomoci mysi - hot-tracking);
    // 'text' je text v Directory Line (cesta + pripadne filter); 'pathLen' je delka cesty v 'text'
    // (zbytek je filtr); 'offset' je offset znaku, od ktereho se ma hledat delici bod; vraci TRUE
    // pokud dalsi delici bod existuje, jeho pozici vraci v 'offset'; vraci FALSE pokud zadny dalsi
    // delici bod neexistuje (konec textu neni povazovan za delici bod)
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETNEXTDIRLINEHOTPATH:
    // adjusts the shortened path text to be displayed in the panel (Directory Line - path shortening with the mouse, hot-tracking); used when the hot text from the Directory Line does not exactly match the path (for example, if it is missing the closing bracket - VMS paths on FTP - "[DIR1.DIR2.DIR3]");
    // 'path' is an in/out buffer containing the path (the buffer size is 'pathBufSize')
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_GETPATHFORMAINWNDTITLE:
    // ziskani textu, ktery se zobrazi v titulku hlavniho okna pokud je zapnute zobrazovani
    // aktualni cesty v titulku hlavniho okna (viz Configuration/Appearance/Display current
    // path...); 'fsName' je aktualni jmeno FS; je-li 'mode' 1, jde o rezim
    // "Directory Name Only" (ma se zobrazit jen jmeno aktualniho adresare - posledni
    // komponenty cesty); je-li 'mode' 2, jde o rezim "Shortened Path" (ma se zobrazit
    // zkracena forma cesty - root (vc. oddelovace cesty) + "..." + oddelovac
    // cesty + posledni komponenta cesty); 'buf' je buffer o velikosti 'bufSize' pro
    // vysledny text; vraci TRUE pokud vraci pozadovany text; vraci FALSE pokud se
    // ma text vytvorit na zaklade udaju o delicich bodech ziskanych pres metodu
    // GetNextDirectoryLineHotPath()
    // POZNAMKA: pokud GetSupportedServices() nevraci i FS_SERVICE_GETPATHFORMAINWNDTITLE,
    //           zobrazuje se v titulku hlavniho okna plna cesta na FS ve vsech rezimech
    //           zobrazovani titulku (i v "Directory Name Only" a "Shortened Path")
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_SHOWINFO:
    // displays a dialog with information about the FS (free space, capacity, name, capabilities, etc.);
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the displayed dialog
    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_COMMANDLINE:
    // spusti prikaz pro FS v aktivnim panelu z prikazove radky pod panely; vraci FALSE pri chybe
    // (prikaz se nevklada do historie prikazove radky a ostatni navratove hodnoty se ignoruji);
    // vraci TRUE pri uspesnem spusteni prikazu (pozor: na vysledcich prikazu nezalezi - dulezite
    // je jen jestli byl spusteny (napr. u FTP jde o to, jestli se ho podarilo dorucit serveru));
    // 'parent' je navrzeny parent pripadnych zobrazovanych dialogu; 'command' je buffer
    // velikosti SALCMDLINE_MAXLEN+1, ktery na vstupu obsahuje spousteny prikaz (skutecna
    // maximalni delka prikazu zavisi na verzi Windows a obsahu promenne prostredi COMSPEC)
    // a na vystupu novy obsah prikazove radky (obvykle se jen vycisti na prazdny retezec);
    // 'selFrom' a 'selTo' vraci pozici oznaceni v novem obsahu prikazove radky (pokud se shoduji,
    // jen se umisti kurzor; je-li vystupem prazdna radka, jsou tyto hodnoty ignorovany)
    // POZOR: tato metoda by nemela primo menit cestu v panelu - hrozi uzavreni FS pri chybe cesty
    //        (metode by prestal existovat ukazatel this)
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_QUICKRENAME:
    // rychle prejmenovani souboru nebo adresare ('isDir' je FALSE/TRUE) 'file' na FS;
    // umozni otevrit vlastni dialog pro rychle prejmenovani (parametr 'mode' je 1)
    // nebo pouzit standardni dialog (pri 'mode'==1 vrati FALSE a 'cancel' take FALSE,
    // pak Salamander otevre standardni dialog a ziskane nove jmeno preda v 'newName' pri
    // dalsim volani QuickRename s 'mode'==2); 'fsName' je aktualni jmeno FS; 'parent' je
    // navrzeny parent pripadnych zobrazovanych dialogu; 'newName' je nove jmeno pokud
    // 'mode'==2; pokud vraci TRUE, v 'newName' se vraci nove jmeno (max. MAX_PATH znaku;
    // ne plne jmeno, jen jmeno polozky v panelu) - Salamander se ho pokusi vyfokusit po
    // refreshi (o refresh se stara sam FS, napriklad pomoci metody
    // CSalamanderGeneralAbstract::PostRefreshPanelFS); pokud vraci FALSE a 'mode'==2,
    // vraci se v 'newName' chybne nove jmeno (pripadne nejakym zpusobem upravene - napr.
    // uz muze byt aplikovana operacni maska) pokud chce uzivatel prerusit operaci, vraci
    // 'cancel' TRUE; vraci-li 'cancel' FALSE, vraci metoda TRUE pri uspesnem dokonceni
    // operace, pokud vrati FALSE pri 'mode'==1, ma se otevrit standardni dialog pro
    // rychle prejmenovani, pokud vrati FALSE pri 'mode'==2, jde o chybu operace (chybne
    // nove jmeno se vraci v 'newName' - znovu se otevre standardni dialog a uzivatel zde
    // muze chybne jmeno opravit)
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file,
                                    BOOL isDir, char* newName, BOOL& cancel) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_ACCEPTSCHANGENOTIF:
    // prijem informace o zmene na ceste 'path' (je-li 'includingSubdirs' TRUE, tak
    // zahrnuje i zmenu v podadresarich cesty 'path'); tato metoda by mela rozhodnout
    // jestli je treba provest refresh tohoto FS (napriklad pomoci metody
    // CSalamanderGeneralAbstract::PostRefreshPanelFS); tyka se jak aktivnich FS, tak
    // odpojenych FS; 'fsName' je aktualni jmeno FS
    // POZNAMKA: pro plugin jako celek existuje metoda
    //           CPluginInterfaceAbstract::AcceptChangeOnPathNotification()
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path,
                                                       BOOL includingSubdirs) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_CREATEDIR:
    // vytvoreni noveho adresare na FS; umozni otevrit vlastni dialog pro vytvoreni
    // adresare (parametr 'mode' je 1) nebo pouzit standardni dialog (pri 'mode'==1 vrati
    // FALSE a 'cancel' take FALSE, pak Salamander otevre standardni dialog a ziskane jmeno
    // adresare preda v 'newName' pri dalsim volani CreateDir s 'mode'==2);
    // 'fsName' je aktualni jmeno FS; 'parent' je navrzeny parent pripadnych zobrazovanych
    // dialogu; 'newName' je jmeno noveho adresare pokud 'mode'==2; pokud vraci TRUE,
    // v 'newName' se vraci jmeno noveho adresare (max. 2 * MAX_PATH znaku; ne plne jmeno,
    // jen jmeno polozky v panelu) - Salamander se ho pokusi vyfokusit po refreshi (o refresh
    // se stara sam FS, napriklad pomoci metody CSalamanderGeneralAbstract::PostRefreshPanelFS);
    // pokud vraci FALSE a 'mode'==2, vraci se v 'newName' chybne jmeno adresare (max. 2 * MAX_PATH
    // znaku, pripadne upravene na absolutni tvar); pokud chce uzivatel prerusit operaci,
    // vraci 'cancel' TRUE; vraci-li 'cancel' FALSE, vraci metoda TRUE pri uspesnem dokonceni
    // operace, pokud vrati FALSE pri 'mode'==1, ma se otevrit standardni dialog pro vytvoreni
    // adresare, pokud vrati FALSE pri 'mode'==2, jde o chybu operace (chybne jmeno
    // adresare se vraci v 'newName' - znovu se otevre standardni dialog a uzivatel
    // zde muze chybne jmeno opravit)
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent,
                                  char* newName, BOOL& cancel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_VIEWFILE:
    // views file 'file' (directories cannot be viewed with the View command) on the current path
    // on the FS; 'fsName' is the current FS name; 'parent' is the parent of any message boxes
    // for errors; 'salamander' is the set of Salamander methods required to implement
    // viewing with caching
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_DELETE:
    // mazani souboru a adresaru oznacenych v panelu; umozni otevrit vlastni dialog s dotazem
    // na mazani (parametr 'mode' je 1; jestli se ma nebo nema zobrazit dotaz zavisi na hodnote
    // SALCFG_CNFRMFILEDIRDEL - TRUE znamena, ze uzivatel chce potvrzovat mazani)
    // nebo pouzit standardni dotaz (pri 'mode'==1 vrati FALSE a 'cancelOrError' take FALSE,
    // pak Salamander otevre standardni dotaz (pokud je SALCFG_CNFRMFILEDIRDEL TRUE)
    // a v pripade kladne odpovedi znovu zavola Delete s 'mode'==2); 'fsName' je aktualni jmeno FS;
    // 'parent' je navrzeny parent pripadnych zobrazovanych dialogu; 'panel' identifikuje panel
    // (PANEL_LEFT nebo PANEL_RIGHT), ve kterem je otevrene FS (z tohoto panelu se ziskavaji
    // soubory/adresare, ktere se maji mazat); 'selectedFiles' + 'selectedDirs' - pocet oznacenych
    // souboru a adresaru, pokud jsou obe hodnoty nulove, maze se soubor/adresar pod kurzorem
    // (fokus), pred volanim metody Delete jsou bud oznacene soubory a adresare nebo je alespon
    // fokus na souboru/adresari, takze je vzdy s cim pracovat (zadne dalsi testy nejsou treba);
    // pokud vraci TRUE a 'cancelOrError' je FALSE, operace probehla korektne a oznacene
    // soubory/adresare se maji odznacit (pokud prezily mazani); pokud chce uzivatel prerusit
    // operaci nebo nastane chyba vraci se 'cancelOrError' TRUE a nedojde k odznaceni
    // souboru/adresaru; pokud vraci FALSE pri 'mode'==1 a 'cancelOrError' je FALSE, ma se
    // otevrit standardni dotaz na mazani
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError) = 0;

    // kopirovani/presun z FS (parametr 'copy' je TRUE/FALSE), v dalsim textu se mluvi jen o
    // kopirovani, ale vse plati shodne i pro presun; 'copy' muze byt TRUE (kopirovani) jen
    // pokud GetSupportedServices() vraci i FS_SERVICE_COPYFROMFS; 'copy' muze byt FALSE
    // (presun nebo prejmenovani) jen pokud GetSupportedServices() vraci i FS_SERVICE_MOVEFROMFS;
    //
    // kopirovani souboru a adresaru (z FS) oznacenych v panelu; umozni otevrit vlastni dialog pro
    // zadani cile kopirovani (parametr 'mode' je 1) nebo pouzit standardni dialog (vrati FALSE
    // a 'cancelOrHandlePath' take FALSE, pak Salamander otevre standardni dialog a ziskanou cilovou
    // cestu preda v 'targetPath' pri dalsim volani CopyOrMoveFromFS s 'mode'==2); pri 'mode'==2
    // je 'targetPath' presne retezec zadany uzivatelem (CopyOrMoveFromFS si ho muze rozanalyzovat
    // po svem); pokud CopyOrMoveFromFS podporuje jen windowsove cilove cesty (nebo nedokaze
    // zpracovat uzivatelem zadanou cestu - napr. vede na jiny FS nebo do archivu), muze vyuzit
    // standardni zpusob zpracovani cesty v Salamanderovi (zatim umi zpracovat jen windowsove cesty,
    // casem mozna zpracuje i FS a archivove cesty pres TEMP adresar pomoci sledu nekolika zakladnich
    // operaci) - vrati FALSE, 'cancelOrHandlePath' TRUE a 'operationMask' TRUE/FALSE
    // (podporuje/nepodporuje operacni masky - pokud nepodporuje a v ceste je maska, zobrazi se
    // chybova hlaska), pak Salamander zpracuje cestu vracenou v 'targetPath' (zatim jen rozdeleni
    // windowsove cesty na existujici cast, neexistujici cast a pripadne masku; umozni take vytvorit
    // podadresare z neexistujici casti) a je-li cesta v poradku, zavola znovu CopyOrMoveFromFS
    // s 'mode'==3 a v 'targetPath' s cilovou cestou a pripadne i s operacni maskou (dva retezce
    // vzajemne oddelene nulou; zadna maska -> dve nuly na konci retezce), pokud je v ceste nejaka
    // chyba, zavola znovu CopyOrMoveFromFS s 'mode'==4 v 'targetPath' s upravenou chybnou cilovou
    // cestou (chyba uz byla uzivateli ohlasena; uzivatel by mel dostat moznost cestu opravit;
    // v ceste mohly byt odstraneny "." a "..", atp.);
    //
    // pokud uzivatel zada operaci drag&dropem (dropne soubory/adresare z FS do stejneho panelu
    // nebo do jineho drop-targetu), je 'mode'==5 a v 'targetPath' je cilova cesta operace (muze
    // byt windowsova cesta, FS cesta a do budoucna se da pocitat i s cestami do archivu),
    // 'targetPath' je ukoncena dvema nulami (pro kompatibilitu s 'mode'==3); 'dropTarget' je
    // v tomto pripade okno drop-targetu (vyuziva se pro reaktivaci drop-targetu po otevreni
    // progress-okna operace, viz CSalamanderGeneralAbstract::ActivateDropTarget); pri 'mode'==5 ma
    // smysl jen navratova hodnota TRUE;
    //
    // 'fsName' je aktualni jmeno FS; 'parent' je navrzeny parent pripadnych zobrazovanych dialogu;
    // 'panel' identifikuje panel (PANEL_LEFT nebo PANEL_RIGHT), ve kterem je otevreny FS (z tohoto
    // panelu se ziskavaji soubory/adresare, ktere se maji kopirovat);
    // 'selectedFiles' + 'selectedDirs' - pocet oznacenych souboru a adresaru, pokud jsou
    // obe hodnoty nulove, kopiruje se soubor/adresar pod kurzorem (fokus), pred volanim
    // metody CopyOrMoveFromFS jsou bud oznacene soubory a adresare nebo je alespon fokus
    // na souboru/adresari, takze je vzdy s cim pracovat (zadne dalsi testy
    // nejsou treba); na vstupu 'targetPath' pri 'mode'==1 obsahuje navrzenou cilovou cestu
    // (jen windowsove cesty bez masky nebo prazdny retezec), pri 'mode'==2 obsahuje retezec
    // cilove cesty zadany uzivatelem ve standardnim dialogu, pri 'mode'==3 obsahuje cilovou
    // cestu a masku (oddelene nulou), pri 'mode'==4 obsahuje chybnou cilovou cestu, pri 'mode'==5
    // obsahuje cilovou cestu (windowsovou, FS nebo do archivu) ukoncenou dvema nulami; pokud
    // metoda vraci FALSE, obsahuje 'targetPath' na vystupu (buffer 2 * MAX_PATH znaku) pri
    // 'cancelOrHandlePath'==FALSE navrzenou cilovou cestu pro standardni dialog a pri
    // 'cancelOrHandlePath'==TRUE retezec cilove cesty ke zpracovani; pokud metoda vraci TRUE a
    // 'cancelOrHandlePath' FALSE, obsahuje 'targetPath' jmeno polozky, na kterou ma prejit fokus
    // ve zdrojovem panelu (buffer 2 * MAX_PATH znaku; ne plne jmeno, jen jmeno polozky v panelu;
    // je-li prazdny retezec, fokus zustava beze zmeny); 'dropTarget' neni NULL jen v pripade
    // zadani cesty operace pomoci drag&dropu (viz popis vyse)
    //
    // pokud vraci TRUE a 'cancelOrHandlePath' je FALSE, operace probehla korektne a oznacene
    // soubory/adresare se maji odznacit; pokud chce uzivatel prerusit operaci nebo nastala
    // chyba, vraci metoda TRUE a 'cancelOrHandlePath' TRUE, v obou pripadech nedojde k odznaceni
    // souboru/adresaru; pokud vraci FALSE, ma se otevrit standardni dialog ('cancelOrHandlePath'
    // je FALSE) nebo se ma standardnim zpusobem zpracovat cesta ('cancelOrHandlePath' je TRUE)
    //
    // POZNAMKA: pokud je nabizena moznost kopirovat/presouvat na cestu do ciloveho panelu,
    //           je treba volat CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath pro cilovy
    //           panel, jinak nebude cesta v tomto panelu vlozena do seznamu pracovnich
    //           adresaru - List of Working Directories (Alt+F12)
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget) = 0;

    // kopirovani/presun z windowsove cesty na FS (parametr 'copy' je TRUE/FALSE), v dalsim textu
    // se mluvi jen o kopirovani, ale vse plati shodne i pro presun; 'copy' muze byt TRUE (kopirovani)
    // jen pokud GetSupportedServices() vraci i FS_SERVICE_COPYFROMDISKTOFS; 'copy' muze byt FALSE
    // (presun nebo prejmenovani) jen pokud GetSupportedServices() vraci i FS_SERVICE_MOVEFROMDISKTOFS;
    //
    // kopirovani vybranych (v panelu nebo jinde) souboru a adresaru na FS; pri 'mode'==1 umoznuje
    // pripravit text cilove cesty pro uzivatele do standardniho dialogu pro kopirovani, jde o situaci,
    // kdy je ve zdrojovem panelu (panel, kde dojde ke spusteni prikazu Copy (klavesa F5)) windowsova
    // cesta a v cilovem panelu tento FS; pri 'mode'==2 a 'mode'==3 muze plugin provest operaci kopirovani nebo
    // ohlasit jednu ze dvou chyb: "chyba v ceste" (napr. obsahuje nepripustne znaky nebo neexistuje)
    // a "v tomto FS nelze provest pozadovanou operaci" (napr. jde sice o FTP, ale otevrena cesta
    // v tomto FS je rozdilna od cilove cesty (napr. u FTP jiny FTP server) - je potreba otevrit
    // jiny/novy FS; tuto chybu nemuze ohlasit nove otevreny FS);
    // POZOR: tato metoda se muze zavolat pro jakoukoliv cilovou FS cestu tohoto pluginu (muze tedy
    //        jit i o cestu s jinym jmenem FS tohoto pluginu)
    //
    // 'fsName' je aktualni jmeno FS; 'parent' je navrzeny parent pripadnych zobrazovanych
    // dialogu; 'sourcePath' je zdrojova windowsova cesta (vsechny vybrane soubory a adresare
    // jsou adresovany relativne k ni), pri 'mode'==1 je NULL; vybrane soubory a adresare
    // jsou zadany enumeracni funkci 'next' jejimz parametrem je 'nextParam', pri 'mode'==1
    // jsou NULL; 'sourceFiles' + 'sourceDirs' - pocet vybranych souboru a adresaru (soucet
    // je vzdy nenulovy); 'targetPath' je in/out buffer min. 2 * MAX_PATH znaku pro cilovou
    // cestu; pri 'mode'==1 je 'targetPath' na vstupu aktualni cesta na tomto FS a na vystupu cilova
    // cesta pro standardni dialog kopirovani; pri 'mode'==2 je 'targetPath' na vstupu uzivatelem
    // zadana cilova cesta (bez uprav, vcetne masky, atd.) a na vystupu se ignoruje az na pripad, kdy
    // metoda vraci FALSE (chyba) a 'invalidPathOrCancel' TRUE (chyba v ceste), v tomto pripade je na
    // vystupu upravena cilova cesta (napr. odstranene "." a ".."), kterou bude uzivatel opravovat
    // ve standardnim dialogu kopirovani; pri 'mode'==3 je 'targetPath' na vstupu drag&dropem
    // zadana cilova cesta a na vystupu se ignoruje; neni-li 'invalidPathOrCancel' NULL (jen 'mode'==2
    // a 'mode'==3), vraci se v nem TRUE, pokud je cesta spatne zadana (obsahuje nepripustne znaky nebo
    // neexistuje, atd.) nebo doslo k preruseni operace (cancel) - chybove/cancel hlaseni se zobrazuje
    // pred ukoncenim teto metody
    //
    // pri 'mode'==1 vraci metoda TRUE pri uspechu, pokud vraci FALSE, pouzije se jako cilova cesta
    // pro standardni dialog kopirovani prazdny retezec; pokud vraci metoda FALSE pri 'mode'==2 a 'mode'==3,
    // ma se pro zpracovani operace hledat jiny FS (je-li 'invalidPathOrCancel' FALSE) nebo ma
    // uzivatel opravit cilovou cestu (je-li 'invalidPathOrCancel' TRUE); pokud vraci metoda TRUE
    // pri 'mode'==2 nebo 'mode'==3, operace probehla a ma dojit k odznaceni vybranych souboru a adresaru
    // (je-li 'invalidPathOrCancel' FALSE) nebo doslo k chybe/preruseni operace a nema dojit
    // k odznaceni vybranych souboru a adresaru (je-li 'invalidPathOrCancel' TRUE)
    //
    // POZOR: Metoda CopyOrMoveFromDiskToFS se muze volat ve trech situacich:
    //        - tento FS je aktivni v panelu
    //        - tento FS je odpojeny
    //        - tento FS prave vzniknul (volanim OpenFS) a po ukonceni metody zase ihned zanikne
    //          (volanim CloseFS) - nebyla od nej volana zadna jina metoda (ani ChangePath)
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_CHANGEATTRS:
    // changes the attributes of the files and directories selected in the panel; each plugin provides its own attribute dialog;
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the custom dialog; 'panel'
    // identifies the panel (PANEL_LEFT or PANEL_RIGHT) in which the FS is open (this panel supplies the files/directories being operated on);
    // 'selectedFiles' + 'selectedDirs' is the number of selected files and directories;
    // if both values are zero, the file/directory under the cursor is used
    // (focus); before ChangeAttributes is called, either files/directories are selected or there is at least
    // focus on a file/directory, so there is always something to work with (no additional checks
    // are needed); if it returns TRUE, the operation completed successfully and the selected files/directories
    // should be deselected; if the user cancels the operation or an error occurs, the method returns
    // FALSE and the files/directories are not deselected
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_SHOWPROPERTIES:
    // displays a properties window for the files and directories selected in the panel; each plugin
    // provides its own properties window;
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the custom window
    // (a Windows properties window is modeless - note: a modeless window must
    // have its own thread); 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT)
    // in which the FS is open (this panel supplies the files/directories
    // being worked with); 'selectedFiles' + 'selectedDirs' is the number of selected
    // files and directories; if both values are zero, the file/directory under the cursor is used
    // (focus); before ShowProperties is called, either files/directories are selected
    // or there is at least focus on a file/directory, so there is always
    // something to work with (no additional checks are needed)
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_CONTEXTMENU:
    // zobrazeni kontextoveho menu pro soubory a adresare oznacene v panelu (kliknuti pravym
    // tlacitkem mysi na polozkach v panelu) nebo pro aktualni cestu v panelu (kliknuti pravym
    // tlacitkem mysi na change-drive tlacitku v panelove toolbare) nebo pro panel (kliknuti
    // pravym tlacitkem mysi za polozkami v panelu); kontextove menu ma kazdy plugin vlastni;
    //
    // 'fsName' je aktualni jmeno FS; 'parent' je navrzeny parent kontextoveho menu;
    // 'menuX' + 'menuY' jsou navrzene souradnice leveho horniho rohu kontextoveho menu;
    // 'type' je typ kontextoveho menu (viz popisy konstant fscmXXX); 'panel'
    // identifikuje panel (PANEL_LEFT nebo PANEL_RIGHT), pro ktery se ma kontextove
    // menu otevrit (z tohoto panelu se ziskavaji soubory/adresare/cesta, se kterymi
    // se pracuje); pri 'type'==fscmItemsInPanel je 'selectedFiles' + 'selectedDirs'
    // pocet oznacenych souboru a adresaru, pokud jsou obe hodnoty nulove, pracuje se se
    // souborem/adresarem pod kurzorem (focusem), pred volanim metody ContextMenu jsou bud
    // oznacene soubory a adresare (a bylo na nich kliknuto) nebo je alespon fokus na
    // souboru/adresari (neni na updiru), takze je vzdy s cim pracovat (zadne dalsi testy
    // nejsou treba); pokud 'type'!=fscmItemsInPanel, 'selectedFiles' + 'selectedDirs'
    // jsou vzdy nastaveny na nulu (ignoruji se)
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_CONTEXTMENU:
    // if an FS is open in the panel and one of the messages WM_INITPOPUP, WM_DRAWITEM,
    // WM_MENUCHAR, or WM_MEASUREITEM arrives, Salamander calls HandleMenuMsg to allow the plugin
    // to work with IContextMenu2 and IContextMenu3
    // the plugin returns TRUE if it handled the message and FALSE otherwise
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) = 0;

    // jen pokud GetSupportedServices() vraci i FS_SERVICE_OPENFINDDLG:
    // otevreni Find dialogu pro FS v panelu; 'fsName' je aktualni jmeno FS; 'panel' identifikuje
    // panel (PANEL_LEFT nebo PANEL_RIGHT), pro ktery se ma otevrit Find dialog (z tohoto panelu
    // se ziskava obvykle cesta pro hledani); vraci TRUE pri uspesnem otevreni Find dialogu;
    // pokud vrati FALSE, Salamander otevre standardni Find Files and Directories dialog
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_OPENACTIVEFOLDER:
    // opens an Explorer window for the current path in the panel
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the displayed dialog
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent) = 0;

    // jen pokud GetSupportedServices() vraci FS_SERVICE_MOVEFROMFS nebo FS_SERVICE_COPYFROMFS:
    // dovoluje ovlivnit povolene drop-effecty pri drag&dropu z tohoto FS; neni-li 'allowedEffects'
    // NULL, obsahuje na vstupu dosud povolene drop-effecty (kombinace DROPEFFECT_MOVE a DROPEFFECT_COPY),
    // na vystupu obsahuje drop-effecty povolene timto FS (effecty by se mely jen ubirat);
    // 'mode' je 0 pri volani, ktere tesne predchazi zahajeni drag&drop operace, effecty vracene
    // v 'allowedEffects' se pouziji pro volani DoDragDrop (tyka se cele drag&drop operace);
    // 'mode' je 1 behem tazeni mysi nad FS z tohoto procesu (muze byt toto FS nebo FS z druheho
    // panelu); pri 'mode' 1 je v 'tgtFSPath' cilova cesta, ktera se pouzije pokud dojde k dropu,
    // jinak je 'tgtFSPath' NULL; 'mode' je 2 pri volani, ktere tesne nasleduje po dokonceni
    // drag&drop operace (uspesnemu i neuspesnemu)
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) = 0;

    // umoznuje pluginu zmenit standardni hlaseni "There are no items in this panel." zobrazovane
    // v situaci, kdy v panelu neni zadna polozka (soubor/adresar/up-dir); vraci FALSE pokud
    // se ma pouzit standardni hlaseni (navratova hodnota 'textBuf' se pak ignoruje); vraci TRUE
    // pokud plugin v 'textBuf' (buffer o velikosti 'textBufSize' znaku) vraci svou alternativu
    // teto hlasky
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_SHOWSECURITYINFO:
    // the user clicked the security icon (see CSalamanderGeneralAbstract::ShowSecurityIcon;
    // for example, FTPS displays a dialog with the server certificate); 'parent' is the suggested parent of the dialog
    virtual void WINAPI ShowSecurityInfo(HWND parent) = 0;

    /* zbyva dokoncit:
// calculate occupied space on FS (Alt+F10 + Ctrl+Shift+F10 + calc. needed space + spacebar key in panel)
#define FS_SERVICE_CALCULATEOCCUPIEDSPACE
// edit from FS (F4)
#define FS_SERVICE_EDITFILE
// edit new file from FS (Shift+F4)
#define FS_SERVICE_EDITNEWFILE
*/
};

//
// ****************************************************************************
// CPluginInterfaceForFSAbstract
//

class CPluginInterfaceForFSAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginInterfaceForFSEncapsulation)
    friend class CPluginInterfaceForFSEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // funkce pro "file system"; vola se pro otevreni FS; 'fsName' je jmeno FS,
    // ktery se ma otevrit; 'fsNameIndex' je index jmena FS, ktery se ma otevrit
    // (index je nula pro fs-name zadane v CSalamanderPluginEntryAbstract::SetBasicPluginData,
    // u ostatnich fs-name index vraci CSalamanderPluginEntryAbstract::AddFSName);
    // vraci ukazatel na interface otevreneho FS CPluginFSInterfaceAbstract nebo
    // NULL pri chybe
    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(const char* fsName, int fsNameIndex) = 0;

    // funkce pro "file system", vola se pro uzavreni FS, 'fs' je ukazatel na
    // interface otevreneho FS, po tomto volani uz je v Salamanderu interface 'fs'
    // povazovan za neplatny a nebude dale pouzivan (funkce paruje s OpenFS)
    // POZOR: v teto metode nesmi dojit k otevreni zadneho okna ani dialogu
    //        (okna lze otevirat v metode CPluginFSInterfaceAbstract::ReleaseObject)
    virtual void WINAPI CloseFS(CPluginFSInterfaceAbstract* fs) = 0;

    // provedeni prikazu na polozce pro FS v Change Drive menu nebo v Drive barach
    // (jeji pridani viz CSalamanderConnectAbstract::SetChangeDriveMenuItem);
    // 'panel' identifikuje panel, se kterym mame pracovat - pro prikaz z Change Drive
    // menu je 'panel' vzdy PANEL_SOURCE (toto menu muze byt vybaleno jen u aktivniho
    // panelu), pro prikaz z Drive bary muze byt PANEL_LEFT nebo PANEL_RIGHT (pokud
    // jsou zapnute dve Drive bary, muzeme pracovat i s neaktivnim panelem)
    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel) = 0;

    // otevreni kontextoveho menu na polozce pro FS v Change Drive menu nebo v Drive
    // barach nebo pro aktivni/odpojeny FS v Change Drive menu; 'parent' je parent
    // kontextoveho menu; 'x' a 'y' jsou souradnice pro vybaleni kontextoveho menu
    // (misto kliknuti praveho tlacitka mysi nebo navrzene souradnice pri Shift+F10);
    // je-li 'pluginFS' NULL jde o polozku pro FS, jinak je 'pluginFS' interface
    // aktivniho/odpojeneho FS ('isDetachedFS' je FALSE/TRUE); neni-li 'pluginFS'
    // NULL, je v 'pluginFSName' jmeno FS otevreneho v 'pluginFS' (jinak je v
    // 'pluginFSName' NULL) a v 'pluginFSNameIndex' index jmena FS otevreneho
    // v 'pluginFS' (pro snazsi detekci o jake jmeno FS jde; jinak je v
    // 'pluginFSNameIndex' -1); pokud vrati FALSE, jsou ostatni navratove hodnoty
    // ignorovany, jinak maji tento vyznam: v 'refreshMenu' vraci TRUE pokud se ma
    // provest obnova Change Drive menu (pro Drive bary se ignoruje, protoze se na
    // nich neukazuji aktivni/odpojene FS); v 'closeMenu' vraci TRUE pokud se ma
    // zavrit Change Drive menu (pro Drive bary neni co zavirat); vraci-li 'closeMenu'
    // TRUE a 'postCmd' neni nula, je po zavreni Change Drive menu (pro Drive bary
    // ihned) jeste zavolano ExecuteChangeDrivePostCommand s parametry 'postCmd'
    // a 'postCmdParam'; 'panel' identifikuje panel, se kterym mame pracovat - pro
    // kontextove menu v Change Drive menu je 'panel' vzdy PANEL_SOURCE (toto menu
    // muze byt vybaleno jen u aktivniho panelu), pro kontextove menu v Drive barach
    // muze byt PANEL_LEFT nebo PANEL_RIGHT (pokud jsou zapnute dve Drive bary, muzeme
    // pracovat i s neaktivnim panelem)
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam) = 0;

    // provedeni prikazu z kontextoveho menu na polozce pro FS nebo pro aktivni/odpojeny FS v
    // Change Drive menu az po zavreni Change Drive menu nebo provedeni prikazu z kontextoveho
    // menu na polozce pro FS v Drive barach (jen pro kompatibilitu s Change Drive menu); vola
    // se jako reakce na navratove hodnoty 'closeMenu' (TRUE), 'postCmd' a 'postCmdParam'
    // ChangeDriveMenuItemContextMenu po zavreni Change Drive menu (pro Drive bary ihned);
    // 'panel' identifikuje panel, se kterym mame pracovat - pro kontextove menu v Change Drive
    // menu je 'panel' vzdy PANEL_SOURCE (toto menu muze byt vybaleno jen u aktivniho panelu),
    // pro kontextove menu v Drive barach muze byt PANEL_LEFT nebo PANEL_RIGHT (pokud jsou
    // zapnute dve Drive bary, muzeme pracovat i s neaktivnim panelem)
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam) = 0;

    // provede spusteni polozky v panelu s otevrenym FS (napr. reakce na klavesu Enter v panelu;
    // u podadresaru/up-diru (o up-dir jde pokud je jmeno ".." a zaroven jde o prvni adresar)
    // se predpoklada zmena cesty, u souboru otevreni kopie souboru na disku s tim, ze se pripadne
    // zmeny nactou zpet na FS); spousteni neni mozne provest v metode FS interfacu, protoze tam
    // nelze volat metody pro zmenu cesty (muze v nich totiz dojit i k zavreni FS);
    // 'panel' urcuje panel, ve kterem probiha spousteni (PANEL_LEFT nebo PANEL_RIGHT);
    // 'pluginFS' je interface FS otevreneho v panelu; 'pluginFSName' je jmeno FS otevreneho
    // v panelu; 'pluginFSNameIndex' je index jmena FS otevreneho v panelu (pro snazsi detekci
    // o jake jmeno FS jde); 'file' je spousteny soubor/adresar/up-dir ('isDir' je 0/1/2);
    // POZOR: volani metody pro zmenu cesty v panelu muze zneplatnit 'pluginFS' (po zavreni FS)
    //        a 'file'+'isDir' (zmena listingu v panelu -> zruseni polozek puvodniho listingu)
    // POZNAMKA: pokud se spousti soubor nebo se s nim jinak pracuje (napr. downloadi se),
    //           je treba volat CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath pro panel
    //           'panel', jinak nebude cesta v tomto panelu vlozena do seznamu pracovnich
    //           adresaru - List of Working Directories (Alt+F12)
    virtual void WINAPI ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex,
                                    CFileData& file, int isDir) = 0;

    // provede disconnect FS, o ktery zadazal uzivatel v Disconnect dialogu; 'parent' je
    // parent pripadnych messageboxu (jde o stale jeste otevreny Disconnect dialog);
    // disconnect neni mozne provest v metode FS interfacu, protoze FS ma zaniknout;
    // 'isInPanel' je TRUE pokud je FS v panelu, pak 'panel' urcuje ve kterem panelu
    // (PANEL_LEFT nebo PANEL_RIGHT); 'isInPanel' je FALSE pokud je FS odpojeny, pak je
    // 'panel' 0; 'pluginFS' je interface FS; 'pluginFSName' je jmeno FS; 'pluginFSNameIndex'
    // je index jmena FS (pro snazsi detekci o jake jmeno FS jde); metoda vraci FALSE,
    // pokud se disconnect nepodaril a Disconnect dialog ma zustat otevreny (jeho obsah
    // se obnovi, aby se projevily predchozi uspesne disconnecty)
    virtual BOOL WINAPI DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                     CPluginFSInterfaceAbstract* pluginFS,
                                     const char* pluginFSName, int pluginFSNameIndex) = 0;

    // converts the user-part of the path in buffer 'fsUserPart' (size MAX_PATH characters) from external
    // to internal format (for example, on FTP: internal format = paths as used by the server,
    // external format = URL format = paths containing hex escape sequences (e.g. "%20" = " "))
    virtual void WINAPI ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) = 0;

    // converts the user-part of the path in buffer 'fsUserPart' (size MAX_PATH characters) from internal
    // to external format
    virtual void WINAPI ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) = 0;

    // This method is called only for a plugin that serves as a replacement for the Network item
    // in the Change Drive menu and in the Drive bars (see CSalamanderGeneralAbstract::SetPluginIsNethood()):
    // by calling this method, Salamander informs the plugin that the user is changing the path from the root of the UNC
    // path "\\server\share" via the up-dir entry ("..") to the plugin FS, to a path with
    // user-part "\\server" in panel 'panel' (PANEL_LEFT or PANEL_RIGHT); purpose of this method:
    // the plugin should immediately list at least this one share at that path so it can receive
    // focus in the panel (which is the normal behavior when changing path via up-dir)
    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_fs)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__

/*   Predbezna verze helpu k pluginovemu interfacu

  Otevreni, zmena, listovani a refresh cesty:
    - pro otevreni cesty v novem FS se vola ChangePath (prvni volani ChangePath je vzdy pro otevreni cesty)
    - pro zmenu cesty se vola ChangePath (druhe a vsechny dalsi volani ChangePath jsou zmeny cesty)
    - pri fatalni chybe ChangePath vraci FALSE (FS cesta se v panelu neotevre, pokud slo
      o zmenu cesty, zkusi se nasledne volat ChangePath pro puvodni cestu, pokud ani to nevyjde,
      dojde k prechodu na fixed-drive cestu)
    - pokud ChangePath vrati TRUE (uspech) a nedoslo ke zkraceni cesty na puvodni (jejiz
      listing je prave nacteny), vola se ListCurrentPath pro ziskani noveho listingu
    - po uspesnem vylistovani vraci ListCurrentPath TRUE
    - pri fatalni chybe vraci ListCurrentPath FALSE a nasledne volani ChangePath
      musi take vratit FALSE
    - pokud aktualni cesta nejde vylistovat, vraci ListCurrentPath FALSE a nasledne volani
      ChangePath musi cestu zmenit a vratit TRUE (zavola se znovu ListCurrentPath), pokud jiz
      cestu nejde zmenit (root, atp.), vrati ChangePath take FALSE (FS cesta se v panelu neotevre,
      pokud slo o zmenu cesty, zkusi se nasledne volat ChangePath pro puvodni cestu, pokud ani to
      nevyjde, dojde k prechodu na fixed-drive cestu)
    - refresh cesty (Ctrl+R) se chova stejne jako zmena cesty na aktualni cestu (cesta
      se vubec nemusi zmenit nebo se muze zkratit nebo v pripade fatalni chyby zmenit na
      fixed-drive); pri refreshi cesty je parametr 'forceRefresh' TRUE pro vsechna volani metod
      ChangePath a ListCurrentPath (FS nesmi pouzit pro zmenu cesty ani nacteni listingu zadne
      cache - user nechce pouzivat cache);

  Pri pruchodu historii (back/forward) se FS interface, ve kterem probehne vylistovani
  FS cesty ('fsName':'fsUserPart'), ziska prvnim moznym zpusobem z nasledujicich:
    - FS interface, ve kterem byla cesta naposledy otevrena jeste nebyl zavren
      a je mezi odpojenymi nebo je aktivni v panelu (neni aktivni v druhem panelu)
    - aktivni FS interface v panelu ('currentFSName') je ze stejneho pluginu jako
      'fsName' a vrati na IsOurPath('currentFSName', 'fsName', 'fsUserPart') TRUE
    - prvni z odpojenych FS interfacu ('currentFSName'), ktery je ze stejneho
      pluginu jako 'fsName' a vrati na IsOurPath('currentFSName', 'fsName',
      'fsUserPart') TRUE
    - novy FS interface
*/
