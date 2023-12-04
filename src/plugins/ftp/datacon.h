// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define DEBUGLOGPACKETSIZEANDWRITESIZE    // povoleno = pisou se do logu aktualni velikosti bloku posilanych pri uploadu na socket a navic kazdou vterinu kolik se podarilo odeslat dat

// ****************************************************************************
// konstanty:

#define DATACON_BYTESTOREADONSOCKET 8192         // po minimalne kolika bytech se ma cist socket (take alokovat buffer pro prectena data)
#define DATACON_BYTESTOREADONSOCKETPREALLOC 8192 // kolik bytu se ma predalokovat pro cteni (aby dalsi cteni okamzite nealokovalo znovu)

#define KEEPALIVEDATACON_READBUFSIZE 8192 // po kolika bytech se ma cist socket (data se zahazuji - jen keep-alive)

#define DATACON_FLUSHBUFFERSIZE 65536     // velikost bufferu pro predavani dat pro overeni/zapis na disk (viz CDataConnectionSocket::FlushBuffer)
#define DATACON_FLUSHTIMEOUT 1000         // doba v milisekundach, za kterou dojde k predani dat pro overeni/zapis na disk v pripade, ze k predani dat nedoslo na zaklade naplneni flush bufferu (viz CDataConnectionSocket::FlushBuffer)
#define DATACON_TESTNODATATRTIMEOUT 10000 // doba v milisekundach, za kterou periodicky dochazi k testovani no-data-transfer timeoutu

#define DATACON_FLUSHTIMERID 50        // ID timeru v data-connectione, ktery zajistuje periodicke flushovani dat (viz DATACON_FLUSHTIMEOUT)
#define DATACON_TESTNODATATRTIMERID 51 // ID timeru v data-connectione, ktery zajistuje periodicke testovani no-data-transfer timeoutu (viz DATACON_TESTNODATATRTIMEOUT)

#define DATACON_DISKWORKWRITEFINISHED 40 // ID zpravy postnute data-connectione v okamziku, kdy FTPDiskThread dokonci zadanou praci na disku - zapis (jen pri primem flushovani dat do souboru v data-connectione, viz CDataConnectionSocket::SetDirectFlushParams)

// POZOR: krome DATACON_UPLOADFLUSHBUFFERSIZE je dulezity tez odhad velikosti "paketu" pri zapisu (viz CUploadDataConnectionSocket::LastPacketSizeEstimation)
#define DATACON_UPLOADFLUSHBUFFERSIZE 65536 // velikost bufferu pro predavani dat pro zapis do data-connectiony (viz CUploadDataConnectionSocket::FlushBuffer)

//
// *********************************************************************************
// CDataConnectionBaseSocket + CDataConnectionSocket + CUploadDataConnectionSocket
//
// objekt socketu, ktery slouzi jako "data connection" na FTP server
// pro dealokaci pouzivat funkci ::DeleteSocket!

class CDataConnectionBaseSocket : public CSocket
{
protected:
    // kriticka sekce pro pristup k datum objektu je CSocket::SocketCritSect
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)

    BOOL UsePassiveMode; // TRUE = pasivni mod (PASV), jinak aktivni mod (PORT)
    BOOL EncryptConnection;
    int CompressData;                 // 0 = nepouziva se MODE Z, neni-li 0, MODE Z se pouziva (prenos komprimovanych dat)
    CFTPProxyForDataCon* ProxyServer; // NULL = "not used (direct connection)" (read-only, inicializuje se v konstruktoru, pristup tedy mozny bez kriticke sekce)
    DWORD ServerIP;                   // IP serveru, kam se pripojujeme v pasivnim modu
    unsigned short ServerPort;        // port serveru, kam se pripojujeme v pasivnim modu

    int LogUID; // UID logu pro odpovidajici "control connection" (-1 dokud neni log zalozen)

    DWORD NetEventLastError; // kod posledni chyby, ktera byla hlasena do ReceiveNetEvent() nebo nastala v PassiveConnect()
    int SSLErrorOccured;     // viz konstanty SSLCONERR_XXX
    BOOL ReceivedConnected;  // TRUE = doslo k otevreni (connect nebo accept) "data connection" (nic nerika o soucasnem stavu)
    DWORD LastActivityTime;  // GetTickCount() z okamziku, kdy se naposledy delalo s "data connection" (sleduje se inicializace (SetPassive() a SetActive()), uspesny connect a read)
    DWORD SocketCloseTime;   // GetTickCount() z okamziku, kdy se zavrel socket "data connection"

    CSynchronizedDWORD* GlobalLastActivityTime; // objekt pro ukladani casu posledni aktivity na skupine data-connection

    BOOL PostMessagesToWorker;        // TRUE = posilat zpravy majiteli (workerovi - jeho identifikace viz dale)
    int WorkerSocketMsg;              // identifikace majitele data-connectiony
    int WorkerSocketUID;              // identifikace majitele data-connectiony
    DWORD WorkerMsgConnectedToServer; // cislo zpravy, kterou si majitel data-connection preje dostat po navazani spojeni se serverem (do parametru 'param' volani PostSocketMessage jde UID tohoto objektu)
    DWORD WorkerMsgConnectionClosed;  // cislo zpravy, kterou si majitel data-connection preje dostat po uzavreni/preruseni spojeni se serverem (do parametru 'param' volani PostSocketMessage jde UID tohoto objektu)
    DWORD WorkerMsgListeningForCon;   // cislo zpravy, kterou si majitel data-connection preje dostat po otevreni portu pro "listen" (do parametru 'param' volani PostSocketMessage jde UID tohoto objektu)

    BOOL WorkerPaused;         // TRUE = worker je ve stavu "paused", takze nesmime prenaset dalsi data
    int DataTransferPostponed; // 1 = behem WorkerPaused==TRUE byl odlozen pozadavek na preneseni dat (FD_READ/FD_WRITE), 2 = byl odlozen FD_CLOSE

    CTransferSpeedMeter TransferSpeedMeter;        // objekt pro mereni rychlosti prenosu dat v teto data-connectione
    CTransferSpeedMeter ComprTransferSpeedMeter;   // objekt pro mereni rychlosti prenosu komprimovanych dat v teto data-connectione (pouziva se jen pri zapnutem MODE Z)
    CTransferSpeedMeter* GlobalTransferSpeedMeter; // objekt pro mereni celkove rychlosti prenosu dat (vsech data-connection workeru FTP operace); je-li NULL, neexistuje

    DWORD ListenOnIP;            // IP, na kterem nasloucha proxy server (ceka na spojeni); je-li INADDR_NONE, nebyla zatim volana metoda ListeningForConnection() nebo proxy server vratil nejakou chybu
    unsigned short ListenOnPort; // port, na kterem nasloucha proxy server (ceka na spojeni)
    CSalZLIB ZLIBInfo;           // Control structure for ZLIB compression

    // SSLConForReuse: neni-li NULL, je socket, ze ktereho mame pouzit SSL session (rika se
    // tomu "SSL session reuse", viz napr. http://vincent.bernat.im/en/blog/2011-ssl-session-reuse-rfc5077.html
    // a pouziva se to z control connectiony pro vsechny jeji data connectiony)
    // POZOR: pouziva se bez synchronizace, funguje na zaklade predpokladu, ze SSLConForReuse
    //        (control connectiona) existuje urcite dele nez tento socket (data connectiona)
    CSocket* SSLConForReuse;

public:
    CDataConnectionBaseSocket(CFTPProxyForDataCon* proxyServer, int encryptConnection,
                              CCertificate* certificate, int compressData, CSocket* conForReuse);
    virtual ~CDataConnectionBaseSocket();

    // popis viz potomci tohoto objektu
    virtual BOOL CloseSocketEx(DWORD* error) = 0;

    // nastavi parametry pro pasivni mod "data connection"; tyto parametry se pouzivaji
    // v metode PassiveConnect(); 'ip' + 'port' jsou parametry pripojeni ziskane od serveru;
    // 'logUID' je UID logu "control connection", ktere patri tato "data connection"
    void SetPassive(DWORD ip, unsigned short port, int logUID);

    // cisteni promennych pred connectem data-socketu
    virtual void ClearBeforeConnect() {}

    // zavola Connect() pro ulozene parametry pro pasivni mod
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    BOOL PassiveConnect(DWORD* error);

    // nastaveni objektu pro ukladani casu posledni aktivity na skupine data-connection
    // (vsech data-connection workeru FTP operace)
    // volani mozne z libovolneho threadu
    void SetGlobalLastActivityTime(CSynchronizedDWORD* globalLastActivityTime);

    // nastavi aktivni mod "data connection"; otevreni socketu viz CSocket::OpenForListening();
    // 'logUID' je UID logu "control connection", ktere patri tato "data connection"
    // volani mozne z libovolneho threadu
    void SetActive(int logUID);

    // vraci TRUE, pokud je "data connection" otevrena pro prenos dat (uz doslo ke spojeni
    // se serverem); v 'transferFinished' (neni-li NULL) vraci TRUE pokud jiz prenos
    // dat probehl (FALSE = jeste nedoslo ke spojeni se serverem)
    BOOL IsTransfering(BOOL* transferFinished);

    // vraci LastActivityTime (ma kritickou sekci)
    DWORD GetLastActivityTime();

    // vola se v okamziku, kdy ma zacit datovy prenos (napr. po poslani prikazu pro listovani);
    // v pasivnim modu se zkontroluje jestli nedoslo k odmitnuti prvniho pokusu o navazani
    // spojeni, pripadne se provede dalsi pokus o navazani spojeni; v aktivnim modu se nedeje
    // nic (mohlo by tu byt povoleni navazani spojeni ("accept"), ale pro vetsi obecnost je
    // navazani spojeni mozne stale);
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    virtual void ActivateConnection();

    // vraci SocketCloseTime (ma kritickou sekci)
    DWORD GetSocketCloseTime();

    // vraci LogUID (v kriticke sekci)
    int GetLogUID();

    // pokud jde o pasivni rezim a je zapnute sifrovani data connectiony, zkusi ji zasifrovat
    void EncryptPassiveDataCon();

    // nastaveni objektu pro mereni celkove rychlosti prenosu dat (vsech data-connection
    // workeru FTP operace)
    // volani mozne z libovolneho threadu
    void SetGlobalTransferSpeedMeter(CTransferSpeedMeter* globalTransferSpeedMeter);

    // jen zapouzdrene volani CSocket::OpenForListeningWithProxy() s parametry z 'ProxyServer'
    BOOL OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                   BOOL* listenError, DWORD* err);

    // zapamatuje si "listen" IP+port (kde se ceka na spojeni) a jestli nedoslo k chybe,
    // pak posle WorkerMsgListeningForCon vlastnikovi data-connectiony
    // POZOR: pri pripojeni bez proxy serveru se tato metoda vola primo z OpenForListeningWithProxy(),
    //        tedy nemusi to byt ze "sockets" threadu
    // POZOR: nesmi se volat z kriticke sekce CSocket::SocketCritSect
    virtual void ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                        BOOL proxyError);

    // pokud se uspesne otevrel "listen" port, vraci TRUE + "listen" IP+port
    // v 'listenOnIP'+'listenOnPort'; pri chybe "listen" vraci FALSE
    BOOL GetListenIPAndPort(DWORD* listenOnIP, unsigned short* listenOnPort);

protected:
    // pokud je PostMessagesToWorker==TRUE, postne zpravu 'msgID' workerovi;
    // POZOR: musi se volat z kriticke sekce SocketCritSect + musi jit o jedine vnoreni
    //        do teto sekce
    void DoPostMessageToWorker(int msgID);

    // vypise do logu chybu z NetEventLastError
    void LogNetEventLastError(BOOL canBeProxyError);

private:
    // zastineni metody CSocket::CloseSocket()
    BOOL CloseSocket(DWORD* error) { TRACE_E("Use CloseSocketEx() instead of CloseSocket()!"); }
};

class CDataConnectionSocket : public CDataConnectionBaseSocket
{
protected:
    // kriticka sekce pro pristup k datum objektu je CSocket::SocketCritSect
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)

    char* ReadBytes;              // buffer pro prectene byty ze socketu (ctou se po prijeti FD_READ)
    int ValidBytesInReadBytesBuf; // pocet platnych bytu v bufferu 'ReadBytes'
    int ReadBytesAllocatedSize;   // alokovana velikost bufferu 'ReadBytes'
    BOOL ReadBytesLowMemory;      // TRUE = nedostatek pameti pro ctena data

    CQuadWord TotalReadBytesCount; // celkovy pocet bytu prectenych z data-connectiony; pri kompresi je zde velikost dekomprimovanych dat prectenych z data-connectiony

    // sdilene mezi vsemi thready - "signaled" po uzavreni "data connection" (po prijeti vsech bytu) a take po dokonceni flushe na disk (viz TgtDiskFileName)
    HANDLE TransferFinished;

    HWND WindowWithStatus;  // handle okna, ktere ma prijimat zpravy o zmene stavu "data connection" (NULL = nic neposilat)
    UINT StatusMessage;     // zprava, ktera se posila pri zmene stavu "data connection"
    BOOL StatusMessageSent; // TRUE = predchozi zprava jeste nebyla dorucena, nema smysl posilat dalsi

    DWORD WorkerMsgFlushData; // cislo zpravy, kterou si majitel data-connection preje dostat jakmile jsou pripravena data ve flush-bufferu pro overeni/zapis na disk (do parametru 'param' volani PostSocketMessage jde UID tohoto objektu)

    BOOL FlushData;              // TRUE pokud se data maji predavat pro overeni/zapis na disk, FALSE=data prijdou komplet do pameti 'ReadBytes'
    char* FlushBuffer;           // flush-buffer pro predani dat pro overeni/zapis na disk (NULL pokud neni alokovany (ValidBytesInFlushBuffer==0) nebo pokud je predany ve workerovi aneb disk-threadu)
    int ValidBytesInFlushBuffer; // pocet platnych bytu v bufferu 'FlushBuffer' (POZOR: 'FlushBuffer' muze byt i NULL, pokud je predany ve workerovi aneb disk-threadu); je-li > 0, cekame az worker dokonci predani dat pro overeni/zapis na disk (zacina postnutim 'WorkerMsgFlushData' workerovi, konci vyvolanim metody FlushDataFinished tohoto objektu z workera)

    char* DecomprDataBuffer;             // dekomprimovana data (z flush-bufferu) pro overeni/zapis na disk (NULL pokud neni alokovany (DecomprDataAllocatedSize==0) nebo pokud je predany ve workerovi aneb disk-threadu)
    int DecomprDataAllocatedSize;        // alokovana velikost bufferu 'DecomprDataBuffer'
    int DecomprDataDelayedBytes;         // pocet bytu v 'DecomprDataBuffer', kterych overeni/zapis na disk bylo odlozeno na dalsi cyklus GiveFlushData & FlushDataFinished
    int AlreadyDecomprPartOfFlushBuffer; // kde ma zacit dalsi kolo dekomprimace dat z 'FlushBuffer' do 'DecomprDataBuffer'

    int NeedFlushReadBuf; // 0 = nic; 1, 2 a 3 = potrebuje flushnout data; je-li 2 nebo 3, potrebuje pak jeste postnout: 2 = FD_READ, 3 = FD_CLOSE
    BOOL FlushTimerAdded; // TRUE pokud existuje timer pro flushnuti dat (uplatni se jen pokud predtim nedojde k flushnuti z duvodu naplneni read-bufferu)

    CQuadWord DataTotalSize; // celkova velikost prenasenych dat v bytech: -1 = neznama

    char TgtDiskFileName[MAX_PATH];           // neni-li "", jde o plne jmeno diskoveho souboru, kam se maji flushnout data (soubor se prepisuje, zadne resumy se zde nedelaji)
    HANDLE TgtDiskFile;                       // cilovy diskovy soubor pro flush dat (NULL = zatim jsme ho neotevreli)
    BOOL TgtDiskFileCreated;                  // TRUE pokud byl vytvoren cilovy diskovy soubor pro flush dat
    DWORD TgtFileLastError;                   // kod posledni chyby, ktera byla hlasena z disk-threadu ze zapisu do souboru TgtDiskFileName
    CQuadWord TgtDiskFileSize;                // aktualni velikost diskoveho souboru pro flush dat
    CCurrentTransferMode CurrentTransferMode; // soucasny rezim prenosu (ascii/binar)
    BOOL AsciiTrModeForBinFileProblemOccured; // TRUE = detekovana chyba "ascii rezim pro binarni soubor"
    int AsciiTrModeForBinFileHowToSolve;      // jak resit problem "ascii rezim pro binarni soubor": 0 = zeptat se uzivatele, 1 = downloadnout znovu v binary rezimu, 2 = prerusit download souboru (cancel), 3 = ignorovat (dokoncit download v ascii rezimu)
    BOOL TgtDiskFileClosed;                   // TRUE = soubor pro flush dat uz je zavreny (zadne dalsi zapisovani uz se neprovede)
    int TgtDiskFileCloseIndex;                // index zavreni souboru v disk-work threadu (-1 = nepouzity)
    BOOL DiskWorkIsUsed;                      // TRUE pokud je DiskWork vlozeny v FTPDiskThread

    BOOL NoDataTransTimeout;  // TRUE = nastal no-data-transfer timeout - zavreli jsme data-connectionu
    BOOL DecomprErrorOccured; // TRUE = nastala chyba pri dekompresi dat (MODE Z) - prisla corruptla data
    //BOOL DecomprMissingStreamEnd;   // bohuzel tenhle test je nepruchozi, napr. Serv-U 7 a 8 proste stream neukoncuje // TRUE = pri dekompresi jsme zatim nenarazili na konec streamu (nekompletni dekomprese) - pouziva se jen pri FlushData==TRUE (tedy ne pro ziskavani listingu, tam probiha dekomprese najednou v GiveData a stream bez konce vyvolava chybu)

    // data bez potreby kriticke sekce (zapisuje se jen v sockets-threadu a disk-threadu, synchronizace pres FTPDiskThread a DiskWorkIsUsed):
    CFTPDiskWork DiskWork; // data prace, ktera se zadava objektu FTPDiskThread (thread provadejici operace na disku)

public:
    // 'flushData' je TRUE pokud se data maji predavat pro overeni/zapis na disk, FALSE=data prijdou komplet do pameti 'ReadBytes'
    CDataConnectionSocket(BOOL flushData, CFTPProxyForDataCon* proxyServer, int encryptConnection,
                          CCertificate* certificate, int compressData, CSocket* conForReuse);
    virtual ~CDataConnectionSocket();

    BOOL IsGood() { return TransferFinished != NULL; } // kriticka sekce neni potreba

    HANDLE GetTransferFinishedEvent() { return TransferFinished; } // kriticka sekce neni potreba

    // jen vola CloseSocket() + nastavuje SocketCloseTime a TransferFinished event
    virtual BOOL CloseSocketEx(DWORD* error);

    // cisteni promennych pred connectem data-socketu
    virtual void ClearBeforeConnect();

    // vraci NetEventLastError, ReadBytesLowMemory, TgtFileLastError, NoDataTransTimeout,
    // SSLErrorOccured, DecomprErrorOccured
    // (maji kritickou sekci)
    void GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr, BOOL* noDataTransTimeout,
                  int* sslErrorOccured, BOOL* decomprErrorOccured);

    // vraci DecomprMissingStreamEnd (ma kritickou sekci)
    //BOOL GetDecomprMissingStreamEnd();

    // predani dat z "data connection"; vraci alokovany buffer s daty a v 'length' (nesmi byt
    // NULL) jejich delku; pri chybe dekomprese (MODE Z) vraci v 'decomprErr' TRUE a pokud
    // jde o chybu v datech, vraci prazdny buffer; vraci NULL jen pri nedostatku pameti
    char* GiveData(int* length, BOOL* decomprErr);

    // vraci status "data connection" (parametry nesmi byt NULL): 'downloaded' (kolik se jiz
    // precetlo/downloadlo), 'total' (celkova velikost cteni/downloadu - neni-li znama, vraci -1),
    // 'connectionIdleTime' (cas v sekundach od posledniho prijmu dat), 'speed' (rychlost spojeni
    // v bytech za sekundu)
    // volani mozne z libovolneho threadu
    void GetStatus(CQuadWord* downloaded, CQuadWord* total, DWORD* connectionIdleTime, DWORD* speed);

    // nastavi handle okna, kteremu se maji posilat (post-msg) notifikacni zpravy
    // pri zmenach stavu objektu (viz metoda GetStatus); 'hwnd' je handle takoveho okna;
    // 'msg' je kod zpravy, na kterou okno reaguje; k poslani dalsi zpravy dojde vzdy
    // az po volani metody StatusMessageReceived (branime se tak posilani nadbytecnych zprav);
    // je-li 'hwnd' NULL, znamena to, ze uz se nemaji posilat zadne zpravy
    void SetWindowWithStatus(HWND hwnd, UINT msg);

    // okno hlasi, ze prijalo zpravu o zmene stavu, a ze se tedy pri dalsi zmene stavu
    // ma opet poslat zprava
    void StatusMessageReceived();

    // povoluje/zakazuje posilani zprav o data-connectine workerovi; vyznam parametru viz
    // PostMessagesToWorker, WorkerSocketMsg, WorkerSocketUID, WorkerMsgConnectedToServer,
    // WorkerMsgConnectionClosed, WorkerMsgFlushData a WorkerMsgListeningForCon
    void SetPostMessagesToWorker(BOOL post, int msg, int uid, DWORD msgIDConnected,
                                 DWORD msgIDConClosed, DWORD msgIDFlushData,
                                 DWORD msgIDListeningForCon);

    // nastavuje TgtDiskFileName a CurrentTransferMode, zaroven povoli flushovani dat
    // primo do souboru TgtDiskFileName (vyuziva FTPDiskThread)
    void SetDirectFlushParams(const char* tgtFileName, CCurrentTransferMode currentTransferMode);

    // vraci stav ciloveho souboru pri flushovani dat primo do souboru TgtDiskFileName;
    // ve 'fileCreated' vraci TRUE pokud byl soubor vytvoren; v 'fileSize' vraci velikost
    // souboru
    void GetTgtFileState(BOOL* fileCreated, CQuadWord* fileSize);

    // vola se po dokonceni datoveho prenosu (uspesnem ci nikoli); ma smysl jen pri flushovani
    // dat primo do souboru TgtDiskFileName
    void CloseTgtFile();

    // vola worker po dokonceni flushnuti dat; 'flushBuffer' je prave flushnuty buffer
    // (vraci se tak do tohoto objektu - vytazeni z objektu viz metoda GiveFlushData();
    // POZOR: muze jit i o buffer z 'DecomprDataBuffer'); pokud jsou dalsi data k flushnuti,
    // dojde k prohozeni bufferu a postnuti WorkerMsgFlushData workerovi; je-li treba posti
    // tez FD_READ nebo FD_CLOSE do tohoto socketu (viz NeedFlushReadBuf);
    // 'enterSocketCritSect' je TRUE az na nize popsanou vyjimku
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread), vyjimka: pokud jsme v CSocketsThread::CritSect a
    //        SocketCritSect, volani je mozne pokud nastavime 'enterSocketCritSect' na FALSE
    void FlushDataFinished(char* flushBuffer, BOOL enterSocketCritSect);

    // jsou-li nova data ve FlushBuffer, vraci TRUE a data ve 'flushBuffer'
    // (nesmi byt NULL) a 'validBytesInFlushBuffer' (nesmi byt NULL), po predani dat
    // vyNULLuje FlushBuffer (bez MODE Z) nebo DecomprDataBuffer (s MODE Z) (vraceni
    // pres FlushDataFinished() nebo dealokace bufferu je na volajicim); v 'deleteTgtFile'
    // (nesmi byt NULL) vraci TRUE pokud jsme prisli na to, ze data v souboru muzou byt
    // poskozena a soubor je tedy nutne smazat; vraci FALSE pokud neni potreba zadny
    // flush dat
    BOOL GiveFlushData(char** flushBuffer, int* validBytesInFlushBuffer, BOOL* deleteTgtFile);

    // jen v rezimu FlushData==TRUE: uvolni nactena data z bufferu; slouzi jen jako
    // obrana pred error tracem v destruktoru objektu (ten hlasi, ze ne vsechna data
    // se podarilo flushnout)
    void FreeFlushData();

    // vola se po zavreni teto data-connection; vraci TRUE pokud jsou vsechna data
    // flushnuta (aneb data-connectiona uz neobsahuje zadna data); je-li 'onlyTest'
    // FALSE a je-li treba, zajisti prohozeni bufferu a postnuti WorkerMsgFlushData
    // workerovi (flushnuti dat); vraci TRUE az po volani FlushDataFinished pro
    // posledni flushovana data
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    BOOL AreAllDataFlushed(BOOL onlyTest);

    // zajisti okamzite nasilne ukonceni data-connectiony: vola CloseSocketEx + pri
    // pouziti primeho flushe dat do souboru z data-connectiony: prerusi flushovani
    // dat + vycisti flush buffer
    void CancelConnectionAndFlushing();

    // jen pri pouziti primeho flushe dat do souboru z data-connectiony: vraci TRUE
    // pokud probiha flush dat; jinak vraci FALSE
    BOOL IsFlushingDataToDisk();

    // vraci TRUE pokud je treba resit problem "ascii transfer mode for binary file", pak
    // v 'howToSolve' vraci jak problem resit: 0 = zeptat se uzivatele, 1 = downloadnout
    // znovu v binary rezimu, 2 = prerusit download souboru (cancel); vraci FALSE pokud
    // k problemu nedoslo nebo pokud uzivatel zvolil: ignorovat (dokoncit download v ascii
    // rezimu)
    BOOL IsAsciiTrForBinFileProblem(int* howToSolve);

    // nastavuje AsciiTrModeForBinFileHowToSolve na howToSolve; vola se po odpovedi uzivatele
    // na dotaz jak resit problem "ascii transfer mode for binary file"; konstanty viz
    // AsciiTrModeForBinFileHowToSolve (0, 1, 2, 3)
    void SetAsciiTrModeForBinFileHowToSolve(int howToSolve);

    // ceka na zavreni souboru, do ktereho se provadel primy flush dat, nebo na timeout
    // ('timeout' v milisekundach nebo hodnota INFINITE = zadny timeout); vraci TRUE
    // pokud se dockal zavreni, FALSE pri timeoutu
    BOOL WaitForFileClose(DWORD timeout);

    // nastaveni celkove velikosti prenasenych dat v bytech (-1 = neznama); ma kritickou sekci +
    // bude informovat "user-iface" objekt (stejne jako o velikosti dosud stazene casti
    // dat, rychlosti stahovani a idle casu connectiony)
    // volani mozne z libovolneho threadu
    void SetDataTotalSize(CQuadWord const& size);

    // vola se, kdyz uzivatel prepne workera do/ze stavu "paused"; data-connectiona by v "paused"
    // stavu mela prestat prenaset data, po skonceni tohoto stavu by se melo v prenosu pokracovat
    void UpdatePauseStatus(BOOL pause);

protected:
    // vola "data connection" v okamziku zmeny stavu (vyhodnocuje se zde, jestli se
    // ma poslat zprava oknu se stavem - viz SetWindowWithStatus)
    void StatusHasChanged();

    // jen pri pouziti primeho flushe dat do souboru z data-connectiony (viz metoda
    // SetDirectFlushParams), jinak nic nedela: zajisti predani flushovanych dat do
    // disk-work threadu
    // POZOR: musi se volat z kriticke sekce SocketCritSect
    void DirectFlushData();

    // vola se po navazani spojeni se serverem
    // POZOR: volat jen z kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    void JustConnected();

    // prehozeni naplneneho bufferu ReadBytes do FlushBuffer
    // POZOR: musi se volat z kriticke sekce SocketCritSect
    void MoveReadBytesToFlushBuffer();

    // ******************************************************************************************
    // metody volane v "sockets" threadu (na zaklade prijmu zprav od systemu nebo jinych threadu)
    //
    // POZOR: volane v sekci SocketsThread->CritSect, mely by se provadet co nejrychleji (zadne
    //        cekani na vstup usera, atd.)
    // ******************************************************************************************

    // prijem udalosti pro tento socket (FD_READ, FD_WRITE, FD_CLOSE, atd.); 'index' je
    // index socketu v poli SocketsThread->Sockets (pouziva se pro opakovane posilani
    // zprav pro socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // prijem vysledku ReceiveNetEvent(FD_CLOSE) - neni-li 'error' NO_ERROR, jde
    // o kod Windowsove chyby (prisla s FD_CLOSE nebo vznikla behem zpracovani FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // prijem timeru s ID 'id' a parametrem 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // prijem postnute zpravy s ID 'id' a parametrem 'param'
    virtual void ReceivePostMessage(DWORD id, void* param);

    // vola se po prijeti a zpracovani FD_ACCEPT (za predpokladu, ze se pro FD_ACCEPT vola metoda
    // CSocket::ReceiveNetEvent): 'success' je uspech acceptu, pri neuspechu je ve 'winError'
    // windowsovy kod chyby a v 'proxyError' TRUE pokud jde o chybu hlasenou proxy serverem
    // (text chyby lze ziskat pres GetProxyError())
    // POZOR: volat jen po jednom vstupu do kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError);
};

class CUploadDataConnectionSocket : public CDataConnectionBaseSocket
{
protected:
    // kriticka sekce pro pristup k datum objektu je CSocket::SocketCritSect
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)

    char* BytesToWrite;     // buffer pro data, ktera se budou zapisovat na socket (zapisou se po prijeti FD_WRITE)
    int BytesToWriteCount;  // pocet platnych bytu v bufferu 'BytesToWrite'
    int BytesToWriteOffset; // pocet jiz odeslanych bytu v bufferu 'BytesToWrite'

    CQuadWord TotalWrittenBytesCount; // celkovy pocet bytu zapsanych do data-connectiony (POZOR: pri MODE Z je zde velikost dekomprimovanych dat)

    char* FlushBuffer;           // flush-buffer pro data pripravena pro zapis na socket (NULL pokud je predany ve workerovi aneb disk-threadu - nacitaji se do nej data z disku)
    int ValidBytesInFlushBuffer; // pocet platnych bytu v bufferu 'FlushBuffer'; je-li > 0, cekame az se dokonci zapis dat na socket a pak doplnime buffer BytesToWrite a zkusime nechat nacist dalsi data ze souboru
    BOOL EndOfFileReached;       // TRUE = jiz nelze pripravit dalsi data pro zapis na socket (docetli jsme az ke konci souboru na disku)
    BOOL WaitingForWriteEvent;   // FALSE = az prijdou data pro zapis, je potreba postnout FD_WRITE; TRUE = prave se ceka, az bude mozne zapsat dalsi data, pak prijde FD_WRITE (neni potreba ho postit)
    BOOL ConnectionClosedOnEOF;  // TRUE = data-connectionu jsme zavreli, protoze jsme zapsali vsechna data az po end-of-file

    char* ComprDataBuffer;             // data, ktera se po kompresi zapisou do flush-bufferu (pro zapis na socket) (NULL pokud neni alokovany (ComprDataAllocatedSize==0) nebo pokud je predany ve workerovi aneb disk-threadu - nacitaji se do nej data z disku)
    int ComprDataAllocatedSize;        // alokovana velikost bufferu 'ComprDataBuffer'
    int ComprDataDelayedOffset;        // pocet jiz zkomprimovanych bytu v 'ComprDataBuffer'
    int ComprDataDelayedCount;         // pocet platnych bytu v 'ComprDataBuffer': komprimace bytu od 'ComprDataDelayedOffset' do 'ComprDataDelayedCount' je odlozena na dalsi cyklus GiveBufferForData & DataBufferPrepared
    int AlreadyComprPartOfFlushBuffer; // kam se ma ulozit dalsi davka komprimovanych dat z 'ComprDataBuffer' do 'FlushBuffer'
    int DecomprBytesInBytesToWrite;    // do kolika bytu se dekomprimuji data z BytesToWrite (od BytesToWriteOffset do BytesToWriteCount)
    int DecomprBytesInFlushBuffer;     // do kolika bytu se dekomprimuji data z FlushBuffer

    DWORD WorkerMsgPrepareData; // cislo zpravy, kterou si majitel data-connection preje dostat jakmile ma pripravit (nacist z disku) dalsi data do flush-bufferu pro poslani na server (do parametru 'param' volani PostSocketMessage jde UID tohoto objektu)
    BOOL PrepareDataMsgWasSent; // TRUE = uz jsme poslali WorkerMsgPrepareData, ted cekame na reakci majitele (workera); FALSE = pokud jsou potreba dalsi data, posleme WorkerMsgPrepareData

    CQuadWord DataTotalSize; // celkova velikost prenasenych dat v bytech (-1 = neinicializovana hodnota)

    BOOL NoDataTransTimeout; // TRUE = nastal no-data-transfer timeout - zavreli jsme data-connectionu

    BOOL FirstWriteAfterConnect;      // TRUE = po tomto pripojeni na server se jeste zadna data na socket nezapisovala
    DWORD FirstWriteAfterConnectTime; // cas prvniho zapisu na socket (zacatek plneni lokalnich bufferu) po tomto pripojeni na server
    DWORD SkippedWriteAfterConnect;   // kolik bytu jsme nepocitali do vypoctu rychlosti uploadu z duvodu, ze sly nejspis do lokalniho bufferu (umele a nesmyslne zvysuje pocatecni rychlost uploadu)
    DWORD LastSpeedTestTime;          // cas posledniho testu rychlosti spojeni (pro odhad kolik bytu najednou zapisovat na socket)
    DWORD LastPacketSizeEstimation;   // posledni odhad optimalni velikosti "paketu" (kolik bytu najednou zapisovat na socket)
    DWORD PacketSizeChangeTime;       // cas posledni zmeny LastPacketSizeEstimation
    DWORD BytesSentAfterPckSizeCh;    // kolik bytu jsme poslali behem jedne vteriny od posledni zmeny LastPacketSizeEstimation
    DWORD PacketSizeChangeSpeed;      // prenosova rychlost pred posledni zmenou LastPacketSizeEstimation
    DWORD TooBigPacketSize;           // velikost paketu, pri ktere uz dochazi k degradaci prenosove rychlosti (napr. z 5MB/s na 160KB/s); -1 pokud takova velikost nebyla zjistena

    BOOL Activated; // Data not sent over socket until ActivateConnection is called

#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
    // sekce pomocnych dat pro DebugLogPacketSizeAndWriteSize
    DWORD DebugLastWriteToLog;        // cas posledniho zapisu do logu (piseme jednou za vterinu, abysme totalne nezahltili log)
    DWORD DebugSentButNotLoggedBytes; // kolik bytu jsme skipli kvuli DebugLastWriteToLog
    DWORD DebugSentButNotLoggedCount; // pocet zapsanych bloku, ktere jsme skipli kvuli DebugLastWriteToLog
#endif                                // DEBUGLOGPACKETSIZEANDWRITESIZE

public:
    CUploadDataConnectionSocket(CFTPProxyForDataCon* proxyServer, int encryptConnection,
                                CCertificate* certificate, int compressData, CSocket* conForReuse);
    virtual ~CUploadDataConnectionSocket();

    BOOL IsGood() { return BytesToWrite != NULL && FlushBuffer != NULL; }

    // vraci NetEventLastError, NoDataTransTimeout a SSLErrorOccured (maji kritickou sekci)
    void GetError(DWORD* netErr, BOOL* noDataTransTimeout, int* sslErrorOccured);

    // jen vola CloseSocket() + nastavuje SocketCloseTime
    virtual BOOL CloseSocketEx(DWORD* error);

    // povoluje/zakazuje posilani zprav o upload data-connectine workerovi; vyznam parametru viz
    // PostMessagesToWorker, WorkerSocketMsg, WorkerSocketUID, WorkerMsgConnectedToServer,
    // WorkerMsgConnectionClosed, WorkerMsgPrepareData a WorkerMsgListeningForCon
    void SetPostMessagesToWorker(BOOL post, int msg, int uid, DWORD msgIDConnected,
                                 DWORD msgIDConClosed, DWORD msgIDPrepareData,
                                 DWORD msgIDListeningForCon);

    // cisteni promennych pred connectem data-socketu
    virtual void ClearBeforeConnect();

    void ActivateConnection();

    // nastaveni celkove velikosti prenasenych dat v bytech; ma kritickou sekci
    // volani mozne z libovolneho threadu
    void SetDataTotalSize(CQuadWord const& size);

    // vraci skutecny pocet bytu preneseny data-connectionou
    void GetTotalWrittenBytesCount(CQuadWord* uploadSize);

    // vraci TRUE pokud byla vsechna data uspesne prenesena
    BOOL AllDataTransferred();

    // je-li FlushBuffer (pri kompresi ComprDataBuffer) prazdny a jeste nebyl predan do
    // workera, vraci TRUE a buffer FlushBuffer (pri kompresi ComprDataBuffer) ve 'flushBuffer'
    // (nesmi byt NULL), po predani bufferu se FlushBuffer (pri kompresi ComprDataBuffer)
    // vyNULLuje (vraceni pres FlushDataFinished() nebo dealokace bufferu je na volajicim);
    // vraci FALSE pokud neni potreba zadny flush dat
    BOOL GiveBufferForData(char** flushBuffer);

    // uvolni pripravena data z obou bufferu (data pro poslani na server); slouzi jen jako
    // obrana pred error tracem v destruktoru objektu (ten hlasi, ze ne vsechna data
    // se podarilo flushnout)
    void FreeBufferedData();

    // vola worker po dokonceni cteni dat z disku; 'flushBuffer' je prave nacteny buffer;
    // pokud je volny druhy buffer pro data a nedosahli jsme jeste konce souboru, dojde
    // k prohozeni bufferu a postnuti WorkerMsgPrepareData workerovi; je-li treba posti
    // tez FD_WRITE do tohoto socketu (viz WaitingForWriteEvent); pri kompresi zajisti
    // postupne naplneni bufferu FlushBuffer komprimovanymi daty (dokud neni plny a nejsme
    // na konci souboru, posti WorkerMsgPrepareData workerovi)
    void DataBufferPrepared(char* flushBuffer, int validBytesInFlushBuffer, BOOL enterCS);

    // worker vola po prijeti odpovedi na STOR/APPE prikaz ze serveru (upload uz je ze strany
    // serveru dokonceny)
    void UploadFinished();

    // vraci status "data connection" (parametry nesmi byt NULL): 'uploaded' (kolik se jiz
    // zapsalo/uploadlo), 'total' (celkova velikost zapisu/uploadu),
    // 'connectionIdleTime' (cas v sekundach od posledniho zapisu dat), 'speed' (rychlost spojeni
    // v bytech za sekundu)
    // volani mozne z libovolneho threadu
    void GetStatus(CQuadWord* uploaded, CQuadWord* total, DWORD* connectionIdleTime, DWORD* speed);

    // vola se, kdyz uzivatel prepne workera do/ze stavu "paused"; data-connectiona by v "paused"
    // stavu mela prestat prenaset data, po skonceni tohoto stavu by se melo v prenosu pokracovat
    void UpdatePauseStatus(BOOL pause);

protected:
    // vola se po navazani spojeni se serverem
    // POZOR: volat jen z kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    void JustConnected();

#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
    // vola se pro vypis hodnot souvisejicich se zmenou velikosti bloku dat posilanych pres 'send'
    // POZOR: volat jen z kriticke sekce CSocket::SocketCritSect
    void DebugLogPacketSizeAndWriteSize(int size, BOOL noChangeOfLastPacketSizeEstimation = FALSE);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE

    // prehozeni naplneneho bufferu FlushBuffer do BytesToWrite
    // POZOR: musi se volat z kriticke sekce SocketCritSect
    void MoveFlushBufferToBytesToWrite();

    // ******************************************************************************************
    // metody volane v "sockets" threadu (na zaklade prijmu zprav od systemu nebo jinych threadu)
    //
    // POZOR: volane v sekci SocketsThread->CritSect, mely by se provadet co nejrychleji (zadne
    //        cekani na vstup usera, atd.)
    // ******************************************************************************************

    // prijem udalosti pro tento socket (FD_READ, FD_WRITE, FD_CLOSE, atd.); 'index' je
    // index socketu v poli SocketsThread->Sockets (pouziva se pro opakovane posilani
    // zprav pro socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // prijem vysledku ReceiveNetEvent(FD_CLOSE) - neni-li 'error' NO_ERROR, jde
    // o kod Windowsove chyby (prisla s FD_CLOSE nebo vznikla behem zpracovani FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // prijem timeru s ID 'id' a parametrem 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // vola se po prijeti a zpracovani FD_ACCEPT (za predpokladu, ze se pro FD_ACCEPT vola metoda
    // CSocket::ReceiveNetEvent): 'success' je uspech acceptu, pri neuspechu je ve 'winError'
    // windowsovy kod chyby a v 'proxyError' TRUE pokud jde o chybu hlasenou proxy serverem
    // (text chyby lze ziskat pres GetProxyError())
    // POZOR: volat jen po jednom vstupu do kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError);
};

//
// ****************************************************************************
// CKeepAliveDataConSocket
//
// objekt socketu, ktery slouzi jako "data connection" na FTP server; vsechna data
// se jednoduse ignoruji, datovy prenost je jen "keep-alive" divadlo pro server
// pro dealokaci pouzivat funkci ::DeleteSocket!

class CKeepAliveDataConSocket : public CSocket
{
protected:
    // kriticka sekce pro pristup k datum objektu je CSocket::SocketCritSect
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)

    BOOL UsePassiveMode; // TRUE = pasivni mod (PASV), jinak aktivni mod (PORT)
    BOOL EncryptConnection;
    CFTPProxyForDataCon* ProxyServer; // NULL = "not used (direct connection)" (read-only, inicializuje se v konstruktoru, pristup tedy mozny bez kriticke sekce)
    DWORD ServerIP;                   // IP serveru, kam se pripojujeme v pasivnim modu
    unsigned short ServerPort;        // port serveru, kam se pripojujeme v pasivnim modu

    int LogUID; // UID logu pro odpovidajici "control connection" (-1 dokud neni log zalozen)

    DWORD NetEventLastError; // kod posledni chyby, ktera byla hlasena do ReceiveNetEvent() nebo nastala v PassiveConnect()
    int SSLErrorOccured;     // viz konstanty SSLCONERR_XXX
    BOOL ReceivedConnected;  // TRUE = doslo k otevreni (connect nebo accept) "data connection" (nic nerika o soucasnem stavu)
    DWORD LastActivityTime;  // GetTickCount() z okamziku, kdy se naposledy delalo s "data connection" (sleduje se inicializace (SetPassive() a SetActive()), uspesny connect a read)
    DWORD SocketCloseTime;   // GetTickCount() z okamziku, kdy se zavrel socket "data connection"

    // pouziva se pri zavirani "data connection" (vyhodnoceni jestli uz se dokoncil keep-alive prikaz)
    // POZOR: nesmi se vstoupit do sekce ParentControlSocket->SocketCritSect (volat metody
    //        ParentControlSocket) ze sekce SocketCritSect tohoto objektu (jiz se vyuziva
    //        opacneho poradi vstupu viz ccsevTimeout v CControlConnectionSocket::WaitForEndOfKeepAlive())
    // POZNAMKA: zaroven se pouziva stejne jako CDataConnectionBaseSocket::SSLConForReuse, viz jeji komentar
    CControlConnectionSocket* ParentControlSocket;
    BOOL CallSetupNextKeepAliveTimer; // TRUE pokud se po uzavreni socketu ma volat SetupNextKeepAliveTimer() objektu ParentControlSocket (neprimo)

    DWORD ListenOnIP;            // IP, na kterem nasloucha proxy server (ceka na spojeni); je-li INADDR_NONE, nebyla zatim volana metoda ListeningForConnection() nebo proxy server vratil nejakou chybu
    unsigned short ListenOnPort; // port, na kterem nasloucha proxy server (ceka na spojeni)

public:
    CKeepAliveDataConSocket(CControlConnectionSocket* parentControlSocket, CFTPProxyForDataCon* proxyServer, int encryptConnection, CCertificate* certificate);
    virtual ~CKeepAliveDataConSocket();

    // vola se v okamziku, kdy "control connection" dostane odpoved serveru ('replyCode')
    // ohlasujici konec datoveho prenosu; vraci TRUE pokud prenos skutecne skoncil (je mozne
    // uvolnit "data connection"); vraci FALSE pokud prenos jeste neskoncil ("data connection"
    // se neda uvolnit), az prenos skutecne skonci, zavola neprimo metodu SetupNextKeepAliveTimer()
    // objektu ParentControlSocket
    BOOL FinishDataTransfer(int replyCode);

    // vraci TRUE, pokud je "data connection" otevrena pro prenos dat (uz doslo ke spojeni
    // se serverem); v 'transferFinished' (neni-li NULL) vraci TRUE pokud jiz prenos
    // dat probehl (FALSE = jeste nedoslo ke spojeni se serverem)
    BOOL IsTransfering(BOOL* transferFinished);

    // jen vola CloseSocket() + nastavuje SocketCloseTime a TransferFinished event
    BOOL CloseSocketEx(DWORD* error);

    // vraci SocketCloseTime (ma kritickou sekci)
    DWORD GetSocketCloseTime();

    // nastavi parametry pro pasivni mod "data connection"; tyto parametry se pouzivaji
    // v metode PassiveConnect(); 'ip' + 'port' jsou parametry pripojeni ziskane od serveru;
    // 'logUID' je UID logu "control connection", ktere patri tato "data connection"
    void SetPassive(DWORD ip, unsigned short port, int logUID);

    // zavola Connect() pro ulozene parametry pro pasivni mod
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    BOOL PassiveConnect(DWORD* error);

    // nastavi aktivni mod "data connection"; otevreni socketu viz CSocket::OpenForListening();
    // 'logUID' je UID logu "control connection", ktere patri tato "data connection"
    // volani mozne z libovolneho threadu
    void SetActive(int logUID);

    // vola se v okamziku, kdy ma zacit datovy prenos (napr. po poslani prikazu pro listovani);
    // v pasivnim modu se zkontroluje jestli nedoslo k odmitnuti prvniho pokusu o navazani
    // spojeni, pripadne se provede dalsi pokus o navazani spojeni; v aktivnim modu se nedeje
    // nic (mohlo by tu byt povoleni navazani spojeni ("accept"), ale pro vetsi obecnost je
    // navazani spojeni mozne stale);
    // POZOR: neni mozne volat tuto metodu z kriticke sekce SocketCritSect (metoda pouziva
    //        SocketsThread)
    // volani mozne z libovolneho threadu
    void ActivateConnection();

    // jen zapouzdrene volani CSocket::OpenForListeningWithProxy() s parametry z 'ProxyServer'
    BOOL OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                   BOOL* listenError, DWORD* err);

    // zapamatuje si "listen" IP+port (kde se ceka na spojeni) a jestli nedoslo k chybe,
    // pak posle CTRLCON_KALISTENFORCON vlastnikovi data-connectiony
    // POZOR: pri pripojeni bez proxy serveru se tato metoda vola primo z OpenForListeningWithProxy(),
    //        tedy nemusi to byt ze "sockets" threadu
    // POZOR: nesmi se volat z kriticke sekce CSocket::SocketCritSect
    virtual void ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                        BOOL proxyError);

    // pokud se uspesne otevrel "listen" port, vraci TRUE + "listen" IP+port
    // v 'listenOnIP'+'listenOnPort'; pri chybe "listen" vraci FALSE
    BOOL GetListenIPAndPort(DWORD* listenOnIP, unsigned short* listenOnPort);

    // pokud jde o pasivni rezim a je zapnute sifrovani data connectiony, zkusi ji zasifrovat
    void EncryptPassiveDataCon();

protected:
    // vola se po navazani spojeni se serverem
    // POZOR: volat jen z kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    void JustConnected();

    // vypise do logu chybu z NetEventLastError
    void LogNetEventLastError(BOOL canBeProxyError);

    // ******************************************************************************************
    // metody volane v "sockets" threadu (na zaklade prijmu zprav od systemu nebo jinych threadu)
    //
    // POZOR: volane v sekci SocketsThread->CritSect, mely by se provadet co nejrychleji (zadne
    //        cekani na vstup usera, atd.)
    // ******************************************************************************************

    // prijem udalosti pro tento socket (FD_READ, FD_WRITE, FD_CLOSE, atd.); 'index' je
    // index socketu v poli SocketsThread->Sockets (pouziva se pro opakovane posilani
    // zprav pro socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // prijem vysledku ReceiveNetEvent(FD_CLOSE) - neni-li 'error' NO_ERROR, jde
    // o kod Windowsove chyby (prisla s FD_CLOSE nebo vznikla behem zpracovani FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // prijem timeru s ID 'id' a parametrem 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // vola se po prijeti a zpracovani FD_ACCEPT (za predpokladu, ze se pro FD_ACCEPT vola metoda
    // CSocket::ReceiveNetEvent): 'success' je uspech acceptu, pri neuspechu je ve 'winError'
    // windowsovy kod chyby a v 'proxyError' TRUE pokud jde o chybu hlasenou proxy serverem
    // (text chyby lze ziskat pres GetProxyError())
    // POZOR: volat jen po jednom vstupu do kriticke sekce CSocket::SocketCritSect a CSocketsThread::CritSect
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError);

private:
    // zastineni metody CSocket::CloseSocket()
    BOOL CloseSocket(DWORD* error) { TRACE_E("Use CloseSocketEx() instead of CloseSocket()!"); }
};
