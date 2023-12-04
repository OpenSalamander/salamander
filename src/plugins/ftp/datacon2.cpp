// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CTransferSpeedMeter
//

CTransferSpeedMeter::CTransferSpeedMeter()
{
    HANDLES(InitializeCriticalSection(&TransferSpeedMeterCS));
    Clear();
}

CTransferSpeedMeter::~CTransferSpeedMeter()
{
    HANDLES(DeleteCriticalSection(&TransferSpeedMeterCS));
}

void CTransferSpeedMeter::Clear()
{
    HANDLES(EnterCriticalSection(&TransferSpeedMeterCS));
    ActIndexInTrBytes = 0;
    ActIndexInTrBytesTimeLim = 0;
    CountOfTrBytesItems = 0;
    LastTransferTime = GetTickCount();
    HANDLES(LeaveCriticalSection(&TransferSpeedMeterCS));
}

DWORD
CTransferSpeedMeter::GetSpeed(DWORD* transferIdleTime)
{
    CALL_STACK_MESSAGE1("CTransferSpeedMeter::GetSpeed()");

    HANDLES(EnterCriticalSection(&TransferSpeedMeterCS));
    DWORD time = GetTickCount();
    if (transferIdleTime != NULL)
        *transferIdleTime = (time - LastTransferTime) / 1000;
    DWORD speed;
    if (CountOfTrBytesItems > 0) // po navazani spojeni je "always true"
    {
        int actIndexAdded = 0;                           // 0 = nebyl zapocitan akt. index, 1 = byl zapocitan akt. index
        int emptyTrBytes = 0;                            // pocet zapocitanych prazdnych kroku
        CQuadWord total(0, 0);                           // celkovy pocet bytu za poslednich max. DATACON_ACTSPEEDNUMOFSTEPS kroku
        int addFromTrBytes = CountOfTrBytesItems - 1;    // pocet uzavrenych kroku k pridani z fronty
        DWORD restTime = 0;                              // cas od posledniho zapocitaneho kroku do tohoto okamziku
        if ((int)(time - ActIndexInTrBytesTimeLim) >= 0) // akt. index uz je uzavreny + mozna budou potreba i prazdne kroky
        {
            emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / DATACON_ACTSPEEDSTEP;
            restTime = (time - ActIndexInTrBytesTimeLim) % DATACON_ACTSPEEDSTEP;
            emptyTrBytes = min(emptyTrBytes, DATACON_ACTSPEEDNUMOFSTEPS);
            if (emptyTrBytes < DATACON_ACTSPEEDNUMOFSTEPS) // nestaci jen prazdne kroky, zapocteme i akt. index
            {
                total = CQuadWord(TransferedBytes[ActIndexInTrBytes], 0);
                actIndexAdded = 1;
            }
            addFromTrBytes = DATACON_ACTSPEEDNUMOFSTEPS - actIndexAdded - emptyTrBytes;
            addFromTrBytes = min(addFromTrBytes, CountOfTrBytesItems - 1); // kolik uzavrenych kroku z fronty jeste zapocist
        }
        else
        {
            restTime = time + DATACON_ACTSPEEDSTEP - ActIndexInTrBytesTimeLim;
            total = CQuadWord(TransferedBytes[ActIndexInTrBytes], 0);
        }

        int actIndex = ActIndexInTrBytes;
        int i;
        for (i = 0; i < addFromTrBytes; i++)
        {
            if (--actIndex < 0)
                actIndex = DATACON_ACTSPEEDNUMOFSTEPS; // pohyb po kruh. fronte
            total += CQuadWord(TransferedBytes[actIndex], 0);
        }
        DWORD t = (addFromTrBytes + actIndexAdded + emptyTrBytes) * DATACON_ACTSPEEDSTEP + restTime;
        if (t > 0)
            speed = (DWORD)((CQuadWord(1000, 0) * total) / CQuadWord(t, 0)).Value;
        else
            speed = 0; // neni z ceho pocitat, zatim hlasime "0 B/s"
    }
    else
        speed = 0; // neni z ceho pocitat, zatim hlasime "0 B/s"
    HANDLES(LeaveCriticalSection(&TransferSpeedMeterCS));
    return speed;
}

void CTransferSpeedMeter::JustConnected()
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CTransferSpeedMeter::JustConnected()");

    HANDLES(EnterCriticalSection(&TransferSpeedMeterCS));
    TransferedBytes[0] = 0;
    ActIndexInTrBytes = 0;
    ActIndexInTrBytesTimeLim = GetTickCount() + DATACON_ACTSPEEDSTEP;
    CountOfTrBytesItems = 1;
    HANDLES(LeaveCriticalSection(&TransferSpeedMeterCS));
}

void CTransferSpeedMeter::BytesReceived(DWORD count, DWORD time)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CTransferSpeedMeter::BytesReceived(,)"); // parametry ignorujeme z rychlostnich duvodu (call-stack uz tak brzdi)

    HANDLES(EnterCriticalSection(&TransferSpeedMeterCS));
    if (count > 0)
        LastTransferTime = time;
    if ((int)(time - ActIndexInTrBytesTimeLim) < 0) // je v akt. casovem intervalu, jen pridame pocet bytu do intervalu
    {
        TransferedBytes[ActIndexInTrBytes] += count;
    }
    else // neni v akt. casovem intervalu, musime zalozit novy interval
    {
        int emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / DATACON_ACTSPEEDSTEP;
        int i = min(emptyTrBytes, DATACON_ACTSPEEDNUMOFSTEPS); // vic uz nema vliv (nuluje se cela fronta)
        if (i > 0 && CountOfTrBytesItems <= DATACON_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems = min(DATACON_ACTSPEEDNUMOFSTEPS + 1, CountOfTrBytesItems + i);
        while (i--)
        {
            if (++ActIndexInTrBytes > DATACON_ACTSPEEDNUMOFSTEPS)
                ActIndexInTrBytes = 0; // pohyb po kruh. fronte
            TransferedBytes[ActIndexInTrBytes] = 0;
        }
        ActIndexInTrBytesTimeLim += (emptyTrBytes + 1) * DATACON_ACTSPEEDSTEP;
        if (++ActIndexInTrBytes > DATACON_ACTSPEEDNUMOFSTEPS)
            ActIndexInTrBytes = 0; // pohyb po kruh. fronte
        if (CountOfTrBytesItems <= DATACON_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems++;
        TransferedBytes[ActIndexInTrBytes] = count;
    }
    HANDLES(LeaveCriticalSection(&TransferSpeedMeterCS));
}

//
// ****************************************************************************
// CSynchronizedDWORD
//

CSynchronizedDWORD::CSynchronizedDWORD()
{
    HANDLES(InitializeCriticalSection(&ValueCS));
    Value = 0;
}

CSynchronizedDWORD::~CSynchronizedDWORD()
{
    HANDLES(DeleteCriticalSection(&ValueCS));
}

void CSynchronizedDWORD::Set(DWORD value)
{
    HANDLES(EnterCriticalSection(&ValueCS));
    Value = value;
    HANDLES(LeaveCriticalSection(&ValueCS));
}

DWORD
CSynchronizedDWORD::Get()
{
    HANDLES(EnterCriticalSection(&ValueCS));
    DWORD ret = Value;
    HANDLES(LeaveCriticalSection(&ValueCS));
    return ret;
}

//
// ****************************************************************************
// CKeepAliveDataConSocket
//

CKeepAliveDataConSocket::CKeepAliveDataConSocket(CControlConnectionSocket* parentControlSocket,
                                                 CFTPProxyForDataCon* proxyServer, int encryptConnection, CCertificate* certificate)
{
    UsePassiveMode = TRUE;
    ProxyServer = proxyServer;
    ServerIP = INADDR_NONE;
    ServerPort = 0;
    LogUID = -1;
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;
    LastActivityTime = GetTickCount();
    SocketCloseTime = GetTickCount(); // pro jistotu, pred volanim GetSocketCloseTime() by se melo prepsat
    ParentControlSocket = parentControlSocket;
    CallSetupNextKeepAliveTimer = FALSE;
    ListenOnIP = INADDR_NONE;
    ListenOnPort = 0;
    EncryptConnection = encryptConnection;
    if (certificate)
    {
        pCertificate = certificate;
        pCertificate->AddRef();
    }
}

CKeepAliveDataConSocket::~CKeepAliveDataConSocket()
{
    if (ProxyServer != NULL)
        delete ProxyServer;
}

void CKeepAliveDataConSocket::SetPassive(DWORD ip, unsigned short port, int logUID)
{
    CALL_STACK_MESSAGE4("CKeepAliveDataConSocket::SetPassive(0x%X, %u, %d)", ip, port, logUID);
    HANDLES(EnterCriticalSection(&SocketCritSect));
    UsePassiveMode = TRUE;
    ServerIP = ip;
    ServerPort = port;
    LogUID = logUID;
    LastActivityTime = GetTickCount(); // hresime na to, ze objekt se inicializuje pred poslanim transfer prikazu (prikaz bude mit kratsi nebo stejny timeout)
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CKeepAliveDataConSocket::PassiveConnect(DWORD* error)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::PassiveConnect()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL auxUsePassiveMode = UsePassiveMode;
    DWORD auxServerIP = ServerIP;
    unsigned short auxServerPort = ServerPort;
    int logUID = LogUID;

    // pred dalsim connectem nulujeme...
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (auxUsePassiveMode)
    {
        DWORD err; // 'ProxyServer' je v data-connectionach pristupny bez kriticke sekce
        BOOL conRes;
        BOOL connectToProxy = FALSE;
        if (ProxyServer != NULL && IsSOCKSOrHTTPProxy(ProxyServer->ProxyType))
        {
            connectToProxy = TRUE;
            in_addr srvAddr;
            srvAddr.s_addr = auxServerIP;
            conRes = ConnectWithProxy(ProxyServer->ProxyHostIP, ProxyServer->ProxyPort,
                                      ProxyServer->ProxyType, &err, inet_ntoa(srvAddr),
                                      auxServerPort, ProxyServer->ProxyUser,
                                      ProxyServer->ProxyPassword, auxServerIP);
        }
        else
            conRes = Connect(auxServerIP, auxServerPort, &err);
        BOOL ret = TRUE;
        if (!conRes) // neni nadeje na uspech
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            NetEventLastError = err; // zaznamename chybu (krome fatalnich chyb (nedostatek pameti, atd.))
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            char buf[500];
            char errBuf[300];
            in_addr srvAddr;
            srvAddr.s_addr = connectToProxy ? ProxyServer->ProxyHostIP : auxServerIP;
            if (err != 0)
            {
                FTPGetErrorText(err, errBuf, 300);
                char* s = errBuf + strlen(errBuf);
                while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                *s = 0; // oriznuti znaku konce radky z textu chyby
                _snprintf_s(buf, _TRUNCATE, LoadStr(connectToProxy ? IDS_LOGMSGUNABLETOCONPRX2 : IDS_LOGMSGUNABLETOOPEN2), inet_ntoa(srvAddr),
                            (connectToProxy ? ProxyServer->ProxyPort : auxServerPort), errBuf);
            }
            else
            {
                _snprintf_s(buf, _TRUNCATE, LoadStr(connectToProxy ? IDS_LOGMSGUNABLETOCONPRX : IDS_LOGMSGUNABLETOOPEN), inet_ntoa(srvAddr),
                            (connectToProxy ? ProxyServer->ProxyPort : auxServerPort));
            }
            Logs.LogMessage(logUID, buf, -1, TRUE);
            ret = FALSE;
        }
        if (error != NULL)
            *error = err;
        return ret;
    }
    else
    {
        TRACE_E("Unexpected situation in CKeepAliveDataConSocket::PassiveConnect() - not in passive mode.");
        if (error != NULL)
            *error = NO_ERROR; // neznama chyba
        return FALSE;
    }
}

void CKeepAliveDataConSocket::SetActive(int logUID)
{
    CALL_STACK_MESSAGE2("CKeepAliveDataConSocket::SetActive(%d)", logUID);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    UsePassiveMode = FALSE;
    LogUID = logUID;

    // pred dalsim pokusem o navazani spojeni nulujeme...
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;
    LastActivityTime = GetTickCount(); // hresime na to, ze objekt se inicializuje pred poslanim transfer prikazu (prikaz bude mit kratsi nebo stejny timeout)

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CKeepAliveDataConSocket::FinishDataTransfer(int replyCode)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::FinishDataTransfer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = TRUE;
    if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS) // LIST nevraci uspech - nemusi vubec zavrit
        CloseSocketEx(NULL);                      // data connection (napr. WarFTPD) - ukoncime ji sami
    else
    {
        BOOL con = IsConnected();
        if (!ReceivedConnected && con) // zatim nebylo navazane spojeni a socket je otevreny
            CloseSocketEx(NULL);       // zavru socket (jen se ceka na spojeni), server zrejme hlasi chybu prikazu (listovani)
        else
            ret = !con; // doslo ke spojeni a uz je zavrene? (hotovo?)
    }
    if (!ret)
        CallSetupNextKeepAliveTimer = TRUE;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

BOOL CKeepAliveDataConSocket::IsTransfering(BOOL* transferFinished)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::IsTransfering()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = IsConnected();
    if (transferFinished != NULL)
        *transferFinished = ReceivedConnected && !ret; // doslo ke spojeni, ale uz je zavrene? (hotovo)
    ret = ReceivedConnected && ret;                    // doslo ke spojeni a je stale otevrene? (transfering)
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

void CKeepAliveDataConSocket::ActivateConnection()
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::ActivateConnection()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL passiveModeRetry = UsePassiveMode && !ReceivedConnected && NetEventLastError != NO_ERROR;
    int logUID = LogUID;

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (passiveModeRetry) // v pasivnim modu doslo k odmitnuti prvniho pokusu o spojeni, provedeme druhy pokus
    {
        // CloseSocketEx(NULL);           // nema smysl zavirat socket stare "data connection" (uz musi byt zavreny)
        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGDATACONRECON), -1, TRUE);
        PassiveConnect(NULL);
    }
}

DWORD
CKeepAliveDataConSocket::GetSocketCloseTime()
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::GetSocketCloseTime()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    DWORD r = SocketCloseTime;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return r;
}

BOOL CKeepAliveDataConSocket::CloseSocketEx(DWORD* error)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::CloseSocketEx()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    SocketCloseTime = GetTickCount();
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return CSocket::CloseSocket(error);
}

void CKeepAliveDataConSocket::EncryptPassiveDataCon()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int err;
    if (UsePassiveMode && EncryptConnection &&
        !EncryptSocket(LogUID, &err, NULL, NULL, NULL, 0, ParentControlSocket))
    {
        SSLErrorOccured = err;
        if (Socket != INVALID_SOCKET) // always true: socket je pripojeny
            CloseSocketEx(NULL);      // zavreme "data connection", dale nema smysl drzet
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CKeepAliveDataConSocket::JustConnected()
{
    ReceivedConnected = TRUE; // pokud prijde FD_READ pred FD_CONNECT (kazdopadne musi byt connected)
    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_TESTNODATATRTIMEOUT,
                            DATACON_TESTNODATATRTIMERID, NULL);
}

void CKeepAliveDataConSocket::LogNetEventLastError(BOOL canBeProxyError)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::LogNetEventLastError()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (NetEventLastError != NO_ERROR)
    {
        char buf[500];
        char errBuf[300];
        if (!canBeProxyError || !GetProxyError(errBuf, 300, NULL, 0, TRUE))
            FTPGetErrorText(NetEventLastError, errBuf, 300);
        char* s = errBuf + strlen(errBuf);
        while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
            s--;
        *s = 0; // oriznuti znaku konce radky z textu chyby
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), errBuf);
        Logs.LogMessage(LogUID, buf, -1, TRUE);
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CKeepAliveDataConSocket::ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::ConnectionAccepted()");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount != 1)
        TRACE_E("Incorrect call to CKeepAliveDataConSocket::ConnectionAccepted(): must be one-time-entered in section SocketCritSect!");
#endif

    if (success && EncryptConnection)
    {
        int err;
        if (!EncryptSocket(LogUID, &err, NULL, NULL, NULL, 0, ParentControlSocket))
        {
            SSLErrorOccured = err;
            if (Socket != INVALID_SOCKET) // always true: socket je pripojeny
                CloseSocketEx(NULL);      // zavreme "data connection", dale nema smysl drzet
            success = FALSE;
        }
    }
    if (success)
    {
        NetEventLastError = NO_ERROR; // v predchozim acceptu mohlo dojit k chybe, ted uz neni aktualni
        SSLErrorOccured = SSLCONERR_NOERROR;
        JustConnected();
        LastActivityTime = GetTickCount(); // doslo k uspesnemu acceptu
    }
    else // chyba - posleme ji aspon do logu
    {
        NetEventLastError = winError;
        if (proxyError && NetEventLastError == NO_ERROR)
            NetEventLastError = ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */;
        LogNetEventLastError(proxyError);
    }
}

void CKeepAliveDataConSocket::ReceiveNetEvent(LPARAM lParam, int index)
{
    CALL_STACK_MESSAGE3("CKeepAliveDataConSocket::ReceiveNetEvent(0x%IX, %d)", lParam, index);
    DWORD eventError = WSAGETSELECTERROR(lParam); // extract error code of event
    BOOL logLastErr = FALSE;
    switch (WSAGETSELECTEVENT(lParam)) // extract event
    {
    case FD_CLOSE: // nekdy chodi pred poslednim FD_READ, nezbyva tedy nez napred zkusit FD_READ a pokud uspeje, poslat si FD_CLOSE znovu (muze pred nim znovu uspet FD_READ)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE; // TRUE = prisel FD_CLOSE + bylo co cist (provedl se jako FD_READ) => posleme si znovu FD_CLOSE (soucasny FD_CLOSE byl plany poplach)
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (ReceivedConnected || UsePassiveMode) // ignorujeme zavreni "listen" socketu
        {
            if (eventError == NO_ERROR) // jen pokud nenastala chyba (podle helpu hrozi jen WSAENETDOWN)
            {
                if (UsePassiveMode) // u aktivniho spojeni musime cekat na FD_ACCEPT (tenhle socket je "listen", a pak az "data connection")
                {
                    if (!ReceivedConnected)
                        JustConnected();
                }

                if (Socket != INVALID_SOCKET) // socket je pripojeny
                {
                    // precteme co nejvice bytu do bufferu, necteme cyklicky, aby se data prijimala postupne;
                    // je-li jeste neco ke cteni, dostaneme znovu FD_READ
                    char buf[KEEPALIVEDATACON_READBUFSIZE];
                    int len;
                    if (!SSLConn)
                        len = recv(Socket, buf, KEEPALIVEDATACON_READBUFSIZE, 0);
                    else
                    {
                        if (SSLLib.SSL_pending(SSLConn) > 0) // je-li neprazdny interni SSL buffer nedojde vubec k volani recv() a tudiz neprijde dalsi FD_READ, tedy musime si ho poslat sami, jinak se prenos dat zastavi
                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                        len = SSLLib.SSL_read(SSLConn, buf, KEEPALIVEDATACON_READBUFSIZE);
                    }
                    if (len >= 0 /*!= SOCKET_ERROR*/) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                    {
                        if (len > 0)
                        {
                            LastActivityTime = GetTickCount(); // doslo k uspesnemu nacteni bytu ze socketu
                            if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                sendFDCloseAgain = TRUE;
                        }
                        else if (SSLConn)
                        {
                            if ((WSAGETSELECTEVENT(lParam) == FD_READ) && (6 /*SSL_ERROR_ZERO_RETURN*/ == SSLLib.SSL_get_error(SSLConn, 0)))
                            {
                                // seen at ftps://ftp.smartftp.com
                                // SSL_ERROR_ZERO_RETURN: The TLS/SSL connection has been closed.
                                // If the protocol version is SSL 3.0 or TLS 1.0, this result code
                                // is returned only if a closure alert has occurred in the protocol,
                                // i.e. if the connection has been closed cleanly.
                                // Note that in this case SSL_ERROR_ZERO_RETURN does not necessarily indicate that the underlying transport has been closed.
                                sendFDCloseAgain = TRUE;
                                lParam = FD_CLOSE;
                            }
                        }
                    }
                    else
                    {
                        DWORD err = !SSLConn ? WSAGetLastError() : SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, len));
                        ;
                        if (err != WSAEWOULDBLOCK)
                        {
                            NetEventLastError = err; // nastala chyba
                            logLastErr = TRUE;
                            CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                        }
                    }
                }
                else
                {
                    // muze nastat: hl. thread stihne zavolat CloseSocket() pred dorucenim FD_READ
                    // TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveNetEvent(FD_READ): Socket is not connected.");
                    // udalost s touto necekanou chybou nebudeme zavadet (reseni: user pouzije ESC)
                }
            }
            else
            {
                if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // chybu si FD_CLOSE zpracuje po svem
                {
                    NetEventLastError = eventError;
                    logLastErr = TRUE;
                    CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                }
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        // ted zpracujeme FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            if (sendFDCloseAgain) // FD_CLOSE byl misto FD_READ => posleme FD_CLOSE znovu
            {
                PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                            (WPARAM)GetSocket(), lParam);
            }
            else // korektni FD_CLOSE
            {
                logLastErr = (eventError != NO_ERROR);
                CSocket::ReceiveNetEvent(lParam, index); // zavolame metodu predka
            }
        }
        break;
    }

    case FD_CONNECT:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (eventError == NO_ERROR)
        {
            JustConnected();
            LastActivityTime = GetTickCount(); // doslo k uspesnemu connectu
        }
        else
        {
            NetEventLastError = eventError;
            logLastErr = TRUE;
            CloseSocketEx(NULL); // zavreme socket "data connection", uz se nemuze otevrit
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case FD_ACCEPT:
        CSocket::ReceiveNetEvent(lParam, index);
        break;
    }

    if (logLastErr)
        LogNetEventLastError(WSAGETSELECTEVENT(lParam) == FD_CONNECT);
}

void CKeepAliveDataConSocket::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::ReceiveTimer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (id == DATACON_TESTNODATATRTIMERID && Socket != INVALID_SOCKET)
    { // periodicke testovani no-data-transfer timeoutu
        if ((GetTickCount() - LastActivityTime) / 1000 >= (DWORD)Config.GetNoDataTransferTimeout())
        { // timeout nastal, zavreme data-connectionu - nasimulujeme, ze to udelal server
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGNODATATRTIMEOUT), -1, TRUE);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            CSocket::ReceiveNetEvent(MAKELPARAM(FD_CLOSE, WSAECONNRESET), GetMsgIndex()); // zavolame metodu predka
            HANDLES(EnterCriticalSection(&SocketCritSect));
        }
        else // zatim timeout nenastal, pridame timer pro dalsi test
        {
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_TESTNODATATRTIMEOUT,
                                    DATACON_TESTNODATATRTIMERID, NULL);
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CKeepAliveDataConSocket::SocketWasClosed(DWORD error)
{
    CALL_STACK_MESSAGE2("CKeepAliveDataConSocket::SocketWasClosed(%u)", error);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    SocketCloseTime = GetTickCount();
    if (error != NO_ERROR)
        NetEventLastError = error;
    BOOL callSetupNextKeepAliveTimerAux = CallSetupNextKeepAliveTimer;

    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    SocketsThread->DeleteTimer(UID, DATACON_TESTNODATATRTIMERID);

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (callSetupNextKeepAliveTimerAux)
        ParentControlSocket->PostMsgToCtrlCon(CTRLCON_KAPOSTSETUPNEXT, NULL);
}

BOOL CKeepAliveDataConSocket::OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                                        BOOL* listenError, DWORD* err)
{
    if (ProxyServer == NULL)
    {
        return CSocket::OpenForListeningWithProxy(listenOnIP, listenOnPort, NULL, INADDR_NONE,
                                                  0, fpstNotUsed, INADDR_NONE, 0, NULL, NULL,
                                                  listenError, err);
    }
    else
    {
        return CSocket::OpenForListeningWithProxy(listenOnIP, listenOnPort,
                                                  ProxyServer->Host,
                                                  ProxyServer->HostIP,
                                                  ProxyServer->HostPort,
                                                  ProxyServer->ProxyType,
                                                  ProxyServer->ProxyHostIP,
                                                  ProxyServer->ProxyPort,
                                                  ProxyServer->ProxyUser,
                                                  ProxyServer->ProxyPassword,
                                                  listenError, err);
    }
}

void CKeepAliveDataConSocket::ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                                     BOOL proxyError)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::ListeningForConnection()");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount != 0)
        TRACE_E("Incorrect call to CKeepAliveDataConSocket::ListeningForConnection(): may not be entered in section SocketCritSect!");
#endif

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (proxyError)
    {
        ListenOnIP = INADDR_NONE;
        ListenOnPort = 0;
    }
    else
    {
        ListenOnIP = listenOnIP;
        ListenOnPort = listenOnPort;
    }
    int dataConUID = UID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    ParentControlSocket->PostMsgToCtrlCon(CTRLCON_KALISTENFORCON, (void*)(INT_PTR)dataConUID);
}

BOOL CKeepAliveDataConSocket::GetListenIPAndPort(DWORD* listenOnIP, unsigned short* listenOnPort)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::GetListenIPAndPort()");

    BOOL ret = TRUE;
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (ListenOnIP != INADDR_NONE)
    {
        *listenOnIP = ListenOnIP;
        *listenOnPort = ListenOnPort;
    }
    else
    {
        *listenOnIP = INADDR_NONE;
        *listenOnPort = 0;
        ret = FALSE;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

//
// ****************************************************************************
// CUploadDataConnectionSocket
//

CUploadDataConnectionSocket::CUploadDataConnectionSocket(CFTPProxyForDataCon* proxyServer,
                                                         int encryptConnection,
                                                         CCertificate* certificate,
                                                         int compressData,
                                                         CSocket* conForReuse)
    : CDataConnectionBaseSocket(proxyServer, encryptConnection, certificate, compressData, conForReuse)
{
    BytesToWrite = (char*)malloc(DATACON_UPLOADFLUSHBUFFERSIZE);
    BytesToWriteCount = 0;
    BytesToWriteOffset = 0;

    TotalWrittenBytesCount.Set(0, 0);

    FlushBuffer = (char*)malloc(DATACON_UPLOADFLUSHBUFFERSIZE);
    ValidBytesInFlushBuffer = 0;
    EndOfFileReached = FALSE;
    WaitingForWriteEvent = TRUE; // prvni FD_WRITE prijde sam, proto se inicializuje na TRUE
    ConnectionClosedOnEOF = FALSE;

    ComprDataBuffer = NULL;
    ComprDataAllocatedSize = 0;
    ComprDataDelayedOffset = 0;
    ComprDataDelayedCount = 0;
    AlreadyComprPartOfFlushBuffer = 0;
    DecomprBytesInBytesToWrite = 0;
    DecomprBytesInFlushBuffer = 0;

    WorkerMsgPrepareData = -1;
    PrepareDataMsgWasSent = FALSE;

    DataTotalSize.Set(-1, -1);

    NoDataTransTimeout = FALSE;

    FirstWriteAfterConnect = FALSE;
    FirstWriteAfterConnectTime = GetTickCount() - 10000;
    SkippedWriteAfterConnect = 0;
    LastSpeedTestTime = 0;
    LastPacketSizeEstimation = 4096;
    PacketSizeChangeTime = 0;
    BytesSentAfterPckSizeCh = 0;
    PacketSizeChangeSpeed = 0;
    TooBigPacketSize = -1;
    Activated = FALSE;
}

CUploadDataConnectionSocket::~CUploadDataConnectionSocket()
{
    if (ValidBytesInFlushBuffer > 0 || BytesToWriteCount > BytesToWriteOffset ||
        ComprDataDelayedOffset < ComprDataDelayedCount || AlreadyComprPartOfFlushBuffer > 0)
    {
        TRACE_E("CUploadDataConnectionSocket::~CUploadDataConnectionSocket(): closing data-connection without fully flushed data!");
    }
    if (FlushBuffer != NULL)
        free(FlushBuffer);
    if (BytesToWrite != NULL)
        free(BytesToWrite);
    if (ComprDataBuffer != NULL)
        free(ComprDataBuffer);
    if (CompressData)
        SalZLIB->DeflateEnd(&ZLIBInfo);
}

BOOL CUploadDataConnectionSocket::CloseSocketEx(DWORD* error)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::CloseSocketEx()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    SocketCloseTime = GetTickCount();
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return CSocket::CloseSocket(error);
}

void CUploadDataConnectionSocket::SetPostMessagesToWorker(BOOL post, int msg, int uid, DWORD msgIDConnected,
                                                          DWORD msgIDConClosed, DWORD msgIDPrepareData,
                                                          DWORD msgIDListeningForCon)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::SetPostMessagesToWorker()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    PostMessagesToWorker = post;
    WorkerSocketMsg = msg;
    WorkerSocketUID = uid;
    WorkerMsgConnectedToServer = msgIDConnected;
    WorkerMsgConnectionClosed = msgIDConClosed;
    WorkerMsgPrepareData = msgIDPrepareData;
    WorkerMsgListeningForCon = msgIDListeningForCon;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::FreeBufferedData()
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::FreeBufferedData()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BytesToWriteOffset = 0;
    BytesToWriteCount = 0;
    ValidBytesInFlushBuffer = 0;
    EndOfFileReached = FALSE; // pokud jsme docetli az na konec souboru, ted po zahozeni dat z konce souboru uz to neni pravda
    ComprDataDelayedOffset = 0;
    ComprDataDelayedCount = 0;
    DecomprBytesInBytesToWrite = 0;
    DecomprBytesInFlushBuffer = 0;
    AlreadyComprPartOfFlushBuffer = 0;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::ClearBeforeConnect()
{
    BytesToWriteCount = 0;
    BytesToWriteOffset = 0;
    TotalWrittenBytesCount.Set(0, 0);
    ValidBytesInFlushBuffer = 0;
    EndOfFileReached = FALSE;
    if (ComprDataBuffer == NULL)
        ComprDataAllocatedSize = 0;
    ComprDataDelayedOffset = 0;
    ComprDataDelayedCount = 0;
    DecomprBytesInBytesToWrite = 0;
    DecomprBytesInFlushBuffer = 0;
    AlreadyComprPartOfFlushBuffer = 0;
    WaitingForWriteEvent = TRUE;
    ConnectionClosedOnEOF = FALSE;
    PrepareDataMsgWasSent = FALSE;
    DataTotalSize.Set(-1, -1);
    if (CompressData)
    {
        // First get rid of leftovers, if any. NOTE: ctor memsets ZLIBInfo to 0
        SalZLIB->DeflateEnd(&ZLIBInfo);
        // Then initialize ZLIBInfo
        int ignErr = SalZLIB->DeflateInit(&ZLIBInfo, 6);
        if (ignErr < 0)
            TRACE_E("SalZLIB->DeflateInit returns unexpected error: " << ignErr);
    }
}

void CUploadDataConnectionSocket::ActivateConnection()
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::ActivateConnection()");

    CDataConnectionBaseSocket::ActivateConnection();
    Activated = TRUE;

    // Dirty hack - reforce write - it may have been postponed till the connection was activated
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (UsePassiveMode && EncryptConnection && DataTransferPostponed == 1)
    {
        PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_WRITE);
        DataTransferPostponed = 0;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::SetDataTotalSize(CQuadWord const& size)
{
    CALL_STACK_MESSAGE2("CUploadDataConnectionSocket::SetDataTotalSize(%f)", size.GetDouble());

    HANDLES(EnterCriticalSection(&SocketCritSect));
    DataTotalSize = size;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::GetTotalWrittenBytesCount(CQuadWord* uploadSize)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::GetTotalWrittenBytesCount()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    *uploadSize = TotalWrittenBytesCount;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CUploadDataConnectionSocket::AllDataTransferred()
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::AllDataTransferred()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = ConnectionClosedOnEOF;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

void CUploadDataConnectionSocket::SocketWasClosed(DWORD error)
{
    CALL_STACK_MESSAGE2("CUploadDataConnectionSocket::SocketWasClosed(%u)", error);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    SocketCloseTime = GetTickCount();
    if (error != NO_ERROR)
        NetEventLastError = error;

    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    DoPostMessageToWorker(WorkerMsgConnectionClosed);

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::UploadFinished()
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::UploadFinished()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

    if (ConnectionClosedOnEOF && SkippedWriteAfterConnect > 0)
    { // zapocitani bytu z lokalnich bufferu do rychlosti uploadu (nevime jiste, ze se lokalni buffery prenesou, ale kdyz je nezapocitame, bude rychlost na malych souborech (ktere se vejdou skoro cele do lokalnich bufferu) zcela spatne - nesmyslne mala)
        DWORD ti = GetTickCount();
        TransferSpeedMeter.BytesReceived(SkippedWriteAfterConnect, ti);
        if (GlobalTransferSpeedMeter != NULL)
            GlobalTransferSpeedMeter->BytesReceived(SkippedWriteAfterConnect, ti);
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::GetStatus(CQuadWord* uploaded, CQuadWord* total, DWORD* connectionIdleTime, DWORD* speed)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::GetStatus(,,,)");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    *uploaded = TotalWrittenBytesCount;
    *total = DataTotalSize;
    if (*total < *uploaded)
        *total = *uploaded;
    *connectionIdleTime = (GetTickCount() - LastActivityTime) / 1000;
    *speed = TransferSpeedMeter.GetSpeed(NULL);
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::UpdatePauseStatus(BOOL pause)
{
    CALL_STACK_MESSAGE2("CUploadDataConnectionSocket::UpdatePauseStatus(%d)", pause);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (WorkerPaused != pause)
    {
        WorkerPaused = pause;
        if (WorkerPaused && DataTransferPostponed != 0)
            TRACE_E("Unexpected situation in CUploadDataConnectionSocket::UpdatePauseStatus(): DataTransferPostponed=" << DataTransferPostponed);
        if (!WorkerPaused)
        {
            LastActivityTime = GetTickCount();
            if (GlobalLastActivityTime != NULL)
                GlobalLastActivityTime->Set(LastActivityTime);
            TransferSpeedMeter.Clear();
            if (CompressData)
                ComprTransferSpeedMeter.Clear();
            TransferSpeedMeter.JustConnected();
            if (CompressData)
                ComprTransferSpeedMeter.JustConnected();
            if (DataTransferPostponed != 0)
            {
                PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, DataTransferPostponed == 1 ? FD_WRITE : FD_CLOSE);
                DataTransferPostponed = 0;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::JustConnected()
{
    ReceivedConnected = TRUE;
    FirstWriteAfterConnect = TRUE;
    SkippedWriteAfterConnect = 0;
    TransferSpeedMeter.JustConnected();
    if (CompressData)
        ComprTransferSpeedMeter.JustConnected();
    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    DoPostMessageToWorker(WorkerMsgConnectedToServer);
    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_TESTNODATATRTIMEOUT,
                            DATACON_TESTNODATATRTIMERID, NULL);
}

void CUploadDataConnectionSocket::ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::ConnectionAccepted()");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount != 1)
        TRACE_E("Incorrect call to CUploadDataConnectionSocket::ConnectionAccepted(): must be one-time-entered in section SocketCritSect!");
#endif

    if (success)
    {
        LastActivityTime = GetTickCount(); // doslo k uspesnemu acceptu
        if (GlobalLastActivityTime != NULL)
            GlobalLastActivityTime->Set(LastActivityTime);
        NetEventLastError = NO_ERROR; // v predchozim acceptu mohlo dojit k chybe, ted uz neni aktualni
        SSLErrorOccured = SSLCONERR_NOERROR;
        JustConnected();
    }
    else // chyba - posleme ji aspon do logu
    {
        NetEventLastError = winError;
        if (proxyError && NetEventLastError == NO_ERROR)
            NetEventLastError = ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */;
        LogNetEventLastError(proxyError);
    }
}

DWORD GetPacketSizeEstimation(DWORD speed, DWORD tooBigPacketSize)
{
    if (tooBigPacketSize == -1)
        tooBigPacketSize = 1000000; // vyradime 'tooBigPacketSize' z cinnosti
    if (speed < 4096 || tooBigPacketSize <= 1024)
        return 512; // tohle to vybere i pokud to zakazuje 'tooBigPacketSize' (neco to proste vybrat musi)
    else
    {
        if (speed < 8192 || tooBigPacketSize <= 4096)
            return 1024;
        else
        {
            if (speed < 32768 || tooBigPacketSize <= 8192)
                return 4096;
            else
            {
                if (speed < 65536 || tooBigPacketSize <= 32768)
                    return 8192;
                else
                    return 32768;
            }
        }
    }
}

#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
void CUploadDataConnectionSocket::DebugLogPacketSizeAndWriteSize(int size, BOOL noChangeOfLastPacketSizeEstimation)
{
    char buf[300];
    if (size == 0)
    {
        if (DebugSentButNotLoggedBytes > 0)
        {
            sprintf(buf, "Sent size: %u bytes (in %u blocks)\r\n", DebugSentButNotLoggedBytes, DebugSentButNotLoggedCount);
            Logs.LogMessage(LogUID, buf, -1, TRUE);
            DebugSentButNotLoggedBytes = 0;
            DebugSentButNotLoggedCount = 0;
        }
        sprintf(buf, "%s send block size %s%u bytes (BytesSentAfterBlkSizeCh=%u,BlockSizeChangeSpeed=%u)\r\n",
                (noChangeOfLastPacketSizeEstimation ? "Keeping" : "Changing"),
                (noChangeOfLastPacketSizeEstimation ? "" : "to "), LastPacketSizeEstimation,
                BytesSentAfterPckSizeCh, PacketSizeChangeSpeed);
        Logs.LogMessage(LogUID, buf, -1, TRUE);
        if (TooBigPacketSize != -1)
        {
            sprintf(buf, "(Too big send block size is %u bytes)\r\n", TooBigPacketSize);
            Logs.LogMessage(LogUID, buf, -1, TRUE);
        }
        DebugLastWriteToLog = GetTickCount();
    }
    else
    {
        DebugSentButNotLoggedBytes += size;
        DebugSentButNotLoggedCount++;
        if (GetTickCount() - DebugLastWriteToLog >= 1000)
        {
            sprintf(buf, "Sent size: %u bytes (in %u blocks)\r\n", DebugSentButNotLoggedBytes, DebugSentButNotLoggedCount);
            Logs.LogMessage(LogUID, buf, -1, TRUE);
            DebugSentButNotLoggedBytes = 0;
            DebugSentButNotLoggedCount = 0;
            DebugLastWriteToLog = GetTickCount();
        }
    }
}
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE

void CUploadDataConnectionSocket::MoveFlushBufferToBytesToWrite()
{
    char* swap = BytesToWrite;
    BytesToWrite = FlushBuffer;
    BytesToWriteCount = ValidBytesInFlushBuffer;
    BytesToWriteOffset = 0;
    DecomprBytesInBytesToWrite = DecomprBytesInFlushBuffer;
    FlushBuffer = swap;
    ValidBytesInFlushBuffer = 0;
    AlreadyComprPartOfFlushBuffer = 0;
    DecomprBytesInFlushBuffer = 0;
}

void CUploadDataConnectionSocket::ReceiveNetEvent(LPARAM lParam, int index)
{
    CALL_STACK_MESSAGE3("CUploadDataConnectionSocket::ReceiveNetEvent(0x%IX, %d)", lParam, index);
    DWORD eventError = WSAGETSELECTERROR(lParam); // extract error code of event
    BOOL logLastErr = FALSE;
    switch (WSAGETSELECTEVENT(lParam)) // extract event
    {
    case FD_WRITE:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));

        if (ReceivedConnected || UsePassiveMode) // ignorujeme moznost zapisu na "listen" socket
        {
            WaitingForWriteEvent = FALSE; // predpoklad: pro zahajeni zapisu bude potreba postnout FD_WRITE (pokud 'send' vrati chybu "would-block", zmenime hodnotu na TRUE)
            if (eventError == NO_ERROR)
            {
                if (UsePassiveMode) // u aktivniho spojeni musime cekat na FD_ACCEPT (tenhle socket je "listen", a pak az "data connection")
                {
                    if (!ReceivedConnected)
                        JustConnected(); // pokud prijde FD_WRITE pred FD_CONNECT (kazdopadne musi byt connected)
                }

                if (WorkerPaused || EncryptConnection && !Activated && UsePassiveMode)
                { // SSL: Don't send data before the connection is confirmed by the server and activated by the worker
                    if (DataTransferPostponed == 0)
                        DataTransferPostponed = 1 /* FD_WRITE */; // nelze zapomenout FD_CLOSE na ukor FD_WRITE
                }
                else
                {
                    if (Socket != INVALID_SOCKET) // socket je pripojeny
                    {
                        BOOL errorOccured = FALSE;
                        if (FirstWriteAfterConnect)
                        {
                            FirstWriteAfterConnect = FALSE;

                            if (EncryptConnection)
                            {
                                int err;
                                if (!EncryptSocket(LogUID, &err, NULL, NULL, NULL, 0, SSLConForReuse))
                                {
                                    errorOccured = TRUE;
                                    SSLErrorOccured = err;
                                    CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                                    FreeBufferedData();
                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                    DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                }
                            }

                            FirstWriteAfterConnectTime = GetTickCount();
                            LastSpeedTestTime = GetTickCount() - 4000; // at se rychlost testne vterinu po zacatku prenosu
                            LastPacketSizeEstimation = 4096;           // pocatecni kompromis (pro prvni sekundu prenosu)
                            if (LastPacketSizeEstimation >= TooBigPacketSize)
                                LastPacketSizeEstimation = 512;
                            PacketSizeChangeTime = LastSpeedTestTime - 1000; // merime jen pokud LastSpeedTestTime==PacketSizeChangeTime, timto mereni vyradime
                            BytesSentAfterPckSizeCh = 0;
                            PacketSizeChangeSpeed = 0;
#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
                            DebugSentButNotLoggedBytes = 0;
                            DebugSentButNotLoggedCount = 0;
                            DebugLogPacketSizeAndWriteSize(0, TRUE);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE
                        }
                        if (!errorOccured && BytesToWriteCount > BytesToWriteOffset) // mame nejaka data k odeslani z bufferu 'BytesToWrite'
                        {
                            while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite'; posila FD_WRITE jen pri chybe "would-block")
                            {
                                int paketSize = LastPacketSizeEstimation; // kolik bytu poslat v jednom kole (pro lokalni spojeni je idealni 32KB, pro inet je lepsi 512 az 4096 bytu)
                                if (BytesToWriteCount - BytesToWriteOffset < paketSize)
                                    paketSize = BytesToWriteCount - BytesToWriteOffset;
                                int sentLen;

                                if (!SSLConn)
                                    sentLen = send(Socket, BytesToWrite + BytesToWriteOffset, paketSize, 0);
                                else
                                    sentLen = SSLLib.SSL_write(SSLConn, BytesToWrite + BytesToWriteOffset, paketSize);

                                if (sentLen >= 0 /*!= SOCKET_ERROR*/) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                                {
#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
                                    DebugLogPacketSizeAndWriteSize(sentLen);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE

                                    int decomprSentLen = sentLen;
                                    if (CompressData)
                                    {
                                        decomprSentLen = (int)(((__int64)sentLen * (__int64)DecomprBytesInBytesToWrite) /
                                                               (BytesToWriteCount - BytesToWriteOffset));
                                        DecomprBytesInBytesToWrite -= decomprSentLen;
                                    }
                                    BytesToWriteOffset += sentLen;
                                    TotalWrittenBytesCount += CQuadWord(decomprSentLen, 0);
                                    //                      if (BytesToWriteOffset >= BytesToWriteCount && ValidBytesInFlushBuffer == 0 && EndOfFileReached)
                                    //                        TRACE_I("TotalWrittenBytesCount=" << (int)TotalWrittenBytesCount.Value);

                                    LastActivityTime = GetTickCount();
                                    if (GlobalLastActivityTime != NULL)
                                        GlobalLastActivityTime->Set(LastActivityTime);
                                    if (LastActivityTime - FirstWriteAfterConnectTime > 100)
                                    { // pri uploadu se na zacatku plni lokalni buffer (klidne 8KB za 1ms, nebo hure 45KB za 8ms pri spojeni s localhost (zrejme se plni nejen send-buffer ale i receive-buffer)), rychlost plneni tohoto bufferu nemerime (umele a nesmyslne zvysuje rychlost uploadu)
                                        TransferSpeedMeter.BytesReceived(decomprSentLen, LastActivityTime);
                                        if (CompressData)
                                            ComprTransferSpeedMeter.BytesReceived(sentLen, LastActivityTime);

                                        if (PacketSizeChangeTime == LastSpeedTestTime) // pri minulem testu rychlosti doslo ke zmene LastPacketSizeEstimation, musime overit jestli se tim prenos nedegradoval (na Windows XP se to deje pri praci na intranetu, rychlost kolem 5MB/s, po zvoleni LastPacketSizeEstimation==32KB klesne prenosova rychlost na 160KB/s, projevuje se jen u nekterych FTP serveru - napr. RaidenFTPD v2.4)
                                        {
                                            if (LastActivityTime - PacketSizeChangeTime <= 1000) // nascitame kolik bytu se prenese behem jedne vteriny po zmene LastPacketSizeEstimation
                                                BytesSentAfterPckSizeCh += sentLen;
                                            else
                                            {
                                                if (PacketSizeChangeSpeed / 3 > BytesSentAfterPckSizeCh) // doslo k totalni degradaci, musime snizit LastPacketSizeEstimation
                                                {
                                                    TooBigPacketSize = LastPacketSizeEstimation;
                                                    DWORD newLastPacketSizeEstimation = GetPacketSizeEstimation(PacketSizeChangeSpeed, TooBigPacketSize);
                                                    if (LastPacketSizeEstimation != newLastPacketSizeEstimation)
                                                    {
                                                        LastPacketSizeEstimation = newLastPacketSizeEstimation;
#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
                                                        DebugLogPacketSizeAndWriteSize(0);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE
                                                        LastSpeedTestTime = LastActivityTime;
                                                        PacketSizeChangeTime = LastActivityTime;
                                                        BytesSentAfterPckSizeCh = 0;
                                                    }
                                                    else // LastPacketSizeEstimation musi zustat na nove hodnote (uz neni kam snizovat)
                                                    {
                                                        TRACE_E("Unexpected situation: TooBigPacketSize==" << TooBigPacketSize << " (it cannot be smaller)!");
                                                        PacketSizeChangeTime = LastSpeedTestTime - 1000;
                                                    }
                                                }
                                                else
                                                    PacketSizeChangeTime = LastSpeedTestTime - 1000; // vse OK, LastPacketSizeEstimation muze zustat na nove hodnote
                                            }
                                        }

                                        if (LastActivityTime - LastSpeedTestTime >= 5000) // jednou za pet sekund kalibrujeme velikost "paketu" podle rychlosti spojeni
                                        {
                                            LastSpeedTestTime = LastActivityTime;
                                            DWORD speed = CompressData ? ComprTransferSpeedMeter.GetSpeed(NULL) : TransferSpeedMeter.GetSpeed(NULL);
                                            //                          if (CompressData) TRACE_I("ComprTransferSpeedMeter=" << speed);
                                            DWORD newLastPacketSizeEstimation = GetPacketSizeEstimation(speed, TooBigPacketSize);
                                            if (LastPacketSizeEstimation != newLastPacketSizeEstimation)
                                            {
                                                LastPacketSizeEstimation = newLastPacketSizeEstimation;
#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
                                                DebugLogPacketSizeAndWriteSize(0);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE
                                                PacketSizeChangeTime = LastActivityTime;
                                                BytesSentAfterPckSizeCh = 0;
                                                PacketSizeChangeSpeed = speed;
                                            }
#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
                                            else
                                            {
                                                DebugLogPacketSizeAndWriteSize(0, TRUE);
                                            }
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE
                                        }
                                        if (GlobalTransferSpeedMeter != NULL)
                                            GlobalTransferSpeedMeter->BytesReceived(decomprSentLen, LastActivityTime);
                                    }
                                    else
                                        SkippedWriteAfterConnect += decomprSentLen;

                                    if (BytesToWriteCount <= BytesToWriteOffset) // uz je poslano vsechno v bufferu BytesToWrite?
                                    {
                                        if (ValidBytesInFlushBuffer > 0)
                                            MoveFlushBufferToBytesToWrite(); // do bufferu BytesToWrite dame data z bufferu FlushBuffer
                                        else
                                            break; // prestaneme posilat (jiz neni co)
                                    }
                                }
                                else
                                {
                                    DWORD err = !SSLConn ? WSAGetLastError() : SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                                    if (err == WSAEWOULDBLOCK)       // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                                        WaitingForWriteEvent = TRUE; // prestaneme posilat, cekame na prijeti dalsiho FD_WRITE
                                    else                             // jina chyba - ohlasime ji
                                    {
                                        NetEventLastError = err;
                                        logLastErr = TRUE;
                                        CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                                        FreeBufferedData();
                                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                        DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                    }
                                    break;
                                }
                            }
                        }
                        if (BytesToWriteCount <= BytesToWriteOffset && EndOfFileReached)
                        { // uz nemame co poslat + jsme na konci souboru => prenos je hotovy
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            SocketsThread->DeleteTimer(UID, DATACON_TESTNODATATRTIMERID);

                            ConnectionClosedOnEOF = TRUE;
                            CloseSocketEx(NULL); // zavreme "data connection", ukoncime tim prenos souboru
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            DoPostMessageToWorker(WorkerMsgConnectionClosed);
                        }
                    }
                    //            else   // stava se pri cancelovani uploadu na lokalni server (velka rychlost uploadu)
                    //              TRACE_E("Unexpected situation in CUploadDataConnectionSocket::ReceiveNetEvent(FD_WRITE): Socket is not connected.");

                    // pokud je jeste otevrena data-connectiona, je misto ve flush bufferu a jeste necekame
                    // na pripravu dat a nedocetli jsme se jeste ke konci souboru, nechame pripravit data
                    // do flush bufferu
                    if (Socket != INVALID_SOCKET && ValidBytesInFlushBuffer == 0 &&
                        !PrepareDataMsgWasSent && !EndOfFileReached)
                    {
                        PrepareDataMsgWasSent = TRUE;
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        DoPostMessageToWorker(WorkerMsgPrepareData);
                    }
                }
            }
            else // hlaseni chyby v FD_WRITE (podle helpu jen WSAENETDOWN)
            {
                NetEventLastError = eventError;
                logLastErr = TRUE;
                CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                FreeBufferedData();
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                DoPostMessageToWorker(WorkerMsgConnectionClosed);
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case FD_CLOSE:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (UsePassiveMode && !ReceivedConnected)
            JustConnected(); // pokud prijde FD_CLOSE pred FD_CONNECT (kazdopadne musi byt connected)
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (WorkerPaused)
            DataTransferPostponed = 2 /* FD_CLOSE */;
        else
        {
            logLastErr = (eventError != NO_ERROR);
            CSocket::ReceiveNetEvent(lParam, index); // zavolame metodu predka
        }
        break;
    }

    case FD_CONNECT:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (eventError == NO_ERROR)
        {
            if (!ReceivedConnected)
                JustConnected();
            LastActivityTime = GetTickCount(); // doslo k uspesnemu connectu
            if (GlobalLastActivityTime != NULL)
                GlobalLastActivityTime->Set(LastActivityTime);
        }
        else
        {
            NetEventLastError = eventError;
            logLastErr = TRUE;
            CloseSocketEx(NULL); // zavreme socket "data connection", uz se nemuze otevrit
            FreeBufferedData();  // nejspis zbytecne, jen pro sychr (kdyby FD_WRITE prisel pred FD_CONNECT)
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            DoPostMessageToWorker(WorkerMsgConnectionClosed);
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }

    case FD_ACCEPT:
        CSocket::ReceiveNetEvent(lParam, index);
        break;
    }

    if (logLastErr)
        LogNetEventLastError(WSAGETSELECTEVENT(lParam) == FD_CONNECT);
}

void CUploadDataConnectionSocket::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::ReceiveTimer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (id == DATACON_TESTNODATATRTIMERID && Socket != INVALID_SOCKET)
    { // periodicke testovani no-data-transfer timeoutu
        if (!WorkerPaused &&
            (GetTickCount() - LastActivityTime) / 1000 >= (DWORD)Config.GetNoDataTransferTimeout())
        { // timeout nastal, zavreme data-connectionu - nasimulujeme, ze to udelal server
            NoDataTransTimeout = TRUE;
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGNODATATRTIMEOUT), -1, TRUE);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            CSocket::ReceiveNetEvent(MAKELPARAM(FD_CLOSE, WSAECONNRESET), GetMsgIndex()); // zavolame metodu predka
            HANDLES(EnterCriticalSection(&SocketCritSect));
        }
        else // zatim timeout nenastal, pridame timer pro dalsi test
        {
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_TESTNODATATRTIMEOUT,
                                    DATACON_TESTNODATATRTIMERID, NULL);
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CUploadDataConnectionSocket::GiveBufferForData(char** flushBuffer)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::GiveBufferForData()");

    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (ValidBytesInFlushBuffer == 0 && FlushBuffer != NULL &&
        (!CompressData || ComprDataAllocatedSize == 0 || ComprDataBuffer != NULL))
    {
        if (CompressData)
        {
            if (ComprDataDelayedOffset < ComprDataDelayedCount) // odlozena komprimace (v predchozim cyklu uz nebylo misto ve FlushBuffer)
            {
                DataBufferPrepared(NULL, 0, FALSE); // v SocketCritSect jiz jsme a nesmime tam byt dvakrat (kvuli DoPostMessageToWorker)
                *flushBuffer = NULL;
            }
            else
            {
                if (ComprDataAllocatedSize == 0)
                {
                    ComprDataAllocatedSize = DATACON_UPLOADFLUSHBUFFERSIZE;
                    if (ComprDataBuffer != NULL)
                    {
                        TRACE_E("Unexpected situation in CUploadDataConnectionSocket::GiveBufferForData(): ComprDataBuffer is not NULL, but ComprDataAllocatedSize is 0");
                        free(ComprDataBuffer);
                    }
                    ComprDataBuffer = (char*)malloc(ComprDataAllocatedSize);
                }
                ret = TRUE;
                *flushBuffer = ComprDataBuffer;
                ComprDataBuffer = NULL;
                ComprDataDelayedOffset = 0;
                ComprDataDelayedCount = 0;
            }
        }
        else
        {
            ret = TRUE;
            *flushBuffer = FlushBuffer;
            FlushBuffer = NULL;
        }
    }
    else
    {
        *flushBuffer = NULL;
        if (ValidBytesInFlushBuffer == 0)
            TRACE_E("CUploadDataConnectionSocket::GiveBufferForData(): FlushBuffer or ComprDataBuffer has been already given!");
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CUploadDataConnectionSocket::DataBufferPrepared(char* flushBuffer, int validBytesInFlushBuffer, BOOL enterCS)
{
    CALL_STACK_MESSAGE2("CUploadDataConnectionSocket::DataBufferPrepared(, %d)", validBytesInFlushBuffer);

    if (enterCS)
        HANDLES(EnterCriticalSection(&SocketCritSect));

    PrepareDataMsgWasSent = FALSE;
    BOOL endOfFile = FALSE;
    if (flushBuffer != NULL)
    {
        if (CompressData)
        {
            if (ComprDataBuffer != NULL)
            {
                TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): ComprDataBuffer is not NULL!");
                if (ComprDataDelayedOffset < ComprDataDelayedCount)
                    TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): FATAL ERROR: ComprDataDelayedOffset < ComprDataDelayedCount: losing file data");
                free(ComprDataBuffer);
            }
            ComprDataBuffer = flushBuffer;
            endOfFile = validBytesInFlushBuffer == 0; // prazdny buffer == jsme na konci souboru
        }
        else
        {
            if (FlushBuffer != NULL)
            {
                TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): FlushBuffer is not NULL!");
                free(FlushBuffer);
            }
            FlushBuffer = flushBuffer;
            if (ValidBytesInFlushBuffer != 0)
                TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): ValidBytesInFlushBuffer is not zero!");
            ValidBytesInFlushBuffer = validBytesInFlushBuffer;
            if (ValidBytesInFlushBuffer == 0)
                EndOfFileReached = TRUE; // prazdny buffer == jsme na konci souboru
        }
    }
    else
    {
        if (CompressData && ComprDataBuffer != NULL && ComprDataDelayedOffset < ComprDataDelayedCount)
        {
            flushBuffer = ComprDataBuffer + ComprDataDelayedOffset;
            validBytesInFlushBuffer = ComprDataDelayedCount - ComprDataDelayedOffset;
        }
        else
            TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): flushBuffer cannot be NULL!");
    }

    if (CompressData && flushBuffer != NULL && FlushBuffer != NULL)
    { // provedeme komprimaci dat do FlushBuffer od offsetu AlreadyComprPartOfFlushBuffer
        ZLIBInfo.avail_in = validBytesInFlushBuffer;
        ZLIBInfo.next_in = (BYTE*)flushBuffer;
        ZLIBInfo.avail_out = DATACON_UPLOADFLUSHBUFFERSIZE - AlreadyComprPartOfFlushBuffer;
        ZLIBInfo.next_out = (BYTE*)FlushBuffer + AlreadyComprPartOfFlushBuffer;

        int err = SalZLIB->Deflate(&ZLIBInfo, endOfFile ? SAL_Z_FINISH : SAL_Z_NO_FLUSH);

        AlreadyComprPartOfFlushBuffer = (int)(ZLIBInfo.next_out - (BYTE*)FlushBuffer);
        DecomprBytesInFlushBuffer += validBytesInFlushBuffer - ZLIBInfo.avail_in;
        if (err == SAL_Z_STREAM_END)
            EndOfFileReached = TRUE; // podarilo se zkomprimovat vsechna data a ukoncit stream == jsme na konci souboru
        if (err < 0)
            TRACE_E("SalZLIB->Deflate returns unexpected error: " << err);
        else // uspesna komprese
        {
            if (ZLIBInfo.avail_in > 0) // maly FlushBuffer, nezkoprimovali jsme vsechna data - zbytek odlozime na priste
            {
                ComprDataDelayedOffset = (int)(ZLIBInfo.next_in - (BYTE*)ComprDataBuffer);
                ComprDataDelayedCount = ComprDataDelayedOffset + ZLIBInfo.avail_in;
                ValidBytesInFlushBuffer = AlreadyComprPartOfFlushBuffer; // timto "prizname" naplneni FlushBuffer
                if (AlreadyComprPartOfFlushBuffer != DATACON_UPLOADFLUSHBUFFERSIZE)
                    TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): Deflate returns 'insufficient output buffer', but AlreadyComprPartOfFlushBuffer is not DATACON_UPLOADFLUSHBUFFERSIZE");
            }
            else
            {
                ComprDataDelayedOffset = 0;
                ComprDataDelayedCount = 0;
                if (!EndOfFileReached && AlreadyComprPartOfFlushBuffer < DATACON_UPLOADFLUSHBUFFERSIZE)
                { // jeste jsme nezakomprimovali EOF a mame volne misto ve FlushBuffer - pokracujeme ve cteni souboru
                    PrepareDataMsgWasSent = TRUE;
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    DoPostMessageToWorker(WorkerMsgPrepareData);
                }
                else
                    ValidBytesInFlushBuffer = AlreadyComprPartOfFlushBuffer; // timto "prizname" naplneni FlushBuffer
            }
        }
    }

    if (ValidBytesInFlushBuffer > 0 && BytesToWriteCount <= BytesToWriteOffset)
    { // je-li potreba cist dalsi data z disku
        MoveFlushBufferToBytesToWrite();

        if (!EndOfFileReached)
        {
            PrepareDataMsgWasSent = TRUE;
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            DoPostMessageToWorker(WorkerMsgPrepareData);
        }
    }

    if (!WaitingForWriteEvent && (BytesToWriteOffset < BytesToWriteCount || EndOfFileReached))
    {
        PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_WRITE);
        WaitingForWriteEvent = TRUE;
    }

    if (enterCS)
        HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::GetError(DWORD* netErr, BOOL* noDataTransTimeout, int* sslErrorOccured)
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::GetError()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (netErr != NULL)
        *netErr = NetEventLastError;
    if (noDataTransTimeout != NULL)
        *noDataTransTimeout = NoDataTransTimeout;
    if (sslErrorOccured != NULL)
        *sslErrorOccured = SSLErrorOccured;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}
