// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

void CFTPWorker::HandleEventInPreparingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                             BOOL& reportWorkerChange)
{
    if ((!DiskWorkIsUsed || event == fweWorkerShouldStop) && ShouldStop) // mame ukoncit workera
    {
        if (SubState != fwssPrepQuitSent && SubState != fwssPrepWaitForDiskAfterQuitSent && !SocketClosed)
        {
            SubState = (SubState == fwssPrepWaitForDisk ? fwssPrepWaitForDiskAfterQuitSent : fwssPrepQuitSent); // abychom neposilali "QUIT" vicekrat
            sendQuitCmd = TRUE;                                                                                 // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
        }
    }
    else // normalni cinnost
    {
        // overime proveditelnost polozky
        BOOL fail = FALSE;
        BOOL wait = FALSE;
        BOOL quitSent = FALSE;
        if (CurItem != NULL) // "always true"
        {
            switch (CurItem->Type)
            {
            case fqitCopyResolveLink: // kopirovani: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
            case fqitMoveResolveLink: // presun: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
                break;                // neni co overovat

            case fqitCopyExploreDir:     // explore adresare nebo linku na adresar pro kopirovani (objekt tridy CFTPQueueItemCopyMoveExplore)
            case fqitMoveExploreDir:     // explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
            case fqitMoveExploreDirLink: // explore linku na adresar pro presun (po dokonceni smaze link na adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    if (((CFTPQueueItemCopyMoveExplore*)CurItem)->TgtDirState == TGTDIRSTATE_UNKNOWN)
                    {
                        // zkusime vytvorit cilovy adresar na disku (zjistime i jestli existuje)
                        if (DiskWorkIsUsed)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                        InitDiskWork(WORKER_DISKWORKFINISHED, fdwtCreateDir,
                                     ((CFTPQueueItemCopyMoveExplore*)CurItem)->TgtPath,
                                     ((CFTPQueueItemCopyMoveExplore*)CurItem)->TgtName,
                                     CurItem->ForceAction, FALSE, NULL, NULL, NULL, 0, NULL);
                        if (CurItem->ForceAction != fqiaNone) // vynucena akce timto prestava platit
                            Queue->UpdateForceAction(CurItem, fqiaNone);
                        if (FTPDiskThread->AddWork(&DiskWork))
                        {
                            DiskWorkIsUsed = TRUE;
                            SubState = fwssPrepWaitForDisk; // pockame si na vysledek
                            wait = TRUE;
                        }
                        else // nelze vytvorit adresar, nelze pokracovat v provadeni polozky
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                        }
                    }
                    // else ; // neni co delat (cilovy adresar existuje)
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // mame vysledek diskove operace (vytvareni cil. adresare)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

                        // pokud uz jsme QUIT poslali, musime zamezit dalsimu poslani QUIT z noveho stavu
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        BOOL itemChange = FALSE;
                        if (DiskWork.NewTgtName != NULL)
                        {
                            Queue->UpdateTgtName((CFTPQueueItemCopyMoveExplore*)CurItem, DiskWork.NewTgtName);
                            DiskWork.NewTgtName = NULL;
                            itemChange = TRUE;
                        }
                        if (DiskWork.State == sqisNone)
                        { // adresar se podarilo vytvorit (i autorename) nebo uz existoval a muzeme ho pouzit
                            Queue->UpdateTgtDirState((CFTPQueueItemCopyMoveExplore*)CurItem, TGTDIRSTATE_READY);
                        }
                        else // pri vytvareni adresare nastala chyba nebo doslo ke skipnuti polozky
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            itemChange = TRUE;
                            fail = TRUE;
                        }
                        if (itemChange)
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                    }
                    else
                        wait = TRUE;
                    break;
                }
                }
                break;
            }

            case fqitUploadCopyExploreDir: // upload: explore adresare pro kopirovani (objekt tridy CFTPQueueItemCopyMoveUploadExplore)
            case fqitUploadMoveExploreDir: // upload: explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveUploadExplore)
            {
                if (CurItem->ForceAction == fqiaNone &&
                    ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtDirState == UPLOADTGTDIRSTATE_UNKNOWN &&
                    !FTPMayBeValidNameComponent(((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtName,
                                                ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath, TRUE,
                                                Oper->GetFTPServerPathType(((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath)))
                { // pokud jmeno nesplnuje konvence pro dany typ cesty, jde o problem "cilovy adresar nelze vytvorit"
                    switch (Oper->GetUploadCannotCreateDir())
                    {
                    case CANNOTCREATENAME_AUTORENAME:
                        break; // autorename (az na nej dojde) si musi poradit i s blbym jmenem

                    case CANNOTCREATENAME_SKIP:
                    {
                        Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTDIR, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                        fail = TRUE;
                        break;
                    }

                    default: // CANNOTCREATENAME_USERPROMPT
                    {
                        Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTDIR, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                        fail = TRUE;
                        break;
                    }
                    }
                }
                // else ; // neni co delat (force akce se delaji ve fwsWorking stavu a nebo cilovy adresar uz existuje a nebo jmeno adresare je OK)
                break;
            }

            case fqitDeleteExploreDir: // explore adresare pro delete (pozn.: linky na adresare mazeme jako celek, ucel operace se splni a nesmaze se nic "navic") (objekt tridy CFTPQueueItemDelExplore)
            case fqitDeleteLink:       // delete pro link (objekt tridy CFTPQueueItemDel)
            case fqitDeleteFile:       // delete pro soubor (objekt tridy CFTPQueueItemDel)
            {
                if (CurItem->Type == fqitDeleteExploreDir && ((CFTPQueueItemDelExplore*)CurItem)->IsHiddenDir || // pokud je adresar/soubor/link skryty, overime, co si s nim user preje delat
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
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                            break;
                        }

                        default: // HIDDENDIRDEL_USERPROMPT
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_DIRISHIDDEN, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
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
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                            break;
                        }

                        default: // HIDDENFILEDEL_USERPROMPT
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_FILEISHIDDEN, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                            break;
                        }
                        }
                    }
                }
            }

            case fqitDeleteDir:             // delete pro adresar (objekt tridy CFTPQueueItemDir)
            case fqitChAttrsExploreDir:     // explore adresare pro zmenu atributu (prida i polozku pro zmenu atributu adresare) (objekt tridy CFTPQueueItemChAttrExplore)
            case fqitChAttrsResolveLink:    // zmena atributu: zjisteni jestli jde o link na adresar (objekt tridy CFTPQueueItem)
            case fqitChAttrsExploreDirLink: // explore linku na adresar pro zmenu atributu (objekt tridy CFTPQueueItem)
                break;                      // neni co overovat

            case fqitCopyFileOrFileLink: // kopirovani souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
            case fqitMoveFileOrFileLink: // presun souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    if (((CFTPQueueItemCopyOrMove*)CurItem)->TgtFileState != TGTFILESTATE_TRANSFERRED)
                    {
                        // zkusime vytvorit/otevrit cilovy soubor na disku
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
                        if (CurItem->ForceAction != fqiaNone) // vynucena akce timto prestava platit
                            Queue->UpdateForceAction(CurItem, fqiaNone);
                        if (FTPDiskThread->AddWork(&DiskWork))
                        {
                            DiskWorkIsUsed = TRUE;
                            SubState = fwssPrepWaitForDisk; // pockame si na vysledek
                            wait = TRUE;
                        }
                        else // nelze vytvorit/otevrit soubor, nelze pokracovat v provadeni polozky
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                        }
                    }
                    // else ; // neni co delat (soubor uz je downloadly)
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // mame vysledek diskove operace (vytvareni cil. souboru)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

                        // pokud uz jsme QUIT poslali, musime zamezit dalsimu poslani QUIT z noveho stavu
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        BOOL itemChange = FALSE;
                        if (DiskWork.NewTgtName != NULL)
                        {
                            Queue->UpdateTgtName((CFTPQueueItemCopyOrMove*)CurItem, DiskWork.NewTgtName);
                            DiskWork.NewTgtName = NULL;
                            itemChange = TRUE;
                        }
                        if (DiskWork.State == sqisNone)
                        { // soubor se podarilo vytvorit nebo otevrit
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
                        else // pri vytvareni/otevirani souboru nastala chyba nebo doslo ke skipnuti polozky
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            itemChange = TRUE;
                            fail = TRUE;
                        }
                        if (itemChange)
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                    }
                    else
                        wait = TRUE;
                    break;
                }
                }
                break;
            }

            case fqitUploadCopyFile: // upload: kopirovani souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
            case fqitUploadMoveFile: // upload: presun souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
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
                    { // pokud jmeno nesplnuje konvence pro dany typ cesty, jde o problem "cilovy soubor nelze vytvorit"
                        switch (Oper->GetUploadCannotCreateFile())
                        {
                        case CANNOTCREATENAME_AUTORENAME:
                            break; // autorename (az na nej dojde) si musi poradit i s blbym jmenem

                        case CANNOTCREATENAME_SKIP:
                        {
                            Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTFILE, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                            break;
                        }

                        default: // CANNOTCREATENAME_USERPROMPT
                        {
                            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTFILE, 0, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                            break;
                        }
                        }
                    }
                    if (!fail && ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtFileState != UPLOADTGTFILESTATE_TRANSFERRED)
                    { // otevreme zdrojovy soubor na disku pro cteni
                        if (DiskWorkIsUsed)
                            TRACE_E("Unexpected situation 4 in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                        InitDiskWork(WORKER_DISKWORKFINISHED, fdwtOpenFileForReading, CurItem->Path, CurItem->Name,
                                     fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
                        if (FTPDiskThread->AddWork(&DiskWork))
                        {
                            DiskWorkIsUsed = TRUE;
                            SubState = fwssPrepWaitForDisk; // pockame si na vysledek
                            wait = TRUE;
                        }
                        else // nelze vytvorit/otevrit soubor, nelze pokracovat v provadeni polozky
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                            fail = TRUE;
                        }
                    }
                    // else ; // neni co delat (upload uz failnul nebo uz je soubor uploadly)
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // mame vysledek diskove operace (otevreni zdrojoveho souboru)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

                        // pokud uz jsme QUIT poslali, musime zamezit dalsimu poslani QUIT z noveho stavu
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        if (DiskWork.State == sqisNone)
                        { // soubor se podarilo otevrit
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
                        else // pri otevirani souboru nastala chyba
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
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

            case fqitUploadMoveDeleteDir: // upload: smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
            {
                switch (SubState)
                {
                case fwssNone:
                {
                    // zkusime smazat prazdny zdrojovy adresar z disku
                    if (DiskWorkIsUsed)
                        TRACE_E("Unexpected situation 3 in CFTPWorker::HandleEventInPreparingState(): DiskWorkIsUsed may not be TRUE here!");
                    InitDiskWork(WORKER_DISKWORKFINISHED, fdwtDeleteDir, CurItem->Path, CurItem->Name,
                                 fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
                    if (FTPDiskThread->AddWork(&DiskWork))
                    {
                        DiskWorkIsUsed = TRUE;
                        SubState = fwssPrepWaitForDisk; // pockame si na vysledek
                        wait = TRUE;
                    }
                    else // nelze smazat adresar, zapiseme chybu do polozky
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                        fail = TRUE;
                    }
                    break;
                }

                case fwssPrepWaitForDisk:
                case fwssPrepWaitForDiskAfterQuitSent:
                {
                    if (event == fweDiskWorkFinished) // mame vysledek diskove operace (mazani prazdneho zdrojoveho adresare)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

                        // pokud uz jsme QUIT poslali, musime zamezit dalsimu poslani QUIT z noveho stavu
                        quitSent = SubState == fwssPrepWaitForDiskAfterQuitSent;

                        if (DiskWork.State == sqisNone)
                        { // adresar se podarilo smazat
                            Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky

                            // jdeme si najit dalsi praci
                            CurItem = NULL;
                            State = fwsLookingForWork; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                            if (quitSent)
                                SubState = fwssLookFWQuitSent; // predame do fwsLookingForWork, ze QUIT uz byl poslany
                            else
                                SubState = fwssNone;
                            postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                            reportWorkerChange = TRUE;
                            wait = TRUE; // preskocime zmeny stavu pod timhle switchem
                        }
                        else // pri mazani adresare nastala chyba
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
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

            case fqitMoveDeleteDir:     // smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
            case fqitMoveDeleteDirLink: // smazani linku na adresar po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                break;                  // neni co overovat

            case fqitChAttrsFile: // zmena atributu souboru (pozn.: u linku se atributy menit nedaji) (objekt tridy CFTPQueueItemChAttr)
            case fqitChAttrsDir:  // zmena atributu adresare (objekt tridy CFTPQueueItemChAttrDir)
            {
                if (CurItem->Type == fqitChAttrsFile && ((CFTPQueueItemChAttr*)CurItem)->AttrErr || // zareagujeme na chybu "neznamy atribut ma byt zachovan, coz neumime"
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
                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                        fail = TRUE;
                        break;
                    }

                    default: // UNKNOWNATTRS_USERPROMPT
                    {
                        Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UNKNOWNATTRS, 0, NULL, Oper);
                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
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
            if (fail) // polozka neni proveditelna, je uz na ni nastaven error, jdeme najit jinou polozku
            {
                CurItem = NULL;
                State = fwsLookingForWork; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                if (quitSent)
                    SubState = fwssLookFWQuitSent; // predame do fwsLookingForWork, ze QUIT uz byl poslany
                else
                    SubState = fwssNone;
            }
            else // zatim vse OK, pokracujeme
            {
                if (SocketClosed)
                {
                    State = fwsConnecting; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                    SubState = fwssNone;   // connectiona neni otevrena, proto quitSent muzeme ignorovat (neni kam poslat QUIT)
                }
                else
                {
                    State = fwsWorking; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                    if (UploadDirGetTgtPathListing)
                        TRACE_E("CFTPWorker::HandleEventInPreparingState(): UploadDirGetTgtPathListing==TRUE!");
                    StatusType = wstNone;
                    if (quitSent)
                        SubState = fwssWorkStopped; // predame do fwsWorking, ze QUIT uz byl poslany
                    else
                        SubState = fwssNone;
                }
            }
            postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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
        run = FALSE;              // pri zmene na TRUE dojde k okamzitemu dalsimu pruchodu smyckou
        BOOL closeSocket = FALSE; // TRUE = ma se zavrit socket workera
        if (ShouldStop)           // mame ukoncit workera (vse ve smycce kvuli 'closeSocket')
        {
            switch (SubState)
            {
                // case fwssNone:  // neni potreba nic provest

                // case fwssConConnect:           // nemuze nastat (jen mezistav)
                // case fwssConReconnect:         // nemuze nastat (jen mezistav)
                // case fwssConSendNextScriptCmd: // nemuze nastat (jen mezistav)
                // case fwssConSendInitCmds:      // nemuze nastat (jen mezistav)
                // case fwssConSendSyst:          // nemuze nastat (jen mezistav)

            case fwssConWaitingForIP: // smazeme timer WORKER_CONTIMEOUTTIMID
            {
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                break;
            }

            case fwssConWaitForConRes: // zavreme spojeni a smazeme timer WORKER_CONTIMEOUTTIMID
            {
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                closeSocket = TRUE;
                break;
            }

            case fwssConWaitForPrompt:       // zavreme spojeni a smazeme timer WORKER_TIMEOUTTIMERID
            case fwssConWaitForScriptCmdRes: // zavreme spojeni a smazeme timer WORKER_TIMEOUTTIMERID (nemusi uz existovat, ale to nevadi)
            case fwssConWaitForInitCmdRes:   // zavreme spojeni a smazeme timer WORKER_TIMEOUTTIMERID (nemusi uz existovat, ale to nevadi)
            case fwssConWaitForSystRes:      // zavreme spojeni a smazeme timer WORKER_TIMEOUTTIMERID (nemusi uz existovat, ale to nevadi)
            {
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_TIMEOUTTIMERID);
                closeSocket = TRUE;
                break;
            }
            }
        }
        else // normalni cinnost
        {
            switch (SubState)
            {
            case fwssNone: // zjistime jestli je potreba ziskavat IP adresu
            {
                if (ConnectAttemptNumber == 0) // pro tohoto workera jde o prvni pokus o nazavani spojeni
                {
                    ConnectAttemptNumber = 1;
                }
                else
                {
                    if (ConnectAttemptNumber == 1) // prvni pokus o reconnect (dalsi pokusy se ridi z fwssConReconnect)
                    {
                        if (ConnectAttemptNumber + 1 > Config.GetConnectRetries() + 1)
                        { // druhy pokus o navazani spojeni (prvni pokus = spojeni, ktere se rozpadlo) je
                            // okamzity (bez cekani po vypadku), ceka se jen mezi jednotlivymi pokusy
                            // o navazani spojeni
                            State = fwsConnectionError; // POZOR: predpoklada nastavene ErrorDescr
                            operStatusMaybeChanged = TRUE;
                            ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                            SubState = fwssNone;
                            postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                            reportWorkerChange = TRUE;
                            break; // konec provadeni stavu fwsConnecting
                        }
                        else
                            ConnectAttemptNumber++;
                    }
                }

                // provedeme reset caches
                HaveWorkingPath = FALSE;
                CurrentTransferMode = ctrmUnknown;

                DWORD serverIP;
                if (Oper->GetServerAddress(&serverIP, host, HOST_MAX_SIZE)) // mame IP
                {
                    SubState = fwssConConnect;
                    run = TRUE;
                }
                else // mame jen jmennou adresu
                {
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    BOOL getHostByAddressRes = GetHostByAddress(host, ++IPRequestUID);
                    RefreshCopiesOfUIDAndMsg(); // obnovime kopie UID+Msg (doslo k jejich zmene)
                    if (!getHostByAddressRes)
                    { // neni sance na uspech -> ohlasime chybu
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKERGETIPERROR),
                                    GetWorkerErrorTxt(NO_ERROR, errBuf, 50 + FTP_MAX_PATH));
                        CorrectErrorDescr();
                        SubState = fwssConReconnect;
                        run = TRUE;
                    }
                    else // budeme cekat az IP adresa serveru prijde jako fwseIPReceived, pak si IP znovu vyzvedneme z operace
                    {
                        // nastavime novy timeout timer
                        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                        if (serverTimeout < 1000)
                            serverTimeout = 1000; // aspon sekundu
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                        // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                        SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                                WORKER_CONTIMEOUTTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop

                        SubState = fwssConWaitingForIP;
                        // run = TRUE;  // nema smysl (zatim nenastala zadna udalost)
                    }
                }
                break;
            }

            case fwssConWaitingForIP: // cekame na prijeti IP adresy (preklad ze jmenne adresy)
            {
                switch (event)
                {
                case fweIPReceived:
                {
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                    SubState = fwssConConnect;
                    run = TRUE;
                    break;
                }

                case fweIPRecFailure:
                {
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
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

            case fwssConConnect: // provedeme Connect()
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

                ResetBuffersAndEvents(); // vyprazdnime buffery (zahodime stara data) a inicializujeme promenne spojene s connectem

                DWORD error;
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                BOOL conRes = ConnectWithProxy(serverIP, port, proxyType, &error, host, hostPort,
                                               proxyUser, proxyPassword, hostIP);
                RefreshCopiesOfUIDAndMsg(); // obnovime kopie UID+Msg (doslo k jejich zmene)
                Logs.SetIsConnected(LogUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg();
                if (conRes)
                {
                    SocketClosed = FALSE; // socket uz je zase otevreny

                    // nastavime novy timeout timer
                    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                    if (serverTimeout < 1000)
                        serverTimeout = 1000; // aspon sekundu
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                            WORKER_CONTIMEOUTTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop

                    SubState = fwssConWaitForConRes;
                    // run = TRUE;  // nema smysl (zatim nenastala zadna udalost)
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

            case fwssConWaitForConRes: // cekame na vysledek Connect()
            {
                switch (event)
                {
                case fweConnected:
                {
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);

                    // nastavime stav workera tak, aby zacal cekat na odpoved na prikaz (i kdyz jsme zadny
                    // neposlali, na odpoved serveru cekame) - nastavime timeout pro prijem odpovedi na prikaz
                    CommandState = fwcsWaitForLoginPrompt;
                    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                    if (serverTimeout < 1000)
                        serverTimeout = 1000; // aspon sekundu
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                            WORKER_TIMEOUTTIMERID, NULL); // chybu ignorujeme, maximalne si user da Stop

                    SubState = fwssConWaitForPrompt;
                    // run = TRUE; // nema smysl (zatim nenastala zadna udalost)
                    break;
                }

                case fweConnectFailure:
                {
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_CONTIMEOUTTIMID);
                    SubState = fwssConReconnect;
                    run = TRUE;
                    closeSocket = TRUE;
                    break;
                }

                case fweConTimeout:
                {
                    // vzhledem k tomu, ze uz v sekci CSocket::SocketCritSect jsme, je tohle volani
                    // mozne i ze sekce CFTPWorker::WorkerCritSect (nehrozi dead-lock)
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

            case fwssConWaitForPrompt: // cekame na login prompt od serveru
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (replyCode != -1)
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&
                            FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION) // napr. 220 - Service ready for new user
                        {
                            Oper->SetServerFirstReply(reply, replySize); // ulozime prvni odpoved serveru (zdroj informaci o verzi serveru)

                            ProxyScriptExecPoint = NULL; // posilame zase od prvniho prikazu login skriptu
                            ProxyScriptLastCmdReply = -1;
                            SubState = Oper->GetEncryptControlConnection() ? fwssConSendAUTH : fwssConSendNextScriptCmd; // budeme posilat prikazy z login skriptu
                            run = TRUE;
                        }
                        else
                        {
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) // napr. 421 Service not available, closing control connection
                            {
                                lstrcpyn(ErrorDescr, CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize),
                                         FTPWORKER_ERRDESCR_BUFSIZE);
                                CorrectErrorDescr();
                                closeSocket = TRUE; // zavreme spojeni (nema vyznam pokracovat)

                                SubState = fwssConReconnect;
                                run = TRUE;
                            }
                            else // necekana reakce, ignorujeme ji
                            {
                                TRACE_E("Unexpected reply: " << CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                            }
                        }
                    }
                    else // neni FTP server
                    {
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_NOTFTPSERVERERROR),
                                    CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                        CorrectErrorDescr();
                        closeSocket = TRUE; // zavreme spojeni (nema vyznam pokracovat)

                        State = fwsConnectionError;
                        operStatusMaybeChanged = TRUE;
                        ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                        SubState = fwssNone;
                        postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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

            case fwssConSendNextScriptCmd: // posleme dalsi prikaz login skriptu
            {
                BOOL fail = FALSE;
                char errDescrBuf[300];
                BOOL needUserInput;
                if (Oper->PrepareNextScriptCmd(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH, &cmdLen,
                                               &ProxyScriptExecPoint, ProxyScriptLastCmdReply,
                                               errDescrBuf, &needUserInput) &&
                    !needUserInput)
                {
                    if (buf[0] == 0) // konec skriptu
                    {
                        if (ProxyScriptLastCmdReply == -1) // skript neobsahuje zadny prikaz, ktery by se poslal na server - napr. pokud se prikazy preskoci kvuli tomu, ze obsahuji nepovinne promenne
                        {
                            lstrcpyn(ErrorDescr, LoadStr(IDS_INCOMPLETEPRXSCR2), FTPWORKER_ERRDESCR_BUFSIZE);
                            CorrectErrorDescr();
                            fail = TRUE;
                        }
                        else
                        {
                            if (FTP_DIGIT_1(ProxyScriptLastCmdReply) == FTP_D1_SUCCESS) // napr. 230 User logged in, proceed
                            {
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGLOGINSUCCESS), -1, TRUE);
                                NextInitCmd = 0;                                                             // poslat prvni init-ftp-prikaz
                                SubState = Oper->GetCompressData() ? fwssConSendMODEZ : fwssConSendInitCmds; // jsme zalogovani, jdeme poslat init-ftp-prikazy
                                run = TRUE;
                            }
                            else // FTP_DIGIT_1(ProxyScriptLastCmdReply) == FTP_D1_PARTIALSUCCESS  // napr. 331 User name okay, need password
                            {    // predpoklada, ze to sem prislo z fwssConWaitForScriptCmdRes a reply+replySize je stale platne
                                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_INCOMPLETEPRXSCR3),
                                            CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                                CorrectErrorDescr();
                                fail = TRUE;
                            }
                        }
                    }
                    else // mame dalsi prikaz na poslani
                    {
                        sendCmd = TRUE;
                        SubState = fwssConWaitForScriptCmdRes;
                        // run = TRUE; // nema smysl (zatim nenastala zadna udalost)
                    }
                }
                else // chyba ve skriptu nebo chybejici hodnota promenne
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
                    closeSocket = TRUE; // zavreme spojeni (nema vyznam pokracovat)

                    State = fwsConnectionError; // POZOR: ErrorDescr se nastavuje vyse (viz "fail = TRUE")
                    operStatusMaybeChanged = TRUE;
                    ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                    SubState = fwssNone;
                    postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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
                                                         NULL /* pro control connection je to vzdy NULL */);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                if (ret)
                                {
                                    if (unverifiedCert != NULL) // zavreme connectionu a pokud zopakujeme az se user dozvi o neduveryhodnem certifikatu a prijme ho nebo zajisti jeho duveryhodnost (v Solve Error dialogu)
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

                        closeSocket = TRUE; // zavreme spojeni (nema vyznam pokracovat)

                        if (retryLoginWithoutAsking) // zkusime to znovu
                        {
                            SubState = fwssConReconnect;
                            run = TRUE;
                        }
                        else // ohlasime chybu uzivateli
                        {
                            State = fwsConnectionError;
                            operStatusMaybeChanged = TRUE;
                            ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                            SubState = fwssNone;
                            postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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

            case fwssConWaitForScriptCmdRes: // cekame na vysledek prikazu z login skriptu
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (replyCode != -1)
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||      // napr. 230 User logged in, proceed
                            FTP_DIGIT_1(replyCode) == FTP_D1_PARTIALSUCCESS) // napr. 331 User name okay, need password
                        {
                            SubState = fwssConSendNextScriptCmd;
                            ProxyScriptLastCmdReply = replyCode;
                            run = TRUE;
                        }
                        else
                        {
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // napr. 421 Service not available (too many users), closing control connection
                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // napr. 530 Not logged in (invalid password)
                            {
                                BOOL retryLoginWithoutAsking;
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR)
                                { // pohodlne reseni chyby "too many users" - zadne dotazy, hned "retry"
                                    // muze vadit: s timto kodem prijde zprava, na kterou je treba reagovat zmenou user/passwd
                                    retryLoginWithoutAsking = TRUE;
                                }
                                else
                                    retryLoginWithoutAsking = Oper->GetRetryLoginWithoutAsking();

                                _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_WORKERLOGINERR),
                                            CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                                CorrectErrorDescr();
                                closeSocket = TRUE; // zavreme spojeni (nema vyznam pokracovat)

                                if (retryLoginWithoutAsking) // zkusime to znovu
                                {
                                    SubState = fwssConReconnect;
                                    run = TRUE;
                                }
                                else // ohlasime chybu uzivateli a pockame az zmeni heslo/account
                                {
                                    State = fwsConnectionError;
                                    operStatusMaybeChanged = TRUE;
                                    ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                                    SubState = fwssNone;
                                    postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                                    reportWorkerChange = TRUE;
                                }
                            }
                            else // necekana reakce, ignorujeme ji
                            {
                                TRACE_E("Unexpected reply: " << CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                            }
                        }
                    }
                    else // neni FTP server
                    {
                        _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_NOTFTPSERVERERROR),
                                    CopyStr(errBuf, 50 + FTP_MAX_PATH, reply, replySize));
                        CorrectErrorDescr();
                        closeSocket = TRUE; // zavreme spojeni (nema vyznam pokracovat)

                        State = fwsConnectionError;
                        operStatusMaybeChanged = TRUE;
                        ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                        SubState = fwssNone;
                        postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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

            case fwssConSendInitCmds: // posleme inicializacni prikazy (user-defined, viz CFTPOperation::InitFTPCommands)
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
                            s++;     // odbourame jen prvni mezeru (aby na zacatek prikazu bylo mozne dat i mezery)
                        if (*s != 0) // je-li co, posleme to na server
                        {
                            if (i++ == NextInitCmd)
                            {
                                NextInitCmd++; // priste bereme dalsi prikaz (silne neefektivni reseni, ale neocekava se zde vetsi mnozstvi prikazu)
                                _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, "%s\r\n", s);
                                strcpy(buf, errBuf);
                                cmdLen = (int)strlen(buf);
                                sendCmd = TRUE;
                                break;
                            }
                        }
                    }
                }
                if (sendCmd) // posilame dalsi init-ftp-prikaz
                {
                    SubState = fwssConWaitForInitCmdRes;
                    // run = TRUE; // nema smysl (zatim nenastala zadna udalost)
                }
                else // jiz jsme poslali vsechny init-ftp-prikazy (nebo zadne neexistuji)
                {
                    SubState = fwssConSendSyst; // zjistime system serveru
                    run = TRUE;
                }
                break;
            }

            case fwssConWaitForInitCmdRes: // cekame na vysledek provadeneho inicializacniho prikazu
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    SubState = fwssConSendInitCmds; // posleme dalsi init-ftp-prikaz (na odpovedich serveru nam nezalezi)
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

            case fwssConSendSyst: // zjistime system serveru (posleme prikaz "SYST")
            {
                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH, ftpcmdSystem, &cmdLen); // nemuze nahlasit chybu
                sendCmd = TRUE;
                SubState = fwssConWaitForSystRes;
                // run = TRUE; // nema smysl (zatim nenastala zadna udalost)
                break;
            }

            case fwssConWaitForSystRes: // cekame na system serveru (vysledek prikazu "SYST")
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    Oper->SetServerSystem(reply, replySize); // ulozime system serveru (zdroj informaci o verzi serveru)

                    // spojeni je navazane, jdeme pracovat
                    State = fwsWorking; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                    if (UploadDirGetTgtPathListing)
                        TRACE_E("CFTPWorker::HandleEventInPreparingState(): UploadDirGetTgtPathListing==TRUE!");
                    SubState = fwssNone;
                    StatusType = wstNone;
                    postActivate = TRUE;      // postneme si aktivaci pro dalsi stav workera
                    ConnectAttemptNumber = 1; // spojeni je navazane, vracime se na jednicku, aby dalsi mel pripadny dalsi reconnect opet "nabito"
                    ErrorDescr[0] = 0;        // opet zacneme sbirat chybova hlaseni
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

            case fwssConReconnect: // vyhodnotime jestli provest reconnect nebo ohlasit chybu workera
            {
                if (ConnectAttemptNumber + 1 > Config.GetConnectRetries() + 1)
                {
                    State = fwsConnectionError; // POZOR: predpoklada nastavene ErrorDescr
                    operStatusMaybeChanged = TRUE;
                    ErrorOccurenceTime = Oper->GiveLastErrorOccurenceTime();
                    SubState = fwssNone;
                    postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                    reportWorkerChange = TRUE;
                }
                else
                {
                    // zkusime najit "sleeping" workera s otevrenou connectionou, kteremu bychom
                    // praci predali (misto zbytecneho reconnectu)
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    BOOL workMoved = Oper->GiveWorkToSleepingConWorker(this);
                    HANDLES(EnterCriticalSection(&WorkerCritSect));

                    if (!workMoved)
                    {
                        // jdeme si pockat na reconnect
                        ConnectAttemptNumber++;

                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                        // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                        SocketsThread->DeleteTimer(UID, WORKER_RECONTIMEOUTTIMID);

                        // nahodime timer pro dalsi pokus o connect
                        int delayBetweenConRetries = Config.GetDelayBetweenConRetries() * 1000;
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                        // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                        SocketsThread->AddTimer(Msg, UID, GetTickCount() + delayBetweenConRetries,
                                                WORKER_RECONTIMEOUTTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop

                        State = fwsWaitingForReconnect; // POZOR: predpoklada nastavene ErrorDescr; neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                        SubState = fwssNone;
                        // postActivate = TRUE;  // neni duvod, cekame na timeout
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

            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
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

    // napocitame masku 'validDataMask'
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
        DWORD* emptyCol = new DWORD[serverType->Columns.Count]; // pomocne predalokovane pole pro GetNextItemFromListing
        if (dataIface->IsGood() && emptyCol != NULL)
        {
            validDataMask |= dataIface->GetPLValidDataMask();
            CFTPParser* parser = serverType->CompiledParser;
            if (parser == NULL)
            {
                parser = CompileParsingRules(HandleNULLStr(serverType->RulesForParsing), &(serverType->Columns),
                                             NULL, NULL, NULL);
                serverType->CompiledParser = parser; // 'parser' nebudeme dealokovat, uz je v 'serverType'
            }
            if (parser != NULL)
            {
                CFileData file;
                const char* listing = allocatedListing;
                const char* listingEnd = allocatedListing + allocatedListingLen;
                BOOL isDir = FALSE;

                int rightsCol = -1; // index sloupce s pravy (pouziva se pro detekci linku)
                if (dataIface != NULL)
                    rightsCol = dataIface->FindRightsColumn();

                // promenne pro Copy a Move operace
                CQuadWord size(-1, -1); // promenna pro velikost aktualniho souboru
                char targetPath[MAX_PATH];
                if (CurItem->Type == fqitCopyExploreDir ||
                    CurItem->Type == fqitMoveExploreDir ||
                    CurItem->Type == fqitMoveExploreDirLink)
                {
                    CFTPQueueItemCopyMoveExplore* cmItem = (CFTPQueueItemCopyMoveExplore*)CurItem;
                    lstrcpyn(targetPath, cmItem->TgtPath, MAX_PATH);
                    SalamanderGeneral->SalPathAppend(targetPath, cmItem->TgtName, MAX_PATH); // musi vyjit, adresar uz existuje na disku a jeho fullname ma max. PATH_MAX_PATH-1 znaku
                }
                else
                    targetPath[0] = 0;
                BOOL is_AS_400_QSYS_LIB_Path = isAS400 && FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", WorkingPath);
                parser->BeforeParsing(listing, listingEnd, StartTimeOfListing.wYear, StartTimeOfListing.wMonth,
                                      StartTimeOfListing.wDay, FALSE); // init parseru
                while (parser->GetNextItemFromListing(&file, &isDir, dataIface, &(serverType->Columns), &listing,
                                                      listingEnd, NULL, &err, emptyCol))
                {
                    if (!isDir || file.NameLen > 2 ||
                        file.Name[0] != '.' || (file.Name[1] != 0 && file.Name[1] != '.')) // nejde o adresare "." a ".."
                    {
                        // pridame polozku pro vyparsovany soubor/adresar do ftpQueueItems
                        CFTPQueueItem* item = NULL;
                        CFTPQueueItemType type;
                        BOOL ok = TRUE;
                        CFTPQueueItemState state = sqisWaiting;
                        DWORD problemID = ITEMPR_OK;
                        BOOL skip = FALSE;
                        switch (CurItem->Type)
                        {
                        case fqitDeleteExploreDir: // explore adresare pro delete (pozn.: linky na adresare mazeme jako celek, ucel operace se splni a nesmaze se nic "navic") (objekt tridy CFTPQueueItemDelExplore)
                        {
                            int skippedItems = 0;  // nepouziva se
                            int uiNeededItems = 0; // nepouziva se
                            item = CreateItemForDeleteOperation(&file, isDir, rightsCol, dataIface, &type, &ok, FALSE,
                                                                operationsHiddenFileDel, operationsHiddenDirDel,
                                                                &state, &problemID, &skippedItems, &uiNeededItems);
                            break;
                        }

                        case fqitCopyExploreDir:     // explore adresare nebo linku na adresar pro kopirovani (objekt tridy CFTPQueueItemCopyMoveExplore)
                        case fqitMoveExploreDir:     // explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
                        case fqitMoveExploreDirLink: // explore linku na adresar pro presun (po dokonceni smaze link na adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
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
                                                                    targetPath, tgtName, // jsme v podadresari, tady uz se jmena nevyrabi z operacni masky
                                                                    &size, sizeInBytes, totalSize);
                            break;
                        }

                        case fqitChAttrsExploreDir:     // explore adresare pro zmenu atributu (prida i polozku pro zmenu atributu adresare) (objekt tridy CFTPQueueItemChAttrExplore)
                        case fqitChAttrsExploreDirLink: // explore linku na adresar pro zmenu atributu (objekt tridy CFTPQueueItem)
                        {
                            int skippedItems = 0;  // nepouziva se
                            int uiNeededItems = 0; // nepouziva se
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
                                ftpQueueItems->Add(item); // pridani operace do fronty
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
                            if (!skip) // jen pokud nejde o skipnuti polozky, ale o chybu nedostatku pameti
                            {
                                TRACE_E(LOW_MEMORY);
                                err = TRUE;
                            }
                        }
                    }
                    // uvolnime data souboru nebo adresare
                    dataIface->ReleasePluginData(file, isDir);
                    SalamanderGeneral->Free(file.Name);

                    if (err)
                        break;
                }
                if (!err && listing == listingEnd)
                    ret = TRUE; // parsovani probehlo uspesne
            }
            else
                err = TRUE; // muze byt jen nedostatek pameti
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
        return FALSE; // uz je v poli
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
        if (res == 0) // nalezeno
        {
            index = m;
            return TRUE;
        }
        else if (res > 0)
        {
            if (l == r || l > m - 1) // nenalezeno
            {
                index = m; // mel by byt na teto pozici
                return FALSE;
            }
            r = m - 1;
        }
        else
        {
            if (l == r) // nenalezeno
            {
                index = m + 1; // mel by byt az za touto pozici
                return FALSE;
            }
            l = m + 1;
        }
    }
}
