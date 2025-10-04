// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

void CFTPWorker::HandleEventInWorkingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                           BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                           int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                           int replyCode)
{
    char ftpPath[FTP_MAX_PATH];
    char errText[200 + FTP_MAX_PATH];
    char hostBuf[HOST_MAX_SIZE];
    unsigned short port;
    char userBuf[USER_MAX_SIZE];
    if (SubState != fwssWorkStopped)
    {
        BOOL conClosedRetryItem = FALSE;
        BOOL lookForNewWork = FALSE;
        BOOL handleShouldStop = FALSE;
        BOOL quitCmdWasSent = FALSE;
        if (SubState == fwssNone) // before starting work we test ShouldStop
        {
            SubState = fwssWorkStartWork;

            if (CurItem->Type == fqitUploadCopyExploreDir || CurItem->Type == fqitUploadMoveExploreDir)
            {
                _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_LOGMSGEXPLORINGDIR),
                            ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtName,
                            ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath);
                Logs.LogMessage(LogUID, errText, -1, TRUE);
            }

            if (ShouldStop)
                handleShouldStop = TRUE; // postActivate = TRUE is unnecessary; with ShouldStop==TRUE the worker will not start working
            else
                goto SKIP_TEST; // so we do not needlessly fiddle with postActivate (optimization)
        }
        else
        {
        SKIP_TEST:
            if (CurItem != NULL) // "always true"
            {
                // if the item uses a data connection (listing and download), handle
                // messages about opening and closing the data connection
                if ((event == fweDataConConnectedToServer || event == fweDataConConnectionClosed) &&
                    (CurItem->Type == fqitDeleteExploreDir ||
                     CurItem->Type == fqitCopyExploreDir ||
                     CurItem->Type == fqitMoveExploreDir ||
                     CurItem->Type == fqitMoveExploreDirLink ||
                     CurItem->Type == fqitChAttrsExploreDir ||
                     CurItem->Type == fqitChAttrsExploreDirLink ||
                     CurItem->Type == fqitCopyFileOrFileLink ||
                     CurItem->Type == fqitMoveFileOrFileLink ||
                     UploadDirGetTgtPathListing))
                {
                    if (event == fweDataConConnectedToServer)
                    {
                        if (ShouldStop)
                        {
                            if (WorkerDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // since we are already inside CSocketsThread::CritSect, this call
                                // is also possible from CSocket::SocketCritSect (no risk of deadlock)
                                if (WorkerDataCon->IsConnected())       // close the data connection; the system will try a "graceful"
                                    WorkerDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                                WorkerDataCon->FreeFlushData();
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }
                        }
                        else
                        {
                            if (ShouldBePaused && WorkerDataCon != NULL) // pausing right before the data transfer starts
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                WorkerDataCon->UpdatePauseStatus(TRUE);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                            }

                            if (WorkerDataConState == wdcsWaitingForConnection)
                                WorkerDataConState = wdcsTransferingData;
                            else
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweDataConConnectedToServer: WorkerDataConState is not wdcsWaitingForConnection!");

                            // start periodic updates of the "download" status display (listing + copy & move from FTP to disk)
                            if (StatusType != wstNone)
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweDataConConnectedToServer: unexpected situation: StatusType != wstNone");
                            StatusType = wstDownloadStatus;
                            StatusConnectionIdleTime = 0;
                            StatusSpeed = 0;
                            StatusTransferred.Set(0, 0);
                            StatusTotal.Set(-1, -1);
                            LastTimeEstimation = -1;

                            // since we are already inside CSocketsThread::CritSect, this call
                            // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + 100 /* perform the first status update "immediately" */,
                                                    WORKER_STATUSUPDATETIMID, NULL); // ignore the error; at worst the status will not update
                        }
                    }
                    else // fweDataConConnectionClosed
                    {
                        if (WorkerDataCon != NULL)
                        {
                            BOOL usesFlushData = CurItem->Type == fqitCopyFileOrFileLink || CurItem->Type == fqitMoveFileOrFileLink;
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            DWORD dataConError;
                            BOOL dataConLowMem;
                            BOOL dataConNoDataTransTimeout;
                            int dataSSLErrorOccured;
                            BOOL dataConDecomprErrorOccured;
                            WorkerDataCon->GetError(&dataConError, &dataConLowMem, NULL, &dataConNoDataTransTimeout,
                                                    &dataSSLErrorOccured, &dataConDecomprErrorOccured);
                            //BOOL dataConDecomprMissingStreamEnd = WorkerDataCon->GetDecomprMissingStreamEnd(); // unfortunately this test cannot be used; for example, Serv-U 7 and 8 simply do not terminate the stream
                            BOOL allDataFlushed = usesFlushData ? WorkerDataCon->AreAllDataFlushed(TRUE) : TRUE;
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            DataConAllDataTransferred = !dataConDecomprErrorOccured && /*!dataConDecomprMissingStreamEnd &&*/
                                                        dataSSLErrorOccured == SSLCONERR_NOERROR &&
                                                        dataConError == NO_ERROR && !dataConLowMem &&
                                                        !dataConNoDataTransTimeout &&
                                                        (!usesFlushData ||
                                                         !DiskWorkIsUsed && allDataFlushed && FlushDataError == fderNone);
                        }
                        if (WorkerDataConState == wdcsTransferingData)
                            WorkerDataConState = wdcsTransferFinished;
                        else
                        {
                            if (WorkerDataConState != wdcsWaitingForConnection) // this also arrives when the data-connection connect fails
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweDataConConnectionClosed: WorkerDataConState is not wdcsTransferingData!");
                        }
                    }
                }
                // if the item uses an upload data connection (uploading files), handle
                // messages about opening and closing the upload data connection
                if ((event == fweUplDataConConnectedToServer || event == fweUplDataConConnectionClosed) &&
                    (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile))
                {
                    if (event == fweUplDataConConnectedToServer)
                    {
                        if (ResumingFileOnServer) // APPE caused the data connection to open -> APPE is probably functional (implemented)
                            Oper->SetDataConWasOpenedForAppendCmd(TRUE);
                        if (ShouldStop)
                        {
                            if (WorkerUploadDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // since we are already inside CSocketsThread::CritSect, this call
                                // is also possible from CSocket::SocketCritSect (no risk of deadlock)
                                if (WorkerUploadDataCon->IsConnected())       // close the data connection; the system will try a "graceful"
                                    WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (we will not learn the result)
                                WorkerUploadDataCon->FreeBufferedData();
                                DeleteSocket(WorkerUploadDataCon);
                                WorkerUploadDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }
                        }
                        else
                        {
                            if (ShouldBePaused && WorkerUploadDataCon != NULL) // pausing right before the data transfer starts
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                WorkerUploadDataCon->UpdatePauseStatus(TRUE);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                            }

                            if (WorkerDataConState == wdcsWaitingForConnection)
                                WorkerDataConState = wdcsTransferingData;
                            else
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweUplDataConConnectedToServer: WorkerDataConState is not wdcsWaitingForConnection!");

                            // start periodic updates of the "upload" status display
                            if (StatusType != wstNone)
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweUplDataConConnectedToServer: unexpected situation: StatusType != wstNone");
                            StatusType = wstUploadStatus;
                            StatusConnectionIdleTime = 0;
                            StatusSpeed = 0;
                            StatusTransferred.Set(0, 0);
                            StatusTotal.Set(-1, -1);
                            LastTimeEstimation = -1;

                            // since we are already inside CSocketsThread::CritSect, this call
                            // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + 100 /* we perform the first status update "immediately" */,
                                                    WORKER_STATUSUPDATETIMID, NULL); // ignore the error; at worst the status will not update
                        }
                    }
                    else // fweUplDataConConnectionClosed
                    {
                        if (WorkerUploadDataCon != NULL)
                        {
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            BOOL allDataTransferred = WorkerUploadDataCon->AllDataTransferred();
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            DataConAllDataTransferred = allDataTransferred;
                        }
                        if (WorkerDataConState == wdcsTransferingData)
                            WorkerDataConState = wdcsTransferFinished;
                        else
                        {
                            if (WorkerDataConState != wdcsWaitingForConnection) // also arrives when the data connection connect fails
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweUplDataConConnectionClosed: WorkerDataConState is not wdcsTransferingData!");
                        }
                    }
                }
                if (event == fweWorkerShouldPause || event == fweWorkerShouldResume)
                { // pausing/resuming during a data transfer
                    BOOL pause = ShouldBePaused;
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerDataCon->UpdatePauseStatus(pause);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerUploadDataCon->UpdatePauseStatus(pause);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                }
                switch (CurItem->Type)
                {
                case fqitDeleteExploreDir:      // explore a directory for delete (note: directory links are deleted as a whole, the operation's purpose is fulfilled and nothing extra is removed) (CFTPQueueItemDelExplore object)
                case fqitCopyExploreDir:        // explore a directory or directory link for copying (CFTPQueueItemCopyMoveExplore object)
                case fqitMoveExploreDir:        // explore a directory for move (deletes the directory after completion) (CFTPQueueItemCopyMoveExplore object)
                case fqitMoveExploreDirLink:    // explore a directory link for move (deletes the directory link after completion) (CFTPQueueItemCopyMoveExplore object)
                case fqitChAttrsExploreDir:     // explore a directory for changing attributes (also adds an item for changing directory attributes) (CFTPQueueItemChAttrExplore object)
                case fqitChAttrsExploreDirLink: // explore a directory link for changing attributes (CFTPQueueItem object)
                {
                    HandleEventInWorkingState2(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                               cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                               conClosedRetryItem, lookForNewWork, handleShouldStop, NULL);
                    break;
                }

                case fqitUploadCopyExploreDir: // upload: explore a directory for copying (CFTPQueueItemCopyMoveUploadExplore object)
                case fqitUploadMoveExploreDir: // upload: explore a directory for move (deletes the directory after completion) (CFTPQueueItemCopyMoveUploadExplore object)
                {
                    if (UploadDirGetTgtPathListing) // list the target path into the upload listing cache
                    {
                        BOOL listingNotAccessible;
                        HandleEventInWorkingState2(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, &listingNotAccessible);
                        if (handleShouldStop || conClosedRetryItem || lookForNewWork)
                        { // listing failed, inform any waiting workers about it
                            UploadDirGetTgtPathListing = FALSE;
                            Oper->GetUserHostPort(userBuf, hostBuf, &port);
                            CFTPServerPathType pathType = Oper->GetFTPServerPathType(((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath);
                            BOOL listingOKErrorIgnored;
                            // calling UploadListingCache.ListingFailed() is possible only because we are inside CSocketsThread::CritSect
                            UploadListingCache.ListingFailed(userBuf, hostBuf, port,
                                                             ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath,
                                                             pathType, listingNotAccessible, NULL, &listingOKErrorIgnored);
                            if (listingOKErrorIgnored && lookForNewWork) // a listing error is reported on the item; cancel this error
                            {
                                lookForNewWork = FALSE;
                                Queue->UpdateItemState(CurItem, sqisProcessing, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                StatusType = wstNone;
                                SubState = fwssWorkStartWork;
                                Oper->ReportItemChange(CurItem->UID); // request a redraw of the item (in case the error already appeared -- almost impossible)
                                postActivate = TRUE;                  // impulse to continue working
                                reportWorkerChange = TRUE;            // we need to hide any progress of fetching the listing

                                // since we are already inside CSocketsThread::CritSect, this call
                                // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
                                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // cancel any timer from previous work
                            }
                        }
                    }
                    else // the remaining work goes here
                    {
                        HandleEventInWorkingState4(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, quitCmdWasSent);
                    }
                    break;
                }

                case fqitCopyResolveLink:    // copy: detect whether this is a link to a file or directory (CFTPQueueItemCopyOrMove object)
                case fqitMoveResolveLink:    // move: detect whether this is a link to a file or directory (CFTPQueueItemCopyOrMove object)
                case fqitChAttrsResolveLink: // change attributes: detect whether this is a link to a directory (CFTPQueueItem object)
                {
                    switch (SubState)
                    {
                    case fwssWorkStartWork: // determine which path we should switch to on the server and send CWD
                    {
                        // before resolving the link for change-attr we must reset the speed meter (the resolve speed
                        // is not measured) - this results in "(unknown)" time-left being shown in the operation dialog
                        if (CurItem->Type == fqitChAttrsResolveLink)
                        {
                            Oper->GetGlobalTransferSpeedMeter()->Clear();
                            Oper->GetGlobalTransferSpeedMeter()->JustConnected();
                        }

                        lstrcpyn(ftpPath, CurItem->Path, FTP_MAX_PATH);
                        CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
                        if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, CurItem->Name, TRUE))
                        { // we have the path, send CWD to the examined directory on the server
                            _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_LOGMSGRESOLVINGLINK), ftpPath);
                            Logs.LogMessage(LogUID, errText, -1, TRUE);

                            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                              ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // cannot report an error
                            sendCmd = TRUE;
                            SubState = fwssWorkResLnkWaitForCWDRes;

                            HaveWorkingPath = FALSE; // we are changing the current working path on the server
                        }
                        else // path syntax error or the path would become too long
                        {
                            // error on the item; write this state into it
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INVALIDPATHTOLINK, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                        break;
                    }

                    case fwssWorkResLnkWaitForCWDRes: // resolve-link: waiting for the "CWD" result (changing to the examined link - if it succeeds, it is a directory link)
                    {
                        switch (event)
                        {
                        // case fweCmdInfoReceived:  // ignore "1xx" replies (only written to the Log)
                        case fweCmdReplyReceived:
                        {
                            BOOL chAttrsResolve = CurItem->Type == fqitChAttrsResolveLink;
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||
                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)
                            {
                                BOOL err = FALSE;
                                CFTPQueueItem* item = NULL;
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // success, the link points to a directory
                                {
                                    switch (CurItem->Type)
                                    {
                                    case fqitCopyResolveLink: // copy: detect whether this is a link to a file or directory (CFTPQueueItemCopyOrMove object)
                                    case fqitMoveResolveLink: // move: detect whether this is a link to a file or directory (CFTPQueueItemCopyOrMove object)
                                    {
                                        item = new CFTPQueueItemCopyMoveExplore;
                                        if (item != NULL)
                                        {
                                            ((CFTPQueueItemCopyMoveExplore*)item)->SetItemCopyMoveExplore(((CFTPQueueItemCopyOrMove*)CurItem)->TgtPath, ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName, TGTDIRSTATE_UNKNOWN);
                                            item->SetItem(CurItem->ParentUID,
                                                          CurItem->Type == fqitCopyResolveLink ? fqitCopyExploreDir : fqitMoveExploreDirLink,
                                                          sqisWaiting, ITEMPR_OK, CurItem->Path, CurItem->Name);
                                        }
                                        else
                                        {
                                            TRACE_E(LOW_MEMORY);
                                            err = TRUE;
                                        }
                                        break;
                                    }

                                    case fqitChAttrsResolveLink: // change attributes: detect whether this is a link to a directory (CFTPQueueItem object)
                                    {
                                        item = new CFTPQueueItem;
                                        if (item != NULL)
                                        {
                                            item->SetItem(CurItem->ParentUID, fqitChAttrsExploreDirLink, sqisWaiting,
                                                          ITEMPR_OK, CurItem->Path, CurItem->Name);
                                        }
                                        else
                                        {
                                            TRACE_E(LOW_MEMORY);
                                            err = TRUE;
                                        }
                                        break;
                                    }
                                    }
                                }
                                else // permanent error; the link probably points to a file (but it could also be "550 Permission denied", unfortunately 550 is also "550 Not a directory", so it is indistinguishable...)
                                {
                                    switch (CurItem->Type)
                                    {
                                    case fqitCopyResolveLink: // copy: detect whether this is a link to a file or directory (CFTPQueueItemCopyOrMove object)
                                    case fqitMoveResolveLink: // move: detect whether this is a link to a file or directory (CFTPQueueItemCopyOrMove object)
                                    {
                                        item = new CFTPQueueItemCopyOrMove;
                                        if (item != NULL)
                                        {
                                            ((CFTPQueueItemCopyOrMove*)item)->SetItemCopyOrMove(((CFTPQueueItemCopyOrMove*)CurItem)->TgtPath, ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName, CQuadWord(-1, -1), ((CFTPQueueItemCopyOrMove*)CurItem)->AsciiTransferMode, TRUE, TGTFILESTATE_UNKNOWN, ((CFTPQueueItemCopyOrMove*)CurItem)->DateAndTimeValid, ((CFTPQueueItemCopyOrMove*)CurItem)->Date, ((CFTPQueueItemCopyOrMove*)CurItem)->Time);
                                            item->SetItem(CurItem->ParentUID,
                                                          CurItem->Type == fqitCopyResolveLink ? fqitCopyFileOrFileLink : fqitMoveFileOrFileLink,
                                                          sqisWaiting, ITEMPR_OK, CurItem->Path, CurItem->Name);
                                        }
                                        else
                                        {
                                            TRACE_E(LOW_MEMORY);
                                            err = TRUE;
                                        }
                                        break;
                                    }

                                        // case fqitChAttrsResolveLink:     // change attributes: detect whether this is a link to a directory (CFTPQueueItem object)
                                        // changing attributes on a link makes no sense, so simply remove the resolve item from the queue
                                    }
                                }

                                if (!err)
                                {
                                    // multiple operations are running on the data; the others must wait for all of them to finish,
                                    // otherwise they would work with inconsistent data
                                    Queue->LockForMoreOperations();

                                    int curItemParent = CurItem->ParentUID;
                                    if (Queue->ReplaceItemWithListOfItems(CurItem->UID, &item, item != NULL ? 1 : 0))
                                    { // CurItem has already been deallocated; it was replaced with the 'item' entry (or just removed)
                                        CurItem = NULL;
                                        BOOL itemAdded = item != NULL;
                                        item = NULL; // prevent 'item' from being deallocated; it has already been added to the queue

                                        // for the item/operation CurItem->ParentUID decrease NotDone by one (for CurItem in the state
                                        // sqisProcessing) and if we add the 'item' entry, increase NotDone by one (the new
                                        // entry is in state sqisWaiting; the other three counters are zero)
                                        Oper->AddToItemOrOperationCounters(curItemParent, (itemAdded ? 1 : 0) - 1, 0, 0, 0, FALSE);

                                        Queue->UnlockForMoreOperations();

                                        Oper->ReportItemChange(-1); // request a redraw of all items

                                        // this worker will have to look for more work
                                        State = fwsLookingForWork; // no need to call Oper->OperationStatusMaybeChanged(); it does not change the operation state (it is not paused and will not be after this change)
                                        SubState = fwssNone;
                                        postActivate = TRUE; // post an activation for the worker's next state
                                        reportWorkerChange = TRUE;

                                        // no need to inform any sleeping worker that new work appeared, because
                                        // this worker will look for more work and therefore find any new entry
                                    }
                                    else
                                    {
                                        err = TRUE; // not enough memory -> record the error in the item

                                        Queue->UnlockForMoreOperations();
                                    }
                                }

                                if (err)
                                {
                                    // error on the item; write this state into it
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                if (item != NULL)
                                    delete item;
                            }
                            else // an error occurred; report it to the user and process the next queue item
                            {
                                CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESOLVELNK, NO_ERROR,
                                                       SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                                       Oper);
                                lookForNewWork = TRUE;
                            }

                            // if this is a resolve link for change-attr, we must reset the speed meter
                            // (resolve speed is not measured, so this moment can be the beginning of measuring the speed of delete/change-attr operations)
                            // to ensure the time-left in the operation dialog displays correctly
                            if (chAttrsResolve)
                            {
                                Oper->GetGlobalTransferSpeedMeter()->Clear();
                                Oper->GetGlobalTransferSpeedMeter()->JustConnected();
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
                    }
                    break;
                }

                case fqitDeleteLink:         // delete for a link (CFTPQueueItemDel object)
                case fqitDeleteFile:         // delete for a file (CFTPQueueItemDel object)
                case fqitDeleteDir:          // delete for a directory (CFTPQueueItemDir object)
                case fqitCopyFileOrFileLink: // copy a file or a link to a file (CFTPQueueItemCopyOrMove object)
                case fqitMoveFileOrFileLink: // move a file or a link to a file (CFTPQueueItemCopyOrMove object)
                case fqitMoveDeleteDir:      // delete a directory after moving its contents (CFTPQueueItemDir object)
                case fqitMoveDeleteDirLink:  // delete a link to a directory after moving its contents (CFTPQueueItemDir object)
                case fqitChAttrsFile:        // change file attributes (note: link attributes cannot be changed) (CFTPQueueItemChAttr object)
                case fqitChAttrsDir:         // change directory attributes (CFTPQueueItemChAttrDir object)
                {
                    while (1)
                    {
                        BOOL nextLoop = FALSE;
                        if ((CurItem->Type == fqitCopyFileOrFileLink || CurItem->Type == fqitMoveFileOrFileLink) &&
                            SubState != fwssWorkStartWork && SubState != fwssWorkSimpleCmdWaitForCWDRes)
                        {
                            HandleEventInWorkingState3(event, sendQuitCmd, postActivate, buf, errBuf,
                                                       cmdLen, sendCmd, reply, replySize, replyCode, errText,
                                                       conClosedRetryItem, lookForNewWork, handleShouldStop);
                        }
                        else
                        {
                            switch (SubState)
                            {
                            case fwssWorkStartWork: // switch to the item's directory on the server (if we are not there yet)
                            {
                                // first log which work we are about to perform
                                int opDescrResID = 0;
                                const char* tgtName = NULL;
                                switch (CurItem->Type)
                                {
                                case fqitDeleteLink:
                                    opDescrResID = IDS_LOGMSGDELETELINK;
                                    break; // delete for a link (CFTPQueueItemDel object)
                                case fqitDeleteFile:
                                    opDescrResID = IDS_LOGMSGDELETEFILE;
                                    break; // delete for a file (CFTPQueueItemDel object)

                                case fqitMoveDeleteDir: // delete a directory after moving its contents (CFTPQueueItemDir object)
                                case fqitDeleteDir:
                                    opDescrResID = IDS_LOGMSGDELETEDIR;
                                    break; // delete for a directory (CFTPQueueItemDir object)

                                case fqitCopyFileOrFileLink: // copy a file or a link to a file (CFTPQueueItemCopyOrMove object)
                                case fqitMoveFileOrFileLink: // move a file or a link to a file (CFTPQueueItemCopyOrMove object)
                                {
                                    if (strcmp(CurItem->Name, ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName) != 0)
                                    {
                                        opDescrResID = IDS_LOGMSGDOWNLOADFILE2;
                                        tgtName = ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName;
                                    }
                                    else
                                        opDescrResID = IDS_LOGMSGDOWNLOADFILE;
                                    break;
                                }

                                case fqitMoveDeleteDirLink:
                                    opDescrResID = IDS_LOGMSGDELETEDIRLINK;
                                    break; // delete a link to a directory after moving its contents (CFTPQueueItemDir object)
                                case fqitChAttrsFile:
                                    opDescrResID = IDS_LOGMSGCHATTRSFILE;
                                    break; // change file attributes (note: link attributes cannot be changed) (CFTPQueueItemChAttr object)
                                case fqitChAttrsDir:
                                    opDescrResID = IDS_LOGMSGCHATTRSDIR;
                                    break; // change directory attributes (CFTPQueueItemChAttrDir object)
                                }
                                _snprintf_s(errText, _TRUNCATE, LoadStr(opDescrResID), CurItem->Name, tgtName);
                                Logs.LogMessage(LogUID, errText, -1, TRUE);

                                BOOL canContinue = TRUE;
                                if (CurItem->Type == fqitDeleteFile || CurItem->Type == fqitDeleteLink)
                                {
                                    Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                    if (LockedFileUID != 0)
                                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): LockedFileUID != 0!");
                                    if (!FTPOpenedFiles.OpenFile(userBuf, hostBuf, port, CurItem->Path,
                                                                 Oper->GetFTPServerPathType(CurItem->Path),
                                                                 CurItem->Name, &LockedFileUID, ffatDelete))
                                    { // another operation is already running on this file; let the user try again later
                                        // error on the item; write this state into it
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_SRCFILEINUSE, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                        canContinue = FALSE;
                                    }
                                    // else ; // the file on the server is not open yet; we can delete it
                                }
                                if (canContinue)
                                {
                                    if (!HaveWorkingPath || strcmp(WorkingPath, CurItem->Path) != 0)
                                    { // we need to change the working path (assumption: the server keeps returning the same path string - the one
                                        // that reached the item during explore-dir or from the panel, in both cases it was the path returned
                                        // by the server in response to the PWD command)
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdChangeWorkingPath, &cmdLen, CurItem->Path); // cannot report an error
                                        sendCmd = TRUE;
                                        SubState = fwssWorkSimpleCmdWaitForCWDRes;

                                        HaveWorkingPath = FALSE; // we are changing the current working path on the server
                                    }
                                    else // the working directory is already set
                                    {
                                        SubState = fwssWorkSimpleCmdStartWork;
                                        nextLoop = TRUE;
                                    }
                                }
                                break;
                            }

                            case fwssWorkSimpleCmdWaitForCWDRes:
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // ignore "1xx" replies (only written to the Log)
                                case fweCmdReplyReceived:
                                {
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // we successfully changed the working path; because this path was "once" returned
                                        // from the server in response to PWD, we assume that PWD would now return this path again
                                        // and therefore we will not send it (optimization with hopefully low risk)
                                        HaveWorkingPath = TRUE;
                                        lstrcpyn(WorkingPath, CurItem->Path, FTP_MAX_PATH);
                                        SubState = fwssWorkSimpleCmdStartWork;
                                        nextLoop = TRUE;
                                    }
                                    else // an error occurred; report it to the user and process the next queue item
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

                            case fwssWorkSimpleCmdStartWork: // start working (the working directory is already set)
                            {
                                if (ShouldStop)
                                    handleShouldStop = TRUE; // check whether the worker should stop
                                else
                                {
                                    switch (CurItem->Type)
                                    {
                                    case fqitDeleteFile:        // delete for a file (CFTPQueueItemDel object)
                                    case fqitDeleteLink:        // delete for a link (CFTPQueueItemDel object)
                                    case fqitMoveDeleteDirLink: // delete a link to a directory after moving its contents (CFTPQueueItemDir object)
                                    {
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdDeleteFile, &cmdLen, CurItem->Name); // cannot report an error
                                        sendCmd = TRUE;
                                        SubState = fwssWorkDelFileWaitForDELERes;
                                        break;
                                    }

                                    case fqitDeleteDir:     // delete for a directory (CFTPQueueItemDir object)
                                    case fqitMoveDeleteDir: // delete a directory after moving its contents (CFTPQueueItemDir object)
                                    {
                                        char vmsDirName[MAX_PATH + 10];
                                        char* dirName = CurItem->Name;
                                        BOOL isVMS = Oper->GetFTPServerPathType(CurItem->Path) == ftpsptOpenVMS;
                                        if (isVMS)
                                        {
                                            FTPMakeVMSDirName(vmsDirName, MAX_PATH + 10, CurItem->Name);
                                            dirName = vmsDirName;
                                        }
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdDeleteDir, &cmdLen, dirName); // cannot report an error
                                        sendCmd = TRUE;
                                        SubState = fwssWorkDelDirWaitForRMDRes;
                                        break;
                                    }

                                    case fqitChAttrsFile: // change file attributes (note: link attributes cannot be changed) (CFTPQueueItemChAttr object)
                                    case fqitChAttrsDir:  // change directory attributes (CFTPQueueItemChAttrDir object)
                                    {
                                        DWORD attr = CurItem->Type == fqitChAttrsFile ? ((CFTPQueueItemChAttr*)CurItem)->Attr : ((CFTPQueueItemChAttrDir*)CurItem)->Attr;
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdChangeAttrs, &cmdLen, attr, CurItem->Name); // cannot report an error
                                        sendCmd = TRUE;
                                        SubState = fwssWorkChAttrWaitForCHMODRes;
                                        break;
                                    }
                                    }
                                }
                                break;
                            }

                            case fwssWorkDelFileWaitForDELERes: // deleting a file/link: waiting for the "DELE" result (file/link deletion)
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // ignore "1xx" replies (only written to the Log)
                                case fweCmdReplyReceived:
                                {
                                    BOOL finished = TRUE;
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // the file/link was deleted successfully; record "completed successfully" in the item
                                        Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;

                                        // if the file/link was deleted, update the listing in the cache
                                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                        UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                                        Oper->GetFTPServerPathType(CurItem->Path),
                                                                        CurItem->Name, FALSE);
                                    }
                                    else // an error occurred
                                    {
                                        if (CurItem->Type == fqitDeleteFile)
                                        { // report the error to the user and process the next queue item
                                            CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETODELETEFILE, NO_ERROR,
                                                                   SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                                                   Oper);
                                            lookForNewWork = TRUE;
                                        }
                                        else // try RMD if DELE failed (hypothesis: a directory link might be deletable via RMD)
                                        {    // CurItem->Type is fqitDeleteLink or fqitMoveDeleteDirLink
                                            if (ShouldStop)
                                                handleShouldStop = TRUE; // check whether the worker should stop
                                            else
                                            {
                                                char vmsDirName[MAX_PATH + 10];
                                                char* dirName = CurItem->Name;
                                                BOOL isVMS = Oper->GetFTPServerPathType(CurItem->Path) == ftpsptOpenVMS;
                                                if (isVMS)
                                                {
                                                    FTPMakeVMSDirName(vmsDirName, MAX_PATH + 10, CurItem->Name);
                                                    dirName = vmsDirName;
                                                }
                                                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                                  ftpcmdDeleteDir, &cmdLen, dirName); // cannot report an error
                                                sendCmd = TRUE;
                                                SubState = fwssWorkDelDirWaitForRMDRes;
                                                finished = FALSE;
                                            }
                                        }
                                    }
                                    if (finished) // use only the final command in the operation for speed measurement (RMD may follow DELE; eliminate that here)
                                    {
                                        // the server replied -> add theoretical bytes to the speed meter
                                        Oper->GetGlobalTransferSpeedMeter()->BytesReceived(SMPLCMD_APPROXBYTESIZE, GetTickCount());
                                    }
                                    break;
                                }

                                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                                {
                                    // if we do not know how the file/link deletion ended, invalidate the listing cache
                                    Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                    UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                                    Oper->GetFTPServerPathType(CurItem->Path),
                                                                    CurItem->Name, TRUE);

                                    conClosedRetryItem = TRUE;
                                    break;
                                }
                                }
                                break;
                            }

                            case fwssWorkDelDirWaitForRMDRes: // deleting a directory/link: waiting for the "RMD" result (directory/link deletion)
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // ignore "1xx" replies (only written to the Log)
                                case fweCmdReplyReceived:
                                {
                                    // the server replied -> add theoretical bytes to the speed meter
                                    Oper->GetGlobalTransferSpeedMeter()->BytesReceived(SMPLCMD_APPROXBYTESIZE, GetTickCount());

                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // the directory/link was deleted successfully; record "completed successfully" in the item
                                        Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;

                                        // if the directory/link was deleted, update the listing in the cache
                                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                        UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                                        Oper->GetFTPServerPathType(CurItem->Path),
                                                                        CurItem->Name, FALSE);
                                    }
                                    else // an error occurred; report it to the user and process the next queue item
                                    {    // CurItem->Type is fqitDeleteLink / fqitMoveDeleteDirLink or fqitDeleteDir / fqitMoveDeleteDir
                                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                        Queue->UpdateItemState(CurItem, sqisFailed,
                                                               (CurItem->Type == fqitDeleteLink || CurItem->Type == fqitMoveDeleteDirLink) ? ITEMPR_UNABLETODELETEFILE : ITEMPR_UNABLETODELETEDIR,
                                                               NO_ERROR,
                                                               SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                                               Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    break;
                                }

                                case fweCmdConClosed: // the connection closed/timed out (see ErrorDescr for details) -> try to restore it
                                {
                                    // if we do not know how the directory/link deletion ended, invalidate the listing cache
                                    Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                    UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                                    Oper->GetFTPServerPathType(CurItem->Path),
                                                                    CurItem->Name, TRUE);

                                    conClosedRetryItem = TRUE;
                                    break;
                                }
                                }
                                break;
                            }

                            case fwssWorkChAttrWaitForCHMODRes:       // change attributes: waiting for the "SITE CHMOD" result (changing file/directory mode, likely Unix only)
                            case fwssWorkChAttrWaitForCHMODQuotedRes: // change attributes (name in quotes): waiting for the "SITE CHMOD" result (changing file/directory mode, likely Unix only)
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // ignore "1xx" replies (only written to the Log)
                                case fweCmdReplyReceived:
                                {
                                    BOOL finished = TRUE;
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // the file/directory attributes were changed successfully; record "completed successfully" in the item
                                        Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    else // CurItem->Type is fqitChAttrsFile or fqitChAttrsDir
                                    {
                                        char* s = CurItem->Name;
                                        while (*s != 0 && *s > ' ')
                                            s++;
                                        if (*s != 0 && SubState == fwssWorkChAttrWaitForCHMODRes) // the file/directory name contains white-spaces; try placing the name in quotes (only if we have not tried this yet)
                                        {
                                            if (ShouldStop)
                                                handleShouldStop = TRUE; // check whether the worker should stop
                                            else
                                            {
                                                DWORD attr = CurItem->Type == fqitChAttrsFile ? ((CFTPQueueItemChAttr*)CurItem)->Attr : ((CFTPQueueItemChAttrDir*)CurItem)->Attr;
                                                s = CurItem->Name;
                                                char nameToQuotes[2 * MAX_PATH]; // in the name we must insert the escape char '\\' before '"'
                                                char* d = nameToQuotes;
                                                char* end = nameToQuotes + 2 * MAX_PATH - 1;
                                                while (*s != 0 && d < end)
                                                {
                                                    if (*s == '"')
                                                        *d++ = '\\';
                                                    *d++ = *s++;
                                                }
                                                *d = 0;
                                                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                                  ftpcmdChangeAttrsQuoted, &cmdLen, attr, nameToQuotes); // cannot report an error
                                                sendCmd = TRUE;
                                                SubState = fwssWorkChAttrWaitForCHMODQuotedRes;
                                                finished = FALSE;
                                            }
                                        }
                                        else // an error occurred; report it to the user and process the next queue item
                                        {
                                            CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOCHATTRS, NO_ERROR,
                                                                   SalamanderGeneral->DupStr(errText) /* low memory = the error will have no details */,
                                                                   Oper);
                                            lookForNewWork = TRUE;
                                        }
                                    }
                                    if (finished) // use only the final command in the operation for speed measurement (RMD may follow DELE; eliminate that here)
                                    {
                                        // the server replied -> add theoretical bytes to the speed meter
                                        Oper->GetGlobalTransferSpeedMeter()->BytesReceived(SMPLCMD_APPROXBYTESIZE, GetTickCount());
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
                            }
                        }
                        if (!nextLoop)
                            break;
                    }
                    break;
                }

                case fqitUploadCopyFile: // upload: copy a file (CFTPQueueItemCopyOrMoveUpload object)
                case fqitUploadMoveFile: // upload: move a file (CFTPQueueItemCopyOrMoveUpload object)
                {
                    if (UploadDirGetTgtPathListing) // list the target path into the upload listing cache
                    {
                        BOOL listingNotAccessible;
                        HandleEventInWorkingState2(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, &listingNotAccessible);
                        if (handleShouldStop || conClosedRetryItem || lookForNewWork)
                        { // listing failed; inform any waiting workers about it
                            UploadDirGetTgtPathListing = FALSE;
                            Oper->GetUserHostPort(userBuf, hostBuf, &port);
                            CFTPServerPathType pathType = Oper->GetFTPServerPathType(((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath);
                            BOOL listingOKErrorIgnored;
                            // calling UploadListingCache.ListingFailed() is possible only because we are inside CSocketsThread::CritSect
                            UploadListingCache.ListingFailed(userBuf, hostBuf, port,
                                                             ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath,
                                                             pathType, listingNotAccessible, NULL, &listingOKErrorIgnored);
                            if (listingOKErrorIgnored && lookForNewWork) // a listing error is reported on the item; cancel this error
                            {
                                lookForNewWork = FALSE;
                                Queue->UpdateItemState(CurItem, sqisProcessing, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                StatusType = wstNone;
                                SubState = fwssWorkStartWork;
                                Oper->ReportItemChange(CurItem->UID); // request a redraw of the item (in case the error already appeared -- almost impossible)
                                postActivate = TRUE;                  // impulse to continue working
                                reportWorkerChange = TRUE;            // we need to hide any progress of fetching the listing

                                // since we are already inside CSocketsThread::CritSect, this call
                                // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
                                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // cancel any timer from previous work
                            }
                        }
                    }
                    else // the remaining work goes here
                    {
                        HandleEventInWorkingState5(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, quitCmdWasSent);
                    }
                    break;
                }

                case fqitUploadMoveDeleteDir: // upload: delete a directory after moving its contents (CFTPQueueItemDir object)
                {                             // deleting the directory from disk is handled in the fwsPreparing state (no connection needed, so there is no fwsWorking state)
                    TRACE_E("Unexpected call to CFTPWorker::HandleEventInWorkingState(): operation item type is fqitUploadMoveDeleteDir!");
                    break;
                }

                default:
                {
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): unknown active operation item type!");
                    break;
                }
                }
            }
            else
                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): missing active operation item!");
        }

        if (ShouldStop && handleShouldStop) // we should stop the worker and it is possible to send the "QUIT" command
        {
            if (sendCmd)
                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): can't send QUIT and user command together!");
            if (!SocketClosed)
                sendQuitCmd = TRUE;     // we must finish and have an open connection -> send the server the "QUIT" command (ignore the reply; it should lead to closing the connection and that is all we need now)
            SubState = fwssWorkStopped; // if there is a connection, avoid sending "QUIT" multiple times; if there is no connection, prevent work from continuing unnecessarily
        }
        else
        {
            if (conClosedRetryItem) // the connection broke; end the operation on the item and try to perform the item again
            {
                CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL); // this transfer failed; close the target file (if we are using one at all)
                CloseOpenedInFile();                                   // this transfer failed; close the source file (if we are using one at all)
                if (LockedFileUID != 0)
                {
                    FTPOpenedFiles.CloseFile(LockedFileUID);
                    LockedFileUID = 0;
                }
                if ((CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) &&
                    ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->RenamedName != NULL)
                {
                    Queue->UpdateRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem, NULL);
                    Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                }
                State = fwsPreparing; // no need to call Oper->OperationStatusMaybeChanged(); it does not change the operation state (it is not paused and will not be after this change)
                if (quitCmdWasSent)
                    SubState = fwssPrepQuitSent; // let fwsPreparing know that QUIT was already sent
                else
                    SubState = fwssNone;
                postActivate = TRUE; // post an activation for the worker's next state
                reportWorkerChange = TRUE;

                // since we are already inside CSocketsThread::CritSect, this call
                // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // cancel any timer from previous work
            }
            else
            {
                if (lookForNewWork)
                {
                    CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL); // if the target file is not closed yet, this transfer failed; close the target file (if we are using one at all)
                    CloseOpenedInFile();                                   // if the source file is not closed yet, this transfer failed; close the source file (if we are using one at all)
                    if (LockedFileUID != 0)
                    {
                        FTPOpenedFiles.CloseFile(LockedFileUID);
                        LockedFileUID = 0;
                    }
                    if ((CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) &&
                        ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->RenamedName != NULL)
                    {
                        Queue->UpdateRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem, NULL);
                    }
                    Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                    if (CurItem->GetItemState() == sqisProcessing)
                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): returned item is still in processing state!");

                    CurItem = NULL;
                    State = fwsLookingForWork; // no need to call Oper->OperationStatusMaybeChanged(); it does not change the operation state (it is not paused and will not be after this change)
                    if (quitCmdWasSent)
                        SubState = fwssLookFWQuitSent; // pass to fwsLookingForWork that QUIT was already sent
                    else
                        SubState = fwssNone;
                    postActivate = TRUE; // post an activation for the worker's next state
                    reportWorkerChange = TRUE;

                    // since we are already inside CSocketsThread::CritSect, this call
                    // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
                    SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // cancel any timer from previous work
                }
            }
        }
    }
}

void CFTPWorker::HandleEvent(CFTPWorkerEvent event, char* reply, int replySize, int replyCode)
{
    CALL_STACK_MESSAGE3("CFTPWorker::HandleEvent(%d, , , %d)", (int)event, replyCode);

    char buf[700 + FTP_MAX_PATH];
    char errBuf[50 + FTP_MAX_PATH];
    char host[HOST_MAX_SIZE];

    BOOL sendQuitCmd = FALSE;  // TRUE = an FTP command "QUIT" should be sent
    BOOL postActivate = FALSE; // TRUE = WORKER_ACTIVATE (fweActivate) should be posted

    int cmdLen = 0;
    BOOL sendCmd = FALSE; // TRUE = send the FTP command 'buf' with length 'cmdLen' and log 'errBuf'

    BOOL operStatusMaybeChanged = FALSE; // TRUE = call Oper->OperationStatusMaybeChanged();
    BOOL reportWorkerChange = FALSE;     // TRUE = call Oper->ReportWorkerChange(workerID, FALSE)

    HANDLES(EnterCriticalSection(&WorkerCritSect));

#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount != 1)
        TRACE_E("CFTPWorker::HandleEvent(): WorkerCritSect.RecursionCount=" << WorkerCritSect.RecursionCount);
    if (SocketCritSect.RecursionCount != 1)
        TRACE_E("CFTPWorker::HandleEvent(): SocketCritSect.RecursionCount=" << SocketCritSect.RecursionCount);
#endif

    int workerID = ID;
    switch (State)
    {
    case fwsLookingForWork:
    {
        if (ShouldStop) // we should stop the worker
        {
            if (SubState != fwssLookFWQuitSent && !SocketClosed)
            {
                SubState = fwssLookFWQuitSent; // prevent sending "QUIT" multiple times
                sendQuitCmd = TRUE;            // we must finish and have an open connection -> send the server the "QUIT" command (ignore the reply; it should lead to closing the connection and that is all we need now)
            }
        }
        else
        {
            if (!ShouldBePaused) // if we do not need to stay paused: normal operation
            {
                if (CurItem != NULL)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEvent(): CurItem is not NULL in state fwsLookingForWork!");
                CurItem = Queue->GetNextWaitingItem(Oper); // try to find an item in the queue
                if (CurItem != NULL)                       // found an item in the queue to process
                {
                    State = fwsPreparing; // no need to call Oper->OperationStatusMaybeChanged(); it does not change the operation state (it is not paused and will not be after this change)
                    SubState = fwssNone;
                    postActivate = TRUE; // post an activation for the worker's next state
                    reportWorkerChange = TRUE;
                    Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                }
                else // did not find any item in the queue to process
                {
                    State = fwsSleeping;
                    operStatusMaybeChanged = TRUE;
                    SubState = fwssNone;
                    // postActivate = TRUE;  // in fwsSleeping nothing happens on fweActivate, so posting it makes no sense
                    reportWorkerChange = TRUE;
                }
            }
        }
        break;
    }

    case fwsSleeping:
    {
        if (ShouldStop) // we should stop the worker
        {
            if (SubState != fwssSleepingQuitSent && !SocketClosed)
            {
                SubState = fwssSleepingQuitSent; // prevent sending "QUIT" multiple times
                sendQuitCmd = TRUE;              // we must finish and have an open connection -> send the server the "QUIT" command (ignore the reply; it should lead to closing the connection and that is all we need now)
            }
        }
        else
        {
            if (event == fweWakeUp)
            {
                // waking the worker (a new item in the queue is probably waiting to be processed)
                State = fwsLookingForWork;
                operStatusMaybeChanged = TRUE;
                SubState = fwssNone;
                postActivate = TRUE; // post an activation for the worker's next state
                reportWorkerChange = TRUE;

                if (SocketClosed)
                    ConnectAttemptNumber = 0; // give the awakened worker without a connection a chance for a new connect
            }
        }
        break;
    }

    case fwsPreparing:
    {
        HandleEventInPreparingState(event, sendQuitCmd, postActivate, reportWorkerChange);
        break;
    }

    case fwsConnecting:
    {
        HandleEventInConnectingState(event, sendQuitCmd, postActivate, reportWorkerChange, buf,
                                     errBuf, host, cmdLen, sendCmd, reply, replySize, replyCode,
                                     operStatusMaybeChanged);
        break;
    }

    case fwsWaitingForReconnect:
    {
        if (ShouldStop || ShouldBePaused)
        {
            // since we are already inside CSocketsThread::CritSect, this call
            // is also possible from CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of deadlock)
            SocketsThread->DeleteTimer(UID, WORKER_RECONTIMEOUTTIMID);
        }
        else // normal activity
        {
            if (event == fweReconTimeout || event == fweWorkerShouldResume) // proceed to another connection attempt
            {
                State = fwsConnecting; // no need to call Oper->OperationStatusMaybeChanged(); it does not change the operation state (it is not paused and will not be after this change)
                SubState = fwssNone;
                postActivate = TRUE; // post an activation for the worker's next state
                reportWorkerChange = TRUE;
            }
        }
        break;
    }

    case fwsConnectionError:
    {
        CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL); // this transfer failed; close the target file (if we are using one at all)
        CloseOpenedInFile();                                   // this transfer failed; close the source file (if we are using one at all)
        if (CurItem != NULL)
        {
            ReturnCurItemToQueue(); // return the item to the queue
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // since we are already inside CSocketsThread::CritSect, this call
            // is also possible from CSocket::SocketCritSect (no risk of deadlock)
            Oper->PostNewWorkAvailable(TRUE); // inform any first sleeping worker that new work is available
            HANDLES(EnterCriticalSection(&WorkerCritSect));
        }
        if (!ShouldStop) // normal operation
        {
            if (event == fweNewLoginParams)
            {
                Logs.LogMessage(LogUID, LoadStr(IDS_WORKERLOGPARCHANGED), -1);
                ErrorDescr[0] = 0; // do not report the error from this point on (the user is trying to fix it)
                if (UnverifiedCertificate != NULL)
                    UnverifiedCertificate->Release();
                UnverifiedCertificate = NULL;
                ConnectAttemptNumber = 0; // allow the connect to succeed even with only one attempt remaining
                State = fwsLookingForWork;
                operStatusMaybeChanged = TRUE;
                SubState = fwssNone;
                postActivate = TRUE; // post an activation for the worker's next state
                reportWorkerChange = TRUE;
            }
        }
        break;
    }

    case fwsWorking:
    {
        HandleEventInWorkingState(event, sendQuitCmd, postActivate, reportWorkerChange, buf,
                                  errBuf, host, cmdLen, sendCmd, reply, replySize, replyCode);
        break;
    }

    case fwsStopped:
    {
        TRACE_I("CFTPWorker::HandleEvent(): worker is already stopped, ignoring event: " << event);
        break;
    }

    default:
        TRACE_E("Unexpected situation in CFTPWorker::HandleEvent(): unknown value for State!");
        break;
    }

#ifdef _DEBUG
    if (WorkerCritSect.RecursionCount != 1)
        TRACE_E("CFTPWorker::HandleEvent()::end: WorkerCritSect.RecursionCount=" << WorkerCritSect.RecursionCount);
    if (SocketCritSect.RecursionCount != 1)
        TRACE_E("CFTPWorker::HandleEvent()::end: SocketCritSect.RecursionCount=" << SocketCritSect.RecursionCount);
#endif

    if (sendQuitCmd)
    {
        // if we obtained the "control connection" from the panel, try to return it (instead of closing it via "QUIT")
        int ctrlUID = ControlConnectionUID;
        if (ctrlUID != -1) // we received the connection from the panel
        {
            DWORD lastIdle, lastIdle2;
            BOOL isNotBusy = SalamanderGeneral->SalamanderIsNotBusy(&lastIdle);
            if (isNotBusy || GetTickCount() - lastIdle < 2000) // high chance that Salamander reaches the "idle" state (the connection can be returned to the panel)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                BOOL isConnected;
                // since we are already inside CSocketsThread::CritSect, this call
                // is also possible from CSocket::SocketCritSect (no risk of deadlock)
                if (SocketsThread->IsSocketConnected(ctrlUID, &isConnected) &&
                    !isConnected) // the socket object of the panel's control connection still exists and has no connection to the server
                {
                    // try to wait for Salamander to become "idle" for about five seconds
                    DWORD startTime = GetTickCount();
                    while (1)
                    {
                        if (isNotBusy || startTime - lastIdle < 500 ||                                   // already idle or was idle half a second ago (it is very unlikely that e.g. a dialog is open -> the idle state should return soon)
                            SalamanderGeneral->SalamanderIsNotBusy(&lastIdle2) || lastIdle2 != lastIdle) // strong chance the connection handover happens immediately (otherwise we prefer to close it via "QUIT")
                        {
                            if (ReturningConnections.Add(ctrlUID, this))
                            {
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                CanDeleteSocket = FALSE;
                                ReturnToControlCon = TRUE;
                                SalamanderGeneral->PostMenuExtCommand(FTPCMD_RETURNCONNECTION, TRUE); // wait for "sal-idle"
                                sendQuitCmd = FALSE;                                                  // attempting to hand over the connection; "QUIT" will not be sent
                            }
                            break;
                        }
                        else
                        {
                            if (GetTickCount() - startTime > 5000)
                                break;
                            Sleep(200);
                        }
                    }
                }

                if (sendQuitCmd)
                    HANDLES(EnterCriticalSection(&WorkerCritSect)); // if problems persist, use "QUIT"
            }
        }

        if (CommandState != fwcsIdle)
            TRACE_E("Incorrect use of send-quit-command in CFTPWorker::HandleEvent(): CommandState is not fwcsIdle before sending new command!");
        int logUID = LogUID;

        if (sendQuitCmd) // we are not handing the socket from the worker back to the panel; send "QUIT"
        {
            Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGDISCONNECT), -1, TRUE);

            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH, ftpcmdQuit, &cmdLen); // cannot report an error
            sendCmd = TRUE;
        }
        else // the connection will be handed over; simulate closing the worker's socket (to end waiting for socket closure when stopping the worker)
        {
            SocketClosed = TRUE;
            ErrorDescr[0] = 0; // this is not an error (and it should no longer be displayed anywhere)
            CommandReplyTimeout = FALSE;

            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            HandleEvent(fweCmdConClosed, NULL, 0, 0); // report the socket "closure" to HandleEvent()
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            ReportWorkerMayBeClosed(); // announce the socket closure (for other waiting threads)
        }
    }
    if (sendCmd)
    {
        if (CommandState != fwcsIdle)
            TRACE_E("Incorrect use of send-command in CFTPWorker::HandleEvent(): CommandState is not fwcsIdle before sending new command!");
        CommandState = fwcsWaitForCmdReply;
        int logUID = LogUID;
        HANDLES(LeaveCriticalSection(&WorkerCritSect));

        DWORD error = NO_ERROR;
        BOOL allBytesWritten;
        if (Write(buf, cmdLen, &error, &allBytesWritten))
        {
            Logs.LogMessage(logUID, errBuf, -1); // record the sent command in the log
            // set up a new timeout timer
            int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
            if (serverTimeout < 1000)
                serverTimeout = 1000; // at least one second
            // since we are already inside CSocketsThread::CritSect, this call
            // is also possible from CSocket::SocketCritSect (no risk of deadlock)
            SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                    WORKER_TIMEOUTTIMERID, NULL); // ignore errors; at worst the user will press Stop
        }
        else
        {
            // the socket most likely closed (FD_CLOSE will arrive eventually); if another error occurred,
            // close the socket hard via CloseSocket() after 100 ms - wait to see which of the two
            // variants happens (while trying to capture an error message and log it)
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            CommandState = fwcsWaitForCmdError;
            CommandTransfersData = FALSE;
            WaitForCmdErrError = error; // store the error from Write(); if we get nothing else, log at least this error
            // ReadFTPErrorReplies();  // cannot be used (calls SkipFTPReply, which skips the reply processed in this method and causes an error after returning); if the socket has long been closed, read its errors right now (no socket events will occur; nothing would be read)
            HANDLES(LeaveCriticalSection(&WorkerCritSect));

            // since we are already inside CSocketsThread::CritSect, this call
            // is also possible from CSocket::SocketCritSect (no risk of deadlock)
            SocketsThread->AddTimer(Msg, UID, GetTickCount() + 100, // give 0.1 seconds to possibly receive bytes from the socket (it may re-post FD_CLOSE, etc.)
                                    WORKER_CMDERRORTIMERID, NULL);  // ignore errors; at worst the user will press Stop
        }
    }
    else
        HANDLES(LeaveCriticalSection(&WorkerCritSect));
    if (postActivate)
    {
        // since we are already inside CSocketsThread::CritSect, this call
        // is also possible from CSocket::SocketCritSect (no risk of deadlock)
        SocketsThread->PostSocketMessage(Msg, UID, WORKER_ACTIVATE, NULL); // ignore errors; at worst the user will press Stop
    }
    if (operStatusMaybeChanged)
        Oper->OperationStatusMaybeChanged();
    if (reportWorkerChange)
        Oper->ReportWorkerChange(workerID, FALSE); // request a redraw of the worker
}
