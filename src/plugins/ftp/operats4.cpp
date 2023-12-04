// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
        if (SubState == fwssNone) // pred zahajenim praci testneme ShouldStop
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
                handleShouldStop = TRUE; // postActivate = TRUE je zbytecne, v ShouldStop==TRUE worker nezacne makat
            else
                goto SKIP_TEST; // at se zbytecne neblbne s postActivate (optimalizace)
        }
        else
        {
        SKIP_TEST:
            if (CurItem != NULL) // "always true"
            {
                // pokud polozka pouziva data-connection (listovani i download), osetrime
                // zpravy o otevreni a zavreni data-connectiony
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
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                if (WorkerDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                                    WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                                WorkerDataCon->FreeFlushData();
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }
                        }
                        else
                        {
                            if (ShouldBePaused && WorkerDataCon != NULL) // pausnuti tesne pred zacatkem prenosu dat
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                WorkerDataCon->UpdatePauseStatus(TRUE);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                            }

                            if (WorkerDataConState == wdcsWaitingForConnection)
                                WorkerDataConState = wdcsTransferingData;
                            else
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweDataConConnectedToServer: WorkerDataConState is not wdcsWaitingForConnection!");

                            // zahajime periodicky update zobrazovani statusu "downloadu" (listing + copy & move z FTP na disk)
                            if (StatusType != wstNone)
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweDataConConnectedToServer: unexpected situation: StatusType != wstNone");
                            StatusType = wstDownloadStatus;
                            StatusConnectionIdleTime = 0;
                            StatusSpeed = 0;
                            StatusTransferred.Set(0, 0);
                            StatusTotal.Set(-1, -1);
                            LastTimeEstimation = -1;

                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                            // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + 100 /* prvni update statusu udelame "okamzite" */,
                                                    WORKER_STATUSUPDATETIMID, NULL); // chybu ignorujeme, max. se nebude updatit status
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
                            //BOOL dataConDecomprMissingStreamEnd = WorkerDataCon->GetDecomprMissingStreamEnd(); // bohuzel tenhle test je nepruchozi, napr. Serv-U 7 a 8 proste stream neukoncuje
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
                            if (WorkerDataConState != wdcsWaitingForConnection) // chodi i pri chybe connectu data-connectiony
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweDataConConnectionClosed: WorkerDataConState is not wdcsTransferingData!");
                        }
                    }
                }
                // pokud polozka pouziva upload data-connection (upload souboru), osetrime
                // zpravy o otevreni a zavreni upload data-connectiony
                if ((event == fweUplDataConConnectedToServer || event == fweUplDataConConnectionClosed) &&
                    (CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile))
                {
                    if (event == fweUplDataConConnectedToServer)
                    {
                        if (ResumingFileOnServer) // APPE zpusobil otevreni data-connectiony -> APPE je nejspis funkcni (implementovany)
                            Oper->SetDataConWasOpenedForAppendCmd(TRUE);
                        if (ShouldStop)
                        {
                            if (WorkerUploadDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                                    WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                                WorkerUploadDataCon->FreeBufferedData();
                                DeleteSocket(WorkerUploadDataCon);
                                WorkerUploadDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }
                        }
                        else
                        {
                            if (ShouldBePaused && WorkerUploadDataCon != NULL) // pausnuti tesne pred zacatkem prenosu dat
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                WorkerUploadDataCon->UpdatePauseStatus(TRUE);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                            }

                            if (WorkerDataConState == wdcsWaitingForConnection)
                                WorkerDataConState = wdcsTransferingData;
                            else
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweUplDataConConnectedToServer: WorkerDataConState is not wdcsWaitingForConnection!");

                            // zahajime periodicky update zobrazovani statusu "uploadu"
                            if (StatusType != wstNone)
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweUplDataConConnectedToServer: unexpected situation: StatusType != wstNone");
                            StatusType = wstUploadStatus;
                            StatusConnectionIdleTime = 0;
                            StatusSpeed = 0;
                            StatusTransferred.Set(0, 0);
                            StatusTotal.Set(-1, -1);
                            LastTimeEstimation = -1;

                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                            // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + 100 /* prvni update statusu udelame "okamzite" */,
                                                    WORKER_STATUSUPDATETIMID, NULL); // chybu ignorujeme, max. se nebude updatit status
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
                            if (WorkerDataConState != wdcsWaitingForConnection) // chodi i pri chybe connectu data-connectiony
                                TRACE_E("CFTPWorker::HandleEventInWorkingState(): fweUplDataConConnectionClosed: WorkerDataConState is not wdcsTransferingData!");
                        }
                    }
                }
                if (event == fweWorkerShouldPause || event == fweWorkerShouldResume)
                { // pausnuti/resumnuti behem prenosu dat
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
                case fqitDeleteExploreDir:      // explore adresare pro delete (pozn.: linky na adresare mazeme jako celek, ucel operace se splni a nesmaze se nic "navic") (objekt tridy CFTPQueueItemDelExplore)
                case fqitCopyExploreDir:        // explore adresare nebo linku na adresar pro kopirovani (objekt tridy CFTPQueueItemCopyMoveExplore)
                case fqitMoveExploreDir:        // explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
                case fqitMoveExploreDirLink:    // explore linku na adresar pro presun (po dokonceni smaze link na adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
                case fqitChAttrsExploreDir:     // explore adresare pro zmenu atributu (prida i polozku pro zmenu atributu adresare) (objekt tridy CFTPQueueItemChAttrExplore)
                case fqitChAttrsExploreDirLink: // explore linku na adresar pro zmenu atributu (objekt tridy CFTPQueueItem)
                {
                    HandleEventInWorkingState2(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                               cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                               conClosedRetryItem, lookForNewWork, handleShouldStop, NULL);
                    break;
                }

                case fqitUploadCopyExploreDir: // upload: explore adresare pro kopirovani (objekt tridy CFTPQueueItemCopyMoveUploadExplore)
                case fqitUploadMoveExploreDir: // upload: explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveUploadExplore)
                {
                    if (UploadDirGetTgtPathListing) // listujeme cilovou cestu do cache listingu pro upload
                    {
                        BOOL listingNotAccessible;
                        HandleEventInWorkingState2(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, &listingNotAccessible);
                        if (handleShouldStop || conClosedRetryItem || lookForNewWork)
                        { // listovani selhalo, informujeme o tom pripadne cekajici workery
                            UploadDirGetTgtPathListing = FALSE;
                            Oper->GetUserHostPort(userBuf, hostBuf, &port);
                            CFTPServerPathType pathType = Oper->GetFTPServerPathType(((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath);
                            BOOL listingOKErrorIgnored;
                            // volani UploadListingCache.ListingFailed() je mozne jen proto, ze jsme v sekci CSocketsThread::CritSect
                            UploadListingCache.ListingFailed(userBuf, hostBuf, port,
                                                             ((CFTPQueueItemCopyMoveUploadExplore*)CurItem)->TgtPath,
                                                             pathType, listingNotAccessible, NULL, &listingOKErrorIgnored);
                            if (listingOKErrorIgnored && lookForNewWork) // na polozce je hlasena chyba listovani, tuto chybu stornujeme
                            {
                                lookForNewWork = FALSE;
                                Queue->UpdateItemState(CurItem, sqisProcessing, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                StatusType = wstNone;
                                SubState = fwssWorkStartWork;
                                Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky (pro pripad, ze uz se chyba stihla ukazat -- temer nemozne)
                                postActivate = TRUE;                  // impulz pro pokracovani v praci
                                reportWorkerChange = TRUE;            // potrebujeme schovat pripadny progres tahani listingu

                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // zrusime pripadny timer z predchozi prace
                            }
                        }
                    }
                    else // ostatni prace jde sem
                    {
                        HandleEventInWorkingState4(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, quitCmdWasSent);
                    }
                    break;
                }

                case fqitCopyResolveLink:    // kopirovani: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
                case fqitMoveResolveLink:    // presun: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
                case fqitChAttrsResolveLink: // zmena atributu: zjisteni jestli jde o link na adresar (objekt tridy CFTPQueueItem)
                {
                    switch (SubState)
                    {
                    case fwssWorkStartWork: // zjistime na jakou cestu se mame prepnout na serveru a posleme CWD
                    {
                        // pred resolvem linku pro change-attr musime vynulovat merak rychlosti (rychlost resolve
                        // se nemeri) - dojde k zobrazeni "(unknown)" time-left v operation-dialogu
                        if (CurItem->Type == fqitChAttrsResolveLink)
                        {
                            Oper->GetGlobalTransferSpeedMeter()->Clear();
                            Oper->GetGlobalTransferSpeedMeter()->JustConnected();
                        }

                        lstrcpyn(ftpPath, CurItem->Path, FTP_MAX_PATH);
                        CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
                        if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, CurItem->Name, TRUE))
                        { // mame cestu, posleme na server CWD do zkoumaneho adresare
                            _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_LOGMSGRESOLVINGLINK), ftpPath);
                            Logs.LogMessage(LogUID, errText, -1, TRUE);

                            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                              ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // nemuze nahlasit chybu
                            sendCmd = TRUE;
                            SubState = fwssWorkResLnkWaitForCWDRes;

                            HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
                        }
                        else // chyba syntaxe cesty nebo by vznikla moc dlouha cesta
                        {
                            // chyba na polozce, zapiseme do ni tento stav
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INVALIDPATHTOLINK, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                        break;
                    }

                    case fwssWorkResLnkWaitForCWDRes: // resolve-link: cekame na vysledek "CWD" (zmena cesty do zkoumaneho linku - podari-li se, je to link na adresar)
                    {
                        switch (event)
                        {
                        // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                        case fweCmdReplyReceived:
                        {
                            BOOL chAttrsResolve = CurItem->Type == fqitChAttrsResolveLink;
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||
                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)
                            {
                                BOOL err = FALSE;
                                CFTPQueueItem* item = NULL;
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // uspech, link vede do adresare
                                {
                                    switch (CurItem->Type)
                                    {
                                    case fqitCopyResolveLink: // kopirovani: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
                                    case fqitMoveResolveLink: // presun: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
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

                                    case fqitChAttrsResolveLink: // zmena atributu: zjisteni jestli jde o link na adresar (objekt tridy CFTPQueueItem)
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
                                else // permanentni chyba, link vede nejspis na soubor (ale muze jit i o "550 Permission denied", bohuzel 550 je i "550 Not a directory", takze nerozlisitelne...)
                                {
                                    switch (CurItem->Type)
                                    {
                                    case fqitCopyResolveLink: // kopirovani: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
                                    case fqitMoveResolveLink: // presun: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
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

                                        // case fqitChAttrsResolveLink:     // zmena atributu: zjisteni jestli jde o link na adresar (objekt tridy CFTPQueueItem)
                                        // zmena atributu u linku je nesmysl, takze resolve polozku proste jen smazeme z fronty
                                    }
                                }

                                if (!err)
                                {
                                    // probiha vice operaci nad daty, ostatni musi pockat az se provedou vsechny,
                                    // jinak budou pracovat s nekonzistentnimi daty
                                    Queue->LockForMoreOperations();

                                    int curItemParent = CurItem->ParentUID;
                                    if (Queue->ReplaceItemWithListOfItems(CurItem->UID, &item, item != NULL ? 1 : 0))
                                    { // CurItem uz je dealokovana, byla nahrazena polozkou 'item' (nebo jen smazana)
                                        CurItem = NULL;
                                        BOOL itemAdded = item != NULL;
                                        item = NULL; // aby nedoslo k dealokaci 'item', uz je pridana ve fronte

                                        // polozce/operaci CurItem->ParentUID snizime o jednu NotDone (za CurItem ve stavu
                                        // sqisProcessing) a pokud pridavame polozku 'item', zvysime NotDone o jednu (nova
                                        // polozka je ve stavu sqisWaiting, zbyla tri pocitadla jsou nulova)
                                        Oper->AddToItemOrOperationCounters(curItemParent, (itemAdded ? 1 : 0) - 1, 0, 0, 0, FALSE);

                                        Queue->UnlockForMoreOperations();

                                        Oper->ReportItemChange(-1); // pozadame o redraw vsech polozek

                                        // tento worker si bude muset najit dalsi praci
                                        State = fwsLookingForWork; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                                        SubState = fwssNone;
                                        postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                                        reportWorkerChange = TRUE;

                                        // neni potreba informovat zadneho spiciho workera, ze se objevila nova prace, protoze
                                        // tento worker ('this') si bude hledat dalsi praci a tedy najde pripadnou novou polozku
                                    }
                                    else
                                    {
                                        err = TRUE; // nedostatek pameti -> vepiseme chybu do polozky

                                        Queue->UnlockForMoreOperations();
                                    }
                                }

                                if (err)
                                {
                                    // chyba na polozce, zapiseme do ni tento stav
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                if (item != NULL)
                                    delete item;
                            }
                            else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                            {
                                CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESOLVELNK, NO_ERROR,
                                                       SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                       Oper);
                                lookForNewWork = TRUE;
                            }

                            // pokud jde o resolve linku pro change-attr, musime vynulovat merak rychlosti
                            // (rychlost resolvu se nemeri, tento okamzik tedy muze byt zacatek mereni rychlosti operaci
                            // mazani/change-attr), aby se korektne ukazoval time-left v operation-dialogu
                            if (chAttrsResolve)
                            {
                                Oper->GetGlobalTransferSpeedMeter()->Clear();
                                Oper->GetGlobalTransferSpeedMeter()->JustConnected();
                            }
                            break;
                        }

                        case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
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

                case fqitDeleteLink:         // delete pro link (objekt tridy CFTPQueueItemDel)
                case fqitDeleteFile:         // delete pro soubor (objekt tridy CFTPQueueItemDel)
                case fqitDeleteDir:          // delete pro adresar (objekt tridy CFTPQueueItemDir)
                case fqitCopyFileOrFileLink: // kopirovani souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
                case fqitMoveFileOrFileLink: // presun souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
                case fqitMoveDeleteDir:      // smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                case fqitMoveDeleteDirLink:  // smazani linku na adresar po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                case fqitChAttrsFile:        // zmena atributu souboru (pozn.: u linku se atributy menit nedaji) (objekt tridy CFTPQueueItemChAttr)
                case fqitChAttrsDir:         // zmena atributu adresare (objekt tridy CFTPQueueItemChAttrDir)
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
                            case fwssWorkStartWork: // prepneme se na serveru do adresare polozky (pokud v nem uz nejsme)
                            {
                                // nejprve zalogujeme jakou praci jdeme delat
                                int opDescrResID = 0;
                                const char* tgtName = NULL;
                                switch (CurItem->Type)
                                {
                                case fqitDeleteLink:
                                    opDescrResID = IDS_LOGMSGDELETELINK;
                                    break; // delete pro link (objekt tridy CFTPQueueItemDel)
                                case fqitDeleteFile:
                                    opDescrResID = IDS_LOGMSGDELETEFILE;
                                    break; // delete pro soubor (objekt tridy CFTPQueueItemDel)

                                case fqitMoveDeleteDir: // smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                                case fqitDeleteDir:
                                    opDescrResID = IDS_LOGMSGDELETEDIR;
                                    break; // delete pro adresar (objekt tridy CFTPQueueItemDir)

                                case fqitCopyFileOrFileLink: // kopirovani souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
                                case fqitMoveFileOrFileLink: // presun souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
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
                                    break; // smazani linku na adresar po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                                case fqitChAttrsFile:
                                    opDescrResID = IDS_LOGMSGCHATTRSFILE;
                                    break; // zmena atributu souboru (pozn.: u linku se atributy menit nedaji) (objekt tridy CFTPQueueItemChAttr)
                                case fqitChAttrsDir:
                                    opDescrResID = IDS_LOGMSGCHATTRSDIR;
                                    break; // zmena atributu adresare (objekt tridy CFTPQueueItemChAttrDir)
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
                                    { // nad timto souborem jiz probiha jina operace, at to user zkusi znovu pozdeji
                                        // chyba na polozce, zapiseme do ni tento stav
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_SRCFILEINUSE, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                        canContinue = FALSE;
                                    }
                                    // else ; // soubor na serveru jeste neni otevreny, muzeme ho smazat
                                }
                                if (canContinue)
                                {
                                    if (!HaveWorkingPath || strcmp(WorkingPath, CurItem->Path) != 0)
                                    { // je treba zmenit pracovni cestu (predpoklad: server vraci stale stejny retezec cesty - ten
                                        // se do polozky dostal pri explore-dir nebo z panelu, v obou pripadech slo o cestu vracenou
                                        // serverem na prikaz PWD)
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdChangeWorkingPath, &cmdLen, CurItem->Path); // nemuze nahlasit chybu
                                        sendCmd = TRUE;
                                        SubState = fwssWorkSimpleCmdWaitForCWDRes;

                                        HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
                                    }
                                    else // pracovni adresar uz je nastaveny
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
                                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                                case fweCmdReplyReceived:
                                {
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // uspesne jsme zmenili pracovni cestu, diky tomu, ze tato cesta byla "kdysi" vracena
                                        // ze serveru na prikaz PWD, predpokladame, ze by ted PWD vratilo opet tuto cestu a tudiz
                                        // ho nebudeme posilat (optimalizace snad s dost malym rizikem)
                                        HaveWorkingPath = TRUE;
                                        lstrcpyn(WorkingPath, CurItem->Path, FTP_MAX_PATH);
                                        SubState = fwssWorkSimpleCmdStartWork;
                                        nextLoop = TRUE;
                                    }
                                    else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                                    {
                                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOCWDONLYPATH, NO_ERROR,
                                                               SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                               Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    break;
                                }

                                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                                {
                                    conClosedRetryItem = TRUE;
                                    break;
                                }
                                }
                                break;
                            }

                            case fwssWorkSimpleCmdStartWork: // zacatek prace (pracovni adresar uz je nastaveny)
                            {
                                if (ShouldStop)
                                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                                else
                                {
                                    switch (CurItem->Type)
                                    {
                                    case fqitDeleteFile:        // delete pro soubor (objekt tridy CFTPQueueItemDel)
                                    case fqitDeleteLink:        // delete pro link (objekt tridy CFTPQueueItemDel)
                                    case fqitMoveDeleteDirLink: // smazani linku na adresar po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                                    {
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdDeleteFile, &cmdLen, CurItem->Name); // nemuze nahlasit chybu
                                        sendCmd = TRUE;
                                        SubState = fwssWorkDelFileWaitForDELERes;
                                        break;
                                    }

                                    case fqitDeleteDir:     // delete pro adresar (objekt tridy CFTPQueueItemDir)
                                    case fqitMoveDeleteDir: // smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
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
                                                          ftpcmdDeleteDir, &cmdLen, dirName); // nemuze nahlasit chybu
                                        sendCmd = TRUE;
                                        SubState = fwssWorkDelDirWaitForRMDRes;
                                        break;
                                    }

                                    case fqitChAttrsFile: // zmena atributu souboru (pozn.: u linku se atributy menit nedaji) (objekt tridy CFTPQueueItemChAttr)
                                    case fqitChAttrsDir:  // zmena atributu adresare (objekt tridy CFTPQueueItemChAttrDir)
                                    {
                                        DWORD attr = CurItem->Type == fqitChAttrsFile ? ((CFTPQueueItemChAttr*)CurItem)->Attr : ((CFTPQueueItemChAttrDir*)CurItem)->Attr;
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdChangeAttrs, &cmdLen, attr, CurItem->Name); // nemuze nahlasit chybu
                                        sendCmd = TRUE;
                                        SubState = fwssWorkChAttrWaitForCHMODRes;
                                        break;
                                    }
                                    }
                                }
                                break;
                            }

                            case fwssWorkDelFileWaitForDELERes: // mazani souboru/link: cekame na vysledek "DELE" (smazani souboru/linku)
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                                case fweCmdReplyReceived:
                                {
                                    BOOL finished = TRUE;
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // soubor/link byl uspesne smazan, zapiseme do polozky "uspesne dokonceno"
                                        Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;

                                        // pokud se soubor/link smazal, zmenime listing v cache
                                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                        UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                                        Oper->GetFTPServerPathType(CurItem->Path),
                                                                        CurItem->Name, FALSE);
                                    }
                                    else // nastala nejaka chyba
                                    {
                                        if (CurItem->Type == fqitDeleteFile)
                                        { // vypiseme chybu uzivateli a jdeme zpracovat dalsi polozku fronty
                                            CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETODELETEFILE, NO_ERROR,
                                                                   SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                                   Oper);
                                            lookForNewWork = TRUE;
                                        }
                                        else // zkusime jeste RMD, kdyz DELE nezabralo (hypoteza: link na adresar by se mohl mazat pres RMD)
                                        {    // CurItem->Type je fqitDeleteLink nebo fqitMoveDeleteDirLink
                                            if (ShouldStop)
                                                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
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
                                                                  ftpcmdDeleteDir, &cmdLen, dirName); // nemuze nahlasit chybu
                                                sendCmd = TRUE;
                                                SubState = fwssWorkDelDirWaitForRMDRes;
                                                finished = FALSE;
                                            }
                                        }
                                    }
                                    if (finished) // pro mereni rychlosti pouzivame az posledni prikaz na operaci (po DELE muze nasledovat RMD, to se zde eliminuje)
                                    {
                                        // odpoved serveru prisla -> pridame teoreticke byty do meraku rychlosti
                                        Oper->GetGlobalTransferSpeedMeter()->BytesReceived(SMPLCMD_APPROXBYTESIZE, GetTickCount());
                                    }
                                    break;
                                }

                                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                                {
                                    // pokud nevime jak dopadl vymaz souboru/linku, zneplatnime listing v cache
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

                            case fwssWorkDelDirWaitForRMDRes: // mazani adresare/linku: cekame na vysledek "RMD" (smazani adresare/linku)
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                                case fweCmdReplyReceived:
                                {
                                    // odpoved serveru prisla -> pridame teoreticke byty do meraku rychlosti
                                    Oper->GetGlobalTransferSpeedMeter()->BytesReceived(SMPLCMD_APPROXBYTESIZE, GetTickCount());

                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // adresar/link byl uspesne smazan, zapiseme do polozky "uspesne dokonceno"
                                        Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;

                                        // pokud se adresar/link smazal, zmenime listing v cache
                                        Oper->GetUserHostPort(userBuf, hostBuf, &port);
                                        UploadListingCache.ReportDelete(userBuf, hostBuf, port, CurItem->Path,
                                                                        Oper->GetFTPServerPathType(CurItem->Path),
                                                                        CurItem->Name, FALSE);
                                    }
                                    else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                                    {    // CurItem->Type je fqitDeleteLink / fqitMoveDeleteDirLink nebo fqitDeleteDir / fqitMoveDeleteDir
                                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                        Queue->UpdateItemState(CurItem, sqisFailed,
                                                               (CurItem->Type == fqitDeleteLink || CurItem->Type == fqitMoveDeleteDirLink) ? ITEMPR_UNABLETODELETEFILE : ITEMPR_UNABLETODELETEDIR,
                                                               NO_ERROR,
                                                               SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                               Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    break;
                                }

                                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                                {
                                    // pokud nevime jak dopadl vymaz adresare/linku, zneplatnime listing v cache
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

                            case fwssWorkChAttrWaitForCHMODRes:       // zmena atributu: cekame na vysledek "SITE CHMOD" (zmena modu souboru/adresare, asi jen pro Unix)
                            case fwssWorkChAttrWaitForCHMODQuotedRes: // zmena atributu (jmeno v uvozovkach): cekame na vysledek "SITE CHMOD" (zmena modu souboru/adresare, asi jen pro Unix)
                            {
                                switch (event)
                                {
                                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                                case fweCmdReplyReceived:
                                {
                                    BOOL finished = TRUE;
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                                    { // soubor/adresar ma uspesne zmenene atributy, zapiseme do polozky "uspesne dokonceno"
                                        Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    else // CurItem->Type je fqitChAttrsFile nebo fqitChAttrsDir
                                    {
                                        char* s = CurItem->Name;
                                        while (*s != 0 && *s > ' ')
                                            s++;
                                        if (*s != 0 && SubState == fwssWorkChAttrWaitForCHMODRes) // jmeno souboru/adresare obsahuje white-spaces, zkusime jeste dat jmeno do uvozovek (jen pokud uz jsme to nezkouseli)
                                        {
                                            if (ShouldStop)
                                                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                                            else
                                            {
                                                DWORD attr = CurItem->Type == fqitChAttrsFile ? ((CFTPQueueItemChAttr*)CurItem)->Attr : ((CFTPQueueItemChAttrDir*)CurItem)->Attr;
                                                s = CurItem->Name;
                                                char nameToQuotes[2 * MAX_PATH]; // ve jmene musime doplnit escape-char '\\' pred '"'
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
                                                                  ftpcmdChangeAttrsQuoted, &cmdLen, attr, nameToQuotes); // nemuze nahlasit chybu
                                                sendCmd = TRUE;
                                                SubState = fwssWorkChAttrWaitForCHMODQuotedRes;
                                                finished = FALSE;
                                            }
                                        }
                                        else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                                        {
                                            CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOCHATTRS, NO_ERROR,
                                                                   SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                                   Oper);
                                            lookForNewWork = TRUE;
                                        }
                                    }
                                    if (finished) // pro mereni rychlosti pouzivame az posledni prikaz na operaci (po DELE muze nasledovat RMD, to se zde eliminuje)
                                    {
                                        // odpoved serveru prisla -> pridame teoreticke byty do meraku rychlosti
                                        Oper->GetGlobalTransferSpeedMeter()->BytesReceived(SMPLCMD_APPROXBYTESIZE, GetTickCount());
                                    }
                                    break;
                                }

                                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
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

                case fqitUploadCopyFile: // upload: kopirovani souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
                case fqitUploadMoveFile: // upload: presun souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
                {
                    if (UploadDirGetTgtPathListing) // listujeme cilovou cestu do cache listingu pro upload
                    {
                        BOOL listingNotAccessible;
                        HandleEventInWorkingState2(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, &listingNotAccessible);
                        if (handleShouldStop || conClosedRetryItem || lookForNewWork)
                        { // listovani selhalo, informujeme o tom pripadne cekajici workery
                            UploadDirGetTgtPathListing = FALSE;
                            Oper->GetUserHostPort(userBuf, hostBuf, &port);
                            CFTPServerPathType pathType = Oper->GetFTPServerPathType(((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath);
                            BOOL listingOKErrorIgnored;
                            // volani UploadListingCache.ListingFailed() je mozne jen proto, ze jsme v sekci CSocketsThread::CritSect
                            UploadListingCache.ListingFailed(userBuf, hostBuf, port,
                                                             ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->TgtPath,
                                                             pathType, listingNotAccessible, NULL, &listingOKErrorIgnored);
                            if (listingOKErrorIgnored && lookForNewWork) // na polozce je hlasena chyba listovani, tuto chybu stornujeme
                            {
                                lookForNewWork = FALSE;
                                Queue->UpdateItemState(CurItem, sqisProcessing, ITEMPR_OK, NO_ERROR, NULL, Oper);
                                StatusType = wstNone;
                                SubState = fwssWorkStartWork;
                                Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky (pro pripad, ze uz se chyba stihla ukazat -- temer nemozne)
                                postActivate = TRUE;                  // impulz pro pokracovani v praci
                                reportWorkerChange = TRUE;            // potrebujeme schovat pripadny progres tahani listingu

                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // zrusime pripadny timer z predchozi prace
                            }
                        }
                    }
                    else // ostatni prace jde sem
                    {
                        HandleEventInWorkingState5(event, sendQuitCmd, postActivate, reportWorkerChange, buf, errBuf, host,
                                                   cmdLen, sendCmd, reply, replySize, replyCode, ftpPath, errText,
                                                   conClosedRetryItem, lookForNewWork, handleShouldStop, quitCmdWasSent);
                    }
                    break;
                }

                case fqitUploadMoveDeleteDir: // upload: smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
                {                             // smazani adresare z disku se provede ve stavu fwsPreparing (neni treba connectiona, takze ani stav fwsWorking)
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

        if (ShouldStop && handleShouldStop) // mame ukoncit workera + je mozne poslat prikaz "QUIT"
        {
            if (sendCmd)
                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): can't send QUIT and user command together!");
            if (!SocketClosed)
                sendQuitCmd = TRUE;     // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
            SubState = fwssWorkStopped; // je-li connectiona, tak abychom neposilali "QUIT" vicekrat + pokud connectiona neni, tak aby prace zbytecne nepokracovala
        }
        else
        {
            if (conClosedRetryItem) // rozpadlo se spojeni, ukoncime operaci na polozce a jdeme na dalsi pokus o provedeni polozky
            {
                CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL); // tento prenos se nepovedl, zavreme cilovy soubor (pokud nejaky vubec pouzivame)
                CloseOpenedInFile();                                   // tento prenos se nepovedl, zavreme zdrojovy soubor (pokud nejaky vubec pouzivame)
                if (LockedFileUID != 0)
                {
                    FTPOpenedFiles.CloseFile(LockedFileUID);
                    LockedFileUID = 0;
                }
                if ((CurItem->Type == fqitUploadCopyFile || CurItem->Type == fqitUploadMoveFile) &&
                    ((CFTPQueueItemCopyOrMoveUpload*)CurItem)->RenamedName != NULL)
                {
                    Queue->UpdateRenamedName((CFTPQueueItemCopyOrMoveUpload*)CurItem, NULL);
                    Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                }
                State = fwsPreparing; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                if (quitCmdWasSent)
                    SubState = fwssPrepQuitSent; // predame do fwsPreparing, ze QUIT uz byl poslany
                else
                    SubState = fwssNone;
                postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                reportWorkerChange = TRUE;

                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // zrusime pripadny timer z predchozi prace
            }
            else
            {
                if (lookForNewWork)
                {
                    CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL); // pokud jeste neni zavreny cilovy soubor, znamena to, ze se tento prenos nepovedl, zavreme cilovy soubor (pokud nejaky vubec pouzivame)
                    CloseOpenedInFile();                                   // pokud jeste neni zavreny zdrojovy soubor, znamena to, ze se tento prenos nepovedl, zavreme zdrojovy soubor (pokud nejaky vubec pouzivame)
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
                    Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                    if (CurItem->GetItemState() == sqisProcessing)
                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState(): returned item is still in processing state!");

                    CurItem = NULL;
                    State = fwsLookingForWork; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                    if (quitCmdWasSent)
                        SubState = fwssLookFWQuitSent; // predame do fwsLookingForWork, ze QUIT uz byl poslany
                    else
                        SubState = fwssNone;
                    postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                    reportWorkerChange = TRUE;

                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // zrusime pripadny timer z predchozi prace
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

    BOOL sendQuitCmd = FALSE;  // TRUE = ma se poslat FTP prikaz "QUIT"
    BOOL postActivate = FALSE; // TRUE = ma se postnout WORKER_ACTIVATE (fweActivate)

    int cmdLen = 0;
    BOOL sendCmd = FALSE; // TRUE = ma se poslat FTP prikaz 'buf' o delce 'cmdLen' + do logu dat 'errBuf'

    BOOL operStatusMaybeChanged = FALSE; // TRUE = volat Oper->OperationStatusMaybeChanged();
    BOOL reportWorkerChange = FALSE;     // TRUE = volat Oper->ReportWorkerChange(workerID, FALSE)

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
        if (ShouldStop) // mame ukoncit workera
        {
            if (SubState != fwssLookFWQuitSent && !SocketClosed)
            {
                SubState = fwssLookFWQuitSent; // abychom neposilali "QUIT" vicekrat
                sendQuitCmd = TRUE;            // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
            }
        }
        else
        {
            if (!ShouldBePaused) // pokud nemame delat pausnuty: normalni cinnost
            {
                if (CurItem != NULL)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEvent(): CurItem is not NULL in state fwsLookingForWork!");
                CurItem = Queue->GetNextWaitingItem(Oper); // zkusime najit polozku ve fronte
                if (CurItem != NULL)                       // nasel ve fronte polozku ke zpracovani
                {
                    State = fwsPreparing; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                    SubState = fwssNone;
                    postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                    reportWorkerChange = TRUE;
                    Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                }
                else // nenasel ve fronte zadnou polozku ke zpracovani
                {
                    State = fwsSleeping;
                    operStatusMaybeChanged = TRUE;
                    SubState = fwssNone;
                    // postActivate = TRUE;  // ve fwsSleeping se na fweActivate stejne nic nedeje, nema ho smysl postit
                    reportWorkerChange = TRUE;
                }
            }
        }
        break;
    }

    case fwsSleeping:
    {
        if (ShouldStop) // mame ukoncit workera
        {
            if (SubState != fwssSleepingQuitSent && !SocketClosed)
            {
                SubState = fwssSleepingQuitSent; // abychom neposilali "QUIT" vicekrat
                sendQuitCmd = TRUE;              // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
            }
        }
        else
        {
            if (event == fweWakeUp)
            {
                // buzeni workeru (nova polozka ve fronte nejspis ceka na zpracovani)
                State = fwsLookingForWork;
                operStatusMaybeChanged = TRUE;
                SubState = fwssNone;
                postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                reportWorkerChange = TRUE;

                if (SocketClosed)
                    ConnectAttemptNumber = 0; // probuzenemu workerovi bez connectiony dame sanci na novy connect
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
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
            // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
            SocketsThread->DeleteTimer(UID, WORKER_RECONTIMEOUTTIMID);
        }
        else // normalni cinnost
        {
            if (event == fweReconTimeout || event == fweWorkerShouldResume) // jdeme na dalsi pokus o connect
            {
                State = fwsConnecting; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                SubState = fwssNone;
                postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
                reportWorkerChange = TRUE;
            }
        }
        break;
    }

    case fwsConnectionError:
    {
        CloseOpenedFile(TRUE, FALSE, NULL, NULL, FALSE, NULL); // tento prenos se nepovedl, zavreme cilovy soubor (pokud nejaky vubec pouzivame)
        CloseOpenedInFile();                                   // tento prenos se nepovedl, zavreme zdrojovy soubor (pokud nejaky vubec pouzivame)
        if (CurItem != NULL)
        {
            ReturnCurItemToQueue(); // vratime polozku do fronty
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            Oper->PostNewWorkAvailable(TRUE); // informujeme pripadneho prvniho spiciho workera
            HANDLES(EnterCriticalSection(&WorkerCritSect));
        }
        if (!ShouldStop) // normalni cinnost
        {
            if (event == fweNewLoginParams)
            {
                Logs.LogMessage(LogUID, LoadStr(IDS_WORKERLOGPARCHANGED), -1);
                ErrorDescr[0] = 0; // od teto chvile uz chybu nebudeme hlasit (user se ji snazi opravit)
                if (UnverifiedCertificate != NULL)
                    UnverifiedCertificate->Release();
                UnverifiedCertificate = NULL;
                ConnectAttemptNumber = 0; // aby se povedl connect i pri max. 1 pokusu o connect
                State = fwsLookingForWork;
                operStatusMaybeChanged = TRUE;
                SubState = fwssNone;
                postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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
        // pokud jsme dostali "control connection" z panelu, pokusime se ji vratit (misto jejiho zavreni pres "QUIT")
        int ctrlUID = ControlConnectionUID;
        if (ctrlUID != -1) // dostali jsme connectionu z panelu
        {
            DWORD lastIdle, lastIdle2;
            BOOL isNotBusy = SalamanderGeneral->SalamanderIsNotBusy(&lastIdle);
            if (isNotBusy || GetTickCount() - lastIdle < 2000) // je velka sance, ze se dockame "idle" stavu Salamandera (bude mozne predat connectionu zpet do panelu)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                BOOL isConnected;
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                if (SocketsThread->IsSocketConnected(ctrlUID, &isConnected) &&
                    !isConnected) // objekt socketu "control connectiony" z panelu stale existuje a nema spojeni se serverem
                {
                    // zkusime pockat na "idle" Salamandera cca pet vterin
                    DWORD startTime = GetTickCount();
                    while (1)
                    {
                        if (isNotBusy || startTime - lastIdle < 500 ||                                   // uz je v "idle" nebo jeste pred pul vterinou byl (takze je silne nepravdepodobne, ze je napr. otevreny dialog -> "idle" stav zase brzy nastane)
                            SalamanderGeneral->SalamanderIsNotBusy(&lastIdle2) || lastIdle2 != lastIdle) // je velka sance, ze k prevzeti connectiony dojde hned (jinak ji radsi zavreme pres "QUIT")
                        {
                            if (ReturningConnections.Add(ctrlUID, this))
                            {
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                CanDeleteSocket = FALSE;
                                ReturnToControlCon = TRUE;
                                SalamanderGeneral->PostMenuExtCommand(FTPCMD_RETURNCONNECTION, TRUE); // pockame na "sal-idle"
                                sendQuitCmd = FALSE;                                                  // zkousime predat connectionu, "QUIT" se nekona
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
                    HANDLES(EnterCriticalSection(&WorkerCritSect)); // problemy: pouzijeme "QUIT"
            }
        }

        if (CommandState != fwcsIdle)
            TRACE_E("Incorrect use of send-quit-command in CFTPWorker::HandleEvent(): CommandState is not fwcsIdle before sending new command!");
        int logUID = LogUID;

        if (sendQuitCmd) // nejde o predani socketu z workera do panelu, posilame "QUIT"
        {
            Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGDISCONNECT), -1, TRUE);

            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH, ftpcmdQuit, &cmdLen); // nemuze nahlasit chybu
            sendCmd = TRUE;
        }
        else // probehne predani spojeni, simulujeme zavreni socketu workera (aby se ukoncilo cekani na zavreni socketu pri Stop workeru)
        {
            SocketClosed = TRUE;
            ErrorDescr[0] = 0; // zadna chyba to neni (navic uz by se nikde nemelo zobrazovat)
            CommandReplyTimeout = FALSE;

            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            HandleEvent(fweCmdConClosed, NULL, 0, 0); // oznamime "zavreni" socketu do HandleEvent()
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            ReportWorkerMayBeClosed(); // ohlasime zavreni socketu (pro ostatni cekajici thready)
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
            Logs.LogMessage(logUID, errBuf, -1); // poslany prikaz dame do logu
            // nastavime novy timeout timer
            int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
            if (serverTimeout < 1000)
                serverTimeout = 1000; // aspon sekundu
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                    WORKER_TIMEOUTTIMERID, NULL); // chybu ignorujeme, maximalne si user da Stop
        }
        else
        {
            // nejspis doslo k zavreni socketu (FD_CLOSE dojde casem), pokud doslo k jine chybe,
            // zavreme po 100ms socket tvrde pres CloseSocket() - pockame si jaka z techto dvou
            // variant nastala (zaroven se budeme snazit chytit error hlasku a vypsat ji do logu)
            HANDLES(EnterCriticalSection(&WorkerCritSect));
            CommandState = fwcsWaitForCmdError;
            CommandTransfersData = FALSE;
            WaitForCmdErrError = error; // zapiseme si error z Write(), kdyz nesezeneme nic jineho, dame do logu aspon tento error
            // ReadFTPErrorReplies();  // nelze pouzit (vola SkipFTPReply, cimz preskoci v teto metode zpracovavanou odpoved a po navratu z teto metody dojde k chybe), pokud je socket davno zavreny, nacteme z nej chyby hned ted (zadne udalosti na socketu nenastanou, nic by se nenacetlo)
            HANDLES(LeaveCriticalSection(&WorkerCritSect));

            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            SocketsThread->AddTimer(Msg, UID, GetTickCount() + 100, // dame 0.1 sekundy na pripadne prijeti bytu ze socketu (muze re-postit FD_CLOSE, atd.)
                                    WORKER_CMDERRORTIMERID, NULL);  // chybu ignorujeme, maximalne si user da Stop
        }
    }
    else
        HANDLES(LeaveCriticalSection(&WorkerCritSect));
    if (postActivate)
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
        SocketsThread->PostSocketMessage(Msg, UID, WORKER_ACTIVATE, NULL); // chybu ignorujeme, maximalne si user da Stop
    }
    if (operStatusMaybeChanged)
        Oper->OperationStatusMaybeChanged();
    if (reportWorkerChange)
        Oper->ReportWorkerChange(workerID, FALSE); // pozadame o redraw workera
}
