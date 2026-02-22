// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
    LastActivityTime = GetTickCount(); // we rely on the object being constructed before sending the transfer command (the command will have a shorter or equal timeout)
    SocketCloseTime = GetTickCount();  // just to be safe, it should be overwritten before calling GetSocketCloseTime()

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
    LastActivityTime = GetTickCount(); // we rely on the object being initialized before sending the transfer command (the command will have a shorter or equal timeout)
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

    // reset before the next connect attempt...
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
        DWORD err; // 'ProxyServer' is accessible in data connections without the critical section
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
        if (!conRes) // there is no hope for success
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            NetEventLastError = err; // record the error (except fatal errors such as insufficient memory, etc.)
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
        TRACE_E("Unexpected situation in CDataConnectionSocket::PassiveConnect() - not in passive mode.");
        if (error != NULL)
            *error = NO_ERROR; // unknown error
        return FALSE;
    }
}

void CDataConnectionBaseSocket::SetActive(int logUID)
{
    CALL_STACK_MESSAGE2("CDataConnectionBaseSocket::SetActive(%d)", logUID);

    HANDLES(EnterCriticalSection(&SocketCritSect));

    UsePassiveMode = FALSE;
    LogUID = logUID;

    // reset before the next attempt to establish a connection...
    ClearBeforeConnect();
    TransferSpeedMeter.Clear();
    if (CompressData)
        ComprTransferSpeedMeter.Clear();
    NetEventLastError = NO_ERROR;
    SSLErrorOccured = SSLCONERR_NOERROR;
    ReceivedConnected = FALSE;

    LastActivityTime = GetTickCount(); // we rely on the object being initialized before sending the transfer command (the command will have a shorter or equal timeout)
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
        *transferFinished = ReceivedConnected && !ret; // the connection was established but is already closed? (finished)
    ret = ReceivedConnected && ret;                    // the connection was established and is still open? (transferring)
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

    if (passiveModeRetry) // in passive mode the first connection attempt was rejected, perform a second attempt
    {
        // CloseSocketEx(NULL);           // there is no point in closing the socket of the old "data connection" (it must already be closed)
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
        *s = 0; // trim newline characters from the error text
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
    AsciiTrModeForBinFileHowToSolve = 0 /* ask the user */;
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
                    if (err < 0) // decompression error: we expect SAL_Z_DATA_ERROR and SAL_Z_BUF_ERROR, the others are unexpected; BTW, SAL_Z_DATA_ERROR theoretically should never occur, it would have to be an internal server error (the data was compressed incorrectly), TCP is reliable; SAL_Z_BUF_ERROR occurs when the stream is not terminated (data transfer interrupted early)
                    {
                        if (err != SAL_Z_DATA_ERROR && (err != SAL_Z_BUF_ERROR || zi.avail_in != 0))
                            TRACE_E("CDataConnectionSocket::GiveData(): SalZLIB->Inflate returns unexpected error: " << err);
                        if (err != SAL_Z_BUF_ERROR || zi.avail_in != 0) // with incomplete data we assume the stream is otherwise OK and the decompressed data are OK as well
                        {
                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDECOMPRERROR), -1, TRUE);
                            *decomprErr = TRUE;
                            *length = 0; // when decompression fails the data may be completely garbage (CRC failed), so we discard them entirely
                        }
                        // else;  // we ignore the missing end-of-stream error; for example Serv-U 7 and 8 do not terminate it (6 still does); probably not a big deal because we check the server reply and the successful closing of the TCP connection (the transfer of all data from the server should therefore be ensured)
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
                // try to shrink the buffer so it does not occupy memory unnecessarily
                // NOTE: realloc(x, 0) frees x and returns NULL!
                ret = (char*)realloc(ret, max(1, *length));
                ignErr = SalZLIB->InflateEnd(&zi);
                if (ignErr < 0)
                    TRACE_E("SalZLIB->InflateEnd returns unexpected error: " << ignErr);
                free(ReadBytes);
            }
            else
            {
                if (ValidBytesInReadBytesBuf < ReadBytesAllocatedSize) // try to shrink the buffer so it does not occupy memory unnecessarily
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
        else // we return an empty buffer
        {
            ret = (char*)malloc(1); // if the allocation fails we are supposed to return NULL, period...
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
    ResetEvent(TransferFinished); // TransferFinished cannot be NULL (IsGood() would return FALSE)
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
        if (Socket != INVALID_SOCKET) // always true: the socket is connected
            CloseSocketEx(NULL);      // close the "data connection", there is no point keeping it any longer
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
    if (TgtDiskFileName[0] == 0 ||                                     // if this is not a direct flush of data to a file
        ValidBytesInFlushBuffer == 0 && ValidBytesInReadBytesBuf == 0) // or all data have already been flushed
    {
        SetEvent(TransferFinished); // TransferFinished cannot be NULL (IsGood() would return FALSE)
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

    if (TgtDiskFileName[0] != 0) // only when directly flushing data to a file from the data connection
    {
        if (TgtDiskFileClosed)
            TRACE_E("Unexpected situation in CDataConnectionSocket::DirectFlushData(): TgtDiskFileClosed is TRUE!");
        else
        {
            char* flushBuffer;
            int validBytesInFlushBuffer;
            BOOL deleteTgtFile;
            BOOL haveFlushData = GiveFlushData(&flushBuffer, &validBytesInFlushBuffer, &deleteTgtFile);

            if (deleteTgtFile) // we need to delete the target file because it may contain corrupted data
            {
                if (TgtDiskFile != NULL)
                {
                    FTPDiskThread->AddFileToClose("", TgtDiskFileName, TgtDiskFile, FALSE, FALSE, NULL, NULL,
                                                  TRUE, NULL, &TgtDiskFileCloseIndex);
                    TgtDiskFile = NULL; // the closing + deletion is only scheduled, but we will no longer work with the file
                    TgtDiskFileCreated = FALSE;
                    TgtDiskFileSize.Set(0, 0);
                }
                TgtDiskFileClosed = TRUE;
            }
            else
            {
                if (haveFlushData) // we have 'flushBuffer', we must hand it over to the disk thread (and free it if there is an error)
                {
                    if (!AsciiTrModeForBinFileProblemOccured && CurrentTransferMode == ctrmASCII &&
                        !SalamanderGeneral->IsANSIText(flushBuffer, validBytesInFlushBuffer))
                    {
                        AsciiTrModeForBinFileProblemOccured = TRUE; // detected the "ASCII mode for a binary file" problem
                        StatusHasChanged();                         // let the list-wait window know about it as soon as possible
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
                    else // unable to flush the data, cannot continue with the download
                    {
                        if (DiskWork.FlushDataBuffer != NULL)
                        {
                            free(DiskWork.FlushDataBuffer);
                            DiskWork.FlushDataBuffer = NULL;
                        }

                        ReadBytesLowMemory = TRUE; // stop due to insufficient memory
                        CloseSocketEx(NULL);       // close the "data connection", there is no point in continuing
                        FreeFlushData();
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                        // DoPostMessageToWorker(WorkerMsgConnectionClosed);  // this should not be necessary here
                    }
                }
                else
                    TRACE_E("Unexpected situation in CDataConnectionSocket::DirectFlushData(): data-connection has nothing to flush!"); // hopefully impossible
            }
        }
    }
}

void CDataConnectionSocket::CancelConnectionAndFlushing()
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::CancelConnectionAndFlushing()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    CloseSocketEx(NULL);
    if (TgtDiskFileName[0] != 0) // only when directly flushing data to a file from the data connection
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
    if (TgtDiskFileName[0] != 0)                                             // only when directly flushing data to a file from the data connection
        ret = ValidBytesInFlushBuffer != 0 || ValidBytesInReadBytesBuf != 0; // is data flushing in progress? (in other words: is there anything to flush?)
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

            if (DiskWork.State == sqisNone) // data flush succeeded
            {
                TgtDiskFileCreated = TRUE;

                // calculate the new file size
                TgtDiskFileSize += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0);

                // if the file was being created, obtain its handle here
                if (TgtDiskFile == NULL)
                    TgtDiskFile = DiskWork.OpenedFile;
                DiskWork.OpenedFile = NULL;

                // if the data connection exists, return the buffer for reuse
                char* flushData = DiskWork.FlushDataBuffer;
                DiskWork.FlushDataBuffer = NULL;
                FlushDataFinished(flushData, FALSE); // WARNING: we use an exception for calling from the critical section + DiskWork may be reused here!!!
            }
            else // an error occurred
            {
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }

                TgtFileLastError = DiskWork.WinError; // store the error when creating/writing to the target file
                CloseSocketEx(NULL);                  // shut down (we will not learn about the result)
                FreeFlushData();
                // since we are already in the CSocketsThread::CritSect section, this call
                // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                // DoPostMessageToWorker(WorkerMsgConnectionClosed);  // this should not be necessary here
            }

            if (!IsConnected() && !DiskWorkIsUsed) // the data connection is closed + all data flushed -> we are done
                SetEvent(TransferFinished);        // TransferFinished cannot be NULL (IsGood() would return FALSE)
        }
        break;
    }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::JustConnected()
{
    ReceivedConnected = TRUE; // if FD_READ arrives before FD_CONNECT (either way it must be connected)
    TransferSpeedMeter.JustConnected();
    // since we are already in the CSocketsThread::CritSect section, this call
    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
    DoPostMessageToWorker(WorkerMsgConnectedToServer);
    // since we are already in the CSocketsThread::CritSect section, this call
    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
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
            if (Socket != INVALID_SOCKET) // always true: the socket is connected
                CloseSocketEx(NULL);      // close the "data connection", there is no point keeping it any longer
            success = FALSE;
        }
    }
    if (success)
    {
        LastActivityTime = GetTickCount(); // the accept succeeded
        if (GlobalLastActivityTime != NULL)
            GlobalLastActivityTime->Set(LastActivityTime);
        NetEventLastError = NO_ERROR; // the previous accept may have failed; that error is no longer relevant
        SSLErrorOccured = SSLCONERR_NOERROR;
        StatusHasChanged();
        JustConnected();
    }
    else
    {
        NetEventLastError = winError;
        if (proxyError && NetEventLastError == NO_ERROR)
            NetEventLastError = ERROR_INVALID_FUNCTION /* it just must not be NO_ERROR */;
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
    case FD_CLOSE: // sometimes arrives before the last FD_READ, so we first try FD_READ and if it succeeds, post FD_CLOSE again (FD_READ may succeed again before it)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE;     // TRUE = FD_CLOSE arrived and there was data to read (handled as FD_READ) => post FD_CLOSE again (the current FD_CLOSE was a false alarm)
        BOOL skipSendingOfFDClose = FALSE; // TRUE = FD_CLOSE arrived and we are waiting for the data flush, so delay it (do not process it now)
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (ReceivedConnected || UsePassiveMode) // ignore closing of the "listen" socket
        {
            if (eventError == NO_ERROR) // only if no error occurred (according to the help only WSAENETDOWN can happen)
            {
                if (UsePassiveMode) // for an active connection we have to wait for FD_ACCEPT (this socket is the "listen" one, and only then comes the "data connection")
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
                            DataTransferPostponed = 1 /* FD_READ */; // must not forget FD_CLOSE in favour of FD_READ
                    }
                    else
                    {
                        DataTransferPostponed = 2 /* FD_CLOSE */;
                        skipSendingOfFDClose = TRUE;
                    }
                }
                else
                {
                    if (Socket != INVALID_SOCKET) // the socket is connected
                    {
                        BOOL lowMem = ReadBytesLowMemory;
                        if (FlushData) // data are handed over for verification/write to disk through FlushBuffer (buffer size DATACON_FLUSHBUFFERSIZE)
                        {
                            if (!lowMem && ReadBytesAllocatedSize < DATACON_FLUSHBUFFERSIZE) // the 'ReadBytes' buffer is not allocated
                            {
                                if (ReadBytes != NULL)
                                {
                                    TRACE_E("Unexpected situation in CDataConnectionSocket::ReceiveNetEvent(): ReadBytes is not NULL, but ReadBytesAllocatedSize < DATACON_FLUSHBUFFERSIZE");
                                    free(ReadBytes);
                                }
                                ReadBytes = (char*)malloc(DATACON_FLUSHBUFFERSIZE);
                                if (ReadBytes != NULL)
                                    ReadBytesAllocatedSize = DATACON_FLUSHBUFFERSIZE;
                                else // not enough memory to store the data in our buffer (only TRACE reports the error)
                                {
                                    TRACE_E(LOW_MEMORY);
                                    lowMem = TRUE;
                                }
                            }

                            if (!lowMem)
                            {
                                if (ReadBytesAllocatedSize - ValidBytesInReadBytesBuf > 0)
                                {
                                    // read as many bytes as possible into the buffer; do not read cyclically so that the data arrive gradually;
                                    // if there is more to read, we will receive FD_READ again
                                    int len;
                                    if (!SSLConn)
                                        len = recv(Socket, ReadBytes + ValidBytesInReadBytesBuf,
                                                   ReadBytesAllocatedSize - ValidBytesInReadBytesBuf, 0);
                                    else
                                    {
                                        if (SSLLib.SSL_pending(SSLConn) > 0) // if the internal SSL buffer is not empty, recv() is not called and no further FD_READ arrives, so we must post it ourselves, otherwise the data transfer stops
                                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                                        len = SSLLib.SSL_read(SSLConn, ReadBytes + ValidBytesInReadBytesBuf,
                                                              ReadBytesAllocatedSize - ValidBytesInReadBytesBuf);
                                    }
                                    if (len >= 0) // we may have read something (0 = the connection is already closed)
                                    {
                                        if (len > 0)
                                        {
                                            ValidBytesInReadBytesBuf += len;          // adjust the number of bytes already read by the newly read ones
                                            TotalReadBytesCount += CQuadWord(len, 0); // adjust the total number of bytes already read by the newly read ones
                                            LastActivityTime = GetTickCount();        // successfully read bytes from the socket
                                            if (GlobalLastActivityTime != NULL)
                                                GlobalLastActivityTime->Set(LastActivityTime);
                                            TransferSpeedMeter.BytesReceived(len, LastActivityTime);
                                            if (GlobalTransferSpeedMeter != NULL)
                                                GlobalTransferSpeedMeter->BytesReceived(len, LastActivityTime);
                                            StatusHasChanged();

                                            if (ReadBytesAllocatedSize - ValidBytesInReadBytesBuf == 0)
                                            { // nowhere to read more data, we need to flush the buffer
                                                if (NeedFlushReadBuf != 0 &&
                                                    (ValidBytesInFlushBuffer == 0 || NeedFlushReadBuf != 1 /* timer flush */))
                                                {
                                                    TRACE_E("CDataConnectionSocket::ReceiveNetEvent(): Unexpected value of NeedFlushReadBuf: " << NeedFlushReadBuf);
                                                }
                                                if (ValidBytesInFlushBuffer == 0) // start flushing data from the buffer (buffers swap and there will be room for reading again)
                                                {
                                                    // if a timer for flushing data with the DATACON_FLUSHTIMEOUT timeout was created, delete it
                                                    if (FlushTimerAdded)
                                                    {
                                                        // since we are already in the CSocketsThread::CritSect section, this call
                                                        // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                                                        SocketsThread->DeleteTimer(UID, DATACON_FLUSHTIMERID);
                                                        FlushTimerAdded = FALSE;
                                                    }

                                                    MoveReadBytesToFlushBuffer();

                                                    // since we are already in the CSocketsThread::CritSect section, this call
                                                    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                                                    DoPostMessageToWorker(WorkerMsgFlushData);

                                                    // if this is direct flushing of data to a file from the data connection, perform it here
                                                    DirectFlushData();

                                                    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                                        sendFDCloseAgain = TRUE;
                                                }
                                                else // wait for the data flush to finish, then swap the buffers and there will be room again
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

                                                // if no timer for flushing data with the DATACON_FLUSHTIMEOUT timeout has been created yet,
                                                // and at the same time the need to flush the buffer is not recorded (NeedFlushReadBuf == 0),
                                                // then create a timer for flushing the data
                                                if (!FlushTimerAdded && NeedFlushReadBuf == 0)
                                                {
                                                    // since we are already in the CSocketsThread::CritSect section, this call
                                                    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
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
                                            NetEventLastError = err; // an error occurred
                                            logLastErr = TRUE;
                                            CloseSocketEx(NULL); // close the "data connection", there is no point keeping it any longer
                                            // since we are already in the CSocketsThread::CritSect section, this call
                                            // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                                            DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                        }
                                    }
                                }
                                else // nowhere to read more data, we must wait until the buffer flush finishes (then the buffers swap and there will be room to read again)
                                {
                                    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                    {
                                        skipSendingOfFDClose = TRUE;
                                        NeedFlushReadBuf = 3; // flush + FD_CLOSE (can overwrite FD_READ)
                                    }
                                    else // WSAGETSELECTEVENT(lParam) == FD_READ
                                    {
                                        if (NeedFlushReadBuf != 3 /* flush + FD_CLOSE */)
                                            NeedFlushReadBuf = 2; // flush + FD_READ (cannot overwrite FD_CLOSE)
                                    }
                                }
                            }
                            else
                            {
                                ReadBytesLowMemory = TRUE; // stop reading due to insufficient memory
                                CloseSocketEx(NULL);       // close the "data connection", there is no point keeping it any longer
                                // since we are already in the CSocketsThread::CritSect section, this call
                                // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                                DoPostMessageToWorker(WorkerMsgConnectionClosed);
                            }
                        }
                        else // all data are stored in the ReadBytes buffer
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
                                else // not enough memory to store the data in our buffer (only TRACE reports the error)
                                {
                                    TRACE_E(LOW_MEMORY);
                                    lowMem = TRUE;
                                }
                            }

                            if (!lowMem)
                            { // read as many bytes as possible into the buffer; do not read cyclically so that the data arrive gradually;
                                // if there is more to read, we will receive FD_READ again
                                int len;

                                if (!SSLConn)
                                    len = recv(Socket, ReadBytes + ValidBytesInReadBytesBuf,
                                               ReadBytesAllocatedSize - ValidBytesInReadBytesBuf, 0);
                                else
                                {
                                    if (SSLLib.SSL_pending(SSLConn) > 0) // if the internal SSL buffer is not empty, recv() is not called and no further FD_READ arrives, so we must post it ourselves, otherwise the data transfer stops
                                        PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                                    len = SSLLib.SSL_read(SSLConn, ReadBytes + ValidBytesInReadBytesBuf,
                                                          ReadBytesAllocatedSize - ValidBytesInReadBytesBuf);
                                }
                                if (len >= 0) // we may have read something (0 = the connection is already closed)
                                {
                                    if (len > 0)
                                    {
                                        ValidBytesInReadBytesBuf += len;          // adjust the number of bytes already read by the newly read ones
                                        TotalReadBytesCount += CQuadWord(len, 0); // adjust the total number of bytes already read by the newly read ones
                                        LastActivityTime = GetTickCount();        // successfully read bytes from the socket
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
                                        NetEventLastError = err; // an error occurred
                                        logLastErr = TRUE;
                                        CloseSocketEx(NULL); // close the "data connection", there is no point keeping it any longer
                                        // since we are already in the CSocketsThread::CritSect section, this call
                                        // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                                        DoPostMessageToWorker(WorkerMsgConnectionClosed);
                                    }
                                }
                            }
                            else
                            {
                                ReadBytesLowMemory = TRUE; // stop reading due to insufficient memory
                                CloseSocketEx(NULL);       // close the "data connection", there is no point keeping it any longer
                                // since we are already in the CSocketsThread::CritSect section, this call
                                // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                                DoPostMessageToWorker(WorkerMsgConnectionClosed);
                            }
                        }
                    }
                    else
                    {
                        // may happen: the main thread manages to call CloseSocket() before FD_READ is delivered
                        // TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveNetEvent(FD_READ): Socket is not connected.");
                        // we will not bother the event loop with this unexpected error (solution: the user presses ESC)
                    }
                }
            }
            else
            {
                if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // FD_CLOSE will handle the error on its own
                {
                    NetEventLastError = eventError;
                    logLastErr = TRUE;
                    CloseSocketEx(NULL); // close the "data connection", there is no point keeping it any longer
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                    DoPostMessageToWorker(WorkerMsgConnectionClosed);
                }
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        // now process FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE && !skipSendingOfFDClose)
        {
            if (sendFDCloseAgain) // FD_CLOSE came instead of FD_READ => post FD_CLOSE again
            {
                PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                            (WPARAM)GetSocket(), lParam);
            }
            else // correct FD_CLOSE
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
            if (!ReceivedConnected)
                JustConnected();
            LastActivityTime = GetTickCount(); // the connect succeeded
            if (GlobalLastActivityTime != NULL)
                GlobalLastActivityTime->Set(LastActivityTime);
            StatusHasChanged();
        }
        else
        {
            NetEventLastError = eventError;
            logLastErr = TRUE;
            CloseSocketEx(NULL); // close the "data connection" socket, it can no longer be opened
            // since we are already in the CSocketsThread::CritSect section, this call
            // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
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
    if (TgtDiskFileName[0] == 0 ||                                     // if this is not a direct flush of data to a file
        ValidBytesInFlushBuffer == 0 && ValidBytesInReadBytesBuf == 0) // or all data have already been flushed
    {
        SetEvent(TransferFinished); // TransferFinished cannot be NULL (IsGood() would return FALSE)
    }
    if (error != NO_ERROR)
        NetEventLastError = error;

    // since we are already in the CSocketsThread::CritSect section, this call
    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
    DoPostMessageToWorker(WorkerMsgConnectionClosed);

    // since we are already in the CSocketsThread::CritSect section, this call
    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
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
    StatusMessageSent = (WindowWithStatus == NULL); // nothing needs to be sent
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
        if (DiskWorkIsUsed) // if some disk flush is in progress, cancel it
        {
            BOOL workIsInProgress;
            if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
            {
                if (workIsInProgress)
                    DiskWork.FlushDataBuffer = NULL; // the work is in progress; we cannot free the buffer with the data being written/tested, leave it to the disk-work thread (see the cancellation part) - we can write to DiskWork because after Cancel the disk thread must no longer access it (it may no longer exist)
                else
                { // the work was cancelled before the disk thread started processing it - deallocate the flush buffer
                    if (DiskWork.FlushDataBuffer != NULL)
                    {
                        free(DiskWork.FlushDataBuffer);
                        DiskWork.FlushDataBuffer = NULL;
                    }
                }
            }
            else // the work is already finished, DATACON_DISKWORKWRITEFINISHED is supposedly on its way (has not arrived yet)
            {
                if (DiskWork.FlushDataBuffer != NULL) // the flush buffer is now useless
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }
                if (DiskWork.State == sqisNone) // data flush succeeded
                {
                    TgtDiskFileCreated = TRUE;

                    // calculate the new file size
                    TgtDiskFileSize += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0);

                    // if the file was being created, obtain its handle here
                    if (TgtDiskFile == NULL)
                        TgtDiskFile = DiskWork.OpenedFile;
                    DiskWork.OpenedFile = NULL;
                }
                else // an error occurred
                {
                    TgtFileLastError = DiskWork.WinError; // store the error when creating/writing to the target file
                    CloseSocketEx(NULL);                  // shut down (we will not learn about the result)
                    FreeFlushData();
                    // WARNING: we are not in the CSocketsThread::CritSect section, so this call is not possible:
                    // DoPostMessageToWorker(WorkerMsgConnectionClosed);  // this should not be necessary here
                }
            }
            DiskWorkIsUsed = FALSE;
        }
        if (TgtDiskFile != NULL)
        {
            FTPDiskThread->AddFileToClose("", TgtDiskFileName, TgtDiskFile, FALSE, FALSE, NULL, NULL,
                                          FALSE, NULL, &TgtDiskFileCloseIndex);
            TgtDiskFile = NULL; // the closing is only scheduled, but we will no longer work with the file
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
    return FALSE; // the file probably was not opened at all (or the closing+delete is handled on the disk-work thread - cancellation during the first create+flush disk job)
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
                //DecomprMissingStreamEnd = FALSE;  // decompression is now complete
            }
            if (err < 0)
            {
                if (err != SAL_Z_DATA_ERROR)
                    TRACE_E("CDataConnectionSocket::GiveFlushData(): SalZLIB->Inflate returns unexpected error: " << err);

                DecomprErrorOccured = TRUE;
                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDECOMPRERROR), -1, TRUE);
                if (Socket != INVALID_SOCKET)
                    CloseSocketEx(NULL); // close the "data connection", there is no point in continuing
                FreeFlushData();
                *deleteTgtFile = TRUE;
            }
            else
            {
                int newBytes = (DecomprDataAllocatedSize - DecomprDataDelayedBytes - ZLIBInfo.avail_out) -
                               (ValidBytesInFlushBuffer - AlreadyDecomprPartOfFlushBuffer - ZLIBInfo.avail_in);
                if (newBytes != 0) // balance the difference caused by decompression (WARNING: it can also be negative)
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

            BOOL isFirstDecompFromFlushBuffer = AlreadyDecomprPartOfFlushBuffer == 0; // TRUE = this is the first portion of decompressed data from FlushBuffer
            if (!DecomprErrorOccured)
                AlreadyDecomprPartOfFlushBuffer = (int)(ZLIBInfo.next_in - (BYTE*)FlushBuffer);
            if (err >= 0)
            {
                *flushBuffer = DecomprDataBuffer;
                if (err != SAL_Z_STREAM_END && !isFirstDecompFromFlushBuffer && *validBytesInFlushBuffer < DecomprDataAllocatedSize)
                { // when continuing to decompress data from FlushBuffer, if it is not the end of the file and the DecomprDataBuffer was not filled (fill level depends on compression ratio, in extreme cases it may be just a single byte), wait with writing to the file until the next cycle (to avoid unnecessary fragmentation)
                    //          TRACE_I("flushing: buffer is not full (only " << *validBytesInFlushBuffer << " bytes), waiting...");
                    DecomprDataDelayedBytes = *validBytesInFlushBuffer;
                    *validBytesInFlushBuffer = 0; // do not write an empty buffer
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
    { // if it is necessary to continue decompressing data from 'FlushBuffer'
        // since we are already in the CSocketsThread::CritSect section, this call
        // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
        DoPostMessageToWorker(WorkerMsgFlushData);

        // if this is direct flushing of data to a file from the data connection, perform it here
        DirectFlushData();
    }
    else
    {
        if ((NeedFlushReadBuf != 0 || ReadBytesAllocatedSize - ValidBytesInReadBytesBuf == 0) &&
            ValidBytesInReadBytesBuf > 0)
        { // if the data need to be flushed again
            // if a timer for flushing data with the DATACON_FLUSHTIMEOUT timeout was created, delete it
            if (FlushTimerAdded)
            {
                // since we are already in the CSocketsThread::CritSect section, this call
                // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                SocketsThread->DeleteTimer(UID, DATACON_FLUSHTIMERID);
                FlushTimerAdded = FALSE;
            }

            MoveReadBytesToFlushBuffer();

            // since we are already in the CSocketsThread::CritSect section, this call
            // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
            DoPostMessageToWorker(WorkerMsgFlushData);

            // if this is direct flushing of data to a file from the data connection, perform it here
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
                { // if it is possible to flush the data, flush them
                    // if a timer for flushing data with the DATACON_FLUSHTIMEOUT timeout was created, delete it
                    if (FlushTimerAdded)
                    {
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                        SocketsThread->DeleteTimer(UID, DATACON_FLUSHTIMERID);
                        FlushTimerAdded = FALSE;
                    }

                    MoveReadBytesToFlushBuffer();

                    // since we are already in the CSocketsThread::CritSect section, this call
                    // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                    DoPostMessageToWorker(WorkerMsgFlushData);

                    // if this is direct flushing of data to a file from the data connection, perform it here
                    DirectFlushData();

                    if (NeedFlushReadBuf != 0) // no data flush was running, so this should be zero
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

        if (!IsConnected())             // the data connection is closed + all data flushed -> we are done
            SetEvent(TransferFinished); // TransferFinished cannot be NULL (IsGood() would return FALSE)
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CDataConnectionSocket::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE1("CDataConnectionSocket::ReceiveTimer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (FlushTimerAdded && id == DATACON_FLUSHTIMERID) // we need to flush the data, the timer expired
    {
        FlushTimerAdded = FALSE;
        if (ValidBytesInReadBytesBuf > 0) // if there is something to flush (almost "always true")
        {
            if (ValidBytesInFlushBuffer == 0)
            { // if it is possible to flush the data, flush them
                MoveReadBytesToFlushBuffer();

                // since we are already in the CSocketsThread::CritSect section, this call
                // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                DoPostMessageToWorker(WorkerMsgFlushData);

                // if this is direct flushing of data to a file from the data connection, perform it here
                DirectFlushData();

                if (NeedFlushReadBuf != 0) // no data flush was running, so this should be zero
                    TRACE_E("CDataConnectionSocket::ReceiveTimer(): Unexpected value of NeedFlushReadBuf: " << NeedFlushReadBuf);
            }
            else
            {
                if (NeedFlushReadBuf == 0)
                    NeedFlushReadBuf = 1; // we need to flush data (FD_READ (NeedFlushReadBuf == 2) and FD_CLOSE (NeedFlushReadBuf == 3) have higher priority, we will not overwrite them)
            }
        }
    }
    else
    {
        if (id == DATACON_TESTNODATATRTIMERID && Socket != INVALID_SOCKET)
        { // periodic testing of the no-data-transfer timeout
            if (!WorkerPaused &&
                (GetTickCount() - LastActivityTime) / 1000 >= (DWORD)Config.GetNoDataTransferTimeout())
            { // timeout occurred, close the data connection - simulate that the server did it
                NoDataTransTimeout = TRUE;
                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGNODATATRTIMEOUT), -1, TRUE);
                HANDLES(LeaveCriticalSection(&SocketCritSect));
                CSocket::ReceiveNetEvent(MAKELPARAM(FD_CLOSE, WSAECONNRESET), GetMsgIndex()); // call the base method
                HANDLES(EnterCriticalSection(&SocketCritSect));
            }
            else // the timeout has not occurred yet, add a timer for the next test
            {
                // since we are already in the CSocketsThread::CritSect section, this call
                // can also be made from the CSocket::SocketCritSect section (no deadlock risk)
                SocketsThread->AddTimer(Msg, UID, GetTickCount() + DATACON_TESTNODATATRTIMEOUT,
                                        DATACON_TESTNODATATRTIMERID, NULL);
            }
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}
