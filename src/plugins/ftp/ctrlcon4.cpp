// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CControlConnectionSocket
//

BOOL CControlConnectionSocket::SetCurrentTransferMode(HWND parent, BOOL asciiMode, BOOL* success,
                                                      char* ftpReplyBuf, int ftpReplyBufSize,
                                                      BOOL forceRefresh, BOOL* canRetry,
                                                      char* retryMsg, int retryMsgBufSize)
{
    CALL_STACK_MESSAGE5("CControlConnectionSocket::SetCurrentTransferMode(, %d, , , %d, %d, , %d)",
                        asciiMode, ftpReplyBufSize, forceRefresh, retryMsgBufSize);

    if (success != NULL)
        *success = FALSE;
    if (ftpReplyBufSize > 0)
        ftpReplyBuf[0] = 0;
    if (canRetry != NULL)
        *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL leaveSect = TRUE;
    BOOL ret = TRUE;

    if (forceRefresh ||
        asciiMode && CurrentTransferMode != ctrmASCII ||
        !asciiMode && CurrentTransferMode != ctrmBinary)
    {
        char cmdBuf[50];
        char logBuf[50];

        HANDLES(LeaveCriticalSection(&SocketCritSect));
        leaveSect = FALSE;

        // change the transfer mode on the server
        PrepareFTPCommand(cmdBuf, 50, logBuf, 50, ftpcmdSetTransferMode, NULL, asciiMode); // cannot fail
        int ftpReplyCode;
        if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                           &ftpReplyCode, ftpReplyBuf, ftpReplyBufSize, FALSE, FALSE, TRUE,
                           canRetry, retryMsg, retryMsgBufSize, NULL))
        {
            if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // success is returned (should be 200)
            {
                HANDLES(EnterCriticalSection(&SocketCritSect));
                leaveSect = TRUE;
                CurrentTransferMode = (asciiMode ? ctrmASCII : ctrmBinary); // the transfer mode was changed
            }
            else
                CurrentTransferMode = ctrmUnknown; // unknown error, might not matter, but return the error for the caller to judge
        }
        else
            ret = FALSE; // error -> connection closed
    }

    if (leaveSect) // the requested transfer mode is already set successfully
    {
        if (success != NULL)
            *success = TRUE;
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
    return ret;
}

//
// **************************************************************************************
// CSendCmdUserIfaceForListAndDownload
//

BOOL CSendCmdUserIfaceForListAndDownload::HadError()
{
    DWORD netErr, tgtFileErr;
    BOOL lowMem;
    int sslErrorOccured;
    BOOL decomprErrorOccured;
    DataConnection->GetError(&netErr, &lowMem, &tgtFileErr, NULL, &sslErrorOccured, &decomprErrorOccured);
    return netErr != NO_ERROR || lowMem || tgtFileErr != NO_ERROR || sslErrorOccured != SSLCONERR_NOERROR ||
           decomprErrorOccured;
}

void CSendCmdUserIfaceForListAndDownload::GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr,
                                                   BOOL* noDataTrTimeout, int* sslErrorOccured,
                                                   BOOL* decomprErrorOccured)
{
    DataConnection->GetError(netErr, lowMem, tgtFileErr, noDataTrTimeout, sslErrorOccured,
                             decomprErrorOccured);
}

HANDLE
CSendCmdUserIfaceForListAndDownload::GetFinishedEvent()
{
    return DataConnection->GetTransferFinishedEvent();
}

void CSendCmdUserIfaceForListAndDownload::InitWnd(const char* fileName, const char* host,
                                                  const char* path, CFTPServerPathType pathType)
{
    char buf[500];
    if (!ForDownload)
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LISTWNDDOWNLOADING), host);
    else
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LISTWNDDOWNLOADINGFILE), fileName, host);
    WaitWnd.SetText(buf);
    WaitWnd.SetPath(path, pathType);
}

void CSendCmdUserIfaceForListAndDownload::AfterWrite(BOOL aborting, DWORD showTime)
{
    WaitWnd.Create(showTime);
    if (!aborting)
        DataConnection->ActivateConnection();
}

BOOL CSendCmdUserIfaceForListAndDownload::HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort)
{
    BOOL offerAbort = isSend && allowCmdAbort ||           // if abort is possible and we have not done it yet
                      DataConnection->IsTransfering(NULL); // if no data transfer is in progress, we cannot abort (only terminate the "control connection")

    WaitWnd.Show(FALSE);
    BOOL esc = SalamanderGeneral->SalMessageBox(parent,
                                                LoadStr(!AlreadyAborted ? (offerAbort ? (ForDownload ? IDS_LISTWNDDOWNLFILESENDCMDESC : IDS_LISTWNDSENDCOMMANDESC) : (ForDownload ? IDS_LISTWNDDOWNLFILESENDCMDESC2 : IDS_LISTWNDSENDCOMMANDESC2)) : IDS_LISTWNDABORTCOMMANDESC),
                                                LoadStr(IDS_FTPPLUGINTITLE),
                                                MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
    if (esc)
    {
        WaitWnd.SetText(LoadStr(ForDownload ? IDS_LISTWNDDOWNLFILEABORTING : IDS_LISTWNDABORTINGCOMMAND));
        if (!offerAbort)
            AlreadyAborted = TRUE;
        if (!AlreadyAborted)
        {
            if (DataConnection->IsTransfering(NULL) ||  // just in case, maybe the connection is already closed and at the same time
                DataConnection->IsFlushingDataToDisk()) // there is nothing left to flush; avoid logging nonsense too often (it can still happen, never mind)
            {
                DataConnection->CancelConnectionAndFlushing(); // close the "data connection"; the system attempts a "graceful" shutdown (we will not learn the result)
                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONTERMINATED), -1, TRUE);
            }
            AlreadyAborted = TRUE;
            if (!allowCmdAbort)
                esc = FALSE; // that is not ESC for listing yet
        }
    }
    else
        SalamanderGeneral->WaitForESCRelease(); // a measure to keep the next action from being interrupted after every ESC in the previous message box
    if (!esc)
        WaitWnd.Show(TRUE); // nothing happened; continue showing the wait window (during abort a different text is displayed in the window)
    return esc;
}

void CSendCmdUserIfaceForListAndDownload::SendingFinished()
{
    WaitWnd.Destroy();
}

BOOL CSendCmdUserIfaceForListAndDownload::IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID,
                                                    char* errBuf, int errBufSize)
{
    BOOL trFinished;
    BOOL ret = FALSE;
    if (DataConnection->IsTransfering(&trFinished))
        *start = GetTickCount(); // waiting for data, so this is not a timeout
    else
    {
        if (trFinished)
        {
            *start = DataConnection->GetSocketCloseTime();
            ret = (GetTickCount() - *start) >= serverTimeout; // the timeout is measured from the connection closing (the moment since when the server can react and also learns about the closure)
        }
        else
            ret = TRUE; // the connection has not opened yet -> treat it as a timeout
    }
    if (ret)
    {
        char errText[300];
        if (DataConnection->GetProxyTimeoutDescr(errText, 300))
        {
            if (errBufSize > 0)
                _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), errText);
            *errorTextID = -1; // the description is directly in 'errBuf'
        }
        else
            *errorTextID = ForDownload ? IDS_LISTWNDDOWNLFILETIMEOUT : IDS_LISTCMDTIMEOUT;
    }
    return ret;
}

void CSendCmdUserIfaceForListAndDownload::CancelDataCon()
{
    DataConnection->CancelConnectionAndFlushing();
}

void CSendCmdUserIfaceForListAndDownload::MaybeSuccessReplyReceived(const char* reply, int replySize)
{
    DataConnection->EncryptPassiveDataCon();

    CQuadWord size;
    if (FTPGetDataSizeInfoFromSrvReply(size, reply, replySize))
    {
        // we have the total size of the listing - 'size'
        DataConnection->SetDataTotalSize(size);
    }
}

BOOL CSendCmdUserIfaceForListAndDownload::CanFinishSending(int replyCode, BOOL* useTimeout)
{
    BOOL trFinished;
    BOOL ret = DataConnection->IsTransfering(&trFinished);
    if (!ret && !trFinished && DataConnection->IsConnected()) // the connection has not been established yet and the socket is open
    {
        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
        {
            *useTimeout = TRUE; // do not close the socket; bypass WarFTPD bugs: it can return "success" even before the data-connection socket is accepted (before the data transfer starts) - we do not bother printing an error if a timeout occurs in this situation (WarFTPD will not perform the data transfer); list & view are not that important and it is unlikely to happen
            ret = TRUE;         // simulate that data are being transferred right now
        }
        else
            DataConnection->CloseSocketEx(NULL); // close the socket (it only waits for the connection); the server apparently reports a command error (listing)
    }
    if (!ret && ForDownload)
    {
        SocketsThread->LockSocketsThread();
        ret = !DataConnection->AreAllDataFlushed(FALSE);
        SocketsThread->UnlockSocketsThread();
        if (!ret)
            DataConnection->CloseTgtFile(); // successful file closing after the data flush completes
    }
    return !ret; // either the connection was never established or it is already closed
}

void CSendCmdUserIfaceForListAndDownload::BeforeWaitingForFinish(int replyCode, BOOL* useTimeout)
{
    if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS) // LIST does not return success - it may not close
    {                                             // the data connection (e.g., WarFTPD) - wait for the remaining data, but preferably with a timeout
        *useTimeout = TRUE;
        //    DataConnection->CloseSocketEx(NULL);   // to have something to show in the Show Raw Listing panel we must fetch the remaining data
    }
}

void CSendCmdUserIfaceForListAndDownload::HandleDataConTimeout(DWORD* start)
{
    DWORD lastActTime = DataConnection->GetLastActivityTime();
    if (*start < lastActTime)
        *start = lastActTime;
    else
    {
        BOOL trFinished;
        if (!DataConnection->IsTransfering(&trFinished) && !trFinished &&
            DataConnection->IsConnected()) // the connection has not been established yet and the socket is open
        {
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONNOTOPENED), -1, TRUE);
        }
        DatConCancelled = TRUE;
        DataConnection->CancelConnectionAndFlushing(); // stop waiting for data; we have been waiting too long (a timeout occurred)
    }
}

void CSendCmdUserIfaceForListAndDownload::HandleESCWhenWaitingForFinish(HWND parent)
{
    WaitWnd.Show(FALSE);
    BOOL esc = SalamanderGeneral->SalMessageBox(parent,
                                                LoadStr(ForDownload ? IDS_LISTWNDDOWNLFILESENDCMDESC : IDS_LISTWNDSENDCOMMANDESC),
                                                LoadStr(IDS_FTPPLUGINTITLE),
                                                MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
    if (esc)
    {
        // WaitWnd.SetText(LoadStr(IDS_LISTWNDABORTINGCOMMAND)); // unnecessary, the window will not be shown again
        // while the user decides how to respond to the abort, the data connection may finish (that is why
        // the dialog says "listing may be incomplete") - then it makes sense to ignore the abort
        if (DataConnection->IsTransfering(NULL) || DataConnection->IsFlushingDataToDisk())
        {
            DataConnection->CancelConnectionAndFlushing(); // close the "data connection"; the system attempts a "graceful" shutdown (we will not learn the result)
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONTERMINATED), -1, TRUE);
            AlreadyAborted = TRUE;
        }
    }
    else
        SalamanderGeneral->WaitForESCRelease(); // a measure to keep the next action from being interrupted after every ESC in the previous message box
    if (!esc)
        WaitWnd.Show(TRUE);
}

// ***********************************************************************************

BOOL CControlConnectionSocket::IsListCommandLIST_a()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = _stricmp(UseLIST_aCommand ? LIST_a_CMD_TEXT : (ListCommand != NULL && *ListCommand != 0 ? ListCommand : LIST_CMD_TEXT),
                        LIST_a_CMD_TEXT) == 0;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CControlConnectionSocket::ToggleListCommandLIST_a()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (IsListCommandLIST_a())
    {
        if (UseLIST_aCommand)
            UseLIST_aCommand = FALSE;
        else
        {
            if (ListCommand != NULL)
                free(ListCommand);
            ListCommand = NULL;
        }
    }
    else
        UseLIST_aCommand = TRUE;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CControlConnectionSocket::ListWorkingPath(HWND parent, const char* path, char* userBuf,
                                               int userBufSize, char** allocatedListing,
                                               int* allocatedListingLen, CFTPDate* listingDate,
                                               BOOL* pathListingIsIncomplete, BOOL* pathListingIsBroken,
                                               BOOL* pathListingMayBeOutdated,
                                               DWORD* pathListingStartTime, BOOL forceRefresh,
                                               int* totalAttemptNum, BOOL* fatalError,
                                               BOOL dontClearCache)
{
    CALL_STACK_MESSAGE4("CControlConnectionSocket::ListWorkingPath(, %s, , %d, , , , , , , , %d, , ,)",
                        path, userBufSize, forceRefresh);

    *fatalError = FALSE;
    *pathListingIsBroken = FALSE;
    BOOL ok = TRUE;
    BOOL ret = TRUE;
    char cmdBuf[50 + FTP_MAX_PATH];
    char logBuf[50 + FTP_MAX_PATH];
    char replyBuf[700];
    char errBuf[900 + FTP_MAX_PATH];
    char listCmd[FTPCOMMAND_MAX_SIZE + 2];

    HANDLES(EnterCriticalSection(&SocketCritSect));
    lstrcpyn(listCmd, UseLIST_aCommand ? LIST_a_CMD_TEXT : (ListCommand != NULL && *ListCommand != 0 ? ListCommand : LIST_CMD_TEXT),
             FTPCOMMAND_MAX_SIZE);
    BOOL usePassiveModeAux = UsePassiveMode;
    int logUID = LogUID; // log UID of this connection
    int useListingsCacheAux = UseListingsCache;
    CFTPProxyForDataCon* dataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
    BOOL dataConProxyServerOK = ProxyServer == NULL || dataConProxyServer != NULL;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    strcat(listCmd, "\r\n");

    int attemptNum = 1;
    if (totalAttemptNum != NULL)
        attemptNum = *totalAttemptNum;
    const char* retryMsgAux = NULL;
    BOOL canRetry = FALSE;
    char retryMsgBuf[300];
    char hostTmp[HOST_MAX_SIZE];

    // obtain the date when the listing was created (we assume the server creates it first and only then sends it)
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD lstStTime = 0;

    // allocate the object for the "data connection"
    CDataConnectionSocket* dataConnection = dataConProxyServerOK ? new CDataConnectionSocket(FALSE, dataConProxyServer, EncryptDataConnection, pCertificate, CompressData, this) : NULL;
    if (dataConnection == NULL || !dataConnection->IsGood())
    {
        if (dataConnection != NULL)
            DeleteSocket(dataConnection); // it will only be deallocated
        else
        {
            if (dataConProxyServer != NULL)
                delete dataConProxyServer;
        }
        dataConnection = NULL;
        TRACE_E(LOW_MEMORY);
        *fatalError = TRUE; // fatal error
    }
    else
    {
        while (1)
        {
            ReuseSSLSessionFailed = FALSE;
            if (ok && usePassiveModeAux) // passive mode (PASV)
            {
                PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                  ftpcmdPassive, NULL); // cannot fail
                int ftpReplyCode;
                if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                   &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                   retryMsgBuf, 300, NULL))
                {
                    DWORD ip;
                    unsigned short port;
                    if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS &&      // success (should be 227)
                        FTPGetIPAndPortFromReply(replyBuf, -1, &ip, &port)) // managed to obtain IP and port
                    {
                        dataConnection->SetPassive(ip, port, logUID);
                        dataConnection->PassiveConnect(NULL); // the first attempt; the result does not matter (it is checked later)
                    }
                    else // passive mode is not supported
                    {
                        HANDLES(EnterCriticalSection(&SocketCritSect));
                        UsePassiveMode = usePassiveModeAux = FALSE; // try it again in the active mode (PORT)
                        HANDLES(LeaveCriticalSection(&SocketCritSect));

                        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);
                    }
                }
                else // error -> connection closed
                {
                    ok = FALSE;
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" is allowed; go to the next reconnect
                    else
                    {
                        *fatalError = TRUE; // fatal error
                        break;
                    }
                }
            }

            if (ok && !usePassiveModeAux) // active mode (PORT)
            {
                DWORD localIP;
                GetLocalIP(&localIP, NULL);   // should not be able to fail
                unsigned short localPort = 0; // listen on any port
                dataConnection->SetActive(logUID);
                if (OpenForListeningAndWaitForRes(parent, dataConnection, &localIP, &localPort, &canRetry,
                                                  retryMsgBuf, 300, GetWaitTime(WAITWND_COMOPER),
                                                  errBuf, 900 + FTP_MAX_PATH))
                {
                    PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdSetPort, NULL, localIP, localPort); // cannot fail
                    int ftpReplyCode;
                    if (!SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                        &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                        retryMsgBuf, 300, NULL)) // we ignore the server's response; the error will appear later (timeout while listing)
                    {                                            // error -> connection closed
                        ok = FALSE;
                        if (canRetry)
                            retryMsgAux = retryMsgBuf; // "retry" is allowed; go to the next reconnect
                        else
                        {
                            *fatalError = TRUE; // fatal error
                            break;
                        }
                    }
                }
                else // failed to open the "listen" socket for receiving the data connection from the server ->
                {    // connection closed (so that the standard Retry can be used)
                    ok = FALSE;
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" is allowed; go to the next reconnect
                    else
                    {
                        *fatalError = TRUE; // fatal error
                        break;
                    }
                }
            }

            if (ok) // if we are still connected, switch the transfer mode to ASCII (ignore success)
            {
                ok = SetCurrentTransferMode(parent, TRUE, NULL, NULL, 0, forceRefresh, &canRetry,
                                            retryMsgBuf, 300);
                if (!ok) // error -> connection closed
                {
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" is allowed; go to the next reconnect
                    else
                    {
                        *fatalError = TRUE; // fatal error
                        break;
                    }
                }
            }

            BOOL sslErrReconnect = FALSE;     // TRUE = reconnect because of SSL errors
            BOOL fastSSLErrReconnect = FALSE; // TRUE = the server certificate changed; an immediate reconnect is desirable (without waiting 20 seconds)
            if (ok)
            {
                int ftpReplyCode;
                CSendCmdUserIfaceForListAndDownload userIface(FALSE, parent, dataConnection, logUID);

                //        if (UsePassiveMode) {
                //          dataConnection->EncryptConnection();
                //        }
                HANDLES(EnterCriticalSection(&SocketCritSect));
                lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
                CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                userIface.InitWnd(NULL, hostTmp, path, pathType);
                lstStTime = IncListingCounter();
                if (SendFTPCommand(parent, listCmd, listCmd, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                   &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                   retryMsgBuf, 300, &userIface))
                {
                    if (!userIface.GetDatConCancelled() && !userIface.WasAborted() && !userIface.HadError() &&
                        FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS &&
                        FTP_DIGIT_2(ftpReplyCode) != FTP_D2_CONNECTION) // it is not just a connection (network) error
                    {                                                   // the server refuses to list
                        BOOL skipMessage = FTPIsEmptyDirListErrReply(replyBuf);
                        if (!skipMessage)
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_LISTPATHERROR), path, replyBuf);
                            // if the user answers IDNO, we stop - the path cannot be listed -> a path change is required
                            ret = SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                                   MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                       MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
                            if (!ret)
                                SalamanderGeneral->WaitForESCRelease(); // a measure to keep the next action from being interrupted after every ESC in the previous message box

                            *pathListingIsBroken = TRUE; // to make it clear that the returned listing is not OK (VxWorks: while listing it can report "error reading entry: 16" and return "550 no files found or ...")
                        }
                        // VMS returns 550 for an empty directory: we cannot leave the path and the listing can easily
                        // be considered OK (it can even be cached - it was not interrupted and the server
                        // will most likely not return a different listing)
                        // ret = FALSE;   // stop - the path cannot be listed -> a path change is required

                        break; // report a "successful listing" (allows working in an empty/unlistable directory)
                    }
                    else
                    {
                        if (userIface.WasAborted()) // the user aborted the listing - finish with an error (an incomplete listing)
                            ok = FALSE;             // do not display the "list can be incomplete" message; the user was warned during abort
                        else
                        {
                            if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS &&
                                    FTP_DIGIT_2(ftpReplyCode) == FTP_D2_CONNECTION || // it is only a network error
                                userIface.HadError() ||                               // the data connection recorded an error reported by the system
                                userIface.GetDatConCancelled())                       // the data connection was interrupted (either it did not open or it closed after an error reported by the server in response to LIST)
                            {
                                ok = FALSE;

                                DWORD err;
                                BOOL lowMem, noDataTrTimeout;
                                int sslErrorOccured;
                                userIface.GetError(&err, &lowMem, NULL, &noDataTrTimeout, &sslErrorOccured, NULL);

                                BOOL sslReuseErr = ReuseSSLSessionFailed &&
                                                   (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_TRANSIENTERROR ||
                                                    FTP_DIGIT_1(ftpReplyCode) == FTP_D1_ERROR);
                                if (sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslErrorOccured == SSLCONERR_CANRETRY ||
                                    sslReuseErr)
                                {                                                                       // we need to perform a reconnect
                                    CloseControlConnection(parent);                                     // close the current control connection
                                    lstrcpyn(retryMsgBuf, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 300); // set the error text for the reconnect wait window
                                    retryMsgAux = retryMsgBuf;
                                    sslErrReconnect = TRUE;
                                    fastSSLErrReconnect = sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslReuseErr;
                                }
                                else
                                {
                                    // display the "list can be incomplete" message; the user has not been warned yet
                                    lstrcpyn(errBuf, LoadStr(IDS_UNABLETOREADLIST), 900 + FTP_MAX_PATH);
                                    int len = (int)strlen(errBuf);
                                    BOOL systErr = FALSE;
                                    BOOL trModeHint = FTP_DIGIT_1(ftpReplyCode) == FTP_D1_TRANSIENTERROR &&
                                                      FTP_DIGIT_2(ftpReplyCode) == FTP_D2_CONNECTION;

                                    if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS ||
                                        FTP_DIGIT_2(ftpReplyCode) != FTP_D2_CONNECTION ||
                                        noDataTrTimeout || sslErrorOccured != SSLCONERR_NOERROR)
                                    { // if we do not have a description of the network error from the server, we settle for the system description
                                        systErr = TRUE;
                                        if (!trModeHint)
                                            trModeHint = err == WSAETIMEDOUT || sslErrorOccured != SSLCONERR_NOERROR;
                                        if (sslErrorOccured != SSLCONERR_NOERROR)
                                        {
                                            lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 700);
                                            strcat(replyBuf, "\r\n");
                                        }
                                        else
                                        {
                                            if (noDataTrTimeout)
                                                lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNODATATRTIMEOUT), 700);
                                            else
                                            {
                                                if (err != NO_ERROR)
                                                {
                                                    if (!dataConnection->GetProxyError(replyBuf, 700, NULL, 0, TRUE))
                                                        FTPGetErrorText(err, replyBuf, 700);
                                                }
                                                else
                                                {
                                                    if (userIface.GetDatConCancelled())
                                                        lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNOTOPENED), 700);
                                                    else
                                                        lstrcpyn(replyBuf, LoadStr(IDS_UNKNOWNERROR), 700);
                                                }
                                            }
                                        }
                                    }
                                    _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE,
                                                LoadStr(systErr ? (trModeHint ? IDS_UNABLETOREADLISTSUFFIX3 : IDS_UNABLETOREADLISTSUFFIX) : (trModeHint ? IDS_UNABLETOREADLISTSUFFIX4 : IDS_UNABLETOREADLISTSUFFIX2)),
                                                replyBuf);
                                    SalamanderGeneral->SalMessageBox(parent, errBuf,
                                                                     LoadStr(IDS_FTPERRORTITLE),
                                                                     MB_OK | MB_ICONEXCLAMATION);
                                }
                            }
                        }
                        if (!sslErrReconnect)
                            break; // aborted or a total success (everything listed - 'ok' == TRUE)
                    }
                }
                else // connection closed
                {
                    if (userIface.WasAborted()) // the user aborted the listing, which terminated the connection (for example on sunsolve.sun.com (Sun Unix) or ftp.chg.ru) - finish with an error (an incomplete listing)
                    {
                        ok = FALSE;   // do not display the "list can be incomplete" message; the user was warned during abort
                        if (canRetry) // take over the message for the message box that announces the connection interruption
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            if (ConnectionLostMsg != NULL)
                                SalamanderGeneral->Free(ConnectionLostMsg);
                            ConnectionLostMsg = SalamanderGeneral->DupStr(retryMsgBuf);
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                        break; // aborted
                    }

                    // error -> 'ok' remains FALSE; proceed to the next reconnect
                    ok = FALSE;
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" is allowed
                    else
                    {
                        *fatalError = TRUE; // fatal error
                        break;
                    }
                }
            }

            if (!ok) // the connection was interrupted; ask whether to reconnect
            {
                if (dataConnection->IsConnected())       // close the old "data connection" (in case the FD_CONNECT did not arrive)
                    dataConnection->CloseSocketEx(NULL); // shutdown (we will not learn the result)

                SetStartTime();
                BOOL startRet = StartControlConnection(parent, userBuf, userBufSize, TRUE, NULL, 0,
                                                       &attemptNum, retryMsgAux, FALSE, sslErrReconnect ? IDS_LISTCOMMANDERROR : -1,
                                                       fastSSLErrReconnect);
                retryMsgAux = NULL;
                if (totalAttemptNum != NULL)
                    *totalAttemptNum = attemptNum;
                if (startRet) // the connection has been restored
                {
                    if (pCertificate) // the control-connection certificate may have changed; pass any new one to the data connection
                        dataConnection->SetCertificate(pCertificate);

                    // change the path to 'path' (the path we are listing)
                    PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdChangeWorkingPath, NULL, path);
                    int ftpReplyCode;
                    if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                       &ftpReplyCode, replyBuf, 700, FALSE, TRUE, FALSE, &canRetry,
                                       retryMsgBuf, 300, NULL))
                    {
                        BOOL pathError = TRUE;
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // there is hope for success; better verify the path
                        {
                            if (GetCurrentWorkingPath(parent, cmdBuf, FTP_MAX_PATH, TRUE, &canRetry,
                                                      retryMsgBuf, 300))
                            {
                                if (strcmp(cmdBuf, path) == 0) // we have the desired working directory on the server
                                                               // (assumption: the server returns the same working-path string)
                                {
                                    pathError = FALSE;
                                    ok = TRUE; // successful reconnect; list again
                                }
                            }
                            else
                            {
                                pathError = FALSE; // error -> connection closed - 'ok' stays FALSE; proceed to the next reconnect
                                if (canRetry)
                                    retryMsgAux = retryMsgBuf; // "retry" is allowed
                                else
                                {
                                    *fatalError = TRUE; // fatal error
                                    break;
                                }
                            }
                        }

                        if (pathError) // display the path error and stop
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
                            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                             MB_OK | MB_ICONEXCLAMATION);
                            ret = FALSE; // stop - the path cannot be listed -> a path change is required

                            // if we did not find any accessible path, disconnect at this point,
                            // the issue is handled during connect (in CControlConnectionSocket::ChangeWorkingPath()),
                            // I am out of patience here ;-)

                            break;
                        }
                    }
                    else // error -> connection closed - 'ok' stays FALSE; proceed to the next reconnect
                    {
                        if (canRetry)
                            retryMsgAux = retryMsgBuf; // "retry" is allowed
                        else
                        {
                            *fatalError = TRUE; // fatal error
                            break;
                        }
                    }
                }
                else // reconnect failed - finish with an error (an incomplete listing)
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_UNABLETOREADLIST),
                                                     LoadStr(IDS_FTPERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
            }
        }
    }

    if (ret && !*fatalError) // there is neither a path error nor a fatal error
    {
        if (dataConnection->IsConnected()) // error: the "data connection" should have been closed long ago
        {
            TRACE_E("Unexpected situation in CControlConnectionSocket::ListWorkingPath(): data connection has left opened!");
            dataConnection->CloseSocketEx(NULL); // shutdown (we will not learn the result)
        }

        // take over the data from the "data connection"
        BOOL decomprErr;
        *allocatedListing = dataConnection->GiveData(allocatedListingLen, &decomprErr);

        if (decomprErr && ok)
        {
            ok = FALSE;

            // display the "list can be incomplete" message; the user has not been warned yet
            lstrcpyn(errBuf, LoadStr(IDS_UNABLETOREADLIST), 900 + FTP_MAX_PATH);
            int len = (int)strlen(errBuf);
            _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(IDS_UNABLETOREADLISTSUFFIX),
                        LoadStr(IDS_ERRDATACONDECOMPRERROR));
            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }

        *pathListingIsIncomplete = !ok; // TRUE in case of a failure/interruption/connection error
        *pathListingMayBeOutdated = FALSE;

        // store the date when the listing was created
        listingDate->Year = st.wYear;
        listingDate->Month = (BYTE)st.wMonth;
        listingDate->Day = (BYTE)st.wDay;
        *pathListingStartTime = lstStTime;

        char userTmp[USER_MAX_SIZE];
        if (forceRefresh &&  // treat a hard refresh as a sign of distrust in the path; drop it from the cache including
            !dontClearCache) // all subpaths (ignore useListingsCacheAux; it does not affect the distrust)
        {                    // called only once we have a replacement listing (until then the user will certainly prefer
                             // an outdated listing over none at all)
            HANDLES(EnterCriticalSection(&SocketCritSect));
            lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
            unsigned short portTmp = Port;
            lstrcpyn(userTmp, User, USER_MAX_SIZE);
            CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            ListingCache.RefreshOnPath(hostTmp, portTmp, userTmp, pathType, path);
        }

        if (ok) // we have a complete listing
        {
            if (!*pathListingIsBroken && useListingsCacheAux && *allocatedListing != NULL)
            { // the user wants to use the cache -> add the newly fetched listing to the cache
                HANDLES(EnterCriticalSection(&SocketCritSect));
                lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
                unsigned short portTmp = Port;
                lstrcpyn(userTmp, User, USER_MAX_SIZE);
                CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
                BOOL isFTPS = EncryptControlConnection == 1;
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                ListingCache.AddOrUpdatePathListing(hostTmp, portTmp, userTmp, pathType, path,
                                                    listCmd, isFTPS, *allocatedListing,
                                                    *allocatedListingLen, listingDate,
                                                    *pathListingStartTime);
            }
        }
        else // failure/interruption/connection error = return at least what we have (the user already knows that "list can be incomplete")
        {
            if (*allocatedListing != NULL)
            { // the buffer does not contain a complete listing -> trim it at the last line ending (CRLF or LF) to make it easier to work with
                char* start = *allocatedListing;
                char* s = start + *allocatedListingLen;
                while (s > start && *(s - 1) != '\n')
                    s--;
                if (s < start + *allocatedListingLen) // there is a place to write the null terminator (just for easier debugging)
                    *s = 0;                           // either at the start of the buffer or after the last LF
                *allocatedListingLen = (int)(s - start);
            }
        }
    }
    if (dataConnection != NULL) // release and possibly close the "data connection"
    {
        if (dataConnection->IsConnected())       // close the "data connection"; the system attempts a "graceful"
            dataConnection->CloseSocketEx(NULL); // shutdown (we will not learn the result)
        DeleteSocket(dataConnection);
    }
    if (*fatalError)
        ret = FALSE; // we certainly will not return success on a fatal error
    return ret;
}

class CFinishingKeepAliveUserIface : public CSendCmdUserIfaceAbstract
{
protected:
    CWaitWindow* WaitWnd;
    HANDLE FinishedEvent;

public:
    CFinishingKeepAliveUserIface(CWaitWindow* waitWnd, HANDLE finishedEvent)
    {
        WaitWnd = waitWnd;
        FinishedEvent = finishedEvent;
    }

    virtual BOOL GetWindowClosePressed() { return WaitWnd->GetWindowClosePressed(); }
    virtual HANDLE GetFinishedEvent() { return FinishedEvent; }

    // the remaining methods are not used
    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText) {}
    virtual void BeforeAborting() {}
    virtual void AfterWrite(BOOL aborting, DWORD showTime) {}
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort) { return FALSE; }
    virtual void SendingFinished() {}
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize) { return FALSE; }
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize) {}
    virtual void CancelDataCon() {}
    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout) { return FALSE; }
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout) {}
    virtual void HandleDataConTimeout(DWORD* start) {}
    virtual void HandleESCWhenWaitingForFinish(HWND parent) {}
};

void CControlConnectionSocket::WaitForEndOfKeepAlive(HWND parent, int waitWndTime)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::WaitForEndOfKeepAlive(, %d)", waitWndTime);

    DWORD startTime = GetTickCount(); // operation start time

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::WaitForEndOfKeepAlive(): from section SocketCritSect!");
#endif

    if (!KeepAliveEnabled && KeepAliveMode != kamNone)
        TRACE_E("CControlConnectionSocket::WaitForEndOfKeepAlive(): Keep-Alive is disabled, but Mode == " << (int)KeepAliveMode);

    if (KeepAliveEnabled &&
        (KeepAliveMode == kamProcessing ||               // a keep-alive command is running; we must wait for it to finish
         KeepAliveMode == kamWaitingForEndOfProcessing)) // we are already waiting for completion (should not happen)
    {
        KeepAliveMode = kamWaitingForEndOfProcessing;
        HANDLE finishedEvent = KeepAliveFinishedEvent;
        int logUID = LogUID;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        // show a wait window to indicate we are waiting for the keep-alive command to finish
        CWaitWindow waitWnd(parent, TRUE);
        waitWnd.SetText(LoadStr(IDS_FINISHINGKEEPALIVECMD));
        DWORD start = GetTickCount();
        DWORD waitTime = start - startTime;
        waitWnd.Create(waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);

        // wait for the keep-alive command to finish or be interrupted (ESC/timeout)
        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
        if (serverTimeout < 1000)
            serverTimeout = 1000; // at least one second
        CFinishingKeepAliveUserIface userIface(&waitWnd, finishedEvent);
        BOOL wait = TRUE;
        while (wait)
        {
            CControlConnectionSocketEvent event;
            DWORD data1, data2;
            DWORD now = GetTickCount();
            if (now - start > (DWORD)serverTimeout)
                now = start + (DWORD)serverTimeout;
            WaitForEventOrESC(parent, &event, &data1, &data2, serverTimeout - (now - start),
                              NULL, &userIface, TRUE);
            switch (event)
            {
            case ccsevESC:
            {
                waitWnd.Show(FALSE);
                if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_KEEPALIVECMDESC),
                                                     LoadStr(IDS_FTPPLUGINTITLE),
                                                     MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                         MB_ICONQUESTION) == IDYES)
                { // cancel
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE);
                    ReleaseKeepAlive(); // release the keep-alive
                    CloseSocket(NULL);  // close the connection
                    Logs.SetIsConnected(logUID, IsConnected());
                    Logs.RefreshListOfLogsInLogsDlg(); // "connection inactive" notification
                    wait = FALSE;
                }
                else
                {
                    SalamanderGeneral->WaitForESCRelease(); // a measure to keep the next action from being interrupted after every ESC in the previous message box
                    waitWnd.Show(TRUE);
                }
                break;
            }

            case ccsevTimeout:
            {
                BOOL isTimeout = TRUE;

                HANDLES(EnterCriticalSection(&SocketCritSect));
                if (KeepAliveDataCon != NULL)
                {
                    BOOL trFinished;
                    if (KeepAliveDataCon->IsTransfering(&trFinished))
                    { // waiting for data, so this is not a timeout
                        start = GetTickCount();
                        isTimeout = FALSE;
                    }
                    else
                    {
                        if (trFinished)
                        {
                            start = KeepAliveDataCon->GetSocketCloseTime();
                            isTimeout = (GetTickCount() - start) >= (DWORD)serverTimeout; // the timeout is measured from the connection closing (the moment since when the server can react and also learns about the closure)
                        }
                        // else isTimeout = TRUE;  // the connection has not opened yet -> treat it as a timeout
                    }
                }
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                if (isTimeout)
                {
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGKEEPALIVECMDTIMEOUT), -1, TRUE);
                    ReleaseKeepAlive(); // release the keep-alive
                    CloseSocket(NULL);  // close the connection
                    Logs.SetIsConnected(logUID, IsConnected());
                    Logs.RefreshListOfLogsInLogsDlg(); // "connection inactive" notification
                    wait = FALSE;
                }
                break;
            }

            case ccsevNewBytesRead:
                break; // ignore it (at worst some old event; after this method we write the command anyway and only then the server replies)

            case ccsevClosed: // connection closed; just wrap up keep-alive and let someone else handle it
            {
                ReleaseKeepAlive();
                AddEvent(ccsevClosed, data1, data2);
                wait = FALSE;
                break;
            }

            case ccsevUserIfaceFinished:
                wait = FALSE;
                break; // the keep-alive command has finished

            default:
            {
                TRACE_E("CControlConnectionSocket::WaitForEndOfKeepAlive: Unexpected event (" << (int)event << ").");
                break;
            }
            }
        }
        waitWnd.Destroy();

        // the keep-alive command has already finished or was interrupted (ESC/timeout)
        HANDLES(EnterCriticalSection(&SocketCritSect));
        KeepAliveMode = kamForbidden;
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
    else
    {
        if (KeepAliveEnabled)
        {
            BOOL deleteTimer = FALSE;
            int uid;
            if (KeepAliveMode == kamWaiting)
            {
                deleteTimer = TRUE;
                uid = UID;
            }
            KeepAliveMode = kamForbidden;
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            if (deleteTimer)
            {
                // in mode 'kamForbidden' the keep-alive timer makes no sense (if it times out,
                // it is simply ignored); delete it
                SocketsThread->DeleteTimer(uid, CTRLCON_KEEPALIVE_TIMERID);
            }
        }
        else
            HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
}

void CControlConnectionSocket::SetupKeepAliveTimer(BOOL immediate)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::SetupKeepAliveTimer(%d)", immediate);

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::SetupKeepAliveTimer(): from section SocketCritSect!");
#endif

    if (!KeepAliveEnabled && KeepAliveMode != kamNone)
        TRACE_E("CControlConnectionSocket::SetupKeepAliveTimer(): Keep-Alive is disabled, but Mode == " << (int)KeepAliveMode);
    BOOL timer = FALSE;
    int msg;
    int uid;
    DWORD ti;
    if (KeepAliveEnabled && KeepAliveMode == kamForbidden) // called after completing a normal command
    {
        KeepAliveMode = kamWaiting;
        timer = TRUE;
        msg = Msg;
        uid = UID;
        KeepAliveStart = GetTickCount();                                   // time of the last normal command executed in the "control connection"
        ti = KeepAliveStart + (immediate ? 0 : KeepAliveSendEvery * 1000); // time when the first keep-alive command should be sent
    }
    else
    {
        if (KeepAliveMode != kamNone)
            TRACE_E("CControlConnectionSocket::SetupKeepAliveTimer(): unexpected Mode == " << (int)KeepAliveMode);
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (timer) // we need to arm the keep-alive timer
        SocketsThread->AddTimer(msg, uid, ti, CTRLCON_KEEPALIVE_TIMERID, NULL);
}

void CControlConnectionSocket::SetupNextKeepAliveTimer()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::SetupNextKeepAliveTimer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::SetupNextKeepAliveTimer(): from section SocketCritSect!");
#endif

    if (!KeepAliveCmdAllBytesWritten)
    { // this should never happen because the server's reply arrives only after the complete command is written
        // of the command (the command is always written at once; it is just a few bytes)
        TRACE_E("Unexpected situation in CControlConnectionSocket::SetupNextKeepAliveTimer(): KeepAliveCmdAllBytesWritten==FALSE!");
        KeepAliveCmdAllBytesWritten = TRUE;
    }

    if (KeepAliveDataCon != NULL || KeepAliveDataConState != kadcsNone)
        TRACE_E("Unexpected situation in CControlConnectionSocket::SetupNextKeepAliveTimer(): KeepAliveDataCon!=NULL or KeepAliveDataConState!=kadcsNone!");

    BOOL timer = FALSE;
    int msg;
    int uid;
    DWORD ti;
    if (KeepAliveMode == kamProcessing) // the keep-alive finished normally; decide whether to set the keep-alive timer again
    {
        ti = GetTickCount() + KeepAliveSendEvery * 1000; // time when the next keep-alive command should be sent
        if ((int)((ti - KeepAliveStart) / 60000) < KeepAliveStopAfter)
        {
            KeepAliveMode = kamWaiting;
            timer = TRUE;
            msg = Msg;
            uid = UID;
        }
        else
        {
            KeepAliveMode = kamNone;                                         // we should no longer perform keep-alive (there is no point in protecting the connection anymore)
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGKASTOPPED), -1, TRUE); // notify the user that the keep-alive mode has stopped
        }
    }
    else
    {
        if (KeepAliveMode == kamWaitingForEndOfProcessing) // the main thread is waiting for the keep-alive command to finish
        {
            SetEvent(KeepAliveFinishedEvent);
        }
        else
        {
            if (KeepAliveMode != kamNone) // kamNone = ReleaseKeepAlive() was called
                TRACE_E("CControlConnectionSocket::SetupNextKeepAliveTimer(): unexpected Mode == " << (int)KeepAliveMode);
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (timer) // we need to arm the keep-alive timer
        SocketsThread->AddTimer(msg, uid, ti, CTRLCON_KEEPALIVE_TIMERID, NULL);
}

void CControlConnectionSocket::ReleaseKeepAlive()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::ReleaseKeepAlive()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::ReleaseKeepAlive(): from section SocketCritSect!");
#endif

    if (KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing)
        SetEvent(KeepAliveFinishedEvent); // let the main thread continue
    BOOL deleteTimer = FALSE;
    int uid;
    if (KeepAliveMode == kamWaiting)
    {
        deleteTimer = TRUE;
        uid = UID;
    }
    KeepAliveMode = kamNone; // keep-alive reinitialization
    KeepAliveCmdAllBytesWritten = TRUE;
    CKeepAliveDataConSocket* closeDataCon = KeepAliveDataCon;
    KeepAliveDataCon = NULL;
    KeepAliveDataConState = kadcsNone;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    // if the "data connection" is open, close it; it certainly will not be needed now
    if (closeDataCon != NULL)
    {
        if (closeDataCon->IsConnected())       // close the "data connection"; the system attempts a "graceful"
            closeDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
        DeleteSocket(closeDataCon);            // release the "data connection" through a SocketsThread method call
    }

    if (deleteTimer)
    {
        // in mode 'kamNone' the keep-alive timer makes no sense (if it times out,
        // it is simply ignored); delete it
        SocketsThread->DeleteTimer(uid, CTRLCON_KEEPALIVE_TIMERID);
    }
}

void CControlConnectionSocket::PostMsgToCtrlCon(int msgID, void* msgParam)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::PostMsgToCtrlCon()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int msg = Msg;
    int uid = UID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    SocketsThread->PostSocketMessage(msg, uid, msgID, msgParam);
}

void CControlConnectionSocket::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::ReceiveTimer(%u,)", id);
    if (id == CTRLCON_KEEPALIVE_TIMERID)
    {
        BOOL sendKACmd = FALSE;
        int cmd;
        HANDLES(EnterCriticalSection(&SocketCritSect));
        int logUID = LogUID;
        BOOL usePassiveModeAux;
        if (KeepAliveEnabled && KeepAliveMode == kamWaiting) // nothing prevents sending the keep-alive command
        {
            KeepAliveMode = kamProcessing;
            ResetEvent(KeepAliveFinishedEvent); // prepare the event so the main thread can be blocked until the keep-alive command finishes
            sendKACmd = TRUE;
            usePassiveModeAux = UsePassiveMode;

            if (KeepAliveCommand == 2 /* NLST */ || KeepAliveCommand == 3 /* LIST */)
            {
                // allocate the object for the "data connection"
                if (KeepAliveDataCon != NULL)
                    TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveTimer(): KeepAliveDataCon is not NULL!");
                CFTPProxyForDataCon* dataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
                BOOL dataConProxyServerOK = ProxyServer == NULL || dataConProxyServer != NULL;
                KeepAliveDataCon = dataConProxyServerOK ? new CKeepAliveDataConSocket(this, dataConProxyServer, EncryptDataConnection, pCertificate) : NULL;
                if (KeepAliveDataCon == NULL)
                {
                    if (dataConProxyServer != NULL)
                        delete dataConProxyServer;
                    if (dataConProxyServerOK)
                        TRACE_E(LOW_MEMORY);
                    KeepAliveCommand = 0; // send "NOOP" instead
                }
                else
                    KeepAliveDataConState = usePassiveModeAux ? kadcsWaitForPassiveReply : kadcsWaitForListen;
            }
            cmd = KeepAliveCommand;
        }
        CKeepAliveDataConSocket* keepAliveDataConAux = KeepAliveDataCon;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (sendKACmd)
        {
            // sending the keep-alive command (no delays; we must not wait for anything)
            char ftpCmd[200];
            ftpCmd[0] = 0;
            BOOL waitForListen = FALSE;
            switch (cmd)
            {
            case 0: // NOOP
            {
                PrepareFTPCommand(ftpCmd, 200, NULL, 0, ftpcmdNoOperation, NULL);
                break;
            }

            case 1: // PWD
            {
                PrepareFTPCommand(ftpCmd, 200, NULL, 0, ftpcmdPrintWorkingPath, NULL);
                break;
            }

            case 2: // NLST
            case 3: // LIST
            {
                if (usePassiveModeAux) // passive mode of the "data connection"
                {
                    PrepareFTPCommand(ftpCmd, 200, NULL, 0, ftpcmdPassive, NULL);
                }
                else // active mode of the "data connection"
                {
                    DWORD localIP;
                    GetLocalIP(&localIP, NULL);   // should not be able to fail
                    unsigned short localPort = 0; // listen on any port
                    DWORD error;
                    keepAliveDataConAux->SetActive(logUID);
                    BOOL listenError;
                    if (!keepAliveDataConAux->OpenForListeningWithProxy(localIP, localPort, &listenError, &error))
                    { // failed to open the "listen" socket for receiving the data connection from
                        // the server (a local operation, this should almost never happen) and it can also be
                        // an error when connecting to the proxy server
                        Logs.LogMessage(logUID, LoadStr(listenError ? IDS_LOGMSGOPENACTDATACONERROR : IDS_LOGMSGOPENACTDATACONERROR2), -1, TRUE);
                    }
                    else
                        waitForListen = TRUE;
                }
                break;
            }

            default:
            {
                TRACE_E("CControlConnectionSocket::ReceiveTimer(): unknown keep-alive command!");
                ftpCmd[0] = 0;
                break;
            }
            }

            if (ftpCmd[0] != 0 || waitForListen)
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGKEEPALIVE), -1, TRUE);
            if (ftpCmd[0] != 0)
                SendKeepAliveCmd(logUID, ftpCmd); // send the keep-alive command
            else
            {
                if (!waitForListen)
                    ReleaseKeepAlive(); // nothing was sent (continuing keep-alive makes no sense), cancel keep-alive
            }
        }
    }
}

void CControlConnectionSocket::ReceivePostMessage(DWORD id, void* param)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::ReceivePostMessage(%u,)", id);
    switch (id)
    {
    case CTRLCON_KAPOSTSETUPNEXT: // the keep-alive command's "data connection" has just finished; the server already sent the listing-end reply, so call SetupNextKeepAliveTimer()
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        BOOL call = (KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing); // nothing unexpected happened?
        CKeepAliveDataConSocket* closeDataCon = KeepAliveDataCon;
        if (call)
        {
            KeepAliveDataCon = NULL;
            KeepAliveDataConState = kadcsNone;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (call)
        {
            if (closeDataCon != NULL)
                DeleteSocket(closeDataCon);
            SetupNextKeepAliveTimer();
        }
        break;
    }

    case CTRLCON_LISTENFORCON: // message about opening the "listen" port (on the proxy server)
    {
        AddEvent(ccsevListenForCon, (DWORD)(DWORD_PTR)param, 0);
        break;
    }

    case CTRLCON_KALISTENFORCON: // keep-alive: message about opening the "listen" port (on the proxy server)
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if ((KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing) &&
            KeepAliveDataConState == kadcsWaitForListen)
        {
            CKeepAliveDataConSocket* kaDataConnection = KeepAliveDataCon;
            int logUID = LogUID; // log UID of this connection
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            if ((int)(INT_PTR)param == kaDataConnection->GetUID()) // process the message only if it is for our data connection
            {
                DWORD listenOnIP;
                unsigned short listenOnPort;
                char buf[300];
                char errBuf[500];
                if (!kaDataConnection->GetListenIPAndPort(&listenOnIP, &listenOnPort)) // "listen" error
                {
                    if (kaDataConnection->GetProxyError(buf, 300, NULL, 0, TRUE))
                    { // log the error
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), buf);
                        Logs.LogMessage(logUID, errBuf, -1, TRUE);
                    }
                    ReleaseKeepAlive(); // we are finishing...
                }
                else // success, send the "PORT" command
                {
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    KeepAliveDataConState = kadcsWaitForSetPortReply;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    PrepareFTPCommand(buf, 300, NULL, 0, ftpcmdSetPort, NULL, listenOnIP, listenOnPort);
                    SendKeepAliveCmd(logUID, buf);
                }
            }
        }
        else
            HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }
    }
}

BOOL CControlConnectionSocket::InitOperation(CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::InitOperation()");
    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = oper->SetConnection(ProxyServer, Host, Port, User, Password, Account,
                                   InitFTPCommands, UsePassiveMode,
                                   UseLIST_aCommand ? LIST_a_CMD_TEXT : ListCommand,
                                   ServerIP, ServerSystem, ServerFirstReply,
                                   UseListingsCache, HostIP);
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

//
// ****************************************************************************
// CListingCacheItem
//

CListingCacheItem::CListingCacheItem(const char* host, unsigned short port, const char* user,
                                     const char* path, const char* listCmd, BOOL isFTPS,
                                     const char* cachedListing, int cachedListingLen,
                                     const CFTPDate& cachedListingDate,
                                     DWORD cachedListingStartTime, CFTPServerPathType pathType)
{
    // copy the data
    BOOL err = (host == NULL || path == NULL || listCmd == NULL);
    Host = SalamanderGeneral->DupStr(host);
    Port = port;
    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    User = SalamanderGeneral->DupStr(user); // if it is NULL it remains NULL, but 'err' does not change
    Path = SalamanderGeneral->DupStr(path);
    ListCmd = SalamanderGeneral->DupStr(listCmd);
    IsFTPS = isFTPS;
    CachedListing = (char*)malloc(cachedListingLen + 1); // +1 to handle a listing with zero length
    if (CachedListing != NULL && cachedListing != NULL)
    {
        memcpy(CachedListing, cachedListing, cachedListingLen);
        CachedListing[cachedListingLen] = 0; // once it is allocated there, make it null-terminated for debugging purposes
    }
    else
        err = TRUE;
    CachedListingLen = cachedListingLen;
    CachedListingDate = cachedListingDate;
    CachedListingStartTime = cachedListingStartTime;
    PathType = pathType;

    // on error free and null the data
    if (err)
    {
        if (User != NULL)
            SalamanderGeneral->Free(User);
        if (Host != NULL)
            SalamanderGeneral->Free(Host);
        if (Path != NULL)
            SalamanderGeneral->Free(Path);
        if (ListCmd != NULL)
            SalamanderGeneral->Free(ListCmd);
        if (CachedListing != NULL)
        {
            memset(CachedListing, 0, CachedListingLen); // it may be sensitive data, so wipe it just in case
            free(CachedListing);
        }
        User = NULL;
        Host = NULL;
        Path = NULL;
        ListCmd = NULL;
        CachedListing = NULL;
    }
    UserLength = FTPGetUserLength(User);
}

CListingCacheItem::~CListingCacheItem()
{
    if (User != NULL)
        SalamanderGeneral->Free(User);
    if (Host != NULL)
        SalamanderGeneral->Free(Host);
    if (Path != NULL)
        SalamanderGeneral->Free(Path);
    if (ListCmd != NULL)
        SalamanderGeneral->Free(ListCmd);
    if (CachedListing != NULL)
    {
        memset(CachedListing, 0, CachedListingLen); // it may be sensitive data, so wipe it just in case
        free(CachedListing);
    }
}

//
// ****************************************************************************
// CListingCache
//

CListingCache::CListingCache() : Cache(100, 50), TotalCacheSize(0, 0)
{
    HANDLES(InitializeCriticalSection(&CacheCritSect));
}

CListingCache::~CListingCache()
{
#ifdef _DEBUG
    int i;
    for (i = 0; i < Cache.Count; i++)
        TotalCacheSize -= CQuadWord(Cache[i]->CachedListingLen, 0);
    if (TotalCacheSize != CQuadWord(0, 0))
        TRACE_E("CListingCache::~CListingCache(): TotalCacheSize is not zero when cache is empty!");
#endif
    HANDLES(DeleteCriticalSection(&CacheCritSect));
}

BOOL CListingCache::Find(const char* host, unsigned short port, const char* user,
                         CFTPServerPathType pathType, const char* path, const char* listCmd,
                         BOOL isFTPS, int* index)
{
    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    int i;
    for (i = 0; i < Cache.Count; i++)
    {
        CListingCacheItem* item = Cache[i];
        if (SalamanderGeneral->StrICmp(host, item->Host) == 0 &&
            (user == NULL && item->User == NULL ||
             item->User != NULL && user != NULL && strcmp(user, item->User) == 0) &&
            port == item->Port &&
            FTPIsTheSameServerPath(pathType, path, item->Path) &&
            isFTPS == item->IsFTPS &&
            SalamanderGeneral->StrICmp(listCmd, item->ListCmd) == 0)
        {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CListingCache::GetPathListing(const char* host, unsigned short port, const char* user,
                                   CFTPServerPathType pathType, char* path, int pathBufSize,
                                   const char* listCmd, BOOL isFTPS, char** cachedListing,
                                   int* cachedListingLen, CFTPDate* cachedListingDate,
                                   DWORD* cachedListingStartTime)
{
    HANDLES(EnterCriticalSection(&CacheCritSect));

    BOOL found = FALSE;
    int index;
    if (Find(host, port, user, pathType, path, listCmd, isFTPS, &index)) // update the cache item
    {
        found = TRUE;
        CListingCacheItem* item = Cache[index];
        *cachedListing = (char*)malloc(item->CachedListingLen + 1); // +1 to handle a listing with zero length
        if (*cachedListing != NULL)
        {
            memcpy(*cachedListing, item->CachedListing, item->CachedListingLen);
            (*cachedListing)[item->CachedListingLen] = 0; // once it is allocated there, make it null-terminated for debugging purposes
            *cachedListingLen = item->CachedListingLen;
        }
        else
            TRACE_E(LOW_MEMORY); // *cachedListingLen stays 0 and the caller handles the memory error
        *cachedListingDate = item->CachedListingDate;
        *cachedListingStartTime = item->CachedListingStartTime;
        lstrcpyn(path, item->Path, pathBufSize);
    }

    HANDLES(LeaveCriticalSection(&CacheCritSect));
    return found;
}

void CListingCache::AddOrUpdatePathListing(const char* host, unsigned short port, const char* user,
                                           CFTPServerPathType pathType, const char* path,
                                           const char* listCmd, BOOL isFTPS,
                                           const char* cachedListing, int cachedListingLen,
                                           const CFTPDate* cachedListingDate,
                                           DWORD cachedListingStartTime)
{
    HANDLES(EnterCriticalSection(&CacheCritSect));

    // if the item is already in the cache, delete it (not worth fiddling with updating its data)
    int index;
    if (Find(host, port, user, pathType, path, listCmd, isFTPS, &index))
    {
        TotalCacheSize -= CQuadWord(Cache[index]->CachedListingLen, 0);
        Cache.Delete(index);
        if (!Cache.IsGood())
            Cache.ResetState();
    }

    // insert a new item into the cache
    CListingCacheItem* item = new CListingCacheItem(host, port, user, path, listCmd, isFTPS,
                                                    cachedListing, cachedListingLen,
                                                    *cachedListingDate,
                                                    cachedListingStartTime, pathType);
    if (item != NULL && item->IsGood())
    {
        Cache.Add(item);
        if (Cache.IsGood())
        {
            TotalCacheSize += CQuadWord(item->CachedListingLen, 0);
            item = NULL; // once it is inserted successfully, it must not be freed later in this method

            // if there are too many items in the cache, remove them starting from the oldest, at least
            // keep the most recently added item in the cache
            int count = 0; // how many items need to be removed (delete them at once, otherwise complexity is O(N*N))
            while (Cache.Count > count + 1 && TotalCacheSize > Config.CacheMaxSize)
                TotalCacheSize -= CQuadWord(Cache[count++]->CachedListingLen, 0);
            if (count > 0)
            {
                Cache.Delete(0, count);
                if (!Cache.IsGood())
                    Cache.ResetState();
            }
        }
        else
            Cache.ResetState();
    }
    if (item != NULL)
        delete item;

    HANDLES(LeaveCriticalSection(&CacheCritSect));
}

void CListingCache::RefreshOnPath(const char* host, unsigned short port, const char* user,
                                  CFTPServerPathType pathType, const char* path, BOOL ignorePath)
{
    HANDLES(EnterCriticalSection(&CacheCritSect));

    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    int delIndex = 0; // variables for deleting in blocks (shifting the array is O(N*N), so we optimize)
    int delCount = 0;
    int i;
    for (i = 0; i < Cache.Count; i++)
    {
        CListingCacheItem* item = Cache[i];
        if (SalamanderGeneral->StrICmp(host, item->Host) == 0 &&
            (user == NULL && item->User == NULL ||
             item->User != NULL && user != NULL && strcmp(user, item->User) == 0) &&
            port == item->Port &&
            (ignorePath || FTPIsPrefixOfServerPath(pathType, path, item->Path, FALSE))) // consider the path including its subpaths
        {
            // remove the item from the cache
            TotalCacheSize -= CQuadWord(item->CachedListingLen, 0);
            if (delIndex + delCount == i)
                delCount++; // contiguous with the block being deleted; just extend it
            else            // we must create a new block and delete the previous one
            {
                if (delCount > 0)
                {
                    Cache.Delete(delIndex, delCount);
                    if (!Cache.IsGood())
                        Cache.ResetState();
                    i -= delCount; // adjust the index after deleting the previous block (it has to lie entirely before 'i')
                }
                delIndex = i;
                delCount = 1;
            }
        }
    }
    if (delCount > 0)
    {
        Cache.Delete(delIndex, delCount);
        if (!Cache.IsGood())
            Cache.ResetState();
    }

    HANDLES(LeaveCriticalSection(&CacheCritSect));
}

void CListingCache::AcceptChangeOnPathNotification(const char* userPart, BOOL includingSubdirs)
{
    char buf[FTP_USERPART_SIZE];
    const char* pathPart = NULL;
    char *user, *host, *portStr, *pathStr;
    int port;
    int userLength = -1;

    HANDLES(EnterCriticalSection(&CacheCritSect));

    int delIndex = 0; // variables for deleting in blocks (shifting the array is O(N*N), so we optimize)
    int delCount = 0;
    int i;
    for (i = 0; i < Cache.Count; i++)
    {
        CListingCacheItem* item = Cache[i];
        if (userLength == -1 || userLength != item->UserLength)
        {
            userLength = item->UserLength;
            lstrcpyn(buf, userPart, FTP_USERPART_SIZE);
            FTPSplitPath(buf, &user, NULL, &host, &portStr, &pathStr, NULL, userLength);
            if (pathStr != NULL && pathStr > buf)
                pathPart = userPart + (pathStr - buf) - 1;
            port = portStr != NULL ? atoi(portStr) : IPPORT_FTP;
            if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
                user = NULL;
            if (host == NULL || pathPart == NULL)
            { // this may still be just a coincidence; we need to try it with an unknown username length
                lstrcpyn(buf, userPart, FTP_USERPART_SIZE);
                FTPSplitPath(buf, &user, NULL, &host, &portStr, &pathStr, NULL, 0);
                if (pathStr != NULL && pathStr > buf)
                    pathPart = userPart + (pathStr - buf) - 1;
                port = portStr != NULL ? atoi(portStr) : IPPORT_FTP;
                if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
                    user = NULL;
                if (host == NULL || pathPart == NULL)
                {
                    TRACE_E("CListingCache::AcceptChangeOnPathNotification(): invalid (or relative) path received: " << userPart);
                    HANDLES(LeaveCriticalSection(&CacheCritSect));
                    return; // there are no such items in the cache; nothing to do
                }
            }
        }
        if (SalamanderGeneral->StrICmp(host, item->Host) == 0 &&
            (user == NULL && item->User == NULL ||
             item->User != NULL && user != NULL && strcmp(user, item->User) == 0) &&
            port == item->Port &&
            FTPIsPrefixOfServerPath(item->PathType, FTPGetLocalPath(pathPart, item->PathType),
                                    item->Path, !includingSubdirs))
        { // the item matches the changed path or its subdirectory, so remove it from the cache
            TotalCacheSize -= CQuadWord(item->CachedListingLen, 0);
            if (delIndex + delCount == i)
                delCount++; // contiguous with the block being deleted; just extend it
            else            // we must create a new block and delete the previous one
            {
                if (delCount > 0)
                {
                    Cache.Delete(delIndex, delCount);
                    if (!Cache.IsGood())
                        Cache.ResetState();
                    i -= delCount; // adjust the index after deleting the previous block (it has to lie entirely before 'i')
                }
                delIndex = i;
                delCount = 1;
            }
        }
    }
    if (delCount > 0)
    {
        Cache.Delete(delIndex, delCount);
        if (!Cache.IsGood())
            Cache.ResetState();
    }

    HANDLES(LeaveCriticalSection(&CacheCritSect));
}
