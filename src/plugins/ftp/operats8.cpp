// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
        // vratime stav ciloveho souboru do puvodniho stavu (krome stavu, kdy dojde k vymazu z duvodu,
        // ze byl soubor pro resume prilis maly a doslo k jeho prepisu)
        CloseOpenedFile(TRUE, FALSE, NULL, NULL,
                        !ResumingOpenedFile,                                  // smazeme cilovy soubor, pokud jsme ho vytvorili (i pokud byl prepsan z duvodu, ze byl pro resume prilis maly)
                        ResumingOpenedFile ? &OpenedFileOriginalSize : NULL); // zarizneme pridane byty z konce souboru pokud jsme ho resumnuli

        int asciiTrModeButBinFile = Oper->GetAsciiTrModeButBinFile();
        switch (asciiTrModeButBinFile)
        {
        case ASCIITRFORBINFILE_USERPROMPT:
        {
            Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_ASCIITRFORBINFILE, NO_ERROR, NULL, Oper);
            break;
        }

        case ASCIITRFORBINFILE_INBINMODE: // bude se tahat znovu, jen v binarnim rezimu
        {
            Queue->UpdateAsciiTransferMode(curItem, FALSE);                                // priste uz v binarnim rezimu
            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
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
        { // Resume or Overwrite: Resume nevysel, provedeme Overwrite
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
            Queue->UpdateForceAction(CurItem, fqiaOverwrite);
            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
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

    // zajistime flush dat z data-connectiony do disk-threadu a po dokonceni flushe zase
    // vratime buffer do data-connectiony (pokud uz je zavrena, buffer jen dealokujeme)
    if (!ShouldStop && WorkerDataCon != NULL && event == fweDataConFlushData ||
        event == fweDiskWorkWriteFinished)
    {
        if (!ShouldStop && WorkerDataCon != NULL && event == fweDataConFlushData)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            char* flushBuffer;
            int validBytesInFlushBuffer;
            BOOL deleteTgtFile;
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            BOOL haveFlushData = WorkerDataCon->GiveFlushData(&flushBuffer, &validBytesInFlushBuffer, &deleteTgtFile);

            HANDLES(EnterCriticalSection(&WorkerCritSect));
            if (deleteTgtFile) // potrebujeme smazat cilovy soubor, protoze mozna obsahuje poskozena data
            {
                // vratime stav ciloveho souboru do puvodniho stavu (krome stavu, kdy dojde k vymazu z duvodu,
                // ze byl soubor pro resume prilis maly a doslo k jeho prepisu)
                CloseOpenedFile(TRUE, FALSE, NULL, NULL,
                                !ResumingOpenedFile,                                  // smazeme cilovy soubor, pokud jsme ho vytvorili (i pokud byl prepsan z duvodu, ze byl pro resume prilis maly)
                                ResumingOpenedFile ? &OpenedFileOriginalSize : NULL); // zarizneme pridane byty z konce souboru pokud jsme ho resumnuli
                // pokud cekame na dokonceni flushnuti dat nebo dokonceni data-connectiony, je potreba
                // postnout fweActivate, aby pokracovalo zpracovani polozky
                if (SubState == fwssWorkCopyWaitForDataConFinish ||
                    SubState == fwssWorkCopyFinishFlushData)
                {
                    postActivate = TRUE;
                }
            }
            else
            {
                if (haveFlushData) // mame 'flushBuffer', musime jej predat do disk-threadu (pri chybe ho uvolnime)
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

                        FlushDataError = fderASCIIForBinaryFile;
                        // pokud cekame na dokonceni flushnuti dat nebo dokonceni data-connectiony, je potreba
                        // postnout fweActivate, aby pokracovalo zpracovani polozky
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
                        else // nelze flushnout data, nelze pokracovat v provadeni polozky
                        {
                            if (DiskWork.FlushDataBuffer != NULL)
                            {
                                free(DiskWork.FlushDataBuffer);
                                DiskWork.FlushDataBuffer = NULL;
                            }

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

                            FlushDataError = fderLowMemory;
                            // pokud cekame na dokonceni flushnuti dat nebo dokonceni data-connectiony, je potreba
                            // postnout fweActivate, aby pokracovalo zpracovani polozky
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
            ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

            // pokud cekame na dokonceni flushnuti dat, je potreba postnout fweActivate, aby
            // pokracovalo zpracovani polozky
            if (SubState == fwssWorkCopyFinishFlushData)
                postActivate = TRUE;

            if (DiskWork.State == sqisNone) // flush dat se podaril
            {
                if (WorkerDataCon != NULL) // pokud data-connection existuje, vratime buffer pro dalsi pouziti
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
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
                        //BOOL dataConDecomprMissingStreamEnd = WorkerDataCon->GetDecomprMissingStreamEnd(); // bohuzel tenhle test je nepruchozi, napr. Serv-U 7 a 8 proste stream neukoncuje
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
                else // pokud jiz data-connection neexistuje, buffer uvolnime
                {
                    if (DiskWork.FlushDataBuffer != NULL)
                        free(DiskWork.FlushDataBuffer);
                }
                DiskWork.FlushDataBuffer = NULL;

                // napocitame novy offset v souboru a velikost souboru
                OpenedFileCurOffset += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0);
                if (OpenedFileCurOffset > OpenedFileSize)
                    OpenedFileSize = OpenedFileCurOffset;
            }
            else // nastala chyba
            {
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }

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

                FlushDataError = fderWriteError;
                // pokud cekame na dokonceni data-connectiony, je potreba postnout fweActivate, aby
                // pokracovalo zpracovani polozky
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
            case fwssWorkSimpleCmdStartWork: // zacatek prace (pracovni adresar uz je nastaveny)
            {
                if (ShouldStop)
                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                else
                {
                    if (curItem->TgtFileState == TGTFILESTATE_TRANSFERRED)
                    { // pokud je soubor jiz preneseny, zbyva jiz jen v pripade Move smazat zdrojovy soubor
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
                        { // soubor na serveru jeste neni otevreny, muzeme s nim pracovat, naalokujeme data-connectionu
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
                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                    DeleteSocket(WorkerDataCon); // bude se jen dealokovat
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

                                // chyba na polozce, zapiseme do ni tento stav
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                lookForNewWork = TRUE;
                            }
                            else // podarila se alokace objektu data-connectiony
                            {
                                WorkerDataConState = wdcsOnlyAllocated;

                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                WorkerDataCon->SetPostMessagesToWorker(TRUE, Msg, UID,
                                                                       WORKER_DATACON_CONNECTED,
                                                                       WORKER_DATACON_CLOSED,
                                                                       WORKER_DATACON_FLUSHDATA,
                                                                       WORKER_DATACON_LISTENINGFORCON);
                                WorkerDataCon->SetGlobalTransferSpeedMeter(Oper->GetGlobalTransferSpeedMeter());
                                WorkerDataCon->SetGlobalLastActivityTime(Oper->GetGlobalLastActivityTime());
                                HANDLES(EnterCriticalSection(&WorkerCritSect));

                                if (Oper->GetUsePassiveMode()) // pasivni mod (PASV)
                                {
                                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                      ftpcmdPassive, &cmdLen); // nemuze nahlasit chybu
                                    sendCmd = TRUE;
                                    SubState = fwssWorkCopyWaitForPASVRes;
                                }
                                else // aktivni mod (PORT)
                                {
                                    nextLoopCopy = TRUE;
                                    SubState = fwssWorkCopyOpenActDataCon;
                                }
                            }
                        }
                        else // nad timto souborem jiz probiha jina operace, at to user zkusi znovu pozdeji
                        {
                            // chyba na polozce, zapiseme do ni tento stav
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_SRCFILEINUSE, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                    }
                }
                break;
            }

            case fwssWorkCopyWaitForPASVRes: // copy/move souboru: cekame na vysledek "PASV" (zjisteni IP+port pro pasivni data-connectionu)
            {
                WaitForPASVRes(event, reply, replySize, replyCode, handleShouldStop, nextLoopCopy, conClosedRetryItem,
                               fwssWorkCopySetType, fwssWorkCopyOpenActDataCon);
                break;
            }

            case fwssWorkCopyOpenActDataCon: // copy/move souboru: otevreme aktivni data-connectionu
            {
                OpenActDataCon(fwssWorkCopyWaitForListen, errBuf, conClosedRetryItem, lookForNewWork);
                break;
            }

            case fwssWorkCopyWaitForListen: // copy/move souboru: cekame na otevreni "listen" portu (otevirame aktivni data-connectionu) - lokalniho nebo na proxy serveru
            {
                WaitForListen(event, handleShouldStop, errBuf, buf, cmdLen, sendCmd,
                              conClosedRetryItem, fwssWorkCopyWaitForPORTRes);
                break;
            }

            case fwssWorkCopyWaitForPORTRes: // copy/move souboru: cekame na vysledek "PORT" (predani IP+port serveru pro aktivni data-connectionu)
            {
                WaitForPORTRes(event, nextLoopCopy, conClosedRetryItem, fwssWorkCopySetType);
                break;
            }

            case fwssWorkCopySetType: // copy/move souboru: nastavime pozadovany prenosovy rezim (ASCII / binary)
            {
                SetTypeA(handleShouldStop, errBuf, buf, cmdLen, sendCmd, nextLoopCopy,
                         (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary),
                         curItem->AsciiTransferMode, fwssWorkCopyWaitForTYPERes, fwssWorkCopyResumeFile);
                break;
            }

            case fwssWorkCopyWaitForTYPERes: // copy/move souboru: cekame na vysledek "TYPE" (prepnuti do ASCII / binary rezimu prenosu dat)
            {
                WaitForTYPERes(event, replyCode, nextLoopCopy, conClosedRetryItem,
                               (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary), fwssWorkCopyResumeFile);
                break;
            }

            case fwssWorkCopyResumeFile: // copy/move souboru: pripadne zajistime resume souboru (poslani prikazu REST)
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

                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                }
                else
                {
                    int resumeOverlap = Config.GetResumeOverlap();
                    int resumeMinFileSize = Config.GetResumeMinFileSize();
                    OpenedFileCurOffset.Set(0, 0);
                    OpenedFileResumedAtOffset.Set(0, 0);
                    if (OpenedFileSize >= CQuadWord(resumeMinFileSize, 0)) // pokud ma Resume smysl (mame cast souboru vetsi nez je minimum pro Resume)
                    {
                        ResumingOpenedFile = TRUE;
                        if (OpenedFileSize > CQuadWord(resumeOverlap, 0)) // REST bude mit kladne cislo v parametru
                        {
                            if (Oper->GetResumeIsNotSupported()) // optimalizace: vime, ze REST selze, ani ho nebudeme posilat
                            {
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMENOTSUP), -1, TRUE);
                                nextLoopCopy = TRUE;
                                SubState = fwssWorkCopyResumeError;
                            }
                            else // posleme REST
                            {
                                OpenedFileCurOffset = OpenedFileSize - CQuadWord(resumeOverlap, 0);
                                char num[50];
                                _ui64toa(OpenedFileCurOffset.Value, num, 10);

                                OpenedFileResumedAtOffset = OpenedFileCurOffset;

                                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                  ftpcmdRestartTransfer, &cmdLen, num); // nemuze nahlasit chybu
                                sendCmd = TRUE;
                                SubState = fwssWorkCopyWaitForResumeRes;
                            }
                        }
                        else // REST je zbytecny, soubor cteme od zacatku, ale Resume delame (existujici cast jen kontrolujeme, pak teprve zapisujeme) -> zacneme se ctenim souboru
                        {
                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMEFROMBEG), -1, TRUE);
                            nextLoopCopy = TRUE;
                            SubState = fwssWorkCopySendRetrCmd;
                        }
                    }
                    else // Overwrite/Create-New nebo je REST zbytecny, soubor cteme od zacatku -> zacneme se ctenim souboru
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

            case fwssWorkCopyWaitForResumeRes: // copy/move souboru: cekame na vysledek "REST" prikazu (resume souboru)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_PARTIALSUCCESS ||
                        FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // vraci se uspech (melo by byt 350, ale akceptujeme i 2xx)
                    {
                        nextLoopCopy = TRUE;
                        SubState = fwssWorkCopySendRetrCmd;
                    }
                    else // 4xx a 5xx
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) // optimalizace: ulozime do operace, ze REST neni podporen
                            Oper->SetResumeIsNotSupported(TRUE);
                        nextLoopCopy = TRUE;
                        SubState = fwssWorkCopyResumeError;
                    }
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
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

                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkCopyResumeError: // copy/move souboru: chyba prikazu "REST" (not implemented, atp.) nebo jiz dopredu vime, ze REST selze
            {
                if (curItem->TgtFileState == TGTFILESTATE_RESUMED) // Overwrite neni mozny, zapiseme do polozky chybu a najdeme si jinou praci
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

                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESUME, NO_ERROR,
                                           NULL, Oper);
                    lookForNewWork = TRUE;
                }
                else // Resume se nevydaril, udelame Overwrite
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

            case fwssWorkCopySendRetrCmd: // copy/move souboru: posleme prikaz RETR (zahajeni cteni souboru, pripadne od offsetu zadaneho pres Resume)
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

                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
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
                                      ftpcmdRetrieveFile, &cmdLen, curItem->Name); // nemuze nahlasit chybu
                    sendCmd = TRUE;

                    postActivate = TRUE;
                    SubState = fwssWorkCopyActivateDataCon;
                }
                break;
            }

            case fwssWorkCopyActivateDataCon: // copy/move souboru: aktivujeme data-connectionu (tesne po poslani prikazu RETR)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    WorkerDataCon->ActivateConnection();
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                SubState = fwssWorkCopyWaitForRETRRes;
                if (event != fweActivate)
                    nextLoopCopy = TRUE; // pokud neslo jen o fweActivate, dorucime udalost do stavu fwssWorkCopyWaitForRETRRes
                break;
            }

            case fwssWorkCopyWaitForRETRRes: // copy/move souboru: cekame na vysledek "RETR" (cekame na konec cteni souboru)
            {
                switch (event)
                {
                case fweCmdInfoReceived: // "1xx" odpovedi obsahuji velikost prenasenych dat
                {
                    if (WorkerDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerDataCon->EncryptPassiveDataCon();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    CQuadWord size;
                    if (!ResumingOpenedFile && // pri resume nektere servery vraci velikost souboru a jine zase velikost zbytku ke stazeni (neni jak poznat, o ktery z techto udaju jde, takze je proste nelze pouzit)
                        FTPGetDataSizeInfoFromSrvReply(size, reply, replySize))
                    {
                        //                if (ResumingOpenedFile && ) // POZOR, NENI VZDY PRAVDA: pri resume neprijde celkova velikost souboru, ale jen resumovane casti -> musime pricist k 'size'
                        //                  size += OpenedFileResumedAtOffset;
                        if (!curItem->SizeInBytes || curItem->Size != size)
                        { // do polozky vepiseme nove zjistenou velikost souboru (kvuli celkovemu progresu + konverze velikosti v blocich/zaznamech/atd na byty)
                            if (!curItem->SizeInBytes)
                                Oper->AddBlkSizeInfo(size, curItem->Size);
                            Queue->UpdateFileSize(curItem, size, TRUE, Oper);
                            Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                        }
                        // mame celkovou velikost souboru - 'size'
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
                    ListCmdReplyText = SalamanderGeneral->DupStr(errText); /* low memory = obejdeme se bez popisu odpovedi */

                    BOOL waitForDataConFinish = FALSE;
                    if (!ShouldStop && WorkerDataCon != NULL)
                    {
                        BOOL trFinished;
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS)
                        { // server vraci chybu ziskavani souboru
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            if (WorkerDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                                WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                        }
                        else
                        {
                            if (WorkerDataCon->IsConnected()) // prenos dat jeste probiha, pockame na dokonceni
                            {
                                waitForDataConFinish = TRUE;
                                if (!WorkerDataCon->IsTransfering(&trFinished) && !trFinished)
                                { // nedoslo k navazani spojeni - pockame 5 sekund, pak pripadne ohlasime chybu (pokud stale nedoslo k navazani spojeni + ListCmdReplyCode je uspech)
                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                                    SocketsThread->DeleteTimer(UID, WORKER_DATACONSTARTTIMID);
                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + 20000,
                                                            WORKER_DATACONSTARTTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop
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
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            if (WorkerDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                                WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                        }
                    }

                    // pokud nemusime cekat na ukonceni "data connection", jdeme ukoncit flushovani dat z "data connection"
                    SubState = waitForDataConFinish ? fwssWorkCopyWaitForDataConFinish : fwssWorkCopyFinishFlushData;
                    if (!waitForDataConFinish)
                        nextLoopCopy = TRUE;
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
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

                    // zpracujeme pripadnou chybu pri flushovani dat
                    if (!HandleFlushDataError(curItem, lookForNewWork))
                    {
                        if (DataConAllDataTransferred && CommandReplyTimeout)
                        { // prijali jsme uspesne vsechna data + server nestihl odpovedet => na 99,9% je soubor downloadly cely a "jen" je zatuhla control-connectiona (deje se pri uploadech/downloadech delsich nez 1,5 hodiny) => forcneme Resume (otestuje velikost souboru + nastavi na nem datum+cas, atd.)
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

            case fwssWorkCopyWaitForDataConFinish: // copy/move souboru: cekame na ukonceni "data connection" (odpoved serveru na "RETR" uz prisla)
            {
                BOOL con = FALSE;
                if (WorkerDataCon != NULL)
                {
                    int retrReply = ListCmdReplyCode;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    if (event == fweDataConStartTimeout) // pokud stale nedoslo k navazani spojeni, nema jiz cenu dele cekat +
                    {                                    // je-li ListCmdReplyCode uspech, retryneme operaci
                        BOOL trFinished;
                        if (WorkerDataCon->IsConnected() &&
                            !WorkerDataCon->IsTransfering(&trFinished) && !trFinished)
                        {                                                 // zavreme "data connection", system se pokusi o "graceful"
                            WorkerDataCon->CloseSocketEx(NULL);           // shutdown (nedozvime se o vysledku)
                            if (FTP_DIGIT_1(retrReply) == FTP_D1_SUCCESS) // retryneme operaci (server sice vratil uspech, ale data-connectiona se ani neotevrela, takze neco neni OK)
                            {
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                WorkerDataCon->FreeFlushData();
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONNOTOPENED), -1, TRUE);
                                // data-connectiona se proste neotevrela (nestihla to nebo doslo k nejake chybe) -> opakujeme pokus o download
                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                lookForNewWork = TRUE;
                                break; // tady neni potreba resit HandleFlushDataError, protoze bez navazani data-connectiony nehrozi zadny FlushDataError!=fderNone
                            }
                        }
                    }
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    con = WorkerDataCon->IsConnected();
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                if (WorkerDataCon == NULL || !con) // bud "data connection" neexistuje nebo uz je uzavrena
                {
                    nextLoopCopy = TRUE;
                    SubState = fwssWorkCopyFinishFlushData;
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_DATACONSTARTTIMID);
                }
                break;
            }

            case fwssWorkCopyFinishFlushData: // copy/move souboru: zajistime dokonceni flushnuti dat z data-connectiony (ta je jiz zavrena)
            {
                BOOL done = !DiskWorkIsUsed; // TRUE jen pokud prave neprobiha flush dat
                if (done && WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    done = WorkerDataCon->AreAllDataFlushed(FALSE);
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                if (done) // vse potrebne je jiz zapsano na disku
                {
                    SubState = fwssWorkCopyProcessRETRRes;
                    nextLoopCopy = TRUE;
                }
                else
                {
                    if (event == fweWorkerShouldStop && ShouldStop)
                    {                                                        // zavreni control-connectiony behem cekani na dokonceni flushnuti dat na disk
                        SubState = fwssWorkCopyFinishFlushDataAfterQuitSent; // abychom neposilali "QUIT" vicekrat
                        sendQuitCmd = TRUE;                                  // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
                    }
                }
                break;
            }

            case fwssWorkCopyFinishFlushDataAfterQuitSent:
                break; // copy/move souboru: po poslani "QUIT" cekame na zavreni control-connectiony + cekame na dokonceni flushnuti dat na disk

            case fwssWorkCopyProcessRETRRes: // copy/move souboru: zpracovani vysledku "RETR" (az po ukonceni "data connection", flushnuti dat na disk a zaroven prijeti odpovedi serveru na "RETR")
            {
                // vsechna data jsou flushnuta (data-connectiona neobsahuje zadna data + je zavrena; DiskWorkIsUsed==FALSE)
                // vysledek prikazu "RETR" je v 'ListCmdReplyCode' a 'ListCmdReplyText'

                // zpracujeme pripadnou chybu pri flushovani dat
                if (!HandleFlushDataError(curItem, lookForNewWork))
                {
                    // dealokujeme objekt data-connectiony, uz ho nebudeme potrebovat (je prazdny)
                    BOOL dataConExisted = WorkerDataCon != NULL;
                    DWORD dataConError = 0;
                    BOOL dataConLowMem = FALSE;
                    BOOL dataConNoDataTransTimeout = FALSE;
                    int dataSSLErrorOccured = SSLCONERR_NOERROR;
                    BOOL dataConDecomprErrorOccured = FALSE;
                    //BOOL dataConDecomprMissingStreamEnd = FALSE;  // bohuzel tenhle test je nepruchozi, napr. Serv-U 7 a 8 proste stream neukoncuje
                    errBuf[0] = 0;
                    if (dataConExisted)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        WorkerDataCon->GetError(&dataConError, &dataConLowMem, NULL, &dataConNoDataTransTimeout,
                                                &dataSSLErrorOccured, &dataConDecomprErrorOccured);
                        //dataConDecomprMissingStreamEnd = WorkerDataCon->GetDecomprMissingStreamEnd();
                        if (!WorkerDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                            errBuf[0] = 0;
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        // data-connection by mela byt zavrena, takze zavirat ji je nejspis zbytecne, ale sychrujeme to...
                        if (WorkerDataCon->IsConnected()) // zavreme "data connection", system se pokusi o "graceful"
                        {
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState3(): data connection has left opened!");
                        }
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        if (dataConExisted) // "always true"
                        {
                            if (dataConLowMem) // "data connection" hlasi nedostatek pameti ("always false")
                            {
                                // chyba na polozce, zapiseme do ni tento stav
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                lookForNewWork = TRUE;
                            }
                            else
                            {
                                if (dataConError != NO_ERROR && !IsConnected())
                                { // odpoved na RETR sice prisla, ale pak pri cekani na dokonceni prenosu pres data-connection
                                    // doslo k preruseni spojeni (data-connectiona i control-connectiona) -> RETRY
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
                                            if (IsConnected()) // "rucne" zavreme control-connectionu
                                            {
                                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                                ForceClose(); // cistci by bylo poslat QUIT, ale zmena certifikatu je hodne nepravdepodobna, tedy je skoda se s tim prgat ciste ;-)
                                            }
                                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                                            conClosedRetryItem = TRUE;
                                        }
                                        else
                                        {
                                            if (!dataConDecomprErrorOccured &&
                                                (FTP_DIGIT_1(ListCmdReplyCode) == FTP_D1_TRANSIENTERROR &&
                                                     (FTP_DIGIT_2(ListCmdReplyCode) == FTP_D2_CONNECTION ||  // hlavne "426 data connection closed, transfer aborted" (je-li to adminem serveru nebo poruchou spojeni nejsem sto rozpoznat, takze prioritu dostava porucha spojeni -> opakujeme pokus o download)
                                                      FTP_DIGIT_2(ListCmdReplyCode) == FTP_D2_FILESYSTEM) && // "450 Transfer aborted.  Link to file server lost."
                                                     dataSSLErrorOccured != SSLCONERR_DONOTRETRY ||          // 426 a 450 bereme jen pokud nebyly vyvolane chybou: nepodarilo se zasifrovat spojeni, jde o permanentni problem
                                                 dataConNoDataTransTimeout ||                                // nami prerusene spojeni kvuli no-data-transfer timeoutu (deje se pri "50%" vypadcich site, data-connectiona se neprerusi, ale nejak se zablokuje prenos dat, vydrzi otevrena klidne 14000 sekund, tohle by to melo resit) -> opakujeme pokus o download
                                                 dataSSLErrorOccured == SSLCONERR_CANRETRY))                 // nepodarilo se zasifrovat spojeni, nejde o permanentni problem
                                            {
                                                SubState = fwssWorkCopyDelayedAutoRetry; // pouzijeme zpozdeny auto-retry, aby stihly prijit vsechny necekane odpovedi ze serveru
                                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                                                SocketsThread->AddTimer(Msg, UID, GetTickCount() + WORKER_DELAYEDAUTORETRYTIMEOUT,
                                                                        WORKER_DELAYEDAUTORETRYTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop
                                            }
                                            else
                                            {
                                                if (!dataConDecomprErrorOccured &&
                                                    FTP_DIGIT_1(ListCmdReplyCode) == FTP_D1_SUCCESS && dataConError != NO_ERROR)   // datove spojeni prerusene po prijmu "uspesne" odpovedi ze serveru (chyba pri cekani na dokonceni prenosu dat), tohle se delo na ftp.simtel.net (sest spojeni najednou + vypadky packetu) + obcas pri downloadu PASV+SSL z lokalniho Filezilla serveru
                                                {                                                                                  // pouzijeme okamzity auto-retry, jinak degraduje rychlost + zadne necekane odpovedi ze serveru necekame
                                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                                }
                                                else
                                                {
                                                    if (dataSSLErrorOccured != SSLCONERR_NOERROR || dataConDecomprErrorOccured)
                                                        lstrcpyn(errText, LoadStr(dataConDecomprErrorOccured ? IDS_ERRDATACONDECOMPRERROR : IDS_ERRDATACONSSLCONNECTERROR), 200 + FTP_MAX_PATH);
                                                    else
                                                    {
                                                        errText[0] = 0;
                                                        if (FTP_DIGIT_1(ListCmdReplyCode) != FTP_D1_SUCCESS && ListCmdReplyText != NULL)
                                                        { // pokud nemame popis sitove chyby ze serveru, spokojime se se systemovym popisem
                                                            lstrcpyn(errText, ListCmdReplyText, 200 + FTP_MAX_PATH);
                                                        }

                                                        if (errText[0] == 0 && errBuf[0] != 0) // zkusime jeste vzit text chyby z proxy serveru
                                                            lstrcpyn(errText, errBuf, 200 + FTP_MAX_PATH);

                                                        //                              if (errText[0] == 0 && dataConDecomprMissingStreamEnd)
                                                        //                                lstrcpyn(errText, LoadStr(IDS_ERRDATACONDECOMPRERROR), 200 + FTP_MAX_PATH);
                                                    }

                                                    // chyba na polozce, zapiseme do ni tento stav
                                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INCOMPLETEDOWNLOAD, dataConError,
                                                                           (errText[0] != 0 ? SalamanderGeneral->DupStr(errText) : NULL), Oper);
                                                }
                                                lookForNewWork = TRUE;
                                            }
                                        }
                                    }
                                    else // download se povedl a soubor je kompletni - jde-li o Move, jeste smazeme zdrojovy soubor
                                    {
                                        if (ResumingOpenedFile && OpenedFileCurOffset < OpenedFileSize)
                                        { // neotestovali jsme cely blok na konci souboru (na serveru je kratsi soubor nez na disku -> kazdopadne se soubory lisi a resume neni mozny)
                                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME2), -1, TRUE);
                                            if (curItem->TgtFileState == TGTFILESTATE_RESUMED) // Overwrite neni mozny, zapiseme do polozky chybu a jdeme hledat jinou praci
                                            {
                                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_RESUMETESTFAILED, NO_ERROR, NULL, Oper);
                                                lookForNewWork = TRUE;
                                            }
                                            else // Resume or Overwrite: Resume nevysel, provedeme Overwrite
                                            {
                                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                                Queue->UpdateForceAction(CurItem, fqiaOverwrite);
                                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                                lookForNewWork = TRUE;
                                            }
                                        }
                                        else
                                        {
                                            CQuadWord size = OpenedFileSize; // zaloha velikosti souboru, po CloseOpenedFile se velikost nuluje

                                            // prenos uspesne ukoncen, zavreme soubor s transferAborted==FALSE
                                            CloseOpenedFile(FALSE, curItem->DateAndTimeValid, &curItem->Date, &curItem->Time, FALSE, NULL);

                                            // oznacime soubor jako jiz preneseny (pro pripad chyby mazani zdrojoveho souboru u Move musime tuto situaci rozlisit)
                                            Queue->UpdateTgtFileState(curItem, TGTFILESTATE_TRANSFERRED);

                                            // jde-li o velikost v blocich, pridame prislusnou dvojici velikosti pro vypocet prum. velikosti bloku
                                            if (!curItem->SizeInBytes)
                                                Oper->AddBlkSizeInfo(size, curItem->Size);

                                            // do polozky vepiseme skutecnou velikost souboru (kvuli celkovemu progresu + konverze velikosti v blocich/zaznamech/atd na byty)
                                            if (!curItem->SizeInBytes || curItem->Size != size)
                                            {
                                                Queue->UpdateFileSize(curItem, size, TRUE, Oper);
                                                Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
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

            case fwssWorkCopyDelayedAutoRetry: // copy/move souboru: cekame WORKER_DELAYEDAUTORETRYTIMEOUT milisekund na auto-retry (aby mohly prijit vsechny neocekavane odpovedi ze serveru)
            {
                if (event == fweDelayedAutoRetry) // uz mame provest auto-retry
                {
                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkCopyTransferFinished: // copy/move souboru: soubor je prenesen, v pripade Move smazeme zdrojovy soubor
            {
                if (CurItem->Type == fqitMoveFileOrFileLink)
                {
                    if (LockedFileUID != 0) // je-li soubor otevreny pro cteni, uz ho muzeme zavrit
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
                    { // soubor na serveru jeste neni otevreny, muzeme ho zkusit smazat
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdDeleteFile, &cmdLen, CurItem->Name); // nemuze nahlasit chybu
                        sendCmd = TRUE;
                        SubState = fwssWorkCopyMoveWaitForDELERes;
                    }
                    else // nad timto souborem jiz probiha jina operace, at to user zkusi znovu pozdeji
                    {
                        // chyba na polozce, zapiseme do ni tento stav
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_SRCFILEINUSE, NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                }
                else // Copy - neni co dal resit, hotovo
                {
                    SubState = fwssWorkCopyDone;
                    nextLoopCopy = TRUE;
                }
                break;
            }

            case fwssWorkCopyMoveWaitForDELERes: // copy/move souboru: cekame na vysledek "DELE" (Move: smazani zdrojoveho souboru/linku po dokonceni prenosu souboru)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    { // zdrojovy soubor/link byl uspesne smazan, tim je hotovo
                        SubState = fwssWorkCopyDone;
                        nextLoopCopy = TRUE;

                        // pokud se soubor/link smazal, zmenime listing v cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, CurItem->Path,
                                                        Oper->GetFTPServerPathType(CurItem->Path),
                                                        CurItem->Name, FALSE);
                    }
                    else // vypiseme chybu uzivateli a jdeme zpracovat dalsi polozku fronty
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETODELSRCFILE, NO_ERROR,
                                               SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                               Oper);
                        lookForNewWork = TRUE;
                    }
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    // pokud nevime jak dopadl vymaz souboru/linku, zneplatnime listing v cache
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

            case fwssWorkCopyDone: // copy/move souboru: hotovo, zavreme soubor a jdeme na dalsi polozku
            {
                // polozka byla uspesne dokoncena, zapiseme do ni tento stav
                Queue->UpdateItemState(CurItem, sqisDone, ITEMPR_OK, NO_ERROR, NULL, Oper);
                lookForNewWork = TRUE;
                break;
            }
            }
            if (!nextLoopCopy)
                break;
        }

        // pred zavrenim souboru je nutne zcancelovat disk-work (aby se nepracovalo se
        // zavrenym handlem souboru)
        if (DiskWorkIsUsed && (conClosedRetryItem || lookForNewWork))
        {
            BOOL workIsInProgress;
            if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
            {
                if (workIsInProgress)
                    DiskWork.FlushDataBuffer = NULL; // prace je rozdelana, nemuzeme uvolnit buffer se zapisovanymi/testovanymi daty, nechame to na disk-work threadu (viz cast cancelovani prace) - do DiskWork muzeme zapisovat, protoze po Cancelu do nej uz disk-thread nesmi pristupovat (napr. uz vubec nemusi existovat)
            }
            // pokud jsme praci zcancelovali jeste pred jejim zapocetim, musime uvolnit flush buffer +
            // pokud je jiz prace hotova, uvolnime flush buffer zde, protoze fweDiskWorkWriteFinished
            // uz prijde jinam (kde se vyignoruje)
            if (DiskWork.FlushDataBuffer != NULL)
            {
                free(DiskWork.FlushDataBuffer);
                DiskWork.FlushDataBuffer = NULL;
            }

            DiskWorkIsUsed = FALSE;
            ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)
        }
    }
}
