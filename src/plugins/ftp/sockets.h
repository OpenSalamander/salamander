// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
// KONSTANTY
// ****************************************************************************

// je-li definovane nastavuje se pro data-connectiony DATACON_SNDBUF_SIZE a DATACON_RCVBUF_SIZE
// resi problem: https://forum.altap.cz/viewtopic.php?f=6&t=31923
// help pro forum: ver1 = 8k (verze bez nastavovani bufferu), ver2 = 4m (4MB RECV buffer, 256KB SEND buffer)
#define DATACON_USES_OUR_BUF_SIZES

#define DATACON_SNDBUF_SIZE (256 * 1024)      // hodnota prevzata z WinSCP (download 2,2MB/s misto 1,6MB/s)
#define DATACON_RCVBUF_SIZE (4 * 1024 * 1024) // hodnota prevzata z WinSCP (download 2,2MB/s misto 1,6MB/s)

#define WM_APP_SOCKET_POSTMSG (WM_APP + 98) // [0, 0] - ma se volat ReceivePostMessage
#define WM_APP_SOCKET_ADDR (WM_APP + 99)    // [0, 0] - ma se volat ReceiveHostByAddress
#define WM_APP_SOCKET_MIN (WM_APP + 100)    // prvni cislo zpravy pouzite pri prijmu udalosti na socketech
#define WM_APP_SOCKET_MAX (WM_APP + 16099)  // posledni cislo zpravy pouzite pri prijmu udalosti na socketech

#define SD_SEND 0x01 // nejak na tuhle konstantu ve winsock.h zapomeli (je ve winsock2.h)

// ****************************************************************************
// GLOBALNI PROMENNE
// ****************************************************************************

extern WSADATA WinSocketsData; // info o implementaci Windows Sockets

// ****************************************************************************
// GLOBALNI FUNKCE
// ****************************************************************************

// inicializace modulu sockets; \parent' je parent messageboxu,
// vraci TRUE je-li inicializace uspesna
BOOL InitSockets(HWND parent);
// uvolneni modulu sockets
void ReleaseSockets();

// uvolni objekt socketu; pouzivat misto volani operatoru delete ("delete socket;");
// (ukonci sledovani udalosti na socketu a vyradi ho z objektu SocketsThread)
void DeleteSocket(class CSocket* socket);

#ifdef _DEBUG
extern BOOL InDeleteSocket; // TRUE pokud jsme uvnitr ::DeleteSocket (pro test primeho volani "delete socket")
#endif

//
// ****************************************************************************
// CSocket
//
// zakladni objekt socketu, pouziva se jen pro definici dalsich objektu
// pro dealokaci pouzivat funkci ::DeleteSocket!

enum CSocketState // stav socketu
{
    ssNotOpened, // socket zatim neni otevreny (Socket == INVALID_SOCKET)

    ssSocks4_Connect,    // SOCKS 4 - CONNECT: cekame na FD_CONNECT (probiha connect na proxy server)
    ssSocks4_WaitForIP,  // SOCKS 4 - CONNECT: cekame na IP FTP serveru
    ssSocks4_WaitForCon, // SOCKS 4 - CONNECT: cekame na vysledek pozadavku na spojeni s FTP serverem

    ssSocks4A_Connect,    // SOCKS 4A - CONNECT: cekame na FD_CONNECT (probiha connect na proxy server)
    ssSocks4A_WaitForCon, // SOCKS 4A - CONNECT: cekame na vysledek pozadavku na spojeni s FTP serverem

    ssSocks5_Connect,      // SOCKS 5 - CONNECT: cekame na FD_CONNECT (probiha connect na proxy server)
    ssSocks5_WaitForMeth,  // SOCKS 5 - CONNECT: cekame jakou metodu autentifikace si server vybere
    ssSocks5_WaitForLogin, // SOCKS 5 - CONNECT: cekame na vysledek loginu na proxy server (poslali jsme user+password)
    ssSocks5_WaitForCon,   // SOCKS 5 - CONNECT: cekame na vysledek pozadavku na spojeni s FTP serverem

    ssHTTP1_1_Connect,    // HTTP 1.1 - CONNECT: cekame na FD_CONNECT (probiha connect na proxy server)
    ssHTTP1_1_WaitForCon, // HTTP 1.1 - CONNECT: cekame na vysledek pozadavku na spojeni s FTP serverem

    ssSocks4_Listen,           // SOCKS 4 - LISTEN: cekame na FD_CONNECT (probiha connect na proxy server)
    ssSocks4_WaitForListenRes, // SOCKS 4 - LISTEN: cekame az proxy otevre port pro "listen" a vrati IP+port, kde posloucha (nebo vrati chybu)
    ssSocks4_WaitForAccept,    // SOCKS 4 - LISTEN: cekame az proxy prijme spojeni z FTP serveru (nebo vrati chybu)

    ssSocks4A_Listen,           // SOCKS 4A - LISTEN: cekame na FD_CONNECT (probiha connect na proxy server)
    ssSocks4A_WaitForListenRes, // SOCKS 4A - LISTEN: cekame az proxy otevre port pro "listen" a vrati IP+port, kde posloucha (nebo vrati chybu)
    ssSocks4A_WaitForAccept,    // SOCKS 4A - LISTEN: cekame az proxy prijme spojeni z FTP serveru (nebo vrati chybu)

    ssSocks5_Listen,             // SOCKS 5 - LISTEN: cekame na FD_CONNECT (probiha connect na proxy server)
    ssSocks5_ListenWaitForMeth,  // SOCKS 5 - LISTEN: cekame jakou metodu autentifikace si server vybere
    ssSocks5_ListenWaitForLogin, // SOCKS 5 - LISTEN: cekame na vysledek loginu na proxy server (poslali jsme user+password)
    ssSocks5_WaitForListenRes,   // SOCKS 5 - LISTEN: cekame az proxy otevre port pro "listen" a vrati IP+port, kde posloucha (nebo vrati chybu)
    ssSocks5_WaitForAccept,      // SOCKS 5 - LISTEN: cekame az proxy prijme spojeni z FTP serveru (nebo vrati chybu)

    ssHTTP1_1_Listen, // HTTP 1.1 - LISTEN: cekame na FD_CONNECT (probiha connect na proxy server)

    ssConnectFailed, // CONNECT: uz jen cekame na reakci na ohlasenou chybu (pres FD_CONNECT s chybou)
    ssListenFailed,  // LISTEN: uz jen cekame na reakci na ohlasenou chybu (pres ListeningForConnection() nebo ConnectionAccepted())

    ssNoProxyOrConnected, // spojeni bez proxy serveru nebo uz jsme pripojeni (proxy server je pro nas transparentni)
};

enum CProxyErrorCode
{
    pecNoError,           // zatim zadna chyba nenastala
    pecGettingHostIP,     // CONNECT: chyba ziskavani IP FTP serveru (SOCKS4)
    pecSendingBytes,      // CONNECT+LISTEN: chyba pri posilani dat proxy serveru
    pecReceivingBytes,    // CONNECT+LISTEN: chyba pri prijimani dat z proxy serveru
    pecUnexpectedReply,   // CONNECT+LISTEN: prijem neocekavane odpovedi - hlasime chybu; ProxyWinError se nepouziva
    pecProxySrvError,     // CONNECT+LISTEN: proxy server hlasi chybu pri pripojovani na FTP server; ProxyWinError je primo text-res-id
    pecNoAuthUnsup,       // CONNECT+LISTEN: proxy server nepodporuje pristup bez autentifikace
    pecUserPassAuthUnsup, // CONNECT+LISTEN: proxy server nepodporuje autentifikaci pres user+password
    pecUserPassAuthFail,  // CONNECT+LISTEN: proxy server neprijal nas user+password
    pecConPrxSrvError,    // LISTEN: chyba pri pripojovani na proxy server
    pecListenUnsup,       // HTTP 1.1: LISTEN: not supported
    pecHTTPProxySrvError, // HTTP 1.1: CONNECT: proxy server vratil chybu, jeji textovy popis je v HTTP11_FirstLineOfReply
};

enum CFTPProxyServerType;

class CSocket
{
public:
    static CRITICAL_SECTION NextSocketUIDCritSect; // kriticka sekce pocitadla (sockety se vytvari v ruznych threadech)

private:
    static int NextSocketUID; // globalni pocitadlo pro objekty socketu

protected:
    // kriticka sekce pro pristup k datum objektu
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)
    CRITICAL_SECTION SocketCritSect;

    int UID; // unikatni cislo tohoto objektu (nepresouva se ve SwapSockets())

    int Msg;                    // cislo zpravy pouzite pro prijem udalosti pro tento objekt (-1 == objekt jeste neni pripojeny)
    SOCKET Socket;              // zapouzdreny Windows Sockets socket; je-li INVALID_SOCKET, neni socket otevreny
    SSL* SSLConn;               // SSL connection, use instead of Socket if non-NULL
    int ReuseSSLSession;        // reuse SSL session teto ctrl-con pro vsechny jeji data-con: 0 = zkusit, 1 = ano, 2 = ne
    BOOL ReuseSSLSessionFailed; // TRUE = reuse SSL session pro posledni oteviranou data-con selhal: jestli se bez nej neobejdeme, nezbyva nez reconnectnout tuto ctrl-con
    CCertificate* pCertificate; // non-NULL on FTPS connections
    BOOL OurShutdown;           // TRUE pokud shutdown iniciovala tato strana (FTP client)
    BOOL IsDataConnection;      // TRUE = socket pro prenos dat, nastavime vetsi buffery (urychleni listingu, downloadu i uploadu)

    CSocketState SocketState; // stav socketu

    // data pro pripojovani pres proxy servery (firewally)
    char* HostAddress;       // jmenna adresa cilove masiny, kam se chceme pripojit
    DWORD HostIP;            // IP adresa 'HostAddress' (==INADDR_NONE dokud neni IP zname)
    unsigned short HostPort; // port cilove masiny, kam se chceme pripojit
    char* ProxyUser;         // username pro proxy server
    char* ProxyPassword;     // password pro proxy server
    DWORD ProxyIP;           // IP adresa proxy serveru (pouziva se jen pro LISTEN - jinak ==INADDR_NONE)

    CProxyErrorCode ProxyErrorCode; // kod chyby vznikle pri pripojovani na FTP server pres proxy server
    DWORD ProxyWinError;            // jestli se pouziva zalezi na hodnote ProxyErrorCode: kod windows chyby (NO_ERROR = pouzit text IDS_UNKNOWNERROR)

    BOOL ShouldPostFD_WRITE; // TRUE = FD_WRITE prisel v dobe pripojovani pres proxy server, takze ho po navazani spojeni s FTP serverem budeme muset preposlat do ReceiveNetEvent()

    char* HTTP11_FirstLineOfReply;    // neni-li NULL, je zde prvni radek odpovedi od HTTP 1.1 proxy serveru (na CONNECT request)
    int HTTP11_EmptyRowCharsReceived; // odpoved serveru konci na CRLFCRLF, zde si ukladame kolik znaku z teho sekvence uz prislo

    DWORD IsSocketConnectedLastCallTime; // 0 pokud jeste nebylo volane CSocketsThread::IsSocketConnected() pro tento socket, jinak GetTickCount() posledniho volani

public:
    CSocket();
    virtual ~CSocket();

    // ******************************************************************************************
    // metody vyuzivane objektem SocketsThread
    // ******************************************************************************************

    // pouziva objekt SocketsThread k predani cisla zpravy (indexu v poli obsluhovanych
    // socketu), kterou ma tento objekt pouzivat pro prijem udalosti; ostatni metody objektu
    // je mozne volat az po pridani objektu do SocketsThread (volanim metody SocketsThread->AddSocket)
    // volani mozne z libovolneho threadu
    void SetMsgIndex(int index);

    // vola pri odpojovani objektu ze SocketsThread
    // volani mozne z libovolneho threadu
    void ResetMsgIndex();

    // vraci index objektu v poli obsluhovanych socketu; vraci -1 pokud objekt neni v tomto poli
    // volani mozne z libovolneho threadu
    int GetMsgIndex();

    // vraci aktualni hodnotu Msg (v krit. sekci)
    // volani mozne z libovolneho threadu
    int GetMsg();

    // pouziva objekt SocketsThread k jednoznacne identifikaci tohoto objektu (cisla zprav se
    // pouzivaji opakovane - po dealokaci objektu se jeho cislo zpravy prideli nove vzniklemu)
    // volani mozne z libovolneho threadu
    int GetUID();

    // pouziva objekt SocketsThread k jednoznacne identifikaci tohoto objektu (cisla zprav se
    // pouzivaji opakovane - po dealokaci objektu se jeho cislo zpravy prideli nove vzniklemu)
    // volani mozne z libovolneho threadu
    SOCKET GetSocket();

    CCertificate* GetCertificate(); // POZOR: vraci certifikat az po volani jeho AddRef(), tedy volajici je zodpovedny za uvolneni pomoci volani Release() certifikatu
    void SetCertificate(CCertificate* certificate);

    // pouziva objekt SocketsThread pri prohazovani objektu socketu, viz
    // CSocketsThread::BeginSocketsSwap(); prohodi Msg a Socket
    // volani mozne z libovolneho threadu
    void SwapSockets(CSocket* sock);

    // vraci TRUE pokud bylo pro tento socket volano CSocketsThread::IsSocketConnected() - cas
    // volani vraci v 'lastCallTime'; vraci FALSE pokud k volani CSocketsThread::IsSocketConnected()
    // jeste nedoslo
    BOOL GetIsSocketConnectedLastCallTime(DWORD* lastCallTime);

    // nastavi IsSocketConnectedLastCallTime na soucasny GetTickCount()
    void SetIsSocketConnectedLastCallTime();

    // ******************************************************************************************
    // neblokujici metody pro praci s objektem (jsou asynchronni, vysledky prijima tento objekt
    // v "sockets" threadu - volaji se v nem "receive" metody tohoto objektu)
    // ******************************************************************************************

    // ziska IP adresu ze jmenne adresy (prip. i primo text IP adresy); 'hostUID' slouzi
    // k identifikaci vysledku pri vice volanich teto metody; vysledek (vcetne 'hostUID')
    // bude v parametrech volani metody ReceiveHostByAddress v "sockets" threadu;
    // vraci TRUE pri sanci na uspech (podari-li se spustit thread zjistujici IP adresu),
    // pri neuspechu vraci FALSE; pokud vraci TRUE a tento objekt nebyl pripojen do
    // SocketsThread, pripoji ho (viz metoda AddSocket)
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread); vyjimkou je, kdyz uz v kriticke sekci CSocketsThread::CritSect jsme
    // volani mozne z libovolneho threadu
    BOOL GetHostByAddress(const char* address, int hostUID = 0);

    // pripoji se na SOCKS 4/4A/5 nebo HTTP 1.1 (typ proxy je v 'proxyType') proxy server
    // 'serverIP' na port 'serverPort' + pokud nejde o tyto proxy servery, funguje stejne
    // jako metoda Connect; vytvori Windows socket a nastavi ho jako neblokujici - zpravy
    // posila do objektu SocketsThread, ktery podle protokolu proxy serveru provede
    // pripojeni na adresu 'host' port 'port' s proxy-user-name 'proxyUser' a proxy-password
    // 'proxyPassword' (jen SOCKS 5 a HTTP 1.1); 'hostIP' (jen SOCKS 4) je IP adresa 'host'
    // (pokud neni znama, pouzit INADDR_NONE); vraci TRUE pri sanci na uspech (vysledek
    // pripojeni na 'host' prijme metoda ReceiveNetEvent - FD_CONNECT), pri neuspechu vraci
    // FALSE a je-li znamy kod Windows chyby, vraci ho v 'error' (neni-li NULL); pokud
    // vraci TRUE a tento objekt nebyl pripojen do SocketsThread, pripoji ho (viz metoda
    // AddSocket)
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    BOOL ConnectWithProxy(DWORD serverIP, unsigned short serverPort, CFTPProxyServerType proxyType,
                          DWORD* err, const char* host, unsigned short port, const char* proxyUser,
                          const char* proxyPassword, DWORD hostIP);

    // pripoji se na IP adresu 'ip' na port 'port'; vytvori Windows socket a nastavi ho
    // jako neblokujici - zpravy posila do objektu SocketsThread, ktery na zaklade techto
    // zprav vyvolava metodu ReceiveNetEvent tohoto objektu; vraci TRUE pri sanci na uspech
    // (vysledek prijme metoda ReceiveNetEvent - FD_CONNECT), pri neuspechu vraci FALSE
    // a je-li znamy kod Windows chyby, vraci ho v 'error' (neni-li NULL); pokud vraci
    // TRUE a tento objekt nebyl pripojen do SocketsThread, pripoji ho (viz metoda AddSocket);
    // 'calledFromConnect' je TRUE jen pri volani z nektere z metod ConnectUsingXXX;
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    BOOL Connect(DWORD ip, unsigned short port, DWORD* error, BOOL calledFromConnect = FALSE);

    // pokud behem pripojovani pres proxy server vznikla chyba, vrati tato metoda
    // TRUE + chybu v 'errBuf'+'formalBuf'; jinak metoda vraci FALSE; 'errBuf' je
    // buffer pro text chyby o delce aspon 'errBufSize' znaku; 'formatBuf' je buffer
    // (o delce aspon 'formatBufSize' znaku) pro formatovaci retezec (pro sprintf)
    // popisujici pri cem chyba nastala; je-li 'oneLineText' TRUE, naplni se jen
    // 'errBuf' a to jedinym radkem textu (bez CR+LF)
    BOOL GetProxyError(char* errBuf, int errBufSize, char* formatBuf, int formatBufSize,
                       BOOL oneLineText);

    // vraci popis timeoutu, ktery nastal pri pripojovani pres proxy server; vraci FALSE
    // pokud jde o timeout pripojovani na FTP server; 'buf' je buffer pro popis timeoutu
    // o delce aspon 'bufSize' znaku
    BOOL GetProxyTimeoutDescr(char* buf, int bufSize);

    // vraci TRUE pokud neni socket zavreny (INVALID_SOCKET)
    BOOL IsConnected();

    // zahaji shutdown socketu - po uspesnem odeslani+potvrzeni neposlanych dat prijde
    // FD_CLOSE, ktery socket zavre (uvolni prostredky Windows socketu); po zahajeni
    // shutdownu jiz nelze zapsat na socket zadna data (pokud Write nezapise cely buffer
    // najednou, je nutne pockat na dokonceni zapisu - udalost ccsevWriteDone);
    // vraci TRUE pri uspechu, FALSE pri chybe - je-li znamy kod Windows chyby, vraci
    // ho v 'error' (neni-li NULL)
    // POZNAMKA: po prijeti FD_CLOSE se vola metoda SocketWasClosed (info o zavreni socketu)
    BOOL Shutdown(DWORD* error);

    // tvrde zavreni socketu (jen kdyz timeoutnul Shutdown) - vola closesocket (dealokace
    // Windows socketu); vraci TRUE pri uspechu, FALSE pri chybe - je-li znamy kod Windows
    // chyby, vraci ho v 'error' (neni-li NULL)
    BOOL CloseSocket(DWORD* error);

    // zasifruje socket, vraci TRUE pri uspechu; pokud certifikat serveru nelze ani
    // overit, ani ho predtim uzivatel neprijmul jako duveryhodny: je-li 'unverifiedCert'
    // NULL, vraci neuspech a v 'sslErrorOccured' SSLCONERR_UNVERIFIEDCERT, neni-li
    // 'unverifiedCert' NULL, vraci uspech a v 'unverifiedCert' certifikat serveru,
    // volajici je zodpovedny za jeho uvolneni volanim unverifiedCert->Release() a
    // v 'errorBuf' (o velikosti 'errorBufLen', je-li 0, muze byt 'errorBuf' NULL)
    // vraci proc nelze certifikat overit; v 'errorID' (neni-li NULL) vraci resource-id
    // textu chyby nebo -1 pokud se zadna chyba nema vypisovat; pri jinych chybach
    // (krome neduveryhodneho certifikatu): v 'unverifiedCert' vraci NULL, v 'errorBuf'
    // (o velikosti 'errorBufLen', je-li 0, muze byt 'errorBuf' NULL) vraci doplnujici
    // text k chybe (vklada se do 'errorID' na pozici %s pres sprintf); v 'sslErrorOccured'
    // (neni-li NULL) vraci chybovy kod (jeden z SSLCONERR_XXX); 'logUID' je UID logu;
    // 'conForReuse' (neni-li NULL) je socket, ze ktereho mame pouzit SSL session (rika se
    // tomu "SSL session reuse", viz napr. http://vincent.bernat.im/en/blog/2011-ssl-session-reuse-rfc5077.html
    // a pouziva se to z control connectiony pro vsechny jeji data connectiony)
    BOOL EncryptSocket(int logUID, int* sslErrorOccured, CCertificate** unverifiedCert,
                       int* errorID, char* errorBuf, int errorBufLen, CSocket* conForReuse);

    // pripoji se na SOCKS 4/4A/5 nebo HTTP 1.1 (typ proxy je v 'proxyType') proxy server
    // 'proxyIP' na port 'proxyPort' a otevre na nem port pro "listen"; IP+port, kde se
    // nasloucha, socket prijme v metode ListeningForConnection(); pokud nejde o tyto
    // proxy servery, funguje jako OpenForListening(), az na to, ze vysledek predava tez
    // pres ListeningForConnection() - vola se primo z metody OpenForListeningWithProxy();
    // 'listenOnIP'+'listenOnPort' se pouziva jen pokud nejde o pripojeni pres tyto proxy
    // servery - 'listenOnIP' je IP teto masiny (pri bindnuti socketu na multi-home masinach
    // nemusi jit IP zjistit, u FTP bereme IP z "control connection"), 'listenOnPort' je
    // port, na kterem se ma naslouchat, pokud na tom nezalezi, pouzije se hodnota 0;
    // vytvori Windows socket a nastavi ho jako neblokujici - zpravy posila do objektu
    // SocketsThread, ktery podle protokolu proxy serveru pozada o otevreni "listen" portu
    // pro pripojeni z adresy 'host' (IP 'hostIP') port 'hostPort' s proxy-user-name
    // 'proxyUser' a proxy-password 'proxyPassword' (jen SOCKS 5 a HTTP 1.1); 'hostIP'
    // (pouziva se jen pro SOCKS 4, jinak INADDR_NONE) je IP adresa 'host' (IP musi byt
    // zname - musi byt otevrene spojeni na 'hostIP', jinak nelze zadat o toto otevreni
    // "listen" portu); vraci TRUE pri sanci na uspech ("listen" IP+port prijme metoda
    // ListeningForConnection(), a pak vysledek pripojeni z 'host' prijme
    // metoda ConnectionAccepted() - za predpokladu, ze se pro FD_ACCEPT vola metoda
    // CSocket::ReceiveNetEvent); pri neuspechu vraci FALSE a je-li znamy kod Windows
    // chyby, vraci ho v 'err' (neni-li NULL) a neni-li 'listenError' NULL, vraci v nem
    // TRUE/FALSE pokud jde o chybu LISTEN(chyba listen bez proxy serveru)/CONNECT(chyba
    // connectu na proxy server); pokud vraci TRUE a tento objekt nebyl pripojen do
    // SocketsThread, pripoji ho (viz metoda AddSocket)
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    BOOL OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                   const char* host, DWORD hostIP, unsigned short hostPort,
                                   CFTPProxyServerType proxyType, DWORD proxyIP,
                                   unsigned short proxyPort, const char* proxyUser,
                                   const char* proxyPassword, BOOL* listenError, DWORD* err);

    // otevre socket a ceka na nem na spojeni (nasloucha); 'listenOnIP' (nesmi byt NULL) je na
    // vstupu IP teto masiny (pri bindnuti socketu na multi-home masinach nemusi jit IP zjistit,
    // u FTP bereme IP z "control connection"), na vystupu je IP, na kterem se ceka na spojeni;
    // 'listenOnPort' (nesmi byt NULL) je na vstupu port, na kterem se ma cekat na spojeni,
    // pokud na tom nezalezi, pouzije se hodnota 0, na vystupu jde o port, na kterem se ceka
    // na spojeni; vytvori Windows socket a nastavi ho jako neblokujici - zpravy posila do
    // objektu SocketsThread, ktery na zaklade techto zprav vyvolava metodu ReceiveNetEvent
    // tohoto objektu (prichozi spojeni je oznameno volanim metody ConnectionAccepted() - za
    // za predpokladu, ze se pro FD_ACCEPT vola metoda CSocket::ReceiveNetEvent); vraci TRUE
    // pri uspesnem otevreni socketu, pri neuspechu vraci FALSE a je-li znamy kod Windows chyby,
    // vraci ho v 'error' (neni-li NULL); pokud vraci TRUE a tento objekt nebyl pripojen do
    // SocketsThread, pripoji ho (viz metoda AddSocket)
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    BOOL OpenForListening(DWORD* listenOnIP, unsigned short* listenOnPort, DWORD* error);

    // vraci lokalni IP adresu, socket musi byt pripojeny (viz funkce getsockname());
    // v 'ip' (nesmi byt NULL) vraci zjistenou IP adresu; vraci TRUE pri uspechu;
    // pri neuspechu vraci FALSE a je-li znamy kod Windows chyby, vraci ho v 'error'
    // (neni-li NULL)
    // volani mozne z libovolneho threadu
    BOOL GetLocalIP(DWORD* ip, DWORD* error);

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
    virtual void SocketWasClosed(DWORD error) {}

    // prijem timeru s ID 'id' a parametrem 'param'
    virtual void ReceiveTimer(DWORD id, void* param) {}

    // prijem postnute zpravy s ID 'id' a parametrem 'param'
    virtual void ReceivePostMessage(DWORD id, void* param) {}

    // interni metoda tohoto objektu: resi pripojeni na proxy server, po pripojeni nebo chybe
    // pripojeni presmeruje udalosti do virtualni metody ReceiveNetEvent()
    void ReceiveNetEventInt(LPARAM lParam, int index);

    // interni metoda tohoto objektu: resi pripojeni na proxy server - konkretne u Socks4 cekani
    // na IP adresu FTP serveru, vse ostatni presmeruje do metody ReceiveHostByAddress();
    // 'index' je index socketu v poli SocketsThread->Sockets (pouziva se pri volani ReceiveNetEvent())
    void ReceiveHostByAddressInt(DWORD ip, int hostUID, int err, int index);

    // vola se po otevreni socketu pro "listen": 'listenOnIP'+'listenOnPort' je IP+port, na kterem se
    // nasloucha; 'proxyError' je TRUE pokud proxy server hlasi chybu pri otevirani socketu pro
    // "listen" (text chyby lze ziskat pres GetProxyError())
    // POZOR: pri pripojeni bez proxy serveru se tato metoda vola primo z OpenForListeningWithProxy(),
    //        tedy nemusi to byt ze "sockets" threadu
    // POZOR: nesmi se volat z kriticke sekce CSocket::SocketCritSect
    virtual void ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                        BOOL proxyError) {}

    // vola se po prijeti a zpracovani FD_ACCEPT (za predpokladu, ze se pro FD_ACCEPT vola metoda
    // CSocket::ReceiveNetEvent): 'success' je uspech acceptu, pri neuspechu je ve 'winError'
    // windowsovy kod chyby (pouziva se jen pri pripojeni bez proxy serveru) a v 'proxyError'
    // TRUE pokud jde o chybu hlasenou proxy serverem (text chyby lze ziskat pres GetProxyError())
    // POZOR: volat jen po jednom vstupu do kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError) {}

protected:
    // pomocna metoda pro nastaveni dat pro pripojeni pres proxy server
    // POZOR: volat jen ze sekce SocketCritSect
    BOOL SetProxyData(const char* hostAddress, unsigned short hostPort,
                      const char* proxyUser, const char* proxyPassword,
                      DWORD hostIP, DWORD* error, DWORD proxyIP);

    // pomocna metoda: posle byty z 'buf' na socket; 'index' je index socketu v poli
    // SocketsThread->Sockets (pouziva se pri volani ReceiveNetEvent()); uvnitr metody
    // se muze opustit kriticka sekce SocketCritSect, v tomto pripade ze v 'csLeft'
    // vraci TRUE; 'isConnect' je TRUE/FALSE pro CONNECT/LISTEN
    // POZOR: volat jen pri jednom zanoreni do sekce SocketCritSect
    void ProxySendBytes(const char* buf, int bufLen, int index, BOOL* csLeft, BOOL isConnect);

    // pomocne metody: 'index' je index socketu v poli SocketsThread->Sockets (pouziva se
    // pri volani ReceiveNetEvent()); uvnitr metody se muze opustit kriticka sekce
    // SocketCritSect, v tomto pripade ze v 'csLeft' vraci TRUE
    // POZOR: volat jen pri jednom zanoreni do sekce SocketCritSect
    //
    // posle request 'request' (1=CONNECT, 2=LISTEN) na SOCKS4 proxy server; 'isConnect' je
    // TRUE/FALSE pro CONNECT/LISTEN; 'isSocks4A' je TRUE/FALSE pro SOCKS 4A/4
    void Socks4SendRequest(int request, int index, BOOL* csLeft, BOOL isConnect, BOOL isSocks4A);
    // posle seznam podporovanych autentifikacnich metod na SOCKS5 proxy server; 'isConnect'
    // je TRUE/FALSE pro CONNECT/LISTEN
    void Socks5SendMethods(int index, BOOL* csLeft, BOOL isConnect);
    // posle request 'request' (1=CONNECT, 2=LISTEN) na SOCKS5 proxy server; 'isConnect' je
    // TRUE/FALSE pro CONNECT/LISTEN
    void Socks5SendRequest(int request, int index, BOOL* csLeft, BOOL isConnect);
    // posle username+password na SOCKS5 proxy server; 'isConnect' je TRUE/FALSE pro CONNECT/LISTEN
    void Socks5SendLogin(int index, BOOL* csLeft, BOOL isConnect);
    // posle pozadavek (na otevreni spojeni na FTP server) na HTTP 1.1 proxy server
    void HTTP11SendRequest(int index, BOOL* csLeft);

    // pomocna metoda: prijem odpovedi z proxy serveru; vraci TRUE pokud se na socketu
    // objevi nejaka data (pokud prisla FD_CLOSE, vrati data + dalsi zpracovani FD_CLOSE
    // je na volajicim); POZOR: pokud vraci FALSE, opustil sekci SocketCritSect; 'buf'
    // je buffer pro tuto odpoved ('read' je na vstupu velikost bufferu 'buf', na
    // vystupu je zde pocet prectenych bytu); 'index' je index socketu v poli
    // SocketsThread->Sockets (pouziva se pri volani ReceiveNetEvent()); 'isConnect' je
    // TRUE/FALSE pri CONNECT/LISTEN_nebo_ACCEPT; 'isListen' ma smysl jen pri
    // 'isConnect'==FALSE a je TRUE/FALSE pri LISTEN/ACCEPT; je-li 'readOnlyToEOL' TRUE,
    // cte se socket po jednom bytu jen do okamziku vyskytu prvniho LF (nacte max.
    // jeden radek vcetne koncoveho LF)
    // POZOR: volat jen pri jednom zanoreni do sekce SocketCritSect a aspon jednom
    //        zanoreni do sekce CSocketsThread::CritSect
    BOOL ProxyReceiveBytes(LPARAM lParam, char* buf, int* read, int index,
                           BOOL isConnect, BOOL isListen, BOOL readOnlyToEOL);

    // pomocna metoda: nastavime vetsi buffery na socketu data-connectiony (urychleni
    // listingu, downloadu i uploadu)
    void SetSndRcvBuffers();
};

//
// ****************************************************************************
// CSocketsThread
//
// thread obsluhy vsech socketu (obsahuje neviditelne okno pro prijem zprav),
// pouzivaji ho objekty socketu, mimo moduly socketu neni potreba

struct CMsgData // data pro WM_APP_SOCKET_ADDR
{
    int SocketMsg;
    int SocketUID;
    DWORD IP;
    int HostUID;
    int Err;

    CMsgData(int socketMsg, int socketUID, DWORD ip, int hostUID, int err)
    {
        SocketMsg = socketMsg;
        SocketUID = socketUID;
        IP = ip;
        HostUID = hostUID;
        Err = err;
    }
};

#define SOCKETSTHREAD_TIMERID 666 // ID Windows timeru CSocketsThread::Timers (viz CSocketsThread::AddTimer())

struct CTimerData // data pro WM_TIMER (viz CSocketsThread::AddTimer())
{
    int SocketMsg;    // cislo zpravy pouzite pro prijem udalosti pro informovany socket; je-li (WM_APP_SOCKET_MIN-1), jde o smazany timer v uzamcene oblasti pole Timers
    int SocketUID;    // UID informovaneho socketu
    DWORD TimeoutAbs; // absolutni cas v milisekundach, ke spusteni timeru dojde az GetTickCount() vrati minimalne tuto hodnotu
    DWORD ID;         // id timeru
    void* Param;      // parametr timeru

    CTimerData(int socketMsg, int socketUID, DWORD timeoutAbs, DWORD id, void* param)
    {
        SocketMsg = socketMsg;
        SocketUID = socketUID;
        TimeoutAbs = timeoutAbs;
        ID = id;
        Param = param;
    }
};

struct CPostMsgData // data pro WM_APP_SOCKET_POSTMSG
{
    int SocketMsg; // cislo zpravy pouzite pro prijem udalosti pro informovany socket
    int SocketUID; // UID informovaneho socketu
    DWORD ID;      // id udalosti
    void* Param;   // parametr udalosti

    CPostMsgData(int socketMsg, int socketUID, DWORD id, void* param)
    {
        SocketMsg = socketMsg;
        SocketUID = socketUID;
        ID = id;
        Param = param;
    }
};

class CSocketsThread : public CThread
{
protected:
    CRITICAL_SECTION CritSect; // kriticka sekce pro pristup k datum objektu
    HANDLE RunningEvent;       // signaled az po spusteni message-loopy v threadu
    HANDLE CanEndThread;       // signaled az po volani IsRunning() - hl. thread uz si precetl 'Running'
    BOOL Running;              // TRUE po uspesnem spusteni message-loopy v threadu (jinak hlasi error)
    HWND HWindow;              // handle neviditelneho okna pro prijem zprav o socketech
    BOOL Terminating;          // TRUE jen po volani Terminate()

    // pole obsluhovanych objektu (potomku CSocket); pozice v poli odpovidaji prijimanym
    // zpravam (index nula == WM_APP_SOCKET_MIN), takze PRVKY POLE NELZE POSOUVAT (vymaz
    // se resi prepisem indexu na NULL)
    TIndirectArray<CSocket> Sockets;
    int FirstFreeIndexInSockets; // nejnizsi volny index uvnitr pole Sockets (-1 = zadny)

    TIndirectArray<CMsgData> MsgData; // data pro WM_APP_SOCKET_ADDR (pri prijmu zpravy distribujeme data z pole)

    TIndirectArray<CTimerData> Timers;    // pole timeru pro sockety
    int LockedTimers;                     // usek pole Timers (pocet prvku od zacatku pole), ktery se nesmi menit (pouziva se behem CSocketsThread::ReceiveTimer()); -1 = takovy usek pole neexistuje
    static DWORD LastWM_TIMER_Processing; // GetTickCount() z okamziku posledniho zpracovani WM_TIMER (WM_TIMER chodi jen pri "idle" messageloopy, coz je pro nas nepripustne)

    TIndirectArray<CPostMsgData> PostMsgs; // data pro WM_APP_SOCKET_POSTMSG (pri prijmu zpravy distribujeme data z pole)

public:
    CSocketsThread();
    ~CSocketsThread();

    // vraci stav objektu (TRUE=OK); nutne volat po konstruktoru pro zjisteni vysledku konstrukce
    BOOL IsGood() { return Sockets.IsGood() && RunningEvent != NULL && CanEndThread != NULL; }

    // vraci handle neviditelneho okna (okno pro prijem zprav o socketech)
    HWND GetHiddenWindow() const { return HWindow; }

    void LockSocketsThread() { HANDLES(EnterCriticalSection(&CritSect)); }
    void UnlockSocketsThread() { HANDLES(LeaveCriticalSection(&CritSect)); }

    // vola hl. thread - pocka, az bude jasne jestli thread nabehl, pak vrati TRUE
    // pri uspesnem behu nebo FALSE pri chybe
    BOOL IsRunning();

    // vola hl. thread, pokud je treba terminovat sockets thread
    void Terminate();

    // prida do pole timeru timer s timeoutem v 'timeoutAbs' (absolutni cas v milisekundach,
    // ke spusteni timeru dojde az GetTickCount() vrati minimalne tuto hodnotu);
    // po spusteni timeru dojde k jeho zruseni z pole timeru; 'socketMsg'+'socketUID' identifikuje
    // socket, kteremu se ma timeout pridavaneho timeru hlasit (viz metoda CSocket::ReceiveTimer());
    // 'id' je ID timeru; 'param' je volitelny parametr timeru, pokud je v nem nejaka alokovana
    // hodnota, musi se o dealokaci postarat objekt CSocket pri prijmu timeru, pri chybe pridavani
    // timeru nebo pri unloadu pluginu; vraci TRUE pri uspesnem pridani timeru, jinak vraci FALSE
    // (jedina chyba nedostatek pameti)
    // volani mozne z libovolneho threadu
    BOOL AddTimer(int socketMsg, int socketUID, DWORD timeoutAbs, DWORD id, void* param);

    // najde a zrusi z pole timeru timer s ID 'id' pro socket s UID 'socketUID';
    // pokud je v poli vic timeru, ktere odpovidaji kriteriu, dojde k vymazu vsech;
    // vraci TRUE pokud byl nalezen a zrusen aspon jeden timer
    // volani mozne z libovolneho threadu
    BOOL DeleteTimer(int socketUID, DWORD id);

    // vlozi do fronty postnutou zpravu - neprimo vyvola metodu CSocket::ReceivePostMessage;
    // zprava nemusi byt prijata k doruceni (nedostatek pameti nebo chyba PostMessage) - pak
    // vraci FALSE; 'socketMsg' a 'socketUID' identifikuje objekt socketu, kam ma zprava dojit;
    // 'id' a 'param' jsou parametry predane do CSocket::ReceivePostMessage, pokud je
    // v 'param' nejaka alokovana hodnota, musi se o dealokaci postarat objekt CSocket
    // pri prijmu zpravy, pri chybe PostSocketMessage nebo pri unloadu pluginu
    // volani mozne z libovolneho threadu
    BOOL PostSocketMessage(int socketMsg, int socketUID, DWORD id, void* param);

    // zjisti jestli existuje objekt socketu s UID 'socketUID';
    // pokud existuje vraci TRUE (jinak FALSE); pokud socket existuje vraci
    // v 'isConnected' (neni-li NULL) TRUE, pokud neni socket zavreny (INVALID_SOCKET),
    // jinak v 'isConnected' vraci FALSE; nastavi v socketu cas posledniho volani
    // IsSocketConnected() - IsSocketConnectedLastCallTime
    BOOL IsSocketConnected(int socketUID, BOOL* isConnected);

    // prida do pole obsluhovanych objektu 'sock' (musi byt alokovany); vraci TRUE pri uspechu
    // ('sock' bude dealokovan automaticky pri zavreni socketu), pokud vraci FALSE, je potreba
    // objekt 'sock' dealokovat
    // volani mozne z libovolneho threadu
    BOOL AddSocket(CSocket* sock);

    // dealokuje/odpojuje ('onlyDetach' je FALSE/TRUE) objekt socketu 'sock' z pole obsluhovanych
    // objektu (zapise na pozici NULL a nastavi FirstFreeIndexInSockets); pokud objekt neni v poli
    // a 'onlyDetach' je TRUE, dojde k jeho dealokaci
    // volani mozne z libovolneho threadu
    void DeleteSocket(CSocket* sock, BOOL onlyDetach = FALSE)
    {
        if (sock != NULL)
        {
            int i = sock->GetMsgIndex();
            if (i != -1)
                DeleteSocketFromIndex(i, onlyDetach); // je-li v poli obsluhovanych objektu
            else
            {
                if (!onlyDetach)
                {
#ifdef _DEBUG
                    BOOL old = InDeleteSocket;
                    InDeleteSocket = TRUE; // sice nemusime byt primo v ::DeleteSocket, ale volani je korektni
#endif
                    delete sock; // neni v poli, uvolnime ho primo
#ifdef _DEBUG
                    InDeleteSocket = old;
#endif
                }
            }
        }
    }

    // dealokuje/odpojuje ('onlyDetach' je FALSE/TRUE) objekt socketu na pozici 'index' v poli
    // obsluhovanych objektu (zapise na pozici NULL a nastavi FirstFreeIndexInSockets)
    // volani mozne z libovolneho threadu
    void DeleteSocketFromIndex(int index, BOOL onlyDetach = FALSE);

    // dvojice metod umoznujici prohozeni objektu socketu - nedorucene a nove udalosti na
    // socketu, vysledky volani GetHostByAddress, timery a postnute zpravy se doruci
    // do prohozeneho objektu socketu; BeginSocketsSwap() zajisti vstup do kriticke
    // sekce sockets threadu (CritSect) a prohozeni objektu socketu; po volani
    // BeginSocketsSwap() je mozne prohodit vnitrni data objektu socketu (mimo dat tridy
    // CSocket, ty uz jsou prohozene) bez rizika, ze bude objektum socketu dorucena
    // nejaka zprava v sockets threadu; pro dokonceni prohozeni je nutne zavolat
    // EndSocketsSwap(), ktery opusti kritickou sekci sockets threadu (CritSect), cimz
    // povoli normalni funkci sockets threadu; 'sock1' a 'sock2' (nesmi byt NULL a
    // musi byt v poli obsluhovanych objektu - viz metoda AddSocket()) jsou objekty
    // prohazovanych socketu
    void BeginSocketsSwap(CSocket* sock1, CSocket* sock2);
    void EndSocketsSwap();

    // ******************************************************************************************
    // soukrome metody objektu, nevolat zvenci (mimo sockets.cpp)
    // ******************************************************************************************

    // vlozi do fronty zpravu o vysledku volani CSocket::GetHostByAddress - neprimo vyvola
    // metodu CSocket::ReceiveHostByAddress; zprava nemusi byt prijata k doruceni (nedostatek
    // pameti nebo chyba PostMessage) - pak vraci FALSE; 'socketMsg' a 'socketUID' identifikuje
    // objekt socketu, kam ma zprava dojit; 'ip', 'hostUID' a 'err' jsou parametry predane
    // do CSocket::ReceiveHostByAddress
    // volani mozne z libovolneho threadu
    BOOL PostHostByAddressResult(int socketMsg, int socketUID, DWORD ip, int hostUID, int err);

    // vola CSocketsThread::WindowProc pri prijmu WM_APP_SOCKET_ADDR
    void ReceiveMsgData();

    // vola CSocketsThread::WindowProc pri prijmu WM_TIMER
    void ReceiveTimer();

    // vola CSocketsThread::WindowProc pri prijmu WM_APP_SOCKET_POSTMSG
    void ReceivePostMessage();

    // vola CSocketsThread::WindowProc pri prijmu WM_APP_SOCKET_MIN az WM_APP_SOCKET_MAX
    LRESULT ReceiveNetEvent(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // tato metoda obsahuje telo threadu
    virtual unsigned Body();

protected:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // najde index v poli Timers, kam by se mel vlozit novy timer s timeoutem 'timeoutAbs'
    // pokud jiz v poli existuji timery s timto timeoutem, novy timer se vklada za ne
    // (zachovava casovou souslednost); 'leftIndex' je prvni index na ktery je mozne
    // novy timer vlozit (pouziva se pri zamknutem zacatku pole Timers)
    // POZOR: volat jen ze sekce 'CritSect'
    int FindIndexForNewTimer(DWORD timeoutAbs, int leftIndex);
};

extern CSocketsThread* SocketsThread; // thread obsluhy vsech socketu, jen vnitrni pouziti v modulech socketu
