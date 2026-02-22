// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

void CFTPWorker::HandleEventInPreparingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                             BOOL& reportWorkerChange)
{
    if ((!DiskWorkIsUsed || event == fweWorkerShouldStop) && ShouldStop) // we should terminate the worker
    {
        if (SubState != fwssPrepQuitSent && SubState != fwssPrepWaitForDiskAfterQuitSent && !SocketClosed)
        {
            SubState = (SubState == fwssPrepWaitForDisk ? fwssPrepWaitForDiskAfterQuitSent : fwssPrepQuitSent); // so that we do not send "QUIT" more than once
            sendQuitCmd = TRUE;                                                                                 // we are supposed to finish and the connection is open -> send the server the "QUIT" command (we ignore the reply, it should lead to closing the connection and nothing else matters now)
        }
    }
    else // normal activity
    {
        // verify whether the item can be processed
        BOOL fail = FALSE;
        BOOL wait = FALSE;
        BOOL quitSent = FALSE;
        if (CurItem != NULL) // "always true"
        {
            switch (CurItem->Type)
            {
            case fqitCopyResolveLink: // copy: detect whether it is a link to a file or directory (object of class CFTPQueueItemCopyOrMove)
            case fqitMoveResolveLink: // move: detect whether it is a link to a file or directory (object of class CFTPQueueItemCopyOrMove)
                break;                // nothing to check

            case fqitCopyExploreDir:     // explore a directory or a link to a directory for copying (object of class CFTPQueueItemCopyMoveExplore)
            case fqitMoveExploreDir:     // explore a directory for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveExplore)
            case fqitMoveExploreDirLink: // explore a link to a directory for moving (deletes the directory link after completion) (object of class CFTPQueueItemCopyMoveExplore)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    if (((CFTPQueueItemCopyMoveExplore*)CurItem)->TgtDirState == TGTDIRSTATE_UNKNOWN)
                    {
                        // try to create the target directory on disk (also see whether it already exists)
                        if (DiskWorkIsUsed)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                        InitDiskWork(WORKER_DISKWORKFINISHED, fdwtCreateDir,
                                     ((CFTPQueueItemCopyMoveExplore*)CurItem)->TgtPath,
                                     ((CFTPQueueItemCopyMoveExplore*)CurItem)->TgtName,
                                     CurItem->ForceAction, FALSE, NULL, NULL, NULL, 0, NULL);
                        if (CurItem->ForceAction != fqiaNone) // the forced action stops being valid here
                            Queue->UpdateForceAction(CurItem, fqiaNone);
                        if (FTPDiskThread->AddWork(&DiskWork))
                        {
                            DiskWorkIsUsed = TRUE;
                            SubState = fwssPrepWaitForDisk; // wait for the result
                            wait = TRUE;
                        }
                        else // cannot create the directory, cannot continue processing the item
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                        }
                    }
                    // else ; // nothing to do (the target directory exists)
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // we have the result of the disk operation (creating the target directory)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // announce the worker has finished (for other waiting threads)

                        // if we have already sent QUIT, prevent sending QUIT again from the new state
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        BOOL itemChange = FALSE;
                        if (DiskWork.NewTgtName != NULL)
                        {
                            Queue->UpdateTgtName((CFTPQueueItemCopyMoveExplore*)CurItem, DiskWork.NewTgtName);
                            DiskWork.NewTgtName = NULL;
                            itemChange = TRUE;
                        }
                        if (DiskWork.State == sqisNone)
                        { // the directory was created (including autorename) or it already existed and we can use it
                            Queue->UpdateTgtDirState((CFTPQueueItemCopyMoveExplore*)CurItem, TGTDIRSTATE_READY);
                        }
                        else // an error occurred while creating the directory or the item was skipped
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            itemChange = TRUE;
                            fail = TRUE;
                        }
                        if (itemChange)
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                    }
                    else
                        wait = TRUE;
                    break;
                }
                }
                break;
            }

            case fqitUploadCopyExploreDir: // upload: explore directories for copying (object of class CFTPQueueItemCopyMoveUploadExplore)
            case fqitUploadMoveExploreDir: // upload: explore directories for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveUploadExplore)
            {
                if (CurItem->ForceAction == fqiaNone &&
                    ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtDirState == UPLOADTGTDIRSTATE_UNKNOWN &&
                    !FTPMayBeValidNameComponent(((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtName,
                                                ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath, TRUE,
                                                Oper->GetFTPServerPathType(((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath)))
                { // if the name does not follow the conventions for the given path type, the problem is "cannot create target directory"
                    switch (Oper->GetUploadCannotCreateDir())
                    {
                    case CANNOTCREATENAME_AUTORENAME:
                        break; // autorename (when it happens) must deal even with a bad name

                    case CANNOTCREATENAME_SKIP:
                    {
                        Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTDIR, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                        fail = TRUE;
                        break;
                    }

                    default: // CANNOTCREATENAME_USERPROMPT
                    {
                        Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTDIR, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                        fail = TRUE;
                        break;
                    }
                    }
                }
                // else ; // nothing to do (force actions are handled in the fwsWorking state, or the target directory already exists, or the directory name is OK)
                break;
            }

            case fqitDeleteExploreDir: // explore directories for delete (note: links to directories are deleted as a whole, the operation's purpose is fulfilled and nothing "extra" is removed) (object of class CFTPQueueItemDelExplore)
            case fqitDeleteLink:       // delete for a link (object of class CFTPQueueItemDel)
            case fqitDeleteFile:       // delete for a file (object of class CFTPQueueItemDel)
            {
                if (CurItem->Type == fqitDeleteExploreDir && ((CFTPQueueItemDelExplore*)CurItem)->IsHiddenDir || // if the directory/file/link is hidden, check what the user wants to do with it
                    (CurItem->Type == fqitDeleteLink || CurItem->Type == fqitDeleteFile) &&
                        ((CFTPQueueItemDel*)CurItem)->IsHiddenFile)
                {
                    int operationsHiddenFileDel;
                    int operationsHiddenDirDel;
                    Oper->GetParamsForDeleteOper(NULL, &operationsHiddenFileDel, &operationsHiddenDirDel);
                    if (CurItem->Type == fqitDeleteExploreDir)
                    {
                        switch (operationsHiddenDirDel)
                        {
                        case HIDDENDIRDEL_DELETEIT:
                            Queue->UpdateIsHiddenDir((CFTPQueueItemDelExplore*)CurItem, FALSE);
                            break;

                        case HIDDENDIRDEL_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_DIRISHIDDEN, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                            break;
                        }

                        default: // HIDDENDIRDEL_USERPROMPT
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_DIRISHIDDEN, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                            break;
                        }
                        }
                    }
                    else
                    {
                        switch (operationsHiddenFileDel)
                        {
                        case HIDDENFILEDEL_DELETEIT:
                            Queue->UpdateIsHiddenFile((CFTPQueueItemDel*)CurItem, FALSE);
                            break;

                        case HIDDENFILEDEL_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_FILEISHIDDEN, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                            break;
                        }

                        default: // HIDDENFILEDEL_USERPROMPT
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_FILEISHIDDEN, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                            break;
                        }
                        }
                    }
                }
            }

            case fqitDeleteDir:             // delete for a directory (object of class CFTPQueueItemDir)
            case fqitChAttrsExploreDir:     // explore directories for attribute changes (also adds an item for changing the directory attributes) (object of class CFTPQueueItemChAttrExplore)
            case fqitChAttrsResolveLink:    // attribute change: determine whether it is a link to a directory (object of class CFTPQueueItem)
            case fqitChAttrsExploreDirLink: // explore a link to a directory for attribute changes (object of class CFTPQueueItem)
                break;                      // nothing to verify

            case fqitCopyFileOrFileLink: // copying a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
            case fqitMoveFileOrFileLink: // moving a file or a link to a file (object of class CFTPQueueItemCopyOrMove)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    if (((CFTPQueueItemCopyOrMove*)CurItem)->TgtFileState != TGTFILESTATE_TRANSFERRED)
                    {
                        // try to create/open the target file on disk
                        if (DiskWorkIsUsed)
                            TRACE_E("Unexpected situation 2 in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                        CFTPDiskWorkType type = fdwtCreateFile; // TGTFILESTATE_UNKNOWN
                        switch (((CFTPQueueItemCopyOrMove*)CurItem)->TgtFileState)
                        {
                        case TGTFILESTATE_CREATED:
                            type = fdwtRetryCreatedFile;
                            break;
                        case TGTFILESTATE_RESUMED:
                            type = fdwtRetryResumedFile;
                            break;
                        }
                        InitDiskWork(WORKER_DISKWORKFINISHED, type,
                                     ((CFTPQueueItemCopyOrMove*)CurItem)->TgtPath,
                                     ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName,
                                     CurItem->ForceAction,
                                     strcmp(CurItem->Name, ((CFTPQueueItemCopyOrMove*)CurItem)->TgtName) != 0,
                                     NULL, NULL, NULL, 0, NULL);
                        if (CurItem->ForceAction != fqiaNone) // the forced action stops being valid here
                            Queue->UpdateForceAction(CurItem, fqiaNone);
                        if (FTPDiskThread->AddWork(&DiskWork))
                        {
                            DiskWorkIsUsed = TRUE;
                            SubState = fwssPrepWaitForDisk; // wait for the result
                            wait = TRUE;
                        }
                        else // cannot create/open the file, cannot continue processing the item
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                        }
                    }
                    // else ; // nothing to do (the file is already downloaded)
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // we have the result of the disk operation (creating the target file)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // announce the worker has finished (for other waiting threads)

                        // if we have already sent QUIT, prevent sending QUIT again from the new state
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        BOOL itemChange = FALSE;
                        if (DiskWork.NewTgtName != NULL)
                        {
                            Queue->UpdateTgtName((CFTPQueueItemCopyOrMove*)CurItem, DiskWork.NewTgtName);
                            DiskWork.NewTgtName = NULL;
                            itemChange = TRUE;
                        }
                        if (DiskWork.State == sqisNone)
                        { // the file was created or opened successfully
                            if (OpenedFile != NULL)
                                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInPreparingState(): OpenedFile is not NULL!");
                            OpenedFile = DiskWork.OpenedFile;
                            DiskWork.OpenedFile = NULL;
                            OpenedFileSize = DiskWork.FileSize;
                            OpenedFileOriginalSize = DiskWork.FileSize;
                            OpenedFileCurOffset.Set(0, 0);
                            OpenedFileResumedAtOffset.Set(0, 0);
                            ResumingOpenedFile = FALSE;
                            CanDeleteEmptyFile = DiskWork.CanDeleteEmptyFile;
                            Queue->UpdateTgtFileState((CFTPQueueItemCopyOrMove*)CurItem,
                                                      DiskWork.CanOverwrite ? TGTFILESTATE_CREATED : TGTFILESTATE_RESUMED);
                        }
                        else // an error occurred while creating/opening the file or the item was skipped
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            itemChange = TRUE;
                            fail = TRUE;
                        }
                        if (itemChange)
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                    }
                    else
                        wait = TRUE;
                    break;
                }
                }
                break;
            }

            case fqitUploadCopyFile: // upload: copying files (object of class CFTPQueueItemCopyOrMoveUpload)
            case fqitUploadMoveFile: // upload: moving files (object of class CFTPQueueItemCopyOrMoveUpload)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    if (CurItem->ForceAction != fqiaUseAutorename && CurItem->ForceAction != fqiaUploadForceAutorename &&
                        CurItem->ForceAction != fqiaUploadContinueAutorename &&
                        ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtFileState == UPLOADTGTFILESTATE_UNKNOWN &&
                        !FTPMayBeValidNameComponent(((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtName,
                                                    ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath, FALSE,
                                                    Oper->GetFTPServerPathType(((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath)))
                    { // if the name does not follow the conventions for the given path type, the problem is "cannot create target file"
                        switch (Oper->GetUploadCannotCreateFile())
                        {
                        case CANNOTCREATENAME_AUTORENAME:
                            break; // autorename (when it happens) must deal even with a bad name

                        case CANNOTCREATENAME_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTFILE, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                            break;
                        }

                        default: // CANNOTCREATENAME_USERPROMPT
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTFILE, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                            break;
                        }
                        }
                    }
                    if (!fail && ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtFileState != UPLOADTGTFILESTATE_TRANSFERRED)
                    { // open the source file on disk for reading
                        if (DiskWorkIsUsed)
                            TRACE_E("Unexpected situation 4 in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                        InitDiskWork(WORKER_DISKWORKFINISHED, fdwtOpenFileForReading, CurItem->Path, CurItem->Name,
                                     fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
                        if (FTPDiskThread->AddWork(&DiskWork))
                        {
                            DiskWorkIsUsed = TRUE;
                            SubState = fwssPrepWaitForDisk; // wait for the result
                            wait = TRUE;
                        }
                        else // cannot create/open the file, cannot continue processing the item
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                        }
                    }
                    // else ; // nothing to do (the upload has already failed or the file is already uploaded)
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // we have the result of the disk operation (opening the source file)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // announce the worker has finished (for other waiting threads)

                        // if we have already sent QUIT, prevent sending QUIT again from the new state
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        if (DiskWork.State == sqisNone)
                        { // the file was opened successfully
                            if (OpenedInFile != NULL)
                                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInPreparingState(): OpenedInFile is not NULL!");
                            OpenedInFile = DiskWork.OpenedFile;
                            DiskWork.OpenedFile = NULL;
                            OpenedInFileSize = DiskWork.FileSize;
                            OpenedInFileCurOffset.Set(0, 0);
                            OpenedInFileNumberOfEOLs.Set(0, 0);
                            OpenedInFileSizeWithCRLF_EOLs.Set(0, 0);
                            FileOnServerResumedAtOffset.Set(0, 0);
                            ResumingFileOnServer = FALSE;
                        }
                        else // an error occurred while opening the file
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                        }
                    }
                    else
                        wait = TRUE;
                    break;
                }
                }
                break;
            }

            case fqitUploadMoveDeleteDir: // upload: delete a directory after its contents were moved (object of class CFTPQueueItemDir)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    // try to delete an empty source directory from disk
                    if (DiskWorkIsUsed)
                        TRACE_E("Unexpected situation 3 in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                    InitDiskWork(WORKER_DISKWORKFINISHED, fdwtDeleteDir, CurItem->Path, CurItem->Name,
                                 fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
                    if (FTPDiskThread->AddWork(&DiskWork))
                    {
                        DiskWorkIsUsed = TRUE;
                        SubState = fwssPrepWaitForDisk; // wait for the result
                        wait = TRUE;
                    }
                    else // cannot delete the directory, store the error in the item
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                        fail = TRUE;
                    }
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // we have the result of the disk operation (deleting the empty source directory)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // announce the worker has finished (for other waiting threads)

                        // if we have already sent QUIT, prevent sending QUIT again from the new state
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        if (DiskWork.State == sqisNone)
                        { // the directory was deleted successfully
                            Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item

                            // go find more work
                            CurItem = NULL;
                            State = fwsLookingForWork; // no need to call Oper->OperationStatusMaybeChanged(), the operation state does not change (it is not paused and will not be after this change)
                            if (quitSent)
                                SubState = fwssLookFWQuitSent; // hand over to fwsLookingForWork that QUIT was already sent
                            else
                                SubState = fwssNone;
                            postActivate = TRUE; // post an activation for the next worker state
                            reportWorkerChange = TRUE;
                            wait = TRUE; // skip state changes below this switch
                        }
                        else // an error occurred while deleting the directory
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                            fail = TRUE;
                        }
                    }
                    else
                        wait = TRUE;
                    break;
                }
                }
                break;
            }

            case fqitMoveDeleteDir:     // deleting a directory after its contents were moved (object of class CFTPQueueItemDir)
            case fqitMoveDeleteDirLink: // deleting a link to a directory after its contents were moved (object of class CFTPQueueItemDir)
                break;                  // nothing to verify

            case fqitChAttrsFile: // change file attributes (note: attributes cannot be changed on links) (object of class CFTPQueueItemChAttr)
            case fqitChAttrsDir:  // change directory attributes (object of class CFTPQueueItemChAttrDir)
            {
                if (CurItem->Type == fqitChAttrsFile && ((CFTPQueueItemChAttr*)CurItem)->AttrErr || // respond to the error "an unknown attribute should be preserved, which we cannot do"
                    CurItem->Type == fqitChAttrsDir && ((CFTPQueueItemChAttrDir*)CurItem)->AttrErr)
                {
                    switch (Oper->GetUnknownAttrs())
                    {
                    case UNKNOWNATTRS_IGNORE:
                    {
                        if (CurItem->Type == fqitChAttrsFile)
                            Queue->UpdateAttrErr((CFTPQueueItemChAttr*)CurItem, FALSE);
                        else // fqitChAttrsDir
                            Queue->UpdateAttrErr((CFTPQueueItemChAttrDir*)CurItem, FALSE);
                        break;
                    }

                    case UNKNOWNATTRS_SKIP:
                    {
                        Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UNKNOWNATTRS, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                        fail = TRUE;
                        break;
                    }

                    default: // UNKNOWNATTRS_USERPROMPT
                    {
                        Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UNKNOWNATTRS, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // request a redraw of the item
                        fail = TRUE;
                        break;
                    }
                    }
                }
                break;
            }

            default:
            {
                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInPreparingState(): unknown active operation item type!");
                break;
            }
            }
        }
        else
        {
            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInPreparingState(): missing active operation item!");
            fail = TRUE;
        }

        if (!wait)
        {
            if (fail) // the item cannot be performed, it already has an error set, go find another item
            {
                CurItem = NULL;
                State = fwsLookingForWork; // no need to call Oper->OperationStatusMaybeChanged(), the operation state does not change (it is not paused and will not be after this change)
                if (quitSent)
                    SubState = fwssLookFWQuitSent; // hand over to fwsLookingForWork that QUIT was already sent
                else
                    SubState = fwssNone;
            }
            else // everything OK so far, continue
            {
                if (SocketClosed)
                {
                    State = fwsConnecting; // no need to call Oper->OperationStatusMaybeChanged(), the operation state does not change (it is not paused and will not be after this change)
                    SubState = fwssNone;   // the connection is not open, therefore we can ignore quitSent (there is nowhere to send QUIT)
                }
                else
                {
                    State = fwsWorking; // no need to call Oper->OperationStatusMaybeChanged(), the operation state does not change (it is not paused and will not be after this change)
                    if (UploadDirGetTgtPathListing)
                        TRACE_E("CFTPWorker::HandleEventInPreparingState(): UploadDirGetTgtPathListing==TRUE!");
                    StatusType = wstNone;
                    if (quitSent)
                        SubState = fwssWorkStopped; // hand over to fwsWorking that QUIT was already sent
                    else
                        SubState = fwssNone;
                }
            }
            postActivate = TRUE; // post an activation for the next worker state
            reportWorkerChange = TRUE;
        }
    }
}

void CFTPWorker::HandleEventInConnectingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                              BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                              int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                              int replyCode, BOOL& operStatusMaybeChanged)
{
    BOOL run;
    do
    {
        run = FALSE;              // when changed to TRUE, the loop runs again immediately
        BOOL closeSocket = FALSE; // TRUE = the worker's socket should be closed
        if (ShouldStop)           // we should terminate the worker (everything inside the loop due to 'closeSocket')
        {
            switch (SubState)
            {
                // case fwssNone:  // nothing needs to be done

                // case fwssConConnect:           // cannot occur (only an intermediate state)
                // case fwssConReconnect:         // cannot occur (only an intermediate state)
                // case fwssConSendNextScriptCmd: // cannot occur (only an intermediate state)
                // case fwssConSendInitCmds:      // cannot occur (only an intermediate state)
                // case fwssConSendSyst:          // cannot occur (only an intermediate state)

            case fwssConWaitingForIP: // delete the WORKER_CONTIMEOUTTIMID timer
            {
                // because we are already inside CSocketsThread::CritSect, this call is also possible
                // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                break;
            }

            case fwssConWaitForConRes: // close the connection and delete the WORKER_CONTIMEOUTTIMID timer
            {
                // because we are already inside CSocketsThread::CritSect, this call is also possible
                // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                closeSocket = TRUE;
                break;
            }

            case fwssConWaitForPrompt:       // close the connection and delete the WORKER_TIMEOUTTIMERID timer
            case fwssConWaitForScriptCmdRes: // close the connection and delete the WORKER_TIMEOUTTIMERID timer (it might no longer exist, but that does not matter)
            case fwssConWaitForInitCmdRes:   // close the connection and delete the WORKER_TIMEOUTTIMERID timer (it might no longer exist, but that does not matter)
            case fwssConWaitForSystRes:      // close the connection and delete the WORKER_TIMEOUTTIMERID timer (it might no longer exist, but that does not matter)
            {
                // because we are already inside CSocketsThread::CritSect, this call is also possible
                // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_TIMEOUTTIMERID);
                closeSocket = TRUE;
                break;
            }
            }
        }
        else // normal activity
        {
            switch (SubState)
            {
            case fwssNone: // determine whether we need to obtain the IP address
            {
                if (ConnectAttemptNumber == 0) // for this worker it is the first attempt to establish a connection
                {
                    ConnectAttemptNumber = 1;
                }
                else
                {
                    if (ConnectAttemptNumber == 1) // first reconnect attempt (additional attempts are handled from fwssConReconnect)
                    {
                        if (ConnectAttemptNumber + 1 > Config.GetConnectRetries() + 1)
                        { // the second attempt to establish a connection (the first attempt = the connection that broke) is
                            // immediate (without waiting after the failure), we only wait between individual attempts
                            // to establish the connection
                            State = fwsConnectionError; // ATTENTION: assumes ErrorDescr is set
                            operStatusMaybeChanged = TRUE;
                            ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                            SubState = fwssNone;
                            postActivate = TRUE; // post an activation for the next worker state
                            reportWorkerChange = TRUE;
                            break; // end of executing the fwsConnecting state
                        }
                        else
                            ConnectAttemptNumber++;
                    }
                }

                // reset caches
                HaveWorkingPath = FALSE;
                CurrentTransferMode = ctrmUnknown;

                DWORD serverIP;
                if (Oper->GetServerAddress(&serverIP, host, HOST_MAX_SIZE)) // we have the IP
                {
                    SubState = fwssConConnect;
                    run = TRUE;
                }
                else // we only have a host name
                {
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    BOOL getHostByAddressRes = GetHostByAddress(host, ++IPRequestUID);
                    RefreshCopiesOfUIDAndMsg(); // refresh the UID+Msg copies (they changed)
                    if (!getHostByAddressRes)
                    { // no chance of success -> report an error
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKERGETIPERROR),
                                    GetWorkerErrorTxt(NO_ERROR, errBuf, 50 + FTP_MAX_PATH));
                        CorrectErrorDescr();
                        SubState = fwssConReconnect;
                        run = TRUE;
                    }
                    else // wait until the server IP address arrives as fwseIPReceived, then obtain the IP from the operation again
                    {
                        // set a new timeout timer
                        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                        if (serverTimeout < 1000)
                            serverTimeout = 1000; // at least one second
                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                        SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                                WORKER_CONTIMEOUTTIMID, NULL); // ignore the error, at worst the user hits Stop

                        SubState = fwssConWaitingForIP;
                        // run = TRUE;  // pointless (no event has occurred yet)
                    }
                }
                break;
            }

            case fwssConWaitingForIP: // waiting for the IP address (translation from the host name)
            {
                switch (event)
                {
                case fweIPReceived:
                {
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                    SubState = fwssConConnect;
                    run = TRUE;
                    break;
                }

                case fweIPRecFailure:
                {
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }

                case fweConTimeout:
                {
                    lstrcpyn(ErrorDescr, LoadStr(IDS_GETIPTIMEOUT), FTPWORKER_ERRDESCR_BUFSIZE);
                    CorrectErrorDescr();
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConConnect: // perform Connect()
            {
                DWORD serverIP;
                unsigned short port;
                CFTPProxyServerType proxyType;
                DWORD hostIP;
                unsigned short hostPort;
                char proxyUser[USER_MAX_SIZE];
                char proxyPassword[PASSWORD_MAX_SIZE];
                Oper->GetConnectInfo(&serverIP, &port, host, &proxyType, &hostIP, &hostPort, proxyUser, proxyPassword);
                if (ConnectAttemptNumber == 1) // connect
                {
                    Oper->GetConnectLogMsg(FALSE, buf, 700 + FTP_MAX_PATH, 0, NULL);
                    Logs.LogMessage(LogUID, buf, -1, TRUE);
                }
                else // reconnect
                {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, errBuf, 50) == 0)
                        sprintf(errBuf, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                    strcat(errBuf, " - ");
                    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, errBuf + strlen(errBuf), 50) == 0)
                        sprintf(errBuf + strlen(errBuf), "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
                    Oper->GetConnectLogMsg(TRUE, buf, 700 + FTP_MAX_PATH, ConnectAttemptNumber, errBuf);
                    Logs.LogMessage(LogUID, buf, -1);
                }

                ResetBuffersAndEvents(); // clear the buffers (discard old data) and initialize variables related to the connection

                DWORD error;
                // because we are already inside CSocketsThread::CritSect, this call is also possible
                // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                BOOL conRes = ConnectWithProxy(serverIP, port, proxyType, &error, host, hostPort,
                                               proxyUser, proxyPassword, hostIP);
                RefreshCopiesOfUIDAndMsg(); // refresh the UID+Msg copies (they changed)
                Logs.SetIsConnected(LogUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg();
                if (conRes)
                {
                    SocketClosed = FALSE; // the socket is open again

                    // set a new timeout timer
                    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                    if (serverTimeout < 1000)
                        serverTimeout = 1000; // at least one second
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                            WORKER_CONTIMEOUTTIMID, NULL); // ignore the error, at worst the user hits Stop

                    SubState = fwssConWaitForConRes;
                    // run = TRUE;  // pointless (no event has occurred yet)
                }
                else
                {
                    _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKEROPENCONERR),
                                GetWorkerErrorTxt(error, errBuf, 50 + FTP_MAX_PATH));
                    CorrectErrorDescr();
                    SubState = fwssConReconnect;
                    run = TRUE;
                }
                break;
            }

            case fwssConWaitForConRes: // wait for the result of Connect()
            {
                switch (event)
                {
                case fweConnected:
                {
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);

                    // set the worker state so it starts waiting for a command reply (even if we did not
                    // send any command, we wait for the server response) - set the timeout for receiving a command reply
                    CommandState = fwcsWaitForLoginPrompt;
                    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                    if (serverTimeout < 1000)
                        serverTimeout = 1000; // at least one second
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                            WORKER_TIMEOUTTIMERID, NULL); // ignore the error, at worst the user hits Stop

                    SubState = fwssConWaitForPrompt;
                    // run = TRUE; // pointless (no event has occurred yet)
                    break;
                }

                case fweConnectFailure:
                {
                    // because we are already inside CSocketsThread::CritSect, this call is also possible
                    // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                    SubState = fwssConReconnect;
                    run = TRUE;
                    closeSocket = TRUE;
                    break;
                }

                case fweConTimeout:
                {
                    // because we are already inside CSocket::SocketCritSect, this call is also possible
                    // from within CFTPWorker::WorkerCritSect (no risk of dead-lock)
                    if (!GetProxyTimeoutDescr(ErrorDescr, FTPWORKER_ERRDESCR_BUFSIZE))
                        lstrcpyn(ErrorDescr, LoadStr(IDS_OPENCONTIMEOUT), FTPWORKER_ERRDESCR_BUFSIZE);
                    CorrectErrorDescr();
                    SubState = fwssConReconnect;
                    run = TRUE;
                    closeSocket = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConWaitForPrompt: // waiting for the login prompt from the server
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the Log)
                case fweCmdReplyReceived:
                {
                    if (replyCode != -1)
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&
                            FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION) // e.g. 220 - Service ready for new user
                        {
                            Oper->SetServerFirstReply(reply, replySize); // store the server's first reply (source of information about the server version)

                            ProxyScriptExecPoint = NULL; // start sending from the first login script command again
                            ProxyScriptLastCmdReply = -1;
                            SubState = Oper->GetEncryptControlConnection() ? fwssConSendAUTH : fwssConSendNextScriptCmd; // we will send commands from the login script
                            run = TRUE;
                        }
                        else
                        {
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) // e.g. 421 Service not available, closing control connection
                            {
                                lstrcpyn(ErrorDescr, CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize),
                                         FTPWORKER_ERRDESCR_BUFSIZE);
                                CorrectErrorDescr();
                                closeSocket = TRUE; // close the connection (no point in continuing)

                                SubState = fwssConReconnect;
                                run = TRUE;
                            }
                            else // unexpected response, ignore it
                            {
                                TRACE_E("Unexpected reply: " << CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                            }
                        }
                    }
                    else // not an FTP server
                    {
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_NOTFTPSERVERERROR),
                                    CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                        CorrectErrorDescr();
                        closeSocket = TRUE; // close the connection (no point in continuing)

                        State = fwsConnectionError;
                        operStatusMaybeChanged = TRUE;
                        ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                        SubState = fwssNone;
                        postActivate = TRUE; // post an activation for the next worker state
                        reportWorkerChange = TRUE;
                    }
                    break;
                }

                case fweCmdConClosed:
                {
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConSendAUTH: // Initiate TLS
                strcpy(buf, "AUTH TLS\r\n");
                cmdLen = (int)strlen(buf);
                strcpy(errBuf, buf); // For logging purposes
                sendCmd = TRUE;
                if (pCertificate)
                    pCertificate->Release();
                pCertificate = Oper->GetCertificate();
                SubState = fwssConWaitForAUTHCmdRes;
                break;

            case fwssConSendPBSZ: // After AUTH TLS, but only if encrypting also data
                strcpy(buf, "PBSZ 0\r\n");
                cmdLen = (int)strlen(buf);
                strcpy(errBuf, buf); // For logging purposes
                sendCmd = TRUE;
                SubState = fwssConWaitForPBSZCmdRes;
                break;

            case fwssConSendPROT: // After PBSZ
                strcpy(buf, "PROT P\r\n");
                cmdLen = (int)strlen(buf);
                strcpy(errBuf, buf); // For logging purposes
                sendCmd = TRUE;
                SubState = fwssConWaitForPROTCmdRes;
                break;

            case fwssConSendMODEZ: // Init compression
                strcpy(buf, "MODE Z\r\n");
                cmdLen = (int)strlen(buf);
                strcpy(errBuf, buf); // For logging purposes
                sendCmd = TRUE;
                SubState = fwssConWaitForMODEZCmdRes;
                break;

            case fwssConSendNextScriptCmd: // send the next login script command
            {
                BOOL fail = FALSE;
                char errDescrBuf[300];
                BOOL needUserInput;
                if (Oper->PrepareNextScriptCmd(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH, &cmdLen,
                                               &ProxyScriptExecPoint, ProxyScriptLastCmdReply,
                                               errDescrBuf, &needUserInput) &&
                    !needUserInput)
                {
                    if (buf[0] == 0) // end of the script
                    {
                        if (ProxyScriptLastCmdReply == -1) // the script does not contain any command that would be sent to the server - e.g. commands were skipped because they contain optional variables
                        {
                            lstrcpyn(ErrorDescr, LoadStr(IDS_INCOMPLETEPRXSCR2), FTPWORKER_ERRDESCR_BUFSIZE);
                            CorrectErrorDescr();
                            fail = TRUE;
                        }
                        else
                        {
                            if (FTP_DIGIT_1(ProxyScriptLastCmdReply) == FTP_D1_SUCCESS) // e.g. 230 User logged in, proceed
                            {
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGLOGINSUCCESS), -1, TRUE);
                                NextInitCmd = 0;                                                             // send the first init-ftp command
                                SubState = Oper->GetCompressData() ? fwssConSendMODEZ : fwssConSendInitCmds; // we are logged in, now send the init-ftp commands
                                run = TRUE;
                            }
                            else // FTP_DIGIT_1(ProxyScriptLastCmdReply) == FTP_D1_PARTIALSUCCESS  // e.g. 331 User name okay, need password
                            {    // assumes we got here from fwssConWaitForScriptCmdRes and reply+replySize is still valid
                                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_INCOMPLETEPRXSCR3),
                                            CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                                CorrectErrorDescr();
                                fail = TRUE;
                            }
                        }
                    }
                    else // there is another command to send
                    {
                        sendCmd = TRUE;
                        SubState = fwssConWaitForScriptCmdRes;
                        // run = TRUE; // pointless (no event has occurred yet)
                    }
                }
                else // script error or missing variable value
                {
                    if (needUserInput)
                        lstrcpyn(ErrorDescr, errDescrBuf, FTPWORKER_ERRDESCR_BUFSIZE);
                    else
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_ERRINPROXYSCRIPT), errDescrBuf);
                    CorrectErrorDescr();
                    fail = TRUE;
                }
                if (fail)
                {
                    closeSocket = TRUE; // close the connection (no point in continuing)

                    State = fwsConnectionError; // NOTE: ErrorDescr is set above (see "fail = TRUE")
                    operStatusMaybeChanged = TRUE;
                    ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                    SubState = fwssNone;
                    postActivate = TRUE; // post an activation for the next worker state
                    reportWorkerChange = TRUE;
                }
                break;
            }

            case fwssConWaitForAUTHCmdRes:
            case fwssConWaitForPBSZCmdRes:
            case fwssConWaitForPROTCmdRes:
            {
                switch (event)
                {
                case fweCmdReplyReceived:
                {
                    BOOL failed = FALSE;
                    BOOL retryLoginWithoutAsking = FALSE;
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    {
                        switch (SubState)
                        {
                        case fwssConWaitForAUTHCmdRes:
                        {
                            int errID;
                            if (InitSSL(LogUID, &errID))
                            {
                                int err;
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                CCertificate* unverifiedCert;
                                BOOL ret = EncryptSocket(LogUID, &err, &unverifiedCert, &errID, errBuf,
                                                         50 + FTP_MAX_PATH,
                                                         NULL /* for the control connection this is always NULL */);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                if (ret)
                                {
                                    if (unverifiedCert != NULL) // close the connection and retry only after the user learns about the untrusted certificate and accepts it or ensures it becomes trusted (in the Solve Error dialog)
                                    {
                                        if (UnverifiedCertificate != NULL)
                                            UnverifiedCertificate->Release();
                                        UnverifiedCertificate = unverifiedCert;
                                        lstrcpyn(ErrorDescr, LoadStr(IDS_SSLNEWUNVERIFIEDCERT), FTPWORKER_ERRDESCR_BUFSIZE);
                                        failed = TRUE;
                                    }
                                    else
                                    {
                                        SubState = Oper->GetEncryptDataConnection() ? fwssConSendPBSZ : fwssConSendNextScriptCmd;
                                        ProxyScriptLastCmdReply = replyCode;
                                        run = TRUE;
                                    }
                                }
                                else
                                {
                                    if (errBuf[0] == 0)
                                        lstrcpyn(ErrorDescr, LoadStr(errID), FTPWORKER_ERRDESCR_BUFSIZE);
                                    else
                                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(errID), errBuf);
                                    failed = TRUE;
                                    retryLoginWithoutAsking = err == SSLCONERR_CANRETRY;
                                }
                            }
                            else
                            {
                                lstrcpyn(ErrorDescr, LoadStr(errID), FTPWORKER_ERRDESCR_BUFSIZE);
                                failed = TRUE;
                            }
                            break;
                        }
                        case fwssConWaitForPBSZCmdRes:
                            SubState = fwssConSendPROT;
                            ProxyScriptLastCmdReply = replyCode;
                            run = TRUE;
                            break;
                        case fwssConWaitForPROTCmdRes:
                            SubState = fwssConSendNextScriptCmd;
                            ProxyScriptLastCmdReply = replyCode;
                            run = TRUE;
                            break;
                        }
                    }
                    else
                    {
                        lstrcpyn(ErrorDescr, LoadStr(SubState == fwssConWaitForAUTHCmdRes ? IDS_SSL_ERR_CONTRENCUNSUP : IDS_SSL_ERR_DATAENCUNSUP), FTPWORKER_ERRDESCR_BUFSIZE);
                        failed = TRUE;
                    }
                    if (failed)
                    {
                        CorrectErrorDescr();

                        closeSocket = TRUE; // close the connection (no point in continuing)

                        if (retryLoginWithoutAsking) // try again
                        {
                            SubState = fwssConReconnect;
                            run = TRUE;
                        }
                        else // report the error to the user
                        {
                            State = fwsConnectionError;
                            operStatusMaybeChanged = TRUE;
                            ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                            SubState = fwssNone;
                            postActivate = TRUE; // post an activation for the next worker state
                            reportWorkerChange = TRUE;
                        }
                    }
                    break;
                }
                case fweCmdConClosed:
                {
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConWaitForMODEZCmdRes:
            {
                switch (event)
                {
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    {
                        SubState = fwssConSendInitCmds;
                        ProxyScriptLastCmdReply = replyCode;
                        run = TRUE;
                    }
                    else
                    {
                        // Server does not support compression -> swallow the error, disable compression and go on
                        // NOTE: Probably cannot happen because CompresData was set to FALSE in main connection
                        replyCode = 200; // Emulate Full success
                        Oper->SetCompressData(FALSE);
                        Logs.LogMessage(LogUID, LoadStr(IDS_MODEZ_LOG_UNSUPBYSERVER), -1);
                    }
                    break;
                }
                case fweCmdConClosed:
                {
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConWaitForScriptCmdRes: // waiting for the result of a login script command
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the Log)
                case fweCmdReplyReceived:
                {
                    if (replyCode != -1)
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||      // e.g. 230 User logged in, proceed
                            FTP_DIGIT_1(replyCode) == FTP_D1_PARTIALSUCCESS) // e.g. 331 User name okay, need password
                        {
                            SubState = fwssConSendNextScriptCmd;
                            ProxyScriptLastCmdReply = replyCode;
                            run = TRUE;
                        }
                        else
                        {
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // e.g. 421 Service not available (too many users), closing control connection
                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // e.g. 530 Not logged in (invalid password)
                            {
                                BOOL retryLoginWithoutAsking;
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR)
                                { // convenient handling of the "too many users" error - no questions, immediately retry
                                    // this may be a problem: this code is accompanied by a message that requires changing the user/password
                                    retryLoginWithoutAsking = TRUE;
                                }
                                else
                                    retryLoginWithoutAsking = Oper->GetRetryLoginWithoutAsking();

                                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKERLOGINERR),
                                            CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                                CorrectErrorDescr();
                                closeSocket = TRUE; // close the connection (no point in continuing)

                                if (retryLoginWithoutAsking) // try again
                                {
                                    SubState = fwssConReconnect;
                                    run = TRUE;
                                }
                                else // report the error to the user and wait until they change the password/account
                                {
                                    State = fwsConnectionError;
                                    operStatusMaybeChanged = TRUE;
                                    ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                                    SubState = fwssNone;
                                    postActivate = TRUE; // post an activation for the next worker state
                                    reportWorkerChange = TRUE;
                                }
                            }
                            else // unexpected response, ignore it
                            {
                                TRACE_E("Unexpected reply: " << CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                            }
                        }
                    }
                    else // not an FTP server
                    {
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_NOTFTPSERVERERROR),
                                    CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                        CorrectErrorDescr();
                        closeSocket = TRUE; // close the connection (no point in continuing)

                        State = fwsConnectionError;
                        operStatusMaybeChanged = TRUE;
                        ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                        SubState = fwssNone;
                        postActivate = TRUE; // post an activation for the next worker state
                        reportWorkerChange = TRUE;
                    }
                    break;
                }

                case fweCmdConClosed:
                {
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConSendInitCmds: // send initialization commands (user-defined, see CFTPOperation::InitFTPCommands)
            {
                Oper->GetInitFTPCommands(buf, 200 + FTP_MAX_PATH);
                if (buf[0] != 0)
                {
                    char* next = buf;
                    char* s;
                    int i = 0;
                    while (GetToken(&s, &next))
                    {
                        if (*s != 0 && *s <= ' ')
                            s++;     // remove only the first space (so commands can still start with spaces)
                        if (*s != 0) // if there is anything, send it to the server
                        {
                            if (i++ == NextInitCmd)
                            {
                                NextInitCmd++; // take the next command next time (very inefficient, but only a few commands are expected here)
                                _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, "%s\r\n", s);
                                strcpy(buf, errBuf);
                                cmdLen = (int)strlen(buf);
                                sendCmd = TRUE;
                                break;
                            }
                        }
                    }
                }
                if (sendCmd) // sending another init-ftp command
                {
                    SubState = fwssConWaitForInitCmdRes;
                    // run = TRUE; // pointless (no event has occurred yet)
                }
                else // all init-ftp commands have been sent (or none exist)
                {
                    SubState = fwssConSendSyst; // determine the server system
                    run = TRUE;
                }
                break;
            }

            case fwssConWaitForInitCmdRes: // waiting for the result of the initialization command
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the Log)
                case fweCmdReplyReceived:
                {
                    SubState = fwssConSendInitCmds; // send another init-ftp command (we do not care about the server replies)
                    run = TRUE;
                    break;
                }

                case fweCmdConClosed:
                {
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConSendSyst: // determine the server system (send the "SYST" command)
            {
                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH, ftpcmdSystem, &cmdLen); // cannot report an error
                sendCmd = TRUE;
                SubState = fwssConWaitForSystRes;
                // run = TRUE; // pointless (no event has occurred yet)
                break;
            }

            case fwssConWaitForSystRes: // waiting for the server system (result of the "SYST" command)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // ignore "1xx" replies (they are only written to the Log)
                case fweCmdReplyReceived:
                {
                    Oper->SetServerSystem(reply, replySize); // store the server system (source of information about the server version)

                    // the connection is established, start working
                    State = fwsWorking; // no need to call Oper->OperationStatusMaybeChanged(), the operation state does not change (it is not paused and will not be after this change)
                    if (UploadDirGetTgtPathListing)
                        TRACE_E("CFTPWorker::HandleEventInPreparingState(): UploadDirGetTgtPathListing==TRUE!");
                    SubState = fwssNone;
                    StatusType = wstNone;
                    postActivate = TRUE;      // post an activation for the next worker state
                    ConnectAttemptNumber = 1; // the connection is established, reset to one so the next reconnect attempt is ready again
                    ErrorDescr[0] = 0;        // start collecting error messages again
                    reportWorkerChange = TRUE;
                    break;
                }

                case fweCmdConClosed:
                {
                    SubState = fwssConReconnect;
                    run = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssConReconnect: // decide whether to perform a reconnect or report a worker error
            {
                if (ConnectAttemptNumber + 1 > Config.GetConnectRetries() + 1)
                {
                    State = fwsConnectionError; // NOTE: assumes ErrorDescr is set
                    operStatusMaybeChanged = TRUE;
                    ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                    SubState = fwssNone;
                    postActivate = TRUE; // post an activation for the next worker state
                    reportWorkerChange = TRUE;
                }
                else
                {
                    // try to find a "sleeping" worker with an open connection to hand over the work to (instead of reconnecting unnecessarily)
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    BOOL workMoved = Oper->GiveWorkToSleepingConWorker(this);
                    HANDLES(EnterCriticalSection(&WorkerCritSect));

                    if (!workMoved)
                    {
                        // wait for a reconnect
                        ConnectAttemptNumber++;

                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                        SocketsThread->DeleteTimer(UID, WORKER_RECONTIMEOUTTIMID);

                        // start a timer for the next connect attempt
                        int delayBetweenConRetries = Config.GetDelayBetweenConRetries() * 1000;
                        // because we are already inside CSocketsThread::CritSect, this call is also possible
                        // from within CSocket::SocketCritSect and CFTPWorker::WorkerCritSect (no risk of dead-lock)
                        SocketsThread->AddTimer(Msg, UID, GetTickCount() + delayBetweenConRetries,
                                                WORKER_RECONTIMEOUTTIMID, NULL); // ignore the error, at worst the user hits Stop

                        State = fwsWaitingForReconnect; // NOTE: assumes ErrorDescr is set; no need to call Oper->OperationStatusMaybeChanged(), the operation state does not change (it is not paused and will not be after this change)
                        SubState = fwssNone;
                        // postActivate = TRUE;  // no reason, waiting for the timeout
                        reportWorkerChange = TRUE;
                    }
                }
                break;
            }
            }
        }
        if (closeSocket)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));

            // because we are already inside CSocketsThread::CritSect, this call is also possible
            // from within CSocket::SocketCritSect (no risk of dead-lock)
            ForceClose();

            HANDLES(EnterCriticalSection(&WorkerCritSect));
        }
    } while (run);
}

BOOL CFTPWorker::ParseListingToFTPQueue(TIndirectArray<CFTPQueueItem>* ftpQueueItems,
                                        const char* allocatedListing, int allocatedListingLen,
                                        CServerType* serverType, BOOL* lowMem, BOOL isVMS, BOOL isAS400,
                                        int transferMode, CQuadWord* totalSize,
                                        BOOL* sizeInBytes, BOOL selFiles,
                                        BOOL selDirs, BOOL includeSubdirs, DWORD attrAndMask,
                                        DWORD attrOrMask, int operationsUnknownAttrs,
                                        int operationsHiddenFileDel, int operationsHiddenDirDel)
{
    BOOL ret = FALSE;
    *lowMem = FALSE;
    ftpQueueItems->DestroyMembers();
    totalSize->Set(0, 0);
    *sizeInBytes = TRUE;

    // compute the 'validDataMask'
    DWORD validDataMask = VALID_DATA_HIDDEN | VALID_DATA_ISLINK; // Name + NameLen + Hidden + IsLink
    int i;
    for (i = 0; i < serverType->Columns.Count; i++)
    {
        switch (serverType->Columns[i]->Type)
        {
        case stctExt:
            validDataMask |= VALID_DATA_EXTENSION;
            break;
        case stctSize:
            validDataMask |= VALID_DATA_SIZE;
            break;
        case stctDate:
            validDataMask |= VALID_DATA_DATE;
            break;
        case stctTime:
            validDataMask |= VALID_DATA_TIME;
            break;
        case stctType:
            validDataMask |= VALID_DATA_TYPE;
            break;
        }
    }

    BOOL err = FALSE;
    CFTPListingPluginDataInterface* dataIface = new CFTPListingPluginDataInterface(&(serverType->Columns), FALSE,
                                                                                   validDataMask, isVMS);
    if (dataIface != NULL)
    {
        DWORD* emptyCol = new DWORD[serverType->Columns.Count]; // helper preallocated array for GetNextItemFromListing
        if (dataIface->IsGood() && emptyCol != NULL)
        {
            validDataMask |= dataIface->GetPLValidDataMask();
            CFTPParser* parser = serverType->CompiledParser;
            if (parser == NULL)
            {
                parser = CompileParsingRules(HandleNULLStr(serverType->RulesForParsing), &(serverType->Columns),
                                             NULL, NULL, NULL);
                serverType->CompiledParser = parser; // we will not deallocate 'parser', it already lives in 'serverType'
            }
            if (parser != NULL)
            {
                CFileData file;
                const char* listing = allocatedListing;
                const char* listingEnd = allocatedListing + allocatedListingLen;
                BOOL isDir = FALSE;

                int rightsCol = -1; // index of the column with permissions (used to detect links)
                if (dataIface != NULL)
                    rightsCol = dataIface->FindRightsColumn();

                // variables for Copy and Move operations
                CQuadWord size(-1, -1); // variable for the current file size
                char targetPath[MAX_PATH];
                if (CurItem->Type == fqitCopyExploreDir ||
                    CurItem->Type == fqitMoveExploreDir ||
                    CurItem->Type == fqitMoveExploreDirLink)
                {
                    CFTPQueueItemCopyMoveExplore* cmItem = (CFTPQueueItemCopyMoveExplore*)CurItem;
                    lstrcpyn(targetPath, cmItem->TgtPath, MAX_PATH);
                    SalamanderGeneral->SalPathAppend(targetPath, cmItem->TgtName, MAX_PATH); // must succeed, the directory already exists on disk and its full name is at most PATH_MAX_PATH-1 characters
                }
                else
                    targetPath[0] = 0;
                BOOL is_AS_400_QSYS_LIB_Path = isAS400 && FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", WorkingPath);
                parser->BeforeParsing(listing, listingEnd, StartTimeOfListing.wYear, StartTimeOfListing.wMonth,
                                      StartTimeOfListing.wDay, FALSE); // parser initialization
                while (parser->GetNextItemFromListing(&file, &isDir, dataIface, &(serverType->Columns), &listing,
                                                      listingEnd, NULL, &err, emptyCol))
                {
                    if (!isDir || file.NameLen > 2 ||
                        file.Name[0] != '.' || (file.Name[1] != 0 && file.Name[1] != '.')) // ignore the "." and ".." directories
                    {
                        // add an item for the parsed file/directory to ftpQueueItems
                        CFTPQueueItem* item = NULL;
                        CFTPQueueItemType type;
                        BOOL ok = TRUE;
                        CFTPQueueItemState state = sqisWaiting;
                        DWORD problemID = ITEMPR_OK;
                        BOOL skip = FALSE;
                        switch (CurItem->Type)
                        {
                        case fqitDeleteExploreDir: // explore directories for delete (note: links to directories are deleted as a whole, the operation's purpose is fulfilled and nothing "extra" is removed) (object of class CFTPQueueItemDelExplore)
                        {
                            int skippedItems = 0;  // unused
                            int uiNeededItems = 0; // unused
                            item = CreateItemForDeleteOperation(&file, isDir, rightsCol, dataIface, &type, &ok, FALSE,
                                                                operationsHiddenFileDel, operationsHiddenDirDel,
                                                                &state, &problemID, &skippedItems, &uiNeededItems);
                            break;
                        }

                        case fqitCopyExploreDir:     // explore a directory or a link to a directory for copying (object of class CFTPQueueItemCopyMoveExplore)
                        case fqitMoveExploreDir:     // explore a directory for moving (deletes the directory after completion) (object of class CFTPQueueItemCopyMoveExplore)
                        case fqitMoveExploreDirLink: // explore a link to a directory for moving (deletes the directory link after completion) (object of class CFTPQueueItemCopyMoveExplore)
                        {
                            char mbrName[MAX_PATH];
                            char* tgtName = file.Name;
                            if (is_AS_400_QSYS_LIB_Path)
                            {
                                FTPAS400CutFileNamePart(mbrName, tgtName);
                                tgtName = mbrName;
                            }
                            item = CreateItemForCopyOrMoveOperation(&file, isDir, rightsCol, dataIface,
                                                                    &type, transferMode, Oper,
                                                                    CurItem->Type == fqitCopyExploreDir,
                                                                    targetPath, tgtName, // we are in a subdirectory, names are no longer generated from the operation mask here
                                                                    &size, sizeInBytes, totalSize);
                            break;
                        }

                        case fqitChAttrsExploreDir:     // explore directories for attribute changes (also adds an item for changing the directory attributes) (object of class CFTPQueueItemChAttrExplore)
                        case fqitChAttrsExploreDirLink: // explore a link to a directory for attribute changes (object of class CFTPQueueItem)
                        {
                            int skippedItems = 0;  // unused
                            int uiNeededItems = 0; // unused
                            item = CreateItemForChangeAttrsOperation(&file, isDir, rightsCol, dataIface,
                                                                     &type, &ok, &state, &problemID,
                                                                     &skippedItems, &uiNeededItems, &skip, selFiles,
                                                                     selDirs, includeSubdirs, attrAndMask,
                                                                     attrOrMask, operationsUnknownAttrs);
                            break;
                        }
                        }
                        if (item != NULL)
                        {
                            if (ok)
                            {
                                item->SetItem(-1, type, state, problemID, WorkingPath, file.Name);
                                ftpQueueItems->Add(item); // add the operation to the queue
                                if (!ftpQueueItems->IsGood())
                                {
                                    ftpQueueItems->ResetState();
                                    ok = FALSE;
                                }
                            }
                            if (!ok)
                            {
                                err = TRUE;
                                delete item;
                            }
                        }
                        else
                        {
                            if (!skip) // only if this is not skipping the item but a low-memory error
                            {
                                TRACE_E(LOW_MEMORY);
                                err = TRUE;
                            }
                        }
                    }
                    // release file or directory data
                    dataIface->ReleasePluginData(file, isDir);
                    SalamanderGeneral->Free(file.Name);

                    if (err)
                        break;
                }
                if (!err && listing == listingEnd)
                    ret = TRUE; // parsing finished successfully
            }
            else
                err = TRUE; // only a lack of memory can happen here
        }
        else
        {
            if (emptyCol == NULL)
                TRACE_E(LOW_MEMORY);
            err = TRUE; // low memory
        }
        if (emptyCol != NULL)
            delete[] emptyCol;
        if (dataIface != NULL)
            delete dataIface;
    }
    else
    {
        TRACE_E(LOW_MEMORY);
        err = TRUE; // low memory
    }
    *lowMem = err;
    return ret;
}

//
// ****************************************************************************
// CExploredPaths
//

BOOL CExploredPaths::ContainsPath(const char* path)
{
    int pathLen = (int)strlen(path);
    int index;
    return GetPathIndex(path, pathLen, index);
}

BOOL CExploredPaths::AddPath(const char* path)
{
    int pathLen = (int)strlen(path);
    int index;
    if (!GetPathIndex(path, pathLen, index))
    {
        char* p = (char*)malloc(sizeof(short) + pathLen + 1);
        if (p != NULL)
        {
            *((short*)p) = pathLen;
            memcpy(((short*)p) + 1, path, pathLen + 1);
            Paths.Insert(index, p);
            if (!Paths.IsGood())
            {
                Paths.ResetState();
                free(p);
            }
        }
        else
            TRACE_E(LOW_MEMORY);
        return TRUE;
    }
    else
        return FALSE; // already present in the array
}

BOOL CExploredPaths::GetPathIndex(const char* path, int pathLen, int& index)
{
    if (Paths.Count == 0)
    {
        index = 0;
        return FALSE;
    }

    int l = 0, r = Paths.Count - 1, m;
    int res;
    while (1)
    {
        m = (l + r) / 2;
        const short* pathM = (const short*)Paths[m];
        res = *pathM - pathLen;
        if (res == 0)
            res = memcmp((const char*)(pathM + 1), path, pathLen);
        if (res == 0) // found
        {
            index = m;
            return TRUE;
        }
        else if (res > 0)
        {
            if (l == r || l > m - 1) // not found
            {
                index = m; // should be at this position
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // not found
            {
                index = m + 1; // should come after this position
                return FALSE;
            }
            l = m + 1;
        }
    }
}
