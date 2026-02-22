// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

void CFTPWorker::OpenActDataCon(CFTPWorkerSubState waitForListen, char* errBuf, BOOL& conClosedRetryItem, BOOL& lookForNewWork)
{
    DWORD localIP;
    unsigned short localPort = 0; // listen on any port
    DWORD error = NO_ERROR;
    int logUID = LogUID;

    HANDLES(LeaveCriticalSection(&WorkerCritSect));

    // Since we are already inside the CSocketsThread::CritSect section, these calls
    // are possible even from the CSocket::SocketCritSect section (no risk of deadlock).
    GetLocalIP(&localIP, NULL); // it should hardly be able to return an error
    BOOL retOpenForListening = FALSE;
    BOOL listenError = TRUE;
    if (WorkerDataCon != NULL)
    {
        WorkerDataCon->SetActive(logUID);
        retOpenForListening = WorkerDataCon->OpenForListeningWithProxy(localIP, localPort, &listenError, &error);
    }

    HANDLES(EnterCriticalSection(&WorkerCritSect));

    if (retOpenForListening)
    {
        WorkerDataConState = wdcsWaitingForConnection;
        SubState = waitForListen;

        // In case the proxy server does not respond within the required time limit, add a timer
        // for the timeout of preparing the data connection (opening the "listen" port).
        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
        if (serverTimeout < 1000)
            serverTimeout = 1000; // at least one second
        // Since we are already inside the CSocketsThread::CritSect section, this call
        // is possible even from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no risk of deadlock).
        SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                WORKER_LISTENTIMEOUTTIMID, NULL); // ignore error; at worst the user presses Stop
    }
    else // failed to open a "listen" socket for receiving the data connection from
    {    // the server (a local operation that should almost never happen) or cannot open a connection to the proxy server
        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            DeleteSocket(WorkerDataCon); // no connection has been established yet; it will only be deallocated
            WorkerDataCon = NULL;
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            WorkerDataConState = wdcsDoesNotExist;
        }

        if (listenError)
        {
            // Item error; record this state into it.
            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LISTENFAILURE, error, NULL, Oper);
            lookForNewWork = TRUE;
        }
        else // cannot open a connection to the proxy server; perform a retry...
        {
            if (error != NO_ERROR)
            {
                FTPGetErrorText(error, errBuf, 50 + FTP_MAX_PATH);
                char* s = errBuf + strlen(errBuf);
                while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                *s = 0; // trim newline characters from the error text
                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_PROXYERRUNABLETOCON2), errBuf);
            }
            else
                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_PROXYERRUNABLETOCON));
            _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), ErrorDescr);
            lstrcpyn(ErrorDescr, errBuf, FTPWORKER_ERRDESCR_BUFSIZE); // we want the error text to contain "data con. err.:"
            CorrectErrorDescr();

            // Write the timeout to the log.
            Logs.LogMessage(LogUID, errBuf, -1, TRUE);

            // "Manually" close the control connection.
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            ForceClose();
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            conClosedRetryItem = TRUE;
        }
    }
}

void CFTPWorker::WaitForListen(CFTPWorkerEvent event, BOOL& handleShouldStop, char* errBuf,
                               char* buf, int& cmdLen, BOOL& sendCmd, BOOL& conClosedRetryItem,
                               CFTPWorkerSubState waitForPORTRes)
{
    if (ShouldStop)
    {
        // Since we are already inside the CSocketsThread::CritSect section, this call
        // It can also be called from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no risk of deadlock).
        SocketsThread->DeleteTimer(UID, WORKER_LISTENTIMEOUTTIMID);

        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
            WorkerDataCon->FreeFlushData();
            DeleteSocket(WorkerDataCon);
            WorkerDataCon = NULL;
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            WorkerDataConState = wdcsDoesNotExist;
        }

        handleShouldStop = TRUE; // check whether the worker should stop
    }
    else
    {
        BOOL needRetry = FALSE;
        switch (event)
        {
        case fweDataConListeningForCon:
        {
            if (WorkerDataCon != NULL) // "always true" (otherwise the 'fweDataConListeningForCon' event would never be generated)
            {
                // Since we are already inside the CSocketsThread::CritSect section, this call
                // It can also be called from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no risk of deadlock).
                SocketsThread->DeleteTimer(UID, WORKER_LISTENTIMEOUTTIMID);

                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                errBuf[0] = 0;
                DWORD listenOnIP;
                unsigned short listenOnPort;
                BOOL ok = WorkerDataCon->GetListenIPAndPort(&listenOnIP, &listenOnPort);
                if (!ok)
                {
                    if (!WorkerDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                        errBuf[0] = 0;
                }
                HANDLES(EnterCriticalSection(&WorkerCritSect));

                if (ok)
                {
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdSetPort, &cmdLen, listenOnIP, listenOnPort); // cannot report an error
                    sendCmd = TRUE;
                    SubState = waitForPORTRes;
                }
                else // error while opening the "listen" port on the proxy server - perform a retry...
                {
                    // Close the data connection.
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // Since we are already inside the CSocketsThread::CritSect section, this call
                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                        WorkerDataCon->FreeFlushData();
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // Prepare the error (timeout) text into 'ErrorDescr'
                    if (errBuf[0] == 0)
                        lstrcpyn(ErrorDescr, LoadStr(IDS_PROXYERROPENACTDATA), FTPWORKER_ERRDESCR_BUFSIZE);
                    else
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), errBuf);
                    needRetry = TRUE;
                }
            }
            else
                TRACE_E("Unexpected situation in CFTPWorker::WaitForListen(): WorkerDataCon == NULL!");
            break;
        }

        case fweDataConListenTimeout:
        {
            // Close the data connection and prepare the error (timeout) text into 'ErrorDescr'.
            errBuf[0] = 0;
            if (WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                if (!WorkerDataCon->GetProxyTimeoutDescr(errBuf, 50 + FTP_MAX_PATH))
                    errBuf[0] = 0;
                // Since we are already inside the CSocketsThread::CritSect section, this call
                // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                    WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                WorkerDataCon->FreeFlushData();
                DeleteSocket(WorkerDataCon);
                WorkerDataCon = NULL;
                HANDLES(EnterCriticalSection(&WorkerCritSect));
                WorkerDataConState = wdcsDoesNotExist;
            }
            if (errBuf[0] == 0)
                lstrcpyn(ErrorDescr, LoadStr(IDS_PREPACTDATACONTIMEOUT), FTPWORKER_ERRDESCR_BUFSIZE);
            else
                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), errBuf);
            needRetry = TRUE;
            break;
        }
        }

        if (needRetry)
        {
            CorrectErrorDescr();

            // Write the timeout to the log.
            _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, "%s\r\n", ErrorDescr);
            Logs.LogMessage(LogUID, errBuf, -1, TRUE);

            // "Manually" close the control connection.
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            ForceClose();
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            conClosedRetryItem = TRUE;
        }
    }
}

void CFTPWorker::WaitForPASVRes(CFTPWorkerEvent event, char* reply, int replySize, int replyCode,
                                BOOL& handleShouldStop, BOOL& nextLoop, BOOL& conClosedRetryItem,
                                CFTPWorkerSubState setType, CFTPWorkerSubState openActDataCon)
{
    switch (event)
    {
    // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the log)
    case fweCmdReplyReceived:
    {
        DWORD ip;
        unsigned short port;
        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&             // success (should be 227)
            FTPGetIPAndPortFromReply(reply, replySize, &ip, &port)) // succeeded in obtaining IP+port
        {
            if (ShouldStop)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // Since we are already inside the CSocketsThread::CritSect section, this call
                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                    DeleteSocket(WorkerDataCon); // no connection has been established yet; it will only be deallocated
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // check whether the worker should stop
            }
            else
            {
                int logUID = LogUID;
                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                // Since we are already inside the CSocketsThread::CritSect section, these calls
                // They can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                if (WorkerDataCon != NULL)
                {
                    WorkerDataCon->SetPassive(ip, port, logUID);
                    WorkerDataCon->PassiveConnect(NULL); // first attempt; the result does not interest us (it is checked later)
                }

                HANDLES(EnterCriticalSection(&WorkerCritSect));
                WorkerDataConState = wdcsWaitingForConnection;

                nextLoop = TRUE;
                SubState = setType;
            }
        }
        else // passive mode is not supported; try active mode instead
        {
            Oper->SetUsePassiveMode(FALSE);
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);

            if (ShouldStop)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // Since we are already inside the CSocketsThread::CritSect section, this call
                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                    DeleteSocket(WorkerDataCon); // no connection has been established yet; it will only be deallocated
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // check whether the worker should stop
            }
            else
            {
                nextLoop = TRUE;
                SubState = openActDataCon;
            }
        }
        break;
    }

    case fweCmdConClosed: // connection closed/timed out (description see ErrorDescr) -> try to restore it
    {
        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            DeleteSocket(WorkerDataCon); // no connection has been established yet; it will only be deallocated
            WorkerDataCon = NULL;
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            WorkerDataConState = wdcsDoesNotExist;
        }

        conClosedRetryItem = TRUE;
        break;
    }
    }
}

void CFTPWorker::WaitForPORTRes(CFTPWorkerEvent event, BOOL& nextLoop, BOOL& conClosedRetryItem,
                                CFTPWorkerSubState setType)
{
    switch (event)
    {
    // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the log)
    case fweCmdReplyReceived:
    {
        nextLoop = TRUE;
        SubState = setType;
        break;
    }

    case fweCmdConClosed: // connection closed/timed out (description see ErrorDescr) -> try to restore it
    {
        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
            WorkerDataCon->FreeFlushData();
            DeleteSocket(WorkerDataCon);
            WorkerDataCon = NULL;
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            WorkerDataConState = wdcsDoesNotExist;
        }

        conClosedRetryItem = TRUE;
        break;
    }
    }
}

void CFTPWorker::SetTypeA(BOOL& handleShouldStop, char* errBuf, char* buf, int& cmdLen,
                          BOOL& sendCmd, BOOL& nextLoop, CCurrentTransferMode trMode,
                          BOOL asciiTrMode, CFTPWorkerSubState waitForTYPERes,
                          CFTPWorkerSubState trModeAlreadySet)
{
    if (ShouldStop)
    {
        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
            WorkerDataCon->FreeFlushData();
            DeleteSocket(WorkerDataCon);
            WorkerDataCon = NULL;
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            WorkerDataConState = wdcsDoesNotExist;
        }

        handleShouldStop = TRUE; // check whether the worker should stop
    }
    else
    {
        if (CurrentTransferMode != trMode) // we need ASCII mode; set it if necessary
        {
            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                              ftpcmdSetTransferMode, &cmdLen, asciiTrMode); // cannot report an error
            sendCmd = TRUE;
            SubState = waitForTYPERes;
        }
        else // the ASCII mode is already set
        {
            nextLoop = TRUE;
            SubState = trModeAlreadySet;
        }
    }
}

void CFTPWorker::WaitForTYPERes(CFTPWorkerEvent event, int replyCode, BOOL& nextLoop, BOOL& conClosedRetryItem,
                                CCurrentTransferMode trMode, CFTPWorkerSubState trModeAlreadySet)
{
    switch (event)
    {
    // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the log)
    case fweCmdReplyReceived:
    {
        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // success is returned (should be 200)
            CurrentTransferMode = trMode;             // the transfer mode was changed
        else
            CurrentTransferMode = ctrmUnknown; // unknown error; it may not matter at all, but we will not cache the data transfer mode

        nextLoop = TRUE;
        SubState = trModeAlreadySet;
        break;
    }

    case fweCmdConClosed: // connection closed/timed out (description see ErrorDescr) -> try to restore it
    {
        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // Since we are already inside the CSocketsThread::CritSect section, this call
            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
            if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
            WorkerDataCon->FreeFlushData();
            DeleteSocket(WorkerDataCon);
            WorkerDataCon = NULL;
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            WorkerDataConState = wdcsDoesNotExist;
        }

        conClosedRetryItem = TRUE;
        break;
    }
    }
}

void CFTPWorker::HandleEventInWorkingState2(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                            BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                            int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                            int replyCode, char* ftpPath, char* errText,
                                            BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                            BOOL& handleShouldStop, BOOL* listingNotAccessible)
{
    // NOTE: this method is also used for listing the target path during upload (UploadDirGetTgtPathListing==TRUE)!!!
    if (listingNotAccessible != NULL)
        *listingNotAccessible = FALSE;
    char* tgtPath = NULL;
    if (UploadDirGetTgtPathListing)
    {
        if (CurItem->Type == fqitUploadCopyExploreDir || CurItem->Type == fqitUploadMoveExploreDir)
            tgtPath = ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath;
        else
        {
            if (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile)
                tgtPath = ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath;
            else
                TRACE_E("CFTPWorker::HandleEventInWorkingState2(): UploadDirGetTgtPathListing: unknown CurItem->Type: " << CurItem->Type);
        }
    }
    while (1)
    {
        BOOL nextLoop = FALSE;
        switch (SubState)
        {
        case fwssWorkStartWork: // determine which path to switch to on the server and send CWD
        {
            // before exploring a directory for delete/change-attr and listing the path for upload we must reset
            // the speed meter (explore speed and upload listing are not measured) - this will display "(unknown)" time-left in the operation dialog
            if (CurItem->Type == fqitDeleteExploreDir ||
                CurItem->Type == fqitChAttrsExploreDir ||
                CurItem->Type == fqitChAttrsExploreDirLink ||
                UploadDirGetTgtPathListing)
            {
                Oper->GetGlobalTransferSpeedMeter()->Clear();
                Oper->GetGlobalTransferSpeedMeter()->JustConnected();
            }

            if (UploadDirGetTgtPathListing)
                lstrcpyn(ftpPath, tgtPath, FTP_MAX_PATH);
            else
                lstrcpyn(ftpPath, CurItem->Path, FTP_MAX_PATH);
            CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
            if (UploadDirGetTgtPathListing || FTPPathAppend(type, ftpPath, FTP_MAX_PATH, CurItem->Name, TRUE))
            { // we have the path; send CWD to the server to enter the inspected directory
                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGLISTINGPATH), ftpPath);
                Logs.LogMessage(LogUID, errText, -1, TRUE);

                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                  ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // cannot report an error
                sendCmd = TRUE;
                SubState = fwssWorkExplWaitForCWDRes;

                HaveWorkingPath = FALSE; // we are changing the current working directory on the server
            }
            else // path syntax error or the resulting path would be too long
            {
                // Item error; record this state into it.
                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INVALIDPATHTODIR, NO_ERROR, NULL, Oper);
                if (listingNotAccessible != NULL)
                    *listingNotAccessible = TRUE;
                lookForNewWork = TRUE;
            }
            break;
        }

        case fwssWorkExplWaitForCWDRes: // explore-dir: waiting for the "CWD" result (changing into the inspected directory)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the log)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                { // success, determine the new current path on the server (send PWD)
                    if (ShouldStop)
                        handleShouldStop = TRUE; // check whether the worker should stop
                    else
                    {
                        if (UploadDirGetTgtPathListing) // during upload PWD makes no sense (we do not test for cycles when traversing paths)
                        {
                            SubState = fwssWorkExplWaitForPWDRes;
                            nextLoop = TRUE;
                        }
                        else
                        {
                            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                              ftpcmdPrintWorkingPath, &cmdLen); // cannot report an error
                            sendCmd = TRUE;
                            SubState = fwssWorkExplWaitForPWDRes;
                        }
                    }
                }
                else // an error occurred; display it to the user and continue processing the next queue item
                {
                    CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                    Queue->UpdateItemState(CurItem, sqisFailed,
                                           UploadDirGetTgtPathListing ? ITEMPR_UNABLETOCWDONLYPATH : ITEMPR_UNABLETOCWD,
                                           NO_ERROR, SalamanderGeneral->DupStr(errText) /* low memory = the error will be without details */,
                                           Oper);
                    if (listingNotAccessible != NULL)
                        *listingNotAccessible = TRUE;
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fweCmdConClosed: // connection closed/timed out (description see ErrorDescr) -> try to restore it
            {
                conClosedRetryItem = TRUE;
                break;
            }
            }
            break;
        }

        case fwssWorkExplWaitForPWDRes: // explore-dir: waiting for the "PWD" result (determining the working path of the inspected directory)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the log)
            case fweCmdReplyReceived:
            {
                BOOL pwdErr = FALSE;
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                { // success, read the PWD result - NOTE: if UploadDirGetTgtPathListing==TRUE, this is the result of CWD (not PWD) - PWD is not needed
                    if (ShouldStop)
                        handleShouldStop = TRUE; // check whether the worker should stop
                    else
                    {
                        if (UploadDirGetTgtPathListing || FTPGetDirectoryFromReply(reply, replySize, ftpPath, FTP_MAX_PATH))
                        { // we have the working path; check whether a cycle (endless loop) is occurring
                            BOOL cycle = FALSE;
                            if (!UploadDirGetTgtPathListing)
                            {
                                lstrcpyn(WorkingPath, ftpPath, FTP_MAX_PATH);
                                HaveWorkingPath = TRUE;

                                // Check whether the path did not shorten (jump to the parent directory = guaranteed endless loop).
                                lstrcpyn(ftpPath, CurItem->Path, FTP_MAX_PATH);
                                CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
                                if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, CurItem->Name, TRUE))
                                { // perform the test only if composing the path succeeds - "always true"
                                    if (!FTPIsTheSameServerPath(type, WorkingPath, ftpPath) &&
                                        FTPIsPrefixOfServerPath(type, WorkingPath, ftpPath))
                                    { // this is a jump to the parent directory
                                        cycle = TRUE;
                                    }
                                }
                                // Check whether we have already visited this path, which would mean entering an endless loop.
                                if (!cycle && Oper->IsAlreadyExploredPath(WorkingPath))
                                    cycle = TRUE;
                            }

                            if (cycle)
                            {
                                // Item error; record this state into it.
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_DIREXPLENDLESSLOOP, NO_ERROR, NULL, Oper);
                                lookForNewWork = TRUE;
                            }
                            else // everything is OK; allocate the data connection
                            {
                                if (WorkerDataCon != NULL)
                                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState2(): WorkerDataCon is not NULL before starting data-connection!");
                                CFTPProxyForDataCon* dataConProxyServer;
                                if (Oper->AllocProxyForDataCon(&dataConProxyServer))
                                {
                                    CCertificate* cert = Oper->GetCertificate();
                                    WorkerDataCon = new CDataConnectionSocket(FALSE, dataConProxyServer, Oper->GetEncryptDataConnection(), cert, Oper->GetCompressData(), this);
                                    if (cert)
                                        cert->Release();
                                }
                                else
                                    WorkerDataCon = NULL;
                                ReuseSSLSessionFailed = FALSE;
                                if (WorkerDataCon == NULL || !WorkerDataCon->IsGood())
                                {
                                    if (WorkerDataCon != NULL)
                                    {
                                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                        // Since we are already inside the CSocketsThread::CritSect section, this call
                                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                                        DeleteSocket(WorkerDataCon); // it will only be deallocated
                                        WorkerDataCon = NULL;
                                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                                        WorkerDataConState = wdcsDoesNotExist;
                                    }
                                    else
                                    {
                                        if (dataConProxyServer != NULL)
                                            delete dataConProxyServer;
                                    }
                                    TRACE_E(LOW_MEMORY);

                                    // Item error; record this state into it.
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                else // the data-connection object was allocated successfully
                                {
                                    WorkerDataConState = wdcsOnlyAllocated;

                                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                    // Since we are already inside the CSocketsThread::CritSect section, this call
                                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                                    WorkerDataCon->SetPostMessagesToWorker(TRUE, Msg, UID,
                                                                           WORKER_DATACON_CONNECTED,
                                                                           WORKER_DATACON_CLOSED, -1 /* not needed for listings */,
                                                                           WORKER_DATACON_LISTENINGFORCON);
                                    WorkerDataCon->SetGlobalLastActivityTime(Oper->GetGlobalLastActivityTime());
                                    // exploring directories for delete/change-attr and upload listing are not measured (the meter is used
                                    // otherwise see SMPLCMD_APPROXBYTESIZE + upload: the meter is for upload, but this is download)
                                    if (CurItem->Type != fqitDeleteExploreDir &&
                                        CurItem->Type != fqitChAttrsExploreDir &&
                                        CurItem->Type != fqitChAttrsExploreDirLink &&
                                        !UploadDirGetTgtPathListing)
                                    {
                                        WorkerDataCon->SetGlobalTransferSpeedMeter(Oper->GetGlobalTransferSpeedMeter());
                                    }
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));

                                    if (Oper->GetUsePassiveMode()) // passive mode (PASV)
                                    {
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdPassive, &cmdLen); // cannot report an error
                                        sendCmd = TRUE;
                                        SubState = fwssWorkExplWaitForPASVRes;
                                    }
                                    else // active mode (PORT)
                                    {
                                        nextLoop = TRUE;
                                        SubState = fwssWorkExplOpenActDataCon;
                                    }
                                }
                            }
                        }
                        else
                            pwdErr = TRUE; // an error occurred; display it to the user and continue processing the next queue item
                    }
                }
                else
                    pwdErr = TRUE; // an error occurred; display it to the user and continue processing the next queue item
                if (pwdErr)
                {
                    CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOPWD, NO_ERROR,
                                           SalamanderGeneral->DupStr(errText) /* low memory = the error will be without details */,
                                           Oper);
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fweCmdConClosed: // connection closed/timed out (description see ErrorDescr) -> try to restore it
            {
                conClosedRetryItem = TRUE;
                break;
            }
            }
            break;
        }

        case fwssWorkExplWaitForPASVRes: // explore-dir: waiting for the "PASV" result (obtaining IP+port for the passive data connection)
        {
            WaitForPASVRes(event, reply, replySize, replyCode, handleShouldStop, nextLoop, conClosedRetryItem,
                           fwssWorkExplSetTypeA, fwssWorkExplOpenActDataCon);
            break;
        }

        case fwssWorkExplOpenActDataCon: // explore-dir: open the active data connection
        {
            OpenActDataCon(fwssWorkExplWaitForListen, errBuf, conClosedRetryItem, lookForNewWork);
            break;
        }

        case fwssWorkExplWaitForListen: // explore-dir: waiting for the "listen" port to open (opening an active data connection) - local or on the proxy server
        {
            WaitForListen(event, handleShouldStop, errBuf, buf, cmdLen, sendCmd,
                          conClosedRetryItem, fwssWorkExplWaitForPORTRes);
            break;
        }

        case fwssWorkExplWaitForPORTRes: // explore-dir: waiting for the "PORT" result (providing IP+port to the server for the active data connection)
        {
            WaitForPORTRes(event, nextLoop, conClosedRetryItem, fwssWorkExplSetTypeA);
            break;
        }

        case fwssWorkExplSetTypeA: // explore-dir: set the transfer mode to ASCII
        {
            SetTypeA(handleShouldStop, errBuf, buf, cmdLen, sendCmd, nextLoop, ctrmASCII, TRUE,
                     fwssWorkExplWaitForTYPERes, fwssWorkExplSendListCmd);
            break;
        }

        case fwssWorkExplWaitForTYPERes: // explore-dir: waiting for the "TYPE" result (switching to ASCII data transfer mode)
        {
            WaitForTYPERes(event, replyCode, nextLoop, conClosedRetryItem, ctrmASCII, fwssWorkExplSendListCmd);
            break;
        }

        case fwssWorkExplSendListCmd: // explore-dir: send the LIST command
        {
            if (ShouldStop)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // Since we are already inside the CSocketsThread::CritSect section, this call
                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                    if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                    DeleteSocket(WorkerDataCon);
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // check whether the worker should stop
            }
            else
            {
                // Obtain the date when the listing was created (we assume the server creates it first and sends it afterwards).
                GetLocalTime(&StartTimeOfListing);
                StartLstTimeOfListing = IncListingCounter();

                Oper->GetListCommand(buf, 200 + FTP_MAX_PATH);
                lstrcpyn(errBuf, buf, 50 + FTP_MAX_PATH);
                cmdLen = (int)strlen(buf);
                CommandTransfersData = TRUE;
                sendCmd = TRUE;

                postActivate = TRUE;
                SubState = fwssWorkExplActivateDataCon;
            }
            break;
        }

        case fwssWorkExplActivateDataCon: // explore-dir: activate the data connection (right after sending the LIST command)
        {
            if (WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                // Since we are already inside the CSocketsThread::CritSect section, this call
                // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                WorkerDataCon->ActivateConnection();
                HANDLES(EnterCriticalSection(&WorkerCritSect));
            }
            SubState = fwssWorkExplWaitForLISTRes;
            if (event != fweActivate)
                nextLoop = TRUE; // if it was not just fweActivate, deliver the event to state fwssWorkExplWaitForLISTRes
            break;
        }

        case fwssWorkExplWaitForLISTRes: // explore-dir: waiting for the "LIST" result (waiting for the listing data transfer to finish)
        {
            switch (event)
            {
            case fweCmdInfoReceived: // "1xx" replies contain the size of the transferred data
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                if (WorkerDataCon != NULL)
                {
                    WorkerDataCon->EncryptPassiveDataCon();
                    CQuadWord size;
                    if (FTPGetDataSizeInfoFromSrvReply(size, reply, replySize))
                        WorkerDataCon->SetDataTotalSize(size); // we have the total size of the listing
                }
                HANDLES(EnterCriticalSection(&WorkerCritSect));
                break;
            }

            case fweCmdReplyReceived:
            {
                ListCmdReplyCode = replyCode;
                CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                if (ListCmdReplyText != NULL)
                    SalamanderGeneral->Free(ListCmdReplyText);
                ListCmdReplyText = SalamanderGeneral->DupStr(errText); /* low memory = we will do without the reply description */

                BOOL waitForDataConFinish = FALSE;
                if (!ShouldStop && WorkerDataCon != NULL)
                {
                    BOOL trFinished;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS ||
                        !WorkerDataCon->IsTransfering(&trFinished) && !trFinished)
                    { // the server returns a listing error or the connection was not established
                        // Since we are already inside the CSocketsThread::CritSect section, this call
                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                        if (WorkerDataCon->IsConnected())
                        {                                       // close the "data connection", the system will attempt a "graceful"
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)

                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                            { // connection was not established yet the server reports success -> perform a retry
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONNOTOPENED), -1, TRUE);
                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will certainly handle this item (no need to post "new work available")
                                lookForNewWork = TRUE;
                                break;
                            }
                        }
                    }
                    else
                    {
                        if (WorkerDataCon->IsConnected()) // data transfer is still in progress; wait for completion
                            waitForDataConFinish = TRUE;
                    }
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                else
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // Since we are already inside the CSocketsThread::CritSect section, this call
                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                }

                // If we do not have to wait for the "data connection" to finish, proceed to processing the LIST command replyCode.
                SubState = waitForDataConFinish ? fwssWorkExplWaitForDataConFinish : fwssWorkExplProcessLISTRes;
                if (!waitForDataConFinish)
                    nextLoop = TRUE;
                break;
            }

            case fweCmdConClosed: // connection closed/timed out (description see ErrorDescr) -> try to restore it
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // Since we are already inside the CSocketsThread::CritSect section, this call
                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                    if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                    DeleteSocket(WorkerDataCon);
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                conClosedRetryItem = TRUE;
                break;
            }
            }
            break;
        }

        case fwssWorkExplWaitForDataConFinish: // explore-dir: waiting for the "data connection" to finish (the server reply to "LIST" has already arrived)
        {
            BOOL con = FALSE;
            if (WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                // Since we are already inside the CSocketsThread::CritSect section, this call
                // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                con = WorkerDataCon->IsConnected();
                HANDLES(EnterCriticalSection(&WorkerCritSect));
            }
            if (WorkerDataCon == NULL || !con) // either the "data connection" does not exist or it has already been closed
            {
                nextLoop = TRUE;
                SubState = fwssWorkExplProcessLISTRes;
            }
            break;
        }

        case fwssWorkExplProcessLISTRes: // explore-dir: process the "LIST" result (only after the "data connection" ends and the server reply to "LIST" is received)
        {
            BOOL delOrChangeAttrExpl = CurItem->Type == fqitDeleteExploreDir ||
                                       CurItem->Type == fqitChAttrsExploreDir ||
                                       CurItem->Type == fqitChAttrsExploreDirLink;
            BOOL uploadFinished = FALSE;
            // The result of the "LIST" command is in 'ListCmdReplyCode' and 'ListCmdReplyText'
            if (ShouldStop)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // Since we are already inside the CSocketsThread::CritSect section, this call
                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                    if (WorkerDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                    DeleteSocket(WorkerDataCon);
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // check whether the worker should stop
            }
            else
            {
                if (WorkerDataCon != NULL) // "always true"
                {
                    // VMS (cs.felk.cvut.cz) reports an error even for an empty directory (cannot be considered an error).
                    BOOL isVMSFileNotFound = ListCmdReplyText != NULL && FTPIsEmptyDirListErrReply(ListCmdReplyText);
                    int listCmdReplyCode = ListCmdReplyCode;
                    DWORD err;
                    BOOL lowMem, noDataTransTimeout;
                    int sslErrorOccured;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // Since we are already inside the CSocketsThread::CritSect section, this call
                    // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                    if (WorkerDataCon->IsConnected()) // close the "data connection", the system will attempt a "graceful"
                    {
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState2(): data connection has left opened!");
                    }
                    WorkerDataCon->GetError(&err, &lowMem, NULL, &noDataTransTimeout, &sslErrorOccured, NULL);
                    if (!WorkerDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                        errBuf[0] = 0;
                    if (lowMem) // the "data connection" reports out-of-memory ("always false")
                    {
                        // Since we are already inside the CSocketsThread::CritSect section, this call
                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;

                        // Item error; record this state into it.
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                    else
                    {
                        if (err != NO_ERROR && !IsConnected())
                        { // the LIST reply did arrive, but while waiting for the transfer to finish through the data connection
                            // the connection was interrupted (both data connection and control connection) -> RETRY

                            // Since we are already inside the CSocketsThread::CritSect section, this call
                            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                            DeleteSocket(WorkerDataCon);
                            WorkerDataCon = NULL;
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            WorkerDataConState = wdcsDoesNotExist;
                            conClosedRetryItem = TRUE;
                        }
                        else
                        {
                            BOOL listingIsNotOK = FTP_DIGIT_1(listCmdReplyCode) != FTP_D1_SUCCESS &&
                                                      FTP_DIGIT_2(listCmdReplyCode) == FTP_D2_CONNECTION ||
                                                  err != NO_ERROR || sslErrorOccured != SSLCONERR_NOERROR ||
                                                  FTP_DIGIT_1(listCmdReplyCode) != FTP_D1_SUCCESS &&
                                                      FTP_DIGIT_2(listCmdReplyCode) != FTP_D2_CONNECTION &&
                                                      !isVMSFileNotFound;
                            BOOL decomprErr = FALSE;
                            int allocatedListingLen = 0;
                            char* allocatedListing = NULL;
                            if (!listingIsNotOK)
                            {
                                // obtain the data from the "data connection"
                                allocatedListing = WorkerDataCon->GiveData(&allocatedListingLen, &decomprErr);
                                if (decomprErr) // on decompression error discard the result and display the error
                                {
                                    listingIsNotOK = TRUE;
                                    allocatedListingLen = 0;
                                    free(allocatedListing);
                                    allocatedListing = NULL;
                                }
                            }
                            // Since we are already inside the CSocketsThread::CritSect section, this call
                            // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                            DeleteSocket(WorkerDataCon);
                            WorkerDataCon = NULL;
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            WorkerDataConState = wdcsDoesNotExist;

                            if (listingIsNotOK)
                            {
                                if (sslErrorOccured == SSLCONERR_UNVERIFIEDCERT ||
                                    ReuseSSLSessionFailed && (FTP_DIGIT_1(listCmdReplyCode) == FTP_D1_TRANSIENTERROR ||
                                                              FTP_DIGIT_1(listCmdReplyCode) == FTP_D1_ERROR))
                                {
                                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                    if (IsConnected()) // "Manually" close the control connection.
                                    {
                                        // Since we are already inside the CSocketsThread::CritSect section, this call
                                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                                        ForceClose(); // Sending QUIT would be cleaner, but a certificate change is very unlikely, so it is not worth bothering with that ;-)
                                    }
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                                    conClosedRetryItem = TRUE;
                                }
                                else
                                {
                                    BOOL quickRetry = FALSE;
                                    if (FTP_DIGIT_1(listCmdReplyCode) == FTP_D1_TRANSIENTERROR &&
                                            (FTP_DIGIT_2(listCmdReplyCode) == FTP_D2_CONNECTION ||          // especially "426 data connection closed, transfer aborted" (I cannot tell whether it is the server admin or a connection failure, so the connection failure takes priority -> retry the download)
                                             FTP_DIGIT_2(listCmdReplyCode) == FTP_D2_FILESYSTEM) &&         // "450 Transfer aborted.  Link to file server lost."
                                            sslErrorOccured != SSLCONERR_DONOTRETRY ||                      // take 426 and 450 only if they were not caused by an error: encryption of the connection failed, it is a permanent problem
                                        noDataTransTimeout ||                                               // connection interrupted by us because of a no-data-transfer timeout (happens with "50%" network outages; the data connection is not broken but the data transfer stalls and stays open for up to 14000 seconds; this should handle it) -> retry the download
                                        sslErrorOccured == SSLCONERR_CANRETRY ||                            // encryption of the connection failed; it is not a permanent problem
                                        FTP_DIGIT_1(listCmdReplyCode) == FTP_D1_SUCCESS && err != NO_ERROR) // data connection interrupted after receiving a "successful" reply from the server (error while waiting for the data transfer to finish); this happened on ftp.simtel.net (six connections at once + packet drops)
                                    {
                                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will certainly handle this item (no need to post "new work available")
                                        quickRetry = TRUE;
                                    }
                                    else
                                    {
                                        if (sslErrorOccured != SSLCONERR_NOERROR)
                                            lstrcpyn(errText, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 200 + FTP_MAX_PATH);
                                        else
                                        {
                                            errText[0] = 0;
                                            if (FTP_DIGIT_1(listCmdReplyCode) != FTP_D1_SUCCESS &&
                                                (FTP_DIGIT_2(listCmdReplyCode) == FTP_D2_CONNECTION ||
                                                 FTP_DIGIT_2(listCmdReplyCode) != FTP_D2_CONNECTION && !isVMSFileNotFound) &&
                                                ListCmdReplyText != NULL)
                                            { // if we do not have a network error description from the server, use the system description
                                                lstrcpyn(errText, ListCmdReplyText, 200 + FTP_MAX_PATH);
                                            }

                                            if (errText[0] == 0 && errBuf[0] != 0) // try to take the error text from the proxy server
                                                lstrcpyn(errText, errBuf, 200 + FTP_MAX_PATH);

                                            if (errText[0] == 0 && decomprErr)
                                                lstrcpyn(errText, LoadStr(IDS_ERRDATACONDECOMPRERROR), 200 + FTP_MAX_PATH);
                                        }

                                        // Item error; record this state into it.
                                        Queue->UpdateItemState(CurItem, sqisFailed,
                                                               UploadDirGetTgtPathListing ? ITEMPR_UPLOADCANNOTLISTTGTPATH : ITEMPR_INCOMPLETELISTING,
                                                               err, (errText[0] != 0 ? SalamanderGeneral->DupStr(errText) : NULL), Oper);
                                    }
                                    if (!quickRetry &&
                                        FTP_DIGIT_1(listCmdReplyCode) != FTP_D1_SUCCESS &&
                                        FTP_DIGIT_2(listCmdReplyCode) != FTP_D2_CONNECTION &&
                                        !isVMSFileNotFound)
                                    {
                                        if (listingNotAccessible != NULL)
                                            *listingNotAccessible = TRUE;
                                    }
                                    lookForNewWork = TRUE;
                                }
                            }
                            else // the listing succeeded and is complete
                            {
                                // store the date when the listing was created
                                CFTPDate listingDate;
                                listingDate.Year = StartTimeOfListing.wYear;
                                listingDate.Month = (BYTE)StartTimeOfListing.wMonth;
                                listingDate.Day = (BYTE)StartTimeOfListing.wDay;

                                CFTPServerPathType pathType = ftpsptUnknown;
                                if (UploadDirGetTgtPathListing)
                                    pathType = Oper->GetFTPServerPathType(tgtPath);
                                else
                                {
                                    if (HaveWorkingPath)
                                        pathType = Oper->GetFTPServerPathType(WorkingPath);
                                    else
                                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState2(): WorkingPath is unknown!");
                                }
                                char userTmp[USER_MAX_SIZE];
                                if (!delOrChangeAttrExpl && !UploadDirGetTgtPathListing) // when deleting, changing attributes, and during upload we do not store listings in the cache because they will change immediately
                                {                                                        // download only: if the user wants to use the cache, add the newly loaded listing into the cache
                                    if (HaveWorkingPath && Oper->GetUseListingsCache() && allocatedListing != NULL)
                                    {
                                        unsigned short port;
                                        Oper->GetUserHostPort(NULL, host, &port);
                                        Oper->GetUser(userTmp, USER_MAX_SIZE);
                                        Oper->GetListCommand(buf, 200 + FTP_MAX_PATH);
                                        ListingCache.AddOrUpdatePathListing(host, port, userTmp, pathType, WorkingPath,
                                                                            buf, Oper->GetEncryptControlConnection(),
                                                                            allocatedListing, allocatedListingLen,
                                                                            &listingDate, StartLstTimeOfListing);
                                    }
                                }

                                char* welcomeReply = Oper->AllocServerFirstReply();
                                char* systReply = Oper->AllocServerSystemReply();
                                char listingServerType[SERVERTYPE_MAX_SIZE];
                                Oper->GetListingServerType(listingServerType);
                                BOOL err2 = allocatedListing == NULL;

                                if (UploadDirGetTgtPathListing) // upload listing: store the listing in the cache
                                {
                                    err2 |= welcomeReply == NULL || systReply == NULL;
                                    if (!err2)
                                    {
                                        unsigned short port;
                                        Oper->GetUserHostPort(userTmp, host, &port);
                                        // The call UploadListingCache.ListingFinished() is possible only because we are in the CSocketsThread::CritSect section.
                                        err2 = !UploadListingCache.ListingFinished(userTmp, host, port, tgtPath,
                                                                                   pathType, allocatedListing, allocatedListingLen,
                                                                                   listingDate, welcomeReply, systReply,
                                                                                   listingServerType[0] != 0 ? listingServerType : NULL);
                                        uploadFinished = !err2;
                                    }
                                }
                                else // download + delete + change-attrs: parse the listing and add new items to the queue
                                {
                                    BOOL isVMS = pathType == ftpsptOpenVMS;
                                    BOOL isAS400 = pathType == ftpsptAS400;
                                    TIndirectArray<CFTPQueueItem>* ftpQueueItems = new TIndirectArray<CFTPQueueItem>(100, 500);
                                    BOOL needSimpleListing = TRUE;
                                    int transferMode = Oper->GetTransferMode(); // parameter for Copy and Move operations
                                    BOOL selFiles, selDirs, includeSubdirs;     // parameters for the Change Attributes operation
                                    DWORD attrAndMask, attrOrMask;
                                    int operationsUnknownAttrs;
                                    Oper->GetParamsForChAttrsOper(&selFiles, &selDirs, &includeSubdirs, &attrAndMask,
                                                                  &attrOrMask, &operationsUnknownAttrs);
                                    int operationsHiddenFileDel;
                                    int operationsHiddenDirDel;
                                    Oper->GetParamsForDeleteOper(NULL, &operationsHiddenFileDel, &operationsHiddenDirDel);

                                    // variables for Copy and Move operations:
                                    CQuadWord totalSize(0, 0); // total size (in bytes or blocks)
                                    BOOL sizeInBytes = TRUE;   // TRUE/FALSE = sizes in bytes/blocks (cannot alternate on one listing - see CFTPListingPluginDataInterface::GetSize())

                                    // reset the helper variable to determine which server type has already been (unsuccessfully) tested
                                    CServerTypeList* serverTypeList = Config.LockServerTypeList();
                                    int serverTypeListCount = serverTypeList->Count;
                                    int j;
                                    for (j = 0; j < serverTypeListCount; j++)
                                        serverTypeList->At(j)->ParserAlreadyTested = FALSE;

                                    CServerType* serverType = NULL;
                                    err2 |= ftpQueueItems == NULL || !HaveWorkingPath;
                                    if (!err2)
                                    {
                                        if (listingServerType[0] != 0) // this is not autodetection; find listingServerType
                                        {
                                            int i;
                                            for (i = 0; i < serverTypeListCount; i++)
                                            {
                                                serverType = serverTypeList->At(i);
                                                const char* s = serverType->TypeName;
                                                if (*s == '*')
                                                    s++;
                                                if (SalamanderGeneral->StrICmp(listingServerType, s) == 0)
                                                {
                                                    // serverType is selected; let us test its parser on the listing.
                                                    serverType->ParserAlreadyTested = TRUE;
                                                    if (ParseListingToFTPQueue(ftpQueueItems, allocatedListing, allocatedListingLen,
                                                                               serverType, &err2, isVMS, isAS400, transferMode, &totalSize,
                                                                               &sizeInBytes, selFiles, selDirs,
                                                                               includeSubdirs, attrAndMask, attrOrMask,
                                                                               operationsUnknownAttrs, operationsHiddenFileDel,
                                                                               operationsHiddenDirDel))
                                                    {
                                                        needSimpleListing = FALSE; // we parsed the listing successfully
                                                    }
                                                    break; // we found the required server type; finish
                                                }
                                            }
                                            if (i == serverTypeListCount)
                                                listingServerType[0] = 0; // listingServerType does not exist -> run autodetection
                                        }

                                        // autodetection - select the server type with the satisfied autodetection condition.
                                        if (!err2 && needSimpleListing && listingServerType[0] == 0)
                                        {
                                            if (welcomeReply == NULL || systReply == NULL)
                                                err2 = TRUE;
                                            else
                                            {
                                                int welcomeReplyLen = (int)strlen(welcomeReply);
                                                int systReplyLen = (int)strlen(systReply);
                                                int i;
                                                for (i = 0; i < serverTypeListCount; i++)
                                                {
                                                    serverType = serverTypeList->At(i);
                                                    if (!serverType->ParserAlreadyTested) // only if we have not tried it yet
                                                    {
                                                        if (serverType->CompiledAutodetCond == NULL)
                                                        {
                                                            serverType->CompiledAutodetCond = CompileAutodetectCond(HandleNULLStr(serverType->AutodetectCond),
                                                                                                                    NULL, NULL, NULL, NULL, 0);
                                                            if (serverType->CompiledAutodetCond == NULL) // this can only be a low-memory error
                                                            {
                                                                err2 = TRUE;
                                                                break;
                                                            }
                                                        }
                                                        if (serverType->CompiledAutodetCond->Evaluate(welcomeReply, welcomeReplyLen,
                                                                                                      systReply, systReplyLen))
                                                        {
                                                            // serverType is selected; let us test its parser on the listing.
                                                            serverType->ParserAlreadyTested = TRUE;
                                                            if (ParseListingToFTPQueue(ftpQueueItems, allocatedListing, allocatedListingLen,
                                                                                       serverType, &err2, isVMS, isAS400, transferMode, &totalSize,
                                                                                       &sizeInBytes, selFiles, selDirs,
                                                                                       includeSubdirs, attrAndMask, attrOrMask,
                                                                                       operationsUnknownAttrs, operationsHiddenFileDel,
                                                                                       operationsHiddenDirDel) ||
                                                                err2)
                                                            {
                                                                if (!err2)
                                                                {
                                                                    const char* s = serverType->TypeName;
                                                                    if (*s == '*')
                                                                        s++;
                                                                    lstrcpyn(listingServerType, s, SERVERTYPE_MAX_SIZE);
                                                                }
                                                                needSimpleListing = err2; // either we parsed the listing successfully or a low-memory error occurred; exit
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // autodetection - select the remaining server types
                                            if (!err2 && needSimpleListing)
                                            {
                                                int i;
                                                for (i = 0; i < serverTypeListCount; i++)
                                                {
                                                    serverType = serverTypeList->At(i);
                                                    if (!serverType->ParserAlreadyTested) // only if we have not tried it yet
                                                    {
                                                        // serverType is selected; let us test its parser on the listing.
                                                        // serverType->ParserAlreadyTested = TRUE;  // pointless, not used later
                                                        if (ParseListingToFTPQueue(ftpQueueItems, allocatedListing, allocatedListingLen,
                                                                                   serverType, &err2, isVMS, isAS400, transferMode, &totalSize,
                                                                                   &sizeInBytes, selFiles, selDirs,
                                                                                   includeSubdirs, attrAndMask, attrOrMask,
                                                                                   operationsUnknownAttrs, operationsHiddenFileDel,
                                                                                   operationsHiddenDirDel) ||
                                                            err2)
                                                        {
                                                            if (!err2)
                                                            {
                                                                const char* s = serverType->TypeName;
                                                                if (*s == '*')
                                                                    s++;
                                                                lstrcpyn(listingServerType, s, SERVERTYPE_MAX_SIZE);
                                                            }
                                                            needSimpleListing = err2; // either we parsed the listing successfully or a low-memory error occurred; exit
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    Config.UnlockServerTypeList();
                                    if (!err2)
                                    {
                                        if (needSimpleListing) // unknown listing format
                                        {                      // write "Unknown Server Type" to the log
                                            lstrcpyn(errText, LoadStr(listingServerType[0] == 0 ? IDS_LOGMSGUNKNOWNSRVTYPE : IDS_LOGMSGUNKNOWNSRVTYPE2),
                                                     199 + FTP_MAX_PATH);
                                            Logs.LogMessage(LogUID, errText, -1, TRUE);

                                            // Item error; record this state into it.
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOPARSELISTING, NO_ERROR, NULL, Oper);
                                            lookForNewWork = TRUE;
                                        }
                                        else // log which parser handled it
                                        {
                                            if (listingServerType[0] != 0) // "always true"
                                            {
                                                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGPARSEDBYSRVTYPE), listingServerType);
                                                Logs.LogMessage(LogUID, errText, -1, TRUE);
                                            }

                                            BOOL nonEmptyDirSkipOrAsk = FALSE;
                                            if (CurItem->Type == fqitDeleteExploreDir && ftpQueueItems->Count > 0 &&
                                                ((CFTPQueueItemDelExplore*)CurItem)->IsTopLevelDir)
                                            { // deleting a non-empty directory - verify how the user wants it to behave
                                                int confirmDelOnNonEmptyDir;
                                                Oper->GetParamsForDeleteOper(&confirmDelOnNonEmptyDir, NULL, NULL);
                                                switch (confirmDelOnNonEmptyDir)
                                                {
                                                case NONEMPTYDIRDEL_USERPROMPT:
                                                {
                                                    // Item error; record this state into it.
                                                    Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_DIRISNOTEMPTY, NO_ERROR, NULL, Oper);
                                                    lookForNewWork = TRUE;
                                                    nonEmptyDirSkipOrAsk = TRUE;
                                                    break;
                                                }

                                                case NONEMPTYDIRDEL_DELETEIT:
                                                    break;

                                                case NONEMPTYDIRDEL_SKIP:
                                                {
                                                    // Item error; record this state into it.
                                                    Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_DIRISNOTEMPTY, NO_ERROR, NULL, Oper);
                                                    lookForNewWork = TRUE;
                                                    nonEmptyDirSkipOrAsk = TRUE;
                                                    break;
                                                }
                                                }
                                            }
                                            if (!nonEmptyDirSkipOrAsk)
                                            {
                                                // if necessary add a "parent" item (for example when deleting it is the item for
                                                // delete the directory after all files/links/subdirectories inside have been deleted
                                                CFTPQueueItemType type = fqitNone;
                                                BOOL ok = TRUE;
                                                CFTPQueueItem* item = NULL;
                                                CFTPQueueItemState state = sqisWaiting;
                                                DWORD problemID = ITEMPR_OK;
                                                BOOL skip = TRUE; // TRUE if the file/directory/link is not supposed to be processed at all
                                                switch (CurItem->Type)
                                                {
                                                case fqitDeleteExploreDir:   // explore directories for delete (note: directory links are removed as a whole, the operation is fulfilled and nothing extra is deleted) (object of class CFTPQueueItemDelExplore)
                                                case fqitMoveExploreDir:     // explore directories for move (after completion the directory is deleted) (object of class CFTPQueueItemCopyMoveExplore)
                                                case fqitMoveExploreDirLink: // explore a directory link for move (after completion delete the directory link) (object of class CFTPQueueItemCopyMoveExplore)
                                                {
                                                    skip = FALSE;
                                                    type = (CurItem->Type == fqitMoveExploreDir ? fqitMoveDeleteDir : (CurItem->Type == fqitDeleteExploreDir ? fqitDeleteDir : fqitMoveDeleteDirLink));

                                                    item = new CFTPQueueItemDir;
                                                    if (item != NULL && !((CFTPQueueItemDir*)item)->SetItemDir(0, 0, 0, 0))
                                                        ok = FALSE;
                                                    break;
                                                }

                                                case fqitChAttrsExploreDir: // explore directories for attribute changes (also adds an item for changing directory attributes) (object of class CFTPQueueItemChAttrExplore)
                                                {
                                                    if (selDirs) // relevant only when attributes should be set on the inspected directory
                                                    {
                                                        skip = FALSE;
                                                        type = fqitChAttrsDir;
                                                        const char* rights = ((CFTPQueueItemChAttrExplore*)CurItem)->OrigRights;

                                                        // calculate new permissions for the file/directory
                                                        DWORD actAttr;
                                                        DWORD attrDiff = 0;
                                                        BOOL attrErr = FALSE;
                                                        if (rights != NULL && GetAttrsFromUNIXRights(&actAttr, &attrDiff, rights))
                                                        {
                                                            DWORD changeMask = (~attrAndMask | attrOrMask) & 0777;
                                                            if ((attrDiff & changeMask) == 0 &&                                                  // we do not change any unknown attribute
                                                                (actAttr & changeMask) == (((actAttr & attrAndMask) | attrOrMask) & changeMask)) // we do not change any known attribute
                                                            {                                                                                    // nothing to do (no attribute change)
                                                                skip = TRUE;
                                                            }
                                                            else
                                                            {
                                                                if (((attrDiff & attrAndMask) & attrOrMask) != (attrDiff & attrAndMask))
                                                                {                        // problem: an unknown attribute needs to be preserved, which we cannot do
                                                                    actAttr |= attrDiff; // put at least 'x' there when we do not know 's' or 't' or whatever is there now (see UNIX permissions)
                                                                    attrErr = TRUE;
                                                                }
                                                                actAttr = (actAttr & attrAndMask) | attrOrMask;
                                                            }
                                                        }
                                                        else // unknown permissions
                                                        {
                                                            actAttr = attrOrMask; // assume no permissions (actAttr==0)
                                                            if (((~attrAndMask | attrOrMask) & 0777) != 0777)
                                                            { // problem: permissions are unknown and some attribute must be preserved (we do not know its value -> we cannot keep it)
                                                                attrErr = TRUE;
                                                            }
                                                        }

                                                        if (!skip)
                                                        {
                                                            if (attrErr)
                                                            {
                                                                switch (operationsUnknownAttrs)
                                                                {
                                                                case UNKNOWNATTRS_IGNORE:
                                                                    attrErr = FALSE;
                                                                    break;

                                                                case UNKNOWNATTRS_SKIP:
                                                                {
                                                                    state = sqisSkipped;
                                                                    problemID = ITEMPR_UNKNOWNATTRS;
                                                                    break;
                                                                }

                                                                default: // UNKNOWNATTRS_USERPROMPT
                                                                {
                                                                    state = sqisUserInputNeeded;
                                                                    problemID = ITEMPR_UNKNOWNATTRS;
                                                                    break;
                                                                }
                                                                }
                                                            }
                                                            if (!attrErr)
                                                                rights = NULL; // if everything is OK, there is no reason to remember the original permissions

                                                            item = new CFTPQueueItemChAttrDir;
                                                            if (item != NULL)
                                                            {
                                                                if (!((CFTPQueueItemChAttrDir*)item)->SetItemDir(0, 0, 0, 0))
                                                                    ok = FALSE;
                                                                else
                                                                    ((CFTPQueueItemChAttrDir*)item)->SetItemChAttrDir((WORD)actAttr, rights, attrErr);
                                                            }
                                                        }
                                                    }
                                                    break;
                                                }

                                                    // No item needs to be added for:
                                                    // case fqitCopyExploreDir:         // explore a directory or a link to a directory for copying (object of class CFTPQueueItemCopyMoveExplore)
                                                    // case fqitChAttrsExploreDirLink:  // explore a directory link for attribute changes (object of class CFTPQueueItem)
                                                }

                                                BOOL parentItemAdded = FALSE;       // TRUE = there is a "parent" item at the end of ftpQueueItems (for example deleting a directory (Delete and Move), changing directory attributes (Change Attrs))
                                                int parentUID = CurItem->ParentUID; // parent UID for items created by expanding the directory
                                                if (item != NULL)
                                                {
                                                    if (ok)
                                                    {
                                                        item->SetItem(CurItem->ParentUID, type, state, problemID, CurItem->Path, CurItem->Name);
                                                        ftpQueueItems->Add(item); // adding the operation to the queue
                                                        if (!ftpQueueItems->IsGood())
                                                        {
                                                            ftpQueueItems->ResetState();
                                                            ok = FALSE;
                                                        }
                                                        else
                                                        {
                                                            parentItemAdded = TRUE;
                                                            parentUID = item->UID;
                                                        }
                                                    }
                                                    if (!ok)
                                                    {
                                                        err2 = TRUE;
                                                        delete item;
                                                    }
                                                    item = NULL;
                                                }
                                                else
                                                {
                                                    if (!skip) // only if it is not skipping the item but a low-memory error
                                                    {
                                                        TRACE_E(LOW_MEMORY);
                                                        err2 = TRUE;
                                                    }
                                                }

                                                if (!err2)
                                                {
                                                    // For items created by exploring the directory (does not apply to a possible "parent" item at the end of the array):
                                                    // set the parents and count items in the states "Skipped", "Failed", and those other than Done
                                                    int count = ftpQueueItems->Count - (parentItemAdded ? 1 : 0);
                                                    int childItemsNotDone = 0;
                                                    int childItemsSkipped = 0;
                                                    int childItemsFailed = 0;
                                                    int childItemsUINeeded = 0;
                                                    int i;
                                                    for (i = 0; i < count; i++)
                                                    {
                                                        CFTPQueueItem* actItem = ftpQueueItems->At(i);
                                                        actItem->ParentUID = parentUID;
                                                        switch (actItem->GetItemState())
                                                        {
                                                        case sqisDone:
                                                            break;
                                                        case sqisSkipped:
                                                            childItemsSkipped++;
                                                            break;
                                                        case sqisUserInputNeeded:
                                                            childItemsUINeeded++;
                                                            break;

                                                        case sqisFailed:
                                                        case sqisForcedToFail:
                                                            childItemsFailed++;
                                                            break;

                                                        default:
                                                            childItemsNotDone++;
                                                            break;
                                                        }
                                                    }
                                                    childItemsNotDone += childItemsSkipped + childItemsFailed + childItemsUINeeded;

                                                    // when adding a "parent" item, set the counts of Skipped+Failed+NotDone in it
                                                    if (parentItemAdded)
                                                    {
                                                        CFTPQueueItemDir* parentItem = (CFTPQueueItemDir*)(ftpQueueItems->At(ftpQueueItems->Count - 1)); // it must necessarily be a descendant of CFTPQueueItemDir (each "parent" item has the counts Skipped+Failed+NotDone)
                                                        parentItem->SetStateAndNotDoneSkippedFailed(childItemsNotDone, childItemsSkipped,
                                                                                                    childItemsFailed, childItemsUINeeded);
                                                        // Now all new items are represented only by the "parent" item -> count the new
                                                        // NotDone + Skipped + Failed + UINeeded apply only to this item
                                                        childItemsNotDone = 1;
                                                        childItemsFailed = 0;
                                                        childItemsSkipped = 0;
                                                        childItemsUINeeded = 0;
                                                        switch (parentItem->GetItemState())
                                                        {
                                                        case sqisDone:
                                                            childItemsNotDone = 0;
                                                            break;
                                                        case sqisSkipped:
                                                            childItemsSkipped = 1;
                                                            break;
                                                        case sqisUserInputNeeded:
                                                            childItemsUINeeded = 1;
                                                            break;

                                                        case sqisFailed:
                                                        case sqisForcedToFail:
                                                            childItemsFailed = 1;
                                                            break;
                                                        }
                                                    }

                                                    int curItemParent = CurItem->ParentUID;

                                                    // multiple operations are in progress on the data; others must wait until all of them are finished,
                                                    // otherwise they will work with inconsistent data.
                                                    Queue->LockForMoreOperations();

                                                    if (Queue->ReplaceItemWithListOfItems(CurItem->UID, ftpQueueItems->GetData(),
                                                                                          ftpQueueItems->Count))
                                                    { // CurItem has already been deallocated; it was replaced by the list of ftpQueueItems entries
                                                        CurItem = NULL;
                                                        ftpQueueItems->DetachMembers(); // the items are already in the queue; they must be removed from the array or they will be deallocated

                                                        // We will consider this path successfully traversed (we collect paths to detect simple cycles).
                                                        if (HaveWorkingPath)
                                                            Oper->AddToExploredPaths(WorkingPath);

                                                        // For the item/operation CurItem->ParentUID decrease NotDone by one (for CurItem in state sqisProcessing) and increase NotDone + Skipped + Failed + UINeeded according to
                                                        // childItemsNotDone + childItemsSkipped + childItemsFailed + childItemsUINeeded
                                                        childItemsNotDone--; // decrease by one for CurItem
                                                        Oper->AddToItemOrOperationCounters(curItemParent, childItemsNotDone,
                                                                                           childItemsSkipped, childItemsFailed,
                                                                                           childItemsUINeeded, FALSE);

                                                        Queue->UnlockForMoreOperations();

                                                        // increase the total transferred data size by the size from the new items
                                                        // (applies only to Copy and Move; otherwise the size is zero).
                                                        Oper->AddToTotalSize(totalSize, sizeInBytes);

                                                        Oper->ReportItemChange(-1); // request a redraw of all items

                                                        // this worker will have to find other work.
                                                        State = fwsLookingForWork; // no need to call Oper->OperationStatusMaybeChanged(); it does not change the operation state (it is not paused and will not be after this change)
                                                        SubState = fwssNone;
                                                        postActivate = TRUE; // post an activation for the next worker state
                                                        reportWorkerChange = TRUE;

                                                        // Inform all potentially sleeping workers that new work has appeared.
                                                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                                        // Since we are already inside the CSocketsThread::CritSect section, this call
                                                        // It can also be called from the CSocket::SocketCritSect section (no risk of deadlock).
                                                        Oper->PostNewWorkAvailable(FALSE);
                                                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                                                    }
                                                    else
                                                    {
                                                        err2 = TRUE; // out of memory -> write the error into the item
                                                        Queue->UnlockForMoreOperations();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if (ftpQueueItems != NULL)
                                        delete ftpQueueItems;
                                }

                                if (err2)
                                {
                                    // Item error; record this state into it.
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                if (welcomeReply != NULL)
                                    SalamanderGeneral->Free(welcomeReply);
                                if (systReply != NULL)
                                    SalamanderGeneral->Free(systReply);
                                if (allocatedListing != NULL)
                                {
                                    memset(allocatedListing, 0, allocatedListingLen); // it may involve sensitive data, better to zero it
                                    free(allocatedListing);
                                }
                            }
                        }
                    }
                }
            }
            ListCmdReplyCode = -1;
            if (ListCmdReplyText != NULL)
            {
                SalamanderGeneral->Free(ListCmdReplyText);
                ListCmdReplyText = NULL;
            }

            // If we are exploring directories for delete/change-attr or doing an upload listing, we must
            // reset the speed meter (explore speed and upload listing are not measured; this
            // that moment can therefore be the start of measuring speed for delete/change-attr/upload operations),
            // so that the time-left is shown correctly in the operation dialog.
            if (delOrChangeAttrExpl || UploadDirGetTgtPathListing)
            {
                Oper->GetGlobalTransferSpeedMeter()->Clear();
                Oper->GetGlobalTransferSpeedMeter()->JustConnected();
            }
            if (uploadFinished)
            {
                UploadDirGetTgtPathListing = FALSE;
                StatusType = wstNone;
                SubState = fwssWorkStartWork;
                postActivate = TRUE;       // trigger to continue working
                reportWorkerChange = TRUE; // we need to hide any progress while fetching the listing

                // Since we are already inside the CSocketsThread::CritSect section, this call
                // It can also be called from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no risk of deadlock).
                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // cancel any timer from the previous work
            }
            break;
        }
        }
        if (!nextLoop)
            break;
    }
}
