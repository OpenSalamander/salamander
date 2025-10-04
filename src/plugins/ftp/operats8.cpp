// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

BOOL CFTPWorker::HandleFlushDataError(CFTPQueueItemCopyOrMove* curItem, BOOL& lookForNewWork)
{
    CFlushDataError flushDataError = FlushDataError;
    FlushDataError = fderNone;
    switch (flushDataError)
    {
    case fderASCIIForBinaryFile:
    {
        // return the state of the target file to its original state (except when it is deleted because
        // the file for resume was too small and it was overwritten)
        CloseOpenedFile(TRUE, FALSE, NULL, NULL,
                        !ResumingOpenedFile,                                  // delete the target file if we created it (even if it was overwritten because it was too small for resume)
                        ResumingOpenedFile ? &OpenedFileOriginalSize : NULL); // trim the bytes added to the end of the file if we resumed it

        int asciiTrModeButBinFile = Oper->GetAsciiTrModeButBinFile();
        switch (asciiTrModeButBinFile)
        {
        case ASCIITRFORBINFILE_USERPROMPT:
        {
            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_ASCIITRFORBINFILE, NO_ERROR, NULL, Oper);
            break;
        }

        case ASCIITRFORBINFILE_INBINMODE: // it will be transferred again, just in binary mode
        {
            Queue->UpdateAsciiTransferMode(curItem, FALSE);                                // next time in binary mode
            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will surely take care of this item (no need to post "new work available")
            break;
        }

        default: // ASCIITRFORBINFILE_SKIP
        {
            if (asciiTrModeButBinFile != ASCIITRFORBINFILE_SKIP)
                TRACE_E("CFTPWorker::HandleFlushDataError(): Unexpected value of Oper->GetAsciiTrModeButBinFile()!");
            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_ASCIITRFORBINFILE, NO_ERROR, NULL, Oper);
            break;
        }
        }
        lookForNewWork = TRUE;
        return TRUE;
    }

    case fderLowMemory:
    {
        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
        lookForNewWork = TRUE;
        return TRUE;
    }

    case fderWriteError:
    {
        if (DiskWork.ProblemID == ITEMPR_RESUMETESTFAILED)
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME), -1, TRUE);
        if (DiskWork.ProblemID == ITEMPR_RESUMETESTFAILED &&
            curItem->TgtFileState != TGTFILESTATE_RESUMED)
        { // Resume or Overwrite: resume failed, perform Overwrite
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
            Queue->UpdateForceAction(CurItem, fqiaOverwrite);
            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will surely take care of this item (no need to post "new work available")
        }
        else
        {
            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
        }
        lookForNewWork = TRUE;
        return TRUE;
    }
    }
    return FALSE;
}

void CFTPWorker::HandleEventInWorkingState3(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                            char* buf, char* errBuf, int& cmdLen, BOOL& sendCmd,
                                            char* reply, int replySize, int replyCode, char* errText,
                                            BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                            BOOL& handleShouldStop)
{
    CFTPQueueItemCopyOrMove* curItem = (CFTPQueueItemCopyOrMove*)CurItem;
    char hostBuf[HOST_MAX_SIZE];
    char userBuf[USER_MAX_SIZE];
    unsigned short portBuf;

    // ensure flushing the data from the data connection to the disk thread and once the flush is complete
    // return the buffer to the data connection (if it is already closed, just deallocate the buffer)
    if (!ShouldStop && WorkerDataCon != NULL && event == fweDataConFlushData ||
        event == fweDiskWorkWriteFinished)
    {
        if (!ShouldStop && WorkerDataCon != NULL && event == fweDataConFlushData)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            char* flushBuffer;
            int validBytesInFlushBuffer;
            BOOL deleteTgtFile;
            // since we are already in the CSocketsThread::CritSect section, this call
            // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
            BOOL haveFlushData = WorkerDataCon->GiveFlushData(&flushBuffer, &validBytesInFlushBuffer, &deleteTgtFile);

            HANDLES(EnterCriticalSection(&WorkerCritSect));
            if (deleteTgtFile) // we need to delete the target file because it may contain corrupted data
            {
                // return the state of the target file to its original state (except when it is deleted because
                // the file for resume was too small and it was overwritten)
                CloseOpenedFile(TRUE, FALSE, NULL, NULL,
                                !ResumingOpenedFile,                                  // delete the target file if we created it (even if it was overwritten because it was too small for resume)
                                ResumingOpenedFile ? &OpenedFileOriginalSize : NULL); // trim the bytes added to the end of the file if we resumed it
                // if we are waiting for data flushing to finish or for the data connection to finish, it is necessary
                // to post fweActivate to continue processing the item
                if (SubState == fwssWorkCopyWaitForDataConFinish ||
                    SubState == fwssWorkCopyFinishFlushData)
                {
                    postActivate = TRUE;
                }
            }
            else
            {
                if (haveFlushData) // we have 'flushBuffer', we must pass it to the disk thread (free it in case of error)
                {
                    if (curItem->AsciiTransferMode && !curItem->IgnoreAsciiTrModeForBinFile &&
                        CurrentTransferMode == ctrmASCII &&
                        Oper->GetAsciiTrModeButBinFile() != ASCIITRFORBINFILE_IGNORE &&
                        !SalamanderGeneral->IsANSIText(flushBuffer, validBytesInFlushBuffer))
                    {
                        if (flushBuffer != NULL)
                            free(flushBuffer);

                        if (WorkerDataCon != NULL)
                        {
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            // since we are already in the CSocketsThread::CritSect section, this call
                            // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                            if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                            WorkerDataCon->FreeFlushData();
                            DeleteSocket(WorkerDataCon);
                            WorkerDataCon = NULL;
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            WorkerDataConState = wdcsDoesNotExist;
                        }

                        FlushDataError = fderASCIIForBinaryFile;
                        // if we are waiting for data flushing to finish or for the data connection to finish, it is necessary
                        // to post fweActivate to continue processing the item
                        if (SubState == fwssWorkCopyWaitForDataConFinish ||
                            SubState == fwssWorkCopyFinishFlushData)
                        {
                            postActivate = TRUE;
                        }
                    }
                    else
                    {
                        if (DiskWorkIsUsed)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState3(): DiskWorkIsUsed may not be TRUE here!");
                        InitDiskWork(WORKER_DISKWORKWRITEFINISHED, fdwtCheckOrWriteFile, NULL, NULL,
                                     fqiaNone, FALSE, flushBuffer, &OpenedFileCurOffset,
                                     ResumingOpenedFile ? (OpenedFileSize > OpenedFileCurOffset ? &OpenedFileSize : &OpenedFileCurOffset) : &OpenedFileCurOffset,
                                     validBytesInFlushBuffer, OpenedFile);
                        if (FTPDiskThread->AddWork(&DiskWork))
                            DiskWorkIsUsed = TRUE;
                        else // cannot flush the data, the item processing cannot continue
                        {
                            if (DiskWork.FlushDataBuffer != NULL)
                            {
                                free(DiskWork.FlushDataBuffer);
                                DiskWork.FlushDataBuffer = NULL;
                            }

                            if (WorkerDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // since we are already in the CSocketsThread::CritSect section, this call
                                // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                                if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                                    WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                                WorkerDataCon->FreeFlushData();
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }

                            FlushDataError = fderLowMemory;
                            // if we are waiting for data flushing to finish or for the data connection to finish, it is necessary
                            // to post fweActivate to continue processing the item
                            if (SubState == fwssWorkCopyWaitForDataConFinish ||
                                SubState == fwssWorkCopyFinishFlushData)
                            {
                                postActivate = TRUE;
                            }
                        }
                    }
                }
                else
                    TRACE_E("CFTPWorker::HandleEventInWorkingState3(): received fweDataConFlushData, but data-connection has nothing to flush");
            }
        }
        else // event == fweDiskWorkWriteFinished
        {
            DiskWorkIsUsed = FALSE;
            ReportWorkerMayBeClosed(); // announce the worker has finished (for other waiting threads)

            // if we are waiting for the data flush to finish, we need to post fweActivate
            // so the item processing can continue
            if (SubState == fwssWorkCopyFinishFlushData)
                postActivate = TRUE;

            if (DiskWork.State == sqisNone) // the data flush succeeded
            {
                if (WorkerDataCon != NULL) // if the data connection exists, return the buffer for reuse
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                    WorkerDataCon->FlushDataFinished(DiskWork.FlushDataBuffer, TRUE);
                    if (!WorkerDataCon->IsConnected())
                    {
                        DWORD dataConError;
                        BOOL dataConLowMem;
                        BOOL dataConNoDataTransTimeout;
                        int dataSSLErrorOccured;
                        BOOL dataConDecomprErrorOccured;
                        WorkerDataCon->GetError(&dataConError, &dataConLowMem, NULL, &dataConNoDataTransTimeout,
                                                &dataSSLErrorOccured, &dataConDecomprErrorOccured);
                        //BOOL dataConDecomprMissingStreamEnd = WorkerDataCon->GetDecomprMissingStreamEnd(); // unfortunately this check is unusable, e.g. Serv-U 7 and 8 simply do not terminate the stream
                        BOOL allDataFlushed = WorkerDataCon->AreAllDataFlushed(TRUE);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        DataConAllDataTransferred = !dataConDecomprErrorOccured && /*!dataConDecomprMissingStreamEnd &&*/
                                                    dataSSLErrorOccured == SSLCONERR_NOERROR &&
                                                    dataConError == NO_ERROR && !dataConLowMem &&
                                                    !dataConNoDataTransTimeout && allDataFlushed;
                    }
                    else
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                else // if the data connection no longer exists, release the buffer
                {
                    if (DiskWork.FlushDataBuffer != NULL)
                        free(DiskWork.FlushDataBuffer);
                }
                DiskWork.FlushDataBuffer = NULL;

                // compute the new file offset and the file size
                OpenedFileCurOffset += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0);
                if (OpenedFileCurOffset > OpenedFileSize)
                    OpenedFileSize = OpenedFileCurOffset;
            }
            else // an error occurred
            {
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }

                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                    if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                    WorkerDataCon->FreeFlushData();
                    DeleteSocket(WorkerDataCon);
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                FlushDataError = fderWriteError;
                // if we are waiting for the data connection to finish, we need to post fweActivate
                // so the item processing can continue
                if (SubState == fwssWorkCopyWaitForDataConFinish)
                    postActivate = TRUE;
            }
        }
    }
    else
    {
        while (1)
        {
            BOOL nextLoopCopy = FALSE;
            switch (SubState)
            {
            case fwssWorkSimpleCmdStartWork: // start of the work (the working directory is already set)
            {
                if (ShouldStop)
                    handleShouldStop = TRUE; // check whether the worker should stop
                else
                {
                    if (curItem->TgtFileState == TGTFILESTATE_TRANSFERRED)
                    { // if the file has already been transferred, only deleting the source file remains for Move
                        SubState = fwssWorkCopyTransferFinished;
                        nextLoopCopy = TRUE;
                    }
                    else
                    {
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        if (LockedFileUID != 0)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState3(): LockedFileUID != 0!");
                        if (FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->Path,
                                                    Oper->GetFTPServerPathType(curItem->Path),
                                                    curItem->Name, &LockedFileUID, ffatRead))
                        { // the file on the server is not open yet, we can work with it, allocate the data connection
                            if (WorkerDataCon != NULL)
                                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState3(): WorkerDataCon is not NULL before starting data-connection!");
                            DataConAllDataTransferred = FALSE;
                            CFTPProxyForDataCon* dataConProxyServer;
                            if (Oper->AllocProxyForDataCon(&dataConProxyServer))
                            {
                                CCertificate* cert = Oper->GetCertificate();
                                WorkerDataCon = new CDataConnectionSocket(TRUE, dataConProxyServer, Oper->GetEncryptDataConnection(), cert, Oper->GetCompressData(), this);
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
                                    // since we are already in the CSocketsThread::CritSect section, this call
                                    // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
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

                                // error on the item, record this state
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                lookForNewWork = TRUE;
                            }
                            else // the data connection object was allocated successfully
                            {
                                WorkerDataConState = wdcsOnlyAllocated;

                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // since we are already in the CSocketsThread::CritSect section, this call
                                // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                                WorkerDataCon->SetPostMessagesToWorker(TRUE, Msg, UID,
                                                                       WORKER_DATACON_CONNECTED,
                                                                       WORKER_DATACON_CLOSED,
                                                                       WORKER_DATACON_FLUSHDATA,
                                                                       WORKER_DATACON_LISTENINGFORCON);
                                WorkerDataCon->SetGlobalTransferSpeedMeter(Oper->GetGlobalTransferSpeedMeter());
                                WorkerDataCon->SetGlobalLastActivityTime(Oper->GetGlobalLastActivityTime());
                                HANDLES(EnterCriticalSection(&WorkerCritSect));

                                if (Oper->GetUsePassiveMode()) // passive mode (PASV)
                                {
                                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                      ftpcmdPassive, &cmdLen); // cannot report an error
                                    sendCmd = TRUE;
                                    SubState = fwssWorkCopyWaitForPASVRes;
                                }
                                else // active mode (PORT)
                                {
                                    nextLoopCopy = TRUE;
                                    SubState = fwssWorkCopyOpenActDataCon;
                                }
                            }
                        }
                        else // another operation is already running on this file, let the user try again later
                        {
                            // error on the item, record this state in it
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_SRCFILEINUSE, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                    }
                }
                break;
            }

            case fwssWorkCopyWaitForPASVRes: // copy/move of a file: waiting for the result of "PASV" (obtain IP+port for the passive data connection)
            {
                WaitForPASVRes(event, reply, replySize, replyCode, handleShouldStop, nextLoopCopy, conClosedRetryItem,
                               fwssWorkCopySetType, fwssWorkCopyOpenActDataCon);
                break;
            }

            case fwssWorkCopyOpenActDataCon: // copy/move of a file: open the active data connection
            {
                OpenActDataCon(fwssWorkCopyWaitForListen, errBuf, conClosedRetryItem, lookForNewWork);
                break;
            }

            case fwssWorkCopyWaitForListen: // copy/move of a file: waiting for the "listen" port to open (opening the active data connection) - local or on the proxy server
            {
                WaitForListen(event, handleShouldStop, errBuf, buf, cmdLen, sendCmd,
                              conClosedRetryItem, fwssWorkCopyWaitForPORTRes);
                break;
            }

            case fwssWorkCopyWaitForPORTRes: // copy/move of a file: waiting for the result of "PORT" (pass the IP+port to the server for the active data connection)
            {
                WaitForPORTRes(event, nextLoopCopy, conClosedRetryItem, fwssWorkCopySetType);
                break;
            }

            case fwssWorkCopySetType: // copy/move of a file: set the desired transfer mode (ASCII / binary)
            {
                SetTypeA(handleShouldStop, errBuf, buf, cmdLen, sendCmd, nextLoopCopy,
                         (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary),
                         curItem->AsciiTransferMode, fwssWorkCopyWaitForTYPERes, fwssWorkCopyResumeFile);
                break;
            }

            case fwssWorkCopyWaitForTYPERes: // copy/move of a file: waiting for the result of "TYPE" (switch to the ASCII / binary data transfer mode)
            {
                WaitForTYPERes(event, replyCode, nextLoopCopy, conClosedRetryItem,
                               (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary), fwssWorkCopyResumeFile);
                break;
            }

            case fwssWorkCopyResumeFile: // copy/move of a file: optionally handle resuming the file (send the REST command)
            {
                if (ShouldStop)
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
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
                    int resumeOverlap = Config.GetResumeOverlap();
                    int resumeMinFileSize = Config.GetResumeMinFileSize();
                    OpenedFileCurOffset.Set(0, 0);
                    OpenedFileResumedAtOffset.Set(0, 0);
                    if (OpenedFileSize >= CQuadWord(resumeMinFileSize, 0)) // resume makes sense (we have a portion of the file larger than the minimum for resume)
                    {
                        ResumingOpenedFile = TRUE;
                        if (OpenedFileSize > CQuadWord(resumeOverlap, 0)) // REST will have a positive number in the parameter
                        {
                            if (Oper->GetResumeIsNotSupported()) // optimization: we know REST will fail, so we will not send it
                            {
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMENOTSUP), -1, TRUE);
                                nextLoopCopy = TRUE;
                                SubState = fwssWorkCopyResumeError;
                            }
                            else // send REST
                            {
                                OpenedFileCurOffset = OpenedFileSize - CQuadWord(resumeOverlap, 0);
                                char num[50];
                                _ui64toa(OpenedFileCurOffset.Value, num, 10);

                                OpenedFileResumedAtOffset = OpenedFileCurOffset;

                                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                  ftpcmdRestartTransfer, &cmdLen, num); // cannot report an error
                                sendCmd = TRUE;
                                SubState = fwssWorkCopyWaitForResumeRes;
                            }
                        }
                        else // REST is unnecessary, we read the file from the beginning, but still perform Resume (check the existing part, then write) -> start reading the file
                        {
                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMEFROMBEG), -1, TRUE);
                            nextLoopCopy = TRUE;
                            SubState = fwssWorkCopySendRetrCmd;
                        }
                    }
                    else // Overwrite/Create-New or REST is unnecessary, read the file from the beginning -> start reading the file
                    {
                        if (OpenedFileSize > CQuadWord(0, 0))
                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMEUSELESS), -1, TRUE);
                        ResumingOpenedFile = FALSE;
                        nextLoopCopy = TRUE;
                        SubState = fwssWorkCopySendRetrCmd;
                    }
                }
                break;
            }

            case fwssWorkCopyWaitForResumeRes: // copy/move of a file: waiting for the result of the "REST" command (resume the file)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // ignore "1xx" responses (they are only written to the Log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_PARTIALSUCCESS ||
                        FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // success returned (should be 350, but we accept 2xx as well)
                    {
                        nextLoopCopy = TRUE;
                        SubState = fwssWorkCopySendRetrCmd;
                    }
                    else // 4xx and 5xx
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) // optimization: store in the operation that REST is not supported
                            Oper->SetResumeIsNotSupported(TRUE);
                        nextLoopCopy = TRUE;
                        SubState = fwssWorkCopyResumeError;
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr) -> try to restore it
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
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
                break;
            }

            case fwssWorkCopyResumeError: // copy/move of a file: the "REST" command failed (not implemented, etc.) or we already know REST will fail
            {
                if (curItem->TgtFileState == TGTFILESTATE_RESUMED) // Overwrite is not possible, record the error and find other work
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                        WorkerDataCon->FreeFlushData();
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESUME, NO_ERROR,
                                           NULL, Oper);
                    lookForNewWork = TRUE;
                }
                else // resume failed, perform Overwrite
                {
                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                    ResumingOpenedFile = FALSE;
                    OpenedFileCurOffset.Set(0, 0);
                    OpenedFileResumedAtOffset.Set(0, 0);
                    nextLoopCopy = TRUE;
                    SubState = fwssWorkCopySendRetrCmd;
                }
                break;
            }

            case fwssWorkCopySendRetrCmd: // copy/move of a file: send the RETR command (start reading the file, optionally from the offset specified by resume)
            {
                if (ShouldStop)
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
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
                    CQuadWord size;
                    if (WorkerDataCon != NULL && curItem->Size != CQuadWord(-1, -1) &&
                        (curItem->SizeInBytes || Oper->GetApproxByteSize(&size, curItem->Size)))
                    {
                        if (curItem->SizeInBytes)
                            size = curItem->Size;
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerDataCon->SetDataTotalSize(size);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    if (FlushDataError != fderNone)
                    {
                        TRACE_E("CFTPWorker::HandleEventInWorkingState3(): fwssWorkCopySendRetrCmd: unexpected value of FlushDataError: " << FlushDataError);
                        FlushDataError = fderNone;
                    }

                    CommandTransfersData = TRUE;
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdRetrieveFile, &cmdLen, curItem->Name); // cannot report an error
                    sendCmd = TRUE;

                    postActivate = TRUE;
                    SubState = fwssWorkCopyActivateDataCon;
                }
                break;
            }

            case fwssWorkCopyActivateDataCon: // copy/move of a file: activate the data connection (right after sending the RETR command)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                    WorkerDataCon->ActivateConnection();
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                SubState = fwssWorkCopyWaitForRETRRes;
                if (event != fweActivate)
                    nextLoopCopy = TRUE; // if it was not just fweActivate, deliver the event to the fwssWorkCopyWaitForRETRRes state
                break;
            }

            case fwssWorkCopyWaitForRETRRes: // copy/move of a file: waiting for the result of "RETR" (waiting for the file read to finish)
            {
                switch (event)
                {
                case fweCmdInfoReceived: // "1xx" responses contain the size of the transferred data
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerDataCon->EncryptPassiveDataCon();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    CQuadWord size;
                    if (!ResumingOpenedFile && // during resume some servers return the file size and others the remaining size to download (there is no way to tell which one it is, so they cannot be used)
                        FTPGetDataSizeInfoFromSrvReply(size, reply, replySize))
                    {
                        //                if (ResumingOpenedFile && ) // WARNING, NOT ALWAYS TRUE: during resume we do not receive the total file size, only the resumed part -> must add it to 'size'
                        //                  size += OpenedFileResumedAtOffset;
                        if (!curItem->SizeInBytes || curItem->Size != size)
                        { // write the newly determined file size into the item (for overall progress + conversion of block/record/etc. sizes to bytes)
                            if (!curItem->SizeInBytes)
                                Oper->AddBlkSizeInfo(size, curItem->Size);
                            Queue->UpdateFileSize(curItem, size, TRUE, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request the item to be redrawn
                        }
                        // we have the total file size - 'size'
                        if (WorkerDataCon != NULL)
                        {
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            WorkerDataCon->SetDataTotalSize(size);
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                        }
                    }
                    break;
                }

                case fweCmdReplyReceived:
                {
                    ListCmdReplyCode = replyCode;
                    CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                    if (ListCmdReplyText != NULL)
                        SalamanderGeneral->Free(ListCmdReplyText);
                    ListCmdReplyText = SalamanderGeneral->DupStr(errText); /* low memory = we can live without the reply description */

                    BOOL waitForDataConFinish = FALSE;
                    if (!ShouldStop && WorkerDataCon != NULL)
                    {
                        BOOL trFinished;
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS)
                        { // the server reports an error retrieving the file
                            // since we are already in the CSocketsThread::CritSect section, this call
                            // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                            if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                        }
                        else
                        {
                            if (WorkerDataCon->IsConnected()) // the data transfer is still running, wait for completion
                            {
                                waitForDataConFinish = TRUE;
                                if (!WorkerDataCon->IsTransfering(&trFinished) && !trFinished)
                                { // connection has not been established - wait 5 seconds, then possibly report an error (if the connection still has not been established and ListCmdReplyCode is success)
                                    // since we are already in the CSocketsThread::CritSect section, this call
                                    // is also possible from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no dead-lock risk)
                                    SocketsThread->DeleteTimer(UID, WORKER_DATACONSTARTTIMID);
                                    // since we are already in the CSocketsThread::CritSect section, this call
                                    // is also possible from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no dead-lock risk)
                                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + 20000,
                                                            WORKER_DATACONSTARTTIMID, NULL); // ignore the error; at worst the user presses Stop
                                }
                            }
                        }
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    else
                    {
                        if (WorkerDataCon != NULL)
                        {
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            // since we are already in the CSocketsThread::CritSect section, this call
                            // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                            if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                                WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                        }
                    }

                    // if we do not have to wait for the "data connection" to finish, proceed to finish flushing the data from the "data connection"
                    SubState = waitForDataConFinish ? fwssWorkCopyWaitForDataConFinish : fwssWorkCopyFinishFlushData;
                    if (!waitForDataConFinish)
                        nextLoopCopy = TRUE;
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr) -> try to restore it
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                        if (WorkerDataCon->IsConnected())       // close the "data connection", the system tries to do a "graceful"
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                        WorkerDataCon->FreeFlushData();
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // process any potential error while flushing the data
                    if (!HandleFlushDataError(curItem, lookForNewWork))
                    {
                        if (DataConAllDataTransferred && CommandReplyTimeout)
                        { // we successfully received all data + the server did not respond => in 99.9% the file is completely downloaded and only the control connection is stuck (happens during uploads/downloads longer than 1.5 hours) => force Resume (it tests the file size + sets the date/time, etc.)
                            _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGDWNLOADFORCRESUM), curItem->Name);
                            Logs.LogMessage(LogUID, errText, -1, TRUE);

                            Queue->UpdateForceAction(CurItem, fqiaResume);
                        }
                        conClosedRetryItem = TRUE;
                    }
                    break;
                }
                }
                break;
            }

            case fwssWorkCopyWaitForDataConFinish: // copy/move of a file: wait for the "data connection" to finish (the server reply to "RETR" has already arrived)
            {
                BOOL con = FALSE;
                if (WorkerDataCon != NULL)
                {
                    int retrReply = ListCmdReplyCode;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    if (event == fweDataConStartTimeout) // if the connection still has not been established, there is no point in waiting longer +
                    {                                    // if ListCmdReplyCode is success, retry the operation
                        BOOL trFinished;
                        if (WorkerDataCon->IsConnected() &&
                            !WorkerDataCon->IsTransfering(&trFinished) && !trFinished)
                        {                                                 // close the "data connection", the system tries to do a "graceful"
                            WorkerDataCon->CloseSocketEx(NULL);           // shutdown (we will not learn about the result)
                            if (FTP_DIGIT_1(retrReply) == FTP_D1_SUCCESS) // retry the operation (the server returned success, but the data connection did not even open, so something is wrong)
                            {
                                // since we are already in the CSocketsThread::CritSect section, this call
                                // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                                WorkerDataCon->FreeFlushData();
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONNOTOPENED), -1, TRUE);
                                // the data connection simply did not open (it did not make it or an error occurred) -> retry the download attempt
                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will surely take care of this item (no need to post "new work available")
                                lookForNewWork = TRUE;
                                break; // no need to handle HandleFlushDataError here, because without establishing the data connection no FlushDataError != fderNone can occur
                            }
                        }
                    }
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                    con = WorkerDataCon->IsConnected();
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                if (WorkerDataCon == NULL || !con) // either the "data connection" does not exist or it is already closed
                {
                    nextLoopCopy = TRUE;
                    SubState = fwssWorkCopyFinishFlushData;
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // is also possible from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no dead-lock risk)
                    SocketsThread->DeleteTimer(UID, WORKER_DATACONSTARTTIMID);
                }
                break;
            }

            case fwssWorkCopyFinishFlushData: // copy/move of a file: ensure the data flush from the data connection finishes (it is already closed)
            {
                BOOL done = !DiskWorkIsUsed; // TRUE only if a data flush is not in progress
                if (done && WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // since we are already in the CSocketsThread::CritSect section, this call
                    // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                    done = WorkerDataCon->AreAllDataFlushed(FALSE);
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                if (done) // everything necessary has already been written to disk
                {
                    SubState = fwssWorkCopyProcessRETRRes;
                    nextLoopCopy = TRUE;
                }
                else
                {
                    if (event == fweWorkerShouldStop && ShouldStop)
                    {                                                        // closing the control connection while waiting for the data flush to finish on disk
                        SubState = fwssWorkCopyFinishFlushDataAfterQuitSent; // avoid sending "QUIT" multiple times
                        sendQuitCmd = TRUE;                                  // we should finish and have an open connection -> send the server the "QUIT" command (ignore the reply; it should close the connection and nothing else matters now)
                    }
                }
                break;
            }

            case fwssWorkCopyFinishFlushDataAfterQuitSent:
                break; // copy/move of a file: after sending "QUIT" wait for the control connection to close and for the data flush to finish on disk

            case fwssWorkCopyProcessRETRRes: // copy/move of a file: process the result of "RETR" (after the "data connection" closed, data were flushed to disk, and the server response to "RETR" was received)
            {
                // all data are flushed (the data connection contains no data and is closed; DiskWorkIsUsed == FALSE)
                // the result of the "RETR" command is in 'ListCmdReplyCode' and 'ListCmdReplyText'

                // process any potential error while flushing the data
                if (!HandleFlushDataError(curItem, lookForNewWork))
                {
                    // deallocate the data-connection object; we no longer need it (it is empty)
                    BOOL dataConExisted = WorkerDataCon != NULL;
                    DWORD dataConError = 0;
                    BOOL dataConLowMem = FALSE;
                    BOOL dataConNoDataTransTimeout = FALSE;
                    int dataSSLErrorOccured = SSLCONERR_NOERROR;
                    BOOL dataConDecomprErrorOccured = FALSE;
                    //BOOL dataConDecomprMissingStreamEnd = FALSE;  // unfortunately this check is unusable, e.g. Serv-U 7 and 8 simply do not terminate the stream
                    errBuf[0] = 0;
                    if (dataConExisted)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerDataCon->GetError(&dataConError, &dataConLowMem, NULL, &dataConNoDataTransTimeout,
                                                &dataSSLErrorOccured, &dataConDecomprErrorOccured);
                        //dataConDecomprMissingStreamEnd = WorkerDataCon->GetDecomprMissingStreamEnd();
                        if (!WorkerDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                            errBuf[0] = 0;
                        // since we are already in the CSocketsThread::CritSect section, this call
                        // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                        // the data connection should be closed already, so closing it is probably redundant, but we play it safe...
                        if (WorkerDataCon->IsConnected()) // close the "data connection", the system tries to do a "graceful"
                        {
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn about the result)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState3(): data connection has left opened!");
                        }
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    if (ShouldStop)
                        handleShouldStop = TRUE; // check whether the worker should stop
                    else
                    {
                        if (dataConExisted) // "always true"
                        {
                            if (dataConLowMem) // the "data connection" reports low memory ("always false")
                            {
                                // error on the item, record this state
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                lookForNewWork = TRUE;
                            }
                            else
                            {
                                if (dataConError != NO_ERROR && !IsConnected())
                                { // the response to RETR arrived, but while waiting for the data connection to finish
                                    // the connection was interrupted (both the data and control connections) -> RETRY
                                    conClosedRetryItem = TRUE;
                                }
                                else
                                {
                                    if (FTP_DIGIT_1(ListCmdReplyCode) != FTP_D1_SUCCESS ||
                                        dataConError != NO_ERROR || dataSSLErrorOccured != SSLCONERR_NOERROR ||
                                        dataConDecomprErrorOccured /*|| dataConDecomprMissingStreamEnd*/)
                                    {
                                        if (dataSSLErrorOccured == SSLCONERR_UNVERIFIEDCERT ||
                                            ReuseSSLSessionFailed && (FTP_DIGIT_1(ListCmdReplyCode) == FTP_D1_TRANSIENTERROR ||
                                                                      FTP_DIGIT_1(ListCmdReplyCode) == FTP_D1_ERROR))
                                        {
                                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                            if (IsConnected()) // "manually" close the control connection
                                            {
                                                // since we are already in the CSocketsThread::CritSect section, this call
                                                // is also possible from the CSocket::SocketCritSect section (no dead-lock risk)
                                                ForceClose(); // it would be cleaner to send QUIT, but a certificate change is very unlikely, so it is not worth the hassle :-)
                                            }
                                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                                            conClosedRetryItem = TRUE;
                                        }
                                        else
                                        {
                                            if (!dataConDecomprErrorOccured &&
                                                (FTP_DIGIT_1(ListCmdReplyCode) == FTP_D1_TRANSIENTERROR &&
                                                     (FTP_DIGIT_2(ListCmdReplyCode) == FTP_D2_CONNECTION ||  // mainly "426 data connection closed, transfer aborted" (whether it was the server admin or a connection issue is impossible to tell, so we prioritize a connection issue -> retry the download)
                                                      FTP_DIGIT_2(ListCmdReplyCode) == FTP_D2_FILESYSTEM) && // "450 Transfer aborted.  Link to file server lost."
                                                     dataSSLErrorOccured != SSLCONERR_DONOTRETRY ||          // accept 426 and 450 only if they were not caused by an error: encryption failed, which is a permanent problem
                                                 dataConNoDataTransTimeout ||                                // connection interrupted by us due to a no-data-transfer timeout (happens with "50%" network dropouts; the data connection stays open but the data transfer blocks, it can stay open for 14000 seconds, this should solve it) -> retry the download
                                                 dataSSLErrorOccured == SSLCONERR_CANRETRY))                 // encryption failed, but it is not a permanent problem
                                            {
                                                SubState = fwssWorkCopyDelayedAutoRetry; // use a delayed auto-retry so that all unexpected server responses have time to arrive
                                                // since we are already in the CSocketsThread::CritSect section, this call
                                                // is also possible from the CSocket::SocketCritSect and CFTPWorker::WorkerCritSect sections (no dead-lock risk)
                                                SocketsThread->AddTimer(Msg, UID, GetTickCount() + WORKER_DELAYEDAUTORETRYTIMEOUT,
                                                                        WORKER_DELAYEDAUTORETRYTIMID, NULL); // ignore the error; at worst the user presses Stop
                                            }
                                            else
                                            {
                                                if (!dataConDecomprErrorOccured &&
                                                    FTP_DIGIT_1(ListCmdReplyCode) == FTP_D1_SUCCESS && dataConError != NO_ERROR)   // data connection interrupted after receiving a "successful" response from the server (error while waiting for the data transfer to finish); this happened on ftp.simtel.net (six connections at once + packet loss) and occasionally when downloading PASV+SSL from a local Filezilla server
                                                {                                                                                  // use an immediate auto-retry; otherwise the speed degrades and we do not expect any unexpected responses from the server
                                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will surely take care of this item (no need to post "new work available")
                                                }
                                                else
                                                {
                                                    if (dataSSLErrorOccured != SSLCONERR_NOERROR || dataConDecomprErrorOccured)
                                                        lstrcpyn(errText, LoadStr(dataConDecomprErrorOccured ? IDS_ERRDATACONDECOMPRERROR : IDS_ERRDATACONSSLCONNECTERROR), 200 + FTP_MAX_PATH);
                                                    else
                                                    {
                                                        errText[0] = 0;
                                                        if (FTP_DIGIT_1(ListCmdReplyCode) != FTP_D1_SUCCESS && ListCmdReplyText != NULL)
                                                        { // if we do not have a network error description from the server, settle for the system description
                                                            lstrcpyn(errText, ListCmdReplyText, 200 + FTP_MAX_PATH);
                                                        }

                                                        if (errText[0] == 0 && errBuf[0] != 0) // try to take the error text from the proxy server
                                                            lstrcpyn(errText, errBuf, 200 + FTP_MAX_PATH);

                                                        //                              if (errText[0] == 0 && dataConDecomprMissingStreamEnd)
                                                        //                                lstrcpyn(errText, LoadStr(IDS_ERRDATACONDECOMPRERROR), 200 + FTP_MAX_PATH);
                                                    }

                                                    // error on the item, record this state
                                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INCOMPLETEDOWNLOAD, dataConError,
                                                                           (errText[0] != 0 ? SalamanderGeneral->DupStr(errText) : NULL), Oper);
                                                }
                                                lookForNewWork = TRUE;
                                            }
                                        }
                                    }
                                    else // the download succeeded and the file is complete - if this is a Move, delete the source file
                                    {
                                        if (ResumingOpenedFile && OpenedFileCurOffset < OpenedFileSize)
                                        { // the entire block at the end of the file was not tested (the server file is shorter than on disk -> the files differ and resume is not possible)
                                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME2), -1, TRUE);
                                            if (curItem->TgtFileState == TGTFILESTATE_RESUMED) // Overwrite is not possible, record the error and look for other work
                                            {
                                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_RESUMETESTFAILED, NO_ERROR, NULL, Oper);
                                                lookForNewWork = TRUE;
                                            }
                                            else // Resume or Overwrite: Resume failed, perform Overwrite
                                            {
                                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                                Queue->UpdateForceAction(CurItem, fqiaOverwrite);
                                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will surely take care of this item (no need to post "new work available")
                                                lookForNewWork = TRUE;
                                            }
                                        }
                                        else
                                        {
                                            CQuadWord size = OpenedFileSize; // backup of the file size, CloseOpenedFile resets the size to zero

                                            // transfer finished successfully, close the file with transferAborted == FALSE
                                            CloseOpenedFile(FALSE, curItem->DateAndTimeValid, &curItem->Date, &curItem->Time, FALSE, NULL);

                                            // mark the file as already transferred (for a Move we must distinguish this if deleting the source file fails)
                                            Queue->UpdateTgtFileState(curItem, TGTFILESTATE_TRANSFERRED);

                                            // if the size is in blocks, add the corresponding size pair for computing the average block size
                                            if (!curItem->SizeInBytes)
                                                Oper->AddBlkSizeInfo(size, curItem->Size);

                                            // write the actual file size into the item (for overall progress + conversion of block/record/etc. sizes to bytes)
                                            if (!curItem->SizeInBytes || curItem->Size != size)
                                            {
                                                Queue->UpdateFileSize(curItem, size, TRUE, Oper);
                                                Oper->ReportItemChange(CurItem->UID); // request the item to be redrawn
                                            }

                                            SubState = fwssWorkCopyTransferFinished;
                                            nextLoopCopy = TRUE;
                                        }
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
                break;
            }

            case fwssWorkCopyDelayedAutoRetry: // copy/move of a file: wait WORKER_DELAYEDAUTORETRYTIMEOUT milliseconds for the auto-retry (so all unexpected server responses can arrive)
            {
                if (event == fweDelayedAutoRetry) // it is time to perform the auto-retry
                {
                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will surely take care of this item (no need to post "new work available")
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkCopyTransferFinished: // copy/move of a file: the file is transferred; for Move delete the source file
            {
                if (CurItem->Type == fqitMoveFileOrFileLink)
                {
                    if (LockedFileUID != 0) // if the file is open for reading, we can close it now
                    {
                        FTPOpenedFiles.CloseFile(LockedFileUID);
                        LockedFileUID = 0;
                    }
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    if (LockedFileUID != 0)
                        TRACE_E("Unexpected situation 2 in CFTPWorker::HandleEventInWorkingState3(): LockedFileUID != 0!");
                    if (FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->Path,
                                                Oper->GetFTPServerPathType(curItem->Path),
                                                curItem->Name, &LockedFileUID, ffatDelete))
                    { // the file on the server is not open yet, we can try to delete it
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdDeleteFile, &cmdLen, CurItem->Name); // cannot report an error
                        sendCmd = TRUE;
                        SubState = fwssWorkCopyMoveWaitForDELERes;
                    }
                    else // another operation is already running on this file, let the user try again later
                    {
                        // error on the item, record this state
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_SRCFILEINUSE, NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                }
                else // Copy - nothing else to do, finished
                {
                    SubState = fwssWorkCopyDone;
                    nextLoopCopy = TRUE;
                }
                break;
            }

            case fwssWorkCopyMoveWaitForDELERes: // copy/move of a file: waiting for the result of "DELE" (Move: delete the source file/link after the transfer completes)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // ignore "1xx" responses (they are only written to the Log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    { // the source file/link was deleted successfully; that finishes it
                        SubState = fwssWorkCopyDone;
                        nextLoopCopy = TRUE;

                        // if the file/link was deleted, update the listing in the cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, CurItem->Path,
                                                        Oper->GetFTPServerPathType(CurItem->Path),
                                                        CurItem->Name, FALSE);
                    }
                    else // display the error to the user and process the next queue item
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETODELSRCFILE, NO_ERROR,
                                               SalamanderGeneral->DupStr(errText) /* low memory = the error will be without details */,
                                               Oper);
                        lookForNewWork = TRUE;
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr) -> try to restore it
                {
                    // if we do not know the result of deleting the file/link, invalidate the cache listing
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, CurItem->Path,
                                                    Oper->GetFTPServerPathType(CurItem->Path),
                                                    CurItem->Name, TRUE);

                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkCopyDone: // copy/move of a file: done, close the file and move to the next item
            {
                // the item finished successfully, record this state in it
                Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                lookForNewWork = TRUE;
                break;
            }
            }
            if (!nextLoopCopy)
                break;
        }

        // before closing the file we must cancel the disk work (so it does not operate on
        // a closed file handle)
        if (DiskWorkIsUsed && (conClosedRetryItem || lookForNewWork))
        {
            BOOL workIsInProgress;
            if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
            {
                if (workIsInProgress)
                    DiskWork.FlushDataBuffer = NULL; // the work is in progress; we cannot free the buffer with the data being written/tested and leave it to the disk-work thread (see the cancellation part) - we can write to DiskWork because after Cancel the disk thread must not access it anymore (for example it might no longer exist)
            }
            // if we cancelled the work before it started, we must free the flush buffer, and
            // if the work is already finished, free the flush buffer here, because fweDiskWorkWriteFinished
            // will now go elsewhere (where it is ignored)
            if (DiskWork.FlushDataBuffer != NULL)
            {
                free(DiskWork.FlushDataBuffer);
                DiskWork.FlushDataBuffer = NULL;
            }

            DiskWorkIsUsed = FALSE;
            ReportWorkerMayBeClosed(); // announce the worker has finished (for other waiting threads)
        }
    }
}
