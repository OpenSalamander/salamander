// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CFTPWorker
//

void CFTPWorker::HandleEventInWorkingState4(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                            BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                            int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                            int replyCode, char* ftpPath, char* errText,
                                            BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                            BOOL& handleShouldStop, BOOL& quitCmdWasSent)
{
    char hostBuf[HOST_MAX_SIZE];
    char userBuf[USER_MAX_SIZE];
    unsigned short portBuf;
    CFTPQueueItemCopyMoveUploadExplore* curItem = (CFTPQueueItemCopyMoveUploadExplore*)CurItem;
    CUploadListingItem* existingItem = NULL; // pro predavani dat o polozce listingu mezi ruznymi SubState
    while (1)
    {
        BOOL nextLoop = FALSE;
        switch (SubState)
        {
        case fwssWorkStartWork: // zjistime v jakem stavu je cilovy adresar
        {
            if (ShouldStop)
                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
            else
            {
                if (curItem->TgtDirState == UPLOADTGTDIRSTATE_UNKNOWN)
                {
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                    BOOL notAccessible, getListing, listingInProgress;
                    if (existingItem != NULL)
                        TRACE_E("CFTPWorker::HandleEventInWorkingState4(): unexpected situation: existingItem != NULL!");
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
                            else // mame listing k dispozici, zjistime pripadnou kolizi jmena adresare
                            {
                                BOOL nameValid = FTPMayBeValidNameComponent(curItem->TgtName, curItem->TgtPath, TRUE, pathType);
                                nextLoop = TRUE;
                                if (existingItem == NULL && nameValid) // bez kolize a validni jmeno -> zkusime vytvorit adresar
                                    SubState = fwssWorkUploadCreateDir;
                                else
                                {                                                         // je-li existingItem == NULL, je (!nameValid==TRUE), proto nejsou treba testy na existingItem != NULL
                                    if (!nameValid || existingItem->ItemType == ulitFile) // invalidni jmeno nebo kolize se souborem -> "dir cannot be created"
                                        SubState = !nameValid ? fwssWorkUploadCantCreateDirInvName : fwssWorkUploadCantCreateDirFileEx;
                                    else
                                    {
                                        if (existingItem->ItemType == ulitDirectory) // kolize s adresarem -> "dir already exists"
                                            SubState = fwssWorkUploadDirExists;
                                        else // (existingItem->ItemType == ulitLink): kolize s linkem -> zjistime co je link zac (soubor/adresar)
                                            SubState = fwssWorkUploadResolveLink;
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
                else // cilovy adresar je jiz pripraveny, jdeme listovat uploadeny adresar na disku
                {
                    SubState = fwssWorkUploadGetTgtPath;
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

        case fwssWorkUploadResolveLink: // upload copy/move souboru: zjistime co je link (soubor/adresar), jehoz jmeno koliduje se jmenem ciloveho adresare na serveru
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
                        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)  // uspech, link vede do adresare
                            SubState = fwssWorkUploadDirExistsDirLink; // optimalizace fwssWorkUploadDirExists: CWD uz je hotove, udelame rovnou PWD
                        else                                           // permanentni chyba, link vede nejspis na soubor (ale muze jit i o "550 Permission denied", bohuzel 550 je i "550 Not a directory", takze nerozlisitelne...)
                            SubState = fwssWorkUploadCantCreateDirFileEx;
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

        case fwssWorkUploadCreateDir: // upload copy/move souboru: vytvorime cilovy adresar na serveru - zacneme nastavenim cilove cesty
        {
            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                              ftpcmdChangeWorkingPath, &cmdLen, curItem->TgtPath); // nemuze nahlasit chybu
            sendCmd = TRUE;
            SubState = fwssWorkUploadCrDirWaitForCWDRes;

            HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
            break;
        }

        case fwssWorkUploadCrDirWaitForCWDRes: // upload copy/move souboru: cekame na vysledek "CWD" (nastaveni cilove cesty)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // cilova cesta je nastavena, vytvorime cilovy adresar
                {
                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGCREATEDIR), curItem->TgtName);
                        Logs.LogMessage(LogUID, errText, -1, TRUE);

                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdCreateDir, &cmdLen, curItem->TgtName); // nemuze nahlasit chybu
                        sendCmd = TRUE;
                        SubState = fwssWorkUploadCrDirWaitForMKDRes;
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

        case fwssWorkUploadCrDirWaitForMKDRes: // upload copy/move souboru: cekame na vysledek "MKD" (vytvoreni ciloveho adresare)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // cilovy adresar je vytvoreny (melo by byt 257)
                {
                    Queue->UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_READY);

                    // pokud se cilovy adresar vytvoril, zmenime listing v cache
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportCreateDirs(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        curItem->TgtName, FALSE);

                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        SubState = fwssWorkUploadGetTgtPath;
                        nextLoop = TRUE;
                    }
                }
                else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                {
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                    if (UploadListingCache.IsListingFromPanel(userBuf, hostBuf, portBuf, curItem->TgtPath, pathType))
                    {
                        Queue->UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_UNKNOWN);
                        UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath, pathType);
                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                        lookForNewWork = TRUE;
                    }
                    else
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        if (CurItem->ForceAction == fqiaUseAutorename) // forcnuty autorename
                        {
                            if (ShouldStop)
                                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                            else
                            {
                                SubState = fwssWorkUploadAutorenameDir;
                                nextLoop = TRUE;
                            }
                        }
                        else
                        {
                            switch (Oper->GetUploadCannotCreateDir())
                            {
                            case CANNOTCREATENAME_USERPROMPT:
                            {
                                Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTDIR, NO_ERROR,
                                                       SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                       Oper);
                                lookForNewWork = TRUE;
                                break;
                            }

                            case CANNOTCREATENAME_SKIP:
                            {
                                Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTDIR, NO_ERROR,
                                                       SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                                       Oper);
                                lookForNewWork = TRUE;
                                break;
                            }

                            default: // case CANNOTCREATENAME_AUTORENAME:
                            {
                                if (ShouldStop)
                                    handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                                else
                                {
                                    SubState = fwssWorkUploadAutorenameDir;
                                    nextLoop = TRUE;
                                }
                                break;
                            }
                            }
                        }
                    }
                }
                break;
            }

            case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
            {
                // pokud nevime jak dopadlo vytvoreni adresare, zneplatnime listing v cache
                Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                UploadListingCache.ReportCreateDirs(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                    Oper->GetFTPServerPathType(curItem->TgtPath),
                                                    curItem->TgtName, TRUE);
                conClosedRetryItem = TRUE;
                break;
            }
            }
            break;
        }

        case fwssWorkUploadCantCreateDirInvName: // upload copy/move souboru: resime chybu "target directory cannot be created" (invalid name)
        case fwssWorkUploadCantCreateDirFileEx:  // upload copy/move souboru: resime chybu "target directory cannot be created" (name already used for file or link)
        {
            if (CurItem->ForceAction == fqiaUseAutorename) // forcnuty autorename
            {
                SubState = fwssWorkUploadAutorenameDir;
                nextLoop = TRUE;
            }
            else
            {
                switch (Oper->GetUploadCannotCreateDir())
                {
                case CANNOTCREATENAME_USERPROMPT:
                {
                    Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADCANNOTCREATETGTDIR,
                                           SubState == fwssWorkUploadCantCreateDirFileEx ? ERROR_ALREADY_EXISTS : NO_ERROR,
                                           NULL, Oper);
                    lookForNewWork = TRUE;
                    break;
                }

                case CANNOTCREATENAME_SKIP:
                {
                    Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADCANNOTCREATETGTDIR,
                                           SubState == fwssWorkUploadCantCreateDirFileEx ? ERROR_ALREADY_EXISTS : NO_ERROR,
                                           NULL, Oper);
                    lookForNewWork = TRUE;
                    break;
                }

                default: // case CANNOTCREATENAME_AUTORENAME:
                {
                    SubState = fwssWorkUploadAutorenameDir;
                    nextLoop = TRUE;
                    break;
                }
                }
            }
            break;
        }

        case fwssWorkUploadDirExistsDirLink: // stejny stav jako fwssWorkUploadDirExists: navic jen to, ze bylo prave provedeno CWD do ciloveho adresare (test jestli je link adresar nebo soubor)
        case fwssWorkUploadDirExists:        // upload copy/move souboru: resime chybu "target directory already exists"
        {
            if (CurItem->ForceAction == fqiaUseAutorename) // forcnuty autorename
            {
                SubState = fwssWorkUploadAutorenameDir;
                nextLoop = TRUE;
            }
            else
            {
                if (CurItem->ForceAction == fqiaUseExistingDir) // forcnuty use-existing-dir
                {
                    Queue->UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_READY);
                    Queue->UpdateForceAction(CurItem, fqiaNone); // vynucena akce timto prestava platit

                    SubState = SubState == fwssWorkUploadDirExistsDirLink ? fwssWorkUploadGetTgtPathSendPWD : fwssWorkUploadGetTgtPath;
                    nextLoop = TRUE;
                }
                else
                {
                    switch (Oper->GetUploadDirAlreadyExists())
                    {
                    case DIRALREADYEXISTS_USERPROMPT:
                    {
                        Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_UPLOADTGTDIRALREADYEXISTS,
                                               NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }

                    case DIRALREADYEXISTS_SKIP:
                    {
                        Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_UPLOADTGTDIRALREADYEXISTS,
                                               NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                        break;
                    }

                    case DIRALREADYEXISTS_JOIN:
                    {
                        Queue->UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_READY);
                        SubState = SubState == fwssWorkUploadDirExistsDirLink ? fwssWorkUploadGetTgtPathSendPWD : fwssWorkUploadGetTgtPath;
                        nextLoop = TRUE;
                        break;
                    }

                    default: // case DIRALREADYEXISTS_AUTORENAME:
                    {
                        SubState = fwssWorkUploadAutorenameDir;
                        nextLoop = TRUE;
                        break;
                    }
                    }
                }
            }
            break;
        }

        case fwssWorkUploadAutorenameDir: // upload copy/move souboru: reseni chyby vytvareni ciloveho adresare - autorename - zacneme nastavenim cilove cesty
        {
            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                              ftpcmdChangeWorkingPath, &cmdLen, curItem->TgtPath); // nemuze nahlasit chybu
            sendCmd = TRUE;
            SubState = fwssWorkUploadAutorenDirWaitForCWDRes;

            HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
            break;
        }

        case fwssWorkUploadAutorenDirWaitForCWDRes: // upload copy/move souboru: autorename - cekame na vysledek "CWD" (nastaveni cilove cesty)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // cilova cesta je nastavena, zahajime generovani jmen ciloveho adresare
                {
                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        UploadAutorenamePhase = 0; // pocatek generovani jmen
                        UploadAutorenameNewName[0] = 0;
                        SubState = fwssWorkUploadAutorenDirSendMKD;
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

        case fwssWorkUploadAutorenDirSendMKD: // upload copy/move souboru: autorename - zkusi vygenerovat dalsi nove jmeno pro cilovy adresar a zkusi tento adresar vytvorit
        {
            Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
            CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
            BOOL notAccessible, getListing, listingInProgress, nameExists;
            int index = 0;
            int usedUploadAutorenamePhase = UploadAutorenamePhase; // pro pripad kolize jmen - faze ve ktere mame zkusit vygenerovat dalsi jmeno
            while (1)
            {
                FTPGenerateNewName(&UploadAutorenamePhase, UploadAutorenameNewName, &index,
                                   curItem->TgtName, pathType, TRUE, FALSE);
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
                        else // mame listing k dispozici, zjistime pripadnou kolizi jmena adresare
                        {
                            if (!nameExists) // bez kolize -> zkusime vytvorit cilovy adresar
                            {
                                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGCREATEDIR), UploadAutorenameNewName);
                                Logs.LogMessage(LogUID, errText, -1, TRUE);

                                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                  ftpcmdCreateDir, &cmdLen, UploadAutorenameNewName); // nemuze nahlasit chybu
                                sendCmd = TRUE;
                                SubState = fwssWorkUploadAutorenDirWaitForMKDRes;
                                break;
                            }
                            else // kolize jmen (se souborem/linkem/adresarem) - zkusime dalsi jmeno ve stejne fazi autorenamu
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

        case fwssWorkUploadAutorenDirWaitForMKDRes: // upload copy/move souboru: autorename - cekame na vysledek "MKD" (vytvoreni ciloveho adresare pod novym jmenem)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // cilovy adresar je vytvoreny (melo by byt 257)
                {
                    // pokud se cilovy adresar vytvoril, zmenime listing v cache
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    UploadListingCache.ReportCreateDirs(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                        Oper->GetFTPServerPathType(curItem->TgtPath),
                                                        UploadAutorenameNewName, FALSE);

                    char* newName = SalamanderGeneral->DupStr(UploadAutorenameNewName);
                    if (newName != NULL)
                    {
                        if (CurItem->ForceAction != fqiaNone) // vynucena akce timto prestava platit
                            Queue->UpdateForceAction(CurItem, fqiaNone);

                        Queue->UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_READY);
                        Queue->UpdateTgtName(curItem, newName);
                        Oper->ReportItemChange(CurItem->UID); // pozadame o redraw polozky

                        if (ShouldStop)
                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        else
                        {
                            SubState = fwssWorkUploadGetTgtPath;
                            nextLoop = TRUE;
                        }
                    }
                    else
                    {
                        TRACE_E(LOW_MEMORY);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, 0, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                }
                else // nastala chyba pri vytvareni adresare
                {
                    if (UploadAutorenamePhase != -1) // jdeme zkusit nagenerovat jeste jine jmeno
                    {
                        if (ShouldStop)
                            handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                        else
                        {
                            SubState = fwssWorkUploadAutorenDirSendMKD;
                            nextLoop = TRUE;
                        }
                    }
                    else // uz nevime jake jine jmeno by se dalo vytvorit, takze ohlasime chybu
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UPLOADCRDIRAUTORENFAILED, NO_ERROR,
                                               SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                               Oper);
                        lookForNewWork = TRUE;
                    }
                }
                break;
            }

            case fweCmdConClosed: // spojeni se zavrelo/timeoutlo (popis viz ErrorDescr) -> zkusime ho obnovit
            {
                // pokud nevime jak dopadlo vytvoreni adresare, zneplatnime listing v cache
                Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                UploadListingCache.ReportCreateDirs(userBuf, hostBuf, portBuf, curItem->TgtPath,
                                                    Oper->GetFTPServerPathType(curItem->TgtPath),
                                                    UploadAutorenameNewName, TRUE);
                conClosedRetryItem = TRUE;
                break;
            }
            }
            break;
        }

        case fwssWorkUploadGetTgtPath: // upload copy/move souboru: zjistime cestu do ciloveho adresare na serveru - zacneme zmenou cesty do nej
        {
            lstrcpyn(ftpPath, curItem->TgtPath, FTP_MAX_PATH);
            CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
            if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, curItem->TgtName, TRUE))
            { // mame cestu, posleme na server CWD do zkoumaneho adresare
                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                  ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // nemuze nahlasit chybu
                sendCmd = TRUE;
                SubState = fwssWorkUploadGetTgtPathWaitForCWDRes;

                HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
            }
            else // chyba syntaxe cesty nebo by vznikla moc dlouha cesta
            {
                // chyba na polozce, zapiseme do ni tento stav
                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INVALIDPATHTODIR, NO_ERROR, NULL, Oper);
                lookForNewWork = TRUE;
            }
            break;
        }

        case fwssWorkUploadGetTgtPathWaitForCWDRes: // upload copy/move souboru: cekame na vysledek "CWD" (nastaveni cesty do ciloveho adresare)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // cesta do ciloveho adresare je nastavena, posleme PWD
                {
                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        nextLoop = TRUE;
                        SubState = fwssWorkUploadGetTgtPathSendPWD;
                    }
                }
                else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                {
                    Oper->GetUserHostPort(userBuf, hostBuf, &portBuf);
                    CFTPServerPathType pathType = Oper->GetFTPServerPathType(curItem->TgtPath);
                    if (UploadListingCache.IsListingFromPanel(userBuf, hostBuf, portBuf, curItem->TgtPath, pathType))
                    {
                        Queue->UpdateUploadTgtDirState(curItem, UPLOADTGTDIRSTATE_UNKNOWN);
                        UploadListingCache.InvalidatePathListing(userBuf, hostBuf, portBuf, curItem->TgtPath, pathType);
                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                    }
                    else
                    {
                        CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOCWD, NO_ERROR,
                                               SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                               Oper);
                    }
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

        case fwssWorkUploadGetTgtPathSendPWD: // upload copy/move souboru: posleme "PWD" (zjisteni cesty do ciloveho adresare)
        {
            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                              ftpcmdPrintWorkingPath, &cmdLen); // nemuze nahlasit chybu
            sendCmd = TRUE;
            SubState = fwssWorkUploadGetTgtPathWaitForPWDRes;
            break;
        }

        case fwssWorkUploadGetTgtPathWaitForPWDRes: // upload copy/move souboru: cekame na vysledek "PWD" (zjisteni cesty do ciloveho adresare)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&
                    FTPGetDirectoryFromReply(reply, replySize, ftpPath, FTP_MAX_PATH))
                { // uspech, mame working path
                    lstrcpyn(WorkingPath, ftpPath, FTP_MAX_PATH);
                    HaveWorkingPath = TRUE;

                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        nextLoop = TRUE;
                        SubState = fwssWorkUploadListDiskDir;
                    }
                }
                else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                {
                    CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOPWD, NO_ERROR,
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

        case fwssWorkUploadListDiskDir: // upload copy/move souboru: jdeme vylistovat uploadovany adresar na disku
        {
            // zkusime vylistovat zdrojovy adresar na disku
            if (DiskWorkIsUsed)
                TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState4(): DiskWorkIsUsed may not be TRUE here!");
            InitDiskWork(WORKER_DISKWORKLISTFINISHED, fdwtListDir, CurItem->Path, CurItem->Name,
                         fqiaNone, FALSE, NULL, NULL, NULL, 0, NULL);
            if (FTPDiskThread->AddWork(&DiskWork))
            {
                DiskWorkIsUsed = TRUE;
                SubState = fwssWorkUploadListDiskWaitForDisk; // pockame si na vysledek
            }
            else // nelze vylistovat zdrojovy adresar, nelze pokracovat v provadeni polozky
            {
                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                lookForNewWork = TRUE;
            }
            break;
        }

        case fwssWorkUploadListDiskWaitForDisk:        // upload copy/move souboru: cekame na dokonceni diskove operace (listovani adresare)
        case fwssWorkUploadListDiskWaitForDiskAftQuit: // upload copy/move souboru: po poslani prikazu "QUIT" + cekame na dokonceni diskove operace (listovani adresare)
        {
            if (event == fweWorkerShouldStop && ShouldStop) // mame ukoncit workera
            {
                if (SubState != fwssWorkUploadListDiskWaitForDiskAftQuit && !SocketClosed)
                {
                    SubState = fwssWorkUploadListDiskWaitForDiskAftQuit; // abychom neposilali "QUIT" vicekrat
                    sendQuitCmd = TRUE;                                  // mame koncit + mame otevrenou connectionu -> posleme serveru prikaz "QUIT" (odpoved ignorujeme, kazdopadne by mela vest k zavreni spojeni a o nic jineho ted nejde)
                }
            }
            else
            {
                if (event == fweDiskWorkListFinished) // mame vysledek diskove operace (listing adresare)
                {
                    DiskWorkIsUsed = FALSE;
                    ReportWorkerMayBeClosed(); // ohlasime dokonceni prace workera (pro ostatni cekajici thready)

                    // pokud uz jsme QUIT poslali, musime zamezit dalsimu poslani QUIT z noveho stavu
                    quitCmdWasSent = SubState == fwssWorkUploadListDiskWaitForDiskAftQuit;

                    if (DiskWork.State == sqisNone)
                    { // adresar se podarilo vylistovat
                        if (!HaveWorkingPath)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState4(): HaveWorkingPath is FALSE!");
                        if (DiskWork.DiskListing == NULL)
                            TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState4(): DiskWork.DiskListing is NULL!");

                        TIndirectArray<CFTPQueueItem>* ftpQueueItems = new TIndirectArray<CFTPQueueItem>(100, 500);
                        int transferMode = Oper->GetTransferMode();
                        BOOL copy = CurItem->Type == fqitUploadCopyExploreDir;
                        CQuadWord totalSize(0, 0); // celkova velikost (v bytech)
                        char sourcePath[MAX_PATH];
                        lstrcpyn(sourcePath, CurItem->Path, MAX_PATH);

                        BOOL err = ftpQueueItems == NULL || !HaveWorkingPath || DiskWork.DiskListing == NULL ||
                                   !SalamanderGeneral->SalPathAppend(sourcePath, CurItem->Name, MAX_PATH) /* always true - error by hlasilo uz DoListDirectory() */;
                        if (!err) // napridavame do fronty polozky pro soubory/adresare z listingu
                        {
                            BOOL ok = TRUE;
                            CFTPServerPathType workingPathType = Oper->GetFTPServerPathType(WorkingPath);
                            BOOL is_AS_400_QSYS_LIB_Path = workingPathType == ftpsptAS400 &&
                                                           FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", WorkingPath);
                            int i;
                            for (i = 0; i < DiskWork.DiskListing->Count; i++)
                            {
                                CDiskListingItem* lstItem = DiskWork.DiskListing->At(i);

                                char mbrName[2 * MAX_PATH];
                                char* tgtName = lstItem->Name;
                                if (is_AS_400_QSYS_LIB_Path)
                                {
                                    lstrcpyn(mbrName, tgtName, MAX_PATH);
                                    FTPAS400AddFileNamePart(mbrName);
                                    tgtName = mbrName;
                                }

                                CFTPQueueItemType type;
                                CFTPQueueItem* item = CreateItemForCopyOrMoveUploadOperation(lstItem->Name, lstItem->IsDir,
                                                                                             &lstItem->Size, &type,
                                                                                             transferMode, Oper, copy,
                                                                                             WorkingPath, tgtName, // jsme v podadresari, tady uz se jmena nevyrabi z operacni masky
                                                                                             &totalSize, workingPathType == ftpsptOpenVMS);
                                if (item != NULL)
                                {
                                    if (ok)
                                    {
                                        item->SetItem(-1, type, sqisWaiting, ITEMPR_OK, sourcePath, lstItem->Name);
                                        ftpQueueItems->Add(item); // pridani operace do fronty
                                        if (!ftpQueueItems->IsGood())
                                        {
                                            ftpQueueItems->ResetState();
                                            ok = FALSE;
                                        }
                                    }
                                    if (!ok)
                                        delete item;
                                }
                                else
                                {
                                    TRACE_E(LOW_MEMORY);
                                    ok = FALSE;
                                }
                                // zjistime jestli ma cenu pokracovat (pokud neni chyba)
                                if (!ok)
                                {
                                    err = TRUE;
                                    break;
                                }
                            }
                        }
                        BOOL parentItemAdded = FALSE;       // TRUE = na konci ftpQueueItems je "parent" polozka (smazani zdrojoveho adresare)
                        int parentUID = CurItem->ParentUID; // parent-UID pro polozky vznikle expanzi adresare
                        if (!err && !copy)                  // je-li potreba, pridame jeste polozku pro smazani zdrojoveho adresare (spusti se az se presune cely obsah adresare)
                        {
                            CFTPQueueItem* item = new CFTPQueueItemDir;
                            if (item != NULL && ((CFTPQueueItemDir*)item)->SetItemDir(0, 0, 0, 0))
                            {
                                item->SetItem(CurItem->ParentUID, fqitUploadMoveDeleteDir, sqisWaiting,
                                              ITEMPR_OK, CurItem->Path, CurItem->Name);
                                ftpQueueItems->Add(item); // pridani operace do fronty
                                if (!ftpQueueItems->IsGood())
                                {
                                    ftpQueueItems->ResetState();
                                    delete item;
                                    err = TRUE;
                                }
                                else
                                {
                                    parentItemAdded = TRUE;
                                    parentUID = item->UID;
                                }
                            }
                            else
                            {
                                if (item == NULL)
                                    TRACE_E(LOW_MEMORY);
                                else
                                    delete item;
                                err = TRUE;
                            }
                        }
                        if (!err)
                        {
                            int count = ftpQueueItems->Count - (parentItemAdded ? 1 : 0);
                            int childItemsNotDone = count;
                            int i;
                            for (i = 0; i < count; i++) // nastavime parenty pro polozky vznikle explorem
                            {
                                CFTPQueueItem* actItem = ftpQueueItems->At(i);
                                actItem->ParentUID = parentUID;
                            }

                            // pokud pridavame "parent" polozku, nastavime do ni pocty Skipped+Failed+NotDone
                            if (parentItemAdded)
                            {
                                CFTPQueueItemDir* parentItem = (CFTPQueueItemDir*)(ftpQueueItems->At(ftpQueueItems->Count - 1)); // musi byt nutne potomek CFTPQueueItemDir (kazda "parent" polozka ma pocty Skipped+Failed+NotDone)
                                parentItem->SetStateAndNotDoneSkippedFailed(childItemsNotDone, 0, 0, 0);
                                // nyni uz vsechny nove polozky reprezentuje jen "parent" polozka -> ulozime nove
                                // NotDone pouze pro tuto polozku
                                childItemsNotDone = 1;
                            }

                            int curItemParent = CurItem->ParentUID;

                            // probiha vice operaci nad daty, ostatni musi pockat az se provedou vsechny,
                            // jinak budou pracovat s nekonzistentnimi daty
                            Queue->LockForMoreOperations();

                            if (Queue->ReplaceItemWithListOfItems(CurItem->UID, ftpQueueItems->GetData(),
                                                                  ftpQueueItems->Count))
                            { // CurItem uz je dealokovana, byla nahrazena seznamem polozek ftpQueueItems
                                CurItem = NULL;
                                ftpQueueItems->DetachMembers(); // polozky uz jsou ve fronte, musime je vyhodit z pole, jinak se dealokuji

                                // polozce/operaci CurItem->ParentUID snizime o jednu NotDone (za CurItem ve stavu
                                // sqisProcessing) a zvysime NotDone podle childItemsNotDone
                                childItemsNotDone--; // snizeni o jednu za CurItem
                                Oper->AddToItemOrOperationCounters(curItemParent, childItemsNotDone, 0, 0, 0, FALSE);

                                Queue->UnlockForMoreOperations();

                                // zvysime celkovou velikost prenasenych dat o velikost z novych polozek
                                Oper->AddToTotalSize(totalSize, TRUE);

                                Oper->ReportItemChange(-1); // pozadame o redraw vsech polozek

                                // tento worker si bude muset najit dalsi praci
                                State = fwsLookingForWork;                                 // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                                SubState = quitCmdWasSent ? fwssLookFWQuitSent : fwssNone; // pripadne predame do fwsLookingForWork, ze QUIT uz byl poslany
                                postActivate = TRUE;                                       // postneme si aktivaci pro dalsi stav workera
                                reportWorkerChange = TRUE;

                                // informujeme vsechny pripadne spici workery, ze se objevila nova prace
                                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                                Oper->PostNewWorkAvailable(FALSE);
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                            }
                            else
                            {
                                err = TRUE; // nedostatek pameti -> vepiseme chybu do polozky
                                Queue->UnlockForMoreOperations();
                            }
                        }
                        if (err)
                        { // chyba na polozce, zapiseme do ni tento stav
                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                            lookForNewWork = TRUE; // quitCmdWasSent uz je nastaveny
                        }
                        if (ftpQueueItems != NULL)
                            delete ftpQueueItems;
                    }
                    else // pri vytvareni adresare nastala chyba nebo doslo ke skipnuti polozky
                    {
                        Queue->UpdateItemState(CurItem, DiskWork.State, DiskWork.ProblemID, DiskWork.WinError, NULL, Oper);
                        lookForNewWork = TRUE; // quitCmdWasSent uz je nastaveny
                    }
                    if (DiskWork.DiskListing != NULL) // pokud mame listing, dealokujeme ho zde (mel by existovat jen pri uspesnem vylistovani)
                    {
                        delete DiskWork.DiskListing;
                        DiskWork.DiskListing = NULL;
                    }
                }
            }
            break;
        }
        }
        if (!nextLoop)
            break;
    }
    if (existingItem != NULL)
    {
        free(existingItem->Name);
        delete existingItem;
    }
}
