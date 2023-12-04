// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
// KONSTANTY
// ****************************************************************************

// zvolene limity pro FTP adresy
#define HOST_MAX_SIZE 201         // jmenna adresa FTP serveru
#define USERPASSACCT_MAX_SIZE 101 // maximum z USER_MAX_SIZE, PASSWORD_MAX_SIZE a ACCOUNT_MAX_SIZE
#define USER_MAX_SIZE 101         // uzivatelske jmeno (pri zmenach pozor na USERPASSACCT_MAX_SIZE)
#define PASSWORD_MAX_SIZE 101     // heslo (pri zmenach pozor na USERPASSACCT_MAX_SIZE)
#define ACCOUNT_MAX_SIZE 101      // account-info (viz FTP prikaz "ACCT") (pri zmenach pozor na USERPASSACCT_MAX_SIZE)
#define FTP_MAX_PATH 301          // cesta na FTP serveru (POZOR: neni to omezeni pro CFileData::Name - to je dlouhe max. MAX_PATH-5 znaku - omezeni Salamandera)
#define FTP_USERPART_SIZE 610     // omezeni na velikost user-part cesty (format: "ftp:" + "//user@host:port/path")
#define BOOKMSRVTYPE_MAX_SIZE 101 // maximum z BOOKMARKNAME_MAX_SIZE a SERVERTYPE_MAX_SIZE
#define BOOKMARKNAME_MAX_SIZE 101 // jmeno bookmarky na FTP server (napr. v dialogu Connect) (pri zmenach pozor na BOOKMSRVTYPE_MAX_SIZE)
#define SERVERTYPE_MAX_SIZE 101   // jmeno typu FTP serveru (napr. v dialogu Connect/Advanced...) (pri zmenach pozor na BOOKMSRVTYPE_MAX_SIZE)
#define FTPCOMMAND_MAX_SIZE 301   // max. delka FTP commandu (pouziva prikaz "Send FTP Command" + "Adv. connect dlg: Command for listing" + max. delka prikazu generovaneho proxy scriptem)
#define AUTODETCOND_MAX_SIZE 1001 // max. delka podminky pouzivane pri autodetekci "server type"
#define PARSER_MAX_SIZE 20481     // max. delka parseru listingu (ted 20KB)
#define STC_ID_MAX_SIZE 101       // Server Type Columns: max. delka ID
#define STC_NAME_MAX_SIZE 101     // Server Type Columns: max. delka jmena
#define STC_DESCR_MAX_SIZE 201    // Server Type Columns: max. delka popisu
#define STC_EMPTYVAL_MAX_SIZE 101 // Server Type Columns: max. delka retezce prazdne hodnoty
#define PROXYSRVNAME_MAX_SIZE 101 // jmeno proxy serveru (viz CFTPProxyServer::ProxyName)
#define PROXYSCRIPT_MAX_SIZE 2049 // max. delka proxy skriptu (ted 2KB) (viz CFTPProxyServer::ProxyScript)

// max. delka retezce do ktereho se ulozi Server Type Column
#define STC_MAXCOLUMNSTR (1 + 2 * (STC_ID_MAX_SIZE + STC_NAME_MAX_SIZE + STC_DESCR_MAX_SIZE + STC_EMPTYVAL_MAX_SIZE - 4) + 10 + 10 + 2 + 1 + 8)

// dimenzovani historie
#define COMMAND_HISTORY_SIZE 10     // max. pocet pamatovanych polozek pro historii prikazu "Send FTP Command"
#define HOSTADDRESS_HISTORY_SIZE 30 // max. pocet pamatovanych polozek pro historii adres FTP serveru v Connect to FTP Server dialogu
#define INITIALPATH_HISTORY_SIZE 30 // max. pocet pamatovanych polozek pro historii init-path na FTP serveru v Connect to FTP Server dialogu

// pocet standardnich jmen a popisu pro sloupce v "server types"
#define STC_STD_NAMES_COUNT 37

// prikazy v menu pluginu:
#define FTPCMD_CONNECTFTPSERVER 1   // connect to FTP server...
#define FTPCMD_DISCONNECT 2         // disconnect...
#define FTPCMD_ADDBOOKMARK 3        // add bookmark...
#define FTPCMD_CLOSECONNOTIF 4      // neni v menu (pomocny prikaz): zkontroluje jestli uz se uzivatel dozvedel o zavreni "control connection"
#define FTPCMD_SHOWLOGS 5           // active panel: show logs...
#define FTPCMD_SENDFTPCOMMAND 6     // send FTP command...
#define FTPCMD_ORGANIZEBOOKMARKS 7  // organize bookmarks...
#define FTPCMD_CHANGETGTPANELPATH 8 // neni v menu (pomocny prikaz): zmena cesty v druhem panelu
#define FTPCMD_SHOWRAWLISTING 9     // show raw listing...
#define FTPCMD_TRMODEAUTO 10        // transfer mode/automatic
#define FTPCMD_TRMODEASCII 11       // transfer mode/ascii
#define FTPCMD_TRMODEBINARY 12      // transfer mode/binary
#define FTPCMD_REFRESHPATH 13       // refresh current path
#define FTPCMD_CANCELOPERATION 14   // neni v menu (pomocny prikaz): cancel FTP operace
#define FTPCMD_RETURNCONNECTION 15  // neni v menu (pomocny prikaz): navrat connectiony z workera do panelu
#define FTPCMD_REFRESHLEFTPANEL 16  // neni v menu (pomocny prikaz): tvrdy refresh leveho panelu
#define FTPCMD_REFRESHRIGHTPANEL 17 // neni v menu (pomocny prikaz): tvrdy refresh praveho panelu
#define FTPCMD_DISCONNECT_F12 18    // disconnect... (pres salamandri Disconnect - tlacitko F12)
#define FTPCMD_ACTIVWELCOMEMSG 19   // neni v menu (pomocny prikaz): aktivuje welcome message (jen pokud existuje; pri zobrazeni messageboxu s chybou zustava neaktivni)
#define FTPCMD_LISTHIDDENFILES 20   // list hidden files (unix)
#define FTPCMD_TRMODESUBMENU 21     // transfer mode submenu (je-li disablovane, help pres Shift+F1 chodi pro submenu)
#define FTPCMD_SHOWLOGSLEFT 22      // left panel: show logs...
#define FTPCMD_SHOWLOGSRIGHT 23     // right panel: show logs...
#define FTPCMD_SHOWCERT 24          // show certificate (SSL)...

// prikazy v kontextovem menu change drive menu pluginu:
#define CHNGDRV_CONNECTFTPSERVER 1  // connect to FTP server...
#define CHNGDRV_DISCONNECT 2        // disconnect...
#define CHNGDRV_OPENDETACHED 3      // open...
#define CHNGDRV_ADDBOOKMARK 4       // add bookmark...
#define CHNGDRV_SHOWLOG 5           // show log
#define CHNGDRV_SENDFTPCOMMAND 6    // send FTP command...
#define CHNGDRV_ORGANIZEBOOKMARKS 7 // organize bookmarks...
#define CHNGDRV_SHOWLOGS 8          // show logs...
#define CHNGDRV_SHOWRAWLISTING 9    // show raw listing...
#define CHNGDRV_LISTHIDDENFILES 10  // list hidden files
#define CHNGDRV_SHOWCERT 11         // show certificate...

// ****************************************************************************
// GLOBALNI PROMENNE
// ****************************************************************************

// globalni data
extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy
extern HICON FTPIcon;         // ikona (16x16) FTP
extern HICON FTPLogIcon;      // ikona (16x16) dialogu FTP Logs
extern HICON FTPLogIconBig;   // velka ikona (32x32) dialogu FTP Logs
extern HICON FTPOperIcon;     // ikona (16x16) dialogu operace
extern HICON FTPOperIconBig;  // velka (32x32) ikona dialogu operace
extern HCURSOR DragCursor;    // kurzor pro drag&drop listbox v Connect dialogu
extern HFONT FixedFont;       // font pro Welcome Message dialog (fixed, aby lepe fungovalo usporadani textu)
extern HFONT SystemFont;      // font prostredi (dialogy, wait-window, atd.)
extern HICON WarningIcon;     // mala (16x16) ikona "warning" do dialogu operace

// obecne rozhrani Salamandera - platne od startu az do ukonceni plug-inu
extern CSalamanderGeneralAbstract* SalamanderGeneral;

// ZLIB compression/decompression interface;
extern CSalamanderZLIBAbstract* SalZLIB;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
extern CSalamanderGUIAbstract* SalamanderGUI;

// FS-name pridelene Salamanderem po loadu plug-inu
extern char AssignedFSName[MAX_PATH];
extern int AssignedFSNameLen; // delka AssignedFSName

// FS-name pro FTP over SSL (FTPS) pridelene Salamanderem po loadu pluginu
extern char AssignedFSNameFTPS[MAX_PATH];
extern int AssignedFSNameIndexFTPS; // index AssignedFSNameFTPS
extern int AssignedFSNameLenFTPS;   // delka AssignedFSNameFTPS

// ukazatele na tabulky mapovani na mala/velka pismena
extern unsigned char* LowerCase;
extern unsigned char* UpperCase;

// casto pouzita chybova hlaska
extern const char* LOW_MEMORY;

extern const char* SAVEBITS_CLASSNAME; // trida pro CWaitWindow

extern int GlobalShowLogUID;      // UID logu, ktery ma FTPCMD_SHOWLOGS zobrazit (-1 == zadny)
extern int GlobalDisconnectPanel; // panel, pro ktery se vola disconnect (-1 == aktivni panel - source)

extern CThreadQueue AuxThreadQueue; // fronta vsech pomocnych threadu

extern BOOL WindowsVistaAndLater; // Windows Vista nebo pozdejsi z rady NT

// globalni promenne pro prikaz FTPCMD_CHANGETGTPANELPATH
extern int TargetPanelPathPanel;
extern char TargetPanelPath[MAX_PATH];

extern char UserDefinedSuffix[100]; // predloadeny string pro oznaceni uzivatelskych "server type"

extern const char* LIST_CMD_TEXT;   // text FTP prikazu "LIST"
extern const char* NLST_CMD_TEXT;   // text FTP prikazu "NLST"
extern const char* LIST_a_CMD_TEXT; // text FTP prikazu "LIST -a"

extern int SortByExtDirsAsFiles; // aktualni hodnota konfiguracni promenne Salamandera SALCFG_SORTBYEXTDIRSASFILES
extern int InactiveBeepWhenDone; // aktualni hodnota konfiguracni promenne Salamandera SALCFG_MINBEEPWHENDONE

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
extern const CFileData** TransferFileData;
extern int* TransferIsDir;
extern char* TransferBuffer;
extern int* TransferLen;
extern DWORD* TransferRowData;
extern CPluginDataInterfaceAbstract** TransferPluginDataIface;
extern DWORD* TransferActCustomData;

// konstanty pouzivane v ftp2.cpp i ftp.cpp
extern const char* CONFIG_SERVERTYPES;
extern const char* CONFIG_STNAME;
extern const char* CONFIG_STADCOND;
extern const char* CONFIG_STCOLUMNS;
extern const char* CONFIG_STRULESFORPARS;
extern const char* CONFIG_FTPSERVERLIST;
extern const char* CONFIG_FTPPROXYLIST;
extern const char* CONFIG_FTPPRXUID;
extern const char* CONFIG_FTPPRXNAME;
extern const char* CONFIG_FTPPRXTYPE;
extern const char* CONFIG_FTPPRXHOST;
extern const char* CONFIG_FTPPRXPORT;
extern const char* CONFIG_FTPPRXUSER;
extern const char* CONFIG_FTPPRXPASSWD_OLD;
extern const char* CONFIG_FTPPRXPASSWD_SCRAMBLED;
extern const char* CONFIG_FTPPRXPASSWD_ENCRYPTED;

extern const char* CONFIG_FTPPRXSCRIPT;

// ****************************************************************************
// GLOBALNI FUNKCE
// ****************************************************************************

// pomocne funkce
BOOL InitFS();
void ReleaseFS();

// nacte z resourcu tohoto SPL modulu retezec s ID 'resID', vraci ukazatel na
// retezec (do globalniho cyklickeho bufferu)
char* LoadStr(int resID);

// provedeni prikazu "Connect FTP Server" - vola se v Alt+F1/F2 a v Drive barach a z menu pluginu
void ConnectFTPServer(HWND parent, int panel);

// provedeni prikazu "Organize Bookmarks" - vola se v Alt+F1/F2 a v Drive barach a z menu pluginu
void OrganizeBookmarks(HWND parent);

// prekopiruje do 'str' obsah 'newStr', je-li potreba realokuje 'str'; 'err' (nei-li NULL) nastavi na TRUE
// pri nedostatku pameti; je-li 'clearMem' TRUE, cisti pamet, ktera se prestava pouzivat (kvuli heslum)
void UpdateStr(char*& str, const char* newStr, BOOL* err = NULL, BOOL clearMem = FALSE);

// scramblovani a opacny proces pouzity pro ukladani hesel do registry
void ScramblePassword(char* password);
void UnscramblePassword(char* password);

// je-li 'str'==NULL, vraci prazdny string misto NULL
char* HandleNULLStr(char* str);

// je-li 's' prazdny retezec, vraci NULL, jinak vraci 's'
const char* GetStrOrNULL(const char* s);

// vraci TRUE pokud 's' neni NULL ani ""
BOOL IsNotEmptyStr(const char* s);

// alokuje kopii bufferu 'password' o velikosti 'size', vrati na nej ukazatel
BYTE* DupEncryptedPassword(const BYTE* password, int size);
void UpdateEncryptedPassword(BYTE** password, int* passwordSize, const BYTE* newPassword, int newPasswordSize);

// vraci cestu k Documents (CSIDL_PERSONAL); 'initDir' je buffer o min. vel. MAX_PATH;
// pri neuspechu vraci prazdny retezec
void GetMyDocumentsPath(char* initDir);

// vraci 'typeName' v zobrazitelne podobe pro uzivatele (s priponou, bez '*');
// 'buf'+'bufSize' definuji pomocny buffer (muze se vracet ukazatel na nej),
// rozumna velikost bufferu je (SERVERTYPE_MAX_SIZE + 101)
char* GetTypeNameForUser(char* typeName, char* buf, int bufSize);

// nacte z resourcu standardni jmeno sloupce s identifikaci 'id' (sloupce pro
// "server type" - reseni podpory prekladu userem definovanych parseru);
// vraci TRUE pokud je 'id' zname
BOOL LoadStdColumnStrName(char* buf, int bufSize, int id);

// nacte z resourcu standardni popis sloupce s identifikaci 'id' (sloupce pro
// "server type" - reseni podpory prekladu userem definovanych parseru)
// vraci TRUE pokud je 'id' zname
BOOL LoadStdColumnStrDescr(char* buf, int bufSize, int id);

// RegEdit neumi import stringu s EOLy z .reg souboru -> nahradime EOLy (CRLF za '|',
// LF za '!' a CR za '$');
// konverze normalniho stringu na string do registry; vraci FALSE pokud se vysledny
// string nevesel do bufferu
BOOL ConvertStringTxtToReg(char* buf, int bufSize, const char* txtStr);

// RegEdit neumi import stringu s EOLy z .reg souboru -> nahradime EOLy (CRLF za '|',
// LF za '!' a CR za '$');
// konverze stringu z registry na normalni string; vraci FALSE pokud se vysledny
// string nevesel do bufferu
BOOL ConvertStringRegToTxt(char* buf, int bufSize, const char* regStr);

// synchronizovany counter: "cas" startu listingu a zmen v upload listingach;
// vraci v krit. sekci inkrementovany counter - prvni volani vraci hodnotu 1
DWORD IncListingCounter();

// vraci prvni okno z rady GetParent(), ktere neni child nebo nema parenta (GetParent() pro nej vraci NULL)
HWND FindPopupParent(HWND wnd);

// nacita scrambled/encrypted/stare heslo
void LoadPassword(HKEY regKey, CSalamanderRegistryAbstract* registry, const char* oldPwdName, const char* scrambledPwdName,
                  const char* encryptedPwdName, BYTE** encryptedPassword, int* encryptedPasswordSize);

// spolecny kod pro EncryptPasswords
// drzene heslo zasifruje pomoci AES (encrypt==TRUE) nebo scrambled metody (encrypt==FALSE)
// vraci FALSE, pokud se AES-encrypted heslo nepodarilo rozsifrovat, jinak vraci TRUE
BOOL EncryptPasswordAux(BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL savePassword, BOOL encrypt);

//
// ****************************************************************************
// CServerTypeList
//

#define INT64_EMPTYNUMBER 0x8000000000000000 // nejmensi 64-bit int cislo (-9223372036854775808)

// typy sloupcu pro "server type"
enum CSrvTypeColumnTypes
{
    stctNone, // 0, neinicializovano
    // standardni sloupce vyuzivajici standardni polozky CFileData
    stctName, // 1, jmeno
    stctExt,  // 2, pripona ke jmenu (bez stctName nema smysl)
    stctSize, // 3, velikost (UNSIGNED INT64) - defaultni prazdna hodnota ve sloupci je "0" + u adresaru je hodnota vzdy "0" a v panelu se zobrazuje "DIR"
    stctDate, // 4, datum (POZOR: v CFileData je UTC cas - nutny prevod local->UTC) - defaultni prazdna hodnota ve sloupci je "1.1.1602" (upraveno podle regional settings)
    stctTime, // 5, cas (POZOR: v CFileData je UTC cas - nutny prevod local->UTC) - defaultni prazdna hodnota ve sloupci je "0:00:00" (upraveno podle regional settings)
    stctType, // 6, popis typu souboru (funguje jen je-li pritomny 'stctExt')
    // obecne sloupce, ulozene mimo CFileData (navazano pred CFileData::PluginData)
    // POZOR: musi sedet cislovani s 'stctFirstGeneral'
    stctGeneralText,                    // 7, typ retezec - defaultni prazdna hodnota ve sloupci je ""
    stctGeneralDate,                    // 8, typ datum (povazovano za local time) - defaultni prazdna hodnota ve sloupci je "" (ma-li CFTPDate::Day hodnotu 0)
    stctGeneralTime,                    // 9, typ cas (povazovano za local time) - defaultni prazdna hodnota ve sloupci je "" (ma-li CFTPTime::Hour hodnotu 24)
    stctGeneralNumber,                  // 10, typ cislo (INT64, zobrazeni ala velikost souboru - napr. "1 234 567") - defaultni prazdna hodnota ve sloupci je "" (ma-li hodnotu INT64_EMPTYNUMBER)
    stctLastItem,                       // 11, pomocna hodnota: jen pro test loadu
    stctFirstGeneral = stctGeneralText, // 7, prvni "general" typ (tyto typy se muzou v jednom server type pouzit opakovane, narozdil od ostatnich)
};

// nacte popis typu z resourcu; vraci TRUE pokud je 'type' zname (platne)
BOOL GetColumnTypeName(char* buf, int bufSize, CSrvTypeColumnTypes type);

// vygeneruje retezec prazdne hodnoty podle 'type'; vraci TRUE pokud je 'type' zname (platne)
BOOL GetColumnEmptyValueForType(char* buf, int bufSize, CSrvTypeColumnTypes type);

// vytahne a overi prazdnou hodnotu z retezce 'empty' (muze byt i NULL - std. hodnota) podle
// 'type'; vraci TRUE pokud je hodnota OK; hodnoty se vraceji ruzne: retezce jsou primo 'empty'
// (POZOR na mapovani NULL->""), unsigned cisla jsou v 'qwVal' (neni-li NULL), signed cisla
// jsou v 'int64Val' (neni-li NULL), casy a datumy jsou v prislusnych castech (oddelenych)
// struktury 'stVal' (neni-li NULL); je-li 'skipDateCheck' TRUE, netestuje se platnost
// datumu (zdlouhave)
BOOL GetColumnEmptyValue(const char* empty, CSrvTypeColumnTypes type, CQuadWord* qwVal,
                         __int64* int64Val, SYSTEMTIME* stVal, BOOL skipDateCheck);

class CSrvTypeColWidths
{
protected:
    int Links; // pocet CSrvTypeColumn, ktere na tento objekt pouzivaji (maji pointer na nej v CSrvTypeColumn::ColWidths)

public:
    int FixedWidth; // LO/HI-WORD: levy/pravy panel: 0 = elasticky sloupec (sirka podle nejsirsiho textu), 1 = pevna sirka (rovna Width)
    int Width;      // LO/HI-WORD: levy/pravy panel: sirka sloupce pri LO/HI-WORD(FixedWidth)==1

public:
    CSrvTypeColWidths()
    {
        Links = 1;
        FixedWidth = 0; // preferujeme elasticky sloupce
        Width = 0;      // minimalni sire (upravi se automaticky podle nazvu sloupce)
    }

    // prilinkuje jeden CSrvTypeColumn, ktery pouziva tento objekt; vraci ukazatel na tento objekt
    CSrvTypeColWidths* AddRef()
    {
        Links++;
        return (CSrvTypeColWidths*)this;
    }

    // odlinkuje jeden CSrvTypeColumn, ktery pouziva tento objekt; vraci TRUE pokud to byl
    // posledni - v tomto pripade je nutna dealokace tohoto objektu
    BOOL Release() { return --Links == 0; }
};

struct CSrvTypeColumn
{
public:
    BOOL Visible;                 // TRUE pokus se ma sloupec ukazovat v panelu
    char* ID;                     // identifikace sloupce pro parser (max. STC_ID_MAX_SIZE-1 znaku)
    int NameID;                   // identifikace stringu pro jmeno (-1 = pouzito nestandardni (neprekladane) jmeno - viz 'NameStr')
    char* NameStr;                // retezec nestandardniho (neprekladaneho) jmena (neni-li NameID -1, je zde NULL) (max. STC_NAME_MAX_SIZE-1 znaku)
    int DescrID;                  // identifikace stringu pro popis na headru sloupce v panelu (-1 = pouzit nestandardni (neprekladany) - viz 'DescrStr')
    char* DescrStr;               // retezec nestandardniho (neprekladaneho) popisu (neni-li DescrID -1, je zde NULL) (max. STC_DESCR_MAX_SIZE-1 znaku)
    CSrvTypeColumnTypes Type;     // typ hodnot ve sloupci
    char* EmptyValue;             // hodnota, ktera se ma pouzit, pokud se zadna hodnota nevyparsuje - retezec (pro kazdy typ hodnot ve sloupci je jina syntaxe; NULL==standardni prazdna hodnota (0, "", 1.1.1602, atd.)) (max. STC_EMPTYVAL_MAX_SIZE-1 znaku)
    BOOL LeftAlignment;           // zarovnani sloupce (ma smysl jen pro Type >= stctFirstGeneral): TRUE = left, FALSE = right
    CSrvTypeColWidths* ColWidths; // link na dvojici FixedWidth+Width pro tento sloupec (je nutne sdilet tato data mezi vsemi kopiemi sloupcu, proto jsou na tomhle "smart-pointeru")

public:
    CSrvTypeColumn(CSrvTypeColWidths* colWidths = NULL);
    ~CSrvTypeColumn();

    BOOL IsGood() { return ColWidths != NULL; }

    // nacte data o sloupci z objektu (struktura musi byt nova)
    void LoadFromObj(CSrvTypeColumn* copyFrom);

    // nacte data o sloupci z retezce (struktura musi byt nova);
    // vraci uspech operace
    BOOL LoadFromStr(const char* str);

    // ulozi data do bufferu 'buf' o velikosti 'bufSize'; vraci FALSE jen pokud se data
    // nevejdou do bufferu
    BOOL SaveToStr(char* buf, int bufSize, BOOL ignoreColWidths = FALSE);

    // alokuje kopii struktury, pri chybe vraci NULL
    CSrvTypeColumn* MakeCopy();

    // nastavuje strukturu
    void Set(BOOL visible, char* id, int nameID, char* nameStr, int descrID,
             char* descrStr, CSrvTypeColumnTypes type, char* emptyValue,
             BOOL leftAlignment, int fixedWidth, int width);

protected:
    // pomocna metoda: zjisti kolik bytu bude potreba pro ulozeni retezce 'str' do alokovaneho
    // retezce v SaveToAllocatedStr (hleda escape sekvence, atd.)
    int SaveStrLen(const char* str);

    // pomocna metoda: ulozi retezec 'str' do alokovaneho retezce v SaveToAllocatedStr; 's' je
    // aktualni pozice v alokovanem retezci; je-li 'addComma' TRUE, ulozi i znak ','
    void SaveStr(char** s, const char* str, BOOL addComma);

    // pomocna metoda: nacte jeden retezec v 'str' pro LoadFromStr (zpracuje escape-sekvence,
    // preskoci koncovou ',' - v 'str' vraci pozici dalsiho zaznamu); naalokovany vysledny
    // retezec vraci v 'result'; neni-li 'limit' -1, jde o max. pocet znaku ve vyslednem
    // retezci (bez null-terminatoru); pri chybe alokace vraci FALSE, jinak vraci TRUE
    BOOL LoadStr(const char** str, char** result, int limit);
};

// proveri jestli je seznam sloupcu v poradku (obsahuje na prvni pozici viditelny sloupec
// Name + sloupec Ext je viditelny a muze byt jen na druhe pozici + jmena promennych jsou
// unikatni a neprazdne + jmena a popisy sloupcu jsou neprazdne + krome "general/any" typu
// se typy v seznamu sloupcu neopakuji + empty value ma (podle typu sloupce) povoleny
// format); vraci TRUE je-li seznam v poradku, jinak vraci FALSE a v 'errResID' (neni-li
// NULL) cislo popisu chyby v resourcich
BOOL ValidateSrvTypeColumns(TIndirectArray<CSrvTypeColumn>* columns, int* errResID);

// vraci TRUE pokud je idenfitikator OK, jinak vraci FALSE a v 'errResID' (neni-li NULL)
// vraci cislo popisu chyby v resourcech
BOOL IsValidIdentifier(const char* s, int* errResID);

class CFTPAutodetCondNode;
class CFTPParser;

struct CServerType
{
    // jmeno typu serveru (max. SERVERTYPE_MAX_SIZE-1 znaku); pokud jmeno zacina znakem '*',
    // jde o "user defined server type" - '*' se pri zobrazeni preskoci a prida se pripona,
    //
    // popis pridavani/updatu novych server types (pri nove verzi FTP Clienta) viz ftp/servers/todo.txt
    //
    char* TypeName;
    char* AutodetectCond;                   // podminka pouzivana pri autodetekci (max. AUTODETCOND_MAX_SIZE-1 znaku); NULL=always true
    TIndirectArray<CSrvTypeColumn> Columns; // definice sloupcu pro parsovani a do panelu
    char* RulesForParsing;                  // pravidla pro parsovani listingu (max. PARSER_MAX_SIZE-1 znaku); NULL=empty

    CFTPAutodetCondNode* CompiledAutodetCond; // neni-li NULL, jde o zkompilovanou aktualni verzi AutodetectCond
    CFTPParser* CompiledParser;               // neni-li NULL, jde o zkompilovanou aktualni verzi RulesForParsing+Columns

    // !!! POZOR pri pridani nove promenne nutny update vsech metod struktury !!!

    // pomocna promenna pro potreby autodetekce parseru listingu - neni potreba inicializovat/kopirovat, atd.
    BOOL ParserAlreadyTested; // TRUE pokud jiz se tento typ serveru na listingu zkousel (neuspesne)

    CServerType() : Columns(5, 5) { Init(); }
    ~CServerType() { Release(); }

    void Init();    // inicializace vsech promennych objektu
    void Release(); // uvolneni a inicializace vsech promennych objektu

    // nastavuje strukturu, pri chybe vraci FALSE
    BOOL Set(const char* typeName, const char* autodetectCond, int columnsCount,
             const char* columnsStr[], const char* rulesForParsing);

    // nastavuje strukturu (krome 'typeName' bere vse z 'copyFrom'), pri chybe vraci FALSE
    BOOL Set(const char* typeName, CServerType* copyFrom);

    // nacteni struktury, vraci TRUE pokud je polozka v poradku (ma se pridat do seznamu)
    BOOL Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    // ulozeni struktury
    void Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);

    // alokuje kopii struktury, pri chybe vraci NULL
    CServerType* MakeCopy();

    // zapise objekt do souboru; 'file' je soubor otevreny pro zapis; vraci Windowsovy kod
    // chyby (NO_ERROR == dokonceno uspesne); nezobrazuje zadne messageboxy
    DWORD ExportToFile(HANDLE file);

    // nacte data ze souboru; 'file' je soubor otevreny pro cteni; vraci uspech
    // operace; v 'err' (nesmi byt NULL) vraci Windowsovou chybu (NO_ERROR == nenastala
    // chyba cteni souboru); v 'errResID' (nesmi byt NULL) vraci ID stringu popisujiciho
    // chybu (0 == zadna chyba)
    BOOL ImportFromFile(HANDLE file, DWORD* err, int* errResID);
};

class CServerTypeList : public TIndirectArray<CServerType>
{
public:
    CServerTypeList() : TIndirectArray<CServerType>(10, 10) {}

    // prida do seznamu typu serveru
    BOOL AddServerType(const char* typeName, const char* autodetectCond, int columnsCount,
                       const char* columnsStr[], const char* rulesForParsing);

    // prida do seznamu typu serveru
    BOOL AddServerType(const char* typeName, CServerType* copyFrom);

    // prida jmena do comboboxu + v 'index' vraci index typu 'serverType' (pokud
    // neni nalezen, vraci 'index'==-1); 'serverType' muze byt i NULL (vraci 'index'==-1)
    void AddNamesToCombo(HWND combo, const char* serverType, int& index);

    // prida jmena do listboxu
    void AddNamesToListbox(HWND listbox);

    // nacteni seznamu
    void Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    // ulozeni seznamu
    void Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);

    // nakopiruje (naalokuje a prenese data) vsechny polozky ze seznamu 'list';
    // vraci TRUE pokud se vse podarilo
    BOOL CopyItemsFrom(CServerTypeList* list);

    // vraci TRUE pokud jiz seznam jmeno 'typeName' obsahuje (bez ohledu na "user defined");
    // neni-li 'exclude' NULL, jde o polozku, ktera se ma z vyhledavani vyjmout (pouziva se
    // pri prejmenovani, aby se nenasla prejmenovavana polozka); v 'index' (neni-li NULL)
    // se vraci index nalezene polozky (-1 pokud polozka neni nalezena)
    BOOL ContainsTypeName(const char* typeName, CServerType* exclude, int* index = NULL);
};

//
// ****************************************************************************
// CFTPServerList
//

struct CFTPServer
{
    char* ItemName;                  // jmeno bookmarky (pro listbox Bookmarks v Connect dialogu) (max. BOOKMARKNAME_MAX_SIZE-1 znaku)
    char* Address;                   // host/IP (max. HOST_MAX_SIZE-1 znaku)
    char* InitialPath;               // remote path (muze byt prazdne i NULL) (max. FTP_MAX_PATH-1 znaku)
    int AnonymousConnection;         // TRUE = anonymni login, FALSE = plati UserName a Password
    char* UserName;                  // jmeno uzivatele, je-li prazdne (i NULL) a nejde o anonymni login, ptame se uzivatele (max. USER_MAX_SIZE-1 znaku)
    BYTE* EncryptedPassword;         // heslo, je-li prazdne (i NULL) a nejde o anonymni login, ptame se uzivatele (max. PASSWORD_MAX_SIZE-1 znaku)
    int EncryptedPasswordSize;       // pocet bajtu drzenych v EncryptedPassword
    int SavePassword;                // TRUE = heslo se ma ukladat na medium (registry/konfig-file)
    int ProxyServerUID;              // proxy server: -2 = default, -1 = not used, jinak UID proxy serveru z Config.FTPProxyServerList
    char* TargetPanelPath;           // neni-li NULL nebo "", ma se nastavit cilova cesta v panelu (max. MAX_PATH-1 znaku)
    char* ServerType;                // typ serveru (pro listing) - NULL==auto-detect, jinak textove jmeno (format viz CServerType::TypeName; seznam serveru se bude postupne obohacovat) (max. SERVERTYPE_MAX_SIZE-1 znaku)
    char* ListCommand;               // prikaz pro listovani - NULL==LIST_CMD_TEXT, jinak text prikazu
    int TransferMode;                // 0 - default, 1 - binary, 2 - ascii, 3 - auto (pouziva Config.ASCIIFileMasks)
    int Port;                        // port serveru, na ktery se pripojujeme (std. FTP port je IPPORT_FTP)
    int UsePassiveMode;              // 0 - PORT mode, 1 - PASV mode, 2 - dle konfigurace (Config.PassiveMode)
    int KeepConnectionAlive;         // keep con. alive: 0 - ne, 1 - ano (vlastni konfigurace), 2 - dle konfigurace (Config.KeepAlive)
    int KeepAliveSendEvery;          // jen pro KeepConnectionAlive==1: po jake dobe posilat keep-alive prikazy
    int KeepAliveStopAfter;          // jen pro KeepConnectionAlive==1: po jake dobe prestat posilat keep-alive prikazy
    int KeepAliveCommand;            // jen pro KeepConnectionAlive==1: prikaz pro keep-alive (0-NOOP, 1-PWD, 2-NLST, 3-LIST)
    int UseMaxConcurrentConnections; // 1 = pouzivat omezeni MaxConcurrentConnections; 2 = def. omezeni, 0 = bez omezeni
    int MaxConcurrentConnections;    // max. pocet soucasnych spojeni na server (control+data se pocita za jedno pripojeni)
    int UseServerSpeedLimit;         // 1 = pouzivat omezeni ServerSpeedLimit; 2 = def. omezeni, 0 = bez omezeni
    double ServerSpeedLimit;         // celkovy speed limit pro tento server
    int UseListingsCache;            // cache pro viewovane soubory a listingy: 2 = def. nast., 1 = pouzivat cache, 0 = nepouzivat cache
    char* InitFTPCommands;           // seznam FTP prikazu, ktere se maji poslat serveru hned po pripojeni
    int EncryptControlConnection;    // 0 - no, 1 - yes
    int EncryptDataConnection;       // 0 - no, 1 - yes
    int CompressData;                // 0 - no; 1-9 - zlib levels; -1 - default, based on configuration

    // !!! POZOR pri pridani nove promenne nutny update vsech metod struktury !!!

    CFTPServer() { Init(); }
    ~CFTPServer() { Release(); }

    void Init();

    // uvolni a vynuluje data struktury
    void Release();

    // alokuje kopii struktury, pri chybe vraci NULL
    // POZOR: cisti user-name, password a save-passwd pri zapnutem anonymous-connection
    CFTPServer* MakeCopy();

    // nastavuje strukturu, pri chybe vraci FALSE
    BOOL Set(const char* itemName,
             const char* address,
             const char* initialPath,
             int anonymousConnection,
             const char* userName,
             const BYTE* encryptedPassword,
             int encryptedPasswordSize,
             int savePassword,
             int proxyServerUID,
             const char* targetPanelPath,
             const char* serverType,
             int transferMode,
             int port,
             int usePassiveMode,
             int keepConnectionAlive,
             int useMaxConcurrentConnections,
             int maxConcurrentConnections,
             int useServerSpeedLimit,
             double serverSpeedLimit,
             int useListingsCache,
             const char* initFTPCommands,
             const char* listCommand,
             int keepAliveSendEvery,
             int keepAliveStopAfter,
             int keepAliveCommand,
             int encryptControlConnection,
             int encryptDataConnection,
             int compressData);

    // nastavuje strukturu podle zdroje 'src', pri chybe vraci FALSE
    BOOL Set(const CFTPServer& src)
    {
        return Set(src.ItemName,
                   src.Address,
                   src.InitialPath,
                   src.AnonymousConnection,
                   src.UserName,
                   src.EncryptedPassword,
                   src.EncryptedPasswordSize,
                   src.SavePassword,
                   src.ProxyServerUID,
                   src.TargetPanelPath,
                   src.ServerType,
                   src.TransferMode,
                   src.Port,
                   src.UsePassiveMode,
                   src.KeepConnectionAlive,
                   src.UseMaxConcurrentConnections,
                   src.MaxConcurrentConnections,
                   src.UseServerSpeedLimit,
                   src.ServerSpeedLimit,
                   src.UseListingsCache,
                   src.InitFTPCommands,
                   src.ListCommand,
                   src.KeepAliveSendEvery,
                   src.KeepAliveStopAfter,
                   src.KeepAliveCommand,
                   src.EncryptControlConnection,
                   src.EncryptDataConnection,
                   src.CompressData);
    }

    // nacteni struktury, vraci TRUE pokud je polozka v poradku (ma se pridat do seznamu)
    BOOL Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    // ulozeni struktury
    void Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);

    // vraci TRUE, pokud lze ziskat (otevrene) heslo;
    // pokud se pouziva master password a neni zadany, zepta se na nej uzivatele (okno zobrazi k 'hParent);
    // v pripade, ze uzivatel nezadal spravny master password nebo se s nim nepodarilo rozsifrovat heslo, vraci FALSE
    BOOL EnsurePasswordCanBeDecrypted(HWND hParent);
};

class CFTPProxyServerList;

class CFTPServerList : public TIndirectArray<CFTPServer>
{
public:
    CFTPServerList() : TIndirectArray<CFTPServer>(10, 10) {}

    // kopiruje (alokuje) vsechny prvky seznamu do seznamu 'dstList' (nejprve ho komplet smaze)
    // POZOR: cisti user-name, password a save-passwd pri zapnutem anonymous-connection
    BOOL CopyMembersToList(CFTPServerList& dstList);

    // prida do seznamu bookmarku na FTP server
    BOOL AddServer(const char* itemName,
                   const char* address = NULL,
                   const char* initialPath = NULL,
                   int anonymousConnection = TRUE,
                   const char* userName = NULL,
                   const BYTE* encryptedPassword = NULL,
                   int encryptedPasswordSize = 0,
                   int savePassword = FALSE,
                   int proxyServerUID = -2,
                   const char* targetPanelPath = NULL,
                   const char* serverType = NULL,
                   int transferMode = 0,
                   int port = IPPORT_FTP,
                   int usePassiveMode = 2,
                   int keepConnectionAlive = 2,
                   int useMaxConcurrentConnections = 2,
                   int maxConcurrentConnections = 1,
                   int useServerSpeedLimit = 2,
                   double serverSpeedLimit = 2,
                   int useListingsCache = 2,
                   const char* initFTPCommands = NULL,
                   const char* listCommand = NULL,
                   int keepAliveSendEvery = -1 /* -1 bude nahrazena Config.KeepAliveSendEvery */,
                   int keepAliveStopAfter = -1 /* -1 bude nahrazena Config.KeepAliveStopAfter */,
                   int keepAliveCommand = -1 /* -1 bude nahrazena Config.KeepAliveCommand */,
                   int encryptControlConnection = 0,
                   int encryptDataConnection = 0,
                   int compressData = -1 /*default*/);

    void AddNamesToListbox(HWND list);

    // projde seznam bookmark a overi jestli CFTPServer::ProxyServerUID je
    // stale platne, pokud neni, zmeni ho na -2 (default proxy server)
    void CheckProxyServersUID(CFTPProxyServerList& ftpProxyServerList);

    // projde seznam bookmark a drzena hesla zasifruje pomoci AES (encrypt==TRUE) nebo scrambled metody (encrypt==FALSE)
    // vraci FALSE, pokud se nektere z AES-encrypted hesel nepodarilo rozsifrovat, jinak vraci TRUE
    BOOL EncryptPasswords(HWND hParent, BOOL encrypt);

    // vraci TRUE, pokud v seznamu bookmarks je polozka, kterou neni sifrovana pomoci AES (je pouze scrambled)
    BOOL ContainsUnsecuredPassword();

    // nacteni seznamu
    void Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    // ulozeni seznamu
    void Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
};

//
// ****************************************************************************
// CFTPProxyServerList
//

enum CFTPProxyServerType
{
    fpstNotUsed = -1, // "prime pripojeni" - symbolicka hodnota pouzivana napr. v GetProxyScriptText()

    fpstSocks4 = 0,
    fpstSocks4A,
    fpstSocks5,
    fpstHTTP1_1,

    fpstFTP_SITE_host_colon_port,      // USER fw_user;PASS fw_pass;SITE host:port;USER user;PASS pass;ACCT acct
    fpstFTP_SITE_host_space_port,      // USER fw_user;PASS fw_pass;SITE host port;USER user;PASS pass;ACCT acct
    fpstFTP_SITE_user_host_colon_port, // USER fw_user;PASS fw_pass;SITE user@host:port;PASS pass;ACCT acct
    fpstFTP_SITE_user_host_space_port, // USER fw_user;PASS fw_pass;SITE user@host port;PASS pass;ACCT acct
    fpstFTP_OPEN_host_port,            // USER fw_user;PASS fw_pass;OPEN host:port;USER user;PASS pass;ACCT acct
    fpstFTP_transparent,               // (fw_host+fw_port se nepouzivaji - connecti se na host+port) USER fw_user;PASS fw_pass;USER user;PASS pass;ACCT acct
    fpstFTP_USER_user_host_colon_port, // USER fw_user;PASS fw_pass;USER user@host:port;PASS pass;ACCT acct
    fpstFTP_USER_user_host_space_port, // USER fw_user;PASS fw_pass;USER user@host port;PASS pass;ACCT acct
    fpstFTP_USER_fireuser_host,        // USER fw_user@host:port;PASS fw_pass;USER user;PASS pass;ACCT acct
    fpstFTP_USER_user_host_fireuser,   // USER user@host:port fw_user;PASS pass;ACCT fw_pass;ACCT acct
    fpstFTP_USER_user_fireuser_host,   // USER user@fw_user@host:port;PASS pass@fw_pass;ACCT acct

    // POZOR: fpstOwnScript musi byt posledni polozka enumu
    fpstOwnScript, // uzivatel si napsal svuj vlastni script pro pripojeni na FTP server
};

struct CFTPProxyServer;

struct CProxyScriptParams // pomocna struktura pro spousteni proxy scriptu
{
    char ProxyHost[HOST_MAX_SIZE];         // adresa/IP proxy serveru ("" = musime se zeptat usera)
    int ProxyPort;                         // port proxy serveru (je vzdy definovany)
    char ProxyUser[USER_MAX_SIZE];         // user name pro login na proxy server ("" = neznamy, skipnout celou radku)
    char ProxyPassword[PASSWORD_MAX_SIZE]; // password pro login na proxy server ("" = musime se zeptat usera)
    char Host[HOST_MAX_SIZE];              // adresa/IP FTP serveru (je vzdy definovany)
    int Port;                              // port FTP serveru (je vzdy definovany)
    char User[USER_MAX_SIZE];              // user name pro login na FTP server ("" = musime se zeptat usera)
    char Password[PASSWORD_MAX_SIZE];      // password pro login na FTP server ("" = musime se zeptat usera)
    char Account[ACCOUNT_MAX_SIZE];        // account pro login na FTP server ("" = musime se zeptat usera)

    BOOL NeedProxyHost;     // TRUE = je nutne zadat adresu/IP proxy serveru
    BOOL NeedProxyPassword; // TRUE = je nutne zadat password pro login na proxy server
    BOOL NeedUser;          // TRUE = je nutne zadat user name pro login na FTP server
    BOOL NeedPassword;      // TRUE = je nutne zadat password pro login na FTP server
    BOOL NeedAccount;       // TRUE = je nutne zadat account pro login na FTP server

    BOOL AllowEmptyPassword; // TRUE = uzivatel zadal umyslne prazdne heslo (napr. Filezilla server vyzaduje zaslani i prazdneho hesla, misto toho, aby pri prazdnem hesle ohlasil uspesne zalogovani uz po zadani jmena uzivatele, asi je to bezpecnejsi pristup)

    BOOL NeedUserInput() { return NeedProxyHost || NeedProxyPassword || NeedUser || NeedPassword || NeedAccount; }

    // 'host' nemuze byt NULL
    CProxyScriptParams(CFTPProxyServer* proxyServer, const char* host,
                       int port, const char* user, const char* password,
                       const char* account, BOOL allowEmptyPassword);
    CProxyScriptParams();
};

// validuje nebo postupne provadi proxy script; pri validaci je '*execPoint',
// 'scriptParams', 'hostBuf', 'port', 'sendCmdBuf' i 'logCmdBuf' NULL a 'lastCmdReply'==-1,
// funkce vraci TRUE pokud je skript v poradku (FALSE pri chybe, v 'errDescrBuf' je text
// chyby); 'script' je text proxy scriptu; '*execPoint' ('execPoint' nesmi byt NULL) je
// aktualni misto spousteni (skript se provadi postupne), je-li NULL, zaciname
// validaci/spousteni od zacatku skriptu; 'lastCmdReply' je vysledek prikazu z predchoziho
// radku (-1 = predchozi radek byl preskocen nebo jde o prvni radek); 'scriptParams'
// obsahuje na vstupu hodnoty jednotlivych promennych; mozne vysledky funkce
// ProcessProxyScript:
// - chyba ve skriptu: funkce vraci FALSE + pozice chyby se vraci v '*execPoint' + popis
//   chyby je v 'errDescrBuf'
// - chybejici hodnota promenne: funkce vraci TRUE a scriptParams->NeedUserInput()
//   vraci TRUE (hodnota '*execPoint' se nemeni)
// - uspesne zjisteni host:port kam se pripojit (jen na zacatku skriptu): vraci TRUE
//   a v 'hostBuf'+'port' vraci kam se pripojit, '*execPoint' ukazuje na zacatek
//   dalsiho radku skriptu
// - uspesne zjisteni jaky prikaz poslat na server (nelze na zacatku skriptu): vraci
//   TRUE a v 'sendCmdBuf' je prikaz (vcetne CRLF na konci), v 'logCmdBuf' je text
//   do logu (heslo se nahrazuje slovem "(hidden)"); '*execPoint' ukazuje na zacatek
//   dalsiho radku skriptu
// - konec scriptu (nelze na zacatku skriptu): vraci TRUE a 'sendCmdBuf' je prazdny
//   retezec, '*execPoint' ukazuje na konec skriptu;
// 'hostBuf' (neni-li NULL) je buffer o velikosti HOST_MAX_SIZE znaku; 'sendCmdBuf'
// i 'logCmdBuf' (nejsou-li NULL) jsou buffery o velikosti FTPCOMMAND_MAX_SIZE znaku;
// 'errDescrBuf' je buffer o velikosti 300 znaku; v 'proxyHostNeeded' (neni-li NULL)
// se vraci TRUE pokud ProxyHost musi byt zadane (pripojujeme se na nej)
BOOL ProcessProxyScript(const char* script, const char** execPoint, int lastCmdReply,
                        CProxyScriptParams* scriptParams, char* hostBuf, unsigned short* port,
                        char* sendCmdBuf, char* logCmdBuf, char* errDescrBuf,
                        BOOL* proxyHostNeeded);

// vraci text proxy scriptu pro dany typ proxy serveru; neni-li script definovan,
// vraci prazdny retezec; je-li 'textForDialog' vraci text pro Add/Edit Proxy Server
// dialog (Socks 4/4A/5 + HTTP 1.1 pro dialog vraci "")
const char* GetProxyScriptText(CFTPProxyServerType type, BOOL textForDialog);

// vraci defaultni port pro proxy server daneho typu
unsigned short GetProxyDefaultPort(CFTPProxyServerType type);

// vraci TRUE pokud dany typ proxy serveru pouziva heslo pro proxy server
BOOL HavePassword(CFTPProxyServerType type);

// vraci TRUE pokud dany typ proxy serveru pouziva adresu a port proxy serveru
BOOL HaveHostAndPort(CFTPProxyServerType type);

// vraci jmeno typu proxy serveru (ziskane pred LoadStr, takze buffer je jen docasny);
const char* GetProxyTypeName(CFTPProxyServerType type);

// vraci TRUE pro SOCKS4/4A/5 a HTTP 1.1 proxy servery
BOOL IsSOCKSOrHTTPProxy(CFTPProxyServerType type);

struct CFTPProxyForDataCon // data o proxy serveru pro data-connectionu
{
    CFTPProxyServerType ProxyType; // typ proxy serveru
    DWORD ProxyHostIP;             // IP proxy serveru (presneji: IP kam se connecti control-connectiona - pouziva se jen pro SOCKS4/4A/5 a HTTP 1.1)
    unsigned short ProxyPort;      // port proxy serveru
    char* ProxyUser;               // user name pro login na proxy server (NULL = prazdny)
    char* ProxyPassword;           // password pro login na proxy server (NULL = prazdny)
    char* Host;                    // adresa FTP serveru
    DWORD HostIP;                  // IP adresa FTP serveru
    unsigned short HostPort;       // port FTP serveru

    CFTPProxyForDataCon(CFTPProxyServerType proxyType, DWORD proxyHostIP,
                        unsigned short proxyPort, const char* proxyUser,
                        const char* proxyPassword, const char* host,
                        DWORD hostIP, unsigned short hostPort);
    ~CFTPProxyForDataCon();
};

struct CFTPProxyServer
{
    // pristup k datum objektu pres kritickou sekci CFTPProxyServerList::ProxyServerListCS

    int ProxyUID;    // unikatni cislo proxy serveru (nemeni se, pouziva se pro adresaci pouziteho proxy serveru)
    char* ProxyName; // unikatni (case insensitive) jmeno proxy serveru v comboboxech

    CFTPProxyServerType ProxyType; // typ proxy serveru

    char* ProxyHost;                // adresa/IP proxy serveru
    int ProxyPort;                  // port proxy serveru
    char* ProxyUser;                // user name pro login na proxy server
    BYTE* ProxyEncryptedPassword;   // password pro login na proxy server
    int ProxyEncryptedPasswordSize; // pocet bajtu drzenych v EncryptedPassword
    char* ProxyPlainPassword;       // password pro login na proxy server; slouzi pouze interne, plni se z ProxyEncryptedPassword
    int SaveProxyPassword;          // TRUE = heslo se ma ukladat na medium (registry/konfig-file)

    char* ProxyScript; // jen pro ProxyType==fpstOwnScript (jinak NULL): skript pro pripojeni na FTP server

    // !!! POZOR pri pridani nove promenne nutny update vsech metod struktury !!!

    CFTPProxyServer(int proxyUID) { Init(proxyUID); }
    ~CFTPProxyServer() { Release(); }

    void Init(int proxyUID);

    // uvolni a vynuluje data struktury
    void Release();

    // alokuje kopii struktury, pri chybe vraci NULL
    CFTPProxyServer* MakeCopy();

    // alokuje strukturu CFTPProxyForDataCon, pri chybe vraci NULL
    CFTPProxyForDataCon* AllocProxyForDataCon(DWORD proxyHostIP, const char* host,
                                              DWORD hostIP, unsigned short hostPort);

    // nastavuje ProxyHost
    BOOL SetProxyHost(const char* proxyHost);

    // nastavuje ProxyPort
    void SetProxyPort(int proxyPort);

    // nastavuje ProxyPlainPassword
    BOOL SetProxyPassword(const char* proxyPassword);

    // nastavuje ProxyUser
    BOOL SetProxyUser(const char* proxyUser);

    // nastavuje strukturu, pri chybe vraci FALSE
    BOOL Set(int proxyUID,
             const char* proxyName,
             CFTPProxyServerType proxyType,
             const char* proxyHost,
             int proxyPort,
             const char* proxyUser,
             const BYTE* proxyEncryptedPassword,
             int proxyEncryptedPasswordSize,
             int saveProxyPassword,
             const char* proxyScript);

    // nastavuje strukturu podle zdroje 'src', pri chybe vraci FALSE
    BOOL Set(const CFTPProxyServer& src)
    {
        return Set(src.ProxyUID,
                   src.ProxyName,
                   src.ProxyType,
                   src.ProxyHost,
                   src.ProxyPort,
                   src.ProxyUser,
                   src.ProxyEncryptedPassword,
                   src.ProxyEncryptedPasswordSize,
                   src.SaveProxyPassword,
                   src.ProxyScript);
    }

    // nacteni struktury, vraci TRUE pokud je polozka v poradku (ma se pridat do seznamu)
    BOOL Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    // ulozeni struktury
    void Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
};

class CFTPProxyServerList : protected TIndirectArray<CFTPProxyServer>
{
protected:
    CRITICAL_SECTION ProxyServerListCS; // kriticka sekce pro pristup ke vsem datum objektu (pocita se se zapisem do dat jen v hl. threadu, tedy cteni z hl. threadu nemusi byt synchronizovane, cteni z jinych threadu byt synchronizovane musi (aby se vylucovalo se zapisem, ktery probiha jen v kriticke sekci))

    int NextFreeProxyUID; // prvni volne unikatni cislo proxy serveru (pro CFTPProxyServer::ProxyUID)

public:
    CFTPProxyServerList();
    ~CFTPProxyServerList();

    TIndirectArray<CFTPProxyServer>::DestroyMembers;

    // vraci TRUE pokud existuje proxy server s UID 'proxyServerUID'
    BOOL IsValidUID(int proxyServerUID);

    // zjisti jestli je jmeno 'proxyName' pro proxy server 'proxyServer' OK
    // (neprazdne a unikatni v ramci ostatnich proxy serveru)
    BOOL IsProxyNameOK(CFTPProxyServer* proxyServer, const char* proxyName);

    // kopiruje (alokuje) vsechny prvky seznamu do seznamu 'dstList' (nejprve ho komplet smaze)
    BOOL CopyMembersToList(CFTPProxyServerList& dstList);

    // plni combobox "Proxy Server"; 'combo' je combobox; 'focusProxyUID' je UID serveru,
    // ktery se ma vybrat v combu (-1 == "not used", -2 == "default"); je-li
    // 'addDefault' TRUE, prida "default" polozku (pouziva se v dialogu Connect to FTP Server)
    void InitCombo(HWND combo, int focusProxyUID, BOOL addDefault);

    // ziskava vyber z comboboxu "Proxy Server"; 'combo' je combobox; ve 'focusedProxyUID'
    // se vraci UID serveru, ktery je vybrany v combu (-1 == "not used", -2 == "default");
    // 'addDefault' je TRUE pokud je pridana "default" polozka (pouziva se v dialogu
    // Connect to FTP Server)
    void GetProxyUIDFromCombo(HWND combo, int& focusedProxyUID, BOOL addDefault);

    // prida do seznamu novy proxy server (naalokuje ho); zaroven ho prida do comboboxu 'combo';
    // 'parent' je parent dialogu "Add Proxy Server"
    void AddProxyServer(HWND parent, HWND combo);

    // necha usera editovat fokusly proxy server v comboboxu 'combo'; 'parent' je parent
    // dialogu "Edit Proxy Server"; 'addDefault' je TRUE pokud je pridana "default"
    // polozka (pouziva se v dialogu Connect to FTP Server)
    void EditProxyServer(HWND parent, HWND combo, BOOL addDefault);

    // smaze proxy server focusnuty v comboboxu 'combo' (smaze ho i ze seznamu
    // proxy serveru); 'addDefault' je TRUE pokud je pridana "default" polozka
    // (pouziva se v dialogu Connect to FTP Server); 'parent' je parent messageboxu
    // s konfirmaci mazani
    void DeleteProxyServer(HWND parent, HWND combo, BOOL addDefault);

    // posun fokusleho proxy serveru nahoru v comboboxu 'combo' (zaroven i
    // v seznamu proxy serveru); 'addDefault' je TRUE pokud je pridana "default"
    // polozka (pouziva se v dialogu Connect to FTP Server)
    void MoveUpProxyServer(HWND combo, BOOL addDefault);

    // posun fokusleho proxy serveru dolu v comboboxu 'combo' (zaroven i
    // v seznamu proxy serveru); 'addDefault' je TRUE pokud je pridana "default"
    // polozka (pouziva se v dialogu Connect to FTP Server)
    void MoveDownProxyServer(HWND combo, BOOL addDefault);

    // do bufferu 'buf' o velikosti 'bufSize' nakopiruje ProxyName proxy serveru
    // s UID 'proxyServerUID'; je-li 'proxyServerUID' -1, nakopiruje "Not Used";
    // vraci FALSE pokud 'proxyServerUID' neni -1 ani platne UID proxy serveru
    BOOL GetProxyName(char* buf, int bufSize, int proxyServerUID);

    // nacteni seznamu
    void Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    // ulozeni seznamu
    void Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);

    // vola proxyServer->Set() v kriticke sekci
    BOOL SetProxyServer(CFTPProxyServer* proxyServer,
                        int proxyUID,
                        const char* proxyName,
                        CFTPProxyServerType proxyType,
                        const char* proxyHost,
                        int proxyPort,
                        const char* proxyUser,
                        const BYTE* proxyEncryptedPassword,
                        int proxyEncryptedPasswordSize,
                        int saveProxyPassword,
                        const char* proxyScript);

    // vraci kopii proxy serveru s UID 'proxyServerUID'; pokud neni nalezen nebo
    // je nedostatek pameti pro jeho kopii, vraci NULL; v 'lowMem' (neni-li NULL)
    // vraci TRUE pri chybe zpusobene nedostatkem pameti
    CFTPProxyServer* MakeCopyOfProxyServer(int proxyServerUID, BOOL* lowMem);

    // vraci typ proxy serveru s UID 'proxyServerUID'; pri neznamem UID vraci fpstNotUsed
    CFTPProxyServerType GetProxyType(int proxyServerUID);

    // projde seznam serveru a drzena hesla zasifruje pomoci AES (encrypt==TRUE) nebo scrambled metody (encrypt==FALSE)
    // vraci FALSE, pokud se nektere z AES-encrypted hesel nepodarilo rozsifrovat, jinak vraci TRUE
    BOOL EncryptPasswords(HWND hParent, BOOL encrypt);

    // vraci TRUE, pokud v seznamu serveru je polozka, kterou neni sifrovana pomoci AES (je pouze scrambled)
    BOOL ContainsUnsecuredPassword();

    // vraci TRUE, pokud lze ziskat (otevrene) heslo pro proxy server s UID 'proxyServerUID'
    // pokud se pouziva master password a neni zadany, zepta se na nej uzivatele (okno zobrazi k 'hParent)
    // v pripade ze uzivatel nezadal spravny master password nebo se s nim nepodarilo rozsifrovat heslo, vraci FALSE
    BOOL EnsurePasswordCanBeDecrypted(HWND hParent, int proxyServerUID);
};

//
// ****************************************************************************
// CConfiguration
//

enum CTransferMode
{
    trmBinary,
    trmASCII,
    trmAutodetect
};

class CConfiguration
{
public:
    DWORD LastCfgPage; // index posledni stranky fokusene v konfiguracnim dialogu

    int ShowWelcomeMessage;         // ukazovat welcome message od serveru
    int PriorityToPanelConnections; // uprednostnovat connectiony pro panely (listovani, download pro view, atd.)
    int EnableTotalSpeedLimit;      // je zapnuty celkovy speed limit pro FTP clienta?
    double TotalSpeedLimit;         // celkovy speed limit pro FTP clienta

    int PassiveMode;                 // defaultne pouzivat passive mode?
    int KeepAlive;                   // defaultne udrzet connectinu pri zivote?
    int UseMaxConcurrentConnections; // defaultne: 1 = pouzivat omezeni MaxConcurrentConnections; 0 = bez omezeni
    int MaxConcurrentConnections;    // defaultni max. pocet soucasnych spojeni na server (control+data se pocita za jedno pripojeni)
    int UseServerSpeedLimit;         // defaultne: 1 = pouzivat omezeni ServerSpeedLimit; 0 = bez omezeni
    double ServerSpeedLimit;         // defaultni celkovy speed limit pro server
    int UseListingsCache;            // cache pro viewovane soubory a listingy: defaultne pouzivat cache?
    int TransferMode;                // rezim prenosu souboru (binar/ascii/auto) (hodnoty typu CTransferMode)

    CSalamanderMaskGroup* ASCIIFileMasks; // pro automaticky rezim prenosu souboru - masky pro "ascii" rezim (ostatni budou "binar")

    int KeepAliveSendEvery; // po jake dobe posilat keep-alive prikazy
    int KeepAliveStopAfter; // po jake dobe prestat posilat keep-alive prikazy
    int KeepAliveCommand;   // prikaz pro keep-alive (0-NOOP, 1-PWD, 2-NLST, 3-LIST)

    int CompressData; // 0 - no; 1-9 - zlib levels;

    CFTPServerList FTPServerList; // seznam bookmark na FTP servery
    int LastBookmark;             // posledni zvolena bookmarka na FTP server z dialogu "Connect to FTP Server" (0 = zvolen "quick connect")

    CFTPProxyServerList FTPProxyServerList; // seznam uzivatelem nadefinovanych proxy serveru
    int DefaultProxySrvUID;                 // UID defaultniho proxy serveru (-1 == "not used")

    int AlwaysNotCloseCon; // TRUE = neptat se pred zmenou cesty z FS - vzdy si user preje "detach" (not to close)
    int AlwaysDisconnect;  // TRUE = neptat se pred disconnectem - vzdy si user preje "close connection"

    int EnableLogging;           // TRUE = povolene logovani komunikace se serverem
    int UseLogMaxSize;           // TRUE = pouzivat LogMaxSize
    int LogMaxSize;              // maximalni velikost logu jednoho serveru [v KB]
    int UseMaxClosedConLogs;     // TRUE = pouzivat MaxClosedConLogs
    int MaxClosedConLogs;        // maximalni pocet logu zavrenych connectionu (starsi se zahazuji)
    int AlwaysShowLogForActPan;  // TRUE = aktivovat log pri aktivaci FTP connectiony v panelu
    int DisableLoggingOfWorkers; // TRUE = zakazane logovani komunikace workeru se serverem

    WINDOWPLACEMENT LogsDlgPlacement;            // pozice dialogu Logs
    WINDOWPLACEMENT OperDlgPlacement;            // pozice dialogu operaci (userovo posledni nastaveni)
    double OperDlgSplitPos;                      // pozice splitu mezi listviewy v dialogu operaci
    int CloseOperationDlgIfSuccessfullyFinished; // TRUE = zavirat automaticky dialog operace v pripade dokonceni operace bez chyb/dotazu v nedotcenem dialogu
    int CloseOperationDlgWhenOperFinishes;       // TRUE = po dokonceni operace zavrit dialog operace (pokud neni otevren nejaky Solve Error dialog)
    int OpenSolveErrIfIdle;                      // TRUE = otevirat automaticky dialog Solve Error v pripade, ze dialog je "idle"

    char* CommandHistory[COMMAND_HISTORY_SIZE]; // historie FTP commandu (z prikazu "Send FTP Command")
    int SendSecretCommand;                      // posledni nastaveni checkboxu "Secret command" v dialogu "Send FTP Command"

    char* HostAddressHistory[HOSTADDRESS_HISTORY_SIZE]; // historie adres serveru v Connect to FTP Server dialogu
    char* InitPathHistory[INITIALPATH_HISTORY_SIZE];    // historie init-path na serveru v Connect to FTP Server dialogu

    int AlwaysReconnect;     // TRUE = neptat se na reconnect (po ztrate spojeni dojde pri dalsim pozadavku na server k reconnectu)
    int WarnWhenConLost;     // TRUE = varovat usera, ze prave ztratil spojeni se serverem
    int HintListHiddenFiles; // TRUE = zobrazovat hint "how to list hidden files (unix)"
    int AlwaysOverwrite;     // TRUE = neptat se na overwrite (tyka se Quick Rename)

    int ConvertHexEscSeq; // TRUE = prevadet hex-escape-sequence ("%20" -> " ") v cestach zadanych uzivatelem

    CQuadWord CacheMaxSize; // max. velikost cache (zatim se uklada jen do pameti); POZOR: v registry je zatim jen jako DWORD!!!

    BOOL DownloadAddToQueue; // TRUE = pri "copy/move from FTP" jen pridat do fronty (nezpracovavat hned v aktivni connectione)
    BOOL DeleteAddToQueue;   // TRUE = pri "delete from FTP" jen pridat do fronty (nezpracovavat hned v aktivni connectione)
    BOOL ChAttrAddToQueue;   // TRUE = pri "change-attributes on FTP" jen pridat do fronty (nezpracovavat hned v aktivni connectione)

    // Copy+Move z FTP: userem preferovane zpusoby reseni nasledujicich problemu
    int OperationsCannotCreateFile;      // viz konstanty CANNOTCREATENAME_XXX
    int OperationsCannotCreateDir;       // viz konstanty CANNOTCREATENAME_XXX
    int OperationsFileAlreadyExists;     // viz konstanty FILEALREADYEXISTS_XXX
    int OperationsDirAlreadyExists;      // viz konstanty DIRALREADYEXISTS_XXX
    int OperationsRetryOnCreatedFile;    // viz konstanty RETRYONCREATFILE_XXX
    int OperationsRetryOnResumedFile;    // viz konstanty RETRYONRESUMFILE_XXX
    int OperationsAsciiTrModeButBinFile; // viz konstanty ASCIITRFORBINFILE_XXX

    // Change Attributes na FTP: userem preferovane zpusoby reseni nasledujicich problemu
    int OperationsUnknownAttrs; // viz konstanty UNKNOWNATTRS_XXX

    // Delete na FTP: userem preferovane zpusoby reseni nasledujicich situaci
    int OperationsNonemptyDirDel; // viz konstanty NONEMPTYDIRDEL_XXX
    int OperationsHiddenFileDel;  // viz konstanty HIDDENFILEDEL_XXX
    int OperationsHiddenDirDel;   // viz konstanty HIDDENDIRDEL_XXX

    // Copy+Move na FTP: userem preferovane zpusoby reseni nasledujicich problemu
    int UploadCannotCreateFile;      // viz konstanty CANNOTCREATENAME_XXX
    int UploadCannotCreateDir;       // viz konstanty CANNOTCREATENAME_XXX
    int UploadFileAlreadyExists;     // viz konstanty FILEALREADYEXISTS_XXX
    int UploadDirAlreadyExists;      // viz konstanty DIRALREADYEXISTS_XXX
    int UploadRetryOnCreatedFile;    // viz konstanty RETRYONCREATFILE_XXX
    int UploadRetryOnResumedFile;    // viz konstanty RETRYONRESUMFILE_XXX
    int UploadAsciiTrModeButBinFile; // viz konstanty ASCIITRFORBINFILE_XXX

    // *****************************************************************************
    // nasledujici data se neukladaji (drzi se jen v pameti, pak se zahodi):
    // *****************************************************************************

    BOOL UseConnectionDataFromConfig; // TRUE jen po stisku tlacitka "Connect" v Connect dialogu
    BOOL ChangingPathInInactivePanel; // TRUE jen pri zmene cesty v neaktivnim panelu (kliknuti na tlacitku FTP v neaktivni drivebare)
    CFTPServer QuickConnectServer;    // data pro Quick Connect (predani dat z Connect dialogu)

    BOOL DisconnectCommandUsed; // TRUE jen po pouziti prikazu "Disconnect" (mimo disconnect pro odpojeny FS)

    int TestParserDlgWidth;  // sirka dialogu "Test of Parser" (-1 = defaultni sirka; -2 = maximalizovany)
    int TestParserDlgHeight; // vyska dialogu "Test of Parser" (-1 = defaultni vyska)

protected:                             // data pouzivana z vice threadu, pristup je synchronizovany:
    CServerTypeList ServerTypeList;    // seznam typu serveru
    CRITICAL_SECTION ServerTypeListCS; // kriticka sekce pro pristup k ServerTypeList

    // kriticka sekce pro pristup k parametrum spojeni, nasledujici promenne jsou
    // chranene touto kritickou sekci
    CRITICAL_SECTION ConParamsCS;
    char AnonymousPasswd[PASSWORD_MAX_SIZE]; // heslo pro anonymni pripojeni (e-mail uzivatele)
    int ServerRepliesTimeout;                // jak dlouho cekat na reakci (odpoved) serveru
    int DelayBetweenConRetries;              // doba pred opakovanim connectu
    int ConnectRetries;                      // kolikrat zkouset reconnect (pokud se nelze zalogovat z duvodu jineho nez "invalid password")
    int ResumeMinFileSize;                   // minimalni velikost souboru, aby mel smysl "resume" (navazani downloadu)
    int ResumeOverlap;                       // kolik bytu na konci souboru testovat pri "resume" (navazani downloadu)
    int NoDataTransferTimeout;               // jak dlouho cekat na data ve stavu "data connection idle" (no-data-transfer)
                                             // konec seznamu promennych chranenych kritickou sekci ConParamsCS

public:
    CConfiguration();
    ~CConfiguration();

    BOOL InitWithSalamanderGeneral();        // inicializace po ziskani ukazatele SalamanderGeneral
    void ReleaseDataFromSalamanderGeneral(); // uklid dat pred zrusenim ukazatele SalamanderGeneral

    // uzamkne pristup k ServerTypeList a vraci ukazatel na nej; odemknuti se provede
    // volanim UnlockServerTypeList()
    CServerTypeList* LockServerTypeList()
    {
        HANDLES(EnterCriticalSection(&ServerTypeListCS));
        return &ServerTypeList;
    }
    // odemknuti pristupu k ServerTypeList (musi parovat k volani LockServerTypeList())
    void UnlockServerTypeList() { HANDLES(LeaveCriticalSection(&ServerTypeListCS)); }

    // pomocne metody pro pristup k promennym chranenym sekci ConParamsCS
    void GetAnonymousPasswd(char* buf, int bufSize);
    void SetAnonymousPasswd(const char* passwd);
    int GetServerRepliesTimeout();
    void SetServerRepliesTimeout(int value);
    int GetDelayBetweenConRetries(); // vraci nejmene hodnotu 1 (validaci hodnoty zajistuje tato metoda)
    void SetDelayBetweenConRetries(int value);
    int GetConnectRetries();
    void SetConnectRetries(int value);
    int GetResumeOverlap();
    void SetResumeOverlap(int value);
    int GetResumeMinFileSize();
    void SetResumeMinFileSize(int value);
    int GetNoDataTransferTimeout();
    void SetNoDataTransferTimeout(int value);

    friend class CConfigPageAdvanced;
};

extern CConfiguration Config; // globalni konfigurace FTP klienta

//
// ****************************************************************************
// CPluginInterface
//

class CPluginInterfaceForFS : public CPluginInterfaceForFSAbstract
{
protected:
    int ActiveFSCount; // pocet aktivnich FS interfacu (jen pro kontrolu dealokace)

public:
    CPluginInterfaceForFS() { ActiveFSCount = 0; }
    int GetActiveFSCount() { return ActiveFSCount; }

    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(const char* fsName, int fsNameIndex);
    virtual void WINAPI CloseFS(CPluginFSInterfaceAbstract* fs);

    virtual void WINAPI ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex,
                                    CFileData& file, int isDir);
    virtual BOOL WINAPI DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                     CPluginFSInterfaceAbstract* pluginFS,
                                     const char* pluginFSName, int pluginFSNameIndex);

    virtual void WINAPI ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart);
    virtual void WINAPI ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart);

    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) {}

    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel);
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam);
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam);
};

class CPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
public:
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask);
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask);
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id);
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) {}
};

class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry);
    virtual void WINAPI Configuration(HWND parent);

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData);

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt();
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param);
    virtual void WINAPI ClearHistory(HWND parent);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event);
};

//
// ****************************************************************************
// FILE-SYSTEM
//

//****************************************************************************
//
// CTopIndexMem
//
// pamet top-indexu listboxu v panelu - pouziva CPluginFSInterface pro korektni
// chovani ExecuteOnFS (zachovani top-indexu po vstupu a vystupu z podadresare)

#define TOP_INDEX_MEM_SIZE 50 // pocet pamatovanych top-indexu (urovni), minimalne 1

class CTopIndexMem
{
protected:
    // cesta pro posledni zapamatovany top-index (pozor je case-sensitive)
    char Path[FTP_MAX_PATH];
    int TopIndexes[TOP_INDEX_MEM_SIZE]; // zapamatovane top-indexy
    int TopIndexesCount;                // pocet zapamatovanych top-indexu

public:
    CTopIndexMem() { Clear(); }

    // vycisti pamet
    void Clear()
    {
        Path[0] = 0;
        TopIndexesCount = 0;
    }

    // uklada top-index pro danou cestu
    void Push(CFTPServerPathType type, const char* path, int topIndex);

    // hleda top-index pro danou cestu, FALSE->nenalezeno
    BOOL FindAndPop(CFTPServerPathType type, const char* path, int& topIndex);
};

//
// ****************************************************************************
// CSimpleListPluginDataInterface
//

class CSimpleListPluginDataInterface : public CPluginDataInterfaceAbstract
{
public:
    static int ListingColumnWidth;      // LO/HI-WORD: levy/pravy panel: sirka sloupce Raw Listing
    static int ListingColumnFixedWidth; // LO/HI-WORD: levy/pravy panel: ma sloupec Raw Listing pevnou sirku?

public:
    virtual BOOL WINAPI CallReleaseForFiles() { return TRUE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return TRUE; }
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir);

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) {}          // FS neuzije
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) { return TRUE; } // FS neuzije

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) { return NULL; }                               // nemame vlastni ikony
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return TRUE; }                       // nemame vlastni ikony
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) { return NULL; } // nemame vlastni ikony
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) { return 0; }         // nemame vlastni ikony

    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                  const CFileData* upperDir);
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth);
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth);

    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount);

    virtual BOOL WINAPI CanBeCopiedToClipboard() { return TRUE; }

    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) { return FALSE; }
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) { return FALSE; }
};

//
// ****************************************************************************
// CFTPListingPluginDataInterface
//

#pragma pack(push, enter_include_ftp_h_dt) // aby byly struktury co nejmensi (obe se vejdou na 4 byty)
#pragma pack(1)

struct CFTPDate // struktura pro ulozeni hodnoty pro sloupec typu stctGeneralDate
{
    BYTE Day;
    BYTE Month;
    WORD Year;
};

struct CFTPTime // struktura pro ulozeni hodnoty pro sloupec typu stctGeneralTime
{
    unsigned Hour : 8;         // BYTE
    unsigned Minute : 8;       // BYTE
    unsigned Second : 6;       // na sekundy staci (cisla < 64)
    unsigned Millisecond : 10; // na milisekundy staci (cisla < 1024)
};

#pragma pack(pop, enter_include_ftp_h_dt)

class CFTPListingPluginDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    TIndirectArray<CSrvTypeColumn>* Columns; // definice sloupcu pro panel (nemuze byt NULL)
    BOOL DeleteColumns;                      // TRUE = v destruktoru se ma volat delete pro Columns
    DWORD ItemDataSize;                      // velikost alokovane struktury (ulozena v CFileData::PluginData) s daty pro sloupce
    DWORD* DataOffsets;                      // offsety dat pro jednotlive sloupce v CFileData::PluginData (-1 pri ulozeni dat primo v CFileData)
    DWORD ValidDataMask;                     // maska platnosti dat ve strukturach CFileData (kombinace VALID_DATA_XXX)

    // velikost se pouziva jen jedna, poradi priorit: CFileData::Size, Size in Bytes, Size in Blocks
    DWORD BytesColumnOffset;  // neni-li -1, jde o offset hodnoty typu stctGeneralNumber reprezentujici pocet bytu souboru (v listingu je sloupec "Size")
    DWORD BlocksColumnOffset; // neni-li -1, jde o offset hodnoty typu stctGeneralNumber reprezentujici pocet bloku souboru (v listingu je sloupec "Blocks")

    // datum+cas posledniho zapisu do souboru, sloupce oznacene: Date, Time; hodnoty ulozene
    // v CFileData::LastWrite nebo stctGeneralDate/stctGeneralTime
    DWORD DateColumnOffset; // neni-li -1, je-li v LastWrite -2, jde o offset hodnoty typu stctGeneralDate reprezentujici datum posledniho zapisu do souboru
    DWORD TimeColumnOffset; // neni-li -1, je-li v LastWrite -2, jde o offset hodnoty typu stctGeneralTime reprezentujici cas posledniho zapisu do souboru

    BOOL IsVMS; // TRUE = listing pochazi z VMS (pouziva metoda GetBasicName())

public:
    CFTPListingPluginDataInterface(TIndirectArray<CSrvTypeColumn>* columns, BOOL deleteColumns,
                                   DWORD validDataMask, BOOL isVMS);
    ~CFTPListingPluginDataInterface();

    // vraci TRUE jen pokud je objekt OK a lze jej pouzivat - zjistuje se po konstrukci objektu
    BOOL IsGood() { return DataOffsets != NULL; }

    // alokuje file.PluginData; vraci FALSE pri nedostatku pameti
    BOOL AllocPluginData(CFileData& file);

    // pripravi data v file.PluginData pro dalsi pouziti (dealokuje a vynuluje stringy)
    void ClearPluginData(CFileData& file);

    // ulozi alokovany retezec 'str' do struktury 'file' do sloupce 'column';
    // k dealokaci 'str' dojde pri volani ReleasePluginData() pro 'file'; pri
    // prepisu stringu dochazi k dealokaci prepisovaneho stringu
    void StoreStringToColumn(CFileData& file, int column, char* str);

    // ulozi den 'day' do struktury 'file' do sloupce 'column' do struktury typu CFTPDate
    void StoreDayToColumn(CFileData& file, int column, BYTE day);

    // ulozi mesic 'month' do struktury 'file' do sloupce 'column' do struktury typu CFTPDate
    void StoreMonthToColumn(CFileData& file, int column, BYTE month);

    // ulozi rok 'year' do struktury 'file' do sloupce 'column' do struktury typu CFTPDate
    void StoreYearToColumn(CFileData& file, int column, WORD year);

    // ulozi datum 'day'+'month'+'year' do struktury 'file' do sloupce 'column' do struktury typu CFTPDate
    void StoreDateToColumn(CFileData& file, int column, BYTE day, BYTE month, WORD year);

    // ulozi cas 'hour'+'minute'+'second'+'millisecond' do struktury 'file' do sloupce 'column'
    // do struktury typu CFTPTime
    void StoreTimeToColumn(CFileData& file, int column, BYTE hour, BYTE minute,
                           BYTE second, WORD millisecond);

    // ulozi cislo 'number' do struktury 'file' do sloupce 'column'
    void StoreNumberToColumn(CFileData& file, int column, __int64 number);

    // vraci retezec ze struktury 'file' ze sloupce 'column'
    char* GetStringFromColumn(const CFileData& file, int column);

    // vraci (v prislusne casti struktury 'stVal') datum ze struktury 'file' ze sloupce 'column'
    void GetDateFromColumn(const CFileData& file, int column, SYSTEMTIME* stVal);

    // vraci v 'date' datum ze struktury 'file' ze sloupce 'column'
    void GetDateFromColumn(const CFileData& file, int column, CFTPDate* date);

    // vraci (v prislusne casti struktury 'stVal') cas ze struktury 'file' ze sloupce 'column'
    void GetTimeFromColumn(const CFileData& file, int column, SYSTEMTIME* stVal);

    // vraci v 'time' cas ze struktury 'file' ze sloupce 'column'
    void GetTimeFromColumn(const CFileData& file, int column, CFTPTime* time);

    // vraci cislo ze struktury 'file' ze sloupce 'column';
    // POZOR: pokud vraci prazdnou hodnotu ve sloupci, vraci INT64_EMPTYNUMBER
    __int64 GetNumberFromColumn(const CFileData& file, int column);

    // najde sloupec "Rights" s textovym obsahem; pokud neexistuje, vraci -1
    int FindRightsColumn();

    // pokud je znama velikost souboru 'file', vraci TRUE a velikost v 'size' a
    // v 'inBytes' vraci TRUE/FALSE podle toho je-li velikost v bytech/blocich;
    // pokud velikost souboru neni znama, vraci FALSE a 'size' ani 'inBytes' nemeni
    BOOL GetSize(const CFileData& file, CQuadWord& size, BOOL& inBytes);

    // vraci zakladni jmeno - zatim se pouziva jen pri oriznuti cisla verze jmen
    // souboru na VMS (napr. pri porovnani s maskami se verze silne nehodi);
    // 'file' je soubor se kterym pracujeme; v 'name'+'ext' (nesmi byt NULL) se
    // vraci ukazatel na jmeno a priponu (jen ukazatel do jmena); 'buffer' je
    // pomocny buffer (velikost minimalne MAX_PATH), do ktereho se uklada upravene
    // jmeno
    void GetBasicName(const CFileData& file, char** name, char** ext, char* buffer);

    // vraci datum a cas posledniho zapisu do souboru; v 'dateAndTimeValid' vraci
    // FALSE pokud tento datum+cas neni znamy; vraci take "prazdne hodnoty":
    // u datumu je date->Day==0, u casu je time->Hour==24
    void GetLastWriteDateAndTime(const CFileData& file, BOOL* dateAndTimeValid,
                                 CFTPDate* date, CFTPTime* time);

    // vraci kombinaci konstant VALID_DATA_PL_SIZE/VALID_DATA_PL_DATE/VALID_DATA_PL_TIME
    // podle BytesColumnOffset/DateColumnOffset/TimeColumnOffset
    DWORD GetPLValidDataMask();

    // ************************************************************************************
    // definice metod rozhrani CPluginDataInterfaceAbstract
    // ************************************************************************************

    virtual BOOL WINAPI CallReleaseForFiles() { return TRUE; }
    virtual BOOL WINAPI CallReleaseForDirs() { return TRUE; }
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir);

    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) {}          // FS neuzije
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) { return TRUE; } // FS neuzije

    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) { return NULL; }                               // nemame vlastni ikony
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) { return TRUE; }                       // nemame vlastni ikony
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) { return NULL; } // nemame vlastni ikony
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) { return 0; }         // nemame vlastni ikony

    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                  const CFileData* upperDir);
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth);
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth);

    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount);

    virtual BOOL WINAPI CanBeCopiedToClipboard() { return TRUE; }

    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size);
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date);
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time);
};

//
// ****************************************************************************
// CPluginFSInterface
//
// sada metod plug-inu, ktere potrebuje Salamander pro praci s file systemem

class CControlConnectionSocket;
class CFTPOperation;
class CFTPWorker;

// kody chyb pri komunikaci mezi ListCurrentPath a ChangePath:
enum CFTPErrorState
{
    fesOK,               // zadna chyba
    fesFatal,            // fatalni chyba pri listovani (ChangePath ma jen vratit FALSE)
    fesInaccessiblePath, // cesta nejde vylistovat, nutne zkratit cestu
};

class CPluginFSInterface : public CPluginFSInterfaceAbstract
{
protected:
    char Host[HOST_MAX_SIZE]; // hostitel (FTP server)
    int Port;                 // port, na kterem bezi FTP server
    char User[USER_MAX_SIZE]; // uzivatel
    char Path[FTP_MAX_PATH];  // aktualni cesta na FTP (remote path; pozor je case-sensitive)

    CFTPErrorState ErrorState;
    BOOL IsDetached;          // je tento FS odpojeny? (FALSE = je v panelu)
    CTopIndexMem TopIndexMem; // pamet top-indexu pro ExecuteOnFS()

    CControlConnectionSocket* ControlConnection; // socket "control connection" na FTP server (NULL == nikdy nebylo pripojeno)
    char RescuePath[FTP_MAX_PATH];               // zachrana cesta na FTP - zkusit, kdyz uz ChangePath() nebude moct zkracovat
    char HomeDir[FTP_MAX_PATH];                  // defaultni cesta na FTP - byla nastavena po loginu na server
    BOOL OverwritePathListing;                   // je-li TRUE, ma se listing v PathListing prepsat novym (az/jestli ho ziskame) - resi situaci, kdy se vola ChangePath a ListCurrentPath uz se nezavola diky tomu, ze se cesta nezmenila
    char* PathListing;                           // neni-li NULL, obsahuje aspon cast (muze byt i "") listingu aktualni cesty
    int PathListingLen;                          // delka retezce v PathListing
    CFTPDate PathListingDate;                    // datum porizeni listingu (potrebne pro "year_or_time")
    BOOL PathListingIsIncomplete;                // TRUE=PathListing obsahuje jen cast (muze byt i "") listingu; FALSE=listing je kompletni
    BOOL PathListingIsBroken;                    // TRUE=PathListing obsahuje poruseny listing (server pri ziskavani listingu vratil chybu)
    BOOL PathListingMayBeOutdated;               // TRUE=PathListing obsahuje listing, ktery jiz nemusi byt aktualni (slouzi Uploadu: pri F5/F6 v panelu se kontroluje jestli neprobiha nejaka operace, ktera by mohla listing poskodit; neresi mozne poskozeni listingu behem drag&drop operace, protoze zacatek drag&dropu neni pluginu znamy)
    DWORD PathListingStartTime;                  // IncListingCounter() z okamziku poslani prikazu "LIST" na server (pri ziskavani listingu PathListing)

    CFTPServerPathType DirLineHotPathType; // pomocna promenna: typ cesty, u ktere se zjistuji hot-texty v Directory Line
    int DirLineHotPathUserLength;          // pomocna promenna: delka aktualni username pokud obsahuje "nepovolene" znaky

    DWORD ChangePathOnlyGetCurPathTime; // pomocna promenna pro optimalizaci ChangePath() volane tesne po ziskani pracovni cesty

    // pomocna promenna pro predavani celkoveho poctu pokusu o pripojeni mezi jednotlivymi volanimi
    // ChangePath(), ListCurrentPath(), GetFullFSPath(), SendUserFTPCommand() a ViewFile()
    int TotalConnectAttemptNum;

    BOOL AutodetectSrvType;                   // TRUE = pro zjisteni typu serveru se ma pouzit autodetekce; FALSE = LastServerType obsahuje uzivatelem vybrany typ serveru (pokud prestane existovat, prepne se AutodetectSrvType na TRUE)
    char LastServerType[SERVERTYPE_MAX_SIZE]; // jmeno typu serveru (bez prip. uvodni '*'), se kterym se naposledy parsoval listing (zaruci stejne sloupce v panelu i pro prazdne adresare)

    BOOL InformAboutUnknownSrvType;   // TRUE dokud jsme usera neinformovali o tom, ze nejsme schopny najit parser listingu (nepodporovany typ serveru)
    BOOL NextRefreshCanUseOldListing; // TRUE jen pokud se jen zmenila konfigurace (parsery, sloupce) a neni tudiz nutne cist znovu listing se serveru
    BOOL NextRefreshWontClearCache;   // TRUE jen pokud doslo k refreshi na zaklade hlaseni "change on path" a neni tedy nutne cistit aktualni cestu z cache (dela se to jinde)

    int TransferMode; // rezim prenosu souboru (binar/ascii/auto) (hodnoty typu CTransferMode)

    BOOL CalledFromDisconnectDialog; // TRUE = user chce disconnectnout toto FS z Disconnect dialogu (klavesa F12)

    BOOL RefreshPanelOnActivation; // TRUE = az se aktivnuje hl. okno Salamandera, provedeme refresh

public:
    CPluginFSInterface();
    ~CPluginFSInterface();

    // vytvori user-part cesty na FTP - format "//user@host:port/path"; 'buffer' je
    // buffer pro vysledek o velikosti 'bufferSize' bytu; 'path' je cesta na FTP, je-li
    // 'path' NULL, bere se aktualni cesta na FTP; je-li 'ignorePath' TRUE, vytvori se
    // user-part cesta bez cesty na FTP; vraci TRUE pokud je buffer 'buffer'
    // dost veliky pro vysledek (jinak je vysledek oriznuty a vraci se FALSE)
    BOOL MakeUserPart(char* buffer, int bufferSize, char* path = NULL, BOOL ignorePath = FALSE);

    // ulozi data connectiony (host+port+user+passwd+...) do Config.FTPServerList
    // user vybira jmeno bookmarky v dialogu
    void AddBookmark(HWND parent);

    // posle FTP prikaz na server (user zada prikaz v dialogu);
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je
    // zaroven parent pripadnych messageboxu s chybami
    void SendUserFTPCommand(HWND parent);

    // ukaze raw listing (text ze serveru) pro aktualni cestu v panelu; 'parent' je parent
    // dialogu s listingem
    void ShowRawListing(HWND parent);

    // test jestli pouzivame "control connection" 'ctrlCon'
    BOOL Contains(const CControlConnectionSocket* ctrlCon) const { return ctrlCon == ControlConnection; }

    // pouziva se jen pro detekci umisteni FS (levy/pravy panel nebo odpojeny FS);
    // na vraceny ukazatel se tedy nepristupuje (jen se porovnava hodnota)
    CControlConnectionSocket* GetControlConnection() { return ControlConnection; }

    // vraci UID logu z "control connectiony" nebo -1 pokud zadny neexistuje
    int GetLogUID();

    // pokud se jeste user nedozvedel o zavreni "control connection", dozvi se to
    // v teto metode; 'parent' je parent pripadnych messageboxu
    void CheckCtrlConClose(HWND parent);

    // zjisti typ cesty na FTP serveru (jen vola ControlConnection->GetFTPServerPathType())
    CFTPServerPathType GetFTPServerPathType(const char* path);

    // je-li zavrena "control connection", nabidne uzivateli jeji nove otevreni
    // (POZOR: nenastavuje pracovni adresar na serveru); vraci TRUE pokud je
    // "control connection" pripravena pro pouziti, zaroven v tomto pripade nastavuje
    // SetStartTime() (aby dalsi cekani uzivatele mohlo navazat na pripadny reconnect);
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je
    // zaroven parent pripadnych messageboxu s chybami; v 'reconnected' (neni-li NULL) vraci
    // TRUE pokud doslo k obnoveni spojeni (byla znovu otevrena "control connection")
    // je-li 'setStartTimeIfConnected' FALSE a neni-li treba spojeni obnovovat, nenastavuje
    // se SetStartTime();'totalAttemptNum'+'retryMsg' jsou parametry pro StartControlConnection();
    // mozne volat jen z hl. threadu (zatim pouziva wait-okenka, atd.)
    BOOL ReconnectIfNeeded(HWND parent, BOOL* reconnected, BOOL setStartTimeIfConnected,
                           int* totalAttemptNum, const char* retryMsg);

    // pomocna metoda pro ParseListing; jen prida up-dir ("..") do 'dir'
    void AddUpdir(BOOL& err, BOOL& needUpDir, CFTPListingPluginDataInterface* dataIface,
                  TIndirectArray<CSrvTypeColumn>* columns, CSalamanderDirectoryAbstract* dir);

    // zkusi parsovani aktualniho listingu (ulozeneho v PathListing) parserem z 'serverType'
    // (nemuze byt NULL); 'isVMS' je TRUE pokud jde o listing z VMS; pokud se parsovani povede,
    // vraci TRUE a v 'dir'+'pluginData' (nejsou-li NULL) vraci alokovana data listingu;
    // neni-li 'findName' NULL, hleda v listingu toto jmeno ('caseSensitive' je TRUE/FALSE podle
    // toho jestli se ma pouzit strcmp/StrICmp) a vraci jestli existuje jako soubor (v 'fileExists'
    // vraci TRUE) nebo jako adresar (v 'dirExists' vraci TRUE); je-li 'findName' NULL, parametry
    // 'caseSensitive'+'fileExists'+'dirExists' se ignoruji; pri chybe vraci FALSE, jde-li o chybu
    // nedostatku pameti vraci TRUE v 'lowMem' (nesmi byt NULL)
    BOOL ParseListing(CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract** pluginData,
                      CServerType* serverType, BOOL* lowMem, BOOL isVMS, const char* findName,
                      BOOL caseSensitive, BOOL* fileExists, BOOL* dirExists);

    void SetNextRefreshCanUseOldListing(BOOL b) { NextRefreshCanUseOldListing = b; }

    // nastavi "check-marky" podle aktualni hodnoty TransferMode do x-teho
    // ('submenuNumber'-teho) submenu menu 'menu'
    void SetTransferModeCheckMarksInSubMenu(HMENU menu, int submenuNumber);

    // nastavi TransferMode podle toho, jaky prikaz ('cmd') v submenu "Transfer Mode"
    // kontextoveho menu si user vybral
    void SetTransferModeByMenuCmd(int cmd);

    // nastavi TransferMode podle toho, jaky prikaz ('id') v submenu "Transfer Mode"
    // pluginoveho menu si user vybral
    void SetTransferModeByMenuCmd2(int id);

    // vraci stav prikazu ('id') v submenu "Transfer Mode" pluginoveho menu podle
    // aktualni hodnoty TransferMode
    DWORD GetTransferModeCmdState(int id);

    // nastavi "check-mark" polozce "List Hidden Files (Unix)" podle aktualni hodnoty
    // list-commandu do menu 'menu'
    void SetListHiddenFilesCheckInMenu(HMENU menu);

    // enabluje polozku "Show Certificate" podle toho, jestli jde o FTPS spojeni nebo ne
    void SetShowCertStateInMenu(HMENU menu);

    // vola se kdyz user klikne v menu na List Hidden Files (Unix) - pokud se
    // pouziva "LIST -a", pouzije se puvodni prikaz pro listovani nebo "LIST";
    // navic zobrazuje messagebox s hintem jak nastavit List Command v bookmarkach
    void ToggleListHiddenFiles(HWND parent);

    // jen vola IsListCommandLIST_a() control-connectiony
    BOOL IsListCommandLIST_a();

    // otevre okno s progresem operace a spusti ji v aktivni "control connection"
    // (polozky zpracovava ve workerovi vytvorenem na zaklade aktivni control-connectiony
    // v panelu (pri volani je zaruceno, ze ControlConnection neni NULL));
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); vraci TRUE
    // pri uspesnem spusteni operace (jinak jde o chybu a operace se zrusi)
    BOOL RunOperation(HWND parent, int operUID, CFTPOperation* oper, HWND dropTargetWnd);

    // ma-li "control connection" UID 'controlConUID', vraci TRUE
    BOOL ContainsConWithUID(int controlConUID);

    // jen presmeruje volani do ControlConnection->GetConnectionFromWorker
    void GetConnectionFromWorker(CFTPWorker* workerWithCon);

    // jen overi ControlConnection!=NULL a zavola ControlConnection->ActivateWelcomeMsg()
    // mozne volat jen v hl. threadu
    void ActivateWelcomeMsg();

    // jen zjisti, jestli je ControlConnection->GetEncryptControlConnection() == 1
    BOOL IsFTPS();

    // projde ostatni otevrene FS a pokud jde o prvni, ktery se pripojuje na 'host'+'port'
    // jako 'user', smazne vsechny listingy pro 'host'+'port'+'user' z cache listingu
    void ClearHostFromListingCacheIfFirstCon(const char* host, int port, const char* user);

    // zjisti, jestli se toto FS pripojuje na 'host'+'port' jako 'user'
    BOOL ContainsHost(const char* host, int port, const char* user);

    // metody zdedene od CPluginFSInterfaceAbstract:
    virtual BOOL WINAPI GetCurrentPath(char* userPart);
    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize);
    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success);
    virtual BOOL WINAPI GetRootPath(char* userPart);

    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart);

    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                   const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                   BOOL forceRefresh, int mode);
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                        CPluginDataInterfaceAbstract*& pluginData,
                                        int& iconsType, BOOL forceRefresh);

    virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason);

    virtual void WINAPI Event(int event, DWORD param);

    virtual void WINAPI ReleaseObject(HWND parent);

    virtual DWORD WINAPI GetSupportedServices();

    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon);
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon);
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect) {}
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue) { *retValue = CQuadWord(0, 0); }
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset);
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize);
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize);
    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent) {}
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo) { return FALSE; }
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                    char* newName, BOOL& cancel);
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs);
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel);
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file);
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError);
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget);
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel);
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs);
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs) {}
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs);
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel) { return FALSE; }
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent) {}
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) {}
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) { return FALSE; }
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) { return FALSE; }
    virtual void WINAPI ShowSecurityInfo(HWND parent);

    friend CPluginInterfaceForFS;
};

// pole se vsemi otevrenymi FS (pouzivat jen z hl. threadu - nesynchronizovane)
extern TIndirectArray<CPluginFSInterface> FTPConnections;

// data interface pro raw listing v panelu
extern CSimpleListPluginDataInterface SimpleListPluginDataInterface;
