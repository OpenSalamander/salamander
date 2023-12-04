// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

        case ASCIITRFORBINFILE_INBINMODE: // bude se tahat znovu, jen v binarnim rezimu
        {
            Queue->UpdateAsciiTransferMode(curItem, FALSE);                                // priste uz v binarnim rezimu
            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
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
    CUploadListingItem* existingItem = NULL; // pro predavani dat o polozce listingu mezi ruznymi SubState

    if (!ShouldStop && WorkerUploadDataCon != NULL && event == fweUplDataConPrepareData ||
        event == fweDiskWorkReadFinished)
    {
        if (!ShouldStop && WorkerUploadDataCon != NULL && event == fweUplDataConPrepareData)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            char* flushBuffer;
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            BOOL haveBufferForData = WorkerUploadDataCon->GiveBufferForData(&flushBuffer);
            HANDLES(EnterCriticalSection(&WorkerCritSect));

            if (haveBufferForData) // mame 'flushBuffer', musime jej predat do disk-threadu, kde se naplni daty ze souboru (pri chybe ho uvolnime)
            {
                if (DiskWorkIsUsed)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): DiskWorkIsUsed may not be TRUE here!");
                InitDiskWork(WORKER_DISKWORKREADFINISHED, curItem->AsciiTransferMode ? fdwtReadFileInASCII : fdwtReadFile,
                             NULL, NULL, fqiaNone, FALSE, flushBuffer, NULL, &OpenedInFileCurOffset, 0, OpenedInFile);
                if (FTPDiskThread->AddWork(&DiskWork))
                    DiskWorkIsUsed = TRUE;
                else // nelze pripravit data, nelze pokracovat v provadeni polozky
                {
                    if (DiskWork.FlushDataBuffer != NULL)
                    {
                        free(DiskWork.FlushDataBuffer);
                        DiskWork.FlushDataBuffer = NULL;
                    }

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

                    PrepareDataError = pderLowMemory;
                }
            }
        }
        else // event == fweDiskWorkReadFinished
        {
            DiskWorkIsUsed = FALSE;
            ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

            if (DiskWork.State == sqisNone) // nacteni dat do bufferu se podarilo
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
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                        WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                    WorkerUploadDataCon->FreeBufferedData();
                    DeleteSocket(WorkerUploadDataCon);
                    WorkerUploadDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;

                    PrepareDataError = pderASCIIForBinaryFile;
                }
                else
                {
                    if (WorkerUploadDataCon != NULL) // pokud data-connection existuje, predame do ni buffer pro zapis do data-connectiony
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        WorkerUploadDataCon->DataBufferPrepared(DiskWork.FlushDataBuffer, DiskWork.ValidBytesInFlushDataBuffer, TRUE);
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    else // pokud jiz data-connection neexistuje, buffer uvolnime
                    {
                        if (DiskWork.FlushDataBuffer != NULL)
                            free(DiskWork.FlushDataBuffer);
                    }
                    DiskWork.FlushDataBuffer = NULL;

                    // prevezmeme novy offset v souboru (napocist nelze, protoze u textovych souboru bude dochazet k prevodu vsech LF na CRLF)
                    OpenedInFileCurOffset = DiskWork.WriteOrReadFromOffset;
                    if (OpenedInFileCurOffset > OpenedInFileSize)
                        OpenedInFileSize = OpenedInFileCurOffset;
                    OpenedInFileNumberOfEOLs += CQuadWord(DiskWork.EOLsInFlushDataBuffer, 0);            // nenulove jen pro textove soubory
                    OpenedInFileSizeWithCRLF_EOLs += CQuadWord(DiskWork.ValidBytesInFlushDataBuffer, 0); // ma smysl jen pro textove soubory
                }
            }
            else // nastala chyba
            {
                if (DiskWork.FlushDataBuffer != NULL)
                {
                    free(DiskWork.FlushDataBuffer);
                    DiskWork.FlushDataBuffer = NULL;
                }

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
            case fwssWorkStartWork: // zjistime v jakem stavu je cilovy adresar
            {
                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGUPLOADFILE), curItem->Name);
                Logs.LogMessage(LogUID, errText, -1, TRUE);

                if (ShouldStop)
                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
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
                            if (listingInProgress) // listovani prave probiha nebo ma ted probehnout
                            {
                                if (getListing) // mame ziskat listing, a pak informovat pripadne ostatni cekajici workery
                                {
                                    UploadDirGetTgtPathListing = TRUE;
                                    postActivate = TRUE; // postneme si impulz pro zacatek stahovani listingu
                                }
                                else
                                {
                                    SubState = fwssWorkUploadWaitForListing; // mame cekat az jiny worker dokonci listovani
                                    reportWorkerChange = TRUE;               // worker vypisuje stav fwssWorkUploadWaitForListing do okna, takze je potreba prekreslit
                                }
                            }
                            else // listing je v cache hotovy nebo "neziskatelny"
                            {
                                if (notAccessible) // listing je cachovany, ale jen jako "neziskatelny"
                                {
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCANNOTLISTTGTPATH, 0, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                else // mame listing k dispozici, zjistime pripadnou kolizi jmena souboru
                                {
                                    nextLoop = TRUE;
                                    if (CurItem->ForceAction == fqiaUploadContinueAutorename) // pokracovani auto-rename (zkusime dalsi jmeno + dalsi STOR)
                                    {
                                        UploadType = utAutorename;
                                        if (curItem->RenamedName != NULL)
                                            TRACE_E("Unexpected situation 2 in CFTPWorker::HandleEventInWorkingState5(): curItem->RenamedName != NULL");
                                        UploadAutorenameNewName[0] = 0;
                                        SubState = fwssWorkUploadFileSetTgtPath;
                                    }
                                    else
                                    {
                                        if (CurItem->ForceAction == fqiaUploadForceAutorename) // z predchoziho zpracovani teto polozky vime, ze je nutne pouzit autorename
                                            SubState = fwssWorkUploadAutorenameFile;
                                        else
                                        {
                                            BOOL nameValid = FTPMayBeValidNameComponent(curItem->TgtName, curItem->TgtPath, FALSE, pathType);
                                            if (existingItem == NULL && nameValid) // bez kolize a validni jmeno -> zkusime vytvorit adresar
                                                SubState = fwssWorkUploadNewFile;
                                            else
                                            {                                                              // je-li existingItem == NULL, je (!nameValid==TRUE), proto nejsou treba testy na existingItem != NULL
                                                if (!nameValid || existingItem->ItemType == ulitDirectory) // invalidni jmeno nebo kolize s adresarem -> "file cannot be created"
                                                    SubState = !nameValid ? fwssWorkUploadCantCreateFileInvName : fwssWorkUploadCantCreateFileDirEx;
                                                else
                                                {
                                                    if (existingItem->ItemType == ulitFile) // kolize se souborem -> "file already exists"
                                                        SubState = fwssWorkUploadFileExists;
                                                    else // (existingItem->ItemType == ulitLink): kolize s linkem -> zjistime co je link zac (soubor/adresar)
                                                        SubState = fwssWorkUploadResolveLink;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else // nedostatek pameti
                        {
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                    }
                    else // cilovy soubor je jiz uploadly, jestli jde o Move, zkusime jeste smaznout zdrojovy soubor na disku
                    {
                        SubState = fwssWorkUploadCopyTransferFinished;
                        nextLoop = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForListing: // upload copy/move souboru: cekame az jiny worker dokonci listovani cilove cesty na serveru (pro zjisteni kolizi)
            {
                if (ShouldStop)
                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                else
                {
                    if (event == fweTgtPathListingFinished) // povereny worker jiz svou praci dokoncil, zkusime novy listing pouzit
                    {
                        SubState = fwssWorkStartWork;
                        reportWorkerChange = TRUE; // worker vypisuje stav fwssWorkUploadWaitForListing do okna, takze je potreba prekreslit
                        nextLoop = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadResolveLink: // upload copy/move souboru: zjistime co je link (soubor/adresar), jehoz jmeno koliduje se jmenem ciloveho souboru na serveru
            {
                lstrcpyn(ftpPath, curItem->TgtPath, FTP_MAX_PATH);
                CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
                if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, curItem->TgtName, TRUE))
                { // mame cestu, posleme na server CWD do zkoumaneho adresare
                    _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGRESOLVINGLINK), ftpPath);
                    Logs.LogMessage(LogUID, errText, -1, TRUE);

                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // nemuze nahlasit chybu
                    sendCmd = TRUE;
                    SubState = fwssWorkUploadResLnkWaitForCWDRes;

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

            case fwssWorkUploadResLnkWaitForCWDRes: // upload copy/move souboru: cekame na vysledek "CWD" (zmena cesty do zkoumaneho linku - podari-li se, je to link na adresar)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||
                        FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)
                    {
                        if (ShouldStop)
                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        else
                        {
                            nextLoop = TRUE;
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // uspech, link vede do adresare
                                SubState = fwssWorkUploadCantCreateFileDirEx;
                            else // permanentni chyba, link vede nejspis na soubor (ale muze jit i o "550 Permission denied", bohuzel 550 je i "550 Not a directory", takze nerozlisitelne...)
                                SubState = fwssWorkUploadFileExists;
                        }
                    }
                    else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESOLVELNK, NO_ERROR,
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

            case fwssWorkUploadCantCreateFileInvName: // upload copy/move souboru: resime chybu "target file cannot be created" (invalid name)
            case fwssWorkUploadCantCreateFileDirEx:   // upload copy/move souboru: resime chybu "target file cannot be created" (name already used for directory or link to directory)
            {
                if (CurItem->ForceAction == fqiaUseAutorename) // forcnuty autorename
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

            case fwssWorkUploadFileExists: // upload copy/move souboru: resime chybu "target file already exists"
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

                default: // zadny force: zjistime jake ma byt standardni chovani
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

            case fwssWorkUploadNewFile:               // upload copy/move souboru: cilovy soubor neexistuje, jdeme ho uploadnout
            case fwssWorkUploadAutorenameFile:        // upload copy/move souboru: reseni chyby vytvareni ciloveho souboru - autorename
            case fwssWorkUploadResumeFile:            // upload copy/move souboru: problem "cilovy soubor existuje" - resume
            case fwssWorkUploadResumeOrOverwriteFile: // upload copy/move souboru: problem "cilovy soubor existuje" - resume or overwrite
            case fwssWorkUploadOverwriteFile:         // upload copy/move souboru: problem "cilovy soubor existuje" - overwrite
            case fwssWorkUploadTestIfFinished:        // upload copy/move souboru: poslali jsme cely soubor + server "jen" neodpovedel, nejspis je soubor OK, otestujeme to
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
                    if (curItem->AsciiTransferMode) // resume v ASCII transfer mode neumime (prilis chaoticke implementace serveru (CRLF konverze), bez zpetne kontroly radsi nepsat)
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADASCIIRESUMENOTSUP,
                                               NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                    else // binarni rezim, jdeme provest APPEND (resume)
                    {
                        if (Oper->GetResumeIsNotSupported()) // APPE neni na tomto serveru implementovano
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
                    if (Oper->GetResumeIsNotSupported() || // APPE neni na tomto serveru implementovano
                        curItem->AsciiTransferMode)        // resume v ASCII transfer mode neumime (prilis chaoticke implementace serveru (CRLF konverze), bez zpetne kontroly radsi nepsat),
                    {                                      // takze udelame rovnou overwrite
                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLRESUMENOTSUP), -1, TRUE);
                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                        UploadType = utOverwriteFile;
                        SubState = fwssWorkUploadFileSetTgtPath;
                        nextLoop = TRUE;
                    }
                    else // binarni rezim, jdeme provest APPEND (resume), pokud se nepovede, zkusime jeste overwrite
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

            case fwssWorkUploadFileSetTgtPath: // upload souboru: nastaveni cilove cesty
            {
                if (!HaveWorkingPath || strcmp(WorkingPath, curItem->TgtPath) != 0)
                { // je treba zmenit pracovni cestu (predpoklad: server vraci stale stejny retezec cesty - ten
                    // se do polozky dostal pri explore-dir nebo z panelu, v obou pripadech slo o cestu vracenou
                    // serverem na prikaz PWD)
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdChangeWorkingPath, &cmdLen, curItem->TgtPath); // nemuze nahlasit chybu
                    sendCmd = TRUE;
                    SubState = fwssWorkUploadFileSetTgtPathWaitForCWDRes;

                    HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
                }
                else // pracovni adresar uz je nastaveny
                {
                    SubState = fwssWorkUploadSetType;
                    nextLoop = TRUE;
                }
                break;
            }

            case fwssWorkUploadFileSetTgtPathWaitForCWDRes: // upload souboru: cekame na vysledek "CWD" (nastaveni cilove cesty)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // cilova cesta je nastavena, zahajime generovani jmen ciloveho adresare
                    {                                             // uspesne jsme zmenili pracovni cestu, diky tomu, ze tato cesta byla "kdysi" vracena
                        // ze serveru na prikaz PWD, predpokladame, ze by ted PWD vratilo opet tuto cestu a tudiz
                        // ho nebudeme posilat (optimalizace snad s dost malym rizikem)
                        HaveWorkingPath = TRUE;
                        lstrcpyn(WorkingPath, curItem->TgtPath, FTP_MAX_PATH);

                        if (ShouldStop)
                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        else
                        {
                            SubState = fwssWorkUploadSetType;
                            nextLoop = TRUE;
                        }
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

            case fwssWorkUploadSetType: // upload copy/move souboru: nastavime pozadovany prenosovy rezim (ASCII / binary)
            {
                if (ShouldStop)
                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                else
                {
                    if (CurrentTransferMode != (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary)) // pokud jiz neni nastaveny, nastavime pozadovany rezim
                    {
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdSetTransferMode, &cmdLen, curItem->AsciiTransferMode); // nemuze nahlasit chybu
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadWaitForTYPERes;
                    }
                    else // pozadovany rezim jiz je nastaveny
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

            case fwssWorkUploadWaitForTYPERes: // upload copy/move souboru: cekame na vysledek "TYPE" (prepnuti do ASCII / binary rezimu prenosu dat)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)                                    // vraci se uspech (melo by byt 200)
                            CurrentTransferMode = (curItem->AsciiTransferMode ? ctrmASCII : ctrmBinary); // prenosovy rezim byl zmenen
                        else
                            CurrentTransferMode = ctrmUnknown; // neznama chyba, nemusi vubec vadit, ale nebudeme "cachovat" rezim prenosu dat

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

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadGetFileSize: // upload souboru: resume: zjisteni velikosti souboru (pres prikaz SIZE nebo z listingu)
            {
                if (Oper->GetSizeCmdIsSupported())
                {
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdGetSize, &cmdLen, curItem->TgtName); // nemuze nahlasit chybu
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

            case fwssWorkUploadWaitForSIZERes: // upload souboru: resume: cekani na odpoved na prikaz SIZE
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // vraci se uspech (melo by byt 213)
                    {
                        if (ShouldStop)
                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
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
                            if (num == end) // odpoved je OK (mame velikost souboru v 'size')
                            {
                                if (UploadType == utOnlyTestFileSize)
                                {
                                    CQuadWord qwSize;
                                    qwSize.SetUI64(size);
                                    if (curItem->AsciiTransferMode)
                                    {
                                        if (qwSize == curItem->SizeWithCRLF_EOLs ||                       // velikost s CRLF
                                            qwSize == curItem->SizeWithCRLF_EOLs - curItem->NumberOfEOLs) // velikost s LF (nebo CR)
                                        {
                                            SubState = fwssWorkUploadTestFileSizeOK; // velikost je OK => prohlasime upload za uspesny
                                        }
                                        else
                                            SubState = fwssWorkUploadTestFileSizeFailed; // jina velikost, provedeme Retry (resime problem "transfer failed"...)
                                    }
                                    else
                                    {
                                        if (qwSize == OpenedInFileSize)
                                            SubState = fwssWorkUploadTestFileSizeOK; // velikost je OK => prohlasime upload za uspesny
                                        else
                                            SubState = fwssWorkUploadTestFileSizeFailed; // jina velikost, provedeme Retry (resime problem "transfer failed"...)
                                    }
                                    nextLoop = TRUE;
                                }
                                else
                                {
                                    if (CQuadWord().SetUI64(size) > OpenedInFileSize)
                                    { // cilovy soubor je vetsi nez zdrojovy, resume nelze provest
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME2), -1, TRUE);
                                        if (UploadType == utResumeOrOverwriteFile) // provedeme overwrite
                                        {
                                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                            UploadType = utOverwriteFile;
                                            SubState = fwssWorkUploadLockFile;
                                            nextLoop = TRUE;
                                        }
                                        else // ohlasime chybu (resume neni mozne provest)
                                        {
                                            // chyba na polozce, zapiseme do ni tento stav
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADUNABLETORESUMEBIGTGT, NO_ERROR, NULL, Oper);
                                            lookForNewWork = TRUE;
                                        }
                                    }
                                    else
                                    {
                                        int resumeMinFileSize = Config.GetResumeMinFileSize();
                                        if ((unsigned __int64)resumeMinFileSize <= size) // pokud ma Resume smysl (mame cast souboru vetsi nez je minimum pro Resume)
                                        {
                                            ResumingFileOnServer = TRUE;
                                            OpenedInFileCurOffset.SetUI64(size);
                                            // POZNAMKA: neumime Resume uploadu textovych souboru, jinak by tady musela byt inicializace OpenedInFileNumberOfEOLs a OpenedInFileSizeWithCRLF_EOLs podle poctu EOLu v jiz uploadnute casti souboru
                                            FileOnServerResumedAtOffset = OpenedInFileCurOffset;
                                        }
                                        else // provedeme overwrite, protoze cilovy soubor je prilis maly pro resume
                                        {
                                            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMEUSELESS), -1, TRUE);
                                            UploadType = utOverwriteFile;
                                        }
                                        nextLoop = TRUE;
                                        SubState = fwssWorkUploadLockFile;
                                    }
                                }
                            }
                            else // neocekavana odpoved na prikaz SIZE
                            {
                                nextLoop = TRUE;
                                SubState = fwssWorkUploadGetFileSizeFromListing;
                            }
                        }
                    }
                    else
                    {
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_ERROR && FTP_DIGIT_2(replyCode) == FTP_D2_SYNTAX)
                        { // SIZE neni implementovano (na NETWARE hlasi tez pri poslani SIZE pro adresar, ale to prozatim ignorujeme)
                            Oper->SetSizeCmdIsSupported(FALSE);
                        }
                        if (ShouldStop)
                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        else
                        {
                            nextLoop = TRUE;
                            SubState = fwssWorkUploadGetFileSizeFromListing;
                        }
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

            case fwssWorkUploadGetFileSizeFromListing: // upload souboru: resume: prikaz SIZE selhal (nebo neni implementovan), zjistime velikost souboru z listingu
            {
                if (existingItem == NULL) // pokud to sem neproslo primo, budeme muset znovu vytahnout udaje o cilovem souboru z listingu
                {
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                    BOOL notAccessible, getListing, listingInProgress;
                    if (UploadListingCache.GetListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                      pathType, Msg, UID, &listingInProgress,
                                                      &notAccessible, &getListing, curItem->TgtName,
                                                      &existingItem, NULL))
                    {
                        if (listingInProgress) // listovani prave probiha nebo ma ted probehnout
                        {
                            if (getListing) // mame ziskat listing, a pak informovat pripadne ostatni cekajici workery
                            {
                                SubState = fwssWorkStartWork;
                                UploadDirGetTgtPathListing = TRUE;
                                postActivate = TRUE; // postneme si impulz pro zacatek stahovani listingu
                            }
                            else
                            {
                                SubState = fwssWorkUploadWaitForListing; // mame cekat az jiny worker dokonci listovani
                                reportWorkerChange = TRUE;               // worker vypisuje stav fwssWorkUploadWaitForListing do okna, takze je potreba prekreslit
                            }
                            break;
                        }
                        else // listing je v cache hotovy nebo "neziskatelny"
                        {
                            if (notAccessible) // listing je cachovany, ale jen jako "neziskatelny"
                            {
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCANNOTLISTTGTPATH, 0, NULL, Oper);
                                lookForNewWork = TRUE;
                                break;
                            }
                            else // mame listing k dispozici
                            {
                                if (existingItem == NULL) // neco se zmenilo: nenasli jsme existujici cilovy soubor - udelame retry (co provest po teto zmene tu proste resit nebudeme)
                                {
                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                    lookForNewWork = TRUE;
                                    break;
                                }
                            }
                        }
                    }
                    else // nedostatek pameti
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
                                if (existingItem->ByteSize == curItem->SizeWithCRLF_EOLs ||                       // velikost s CRLF
                                    existingItem->ByteSize == curItem->SizeWithCRLF_EOLs - curItem->NumberOfEOLs) // velikost s LF (nebo CR)
                                {
                                    SubState = fwssWorkUploadTestFileSizeOK; // velikost je OK => prohlasime upload za uspesny
                                }
                                else
                                    SubState = fwssWorkUploadTestFileSizeFailed; // jina velikost, provedeme Retry (resime problem "transfer failed"...)
                            }
                            else
                            {
                                if (existingItem->ByteSize == OpenedInFileSize)
                                    SubState = fwssWorkUploadTestFileSizeOK; // velikost je OK => prohlasime upload za uspesny
                                else
                                    SubState = fwssWorkUploadTestFileSizeFailed; // jina velikost, provedeme Retry (resime problem "transfer failed"...)
                            }
                            nextLoop = TRUE;
                        }
                        else
                        {
                            if (existingItem->ItemType == ulitFile && existingItem->ByteSize == UPLOADSIZE_NEEDUPDATE)
                            { // soubor po uploadu v ASCII rezimu, jeho velikost budeme znat az po refreshi listingu - invalidatneme listing + retryneme polozku (ta si stahne listing znovu)
                                UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                                         Oper->GetFTPServerPathType(curItem->TgtPath));
                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                            }
                            else // velikost ciloveho souboru v bytech nelze zjistit (ani z listingu ani pres prikaz SIZE)
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
                        { // soubor na serveru jeste neni otevreny, muzeme s nim pracovat
                            if (existingItem->ItemType == ulitFile && existingItem->ByteSize != UPLOADSIZE_UNKNOWN &&
                                existingItem->ByteSize != UPLOADSIZE_NEEDUPDATE)
                            {
                                if (existingItem->ByteSize > OpenedInFileSize)
                                { // cilovy soubor je vetsi nez zdrojovy, resume nelze provest
                                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUNABLETORESUME2), -1, TRUE);
                                    if (UploadType == utResumeOrOverwriteFile) // provedeme overwrite
                                    {
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                        UploadType = utOverwriteFile;
                                        SubState = fwssWorkUploadDelForOverwrite;
                                        nextLoop = TRUE;
                                    }
                                    else // ohlasime chybu (resume neni mozne provest)
                                    {
                                        // chyba na polozce, zapiseme do ni tento stav
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADUNABLETORESUMEBIGTGT, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                }
                                else
                                {
                                    int resumeMinFileSize = Config.GetResumeMinFileSize();
                                    if (CQuadWord(resumeMinFileSize, 0) <= existingItem->ByteSize) // pokud ma Resume smysl (mame uploadnutou cast souboru vetsi nez je minimum pro Resume)
                                    {
                                        ResumingFileOnServer = TRUE;
                                        OpenedInFileCurOffset = existingItem->ByteSize;
                                        // POZNAMKA: neumime Resume uploadu textovych souboru, jinak by tady musela byt inicializace OpenedInFileNumberOfEOLs a OpenedInFileSizeWithCRLF_EOLs podle poctu EOLu v jiz uploadnute casti souboru
                                        FileOnServerResumedAtOffset = OpenedInFileCurOffset;
                                    }
                                    else // provedeme overwrite, protoze cilovy soubor je prilis maly pro resume
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
                                { // soubor po uploadu v ASCII rezimu, jeho velikost budeme znat az po refreshi listingu - invalidatneme listing + retryneme polozku (ta si stahne listing znovu)
                                    UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                                             Oper->GetFTPServerPathType(curItem->TgtPath));
                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                    lookForNewWork = TRUE;
                                }
                                else // velikost ciloveho souboru v bytech nelze zjistit (ani z listingu ani pres prikaz SIZE)
                                {
                                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLUNABLETOGETSIZE), -1, TRUE);
                                    if (UploadType == utResumeOrOverwriteFile) // provedeme overwrite
                                    {
                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                        UploadType = utOverwriteFile;
                                        SubState = fwssWorkUploadDelForOverwrite;
                                        nextLoop = TRUE;
                                    }
                                    else // ohlasime chybu (resume neni mozne provest)
                                    {
                                        // chyba na polozce, zapiseme do ni tento stav
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADUNABLETORESUMEUNKSIZ, NO_ERROR, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                }
                            }
                        }
                        else // nad timto souborem jiz probiha jina operace, at to user zkusi znovu pozdeji
                        {
                            // chyba na polozce, zapiseme do ni tento stav
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_TGTFILEINUSE, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE;
                        }
                    }
                }
                break;
            }

            case fwssWorkUploadTestFileSizeOK:     // upload copy/move souboru: po chybe uploadu test velikosti souboru vysel OK
            case fwssWorkUploadTestFileSizeFailed: // upload copy/move souboru: po chybe uploadu test velikosti souboru nevysel OK
            {
                char num[100];
                SalamanderGeneral->PrintDiskSize(num, OpenedInFileSize, 2);
                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE,
                            LoadStr(SubState == fwssWorkUploadTestFileSizeOK ? (curItem->AsciiTransferMode ? IDS_LOGMSGUPLOADISCOMPL2 : IDS_LOGMSGUPLOADISCOMPL) : (curItem->AsciiTransferMode ? IDS_LOGMSGUPLOADISNOTCOMPL2 : IDS_LOGMSGUPLOADISNOTCOMPL)),
                            curItem->TgtName, num);
                Logs.LogMessage(LogUID, errText, -1, TRUE);

                Queue->UpdateForceAction(CurItem, fqiaNone); // vynucena akce timto prestava platit
                if (SubState == fwssWorkUploadTestFileSizeOK)
                {
                    // zavreme zdrojovy soubor na disku
                    CloseOpenedInFile();

                    // oznacime soubor jako jiz preneseny (pro pripad chyby mazani zdrojoveho souboru u Move musime tuto situaci rozlisit)
                    Queue->UpdateTgtFileState(curItem, UPLOADTGTFILESTATE_TRANSFERRED);

                    SubState = fwssWorkUploadCopyTransferFinished;
                    nextLoop = TRUE;
                }
                else
                {
                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadGenNewName: // upload souboru: autorename: generovani noveho jmena
            {
                Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                BOOL notAccessible, getListing, listingInProgress, nameExists;
                int index = 0;
                UploadAutorenamePhase = curItem->AutorenamePhase;
                int usedUploadAutorenamePhase = UploadAutorenamePhase; // pro pripad kolize jmen - faze ve ktere mame zkusit vygenerovat dalsi jmeno
                while (1)
                {
                    FTPGenerateNewName(&UploadAutorenamePhase, UploadAutorenameNewName, &index,
                                       curItem->TgtName, pathType, FALSE, strcmp(curItem->TgtName, curItem->Name) != 0);
                    // mame nove jmeno, overime jestli nekoliduje s nejakym jmenem z listingu cilove cesty
                    if (UploadListingCache.GetListing(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                      pathType, Msg, UID, &listingInProgress,
                                                      &notAccessible, &getListing,
                                                      UploadAutorenameNewName, NULL, &nameExists))
                    {
                        if (listingInProgress) // listovani prave probiha nebo ma ted probehnout
                        {
                            if (getListing) // mame ziskat listing, a pak informovat pripadne ostatni cekajici workery
                            {
                                UploadDirGetTgtPathListing = TRUE;
                                SubState = fwssWorkStartWork;
                                postActivate = TRUE; // postneme si impulz pro zacatek stahovani listingu
                            }
                            else
                            {
                                SubState = fwssWorkUploadWaitForListing; // mame cekat az jiny worker dokonci listovani
                                reportWorkerChange = TRUE;               // worker vypisuje stav fwssWorkUploadWaitForListing do okna, takze je potreba prekreslit
                            }
                            break;
                        }
                        else // listing je v cache hotovy nebo "neziskatelny"
                        {
                            if (notAccessible) // listing je cachovany, ale jen jako "neziskatelny" (hodne nepravdepodobne, pred chvilkou byl listing jeste "ready")
                            {
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCANNOTLISTTGTPATH, 0, NULL, Oper);
                                lookForNewWork = TRUE;
                                break;
                            }
                            else // mame listing k dispozici, zjistime pripadnou kolizi jmena souboru
                            {
                                if (LockedFileUID != 0)
                                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): LockedFileUID != 0!");
                                if (!nameExists && // neni kolize s existujicim souborem/linkem/adresarem
                                    FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                            pathType, UploadAutorenameNewName,
                                                            &LockedFileUID, ffatWrite)) // cilove jmeno jeste neni otevrene pro jinou operaci - neresime "low memory" (kdyby nastalo, nekonecny cyklus snad nebude - vypadlo by to na UploadListingCache.GetListing s "low memory")
                                {                                                       // bez kolize -> zkusime vytvorit cilovy soubor
                                    char* newName = SalamanderGeneral->DupStr(UploadAutorenameNewName);
                                    if (newName == NULL)
                                    {
                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                                        lookForNewWork = TRUE;
                                    }
                                    else
                                    {
                                        Queue->UpdateRenamedName(curItem, newName);
                                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
                                        SubState = fwssWorkUploadDelForOverwrite;
                                        nextLoop = TRUE;
                                    }
                                    break;
                                }
                                else // kolize jmen (se souborem/linkem/adresarem) nebo uz probiha jina operace s timto souborem - zkusime dalsi jmeno ve stejne fazi autorenamu
                                    UploadAutorenamePhase = usedUploadAutorenamePhase;
                            }
                        }
                    }
                    else // nedostatek pameti
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }
                }
                break;
            }

            case fwssWorkUploadLockFile: // upload souboru: otevreni souboru v FTPOpenedFiles
            {
                Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                if (LockedFileUID != 0)
                    TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState5(): LockedFileUID != 0!");
                if (FTPOpenedFiles.OpenFile(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                            Oper->GetFTPServerPathType(curItem->TgtPath),
                                            curItem->TgtName, &LockedFileUID, ffatWrite))
                { // soubor na serveru jeste neni otevreny, muzeme s nim pracovat
                    SubState = fwssWorkUploadDelForOverwrite;
                    nextLoop = TRUE;
                }
                else // nad timto souborem jiz probiha jina operace, at to user zkusi znovu pozdeji
                {
                    // chyba na polozce, zapiseme do ni tento stav
                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_TGTFILEINUSE, NO_ERROR, NULL, Oper);
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadDelForOverwrite: // upload souboru: pokud jde o overwrite a ma se pouzit napred delete, zavolame ho zde
            {
                if (UseDeleteForOverwrite && UploadType == utOverwriteFile)
                { // soubor uz je zamceny pro zapis, mazani je jen mezikrok, neni nutne volat FTPOpenedFiles.OpenFile()
                    PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdDeleteFile, &cmdLen, curItem->TgtName); // nemuze nahlasit chybu
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

            case fwssWorkUploadDelForOverWaitForDELERes: // upload souboru: cekani na vysledek DELE pred overwritem
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    { // cilovy soubor/link byl uspesne smazan
                        // pokud se soubor/link smazal, zmenime listing v cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        curItem->TgtName, FALSE);
                    }
                    // else; // neuspech mazani nas netrapi, zkusime jeste STOR a pokud se ani tak prepis nepovede, vratime userovi chybu
                    SubState = fwssWorkUploadFileAllocDataCon;
                    nextLoop = TRUE;
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    // pokud nevime jak dopadl vymaz souboru/linku, zneplatnime listing v cache
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

            case fwssWorkUploadFileAllocDataCon: // upload souboru: alokace data-connectiony
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
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        DeleteSocket(WorkerUploadDataCon); // bude se jen dealokovat
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
                    WorkerUploadDataCon->SetPostMessagesToWorker(TRUE, Msg, UID,
                                                                 WORKER_UPLDATACON_CONNECTED,
                                                                 WORKER_UPLDATACON_CLOSED,
                                                                 WORKER_UPLDATACON_PREPAREDATA,
                                                                 WORKER_UPLDATACON_LISTENINGFORCON);
                    WorkerUploadDataCon->SetGlobalTransferSpeedMeter(Oper->GetGlobalTransferSpeedMeter());
                    WorkerUploadDataCon->SetGlobalLastActivityTime(Oper->GetGlobalLastActivityTime());
                    HANDLES(EnterCriticalSection(&WorkerCritSect));

                    if (Oper->GetUsePassiveMode()) // pasivni mod (PASV)
                    {
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdPassive, &cmdLen); // nemuze nahlasit chybu
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadWaitForPASVRes;
                    }
                    else // aktivni mod (PORT)
                    {
                        nextLoop = TRUE;
                        SubState = fwssWorkUploadOpenActDataCon;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForPASVRes: // upload copy/move souboru: cekame na vysledek "PASV" (zjisteni IP+port pro pasivni data-connectionu)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    DWORD ip;
                    unsigned short port;
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&             // uspech (melo by byt 227)
                        FTPGetIPAndPortFromReply(reply, replySize, &ip, &port)) // podarilo se ziskat IP+port
                    {
                        if (ShouldStop)
                        {
                            if (WorkerUploadDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                DeleteSocket(WorkerUploadDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
                                WorkerUploadDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }

                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        }
                        else
                        {
                            if (PrepareDataError != pderNone) // ted uz muze zacit prenos dat na server (i kdyz jeste server nezna jmeno ciloveho souboru)
                            {
                                TRACE_E("CFTPWorker::HandleEventInWorkingState5(): fwssWorkUploadWaitForPASVRes: unexpected value of PrepareDataError: " << PrepareDataError);
                                PrepareDataError = pderNone;
                            }

                            int logUID = LogUID;
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));

                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, jsou tato volani
                            // mozna i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            if (WorkerUploadDataCon != NULL)
                            {
                                WorkerUploadDataCon->SetPassive(ip, port, logUID);
                                WorkerUploadDataCon->PassiveConnect(NULL); // prvni pokus, vysledek nas nezajima (testuje se pozdeji)
                            }

                            HANDLES(EnterCriticalSection(&WorkerCritSect));
                            WorkerDataConState = wdcsWaitingForConnection;

                            nextLoop = TRUE;
                            SubState = fwssWorkUploadSendSTORCmd;
                        }
                    }
                    else // pasivni rezim neni podporovan, zkusime jeste aktivni rezim
                    {
                        Oper->SetUsePassiveMode(FALSE);
                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);

                        if (ShouldStop)
                        {
                            if (WorkerUploadDataCon != NULL)
                            {
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                DeleteSocket(WorkerUploadDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
                                WorkerUploadDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                            }

                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        }
                        else
                        {
                            nextLoop = TRUE;
                            SubState = fwssWorkUploadOpenActDataCon;
                        }
                    }
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        DeleteSocket(WorkerUploadDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
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

            case fwssWorkUploadOpenActDataCon: // upload copy/move souboru: otevreme aktivni data-connectionu
            {
                DWORD localIP;
                unsigned short localPort = 0; // listen on any port
                DWORD error = NO_ERROR;
                int logUID = LogUID;

                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, jsou tato volani
                // mozna i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                GetLocalIP(&localIP, NULL); // snad ani nemuze vratit chybu
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
                    if (PrepareDataError != pderNone) // ted uz muze zacit prenos dat na server (i kdyz jeste server nezna jmeno ciloveho souboru)
                    {
                        TRACE_E("CFTPWorker::HandleEventInWorkingState5(): fwssWorkUploadOpenActDataCon: unexpected value of PrepareDataError: " << PrepareDataError);
                        PrepareDataError = pderNone;
                    }

                    WorkerDataConState = wdcsWaitingForConnection;
                    SubState = fwssWorkUploadWaitForListen;

                    // pro pripad, ze se proxy server neozve v pozadovanem casovem limitu, pridame timer pro
                    // timeout pripravy datove connectiony (otevirani "listen" portu)
                    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
                    if (serverTimeout < 1000)
                        serverTimeout = 1000; // aspon sekundu
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->AddTimer(Msg, UID, GetTickCount() + serverTimeout,
                                            WORKER_LISTENTIMEOUTTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop
                }
                else // nepodarilo se otevrit "listen" socket pro prijem datoveho spojeni ze
                {    // serveru (lokalni operace, nejspis nikdy nenastane) nebo nelze otevrit spojeni na proxy server
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        DeleteSocket(WorkerUploadDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    if (listenError)
                    {
                        // chyba na polozce, zapiseme do ni tento stav
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LISTENFAILURE, error, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                    else // nelze otevrit spojeni na proxy server, provedeme retry...
                    {
                        if (error != NO_ERROR)
                        {
                            FTPGetErrorText(error, errBuf, 50 + FTP_MAX_PATH);
                            char* s = errBuf + strlen(errBuf);
                            while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                                s--;
                            *s = 0; // oriznuti znaku konce radky z textu chyby
                            _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_PROXYERRUNABLETOCON2), errBuf);
                        }
                        else
                            _snprintf_s(ErrorDescr, _TRUNCATE, LoadStr(IDS_PROXYERRUNABLETOCON));
                        _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), ErrorDescr);
                        lstrcpyn(ErrorDescr, errBuf, FTPWORKER_ERRDESCR_BUFSIZE); // chceme aby v textu chyby bylo "data con. err.:"
                        CorrectErrorDescr();

                        // vypiseme timeout do logu
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);

                        // "rucne" zavreme control-connectionu
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        ForceClose();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));

                        conClosedRetryItem = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForListen: // upload copy/move souboru: cekame na otevreni "listen" portu (otevirame aktivni data-connectionu) - lokalniho nebo na proxy serveru
            {
                if (ShouldStop)
                {
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                    // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                    SocketsThread->DeleteTimer(UID, WORKER_LISTENTIMEOUTTIMID);

                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                }
                else
                {
                    BOOL needRetry = FALSE;
                    switch (event)
                    {
                    case fweUplDataConListeningForCon:
                    {
                        if (WorkerUploadDataCon != NULL) // "always true" (jinak by udalost 'fweUplDataConListeningForCon' vubec nevznikla)
                        {
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                            // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
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
                                                  ftpcmdSetPort, &cmdLen, listenOnIP, listenOnPort); // nemuze nahlasit chybu
                                sendCmd = TRUE;
                                SubState = fwssWorkUploadWaitForPORTRes;
                            }
                            else // chyba pri otevirani "listen" portu na proxy serveru - provedeme retry...
                            {
                                // zavreme data-connectionu
                                if (WorkerUploadDataCon != NULL)
                                {
                                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                    if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                                        WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                                    DeleteSocket(WorkerUploadDataCon);
                                    WorkerUploadDataCon = NULL;
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                                    WorkerDataConState = wdcsDoesNotExist;
                                }

                                // pripravime text chyby (timeoutu) do 'ErrorDescr'
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
                        // zavreme data-connectionu + pripravime text chyby (timeoutu) do 'ErrorDescr'
                        errBuf[0] = 0;
                        if (WorkerUploadDataCon != NULL)
                        {
                            HANDLES(LeaveCriticalSection(&WorkerCritSect));
                            if (!WorkerUploadDataCon->GetProxyTimeoutDescr(errBuf, 50 + FTP_MAX_PATH))
                                errBuf[0] = 0;
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                            if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                                WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
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

                        // vypiseme timeout do logu
                        _snprintf_s(errBuf, 50 + FTP_MAX_PATH, _TRUNCATE, "%s\r\n", ErrorDescr);
                        Logs.LogMessage(LogUID, errBuf, -1, TRUE);

                        // "rucne" zavreme control-connectionu
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        ForceClose();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));

                        conClosedRetryItem = TRUE;
                    }
                }
                break;
            }

            case fwssWorkUploadWaitForPORTRes: // upload copy/move souboru: cekame na vysledek "PORT" (predani IP+port serveru pro aktivni data-connectionu)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    nextLoop = TRUE;
                    SubState = fwssWorkUploadSendSTORCmd;
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // zpracujeme pripadnou chybu pri priprave dat
                    if (!HandlePrepareDataError(curItem, lookForNewWork))
                        conClosedRetryItem = TRUE;
                    break;
                }
                }
                break;
            }

            case fwssWorkUploadSendSTORCmd: // upload copy/move souboru: posleme prikaz STOR/APPE (zahajeni ukladani souboru na server)
            {
                if (ShouldStop)
                {
                    if (WorkerUploadDataCon != NULL)
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        if (WorkerUploadDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                            WorkerUploadDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                        DeleteSocket(WorkerUploadDataCon);
                        WorkerUploadDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;
                    }

                    // zpracujeme pripadnou chybu pri priprave dat (ma prioritu pred stopnutim workera, to se kdyztak provede o chvilku pozdeji)
                    if (!HandlePrepareDataError(curItem, lookForNewWork))
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
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
                                      UploadType == utAutorename ? curItem->RenamedName : curItem->TgtName); // nemuze nahlasit chybu
                    sendCmd = TRUE;

                    // TgtFileState zmenime hned, protoze TgtFileState (krome UPLOADTGTFILESTATE_TRANSFERRED) ma smysl
                    // jen tehdy, kdyz se na serveru vytvori soubor tohoto jmena (pokud se soubor nevytvori (napr. kvuli
                    // chybe spojeni), TgtFileState se nepouzije)
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

            case fwssWorkUploadActivateDataCon: // upload copy/move souboru: aktivujeme data-connectionu (tesne po poslani prikazu STOR)
            {
                if (!Oper->GetEncryptDataConnection() && (WorkerUploadDataCon != NULL))
                { // FIXME: 2009.01.29: I believe ActivateConnection can be called later
                    // even when not encrypting, but I am too afraid to change that
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    WorkerUploadDataCon->ActivateConnection();
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                }
                SubState = fwssWorkUploadWaitForSTORRes;
                if (event != fweActivate)
                    nextLoop = TRUE; // pokud neslo jen o fweActivate, dorucime udalost do stavu fwssWorkUploadWaitForSTORRes
                break;
            }

            case fwssWorkUploadWaitForSTORRes: // upload copy/move souboru: cekame na vysledek "STOR/APPE" (cekame na konec uploadu souboru)
            {
                switch (event)
                {
                // "1xx" odpovedi ignorujeme (jen se pisou do Logu) unless encrypting data
                // We can't start encryption before receiving successful 1xx reply
                // Otherwise SSL_connect fails in ActivateConnection if 5xx reply is about to be received
                case fweCmdInfoReceived:
                    if (Oper->GetEncryptDataConnection() && (WorkerUploadDataCon != NULL))
                    {
                        HANDLES(LeaveCriticalSection(&WorkerCritSect));
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        WorkerUploadDataCon->ActivateConnection();
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                    }
                    break;
                case fweCmdReplyReceived:
                {
                    // vyhodnotime vysledek prikazu STOR/APPE, bud nechame velikost "needupdate" (STOR v ascii-mode nebo APPE) nebo ji nastavime (STOR v binary-mode) nebo jen invalidatneme listing (chyba)
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

                    // zrusime data-connectionu, uz nebude potreba (uz ji stejne nikdo nesleduje - server uz hlasi vysledek = server uz ceka na dalsi prikazy)
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

                    BOOL uploadTypeIsAutorename = (UploadType == utAutorename); // UploadType se nuluje pri zavirani OpenedInFile, proto si to musime zjistit uz tady
                    BOOL canUseRenamedName = TRUE;
                    BOOL canClearForceAction = TRUE;
                    if (!ShouldStop && PrepareDataError == pderASCIIForBinaryFile)
                    { // pri zjisteni binarniho souboru v ASCII rezimu zajistime vymaz ciloveho souboru
                        // soubor uz je zamceny pro zapis, mazani je jen mezikrok, neni nutne volat FTPOpenedFiles.OpenFile()
                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdDeleteFile, &cmdLen,
                                          UploadType == utAutorename ? curItem->RenamedName : curItem->TgtName); // nemuze nahlasit chybu
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadWaitForDELERes;
                    }
                    else
                    {
                        if (!HandlePrepareDataError(curItem, lookForNewWork)) // zpracujeme pripadnou chybu pri priprave dat
                        {
                            if (ShouldStop)
                                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                            else
                            {
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS && allDataTransferred &&
                                    dataConError == NO_ERROR && dataSSLErrorOccured == SSLCONERR_NOERROR)
                                {                                                                     // prenos uspesne dokoncen (server hlasi uspech, data-connectina hlasi vse preneseno + zadna chyba)
                                    if (ResumingFileOnServer && uploadRealSize != UPLOADSIZE_UNKNOWN) // musi byt pred CloseOpenedInFile() - tam se nuluje ResumingFileOnServer a FileOnServerResumedAtOffset
                                        uploadRealSize += FileOnServerResumedAtOffset;                // u resumnutych souboru pricteme resume-offset

                                    // zavreme zdrojovy soubor na disku
                                    CloseOpenedInFile();

                                    // oznacime soubor jako jiz preneseny (pro pripad chyby mazani zdrojoveho souboru u Move musime tuto situaci rozlisit)
                                    Queue->UpdateTgtFileState(curItem, UPLOADTGTFILESTATE_TRANSFERRED);

                                    if (uploadRealSize != UPLOADSIZE_UNKNOWN && curItem->Size != uploadRealSize)
                                    {
                                        Queue->UpdateFileSize(curItem, uploadRealSize, Oper);
                                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky
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
                                        if ((!ResumingFileOnServer || Oper->GetDataConWasOpenedForAppendCmd()) && // proftpd (Linux) vraci opakovane 45x (zakazany append, povoluje se nekde v konfigu), warftpd vraci opakovane 42x (nejaky write error) -- kazdopadne nelze do nekonecna zkouset znovu APPE (ovsem pokud APPE zpusobilo otevreni data-connectiony, provedeme auto-retry, protoze na 99.9% je APPE funkcni)
                                                FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR &&
                                                (FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION ||  // hlavne "426 data connection closed, transfer aborted" (je-li to adminem serveru nebo poruchou spojeni nejsem sto rozpoznat, takze prioritu dostava porucha spojeni -> opakujeme pokus o upload)
                                                 FTP_DIGIT_2(replyCode) == FTP_D2_FILESYSTEM) && // "450 Transfer aborted.  Link to file server lost."
                                                dataSSLErrorOccured != SSLCONERR_DONOTRETRY ||   // 426 a 450 bereme jen pokud nebyly vyvolane chybou: nepodarilo se zasifrovat spojeni, jde o permanentni problem
                                            dataConNoDataTransTimeout ||                         // nami prerusene spojeni kvuli no-data-transfer timeoutu (deje se pri "50%" vypadcich site, data-connectiona se neprerusi, ale nejak se zablokuje prenos dat, vydrzi otevrena klidne 14000 sekund, tohle by to melo resit) -> opakujeme pokus o download
                                            dataSSLErrorOccured == SSLCONERR_CANRETRY)           // nepodarilo se zasifrovat spojeni, nejde o permanentni problem
                                        {
                                            SubState = fwssWorkCopyDelayedAutoRetry; // pouzijeme zpozdeny auto-retry, aby stihly prijit vsechny necekane odpovedi ze serveru
                                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                            // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                                            SocketsThread->AddTimer(Msg, UID, GetTickCount() + WORKER_DELAYEDAUTORETRYTIMEOUT,
                                                                    WORKER_DELAYEDAUTORETRYTIMID, NULL); // chybu ignorujeme, maximalne si user da Stop
                                        }
                                        else
                                        {
                                            if (!ResumingFileOnServer &&
                                                FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS &&
                                                (uploadRealSize == CQuadWord(0, 0) || uploadRealSize == UPLOADSIZE_UNKNOWN) &&
                                                dataSSLErrorOccured == SSLCONERR_NOERROR)
                                            { // STOR hlasi chybu + nic se neuploadlo == predpokladame chybu "cannot create target file name"
                                                if (UploadType == utOverwriteFile && !UseDeleteForOverwrite)
                                                {
                                                    UseDeleteForOverwrite = TRUE;
                                                    canClearForceAction = FALSE;
                                                    Queue->UpdateForceAction(CurItem, fqiaOverwrite);                              // pro pripad, ze nejde o cisty Overwrite (ale treba Resume-or-Overwrite)
                                                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                                }
                                                else
                                                {
                                                    if (UploadType == utAutorename)
                                                    {
                                                        if (UploadAutorenamePhase != -1) // jdeme zkusit nagenerovat jeste jine jmeno
                                                        {
                                                            canClearForceAction = FALSE;
                                                            Queue->UpdateForceAction(CurItem, fqiaUploadContinueAutorename);
                                                            Queue->UpdateAutorenamePhase(curItem, UploadAutorenamePhase);
                                                            canUseRenamedName = FALSE;                                                     // TgtName nechame puvodni (RenamedName zapomeneme, bude se generovat jmeno typove z dalsi faze FTPGenerateNewName)
                                                            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                                        }
                                                        else // uz nevime jake jine jmeno by se dalo vytvorit, takze ohlasime chybu
                                                        {
                                                            CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADFILEAUTORENFAILED, NO_ERROR,
                                                                                   SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                                                   Oper);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                                        if (CurItem->ForceAction == fqiaUseAutorename) // forcnuty autorename
                                                        {
                                                            canClearForceAction = FALSE;
                                                            Queue->UpdateForceAction(CurItem, fqiaUploadForceAutorename);
                                                            Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
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
                                                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
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
                                                    FTP_DIGIT_2(replyCode) == FTP_D2_SYNTAX && // server neumi APPE, napr. "500 command not understood"
                                                    dataSSLErrorOccured == SSLCONERR_NOERROR)
                                                {
                                                    Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGRESUMENOTSUP), -1, TRUE);
                                                    Oper->SetResumeIsNotSupported(TRUE);       // dalsi APPE uz zkouset nebudeme (leda po Retry nebo Solve Error, kde se tohle zase nuluje)
                                                    if (UploadType == utResumeOrOverwriteFile) // provedeme overwrite
                                                    {
                                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                                        canClearForceAction = FALSE;
                                                        Queue->UpdateForceAction(CurItem, fqiaOverwrite);
                                                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                                    }
                                                    else
                                                    {
                                                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETORESUME, NO_ERROR,
                                                                               NULL, Oper);
                                                    }
                                                }
                                                else
                                                {
                                                    if (ResumingFileOnServer && UploadType == utResumeOrOverwriteFile && // chyba resume, jeste zkusime overwrite
                                                        dataSSLErrorOccured == SSLCONERR_NOERROR)
                                                    {
                                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGUPLRESUMEERR), -1, TRUE);
                                                        Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGOVERWRTGTFILE), -1, TRUE);
                                                        canClearForceAction = FALSE;
                                                        Queue->UpdateForceAction(CurItem, fqiaOverwrite);
                                                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                                    }
                                                    else
                                                    {
                                                        if (dataSSLErrorOccured != SSLCONERR_NOERROR)
                                                            lstrcpyn(errText, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 200 + FTP_MAX_PATH);
                                                        else
                                                        {
                                                            errText[0] = 0;
                                                            if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS)
                                                            { // pokud nemame popis sitove chyby ze serveru, spokojime se se systemovym popisem
                                                                CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                                                            }

                                                            if (errText[0] == 0 && errBuf[0] != 0) // zkusime jeste vzit text chyby z proxy serveru
                                                                lstrcpyn(errText, errBuf, 200 + FTP_MAX_PATH);
                                                        }

                                                        // chyba na polozce, zapiseme do ni tento stav
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

                    // krome pripadu, kdy STOR hlasi chybu "cannot create target file name" (coz je kdyz STOR hlasi
                    // nejakou chybu + nic se neuploadlo) povazujeme poslani prikazu STOR/APPE za dokonceni
                    // forcovane akce: "overwrite", "resume" a "resume or overwrite"
                    if (CurItem->ForceAction != fqiaNone && canClearForceAction) // vynucena akce timto prestava platit
                        Queue->UpdateForceAction(CurItem, fqiaNone);

                    // autorename: zapiseme do polozky nove jmeno - i kdyby STOR hodil chybu, porad je vic pravda, ze se soubor
                    // ukladal pod novym jmenem, nez pod puvodnim - napr. u prepisu existujiciho souboru je to zrejme
                    // POZNAMKA: neni potreba volat Oper->ReportItemChange(CurItem->UID), protoze RenamedName se pouziva
                    // prioritne pred TgtName (takze nove jmeno uz je davno vypsane)
                    if (uploadTypeIsAutorename && canUseRenamedName)
                        Queue->ChangeTgtNameToRenamedName(curItem);
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    if (UploadType == utAutorename)
                        Queue->ChangeTgtNameToRenamedName(curItem); // i kdyby STOR hodil chybu, porad je vic pravda, ze se soubor ukladal pod novym jmenem, nez pod puvodnim - napr. u prepisu existujiciho souboru je to zrejme
                    // vysledek prikazu STOR nezname, invalidatneme listing
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportFileUploaded(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                          Oper->GetFTPServerPathType(curItem->TgtPath),
                                                          curItem->TgtName, UPLOADSIZE_UNKNOWN, TRUE);

                    // krome pripadu, kdy STOR hlasi chybu "cannot create target file name" (coz je kdyz STOR hlasi
                    // nejakou chybu + nic se neuploadlo) povazujeme poslani prikazu STOR/APPE za dokonceni
                    // forcovane akce: "overwrite", "resume" a "resume or overwrite"
                    if (CurItem->ForceAction != fqiaNone) // vynucena akce timto prestava platit
                        Queue->UpdateForceAction(CurItem, fqiaNone);

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

                    // zpracujeme pripadnou chybu pri priprave dat
                    if (!HandlePrepareDataError(curItem, lookForNewWork))
                    {
                        if (DataConAllDataTransferred && CommandReplyTimeout)
                        { // poslali jsme uspesne vsechna data + server nestihl odpovedet => na 99,9% je soubor uploadly cely a "jen" je zatuhla control-connectiona (deje se pri uploadech/downloadech delsich nez 1,5 hodiny)
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

            case fwssWorkCopyDelayedAutoRetry: // copy/move souboru: cekame WORKER_DELAYEDAUTORETRYTIMEOUT milisekund na auto-retry (aby mohly prijit vsechny neocekavane odpovedi ze serveru)
            {
                if (event == fweDelayedAutoRetry) // uz mame provest auto-retry
                {
                    Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                    lookForNewWork = TRUE;
                }
                break;
            }

            case fwssWorkUploadWaitForDELERes: // upload copy/move souboru: pderASCIIForBinaryFile: cekame na vysledek prikazu DELE (smazani ciloveho souboru)
            {
                switch (event)
                {
                // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
                case fweCmdReplyReceived:
                {
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                    { // cilovy soubor/link byl uspesne smazan
                        // pokud se soubor/link smazal, zmenime listing v cache
                        Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                        UploadListingCache.ReportDelete(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        curItem->TgtName, FALSE);
                    }
                    // else; // neuspech mazani nas netrapi, user bude resit prepis/append v Solve Error dialogu
                    HandlePrepareDataError(curItem, lookForNewWork); // PrepareDataError == pderASCIIForBinaryFile
                    break;
                }

                case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
                {
                    // pokud nevime jak dopadl vymaz souboru/linku, zneplatnime listing v cache
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

            case fwssWorkUploadCopyTransferFinished: // cilovy soubor je jiz uploadly, jde-li o Move, zkusime jeste smaznout zdrojovy soubor na disku
            {
                if (CurItem->Type == fqitUploadMoveFile)
                { // Move - zkusime jeste smaznout zdrojovy soubor na disku
                    if (DiskWorkIsUsed)
                        TRACE_E("Unexpected situation 2 in CFTPWorker::HandleEventInWorkingState5(): DiskWorkIsUsed may not be TRUE here!");
                    InitDiskWork(WORKER_DISKWORKDELFILEFINISHED, fdwtDeleteFile, CurItem->Path, CurItem->Name,
                                 fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
                    if (FTPDiskThread->AddWork(&DiskWork))
                    {
                        DiskWorkIsUsed = TRUE;
                        SubState = fwssWorkUploadDelFileWaitForDisk; // pockame si na vysledek
                    }
                    else // nelze smazat zdrojovy soubor, nelze pokracovat v provadeni polozky
                    {
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                }
                else // Copy - neni co dal resit, hotovo
                {
                    SubState = fwssWorkUploadCopyDone;
                    nextLoop = TRUE;
                }
                break;
            }

            case fwssWorkUploadDelFileWaitForDisk:        // upload copy/move souboru: cekame na dokonceni diskove operace (smazani zdrojoveho souboru)
            case fwssWorkUploadDelFileWaitForDiskAftQuit: // upload copy/move souboru: po poslani prikazu "QUIT" + cekame na dokonceni diskove operace (smazani zdrojoveho souboru)
            {
                if (event == fweWorkerShouldStop && ShouldStop) // mame ukoncit workera
                {
                    if (SubState != fwssWorkUploadDelFileWaitForDiskAftQuit && !SocketClosed)
                    {
                        SubState = fwssWorkUploadDelFileWaitForDiskAftQuit; // abychom neposilali "QUIT" vicekrat
                        sendQuitCmd = TRUE;                                 // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
                    }
                }
                else
                {
                    if (event == fweDiskWorkDelFileFinished) // mame vysledek diskove operace (smazani souboru)
                    {
                        DiskWorkIsUsed = FALSE;
                        ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

                        // pokud uz jsme QUIT poslali, musime zamezit dalsimu poslani QUIT z noveho stavu
                        quitCmdWasSent = SubState == fwssWorkUploadDelFileWaitForDiskAftQuit;

                        if (DiskWork.State == sqisNone)
                        { // soubor se podarilo smazat
                            SubState = fwssWorkUploadCopyDone;
                            nextLoop = TRUE;
                        }
                        else // pri mazani souboru nastala chyba
                        {
                            Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                            lookForNewWork = TRUE; // quitCmdWasSent uz je nastaveny
                        }
                    }
                }
                break;
            }

            case fwssWorkUploadCopyDone: // upload copy/move souboru: hotovo, jdeme na dalsi polozku
            {
                // polozka byla uspesne dokoncena, zapiseme do ni tento stav
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

        // pred zavrenim souboru je nutne zcancelovat disk-work (aby se nepracovalo se
        // zavrenym handlem souboru + aby sel stopnout worker)
        if (DiskWorkIsUsed && (conClosedRetryItem || lookForNewWork || handleShouldStop))
        {
            BOOL workIsInProgress;
            if (FTPDiskThread->CancelWork(&DiskWork, &workIsInProgress))
            {
                if (workIsInProgress)
                    DiskWork.FlushDataBuffer = NULL; // prace je rozdelana, nemuzeme uvolnit buffer pro ctena data, nechame to na disk-work threadu (viz cast cancelovani prace) - do DiskWork muzeme zapisovat, protoze po Cancelu do nej uz disk-thread nesmi pristupovat (napr. uz vubec nemusi existovat)
            }
            // pokud jsme praci zcancelovali jeste pred jejim zapocetim, musime uvolnit flush buffer +
            // pokud je jiz prace hotova, uvolnime flush buffer zde, protoze fweDiskWorkReadFinished
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
