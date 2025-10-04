// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CControlConnectionSocket
//

void CControlConnectionSocket::DownloadOneFile(HWND parent, const char* fileName,
                                               CQuadWord const& fileSizeInBytes,
                                               BOOL asciiMode, const char* workPath,
                                               const char* tgtFileName, BOOL* newFileCreated,
                                               BOOL* newFileIncomplete, CQuadWord* newFileSize,
                                               int* totalAttemptNum, int panel, BOOL notInPanel,
                                               char* userBuf, int userBufSize)
{
    CALL_STACK_MESSAGE10("CControlConnectionSocket::DownloadOneFile(, %s, , %d, %s, %s, , , , %d, %d, %d, %s, %d)",
                         fileName, asciiMode, workPath, tgtFileName, *totalAttemptNum, panel,
                         notInPanel, userBuf, userBufSize);

    *newFileCreated = FALSE;
    *newFileIncomplete = FALSE;
    newFileSize->Set(0, 0);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL usePassiveModeAux = UsePassiveMode;
    int logUID = LogUID; // log UID of this connection
    char errBuf[900 + FTP_MAX_PATH];
    char hostBuf[HOST_MAX_SIZE];
    char userBuffer[USER_MAX_SIZE];
    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
    unsigned short portBuf = Port;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    int lockedFileUID; // UID of the locked file (in FTPOpenedFiles) - we lock the file for download
    if (FTPOpenedFiles.OpenFile(userBuffer, hostBuf, portBuf, workPath,
                                GetFTPServerPathType(workPath),
                                fileName, &lockedFileUID, ffatRead))
    { // the file on the server is not open yet, we can work with it, allocate an object for the "data connection"
        HANDLES(EnterCriticalSection(&SocketCritSect));
        CFTPProxyForDataCon* dataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
        BOOL dataConProxyServerOK = ProxyServer == NULL || dataConProxyServer != NULL;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        CDataConnectionSocket* dataConnection = dataConProxyServerOK ? new CDataConnectionSocket(TRUE, dataConProxyServer, EncryptDataConnection, pCertificate, CompressData, this) : NULL;
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
        }
        else
        {
            char cmdBuf[50 + FTP_MAX_PATH];
            char logBuf[50 + FTP_MAX_PATH];
            const char* retryMsgAux = NULL;
            BOOL canRetry = FALSE;
            char retryMsgBuf[300];
            BOOL reconnected = FALSE;
            char replyBuf[700];
            BOOL setStartTimeIfConnected = TRUE;
            BOOL sslErrReconnect = FALSE;     // TRUE = reconnect because of SSL errors
            BOOL fastSSLErrReconnect = FALSE; // TRUE = server certificate changed, an immediate reconnect is desirable (without 20 seconds of waiting)
            while (ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                                     userBuf, userBufSize, &reconnected,
                                     setStartTimeIfConnected, totalAttemptNum,
                                     retryMsgAux, NULL,
                                     sslErrReconnect ? IDS_DOWNLOADONEFILEERROR : -1,
                                     fastSSLErrReconnect)) // if needed, we reconnect
            {
                if (pCertificate) // the control connection certificate may have changed; pass any new one to the data connection
                    dataConnection->SetCertificate(pCertificate);
                sslErrReconnect = FALSE;
                fastSSLErrReconnect = FALSE;
                setStartTimeIfConnected = TRUE;
                BOOL run = FALSE;
                BOOL ok = TRUE;
                char newPath[FTP_MAX_PATH];
                BOOL needChangeDir = reconnected; // after a reconnect we try to set the working directory again
                if (!reconnected)                 // we have been connected for a while, check whether the working directory matches 'workPath'
                {
                    // use the cache; in normal cases the path should be there
                    ok = GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300);
                    if (!ok && canRetry) // "retry" is allowed
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    if (ok && strcmp(newPath, workPath) != 0) // the working directory on the server differs - change required
                        needChangeDir = TRUE;                 // (assumption: the server always returns the same working path string)
                }
                if (ok && needChangeDir) // if we need to change the working directory
                {
                    BOOL success;
                    // SendChangeWorkingPath() contains ReconnectIfNeeded() when the connection drops; luckily that
                    // does not matter, because the code preceding this call runs only if no reconnect happened
                    // - "if (!reconnected)" - if a reconnect occurs, both code paths are identical
                    ok = SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, workPath,
                                               userBuf, userBufSize, &success,
                                               replyBuf, 700, NULL,
                                               totalAttemptNum, NULL, TRUE, NULL);
                    if (ok && !success && workPath[0] != 0) // send succeeded but the server reports an error (+ignore errors with an empty path) -> file cannot be
                    {                                       // downloaded (it is on the current path in the panel)
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), workPath, replyBuf);
                        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        ok = FALSE;
                    }
                }

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
                            FTPGetIPAndPortFromReply(replyBuf, -1, &ip, &port)) // successfully obtained IP+port
                        {
                            dataConnection->SetPassive(ip, port, logUID);
                            dataConnection->PassiveConnect(NULL); // first attempt; the result does not matter (checked later)
                        }
                        else // passive mode is not supported
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            UsePassiveMode = usePassiveModeAux = FALSE; // we will try again in the active mode (PORT)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));

                            Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);
                        }
                    }
                    else // error -> connection closed
                    {
                        ok = FALSE;
                        if (canRetry) // "retry" is allowed
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (ok && !usePassiveModeAux) // active mode (PORT)
                {
                    DWORD localIP;
                    GetLocalIP(&localIP, NULL);   // this should not be able to return an error
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
                                            retryMsgBuf, 300, NULL)) // ignore the server reply; the error shows later (timeout when listing)
                        {                                            // error -> connection closed
                            ok = FALSE;
                            if (canRetry) // "retry" is allowed
                            {
                                run = TRUE;
                                retryMsgAux = retryMsgBuf;
                            }
                        }
                    }
                    else // failed to open the "listen" socket for accepting the data connection from the server ->
                    {    // connection closed (so the standard Retry can be used)
                        ok = FALSE;
                        if (canRetry) // "retry" is allowed, proceed to the next reconnect
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (ok) // if we are still connected, change the transfer mode according to 'asciiMode' (ignore success)
                {
                    if (!SetCurrentTransferMode(parent, asciiMode, NULL, NULL, 0, FALSE, &canRetry,
                                                retryMsgBuf, 300))
                    { // error -> connection closed
                        ok = FALSE;
                        if (canRetry) // "retry" is allowed
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (ok)
                {
                    // set the target file name in the data connection; data flushes will be performed
                    // into this file (it is always overwritten, we will not do any resume here)
                    dataConnection->SetDirectFlushParams(tgtFileName, asciiMode ? ctrmASCII : ctrmBinary);

                    if (fileSizeInBytes != CQuadWord(-1, -1)) // if the file size in bytes is known, set it
                        dataConnection->SetDataTotalSize(fileSizeInBytes);

                    int ftpReplyCode;
                    CSendCmdUserIfaceForListAndDownload userIface(TRUE, parent, dataConnection, logUID);

                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                    CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, workPath);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdRetrieveFile, NULL, fileName); // cannot report an error
                    BOOL fileIncomplete = TRUE;
                    BOOL tgtFileError = FALSE;
                    userIface.InitWnd(fileName, hostBuf, workPath, pathType);
                    BOOL sendCmdRes = SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                                     &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                                     retryMsgBuf, 300, &userIface);
                    int asciiTrForBinFileHowToSolve = 0;
                    if (dataConnection->IsAsciiTrForBinFileProblem(&asciiTrForBinFileHowToSolve))
                    {                                         // the "ascii transfer mode for binary file" problem was detected
                        if (asciiTrForBinFileHowToSolve == 0) // we should ask the user
                        {
                            INT_PTR res = CViewErrAsciiTrForBinFileDlg(parent).Execute();
                            if (res == IDOK)
                                asciiTrForBinFileHowToSolve = 1;
                            else
                            {
                                if (res == IDIGNORE)
                                    asciiTrForBinFileHowToSolve = 3;
                                else
                                    asciiTrForBinFileHowToSolve = 2;
                            }
                            dataConnection->SetAsciiTrModeForBinFileHowToSolve(asciiTrForBinFileHowToSolve);
                            // repaint the main window
                            UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                        }
                        if (asciiTrForBinFileHowToSolve == 1) // download again in binary mode
                        {
                            // wait for the file to close in the disk cache; otherwise the file cannot be deleted
                            dataConnection->WaitForFileClose(5000); // max. 5 seconds

                            SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL);
                            DeleteFile(tgtFileName);
                            asciiMode = FALSE; // download again in binary mode

                            ok = FALSE; // repeat the download
                            run = TRUE;
                            retryMsgAux = NULL;
                            setStartTimeIfConnected = FALSE;
                        }
                        else
                        {
                            if (asciiTrForBinFileHowToSolve == 2) // cancel the file download
                            {
                                // wait for the file to close in the disk cache; otherwise the file cannot be deleted
                                dataConnection->WaitForFileClose(5000); // max. 5 seconds

                                SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL);
                                DeleteFile(tgtFileName);
                                ok = FALSE; // do not show any message; the user already confirmed the cancel
                            }
                        }
                    }
                    else
                        asciiTrForBinFileHowToSolve = 3;
                    if (asciiTrForBinFileHowToSolve == 3) // the "ascii transfer mode for binary file" problem did not occur or should be ignored
                    {
                        if (sendCmdRes)
                        {
                            if (userIface.WasAborted()) // user aborted the download - end with an error (incomplete file)
                                ok = FALSE;             // do not display any message; the user confirmed the prompt during the abort
                            else
                            {
                                if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS || // server reports a download error
                                    userIface.HadError() ||                        // the data connection recorded an error reported by the system
                                    //dataConnection->GetDecomprMissingStreamEnd() || // unfortunately this test is not viable; e.g. Serv-U 7 and 8 simply do not terminate the stream // if the data are compressed (MODE Z), we must receive the complete data stream, otherwise it is an error
                                    userIface.GetDatConCancelled()) // the data connection was interrupted (either it was not opened or it was closed after an error reported by the server in the reply to RETR)
                                {
                                    ok = FALSE;

                                    DWORD netErr, tgtFileErr;
                                    BOOL lowMem, noDataTrTimeout;
                                    int sslErrorOccured;
                                    BOOL decomprErrorOccured;
                                    userIface.GetError(&netErr, &lowMem, &tgtFileErr, &noDataTrTimeout, &sslErrorOccured,
                                                       &decomprErrorOccured);
                                    //BOOL decomprMissingStreamEnd = dataConnection->GetDecomprMissingStreamEnd();
                                    BOOL sslReuseErr = ReuseSSLSessionFailed &&
                                                       (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_TRANSIENTERROR ||
                                                        FTP_DIGIT_1(ftpReplyCode) == FTP_D1_ERROR);
                                    if (sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslErrorOccured == SSLCONERR_CANRETRY ||
                                        sslReuseErr)
                                    {                                                                       // we need to reconnect
                                        CloseControlConnection(parent);                                     // close the current control connection
                                        lstrcpyn(retryMsgBuf, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 300); // set the error text for the reconnect wait dialog
                                        retryMsgAux = retryMsgBuf;
                                        sslErrReconnect = TRUE;
                                        run = TRUE;
                                        fastSSLErrReconnect = sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslReuseErr;
                                    }
                                    else
                                    {
                                        // display the message "Unable to download file from server"
                                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERROR), fileName, workPath);
                                        int len = (int)strlen(errBuf);
                                        BOOL isNetErr = TRUE;
                                        int useSuffixResID = IDS_DOWNLOADFILEERRORSUFIX2; // server reply:  (prefix for the message in 'replyBuf')
                                        if (tgtFileErr != NO_ERROR || FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS ||
                                            noDataTrTimeout || sslErrorOccured != SSLCONERR_NOERROR ||
                                            decomprErrorOccured /*|| decomprMissingStreamEnd*/)
                                        {                                                 // if we do not have an error description from the server, settle for the system description
                                            useSuffixResID = IDS_DOWNLOADFILEERRORSUFIX1; // error:
                                            isNetErr = sslErrorOccured != SSLCONERR_NOERROR || netErr != NO_ERROR ||
                                                       tgtFileErr == NO_ERROR || decomprErrorOccured /*|| decomprMissingStreamEnd*/;
                                            if (!isNetErr)
                                            {
                                                netErr = tgtFileErr;
                                                tgtFileError = TRUE;
                                            }
                                            if (sslErrorOccured != SSLCONERR_NOERROR || decomprErrorOccured)
                                                lstrcpyn(replyBuf, LoadStr(decomprErrorOccured ? IDS_ERRDATACONDECOMPRERROR : IDS_ERRDATACONSSLCONNECTERROR), 700);
                                            else
                                            {
                                                if (noDataTrTimeout)
                                                    lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNODATATRTIMEOUT), 700);
                                                else
                                                {
                                                    if (netErr != NO_ERROR)
                                                    {
                                                        if (!dataConnection->GetProxyError(replyBuf, 700, NULL, 0, TRUE))
                                                        {
                                                            if (!isNetErr)
                                                                useSuffixResID = IDS_DOWNLOADFILEERRORSUFIX3; // error writing target file:
                                                            FTPGetErrorText(netErr, replyBuf, 700);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        if (userIface.GetDatConCancelled())
                                                            lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNOTOPENED), 700);
                                                        else
                                                        {
                                                            /*if (decomprMissingStreamEnd) lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONDECOMPRERROR), 700);
                              else*/
                                                            lstrcpyn(replyBuf, LoadStr(IDS_UNKNOWNERROR), 700);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(useSuffixResID), replyBuf);
                                        SalamanderGeneral->SalMessageBox(parent, errBuf,
                                                                         LoadStr(IDS_FTPERRORTITLE),
                                                                         MB_OK | MB_ICONEXCLAMATION);
                                    }
                                }
                                else
                                    fileIncomplete = FALSE; // everything OK, download succeeded
                            }
                        }
                        else // connection closed
                        {
                            if (userIface.WasAborted()) // the user aborted the download, which terminated the connection (e.g. sunsolve.sun.com (Sun Unix) or ftp.chg.ru) - end with an error (incomplete listing)
                            {
                                ok = FALSE;   // do not show the "file can be incomplete" message; the user was warned during the abort
                                if (canRetry) // adopt the message for the message box that reports the connection interruption
                                {
                                    HANDLES(EnterCriticalSection(&SocketCritSect));
                                    if (ConnectionLostMsg != NULL)
                                        SalamanderGeneral->Free(ConnectionLostMsg);
                                    ConnectionLostMsg = SalamanderGeneral->DupStr(retryMsgBuf);
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                }
                            }
                            else
                            {
                                // error -> 'ok' stays FALSE, proceed to the next reconnect
                                ok = FALSE;
                                if (canRetry) // "retry" is allowed
                                {
                                    run = TRUE;
                                    retryMsgAux = retryMsgBuf;
                                }
                            }
                        }

                        // wait for the file to close in the disk cache; otherwise it cannot be deleted and
                        // viewers might not open it either (without SHARE_WRITE it cannot be opened)
                        dataConnection->WaitForFileClose(5000); // max. 5 seconds

                        if (run) // go for another attempt; clean the target file just in case (it may have been created before the error/interruption)
                        {        // we are not in any critical section, so even if the disk operation stalls for a while, nothing happens
                            SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL);
                            DeleteFile(tgtFileName);
                        }
                        else // finish the download
                        {
                            dataConnection->GetTgtFileState(newFileCreated, newFileSize); // find out whether the file exists + its size
                            *newFileIncomplete = fileIncomplete;                          // also store whether the file is complete
                            if (!*newFileCreated &&                                       // the data connection cannot create empty files (it creates the file only just before writing), so we must do it here
                                !fileIncomplete &&                                        // this is not merely an error when obtaining the file
                                !tgtFileError)                                            // just in case no target file error occurred (to avoid it happening again here)
                            {
                                if (*newFileSize != CQuadWord(0, 0))
                                    TRACE_E("CControlConnectionSocket::DownloadOneFile(): unexpected situation: file was not created, but its size is not null!");

                                // we are not in any critical section, so even if the disk operation stalls for a while, nothing happens
                                SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL); // so a read-only file can be overwritten
                                HANDLE file = HANDLES_Q(CreateFile(tgtFileName, GENERIC_WRITE,
                                                                   FILE_SHARE_READ, NULL,
                                                                   CREATE_ALWAYS,
                                                                   FILE_FLAG_SEQUENTIAL_SCAN,
                                                                   NULL));
                                if (file != INVALID_HANDLE_VALUE)
                                {
                                    *newFileCreated = TRUE;
                                    HANDLES(CloseHandle(file));
                                }
                                else
                                {
                                    DWORD err = GetLastError();

                                    // display the message "Unable to download file from server"
                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERROR), fileName, workPath);
                                    int len = (int)strlen(errBuf);
                                    if (err != NO_ERROR)
                                        FTPGetErrorText(err, replyBuf, 700);
                                    else
                                        lstrcpyn(replyBuf, LoadStr(IDS_UNKNOWNERROR), 700);
                                    _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERRORSUFIX3), replyBuf);
                                    SalamanderGeneral->SalMessageBox(parent, errBuf,
                                                                     LoadStr(IDS_FTPERRORTITLE),
                                                                     MB_OK | MB_ICONEXCLAMATION);
                                }
                            }
                        }
                    }
                }

                if (dataConnection->IsConnected())       // close the "data connection" if needed; the system attempts a "graceful"
                    dataConnection->CloseSocketEx(NULL); // shutdown (we do not learn the result)

                if (!run)
                    break;
            }

            // release the "data connection"
            DeleteSocket(dataConnection);
        }
        FTPOpenedFiles.CloseFile(lockedFileUID);
    }
    else
    {
        // display the message "Unable to download file from server - file is locked by another operation"
        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERROR), fileName, workPath);
        int len = (int)strlen(errBuf);
        _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERRORSUFIX4));
        SalamanderGeneral->SalMessageBox(parent, errBuf,
                                         LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
    }
}

BOOL CControlConnectionSocket::CreateDir(char* changedPath, HWND parent, char* newName,
                                         const char* workPath, int* totalAttemptNum, int panel,
                                         BOOL notInPanel, char* userBuf, int userBufSize)
{
    CALL_STACK_MESSAGE8("CControlConnectionSocket::CreateDir(, , %s, , %s, %d, %d, %d, %s, %d)",
                        newName, workPath, *totalAttemptNum, panel, notInPanel, userBuf, userBufSize);

    changedPath[0] = 0;

    BOOL retSuccess = FALSE;
    BOOL reconnected = FALSE;
    BOOL setStartTimeIfConnected = TRUE;
    BOOL canRetry = FALSE;
    const char* retryMsgAux = NULL;
    char retryMsgBuf[300];
    char replyBuf[700];
    char errBuf[900 + FTP_MAX_PATH];
    char cmdBuf[50 + 2 * MAX_PATH];
    char logBuf[50 + 2 * MAX_PATH];
    char hostBuf[HOST_MAX_SIZE];
    char userBuffer[USER_MAX_SIZE];
    while (ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                             userBuf, userBufSize, &reconnected,
                             setStartTimeIfConnected, totalAttemptNum,
                             retryMsgAux, NULL, -1, FALSE)) // if needed, we reconnect
    {
        setStartTimeIfConnected = TRUE;
        BOOL run = FALSE;
        BOOL ok = TRUE;
        char newPath[FTP_MAX_PATH];
        BOOL needChangeDir = reconnected; // after a reconnect we try to set the working directory again
        if (!reconnected)                 // we have been connected for a while, check whether the working directory matches 'workPath'
        {
            // use the cache; in normal cases the path should be there
            ok = GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300);
            if (!ok && canRetry) // "retry" is allowed
            {
                run = TRUE;
                retryMsgAux = retryMsgBuf;
            }
            if (ok && strcmp(newPath, workPath) != 0) // the working directory on the server differs - change required
                needChangeDir = TRUE;                 // (assumption: the server always returns the same working path string)
        }
        if (ok && needChangeDir) // if we need to change the working directory
        {
            BOOL success;
            // SendChangeWorkingPath() contains ReconnectIfNeeded() when the connection drops; luckily that
            // does not matter, because the code preceding this call runs only if no reconnect happened
            // - "if (!reconnected)" - if a reconnect occurs, both code paths are identical
            ok = SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, workPath,
                                       userBuf, userBufSize, &success,
                                       replyBuf, 700, NULL,
                                       totalAttemptNum, NULL, TRUE, NULL);
            if (ok && !success && workPath[0] != 0) // send succeeded but the server reports an error (+ignore errors with an empty path) -> file cannot be
            {                                       // downloaded (it is on the current path in the panel)
                _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), workPath, replyBuf);
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
        }

        if (ok)
        {
            // create the requested directory
            PrepareFTPCommand(cmdBuf, 50 + 2 * MAX_PATH, logBuf, 50 + 2 * MAX_PATH,
                              ftpcmdCreateDir, NULL, newName); // cannot fail
            BOOL refreshWorkingPath = TRUE;
            int ftpReplyCode;
            if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                               &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                               retryMsgBuf, 300, NULL))
            {
                retSuccess = FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS;
                if (retSuccess && workPath[0] != 0) // if the directory/directories were created, adjust the listing(s) (if the FTP command does not report success, we simply assume no directory was created - on VMS multiple directories can be created at once; partial creation might occur, but we do not handle that)
                {
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                    unsigned short portBuf = Port;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    UploadListingCache.ReportCreateDirs(hostBuf, userBuffer, portBuf, workPath,
                                                        GetFTPServerPathType(workPath), newName, FALSE);
                }
                if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS && // success is returned (should be 257)
                    FTPGetDirectoryFromReply(replyBuf, (int)strlen(replyBuf), newPath, FTP_MAX_PATH))
                {                   // directory 'newPath' has just been created
                    newName[0] = 0; // no focus after refresh yet
                    CFTPServerPathType pathType = GetFTPServerPathType(newPath);
                    char cutDir[FTP_MAX_PATH];
                    if (pathType != ftpsptUnknown &&
                        FTPCutDirectory(pathType, newPath, FTP_MAX_PATH, cutDir, FTP_MAX_PATH, NULL))
                    {
                        if (!FTPIsPrefixOfServerPath(pathType, workPath, newPath))
                        {
                            lstrcpyn(changedPath, newPath, FTP_MAX_PATH);
                            refreshWorkingPath = FALSE;
                        }
                        if (FTPIsTheSameServerPath(pathType, newPath, workPath))
                            lstrcpyn(newName, cutDir, 2 * MAX_PATH); // directory name for focus after the refresh
                    }
                    else // probably the server returns a relative directory name in reply "257" (e.g. warftpd)
                    {
                        lstrcpyn(newName, newPath, 2 * MAX_PATH); // directory name for focus after the refresh
                    }
                }
                else // error (including unexpected format of reply "257")
                {
                    if (!retSuccess) // do not display an error message for a successful reply
                    {
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CREATEDIRERROR), newName, replyBuf);
                        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                         MB_OK | MB_ICONEXCLAMATION);
                    }
                }
            }
            else // error -> connection closed
            {
                if (workPath[0] != 0) // we do not know whether the directory/directories were created; invalidate the listing(s)
                {
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                    unsigned short portBuf = Port;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    UploadListingCache.ReportCreateDirs(hostBuf, userBuffer, portBuf, workPath,
                                                        GetFTPServerPathType(workPath), newName, TRUE);
                }
                if (canRetry) // "retry" is allowed
                {
                    run = TRUE;
                    retryMsgAux = retryMsgBuf;
                }
            }
            if (refreshWorkingPath)
                lstrcpyn(changedPath, workPath, FTP_MAX_PATH);
        }

        if (!run)
            break;
    }
    return retSuccess;
}

BOOL CControlConnectionSocket::QuickRename(char* changedPath, HWND parent, const char* fromName,
                                           char* newName, const char* workPath, int* totalAttemptNum,
                                           int panel, BOOL notInPanel, char* userBuf, int userBufSize,
                                           BOOL isVMS, BOOL isDir)
{
    CALL_STACK_MESSAGE11("CControlConnectionSocket::QuickRename(, , %s, %s, , %s, %d, %d, %d, %s, %d, %d, %d)",
                         fromName, newName, workPath, *totalAttemptNum, panel, notInPanel, userBuf,
                         userBufSize, isVMS, isDir);

    changedPath[0] = 0;
    if (strcmp(fromName, newName) == 0)
    {
        newName[0] = 0;
        return TRUE; // nothing to do; the name does not change
    }

    char fromNameBuf[MAX_PATH + 10];
    const char* fromNameForSrv = fromName;
    if (isVMS && isDir)
    {
        FTPMakeVMSDirName(fromNameBuf, MAX_PATH + 10, fromName);
        fromNameForSrv = fromNameBuf;
    }

    HANDLES(EnterCriticalSection(&SocketCritSect));
    char hostBuf[HOST_MAX_SIZE];
    char userBuffer[USER_MAX_SIZE];
    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
    unsigned short portBuf = Port;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    BOOL retSuccess = FALSE;
    BOOL srcLocked = FALSE;
    BOOL tgtLocked = FALSE;
    char errBuf[900 + FTP_MAX_PATH];
    CFTPServerPathType pathType = GetFTPServerPathType(workPath);
    int lockedFromFileUID; // UID of the locked file (in FTPOpenedFiles) - we lock the file for renaming
    if (isDir || FTPOpenedFiles.OpenFile(userBuffer, hostBuf, portBuf, workPath, pathType,
                                         fromNameForSrv, &lockedFromFileUID, ffatRename))
    {                        // the file on the server is not open yet, we can work with it, allocate an object for the "data connection"
        int lockedToFileUID; // UID of the locked file (in FTPOpenedFiles) - we lock the file for renaming
        if (isDir || FTPOpenedFiles.OpenFile(userBuffer, hostBuf, portBuf, workPath, pathType,
                                             newName, &lockedToFileUID, ffatRename))
        { // the file on the server is not open yet, we can work with it, allocate an object for the "data connection"
            BOOL reconnected = FALSE;
            BOOL setStartTimeIfConnected = TRUE;
            BOOL canRetry = FALSE;
            const char* retryMsgAux = NULL;
            char retryMsgBuf[300];
            char replyBuf[700];
            char cmdBuf[50 + 2 * MAX_PATH];
            char logBuf[50 + 2 * MAX_PATH];
            while (ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                                     userBuf, userBufSize, &reconnected,
                                     setStartTimeIfConnected, totalAttemptNum,
                                     retryMsgAux, NULL, -1, FALSE)) // if needed, we reconnect
            {
                setStartTimeIfConnected = TRUE;
                BOOL run = FALSE;
                BOOL ok = TRUE;
                char newPath[FTP_MAX_PATH];
                BOOL needChangeDir = reconnected; // after a reconnect we try to set the working directory again
                if (!reconnected)                 // we have been connected for a while, check whether the working directory matches 'workPath'
                {
                    // use the cache; in normal cases the path should be there
                    ok = GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300);
                    if (!ok && canRetry) // "retry" is allowed
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    if (ok && strcmp(newPath, workPath) != 0) // the working directory on the server differs - change required
                        needChangeDir = TRUE;                 // (assumption: the server always returns the same working path string)
                }
                if (ok && needChangeDir) // if we need to change the working directory
                {
                    BOOL success;
                    // SendChangeWorkingPath() contains ReconnectIfNeeded() when the connection drops; luckily that
                    // does not matter, because the code preceding this call runs only if no reconnect happened
                    // - "if (!reconnected)" - if a reconnect occurs, both code paths are identical
                    ok = SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, workPath,
                                               userBuf, userBufSize, &success,
                                               replyBuf, 700, NULL,
                                               totalAttemptNum, NULL, TRUE, NULL);
                    if (ok && !success && workPath[0] != 0) // send succeeded but the server reports an error (+ignore errors with an empty path) -> file cannot be
                    {                                       // downloaded (it is on the current path in the panel)
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), workPath, replyBuf);
                        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        ok = FALSE;
                    }
                }

                if (ok)
                {
                    // send the "rename from" command first (later send the follow-up "rename to")
                    PrepareFTPCommand(cmdBuf, 50 + 2 * MAX_PATH, logBuf, 50 + 2 * MAX_PATH,
                                      ftpcmdRenameFrom, NULL, fromNameForSrv); // cannot fail
                    int ftpReplyCode;
                    if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                       &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                       retryMsgBuf, 300, NULL))
                    {
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_PARTIALSUCCESS) // 350 Requested file action pending further information
                        {                                                       // we need to send "rename to"
                            PrepareFTPCommand(cmdBuf, 50 + 2 * MAX_PATH, logBuf, 50 + 2 * MAX_PATH,
                                              ftpcmdRenameTo, NULL, newName); // cannot fail
                            if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                               &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                               retryMsgBuf, 300, NULL))
                            {
                                if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // success is returned (should be 250)
                                {                                                // quick rename completed successfully - leave the new name in 'newName' so it can be focused after the refresh
                                    retSuccess = TRUE;
                                    if (workPath[0] != 0)
                                    {
                                        HANDLES(EnterCriticalSection(&SocketCritSect));
                                        lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                                        lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                                        portBuf = Port;
                                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                                        UploadListingCache.ReportRename(hostBuf, userBuffer, portBuf, workPath,
                                                                        pathType, fromName, newName, FALSE);
                                    }
                                }
                                else // error
                                {
                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_QUICKRENAMEERROR), fromName, newName, replyBuf);
                                    SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                                     MB_OK | MB_ICONEXCLAMATION);
                                }
                            }
                            else // error -> connection closed
                            {
                                if (workPath[0] != 0)
                                {
                                    HANDLES(EnterCriticalSection(&SocketCritSect));
                                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                                    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                                    portBuf = Port;
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                    UploadListingCache.ReportRename(hostBuf, userBuffer, portBuf, workPath,
                                                                    pathType, fromName, newName, TRUE);
                                }
                                if (canRetry) // "retry" is allowed
                                {
                                    run = TRUE;
                                    retryMsgAux = retryMsgBuf;
                                }
                            }
                            lstrcpyn(changedPath, workPath, FTP_MAX_PATH);
                        }
                        else // error (including an unexpected reply)
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_QUICKRENAMEERROR), fromName, newName, replyBuf);
                            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                             MB_OK | MB_ICONEXCLAMATION);
                        }
                    }
                    else // error -> connection closed
                    {
                        if (canRetry) // "retry" is allowed
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (!run)
                    break;
            }
            if (!isDir)
                FTPOpenedFiles.CloseFile(lockedToFileUID);
        }
        else
            tgtLocked = TRUE; // rename-to-file is already open
        if (!isDir)
            FTPOpenedFiles.CloseFile(lockedFromFileUID);
    }
    else
        srcLocked = TRUE; // rename-from-file is already open
    if (srcLocked || tgtLocked)
    {
        // display the message "Unable to rename file on server - src or tgt file is locked by another operation"
        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_QUICKRENAMEFILEERR), fromNameForSrv, newName);
        int len = (int)strlen(errBuf);
        _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE,
                    LoadStr(srcLocked ? IDS_QUICKRENAMEFILEERRSUF1 : IDS_QUICKRENAMEFILEERRSUF2));
        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
    }
    return retSuccess;
}

BOOL CControlConnectionSocket::OpenForListeningAndWaitForRes(HWND parent, CDataConnectionSocket* dataConnection,
                                                             DWORD* listenOnIP, unsigned short* listenOnPort,
                                                             BOOL* canRetry, char* retryMsg, int retryMsgBufSize,
                                                             int waitWndTime, char* errBuf, int errBufSize)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::OpenForListeningAndWaitForRes()");

    char buf[300];

    parent = FindPopupParent(parent);
    DWORD startTime = GetTickCount(); // operation start time
    *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    CWaitWindow waitWnd(parent, TRUE);
    waitWnd.SetText(LoadStr(IDS_PREPARINGACTDATACON));

    HWND focusedWnd = NULL;
    BOOL parentIsEnabled = IsWindowEnabled(parent);
    CSetWaitCursorWindow* winParent = NULL;
    if (parentIsEnabled) // we cannot leave the parent enabled (the wait window is not modal)
    {
        // store the focus from 'parent' (if the focus is not from 'parent', save NULL)
        focusedWnd = GetFocus();
        HWND hwnd = focusedWnd;
        while (hwnd != NULL && hwnd != parent)
            hwnd = GetParent(hwnd);
        if (hwnd != parent)
            focusedWnd = NULL;
        // disable 'parent'; re-enable it along with the focus
        EnableWindow(parent, FALSE);

        // set the wait cursor over the parent; unfortunately we cannot do it otherwise
        winParent = new CSetWaitCursorWindow;
        if (winParent != NULL)
            winParent->AttachToWindow(parent);
    }

    BOOL ret = FALSE;

    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // at least a second

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID;                                  // log UID of this connection
    BOOL handleKeepAlive = KeepAliveMode != kamForbidden; // TRUE if keep-alive is not handled one level above (it must be handled here)
    DWORD auxServerIP = ServerIP;
    int proxyPort = ProxyServer != NULL ? ProxyServer->ProxyPort : 0;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (handleKeepAlive)
    {
        // wait for the keep-alive command to finish (if it is running) + set
        // keep-alive to 'kamForbidden' (a normal command is in progress)
        DWORD waitTime = GetTickCount() - startTime;
        WaitForEndOfKeepAlive(parent, waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);
    }

    dataConnection->SetPostMessagesToWorker(TRUE, GetMsg(), GetUID(), -1, -1, -1, CTRLCON_LISTENFORCON);

    BOOL doNotCloseCon = FALSE; // TRUE = on error the control connection must not be closed
    BOOL listenError;
    DWORD err;
    if (dataConnection->OpenForListeningWithProxy(*listenOnIP, *listenOnPort, &listenError, &err))
    {
        DWORD start = GetTickCount();
        DWORD waitTime = start - startTime;
        waitWnd.Create(waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);

        BOOL newBytesReadReceived = FALSE;
        DWORD newBytesReadData1 = 0;
        BOOL closedReceived = FALSE;
        DWORD closedData1 = 0;
        BOOL run = TRUE;
        while (run)
        {
            // wait for an event on the socket (server reply) or ESC
            CControlConnectionSocketEvent event;
            DWORD data1, data2;
            DWORD now = GetTickCount();
            if (now - start > (DWORD)serverTimeout)
                now = start + (DWORD)serverTimeout;
            WaitForEventOrESC(parent, &event, &data1, &data2, serverTimeout - (now - start),
                              &waitWnd, NULL, FALSE);
            switch (event)
            {
            case ccsevESC:
            {
                waitWnd.Show(FALSE);
                BOOL esc = SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_PREPACTDATACONESC),
                                                            LoadStr(IDS_FTPPLUGINTITLE),
                                                            MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
                if (esc)
                {
                    run = FALSE;
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE); // ESC (cancel) into the log
                }
                else
                {
                    SalamanderGeneral->WaitForESCRelease(); // ensure that subsequent actions are not interrupted after every ESC in the previous message box
                    waitWnd.Show(TRUE);
                }
                break;
            }

            case ccsevTimeout:
            {
                if (!dataConnection->GetProxyTimeoutDescr(buf, 300))
                    lstrcpyn(buf, LoadStr(IDS_PREPACTDATACONTIMEOUT), 300);
                if (retryMsgBufSize > 0)
                {
                    _snprintf_s(retryMsg, retryMsgBufSize, _TRUNCATE, "%s\r\n", buf);
                    Logs.LogMessage(logUID, retryMsg, -1, TRUE);
                }
                *canRetry = TRUE;
                run = FALSE;
                break;
            }

            case ccsevListenForCon:
            {
                if ((int)data1 == dataConnection->GetUID()) // process the message only if it is for our data connection
                {
                    if (!dataConnection->GetListenIPAndPort(listenOnIP, listenOnPort)) // "listen" error
                    {
                        if (dataConnection->GetProxyError(buf, 300, NULL, 0, TRUE) &&
                            errBufSize > 0)
                        { // write the error to the log
                            _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), buf);
                            Logs.LogMessage(logUID, errBuf, -1, TRUE);
                        }
                        *canRetry = TRUE;
                        lstrcpyn(retryMsg, LoadStr(IDS_PROXYERROPENACTDATA), retryMsgBufSize);
                    }
                    else
                        ret = TRUE; // success, return the "listen" IP+port
                    run = FALSE;
                }
                break;
            }

            case ccsevClosed: // resend the connection loss after we finish here
            {
                closedReceived = TRUE;
                closedData1 = data1;
                break;
            }

            case ccsevNewBytesRead: // resend the message about reading new bytes after we finish here
            {
                newBytesReadReceived = TRUE;
                newBytesReadData1 = data1;
                break;
            }

            default:
                TRACE_E("CControlConnectionSocket::OpenForListeningAndWaitForRes(): unexpected event = " << event);
                break;
            }
        }
        waitWnd.Destroy();

        if (newBytesReadReceived)
            AddEvent(ccsevNewBytesRead, newBytesReadData1, 0, newBytesReadData1 == NO_ERROR); // can be overwritten only if it is not an error
        if (closedReceived)
            AddEvent(ccsevClosed, closedData1, 0);
    }
    else
    {
        if (listenError) // CSocket::OpenForListening() failed - "retry" makes no sense (it was a local operation)
        {
            if (errBufSize > 0)
            {
                _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_OPENACTDATACONERROR),
                            (err != NO_ERROR ? FTPGetErrorText(err, buf, 300) : LoadStr(IDS_UNKNOWNERROR)));
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
            doNotCloseCon = TRUE;
        }
        else // connection error on the proxy server; log the error, close the control connection, and perform "retry"
        {
            in_addr srvAddr;
            srvAddr.s_addr = auxServerIP;
            if (err != NO_ERROR)
            {
                FTPGetErrorText(err, errBuf, errBufSize);
                char* s = errBuf + strlen(errBuf);
                while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                *s = 0; // trim newline characters from the error text
                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOGMSGUNABLETOCONPRX2), inet_ntoa(srvAddr), proxyPort, errBuf);
            }
            else
                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOGMSGUNABLETOCONPRX), inet_ntoa(srvAddr), proxyPort);
            Logs.LogMessage(logUID, buf, -1, TRUE);

            *canRetry = TRUE;
            lstrcpyn(retryMsg, LoadStr(IDS_PROXYERRUNABLETOCON), retryMsgBufSize);
        }
    }

    if (parentIsEnabled) // if we disabled the parent, enable it again
    {
        // remove the wait cursor from the parent
        if (winParent != NULL)
        {
            winParent->DetachWindow();
            delete winParent;
        }

        // enable 'parent'
        EnableWindow(parent, TRUE);
        // if 'parent' is active, restore the focus as well
        if (GetForegroundWindow() == parent)
        {
            if (parent == SalamanderGeneral->GetMainWindowHWND())
                SalamanderGeneral->RestoreFocusInSourcePanel();
            else
            {
                if (focusedWnd != NULL)
                    SetFocus(focusedWnd);
            }
        }
    }

    if (ret || doNotCloseCon) // success or an error where the connection must not be closed
    {
        if (handleKeepAlive)
        {
            // if everything is OK, set the keep-alive timer
            SetupKeepAliveTimer();
        }
    }
    else // error after which "retry" will run; we must close the connection (the standard "retry" starts with connect...)
    {
        CloseSocket(NULL); // close the socket (if it is open); the system attempts a "graceful" shutdown (we do not learn the result)
        Logs.SetIsConnected(logUID, IsConnected());
        Logs.RefreshListOfLogsInLogsDlg(); // message "connection inactive"

        if (handleKeepAlive)
        {
            // release keep-alive; it will no longer be needed (no connection is established now)
            ReleaseKeepAlive();
        }
    }

    return ret;
}
