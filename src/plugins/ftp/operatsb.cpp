// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

BOOL CFTPWorker::HandlePrepareDataError(CFTPQueueItemCopyOrMoveUpload* curItem, BOOL& lookForNewWork)
{
    CPrepareDataError prepareDataError = PrepareDataError;
    PrepareDataError = pderNone;
    switch (prepareDataError)
    {
    case pderASCIIForBinaryFile:
    {
        int asciiTrModeButBinFile = Oper->GetUploadAsciiTrModeButBinFile();
        switch (asciiTrModeButBinFile)
        {
        case ASCIITRFORBINFILE_USERPROMPT:
        {
            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_ASCIITRFORBINFILE, NO_ERROR, NULL, Oper);
            break;
        }

        case ASCIITRFORBINFILE_INBINMODE: // it will be transferred again, but in binary mode
        {
            Queue->UpdateAsciiTransferMode(curItem, FALSE);                                // next time already in binary mode
            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will surely handle this item (no need to post "new work available")
            break;
        }

        default: // ASCIITRFORBINFILE_SKIP
        {
            if (asciiTrModeButBinFile != ASCIITRFORBINFILE_SKIP)
                TRACE_E("CFTPWorker::HandlePrepareDataError(): Unexpected value of Oper->GetUploadAsciiTrModeButBinFile()!");
            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_ASCIITRFORBINFILE, NO_ERROR, NULL, Oper);
            break;
        }
        }
        lookForNewWork = TRUE;
        return TRUE;
    }

    case pderLowMemory:
    {
        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
        lookForNewWork = TRUE;
        return TRUE;
    }

    case pderReadError:
    {
        Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
        lookForNewWork = TRUE;
        return TRUE;
    }
    }
    return FALSE;
}

void CFTPWorker::HandleEventInWorkingState5(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                            BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                            int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                            int replyCode, char* ftpPath, char* errText,
                                            BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                            BOOL& handleShouldStop, BOOL& quitCmdWasSent)
{
    char hostBuf[HOST_MAX_SIZE];
    char userBuf[USER_MAX_SIZE];
    unsigned short portBuf;
    CFTPQueueItemCopyOrMoveUpload* curItem = (CFTPQueueItemCopyOrMoveUpload*)CurItem;
    CUploadListingItem* existingItem = NULL; // for passing listing item data between various SubStates

    if (!ShouldStop && WorkerUploadDataCon != NULL && event == fweUplDataConPrepareData ||
        event == fweDiskWorkReadFinished)
    {
        if (!ShouldStop && WorkerUploadDataCon != NULL && event == fweUplDataConPrepareData)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            char* flushBuffer;
            // because we are already inside CSocketsThread::CritSect, this call is also possible
            // from inside CSocket::SocketCritSect (no deadlock risk)
            BOOL haveBufferForData = WorkerUploadDataCon->GiveBufferForData(&flushBuffer);
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            if (haveBufferForData) // we have 'flushBuffer'; it must be handed over to the disk thread, where it will be filled with file data (if there is an error we release it)
            {
                if (DiskWorkIsUsed)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): DiskWorkIsUsed may not be TRUE here!");
                InitDiskWork(WORKER_DISKWORKREADFINISHED, curItem->AsciiTransferMode ? fdwtReadFileInASCII : fdwtReadFile,
                             NULL, NULL, fqiaNone, FALSE, flushBuffer, NULL, &OpenedInFileCurOffset, 0, OpenedInFile);
                if (FTPDiskThread->AddWork(&DiskWork))
                    DiskWorkIsUsed = TRUE;
                else // unable to prepare the data, we cannot continue processing the item
                {
                    if (DiskWork.FlushDataBuffer != NULL)
                    {
                        free(DiskWork.FlushDataBuffer);
                        DiskWork.FlushDataBuffer = NULL;
                    }

                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from inside CSocket::SocketCritSect (no deadlock risk)
                        if (WorkerUploadDataCon->IsConnected())       // close the data connection; the system will attempt a "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
                        WorkerUploadDataCon->FreeBufferedData();
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    PrepareDataError = pderLowMemory;
                }
            }
        }
        else // event == fweDiskWorkReadFinished
        {
            DiskWorkIsUsed = FALSE;
            ReportWorkerMayBeClosed(); // announce completion of the worker's job (for other waiting threads)

            if (DiskWork.State == sqisNone) // loading data into the buffer succeeded
            {
                if (DiskWork.FlushDataBuffer != NULL && WorkerUploadDataCon != NULL &&
                    curItem->AsciiTransferMode && !curItem->IgnoreAsciiTrModeForBinFile &&
                    CurrentTransferMode == ctrmASCII &&
                    Oper->GetUploadAsciiTrModeButBinFile() != ASCIITRFORBINFILE_IGNORE &&
                    !SalamanderGeneral->IsANSIText(DiskWork.FlushDataBuffer, DiskWork.ValidBytesInFlushDataBuffer))
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;

                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from inside CSocket::SocketCritSect (no deadlock risk)
                    if (WorkerUploadDataCon->IsConnected())       // close the data connection; the system will attempt a "graceful"
                        WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
                    WorkerUploadDataCon->FreeBufferedData();
                    DeleteSocket(WorkerUploadDataCon);
                    WorkerUploadDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;

                    PrepareDataError = pderASCIIForBinaryFile;
                }
                else
                {
                    if (WorkerUploadDataCon != NULL) // if the data connection exists, pass the buffer into it for writing into the data connection
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from inside CSocket::SocketCritSect (no deadlock risk)
                        WorkerUploadDataCon->DataBufferPrepared(DiskWork.FlushDataBuffer, DiskWork.ValidBytesInFlushDataBuffer, TRUE);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    else // if the data connection no longer exists, release the buffer
                    {
                        if (DiskWork.FlushDataBuffer != NULL)
                            free(DiskWork.FlushDataBuffer);
                    }
                    DiskWork.FlushDataBuffer = NULL;

                    // take over the new offset in the file (we cannot simply add it because text files convert all LF to CRLF)
                    OpenedInFileCurOffset = DiskWork.WriteOrReadFromOffset;
                    if (OpenedInFileCurOffset > OpenedInFileSize)
                        OpenedInFileSize = OpenedInFileCurOffset;
                    OpenedInFileNumberOfEOLs += CQuadWord(DiskWork.EOLsInFlushDataBuffer, 0);            // non-zero only for text files
                    OpenedInFileSizeWithCRLF_EOLs += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0); // only meaningful for text files
                }
            }
            else // an error occurred
            {
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }

                if (WorkerUploadDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from inside CSocket::SocketCritSect (no deadlock risk)
                    if (WorkerUploadDataCon->IsConnected())       // close the data connection; the system will attempt a "graceful"
                        WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we do not learn the result)
                    WorkerUploadDataCon->FreeBufferedData();
                    DeleteSocket(WorkerUploadDataCon);
                    WorkerUploadDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                PrepareDataError = pderReadError;
            }
        }
    }
    else
    {
        while (1)
        {
            BOOL nextLoop = FALSE;
            switch (SubState)
            {
            case fwssWorkStartWork: // determine the state of the target directory
            {
                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGUPLOADFILE), curItem->Name);
                Logs.LogMessage(LogUID, errText, -1, TRUE);

                if (ShouldStop)
                    handleShouldStop = TRUE; // check whether the worker should stop
                else
                {
                    if (curItem->TgtFileState != UPLOADTGTFILESTATE_TRANSFERRED)
                    {
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                        BOOL notAccessible, getListing, listingInProgress;
                        if (existingItem != NULL)
                            TRACE_E("CFTPWorker::HandleEventInWorkingState5(): unexpected situation: existingItem != NULL!");
                        if (UploadListingCache.GetListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                          pathType, Msg, UID, &listingInProgress,
                                                          &notAccessible, &getListing, curItem->TgtName,
                                                          &existingItem, NULL))
                        {
                            if (listingInProgress) // listing is currently taking place or is about to happen
                            {
                                if (getListing) // we are to obtain the listing and then notify any other waiting workers
                                {
                                    UploadDirGetTgtPathListing = TRUE;
                                    postActivate = TRUE; // post an impulse to start downloading the listing
                                }
                                else
                                {
                                    SubState = fwssWorkUploadWaitForListing; // we should wait until another worker finishes listing
                                    reportWorkerChange = TRUE;               // the worker prints the fwssWorkUploadWaitForListing state to the window, so a redraw is needed
                                }
                            }
                            else // the listing is already cached or marked as "unobtainable"
                            {
                                if (notAccessible) // the listing is cached, but only as "unobtainable"
                                {
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCANNOTLISTTGTPATH, 0, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                else // the listing is available; check for a possible file name collision
                                {
                                    nextLoop = TRUE;
                                    if (CurItem->ForceAction == fqiaUploadContinueAutorename) // continuing auto-rename (try another name + another STOR)
                                    {
                                        UploadType = utAutorename;
                                        if (curItem->RenamedName != NULL)
                                            TRACE_E("Unexpected situation 2 in CFTPWorker::HandleEventInWorkingState5(): curItem->RenamedName != NULL");
                                        UploadAutorenameNewName[0] = 0;
                                        SubState = fwssWorkUploadFileSetTgtPath;
                                    }
                                    else
                                    {
                                        if (CurItem->ForceAction == fqiaUploadForceAutorename) // from the previous processing of this item we know auto-rename must be used
                                            SubState = fwssWorkUploadAutorenameFile;
                                        else
                                        {
                                            BOOL nameValid = FTPMayBeValidNameComponent(curItem->TgtName, curItem->TgtPath, FALSE, pathType);
                                            if (existingItem == NULL && nameValid) // no collision and a valid name -> attempt to create the directory
                                                SubState = fwssWorkUploadNewFile;
                                            else
                                            {                                                              // if existingItem == NULL, then (!nameValid==TRUE), so tests for existingItem != NULL are unnecessary
                                                if (!nameValid || existingItem->ItemType == ulitDirectory) // invalid name or collision with a directory -> "file cannot be created"
                                                    SubState = !nameValid ? fwssWorkUploadCantCreateFileInvName : fwssWorkUploadCantCreateFileDirEx;
                                                else
                                                {
                                                    if (existingItem->ItemType == ulitFile) // collision with a file -> "file already exists"
                                                        SubState = fwssWorkUploadFileExists;
                                                    else // (existingItem->ItemType == ulitLink): collision with a link -> determine what the link is (file/directory)
                                                        SubState = fwssWorkUploadResolveLink;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else // insufficient memory
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                    }
                    else // the target file has already been uploaded; if this is a Move, try to delete the source file on disk
                    {
                        SubState = fwssWorkUploadCopyTransferFinished;
                        nextLoop = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForListing: // upload copy/move file: wait until another worker finishes listing the target path on the server (to detect collisions)
            {
                if (ShouldStop)
                    handleShouldStop = TRUE; // check whether the worker should stop
                else
                {
                    if (event == fweTgtPathListingFinished) // the designated worker has already finished, so try to use the new listing
                    {
                        SubState = fwssWorkStartWork;
                        reportWorkerChange = TRUE; // the worker prints the fwssWorkUploadWaitForListing state to the window, so a redraw is needed
                        nextLoop = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadResolveLink: // upload copy/move file: determine what the link is (file/directory) whose name collides with the target file name on the server
            {
                lstrcpyn(ftpPath, curItem->TgtPath, FTP_MAX_PATH);
                CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
                if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, curItem->TgtName, TRUE))
                { // we have the path; send CWD to the server into the directory being examined
                    _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGRESOLVINGLINK), ftpPath);
                    Logs.LogMessage(LogUID, errText, -1, TRUE);

                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // cannot report an error
                    sendCmd = TRUE;
                    SubState = fwssWorkUploadResLnkWaitForCWDRes;

                    HaveWorkingPath = FALSE; // changing the current working path on the server
                }
                else // path syntax error or the path would become too long
                {
                    // error on the item; record this state into it
                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INVALIDPATHTOLINK, NO_ERROR, NULL, Oper);
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadResLnkWaitForCWDRes: // upload copy/move file: waiting for the result of "CWD" (changing into the examined link - if it succeeds, the link points to a directory)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written into the log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||
                        FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)
                    {
                        if (ShouldStop)
                            handleShouldStop = TRUE; // check whether the worker should stop
                        else
                        {
                            nextLoop = TRUE;
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // success; the link points to a directory
                                SubState = fwssWorkUploadCantCreateFileDirEx;
                            else // permanent error; the link probably points to a file (but it could also be "550 Permission denied"; unfortunately 550 is also "550 Not a directory", so it is indistinguishable...)
                                SubState = fwssWorkUploadFileExists;
                        }
                    }
                    else // an error occurred; report it to the user and move on to the next queue item
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESOLVELNK, NO_ERROR,
                                               SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                               Oper);
                        lookForNewWork = TRUE;
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadCantCreateFileInvName: // upload copy/move file: handling the "target file cannot be created" error (invalid name)
            case fwssWorkUploadCantCreateFileDirEx:   // upload copy/move file: handling the "target file cannot be created" error (name already used for a directory or a link to a directory)
            {
                if (CurItem->ForceAction == fqiaUseAutorename) // forced auto-rename
                {
                    SubState = fwssWorkUploadAutorenameFile;
                    nextLoop = TRUE;
                }
                else
                {
                    switch (Oper->GetUploadCannotCreateFile())
                    {
                    case CANNOTCREATENAME_USERPROMPT:
                    {
                        Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTFILE,
                                               SubState == fwssWorkUploadCantCreateFileDirEx ? ERROR_ALREADY_EXISTS : NO_ERROR,
                                               NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }

                    case CANNOTCREATENAME_SKIP:
                    {
                        Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTFILE,
                                               SubState == fwssWorkUploadCantCreateFileDirEx ? ERROR_ALREADY_EXISTS : NO_ERROR,
                                               NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }

                    default: // case CANNOTCREATENAME_AUTORENAME:
                    {
                        SubState = fwssWorkUploadAutorenameFile;
                        nextLoop = TRUE;
                        break;
                    }
                    }
                }
                break;
            }

            case fwssWorkUploadFileExists: // upload copy/move file: handling the "target file already exists" error
            {
                nextLoop = TRUE;
                switch (CurItem->ForceAction)
                {
                case fqiaUseAutorename:
                    SubState = fwssWorkUploadAutorenameFile;
                    break;
                case fqiaUploadTestIfFinished:
                    SubState = fwssWorkUploadTestIfFinished;
                    break;
                case fqiaResume:
                    SubState = fwssWorkUploadResumeFile;
                    break;
                case fqiaResumeOrOverwrite:
                    SubState = fwssWorkUploadResumeOrOverwriteFile;
                    break;
                case fqiaOverwrite:
                    SubState = fwssWorkUploadOverwriteFile;
                    break;

                default: // no forced action: determine what the standard behavior should be
                {
                    switch (curItem->TgtFileState)
                    {
                    case UPLOADTGTFILESTATE_CREATED:
                    {
                        switch (Oper->GetUploadRetryOnCreatedFile())
                        {
                        case RETRYONCREATFILE_USERPROMPT:
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_RETRYONCREATFILE,
                                                   NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                            nextLoop = FALSE;
                            break;
                        }

                        case RETRYONCREATFILE_AUTORENAME:
                            SubState = fwssWorkUploadAutorenameFile;
                            break;
                        case RETRYONCREATFILE_RESUME:
                            SubState = fwssWorkUploadResumeFile;
                            break;
                        case RETRYONCREATFILE_RES_OVRWR:
                            SubState = fwssWorkUploadResumeOrOverwriteFile;
                            break;
                        case RETRYONCREATFILE_OVERWRITE:
                            SubState = fwssWorkUploadOverwriteFile;
                            break;

                        default: // case RETRYONCREATFILE_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_RETRYONCREATFILE,
                                                   NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                            nextLoop = FALSE;
                            break;
                        }
                        }
                        break;
                    }

                    case UPLOADTGTFILESTATE_RESUMED:
                    {
                        switch (Oper->GetUploadRetryOnResumedFile())
                        {
                        case RETRYONRESUMFILE_USERPROMPT:
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_RETRYONRESUMFILE,
                                                   NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                            nextLoop = FALSE;
                            break;
                        }

                        case RETRYONRESUMFILE_AUTORENAME:
                            SubState = fwssWorkUploadAutorenameFile;
                            break;
                        case RETRYONRESUMFILE_RESUME:
                            SubState = fwssWorkUploadResumeFile;
                            break;
                        case RETRYONRESUMFILE_RES_OVRWR:
                            SubState = fwssWorkUploadResumeOrOverwriteFile;
                            break;
                        case RETRYONRESUMFILE_OVERWRITE:
                            SubState = fwssWorkUploadOverwriteFile;
                            break;

                        default: // case RETRYONRESUMFILE_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_RETRYONRESUMFILE,
                                                   NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                            nextLoop = FALSE;
                            break;
                        }
                        }
                        break;
                    }

                    default: // case UPLOADTGTFILESTATE_UNKNOWN:
                    {
                        switch (Oper->GetUploadFileAlreadyExists())
                        {
                        case FILEALREADYEXISTS_USERPROMPT:
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADTGTFILEALREADYEXISTS,
                                                   NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                            nextLoop = FALSE;
                            break;
                        }

                        case FILEALREADYEXISTS_AUTORENAME:
                            SubState = fwssWorkUploadAutorenameFile;
                            break;
                        case FILEALREADYEXISTS_RESUME:
                            SubState = fwssWorkUploadResumeFile;
                            break;
                        case FILEALREADYEXISTS_RES_OVRWR:
                            SubState = fwssWorkUploadResumeOrOverwriteFile;
                            break;
                        case FILEALREADYEXISTS_OVERWRITE:
                            SubState = fwssWorkUploadOverwriteFile;
                            break;

                        default: // case FILEALREADYEXISTS_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADTGTFILEALREADYEXISTS,
                                                   NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                            nextLoop = FALSE;
                            break;
                        }
                        }
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadNewFile:               // upload copy/move file: the target file does not exist; go upload it
            case fwssWorkUploadAutorenameFile:        // upload copy/move file: resolving the target file creation error - auto-rename
            case fwssWorkUploadResumeFile:            // upload copy/move file: the problem "target file exists" - resume
            case fwssWorkUploadResumeOrOverwriteFile: // upload copy/move file: the problem "target file exists" - resume or overwrite
            case fwssWorkUploadOverwriteFile:         // upload copy/move file: the problem "target file exists" - overwrite
            case fwssWorkUploadTestIfFinished:        // upload copy/move file: the whole file was sent and the server simply did not reply; the file is probably OK, so test it
            {
                switch (SubState)
                {
                case fwssWorkUploadNewFile:
                {
                    UploadType = utNewFile;
                    SubState = fwssWorkUploadFileSetTgtPath;
                    nextLoop = TRUE;
                    break;
                }

                case fwssWorkUploadTestIfFinished:
                {
                    UploadType = utOnlyTestFileSize;
                    SubState = fwssWorkUploadFileSetTgtPath;
                    nextLoop = TRUE;
                    break;
                }

                case fwssWorkUploadResumeFile:
                {
                    if (curItem->AsciiTransferMode) // we cannot do resume in ASCII transfer mode (server implementations are too chaotic with CRLF conversions), better not write without verification
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADASCIIRESUMENOTSUP,
                                               NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                    else // binary mode; perform APPEND (resume)
                    {
                        if (Oper->GetResumeIsNotSupported()) // APPE is not implemented on this server
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESUME, NO_ERROR,
                                                   NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                        else
                        {
                            UploadType = utResumeFile;
                            SubState = fwssWorkUploadFileSetTgtPath;
                            nextLoop = TRUE;
                        }
                    }
                    break;
                }

                case fwssWorkUploadResumeOrOverwriteFile:
                {
                    if (Oper->GetResumeIsNotSupported() || // APPE is not implemented on this server
                        curItem->AsciiTransferMode)        // we cannot do resume in ASCII transfer mode (server implementations are too chaotic with CRLF conversions), better not write without verification,
                    {                                      // so perform overwrite immediately
                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLRESUMENOTSUP), -1, TRUE);
                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                        UploadType = utOverwriteFile;
                        SubState = fwssWorkUploadFileSetTgtPath;
                        nextLoop = TRUE;
                    }
                    else // binary mode; perform APPEND (resume); if it fails, try overwrite
                    {
                        UploadType = utResumeOrOverwriteFile;
                        SubState = fwssWorkUploadFileSetTgtPath;
                        nextLoop = TRUE;
                    }
                    break;
                }

                case fwssWorkUploadOverwriteFile:
                {
                    UploadType = utOverwriteFile;
                    SubState = fwssWorkUploadFileSetTgtPath;
                    nextLoop = TRUE;
                    break;
                }

                default: // case fwssWorkUploadAutorenameFile:
                {
                    UploadType = utAutorename;
                    if (curItem->RenamedName != NULL)
                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): curItem->RenamedName != NULL");
                    Queue->UpdateAutorenamePhase(curItem, 0);
                    UploadAutorenameNewName[0] = 0;
                    SubState = fwssWorkUploadFileSetTgtPath;
                    nextLoop = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadFileSetTgtPath: // file upload: set the target path
            {
                if (!HaveWorkingPath || strcmp(WorkingPath, curItem->TgtPath) != 0)
                { // the working path must be changed (assumption: the server returns the same path string - it
                    // got into the item during explore-dir or from the panel; in both cases it was the path returned
                    // by the server in response to PWD)
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdChangeWorkingPath, &cmdLen, curItem->TgtPath); // cannot report an error
                    sendCmd = TRUE;
                    SubState = fwssWorkUploadFileSetTgtPathWaitForCWDRes;

                    HaveWorkingPath = FALSE; // changing the current working path on the server
                }
                else // the working directory is already set
                {
                    SubState = fwssWorkUploadSetType;
                    nextLoop = TRUE;
                }
                break;
            }

            case fwssWorkUploadFileSetTgtPathWaitForCWDRes: // file upload: waiting for the result of "CWD" (setting the target path)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written into the log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // the target path is set; start generating target directory names
                    {                                             // we have successfully changed the working path; since this path was once returned
                        // by the server in response to PWD, we assume PWD would return it again now, and therefore
                        // we will not send it (an optimization hopefully with very low risk)
                        HaveWorkingPath = TRUE;
                        lstrcpyn(WorkingPath, curItem->TgtPath, FTP_MAX_PATH);

                        if (ShouldStop)
                            handleShouldStop = TRUE; // check whether the worker should stop
                        else
                        {
                            SubState = fwssWorkUploadSetType;
                            nextLoop = TRUE;
                        }
                    }
                    else // an error occurred; report it to the user and move on to the next queue item
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOCWDONLYPATH, NO_ERROR,
                                               SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                               Oper);
                        lookForNewWork = TRUE;
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadSetType: // upload copy/move file: set the desired transfer mode (ASCII / binary)
            {
                if (ShouldStop)
                    handleShouldStop = TRUE; // check whether the worker should stop
                else
                {
                    if (CurrentTransferMode != (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary)) // if it is not already set, configure the desired mode
                    {
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdSetTransferMode, &cmdLen, curItem->AsciiTransferMode); // cannot report an error
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadWaitForTYPERes;
                    }
                    else // the desired mode is already set
                    {
                        nextLoop = TRUE;
                        switch (UploadType)
                        {
                        case utResumeFile:
                        case utResumeOrOverwriteFile:
                        case utOnlyTestFileSize:
                            SubState = fwssWorkUploadGetFileSize;
                            break;

                        case utAutorename:
                            SubState = fwssWorkUploadGenNewName;
                            break;

                        default:
                            SubState = fwssWorkUploadLockFile;
                            break;
                        }
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForTYPERes: // upload copy/move file: waiting for the result of "TYPE" (switching to ASCII / binary data transfer mode)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written into the log)
                case fweCmdReplyReceived:
                {
                    if (ShouldStop)
                        handleShouldStop = TRUE; // check whether the worker should stop
                    else
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)                                    // success is returned (should be 200)
                            CurrentTransferMode = (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary); // the transfer mode was changed
                        else
                            CurrentTransferMode = ctrmUnknown; // unknown error; it may not matter, but we will not cache the transfer mode

                        nextLoop = TRUE;
                        switch (UploadType)
                        {
                        case utResumeFile:
                        case utResumeOrOverwriteFile:
                        case utOnlyTestFileSize:
                            SubState = fwssWorkUploadGetFileSize;
                            break;

                        case utAutorename:
                            SubState = fwssWorkUploadGenNewName;
                            break;

                        default:
                            SubState = fwssWorkUploadLockFile;
                            break;
                        }
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadGetFileSize: // file upload: resume - determine the file size (using the SIZE command or the listing)
            {
                if (Oper->GetSizeCmdIsSupported())
                {
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdGetSize, &cmdLen, curItem->TgtName); // cannot report an error
                    sendCmd = TRUE;
                    SubState = fwssWorkUploadWaitForSIZERes;
                }
                else
                {
                    nextLoop = TRUE;
                    SubState = fwssWorkUploadGetFileSizeFromListing;
                }
                break;
            }

            case fwssWorkUploadWaitForSIZERes: // file upload: resume - waiting for the response to the SIZE command
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written into the log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // success is returned (should be 213)
                    {
                        if (ShouldStop)
                            handleShouldStop = TRUE; // check whether the worker should stop
                        else
                        {
                            char* num = reply + 4;
                            char* end = reply + replySize;
                            unsigned __int64 size = 0;
                            while (num < end)
                            {
                                if (*num >= '0' && *num <= '9')
                                    size = size * 10 + (*num++ - '0');
                                else
                                {
                                    if (*num <= ' ' || *num == '+')
                                        num++;
                                    else
                                        break;
                                }
                            }
                            if (num == end) // the response is OK (we have the file size in 'size')
                            {
                                if (UploadType == utOnlyTestFileSize)
                                {
                                    CQuadWord qwSize;
                                    qwSize.SetUI64(size);
                                    if (curItem->AsciiTransferMode)
                                    {
                                        if (qwSize == curItem->SizeWithCRLF_EOLs ||                       // size with CRLF
                                            qwSize == curItem->SizeWithCRLF_EOLs - curItem->NumberOfEOLs) // size with LF (or CR)
                                        {
                                            SubState = fwssWorkUploadTestFileSizeOK; // size is OK => declare the upload successful
                                        }
                                        else
                                            SubState = fwssWorkUploadTestFileSizeFailed; // different size; perform a retry (handling the "transfer failed" problem...)
                                    }
                                    else
                                    {
                                        if (qwSize == OpenedInFileSize)
                                            SubState = fwssWorkUploadTestFileSizeOK; // size is OK => declare the upload successful
                                        else
                                            SubState = fwssWorkUploadTestFileSizeFailed; // different size; perform a retry (handling the "transfer failed" problem...)
                                    }
                                    nextLoop = TRUE;
                                }
                                else
                                {
                                    if (CQuadWord().SetUI64(size) > OpenedInFileSize)
                                    { // the target file is larger than the source file; resume cannot be performed
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME2), -1, TRUE);
                                        if (UploadType == utResumeOrOverwriteFile) // perform overwrite
                                        {
                                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                            UploadType = utOverwriteFile;
                                            SubState = fwssWorkUploadLockFile;
                                            nextLoop = TRUE;
                                        }
                                        else // report an error (resume cannot be performed)
                                        {
                                            // error on the item; record this state into it
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADUNABLETORESUMEBIGTGT, NO_ERROR, NULL, Oper);
                                            lookForNewWork = TRUE;
                                        }
                                    }
                                    else
                                    {
                                        int resumeMinFileSize = Config.GetResumeMinFileSize();
                                        if ((unsigned __int64)resumeMinFileSize <= size) // resume makes sense (the portion of the file is larger than the minimum for resume)
                                        {
                                            ResumingFileOnServer = TRUE;
                                            OpenedInFileCurOffset.SetUI64(size);
                                            // NOTE: we cannot resume uploads of text files; otherwise this would have to initialize OpenedInFileNumberOfEOLs and OpenedInFileSizeWithCRLF_EOLs based on the number of EOLs in the already uploaded part of the file
                                            FileOnServerResumedAtOffset = OpenedInFileCurOffset;
                                        }
                                        else // perform overwrite because the target file is too small for resume
                                        {
                                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMEUSELESS), -1, TRUE);
                                            UploadType = utOverwriteFile;
                                        }
                                        nextLoop = TRUE;
                                        SubState = fwssWorkUploadLockFile;
                                    }
                                }
                            }
                            else // unexpected response to the SIZE command
                            {
                                nextLoop = TRUE;
                                SubState = fwssWorkUploadGetFileSizeFromListing;
                            }
                        }
                    }
                    else
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_ERROR && FTP_DIGIT_2(replyCode) == FTP_D2_SYNTAX)
                        { // SIZE is not implemented (on NETWARE it also reports this when SIZE is sent for a directory, but for now we ignore that)
                            Oper->SetSizeCmdIsSupported(FALSE);
                        }
                        if (ShouldStop)
                            handleShouldStop = TRUE; // check whether the worker should stop
                        else
                        {
                            nextLoop = TRUE;
                            SubState = fwssWorkUploadGetFileSizeFromListing;
                        }
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadGetFileSizeFromListing: // file upload: resume - the SIZE command failed (or is not implemented), determine the file size from the listing
            {
                if (existingItem == NULL) // if it did not arrive here directly, retrieve target file information from the listing again
                {
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                    BOOL notAccessible, getListing, listingInProgress;
                    if (UploadListingCache.GetListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                      pathType, Msg, UID, &listingInProgress,
                                                      &notAccessible, &getListing, curItem->TgtName,
                                                      &existingItem, NULL))
                    {
                        if (listingInProgress) // listing is currently taking place or is about to happen
                        {
                            if (getListing) // we are to obtain the listing and then notify any other waiting workers
                            {
                                SubState = fwssWorkStartWork;
                                UploadDirGetTgtPathListing = TRUE;
                                postActivate = TRUE; // post an impulse to start downloading the listing
                            }
                            else
                            {
                                SubState = fwssWorkUploadWaitForListing; // we should wait until another worker finishes listing
                                reportWorkerChange = TRUE;               // the worker prints the fwssWorkUploadWaitForListing state to the window, so a redraw is needed
                            }
                            break;
                        }
                        else // the listing is already cached or marked as "unobtainable"
                        {
                            if (notAccessible) // the listing is cached, but only as "unobtainable"
                            {
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCANNOTLISTTGTPATH, 0, NULL, Oper);
                                lookForNewWork = TRUE;
                                break;
                            }
                            else // the listing is available
                            {
                                if (existingItem == NULL) // something changed: the target file no longer exists - perform a retry (we will not handle what to do after this change here)
                                {
                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will surely handle this item (no need to post "new work available")
                                    lookForNewWork = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    else // insufficient memory
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }
                }
                if (existingItem != NULL) // "always true"
                {
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    if (UploadType == utOnlyTestFileSize)
                    {
                        if (existingItem->ItemType == ulitFile && existingItem->ByteSize != UPLOADSIZE_UNKNOWN &&
                            existingItem->ByteSize != UPLOADSIZE_NEEDUPDATE)
                        {
                            if (curItem->AsciiTransferMode)
                            {
                                if (existingItem->ByteSize == curItem->SizeWithCRLF_EOLs ||                       // size with CRLF
                                    existingItem->ByteSize == curItem->SizeWithCRLF_EOLs - curItem->NumberOfEOLs) // size with LF (or CR)
                                {
                                    SubState = fwssWorkUploadTestFileSizeOK; // size is OK => declare the upload successful
                                }
                                else
                                    SubState = fwssWorkUploadTestFileSizeFailed; // different size; perform a retry (handling the "transfer failed" problem...)
                            }
                            else
                            {
                                if (existingItem->ByteSize == OpenedInFileSize)
                                    SubState = fwssWorkUploadTestFileSizeOK; // size is OK => declare the upload successful
                                else
                                    SubState = fwssWorkUploadTestFileSizeFailed; // different size; perform a retry (handling the "transfer failed" problem...)
                            }
                            nextLoop = TRUE;
                        }
                        else
                        {
                            if (existingItem->ItemType == ulitFile && existingItem->ByteSize == UPLOADSIZE_NEEDUPDATE)
                            { // the file was uploaded in ASCII mode; its size will be known only after refreshing the listing - invalidate the listing and retry the item (it will download the listing again)
                                UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                                         Oper->GetFTPServerPathType(curItem->TgtPath));
                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will surely handle this item (no need to post "new work available")
                            }
                            else // the target file size in bytes cannot be determined (neither from the listing nor via the SIZE command)
                            {
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADTESTIFFINISHEDNOTSUP,
                                                       NO_ERROR, NULL, Oper);
                            }
                            lookForNewWork = TRUE;
                        }
                    }
                    else
                    {
                        if (LockedFileUID != 0)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): LockedFileUID != 0!");
                        if (FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                    Oper->GetFTPServerPathType(curItem->TgtPath),
                                                    curItem->TgtName, &LockedFileUID, ffatWrite))
                        { // the file on the server is not open yet; we can work with it
                            if (existingItem->ItemType == ulitFile && existingItem->ByteSize != UPLOADSIZE_UNKNOWN &&
                                existingItem->ByteSize != UPLOADSIZE_NEEDUPDATE)
                            {
                                if (existingItem->ByteSize > OpenedInFileSize)
                                { // the target file is larger than the source file; resume cannot be performed
                                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME2), -1, TRUE);
                                    if (UploadType == utResumeOrOverwriteFile) // perform overwrite
                                    {
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                        UploadType = utOverwriteFile;
                                        SubState = fwssWorkUploadDelForOverwrite;
                                        nextLoop = TRUE;
                                    }
                                    else // report an error (resume cannot be performed)
                                    {
                                        // error on the item; record this state into it
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADUNABLETORESUMEBIGTGT, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                }
                                else
                                {
                                    int resumeMinFileSize = Config.GetResumeMinFileSize();
                                    if (CQuadWord(resumeMinFileSize, 0) <= existingItem->ByteSize) // resume makes sense (the uploaded part of the file is larger than the minimum for resume)
                                    {
                                        ResumingFileOnServer = TRUE;
                                        OpenedInFileCurOffset = existingItem->ByteSize;
                                        // NOTE: we cannot resume uploads of text files; otherwise this would have to initialize OpenedInFileNumberOfEOLs and OpenedInFileSizeWithCRLF_EOLs based on the number of EOLs in the already uploaded part of the file
                                        FileOnServerResumedAtOffset = OpenedInFileCurOffset;
                                    }
                                    else // perform overwrite because the target file is too small for resume
                                    {
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMEUSELESS), -1, TRUE);
                                        UploadType = utOverwriteFile;
                                    }
                                    nextLoop = TRUE;
                                    SubState = fwssWorkUploadDelForOverwrite;
                                }
                            }
                            else
                            {
                                if (existingItem->ItemType == ulitFile && existingItem->ByteSize == UPLOADSIZE_NEEDUPDATE)
                                { // the file was uploaded in ASCII mode; its size will be known only after refreshing the listing - invalidate the listing and retry the item (it will download the listing again)
                                    UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                                             Oper->GetFTPServerPathType(curItem->TgtPath));
                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will surely handle this item (no need to post "new work available")
                                    lookForNewWork = TRUE;
                                }
                                else // the target file size in bytes cannot be determined (neither from the listing nor via the SIZE command)
                                {
                                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLUNABLETOGETSIZE), -1, TRUE);
                                    if (UploadType == utResumeOrOverwriteFile) // perform overwrite
                                    {
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                        UploadType = utOverwriteFile;
                                        SubState = fwssWorkUploadDelForOverwrite;
                                        nextLoop = TRUE;
                                    }
                                    else // report an error (resume cannot be performed)
                                    {
                                        // error on the item; record this state into it
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADUNABLETORESUMEUNKSIZ, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                }
                            }
                        }
                        else // another operation is already in progress on this file; let the user try again later
                        {
                            // error on the item; record this state into it
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_TGTFILEINUSE, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                    }
                }
                break;
            }

            case fwssWorkUploadTestFileSizeOK:     // upload copy/move file: after an upload error the file size test succeeded
            case fwssWorkUploadTestFileSizeFailed: // upload copy/move file: after an upload error the file size test failed
            {
                char num[100];
                SalamanderGeneral->PrintDiskSize(num, OpenedInFileSize, 2);
                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE,
                            LoadStr(SubState == fwssWorkUploadTestFileSizeOK ? (curItem->AsciiTransferMode ? IDS_LOGMSGUPLOADISCOMPL2 : IDS_LOGMSGUPLOADISCOMPL) : (curItem->AsciiTransferMode ? IDS_LOGMSGUPLOADISNOTCOMPL2 : IDS_LOGMSGUPLOADISNOTCOMPL)),
                            curItem->TgtName, num);
                Logs.LogMessage(LogUID, errText, -1, TRUE);

                Queue->UpdateForceAction(CurItem, fqiaNone); // the forced action no longer applies
                if (SubState == fwssWorkUploadTestFileSizeOK)
                {
                    // close the source file on disk
                    CloseOpenedInFile();

                    // mark the file as already transferred (in case deleting the source file during Move fails we must distinguish this situation)
                    Queue->UpdateTgtFileState(curItem, UPLOADTGTFILESTATE_TRANSFERRED);

                    SubState = fwssWorkUploadCopyTransferFinished;
                    nextLoop = TRUE;
                }
                else
                {
                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will go look for new work, so some worker will surely handle this item (no need to post "new work available")
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadGenNewName: // file upload: auto-rename - generating a new name
            {
                Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                BOOL notAccessible, getListing, listingInProgress, nameExists;
                int index = 0;
                UploadAutorenamePhase = curItem->AutorenamePhase;
                int usedUploadAutorenamePhase = UploadAutorenamePhase; // in case of a name collision - the phase in which we should try to generate another name
                while (1)
                {
                    FTPGenerateNewName(&UploadAutorenamePhase, UploadAutorenameNewName, &index,
                                       curItem->TgtName, pathType, FALSE, strcmp(curItem->TgtName, curItem->Name) != 0);
                    // we have a new name; verify whether it collides with a name from the target path listing
                    if (UploadListingCache.GetListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                      pathType, Msg, UID, &listingInProgress,
                                                      &notAccessible, &getListing,
                                                      UploadAutorenameNewName, NULL, &nameExists))
                    {
                        if (listingInProgress) // listing is currently taking place or is about to happen
                        {
                            if (getListing) // we are to obtain the listing and then notify any other waiting workers
                            {
                                UploadDirGetTgtPathListing = TRUE;
                                SubState = fwssWorkStartWork;
                                postActivate = TRUE; // post an impulse to start downloading the listing
                            }
                            else
                            {
                                SubState = fwssWorkUploadWaitForListing; // we should wait until another worker finishes listing
                                reportWorkerChange = TRUE;               // the worker prints the fwssWorkUploadWaitForListing state to the window, so a redraw is needed
                            }
                            break;
                        }
                        else // the listing is already cached or marked as "unobtainable"
                        {
                            if (notAccessible) // the listing is cached, but only as "unobtainable" (very unlikely; the listing was "ready" a moment ago)
                            {
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCANNOTLISTTGTPATH, 0, NULL, Oper);
                                lookForNewWork = TRUE;
                                break;
                            }
                            else // the listing is available; check for a potential file name collision
                            {
                                if (LockedFileUID != 0)
                                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): LockedFileUID != 0!");
                                if (!nameExists && // no collision with an existing file/link/directory
                                    FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                            pathType, UploadAutorenameNewName,
                                                            &LockedFileUID, ffatWrite)) // the target name is not yet opened by another operation - ignore "low memory" (if it happened, the loop should end at UploadListingCache.GetListing with "low memory")
                                {                                                       // without collision -> try to create the target file
                                    char* newName = SalamanderGeneral->DupStr(UploadAutorenameNewName);
                                    if (newName == NULL)
                                    {
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    else
                                    {
                                        Queue->UpdateRenamedName(curItem, newName);
                                        Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                                        SubState = fwssWorkUploadDelForOverwrite;
                                        nextLoop = TRUE;
                                    }
                                    break;
                                }
                                else // name collision (with a file/link/directory) or another operation is already using this file - try another name within the same auto-rename phase
                                    UploadAutorenamePhase = usedUploadAutorenamePhase;
                            }
                        }
                    }
                    else // insufficient memory
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }
                }
                break;
            }

            case fwssWorkUploadLockFile: // file upload: open the file in FTPOpenedFiles
            {
                Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                if (LockedFileUID != 0)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): LockedFileUID != 0!");
                if (FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                            Oper->GetFTPServerPathType(curItem->TgtPath),
                                            curItem->TgtName, &LockedFileUID, ffatWrite))
                { // the file on the server is not open yet; we can work with it
                    SubState = fwssWorkUploadDelForOverwrite;
                    nextLoop = TRUE;
                }
                else // another operation is already in progress on this file; let the user try again later
                {
                    // error on the item; record this state into it
                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_TGTFILEINUSE, NO_ERROR, NULL, Oper);
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadDelForOverwrite: // file upload: if overwrite should delete first, perform it here
            {
                if (UseDeleteForOverwrite && UploadType == utOverwriteFile)
                { // the file is already locked for writing; deletion is just an intermediate step, no need to call FTPOpenedFiles.OpenFile()
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdDeleteFile, &cmdLen, curItem->TgtName); // cannot report an error
                    sendCmd = TRUE;
                    SubState = fwssWorkUploadDelForOverWaitForDELERes;
                }
                else
                {
                    SubState = fwssWorkUploadFileAllocDataCon;
                    nextLoop = TRUE;
                }
                break;
            }

            case fwssWorkUploadDelForOverWaitForDELERes: // file upload: waiting for the DELE result before overwrite
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written into the log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    { // the target file/link was successfully deleted
                        // if the file/link was deleted, update the cached listing
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        curItem->TgtName, FALSE);
                    }
                    // else; // deletion failure is not critical; try STOR, and if overwriting still fails, return an error to the user
                    SubState = fwssWorkUploadFileAllocDataCon;
                    nextLoop = TRUE;
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    // if we do not know whether the file/link was deleted, invalidate the listing in the cache
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                    Oper->GetFTPServerPathType(curItem->TgtPath),
                                                    curItem->TgtName, TRUE);
                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadFileAllocDataCon: // file upload: allocate the data connection
            {
                if (WorkerUploadDataCon != NULL)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): WorkerUploadDataCon is not NULL before starting data-connection!");
                DataConAllDataTransferred = FALSE;
                CFTPProxyForDataCon* dataConProxyServer;
                if (Oper->AllocProxyForDataCon(&dataConProxyServer))
                {
                    CCertificate* cert = Oper->GetCertificate();
                    WorkerUploadDataCon = new CUploadDataConnectionSocket(dataConProxyServer, Oper->GetEncryptDataConnection(), cert, Oper->GetCompressData(), this);
                    if (cert)
                        cert->Release();
                }
                else
                    WorkerUploadDataCon = NULL;
                ReuseSSLSessionFailed = FALSE;
                if (WorkerUploadDataCon == NULL || !WorkerUploadDataCon->IsGood())
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from inside CSocket::SocketCritSect (no deadlock risk)
                        DeleteSocket(WorkerUploadDataCon); // it will only be deallocated
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }
                    else
                    {
                        if (dataConProxyServer != NULL)
                            delete dataConProxyServer;
                    }
                    TRACE_E(LOW_MEMORY);

                    // error on the item; record this state into it
                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                    lookForNewWork = TRUE;
                }
                else // the data connection object was allocated successfully
                {
                    WorkerDataConState = wdcsOnlyAllocated;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from inside CSocket::SocketCritSect (no deadlock risk)
                    WorkerUploadDataCon->SetPostMessagesToWorker(TRUE, Msg, UID,
                                                                 WORKER_UPLDATACON_CONNECTED,
                                                                 WORKER_UPLDATACON_CLOSED,
                                                                 WORKER_UPLDATACON_PREPAREDATA,
                                                                 WORKER_UPLDATACON_LISTENINGFORCON);
                    WorkerUploadDataCon->SetGlobalTransferSpeedMeter(Oper->GetGlobalTransferSpeedMeter());
                    WorkerUploadDataCon->SetGlobalLastActivityTime(Oper->GetGlobalLastActivityTime());
                    HANDLES(EnterCriticalSection(&WorkerCritSect));

                    if (Oper->GetUsePassiveMode()) // passive mode (PASV)
                    {
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdPassive, &cmdLen); // cannot report an error
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadWaitForPASVRes;
                    }
                    else // active mode (PORT)
                    {
                        nextLoop = TRUE;
                        SubState = fwssWorkUploadOpenActDataCon;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForPASVRes: // upload copy/move file: waiting for the result of "PASV" (obtaining IP+port for the passive data connection)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written into the log)
                case fweCmdReplyReceived:
                {
                    DWORD ip;
                    unsigned short port;
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&             // success (should be 227)
                        FTPGetIPAndPortFromReply(reply, replySize, &ip, &port)) // successfully obtained IP+port
                    {
                        if (ShouldStop)
                        {
                            if (WorkerUploadDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // because we are already inside CSocketsThread::CritSect, this call is also possible
                                // from inside CSocket::SocketCritSect (no deadlock risk)
                                DeleteSocket(WorkerUploadDataCon); // no connection was made yet; it will only be deallocated
                                WorkerUploadDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }

                            handleShouldStop = TRUE; // check whether the worker should stop
                        }
                        else
                        {
                            if (PrepareDataError != pderNone) // the data transfer to the server may start now (even though the server does not yet know the target file name)
                            {
                                TRACE_E("CFTPWorker::HandleEventInWorkingState5(): fwssWorkUploadWaitForPASVRes: unexpected value of PrepareDataError: " << PrepareDataError);
                                PrepareDataError = pderNone;
                            }

                            int logUID = LogUID;
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));

                            // because we are already inside CSocketsThread::CritSect, these calls are also possible
                            // from inside CSocket::SocketCritSect (no deadlock risk)
                            if (WorkerUploadDataCon != NULL)
                            {
                                WorkerUploadDataCon->SetPassive(ip, port, logUID);
                                WorkerUploadDataCon->PassiveConnect(NULL); // first attempt; we do not care about the result yet (verified later)
                            }

                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            WorkerDataConState = wdcsWaitingForConnection;

                            nextLoop = TRUE;
                            SubState = fwssWorkUploadSendSTORCmd;
                        }
                    }
                    else // passive mode is not supported; try the active mode
                    {
                        Oper->SetUsePassiveMode(FALSE);
                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);

                        if (ShouldStop)
                        {
                            if (WorkerUploadDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // because we are already inside CSocketsThread::CritSect, this call is also possible
                                // from inside CSocket::SocketCritSect (no deadlock risk)
                                DeleteSocket(WorkerUploadDataCon); // no connection was made yet; it will only be deallocated
                                WorkerUploadDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }

                            handleShouldStop = TRUE; // check whether the worker should stop
                        }
                        else
                        {
                            nextLoop = TRUE;
                            SubState = fwssWorkUploadOpenActDataCon;
                        }
                    }
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from inside CSocket::SocketCritSect (no deadlock risk)
                        DeleteSocket(WorkerUploadDataCon); // no connection was made yet; it will only be deallocated
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadOpenActDataCon: // upload copy/move file: open the active data connection
            {
                DWORD localIP;
                unsigned short localPort = 0; // listen on any port
                DWORD error = NO_ERROR;
                int logUID = LogUID;

                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                // because we are already inside CSocketsThread::CritSect, these calls are also possible
                // from inside CSocket::SocketCritSect (no deadlock risk)
                GetLocalIP(&localIP, NULL); // should not even be able to return an error
                BOOL retOpenForListening = FALSE;
                BOOL listenError = TRUE;
                if (WorkerUploadDataCon != NULL)
                {
                    WorkerUploadDataCon->SetActive(logUID);
                    retOpenForListening = WorkerUploadDataCon->OpenForListeningWithProxy(localIP, localPort, &listenError, &error);
                }

                HANDLES(EnterCriticalSection(&WorkerCritSect));

                if (retOpenForListening)
                {
                    if (PrepareDataError != pderNone) // data transfer to the server can start now (even though the server does not yet know the target file name)
                    {
                        TRACE_E("CFTPWorker::HandleEventInWorkingState5(): fwssWorkUploadOpenActDataCon: unexpected value of PrepareDataError: " << PrepareDataError);
                        PrepareDataError = pderNone;
                    }

                    WorkerDataConState = wdcsWaitingForConnection;
                    SubState = fwssWorkUploadWaitForListen;

                    // in case the proxy server does not respond within the required time limit, add a timer for
                    // the data connection preparation timeout (opening the "listen" port)
                    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                    if (serverTimeout < 1000)
                        serverTimeout = 1000; // at least one second
                    // because we are already in CSocketsThread::CritSect, this call is also
                    // possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no deadlock risk)
                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                            WORKER_LISTENTIMEOUTTIMID, NULL); // ignore the error; at worst the user will press Stop
                }
                else // failed to open the "listen" socket to accept the data connection from
                {    // the server (local operation, most likely never happens) or cannot open the connection to the proxy server
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        DeleteSocket(WorkerUploadDataCon); // no connection has been established yet, so we just deallocate it
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    if (listenError)
                    {
                        // there is an error on the item, record this state in it
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LISTENFAILURE, error, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                    else // unable to open the connection to the proxy server, perform a retry...
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

                        // log the timeout
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);

                        // "manually" close the control connection
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        ForceClose();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));

                        conClosedRetryItem = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForListen: // upload copy/move file: waiting for the "listen" port to open (opening the active data connection) - local or on the proxy server
            {
                if (ShouldStop)
                {
                    // because we are already in CSocketsThread::CritSect, this call is also
                    // possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no deadlock risk)
                    SocketsThread->DeleteTimer(UID, WORKER_LISTENTIMEOUTTIMID);

                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    handleShouldStop = TRUE; // check whether the worker should be stopped
                }
                else
                {
                    BOOL needRetry = FALSE;
                    switch (event)
                    {
                    case fweUplDataConListeningForCon:
                    {
                        if (WorkerUploadDataCon != NULL) // "always true" (otherwise the event 'fweUplDataConListeningForCon' would never be generated)
                        {
                            // because we are already in CSocketsThread::CritSect, this call is also
                            // possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no deadlock risk)
                            SocketsThread->DeleteTimer(UID, WORKER_LISTENTIMEOUTTIMID);

                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            errBuf[0] = 0;
                            DWORD listenOnIP;
                            unsigned short listenOnPort;
                            BOOL ok = WorkerUploadDataCon->GetListenIPAndPort(&listenOnIP, &listenOnPort);
                            if (!ok)
                            {
                                if (!WorkerUploadDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                                    errBuf[0] = 0;
                            }
                            HANDLES(EnterCriticalSection(&WorkerCritSect));

                            if (ok)
                            {
                                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                  ftpcmdSetPort, &cmdLen, listenOnIP, listenOnPort); // cannot report an error
                                sendCmd = TRUE;
                                SubState = fwssWorkUploadWaitForPORTRes;
                            }
                            else // error when opening the "listen" port on the proxy server - perform a retry...
                            {
                                // close the data connection
                                if (WorkerUploadDataCon != NULL)
                                {
                                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                    // because we are already in CSocketsThread::CritSect, this call is also
                                    // possible from CSocket::SocketCritSect (no deadlock risk)
                                    if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                                        WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                                    DeleteSocket(WorkerUploadDataCon);
                                    WorkerUploadDataCon = NULL;
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                                    WorkerDataConState = wdcsDoesNotExist;
                                }

                                // prepare the error (timeout) text in 'ErrorDescr'
                                if (errBuf[0] == 0)
                                    lstrcpyn(ErrorDescr, LoadStr(IDS_PROXYERROPENACTDATA), FTPWORKER_ERRDESCR_BUFSIZE);
                                else
                                    _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), errBuf);
                                needRetry = TRUE;
                            }
                        }
                        else
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): WorkerUploadDataCon == NULL!");
                        break;
                    }

                    case fweDataConListenTimeout:
                    {
                        // close the data connection and prepare the error (timeout) text in 'ErrorDescr'
                        errBuf[0] = 0;
                        if (WorkerUploadDataCon != NULL)
                        {
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            if (!WorkerUploadDataCon->GetProxyTimeoutDescr(errBuf, 50 + FTP_MAX_PATH))
                                errBuf[0] = 0;
                            // because we are already in CSocketsThread::CritSect, this call is also
                            // possible from CSocket::SocketCritSect (no deadlock risk)
                            if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                                WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                            DeleteSocket(WorkerUploadDataCon);
                            WorkerUploadDataCon = NULL;
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

                        // log the timeout
                        _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, "%s\r\n", ErrorDescr);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);

                        // "manually" close the control connection
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        ForceClose();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));

                        conClosedRetryItem = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForPORTRes: // upload copy/move file: waiting for the "PORT" result (passing IP+port to the server for the active data connection)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written to the log)
                case fweCmdReplyReceived:
                {
                    nextLoop = TRUE;
                    SubState = fwssWorkUploadSendSTORCmd;
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // handle any data preparation error
                    if (!HandlePrepareDataError(curItem, lookForNewWork))
                        conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadSendSTORCmd: // upload copy/move file: send the STOR/APPE command (start storing the file on the server)
            {
                if (ShouldStop)
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // handle any data preparation error (it has priority over stopping the worker, that can happen a bit later)
                    if (!HandlePrepareDataError(curItem, lookForNewWork))
                        handleShouldStop = TRUE; // check whether the worker should be stopped
                }
                else
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        CQuadWord size = OpenedInFileSize;
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerUploadDataCon->SetDataTotalSize(size);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }

                    if (UploadType == utAutorename)
                    {
                        _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGUPLOADRENFILE), curItem->RenamedName);
                        Logs.LogMessage(LogUID, errText, -1, TRUE);
                    }

                    CommandTransfersData = TRUE;
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ResumingFileOnServer ? ftpcmdAppendFile : ftpcmdStoreFile, &cmdLen,
                                      UploadType == utAutorename ? curItem->RenamedName : curItem->TgtName); // cannot report an error
                    sendCmd = TRUE;

                    // Change TgtFileState immediately, because TgtFileState (except for UPLOADTGTFILESTATE_TRANSFERRED) only makes sense
                    // when a file of this name is created on the server (if the file is not created (for example due to a connection error),
                    // TgtFileState is not used)
                    Queue->UpdateTgtFileState(curItem, UploadType == utResumeFile ? UPLOADTGTFILESTATE_RESUMED /* "resume" */ : UPLOADTGTFILESTATE_CREATED /* "resume or overwrite" + "overwrite" */);

                    postActivate = TRUE;
                    SubState = fwssWorkUploadActivateDataCon;

                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportStoreFile(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                       Oper->GetFTPServerPathType(curItem->TgtPath),
                                                       UploadType == utAutorename ? curItem->RenamedName : curItem->TgtName);
                }
                break;
            }

            case fwssWorkUploadActivateDataCon: // upload copy/move file: activate the data connection (right after sending the STOR command)
            {
                if (!Oper->GetEncryptDataConnection() && (WorkerUploadDataCon != NULL))
                { // FIXME: 2009.01.29: I believe ActivateConnection can be called later
                    // even when not encrypting, but I am too afraid to change that
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // because we are already in CSocketsThread::CritSect, this call is also
                    // possible from CSocket::SocketCritSect (no deadlock risk)
                    WorkerUploadDataCon->ActivateConnection();
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                SubState = fwssWorkUploadWaitForSTORRes;
                if (event != fweActivate)
                    nextLoop = TRUE; // if it was not only fweActivate, deliver the event to fwssWorkUploadWaitForSTORRes
                break;
            }

            case fwssWorkUploadWaitForSTORRes: // upload copy/move file: waiting for the "STOR/APPE" result (waiting for the upload to finish)
            {
                switch (event)
                {
                // "1xx" replies are ignored (they are only written to the log) unless encrypting data
                // We can't start encryption before receiving successful 1xx reply
                // Otherwise SSL_connect fails in ActivateConnection if 5xx reply is about to be received
                case fweCmdInfoReceived:
                    if (Oper->GetEncryptDataConnection() && (WorkerUploadDataCon != NULL))
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        WorkerUploadDataCon->ActivateConnection();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    break;
                case fweCmdReplyReceived:
                {
                    // evaluate the result of the STOR/APPE command: either leave the size as "needupdate" (STOR in ASCII mode or APPE), set it (STOR in binary mode), or just invalidate the listing (error)
                    CQuadWord uploadRealSize;
                    BOOL allDataTransferred = FALSE;
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerUploadDataCon->GetTotalWrittenBytesCount(&uploadRealSize);
                        allDataTransferred = WorkerUploadDataCon->AllDataTransferred();
                        if (allDataTransferred)
                            WorkerUploadDataCon->UploadFinished();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    else
                        uploadRealSize = UPLOADSIZE_UNKNOWN;
                    CQuadWord uploadSize;
                    if (curItem->AsciiTransferMode || ResumingFileOnServer)
                        uploadSize = UPLOADSIZE_NEEDUPDATE;
                    else
                        uploadSize = uploadRealSize;
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportFileUploaded(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                          Oper->GetFTPServerPathType(curItem->TgtPath),
                                                          UploadType == utAutorename ? curItem->RenamedName : curItem->TgtName,
                                                          uploadSize,
                                                          FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS || !allDataTransferred);

                    // close the data connection, it is no longer needed (nobody is monitoring it anymore - the server already reports the result and waits for more commands)
                    DWORD dataConError = NO_ERROR;
                    BOOL dataConNoDataTransTimeout = FALSE;
                    int dataSSLErrorOccured = SSLCONERR_NOERROR;
                    errBuf[0] = 0;
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerUploadDataCon->GetError(&dataConError, &dataConNoDataTransTimeout, &dataSSLErrorOccured);
                        if (!WorkerUploadDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                            errBuf[0] = 0;
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                        WorkerUploadDataCon->FreeBufferedData();
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    BOOL uploadTypeIsAutorename = (UploadType == utAutorename); // UploadType is cleared when closing OpenedInFile, so we have to check it here
                    BOOL canUseRenamedName = TRUE;
                    BOOL canClearForceAction = TRUE;
                    if (!ShouldStop && PrepareDataError == pderASCIIForBinaryFile)
                    { // when a binary file is detected in ASCII mode, ensure the target file is deleted
                        // the file is already locked for writing, deletion is only an intermediate step, no need to call FTPOpenedFiles.OpenFile()
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdDeleteFile, &cmdLen,
                                          UploadType == utAutorename ? curItem->RenamedName : curItem->TgtName); // cannot report an error
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadWaitForDELERes;
                    }
                    else
                    {
                        if (!HandlePrepareDataError(curItem, lookForNewWork)) // handle any data preparation error
                        {
                            if (ShouldStop)
                                handleShouldStop = TRUE; // check whether the worker should be stopped
                            else
                            {
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS && allDataTransferred &&
                                    dataConError == NO_ERROR && dataSSLErrorOccured == SSLCONERR_NOERROR)
                                {                                                                     // transfer completed successfully (server reports success, the data connection reports everything transferred + no error)
                                    if (ResumingFileOnServer && uploadRealSize != UPLOADSIZE_UNKNOWN) // must be before CloseOpenedInFile() - that resets ResumingFileOnServer and FileOnServerResumedAtOffset
                                        uploadRealSize += FileOnServerResumedAtOffset;                // add the resume offset for resumed files

                                    // close the source file on disk
                                    CloseOpenedInFile();

                                    // mark the file as already transferred (in case deleting the source file fails during Move we need to distinguish this situation)
                                    Queue->UpdateTgtFileState(curItem, UPLOADTGTFILESTATE_TRANSFERRED);

                                    if (uploadRealSize != UPLOADSIZE_UNKNOWN && curItem->Size != uploadRealSize)
                                    {
                                        Queue->UpdateFileSize(curItem, uploadRealSize, Oper);
                                        Oper->ReportItemChange(CurItem->UID); // request item redraw
                                    }

                                    SubState = fwssWorkUploadCopyTransferFinished;
                                    nextLoop = TRUE;
                                }
                                else
                                {
                                    if (dataSSLErrorOccured == SSLCONERR_UNVERIFIEDCERT ||
                                        ReuseSSLSessionFailed && (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                                                  FTP_DIGIT_1(replyCode) == FTP_D1_ERROR))
                                    {
                                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                        if (IsConnected()) // "manually" close the control connection
                                        {
                                            // because we are already in CSocketsThread::CritSect, this call is also
                                            // possible from CSocket::SocketCritSect (no deadlock risk)
                                            ForceClose(); // sending QUIT would be cleaner, but a certificate change is very unlikely, so it's not worth the hassle ;-)
                                        }
                                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                                        conClosedRetryItem = TRUE;
                                    }
                                    else
                                    {
                                        if ((!ResumingFileOnServer || Oper->GetDataConWasOpenedForAppendCmd()) && // proftpd (Linux) repeatedly returns 45x (append disabled, enabled somewhere in the config), warftpd repeatedly returns 42x (some write error) -- in any case we cannot keep trying APPE endlessly (however if APPE opened the data connection, perform auto-retry because APPE works 99.9% of the time)
                                                FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR &&
                                                (FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION ||  // mainly "426 data connection closed, transfer aborted" (I cannot tell whether it was caused by the server admin or a connection failure, so priority goes to assuming a connection issue -> retry the upload)
                                                 FTP_DIGIT_2(replyCode) == FTP_D2_FILESYSTEM) && // "450 Transfer aborted.  Link to file server lost."
                                                dataSSLErrorOccured != SSLCONERR_DONOTRETRY ||   // take 426 and 450 only if they were not caused by: failed to encrypt the connection, which is a permanent problem
                                            dataConNoDataTransTimeout ||                         // connection interrupted by us due to the no-data-transfer timeout (happens during "50%" network outages, the data connection stays up but data transfer stalls, can remain open for 14000 seconds, this should address it) -> retry the upload attempt
                                            dataSSLErrorOccured == SSLCONERR_CANRETRY)           // failed to encrypt the connection, but it is not a permanent problem
                                        {
                                            SubState = fwssWorkCopyDelayedAutoRetry; // use delayed auto-retry so all unexpected replies from the server can arrive
                                            // because we are already in CSocketsThread::CritSect, this call is also
                                            // possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no deadlock risk)
                                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + WORKER_DELAYEDAUTORETRYTIMEOUT,
                                                                    WORKER_DELAYEDAUTORETRYTIMID, NULL); // ignore the error; at worst the user will press Stop
                                        }
                                        else
                                        {
                                            if (!ResumingFileOnServer &&
                                                FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS &&
                                                (uploadRealSize == CQuadWord(0, 0) || uploadRealSize == UPLOADSIZE_UNKNOWN) &&
                                                dataSSLErrorOccured == SSLCONERR_NOERROR)
                                            { // STOR reports an error + nothing was uploaded == assume the "cannot create target file name" error
                                                if (UploadType == utOverwriteFile && !UseDeleteForOverwrite)
                                                {
                                                    UseDeleteForOverwrite = TRUE;
                                                    canClearForceAction = FALSE;
                                                    Queue->UpdateForceAction(CurItem, fqiaOverwrite);                              // in case it is not a pure Overwrite (but for example Resume-or-Overwrite)
                                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                                                }
                                                else
                                                {
                                                    if (UploadType == utAutorename)
                                                    {
                                                        if (UploadAutorenamePhase != -1) // try to generate another name
                                                        {
                                                            canClearForceAction = FALSE;
                                                            Queue->UpdateForceAction(CurItem, fqiaUploadContinueAutorename);
                                                            Queue->UpdateAutorenamePhase(curItem, UploadAutorenamePhase);
                                                            canUseRenamedName = FALSE;                                                     // keep TgtName as the original (forget RenamedName; another name will be generated by the next FTPGenerateNewName phase)
                                                            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                                                        }
                                                        else // we no longer know what other name could be created, so report an error
                                                        {
                                                            CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADFILEAUTORENFAILED, NO_ERROR,
                                                                                   SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                                                                   Oper);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                                        if (CurItem->ForceAction == fqiaUseAutorename) // forced autorename
                                                        {
                                                            canClearForceAction = FALSE;
                                                            Queue->UpdateForceAction(CurItem, fqiaUploadForceAutorename);
                                                            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                                                        }
                                                        else
                                                        {
                                                            switch (Oper->GetUploadCannotCreateFile())
                                                            {
                                                            case CANNOTCREATENAME_USERPROMPT:
                                                            {
                                                                Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTFILE,
                                                                                       NO_ERROR, SalamanderGeneral->DupStr(errText), Oper);
                                                                break;
                                                            }

                                                            case CANNOTCREATENAME_SKIP:
                                                            {
                                                                Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTFILE,
                                                                                       NO_ERROR, SalamanderGeneral->DupStr(errText), Oper);
                                                                break;
                                                            }

                                                            default: // case CANNOTCREATENAME_AUTORENAME:
                                                            {
                                                                canClearForceAction = FALSE;
                                                                Queue->UpdateForceAction(CurItem, fqiaUploadForceAutorename);
                                                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                                                                break;
                                                            }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                if (ResumingFileOnServer && FTP_DIGIT_1(replyCode) == FTP_D1_ERROR &&
                                                    FTP_DIGIT_2(replyCode) == FTP_D2_SYNTAX && // the server does not support APPE, e.g. "500 command not understood"
                                                    dataSSLErrorOccured == SSLCONERR_NOERROR)
                                                {
                                                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMENOTSUP), -1, TRUE);
                                                    Oper->SetResumeIsNotSupported(TRUE);       // do not try APPE again (unless after Retry or Solve Error, where this is reset)
                                                    if (UploadType == utResumeOrOverwriteFile) // perform overwrite
                                                    {
                                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                                        canClearForceAction = FALSE;
                                                        Queue->UpdateForceAction(CurItem, fqiaOverwrite);
                                                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                                                    }
                                                    else
                                                    {
                                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESUME, NO_ERROR,
                                                                               NULL, Oper);
                                                    }
                                                }
                                                else
                                                {
                                                    if (ResumingFileOnServer && UploadType == utResumeOrOverwriteFile && // resume failed, try overwrite instead
                                                        dataSSLErrorOccured == SSLCONERR_NOERROR)
                                                    {
                                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLRESUMEERR), -1, TRUE);
                                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                                        canClearForceAction = FALSE;
                                                        Queue->UpdateForceAction(CurItem, fqiaOverwrite);
                                                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                                                    }
                                                    else
                                                    {
                                                        if (dataSSLErrorOccured != SSLCONERR_NOERROR)
                                                            lstrcpyn(errText, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 200 + FTP_MAX_PATH);
                                                        else
                                                        {
                                                            errText[0] = 0;
                                                            if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS)
                                                            { // if we have no description of the network error from the server, fall back to the system description
                                                                CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                                            }

                                                            if (errText[0] == 0 && errBuf[0] != 0) // try to use the error text from the proxy server
                                                                lstrcpyn(errText, errBuf, 200 + FTP_MAX_PATH);
                                                        }

                                                        // an error occurred on the item, record this state in it
                                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INCOMPLETEUPLOAD, dataConError,
                                                                               (errText[0] != 0 ? SalamanderGeneral->DupStr(errText) : NULL), Oper);
                                                    }
                                                }
                                            }
                                            lookForNewWork = TRUE;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // except when STOR reports the "cannot create target file name" error (which is when STOR reports
                    // some error + nothing was uploaded) we consider sending STOR/APPE to finish
                    // the forced actions: "overwrite", "resume", and "resume or overwrite"
                    if (CurItem->ForceAction != fqiaNone && canClearForceAction) // the forced action no longer applies
                        Queue->UpdateForceAction(CurItem, fqiaNone);

                    // autorename: record the new name in the item - even if STOR failed, it is still more accurate that the file
                    // was stored under the new name than under the original one - for example when overwriting an existing file this is obvious
                    // NOTE: no need to call Oper->ReportItemChange(CurItem->UID), because RenamedName is used
                    // before TgtName (so the new name has already been displayed)
                    if (uploadTypeIsAutorename && canUseRenamedName)
                        Queue->ChangeTgtNameToRenamedName(curItem);
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    if (UploadType == utAutorename)
                        Queue->ChangeTgtNameToRenamedName(curItem); // even if STOR failed, it is still more accurate that the file was stored under the new name than the original one - for example when overwriting an existing file this is obvious
                    // the result of the STOR command is unknown, invalidate the listing
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportFileUploaded(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                          Oper->GetFTPServerPathType(curItem->TgtPath),
                                                          curItem->TgtName, UPLOADSIZE_UNKNOWN, TRUE);

                    // except for the case when STOR reports the "cannot create target file name" error (which is when STOR reports
                    // some error + nothing was uploaded) we consider sending the STOR/APPE command to complete
                    // the forced actions: "overwrite", "resume", and "resume or overwrite"
                    if (CurItem->ForceAction != fqiaNone) // the forced action no longer applies
                        Queue->UpdateForceAction(CurItem, fqiaNone);

                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // because we are already in CSocketsThread::CritSect, this call is also
                        // possible from CSocket::SocketCritSect (no deadlock risk)
                        if (WorkerUploadDataCon->IsConnected())       // close the "data connection", the system will attempt a "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                        WorkerUploadDataCon->FreeBufferedData();
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // handle any data preparation error
                    if (!HandlePrepareDataError(curItem, lookForNewWork))
                    {
                        if (DataConAllDataTransferred && CommandReplyTimeout)
                        { // we successfully sent all data and the server did not respond => the file is 99.9% uploaded and only the control connection is stuck (happens during uploads/downloads longer than 1.5 hours)
                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLNOTCONFIRMED), -1, TRUE);
                            Queue->UpdateTextFileSizes(curItem, OpenedInFileSizeWithCRLF_EOLs, OpenedInFileNumberOfEOLs);
                            Queue->UpdateForceAction(CurItem, fqiaUploadTestIfFinished);
                        }
                        conClosedRetryItem = TRUE;
                    }
                    break;
                }
                }
                break;
            }

            case fwssWorkCopyDelayedAutoRetry: // copy/move file: wait WORKER_DELAYEDAUTORETRYTIMEOUT milliseconds for auto-retry (so all unexpected replies from the server can arrive)
            {
                if (event == fweDelayedAutoRetry) // time to perform the auto-retry
                {
                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // at least this worker will look for new work, so some worker will take care of this item (no need to post "new work available")
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadWaitForDELERes: // upload copy/move file: pderASCIIForBinaryFile: waiting for the DELE command result (delete the target file)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" replies are ignored (they are only written to the log)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    { // the target file/link was deleted successfully
                        // if the file/link was deleted, update the listing cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        curItem->TgtName, FALSE);
                    }
                    // else; // a deletion failure does not bother us; the user will resolve overwrite/append in the Solve Error dialog
                    HandlePrepareDataError(curItem, lookForNewWork); // PrepareDataError == pderASCIIForBinaryFile
                    break;
                }

                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                {
                    // if we do not know whether the file/link was deleted, invalidate the listing cache
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                    Oper->GetFTPServerPathType(curItem->TgtPath),
                                                    curItem->TgtName, TRUE);
                    HandlePrepareDataError(curItem, lookForNewWork); // PrepareDataError == pderASCIIForBinaryFile
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadCopyTransferFinished: // the target file has been uploaded; for Move, try deleting the source file on disk
            {
                if (CurItem->Type == fqitUploadMoveFile)
                { // Move - try deleting the source file on disk
                    if (DiskWorkIsUsed)
                        TRACE_E("Unexpected situation 2 in CFTPWorker::HandleEventInWorkingState5(): DiskWorkIsUsed may not be TRUE here!");
                    InitDiskWork(WORKER_DISKWORKDELFILEFINISHED, fdwtDeleteFile, CurItem->Path, CurItem->Name,
                                 fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
                    if (FTPDiskThread->AddWork(&DiskWork))
                    {
                        DiskWorkIsUsed = TRUE;
                        SubState = fwssWorkUploadDelFileWaitForDisk; // wait for the result
                    }
                    else // unable to delete the source file, cannot continue processing the item
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                }
                else // Copy - nothing more to handle, done
                {
                    SubState = fwssWorkUploadCopyDone;
                    nextLoop = TRUE;
                }
                break;
            }

            case fwssWorkUploadDelFileWaitForDisk:        // upload copy/move file: waiting for the disk operation to finish (deleting the source file)
            case fwssWorkUploadDelFileWaitForDiskAftQuit: // upload copy/move file: after sending the "QUIT" command + waiting for the disk operation to finish (deleting the source file)
            {
                if (event == fweWorkerShouldStop && ShouldStop) // we are supposed to stop the worker
                {
                    if (SubState != fwssWorkUploadDelFileWaitForDiskAftQuit && !SocketClosed)
                    {
                        SubState = fwssWorkUploadDelFileWaitForDiskAftQuit; // so we do not send "QUIT" multiple times
                        sendQuitCmd = TRUE;                                 // we are supposed to finish and the connection is still open -> send the server the "QUIT" command (ignore the reply; it should just close the connection and nothing else matters now)
                    }
                }
                else
                {
                    if (event == fweDiskWorkDelFileFinished) // we have the disk operation result (file deletion)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // notify that the worker has finished (for the other waiting threads)

                        // if we have already sent QUIT, prevent sending QUIT again from the new state
                        quitCmdWasSent = SubState == fwssWorkUploadDelFileWaitForDiskAftQuit;

                        if (DiskWork.State == sqisNone)
                        { // the file was deleted successfully
                            SubState = fwssWorkUploadCopyDone;
                            nextLoop = TRUE;
                        }
                        else // an error occurred while deleting the file
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            lookForNewWork = TRUE; // quitCmdWasSent is already set
                        }
                    }
                }
                break;
            }

            case fwssWorkUploadCopyDone: // upload copy/move file: finished, move on to the next item
            {
                // the item was completed successfully, store this state in it
                Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                lookForNewWork = TRUE;
                break;
            }
            }
            if (!nextLoop)
                break;
        }

        if (existingItem != NULL)
        {
            SalamanderGeneral->Free(existingItem->Name);
            delete existingItem;
        }

        // before closing the file we must cancel the disk work (to avoid working with a
        // closed file handle and to allow the worker to be stopped)
        if (DiskWorkIsUsed && (conClosedRetryItem || lookForNewWork || handleShouldStop))
        {
            BOOL workIsInProgress;
            if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
            {
                if (workIsInProgress)
                    DiskWork.FlushDataBuffer = NULL; // work is in progress; we cannot free the buffer with read data, leave it to the disk-work thread (see the cancellation section) - we can write to DiskWork because after Cancel the disk thread must no longer access it (it might not even exist)
            }
            // if we cancelled the work before it started, we must release the flush buffer, and
            // if the work has already finished, release the flush buffer here because fweDiskWorkReadFinished
            // it will be delivered elsewhere (where it will be ignored)
            if (DiskWork.FlushDataBuffer != NULL)
            {
                free(DiskWork.FlushDataBuffer);
                DiskWork.FlushDataBuffer = NULL;
            }

            DiskWorkIsUsed = FALSE;
            ReportWorkerMayBeClosed(); // notify that the worker has finished (for the other waiting threads)
        }
    }
}
