// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ***************************************************************************
// funkce:

// pomocna funkce pro prevod retezce ze streamu na null-terminated retezec
// 'buf'+'bufSize' je vystupni buffer; 'txt'+'size' je vstupni retezec
// vraci 'buf'
char* CopyStr(char* buf, int bufSize, const char* txt, int size);

// pomocna funkce pro rozklad retezce s init-ftp-prikazy (oddelene ';') na
// jednotlive prikazy; vraci TRUE pokud je k dispozici dalsi prikaz (prikaz
// vraci v 's'); 'next' je IN/OUT promenna, inicializuje se na zacatek retezce
// a mezi volanimi GetToken se nesmi menit
BOOL GetToken(char** s, char** next);

// kody prikazu pro PrepareFTPCommand (v [] jsou parametry do vypustky)
enum CFtpCmdCode
{
    ftpcmdQuit,              // [] - logout from FTP server
    ftpcmdSystem,            // [] - zjisti operacni system na serveru (muze byt jen simulace)
    ftpcmdAbort,             // [] - abort prave provadeneho prikazu
    ftpcmdPrintWorkingPath,  // [] - zjisti pracovni (aktualni) adresar na FTP serveru
    ftpcmdChangeWorkingPath, // [char *path] - zmena pracovniho adresare na FTP serveru
    ftpcmdSetTransferMode,   // [BOOL ascii] - nastaveni prenosoveho rezimu (ASCII/BINAR(IMAGE))
    ftpcmdPassive,           // [] - zada server, aby pouzil pro datove spojeni "listen" (klient navazuje datove spojeni)
    ftpcmdSetPort,           // [DWORD IP, unsigned short port] - nastavi na serveru ip+port pro datove spojeni
    ftpcmdNoOperation,       // [] - keep-alive command "no operation"
    ftpcmdDeleteFile,        // [char *filename] - smazani souboru 'filename'
    ftpcmdDeleteDir,         // [char *dirname] - smazani adresare 'dirname'
    ftpcmdChangeAttrs,       // [int newAttr, char *name] - zmena atributu (modu) souboru/adresare 'name' na 'newAttr'
    ftpcmdChangeAttrsQuoted, // [int newAttr, char *nameToQuotes (pred vsechny znaky '"' ve jmene je nutne vlozit '\\')] - zmena atributu (modu) souboru/adresare 'name' na 'newAttr' - jmeno souboru/adresare je v uvozovkach (Linux FTP to vyzaduje pro jmena s mezerama)
    ftpcmdRestartTransfer,   // [char *number] - prikaz REST (resume / restart-transfer)
    ftpcmdRetrieveFile,      // [char *filename] - download souboru 'filename'
    ftpcmdCreateDir,         // [char *path] - vytvoreni adresare 'path'
    ftpcmdRenameFrom,        // [char *fromName] - zahajeni prejmenovani ("rename from")
    ftpcmdRenameTo,          // [char *newName] - ukonceni prejmenovani ("rename to")
    ftpcmdStoreFile,         // [char *filename] - upload souboru 'filename'
    ftpcmdGetSize,           // [char *filename] - zjisteni velikosti souboru (muze byt i link na soubor)
    ftpcmdAppendFile,        // [char *filename] - upload: append souboru 'filename'
};

// pripravi text prikazu pro FTP server (vcetne CRLF na konci), vraci TRUE
// pokud se prikaz vesel do bufferu 'buf' (o velikosti 'bufSize'); 'ftpCmd'
// je kod prikazu (viz CFtpCmdCode); v 'cmdLen' (neni-li NULL) vraci delku
// pripraveneho textu prikazu (je vzdy ukoncen nulou na konci);
// 'logBuf' o delce 'logBufSize' bude obsahovat verzi prikazu vhodnou pro
// log-file (misto hesel hvezdicky, atd.) - jde o null-terminated string
BOOL PrepareFTPCommand(char* buf, int bufSize, char* logBuf, int logBufSize,
                       CFtpCmdCode ftpCmd, int* cmdLen, ...);

// pomocna funkce pro pripravu textu chyb
const char* GetFatalErrorTxt(int fatalErrorTextID, char* errBuf);
// pomocna funkce pro pripravu textu chyb
const char* GetOperationFatalErrorTxt(int opFatalError, char* errBuf);

// ****************************************************************************
// makra:

// makra pro vytazeni cislic z FTP reply code (ocekava se int v rozsahu 0-999)
#define FTP_DIGIT_1(n) ((n) / 100)       // 1. cislice
#define FTP_DIGIT_2(n) (((n) / 10) % 10) // 2. cislice
#define FTP_DIGIT_3(n) ((n) % 10)        // 3. cislice

// ****************************************************************************
// konstanty:

#define WAITWND_STARTCON 2000   // start control connection: ukazat wait-wnd za 2 sekundy
#define WAITWND_CLOSECON 2000   // close control connection: ukazat wait-wnd za 2 sekundy
#define WAITWND_COMOPER 2000    // bezna operace (poslani prikazu): ukazat wait-wnd za 2 sekundy
#define WAITWND_PARSINGLST 2000 // parsovani listingu: ukazat wait-wnd za 2 sekundy
#define WAITWND_CONTOOPER 2000  // predani aktivni "control connection" do workera operace: ukazat wait-wnd za 2 sekundy
#define WAITWND_CLWORKCON 250   // close control connections of workers in operations: ukazat wait-wnd za ctvrt vteriny (puvodne bylo za 2 sekundy - useri zkouseli mackat Add a dalsi buttony, coz nejde, ale neni to visualizovane - pri otevrenem wait-okenku je jasne, proc to nejde)
#define WAITWND_CLOPERDLGS 2000 // close all operation dialogs (when plugin is unloaded): ukazat wait-wnd za 2 sekundy

#define CRTLCON_BYTESTOWRITEONSOCKETPREALLOC 512 // kolik bytu se ma predalokovat pro zapis (aby dalsi zapis zbytecne nealokoval treba kvuli 1 bytovemu presahu)
#define CRTLCON_BYTESTOREADONSOCKET 1024         // po minimalne kolika bytech se ma cist socket (take alokovat buffer pro prectena data)
#define CRTLCON_BYTESTOREADONSOCKETPREALLOC 512  // kolik bytu se ma predalokovat pro cteni (aby dalsi cteni okamzite nealokovalo znovu)

// kody odpovedi pro 1. cislici FTP reply code
#define FTP_D1_MAYBESUCCESS 1   // mozna uspech (klient musi cekat na dalsi reply)
#define FTP_D1_SUCCESS 2        // uspech (klient muze poslat dalsi prikaz)
#define FTP_D1_PARTIALSUCCESS 3 // castecny uspech (klient musi poslat dalsi prikaz v sekvenci)
#define FTP_D1_TRANSIENTERROR 4 // docasna chyba (neuspech, ale je sance aby stejny prikaz priste uspel)
#define FTP_D1_ERROR 5          // chyba (prikaz nema cenu opakovat, je ho treba zmenit)

// kody odpovedi pro 2. cislici FTP reply code
#define FTP_D2_SYNTAX 0         // u chyb jde o syntaxi, jinak funkcne jinam nezarazene prikazy
#define FTP_D2_INFORMATION 1    // informace (status nebo help, atd.)
#define FTP_D2_CONNECTION 2     // tyka se pripojeni
#define FTP_D2_AUTHENTICATION 3 // tyka se prihlasovani/autentifikace/uctovani
#define FTP_D2_UNKNOWN 4        // not specified
#define FTP_D2_FILESYSTEM 5     // file system action

#define CTRLCON_KEEPALIVE_TIMERID 1 // CControlConnectionSocket: ID timeru pro keep-alive

#define CTRLCON_KAPOSTSETUPNEXT 2 // CControlConnectionSocket: ID postnute zpravy pro provedeni SetupNextKeepAliveTimer()
#define CTRLCON_LISTENFORCON 3    // CControlConnectionSocket: ID postnute zpravy o otevreni portu pro "listen" (na proxy serveru)
#define CTRLCON_KALISTENFORCON 4  // CControlConnectionSocket: keep-alive: ID postnute zpravy o otevreni portu pro "listen" (na proxy serveru)

//
// ****************************************************************************
// CDynString
//
// pomocny objekt pro dynamicky alokovany string

struct CDynString
{
    char* Buffer;
    int Length;
    int Allocated;

    CDynString()
    {
        Buffer = NULL;
        Length = 0;
        Allocated = 0;
    }

    ~CDynString()
    {
        if (Buffer != NULL)
            free(Buffer);
    }

    void Clear()
    {
        Length = 0;
        if (Buffer != NULL)
            Buffer[0] = 0;
    }

    BOOL Append(const char* str, int len); // vraci TRUE pro uspechu; je-li 'len' -1, bere se "len=strlen(str)"

    // preskoci minimalne 'len' znaku, preskakuje po celych radcich; pocty preskocenych znaku/radek
    // pricita do 'skippedChars'/'skippedLines'
    void SkipBeginning(DWORD len, int* skippedChars, int* skippedLines);

    const char* GetString() const { return Buffer; }
};

//
// ****************************************************************************
// CLogs
//
// sprava logu pro vsechna pripojeni (control connection v panelech a workeri v operacich)

class CControlConnectionSocket;

class CLogData
{
protected:
    static int NextLogUID;          // globalni pocitadlo pro objekty logu
    static int OldestDisconnectNum; // disconnect-cislo nejstarsiho logu serveru, ktery se disconnectnul
    static int NextDisconnectNum;   // disconnect-cislo pro dalsi log serveru, ktery se disconnectne

    // identifikace logu:
    int UID;             // unikatni cislo logu (hodnota -1 je vyhrazena pro "invalid UID")
    char* Host;          // adresa serveru
    unsigned short Port; // pouzivany port na serveru
    char* User;          // uzivatelovo jmeno

    BOOL CtrlConOrWorker; // TRUE/FALSE = logujeme "control connection" z panelu / workera
    BOOL WorkerIsAlive;   // TRUE/FALSE = worker existuje / uz neexistuje
    // "control connection", kterou logujeme (NULL == FS jiz neexistuje);
    // POZOR: nesmime sahat do objektu "control connection" - zakazane vnoreni kritickych sekci
    CControlConnectionSocket* CtrlCon;
    BOOL Connected;    // TRUE/FALSE == aktivni/neaktivni "control connection" (panel i worker)
    int DisconnectNum; // je-li (CtrlCon==NULL && !WorkerIsAlive), je zde cislo udavajici stari mrtveho logu (abychom rusili vzdy od nejdele mrtveho logu)

    CDynString Text;  // samotny text logu
    int SkippedChars; // pocet skipnutych znaku od posledniho vypisu do edit-okna v Logs
    int SkippedLines; // pocet skipnutych radek od posledniho vypisu do edit-okna v Logs

protected:
    CLogData(const char* host, unsigned short port, const char* user,
             CControlConnectionSocket* ctrlCon, BOOL connected, BOOL isWorker);
    ~CLogData();

    BOOL IsGood() { return Host != NULL && User != NULL; }

    // zmena usera, vraci TRUE pri uspechu (pri neuspechu necha v User prazdny retezec)
    BOOL ChangeUser(const char* user);

    friend class CLogs;
    friend TIndirectArray<CLogData>;
};

class CLogsDlg;

class CLogs
{
protected:
    CRITICAL_SECTION LogCritSect; // kriticka sekce objektu (POZOR: uvnitr teto sekce je zakazany vstup do jinych sekci)

    TIndirectArray<CLogData> Data; // pole vsech logu
    int LastUID;                   // jednomistna vyhledavaci cache - je-li LastUID==UID, zkusi pouzit LastIndex
    int LastIndex;                 // pomocny udaj pro vyhledavaci cache (hledani logu podle UID v Data)

    CLogsDlg* LogsDlg; // obsahuje ukazatel na otevreny (NULL = zavreny) Logs dialog
    HANDLE LogsThread; // handle threadu, ve kterem bezel/bezi naposledy otevreny Logs dialog

public:
    CLogs() : Data(5, 5)
    {
        HANDLES(InitializeCriticalSection(&LogCritSect));
        LastUID = -1;
        LastIndex = -1;
        LogsDlg = NULL;
        LogsThread = NULL;
    }

    ~CLogs() { HANDLES(DeleteCriticalSection(&LogCritSect)); }

    // vytvori novy log, vraci TRUE pri uspechu + v 'uid' (nesmi byt NULL) UID noveho logu;
    // POZOR: nebere ohled na Config.EnableLogging
    BOOL CreateLog(int* uid, const char* host, unsigned short port, const char* user,
                   CControlConnectionSocket* ctrlCon, BOOL connected, BOOL isWorker);

    // nastavi v logu s UID=='uid' CLogData::Connected na 'isConnected'
    void SetIsConnected(int uid, BOOL isConnected);

    // oznaci log s UID=='uid' jako log zavreneho spojeni; pouziva se pro oznameni zavreni spojeni
    // na FTP server; vraci TRUE pri uspechu (UID nalezeno)
    BOOL ClosingConnection(int uid);

    // zmena jmena uzivatele (behem pripojovani muze dojit k teto zmene); vraci TRUE pri uspechu
    BOOL ChangeUser(int uid, const char* user);

    // prida text 'str' (delka 'len') do logu s UID=='uid'; je-li 'uid' -1 ("invalid UID"), nema se
    // nic logovat; je-li 'len' -1, bere se "len=strlen(str)"; je-li 'addTimeToLog' TRUE, umisti
    // se pred hlasku aktualni cas; vraci TRUE pri uspesnem pridani do logu nebo je-li 'uid' -1
    BOOL LogMessage(int uid, const char* str, int len, BOOL addTimeToLog = FALSE);

    // vraci TRUE pokud je log s UID=='uid' v poli logu (tedy log lze prohlizet v dialogu Logs)
    BOOL HasLogWithUID(int uid);

    // vola se po zmene parametru logovani v konfiguraci
    void ConfigChanged();

    // prida logy do combo-boxu; vraci fokus v 'focusIndex' (nesmi byt NULL) podle
    // 'prevItemUID' (UID polozky, kterou chceme vybrat, pripadne muze byt i -1),
    // pokud 'prevItemUID' nenajde, vraci v 'focusIndex' -1; v 'empty' (nesmi byt NULL) vraci
    // TRUE pokud neexistuji zadne logy
    void AddLogsToCombo(HWND combo, int prevItemUID, int* focusIndex, BOOL* empty);

    // nastavi text logu s UID 'logUID' do editu 'edit'; je-li 'update' TRUE, jde o update textu
    void SetLogToEdit(HWND edit, int logUID, BOOL update);

    // nastavi hodnotu 'LogsDlg' (v kriticke sekci)
    void SetLogsDlg(CLogsDlg* logsDlg);

    // vrati TRUE, pokud je sance, ze se bude aktivovat Logs dialog (ve svem threadu), vraci
    // FALSE pri chybe; neni-li 'showLogUID' -1, jde o UID logu, ktery se ma aktivovat;
    BOOL ActivateLogsDlg(int showLogUID);

    // pokud je otevreny Logs dialog, aktivuje v nem log s UID 'showLogUID'
    void ActivateLog(int showLogUID);

    // je-li otevreny Logs dialog, zavre ho a sekundu pocka na ukonceni jeho threadu
    // (pokud se nedocka, nic se nedeje)
    void CloseLogsDlg();

    // ulozi aktualni pozici dialogu Logs do konfigurace
    void SaveLogsDlgPos();

    // refreshne seznam logu v dialogu Logs (existuje-li)
    void RefreshListOfLogsInLogsDlg();

    // ulozi do souboru (necha usera vybrat) log; 'itemName' je jmeno logu; 'uid' je UID logu
    void SaveLog(HWND parent, const char* itemName, int uid);

    // nakopiruje na clipboard log; 'itemName' je jmeno logu; 'uid' je UID logu
    void CopyLog(HWND parent, const char* itemName, int uid);

    // vyprazdni text logu; 'itemName' je jmeno logu; 'uid' je UID logu
    void ClearLog(HWND parent, const char* itemName, int uid);

    // odstrani log; 'itemName' je jmeno logu; 'uid' je UID logu
    void RemoveLog(HWND parent, const char* itemName, int uid);

    // ulozi do souboru (necha usera vybrat) vsechny logy
    void SaveAllLogs(HWND parent);

    // nakopiruje na clipboard vsechny logy
    void CopyAllLogs(HWND parent);

    // odstrani vsechny logy
    void RemoveAllLogs(HWND parent);

protected:
    // zrusi pripadne nadbytecne stare logy;
    // POZOR: musi se volat z kriticke sekce
    void LimitClosedConLogs();

    // v 'index' vraci index logu s UID 'uid'; pokud neni log nalezen vraci FALSE (jinak TRUE)
    // POZOR: musi se volat z kriticke sekce
    BOOL GetLogIndex(int uid, int* index);
};

extern CLogs Logs; // logy vsech pripojeni na FTP servery

//
// ****************************************************************************
// CListingCache
//
// cache listingu na FTP serverech - pouziva se pro zmenu cesty a listovani cesty
// bez pristupu na server

struct CListingCacheItem
{
public:
    // parametry pripojeni na server:
    char* Host;          // host-address (nesmi byt NULL)
    unsigned short Port; // port na kterem bezi FTP server
    char* User;          // user-name, NULL==anonymous
    int UserLength;      // jen optimalizacni promenna: delka 'User' pokud jsou v nem "zakazane" znaky (je-li 'User==NULL', je zde nula)

    char* Path;                  // cachovana cesta (lokalni na serveru)
    CFTPServerPathType PathType; // typ cachovane cesty

    char* ListCmd; // prikaz, kterym byl listing ziskan (jiny prikaz = muze byt jiny listing)
    BOOL IsFTPS;   // TRUE = jde o FTPS, FALSE = jde o FTP

    char* CachedListing;          // listing cachovane cesty
    int CachedListingLen;         // delka listingu cachovane cesty
    CFTPDate CachedListingDate;   // datum, kdy byl listing porizen (potrebne pro spravne vyhodnoceni "year_or_time")
    DWORD CachedListingStartTime; // IncListingCounter() z okamziku, kdy byl poslan prikaz "LIST" ke ziskani tohoto listingu

    CListingCacheItem(const char* host, unsigned short port, const char* user, const char* path,
                      const char* listCmd, BOOL isFTPS, const char* cachedListing, int cachedListingLen,
                      const CFTPDate& cachedListingDate, DWORD cachedListingStartTime,
                      CFTPServerPathType pathType);
    ~CListingCacheItem();

    BOOL IsGood() { return Host != NULL; }
};

class CListingCache
{
protected:
    CRITICAL_SECTION CacheCritSect;          // kriticka sekce objektu
    TIndirectArray<CListingCacheItem> Cache; // pole cachovanych listingu cest
    CQuadWord TotalCacheSize;                // celkova velikost polozek v cache (casem bude na disku, proto quad-word)

public:
    CListingCache();
    ~CListingCache();

    // vraci TRUE pokud je k dispozici pouzitelny listing cesty 'path' (typu 'pathType')
    // na serveru 'host', kde je uzivatel 'user' pripojen na portu 'port'; listing se
    // vraci v alokovanem retezci 'cachedListing' (nesmi byt NULL, pokud vraci NULL,
    // jde o chybu alokace pameti), delka retezce se vraci v 'cachedListingLen' (nesmi
    // byt NULL), volajici je odpovedny za dealokaci; datum porizeni listingu se vraci
    // v 'cachedListingDate' (nesmi byt NULL); v 'path' se vraci presne zneni cachovane cesty
    // (jak ji vratil server v okamziku vlozeni do cache), 'path' je buffer o velikosti
    // 'pathBufSize' bytu
    // mozne volat z libovolneho threadu
    BOOL GetPathListing(const char* host, unsigned short port, const char* user,
                        CFTPServerPathType pathType, char* path, int pathBufSize,
                        const char* listCmd, BOOL isFTPS, char** cachedListing,
                        int* cachedListingLen, CFTPDate* cachedListingDate,
                        DWORD* cachedListingStartTime);

    // prida nebo obnovi (prepise) listing cesty 'path' (typu 'pathType') na serveru
    // 'host', kde je uzivatel 'user' pripojen na portu 'port'; listing je v retezci
    // 'cachedListing' (nesmi byt NULL), delka retezce je v 'cachedListingLen';
    // datum porizeni listingu je v 'cachedListingDate' (nesmi byt NULL);
    // mozne volat z libovolneho threadu
    void AddOrUpdatePathListing(const char* host, unsigned short port, const char* user,
                                CFTPServerPathType pathType, const char* path,
                                const char* listCmd, BOOL isFTPS,
                                const char* cachedListing, int cachedListingLen,
                                const CFTPDate* cachedListingDate,
                                DWORD cachedListingStartTime);

    // hlasi do cache, ze user dal tvrdy refresh na ceste 'path' (typu 'pathType')
    // na serveru 'host', kde je uzivatel 'user' pripojen na portu 'port'; znamena
    // to projev neduvery v aktualnost cache, smazeme tedy z cache tuto cestu vcetne
    // jejich podcest; je-li 'ignorePath' TRUE, smazeme z cache listingy vsech cest
    // ze serveru 'host', kde je uzivatel 'user' pripojen na portu 'port'
    // mozne volat z libovolneho threadu
    void RefreshOnPath(const char* host, unsigned short port, const char* user,
                       CFTPServerPathType pathType, const char* path, BOOL ignorePath = FALSE);

    // hlasi do cache, ze doslo ke zmene na ceste 'userPart' (format cesty FS user-part);
    // je-li 'includingSubdirs' TRUE, tak zahrnuje i zmenu v podadresarich cesty 'userPart';
    // smazeme z cache zmenene cesty (aby se priste zase nacetly ze serveru)
    void AcceptChangeOnPathNotification(const char* userPart, BOOL includingSubdirs);

protected:
    // hleda polozku v cache; pokud polozku najde, vraci TRUE a jeji index v 'index'
    // (nesmi byt NULL); vraci FALSE pokud polozka v cache neexistuje;
    // POZOR: volat jen z kriticke sekce CacheCritSect
    BOOL Find(const char* host, unsigned short port, const char* user,
              CFTPServerPathType pathType, const char* path, const char* listCmd,
              BOOL isFTPS, int* index);
};

//
// ****************************************************************************
// CControlConnectionSocket
//
// objekt socketu, ktery slouzi jako "control connection" na FTP server
// pro dealokaci pouzivat funkci ::DeleteSocket!

// kody udalosti pro CControlConnectionSocket::WaitForEventOrESC
enum CControlConnectionSocketEvent
{
    ccsevESC,               // jen navratova hodnota z WaitForEventOrESC
    ccsevTimeout,           // jen navratova hodnota z WaitForEventOrESC
    ccsevWriteDone,         // dokonceni zapisu bufferu (viz metoda Write)
    ccsevNewBytesRead,      // precteni dalsiho bloku dat do bufferu socketu (viz metoda ReadFTPReply)
    ccsevClosed,            // socket byl uzavren
    ccsevIPReceived,        // dostali jsme IP
    ccsevConnected,         // otevreni spojeni na server
    ccsevUserIfaceFinished, // user-iface hlasi dokonceni prace (zavreni "data connection" nebo dokonceni "keep alive" prikazu)
    ccsevListenForCon,      // uspesne nebo neuspesne otevreni "listen" portu na proxy serveru
};

struct CControlConnectionSocketEventData
{
    CControlConnectionSocketEvent Event; // cislo udalosti (viz ccsevXXX)
    DWORD Data1;
    DWORD Data2;

    // popis pouziti Data1 a Data2 pro jednotlive udalosti:
    //
    // udalost ccsevWriteDone:
    //   Data1 - je-li NO_ERROR, zapis byl uspesne dokoncen; jinak obsahuje kod Windows chyby
    //
    // udalost ccsevNewBytesRead:
    //   Data1 - je-li NO_ERROR, nacetly se do bufferu 'ReadBytes' nove byty; jinak obsahuje
    //           kod Windows chyby
    //
    // udalost ccsevClosed:
    //   Data1 - je-li NO_ERROR, socket byl zavren "gracefully"; jinak byl zavren "abortively"
    //
    // udalost ccsevIPReceived:
    //   Data1 - obsahuje parametr 'ip' (z volani ReceiveHostByAddress)
    //   Data2 - obsahuje parametr 'error' (z volani ReceiveHostByAddress)
    //
    // udalost ccsevConnected:
    //   Data1 - chybovy kod z FD_CONNECT (NO_ERROR = uspesne pripojeno)
    //
    // udalost ccsevUserIfaceFinished:
    //   nevyuziva ani Data1, ani Data2
    //
    // udalost ccsevListenForCon:
    //   Data1 - UID data connectiony, ktera se ozyva
};

// kody pro prenosovy rezim v otevrene "control connection" (server si pamatuje posledni nastaveni)
enum CCurrentTransferMode
{
    ctrmUnknown, // neznamy - zatim jsme ho nenastavovali nebo ho mohl user zmenit
    ctrmBinary,  // binary mod (image mod)
    ctrmASCII    // ascii mod
};

// pomocne rozhrani pro CControlConnectionSocket::SendFTPCommand() - resi ruzna
// uzivatelska rozhrani pri posilani FTP prikazu (napr. PWD a LIST se dost lisi)
class CSendCmdUserIfaceAbstract
{
public:
    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText) = 0;
    virtual void BeforeAborting() = 0;
    virtual void AfterWrite(BOOL aborting, DWORD showTime) = 0;
    virtual BOOL GetWindowClosePressed() = 0;
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort) = 0;
    virtual void SendingFinished() = 0;
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize) = 0;
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize) = 0; // FTP reply code: 1xx
    virtual void CancelDataCon() = 0;

    // sada metod pro cekani na zavreni user-ifacu ("data connection") v pripade uspesneho
    // dokonceni prikazu na serveru
    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout) = 0;       // pokud vrati FALSE, bude se cekat na event ziskany pres GetFinishedEvent()
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout) = 0; // vola se po prvnim CanFinishSending(), ktere vrati FALSE
    virtual void HandleDataConTimeout(DWORD* start) = 0;                      // vola se pouze tehdy, kdyz BeforeWaitingForFinish vrati v 'useTimeout' TRUE
    virtual HANDLE GetFinishedEvent() = 0;                                    // az bude "signaled", testne se znovu CanFinishSending()
    virtual void HandleESCWhenWaitingForFinish(HWND parent) = 0;              // user dal ESC pri cekani (po teto metode se testne znovu CanFinishSending())
};

class CSendCmdUserIfaceForListAndDownload : public CSendCmdUserIfaceAbstract
{
protected:
    BOOL ForDownload;     // vyuziti objektu pro: TRUE = download, FALSE = list
    BOOL DatConCancelled; // TRUE = data-connection byla zcancelovana (bud nedoslo k jejimu otevreni (acceptu) nebo zavreni po prijmu chyby ze serveru)

    CListWaitWindow WaitWnd;

    CDataConnectionSocket* DataConnection;
    BOOL AlreadyAborted;
    int LogUID;

public:
    CSendCmdUserIfaceForListAndDownload(BOOL forDownload, HWND parent,
                                        CDataConnectionSocket* dataConnection,
                                        int logUID)
        : WaitWnd(parent, dataConnection, &AlreadyAborted)
    {
        ForDownload = forDownload;
        DatConCancelled = FALSE;
        DataConnection = dataConnection;
        AlreadyAborted = FALSE;
        LogUID = logUID;
    }

    BOOL WasAborted() { return AlreadyAborted; }
    BOOL HadError();
    void GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr, BOOL* noDataTrTimeout,
                  int* sslErrorOccured, BOOL* decomprErrorOccured);
    BOOL GetDatConCancelled() { return DatConCancelled; }

    void InitWnd(const char* fileName, const char* host, const char* path,
                 CFTPServerPathType pathType);
    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText) {}
    virtual void BeforeAborting() { WaitWnd.SetText(LoadStr(IDS_ABORTINGCOMMAND)); }
    virtual void AfterWrite(BOOL aborting, DWORD showTime);
    virtual BOOL GetWindowClosePressed() { return WaitWnd.GetWindowClosePressed(); }
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort);
    virtual void SendingFinished();
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize);
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize);
    virtual void CancelDataCon();

    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout);
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout);
    virtual void HandleDataConTimeout(DWORD* start);
    virtual HANDLE GetFinishedEvent();
    virtual void HandleESCWhenWaitingForFinish(HWND parent);
};

// stavy provadeni keep-alive v "control connection"
enum CKeepAliveMode
{
    kamNone,                      // keep-alive se neresi (neni navazane spojeni, je zakazany, atd.)
    kamWaiting,                   // ceka se az ubehne keep-alive doba (pred posilanim keep-alive prikazu)
    kamProcessing,                // prave se provadi keep-alive prikazy
    kamWaitingForEndOfProcessing, // prave se provadi keep-alive prikazy + normalni prikazy uz cekaji na dokonceni keep-alive prikazu
    kamForbidden,                 // prave probiha normalni prikaz, keep-alive je az do jeho dobehnuti zakazany
};

enum CKeepAliveDataConState
{
    kadcsNone,                // nenastaveno (zatim se "data connection" nestartuje)
    kadcsWaitForPassiveReply, // passive: ceka na odpoved na prikaz PASV (ip+port, kam se ma otevrit socket "data connection")
    kadcsWaitForListen,       // active: ceka na otevreni listen portu (lokalne nebo na proxy serveru) ("listen" rezim "data connection")
    kadcsWaitForSetPortReply, // active: ceka na odpoved na prikaz PORT ("listen" rezim "data connection")
    kadcsWaitForListStart,    // active+passive: prikaz "list" byl odeslan, ceka na spojeni ze serveru (connect "data connection")
    kadcsDone,                // active+passive: server uz ohlasil konec listingu
};

class CWaitWindow;
class CKeepAliveDataConSocket;
class CFTPOperation;
class CFTPWorker;

class CControlConnectionSocket : public CSocket
{
private:
    // data tykajici se udalosti (pracovat s nimi jen pres AddEvent a GetEvent):
    // udalosti generuje "receive" cast (v "sockets" threadu spoustene "receive" metody), udalosti
    // prijima ridici cast (cekani na ESC/timeout nebo dokonceni prace se socketem v hl. threadu)
    // POZNAMKA: jsou podporeny i prepisovatelne udalosti, ktere se ve fronte udrzi jen dokud neni
    //           nagenerovana dalsi udalost (ochrana proti zbytecnemu shromazdovani)
    //
    // kriticka sekce pro praci s udalostmi
    // POZOR: v teto sekci nesmi dojit ke vnoreni do CSocket::SocketCritSect ani do
    // SocketsThread->CritSect (nesmi se volat metody SocketsThread)
    CRITICAL_SECTION EventCritSect;
    TIndirectArray<CControlConnectionSocketEventData> Events; // fronta udalosti
    int EventsUsedCount;                                      // pocet skutecne pouzivanych prvku pole Events (prvky se pouzivaji opakovane)
    HANDLE NewEvent;                                          // system event: signaled pokud Events obsahuje nejakou udalost
    BOOL RewritableEvent;                                     // TRUE pokud nova udalost muze prepsat posledni udalost ve fronte

protected:
    // kriticka sekce pro pristup k datum objektu je CSocket::SocketCritSect
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)

    // parametry pripojeni na FTP server
    CFTPProxyServer* ProxyServer; // NULL = "not used (direct connection)"
    char Host[HOST_MAX_SIZE];
    unsigned short Port;
    char User[USER_MAX_SIZE];
    char Password[PASSWORD_MAX_SIZE];
    char Account[ACCOUNT_MAX_SIZE];
    int UseListingsCache;
    char* InitFTPCommands;
    BOOL UsePassiveMode;
    char* ListCommand;
    BOOL UseLIST_aCommand; // TRUE = ignoruj ListCommand, pouzij "LIST -a" (list hidden files (unix))

    DWORD ServerIP;                           // IP adresa serveru (==INADDR_NONE dokud neni IP zname)
    BOOL CanSendOOBData;                      // FALSE pokud server nepodporuje OOB data (pouziva se pri posilani prikazu pro abortovani)
    char* ServerSystem;                       // system serveru (odpoved na prikaz SYST) - muze byt i NULL
    char* ServerFirstReply;                   // prvni odpoved serveru (obsahuje casto verzi FTP serveru) - muze byt i NULL
    BOOL HaveWorkingPath;                     // TRUE pokud je WorkingPath platne
    char WorkingPath[FTP_MAX_PATH];           // aktualni pracovni cesta na FTP serveru
    CCurrentTransferMode CurrentTransferMode; // aktualni prenosovy rezim na FTP serveru (jen pamet pro posledni FTP prikaz "TYPE")

    BOOL EventConnectSent; // TRUE jen pokud jiz byla poslana udalost ccsevConnected (resi prichod FD_READ pred FD_CONNECT)

    char* BytesToWrite;            // buffer pro byty, ktere se nezapsaly ve Write (zapisou se po prijeti FD_WRITE)
    int BytesToWriteCount;         // pocet platnych bytu v bufferu 'BytesToWrite'
    int BytesToWriteOffset;        // pocet jiz odeslanych bytu v bufferu 'BytesToWrite'
    int BytesToWriteAllocatedSize; // alokovana velikost bufferu 'BytesToWrite'

    char* ReadBytes;            // buffer pro prectene byty ze socketu (ctou se po prijeti FD_READ)
    int ReadBytesCount;         // pocet platnych bytu v bufferu 'ReadBytes'
    int ReadBytesOffset;        // pocet jiz zpracovanych (preskocenych) bytu v bufferu 'ReadBytes'
    int ReadBytesAllocatedSize; // alokovana velikost bufferu 'ReadBytes'

    DWORD StartTime; // cas zacatku sledovane operace (slouzi pro vypocet timeoutu + wait-okenek)

    int LogUID; // UID logu pro tuto connectionu (-1 dokud neni log zalozen)

    // neni-li NULL, je to chybova hlaska o duvodu zavreni spojeni v odpojenem FS (zobrazi se
    // v messageboxu pri pripojeni FS do panelu)
    char* ConnectionLostMsg;

    HWND OurWelcomeMsgDlg; // handle okna welcome-msg (neni treba synchronizovat, pristup jen z hl. threadu) - NULL=jeste nebylo otevrene, pozor: okno uz muze byt zavrene

    BOOL KeepAliveEnabled;            // TRUE pokud se v teto "control connection" mame snazit zamezit odpojeni posilanim keep-alive prikazu
    int KeepAliveSendEvery;           // po jake dobe posilat keep-alive prikazy (perioda) (kopie kvuli pouziti z vice threadu)
    int KeepAliveStopAfter;           // po jake dobe prestat posilat keep-alive prikazy (kopie kvuli pouziti z vice threadu)
    int KeepAliveCommand;             // prikaz pro keep-alive (0-NOOP, 1-PWD, 2-NLST, 3-LIST) (kopie kvuli pouziti z vice threadu)
    DWORD KeepAliveStart;             // GetTickCount() posledniho provedeneho prikazu (mimo keep-alive) v "control connection"
    CKeepAliveMode KeepAliveMode;     // soucasny stav zpracovani keep-alive v "control connection"
    BOOL KeepAliveCmdAllBytesWritten; // FALSE = posledni keep-alive prikaz jeste nebyl komplet odeslan (musime cekat na FD_WRITE)
    HANDLE KeepAliveFinishedEvent;    // "signaled" po ukonceni keep-alive prikazu (vyuziva hl. thread, kdyz ceka na spusteni normalniho prikazu)
    // "data connection" pro keep-alive prikaz (NLST+LIST) - jen zahazuje data (destrukce objektu
    // probiha v sekci SocketsThread->CritSect, takze objekt nemuze "receive" metodam zmizet "pod
    // nohama" + pred destrukci objektu probiha NULLovani KeepAliveDataCon v sekci tohoto objektu
    // (objekt lze "ziskat" jen pro prvni destrukci))
    CKeepAliveDataConSocket* KeepAliveDataCon;
    CKeepAliveDataConState KeepAliveDataConState; // stav "data connection" pro keep-alive prikaz (NLST+LIST) - pro aktivni i pasivni spojeni

    int EncryptControlConnection;
    int EncryptDataConnection;

    int CompressData;

public:
    CControlConnectionSocket();
    virtual ~CControlConnectionSocket();

    // ******************************************************************************************
    // metody pro praci s control socketem
    // ******************************************************************************************

    BOOL IsGood() { return NewEvent != NULL && KeepAliveFinishedEvent != NULL; }

    // nastavi parametry pripojeni na FTP server; retezce nesmi byt NULL (vyjimka je
    // 'initFTPCommands' a 'listCommand' - ty mohou byt NULL)
    // mozne volat z libovolneho threadu
    void SetConnectionParameters(const char* host, unsigned short port, const char* user,
                                 const char* password, BOOL useListingsCache,
                                 const char* initFTPCommands, BOOL usePassiveMode,
                                 const char* listCommand, BOOL keepAliveEnabled,
                                 int keepAliveSendEvery, int keepAliveStopAfter,
                                 int keepAliveCommand, int proxyServerUID,
                                 int encryptControlConnection, int encryptDataConnection,
                                 int compressData);

    // metody pro sledovani doby spousteni operace se socketem:
    // POZOR: neni synchronizovane - pouzivat z jednoho threadu nebo vyuzit jine synchronizace
    //
    // nastavi cas zacatku operace (casovy krok zhruba 10ms - vyuziva GetTickCount)
    void SetStartTime(BOOL setOldTime = FALSE) { StartTime = GetTickCount() - (setOldTime ? 60000 : 0); }
    // vraci pocet ms od zacatku operace (max. doba je zhruba 50dni)
    DWORD GetTimeFromStart();
    // odecita od 'showTime' cas od zacatku operace, vraci minimalne 0 ms (zadne zaporne cisla)
    DWORD GetWaitTime(DWORD showTime);

    int GetEncryptControlConnection() { return EncryptControlConnection; }
    int GetEncryptDataConnection() { return EncryptDataConnection; }
    int GetCompressData() { return CompressData; }

    // otevre "control connection" na FTP server (zadany predchozim volanim SetConnectionParameters);
    // ocekava nastaveny SetStartTime() - zobrazuje wait-okenko s pouzitim GetWaitTime();
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je zaroven
    // parent pripadnych messageboxu s chybami; 'reconnect' je TRUE pokud se jedna o reconnect
    // zavrene "control connection"; neni-li 'workDir' NULL, zjisti aktualni pracovni cestu
    // na serveru (hned po pripojeni) a ulozi ji do 'workDir' (buffer o velikosti 'workDirBufSize');
    // neni-li 'totalAttemptNum' NULL, jde o in/out promennou s celkovym poctem pokusu o pripojeni
    // (pred prvnim volanim inicializovat na hodnotu 1); neni-li 'retryMsg' NULL, zobrazi se pred
    // dalsim pokusem o pripojeni (nejsou-li jiz vsechny vycerpane) hlaseni 'retryMsg' (text pro
    // retry wait-okenko) - umoznuje simulaci stavu, kdy k preruseni spojeni doslo uvnitr teto metody;
    // neni-li 'reconnectErrResID' -1, pouzije se jako text pro reconnect wait okenko (je-li -1,
    // pouzije se text IDS_SENDCOMMANDERROR); je-li 'useFastReconnect' TRUE, probehne reconnect
    // bez cekani;
    // vraci TRUE pokud se podarilo pripojit; jmeno uzivatele se muze behem pripojovani zmenit
    // (nezavisi na uspechu metody) - aktualni jmeno se vraci v 'user' (max. 'userSize' bytu);
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL StartControlConnection(HWND parent, char* user, int userSize, BOOL reconnect,
                                char* workDir, int workDirBufSize, int* totalAttemptNum,
                                const char* retryMsg, BOOL canShowWelcomeDlg,
                                int reconnectErrResID, BOOL useFastReconnect);

    // zmeni pracovni cestu v "control connection"; ocekava nastaveny SetStartTime() - v pripade,
    // ze neni treba obnovit spojeni, zobrazuje wait-okenko s pouzitim GetWaitTime();
    // 'notInPanel' je TRUE jde-li o odpojeny FS (neni v panelu); je-li 'notInPanel'
    // FALSE, jde o spojeni v panelu - je-li 'leftPanel' TRUE, jde o levy panel,
    // jinak jde o pravy panel; 'parent' je "foreground" okno threadu (po
    // stisku ESC se pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne napriklad
    // v jine aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent() nebo pluginem
    // otevreny dialog); 'parent' je zaroven parent pripadnych messageboxu s chybami;
    // 'path' je nova pracovni cesta; je-li 'parsedPath' TRUE, je napred nutne zjistit jestli
    // se ze zacatku 'path' nema oriznout slash (napr. u OpenVMS/MVS cest) - pouziva se v pripade,
    // kdy se 'path' ziska parsovanim user-part casti cesty na FS; v 'path' vraci (max.
    // 'pathBufSize' bytu) novou pracovni cestu; 'userBuf'+'userBufSize' je in/out buffer pro
    // user-name na FTP serveru (muze se pri reconnectu zmenit); je-li 'forceRefresh' TRUE,
    // nesmi se pri zmene cesty pouzit zadne cachovane udaje (nutne menit cestu primo na
    // serveru); 'mode' je rezim zmeny cesty (viz CPluginFSInterfaceAbstract::ChangePath);
    // je-li 'cutDirectory' TRUE, je potreba 'path' pred pouzitim zkratit (o jeden adresar);
    // v pripade, ze se cesta zkracuje z duvodu, ze jde o cestu k souboru (staci domenka, ze
    // by mohlo jit o cestu k souboru - po vylistovani cesty se overuje jestli soubor existuje,
    // pripadne se zobrazi uzivateli chyba) a 'cutFileName' neni NULL (mozne jen v 'mode' 3
    // a pri 'cutDirectory' FALSE), vraci v bufferu 'cutFileName' (o velikosti MAX_PATH znaku)
    // jmeno tohoto souboru (bez cesty), jinak v bufferu 'cutFileName' vraci prazdny retezec;
    // neni-li 'pathWasCut' NULL, vraci se v nem TRUE pokud doslo ke zkraceni cesty; Salamander
    // pouziva 'cutFileName' a 'pathWasCut' u prikazu Change Directory (Shift+F7) pri zadani
    // jmena souboru - dochazi k fokusu tohoto souboru; 'rescuePath' obsahuje posledni pristupnou
    // a listovatelnou cestu na serveru, ktera se ma pouzit az vse ostatni zklame (lepsi je
    // jina cesta nez disconnect) - jde o in/out retezec (max. velikost FTP_MAX_PATH znaku);
    // je-li 'showChangeInLog' TRUE, ma se v logu objevit hlaska "Changing path to...";
    // je-li listing v cache (nemusi se ziskavat na serveru), vraci se v 'cachedListing' (nesmi
    // byt NULL) jako alokovany retezec, delka retezce se vraci v 'cachedListingLen' (nesmi
    // byt NULL), volajici je odpovedny za dealokaci; v 'cachedListingDate' (nesmi byt NULL)
    // se vraci datum porizeni listingu; 'totalAttemptNum'+'skipFirstReconnectIfNeeded'
    // je parametr pro SendChangeWorkingPath(); vraci FALSE pokud se zmena cesty nepodarila
    // (nerika nic o "control connection" - muze byt otevrena i zavrena)
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL ChangeWorkingPath(BOOL notInPanel, BOOL leftPanel, HWND parent, char* path,
                           int pathBufSize, char* userBuf, int userBufSize, BOOL parsedPath,
                           BOOL forceRefresh, int mode, BOOL cutDirectory,
                           char* cutFileName, BOOL* pathWasCut, char* rescuePath,
                           BOOL showChangeInLog, char** cachedListing, int* cachedListingLen,
                           CFTPDate* cachedListingDate, DWORD* cachedListingStartTime,
                           int* totalAttemptNum, BOOL skipFirstReconnectIfNeeded);

    // listuje pracovni cestu v "control connection" (nepouziva zadne cache); ocekava nastaveny
    // SetStartTime() (zobrazuje wait-okenko s pouzitim GetWaitTime()), nastavenou pracovni
    // cestu na 'path' a otevrene spojeni (je-li zavrene, nabidne reconnect); 'parent'
    // je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni jestli byl stisknut
    // ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde o
    // SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je zaroven
    // parent pripadnych messageboxu s chybami; 'path' je pracovni cesta; 'userBuf'+'userBufSize'
    // je in/out buffer pro user-name na FTP serveru (muze se pri reconnectu zmenit); vraci TRUE
    // pokud se podarilo ziskat aspon cast listingu (v nejhorsim prazdny retezec), listing se
    // vraci v alokovanem retezci 'allocatedListing' (nesmi byt NULL, pokud vraci NULL, jde
    // o chybu alokace pameti), delka retezce se vraci v 'allocatedListingLen' (nesmi byt NULL),
    // volajici je odpovedny za dealokaci; v 'listingDate' (nesmi byt NULL) se vraci datum porizeni
    // listingu; v 'pathListingIsIncomplete' (nesmi byt NULL) se vraci TRUE pokud listing neni
    // kompletni (byl prerusen) nebo FALSE pokud je listing kompletni; v 'pathListingIsBroken'
    // (nesmi byt NULL) se vraci TRUE pokud prikaz pro listovani vratil chybu (3xx, 4xx nebo 5xx);
    // je-li 'forceRefresh' TRUE, nesmi se pouzivat zadne cache, vse je nutne provest primo
    // na serveru; 'totalAttemptNum' jsou parametry pro StartControlConnection(); vraci FALSE
    // pokud server odmita cestu vylistovat (nerika nic o "control connection" - muze byt otevrena
    // i zavrena); vraci FALSE a ve 'fatalError' (nesmi byt NULL) TRUE, pokud nastala fatalni
    // chyba (server neodpovida jako FTP server, atp.); 'dontClearCache' je TRUE pokud se nema
    // volat ListingCache.RefreshOnPath() pro cisteni cache pro aktualni cestu (cisti se jinde);
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL ListWorkingPath(HWND parent, const char* path, char* userBuf, int userBufSize,
                         char** allocatedListing, int* allocatedListingLen,
                         CFTPDate* listingDate, BOOL* pathListingIsIncomplete,
                         BOOL* pathListingIsBroken, BOOL* pathListingMayBeOutdated,
                         DWORD* listingStartTime, BOOL forceRefresh, int* totalAttemptNum,
                         BOOL* fatalError, BOOL dontClearCache);

    // zjisti aktualni pracovni cestu v "control connection"; ocekava nastaveny
    // SetStartTime() - zobrazuje wait-okenko s pouzitim GetWaitTime(); 'parent' je
    // "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni jestli byl stisknut
    // ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde o
    // SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je
    // zaroven parent pripadnych messageboxu s chybami; v 'path' vraci (max. 'pathBufSize'
    // bytu) aktualni pracovni cestu, pri chybe zjistovani pracovni cesty (napr. "jeste
    // nebyla definovana") vraci prazdny retezec; je-li 'forceRefresh' TRUE, nesmi se pri
    // zjistovani cesty pouzit zadne cachovane udaje (nutne ziskat cestu primo ze serveru);
    // vraci FALSE pokud se zjisteni cesty nepodarilo (napr. spatny format odpovedi
    // serveru), doslo k preruseni spojeni (pri timeoutu automaticky tvrde zavira
    // spojeni - posilat "QUIT" nema smysl) - neni-li 'canRetry' NULL, muze se text chyby
    // vratit v 'retryMsg' (buffer o velikosti 'retryMsgBufSize') - v 'canRetry' se vraci
    // TRUE, jinak se chyba zobrazi v messageboxu ('canRetry' je bud NULL nebo se v nem
    // vraci FALSE);
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL GetCurrentWorkingPath(HWND parent, char* path, int pathBufSize, BOOL forceRefresh,
                               BOOL* canRetry, char* retryMsg, int retryMsgBufSize);

    // je-li treba, nastavi rezim prenosu (ASCII/BINAR(IMAGE) - v 'asciiMode' je TRUE/FALSE)
    // v "control connection"; ocekava nastaveny SetStartTime() - zobrazuje wait-okenko
    // s pouzitim GetWaitTime(); 'parent' je "foreground" okno threadu (po stisku ESC se
    // pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne napriklad v jine
    // aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent() nebo pluginem
    // otevreny dialog); 'parent' je zaroven parent pripadnych messageboxu s chybami;
    // neni-li 'success' NULL, vraci se v nem TRUE pokud server vrati uspech; pokud server
    // vrati neuspech, text odpovedi serveru je v bufferu 'ftpReplyBuf' (max. velikost
    // 'ftpReplyBufSize'), je null-terminated - je-li delsi nez buffer, je jednoduse oriznut;
    // je-li 'forceRefresh' TRUE, nesmi se pri nastavovani rezimu prenosu pouzit zadne
    // cachovane udaje (rezim se nastavi, i kdyz by teoreticky nemusel, protoze uz
    // je nastaveny); vraci FALSE pokud se nastaveni rezimu prenosu nepodarilo, doslo
    // k preruseni spojeni (pri timeoutu automaticky tvrde zavira spojeni - posilat
    // "QUIT" nema smysl) - neni-li 'canRetry' NULL, muze se text chyby vratit v 'retryMsg'
    // (buffer o velikosti 'retryMsgBufSize') - v 'canRetry' se vraci TRUE, jinak se chyba
    // zobrazi v messageboxu ('canRetry' je bud NULL nebo se v nem vraci FALSE);
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL SetCurrentTransferMode(HWND parent, BOOL asciiMode, BOOL* success, char* ftpReplyBuf,
                                int ftpReplyBufSize, BOOL forceRefresh, BOOL* canRetry,
                                char* retryMsg, int retryMsgBufSize);

    // vyprazdneni cache pro aktualni pracovni cestu: synchronizovane HaveWorkingPath=FALSE,
    void ResetWorkingPathCache();

    // zneplatneni zapamatovaneho prenosoveho rezimu: synchronizovane CurrentTransferMode=ctrmUnknown
    void ResetCurrentTransferModeCache();

    // posle prikaz na zmenu pracovni cesty v "control connection" na 'path'; ocekava nastaveny
    // SetStartTime() - v pripade, ze neni treba obnovit spojeni, zobrazuje wait-okenko
    // s pouzitim GetWaitTime(); 'parent' je "foreground" okno threadu (po stisku ESC
    // se pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne napriklad
    // v jine aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent()
    // nebo pluginem otevreny dialog); 'parent' je zaroven parent pripadnych messageboxu s chybami;
    // v 'success' (nesmi byt NULL) se vraci TRUE pri uspechu operace;
    // 'notInPanel'+'leftPanel'+'userBuf'+'userBufSize'+'totalAttemptNum'+'retryMsg' jsou parametry
    // pro ReconnectIfNeeded(); text odpovedi serveru je v bufferu 'ftpReplyBuf' (max. velikost
    // 'ftpReplyBufSize'), je null-terminated - je-li delsi nez buffer, je jednoduse oriznut;
    // neni-li 'startPath' NULL a dojde k obnoveni spojeni, posle se nejdrive prikaz pro
    // zmenu pracovni cesty na 'startPath', a pak az na 'path' (resi relativni zmenu cesty);
    // neni-li 'startPath' NULL a neni potreba obnovit spojeni, zkontroluje pres metodu
    // GetCurrentWorkingPath (s parametrem 'forceRefresh'=FALSE) jestli pracovni cesta na
    // serveru je presne 'startPath', pokud ne, posle se nejdrive prikaz pro zmenu pracovni
    // cesty na 'startPath', a pak az na 'path'; je-li 'skipFirstReconnectIfNeeded' TRUE,
    // predpoklada se, ze je spojeni navazane (pouziva se pro navazani pripadneho "retry");
    // v 'userRejectsReconnect' (neni-li NULL) se vraci TRUE pokud je operace neuspesna jen
    // z duvodu, ze user odmita reconnect; vraci FALSE jen pokud se nepodarilo prikaz
    // poslat - doslo k preruseni spojeni (pri timeoutu automaticky tvrde zavira
    // spojeni - posilat "QUIT" nema smysl)
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL SendChangeWorkingPath(BOOL notInPanel, BOOL leftPanel, HWND parent, const char* path,
                               char* userBuf, int userBufSize, BOOL* success, char* ftpReplyBuf,
                               int ftpReplyBufSize, const char* startPath, int* totalAttemptNum,
                               const char* retryMsg, BOOL skipFirstReconnectIfNeeded,
                               BOOL* userRejectsReconnect);

    // zjisti typ cesty na FTP serveru (vola v kriticke sekci ::GetFTPServerPathType()
    // s parametry 'ServerSystem' a 'ServerFirstReply')
    CFTPServerPathType GetFTPServerPathType(const char* path);

    // zjisti jestli 'ServerSystem' obsahuje jmeno 'systemName'
    BOOL IsServerSystem(const char* systemName);

    // je-li zavrena "control connection", nabidne uzivateli jeji nove otevreni
    // (POZOR: nenastavuje pracovni adresar na serveru); vraci TRUE pokud je
    // "control connection" pripravena pro pouziti, zaroven v tomto pripade nastavuje
    // SetStartTime() (aby dalsi cekani uzivatele mohlo navazat na pripadny reconnect);
    // 'notInPanel' je TRUE jde-li o odpojeny FS (neni v panelu); je-li 'notInPanel' FALSE,
    // jde o spojeni v panelu - je-li 'leftPanel' TRUE, jde o levy panel, jinak jde o pravy panel;
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je
    // zaroven parent pripadnych messageboxu s chybami; 'userBuf'+'userBufSize' je buffer
    // pro novy user-name na FTP serveru (muze se pri reconnectu zmenit); v 'reconnected'
    // (neni-li NULL) vraci TRUE pokud doslo k obnoveni spojeni (byla znovu otevrena
    // "control connection"); je-li 'setStartTimeIfConnected' FALSE a neni-li treba spojeni
    // obnovovat, nenastavuje se SetStartTime(); 'totalAttemptNum'+'retryMsg'+'reconnectErrResID'+
    // 'useFastReconnect' jsou parametry pro StartControlConnection(); v 'userRejectsReconnect'
    // (neni-li NULL) vraci TRUE pokud user odmitne provest reconnect
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL ReconnectIfNeeded(BOOL notInPanel, BOOL leftPanel, HWND parent, char* userBuf,
                           int userBufSize, BOOL* reconnected, BOOL setStartTimeIfConnected,
                           int* totalAttemptNum, const char* retryMsg,
                           BOOL* userRejectsReconnect, int reconnectErrResID,
                           BOOL useFastReconnect);

    // posle prikaz na FTP server a vrati odpoved serveru (POZOR: odpovedi typu FTP_D1_MAYBESUCCESS
    // nevraci - automaticky ceka na dalsi odpoved serveru); 'parent' je "foreground" okno threadu
    // (po stisku ESC se pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne napriklad v jine
    // aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny
    // dialog); 'parent' je zaroven parent pripadnych messageboxu s chybami; 'ftpCmd' je
    // posilany prikaz; 'logCmd' je retezec pro Log (shodny s 'ftpCmd' az na vynechana hesla, atp.);
    // 'waitWndText' je text pro wait okenko (NULL = standardni text zobrazujici zpravu o posilani
    // 'logCmd'); 'waitWndTime' je zpozdeni zobrazeni wait okenka; je-li 'allowCmdAbort' TRUE, po
    // prvnim ESC se posila "ABOR" (zrejme ma smysl jen pro prikazy s data connection) a az po druhem
    // ESC se prerusi spojeni, je-li 'allowCmdAbort' FALSE, prerusi se spojeni uz po prvnim ESC;
    // je-li 'resetWorkingPathCache' TRUE, zavola se po poslani prikazu metoda
    // ResetWorkingPathCache(), pouziva se v pripade, ze posilany prikaz muze zmenit aktualni
    // pracovni cestu; je-li 'resetCurrentTransferModeCache' TRUE, zavola se po poslani prikazu
    // metoda ResetCurrentTransferModeCache(), pouziva se v pripade, ze posilany prikaz muze zmenit
    // aktualni prenosovy rezim; vraci TRUE pokud se poslani prikazu nebo jeho preruseni a prijeti
    // odpovedi podarilo, v 'cmdAborted' (neni-li NULL) vraci TRUE pokud byl prikaz uspesne
    // prerusen; kod odpovedi se vraci v 'ftpReplyCode' (nesmi byt NULL) - je platne (!=-1)
    // i pri 'cmdAborted'==TRUE; text odpovedi je v bufferu 'ftpReplyBuf' (max. velikost
    // 'ftpReplyBufSize'), je null-terminated - je-li delsi nez buffer, je jednoduse oriznut;
    // je-li 'specialUserInterface' NULL, ma se pro uzivatelske rozhrani pouzit standardni
    // wait-okenko, jinak se ma pouzit objekt predany pres 'specialUserInterface' (vyuziva se
    // napr. u listovani aktualni cesty);
    // vraci FALSE pokud doslo k preruseni spojeni (pri timeoutu automaticky tvrde zavira
    // spojeni - posilat "QUIT" nema smysl) - neni-li 'canRetry' NULL, muze se text chyby vratit
    // v 'retryMsg' (buffer o velikosti 'retryMsgBufSize') - v 'canRetry' se vraci TRUE,
    // jinak se chyba zobrazi v messageboxu ('canRetry' je bud NULL nebo se v nem vraci FALSE);
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    //
    // POZOR: u abortovani prikazu ('allowCmdAbort'==TRUE) neni doreseny system prijmu odpovedi
    //        serveru (servery vraci bud jednu odpoved jen pro ABOR nebo dve odpovedi (list + abort),
    //        bohuzel maji stejny kod 226, takze nelze poznat o kterou situaci jde) - resi se to
    //        tak, ze se zkusi prijmou vse co server posle v jednom paketu (posila pravdepodobne
    //        obe odpovedi najednou), pripadne dalsi odpovedi se vyignoruji jako "unexpected" pred
    //        dalsim FTP prikazem posilanym touto metodou
    BOOL SendFTPCommand(HWND parent, const char* ftpCmd, const char* logCmd, const char* waitWndText,
                        int waitWndTime, BOOL* cmdAborted, int* ftpReplyCode, char* ftpReplyBuf,
                        int ftpReplyBufSize, BOOL allowCmdAbort, BOOL resetWorkingPathCache,
                        BOOL resetCurrentTransferModeCache, BOOL* canRetry, char* retryMsg,
                        int retryMsgBufSize, CSendCmdUserIfaceAbstract* specialUserInterface);

    // zavre "control connection" na FTP server (prip. po timeoutu provede tvrde zavreni socketu);
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je zaroven
    // parent pripadnych messageboxu s chybami
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    void CloseControlConnection(HWND parent);

    // pokud se jeste user nedozvedel o zavreni "control connection" v panelu, dozvi se to
    // v teto metode; 'notInPanel' je TRUE jde-li o odpojeny FS (neni v panelu); je-li
    // 'notInPanel' FALSE, jde o spojeni v panelu - je-li 'leftPanel' TRUE, jde o levy panel,
    // jinak jde o pravy panel; 'parent' je parent pripadnych messageboxu; je-li 'quiet'
    // TRUE, zobrazi se hlaska s informaci o zavreni "control connection" jen do logu,
    // je-li 'quiet' FALSE, zobrazi se i messagebox
    // POZOR: spoleha se na to, ze v "idle" Salamandera se s "control connection"
    //        nepracuje (nic neceka na udalosti, atd.)
    void CheckCtrlConClose(BOOL notInPanel, BOOL leftPanel, HWND parent, BOOL quiet);

    // vraci UID logu (-1 neexistuje-li log)
    int GetLogUID();

    // prida text 'str' (delka 'len') do logu teto "control connection"; je-li 'len' -1, bere
    // se "len=strlen(str)"; je-li 'addTimeToLog' TRUE, umisti se pred hlasku aktualni cas;
    // vraci TRUE pri uspesnem pridani do logu nebo pokud log vubec neexistuje
    BOOL LogMessage(const char* str, int len, BOOL addTimeToLog = FALSE);

    // je-li otevrene okno welcome-messsage a je aktivovano hl. okno Salamandera, provede se
    // aktivace okna welcome-messsage (napr. po zavreni message-boxu s chybou se aktivuje hl.
    // okno misto okna welcome-messsage)
    // POZOR: volani mozne jen v hl. threadu
    void ActivateWelcomeMsg();

    // odpoji okno welcome-msg od teto "control connection" (dale se uz nebude aktivovat pres
    // metodu ActivateWelcomeMsg)
    // POZOR: volani mozne jen v hl. threadu
    void DetachWelcomeMsg() { OurWelcomeMsgDlg = NULL; }

    // vraci naalokovany retezec s odpovedi serveru na prikaz SYST (get operating system);
    // pri chybe alokace vraci NULL
    char* AllocServerSystemReply();

    // vraci naalokovany retezec s prvni odpovedi serveru; pri chybe alokace vraci NULL
    char* AllocServerFirstReply();

    // postne socketu message 'msgID' (CTRLCON_KAPOSTSETUPNEXT: aby v pripade, ze
    // 'KeepAliveMode' je 'kamProcessing' nebo 'kamWaitingForEndOfProcessing', vyvolal
    // metodu SetupNextKeepAliveTimer()) (CTRLCON_KALISTENFORCON: doslo k otevreni
    // "listen" portu na proxy serveru)
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // mozne volat z libovolneho threadu
    void PostMsgToCtrlCon(int msgID, void* msgParam);

    // inicializuje objekt operace - vola jeho SetConnection() a vraci TRUE pri uspechu;
    // POZOR: operace 'oper' nesmi byt vlozena v FTPOperationsList !!!
    BOOL InitOperation(CFTPOperation* oper);

    // predani aktivni "control connection" do nove vznikleho workera 'newWorker' (nemuze
    // byt NULL); 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog)
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    void GiveConnectionToWorker(CFTPWorker* newWorker, HWND parent);

    // prevzeti aktivni "control connection" z workera 'workerWithCon' (nemuze byt NULL),
    // pokud jiz tento objekt ma connectionu otevrenou, k prevzeti nedojde;
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    void GetConnectionFromWorker(CFTPWorker* workerWithCon);

    // vraci (v krit. sekci) hodnotu UseListingsCache
    BOOL GetUseListingsCache();

    // provede download jednoho souboru na pracovni ceste; jmeno souboru (bez cesty)
    // je 'fileName'; je-li znama velikost souboru v bytech, je v 'fileSizeInBytes',
    // pokud znama neni, je zde CQuadWord(-1, -1); 'asciiMode' je TRUE/FALSE pokud mame
    // prenaset soubor v ASCII/binarnim prenosovem rezimu; 'parent' je "foreground"
    // okno threadu (po stisku ESC se pouziva pro zjisteni jestli byl stisknut
    // ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde o
    // SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent'
    // je zaroven parent pripadnych messageboxu s chybami; 'workPath' je pracovni cesta;
    // 'tgtFileName' je jmeno ciloveho souboru (kam provest download); v 'newFileCreated'
    // vraci TRUE pokud se podarilo downloadnout aspon cast souboru (aspon neco je na
    // disku); v 'newFileIncomplete' vraci TRUE pokud se nepodarilo stahnout kompletni
    // soubor; je-li 'newFileCreated' TRUE, vraci v 'newFileSize' velikost souboru na disku;
    // 'totalAttemptNum', 'panel', 'notInPanel', 'userBuf' a 'userBufSize' jsou parametry
    // pro ReconnectIfNeeded()
    void DownloadOneFile(HWND parent, const char* fileName, CQuadWord const& fileSizeInBytes,
                         BOOL asciiMode, const char* workPath, const char* tgtFileName,
                         BOOL* newFileCreated, BOOL* newFileIncomplete, CQuadWord* newFileSize,
                         int* totalAttemptNum, int panel, BOOL notInPanel, char* userBuf,
                         int userBufSize);

    // vytvori adresar 'newName' (muze byt libovolny retezec od usera - bereme jako plnou nebo
    // relativni cestu na serveru - ignorujeme syntaxi cest Salamandera); 'parent' je parent
    // pripadnych messageboxu; 'newName' je jmeno vytvareneho adresare; v 'newName' (buffer
    // 2 * MAX_PATH znaku) se pri uspechu vraci jmeno adresare pro focus v panelu pri dalsim
    // refreshi; vraci uspech (pri neuspechu se ocekava, ze user jiz videl okno s chybou); 'workPath'
    // je pracovni cesta; 'totalAttemptNum', 'panel', 'notInPanel', 'userBuf' a 'userBufSize'
    // jsou parametry pro ReconnectIfNeeded(); v 'changedPath' (aspon FTP_MAX_PATH znaku)
    // vraci cestu na serveru, kterou je potreba refreshnout (je-li 'changedPath' prazdne,
    // zadny refresh neni potreba)
    BOOL CreateDir(char* changedPath, HWND parent, char* newName, const char* workPath,
                   int* totalAttemptNum, int panel, BOOL notInPanel, char* userBuf,
                   int userBufSize);

    // prejmenuje soubor/adresar 'fromName' na 'newName'; 'parent' je parent pripadnych
    // messageboxu; v 'newName' (buffer 2 * MAX_PATH znaku) se pri uspechu vraci jmeno
    // souboru/adresare pro focus v panelu pri dalsim refreshi; vraci uspech (pri neuspechu
    // se ocekava, ze user jiz videl okno s chybou); 'workPath' je pracovni cesta;
    // 'totalAttemptNum', 'panel', 'notInPanel', 'userBuf' a 'userBufSize'
    // jsou parametry pro ReconnectIfNeeded(); v 'changedPath' (aspon FTP_MAX_PATH znaku)
    // vraci cestu na serveru, kterou je potreba refreshnout (je-li 'changedPath' prazdne,
    // zadny refresh neni potreba)
    BOOL QuickRename(char* changedPath, HWND parent, const char* fromName, char* newName,
                     const char* workPath, int* totalAttemptNum, int panel, BOOL notInPanel,
                     char* userBuf, int userBufSize, BOOL isVMS, BOOL isDir);

    // posle pozadavek na otevreni "listen" portu (bud na lokalni masine nebo na proxy serveru)
    // pro data-connectionu 'dataConnection'; na vstupu je 'listenOnIP'+'listenOnPort' IP+port,
    // kde by se mel otevrit "listen" port (ma smysl jen bez proxy serveru);
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni jestli byl
    // stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde o
    // SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'parent' je zaroven
    // parent pripadnych messageboxu s chybami; 'waitWndTime' je zpozdeni zobrazeni wait okenka;
    // vraci TRUE pri uspechu - v 'listenOnIP'+'listenOnPort' IP+port, kde cekame na spojeni
    // z FTP serveru; vraci FALSE pokud doslo k preruseni nebo chybe; pokud ma smysl zkouset
    // "retry", tvrde prerusi spojeni (mohli bysme posilat "QUIT", ale prozatim si zjednodusime
    // zivot) a vraci text chyby v 'retryMsg' (buffer o velikosti 'retryMsgBufSize', nesmi byt 0) a
    // v 'canRetry' (nesmi byt NULL) vraci TRUE; pokud "retry" nema smysl, chyba se zobrazi
    // v messageboxu a v 'canRetry' se vrati FALSE (spojeni se neprerusuje); 'errBuf' je
    // pomocny buffer o velikosti 'errBufSize' (nesmi byt 0) - pouziva se pro texty zobrazovane
    // v messageboxech;
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    BOOL OpenForListeningAndWaitForRes(HWND parent, CDataConnectionSocket* dataConnection,
                                       DWORD* listenOnIP, unsigned short* listenOnPort,
                                       BOOL* canRetry, char* retryMsg, int retryMsgBufSize,
                                       int waitWndTime, char* errBuf, int errBufSize);

    // vraci TRUE pokud se pro listovani pouziva prikaz "LIST -a"
    BOOL IsListCommandLIST_a();

    // pokud se pro listovani pouziva prikaz "LIST -a", pouzije ListCommand nebo "LIST";
    // jinak pouzije "LIST -a"
    void ToggleListCommandLIST_a();

protected:
    // ******************************************************************************************
    // pomocne metody - nepouzivat mimo tento objekt
    // ******************************************************************************************

    // pridani udalosti; je-li 'rewritable' TRUE, bude tato udalost prepsana pri pokusu
    // o pridani dalsi udalosti; 'event'+'data1'+'data2' jsou data udalosti; vraci TRUE pokud
    // se udalost podarilo pridat do fronty, vraci FALSE pri nedostatku pameti
    BOOL AddEvent(CControlConnectionSocketEvent event, DWORD data1, DWORD data2, BOOL rewritable = FALSE);

    // ziskani udalosti; udalost se vraci v 'event'+'data1'+'data2' (nesmi byt NULL); vraci TRUE
    // pokud vraci udalost z fronty, vraci FALSE pokud ve fronte zadna udalost nebyla
    BOOL GetEvent(CControlConnectionSocketEvent* event, DWORD* data1, DWORD* data2);

    // ceka na udalost na socketu (udalost = probehla dalsi faze spoustene operace nebo ESC);
    // 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); v 'event'+'data1'+
    // 'data2' (nesmi byt NULL) se vraci udalost (jedna z ccsevXXX; ccsevESC pokud user stiskl
    // ESC); 'milliseconds' je cas v ms, po ktery se ma cekat na udalost, po teto dobe vraci
    // metoda 'event'==ccsevTimeout, je-li 'milliseconds' INFINITE, ma se cekat bez casoveho
    // omezeni; neni-li 'waitWnd' NULL, sleduje stisk close buttonu ve wait-okenku 'waitWnd';
    // neni-li 'userIface' NULL, sleduje stisk close buttonu v uzivatelskem rozhrani 'userIface',
    // zaroven je-li navic 'waitForUserIfaceFinish' TRUE, sleduje misto udalosti na socketu
    // udalost ohlasujici dokonceni uzivatelskeho rozhrani ("data connection" + "keep alive");
    // mozne volat z libovolneho threadu
    void WaitForEventOrESC(HWND parent, CControlConnectionSocketEvent* event,
                           DWORD* data1, DWORD* data2, int milliseconds, CWaitWindow* waitWnd,
                           CSendCmdUserIfaceAbstract* userIface, BOOL waitForUserIfaceFinish);

    // pomocna metoda pro detekovani jestli jiz je v bufferu 'ReadBytes' cela odpoved
    // od FTP serveru; vraci TRUE pri uspechu - v 'reply' (nesmi byt NULL) vraci ukazatel
    // na zacatek odpovedi, v 'replySize' (nesmi byt NULL) vraci delku odpovedi,
    // v 'replyCode' (neni-li NULL) vraci FTP kod odpovedi nebo -1 pokud odpoved nema
    // zadny kod (nezacina na triciferne cislo); pokud jeste neni odpoved kompletni, vraci
    // FALSE - dalsi volani ReadFTPReply ma smysl az po prijeti nejake udalosti
    // (ccsevNewBytesRead nebo jine, ktera mohla ccsevNewBytesRead prepsat, protoze
    // ccsevNewBytesRead je v pripade uspesneho cteni prepsatelna)
    // POZOR: nutne volat z kriticke sekce SocketCritSect (jinak by se buffer 'ReadBytes'
    // mohl libovolne zmenit)
    BOOL ReadFTPReply(char** reply, int* replySize, int* replyCode = NULL);

    // pomocna metoda pro uvolneni odpovedi od FTP serveru (o delce 'replySize') z bufferu
    // 'ReadBytes'
    // POZOR: nutne volat z kriticke sekce SocketCritSect (jinak by se buffer 'ReadBytes'
    // mohl libovolne zmenit)
    void SkipFTPReply(int replySize);

    // zapise na socket (provede "send") byty z bufferu 'buffer' o delce 'bytesToWrite'
    // (je-li 'bytesToWrite' -1, zapise strlen(buffer) bytu); pri chybe vraci FALSE a
    // je-li znamy kod Windows chyby, vraci ho v 'error' (neni-li NULL); vraci-li TRUE,
    // alespon cast bytu z bufferu byla uspesne zapsana; v 'allBytesWritten' (nesmi byt
    // NULL) se vraci TRUE pokud probehl zapis celeho bufferu; vraci-li 'allBytesWritten'
    // FALSE, je pred dalsim volanim metody Write nutne pockat na udalost ccsevWriteDone
    // (jakmile dojde, je zapis hotovy)
    // mozne volat z libovolneho threadu
    BOOL Write(const char* buffer, int bytesToWrite, DWORD* error, BOOL* allBytesWritten);

    // vyprazdni buffery pro cteni i zapis a frontu udalosti (hodi se napr. pred
    // znovu-otevrenim pripojeni)
    void ResetBuffersAndEvents();

    // pokud v teto "control connection" probiha posilani keep-alive prikazu, pocka na
    // jeho dokonceni; pokud neprobiha posilani keep-alive prikazu, ukonci se okamzite;
    // zrusi timer pro keep-alive (pokud existuje); prepne rezim 'KeepAliveMode' na
    // 'kamForbidden'; vypadek spojeni hlasi do logu a vyvola i messagebox s chybou
    // vedouci k preruseni (reconnect zajisti az dalsi normalni prikaz); chyby keep-alive
    // prikazu hlasi jen do logu; 'parent' je jiz disablovane "foreground" okno threadu
    // (po stisku ESC se pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne
    // napriklad v jine aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent()
    // nebo pluginem otevreny dialog); 'parent' je zaroven parent pripadnych messageboxu
    // s chybami; 'waitWndTime' je zpozdeni zobrazeni wait okenka;
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // mozne volat jen z hl. threadu (pouziva wait-okenka, atd.)
    void WaitForEndOfKeepAlive(HWND parent, int waitWndTime);

    // vola se po dokonceni normalniho prikazu ('KeepAliveMode' musi byt 'kamForbidden' nebo
    // 'kamNone'), kdy je potreba nastavit timer pro keep-alive; prepne rezim 'KeepAliveMode'
    // na 'kamWaiting' (nebo 'kamNone' pokud je keep-alive zakazany); je-li 'immediate'
    // TRUE, provede se prvni keep-alive prikaz co nejdrive
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // mozne volat z libovolneho threadu
    void SetupKeepAliveTimer(BOOL immediate = FALSE);

    // vola se po dokonceni keep-alive prikazu ('KeepAliveMode' by mel byt 'kamProcessing'
    // nebo 'kamWaitingForEndOfProcessing' nebo 'kamNone' (po volani ReleaseKeepAlive())),
    // kdy je mozna potreba nastavit novy timer pro dalsi keep-alive prikaz; prepne rezim
    // 'KeepAliveMode' na 'kamWaiting' (jen pri 'KeepAliveMode' == 'kamProcessing');
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // mozne volat z libovolneho threadu
    void SetupNextKeepAliveTimer();

    // uvolni prostredky ziskane pro provedeni keep-alive prikazu, zrusi timer pro
    // keep-alive (pokud existuje), nastavi 'KeepAliveMode' na 'kamNone' a uvolni
    // 'KeepAliveDataCon' (maze SocketsThread ve sve kriticke sekci, takze behem
    // provadeni metod socketu v tomto threadu uvolneni nehrozi); muze se volat
    // v jakemkoliv stavu 'KeepAliveMode';
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // mozne volat z libovolneho threadu
    void ReleaseKeepAlive();

    // posle keep-alive prikaz 'ftpCmd' na server; 'logUID' je UID logu
    BOOL SendKeepAliveCmd(int logUID, const char* ftpCmd);

    // ******************************************************************************************
    // metody volane v "sockets" threadu (na zaklade prijmu zprav od systemu nebo jinych threadu)
    //
    // POZOR: volane v sekci SocketsThread->CritSect, mely by se provadet co nejrychleji (zadne
    //        cekani na vstup usera, atd.)
    // ******************************************************************************************

    // prijem vysledku volani GetHostByAddress; je-li 'ip' == INADDR_NONE jde o chybu a v 'err'
    // muze byt chybovy kod (pokud 'err' != 0)
    virtual void ReceiveHostByAddress(DWORD ip, int hostUID, int err);

    // prijem udalosti pro tento socket (FD_READ, FD_WRITE, FD_CLOSE, atd.); 'index' je
    // index socketu v poli SocketsThread->Sockets (pouziva se pro opakovane posilani
    // zprav pro socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // prijem vysledku ReceiveNetEvent(FD_CLOSE) - neni-li 'error' NO_ERROR, jde
    // o kod Windowsove chyby (prisla s FD_CLOSE nebo vznikla behem zpracovani FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // prijem timeru i ID 'id' a parametrem 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // prijem postnute zpravy s ID 'id' a parametrem 'param'
    virtual void ReceivePostMessage(DWORD id, void* param);
};

//
// ****************************************************************************
// CClosedCtrlConChecker
//
// resi informovani uzivatele o zavreni "control connection" mimo operace (napr.
// timeout nebo "kick" vedouci k odpojeni z FTP serveru)

class CClosedCtrlConChecker
{
protected:
    CRITICAL_SECTION DataSect; // kriticka sekce pro pristup k datum objektu

    // pole zavrenych socketu, ktere budeme kontrolovat (vzdy pri prijeti FTPCMD_CLOSECONNOTIF)
    TIndirectArray<CControlConnectionSocket> CtrlConSockets;

    BOOL CmdNotPost; // FALSE pokud jiz byl postnuty prikaz FTPCMD_CLOSECONNOTIF

public:
    CClosedCtrlConChecker();
    ~CClosedCtrlConChecker();

    // pridani zavrene "control connection" do testu; vraci TRUE pri uspechu
    BOOL Add(CControlConnectionSocket* sock);

    // informuje uzivatele o zavreni "control connection", pokud se tak jiz nestalo;
    // vola se v hlavnim threadu v "idle"; 'parent' je parent pripadnych messageboxu
    void Check(HWND parent);
};

extern CClosedCtrlConChecker ClosedCtrlConChecker; // resi informovani uzivatele o zavreni "control connection" mimo operace
extern CListingCache ListingCache;                 // cache listingu cest na serverech (pouziva se pri zmene a listovani cest)

// "control connection" z leveho/praveho panelu (NULL == v levem/pravem panelu neni nase FS)
// POZOR: nesmime sahat do objektu "control connection" - jen pro zjisteni umisteni FS v panelu (levy/pravy/odpojeny)
extern CControlConnectionSocket* LeftPanelCtrlCon;
extern CControlConnectionSocket* RightPanelCtrlCon;
extern CRITICAL_SECTION PanelCtrlConSect; // kriticka sekce pro pristup k LeftPanelCtrlCon a RightPanelCtrlCon

void RefreshValuesOfPanelCtrlCon(); // obnovi hodnoty LeftPanelCtrlCon a RightPanelCtrlCon podle aktualniho stavu
