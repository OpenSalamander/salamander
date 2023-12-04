// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// aktualni verze konfigurace (popis viz. mainwnd2.cpp)
extern const DWORD THIS_CONFIG_VERSION;

// Expirace verze: pro beta a PB verze odkomentovat, pro ostre verze zakomentovat:
//#define USE_BETA_EXPIRATION_DATE

// pro PB (EAP) verze odkomentovat, pro ostatni verze zakomentovat:
//#define THIS_IS_EAP_VERSION

#ifdef USE_BETA_EXPIRATION_DATE

// urcuje prvni den, kdy uz tato beta verze nepobezi
extern SYSTEMTIME BETA_EXPIRATION_DATE;

#endif // USE_BETA_EXPIRATION_DATE

// jen pro DEBUG verzi Salama: umozni debugovat tvorbu bugreportu (defaultne se nedela,
// exceptiona se jen preda MSVC debuggeru)
//#define ENABLE_BUGREPORT_DEBUGGING   1

// slouzi pro detekci, zda wheel message prisla pres hook nebo primo
extern BOOL MouseWheelMSGThroughHook; // TRUE: zprava sla skrz hook v case MouseWheelMSGTime; FALSE: zprava sla skrz panel v case MouseWheelMSGTime
extern DWORD MouseWheelMSGTime;       // casove razitko pri posledni zprave
#define MOUSEWHEELMSG_VALID 100       // [ms] pocet milisekund, po ktere je validni jeden kanal (hook vs okno)

enum
{
    otViewerWindow = 10,
};

// podpora pro horizontalni scroll (funguje jiz na W2K/XP s Intellipoint ovladacema, oficialne podporeno od Visty)
#define WM_MOUSEHWHEEL 0x020E
BOOL PostMouseWheelMessage(MSG* pMSG);

// zjisti jestli je velka sance (jiste se to urcit neda), ze Salamander v pristich par
// okamzicich nebude "busy" (nebude otevreny zadny modalni dialog a nebude se zpracovavat
// zadna zprava) - v tomto pripade vraci TRUE (jinak FALSE); neni-li 'lastIdleTime' NULL,
// vraci se v nem GetTickCount() z okamziku posledniho prechodu z "idle" do "busy" stavu
// je mozne volat z libovolneho threadu
BOOL SalamanderIsNotBusy(DWORD* lastIdleTime);

// otevre HTML help Salamandera nebo pluginu, jazyk helpu (adresar s .chm soubory) vybira takto:
// -adresar ziskany z aktualniho .slg souboru Salamandera (viz SLGHelpDir v shared\versinfo.rc)
// -HELP\ENGLISH\*.chm
// -prvni nalezeny podadresar v podadresari HELP
// 'helpFileName' je jmeno .chm souboru, se kterym se ma pracovat (jmeno je bez cesty), je-li NULL,
// jde o "salamand.chm"; 'parent' je parent messageboxu s chybou; 'command' je prikaz HTML helpu,
// viz HHCDisplayXXX; 'dwData' je parametr prikazu HTML helpu, viz HHCDisplayXXX
// je mozne volat z libovolneho threadu
// Pokud je 'quiet' TRUE, nezobrazi se chybova hlaska.
// Vraci TRUE, pokud se help podarilo otevrite, jinak vraci FALSE;
BOOL OpenHtmlHelp(char* helpFileName, HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet);

extern CRITICAL_SECTION OpenHtmlHelpCS; // kriticka sekce pro OpenHtmlHelp()

/* jednoduche zajisteni behu v kriticke sekci, priklad pouziti:
  static CCriticalSection cs;
  CEnterCriticalSection enterCS(cs);
*/

class CCriticalSection
{
public:
    CRITICAL_SECTION cs;

    CCriticalSection() { InitializeCriticalSection(&cs); }
    ~CCriticalSection() { DeleteCriticalSection(&cs); }

    void Enter() { EnterCriticalSection(&cs); }
    void Leave() { LeaveCriticalSection(&cs); }
};

class CEnterCriticalSection
{
protected:
    CCriticalSection* CS;

public:
    CEnterCriticalSection(CCriticalSection& cs)
    {
        CS = &cs;
        CS->Enter();
    }

    ~CEnterCriticalSection()
    {
        CS->Leave();
    }
};

// protoze windowsova GetTempFileName nefunguje, napsali jsme si vlastniho klona:
// vytvori soubor/adresar (podle 'file') na ceste 'path' (NULL -> Window TEMP dir),
// s prefixem 'prefix', vraci jmeno vytvoreneho souboru v 'tmpName' (min. velikost MAX_PATH),
// vraci "uspech?" (pri neuspechu vraci pres SetLastError kod Windows chyby - pro kompatib.)
BOOL SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file);

// protoze windowsova verze MoveFile nezvlada prejmenovani souboru s read-only atributem na Novellu,
// napsali jsme si vlastni (nastane-li chyba pri MoveFile, zkusi shodit read-only, provest operaci,
// a pak ho zase nahodit)
BOOL SalMoveFile(const char* srcName, const char* destName);

// varianta k windowsove verzi GetFileSize (ma jednodussi osetreni chyb); 'file' je otevreny
// soubor pro volani GetFileSize(); v 'size' vraci ziskanou velikost souboru; vraci uspech,
// pri FALSE (chyba) je v 'err' windowsovy kod chyby a v 'size' nula
BOOL SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err);
BOOL SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err); // 'err' muze byt NULL pokud nas nezajima

struct COperation;

// zjisti velikost souboru, na ktery vede symlink 'fileName'; je-li 'op' ruzne od NULL,
// pri cancelu se vnitrek 'op' uvolni; je-li 'fileName' NULL, bere se 'op->SourceName';
// velikost vraci v 'size'; 'ignoreAll' je in + out, je-li TRUE vsechny chyby se ignoruji
// (pred akci je treba priradit FALSE, jinak se okno s chybou vubec nezobrazi, pak uz
// nemenit); pri chybe zobrazi standardni okno s dotazem Retry / Ignore / Ignore All / Cancel
// s parentem 'parent'; pokud velikost uspesne zjisti, vraci TRUE; pri chybe a stisku
// tlacitka Ignore / Igore All v okne s chybou, vraci FALSE a v 'cancel' vraci FALSE;
// je-li 'ignoreAll' TRUE, okno se neukaze, na tlacitko se neceka, chova se jako by
// uzivatel stiskl Ignore; pri chybe a stisku Cancel v okne s chybou vraci FALSE a
// v 'cancel' vraci TRUE
BOOL GetLinkTgtFileSize(HWND parent, const char* fileName, COperation* op, CQuadWord* size,
                        BOOL* cancel, BOOL* ignoreAll);

// protoze windowsova verze GetFileAttributes neumi pracovat se jmeny koncicimi mezerou/teckou,
// napsali jsme si vlastni (u techto jmen pridava backslash na konec, cimz uz pak
// GetFileAttributes funguje spravne, ovsem jen pro adresare, pro soubory s mezerou/teckou na
// konci reseni nemame, ale aspon se to nezjistuje od jineho souboru - windowsova verze
// orizne mezery/tecky a pracuje tak s jinym souborem/adresarem)
DWORD SalGetFileAttributes(const char* fileName);

// pokud ma soubor/adresar 'name' read-only atribut, pokusime se ho vypnout
// (duvod: napr. aby sel smazat pres DeleteFile); pokud uz mame atributy 'name'
// nactene, predame je v 'attr', je-li 'attr' -1, ctou se atributy 'name' z disku;
// vraci TRUE pokud se provede pokus o zmenu atributu (uspech se nekontroluje)
// POZNAMKA: vypina jen read-only atribut, aby v pripade vice hardlinku nedoslo
// k zbytecne velke zmene atributu na zbyvajicich hardlinkach souboru (atributy
// vsechny hardlinky sdili)
BOOL ClearReadOnlyAttr(const char* name, DWORD attr = -1);

// smaze link na adresar (junction point, symbolic link, mount point); pri uspechu
// vraci TRUE; pri chybe vraci FALSE a neni-li 'err' NULL, vraci kod chyby v 'err'
BOOL DeleteDirLink(const char* name, DWORD* err);

// vraci TRUE, pokud je cesta 'path' na NOVELLskem svazku (slouzi k detekci toho,
// jestli lze pouzit fast-directory-move)
BOOL IsNOVELLDrive(const char* path);

// vraci TRUE, pokud je cesta 'path' na LANTASTICskem svazku (slouzi k detekci toho,
// jestli je nutne po kopirovani kontrolovat velikost souboru); pro optimalizacni
// ucely se vyuzivaji 'lastLantasticCheckRoot' (pro prvni volani "", pak nemenit)
// a 'lastIsLantasticPath' (vysledek pro 'lastLantasticCheckRoot')
BOOL IsLantasticDrive(const char* path, char* lastLantasticCheckRoot, BOOL& lastIsLantasticPath);

// vraci TRUE pro sitove cesty
BOOL IsNetworkPath(const char* path);

// vraci TRUE pokud 'path' lezi na svazku, ktery podporuje ADS (nebo nastala chyba pri
// zjistovani o jaky file-system jde) a jsme pod NT/W2K/XP; neni-li 'isFAT32' NULL,
// vraci v nem TRUE pokud 'path' vede na FAT32 svazek; vraci FALSE jen pokud je
// jiste, ze FS nepodporuje ADS
BOOL IsPathOnVolumeSupADS(const char* path, BOOL* isFAT32);

// test jestli jde o Sambu (Linuxova podpora sdileni disku s Windows)
BOOL IsSambaDrivePath(const char* path);

// test jestli jde o UNC cestu (detekuje oba formaty: \\server\share i \\?\UNC\server\share)
BOOL IsUNCPath(const char* path);

// test jestli jde o UNC root (detekuje jen format: \\server\share)
BOOL IsUNCRootPath(const char* path);

// vytvoreni souboru se jmenem 'fileName' pres klasicke volani Win32 API
// CreateFile (lpSecurityAttributes==NULL, dwCreationDisposition==CREATE_NEW,
// hTemplateFile==NULL); tato metoda resi kolizi 'fileName' s dosovym nazvem
// jiz existujiciho souboru/adresare (jen pokud nejde i o kolizi s dlouhym
// jmenem souboru/adresare) - zajisti zmenu dosoveho jmena tak, aby se soubor se
// jmenem 'fileName' mohl vytvorit (zpusob: docasne prejmenuje konfliktni
// soubor/adresar na jine jmeno a po vytvoreni 'fileName' ho prejmenuje zpet);
// vraci handle souboru nebo pri chybe INVALID_HANDLE_VALUE (chyba je v GetLastError());
// neni-li 'encryptionNotSupported' NULL a soubor nejde otevrit s Encrypted
// atributem, zkusi ho otevrit jeste bez Encrypted atributu, pokud se to povede,
// soubor je smazan a do 'encryptionNotSupported' se zapise TRUE - navratova hodnota
// funkce a GetLastError() obsahuji "puvodni" chybu (otevirani s Encrypted atributem)
HANDLE SalCreateFileEx(const char* fileName, DWORD desiredAccess,
                       DWORD shareMode, DWORD flagsAndAttributes,
                       BOOL* encryptionNotSupported);

// zkontroluje posledni komponentu jmena v ceste 'path', pokud obsahuje na
// zacatku nebo na konci mezeru nebo na konci tecku, vraci TRUE, jinak FALSE
BOOL FileNameInvalidForManualCreate(const char* path);

// oriznuti mezer na zacatku a na konci jmena (CutWS nebo StripWS nebo CutWhiteSpace nebo StripWhiteSpace)
// vraci TRUE pokud doslo k orezu
BOOL CutSpacesFromBothSides(char* path);

// oriznuti mezer na zacatku a mezer a tecek na konci jmena, dela to tak Explorer
// a lidi tlacili, ze to tak taky chteji, viz https://forum.altap.cz/viewtopic.php?f=16&t=5891
// a https://forum.altap.cz/viewtopic.php?f=2&t=4210
// vraci TRUE pokud se zmeni obsah 'path'
BOOL MakeValidFileName(char* path);

// pokud 'name' konci na mezeru/tecku, udela se kopie 'name' do 'nameCopy' a doplni
// se na konec '\\', pak se 'name' nasmeruje do 'nameCopy'; bezne API funkce
// orezavaji tise mezery/tecky z konce cesty a pracuji pak s jinymi soubory/adresari
// nez chceme, pridany '\\' na konci to resi
void MakeCopyWithBackslashIfNeeded(const char*& name, char (&nameCopy)[3 * MAX_PATH]);

// vraci TRUE pokud jmeno konci na backslash ('\\' pridany na konci resi invalidni jmena)
BOOL NameEndsWithBackslash(const char* name);

// pokud 'name' konci na mezeru/tecku nebo obsahuje ':' (kolize s ADS), vraci TRUE, jinak FALSE,
// je-li 'ignInvalidName' TRUE, vraci TRUE jen pokud 'name' obsahuje ':' (kolize s ADS)
BOOL FileNameIsInvalid(const char* name, BOOL isFullName, BOOL ignInvalidName = FALSE);

// vraci FALSE, pokud obsazene komponenty cesty konci na mezeru/tecku
// a je-li 'cutPath' TRUE, zaroven cestu zkrati k prvni invalidni komponente
// (pro chybove hlaseni), jinak vraci TRUE
BOOL PathContainsValidComponents(char* path, BOOL cutPath);

// vytvoreni adresare se jmenem 'name' pres klasicke volani Win32 API
// CreateDirectory(lpSecurityAttributes==NULL); tato metoda resi kolizi 'name'
// s dosovym nazvem jiz existujiciho souboru/adresare (jen pokud nejde i o
// kolizi s dlouhym jmenem souboru/adresare) - zajisti zmenu dosoveho jmena
// tak, aby se adresar se jmenem 'name' mohl vytvorit (zpusob: docasne prejmenuje
// konfliktni soubor/adresar na jine jmeno a po vytvoreni 'name' ho
// prejmenuje zpet); dale resi jmena koncici na mezery (umi je vytvorit, narozdil
// od CreateDirectory, ktera mezery bez varovani orizne a vytvori tak vlastne
// jiny adresar); vraci TRUE pri uspechu, pri chybe FALSE (vraci v 'err'
// (neni-li NULL) kod Windows chyby)
BOOL SalCreateDirectoryEx(const char* name, DWORD* err);

void InitLocales();                                       // nutne volat pres pouzitim NumberToStr a PrintDiskSize
char* NumberToStr(char* buffer, const CQuadWord& number); // prevod int -> prehlednejsi string, !char buffer[50]!
int NumberToStr2(char* buffer, const CQuadWord& number);  // prevod int -> prehlednejsi string, !char buffer[50]!, vraci pocet znaku nakopirovanych do bufferu
char* GetErrorText(DWORD error);                          // prevadi cislo chyby na string
WCHAR* GetErrorTextW(DWORD error);                        // prevadi cislo chyby na string
BOOL IsDirError(DWORD err);                               // tyka se chyba prace s adresari ?

// normal i UNC cesty: maji stejny root?
BOOL HasTheSameRootPath(const char* path1, const char* path2);

// zjisti, jestli maji obe cesty stejny root a jsou z jednoho svazku (resi
// cesty obsahujici reparse pointy a substy)
// POZOR: jde o dost POMALOU funkci (az 200ms)
BOOL HasTheSameRootPathAndVolume(const char* p1, const char* p2);

// vraci TRUE, pokud jsou cesty 'path1' a 'path2' ze stejneho svazku; v 'resIsOnlyEstimation'
// (neni-li NULL) vraci TRUE, pokud neni vysledek jisty (jisty je jen v pripade, ze se
// podarilo ziskat "volume name" (GUID) u obou cest, coz pripada v uvahu jen pro lokalni
// cesty pod W2K nebo novejsimi z rady NT)
// POZOR: jde o dost POMALOU funkci (az 200ms)
BOOL PathsAreOnTheSameVolume(const char* path1, const char* path2, BOOL* resIsOnlyEstimation);

// porovna dve cesty: ignore-case, ignoruje take jeden backslash na zacatku a konci cest
BOOL IsTheSamePath(const char* path1, const char* path2);

// zjisti, jestli je cesta typu plugin FS, 'path' je zjistovana cesta, 'fsName' je
// buffer MAX_PATH znaku pro jmeno FS (nebo NULL), vraci 'userPart' (je-li != NULL) - ukazatel
// do 'path' na prvni znak pluginem definovane cesty (za prvni ':')
BOOL IsPluginFSPath(const char* path, char* fsName = NULL, const char** userPart = NULL);
BOOL IsPluginFSPath(char* path, char* fsName = NULL, char** userPart = NULL);

// test, jestli jde o cestu s URL, napr. "file:///c|/WINDOWS/clock.avi" = "c:\\WINDOWS\\clock.avi"
BOOL IsFileURLPath(const char* path);

// zjisti podle pripony souboru, jestli jde o link (.lnk, .pif nebo .url); je-li, vraci 1,
// jinak vraci 0
int IsFileLink(const char* fileExtension);

// ziska UNC i normalni root cestu z 'path', v 'root' vraci cestu ve formatu 'C:\' nebo '\\SERVER\SHARE\',
// vraci pocet znaku root cesty (bez null-terminatoru); 'root' je buffer aspon MAX_PATH znaku, pri delsi
// UNC root ceste dojde k orezu na MAX_PATH-2 znaku a doplneni backslashe (stejne to na 100% neni root cesta)
int GetRootPath(char* root, const char* path);

// vraci ukazatel za root (presneji na backslash hned za rootem) UNC i normalni cesty 'path'
const char* SkipRoot(const char* path);

// vraci TRUE pokud se podari 'path' (UNC i normal cesta) zkratit o posledni adresar
// (zariznuti na poslednim backslashi - v oriznute ceste zustava na konci backslash jen u 'c:\'),
// v 'cutDir' se vraci ukazatel na posledni adresar (odriznutou cast)
// nahrazka za PathRemoveFileSpec
BOOL CutDirectory(char* path, char** cutDir = NULL);

// spoji 'path' a 'name' do 'path', zajisti spojeni backslashem, 'path' je buffer alespon 'pathSize' znaku
// vraci TRUE pokud se 'name' veslo za 'path'; je-li 'path' nebo 'name' prazdne,
// spojovaci (pocatecni/ukoncovaci) backslash nebude (napr. "c:\" + "" -> "c:")
BOOL SalPathAppend(char* path, const char* name, int pathSize);

// pokud jeste 'path' nekonci na backslash, prida ho na konec 'path'; 'path' je buffer alespon 'pathSize'
// znaku; vraci TRUE pokud se backslash vesel za 'path'; je-li 'path' prazdne, backslash se neprida
BOOL SalPathAddBackslash(char* path, int pathSize);

// pokud je v 'path' na konci backslash, odstrani ho
void SalPathRemoveBackslash(char* path);

// prevede vsechny '/' na '\\' a zaroven pokud jsou za sebou dva nebo vice '\\',
// necha jen jeden (krome dvou '\\' na zacatku stringu, coz oznacuje UNC cestu)
void SlashesToBackslashesAndRemoveDups(char* path);

// z plneho jmena udela jmeno ("c:\path\file" -> "file")
void SalPathStripPath(char* path);

// pokud je ve jmene pripona, odstrani ji
void SalPathRemoveExtension(char* path);

// pokud ve jmenu 'path' jeste neni pripona, prida priponu 'extension' (napr. ".txt"), 'path' je buffer
// alespon 'pathSize' znaku, vraci FALSE pokud buffer 'path' nestaci pro vyslednou cestu
BOOL SalPathAddExtension(char* path, const char* extension, int pathSize);

// zmeni/prida priponu 'extension' (napr. ".txt") ve jmenu 'path', 'path' je buffer
// alespon 'pathSize' znaku, vraci FALSE pokud buffer 'path' nestaci pro vyslednou cestu
BOOL SalPathRenameExtension(char* path, const char* extension, int pathSize);

// vraci ukazatel do 'path' na jmeno souboru/adresare (backslash na konci 'path' ignoruje),
// pokud jmeno neobsahuje jine backslashe nez na konci retezce, vraci 'path'
const char* SalPathFindFileName(const char* path);

// Pracuje pro normalni i UNC cesty.
// Vrati pocet znaku spolecne cesty. Na normalni ceste musi byt root ukonceny zpetnym
// lomitkem, jinak funkce vrati 0. ("C:\"+"C:"->0, "C:\A\B"+"C:\"->3, "C:\A\B\"+"C:\A"->4,
// "C:\AA\BB"+"C:\AA\CC"->5)
int CommonPrefixLength(const char* path1, const char* path2);

// Vraci TRUE, pokud je cesta 'prefix' zakladem cesty 'path'. Jinak vraci FALSE.
// "C:\aa","C:\Aa\BB"->TRUE
// "C:\aa","C:\aaa"->FALSE
// "C:\aa\","C:\Aa"->TRUE
// "\\server\share","\\server\share\aaa"->TRUE
// Pracuje pro normalni i UNC cesty.
BOOL SalPathIsPrefix(const char* prefix, const char* path);

// odstrani ".." (vynecha ".." spolu s jednim podadresarem vlevo) a "." (vynecha jen ".")
// z cesty; podminkou je backslash jako oddelovac podadresaru; 'afterRoot' ukazuje za root
// zpracovavane cesty (zmeny cesty se deji jen za 'afterRoot'); vraci TRUE pokud se upravy
// podarily, FALSE pokud se nedaji odstranit ".." (vlevo uz je root)
BOOL SalRemovePointsFromPath(char* afterRoot);
BOOL SalRemovePointsFromPath(WCHAR* afterRoot);

// upravuje relativni nebo absolutni cestu na absolutni bez '.', '..' a koncoveho backslashe (krom
// "X:\"); je-li 'curDir' NULL, relativni cesty typu "\path" a "path" vraci chybu (neurcitelne), jinak je
// 'curDir' platna upravena aktualni cesta (UNC i normal); aktualni cesty ostatnich disku (mimo
// 'curDir'; jen normal, ne UNC) jsou v DefaultDir (pred pouzitim je dobre zavolat
// CMainWindow::UpdateDefaultDir); 'name' - in/out buffer alespon MAX_PATH znaku (jeho velikost je v 'nameBufSize');
// neni-li 'nextFocus' NULL a zadana relativni cesta neobsahuje backslash - strcpy(nextFocus, name)
// vraci TRUE - jmeno 'name' je pripraveno k pouziti, jinak neni-li 'errTextID' NULL obsahuje
// chybu (konstanty pro LoadStr - IDS_SERVERNAMEMISSING, IDS_SHARENAMEMISSING, IDS_TOOLONGPATH,
// IDS_INVALIDDRIVE, IDS_INCOMLETEFILENAME, IDS_EMPTYNAMENOTALLOWED a IDS_PATHISINVALID);
// v 'callNethood' (neni-li NULL) se vraci TRUE ma-li se volat Nethood plugin pri chybach
// IDS_SERVERNAMEMISSING a IDS_SHARENAMEMISSING, je-li 'allowRelPathWithSpaces' TRUE, neoreze
// mezery ze zacatku relativni cesty (bezne dela, aby si lidi nevytvareli omylem nazvy s mezerami
// na zacatku, mezery a tecky na konci orezou wokna)
// vraci TRUE pokud v ceste neni chyba, jinak se vraci FALSE (napr. "\\\" nebo "\\server\\")
BOOL SalGetFullName(char* name, int* errTextID = NULL, const char* curDir = NULL,
                    char* nextFocus = NULL, BOOL* callNethood = NULL, int nameBufSize = MAX_PATH,
                    BOOL allowRelPathWithSpaces = FALSE);

// zkusi pristup na cestu 'path' (normal nebo UNC), probiha ve vedlejsim threadu, takze
// umoznuje prerusit zkousku klavesou ESC (po jiste dobe vybali okenko s hlasenim o ESC)
// 'echo' TRUE znamena povoleny vypis chybove hlasky (pokud cesta nebude pristupna);
// 'err' ruzne od ERROR_SUCCESS v kombinaci s 'echo' TRUE pouze zobrazi chybu (na cestu
// se jiz nepristupuje); 'postRefresh' je parametr do volani EndStopRefresh (normalne TRUE);
// 'parent' je parent messageboxu; vraci ERROR_SUCCESS v pripade, ze je cesta v poradku,
// jinak vraci standardni Windowsovy kod chyby nebo ERROR_USER_TERMINATED v pripade, ze
// user pouzil klavesu ESC k preruseni testu
DWORD SalCheckPath(BOOL echo, const char* path, DWORD err, BOOL postRefresh, HWND parent);

// zkusi jestli je cesta 'path' pristupna, prip. obnovi sitova spojeni pomoci funkci
// CheckAndRestoreNetworkConnection a CheckAndConnectUNCNetworkPath; vraci TRUE pokud
// je cesta pristupna; 'parent' je parent messageboxu; 'tryNet' je TRUE pokud ma smysl
// zkouset obnovit sitova spojeni
BOOL SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet);

// zkusi jestli je cesta 'path' pristupna, prip. ji zkrati; je-li 'tryNet' TRUE, prip. obnovi
// sitova spojeni pomoci funkci CheckAndRestoreNetworkConnection a CheckAndConnectUNCNetworkPath
// (je-li 'donotReconnect' TRUE, zjisti se pouze chyba, obnova spojeni uz se neprovede) a nastavi
// 'tryNet' na FALSE; vraci 'err' (kod chyby aktualni cesty), 'lastErr' (kod chyby vedouci ke
// zkraceni cesty), 'pathInvalid' (TRUE pokud se zkusila obnova sit. spojeni bez uspechu),
// 'cut' (TRUE pokud je vysledna cesta zkracena); 'parent' je parent messageboxu; vraci TRUE
// pokud je vysledna cesta 'path' pristupna
BOOL SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err, DWORD& lastErr,
                                   BOOL& pathInvalid, BOOL& cut, BOOL donotReconnect);

// rozpozna o jaky typ cesty (FS/windowsova/archiv) jde a postara se o rozdeleni na
// jeji casti (u FS jde o fs-name a fs-user-part, u archivu jde o path-to-archive a
// path-in-archive, u windowsovych cest jde o existujici cast a zbytek cesty), u FS cest
// se nic nekontroluje, u windowsovych (normal + UNC) cest se kontroluje kam az cesta existuje
// (prip. obnovi sitove spojeni), u archivu se kontroluje existence souboru archivu
// (rozliseni archivu dle pripony); pouziva SalGetFullName, proto je dobre napred zavolat
// CMainWindow::UpdateDefaultDir)
// 'path' je plna nebo relativni cesta (buffer min. 'pathBufSize' znaku; u relativnich cest se
// uvazuje aktualni cesta 'curPath' (neni-li NULL) jako zaklad pro vyhodnoceni plne cesty;
// 'curPathIsDiskOrArchive' je TRUE pokud je 'curPath' windowsova nebo archivova cesta;
// pokud je aktualni cesta archivova, obsahuje 'curArchivePath' jmeno archivu, jinak je NULL),
// do 'path' se ulozi vysledna plna cesta (musi byt min. 'pathBufSize' znaku); vraci TRUE pri
// uspesnem rozpoznani, pak 'type' je typ cesty (viz PATH_TYPE_XXX) a 'secondPart' je nastavene:
// - do 'path' na pozici za existujici cestu (za '\\' nebo na konci retezce; existuje-li
//   v ceste soubor, ukazuje za cestu k tomuto souboru) (typ cesty windows), POZOR: neresi
//   se delka vracene casti cesty (cela cesta muze byt delsi nez MAX_PATH)
// - za soubor archivu (typ cesty archiv), POZOR: neresi se delka cesty v archivu (muze byt
//   delsi nez MAX_PATH)
// - za ':' za nazvem file-systemu - user-part cesty file-systemu (typ cesty FS), POZOR: neresi
//   se delka user-part cesty (muze byt delsi nez MAX_PATH);
// pokud vraci TRUE je jeste 'isDir' nastavena na:
// - TRUE pokud existujici cast cesty je adresar, FALSE == soubor (typ cesty windows)
// - FALSE pro cesty typu archiv a FS;
// pokud vrati FALSE, userovi byla vypsana chyba (az na jednu vyjimku - viz popis SPP_INCOMLETEPATH),
// ktera pri rozpoznavani nastala (neni-li 'error' NULL, vraci se v nem jedna z konstant SPP_XXX);
// 'errorTitle' je titulek messageboxu s chybou; pokud je 'nextFocus' != NULL a windowsova/archivova
// cesta neobsahuje '\\' nebo na '\\' jen konci, nakopiruje se cesta do 'nextFocus' (viz SalGetFullName)
BOOL SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                  const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                  const char* curPath, const char* curArchivePath, int* error,
                  int pathBufSize);

// ziska z windowsove cilove cesty existujici cast a operacni masku; pripadnou neexistujici cast
// umozni vytvorit; pri uspechu vraci TRUE a existujici windowsovou cilovou cestu (v 'path')
// a nalezenou operacni masku (v 'mask' - ukazuje do bufferu 'path', ale cesta a maska jsou oddelene
// nulou; pokud v ceste neni maska, automaticky vytvori masku "*.*"); 'parent' - parent pripadnych
// messageboxu; 'title' + 'errorTitle' jsou titulky messageboxu s informaci + chybou; 'selCount' je
// pocet oznacenych souboru a adresaru; 'path' je na vstupu cilova cesta ke zpracovani, na vystupu
// (alespon 2 * MAX_PATH znaku) existujici cilova cesta; 'secondPart' ukazuje do 'path' na pozici
// za existujici cestu (za '\\' nebo na konec retezce; existuje-li v ceste soubor, ukazuje za cestu
// k tomuto souboru); 'pathIsDir' je TRUE/FALSE pokud existujici cast cesty je adresar/soubor;
// 'backslashAtEnd' je TRUE pokud byl pred provedenim "parse" na konci 'path' backslash (napr.
// SalParsePath takovy backslash rusi); 'dirName' + 'curDiskPath' nejsou NULL pokud je oznaceny
// max. jeden soubor/adresar (jeho jmeno bez cesty je v 'dirName'; pokud neni nic oznacene, bere
// se focus) a aktualni cesta je windowsova (cesta je v 'curDiskPath'); 'mask' je na vystupu
// ukazatel na operacni masku do bufferu 'path'; pokud je v ceste chyba, vraci metoda FALSE,
// problem uz byl uzivateli ohlasen
BOOL SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                         char* path, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                         const char* dirName, const char* curDiskPath, char*& mask);

// ziska z cilove cesty existujici cast a operacni masku; pripadnou neexistujici cast rozpozna; pri
// uspechu vraci TRUE, relativni cestu k vytvoreni (v 'newDirs'), existujici cilovou cestu (v 'path';
// existujici jen za predpokladu vytvoreni relativni cesty 'newDirs') a nalezenou operacni masku
// (v 'mask' - ukazuje do bufferu 'path', ale cesta a maska jsou oddelene nulou; pokud v ceste neni
// maska, automaticky vytvori masku "*.*"); 'parent' - parent pripadnych messageboxu;
// 'title' + 'errorTitle' jsou titulky messageboxu s informaci + chybou; 'selCount' je pocet oznacenych
// souboru a adresaru; 'path' je na vstupu cilova cesta ke zpracovani, na vystupu (alespon 2 * MAX_PATH
// znaku) existujici cilova cesta (vzdy konci backslashem); 'afterRoot' ukazuje do 'path' za root cesty
// (za '\\' nebo na konec retezce); 'secondPart' ukazuje do 'path' na pozici za existujici cestu (za
// '\\' nebo na konec retezce; existuje-li v ceste soubor, ukazuje za cestu k tomuto souboru);
// 'pathIsDir' je TRUE/FALSE pokud existujici cast cesty je adresar/soubor; 'backslashAtEnd' je
// TRUE pokud byl pred provedenim "parse" na konci 'path' backslash (napr. SalParsePath takovy
// backslash rusi); 'dirName' + 'curPath' nejsou NULL pokud je oznaceny max. jeden soubor/adresar
// (jeho jmeno bez cesty je v 'dirName'; jeho cesta je v 'curPath'; pokud neni nic oznacene, bere
// se focus); 'mask' je na vystupu ukazatel na operacni masku do bufferu 'path'; neni-li 'newDirs' NULL,
// pak jde o buffer (o velikosti alespon MAX_PATH) pro relativni cestu (vzhledem k existujici ceste
// v 'path'), kterou je nutne vytvorit (uzivatel s vytvorenim souhlasi, byl pouzit stejny dotaz jako
// u kopirovani z disku na disk; prazdny retezec = nic nevytvaret); je-li 'newDirs' NULL a je-li
// potreba vytvorit nejakou relativni cestu, je jen vypsana chyba; 'isTheSamePathF' je funkce pro
// porovnani dvou cest (potrebna jen pokud 'curPath' neni NULL), je-li NULL pouzije se IsTheSamePath;
// pokud je v ceste chyba, vraci metoda FALSE, problem uz byl uzivateli ohlasen
BOOL SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle, int selCount,
                         char* path, char* afterRoot, char* secondPart, BOOL pathIsDir, BOOL backslashAtEnd,
                         const char* dirName, const char* curPath, char*& mask, char* newDirs,
                         SGP_IsTheSamePathF isTheSamePathF);

// zjisti jestli je mozne retezec 'fileNameComponent' pouzit jako komponentu
// jmena na Windows filesystemu (resi retezce delsi nez MAX_PATH-4 (4 = "C:\"
// + null-terminator), prazdny retezec, retezce znaku '.', retezce white-spaces,
// znaky "*?\\/<>|\":" a jednoducha jmena typu "prn" a "prn  .txt")
BOOL SalIsValidFileNameComponent(const char* fileNameComponent);

// pretvori retezec 'fileNameComponent' tak, aby mohl byt pouzity jako komponenta
// jmena na Windows filesystemu (resi retezce delsi nez MAX_PATH-4 (4 = "C:\"
// + null-terminator), resi prazdny retezec, retezce znaku '.', retezce
// white-spaces, znaky "*?\\/<>|\":" nahradi '_' + jednoducha jmena typu "prn"
// a "prn  .txt" doplni o '_' na konci nazvu); 'fileNameComponent' musi jit
// rozsirit alespon o jeden znak (maximalne se vsak z 'fileNameComponent'
// pouzije MAX_PATH bytu)
void SalMakeValidFileNameComponent(char* fileNameComponent);

// tisk velikosti mista na disku, mode==0 "1.23 MB", mode==1 "1 230 000 bytes, 1.23 MB",
// mode==2 "1 230 000 bytes", mode==3 (vzdy v celych KB), mode==4 (jako mode==0, ale vzdy
// aspon 3 platne cislice, napr. "2.00 MB")
char* PrintDiskSize(char* buf, const CQuadWord& size, int mode);

// prevadi pocet sekund na retezec ("5 sec", "1 hr 34 min", atp.); 'buf' je
// buffer pro vysledny text, musi byt velky aspon 100 znaku; 'secs' je pocet sekund;
// vraci 'buf'
char* PrintTimeLeft(char* buf, CQuadWord const& secs);

// zdvojuje '&' - hodi se pro cesty zobrazovane v menu ('&&' se zobrazi jako '&');
// 'buffer' je vstupne/vystupni retezec, 'bufferSize' je velikost 'buffer' v bytech;
// vraci TRUE pokud zdvojenim nedoslo ke ztrate znaku z konce retezce (buffer byl dost
// veliky)
BOOL DuplicateAmpersands(char* buffer, int bufferSize, BOOL skipFirstAmpersand = FALSE);

// likviduje '&' - hodi se pro prikazy z menu, ktere potrebujeme zobrazit ocistene
// od horkych klaves; najde-li dvojici "&&", nahradi ji jednim znakem '&'
// 'text' je vstupne/vystupni retezec
void RemoveAmpersands(char* text);

// zdvojuje '\\' - hodi se pro texty, ktere posilame do LookForSubTexts, ktera '\\\\'
// zase zredukuje na '\\'; 'buffer' je vstupne/vystupni retezec, 'bufferSize' je velikost
// 'buffer' v bytech; vraci TRUE pokud zdvojenim nedoslo ke ztrate znaku z konce retezce
// (buffer byl dost veliky)
BOOL DuplicateBackslashes(char* buffer, int bufferSize);

// zdvojuje '$' - slouzi pro import starych cest (hotpaths), ktere mohou obsahovat $(SalDir)
// a nove na nich podporuje Sal/Env promenne jako napriklad $(SalDir) nebo $(WinDir)
// behem zavadeni jsem narazil na to, ze 2.5RC1, kde jsme podporili tyto promenne pro editory,
// viewery, archivery se tato expanze nedelala; dodatecne to neresim, zavedu konverzi pouze
// pro HotPaths
// 'buffer' je vstupne/vystupni retezec, 'bufferSize' je velikost 'buffer' v bytech;
// vraci TRUE pokud zdvojenim nedoslo ke ztrate znaku z konce retezce (buffer byl dost
// veliky)
BOOL DuplicateDollars(char* buffer, int bufferSize);

// najde si v 'buf' jmeno (preskoci mezery na zacatku i konci) a pokud existuje ('buf'
// neobsahuje jen mezery), neni v uvozovkach a obsahuje aspon jednu mezeru, da ho do
// uvozovek; vraci FALSE pokud neni dost mista na pridani uvozovek ('bufSize' je
// velikost 'buf')
BOOL AddDoubleQuotesIfNeeded(char* buf, int bufSize);

// oriznuti '"' na zacatku a na konci 'path' (CutDoubleQuotes nebo StripDoubleQuotes nebo CutQuotes nebo StripQuotes)
// vraci TRUE pokud doslo k orezu
BOOL CutDoubleQuotesFromBothSides(char* path);

// do 1/5 sekundy pockame na pusteni ESC (aby se po ESC v dialogu hned neprerusilo
// napr. cteni listingu v panelu)
void WaitForESCRelease();

// zkontroluje jestli je root-parent okna 'parent' foreground window, pokud ne,
// udela se FlashWindow(root-parent, TRUE) a vrati se root-parent, jinak se vraci NULL
HWND GetWndToFlash(HWND parent);

// projde v threadu 'tid' (0=aktualni) vsechna okna (EnumThreadWindows) a vsem enablovanym a
// viditelnym dialogum (class name "#32770") vlastnenym oknem 'parent' postne WM_CLOSE;
// pouziva se pri critical shutdown k odblokovani okna/dialogu, nad kterym jsou otevrene
// modalni dialogy, hrozi-li vice vrstev, je nutne volat opakovane
void CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid = 0);

// vraci zobrazitelnou podobu atributu souboru/adresare; 'text' je buffer aspon 10 znaku;
// 'attrs' jsou atributy souboru/adresare
void GetAttrsString(char* text, DWORD attrs);

// nakopiruje retezec 'srcStr' za retezec 'dstStr' (za jeho koncovou nulu);
// 'dstStr' je buffer o velikosti 'dstBufSize' (musi byt nejmene rovno 2);
// pokud se do bufferu oba retezce nevejdou, jsou zkraceny (vzdy tak, aby se
// veslo co nejvice znaku z obou retezu)
void AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr);

// vytvori plne jmeno souboru (alokovane); pokud neni 'dosName' NULL a 'path'+'name' je prilis
// dlouhe jmeno, zkusi se spojit 'path'+'dosName'; pokud 'skip', 'skipAll' a 'sourcePath' nejsou
// NULL a dojde k chybe "prilis dlouhe jmeno", umozni uzivateli toto jmeno preskocit (v tomto pripade
// vraci NULL a ve 'skip' TRUE), pokud user da "Skip All", nastavi 'skipAll' na TRUE
// 'sourcePath' je pouzita pro Focus tlacitko (v panelu zobrazime prilis dlouhou komponentu ve zdrojove
// ceste, ktera by v cilove ceste zpusobila problem).
char* BuildName(char* path, char* name, char* dosName = NULL, BOOL* skip = NULL, BOOL* skipAll = NULL,
                const char* sourcePath = NULL);

// vraci z panelu datum+cas pro soubor/adresar 'f' (resi i datumy+casy dodavane pluginy - nemusi
// byt platne)
void GetFileDateAndTimeFromPanel(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData,
                                 const CFileData* f, BOOL isDir, SYSTEMTIME* st, BOOL* validDate,
                                 BOOL* validTime);

// vraci z panelu velikost pro soubor/adresar 'f' (resi i velikosti dodavane pluginy - nemusi
// byt platne)
void GetFileSizeFromPanel(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData,
                          const CFileData* f, BOOL isDir, CQuadWord* size, BOOL* validSize);

void DrawSplitLine(HWND HWindow, int newDragSplitX, int oldDragSplitX, RECT client);
BOOL InitializeCheckThread(); // inicializace threadu pro CFilesWindow::CheckPath()
void ReleaseCheckThreads();   // uvolneni threadu pro CFilesWindow::CheckPath()
void InitDefaultDir();        // inicializace pole DefaultDir (posledni navstivene cesty na vsech drivech)

// zobrazi/schova zpravu ve vlastnim threadu (neodcerpa message-queue), zobrazi najednou jen
// jednu zpravu, opakovane volani ohlasi chybu do TRACE (neni fatalni), 'delay' je zpozdeni
// pred otevrenim okenka (od okamziku volani CreateSafeWaitWindow)
// 'message' muze byt vicerakova; jednotlive radky se oddeluji znakem '\n'
// 'caption' muze byt NULL: pouzije se pak "Open Salamander"
// 'showCloseButton' udava, zda bude okenko obsahovat tlacitko Close
// 'hForegroundWnd' urcuje okno, ktere musi byt aktivni, aby bylo okenko zobrazeno
// a zaroven urcuje okno, ktere bude aktivovana pri kliknuti do wait okenka
void CreateSafeWaitWindow(const char* message, const char* caption, int delay,
                          BOOL showCloseButton, HWND hForegroundWnd);
void DestroySafeWaitWindow(BOOL killThread = FALSE);
// zhasne 'show'==FALSE a pak zobrazi 'show'==TRUE vytvorene okenko
// volat jako reakci na WM_ACTIVATE z okna hForegroundWnd:
//    case WM_ACTIVATE:
//    {
//      ShowSafeWaitWindow(LOWORD(wParam) != WA_INACTIVE);
//      break;
//    }
// Pokud je thread (ze ktereho bylo okenko vytvoreno) zamestnan, nedochazi
// k distribuci zprav, takze nedojde ani k doruceni WM_ACTIVATE pri kliknuti
// na jinou aplikaci. Zpravy se doruci az ve chvili zobrazeni messageboxu,
// coz presne potrebujeme: docasne se nechame schovat a pozdeji (po zavreni
// messageboxu a aktivaci okna hForegroundWnd) zase zobrazit.
void ShowSafeWaitWindow(BOOL show);
// po zavolani CreateSafeWaitWindow nebo ShowSafeWaitWindow vraci funkce FALSE az do doby,
// kdy user kliknul mysi na Close tlacitko (pokud je zobrazeno); pak vraci TRUE
BOOL GetSafeWaitWindowClosePressed();
// vraci TRUE pokud uzivatel macka ESC nebo kliknul mysi na Close tlacitko
BOOL UserWantsToCancelSafeWaitWindow();
// slouzi pro dodatecnou zmenu textu v okenku
// POZOR: nedochazi k novemu layoutovani okna a pokud dojde k vetsimu natazeni
// textu, bude orezan; pouzivat napriklad pro countdown: 60s, 55s, 50s, ...
void SetSafeWaitWindowText(const char* message);

// vraci TRUE pokud je Salamander aktivni (foreground window PID == current PID)
BOOL SalamanderActive();

// odstraneni adresare vcetne jeho obsahu (SHFileOperation je priserne pomaly)
void RemoveTemporaryDir(const char* dir);

// pomocna funkce pro pridani jmena do seznamu jmen (oddelenych mezerou), vraci uspech
BOOL AddToListOfNames(char** list, char* listEnd, const char* name, int nameLen);

// pokud adresar neexistuje, umozni jej vytvorit,
// pokud adresar existuje nebo je uspesne vytvoren vraci TRUE
// parent je parent messageboxu s chybami, NULL = hlavni okno Salamandera
// quiet = TRUE  - neptat se jestli chce vytvaret, ale pozor, chyby ukazuje pokud errBuf == NULL
// pokud errBuf != NULL, je errBufSize velikost bufferu pro popis chyby,
// pokud newDir != NULL, je v newDir vracen prvni vytvoreny podadresar (plnou cestou),
// pokud cesta komplet existuje, je newDir=="", newDir ukazuje na buffer o velikosti MAX_PATH
// noRetryButton = TRUE - chybove dialogy nemaji obsahovat Retry/Cancel buttony, ale jen OK button
// manualCrDir = TRUE - nedovolime vytvorit adresar s mezerou na zacatku (pri rucnim vytvareni
// adresare, jinak Windowsum mezery na zacatku nevadi)
BOOL CheckAndCreateDirectory(const char* dir, HWND parent = NULL, BOOL quiet = FALSE, char* errBuf = NULL,
                             int errBufSize = 0, char* newDir = NULL, BOOL noRetryButton = FALSE,
                             BOOL manualCrDir = FALSE);

// smaze z disku prazdne podadresare v 'dir' a je-li 'dir' po vymazu podadresaru prazdny,
// je smazan take
void RemoveEmptyDirs(const char* dir);

// vykona rutina pro otevirani vieweru - pouziva se v CFilesWindow::ViewFile a
// CSalamanderForViewFileOnFS::OpenViewer; dalsi vyuziti se neocekava, proto nejsou
// popsany parametry a navratove hodnoty
BOOL ViewFileInt(HWND parent, const char* name, BOOL altView, DWORD handlerID, BOOL returnLock,
                 HANDLE& lock, BOOL& lockOwner, BOOL addToHistory, int enumFileNamesSourceUID,
                 int enumFileNamesLastFileIndex);

// prevede string ('str' o delce 'len') na unsigned __int64 (muze predchazet
// znamenko '+'; ignoruje white-spaces na zacatku a konci retezce);
// neni-li 'isNum' NULL, vraci se v nem TRUE, pokud cely retezec
// 'str' reprezentuje cislo
unsigned __int64 StrToUInt64(const char* str, int len, BOOL* isNum = NULL);

// spousti handler exceptiony "in-page-error" a "access violation - read/write on XXX" (testuje se
// jestli se exceptiona vztahuje k souboru - 'fileMem' je pocatecni adresa, 'fileMemSize' je velikost
// aktualniho view mapovaneho souboru), pouzito pri mapovani souboru do pameti (chyba
// cteni/zapisu vyvola tuto exceptionu)
int HandleFileException(EXCEPTION_POINTERS* e, char* fileMem, DWORD fileMemSize);

struct CSalamanderVarStrEntry;

// ValidateVarString a ExpandVarString:
// metody pro overovani a expanzi retezcu s promennymi ve tvaru "$(var_name)", "$(var_name:num)"
// (num je sirka promenne, jde o ciselnou hodnotu od 1 do 9999), "$(var_name:max)" ("max" je
// symbol, ktery oznacuje, ze sirka promenne se ridi hodnotou v poli 'maxVarWidths', podrobnosti
// u ExpandVarString) a "$[env_var]" (expanduje hodnotu promenne prostredi); pouziva se pokud si
// uzivatel muze zadat format retezce (jako napr. v info-line) priklad retezce s promennymi:
// "$(files) files and $(dirs) directories" - promenne 'files' a 'dirs'

// kontroluje syntaxi 'varText' (retezce s promennymi), vraci FALSE pokud najde chybu, jeji
// pozici umisti do 'errorPos1' (offset zacatku chybne casti) a 'errorPos2' (offset konce chybne
// casti); 'variables' je pole struktur CSalamanderVarStrEntry, ktere je ukonceno strukturou s
// Name==NULL; 'msgParent' je parent message-boxu s chybami, je-li NULL, chyby se nevypisuji
BOOL ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                       const CSalamanderVarStrEntry* variables);

// naplni 'buffer' vysledkem expanze 'varText' (retezce s promennymi), vraci FALSE pokud je
// 'buffer' maly (predpoklada overeni retezce s promennymi pres ValidateVarString, jinak
// vraci FALSE i pri chybe syntaxe) nebo uzivatel kliknul na Cancel pri chybe environment-variable
// (nenalezena nebo prilis velka); 'bufferLen' je velikost bufferu 'buffer';
// 'variables' je pole struktur CSalamanderVarStrEntry, ktere je ukonceno strukturou
// s Name==NULL; 'param' je ukazatel, ktery se predava do CSalamanderVarStrEntry::Execute
// pri expanzi nalezene promenne; 'msgParent' je parent message-boxu s chybami, je-li NULL,
// chyby se nevypisuji; je-li 'ignoreEnvVarNotFoundOrTooLong' TRUE, ignoruji se chyby
// environment-variable (nenalezena nebo prilis velka), je-li FALSE, zobrazi se messagebox
// s chybou; neni-li 'varPlacements' NULL, ukazuje na pole DWORDu o '*varPlacementsCount' polozkach,
// ktere bude naplneno DWORDy slozenymi vzdy z pozice promenne ve vystupnim bufferu (spodni WORD)
// a poctu znaku promenne (horni WORD); neni-li 'varPlacementsCount' NULL, vraci se v nem pocet
// naplnenych polozek v poli 'varPlacements' (jde vlastne o pocet promennych ve vstupnim
// retezci);
// pokud se tato metoda pouziva jen k expanzi retezce pro jednu hodnotu 'param', meli by
// byt nastaveny 'detectMaxVarWidths' na FALSE, 'maxVarWidths' na NULL a 'maxVarWidthsCount'
// na 0; pokud se ovsem pouziva tato metoda pro expanzi retezce opakovane pro urcitou
// mnozinu hodnot 'param' (napr. u Make File List je to expanze radky postupne pro vsechny
// oznacene soubory a adresare), ma smysl pouzivat i promenne ve tvaru "$(varname:max)",
// u techto promennych se sirka urci jako nejvetsi sirka expandovane promenne v ramci cele
// mnoziny hodnot; omereni nejvetsi sirky expandovane promenne se provadi v prvnim cyklu
// (pro vsechny hodnoty mnoziny) volani ExpandVarString, v prvnim cyklu ma parametr
// 'detectMaxVarWidths' hodnotu TRUE a pole 'maxVarWidths' o 'maxVarWidthsCount' polozkach
// je predem nulovane (slouzi pro ulozeni maxim mezi jednotlivymi volanimi ExpandVarString);
// samotna expanze pak probiha v druhem cyklu (pro vsechny hodnoty mnoziny) volani
// ExpandVarString, v druhem cyklu ma parametr 'detectMaxVarWidths' hodnotu FALSE a pole
// 'maxVarWidths' o 'maxVarWidthsCount' polozkach obsahuje predpocitane nejvetsi sirky
// (z prvniho cyklu)
BOOL ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                     const CSalamanderVarStrEntry* variables, void* param,
                     BOOL ignoreEnvVarNotFoundOrTooLong = FALSE,
                     DWORD* varPlacements = NULL, int* varPlacementsCount = NULL,
                     BOOL detectMaxVarWidths = FALSE, int* maxVarWidths = NULL,
                     int maxVarWidthsCount = 0);

// ulozi na clipboard Unicode verzi textu str o delce len znaku
// vraci ERROR_SUCCESS nebo GetLastError
DWORD AddUnicodeToClipboard(const char* str, int len);

// vrzne text na clipboard; pokud showEcho, zobrazi message box, ze jako OK
// pokud je textLen==-1, napocita si delku sam
BOOL CopyTextToClipboard(const char* text, int textLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL);
BOOL CopyTextToClipboardW(const wchar_t* text, int textLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL);
BOOL CopyHTextToClipboard(HGLOBAL hGlobalText, int textLen = -1, BOOL showEcho = FALSE, HWND hEchoParent = NULL);

// zjisti z bufferu 'pattern' o delce 'patternLen' jestli jde o text (existuje kodova stranka,
// ve ktere obsahuje jen povolene znaky - zobrazitelne a ridici) a pokud jde o text, zjisti take
// jeho kodovou stranku (nejpravdepodobnejsi); 'parent' je parent messageboxu; je-li 'forceText'
// TRUE, neprovadi se kontrola na nepovolene znaky (pouziva se, pokud 'pattern' obsahuje text);
// neni-li 'isText' NULL, vraci se v nem TRUE pokud jde o text; neni-li 'codePage' NULL, jde
// o buffer (min. 101 znaku) pro jmeno kodove stranky (nejpravdepodobnejsi)
void RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                       BOOL* isText, char* codePage);

// nastavi jmeno volajicimu threadu ve VC debuggeru
void SetThreadNameInVC(LPCSTR szThreadName);

// nastavi jmeno volajicimu threadu ve VC debuggeru a Trace Serveru
void SetThreadNameInVCAndTrace(const char* name);

// nacitani konfigurace
class CEditorMasks;
class CViewerMasks;

// funkce spojene s drag&dropem a dalsimi shellovymi vecmi

class CFilesWindow;

// operace shellu
enum CShellAction
{
    saLeftDragFiles,
    saRightDragFiles,
    saContextMenu,
    saCopyToClipboard,
    saCutToClipboard,
    saProperties,
    saPermissions, // stejne jako saProperties, jen se pokusi vybrat "security tab"
};

class CCopyMoveData;
struct CDragDropOperData;

const char* GetCurrentDir(POINTL& pt, void* param, DWORD* effect, BOOL rButton, BOOL& tgtFile,
                          DWORD keyState, int& tgtType, int srcType);
const char* GetCurrentDirClipboard(POINTL& pt, void* param, DWORD* effect, BOOL rButton,
                                   BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType);
BOOL DoCopyMove(BOOL copy, char* targetDir, CCopyMoveData* data, void* param);
void DoDragDropOper(BOOL copy, BOOL toArchive, const char* archiveOrFSName, const char* archivePathOrUserPart,
                    CDragDropOperData* data, void* param);
void DoGetFSToFSDropEffect(const char* srcFSPath, const char* tgtFSPath,
                           DWORD allowedEffects, DWORD keyState,
                           DWORD* dropEffect, void* param);
BOOL UseOwnRutine(IDataObject* pDataObject);
BOOL MouseConfirmDrop(DWORD& effect, DWORD& defEffect, DWORD& grfKeyState);
void DropEnd(BOOL drop, BOOL shortcuts, void* param, BOOL ownRutine, BOOL isFakeDataObject, int tgtType);
void EnterLeaveDrop(BOOL enter, void* param);

// uklada na clipboard prefered drop effect a informaci o puvodu ze Salamandera
void SetClipCutCopyInfo(HWND hwnd, BOOL copy, BOOL salObject);

void ShellAction(CFilesWindow* panel, CShellAction action, BOOL useSelection = TRUE,
                 BOOL posByMouse = TRUE, BOOL onlyPanelMenu = FALSE);
void ExecuteAssociation(HWND hWindow, const char* path, const char* name);

BOOL CanUseShellExecuteWndAsParent(const char* cmdName);

// zjisti jestli je soubor placeholder (online soubor ve OneDrive slozce),
// viz http://msdn.microsoft.com/en-us/library/windows/desktop/dn323738%28v=vs.85%29.aspx
BOOL IsFilePlaceholder(WIN32_FIND_DATA const* findData);

// pred otevrenim editoru nebo vieweru se placeholder prevadi na offline file,
// aby s nim umel viewer/editor pracovat
//BOOL MakeFileAvailOfflineIfOneDriveOnWin81(HWND parent, const char *name);

// nastavi prioritu threadu na normal a vyvola v try-except bloku menu->InvokeCommand()
// pred ukoncenim zase nastavi prioritu threadu na puvodni hodnotu
BOOL SafeInvokeCommand(IContextMenu2* menu, CMINVOKECOMMANDINFO& ici);

// pokud je 'hInstance' NULL, bude se cist z HLanguage; jinak z 'hInstance'
char* LoadStr(int resID, HINSTANCE hInstance = NULL);   // taha string z resourcu
WCHAR* LoadStrW(int resID, HINSTANCE hInstance = NULL); // taha wide-string z resourcu

// podpora pro tvorbu parametrizovanych textu (reseni jednotnych a mnoznych cisel
// v textech); 'lpFmt' je formatovaci retezec pro vysledny text - popis jeho formatu
// nasleduje; vysledny text se vraci v bufferu 'lpOut' o velikosti 'nOutMax' bytu;
// 'lpParArray' je pole parametru textu, 'nParCount' je pocet techto parametru;
// vraci delku vysledneho textu
//
// popis formatovaciho retezce:
//   - na zacatku kazdeho formatovaciho retezce je signatura "{!}"
//   - uznavaji se nasledujici escape sekvence pro potlaceni vyznamu specialnich
//     znaku ve formatovacim retezci (znak backslashe v tomto popisu
//     neni zdvojen): "\\" = "\", "\{" = "{", "\}" = "}", "\:" = ":" a "\|" = "|"
//   - text, ktery neni ve slozenych zavorkach se do vysledneho retezce
//     prenasi beze zmen (az na escape sekvence)
//   - parametrizovany text je ve slozenych zavorkach
//   - kazdy parametrizovany text pouziva jeden parametr z 'lpParArray' - jde
//     o 64-bitovy unsigned int
//   - parametrizovany text obsahuje ruzne vysledne texty pro ruzne intervaly
//     hodnot parametru
//   - jednotlive vysledne texty a meze intervalu se oddeluji znakem "|"
//   - parametrizovany text "{}" se pouziva pro preskoceni jednoho parametru
//     z pole 'lpParArray' (nevytvari zadny vystupni text)
//   - pokud je na zacatku parametrizovaneho textu cislo nasledovane dvojteckou,
//     jde o index parametru (od jedne az do poctu parametru), ktery se ma pouzit,
//     pokud neni index uveden, prideli se automaticky (postupne od jedne az do
//     poctu parametru)
//   - pri zadani indexu parametru nedochazi ke zmene postupne prirazovaneho
//     indexu, napr. v "{!}%d file{2:s|0||1|s} and %d director{y|1|ies}" se pro
//     prvni parametrizovany text pouzije parametr s indexem 2 a pro druhy
//     s indexem 1
//   - parametrizovanych textu se zadanym indexem je mozne pouzit libovolny pocet
//
// priklady formatovacich retezcu:
//   - "{!}director{y|1|ies}" pro hodnotu parametru od 0 do 1 (vcetne) bude
//     "directory" a pro hodnotu od 2 do "nekonecna" (2^64-1) bude "directories"
//   - "{!}soubo{rů|0|r|1|ry|4|rů}" pro hodnotu parametru 0 bude "souborů",
//     pro 1 bude "soubor", pro 2 az 4 (vcetne) bude "soubory" a od 5
//     do "nekonecna" bude "souborů"
int ExpandPluralString(char* lpOut, int nOutMax, const char* lpFmt, int nParCount,
                       const CQuadWord* lpParArray);

//
// Do lpOut zapise retezec v zavislosti na promennych files a dirs:
// files > 0 && dirs == 0  ->  XXX (selected/hidden) files
// files == 0 && dirs > 0  ->  YYY (selected/hidden) directories
// files > 0 && dirs > 0   ->  XXX (selected/hidden) files and YYY directories
//
// kde XXX a YYY odpovidaji hodnotam promennych files a dirs.
// Promenna selectedForm ridi vlozeni slova selected
//
// V forDlgCaption je TRUE/FALSE pokud text je/neni urceny pro titulek dialogu
// (v anglictine jsou nutna velka pocatecni pismena).
//
// Vrati pocet nakopirovanych znaku bez terminatoru.
//
// popis konstant epfdmXXX viz spl_gen.h
int ExpandPluralFilesDirs(char* lpOut, int nOutMax, int files, int dirs,
                          int mode, BOOL forDlgCaption);
int ExpandPluralBytesFilesDirs(char* lpOut, int nOutMax, const CQuadWord& selectedBytes,
                               int files, int dirs, BOOL useSubTexts);

// V textu nalezne pary '<' '>', vyradi je z bufferu a prida odkazy na
// jejich obsah do 'varPlacements'. 'varPlacements' je pole DWORDu o '*varPlacementsCount'
// polozkach, DWORDy jsou slozene vzdy z pozice odkazu ve vystupnim bufferu (spodni WORD)
// a poctu znaku odkazu (horni WORD). Retezce "\<", "\>", "\\" jsou chapany
// jako escape sekvence a budou nahrazeny znaky '<', '>' a '\\'.
// Vraci TRUE v pripade uspechu, jinak FALSE; vzdy nastavi 'varPlacementsCount' na
// pocet zpracovanych promennych.
BOOL LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount);

void MinimizeApp(HWND mainWnd);             // minimalizace app.
void RestoreApp(HWND mainWnd, HWND dlgWnd); // obnova z minimized stavu app.
                                            // meni format jmena (velikost pismen), filename musi byt vzdy terminovan znakem nula5
void AlterFileName(char* tgtName, char* filename, int filenameLen, int format, int change, BOOL dir);

// vraci string s velikosti a casy souboru; Ve 'fileTime' vraci cas, promenna muze byt NULL;
// neni-li 'getTimeFailed' NULL, zapise se do nej TRUE pri chybe ziskavani casu souboru
void GetFileOverwriteInfo(char* buff, int buffLen, HANDLE file, const char* fileName, FILETIME* fileTime = NULL, BOOL* getTimeFailed = NULL);

void ColorsChanged(BOOL refresh, BOOL colorsOnly, BOOL reloadUMIcons);                // volat po zmene barev
HICON GetDriveIcon(const char* root, UINT type, BOOL accessible, BOOL large = FALSE); // ikona drivu
HICON SalLoadIcon(HINSTANCE hDLL, int id, int iconSize);

// SetCurrentDirectory(systemovy adresar) - odpojeni od adresare v panelu
void SetCurrentDirectoryToSystem();

// nahradi substy v ceste 'resPath' jejich cilovymi cestami (prevod na cestu bez SUBST drive-letters);
// vraci FALSE pri chybe
BOOL ResolveSubsts(char* resPath);

// Provede resolve substu a reparse pointu pro cestu 'path', nasledne se pro mount-point cesty
// (pokud chybi tak pro root cesty) pokusi ziskat GUID path. Pri neuspechu vrati FALSE. Pri
// uspechu, vrati TRUE a nastavi 'mountPoint' a 'guidPath' (pokud jsou ruzne od NULL, musi
// odkazovat na buffery o velikosti minimalne MAX_PATH; retezce budou zakonceny zpetnym lomitkem).
BOOL GetResolvedPathMountPointAndGUID(const char* path, char* mountPoint, char* guidPath);

// pokus o vraceni korektnich hodnot (umi i reparse pointy - zadava se komplet cesta misto rootu)
CQuadWord MyGetDiskFreeSpace(const char* path, CQuadWord* total = NULL);
// POZOR: nepouzivat navratovky 'lpNumberOfFreeClusters' a 'lpTotalNumberOfClusters', protoze u vetsich
//        disku jsou v nich nesmysly (DWORD nemusi stacit pro celkovy pocet clusteru), resit pres
//        MyGetDiskFreeSpace(path, total), ktera vraci 64-bitova cisla
BOOL MyGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters);

// vylepsena GetVolumeInformation: pracuje s cestou (prochazi reparse pointy a substy);
// v 'rootOrCurReparsePoint' (neni-li NULL, musi byt aspon MAX_PATH znaku) vraci bud
// root nebo cestu k aktualnimu (poslednimu) lokalnimu reparse pointu na ceste 'path'
// (POZOR: nefunguje pokud neni medium v drivu, GetCurrentLocalReparsePoint() timto netrpi);
// v 'junctionOrSymlinkTgt' (neni-li NULL, musi byt aspon MAX_PATH znaku) vraci
// cil aktualniho reparse pointu nebo prazdny retezec (pokud zadny reparse point neexistuje nebo
// je neznameho typu nebo jde o volume mount point); v 'linkType' (neni-li NULL) vraci typ aktualniho
// reparse pointu: 0 (neznamy nebo neexistuje), 1 (MOUNT POINT), 2 (JUNCTION POINT), 3 (SYMBOLIC LINK)
BOOL MyGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, char* junctionOrSymlinkTgt, int* linkType,
                            LPTSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                            LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                            LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

// vraci cilovou cestu reparse pointu 'repPointDir' v bufferu 'repPointDstBuf' (neni-li NULL)
// o velikosti 'repPointDstBufSize'; 'repPointDir' a 'repPointDstBuf' muzou ukazovat
// do jednoho bufferu (IN/OUT buffer); je-li 'makeRelPathAbs' TRUE a jde o relativni
// symbolicky link, prevede cilovou cestu linku na absolutni;
// vraci TRUE pri uspechu + v 'repPointType' (neni-li NULL) vraci typ reparse pointu:
// 1 (MOUNT POINT), 2 (JUNCTION POINT), 3 (SYMBOLIC LINK)
BOOL GetReparsePointDestination(const char* repPointDir, char* repPointDstBuf, DWORD repPointDstBufSize,
                                int* repPointType, BOOL makeRelPathAbs);

// v 'currentReparsePoint' (aspon MAX_PATH znaku) vraci aktualni (posledni) lokalni
// reparse point, pri neuspechu vraci klasicky root; pri neuspechu vraci FALSE; neni-li
// 'error' NULL, zapise se do nej pri chybe TRUE
BOOL GetCurrentLocalReparsePoint(const char* path, char* currentReparsePoint, BOOL* error = NULL);

// volat jen pro cesty 'path', jejichz root (po odstraneni substu) je DRIVE_FIXED (jinde nema smysl hledat
// reparse pointy); hledame cestu bez reparse pointu, vedouci na stejny svazek jako 'path'; pro cestu
// obsahujici symlink vedouci na sitovou cestu (UNC nebo mapovanou) vracime jen root teto sitove cesty
// (ani Vista neumi delat s reparse pointy na sitovych cestach, takze to je nejspis zbytecne drazdit);
// pokud takova cesta neexistuje z duvodu, ze aktualni (posledni) lokalni reparse point je volume mount
// point (nebo neznamy typ reparse pointu), vracime cestu k tomuto volume mount pointu (nebo reparse
// pointu neznameho typu); pokud cesta obsahuje vice nez 50 reparse pointu (nejspis nekonecny cyklus),
// vracime puvodni cestu;
//
// 'resPath' je buffer pro vysledek o velikosti MAX_PATH; 'path' je puvodni cesta; v 'cutResPathIsPossible'
// (nesmi byt NULL) vracime FALSE pokud vysledna cesta v 'resPath' obsahuje na konci reparse point (volume
// mount point nebo neznamy typ reparse pointu) a tudiz ji nesmime zkracovat (dostali bysme se tim nejspis
// na jiny svazek); je-li 'rootOrCurReparsePointSet' neNULLove a obsahuje-li FALSE a na puvodni ceste je
// aspon jeden lokalni reparse point (reparse pointy na sitove casti cesty ignorujeme), vracime v teto
// promenne TRUE + v 'rootOrCurReparsePoint' (neni-li NULL) vracime plnou cestu k aktualnimu (poslednimu
// lokalnimu) reparse pointu (pozor, ne kam vede); cilovou cestu aktualniho reparse pointu (jen je-li to
// junction nebo symlink) vracime v 'junctionOrSymlinkTgt' (neni-li NULL) + typ vracime v 'linkType':
// 2 (JUNCTION POINT), 3 (SYMBOLIC LINK); v 'netPath' (neni-li NULL) vracime sitovou cestu, na kterou
// vede aktualni (posledni) lokalni symlink v ceste - v teto situaci se root sitove cesty vraci v 'resPath'
void ResolveLocalPathWithReparsePoints(char* resPath, const char* path, BOOL* cutResPathIsPossible,
                                       BOOL* rootOrCurReparsePointSet, char* rootOrCurReparsePoint,
                                       char* junctionOrSymlinkTgt, int* linkType, char* netPath);

// vylepsena GetDriveType: pracuje s cestou (prochazi reparse pointy a substy)
UINT MyGetDriveType(const char* path);

// nase vlastni QueryDosDevice
// 'driveNum' je 0-based (0=A: 2=C: ...)
BOOL MyQueryDosDevice(BYTE driveNum, char* target, int maxTarget);

// detekuje zda 'driveNum' (0=A: 2=C: ...) je substed a pokud ano, kam je pripojeny
// pokud drive neni substed, vraci FALSE
// pokud je substed, vraci TRUE a do promenne 'path' (o maximalni delce 'pathMax')
// ulozi cestu, kam je subst pripojeny
// pokud je 'path' NULL, cesta nebude vracena
// muze vratit cestu v UNC tvaru
BOOL GetSubstInformation(BYTE driveNum, char* path, int pathMax);

// nahradi v retezci posledni znak '.' decimalnim separatorem ziskanym ze systemu LOCALE_SDECIMAL
// delka retezce muze narust, protoze separator muze mit podle MSDN az 4 znaky
// vraci TRUE, pokud byl buffer dostatecne veliky a operaci se povedlo dokoncit, jinak vraci FALSE
BOOL PointToLocalDecimalSeparator(char* buffer, int bufferSize);

typedef WINBASEAPI LONG(WINAPI* MY_FMExtensionProc)(HWND hwnd,
                                                    WORD wMsg,
                                                    LONG lParam);
void GetMessagePos(POINT& p);

// Vrati handle ikony ziskany pomoci SHGetFileInfo nebo NULL v pripade neuspechu.
// Volajici je zodpovedny za destrukci ikony. Ikona je prirazena do HANDLES.
HICON GetFileOrPathIconAux(const char* path, BOOL large, BOOL isDir);

// pokud je root UNCPath nepristupny (pro listing), zkusi navazat sitove spojeni,
// o jmeno a heslo uzivatele si rekne samo, vraci TRUE pokud bylo spojeni navazano,
// vraci FALSE pokud UNCPath neni UNC cesta nebo je root UNCPath pristupny nebo
// pokud se spojeni nepodarilo navazat, v 'pathInvalid' vraci TRUE pokud user prerusil
// dialog zadani jmena+hesla nebo jsme neuspesne zkusili navazat spojeni (napr.
// "credentials conflict"); je-li 'donotReconnect' TRUE, nezkousi se navazat sitove
// spojeni, vrati se rovnou, ze to nevyslo
BOOL CheckAndConnectUNCNetworkPath(HWND parent, const char* UNCPath, BOOL& pathInvalid,
                                   BOOL donotReconnect);

// pokusi se obnovit sitove spojeni (existovalo-li) na 'drive:', parent - predek dialogu,
// vraci TRUE pokud se obnova spojeni podarila (sit. disk je jiz opet namapovan)
// v 'pathInvalid' vraci TRUE pokud user prerusil dialog zadani jmena+hesla nebo
// jsme neuspesne zkusili navazat spojeni (napr. "credentials conflict")
BOOL CheckAndRestoreNetworkConnection(HWND parent, const char drive, BOOL& pathInvalid);

// funkce pro spravu threadu, pri ukoncovani procesu se tyto thready maji uzavrit,
// pokud to neudelaji sami, musi se terminovat
void AddAuxThread(HANDLE view, BOOL testIfFinished = FALSE);
void TerminateAuxThreads();

// vrati TRUE, pokud specifikovany soubor existuje; jinak vrati FALSE
extern "C" BOOL FileExists(const char* fileName);

// vrati TRUE, pokud specifikovany adresar existuje; jinak vrati FALSE
BOOL DirExists(const char* dirName);

// tool tip
void SetCurrentToolTip(HWND hNotifyWindow, DWORD id, int showDelay = 0); // popis v tooltip.h
void SuppressToolTipOnCurrentMousePos();                                 // popis v tooltip.h

// zajisti skoky kurzoru pri Ctrl+Left/Right po zpetnych lomitkach a mezerach
// k editline nebo comboboxu priradi EditWordBreakProc
// je mozne zavolat napriklad v WM_INITDIALOG
// neni nutne odinstalovavat
BOOL InstallWordBreakProc(HWND hWindow);

// vymaze vsechny polozky z dropdown listboxu od comboboxu
// pouzivame pri promazavani historii
void ClearComboboxListbox(HWND hCombo);

// struktura pro WM_USER_VIEWFILE a WM_USER_VIEWFILEWITH
struct COpenViewerData
{
    char* FileName;
    int EnumFileNamesSourceUID;
    int EnumFileNamesLastFileIndex;
};

//
// ****************************************************************************

#define WM_USER_REFRESH_DIR WM_APP + 100   // [BOOL setRefreshEvent, time]
#define WM_USER_S_REFRESH_DIR WM_APP + 101 // [BOOL setRefreshEvent, time]

#define WM_USER_SETDIALOG WM_APP + 103 // [CProgressData *data, 0] \
                                       // nebo [0, 0] - setprogress
#define WM_USER_DIALOG WM_APP + 104    // [int dlgID, void *data]

// icon reader prave nacetl ikonu, vlozil ji do iconcache a rika to panelu
// bezicimu v hlavnim threadu, aby se mohl prekreslit a ikonku zazalohovat do asociaci
// index je dohledana pozice polozky v CFilesWindow::Files/Dirs
#define WM_USER_REFRESHINDEX WM_APP + 105 // [int index, 0]

#define WM_USER_END_SUSPMODE WM_APP + 106  // [0, 0] - rychlejsi aktivace okna
#define WM_USER_DRIVES_CHANGE WM_APP + 107 // [0, 0]
#define WM_USER_ICON_NOTIFY WM_APP + 108   // [0, 0] - mysak je nad ikonkou v taskbare
#define WM_USER_EDIT WM_APP + 110          // [begin, end] oznac tento interval
#define WM_USER_SM_END_NOTIFY WM_APP + 111 // [0, 0] za 200ms naplanuje WM_USER_SM_END_NOTIFY_DELAYED
#define WM_USER_DISPLAYPOPUP WM_APP + 112  // [0, commandID] je treba vybali popupmenu
//#define WM_USER_SETPATHS        WM_APP + 113    // nepouzivat, stara zprava, kterou nam teoreticky mohou stare aplikace zasilat

#define WM_USER_CHAR WM_APP + 114 // notifikace z listview
// [command, index]
// command = 0 pro normalni konfiguraci,
// command = 1 pro otevreni stranky Hot Paths; index pak vyjadruje polozku
// command = 2 pro otevreni stranky User Menu
// command = 3 pro otevreni stranky Internal Viewer
// command = 4 pro otevreni stranky Views, index vyjadruje pohled
// command = 5 pro otevreni stranky Panels
#define WM_USER_CONFIGURATION WM_APP + 115
#define WM_USER_MOUSEWHEEL WM_APP + 116     // [wParam, lParam] z WM_MOUSEWHEEL
#define WM_USER_SKIPONEREFRESH WM_APP + 117 // [0, 0] za 500 ms SkipOneActivateRefresh = FALSE
#define WM_USER_FLASHWINDOW WM_APP + 118    // [0, 0] zablika oknem
#define WM_USER_SHOWWINDOW WM_APP + 119     // [0, 0] vytahne dopopredi okno (restorne, je-li treba)
#define WM_USER_DROPCOPYMOVE WM_APP + 120   // [CTmpDropData *, 0]
#define WM_USER_CHANGEDIR WM_APP + 121      // [convertFSPathToInternal, newDir] - panel zmeni svou cestu (vola ChangeDir)
#define WM_USER_FOCUSFILE WM_APP + 122      // [fileName, newPath] - panel zmeni svou cestu a vybere prislusny soubor
#define WM_USER_CLOSEFIND WM_APP + 123      // [0, 0] - zavola DestroyWindow z threadu find okna
#define WM_USER_SELCHANGED WM_APP + 124     // [0, 0] - notifikaci o zmene selectu
#define WM_USER_MOUSEHWHEEL WM_APP + 126    // [wParam, lParam] z WM_MOUSEHWHEEL

#define WM_USER_CLOSEMENU WM_APP + 130 // [0, 0] - interni pro menu - je treba se zavrit

#define WM_USER_REFRESH_PLUGINFS WM_APP + 133 // [0, 0] - volat u FS Event(FSE_ACTIVATEREFRESH)
#define WM_USER_REFRESH_SHARES WM_APP + 134   // [0, 0] - snooper.cpp hlasi zmenu sharu v registry
#define WM_USER_PROCESSDELETEMAN WM_APP + 135 // [0, 0] - cache.cpp: DeleteManager - spusteni zpracovani novych dat v hl. threadu

#define WM_USER_CANCELPROGRDLG WM_APP + 136  // [0, 0] - CProgressDlgArray: po doruceni teto zpravy do CProgressDialog dojde k cancelu operace (bez dotazu; zavre se dialog operace)
#define WM_USER_FOCUSPROGRDLG WM_APP + 137   // [0, 0] - CProgressDlgArray: po doruceni teto zpravy do CProgressDialog dojde aktivaci dialogu (nebo jeho popupu)
#define WM_USER_ICONREADING_END WM_APP + 138 // [0, 0] - notifikace o ukonceni nacitani ikonek v panelu

//presunuto do shexreg.h (konstanta se nesmi zmenit): #define WM_USER_SALSHEXT_PASTE  WM_APP + 139 // [postMsgIndex, 0] - SalamExt zada o provedeni prikazu Paste

#define WM_USER_DROPUNPACK WM_APP + 140    // [allocatedTgtPath, operation] - drag&drop z archivu: cilova cesta + operace zjistena, nechame provest unpack
#define WM_USER_PROGRDLGEND WM_APP + 141   // [cmd, 0] - CProgressDialog: pod W2K+ asi zbytecne: bypass bugy (zavrene dialogy zustavaly v taskbare) - zpozdene zavreni dialogu
#define WM_USER_PROGRDLGSTART WM_APP + 142 // [0, 0] - CProgressDialog: pod W2K+ asi zbytecne: bypass bugy (na obrazovce zustaval bordel po dialogu) - zpozdeny start worker threadu

//presunuto do shexreg.h (konstanta se nesmi zmenit): #define WM_USER_SALSHEXT_TRYRELDATA WM_APP + 143 // [0, 0] - SalamExt hlasi odblokovani paste-dat (viz CSalShExtSharedMem::BlockPasteDataRelease), nejsou-li data dale chranena, nechame je zrusit

#define WM_USER_DROPFROMFS WM_APP + 144    // [allocatedTgtPath, operation] - drag&drop z FS: cilova cesta + operace zjistena, nechame provest copy/move z FS
#define WM_USER_DROPTOARCORFS WM_APP + 145 // [CTmpDragDropOperData *, 0]

#define WM_USER_SHCHANGENOTIFY WM_APP + 146 // message for SHChangeNotifyRegister [pidlList, SHCNE_xxx (event that occured)]

#define WM_USER_REFRESH_DIR_EX WM_APP + 147 // [long_wait, time] - za (long_wait?5000:200) ms posle WM_USER_REFRESH_DIR_EX_DELAYED

#define WM_USER_SETPROGRESS WM_APP + 148 // [progress, text] slouzi pro prekroceni threadu

// icon reader prave nacetl icon-overlay a rika to panelu bezicimu v hlavnim threadu, aby
// se mohl prekreslit
// index je pozice polozky v CFilesWindow::Files/Dirs
#define WM_USER_REFRESHINDEX2 WM_APP + 149 // [int index, 0]

#define WM_USER_DONEXTFOCUS WM_APP + 150       // [0, 0] - notifikace o zmene NextFocusName
#define WM_USER_CREATEWAITWND WM_APP + 151     // [parent or NULL, delay] - zprava pro thread safe-wait-message: "create && show"
#define WM_USER_DESTROYWAITWND WM_APP + 152    // [killThread, 0] - zprava pro thread safe-wait-message: "hide && destery"
#define WM_USER_SHOWWAITWND WM_APP + 153       // [show, 0] - zprava pro thread safe-wait-message: "show || hide"
#define WM_USER_SETWAITMSG WM_APP + 154        // [0, 0] - zprava pro thread safe-wait-message: byl zmenen text--prekresli se
#define WM_USER_REPAINTALLICONS WM_APP + 155   // [0, 0] - refresh ikon v obou panelech
#define WM_USER_REPAINTSTATUSBARS WM_APP + 156 // [0, 0] - refresh throbberu (dirline) v obou panelech

#define WM_USER_VIEWERCONFIG WM_APP + 158 // [hWnd, 0] - hWnd urcuje viewer, ktery bude po dokonfigurovani zkusen vytahnout nahoru

#define WM_USER_UPDATEPANEL WM_APP + 159 // [0, 0] - pokud je dorucena \
                                         // (doruci ji messageloopa pri otevreni messageboxu), \
                                         // bude invalidatnut panel a bude nastavena scrollbara \
                                         // (pouze pro interni pouziti !!!)

#define WM_USER_AUTOCONFIG WM_APP + 160     // KICKER - autoconfig
#define WM_USER_ACFINDFINISHED WM_APP + 161 // KICKER - autoconfig
#define WM_USER_ACSEARCHING WM_APP + 162    // KICKER - autoconfig
#define WM_USER_ACADDFILE WM_APP + 163      // KICKER - autoconfig
#define WM_USER_ACERROR WM_APP + 164        // KICKER - autoconfig

#define WM_USER_QUERYCLOSEFIND WM_APP + 170  // [0, quiet] - zepta se v threadu find okna, jestli ho muzeme zavrit + na dotaz zastavi pripadne probihajici hledani
#define WM_USER_COLORCHANGEFIND WM_APP + 171 // [0, 0] - informuje find okna o zmene barev

#define WM_USER_HELPHITTEST WM_APP + 172  // lResult = dwContext, lParam = MAKELONG(x,y)
#define WM_USER_EXITHELPMODE WM_APP + 173 // [0, 0]

#define WM_USER_POSTCMDORUNLOADPLUGIN WM_APP + 180 // [plug-in iface, 0, 1 nebo salCmd+2 nebo menuCmd+502] - nastavi ShouldUnload nebo ShouldRebuildMenu nebo prida salCmd/menuCmd do dat plug-inu
#define WM_USER_POSTMENUEXTCMD WM_APP + 181        // [plug-in iface, cmdID] - post menu-ext-cmd z plug-inu

#define WM_USER_SHOWPLUGINMSGBOX WM_APP + 185 // [0, 0] - otevreni msg-boxu o plug-inu nad Bug Report dialogem

// prikazy pro hlavni thread (v jinem threadu je nelze spoustet) - vyuziva Find dialog (bezi ve svem threadu)
#define WM_USER_VIEWFILE WM_APP + 190     // [COpenViewerData *, altView] - otevreni souboru v (alternate) vieweru
#define WM_USER_EDITFILE WM_APP + 191     // [name, 0] - otevreni souboru v editoru
#define WM_USER_VIEWFILEWITH WM_APP + 192 // [COpenViewerData *, handlerID] - otevreni souboru ve vybranem vieweru
#define WM_USER_EDITFILEWITH WM_APP + 193 // [name, handlerID] - otevreni souboru ve vybranem editoru

#define WM_USER_DISPACHCHANGENOTIF WM_APP + 194 // [0, time] - zadost o rozeslani zprav o zmenach na cestach

#define WM_USER_DISPACHCFGCHANGE WM_APP + 195 // [0, 0] - zadost o rozeslani zprav o zmenach v konfiguraci mezi plug-iny

#define WM_USER_CFGCHANGED WM_APP + 196 // [0, 0] - zasilano internim viewerum a find oknum po zmene konfigurace

#define WM_USER_CLEARHISTORY WM_APP + 197 // [0, 0] - informuje okno, ze ma promazat vsechny comboboxy obsahujici historie

#define WM_USER_REFRESHTOOLTIP WM_APP + 198 // posti se do tooltip okna: dojde k novemu vytazeni textu, resize okna a prekresleni
#define WM_USER_HIDETOOLTIP WM_APP + 199    // zprava pro prekroceni hranic threadu; zhasne tooltip

////////////////////////////////////////////////////////
//                                                    //
// Prostor WM_APP + 200 az WM_APP + 399 je urcen      //
// pro zpravy chodici take do oken pluginu.           //
// Definice je v plugins\shared\spl_*.h               //
//                                                    //
////////////////////////////////////////////////////////

#define WM_USER_ENUMFILENAMES WM_APP + 400 // [requestUID, 0] - informuje zdroj (panely a Findy), ze maji resit pozadavek na enumeraci souboru pro viewer

#define WM_USER_SM_END_NOTIFY_DELAYED WM_APP + 401  // [0, 0] notifikace o ukonceni suspend modu (zpozdena o 200ms, aby nedochazelo ke kolizi s WM_QUERYENDSESSION pri Shutdown / Log Off)
#define WM_USER_REFRESH_DIR_EX_DELAYED WM_APP + 402 // [FALSE, time] - lisi se od WM_USER_REFRESH_DIR tim, ze jde pravdepodobne o zbytecny refresh (duvod: aktivace okna, zadost o lock-volume (dela se obdoba "hands-off"), atp.) (zpozdena o 200ms nebo 5s, aby nedochazelo ke kolizi s WM_QUERYENDSESSION pri Shutdown / Log Off nebo aby si proces zadajici o lock-volume stihl volume zamknout)

#define WM_USER_CLOSE_MAINWND WM_APP + 403 // [0, 0] - pouziva se misto WM_CLOSE pro zavreni hlavniho okna Salamandera (vyhoda: je mozne zjistit, jestli se nedistribuuje z jine nez hlavni message-loopy)

#define WM_USER_HELP_MOUSEMOVE WM_APP + 405  // [0, mousePos] - zasilano behem Shift+F1 (ctx help) mode vsem child oknum; po obdrzeni jedne nebo vice WM_USER_MOUSEMOVE prijde WM_USER_MOUSELEAVE; slouzi k trackovani kurzoru mysi bez, aniz bychom nahazaovali capture
#define WM_USER_HELP_MOUSELEAVE WM_APP + 406 // [0, 0] - prichazi po WM_USER_MOUSEMOVE, kdyz kurzor opousti childa

//#define WM_USER_RENAME_NEXT_ITEM       WM_APP + 407 // [next, 0] - postne se po zmacknuti (Shift)Tab v inplace QuickRename pro prechod na (predchozi) dalsi polozku; inspired by Vista; 'next' je TRUE pro dalsi a FALSE pro predchozi

#define WM_USER_PROGRDLG_UPDATEICON WM_APP + 408 // [0, 0] - CProgressDlgArray: po doruceni teto zpravy do CProgressDialog dojde k novemu nastaveni ikonky dialogu

#define WM_USER_FORCECLOSE_MAINWND WM_APP + 409 // [0, 0] - vynucene zavreni hlavniho okna Salamandera

#define WM_USER_INACTREFRESH_DIR WM_APP + 410 // [0, time] - zpozdeny refresh pri neaktivnim hl. okne Salamandera

#define WM_USER_WAKEUP_FROM_IDLE WM_APP + 411 // [0, 0] - pokud je hlavni thread v IDLE, probere se

#define WM_USER_FINDFULLROWSEL WM_APP + 412 // [0, 0] - find okna maji nastavit sve list view, aby odpovidalo promenne Configuration.FindFullRowSelect

#define WM_USER_SLGINCOMPLETE WM_APP + 414 // [0, 0] - upozorneni, ze SLG neni kompletne prelozene, motivacni text aby se zapojili

#define WM_USER_USERMENUICONS_READY WM_APP + 415 // [bkgndReaderData, threadID] - notifikace pro hl. okno, ze se dokoncilo cteni ikon pro User Menu v threadu s ID 'threadID'

// states for Shift+F1 help mode
#define HELP_INACTIVE 0 // not in Shift+F1 help mode (must be 0)
#define HELP_ACTIVE 1   // in Shift+F1 help mode (non-zero)
#define HELP_ENTERING 2 // entering Shift+F1 help mode (non-zero)

#define STACK_CALLS_BUF_SIZE 5000       // kazdy thread bude mit 5KB prostor pro textovy call-stack
#define STACK_CALLS_MAX_MESSAGE_LEN 500 // nejdelsi zpravu uvazujeme 500 znaku

#define MENU_MARK_CX 9 // rozmery check mark pro menu
#define MENU_MARK_CY 9

#define BOTTOMBAR_CX 17 // rozmer tlacitka v souboru bottomtb.bmp (v bodech)
#define BOTTOMBAR_CY 13

// barvy v CurrentColors[x]
#define FOCUS_ACTIVE_NORMAL 0 // barvy pera pro ramecek kolem polozky
#define FOCUS_ACTIVE_SELECTED 1
#define FOCUS_FG_INACTIVE_NORMAL 2
#define FOCUS_FG_INACTIVE_SELECTED 3
#define FOCUS_BK_INACTIVE_NORMAL 4
#define FOCUS_BK_INACTIVE_SELECTED 5

#define ITEM_FG_NORMAL 6 // barvy textu polozek v panelu
#define ITEM_FG_SELECTED 7
#define ITEM_FG_FOCUSED 8
#define ITEM_FG_FOCSEL 9
#define ITEM_FG_HIGHLIGHT 10

#define ITEM_BK_NORMAL 11 // barvy pozadi polozek v panelu
#define ITEM_BK_SELECTED 12
#define ITEM_BK_FOCUSED 13
#define ITEM_BK_FOCSEL 14
#define ITEM_BK_HIGHLIGHT 15

#define ICON_BLEND_SELECTED 16 // barvy pro blend ikonek
#define ICON_BLEND_FOCUSED 17
#define ICON_BLEND_FOCSEL 18

#define PROGRESS_FG_NORMAL 19 // barvy progress bary
#define PROGRESS_FG_SELECTED 20
#define PROGRESS_BK_NORMAL 21
#define PROGRESS_BK_SELECTED 22

#define HOT_PANEL 23    // barva hot polozky v panelu
#define HOT_ACTIVE 24   //                   v aktivnim titulku panelu
#define HOT_INACTIVE 25 //                   v neaktivni tiulku panelu, statusbar,...

#define ACTIVE_CAPTION_FG 26   // barva textu v aktivnim titulku panelu
#define ACTIVE_CAPTION_BK 27   // barva pozadi v aktivnim titulku panelu
#define INACTIVE_CAPTION_FG 28 // barva textu v neaktivnim titulku panelu
#define INACTIVE_CAPTION_BK 29 // barva pozadi v neaktivnim titulku panelu

#define THUMBNAIL_FRAME_NORMAL 30 // barvy pera pro ramecek kolem thumbnails
#define THUMBNAIL_FRAME_FOCUSED 31
#define THUMBNAIL_FRAME_SELECTED 32
#define THUMBNAIL_FRAME_FOCSEL 33

#define VIEWER_FG_NORMAL 0 // normalni barvy viewru
#define VIEWER_BK_NORMAL 1
#define VIEWER_FG_SELECTED 2 // selected text
#define VIEWER_BK_SELECTED 3

#define NUMBER_OF_COLORS 34       // pocet barev ve schematu
#define NUMBER_OF_VIEWERCOLORS 4  // pocet barev pro viewer
#define NUMBER_OF_CUSTOMCOLORS 16 // uzivatelelm definovane barvy v barevnem dialogu

// interni drzak barvy, ktery navic obsahuje flag
typedef DWORD SALCOLOR;

// SALCOLOR flags
#define SCF_DEFAULT 0x01 // barevna slozka se ignoruje a pouzije se default hodnota

#define GetCOLORREF(rgbf) ((COLORREF)rgbf & 0x00ffffff)
#define RGBF(r, g, b, f) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16) | (((DWORD)(BYTE)(f)) << 24)))
#define GetFValue(rgbf) ((BYTE)((rgbf) >> 24))

inline void SetRGBPart(SALCOLOR* salColor, COLORREF rgb)
{
    *salColor = rgb & 0x00ffffff | (((DWORD)(BYTE)((BYTE)((*salColor) >> 24))) << 24);
}

extern SALCOLOR* CurrentColors;               // aktualni barvy
extern SALCOLOR UserColors[NUMBER_OF_COLORS]; // zmenene barvy

extern SALCOLOR SalamanderColors[NUMBER_OF_COLORS]; // standardni barvy
extern SALCOLOR ExplorerColors[NUMBER_OF_COLORS];   // standardni barvy
extern SALCOLOR NortonColors[NUMBER_OF_COLORS];     // standardni barvy
extern SALCOLOR NavigatorColors[NUMBER_OF_COLORS];  // standardni barvy

extern SALCOLOR ViewerColors[NUMBER_OF_VIEWERCOLORS]; // barvy vieweru

extern COLORREF CustomColors[NUMBER_OF_CUSTOMCOLORS]; // pro standardni color dialog

#define CARET_WIDTH 2
#define MIN_PANELWIDTH 5 // uzsi panel nedostava focus

#define REFRESH_PAUSE 200 // pauza mezi dvema nejblizsimi refreshi

extern int SPACE_WIDTH; // mezera mezi sloupcema v detailed view

#define MENU_CHECK_WIDTH 8 // rozmery check bitmap pro menu
#define MENU_CHECK_HEIGHT 8

// pocty pamatovanych stringu
#define SELECT_HISTORY_SIZE 20    // select / unselect
#define COPY_HISTORY_SIZE 20      // copy / move
#define EDIT_HISTORY_SIZE 30      // command line
#define CHANGEDIR_HISTORY_SIZE 20 // Shift+F7
#define PATH_HISTORY_SIZE 30      // forward/backward historie cest + historie navstivenych cest (Alt+F12)
#define FILTER_HISTORY_SIZE 15    // filter
#define FILELIST_HISTORY_SIZE 15
#define CREATEDIR_HISTORY_SIZE 20   // create directory
#define QUICKRENAME_HISTORY_SIZE 20 // quick rename
#define EDITNEW_HISTORY_SIZE 20     // edit new
#define CONVERT_HISTORY_SIZE 15     // convert

#define VK_LBRACKET 219
#define VK_BACKSLASH 220
#define VK_RBRACKET 221

// kdy testovat pokus o preruseni stavby scriptu
#define BS_TIMEOUT 200 // milisekund od posledniho testu

// identifikace bandu v rebaru
#define BANDID_MENU 1
#define BANDID_TOPTOOLBAR 2
#define BANDID_UMTOOLBAR 3
#define BANDID_DRIVEBAR 4
#define BANDID_DRIVEBAR2 5
#define BANDID_WORKER 6
#define BANDID_HPTOOLBAR 7
#define BANDID_PLUGINSBAR 8

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// urcuji duvod, proc jsou v panelu neobsazeny nektere soubory/adresare
#define HIDDEN_REASON_ATTRIBUTE 0x00000001 // maji atribut hidden nebo system a v konfiguraci je potlaceni zobrazovani takovych souboru/adresaru
#define HIDDEN_REASON_FILTER 0x00000002    // soubor je odfiltrovan na zaklade filtru v panelu
#define HIDDEN_REASON_HIDECMD 0x00000004   // jmeno bylo skryto prikazem Hide Selected/Unselected Names

// bitove pole drivu 'a' .. 'z'
#define DRIVES_MASK 0x03FFFFFF

//
// ****************************************************************************

// Windows XP, Windows 2003.NET, Vista, Windows 7, Windows 8, Windows 8.1, Windows 10
extern BOOL WindowsXP64AndLater;  // Windows XP64 nebo pozdejsi
extern BOOL WindowsVistaAndLater; // Windows Vista nebo pozdejsi
extern BOOL Windows7AndLater;     // Windows 7 nebo pozdejsi
extern BOOL Windows8AndLater;     // Windows 8 nebo pozdejsi
extern BOOL Windows8_1AndLater;   // Windows 8.1 nebo pozdejsi
extern BOOL Windows10AndLater;    // Windows 10 nebo pozdejsi

extern BOOL Windows64Bit; // x64 verze Windows

extern BOOL RunningAsAdmin; // TRUE pokud Salamander bezi "As Administrator"

extern DWORD CCVerMajor; // verze DLL common controls
extern DWORD CCVerMinor;

extern const char* SALAMANDER_TEXT_VERSION; // textove oznaceni aplikace vcetne verze

extern const char *LOW_MEMORY,
    *MAINWINDOW_NAME,
    *CMAINWINDOW_CLASSNAME,
    *CFILESBOX_CLASSNAME,
    *SAVEBITS_CLASSNAME,
    *SHELLEXECUTE_CLASSNAME;

extern const char* STR_NONE; // "(none)" - plug-iny: pro DLLName a Version pokud jsou nezjistitelne

extern char DefaultDir['z' - 'a' + 1][MAX_PATH]; // kam jit pri zmene disku

extern int MyTimeCounter;                   // po kazdem pouziti inkrementovat !
extern CRITICAL_SECTION TimeCounterSection; // pro synchronizaci pristupu k ^

extern HINSTANCE NtDLL;               // handle k ntdll.dll
extern HINSTANCE Shell32DLL;          // handle k shell32.dll (ikonky)
extern HINSTANCE ImageResDLL;         // handle k imageres.dll (ikonky - Vista+)
extern HINSTANCE User32DLL;           // handle k user32.dll (DisableProcessWindowsGhosting)
extern HINSTANCE HLanguage;           // handle k jazykove zavislym resourcum (cesta: Configuration.LoadedSLGName)
extern char CurrentHelpDir[MAX_PATH]; // po prvnim pouziti helpu je zde cesta do adresare helpu (umisteni vsech .chm souboru)
extern WORD LanguageID;               // language-id jazykove zavislych resourcu (.SLG souboru)

extern BOOL UseCustomPanelFont; // pokud je TRUE, vychazi Font a FontUL ze struktury LogFont; jinak ze systemoveho fontu (default)
extern HFONT Font;              // panel font
extern HFONT FontUL;            // podtrzena verze
extern int FontCharHeight;      // vyska fontu
extern LOGFONT LogFont;         // struktura popisujici panel font

BOOL CreatePanelFont(); // naplni Font, FontULa FontCharHeight na zaklade LogFont

extern HFONT EnvFont;         // font prostredi (edit, toolbar, header, status)
extern HFONT EnvFontUL;       // font listboxu podtrzeny
extern int EnvFontCharHeight; // vyska fontu
extern HFONT TooltipFont;     // font pro tooltips (a statusbars, ale tam ho nepouzivame)

BOOL GetSystemGUIFont(LOGFONT* lf); // vrati font pouzivany pro hlavni okno Salamandera
BOOL CreateEnvFonts();              // naplni EnvFont, EnvFontUL, EnvFontCharHeight, TooltipFont na zaklade metrics

extern DWORD MouseHoverTime; // po jake dobe ma dojit k vysviceni

extern HBRUSH HNormalBkBrush;        // pozadi obycejne polozky v panelu
extern HBRUSH HFocusedBkBrush;       // pozadi focusle polozky v panelu
extern HBRUSH HSelectedBkBrush;      // pozadi selected polozky v panelu
extern HBRUSH HFocSelBkBrush;        // pozadi focused & selected polozky v panelu
extern HBRUSH HDialogBrush;          // vypln pozadi dialogu
extern HBRUSH HButtonTextBrush;      // text tlacitek
extern HBRUSH HDitherBrush;          // sachovnice o barevne hloubce 1 bit; pri kresleni lze barvu nastavit pres SetTextColor/SetBkColor
extern HBRUSH HActiveCaptionBrush;   // pozadi aktivniho tiutlku panelu
extern HBRUSH HInactiveCaptionBrush; // pozadi neaktivniho tiutlku panelu

extern HBRUSH HMenuSelectedBkBrush;
extern HBRUSH HMenuSelectedTextBrush;
extern HBRUSH HMenuHilightBrush;
extern HBRUSH HMenuGrayTextBrush;

extern HACCEL AccelTable1; // akceleratory v panelech a cmdline
extern HACCEL AccelTable2; // akceleratory v cmdline

extern int SystemDPI;

enum CIconSizeEnum
{
    ICONSIZE_16,   // 16x16 @ 100%DPI, 20x20 @ 125%DPI, 24x24 @ 150%DPI, ...
    ICONSIZE_32,   // 32x32 @ 100%DPI, ...
    ICONSIZE_48,   // 48x48 @ 100%DPI, ...
    ICONSIZE_COUNT // items count
};

extern int IconSizes[ICONSIZE_COUNT]; // velikosti ikonek: 16, 32, 48
extern int IconLRFlags;               // ridi barevnou hloubku nacitanych ikon

// POZOR: na Viste se pouziva pro ikony 48x48 overlay ICONSIZE_32 a pro thumbnaily overlay ICONSIZE_48
extern HICON HSharedOverlays[ICONSIZE_COUNT];   // shared (ruka) ve vsech velikostech
extern HICON HShortcutOverlays[ICONSIZE_COUNT]; // shortcut (levy dolni roh) ve vsech velikostech
extern HICON HSlowFileOverlays[ICONSIZE_COUNT]; // slow files

extern CIconList* SimpleIconLists[ICONSIZE_COUNT]; // simple icons ve vsech velikostech

#define THROBBER_WIDTH 12 // rozmery jednoho policka
#define THROBBER_HEIGHT 12
#define THROBBER_COUNT 36     // celkovy pocet policek
#define IDT_THROBBER_DELAY 30 // delay [ms] v animaci jednoho policka
extern CIconList* ThrobberFrames;

#define LOCK_WIDTH 8 // rozmery jednoho policka
#define LOCK_HEIGHT 13
extern CIconList* LockFrames;

extern HICON HGroupIcon;   // skupina pro UserMenu popupy
extern HICON HFavoritIcon; // hot path

#define TILE_LEFT_MARGIN 4 // pocet bodu vlevo pred ikonkou

extern RGBQUAD ColorTable[256]; // paleta pouzivana pro vsechny toolbary (vcetne pluginu)

// jednotlive pozice imagelistu SymbolsImageList a LargeSymbolsImageList
enum CSymbolsImageListIndexes
{
    symbolsExecutable,    // 0: exe/bat/pif/com
    symbolsDirectory,     // 1: dir
    symbolsNonAssociated, // 2: neasociovany soubor
    symbolsAssociated,    // 3: asociovany soubor
    symbolsUpDir,         // 4: up-dir ".."
    symbolsArchive,       // 5: archiv
    symbolsCount          // TERMINATOR
};

extern HIMAGELIST HFindSymbolsImageList; // symboly pro find
extern HIMAGELIST HMenuMarkImageList;    // check marky pro menu
extern HIMAGELIST HGrayToolBarImageList; // toolbar a menu v sedivem provedeni (pocitano z barevneho)
extern HIMAGELIST HHotToolBarImageList;  // toolbar a menu v barevnem provedeni
extern HIMAGELIST HBottomTBImageList;    // bottom toolbar (F1 - F12)
extern HIMAGELIST HHotBottomTBImageList; // bottom toolbar (F1 - F12)

extern HPEN HActiveNormalPen; // pera pro ramecek kolem polozky
extern HPEN HActiveSelectedPen;
extern HPEN HInactiveNormalPen;
extern HPEN HInactiveSelectedPen;

extern HPEN HThumbnailNormalPen; // pera pro ramecek kolem thumbnail
extern HPEN HThumbnailFucsedPen;
extern HPEN HThumbnailSelectedPen;
extern HPEN HThumbnailFocSelPen;

extern HPEN BtnShadowPen;
extern HPEN BtnHilightPen;
extern HPEN Btn3DLightPen;
extern HPEN BtnFacePen;
extern HPEN WndFramePen;
extern HPEN WndPen;

extern HBITMAP HFilter; // bitmapa - panel skryva nejake soubory nebo adresare

extern HBITMAP HHeaderSort; // sipky pro HeaderLine

extern CBitmap ItemBitmap; // devecka pro vsechno mozne: kresleni polozek v panelu, header line, ...

extern HBITMAP HUpDownBitmap; // Sipky pro rolovani uvnitr kratkych popup menu.
extern HBITMAP HZoomBitmap;   // Zoom panelu

//extern HBITMAP HWorkerBitmap;

extern HCURSOR HHelpCursor; // Context Help cursor - loadi se az kdyz je potreba

#define THUMBNAIL_SIZE_DEFAULT 94 // podle XP
#define THUMBNAIL_SIZE_MIN 48     // pokud budeme chtit podpori mensi nez 48, je potreba zobrazovat mensi ikony
#define THUMBNAIL_SIZE_MAX 1000

extern BOOL DragFullWindows; // pokud je TRUE, mame menit velikost panelu realtime, jinak az po releasu (optimalizace pro remote desktop)

// CConfiguration::SizeFormat (format sloupce Size v panelech)
// !POZOR! nemenit konstanty, jsou vyvezene do pluginu pres SALCFG_SIZEFORMAT
#define SIZE_FORMAT_BYTES 0 // v bajtech (Open Salamander)
#define SIZE_FORMAT_KB 1    // v KB (Windows Explorer)
#define SIZE_FORMAT_MIXED 2 // bajty, KB, MB, GB, ...

// jmena klicu v registry
extern const char* SALAMANDER_ROOT_REG;
extern const char* SALAMANDER_SAVE_IN_PROGRESS;
extern const char* SALAMANDER_COPY_IS_OK;
extern const char* SALAMANDER_AUTO_IMPORT_CONFIG;
extern const char* SALAMANDER_CONFIG_REG;
extern const char* SALAMANDER_VERSION_REG;
extern const char* SALAMANDER_VERSIONREG_REG;
extern const char* CONFIG_ONLYONEINSTANCE_REG;
extern const char* CONFIG_LANGUAGE_REG;
extern const char* CONFIG_ALTLANGFORPLUGINS_REG;
extern const char* CONFIG_LANGUAGECHANGED_REG;
extern const char* CONFIG_USEALTLANGFORPLUGINS_REG;
extern const char* CONFIG_STATUSAREA_REG;
extern const char* CONFIG_SHOWSPLASHSCREEN_REG;
extern const char* CONFIG_ENABLECUSTICOVRLS_REG;
extern const char* CONFIG_DISABLEDCUSTICOVRLS_REG;
extern const char* VIEWERS_MASKS_REG;
extern const char* VIEWERS_COMMAND_REG;
extern const char* VIEWERS_ARGUMENTS_REG;
extern const char* VIEWERS_INITDIR_REG;
extern const char* VIEWERS_TYPE_REG;
extern const char* EDITORS_MASKS_REG;
extern const char* EDITORS_COMMAND_REG;
extern const char* EDITORS_ARGUMENTS_REG;
extern const char* EDITORS_INITDIR_REG;
extern const char* SALAMANDER_PLUGINSCONFIG;
extern const char* SALAMANDER_PLUGINS_NAME;
extern const char* SALAMANDER_PLUGINS_DLLNAME;
extern const char* SALAMANDER_PLUGINS_VERSION;
extern const char* SALAMANDER_PLUGINS_COPYRIGHT;
extern const char* SALAMANDER_PLUGINS_EXTENSIONS;
extern const char* SALAMANDER_PLUGINS_DESCRIPTION;
extern const char* SALAMANDER_PLUGINS_LASTSLGNAME;
extern const char* SALAMANDER_PLUGINS_HOMEPAGE;
//extern const char *SALAMANDER_PLUGINS_PLGICONS;
extern const char* SALAMANDER_PLUGINS_PLGICONLIST;
extern const char* SALAMANDER_PLUGINS_PLGICONINDEX;
extern const char* SALAMANDER_PLUGINS_PLGSUBMENUICONINDEX;
extern const char* SALAMANDER_PLUGINS_SUBMENUINPLUGINSBAR;
extern const char* SALAMANDER_PLUGINS_THUMBMASKS;
extern const char* SALAMANDER_PLUGINS_REGKEYNAME;
extern const char* SALAMANDER_PLUGINS_FSNAME;
extern const char* SALAMANDER_PLUGINS_FUNCTIONS;
extern const char* SALAMANDER_PLUGINS_LOADONSTART;
extern const char* SALAMANDER_PLUGINS_MENU;
extern const char* SALAMANDER_PLUGINS_MENUITEMNAME;
extern const char* SALAMANDER_PLUGINS_MENUITEMHOTKEY;
extern const char* SALAMANDER_PLUGINS_MENUITEMSTATE;
extern const char* SALAMANDER_PLUGINS_MENUITEMID;
extern const char* SALAMANDER_PLUGINS_MENUITEMSKILLLEVEL;
extern const char* SALAMANDER_PLUGINS_MENUITEMICONINDEX;
extern const char* SALAMANDER_PLUGINS_MENUITEMTYPE;
extern const char* SALAMANDER_PLUGINS_FSCMDNAME;
extern const char* SALAMANDER_PLUGINS_FSCMDICON;
extern const char* SALAMANDER_PLUGINS_FSCMDVISIBLE;
extern const char* SALAMANDER_PLUGINSORDER_SHOW;
extern const char* SALAMANDER_PLUGINS_ISNETHOOD;
extern const char* SALAMANDER_PLUGINS_USESPASSWDMAN;

// nasledujicich 8 retezcu je jen pro load konfigu verze 6 a nizsi, novejsi verze
// jiz pouzivaji SALAMANDER_PLUGINS_FUNCTIONS (ulozeni v bitech DWORDove masky funkci)
extern const char* SALAMANDER_PLUGINS_PANELVIEW;
extern const char* SALAMANDER_PLUGINS_PANELEDIT;
extern const char* SALAMANDER_PLUGINS_CUSTPACK;
extern const char* SALAMANDER_PLUGINS_CUSTUNPACK;
extern const char* SALAMANDER_PLUGINS_CONFIG;
extern const char* SALAMANDER_PLUGINS_LOADSAVE;
extern const char* SALAMANDER_PLUGINS_VIEWER;
extern const char* SALAMANDER_PLUGINS_FS;

// clipboard format pro SalIDataObject (znacka naseho IDataObjectu na clipboardu)
extern const char* SALCF_IDATAOBJECT;
// clipboard format pro CFakeDragDropDataObject (urcuje cestu, ktera se ma po dropu objevit
// v directory-line, command-line + blokuje drop do usermenu-toolbar); pokud je tazeno
// vic adresaru/souboru, je cesta prazdny retezec (drop do directory/command-line neni mozny)
extern const char* SALCF_FAKE_REALPATH;
// clipboard format pro CFakeDragDropDataObject (urcuje typ zdroje - 1=archiv, 2=FS)
extern const char* SALCF_FAKE_SRCTYPE;
// clipboard format pro CFakeDragDropDataObject (jen je-li zdroj FS: zdrojova FS cesta)
extern const char* SALCF_FAKE_SRCFSPATH;

// promenne pro CanChangeDirectory() a AllowChangeDirectory()
extern int ChangeDirectoryAllowed; // 0 znamena, ze je mozne menit adresar
extern BOOL ChangeDirectoryRequest;
// funkce obsluhujici automatickou zmenu aktualniho adresare na systemovy
// - kvuli odmapovavani z jinych softu a mazani adresaru zobrazenych Salamandrem
BOOL CanChangeDirectory();
void AllowChangeDirectory(BOOL allow);

// promenna pro BeginStopRefresh() a EndStopRefresh()
extern int StopRefresh;
// po volani se neprovede zadny refresh adresare
void BeginStopRefresh(BOOL debugSkipOneCaller = FALSE, BOOL debugDoNotTestCaller = FALSE);
// uvolni refreshovani -> pripadne posle WM_USER_SM_END_NOTIFY hlavnimu oknu (probehnou promeskane refreshe)
void EndStopRefresh(BOOL postRefresh = TRUE, BOOL debugSkipOneCaller = FALSE, BOOL debugDoNotTestCaller = FALSE);

// promenna kontrolovana v hl. message-loope v "idle" casti, je-li TRUE, vyhleda a unloadne
// plug-iny s ShouldUnload==TRUE, rebuildne menu pluginum s ShouldRebuildMenu==TRUE + spusti
// prikazy postnute z pluginu + pozadovane prikazy Salamandera
extern BOOL ExecCmdsOrUnloadMarkedPlugins;

// promenna kontrolovana v hl. message-loope v "idle" casti, je-li TRUE, otevre Pack/Unpack
// dialog pro pluginy s OpenPackDlg==TRUE nebo OpenUnpackDlg==TRUE
extern BOOL OpenPackOrUnpackDlgForMarkedPlugins;

// promenna pro BeginStopIconRepaint() a EndStopIconRepaint()
extern int StopIconRepaint;
extern BOOL PostAllIconsRepaint;
// po volani se neprovede zadny refresh ikony v panelech
void BeginStopIconRepaint();
// uvolni repaint -> pripadne posle WM_USER_REPAINTALLICONS hlavnimu oknu (obnova vsech ikon)
void EndStopIconRepaint(BOOL postRepaint = TRUE);

// promenna pro BeginStopStatusbarRepaint() a EndStopStatusbarRepaint()
extern int StopStatusbarRepaint;
extern BOOL PostStatusbarRepaint;
// po volani se prestane prekreslovat throbber
void BeginStopStatusbarRepaint();
// uvolni repaint
void EndStopStatusbarRepaint();

// v modulu msgbox.cpp - centrovany messagebox podle pra parenta od hParent
int SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
int SalMessageBoxEx(const MSGBOXEX_PARAMS* params);

// vykresli ikonky z imagelistu s nastavenyma stylama
#define IMAGE_STATE_FOCUSED 0x00000001
#define IMAGE_STATE_SELECTED 0x00000002
#define IMAGE_STATE_HIDDEN 0x00000004
#define IMAGE_STATE_SHARED 0x00000100
#define IMAGE_STATE_SHORTCUT 0x00000200
#define IMAGE_STATE_MASK 0x00000400
#define IMAGE_STATE_OFFLINE 0x00000800
BOOL StateImageList_Draw(CIconList* iconList, int imageIndex, HDC hDC, int xDst, int yDst,
                         DWORD state, CIconSizeEnum iconSize, DWORD iconOverlayIndex,
                         const RECT* overlayRect, BOOL overlayOnly, BOOL iconOverlayFromPlugin,
                         int pluginIconOverlaysCount, HICON* pluginIconOverlays);
DWORD GetImageListColorFlags(); // vrati ILC_COLOR??? podle verzi Windows - odladene pro pouziti imagelistu v listviewech

// API GetOpenFileName/GetSaveFileName v pripade ze cesta k souboru (OPENFILENAME::lpstrFile)
// neexistuje (nebo obsahuje napriklad C:\) vrati FALSE a CommDlgExtendedError() == FNERR_INVALIDFILENAME.
// Proto zavadime jejich "bezpecnou" variantu, ktera tento pripad detekuje a pokusi se otevrit
// dialog pro Documents nebo Desktop.
BOOL SafeGetOpenFileName(LPOPENFILENAME lpofn);
BOOL SafeGetSaveFileName(LPOPENFILENAME lpofn);

extern char DecimalSeparator[5]; // "znaky" (max. 4 znaky) vytazene ze systemu
extern int DecimalSeparatorLen;  // delka ve znacich bez nuly na konci
extern char ThousandsSeparator[5];
extern int ThousandsSeparatorLen;

extern DWORD SalamanderStartTime;     // cas startu Salamandera (GetTickCount)
extern DWORD SalamanderExceptionTime; // cas exceptiony v Salamanderu (GetTickCount) nebo cas posledniho vyvolani Bug Report dialogu

extern BOOL SkipOneActivateRefresh; // ma se preskocit refresh pri aktivaci hl. okna? (pro interni viewry)

extern int MenuNewExceptionHasOccured; // spadlo uz menu New? (mozna prepsalo nekde pamet)
extern int FGIExceptionHasOccured;     // spadlo uz SHGetFileInfo?
extern int ICExceptionHasOccured;      // spadlo uz InvokeCommand?
extern int QCMExceptionHasOccured;     // spadlo uz QueryContextMenu?
extern int OCUExceptionHasOccured;     // spadlo uz OleUninitialize nebo CoUninitialize?
extern int GTDExceptionHasOccured;     // spadlo uz GetTargetDirectory?
extern int SHLExceptionHasOccured;     // spadlo uz neco z ShellLib?
extern int RelExceptionHasOccured;     // spadlo uz nejaky volani IUnknown metody Release()?

extern BOOL SalamanderBusy;          // je Salamander busy?
extern DWORD LastSalamanderIdleTime; // GetTickCount() z okamziku, kdy SalamanderBusy naposledy presel na TRUE

extern int PasteLinkIsRunning; // pokud je vetsi nez nula, probiha prave Past Shortcuts prikaz v jednom z panelu

extern BOOL CannotCloseSalMainWnd; // TRUE = nesmi dojit k zavreni hlavniho okna

extern const char* DirColumnStr;      // LoadStr(IDS_DIRCOLUMN) - pouziva se prilis casto, cachujeme
extern int DirColumnStrLen;           // delka retezce
extern const char* ColExtStr;         // LoadStr(IDS_COLUMN_NAME_EXT) - pouziva se prilis casto, cachujeme
extern int ColExtStrLen;              // delka retezce
extern int TextEllipsisWidth;         // sirka retezce "..." zobrazenho fontem 'Font'
extern int TextEllipsisWidthEnv;      // sirka retezce "..." zobrazenho fontem 'FontEnv'
extern const char* ProgDlgHoursStr;   // LoadStr(IDS_PROGDLGHOURS) - pouziva se prilis casto, cachujeme
extern const char* ProgDlgMinutesStr; // LoadStr(IDS_PROGDLGMINUTES) - pouziva se prilis casto, cachujeme
extern const char* ProgDlgSecsStr;    // LoadStr(IDS_PROGDLGSECS) - pouziva se prilis casto, cachujeme

extern char FolderTypeName[80];         // file-type pro vsechny adresare (ziskany ze systemoveho adresare)
extern int FolderTypeNameLen;           // delka retezce FolderTypeName
extern const char* UpDirTypeName;       // LoadStr(IDS_UPDIRTYPENAME) - pouziva se prilis casto, cachujeme
extern int UpDirTypeNameLen;            // delka retezce
extern const char* CommonFileTypeName;  // LoadStr(IDS_COMMONFILETYPE) - pouziva se prilis casto, cachujeme
extern int CommonFileTypeNameLen;       // delka retezce CommonFileTypeName
extern const char* CommonFileTypeName2; // LoadStr(IDS_COMMONFILETYPE2) - pouziva se prilis casto, cachujeme

extern char WindowsDirectory[MAX_PATH]; // cachovany vysledek GetWindowsDirectory

//#ifdef MSVC_RUNTIME_CHECKS
#define RTC_ERROR_DESCRIPTION_SIZE 2000 // buffer pro popis run-time check chyby
extern char RTCErrorDescription[RTC_ERROR_DESCRIPTION_SIZE];
//#endif // MSVC_RUNTIME_CHECKS

// cesta, kde vytvorime bug report a minidump, umisteni: do Visty u salamand.exe, ve Viste (a dale) v CSIDL_APPDATA + "\\Open Salamander"
extern char BugReportPath[MAX_PATH];

// nazev souboru, ktery bude importovan (pokud existuje) do registry
extern char ConfigurationName[MAX_PATH];
extern BOOL ConfigurationNameIgnoreIfNotExists;

extern HWND PluginProgressDialog; // pokud si plug-in otevre progress dialog, je zde jeho HWND, jinak NULL
extern HWND PluginMsgBoxParent;   // parent pro messageboxy plug-inu (hlavni okno, Plugins dialog, atd.)

extern BOOL CriticalShutdown; // TRUE = probiha "critical shutdown", neni cas se ptat, rychle koncime, 5s do zabiti

// "preklad" POSIX jmena na MS
#define itoa _itoa
#define stricmp _stricmp
#define strnicmp _strnicmp

// skill levels
#define SKILL_LEVEL_BEGINNER 0
#define SKILL_LEVEL_INTERMEDIATE 1
#define SKILL_LEVEL_ADVANCED 2

// konvertuje promennou CConfiguration::SkillLevel do SkillLevel pro menu.
DWORD CfgSkillLevelToMenu(BYTE cfgSkillLevel);

// atributy, ktere ukazujeme v panelu a kteryma je treba maskovat napriklad pri porovnani adresaru
// POZOR: FILE_ATTRIBUTE_DIRECTORY nezobrazujeme jako atribut, takze v masce nema co delat
#define DISPLAYED_ATTRIBUTES (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | \
                              FILE_ATTRIBUTE_SYSTEM | \
                              FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_ENCRYPTED | \
                              FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_COMPRESSED | \
                              FILE_ATTRIBUTE_OFFLINE)

// timers
#define IDT_SCROLL 930
#define IDT_REPAINT 931
#define IDT_DRAGDROPTESTAGAIN 932
#define IDT_PANELSCROLL 933
#define IDT_SINGLECLICKSELECT 934
#define IDT_FLASHICON 935
#define IDT_QUICKRENAMEBEGIN 936
#define IDT_PLUGINFSTIMERS 937
#define IDT_EDITLB 938
#define IDT_PROGRESSSELFMOVE 939
#define IDT_DELETEMNGR_PROCESS 940
#define IDT_ADDNEWMODULES 941
#define IDT_POSTENDSUSPMODE 942
#define IDT_ASSOCIATIONSCHNG 943
#define IDT_SM_END_NOTIFY 944
#define IDT_REFRESH_DIR_EX 945
#define IDT_UPDATESTATUS 946
#define IDT_ICONOVRREFRESH 947
#define IDT_INACTIVEREFRESH 948
#define IDT_THROBBER 949
#define IDT_DELAYEDTHROBBER 950
#define IDT_UPDATETASKLIST 951

// POZOR: skoro vsechny funkce v teto sekci pri chybe zobrazuji hlaseni o LOAD / SAVE
//        konfigurace, coz z nich dela nevhodne pro bezny pristup do Registry,
//        reseni viz funkce na zacatku regwork.h: OpenKeyAux, CreateKeyAux, atd.
BOOL ClearKey(HKEY key);
BOOL CreateKey(HKEY hKey, const char* name, HKEY& createdKey);
BOOL OpenKey(HKEY hKey, const char* name, HKEY& openedKey);
void CloseKey(HKEY key);
BOOL DeleteKey(HKEY hKey, const char* name);
BOOL DeleteValue(HKEY hKey, const char* name);
// pro dataSize = -1 si funkce napocita delku stringu pres strlen
BOOL SetValue(HKEY hKey, const char* name, DWORD type,
              const void* data, DWORD dataSize);
BOOL GetValue(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize);
BOOL GetSize(HKEY hKey, const char* name, DWORD type, DWORD& bufferSize);
BOOL LoadRGB(HKEY hKey, const char* name, COLORREF& color);
BOOL SaveRGB(HKEY hKey, const char* name, COLORREF color);
BOOL LoadRGBF(HKEY hKey, const char* name, SALCOLOR& color);
BOOL SaveRGBF(HKEY hKey, const char* name, SALCOLOR color);
BOOL LoadLogFont(HKEY hKey, const char* name, LOGFONT* logFont);
BOOL SaveLogFont(HKEY hKey, const char* name, LOGFONT* logFont);
BOOL LoadHistory(HKEY hKey, const char* name, char* history[], int maxCount);
BOOL SaveHistory(HKEY hKey, const char* name, char* history[], int maxCount, BOOL onlyClear = FALSE);
BOOL LoadViewers(HKEY hKey, const char* name, CViewerMasks* viewerMasks);
BOOL SaveViewers(HKEY hKey, const char* name, CViewerMasks* viewerMasks);
BOOL LoadEditors(HKEY hKey, const char* name, CEditorMasks* editorMasks);
BOOL SaveEditors(HKEY hKey, const char* name, CEditorMasks* editorMasks);

BOOL ExportConfiguration(HWND hParent, const char* fileName, BOOL clearKeyBeforeImport);
BOOL ImportConfiguration(HWND hParent, const char* fileName, BOOL ignoreIfNotExists,
                         BOOL autoImportConfig, BOOL* importCfgFromFileWasSkipped);

class CHighlightMasks;
void UpdateDefaultColors(SALCOLOR* colors, CHighlightMasks* highlightMasks, BOOL processColors, BOOL processMasks);

extern BOOL ImageDragging;                                                // prave se tahne image
extern BOOL ImageDraggingVisible;                                         // je prave image zobrazeny?
void ImageDragBegin(int width, int height, int dxHotspot, int dyHotspot); // velikost tazeneho obrazku
void ImageDragEnd();                                                      // ukonceni tazeni
BOOL ImageDragInterfereRect(const RECT* rect);                            // rect je v obrazovkovych souradnicich, zjistit, jestli tazena polozka zasahuje do rect
void ImageDragEnter(int x, int y);                                        // x a y jsou obrazovkove souradnice
void ImageDragMove(int x, int y);                                         // x a y jsou obrazovkove souradnice
void ImageDragLeave();
void ImageDragShow(BOOL show); // zhasne / rozsviti, neovlivni ImageDragging, pouze ImageDraggingVisible

// nastavuje kurzor ve tvaru ruky
HCURSOR SetHandCursor();

//******************************************************************************
//
// CreateToolbarBitmaps
//
// IN:   hInstance    - instance, ve ktere lezi bitmapa s resID
//       resID        - identifikator vstupni bitmapy
//       transparent  - tato barva bude pruhledna
//       bkColorForAlpha - barva, ktera bude prosvitat pod alpha castma ikonek (WinXP)
// OUT:  hMaskBitmap  - maska (b&w)
//       hGrayBitmap  - sedive provedeni
//       hColorBitmap - barevne provedeni
//

struct CSVGIcon
{
    int ImageIndex;
    const char* SVGName;
};

BOOL CreateToolbarBitmaps(HINSTANCE hInstance, int resID, COLORREF transparent, COLORREF bkColorForAlpha,
                          HBITMAP& hMaskBitmap, HBITMAP& hGrayBitmap, HBITMAP& hColorBitmap, BOOL appendIcons,
                          const CSVGIcon* svgIcons, int svgIconsCount);

//****************************************************************************
//
// CreateGrayscaleAndMaskBitmaps
//
// Vytvori novou bitmapu o hloubce 24 bitu, nakopiruje do ni zdrojovou
// bitmapu a prevede ji na stupne sedi. Zaroven pripravi druhou bitmapu
// s maskou dle transparentni barvy.
//

BOOL CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                   HBITMAP& hGrayscale, HBITMAP& hMask);

//******************************************************************************
//
// UpdateCrc32
//   Updates CRC-32 (32-bit Cyclic Redundancy Check) with specified array of bytes.
//
// Parameters
//   'buffer'
//      [in] Pointer to the starting address of the block of memory to update 'crcVal' with.
//
//   'count'
//      [in] Size, in bytes, of the block of memory to update 'crcVal' with.
//
//   'crcVal'
//      [in] Initial crc value. Set this value to zero to calculate CRC-32 of the 'buffer'.
//
// Return Values
//   Returns updated CRC-32 value.
//

DWORD UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal);

//******************************************************************************
//
// Rizeni Idle procesingu (CMainWindow::OnEnterIdle)
//
// promenne jsou pro snadnou dostupnost globalni
// a ne jako atributy tridy CMainWindow
//

extern BOOL IdleRefreshStates;  // pokud je nastavena, budou pri nejblizim CMainWindow::OnEnterIdle vytazeny stavove promenne pro commandy (toolbar, menu)
extern BOOL IdleForceRefresh;   // je-li nastavena IdleRefreshStates, nastaveni promenne IdleForceRefresh vyradi cache na urovni Salamandera
extern BOOL IdleCheckClipboard; // pokud je IdleRefreshStates==TRUE a zaroven je nastavena tato promenna, bude se kontrolovat tak clipboard (casove narocne)

// ".." neni pocitano mezi soubory|adresare
extern DWORD EnablerUpDir;                // existuje parent directory?
extern DWORD EnablerRootDir;              // nejsme jeste v rootu? (pozor: UNC root ma updir, ale je to root)
extern DWORD EnablerForward;              // je v historii dostupny forward?
extern DWORD EnablerBackward;             // je v historii dostupny backward?
extern DWORD EnablerFileOnDisk;           // focus je na souboru && panel je disk
extern DWORD EnablerFileOnDiskOrArchive;  // focus je na souboru && panel je disk nebo archiv
extern DWORD EnablerFileOrDirLinkOnDisk;  // focus je na souboru nebo adresari linku && panel je disk
extern DWORD EnablerFiles;                // focus|select je na souborech|adresarich
extern DWORD EnablerFilesOnDisk;          // focus|select je na souborech|adresarich && panel je disk
extern DWORD EnablerFilesOnDiskCompress;  // focus|select je na souborech|adresarich && panel je disk && je podporovana komprese
extern DWORD EnablerFilesOnDiskEncrypt;   // focus|select je na souborech|adresarich && panel je disk && je podporovano sifrovani
extern DWORD EnablerFilesOnDiskOrArchive; // focus|select je na souborech|adresarich && panel je disk nebo archiv
extern DWORD EnablerOccupiedSpace;        // panel je disk nebo archiv s VALID_DATA_SIZE a zaroven plati EnablerFilesOnDiskOrArchive
extern DWORD EnablerFilesCopy;            // focus|select je na souborech|adresarich && panel je disk, archiv nebo FS s podporou "copy from fs"
extern DWORD EnablerFilesMove;            // focus|select je na souborech|adresarich && panel je disk nebo FS s podporou "move from fs"
extern DWORD EnablerFilesDelete;          // focus|select je na souborech|adresarich && (panel je disk, editovatelny archiv nebo FS s podporou "delete")
extern DWORD EnablerFileDir;              // focus je na souboru|adresari
extern DWORD EnablerFileDirANDSelected;   // focus je na souboru|adresari && jsou oznaceny nejake soubory|adresare
extern DWORD EnablerQuickRename;          // focus je na souboru|adresari && panel je disk nebo FS (s podporou quick-rename)
extern DWORD EnablerOnDisk;               // panel je disk
extern DWORD EnablerCalcDirSizes;         // panel je disk nebo archiv s VALID_DATA_SIZE
extern DWORD EnablerPasteFiles;           // je mozne provest Paste? (soubory na clipboardu) (vyuzite jako pamet posledniho stavu clipboardu pro 'pasteFiles' v CMainWindow::RefreshCommandStates())
extern DWORD EnablerPastePath;            // je mozne provest Paste? (text cesty na clipboardu) (vyuzite jako pamet posledniho stavu clipboardu pro 'pastePath' v CMainWindow::RefreshCommandStates())
extern DWORD EnablerPasteLinks;           // je mozne provest Paste Links? (soubory pres "copy" na clipboardu) (vyuzite jako pamet posledniho stavu clipboardu pro 'pasteLinks' v CMainWindow::RefreshCommandStates())
extern DWORD EnablerPasteSimpleFiles;     // jsou na clipboardu soubory/adresare z jedine cesty? (aneb: je sance na Paste do archivu nebo FS?)
extern DWORD EnablerPasteDefEffect;       // jaky je defaultni paste-effect, muze byt i kombinace DROPEFFECT_COPY+DROPEFFECT_MOVE (aneb: slo o Copy nebo Cut?)
extern DWORD EnablerPasteFilesToArcOrFS;  // je mozny Paste souboru do archivu/FS v aktualnim panelu? (v panelu je archiv/FS && EnablerPasteSimpleFiles && operace podle EnablerPasteDefEffect je mozna)
extern DWORD EnablerPaste;                // je mozne provest Paste? (soubory na clipboardu && panel je disk || je mozne provest paste do archivu nebo FS || text cesty na clipboardu)
extern DWORD EnablerPasteLinksOnDisk;     // je mozne provest Paste Links a panel je disk?
extern DWORD EnablerSelected;             // jsou oznaceny nejake soubory|adresare
extern DWORD EnablerUnselected;           // existuje alespon jeden neoznaceny soubor|adresar (UpDir ".." se neuvazuje)
extern DWORD EnablerHiddenNames;          // pole HiddenNames obsahuje nejake nazvy
extern DWORD EnablerSelectionStored;      // je ulozena selection v OldSelection aktivniho panelu?
extern DWORD EnablerGlobalSelStored;      // je ulozena selection v GlobalSelection?
extern DWORD EnablerSelGotoPrev;          // existuje pred focusem vybrana polozka?
extern DWORD EnablerSelGotoNext;          // existuje za focusem vybrana polozka?
extern DWORD EnablerLeftUpDir;            // existuje v levem panelu parent directory?
extern DWORD EnablerRightUpDir;           // existuje v pravem panelu parent directory?
extern DWORD EnablerLeftRootDir;          // nejsme jeste v levem panelu v rootu? (pozor: UNC root ma updir, ale je to root)
extern DWORD EnablerRightRootDir;         // nejsme jeste v pravem panelu v rootu? (pozor: UNC root ma updir, ale je to root)
extern DWORD EnablerLeftForward;          // je v historii leveho panelu dostupny forward?
extern DWORD EnablerRightForward;         // je v historii praveho panelu dostupny forward?
extern DWORD EnablerLeftBackward;         // je v historii leveho panelu dostupny backward?
extern DWORD EnablerRightBackward;        // je v historii praveho panelu dostupny backward?
extern DWORD EnablerFileHistory;          // je v view/edit historii dostupny soubor?
extern DWORD EnablerDirHistory;           // je v directory historii dostupny adresar?
extern DWORD EnablerCustomizeLeftView;    // je mozne konfigurovat sloupce pro levy pohled?
extern DWORD EnablerCustomizeRightView;   // je mozne konfigurovat sloupce pro pravy pohled?
extern DWORD EnablerDriveInfo;            // je mozne zobrazit Drive Info?
extern DWORD EnablerCreateDir;            // panel je disk nebo FS (s podporou create-dir)
extern DWORD EnablerViewFile;             // focus je na souboru && panel je disk, archiv nebo FS (s podporou view-file)
extern DWORD EnablerChangeAttrs;          // focus|select je na souborech|adresarich && panel je disk nebo FS (s podporou change-attributes)
extern DWORD EnablerShowProperties;       // focus|select je na souborech|adresarich && panel je disk nebo FS (s podporou show-properties)
extern DWORD EnablerItemsContextMenu;     // focus|select je na souborech|adresarich && panel je disk nebo FS (s podporou context-menu)
extern DWORD EnablerOpenActiveFolder;     // panel je disk nebo FS (s podporou open-active-folder)
extern DWORD EnablerPermissions;          // focus|select je na souborech|adresarich && panel je disk, bezime nejmene na W2K, disk umi ACL (NTFS)

//******************************************************************************
//
// ToolBar Bitmap indexes
//
// Polozky lze pridavat do pole.
// Pole je rozdeleno na dve casti. V Prnim jsou indexy, ke kterym se v bitmape
// opravdu nachazeji obrazky.
// Pak nasleduji indexy, na ktere jsou vytazeny ikonky z shell32.dll.
// Tyto dve skupiny musi byt vzdy celistve a neni mozne indexy stridat.
//

#define IDX_TB_CONNECTNET 0    // Connect Network Drive
#define IDX_TB_DISCONNECTNET 1 // Disconnect Network Drive
#define IDX_TB_SHARED_DIRS 2   // Shared Directories
#define IDX_TB_CHANGE_DIR 3    // Change Directory
#define IDX_TB_CREATEDIR 4     // Create Directory
#define IDX_TB_NEW 5           // New
#define IDX_TB_FINDFILE 6      // Find Files
#define IDX_TB_PREV_SELECTED 7 // Previous Selected Item
#define IDX_TB_NEXT_SELECTED 8 // Next Selected Item
#define IDX_TB_SORTBYNAME 9    // Sort by Name
#define IDX_TB_SORTBYTYPE 10   // Sort by Type
#define IDX_TB_SORTBYSIZE 11   // Sort by Size
#define IDX_TB_SORTBYDATE 12   // Sort by Date
#define IDX_TB_PARENTDIR 13    // Parent Directory
#define IDX_TB_ROOTDIR 14      // Root Directory
#define IDX_TB_FILTER 15       // Filter
#define IDX_TB_BACK 16         // Back
#define IDX_TB_FORWARD 17      // Forward
#define IDX_TB_REFRESH 18      // Refresh
#define IDX_TB_SWAPPANELS 19   // Swap Panels
#define IDX_TB_CHANGEATTR 20   // Change Attributes
#define IDX_TB_USERMENU 21     // User Menu
#define IDX_TB_COMMANDSHELL 22 // Command Shell
#define IDX_TB_COPY 23         // Copy
#define IDX_TB_MOVE 24         // Move
#define IDX_TB_DELETE 25       // Delete
// 1x not used
#define IDX_TB_COMPRESS 27       // Compress
#define IDX_TB_UNCOMPRESS 28     // UnCompress
#define IDX_TB_QUICKRENAME 29    // Quick Rename
#define IDX_TB_CHANGECASE 30     // Change Case
#define IDX_TB_VIEW 31           // View
#define IDX_TB_CLIPBOARDCUT 32   // Clipboard Cut
#define IDX_TB_CLIPBOARDCOPY 33  // Clipboard Copy
#define IDX_TB_CLIPBOARDPASTE 34 // Clipboard Paste
#define IDX_TB_PERMISSIONS 35    // Permissions
#define IDX_TB_PROPERTIES 36     // Properties
#define IDX_TB_COMPAREDIR 37     // Comapare Directories
#define IDX_TB_DRIVEINFO 38      // Drive Information
#define IDX_TB_RESELECT 39       // Reselect
#define IDX_TB_HELP 40           // Help
#define IDX_TB_CONTEXTHELP 41    // Context Help
// 1x not used
#define IDX_TB_EDIT 43              // Edit
#define IDX_TB_SORTBYEXT 44         // Sort by Extension
#define IDX_TB_SELECT 45            // Select
#define IDX_TB_UNSELECT 46          // Unselect
#define IDX_TB_INVERTSEL 47         // Invert selection
#define IDX_TB_SELECTALL 48         // Select all
#define IDX_TB_PACK 49              // Pack
#define IDX_TB_UNPACK 50            // UnPack
#define IDX_TB_CONVERT 51           // Convert
#define IDX_TB_UNSELECTALL 52       // Unselect all
#define IDX_TB_VIEW_MODE 53         // View Mode
#define IDX_TB_HOTPATHS 54          // Hot Paths
#define IDX_TB_FOCUS 55             // Focus (zelena sipka)
#define IDX_TB_STOP 56              // Stop (cervene kolecko s krizkem)
#define IDX_TB_EMAIL 57             // Email Files
#define IDX_TB_EDITNEW 58           // Edit New
#define IDX_TB_PASTESHORTCUT 59     // Paste Shortcut
#define IDX_TB_FOCUSSHORTCUT 60     // Focus Shortcut or Link Target
#define IDX_TB_CALCDIRSIZES 61      // Calculate Directory Sizes
#define IDX_TB_OCCUPIEDSPACE 62     // Calculate Occupied Space
#define IDX_TB_SAVESELECTION 63     // Save Selection
#define IDX_TB_LOADSELECTION 64     // Load Selection
#define IDX_TB_SEL_BY_EXT 65        // Select Files With Same Extension
#define IDX_TB_UNSEL_BY_EXT 66      // Unselect Files With Same Extension
#define IDX_TB_SEL_BY_NAME 67       // Select Files With Same Name
#define IDX_TB_UNSEL_BY_NAME 68     // Unselect Files With Same Name
#define IDX_TB_OPEN_FOLDER 69       // Open Folder
#define IDX_TB_CONFIGURARTION 70    // Configuration
#define IDX_TB_OPEN_IN_OTHER_ACT 71 // Focus Name in Other Panel
#define IDX_TB_OPEN_IN_OTHER 72     // Open Name in Other Panel
#define IDX_TB_AS_OTHER_PANEL 73    // Go To Path From Other Panel
#define IDX_TB_HIDE_UNSELECTED 74   // Hide Unselected Names
#define IDX_TB_HIDE_SELECTED 75     // Hide Selected Names
#define IDX_TB_SHOW_ALL 76          // Show All Names
#define IDX_TB_SMART_COLUMN_MODE 77 // Smart Column Mode

#define IDX_TB_FD 78 // first "dynamic added" index
// nasledujici ikony budou pridany k bitmape dynamicky
// a nektere budou nacteny z shell32.dll

#define IDX_TB_CHANGEDRIVEL IDX_TB_FD + 0 // Change Drive Left
#define IDX_TB_CHANGEDRIVER IDX_TB_FD + 1 // Change Drive Right
#define IDX_TB_OPENACTIVE IDX_TB_FD + 2   // Open Active Folder
#define IDX_TB_OPENDESKTOP IDX_TB_FD + 3  // Open Desktop
#define IDX_TB_OPENMYCOMP IDX_TB_FD + 4   // Open Computer
#define IDX_TB_OPENCONTROL IDX_TB_FD + 5  // Open Control Panel
#define IDX_TB_OPENPRINTERS IDX_TB_FD + 6 // Open Printers
#define IDX_TB_OPENNETWORK IDX_TB_FD + 7  // Open Network
#define IDX_TB_OPENRECYCLE IDX_TB_FD + 8  // Open Recycle Bin
#define IDX_TB_OPENFONTS IDX_TB_FD + 9    // Open Fonts
#define IDX_TB_OPENMYDOC IDX_TB_FD + 10   // Open Documents

#define IDX_TB_COUNT IDX_TB_FD + 11 // pocet bitmap vcetne tahanych z shell32.dll

//******************************************************************************
//
// Custom Exceptions
//

#define OPENSAL_EXCEPTION_RTC 0xE0EA4321   // vyvolame v rtc callbacku
#define OPENSAL_EXCEPTION_BREAK 0xE0EA4322 // vyvolame v pripade breaku (z jineho salama nebo promoci salbreak)

//******************************************************************************
//
// Sada promennych a funkci pro otevirani asociaci pomoci SalOpen.exe
//

// sdilena pamet
extern HANDLE SalOpenFileMapping;
extern void* SalOpenSharedMem;

// uvolneni sluzby
void ReleaseSalOpen();

// spusteni salopen.exe a predani 'fileName' pres sdilenou pamet
// vraci TRUE pokud se to povedlo, jinak FALSE (asociace by se mela spustit jinak)
BOOL SalOpenExecute(HWND hWindow, const char* fileName);

//******************************************************************************

// mapovani salCmd (cislo prikazu Salamandera spousteneho z plug-inu, viz SALCMD_XXX)
// na cislo prikazu pro WM_COMMAND
int GetWMCommandFromSalCmd(int salCmd);

//******************************************************************************

// pocet polozek v poli SalamanderConfigurationRoots
#define SALCFG_ROOTS_COUNT 83

// id hlavniho threadu (platne az po vstupu do WinMain())
extern DWORD MainThreadID;

extern BOOL IsNotAlphaNorNum[256]; // pole TRUE/FALSE pro znaky (TRUE = neni pismeno ani cislice)
extern BOOL IsAlpha[256];          // pole TRUE/FALSE pro znaky (TRUE = pismeno)

extern int UserCharset; // defaultni useruv charset pro fonty

// granularita alokaci (potreba pro pouzivani souboru mapovanych do pameti)
extern DWORD AllocationGranularity;

// ma se cekat na pusteni ESC pred zacatkem listovani cesty v panelu?
extern BOOL WaitForESCReleaseBeforeTestingESC;

// vrati pozici v screen coord., kde se ma vybalit context menu
// pouziva se pri vybalovani contextoveho menu pomoci klavesnice (Shift+F10 nebo VK_APP)
void GetListViewContextMenuPos(HWND hListView, POINT* p);

// na zaklade barevne hloubky displeje urci, jestli pouzivat 256 barevne
// nebo 16 barevne bitmapy.
BOOL Use256ColorsBitmap();

// obnovi focus ve zdrojovem panelu (pouziva se pokud mizi focus - po disablovani hl. okna, atp.)
void RestoreFocusInSourcePanel();

#define ISSLGINCOMPLETE_SIZE 200
extern char IsSLGIncomplete[ISSLGINCOMPLETE_SIZE];

//******************************************************************************
// enumerace jmen souboru z panelu/Findu pro viewery

// init+release dat spojenych s enumeraci
void InitFileNamesEnumForViewers();
void ReleaseFileNamesEnumForViewers();

enum CFileNamesEnumRequestType
{
    fnertFindNext,     // hledame dalsi soubor ve zdroji
    fnertFindPrevious, // hledame predchozi soubor ve zdroji
    fnertIsSelected,   // zjistujeme oznaceni souboru ve zdroji
    fnertSetSelection, // nastavujeme oznaceni souboru ve zdroji
};

struct CFileNamesEnumData
{
    // pozadavek:
    int RequestUID;                        // cislo pozadavku
    CFileNamesEnumRequestType RequestType; // typ pozadavku
    int SrcUID;
    int LastFileIndex;
    char LastFileName[MAX_PATH];
    BOOL PreferSelected;
    BOOL OnlyAssociatedExtensions;
    CPluginInterfaceAbstract* Plugin; // pouziva se pri 'OnlyAssociatedExtensions'==TRUE, oznacuje pro jaky plugin filtrovat jmena souboru ('Plugin'==NULL = interni viewer)
    char FileName[MAX_PATH];
    BOOL Select;
    BOOL TimedOut; // TRUE pokud uz na vysledek nikdo neceka (zbytecne provadet hledani jmena)

    // vysledek:
    BOOL Found; // TRUE pokud bylo nalezeno pozadovane jmeno souboru
    BOOL NoMoreFiles;
    BOOL SrcBusy;
    BOOL IsFileSelected;
};

// sekce pro praci s daty spojenymi s enumeraci (FileNamesEnumSources, FileNamesEnumData,
// FileNamesEnumDone, NextRequestUID a NextSourceUID)
extern CRITICAL_SECTION FileNamesEnumDataSect;
// struktura s pozadavkem+vysledky enumerace
extern CFileNamesEnumData FileNamesEnumData;
// event je "signaled" jakmile zdroj naplni vysledek do FileNamesEnumData
extern HANDLE FileNamesEnumDone;

#define FILENAMESENUM_TIMEOUT 1000 // timeout pro doruceni zpravy WM_USER_ENUMFILENAMES oknu zdroje

// vraci TRUE pokud je zdroj enumerace panel, v 'panel' pak vraci PANEL_LEFT nebo
// PANEL_RIGHT; pokud nebyl zdroj enumerace nalezen nebo jde o Find okno, vraci FALSE
BOOL IsFileEnumSourcePanel(int srcUID, int* panel);

// vraci dalsi jmeno souboru pro viewer ze zdroje (levy/pravy panel nebo Findy);
// 'srcUID' je unikatni identifikator zdroje (predava se jako parametr pri otevirani
// vieweru); 'lastFileIndex' (nesmi byt NULL) je IN/OUT parametr, ktery by plugin mel
// menit jen pokud chce vratit jmeno prvniho souboru, v tomto pripade nastavit 'lastFileIndex'
// na -1; pocatecni hodnota 'lastFileIndex' se predava jako parametr pri otevirani
// vieweru; 'lastFileName' je plne jmeno aktualniho souboru (prazdny retezec pokud neni
// zname, napr. je-li 'lastFileIndex' -1); je-li 'preferSelected' TRUE a aspon jedno
// jmeno oznacene, budou se vracet oznacena jmena; je-li 'onlyAssociatedExtensions'
// TRUE, vraci jen soubory s priponou asociovanou s viewerem tohoto pluginu (F3 na tomto
// souboru by se pokusilo otevrit viewer tohoto pluginu + ignoruje pripadne zastineni
// viewerem jineho pluginu); 'fileName' je buffer pro ziskane jmeno (velikost alespon
// MAX_PATH); vraci TRUE pokud se podari jmeno ziskat; vraci FALSE pri chybe: zadne
// dalsi jmeno souboru ve zdroji neni (neni-li 'noMoreFiles' NULL, vraci se v nem TRUE),
// zdroj je zaneprazdnen (nezpracovava zpravy; neni-li 'srcBusy' NULL, vraci se v nem
// TRUE), jinak zdroj prestal existovat (zmena cesty v panelu, zmena razeni, atp.)
BOOL GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                              BOOL preferSelected, BOOL onlyAssociatedExtensions,
                              char* fileName, BOOL* noMoreFiles, BOOL* srcBusy,
                              CPluginInterfaceAbstract* plugin);

// vraci predchozi jmeno souboru pro viewer ze zdroje (levy/pravy panel nebo Findy);
// 'srcUID' je unikatni identifikator zdroje (predava se jako parametr pri otevirani
// vieweru); 'lastFileIndex' (nesmi byt NULL) je IN/OUT parametr, ktery by plugin mel
// menit jen pokud chce vratit jmeno posledniho souboru, v tomto pripade nastavit 'lastFileIndex'
// na -1; pocatecni hodnota 'lastFileIndex' se predava jako parametr pri otevirani
// vieweru; 'lastFileName' je plne jmeno aktualniho souboru (prazdny retezec pokud neni
// zname, napr. je-li 'lastFileIndex' -1); je-li 'preferSelected' TRUE a aspon jedno
// jmeno oznacene, budou se vracet oznacena jmena; je-li 'onlyAssociatedExtensions' TRUE,
// vraci jen soubory s priponou asociovanou s viewerem tohoto pluginu (F3 na tomto
// souboru by se pokusilo otevrit viewer tohoto pluginu + ignoruje pripadne zastineni
// viewerem jineho pluginu); 'fileName' je buffer pro ziskane jmeno (velikost alespon
// MAX_PATH); vraci TRUE pokud se podari jmeno ziskat; vraci FALSE pri chybe: zadne
// predchozi jmeno souboru ve zdroji neni (neni-li 'noMoreFiles' NULL, vraci se v nem
// TRUE), zdroj je zaneprazdnen (nezpracovava zpravy; neni-li 'srcBusy' NULL, vraci
// se v nem TRUE), jinak zdroj prestal existovat (zmena cesty v panelu, zmena
// razeni, atp.)
BOOL GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                  BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                  char* fileName, BOOL* noMoreFiles, BOOL* srcBusy,
                                  CPluginInterfaceAbstract* plugin);

// zjisti, jestli je aktualni soubor z vieweru oznaceny (selected) ve zdroji (levy/pravy
// panel nebo Findy); 'srcUID' je unikatni identifikator zdroje (predava se jako parametr
// pri otevirani vieweru); 'lastFileIndex' je parametr, ktery by plugin nemel menit,
// pocatecni hodnota 'lastFileIndex' se predava jako parametr pri otevirani vieweru;
// 'lastFileName' je plne jmeno aktualniho souboru; vraci TRUE pokud se podarilo zjistit,
// jestli je aktialni soubor oznaceny, vysledek je v 'isFileSelected' (nesmi byt NULL);
// vraci FALSE pri chybe: zdroj prestal existovat (zmena cesty v panelu, atp.) nebo soubor
// 'lastFileName' uz ve zdroji neni (pri techto dvou chybach, neni-li 'srcBusy' NULL,
// vraci se v nem FALSE), zdroj je zaneprazdnen (nezpracovava zpravy; pri teto chybe,
// neni-li 'srcBusy' NULL, vraci se v nem TRUE)
BOOL IsFileNameForViewerSelected(int srcUID, int lastFileIndex, const char* lastFileName,
                                 BOOL* isFileSelected, BOOL* srcBusy);

// nastavi oznaceni (selectionu) na aktualnim souboru z vieweru ve zdroji (levy/pravy
// panel nebo Findy); 'srcUID' je unikatni identifikator zdroje (predava se jako parametr
// pri otevirani vieweru); 'lastFileIndex' je parametr, ktery by plugin nemel menit,
// pocatecni hodnota 'lastFileIndex' se predava jako parametr pri otevirani vieweru;
// 'lastFileName' je plne jmeno aktualniho souboru; 'select' je TRUE/FALSE pokud se ma
// aktualni soubor oznacit/odznacit; vraci TRUE pri uspechu; vraci FALSE pri chybe:
// zdroj prestal existovat (zmena cesty v panelu, atp.) nebo soubor 'lastFileName' uz
// ve zdroji neni (pri techto dvou chybach, neni-li 'srcBusy' NULL, vraci se v nem FALSE),
// zdroj je zaneprazdnen (nezpracovava zpravy; pri teto chybe, neni-li 'srcBusy' NULL,
// vraci se v nem TRUE)
BOOL SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex, const char* lastFileName,
                                     BOOL select, BOOL* srcBusy);

// zmeni zdroji (panelu nebo Findu) UID (negeneruje nove, aktualizuje pole
// FileNamesEnumSources a vrati nove UID v 'srcUID')
void EnumFileNamesChangeSourceUID(HWND hWnd, int* srcUID);

// prida zdroji (panelu nebo Findu) UID (negeneruje nove, prida dvojici
// hWnd+UID do pole FileNamesEnumSources a vrati nove UID v 'srcUID')
void EnumFileNamesAddSourceUID(HWND hWnd, int* srcUID);

// vyradi zdroj (panelu nebo Findu) z pole FileNamesEnumSources
void EnumFileNamesRemoveSourceUID(HWND hWnd);

//******************************************************************************
// neblokujici cteni volume-name CD drivu

extern CRITICAL_SECTION ReadCDVolNameCS;   // kriticka sekce pro pristup k datum
extern UINT_PTR ReadCDVolNameReqUID;       // UID pozadavku (pro rozpoznani jestli na vysledek jeste nekdo ceka)
extern char ReadCDVolNameBuffer[MAX_PATH]; // IN/OUT buffer (root/volume_name)

//******************************************************************************
// funkce pro praci s historiemi posledne pouzitych hodnot v comboboxech

// prida do sdilene historie ('historyArr'+'historyItemsCount') naalokovanou kopii
// nove hodnoty 'value'; je-li 'caseSensitiveValue' TRUE, hleda se hodnota (retezec)
// v poli historie pomoci case-sensitive porovnani (FALSE = case-insensitive porovnani),
// nalezena hodnota se pouze presouva na prvni misto v poli historie
void AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                const char* value, BOOL caseSensitiveValue);

// prida do comboboxu ('combo') texty ze sdilene historie ('historyArr'+'historyItemsCount');
// pred pridavanim provede reset obsahu comboboxu (viz CB_RESETCONTENT)
void LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount);

//******************************************************************************

// funkce pro pridani vsech dosud neznamych aktivnich (naloadenych) modulu procesu
void AddNewlyLoadedModulesToGlobalModulesStore();

//******************************************************************************

// quicksort s porovnavanim pres StrICmp
void SortNames(char* files[], int left, int right);

// hleda retezec 'name' v poli 'usedNames' (pole je serazene pomoci StrICmp);
// vraci TRUE pri nalezeni + nalezeny index v 'index' (neni-li NULL); vraci
// FALSE pokud prvek nebyl nalezen + index pro vlozeni v 'index' (neni-li NULL)
BOOL ContainsString(TIndirectArray<char>* usedNames, const char* name, int* index = NULL);

//******************************************************************************

// v pripade uspechu vrati TRUE a cestu na "Documents", pripadne na "Desktop"
// v pripade neuspechu vrati FALSE
// 'pathLen' udava velikost bufferu 'path'; funkce zajisti terminovani retezce i v
// pripade zkraceni
BOOL GetMyDocumentsOrDesktopPath(char* path, int pathLen);

// To optimize performance, it is good practice for applications to detect whether they
// are running in a Terminal Services client session. For example, when an application
// is running on a remote session, it should eliminate unnecessary graphic effects, as
// described in Graphic Effects. If the user is running the application in a console
// session (directly on the terminal), it is not necessary for the application to
// optimize its behavior.
//
// Returns TRUE if the application is running in a remote session and FALSE if the
// application is running on the console.
BOOL IsRemoteSession(void);

// vraci TRUE, pokud je uzivatel mezi Administratory
// v pripade chyby vraci FALSE
BOOL IsUserAdmin();

//******************************************************************************

// pro zajisteni uniku z odstranenych drivu na fixed drive (po vysunuti device - USB flash disk, atd.)
extern BOOL ChangeLeftPanelToFixedWhenIdleInProgress; // TRUE = prave se meni cesta, nastaveni ChangeLeftPanelToFixedWhenIdle na TRUE je zbytecne
extern BOOL ChangeLeftPanelToFixedWhenIdle;
extern BOOL ChangeRightPanelToFixedWhenIdleInProgress; // TRUE = prave se meni cesta, nastaveni ChangeRightPanelToFixedWhenIdle na TRUE je zbytecne
extern BOOL ChangeRightPanelToFixedWhenIdle;
extern BOOL OpenCfgToChangeIfPathIsInaccessibleGoTo; // TRUE = v idle otevre konfiguraci na Drives a focusne "If path in panel is inaccessible, go to:"

// root drivu (i UNC), pro ktery je zobrazen messagebox "drive not ready" s Retry+Cancel
// tlacitky (pouziva se pro automaticke Retry po vlozeni media do drivu)
extern char CheckPathRootWithRetryMsgBox[MAX_PATH];
// dialog "drive not ready" s Retry+Cancel tlacitky (pouziva se pro automaticke Retry po
// vlozeni media do drivu)
extern HWND LastDriveSelectErrDlgHWnd;

// GetDriveFormFactor returns the drive form factor.
//  It returns 350 if the drive is a 3.5" floppy drive.
//  It returns 525 if the drive is a 5.25" floppy drive.
//  It returns 800 if the drive is a 8" floppy drive.
//  It returns   1 if the drive supports removable media other than 3.5", 5.25", and 8" floppies.
//  It returns   0 on error.
//  iDrive is 1 for drive A:, 2 for drive B:, etc.
DWORD GetDriveFormFactor(int iDrive);

//******************************************************************************

// seradi pole pluginu podle PluginFSCreateTime (pro zobrazeni v Alt+F1/F2 a Disconnect dialogu)
void SortPluginFSTimes(CPluginFSInterfaceEncapsulation** list, int left, int right);

// vraci poradove cislo pro text polozky do change drive menu a Disconnect dialog;
// viz CPluginFSInterfaceEncapsulation::ChngDrvDuplicateItemIndex
int GetIndexForDrvText(CPluginFSInterfaceEncapsulation** fsList, int count,
                       CPluginFSInterfaceAbstract* fsIface, int currentIndex);

//******************************************************************************

// pomoci TweakUI si mohou uzivatele menit ikonku shortcuty (default, custom, zadna)
// tato funkce vytahne HShortcutOverlayXX
// existujici zahodi
BOOL GetShortcutOverlay();

// vraci textovou podobou 'hotKey' (LOBYTE=vk, HIBYTE=mods), 'buff' musi mit nejmene 50 znaku
void GetHotKeyText(WORD hotKey, char* buff);

// vraci bits per pixel displeje
int GetCurrentBPP(HDC hDC = NULL);

// iteruje pres parenty smerem k tomu nejparentovanejsimu
HWND GetTopLevelParent(HWND hWindow);

//******************************************************************************

// promenne pouzite behem ukladani konfigurace pri shutdownu, log-offu nebo restartu (musime
// pumpovat zpravy, aby nas system nesestrelil jako "not responding" softik)
class CWaitWindow;
extern CWaitWindow* GlobalSaveWaitWindow; // pokud existuje globalni wait okenko pro Save, je zde (jinak je zde NULL)
extern int GlobalSaveWaitWindowProgress;  // aktualni hodnota progresu globalniho wait okenka pro Save

extern BOOL IsSetSALAMANDER_SAVE_IN_PROGRESS; // TRUE = v registry je vytvorena hodnota SALAMANDER_SAVE_IN_PROGRESS (detekce preruseni ukladani konfigurace)

//******************************************************************************

// podpurna struktura a funkce pro otevreni kontextoveho menu + spousteni jeho polozek
// v CSalamanderGeneral::OpenNetworkContextMenu()

struct CTmpEnumData
{
    int* Indexes;
    CFilesWindow* Panel;
};

const char* EnumFileNames(int index, void* param);

void ShellActionAux5(UINT flags, CFilesWindow* panel, HMENU h);
void AuxInvokeCommand(CFilesWindow* panel, CMINVOKECOMMANDINFO* ici);
void ShellActionAux6(CFilesWindow* panel);

//******************************************************************************

// vraci v 'path' (buffer aspon MAX_PATH znaku) cestu Configuration.IfPathIsInaccessibleGoTo;
// zohlednuje nastaveni Configuration.IfPathIsInaccessibleGoToIsMyDocs
void GetIfPathIsInaccessibleGoTo(char* path, BOOL forceIsMyDocs = FALSE);

// nacte z konfigurace v registry konfiguraci icon overlay handleru
void LoadIconOvrlsInfo(const char* root);

// vraci TRUE pokud je icon overlay handler zakazany (nebo jsou zakazane vsechny)
BOOL IsDisabledCustomIconOverlays(const char* name);

// vraci TRUE pokud je icon overlay handler v seznamu zakazanych icon overlay handleru
BOOL IsNameInListOfDisabledCustomIconOverlays(const char* name);

// vycisti seznam zakazanych icon overlay handleru
void ClearListOfDisabledCustomIconOverlays();

// prida 'name' do seznamu zakazanych icon overlay handleru
BOOL AddToListOfDisabledCustomIconOverlays(const char* name);

// nacte ikonu z ImageResDLL
HICON SalLoadImage(int vistaResID, int otherResID, int cx, int cy, UINT flags);

// nacte ikonu pro archivy
HICON LoadArchiveIcon(int cx, int cy, UINT flags);

// ziska login pro zadanou sitovou cestu, pripadne obnovi jeji mapovani
BOOL RestoreNetworkConnection(HWND parent, const char* name, const char* remoteName, DWORD* retErr = NULL,
                              LPNETRESOURCE lpNetResource = NULL);

// sestavi text do sloupce Type v panelu pro neasociovany soubor (napr. "AAA File" nebo "File")
void GetCommonFileTypeStr(char* buf, int* resLen, const char* ext);

// najde zdvojene separatory a vymaze ty nadbytecne (na Viste se mi objevily zdvojene
// separatory v kontextovem menu na .bar souborech)
void RemoveUselessSeparatorsFromMenu(HMENU h);

// vraci adresar "Open Salamander" na ceste CSIDL_APPDATA do 'buf' (buffer o velikosti MAX_PATH)
BOOL GetOurPathInRoamingAPPDATA(char* buf);

// vytvori adresar "Open Salamander" na ceste CSIDL_APPDATA; vraci TRUE pokud se cesta
// vejde do MAX_PATH (jeji existence neni zarucena, vysledek CreateDirectory se nekontroluje);
// neni-li 'buf' NULL, jde o buffer o velikosti MAX_PATH, ve kterem se vraci tato cesta
// POZOR: pouziti jen Vista+
BOOL CreateOurPathInRoamingAPPDATA(char* buf);

#ifndef _WIN64

// jen 32-bitova verze pod jen Win64: zjistuje jestli jde o cestu, kterou redirector presmeruje do
// SysWOW64 nebo naopak zpet do System32
BOOL IsWin64RedirectedDir(const char* path, char** lastSubDir, BOOL failIfDirWithSameNameExists);

// jen 32-bitova verze pod Win64: zjistuje jestli selectiona obsahuje pseudo-adresar, ktery redirector
// presmeruje do SysWOW64 nebo naopak zpet do System32 a zaroven na disku neexistuje stejne pojmenovany
// adresar (pseudo-adresar pridany jen na zaklade AddWin64RedirectedDir)
BOOL ContainsWin64RedirectedDir(CFilesWindow* panel, int* indexes, int count, char* redirectedDir,
                                BOOL onlyAdded);

#endif // _WIN64

// nase varianty funkci RegQueryValue a RegQueryValueEx, narozdil od API variant
// zajistuji pridani null-terminatoru pro typy REG_SZ, REG_MULTI_SZ a REG_EXPAND_SZ
// POZOR: pri zjistovani potrebne velikosti bufferu vraci o jeden nebo dva (dva
//        jen u REG_MULTI_SZ) znaky vic pro pripad, ze by string bylo potreba
//        zakoncit nulou/nulami
extern "C"
{
    LONG SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData);
    LONG SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                            LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
}

// Win7 a novejsi OS - notifikace od taskbar, ze doslo k vytvoreni tlacitka pro okno
// nastavuje se pri startu Salamandera, testovat zda je nenulova
extern UINT TaskbarBtnCreatedMsg;

// vrati rozmer ikony s ohledem na promennou SystemDPI
// pokud je 'large' TRUE, vraci rozmer pro velkou ikonu, jinak pro malou
int GetIconSizeForSystemDPI(CIconSizeEnum iconSize);

// vraci aktualni systemove DPI (96, 120, 144, ...)
int GetSystemDPI();

// vraci scale odpovidajici aktualnimu DPI; misto 1.0 vraci 100, pro 1.25 vraci 125, atd
int GetScaleForSystemDPI();
