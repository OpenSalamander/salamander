// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
    if (CountOfTrBytesItems > 0) // after establishing the connection this is "always true"
    {
        int actIndexAdded = 0;                           // 0 = current index was not included, 1 = current index was included
        int emptyTrBytes = 0;                            // number of counted empty steps
        CQuadWord total(0, 0);                           // total number of bytes over the last max. DATACON_ACTSPEEDNUMOFSTEPS steps
        int addFromTrBytes = CountOfTrBytesItems - 1;    // number of closed steps to add from the queue
        DWORD restTime = 0;                              // time from the last counted step up to now
        if ((int)(time - ActIndexInTrBytesTimeLim) >= 0) // current index is already closed and empty steps might be needed
        {
            emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / DATACON_ACTSPEEDSTEP;
            restTime = (time - ActIndexInTrBytesTimeLim) % DATACON_ACTSPEEDSTEP;
            emptyTrBytes = min(emptyTrBytes, DATACON_ACTSPEEDNUMOFSTEPS);
            if (emptyTrBytes < DATACON_ACTSPEEDNUMOFSTEPS) // empty steps alone are not enough, include the current index as well
            {
                total = CQuadWord(TransferedBytes[ActIndexInTrBytes], 0);
                actIndexAdded = 1;
            }
            addFromTrBytes = DATACON_ACTSPEEDNUMOFSTEPS - actIndexAdded - emptyTrBytes;
            addFromTrBytes = min(addFromTrBytes, CountOfTrBytesItems - 1); // how many closed steps from the queue still need to be counted
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
                actIndex = DATACON_ACTSPEEDNUMOFSTEPS; // move along the circular queue
            total += CQuadWord(TransferedBytes[actIndex], 0);
        }
        DWORD t = (addFromTrBytes + actIndexAdded + emptyTrBytes) * DATACON_ACTSPEEDSTEP + restTime;
        if (t > 0)
            speed = (DWORD)((CQuadWord(1000, 0) * total) / CQuadWord(t, 0)).Value;
        else
            speed = 0; // nothing to calculate yet, report "0 B/s" for now
    }
    else
        speed = 0; // nothing to calculate yet, report "0 B/s" for now
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
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CTransferSpeedMeter::BytesReceived(,)"); // parameters ignored for performance reasons (the call stack slows it down enough)

    HANDLES(EnterCriticalSection(&TransferSpeedMeterCS));
    if (count > 0)
        LastTransferTime = time;
    if ((int)(time - ActIndexInTrBytesTimeLim) < 0) // within the current time interval, just add the byte count to the interval
    {
        TransferedBytes[ActIndexInTrBytes] += count;
    }
    else // not in the current time interval, we have to start a new one
    {
        int emptyTrBytes = (time - ActIndexInTrBytesTimeLim) / DATACON_ACTSPEEDSTEP;
        int i = min(emptyTrBytes, DATACON_ACTSPEEDNUMOFSTEPS); // more has no effect (the entire queue would be cleared)
        if (i > 0 && CountOfTrBytesItems <= DATACON_ACTSPEEDNUMOFSTEPS)
            CountOfTrBytesItems = min(DATACON_ACTSPEEDNUMOFSTEPS + 1, CountOfTrBytesItems + i);
        while (i--)
        {
            if (++ActIndexInTrBytes > DATACON_ACTSPEEDNUMOFSTEPS)
                ActIndexInTrBytes = 0; // move along the circular queue
            TransferedBytes[ActIndexInTrBytes] = 0;
        }
        ActIndexInTrBytesTimeLim += (emptyTrBytes + 1) * DATACON_ACTSPEEDSTEP;
        if (++ActIndexInTrBytes > DATACON_ACTSPEEDNUMOFSTEPS)
            ActIndexInTrBytes = 0; // move along the circular queue
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
    SocketCloseTime = GetTickCount(); // just in case, it should be overwritten before calling GetSocketCloseTime()
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
    LastActivityTime = GetTickCount(); // we rely on the object being initialized before the transfer command is sent (the command will have an equal or shorter timeout)
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

    // before the next connect we reset...
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (auxUsePassiveMode)
    {
        DWORD err; // 'ProxyServer' is accessible in data connections without a critical section
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
        if (!conRes) // no chance of success
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            NetEventLastError = err; // record the error (except fatal errors such as out-of-memory)
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
                *s = 0; // trim newline characters from the error text
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
            *error = NO_ERROR; // unknown error
        return FALSE;
    }
}

void CKeepAliveDataConSocket::SetActive(int logUID)
{
    CALL_STACK_MESSAGE2("CKeepAliveDataConSocket::SetActive(%d)", logUID);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    UsePassiveMode = FALSE;
    LogUID = logUID;

    // before another attempt to establish the connection we reset...
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;
    LastActivityTime = GetTickCount(); // we rely on the object being initialized before the transfer command is sent (the command will have an equal or shorter timeout)

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CKeepAliveDataConSocket::FinishDataTransfer(int replyCode)
{
    CALL_STACK_MESSAGE1("CKeepAliveDataConSocket::FinishDataTransfer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = TRUE;
    if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS) // LIST does not return success – it might not close the data connection at all
        CloseSocketEx(NULL);                      // data connection (e.g. WarFTPD) - close it ourselves
    else
    {
        BOOL con = IsConnected();
        if (!ReceivedConnected && con) // the connection has not been established yet and the socket is open
            CloseSocketEx(NULL);       // close the socket (just waiting for a connection), the server probably reports a command error (listing)
        else
            ret = !con; // was the connection established and already closed? (finished?)
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
        *transferFinished = ReceivedConnected && !ret; // was the connection established but already closed? (finished)
    ret = ReceivedConnected && ret;                    // was the connection established and still open? (transferring)
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

    if (passiveModeRetry) // in passive mode the first connection attempt was rejected, perform a second attempt
    {
        // CloseSocketEx(NULL);           // no point in closing the old data-connection socket (it must already be closed)
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
        if (Socket != INVALID_SOCKET) // always true: the socket is connected
            CloseSocketEx(NULL);      // close the data connection; keeping it open no longer makes sense
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CKeepAliveDataConSocket::JustConnected()
{
    ReceivedConnected = TRUE; // if FD_READ arrives before FD_CONNECT (it has to be connected anyway)
    // because we are already inside CSocketsThread::CritSect, this call is
    // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
        *s = 0; // trim newline characters from the error text
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
            if (Socket != INVALID_SOCKET) // always true: the socket is connected
                CloseSocketEx(NULL);      // close the data connection; keeping it open no longer makes sense
            success = FALSE;
        }
    }
    if (success)
    {
        NetEventLastError = NO_ERROR; // a previous accept may have failed; it is no longer relevant now
        SSLErrorOccured = SSLCONERR_NOERROR;
        JustConnected();
        LastActivityTime = GetTickCount(); // a successful accept occurred
    }
    else // an error occurred - log it at least
    {
        NetEventLastError = winError;
        if (proxyError && NetEventLastError == NO_ERROR)
            NetEventLastError = ERROR_INVALID_FUNCTION /* it just must not be NO_ERROR */;
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
    case FD_CLOSE: // sometimes arrives before the final FD_READ, so try FD_READ first and if it succeeds post FD_CLOSE again (another FD_READ may still succeed before it)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE; // TRUE = FD_CLOSE arrived and there was data to read (handled as FD_READ) => post FD_CLOSE again (the current FD_CLOSE was a false alarm)
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (ReceivedConnected || UsePassiveMode) // ignore closing of the listen socket
        {
            if (eventError == NO_ERROR) // only if no error occurred (documentation says only WSAENETDOWN can happen)
            {
                if (UsePassiveMode) // for an active connection we must wait for FD_ACCEPT (this socket is the listener, then the data connection)
                {
                    if (!ReceivedConnected)
                        JustConnected();
                }

                if (Socket != INVALID_SOCKET) // the socket is connected
                {
                    // read as many bytes as possible into the buffer; do not loop so the data arrives gradually
                    // if there is still data to read we will receive another FD_READ
                    char buf[KEEPALIVEDATACON_READBUFSIZE];
                    int len;
                    if (!SSLConn)
                        len = recv(Socket, buf, KEEPALIVEDATACON_READBUFSIZE, 0);
                    else
                    {
                        if (SSLLib.SSL_pending(SSLConn) > 0) // if the internal SSL buffer is not empty recv() is not called and no further FD_READ arrives, so we must post it ourselves or the data transfer stops
                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                        len = SSLLib.SSL_read(SSLConn, buf, KEEPALIVEDATACON_READBUFSIZE);
                    }
                    if (len >= 0 /*!= SOCKET_ERROR*/) // we may have read something (0 = the connection is already closed)
                    {
                        if (len > 0)
                        {
                            LastActivityTime = GetTickCount(); // bytes were successfully read from the socket
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
                            NetEventLastError = err; // an error occurred
                            logLastErr = TRUE;
                            CloseSocketEx(NULL); // close the data connection; keeping it open no longer makes sense
                        }
                    }
                }
                else
                {
                    // can happen if the main thread closes the socket before FD_READ is delivered
                    // TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveNetEvent(FD_READ): Socket is not connected.");
                    // we will not queue an event with this unexpected error (solution: the user presses ESC)
                }
            }
            else
            {
                if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // let FD_CLOSE handle the error on its own
                {
                    NetEventLastError = eventError;
                    logLastErr = TRUE;
                    CloseSocketEx(NULL); // close the data connection; keeping it open no longer makes sense
                }
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        // now process FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            if (sendFDCloseAgain) // FD_CLOSE arrived instead of FD_READ => post FD_CLOSE again
            {
                PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                            (WPARAM)GetSocket(), lParam);
            }
            else // proper FD_CLOSE
            {
                logLastErr = (eventError != NO_ERROR);
                CSocket::ReceiveNetEvent(lParam, index); // call the base method
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
            LastActivityTime = GetTickCount(); // the connect succeeded
        }
        else
        {
            NetEventLastError = eventError;
            logLastErr = TRUE;
            CloseSocketEx(NULL); // close the data-connection socket; it cannot open anymore
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
    { // periodic no-data-transfer timeout check
        if ((GetTickCount() - LastActivityTime) / 1000 >= (DWORD)Config.GetNoDataTransferTimeout())
        { // timeout occurred, close the data connection to simulate the server doing it
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGNODATATRTIMEOUT), -1, TRUE);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            CSocket::ReceiveNetEvent(MAKELPARAM(FD_CLOSE, WSAECONNRESET), GetMsgIndex()); // call the base method
            HANDLES(EnterCriticalSection(&SocketCritSect));
        }
        else // the timeout has not happened yet, schedule another timer
        {
            // because we are already inside CSocketsThread::CritSect, this call is
            // also possible from CSocket::SocketCritSect (no risk of deadlock)
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

    // because we are already inside CSocketsThread::CritSect, this call is
    // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
    WaitingForWriteEvent = TRUE; // the first FD_WRITE arrives automatically, so initialize it to TRUE
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
    EndOfFileReached = FALSE; // if we read all the way to the end of the file, after discarding trailing data this is no longer true
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

    // because we are already inside CSocketsThread::CritSect, this call is
    // also possible from CSocket::SocketCritSect (no risk of deadlock)
    DoPostMessageToWorker(WorkerMsgConnectionClosed);

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CUploadDataConnectionSocket::UploadFinished()
{
    CALL_STACK_MESSAGE1("CUploadDataConnectionSocket::UploadFinished()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

    if (ConnectionClosedOnEOF && SkippedWriteAfterConnect > 0)
    { // account bytes from local buffers in the upload speed (we are not sure the local buffers will be sent, but without them the speed on small files that fit almost entirely into local buffers would be completely wrong—absurdly low)
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
    // because we are already inside CSocketsThread::CritSect, this call is
    // also possible from CSocket::SocketCritSect (no risk of deadlock)
    DoPostMessageToWorker(WorkerMsgConnectedToServer);
    // because we are already inside CSocketsThread::CritSect, this call is
    // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
        LastActivityTime = GetTickCount(); // a successful accept occurred
        if (GlobalLastActivityTime != NULL)
            GlobalLastActivityTime->Set(LastActivityTime);
        NetEventLastError = NO_ERROR; // a previous accept may have failed; it is no longer relevant now
        SSLErrorOccured = SSLCONERR_NOERROR;
        JustConnected();
    }
    else // an error occurred - log it at least
    {
        NetEventLastError = winError;
        if (proxyError && NetEventLastError == NO_ERROR)
            NetEventLastError = ERROR_INVALID_FUNCTION /* it just must not be NO_ERROR */;
        LogNetEventLastError(proxyError);
    }
}

DWORD GetPacketSizeEstimation(DWORD speed, DWORD tooBigPacketSize)
{
    if (tooBigPacketSize == -1)
        tooBigPacketSize = 1000000; // disable tooBigPacketSize
    if (speed < 4096 || tooBigPacketSize <= 1024)
        return 512; // select this even if tooBigPacketSize forbids it (something must be chosen)
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

        if (ReceivedConnected || UsePassiveMode) // ignore the possibility of writing to the listen socket
        {
            WaitingForWriteEvent = FALSE; // assumption: starting the write will require posting FD_WRITE (if send returns a "would-block" error we switch the value to TRUE)
            if (eventError == NO_ERROR)
            {
                if (UsePassiveMode) // for an active connection we must wait for FD_ACCEPT (this socket is the listener, then the data connection)
                {
                    if (!ReceivedConnected)
                        JustConnected(); // if FD_WRITE arrives before FD_CONNECT (it still must be connected)
                }

                if (WorkerPaused || EncryptConnection && !Activated && UsePassiveMode)
                { // SSL: Don't send data before the connection is confirmed by the server and activated by the worker
                    if (DataTransferPostponed == 0)
                        DataTransferPostponed = 1 /* FD_WRITE */; // do not drop FD_CLOSE in favor of FD_WRITE
                }
                else
                {
                    if (Socket != INVALID_SOCKET) // the socket is connected
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
                                    CloseSocketEx(NULL); // close the data connection; keeping it open no longer makes sense
                                    FreeBufferedData();
                                    // because we are already inside CSocketsThread::CritSect, this call is
                                    // also possible from CSocket::SocketCritSect (no risk of deadlock)
                                    DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                }
                            }

                            FirstWriteAfterConnectTime = GetTickCount();
                            LastSpeedTestTime = GetTickCount() - 4000; // let the speed test run one second after the transfer starts
                            LastPacketSizeEstimation = 4096;           // initial compromise (for the first second of the transfer)
                            if (LastPacketSizeEstimation >= TooBigPacketSize)
                                LastPacketSizeEstimation = 512;
                            PacketSizeChangeTime = LastSpeedTestTime - 1000; // measure only when LastSpeedTestTime == PacketSizeChangeTime; this disables that measurement
                            BytesSentAfterPckSizeCh = 0;
                            PacketSizeChangeSpeed = 0;
#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
                            DebugSentButNotLoggedBytes = 0;
                            DebugSentButNotLoggedCount = 0;
                            DebugLogPacketSizeAndWriteSize(0, TRUE);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE
                        }
                        if (!errorOccured && BytesToWriteCount > BytesToWriteOffset) // we have data to send from the BytesToWrite buffer
                        {
                            while (1) // loop needed because of send (it does not post FD_WRITE when sentLen < bytesToWrite; it posts FD_WRITE only on a "would-block" error)
                            {
                                int paketSize = LastPacketSizeEstimation; // how many bytes to send in one go (32KB is ideal for local connections, 512 to 4096 bytes work better for the internet)
                                if (BytesToWriteCount - BytesToWriteOffset < paketSize)
                                    paketSize = BytesToWriteCount - BytesToWriteOffset;
                                int sentLen;

                                if (!SSLConn)
                                    sentLen = send(Socket, BytesToWrite + BytesToWriteOffset, paketSize, 0);
                                else
                                    sentLen = SSLLib.SSL_write(SSLConn, BytesToWrite + BytesToWriteOffset, paketSize);

                                if (sentLen >= 0 /*!= SOCKET_ERROR*/) // at least something was successfully sent (or rather accepted by Windows; actual delivery is anyone's guess)
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
                                    { // during upload the local buffer fills first (e.g. 8KB in 1ms or 45KB in 8ms when connected to localhost, apparently both send and receive buffers fill up), we do not measure the rate of this filling because it would artificially and unfairly boost the upload speed
                                        TransferSpeedMeter.BytesReceived(decomprSentLen, LastActivityTime);
                                        if (CompressData)
                                            ComprTransferSpeedMeter.BytesReceived(sentLen, LastActivityTime);

                                        if (PacketSizeChangeTime == LastSpeedTestTime) // the previous speed test changed LastPacketSizeEstimation, we must verify that the transfer was not degraded (on Windows XP this happens on intranets around 5 MB/s; selecting LastPacketSizeEstimation==32KB drops transfer speed to 160 KB/s, observed only with some FTP servers such as RaidenFTPD v2.4)
                                        {
                                            if (LastActivityTime - PacketSizeChangeTime <= 1000) // accumulate how many bytes transfer during one second after changing LastPacketSizeEstimation
                                                BytesSentAfterPckSizeCh += sentLen;
                                            else
                                            {
                                                if (PacketSizeChangeSpeed / 3 > BytesSentAfterPckSizeCh) // transfer degraded completely, we must reduce LastPacketSizeEstimation
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
                                                    else // LastPacketSizeEstimation must stay at the new value (there is nowhere lower to go)
                                                    {
                                                        TRACE_E("Unexpected situation: TooBigPacketSize==" << TooBigPacketSize << " (it cannot be smaller)!");
                                                        PacketSizeChangeTime = LastSpeedTestTime - 1000;
                                                    }
                                                }
                                                else
                                                    PacketSizeChangeTime = LastSpeedTestTime - 1000; // all good, LastPacketSizeEstimation can stay at the new value
                                            }
                                        }

                                        if (LastActivityTime - LastSpeedTestTime >= 5000) // every five seconds calibrate the packet size according to the connection speed
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

                                    if (BytesToWriteCount <= BytesToWriteOffset) // is everything in the BytesToWrite buffer already sent?
                                    {
                                        if (ValidBytesInFlushBuffer > 0)
                                            MoveFlushBufferToBytesToWrite(); // move data from the FlushBuffer into the BytesToWrite buffer
                                        else
                                            break; // stop sending (nothing left)
                                    }
                                }
                                else
                                {
                                    DWORD err = !SSLConn ? WSAGetLastError() : SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                                    if (err == WSAEWOULDBLOCK)       // nothing else can be sent (Windows has no buffer space left)
                                        WaitingForWriteEvent = TRUE; // stop sending and wait for another FD_WRITE
                                    else                             // a different error occurred - report it
                                    {
                                        NetEventLastError = err;
                                        logLastErr = TRUE;
                                        CloseSocketEx(NULL); // close the data connection; keeping it open no longer makes sense
                                        FreeBufferedData();
                                        // because we are already inside CSocketsThread::CritSect, this call is
                                        // also possible from CSocket::SocketCritSect (no risk of deadlock)
                                        DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                    }
                                    break;
                                }
                            }
                        }
                        if (BytesToWriteCount <= BytesToWriteOffset && EndOfFileReached)
                        { // nothing left to send and we are at EOF => transfer is complete
                            // because we are already inside CSocketsThread::CritSect, this call is
                            // also possible from CSocket::SocketCritSect (no risk of deadlock)
                            SocketsThread->DeleteTimer(UID, DATACON_TESTNODATATRTIMERID);

                            ConnectionClosedOnEOF = TRUE;
                            CloseSocketEx(NULL); // close the data connection, finishing the file transfer
                            // because we are already inside CSocketsThread::CritSect, this call is
                            // also possible from CSocket::SocketCritSect (no risk of deadlock)
                            DoPostMessageToWorker(WorkerMsgConnectionClosed);
                        }
                    }
                    //            else   // happens when cancelling an upload to a local server (very high upload speed)
                    //              TRACE_E("Unexpected situation in CUploadDataConnectionSocket::ReceiveNetEvent(FD_WRITE): Socket is not connected.");

                    // if the data connection is still open, there is space in the flush buffer, and we are not waiting yet
                    // for data preparation and we have not read to EOF yet, ask to prepare more data
                    // into the flush buffer
                    if (Socket != INVALID_SOCKET && ValidBytesInFlushBuffer == 0 &&
                        !PrepareDataMsgWasSent && !EndOfFileReached)
                    {
                        PrepareDataMsgWasSent = TRUE;
                        // because we are already inside CSocketsThread::CritSect, this call is
                        // also possible from CSocket::SocketCritSect (no risk of deadlock)
                        DoPostMessageToWorker(WorkerMsgPrepareData);
                    }
                }
            }
            else // error reported in FD_WRITE (documentation says only WSAENETDOWN)
            {
                NetEventLastError = eventError;
                logLastErr = TRUE;
                CloseSocketEx(NULL); // close the data connection; keeping it open no longer makes sense
                FreeBufferedData();
                // because we are already inside CSocketsThread::CritSect, this call is
                // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
            JustConnected(); // if FD_CLOSE arrives before FD_CONNECT (it still has to be connected)
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (WorkerPaused)
            DataTransferPostponed = 2 /* FD_CLOSE */;
        else
        {
            logLastErr = (eventError != NO_ERROR);
            CSocket::ReceiveNetEvent(lParam, index); // call the base method
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
            LastActivityTime = GetTickCount(); // the connect succeeded
            if (GlobalLastActivityTime != NULL)
                GlobalLastActivityTime->Set(LastActivityTime);
        }
        else
        {
            NetEventLastError = eventError;
            logLastErr = TRUE;
            CloseSocketEx(NULL); // close the data-connection socket; it cannot open anymore
            FreeBufferedData();  // probably unnecessary, just to be safe (in case FD_WRITE arrives before FD_CONNECT)
            // because we are already inside CSocketsThread::CritSect, this call is
            // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
    { // periodic no-data-transfer timeout check
        if (!WorkerPaused &&
            (GetTickCount() - LastActivityTime) / 1000 >= (DWORD)Config.GetNoDataTransferTimeout())
        { // timeout occurred, close the data connection to simulate the server doing it
            NoDataTransTimeout = TRUE;
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGNODATATRTIMEOUT), -1, TRUE);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            CSocket::ReceiveNetEvent(MAKELPARAM(FD_CLOSE, WSAECONNRESET), GetMsgIndex()); // call the base method
            HANDLES(EnterCriticalSection(&SocketCritSect));
        }
        else // the timeout has not happened yet, schedule another timer
        {
            // because we are already inside CSocketsThread::CritSect, this call is
            // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
            if (ComprDataDelayedOffset < ComprDataDelayedCount) // deferred compression (there was no space in FlushBuffer in the previous cycle)
            {
                DataBufferPrepared(NULL, 0, FALSE); // we are already inside SocketCritSect and must not enter it twice (because of DoPostMessageToWorker)
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
            endOfFile = validBytesInFlushBuffer == 0; // empty buffer == we are at the end of the file
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
                EndOfFileReached = TRUE; // empty buffer == we are at the end of the file
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
    { // compress data into FlushBuffer starting at AlreadyComprPartOfFlushBuffer
        ZLIBInfo.avail_in = validBytesInFlushBuffer;
        ZLIBInfo.next_in = (BYTE*)flushBuffer;
        ZLIBInfo.avail_out = DATACON_UPLOADFLUSHBUFFERSIZE - AlreadyComprPartOfFlushBuffer;
        ZLIBInfo.next_out = (BYTE*)FlushBuffer + AlreadyComprPartOfFlushBuffer;

        int err = SalZLIB->Deflate(&ZLIBInfo, endOfFile ? SAL_Z_FINISH : SAL_Z_NO_FLUSH);

        AlreadyComprPartOfFlushBuffer = (int)(ZLIBInfo.next_out - (BYTE*)FlushBuffer);
        DecomprBytesInFlushBuffer += validBytesInFlushBuffer - ZLIBInfo.avail_in;
        if (err == SAL_Z_STREAM_END)
            EndOfFileReached = TRUE; // managed to compress all data and finish the stream == we are at the end of the file
        if (err < 0)
            TRACE_E("SalZLIB->Deflate returns unexpected error: " << err);
        else // successful compression
        {
            if (ZLIBInfo.avail_in > 0) // FlushBuffer is too small, not all data was compressed - postpone the rest for later
            {
                ComprDataDelayedOffset = (int)(ZLIBInfo.next_in - (BYTE*)ComprDataBuffer);
                ComprDataDelayedCount = ComprDataDelayedOffset + ZLIBInfo.avail_in;
                ValidBytesInFlushBuffer = AlreadyComprPartOfFlushBuffer; // this acknowledges that FlushBuffer is filled
                if (AlreadyComprPartOfFlushBuffer != DATACON_UPLOADFLUSHBUFFERSIZE)
                    TRACE_E("CUploadDataConnectionSocket::DataBufferPrepared(): Deflate returns 'insufficient output buffer', but AlreadyComprPartOfFlushBuffer is not DATACON_UPLOADFLUSHBUFFERSIZE");
            }
            else
            {
                ComprDataDelayedOffset = 0;
                ComprDataDelayedCount = 0;
                if (!EndOfFileReached && AlreadyComprPartOfFlushBuffer < DATACON_UPLOADFLUSHBUFFERSIZE)
                { // we have not compressed EOF yet and there is space in FlushBuffer - continue reading the file
                    PrepareDataMsgWasSent = TRUE;
                    // because we are already inside CSocketsThread::CritSect, this call is
                    // also possible from CSocket::SocketCritSect (no risk of deadlock)
                    DoPostMessageToWorker(WorkerMsgPrepareData);
                }
                else
                    ValidBytesInFlushBuffer = AlreadyComprPartOfFlushBuffer; // this acknowledges that FlushBuffer is filled
            }
        }
    }

    if (ValidBytesInFlushBuffer > 0 && BytesToWriteCount <= BytesToWriteOffset)
    { // if more data needs to be read from disk
        MoveFlushBufferToBytesToWrite();

        if (!EndOfFileReached)
        {
            PrepareDataMsgWasSent = TRUE;
            // because we are already inside CSocketsThread::CritSect, this call is
            // also possible from CSocket::SocketCritSect (no risk of deadlock)
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
