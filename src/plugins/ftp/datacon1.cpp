// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "spl_zlib.h"

//
// ****************************************************************************
// CDataConnectionBaseSocket
//

CDataConnectionBaseSocket::CDataConnectionBaseSocket(CFTPProxyForDataCon* proxyServer,
                                                     int encryptConnection, CCertificate* certificate,
                                                     int compressData, CSocket* conForReuse)
{
    UsePassiveMode = TRUE;
    ProxyServer = proxyServer;
    ServerIP = INADDR_NONE;
    ServerPort = 0;

    LogUID = -1;

    IsDataConnection = TRUE;

    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;
    LastActivityTime = GetTickCount(); // hresime na to, ze objekt se konstruuje pred poslanim transfer prikazu (prikaz bude mit kratsi nebo stejny timeout)
    SocketCloseTime = GetTickCount();  // pro jistotu, pred volanim GetSocketCloseTime() by se melo prepsat

    GlobalLastActivityTime = NULL;

    PostMessagesToWorker = FALSE;
    WorkerSocketMsg = -1;
    WorkerSocketUID = -1;
    WorkerMsgConnectedToServer = -1;
    WorkerMsgConnectionClosed = -1;
    WorkerMsgListeningForCon = -1;

    WorkerPaused = FALSE;
    DataTransferPostponed = 0;

    GlobalTransferSpeedMeter = NULL;

    ListenOnIP = INADDR_NONE;
    ListenOnPort = 0;
    EncryptConnection = encryptConnection;
    if (certificate)
    {
        pCertificate = certificate;
        pCertificate->AddRef();
    }
    CompressData = compressData;
    memset(&ZLIBInfo, 0, sizeof(ZLIBInfo));

    SSLConForReuse = conForReuse;
}

CDataConnectionBaseSocket::~CDataConnectionBaseSocket()
{
    if (ProxyServer != NULL)
        delete ProxyServer;
}

void CDataConnectionBaseSocket::SetPassive(DWORD ip, unsigned short port, int logUID)
{
    CALL_STACK_MESSAGE4("CDataConnectionBaseSocket::SetPassive(0x%X, %u, %d)", ip, port, logUID);
    HANDLES(EnterCriticalSection(&SocketCritSect));
    UsePassiveMode = TRUE;
    ServerIP = ip;
    ServerPort = port;
    LogUID = logUID;
    LastActivityTime = GetTickCount(); // hresime na to, ze objekt se inicializuje pred poslanim transfer prikazu (prikaz bude mit kratsi nebo stejny timeout)
    if (GlobalLastActivityTime != NULL)
        GlobalLastActivityTime->Set(LastActivityTime);
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionBaseSocket::SetGlobalLastActivityTime(CSynchronizedDWORD* globalLastActivityTime)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::SetGlobalLastActivityTime()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    GlobalLastActivityTime = globalLastActivityTime;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionBaseSocket::PassiveConnect(DWORD* error)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::PassiveConnect()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL auxUsePassiveMode = UsePassiveMode;
    DWORD auxServerIP = ServerIP;
    unsigned short auxServerPort = ServerPort;
    int logUID = LogUID;

    // pred dalsim connectem nulujeme...
    ClearBeforeConnect();
    TransferSpeedMeter.Clear();
    if (CompressData)
        ComprTransferSpeedMeter.Clear();
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
        {
            if (!HostAddress)
            {
                //         HostAddress = SalamanderGeneral->DupStr();// FIXME!!
            }
            conRes = Connect(auxServerIP, auxServerPort, &err);
        }
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
        TRACE_E("Unexpected situation in CDataConnectionSocket::PassiveConnect() - not in passive mode.");
        if (error != NULL)
            *error = NO_ERROR; // neznama chyba
        return FALSE;
    }
}

void CDataConnectionBaseSocket::SetActive(int logUID)
{
    CALL_STACK_MESSAGE2("CDataConnectionBaseSocket::SetActive(%d)", logUID);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    UsePassiveMode = FALSE;
    LogUID = logUID;

    // pred dalsim pokusem o navazani spojeni nulujeme...
    ClearBeforeConnect();
    TransferSpeedMeter.Clear();
    if (CompressData)
        ComprTransferSpeedMeter.Clear();
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;

    LastActivityTime = GetTickCount(); // hresime na to, ze objekt se inicializuje pred poslanim transfer prikazu (prikaz bude mit kratsi nebo stejny timeout)
    if (GlobalLastActivityTime != NULL)
        GlobalLastActivityTime->Set(LastActivityTime);

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionBaseSocket::IsTransfering(BOOL* transferFinished)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::IsTransfering()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = IsConnected();
    if (transferFinished != NULL)
        *transferFinished = ReceivedConnected && !ret; // doslo ke spojeni, ale uz je zavrene? (hotovo)
    ret = ReceivedConnected && ret;                    // doslo ke spojeni a je stale otevrene? (transfering)
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

DWORD
CDataConnectionBaseSocket::GetLastActivityTime()
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::GetLastActivityTime()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    DWORD ret = LastActivityTime;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CDataConnectionBaseSocket::ActivateConnection()
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::ActivateConnection()");

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
CDataConnectionBaseSocket::GetSocketCloseTime()
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::GetSocketCloseTime()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    DWORD r = SocketCloseTime;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return r;
}

void CDataConnectionBaseSocket::DoPostMessageToWorker(int msgID)
{
#ifdef _DEBUG
    if (SocketCritSect.RecursionCount != 1)
        TRACE_E("Incorrect call to CDataConnectionBaseSocket::DoPostMessageToWorker(): must be one-time-entered in section SocketCritSect!");
#endif

    if (PostMessagesToWorker)
    {
        int msg = WorkerSocketMsg;
        int uid = WorkerSocketUID;
        int dataConUID = UID;
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        SocketsThread->PostSocketMessage(msg, uid, msgID, (void*)(INT_PTR)dataConUID);
        HANDLES(EnterCriticalSection(&SocketCritSect));
    }
}

int CDataConnectionBaseSocket::GetLogUID()
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::GetLogUID()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int ret = LogUID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CDataConnectionBaseSocket::SetGlobalTransferSpeedMeter(CTransferSpeedMeter* globalTransferSpeedMeter)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::SetGlobalTransferSpeedMeter()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    GlobalTransferSpeedMeter = globalTransferSpeedMeter;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionBaseSocket::OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
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

void CDataConnectionBaseSocket::ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                                       BOOL proxyError)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::ListeningForConnection()");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount != 0)
        TRACE_E("Incorrect call to CDataConnectionBaseSocket::ListeningForConnection(): may not be entered in section SocketCritSect!");
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
    DoPostMessageToWorker(WorkerMsgListeningForCon);
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionBaseSocket::GetListenIPAndPort(DWORD* listenOnIP, unsigned short* listenOnPort)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::GetListenIPAndPort()");

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

void CDataConnectionBaseSocket::LogNetEventLastError(BOOL canBeProxyError)
{
    CALL_STACK_MESSAGE1("CDataConnectionBaseSocket::LogNetEventLastError()");

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

//
// ****************************************************************************
// CDataConnectionSocket
//

CDataConnectionSocket::CDataConnectionSocket(BOOL flushData, CFTPProxyForDataCon* proxyServer,
                                             int encryptConnection, CCertificate* certificate,
                                             int compressData, CSocket* conForReuse)
    : CDataConnectionBaseSocket(proxyServer, encryptConnection, certificate, compressData, conForReuse)
{
    ReadBytes = NULL;
    ValidBytesInReadBytesBuf = 0;
    ReadBytesAllocatedSize = 0;
    ReadBytesLowMemory = FALSE;

    TotalReadBytesCount.Set(0, 0);

    TransferFinished = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual, nonsignaled
    if (TransferFinished == NULL)
        TRACE_E("Unable to create synchronization event object needed for proper closing of data connection.");

    WindowWithStatus = NULL;
    StatusMessage = 0;
    StatusMessageSent = TRUE;

    WorkerMsgFlushData = -1;

    FlushData = flushData;
    FlushBuffer = NULL;
    ValidBytesInFlushBuffer = 0;
    NeedFlushReadBuf = 0;
    FlushTimerAdded = FALSE;

    DecomprDataBuffer = NULL;
    DecomprDataAllocatedSize = 0;
    DecomprDataDelayedBytes = 0;
    AlreadyDecomprPartOfFlushBuffer = 0;

    DataTotalSize.Set(-1, -1);

    TgtDiskFileName[0] = 0;
    TgtDiskFile = NULL;
    TgtDiskFileCreated = FALSE;
    TgtFileLastError = NO_ERROR;
    TgtDiskFileSize.Set(0, 0);
    CurrentTransferMode = ctrmUnknown;
    AsciiTrModeForBinFileProblemOccured = FALSE;
    AsciiTrModeForBinFileHowToSolve = 0 /* ptat se uzivatele */;
    TgtDiskFileClosed = FALSE;
    TgtDiskFileCloseIndex = -1;
    DiskWorkIsUsed = FALSE;

    NoDataTransTimeout = FALSE;
    DecomprErrorOccured = FALSE;
    //DecomprMissingStreamEnd = FlushData && CompressData != 0;

    memset(&DiskWork, 0, sizeof(DiskWork));
}

CDataConnectionSocket::~CDataConnectionSocket()
{
    if (DiskWorkIsUsed)
        TRACE_E("CDataConnectionSocket::~CDataConnectionSocket(): DiskWorkIsUsed is TRUE!");
    if (TgtDiskFile != NULL)
        TRACE_E("CDataConnectionSocket::~CDataConnectionSocket(): TgtDiskFile is not NULL!");
    if (FlushData && (ValidBytesInFlushBuffer > 0 || ValidBytesInReadBytesBuf > 0 || DecomprDataDelayedBytes > 0))
        TRACE_E("CDataConnectionSocket::~CDataConnectionSocket(): closing data-connection without fully flushed data!");
    if (ReadBytes != NULL)
        free(ReadBytes);
    if (FlushBuffer != NULL)
        free(FlushBuffer);
    if (DecomprDataBuffer != NULL)
        free(DecomprDataBuffer);
    if (TransferFinished != NULL)
        HANDLES(CloseHandle(TransferFinished));
    if (CompressData)
        SalZLIB->InflateEnd(&ZLIBInfo);
}

char* CDataConnectionSocket::GiveData(int* length, BOOL* decomprErr)
{
    char* ret = NULL;
    *length = 0;
    *decomprErr = FALSE;
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (FlushData)
        TRACE_E("Incorrect call to CDataConnectionSocket::GiveData(): data are flushed (not collected in memory)!");
    else
    {
        if (ValidBytesInReadBytesBuf > 0)
        {
            if (CompressData)
            {
                CSalZLIB zi;
                size_t size = 2 * DATACON_FLUSHBUFFERSIZE; // Assume 50% compression ratio

                int ignErr = SalZLIB->InflateInit(&zi);
                if (ignErr < 0)
                    TRACE_E("SalZLIB->InflateInit returns unexpected error: " << ignErr);
                ret = (char*)malloc(size);
                zi.avail_in = ValidBytesInReadBytesBuf;
                zi.next_in = (BYTE*)ReadBytes;
                zi.next_out = (BYTE*)ret;
                zi.avail_out = (UINT)size;
                *length = 0;
                for (;;)
                {
                    BYTE* prev = zi.next_out;
                    int err = SalZLIB->Inflate(&zi, SAL_Z_NO_FLUSH);
                    *length += (int)(zi.next_out - prev);
                    if (err == SAL_Z_STREAM_END)
                    {
                        if (zi.avail_in > 0)
                            TRACE_E("CDataConnectionSocket::GiveData(): ignoring data (" << zi.avail_in << " bytes) received after end of compressed stream");
                        break;
                    }
                    if (err < 0) // chyba pri dekompresi: pocitame se SAL_Z_DATA_ERROR a SAL_Z_BUF_ERROR, ostatni jsou necekane; BTW, SAL_Z_DATA_ERROR by taky teoreticky nemela nikdy nastat, muselo by jit o interni chybu serveru (ze by data spatne zakomprimoval), TCP je zabezpeceny; SAL_Z_BUF_ERROR nastane pokud stream neni ukonceny (predcasne preruseny data-transfer)
                    {
                        if (err != SAL_Z_DATA_ERROR && (err != SAL_Z_BUF_ERROR || zi.avail_in != 0))
                            TRACE_E("CDataConnectionSocket::GiveData(): SalZLIB->Inflate returns unexpected error: " << err);
                        if (err != SAL_Z_BUF_ERROR || zi.avail_in != 0) // pri nekompletnich datech predpokladame, ze jinak je stream OK a dekomprimovana data jsou tez OK
                        {
                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDECOMPRERROR), -1, TRUE);
                            *decomprErr = TRUE;
                            *length = 0; // pri chybe dekomprimace muzou byt data uplne nesmyslna (nevyslo CRC), takze je radsi komplet zrusime
                        }
                        // else;  // chybu neukonceni streamu ignorujeme, neukoncuje napr. Serv-U 7 a 8 (6 ho jeste ukoncuje); asi zadna traga, protoze se kontroluje server reply + uspesne zavreni TCP spojeni (prenos vsech dat ze serveru by tedy mel byt zajisten)
                        break;
                    }
                    if (zi.avail_out == 0)
                    {
                        zi.avail_out = DATACON_FLUSHBUFFERSIZE;
                        size += zi.avail_out;
                        ret = (char*)realloc(ret, size);
                        zi.next_out = (BYTE*)ret + *length;
                    }
                }
                // zkusime buffer zmensit, aby zbytecne nezabiral pamet
                // NOTE: realloc(x, 0) frees x and returns NULL!
                ret = (char*)realloc(ret, max(1, *length));
                ignErr = SalZLIB->InflateEnd(&zi);
                if (ignErr < 0)
                    TRACE_E("SalZLIB->InflateEnd returns unexpected error: " << ignErr);
                free(ReadBytes);
            }
            else
            {
                if (ValidBytesInReadBytesBuf < ReadBytesAllocatedSize) // zkusime buffer zmensit, aby zbytecne nezabiral pamet
                    ret = (char*)realloc(ReadBytes, ValidBytesInReadBytesBuf);
                if (ret == NULL)
                    ret = ReadBytes;
                *length = ValidBytesInReadBytesBuf;
            }
            ReadBytes = NULL;
            ValidBytesInReadBytesBuf = 0;
            ReadBytesAllocatedSize = 0;
            TotalReadBytesCount.Set(0, 0);
        }
        else // vracime prazdny buffer
        {
            ret = (char*)malloc(1); // pokud se nepodari alokace, mame vracet NULL, hotovo...
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CDataConnectionSocket::ClearBeforeConnect()
{
    ValidBytesInReadBytesBuf = 0;
    TotalReadBytesCount.Set(0, 0);
    ValidBytesInFlushBuffer = 0;
    if (DecomprDataBuffer == NULL)
        DecomprDataAllocatedSize = 0;
    DecomprDataDelayedBytes = 0;
    AlreadyDecomprPartOfFlushBuffer = 0;
    NeedFlushReadBuf = 0;
    FlushTimerAdded = FALSE;
    TgtFileLastError = NO_ERROR;
    ReadBytesLowMemory = FALSE;
    ResetEvent(TransferFinished); // TransferFinished nemuze byt NULL (IsGood() by vratila FALSE)
    StatusHasChanged();
    AsciiTrModeForBinFileProblemOccured = FALSE;
    DataTotalSize.Set(-1, -1);
    TgtDiskFileClosed = FALSE;
    TgtDiskFileCloseIndex = -1;
    TgtDiskFileSize.Set(0, 0);
    TgtDiskFileCreated = FALSE;
    if (CompressData)
    {
        // First get rid of leftovers, if any. NOTE: ctor memsets ZLIBInfo to 0
        SalZLIB->InflateEnd(&ZLIBInfo);
        // Then initialize ZLIBInfo
        int ignErr = SalZLIB->InflateInit(&ZLIBInfo);
        if (ignErr < 0)
            TRACE_E("SalZLIB->InflateInit returns unexpected error: " << ignErr);
    }
    NoDataTransTimeout = FALSE;
    DecomprErrorOccured = FALSE;
    //DecomprMissingStreamEnd = FlushData && CompressData != 0;
}

void CDataConnectionBaseSocket::EncryptPassiveDataCon()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int err;
    if (UsePassiveMode && EncryptConnection &&
        !EncryptSocket(LogUID, &err, NULL, NULL, NULL, 0, SSLConForReuse))
    {
        SSLErrorOccured = err;
        if (Socket != INVALID_SOCKET) // always true: socket je pripojeny
            CloseSocketEx(NULL);      // zavreme "data connection", dale nema smysl drzet
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr, BOOL* noDataTransTimeout,
                                     int* sslErrorOccured, BOOL* decomprErrorOccured)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::GetError()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (netErr != NULL)
        *netErr = NetEventLastError;
    if (lowMem != NULL)
        *lowMem = ReadBytesLowMemory;
    if (tgtFileErr != NULL)
        *tgtFileErr = TgtFileLastError;
    if (noDataTransTimeout != NULL)
        *noDataTransTimeout = NoDataTransTimeout;
    if (sslErrorOccured != NULL)
        *sslErrorOccured = SSLErrorOccured;
    if (decomprErrorOccured != NULL)
        *decomprErrorOccured = DecomprErrorOccured;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

/*
BOOL
CDataConnectionSocket::GetDecomprMissingStreamEnd()
{
  CALL_STACK_MESSAGE1("CDataConnectionSocket::GetDecomprMissingStreamEnd()");

  HANDLES(EnterCriticalSection(&SocketCritSect));
  BOOL ret = DecomprMissingStreamEnd;
  HANDLES(LeaveCriticalSection(&SocketCritSect));
  return ret;
}
*/

BOOL CDataConnectionSocket::CloseSocketEx(DWORD* error)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::CloseSocketEx()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

    SocketCloseTime = GetTickCount();
    if (TgtDiskFileName[0] == 0 ||                                     // pokud nejde o primy flush dat do souboru
        ValidBytesInFlushBuffer == 0 && ValidBytesInReadBytesBuf == 0) // nebo jsou vsechna data jiz flushnuta
    {
        SetEvent(TransferFinished); // TransferFinished nemuze byt NULL (IsGood() by vratila FALSE)
    }

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return CSocket::CloseSocket(error);
}

void CDataConnectionSocket::DirectFlushData()
{
#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0)
        TRACE_E("Incorrect call to CDataConnectionSocket::DirectFlushData(): must be entered in section SocketCritSect!");
#endif

    if (TgtDiskFileName[0] != 0) // jen pri pouziti primeho flushe dat do souboru z data-connectiony
    {
        if (TgtDiskFileClosed)
            TRACE_E("Unexpected situation in CDataConnectionSocket::DirectFlushData(): TgtDiskFileClosed is TRUE!");
        else
        {
            char* flushBuffer;
            int validBytesInFlushBuffer;
            BOOL deleteTgtFile;
            BOOL haveFlushData = GiveFlushData(&flushBuffer, &validBytesInFlushBuffer, &deleteTgtFile);

            if (deleteTgtFile) // potrebujeme smazat cilovy soubor, protoze mozna obsahuje poskozena data
            {
                if (TgtDiskFile != NULL)
                {
                    FTPDiskThread->AddFileToClose("", TgtDiskFileName, TgtDiskFile, FALSE, FALSE, NULL, NULL,
                                                  TRUE, NULL, &TgtDiskFileCloseIndex);
                    TgtDiskFile = NULL; // sice je zavirani+mazani teprve naplanovane, ale my uz se souborem pracovat nebudeme
                    TgtDiskFileCreated = FALSE;
                    TgtDiskFileSize.Set(0, 0);
                }
                TgtDiskFileClosed = TRUE;
            }
            else
            {
                if (haveFlushData) // mame 'flushBuffer', musime jej predat do disk-threadu (pri chybe ho uvolnime)
                {
                    if (!AsciiTrModeForBinFileProblemOccured && CurrentTransferMode == ctrmASCII &&
                        !SalamanderGeneral->IsANSIText(flushBuffer, validBytesInFlushBuffer))
                    {
                        AsciiTrModeForBinFileProblemOccured = TRUE; // detekovana chyba "ascii rezim pro binarni soubor"
                        StatusHasChanged();                         // at se to list-wait-wnd dozvi co nejdrive
                    }
                    if (DiskWorkIsUsed)
                        TRACE_E("Unexpected situation in CDataConnectionSocket::DirectFlushData(): DiskWorkIsUsed may not be TRUE here!");

                    DiskWork.SocketMsg = Msg;
                    DiskWork.SocketUID = UID;
                    DiskWork.MsgID = DATACON_DISKWORKWRITEFINISHED;
                    DiskWork.Type = fdwtCreateAndWriteFile;
                    lstrcpyn(DiskWork.Name, TgtDiskFileName, MAX_PATH);
                    DiskWork.WinError = NO_ERROR;
                    DiskWork.State = sqisNone;
                    if (DiskWork.OpenedFile != NULL)
                        TRACE_E("CDataConnectionSocket::DirectFlushData(): DiskWork.OpenedFile is not NULL!");
                    DiskWork.OpenedFile = NULL;
                    if (DiskWork.FlushDataBuffer != NULL)
                        TRACE_E("CDataConnectionSocket::DirectFlushData(): DiskWork.FlushDataBuffer must be NULL!");
                    DiskWork.FlushDataBuffer = flushBuffer;
                    DiskWork.ValidBytesInFlushDataBuffer = validBytesInFlushBuffer;
                    DiskWork.WorkFile = TgtDiskFile;
                    if (FTPDiskThread->AddWork(&DiskWork))
                        DiskWorkIsUsed = TRUE;
                    else // nelze flushnout data, nelze pokracovat v downloadu
                    {
                        if (DiskWork.FlushDataBuffer != NULL)
                        {
                            free(DiskWork.FlushDataBuffer);
                            DiskWork.FlushDataBuffer = NULL;
                        }

                        ReadBytesLowMemory = TRUE; // koncime pro nedostatek pameti
                        CloseSocketEx(NULL);       // zavreme "data connection", dale nema smysl pokracovat
                        FreeFlushData();
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        // DoPostMessageToWorker(WorkerMsgConnectionClosed);  // tohle by tady nemelo byt potreba
                    }
                }
                else
                    TRACE_E("Unexpected situation in CDataConnectionSocket::DirectFlushData(): data-connection has nothing to flush!"); // snad nemozne
            }
        }
    }
}

void CDataConnectionSocket::CancelConnectionAndFlushing()
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::CancelConnectionAndFlushing()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    CloseSocketEx(NULL);
    if (TgtDiskFileName[0] != 0) // jen pri pouziti primeho flushe dat do souboru z data-connectiony
    {
        CloseTgtFile();
        FreeFlushData();
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionSocket::IsFlushingDataToDisk()
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::IsFlushingDataToDisk()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = FALSE;
    if (TgtDiskFileName[0] != 0)                                             // jen pri pouziti primeho flushe dat do souboru z data-connectiony
        ret = ValidBytesInFlushBuffer != 0 || ValidBytesInReadBytesBuf != 0; // probiha flush dat? (aneb: je co flushnout?)
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

BOOL CDataConnectionSocket::IsAsciiTrForBinFileProblem(int* howToSolve)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::IsAsciiTrForBinFileProblem()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = AsciiTrModeForBinFileProblemOccured;
    if (ret && AsciiTrModeForBinFileHowToSolve != 3)
        *howToSolve = AsciiTrModeForBinFileHowToSolve;
    else
    {
        *howToSolve = 0;
        ret = FALSE;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CDataConnectionSocket::SetAsciiTrModeForBinFileHowToSolve(int howToSolve)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::SetAsciiTrModeForBinFileHowToSolve()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    AsciiTrModeForBinFileHowToSolve = howToSolve;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::ReceivePostMessage(DWORD id, void* param)
{
    CALL_STACK_MESSAGE2("CDataConnectionSocket::ReceivePostMessage(%u,)", id);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    switch (id)
    {
    case DATACON_DISKWORKWRITEFINISHED:
    {
        if (!TgtDiskFileClosed)
        {
            if (!DiskWorkIsUsed)
                TRACE_E("CDataConnectionSocket::ReceivePostMessage(): DATACON_DISKWORKWRITEFINISHED: DiskWorkIsUsed is not TRUE!");
            DiskWorkIsUsed = FALSE;

            if (DiskWork.State == sqisNone) // flush dat se podaril
            {
                TgtDiskFileCreated = TRUE;

                // napocitame novou velikost souboru
                TgtDiskFileSize += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0);

                // pokud se vytvarel soubor, ziskame zde jeho handle
                if (TgtDiskFile == NULL)
                    TgtDiskFile = DiskWork.OpenedFile;
                DiskWork.OpenedFile = NULL;

                // pokud data-connection existuje, vratime buffer pro dalsi pouziti
                char* flushData = DiskWork.FlushDataBuffer;
                DiskWork.FlushDataBuffer = NULL;
                FlushDataFinished(flushData, FALSE); // POZOR: pouzivame vyjimku pro volani z kriticke sekce + zde muze dojit k dalsimu vyuziti DiskWork!!!
            }
            else // nastala chyba
            {
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }

                TgtFileLastError = DiskWork.WinError; // nastavime chybu pri vytvareni/zapisu do ciloveho souboru
                CloseSocketEx(NULL);                  // shutdown (nedozvime se o vysledku)
                FreeFlushData();
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                // DoPostMessageToWorker(WorkerMsgConnectionClosed);  // tohle by tady nemelo byt potreba
            }

            if (!IsConnected() && !DiskWorkIsUsed) // data-connection je zavrena + vsechna data flushnuta -> koncime
                SetEvent(TransferFinished);        // TransferFinished nemuze byt NULL (IsGood() by vratila FALSE)
        }
        break;
    }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::JustConnected()
{
    ReceivedConnected = TRUE; // pokud prijde FD_READ pred FD_CONNECT (kazdopadne musi byt connected)
    TransferSpeedMeter.JustConnected();
    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    DoPostMessageToWorker(WorkerMsgConnectedToServer);
    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_TESTNODATATRTIMEOUT,
                            DATACON_TESTNODATATRTIMERID, NULL);
}

void CDataConnectionSocket::ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::ConnectionAccepted()");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount != 1)
        TRACE_E("Incorrect call to CDataConnectionSocket::ConnectionAccepted(): must be one-time-entered in section SocketCritSect!");
#endif

    if (success && EncryptConnection)
    {
        int err;
        if (!EncryptSocket(LogUID, &err, NULL, NULL, NULL, 0, SSLConForReuse))
        {
            SSLErrorOccured = err;
            if (Socket != INVALID_SOCKET) // always true: socket je pripojeny
                CloseSocketEx(NULL);      // zavreme "data connection", dale nema smysl drzet
            success = FALSE;
        }
    }
    if (success)
    {
        LastActivityTime = GetTickCount(); // doslo k uspesnemu acceptu
        if (GlobalLastActivityTime != NULL)
            GlobalLastActivityTime->Set(LastActivityTime);
        NetEventLastError = NO_ERROR; // v predchozim acceptu mohlo dojit k chybe, ted uz neni aktualni
        SSLErrorOccured = SSLCONERR_NOERROR;
        StatusHasChanged();
        JustConnected();
    }
    else
    {
        NetEventLastError = winError;
        if (proxyError && NetEventLastError == NO_ERROR)
            NetEventLastError = ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */;
        LogNetEventLastError(proxyError);
    }
}

void CDataConnectionSocket::MoveReadBytesToFlushBuffer()
{
    char* flushBuf = FlushBuffer;
    ValidBytesInFlushBuffer = ValidBytesInReadBytesBuf;
    AlreadyDecomprPartOfFlushBuffer = 0;
    FlushBuffer = ReadBytes;
    ReadBytes = flushBuf;
    if (ReadBytes == NULL)
        ReadBytesAllocatedSize = 0;
    ValidBytesInReadBytesBuf = 0;
}

void CDataConnectionSocket::ReceiveNetEvent(LPARAM lParam, int index)
{
    SLOW_CALL_STACK_MESSAGE3("CDataConnectionSocket::ReceiveNetEvent(0x%IX, %d)", lParam, index);
    DWORD eventError = WSAGETSELECTERROR(lParam); // extract error code of event
    BOOL logLastErr = FALSE;
    switch (WSAGETSELECTEVENT(lParam)) // extract event
    {
    case FD_CLOSE: // nekdy chodi pred poslednim FD_READ, nezbyva tedy nez napred zkusit FD_READ a pokud uspeje, poslat si FD_CLOSE znovu (muze pred nim znovu uspet FD_READ)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE;     // TRUE = prisel FD_CLOSE + bylo co cist (provedl se jako FD_READ) => posleme si znovu FD_CLOSE (soucasny FD_CLOSE byl plany poplach)
        BOOL skipSendingOfFDClose = FALSE; // TRUE = prisel FD_CLOSE + cekame na flushnuti dat, takze ho zpozdime (ted ho nezpracujeme)
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (ReceivedConnected || UsePassiveMode) // ignorujeme zavreni "listen" socketu
        {
            if (eventError == NO_ERROR) // jen pokud nenastala chyba (podle helpu hrozi jen WSAENETDOWN)
            {
                if (UsePassiveMode) // u aktivniho spojeni musime cekat na FD_ACCEPT (tenhle socket je "listen", a pak az "data connection")
                {
                    if (!ReceivedConnected)
                        JustConnected();
                    if (EncryptConnection && SSLConn == NULL)
                        EncryptPassiveDataCon();
                }

                if (WorkerPaused)
                {
                    if (WSAGETSELECTEVENT(lParam) == FD_READ)
                    {
                        if (DataTransferPostponed == 0)
                            DataTransferPostponed = 1 /* FD_READ */; // nelze zapomenout FD_CLOSE na ukor FD_READ
                    }
                    else
                    {
                        DataTransferPostponed = 2 /* FD_CLOSE */;
                        skipSendingOfFDClose = TRUE;
                    }
                }
                else
                {
                    if (Socket != INVALID_SOCKET) // socket je pripojeny
                    {
                        BOOL lowMem = ReadBytesLowMemory;
                        if (FlushData) // data se predavaji pro overeni/zapis na disk pres FlushBuffer (velikost bufferu DATACON_FLUSHBUFFERSIZE)
                        {
                            if (!lowMem && ReadBytesAllocatedSize < DATACON_FLUSHBUFFERSIZE) // nealokovany buffer 'ReadBytes'
                            {
                                if (ReadBytes != NULL)
                                {
                                    TRACE_E("Unexpected situation in CDataConnectionSocket::ReceiveNetEvent(): ReadBytes is not NULL, but ReadBytesAllocatedSize < DATACON_FLUSHBUFFERSIZE");
                                    free(ReadBytes);
                                }
                                ReadBytes = (char*)malloc(DATACON_FLUSHBUFFERSIZE);
                                if (ReadBytes != NULL)
                                    ReadBytesAllocatedSize = DATACON_FLUSHBUFFERSIZE;
                                else // nedostatek pameti pro ulozeni dat v nasem bufferu (chybu hlasi jen TRACE)
                                {
                                    TRACE_E(LOW_MEMORY);
                                    lowMem = TRUE;
                                }
                            }

                            if (!lowMem)
                            {
                                if (ReadBytesAllocatedSize - ValidBytesInReadBytesBuf > 0)
                                {
                                    // precteme co nejvice bytu do bufferu, necteme cyklicky, aby se data prijimala postupne;
                                    // je-li jeste neco ke cteni, dostaneme znovu FD_READ
                                    int len;
                                    if (!SSLConn)
                                        len = recv(Socket, ReadBytes + ValidBytesInReadBytesBuf,
                                                   ReadBytesAllocatedSize - ValidBytesInReadBytesBuf, 0);
                                    else
                                    {
                                        if (SSLLib.SSL_pending(SSLConn) > 0) // je-li neprazdny interni SSL buffer nedojde vubec k volani recv() a tudiz neprijde dalsi FD_READ, tedy musime si ho poslat sami, jinak se prenos dat zastavi
                                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                                        len = SSLLib.SSL_read(SSLConn, ReadBytes + ValidBytesInReadBytesBuf,
                                                              ReadBytesAllocatedSize - ValidBytesInReadBytesBuf);
                                    }
                                    if (len >= 0) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                                    {
                                        if (len > 0)
                                        {
                                            ValidBytesInReadBytesBuf += len;          // upravime pocet jiz nactenych bytu o nove nactene
                                            TotalReadBytesCount += CQuadWord(len, 0); // upravime celkovy pocet jiz nactenych bytu o nove nactene
                                            LastActivityTime = GetTickCount();        // doslo k uspesnemu nacteni bytu ze socketu
                                            if (GlobalLastActivityTime != NULL)
                                                GlobalLastActivityTime->Set(LastActivityTime);
                                            TransferSpeedMeter.BytesReceived(len, LastActivityTime);
                                            if (GlobalTransferSpeedMeter != NULL)
                                                GlobalTransferSpeedMeter->BytesReceived(len, LastActivityTime);
                                            StatusHasChanged();

                                            if (ReadBytesAllocatedSize - ValidBytesInReadBytesBuf == 0)
                                            { // dalsi data uz neni kam cist, je potreba flushnout buffer
                                                if (NeedFlushReadBuf != 0 &&
                                                    (ValidBytesInFlushBuffer == 0 || NeedFlushReadBuf != 1 /* timer flush */))
                                                {
                                                    TRACE_E("CDataConnectionSocket::ReceiveNetEvent(): Unexpected value of NeedFlushReadBuf: " << NeedFlushReadBuf);
                                                }
                                                if (ValidBytesInFlushBuffer == 0) // spustime flushnuti dat z bufferu (buffery se zameni a bude zase misto pro cteni)
                                                {
                                                    // pokud byl zalozen timer pro flushnuti dat s timeoutem DATACON_FLUSHTIMEOUT, tak ho smazeme
                                                    if (FlushTimerAdded)
                                                    {
                                                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                                        SocketsThread->DeleteTimer(UID, DATACON_FLUSHTIMERID);
                                                        FlushTimerAdded = FALSE;
                                                    }

                                                    MoveReadBytesToFlushBuffer();

                                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                                    DoPostMessageToWorker(WorkerMsgFlushData);

                                                    // pokud jde o primy flush dat do souboru z data-connectiony, provede se zde
                                                    DirectFlushData();

                                                    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                                        sendFDCloseAgain = TRUE;
                                                }
                                                else // pockame az dobehne flushnuti dat, pak se buffery prohodi a zase bude misto
                                                {
                                                    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                                    {
                                                        skipSendingOfFDClose = TRUE;
                                                        NeedFlushReadBuf = 3; // flush + FD_CLOSE
                                                    }
                                                    else
                                                        NeedFlushReadBuf = 2; // flush + FD_READ
                                                }
                                            }
                                            else
                                            {
                                                if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                                    sendFDCloseAgain = TRUE;

                                                // pokud jeste nebyl zalozen timer pro flushnuti dat s timeoutem DATACON_FLUSHTIMEOUT,
                                                // a zaroven jeste neni zaznamenana potreba flushnout buffer (NeedFlushReadBuf == 0),
                                                // tak zalozime timer pro flushnuti dat
                                                if (!FlushTimerAdded && NeedFlushReadBuf == 0)
                                                {
                                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                                    FlushTimerAdded = SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_FLUSHTIMEOUT,
                                                                                              DATACON_FLUSHTIMERID, NULL);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            if (SSLConn && (WSAGETSELECTEVENT(lParam) == FD_READ) && (6 /*SSL_ERROR_ZERO_RETURN*/ == SSLLib.SSL_get_error(SSLConn, 0)))
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
                                        if (err != WSAEWOULDBLOCK)
                                        {
                                            NetEventLastError = err; // nastala chyba
                                            logLastErr = TRUE;
                                            CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                            DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                        }
                                    }
                                }
                                else // data uz neni kam cist, musime pockat na dokonceni flushnuti bufferu (pak se buffery zameni a bude zase misto pro cteni)
                                {
                                    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                    {
                                        skipSendingOfFDClose = TRUE;
                                        NeedFlushReadBuf = 3; // flush + FD_CLOSE (muze prepsat FD_READ)
                                    }
                                    else // WSAGETSELECTEVENT(lParam) == FD_READ
                                    {
                                        if (NeedFlushReadBuf != 3 /* flush + FD_CLOSE */)
                                            NeedFlushReadBuf = 2; // flush + FD_READ (nemuze prepsat FD_CLOSE)
                                    }
                                }
                            }
                            else
                            {
                                ReadBytesLowMemory = TRUE; // koncime cteni pro nedostatek pameti
                                CloseSocketEx(NULL);       // zavreme "data connection", dale nema smysl drzet
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                DoPostMessageToWorker(WorkerMsgConnectionClosed);
                            }
                        }
                        else // vsechna data se ukladaji do bufferu ReadBytes
                        {
                            if (!lowMem &&
                                ReadBytesAllocatedSize - ValidBytesInReadBytesBuf < DATACON_BYTESTOREADONSOCKET) // maly buffer 'ReadBytes'
                            {
                                int newSize = ValidBytesInReadBytesBuf + DATACON_BYTESTOREADONSOCKET +
                                              DATACON_BYTESTOREADONSOCKETPREALLOC;
                                char* newBuf = (char*)realloc(ReadBytes, newSize);
                                if (newBuf != NULL)
                                {
                                    ReadBytes = newBuf;
                                    ReadBytesAllocatedSize = newSize;
                                }
                                else // nedostatek pameti pro ulozeni dat v nasem bufferu (chybu hlasi jen TRACE)
                                {
                                    TRACE_E(LOW_MEMORY);
                                    lowMem = TRUE;
                                }
                            }

                            if (!lowMem)
                            { // precteme co nejvice bytu do bufferu, necteme cyklicky, aby se data prijimala postupne;
                                // je-li jeste neco ke cteni, dostaneme znovu FD_READ
                                int len;

                                if (!SSLConn)
                                    len = recv(Socket, ReadBytes + ValidBytesInReadBytesBuf,
                                               ReadBytesAllocatedSize - ValidBytesInReadBytesBuf, 0);
                                else
                                {
                                    if (SSLLib.SSL_pending(SSLConn) > 0) // je-li neprazdny interni SSL buffer nedojde vubec k volani recv() a tudiz neprijde dalsi FD_READ, tedy musime si ho poslat sami, jinak se prenos dat zastavi
                                        PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                                    len = SSLLib.SSL_read(SSLConn, ReadBytes + ValidBytesInReadBytesBuf,
                                                          ReadBytesAllocatedSize - ValidBytesInReadBytesBuf);
                                }
                                if (len >= 0) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                                {
                                    if (len > 0)
                                    {
                                        ValidBytesInReadBytesBuf += len;          // upravime pocet jiz nactenych bytu o nove nactene
                                        TotalReadBytesCount += CQuadWord(len, 0); // upravime celkovy pocet jiz nactenych bytu o nove nactene
                                        LastActivityTime = GetTickCount();        // doslo k uspesnemu nacteni bytu ze socketu
                                        if (GlobalLastActivityTime != NULL)
                                            GlobalLastActivityTime->Set(LastActivityTime);
                                        TransferSpeedMeter.BytesReceived(len, LastActivityTime);
                                        if (GlobalTransferSpeedMeter != NULL)
                                            GlobalTransferSpeedMeter->BytesReceived(len, LastActivityTime);
                                        StatusHasChanged();
                                        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                            sendFDCloseAgain = TRUE;
                                    }
                                    else
                                    {
                                        if (SSLConn && (WSAGETSELECTEVENT(lParam) == FD_READ) && (6 /*SSL_ERROR_ZERO_RETURN*/ == SSLLib.SSL_get_error(SSLConn, 0)))
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
                                    if (err != WSAEWOULDBLOCK)
                                    {
                                        NetEventLastError = err; // nastala chyba
                                        logLastErr = TRUE;
                                        CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                        DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                    }
                                }
                            }
                            else
                            {
                                ReadBytesLowMemory = TRUE; // koncime cteni pro nedostatek pameti
                                CloseSocketEx(NULL);       // zavreme "data connection", dale nema smysl drzet
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                DoPostMessageToWorker(WorkerMsgConnectionClosed);
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
            }
            else
            {
                if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // chybu si FD_CLOSE zpracuje po svem
                {
                    NetEventLastError = eventError;
                    logLastErr = TRUE;
                    CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl drzet
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    DoPostMessageToWorker(WorkerMsgConnectionClosed);
                }
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        // ted zpracujeme FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE && !skipSendingOfFDClose)
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
            if (!ReceivedConnected)
                JustConnected();
            LastActivityTime = GetTickCount(); // doslo k uspesnemu connectu
            if (GlobalLastActivityTime != NULL)
                GlobalLastActivityTime->Set(LastActivityTime);
            StatusHasChanged();
        }
        else
        {
            NetEventLastError = eventError;
            logLastErr = TRUE;
            CloseSocketEx(NULL); // zavreme socket "data connection", uz se nemuze otevrit
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

void CDataConnectionSocket::SocketWasClosed(DWORD error)
{
    CALL_STACK_MESSAGE2("CDataConnectionSocket::SocketWasClosed(%u)", error);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    SocketCloseTime = GetTickCount();
    if (TgtDiskFileName[0] == 0 ||                                     // pokud nejde o primy flush dat do souboru
        ValidBytesInFlushBuffer == 0 && ValidBytesInReadBytesBuf == 0) // nebo jsou vsechna data jiz flushnuta
    {
        SetEvent(TransferFinished); // TransferFinished nemuze byt NULL (IsGood() by vratila FALSE)
    }
    if (error != NO_ERROR)
        NetEventLastError = error;

    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    DoPostMessageToWorker(WorkerMsgConnectionClosed);

    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    SocketsThread->DeleteTimer(UID, DATACON_TESTNODATATRTIMERID);

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::GetStatus(CQuadWord* downloaded, CQuadWord* total,
                                      DWORD* connectionIdleTime, DWORD* speed)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::GetStatus(,,,)");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    *downloaded = TotalReadBytesCount;
    *total = DataTotalSize;
    if (*total < *downloaded)
        *total = *downloaded;
    *connectionIdleTime = (GetTickCount() - LastActivityTime) / 1000;
    *speed = TransferSpeedMeter.GetSpeed(NULL);
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::SetWindowWithStatus(HWND hwnd, UINT msg)
{
    CALL_STACK_MESSAGE2("CDataConnectionSocket::SetWindowWithStatus(, %u)", msg);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    WindowWithStatus = hwnd;
    StatusMessage = msg;
    StatusMessageSent = (WindowWithStatus == NULL); // nic se nema posilat
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::StatusMessageReceived()
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::StatusMessageReceived()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    StatusMessageSent = FALSE;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::SetPostMessagesToWorker(BOOL post, int msg, int uid,
                                                    DWORD msgIDConnected, DWORD msgIDConClosed,
                                                    DWORD msgIDFlushData, DWORD msgIDListeningForCon)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::SetPostMessagesToWorker()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    PostMessagesToWorker = post;
    WorkerSocketMsg = msg;
    WorkerSocketUID = uid;
    WorkerMsgConnectedToServer = msgIDConnected;
    WorkerMsgConnectionClosed = msgIDConClosed;
    WorkerMsgFlushData = msgIDFlushData;
    WorkerMsgListeningForCon = msgIDListeningForCon;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::SetDirectFlushParams(const char* tgtFileName, CCurrentTransferMode currentTransferMode)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::SetDirectFlushParams()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    lstrcpyn(TgtDiskFileName, tgtFileName, MAX_PATH);
    CurrentTransferMode = currentTransferMode;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::GetTgtFileState(BOOL* fileCreated, CQuadWord* fileSize)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::GetTgtFileState()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    *fileCreated = TgtDiskFileCreated;
    *fileSize = TgtDiskFileSize;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::CloseTgtFile()
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::CloseTgtFile()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (TgtDiskFileName[0] != 0)
    {
        if (DiskWorkIsUsed) // je-li rozjete nejake flushovani na disk, zrusime jej
        {
            BOOL workIsInProgress;
            if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
            {
                if (workIsInProgress)
                    DiskWork.FlushDataBuffer = NULL; // prace je rozdelana, nemuzeme uvolnit buffer se zapisovanymi/testovanymi daty, nechame to na disk-work threadu (viz cast cancelovani prace) - do DiskWork muzeme zapisovat, protoze po Cancelu do nej uz disk-thread nesmi pristupovat (napr. uz vubec nemusi existovat)
                else
                { // prace byla zcanclovana pred tim, nez ji disk-thread zacal provadet - provedeme dealokaci flush bufferu
                    if (DiskWork.FlushDataBuffer != NULL)
                    {
                        free(DiskWork.FlushDataBuffer);
                        DiskWork.FlushDataBuffer = NULL;
                    }
                }
            }
            else // prace uz je hotova, uz je pry na ceste DATACON_DISKWORKWRITEFINISHED (jeste nedorazil)
            {
                if (DiskWork.FlushDataBuffer != NULL) // flush buffer uz je na nic
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }
                if (DiskWork.State == sqisNone) // flush dat se podaril
                {
                    TgtDiskFileCreated = TRUE;

                    // napocitame novou velikost souboru
                    TgtDiskFileSize += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0);

                    // pokud se vytvarel soubor, ziskame zde jeho handle
                    if (TgtDiskFile == NULL)
                        TgtDiskFile = DiskWork.OpenedFile;
                    DiskWork.OpenedFile = NULL;
                }
                else // nastala chyba
                {
                    TgtFileLastError = DiskWork.WinError; // nastavime chybu pri vytvareni/zapisu do ciloveho souboru
                    CloseSocketEx(NULL);                  // shutdown (nedozvime se o vysledku)
                    FreeFlushData();
                    // POZOR: nejsme v sekci CSocketsThread::CritSect, takze toto volani neni mozne:
                    // DoPostMessageToWorker(WorkerMsgConnectionClosed);  // tohle by tady nemelo byt potreba
                }
            }
            DiskWorkIsUsed = FALSE;
        }
        if (TgtDiskFile != NULL)
        {
            FTPDiskThread->AddFileToClose("", TgtDiskFileName, TgtDiskFile, FALSE, FALSE, NULL, NULL,
                                          FALSE, NULL, &TgtDiskFileCloseIndex);
            TgtDiskFile = NULL; // sice je zavirani teprve naplanovane, ale my uz se souborem pracovat nebudeme
        }
        TgtDiskFileClosed = TRUE;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionSocket::WaitForFileClose(DWORD timeout)
{
    CALL_STACK_MESSAGE2("CDataConnectionSocket::WaitForFileClose(%u)", timeout);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int tgtDiskFileCloseIndex = TgtDiskFileCloseIndex;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (tgtDiskFileCloseIndex != -1)
        return FTPDiskThread->WaitForFileClose(tgtDiskFileCloseIndex, timeout);
    return FALSE; // soubor zrejme vubec nebyl otevreny (nebo je zavreni+delete na disk-work threadu - cancel behem prvni create+flush disk-prace)
}

void CDataConnectionSocket::SetDataTotalSize(CQuadWord const& size)
{
    CALL_STACK_MESSAGE2("CDataConnectionSocket::SetDataTotalSize(%f)", size.GetDouble());

    HANDLES(EnterCriticalSection(&SocketCritSect));
    DataTotalSize = size;
    StatusHasChanged();
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::UpdatePauseStatus(BOOL pause)
{
    CALL_STACK_MESSAGE2("CDataConnectionSocket::UpdatePauseStatus(%d)", pause);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (WorkerPaused != pause)
    {
        WorkerPaused = pause;
        if (WorkerPaused && DataTransferPostponed != 0)
            TRACE_E("Unexpected situation in CDataConnectionSocket::UpdatePauseStatus(): DataTransferPostponed=" << DataTransferPostponed);
        if (!WorkerPaused)
        {
            LastActivityTime = GetTickCount();
            if (GlobalLastActivityTime != NULL)
                GlobalLastActivityTime->Set(LastActivityTime);
            TransferSpeedMeter.Clear();
            TransferSpeedMeter.JustConnected();
            if (DataTransferPostponed != 0)
            {
                PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, DataTransferPostponed == 1 ? FD_READ : FD_CLOSE);
                DataTransferPostponed = 0;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::StatusHasChanged()
{
    SLOW_CALL_STACK_MESSAGE1("CDataConnectionSocket::StatusHasChanged()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (!StatusMessageSent && WindowWithStatus != NULL)
    {
        PostMessage(WindowWithStatus, StatusMessage, 0, 0);
        StatusMessageSent = TRUE;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionSocket::GiveFlushData(char** flushBuffer, int* validBytesInFlushBuffer, BOOL* deleteTgtFile)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::GiveFlushData(,)");

    BOOL ret = FALSE;
    *deleteTgtFile = FALSE;
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (ValidBytesInFlushBuffer > 0 &&
        (!CompressData || AlreadyDecomprPartOfFlushBuffer < ValidBytesInFlushBuffer) &&
        FlushBuffer != NULL &&
        (!CompressData || DecomprDataAllocatedSize == 0 || DecomprDataBuffer != NULL))
    {
        if (CompressData)
        {
            if (DecomprDataAllocatedSize == 0)
            {
                DecomprDataAllocatedSize = DATACON_FLUSHBUFFERSIZE;
                if (DecomprDataBuffer != NULL)
                {
                    TRACE_E("Unexpected situation in CDataConnectionSocket::GiveFlushData(): DecomprDataBuffer is not NULL, but DecomprDataAllocatedSize is 0");
                    free(DecomprDataBuffer);
                }
                DecomprDataBuffer = (char*)malloc(DecomprDataAllocatedSize);
            }

            ZLIBInfo.avail_in = ValidBytesInFlushBuffer - AlreadyDecomprPartOfFlushBuffer;
            ZLIBInfo.next_in = (BYTE*)FlushBuffer + AlreadyDecomprPartOfFlushBuffer;
            ZLIBInfo.next_out = (BYTE*)DecomprDataBuffer + DecomprDataDelayedBytes;
            ZLIBInfo.avail_out = DecomprDataAllocatedSize - DecomprDataDelayedBytes;

            int err = SalZLIB->Inflate(&ZLIBInfo, SAL_Z_NO_FLUSH);

            *validBytesInFlushBuffer = (int)(ZLIBInfo.next_out - (BYTE*)DecomprDataBuffer);
            if (err == SAL_Z_STREAM_END)
            {
                if (ZLIBInfo.avail_in > 0)
                    TRACE_E("CDataConnectionSocket::GiveFlushData(): ignoring data (" << ZLIBInfo.avail_in << " bytes) received after end of compressed stream");
                //DecomprMissingStreamEnd = FALSE;  // dekomprese uz je kompletni
            }
            if (err < 0)
            {
                if (err != SAL_Z_DATA_ERROR)
                    TRACE_E("CDataConnectionSocket::GiveFlushData(): SalZLIB->Inflate returns unexpected error: " << err);

                DecomprErrorOccured = TRUE;
                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDECOMPRERROR), -1, TRUE);
                if (Socket != INVALID_SOCKET)
                    CloseSocketEx(NULL); // zavreme "data connection", dale nema smysl pokracovat
                FreeFlushData();
                *deleteTgtFile = TRUE;
            }
            else
            {
                int newBytes = (DecomprDataAllocatedSize - DecomprDataDelayedBytes - ZLIBInfo.avail_out) -
                               (ValidBytesInFlushBuffer - AlreadyDecomprPartOfFlushBuffer - ZLIBInfo.avail_in);
                if (newBytes != 0) // vyrovname rozdil zpusobeny dekompresi (POZOR: muze byt i zaporny)
                {
                    if (newBytes > 0)
                    {
                        TotalReadBytesCount += CQuadWord(newBytes, 0);
                        //            if (err == SAL_Z_STREAM_END) TRACE_I("TotalReadBytesCount=" << TotalReadBytesCount.Value);
                        DWORD ti = GetTickCount();
                        TransferSpeedMeter.BytesReceived(newBytes, ti);
                        if (GlobalTransferSpeedMeter != NULL)
                            GlobalTransferSpeedMeter->BytesReceived(newBytes, ti);
                    }
                    else
                    {
                        if (TotalReadBytesCount < CQuadWord(-newBytes, 0))
                            TRACE_E("Unexpected situation in CDataConnectionSocket::GiveFlushData(): TotalReadBytesCount becomes negative value!");
                        TotalReadBytesCount -= CQuadWord(-newBytes, 0);
                        //            TRACE_I("newBytes=" << newBytes << ", decompressed=" << (DecomprDataAllocatedSize - DecomprDataDelayedBytes - ZLIBInfo.avail_out));
                    }
                    StatusHasChanged();
                }
            }

            BOOL isFirstDecompFromFlushBuffer = AlreadyDecomprPartOfFlushBuffer == 0; // TRUE = jde o prvni cast dekomprimovanych dat z FlushBuffer
            if (!DecomprErrorOccured)
                AlreadyDecomprPartOfFlushBuffer = (int)(ZLIBInfo.next_in - (BYTE*)FlushBuffer);
            if (err >= 0)
            {
                *flushBuffer = DecomprDataBuffer;
                if (err != SAL_Z_STREAM_END && !isFirstDecompFromFlushBuffer && *validBytesInFlushBuffer < DecomprDataAllocatedSize)
                { // pri pokracovani dekomprese dat z FlushBuffer v pripade, ze nejde o konec souboru a nedoslo k zaplneni bufferu DecomprDataBuffer (zaplneni zavisi na kompresnim pomeru, extremne muze jit i jen o jeden byte), pockame se zapisem do souboru na dalsi cyklus (aby nedochazelo ke zbytecne fragmentaci souboru)
                    //          TRACE_I("flushing: buffer is not full (only " << *validBytesInFlushBuffer << " bytes), waiting...");
                    DecomprDataDelayedBytes = *validBytesInFlushBuffer;
                    *validBytesInFlushBuffer = 0; // zapis prazdneho bufferu se neprovede
                }
                else
                {
                    DecomprDataDelayedBytes = 0;
                    //          TRACE_I("flushing: " << *validBytesInFlushBuffer << " bytes");
                }
                DecomprDataBuffer = NULL;
                ret = TRUE;
            }
            else
            {
                *flushBuffer = NULL;
                *validBytesInFlushBuffer = 0;
            }
        }
        else
        {
            *flushBuffer = FlushBuffer;
            *validBytesInFlushBuffer = ValidBytesInFlushBuffer;
            FlushBuffer = NULL;
            ret = TRUE;
        }
    }
    else
    {
        *flushBuffer = NULL;
        *validBytesInFlushBuffer = 0;
        if (ValidBytesInFlushBuffer > 0 &&
            (!CompressData || AlreadyDecomprPartOfFlushBuffer < ValidBytesInFlushBuffer))
        {
            TRACE_E("CDataConnectionSocket::GiveFlushData(): FlushBuffer or DecomprDataBuffer has been already given!");
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CDataConnectionSocket::FlushDataFinished(char* flushBuffer, BOOL enterSocketCritSect)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::FlushDataFinished()");

    if (enterSocketCritSect)
        HANDLES(EnterCriticalSection(&SocketCritSect));

    if (flushBuffer != NULL)
    {
        if (CompressData)
        {
            if (DecomprDataBuffer == NULL)
                DecomprDataBuffer = flushBuffer;
            else
            {
                TRACE_E("CDataConnectionSocket::FlushDataFinished(): DecomprDataBuffer is not NULL!");
                if (DecomprDataDelayedBytes > 0)
                    TRACE_E("CDataConnectionSocket::FlushDataFinished(): FATAL ERROR: DecomprDataDelayedBytes > 0: losing file data");
                free(flushBuffer);
            }
        }
        else
        {
            if (FlushBuffer == NULL)
                FlushBuffer = flushBuffer;
            else
            {
                TRACE_E("CDataConnectionSocket::FlushDataFinished(): FlushBuffer is not NULL!");
                free(flushBuffer);
            }
        }
    }
    else
        TRACE_E("CDataConnectionSocket::FlushDataFinished(): flushBuffer cannot be NULL!");

    if (CompressData && AlreadyDecomprPartOfFlushBuffer < ValidBytesInFlushBuffer)
    { // je-li treba pokracovat v dekompresi dat z 'FlushBuffer'
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        DoPostMessageToWorker(WorkerMsgFlushData);

        // pokud jde o primy flush dat do souboru z data-connectiony, provede se zde
        DirectFlushData();
    }
    else
    {
        if ((NeedFlushReadBuf != 0 || ReadBytesAllocatedSize - ValidBytesInReadBytesBuf == 0) &&
            ValidBytesInReadBytesBuf > 0)
        { // je-li treba znovu flushnout data
            // pokud byl zalozen timer pro flushnuti dat s timeoutem DATACON_FLUSHTIMEOUT, tak ho smazeme
            if (FlushTimerAdded)
            {
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                SocketsThread->DeleteTimer(UID, DATACON_FLUSHTIMERID);
                FlushTimerAdded = FALSE;
            }

            MoveReadBytesToFlushBuffer();

            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            DoPostMessageToWorker(WorkerMsgFlushData);

            // pokud jde o primy flush dat do souboru z data-connectiony, provede se zde
            DirectFlushData();
        }
        else
        {
            ValidBytesInFlushBuffer = 0;
            AlreadyDecomprPartOfFlushBuffer = 0;
        }

        if (NeedFlushReadBuf == 2 /* FD_READ */ || NeedFlushReadBuf == 3 /* FD_CLOSE */)
        {
            PostMessage(SocketsThread->GetHiddenWindow(), GetMsg(),
                        (WPARAM)GetSocket(), (NeedFlushReadBuf == 2 ? FD_READ : FD_CLOSE));
        }
        NeedFlushReadBuf = 0;
    }

    if (enterSocketCritSect)
        HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CDataConnectionSocket::AreAllDataFlushed(BOOL onlyTest)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::AreAllDataFlushed()");

    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (FlushData)
    {
        if (ValidBytesInFlushBuffer == 0 && ValidBytesInReadBytesBuf == 0)
            ret = TRUE;
        else
        {
            if (!onlyTest)
            {
                if (ValidBytesInFlushBuffer == 0)
                { // je-li mozne flushnout data, flushneme je
                    // pokud byl zalozen timer pro flushnuti dat s timeoutem DATACON_FLUSHTIMEOUT, tak ho smazeme
                    if (FlushTimerAdded)
                    {
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        SocketsThread->DeleteTimer(UID, DATACON_FLUSHTIMERID);
                        FlushTimerAdded = FALSE;
                    }

                    MoveReadBytesToFlushBuffer();

                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    DoPostMessageToWorker(WorkerMsgFlushData);

                    // pokud jde o primy flush dat do souboru z data-connectiony, provede se zde
                    DirectFlushData();

                    if (NeedFlushReadBuf != 0) // neprobihal flush dat, takze by tu mela byt nula
                        TRACE_E("CDataConnectionSocket::AreAllDataFlushed(): Unexpected value of NeedFlushReadBuf: " << NeedFlushReadBuf);
                }
            }
        }
    }
    else
        TRACE_E("CDataConnectionSocket::AreAllDataFlushed(): FlushData must be TRUE!");
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CDataConnectionSocket::FreeFlushData()
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::FreeFlushData()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (FlushData)
    {
        ValidBytesInReadBytesBuf = 0;
        ValidBytesInFlushBuffer = 0;
        AlreadyDecomprPartOfFlushBuffer = 0;
        NeedFlushReadBuf = 0;
        DecomprDataDelayedBytes = 0;

        if (!IsConnected())             // data-connection je zavrena + vsechna data flushnuta -> koncime
            SetEvent(TransferFinished); // TransferFinished nemuze byt NULL (IsGood() by vratila FALSE)
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::ReceiveTimer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (FlushTimerAdded && id == DATACON_FLUSHTIMERID) // mame provest flushnuti dat, timer vyprsel
    {
        FlushTimerAdded = FALSE;
        if (ValidBytesInReadBytesBuf > 0) // pokud je co flushnout (temer "always true")
        {
            if (ValidBytesInFlushBuffer == 0)
            { // je-li mozne flushnout data, flushneme je
                MoveReadBytesToFlushBuffer();

                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                DoPostMessageToWorker(WorkerMsgFlushData);

                // pokud jde o primy flush dat do souboru z data-connectiony, provede se zde
                DirectFlushData();

                if (NeedFlushReadBuf != 0) // neprobihal flush dat, takze by tu mela byt nula
                    TRACE_E("CDataConnectionSocket::ReceiveTimer(): Unexpected value of NeedFlushReadBuf: " << NeedFlushReadBuf);
            }
            else
            {
                if (NeedFlushReadBuf == 0)
                    NeedFlushReadBuf = 1; // je potreba flushnout data (FD_READ (NeedFlushReadBuf == 2) ani FD_CLOSE (NeedFlushReadBuf == 3) maji vyssi prioritu, neprepiseme je)
            }
        }
    }
    else
    {
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
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}
