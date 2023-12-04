// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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

    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, jsou tato volani
    // mozna i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
    GetLocalIP(&localIP, NULL); // snad ani nemuze vratit chybu
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
        if (WorkerDataCon != NULL)
        {
            HANDLES(LeaveCriticalSection(&WorkerCritSect));
            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
            DeleteSocket(WorkerDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
            WorkerDataCon = NULL;
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
}

void CFTPWorker::WaitForListen(CFTPWorkerEvent event, BOOL& handleShouldStop, char* errBuf,
                               char* buf, int& cmdLen, BOOL& sendCmd, BOOL& conClosedRetryItem,
                               CFTPWorkerSubState waitForPORTRes)
{
    if (ShouldStop)
    {
        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
        // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
        SocketsThread->DeleteTimer(UID, WORKER_LISTENTIMEOUTTIMID);

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
        BOOL needRetry = FALSE;
        switch (event)
        {
        case fweDataConListeningForCon:
        {
            if (WorkerDataCon != NULL) // "always true" (jinak by udalost 'fweDataConListeningForCon' vubec nevznikla)
            {
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
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
                                      ftpcmdSetPort, &cmdLen, listenOnIP, listenOnPort); // nemuze nahlasit chybu
                    sendCmd = TRUE;
                    SubState = waitForPORTRes;
                }
                else // chyba pri otevirani "listen" portu na proxy serveru - provedeme retry...
                {
                    // zavreme data-connectionu
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

                    // pripravime text chyby (timeoutu) do 'ErrorDescr'
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
            // zavreme data-connectionu + pripravime text chyby (timeoutu) do 'ErrorDescr'
            errBuf[0] = 0;
            if (WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                if (!WorkerDataCon->GetProxyTimeoutDescr(errBuf, 50 + FTP_MAX_PATH))
                    errBuf[0] = 0;
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
}

void CFTPWorker::WaitForPASVRes(CFTPWorkerEvent event, char* reply, int replySize, int replyCode,
                                BOOL& handleShouldStop, BOOL& nextLoop, BOOL& conClosedRetryItem,
                                CFTPWorkerSubState setType, CFTPWorkerSubState openActDataCon)
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
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    DeleteSocket(WorkerDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
            }
            else
            {
                int logUID = LogUID;
                HANDLES(LeaveCriticalSection(&WorkerCritSect));

                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, jsou tato volani
                // mozna i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                if (WorkerDataCon != NULL)
                {
                    WorkerDataCon->SetPassive(ip, port, logUID);
                    WorkerDataCon->PassiveConnect(NULL); // prvni pokus, vysledek nas nezajima (testuje se pozdeji)
                }

                HANDLES(EnterCriticalSection(&WorkerCritSect));
                WorkerDataConState = wdcsWaitingForConnection;

                nextLoop = TRUE;
                SubState = setType;
            }
        }
        else // pasivni rezim neni podporovan, zkusime jeste aktivni rezim
        {
            Oper->SetUsePassiveMode(FALSE);
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);

            if (ShouldStop)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    DeleteSocket(WorkerDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
            }
            else
            {
                nextLoop = TRUE;
                SubState = openActDataCon;
            }
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
            DeleteSocket(WorkerDataCon); // zatim nedoslo ke spojeni, bude se jen dealokovat
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
    // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
    case fweCmdReplyReceived:
    {
        nextLoop = TRUE;
        SubState = setType;
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
        if (CurrentTransferMode != trMode) // potrebujeme ASCII rezim, pripadne ho nastavime
        {
            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                              ftpcmdSetTransferMode, &cmdLen, asciiTrMode); // nemuze nahlasit chybu
            sendCmd = TRUE;
            SubState = waitForTYPERes;
        }
        else // ASCII rezim jiz je nastaveny
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
    // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
    case fweCmdReplyReceived:
    {
        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS) // vraci se uspech (melo by byt 200)
            CurrentTransferMode = trMode;             // prenosovy rezim byl zmenen
        else
            CurrentTransferMode = ctrmUnknown; // neznama chyba, nemusi vubec vadit, ale nebudeme "cachovat" rezim prenosu dat

        nextLoop = TRUE;
        SubState = trModeAlreadySet;
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
}

void CFTPWorker::HandleEventInWorkingState2(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                            BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                            int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                            int replyCode, char* ftpPath, char* errText,
                                            BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                            BOOL& handleShouldStop, BOOL* listingNotAccessible)
{
    // POZOR: tato metoda se pouziva i pro listovani cilove cesty pri Uploadu (UploadDirGetTgtPathListing==TRUE)!!!
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
        case fwssWorkStartWork: // zjistime na jakou cestu se mame prepnout na serveru a posleme CWD
        {
            // pred explorem adresare pro mazani/change-attr a listovanim cesty pro upload musime vynulovat
            // merak rychlosti (rychlost explore ani upload-listovani se nemeri) - dojde k zobrazeni
            // "(unknown)" time-left v operation-dialogu
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
            { // mame cestu, posleme na server CWD do zkoumaneho adresare
                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGLISTINGPATH), ftpPath);
                Logs.LogMessage(LogUID, errText, -1, TRUE);

                PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                  ftpcmdChangeWorkingPath, &cmdLen, ftpPath); // nemuze nahlasit chybu
                sendCmd = TRUE;
                SubState = fwssWorkExplWaitForCWDRes;

                HaveWorkingPath = FALSE; // menime aktualni pracovni cestu na serveru
            }
            else // chyba syntaxe cesty nebo by vznikla moc dlouha cesta
            {
                // chyba na polozce, zapiseme do ni tento stav
                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_INVALIDPATHTODIR, NO_ERROR, NULL, Oper);
                if (listingNotAccessible != NULL)
                    *listingNotAccessible = TRUE;
                lookForNewWork = TRUE;
            }
            break;
        }

        case fwssWorkExplWaitForCWDRes: // explore-dir: cekame na vysledek "CWD" (zmena cesty do zkoumaneho adresare)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                { // uspech, zjistime novou aktualni cestu na serveru (posleme PWD)
                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        if (UploadDirGetTgtPathListing) // pri uploadu nema PWD smysl (netestujeme zacykleni pri prochazeni cest)
                        {
                            SubState = fwssWorkExplWaitForPWDRes;
                            nextLoop = TRUE;
                        }
                        else
                        {
                            PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                              ftpcmdPrintWorkingPath, &cmdLen); // nemuze nahlasit chybu
                            sendCmd = TRUE;
                            SubState = fwssWorkExplWaitForPWDRes;
                        }
                    }
                }
                else // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                {
                    CopyStr(errText, 200 + FTP_MAX_PATH, reply, replySize);
                    Queue->UpdateItemState(CurItem, sqisFailed,
                                           UploadDirGetTgtPathListing ? ITEMPR_UNABLETOCWDONLYPATH : ITEMPR_UNABLETOCWD,
                                           NO_ERROR, SalamanderGeneral->DupStr(errText) /* low memory = chyba bude bez detailu */,
                                           Oper);
                    if (listingNotAccessible != NULL)
                        *listingNotAccessible = TRUE;
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

        case fwssWorkExplWaitForPWDRes: // explore-dir: cekame na vysledek "PWD" (zjisteni pracovni cesty zkoumaneho adresare)
        {
            switch (event)
            {
            // case fweCmdInfoReceived:  // "1xx" odpovedi ignorujeme (jen se pisou do Logu)
            case fweCmdReplyReceived:
            {
                BOOL pwdErr = FALSE;
                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                { // uspech, precteme vysledek PWD - POZOR: je-li UploadDirGetTgtPathListing==TRUE, jde o vysledek CWD (nikoli PWD) - PWD neni potreba
                    if (ShouldStop)
                        handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
                    else
                    {
                        if (UploadDirGetTgtPathListing || FTPGetDirectoryFromReply(reply, replySize, ftpPath, FTP_MAX_PATH))
                        { // mame working path, zkusime, jestli nedochazi k zacykleni (nekonecna smycka)
                            BOOL cycle = FALSE;
                            if (!UploadDirGetTgtPathListing)
                            {
                                lstrcpyn(WorkingPath, ftpPath, FTP_MAX_PATH);
                                HaveWorkingPath = TRUE;

                                // zkontrolujeme jestli se cesta nezkratila (skok do nadrazeneho adresare = garance nekonecneho cyklu)
                                lstrcpyn(ftpPath, CurItem->Path, FTP_MAX_PATH);
                                CFTPServerPathType type = Oper->GetFTPServerPathType(ftpPath);
                                if (FTPPathAppend(type, ftpPath, FTP_MAX_PATH, CurItem->Name, TRUE))
                                { // test provedeme jen pokud se povede slozeni cesty - "always true"
                                    if (!FTPIsTheSameServerPath(type, WorkingPath, ftpPath) &&
                                        FTPIsPrefixOfServerPath(type, WorkingPath, ftpPath))
                                    { // jde o skok do nadrazeneho adresare
                                        cycle = TRUE;
                                    }
                                }
                                // zkontrolujeme, jestli uz jsme tuto cestu nenavstivili, coz by znamenalo vstup do nekonecneho cyklu
                                if (!cycle && Oper->IsAlreadyExploredPath(WorkingPath))
                                    cycle = TRUE;
                            }

                            if (cycle)
                            {
                                // chyba na polozce, zapiseme do ni tento stav
                                Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_DIREXPLENDLESSLOOP, NO_ERROR, NULL, Oper);
                                lookForNewWork = TRUE;
                            }
                            else // vse OK, naalokujeme data-connectionu
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
                                                                           WORKER_DATACON_CLOSED, -1 /* pri listovani neni potreba */,
                                                                           WORKER_DATACON_LISTENINGFORCON);
                                    WorkerDataCon->SetGlobalLastActivityTime(Oper->GetGlobalLastActivityTime());
                                    // explore adresare pro mazani/change-attr a upload-listovani se nemeri (merak se pouziva
                                    // jinak, viz SMPLCMD_APPROXBYTESIZE + upload: merak je pro upload, ale tohle je download)
                                    if (CurItem->Type != fqitDeleteExploreDir &&
                                        CurItem->Type != fqitChAttrsExploreDir &&
                                        CurItem->Type != fqitChAttrsExploreDirLink &&
                                        !UploadDirGetTgtPathListing)
                                    {
                                        WorkerDataCon->SetGlobalTransferSpeedMeter(Oper->GetGlobalTransferSpeedMeter());
                                    }
                                    HANDLES(EnterCriticalSection(&WorkerCritSect));

                                    if (Oper->GetUsePassiveMode()) // pasivni mod (PASV)
                                    {
                                        PrepareFTPCommand(buf, 200 + FTP_MAX_PATH, errBuf, 50 + FTP_MAX_PATH,
                                                          ftpcmdPassive, &cmdLen); // nemuze nahlasit chybu
                                        sendCmd = TRUE;
                                        SubState = fwssWorkExplWaitForPASVRes;
                                    }
                                    else // aktivni mod (PORT)
                                    {
                                        nextLoop = TRUE;
                                        SubState = fwssWorkExplOpenActDataCon;
                                    }
                                }
                            }
                        }
                        else
                            pwdErr = TRUE; // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                    }
                }
                else
                    pwdErr = TRUE; // nastala nejaka chyba, vypiseme ji uzivateli a jdeme zpracovat dalsi polozku fronty
                if (pwdErr)
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

        case fwssWorkExplWaitForPASVRes: // explore-dir: cekame na vysledek "PASV" (zjisteni IP+port pro pasivni data-connectionu)
        {
            WaitForPASVRes(event, reply, replySize, replyCode, handleShouldStop, nextLoop, conClosedRetryItem,
                           fwssWorkExplSetTypeA, fwssWorkExplOpenActDataCon);
            break;
        }

        case fwssWorkExplOpenActDataCon: // explore-dir: otevreme aktivni data-connectionu
        {
            OpenActDataCon(fwssWorkExplWaitForListen, errBuf, conClosedRetryItem, lookForNewWork);
            break;
        }

        case fwssWorkExplWaitForListen: // explore-dir: cekame na otevreni "listen" portu (otevirame aktivni data-connectionu) - lokalniho nebo na proxy serveru
        {
            WaitForListen(event, handleShouldStop, errBuf, buf, cmdLen, sendCmd,
                          conClosedRetryItem, fwssWorkExplWaitForPORTRes);
            break;
        }

        case fwssWorkExplWaitForPORTRes: // explore-dir: cekame na vysledek "PORT" (predani IP+port serveru pro aktivni data-connectionu)
        {
            WaitForPORTRes(event, nextLoop, conClosedRetryItem, fwssWorkExplSetTypeA);
            break;
        }

        case fwssWorkExplSetTypeA: // explore-dir: nastavime prenosovy rezim na ASCII
        {
            SetTypeA(handleShouldStop, errBuf, buf, cmdLen, sendCmd, nextLoop, ctrmASCII, TRUE,
                     fwssWorkExplWaitForTYPERes, fwssWorkExplSendListCmd);
            break;
        }

        case fwssWorkExplWaitForTYPERes: // explore-dir: cekame na vysledek "TYPE" (prepnuti do ASCII rezimu prenosu dat)
        {
            WaitForTYPERes(event, replyCode, nextLoop, conClosedRetryItem, ctrmASCII, fwssWorkExplSendListCmd);
            break;
        }

        case fwssWorkExplSendListCmd: // explore-dir: posleme prikaz LIST
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
                    DeleteSocket(WorkerDataCon);
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
            }
            else
            {
                // ziskame datum, kdy byl listing vytvoren (predpokladame, ze jej server nejdrive vytvori, pak az posle)
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

        case fwssWorkExplActivateDataCon: // explore-dir: aktivujeme data-connectionu (tesne po poslani prikazu LIST)
        {
            if (WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                WorkerDataCon->ActivateConnection();
                HANDLES(EnterCriticalSection(&WorkerCritSect));
            }
            SubState = fwssWorkExplWaitForLISTRes;
            if (event != fweActivate)
                nextLoop = TRUE; // pokud neslo jen o fweActivate, dorucime udalost do stavu fwssWorkExplWaitForLISTRes
            break;
        }

        case fwssWorkExplWaitForLISTRes: // explore-dir: cekame na vysledek "LIST" (cekame na konec prenosu dat listingu)
        {
            switch (event)
            {
            case fweCmdInfoReceived: // "1xx" odpovedi obsahuji velikost prenasenych dat
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                if (WorkerDataCon != NULL)
                {
                    WorkerDataCon->EncryptPassiveDataCon();
                    CQuadWord size;
                    if (FTPGetDataSizeInfoFromSrvReply(size, reply, replySize))
                        WorkerDataCon->SetDataTotalSize(size); // mame celkovou velikost listingu
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
                ListCmdReplyText = SalamanderGeneral->DupStr(errText); /* low memory = obejdeme se bez popisu odpovedi */

                BOOL waitForDataConFinish = FALSE;
                if (!ShouldStop && WorkerDataCon != NULL)
                {
                    BOOL trFinished;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS ||
                        !WorkerDataCon->IsTransfering(&trFinished) && !trFinished)
                    { // server vraci chybu listingu nebo nedoslo k navazani spojeni
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        if (WorkerDataCon->IsConnected())
                        {                                       // zavreme "data connection", system se pokusi o "graceful"
                            WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)

                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
                            { // nedoslo k navazani spojeni + server hlasi uspech -> provedeme retry
                                DeleteSocket(WorkerDataCon);
                                WorkerDataCon = NULL;
                                HANDLES(EnterCriticalSection(&WorkerCritSect));
                                WorkerDataConState = wdcsDoesNotExist;
                                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONNOTOPENED), -1, TRUE);
                                Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
                                lookForNewWork = TRUE;
                                break;
                            }
                        }
                    }
                    else
                    {
                        if (WorkerDataCon->IsConnected()) // prenos dat jeste probiha, pockame na dokonceni
                            waitForDataConFinish = TRUE;
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

                // pokud nemusime cekat na ukonceni "data connection", jdeme na zpracovani replyCode prikazu LIST
                SubState = waitForDataConFinish ? fwssWorkExplWaitForDataConFinish : fwssWorkExplProcessLISTRes;
                if (!waitForDataConFinish)
                    nextLoop = TRUE;
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

        case fwssWorkExplWaitForDataConFinish: // explore-dir: cekame na ukonceni "data connection" (odpoved serveru na "LIST" uz prisla)
        {
            BOOL con = FALSE;
            if (WorkerDataCon != NULL)
            {
                HANDLES(LeaveCriticalSection(&WorkerCritSect));
                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                con = WorkerDataCon->IsConnected();
                HANDLES(EnterCriticalSection(&WorkerCritSect));
            }
            if (WorkerDataCon == NULL || !con) // bud "data connection" neexistuje nebo uz je uzavrena
            {
                nextLoop = TRUE;
                SubState = fwssWorkExplProcessLISTRes;
            }
            break;
        }

        case fwssWorkExplProcessLISTRes: // explore-dir: zpracovani vysledku "LIST" (az po ukonceni "data connection" a zaroven prijeti odpovedi serveru na "LIST")
        {
            BOOL delOrChangeAttrExpl = CurItem->Type == fqitDeleteExploreDir ||
                                       CurItem->Type == fqitChAttrsExploreDir ||
                                       CurItem->Type == fqitChAttrsExploreDirLink;
            BOOL uploadFinished = FALSE;
            // vysledek prikazu "LIST" je v 'ListCmdReplyCode' a 'ListCmdReplyText'
            if (ShouldStop)
            {
                if (WorkerDataCon != NULL)
                {
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    if (WorkerDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                    DeleteSocket(WorkerDataCon);
                    WorkerDataCon = NULL;
                    HANDLES(EnterCriticalSection(&WorkerCritSect));
                    WorkerDataConState = wdcsDoesNotExist;
                }

                handleShouldStop = TRUE; // zkontrolujeme jestli se nema stopnout worker
            }
            else
            {
                if (WorkerDataCon != NULL) // "always true"
                {
                    // VMS (cs.felk.cvut.cz) hlasi chybu i u prazdneho adresare (nelze povazovat za chybu)
                    BOOL isVMSFileNotFound = ListCmdReplyText != NULL && FTPIsEmptyDirListErrReply(ListCmdReplyText);
                    int listCmdReplyCode = ListCmdReplyCode;
                    DWORD err;
                    BOOL lowMem, noDataTransTimeout;
                    int sslErrorOccured;
                    HANDLES(LeaveCriticalSection(&WorkerCritSect));
                    // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                    // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                    if (WorkerDataCon->IsConnected()) // zavreme "data connection", system se pokusi o "graceful"
                    {
                        WorkerDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
                        TRACE_E("Unexpected situation in CFTPWorker::HandleEventInWorkingState2(): data connection has left opened!");
                    }
                    WorkerDataCon->GetError(&err, &lowMem, NULL, &noDataTransTimeout, &sslErrorOccured, NULL);
                    if (!WorkerDataCon->GetProxyError(errBuf, 50 + FTP_MAX_PATH, NULL, 0, TRUE))
                        errBuf[0] = 0;
                    if (lowMem) // "data connection" hlasi nedostatek pameti ("always false")
                    {
                        // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                        // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
                        DeleteSocket(WorkerDataCon);
                        WorkerDataCon = NULL;
                        HANDLES(EnterCriticalSection(&WorkerCritSect));
                        WorkerDataConState = wdcsDoesNotExist;

                        // chyba na polozce, zapiseme do ni tento stav
                        Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                        lookForNewWork = TRUE;
                    }
                    else
                    {
                        if (err != NO_ERROR && !IsConnected())
                        { // odpoved na LIST sice prisla, ale pak pri cekani na dokonceni prenosu pres data-connection
                            // doslo k preruseni spojeni (data-connectiona i control-connectiona) -> RETRY

                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
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
                                // prevezmeme data z "data connection"
                                allocatedListing = WorkerDataCon->GiveData(&allocatedListingLen, &decomprErr);
                                if (decomprErr) // pri chybe dekomprese vysledek zahodime + vypiseme chybu
                                {
                                    listingIsNotOK = TRUE;
                                    allocatedListingLen = 0;
                                    free(allocatedListing);
                                    allocatedListing = NULL;
                                }
                            }
                            // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je toto volani
                            // mozne i ze sekce CSocket::SocketCritSect (nehrozi dead-lock)
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
                                    BOOL quickRetry = FALSE;
                                    if (FTP_DIGIT_1(listCmdReplyCode) == FTP_D1_TRANSIENTERROR &&
                                            (FTP_DIGIT_2(listCmdReplyCode) == FTP_D2_CONNECTION ||          // hlavne "426 data connection closed, transfer aborted" (je-li to adminem serveru nebo poruchou spojeni nejsem sto rozpoznat, takze prioritu dostava porucha spojeni -> opakujeme pokus o download)
                                             FTP_DIGIT_2(listCmdReplyCode) == FTP_D2_FILESYSTEM) &&         // "450 Transfer aborted.  Link to file server lost."
                                            sslErrorOccured != SSLCONERR_DONOTRETRY ||                      // 426 a 450 bereme jen pokud nebyly vyvolane chybou: nepodarilo se zasifrovat spojeni, jde o permanentni problem
                                        noDataTransTimeout ||                                               // nami prerusene spojeni kvuli no-data-transfer timeoutu (deje se pri "50%" vypadcich site, data-connectiona se neprerusi, ale nejak se zablokuje prenos dat, vydrzi otevrena klidne 14000 sekund, tohle by to melo resit) -> opakujeme pokus o download
                                        sslErrorOccured == SSLCONERR_CANRETRY ||                            // nepodarilo se zasifrovat spojeni, nejde o permanentni problem
                                        FTP_DIGIT_1(listCmdReplyCode) == FTP_D1_SUCCESS && err != NO_ERROR) // datove spojeni prerusene po prijmu "uspesne" odpovedi ze serveru (chyba pri cekani na dokonceni prenosu dat), tohle se delo na ftp.simtel.net (sest spojeni najednou + vypadky packetu)
                                    {
                                        Queue->UpdateItemState(CurItem, sqisWaiting, ITEMPR_OK, NO_ERROR, NULL, Oper); // aspon tento worker pujde hledat novou praci, takze o tuto polozku se jiste nejaky worker postara (netreba postit "new work available")
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
                                            { // pokud nemame popis sitove chyby ze serveru, spokojime se se systemovym popisem
                                                lstrcpyn(errText, ListCmdReplyText, 200 + FTP_MAX_PATH);
                                            }

                                            if (errText[0] == 0 && errBuf[0] != 0) // zkusime jeste vzit text chyby z proxy serveru
                                                lstrcpyn(errText, errBuf, 200 + FTP_MAX_PATH);

                                            if (errText[0] == 0 && decomprErr)
                                                lstrcpyn(errText, LoadStr(IDS_ERRDATACONDECOMPRERROR), 200 + FTP_MAX_PATH);
                                        }

                                        // chyba na polozce, zapiseme do ni tento stav
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
                            else // listing se povedl a je kompletni
                            {
                                // ulozime datum, kdy byl listing vytvoren
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
                                if (!delOrChangeAttrExpl && !UploadDirGetTgtPathListing) // pri mazani, zmene atributu a pri uploadu listingy do cache nedavame, protoze se budou hned menit
                                {                                                        // jen download: pokud chce user pouzivat cache, pridame nove nacteny listing do cache
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

                                if (UploadDirGetTgtPathListing) // upload-listing: vlozime listing do cache
                                {
                                    err2 |= welcomeReply == NULL || systReply == NULL;
                                    if (!err2)
                                    {
                                        unsigned short port;
                                        Oper->GetUserHostPort(userTmp, host, &port);
                                        // volani UploadListingCache.ListingFinished() je mozne jen proto, ze jsme v sekci CSocketsThread::CritSect
                                        err2 = !UploadListingCache.ListingFinished(userTmp, host, port, tgtPath,
                                                                                   pathType, allocatedListing, allocatedListingLen,
                                                                                   listingDate, welcomeReply, systReply,
                                                                                   listingServerType[0] != 0 ? listingServerType : NULL);
                                        uploadFinished = !err2;
                                    }
                                }
                                else // download + mazani + change-attrs: rozparsujeme listing a pridame nove polozky do fronty
                                {
                                    BOOL isVMS = pathType == ftpsptOpenVMS;
                                    BOOL isAS400 = pathType == ftpsptAS400;
                                    TIndirectArray<CFTPQueueItem>* ftpQueueItems = new TIndirectArray<CFTPQueueItem>(100, 500);
                                    BOOL needSimpleListing = TRUE;
                                    int transferMode = Oper->GetTransferMode(); // parametr pro operace Copy a Move
                                    BOOL selFiles, selDirs, includeSubdirs;     // parametry pro operaci Change Attributes
                                    DWORD attrAndMask, attrOrMask;
                                    int operationsUnknownAttrs;
                                    Oper->GetParamsForChAttrsOper(&selFiles, &selDirs, &includeSubdirs, &attrAndMask,
                                                                  &attrOrMask, &operationsUnknownAttrs);
                                    int operationsHiddenFileDel;
                                    int operationsHiddenDirDel;
                                    Oper->GetParamsForDeleteOper(NULL, &operationsHiddenFileDel, &operationsHiddenDirDel);

                                    // promenne pro Copy a Move operace:
                                    CQuadWord totalSize(0, 0); // celkova velikost (v bytech nebo blocich)
                                    BOOL sizeInBytes = TRUE;   // TRUE/FALSE = velikosti v bytech/blocich (na jednom listingu se nemuze stridat - viz CFTPListingPluginDataInterface::GetSize())

                                    // nulovani pomocne promenne pro urceni ktery typ serveru jiz byl (neuspesne) vyzkousen
                                    CServerTypeList* serverTypeList = Config.LockServerTypeList();
                                    int serverTypeListCount = serverTypeList->Count;
                                    int j;
                                    for (j = 0; j < serverTypeListCount; j++)
                                        serverTypeList->At(j)->ParserAlreadyTested = FALSE;

                                    CServerType* serverType = NULL;
                                    err2 |= ftpQueueItems == NULL || !HaveWorkingPath;
                                    if (!err2)
                                    {
                                        if (listingServerType[0] != 0) // nejde o autodetekci, najdeme listingServerType
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
                                                    // serverType je vybrany, jdeme vyzkouset jeho parser na listingu
                                                    serverType->ParserAlreadyTested = TRUE;
                                                    if (ParseListingToFTPQueue(ftpQueueItems, allocatedListing, allocatedListingLen,
                                                                               serverType, &err2, isVMS, isAS400, transferMode, &totalSize,
                                                                               &sizeInBytes, selFiles, selDirs,
                                                                               includeSubdirs, attrAndMask, attrOrMask,
                                                                               operationsUnknownAttrs, operationsHiddenFileDel,
                                                                               operationsHiddenDirDel))
                                                    {
                                                        needSimpleListing = FALSE; // uspesne jsme rozparsovali listing
                                                    }
                                                    break; // nasli jsme pozadovany typ serveru, koncime
                                                }
                                            }
                                            if (i == serverTypeListCount)
                                                listingServerType[0] = 0; // listingServerType neexistuje -> probehne autodetekce
                                        }

                                        // autodetekce - vyber typu serveru se splnenou autodetekcni podminkou
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
                                                    if (!serverType->ParserAlreadyTested) // jen pokud jsme ho uz nezkouseli
                                                    {
                                                        if (serverType->CompiledAutodetCond == NULL)
                                                        {
                                                            serverType->CompiledAutodetCond = CompileAutodetectCond(HandleNULLStr(serverType->AutodetectCond),
                                                                                                                    NULL, NULL, NULL, NULL, 0);
                                                            if (serverType->CompiledAutodetCond == NULL) // muze byt jen chyba nedostatku pameti
                                                            {
                                                                err2 = TRUE;
                                                                break;
                                                            }
                                                        }
                                                        if (serverType->CompiledAutodetCond->Evaluate(welcomeReply, welcomeReplyLen,
                                                                                                      systReply, systReplyLen))
                                                        {
                                                            // serverType je vybrany, jdeme vyzkouset jeho parser na listingu
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
                                                                needSimpleListing = err2; // uspesne jsme rozparsovali listing nebo doslo k chybe nedostatku pameti, koncime
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // autodetekce - vyber zbyvajicich typu serveru
                                            if (!err2 && needSimpleListing)
                                            {
                                                int i;
                                                for (i = 0; i < serverTypeListCount; i++)
                                                {
                                                    serverType = serverTypeList->At(i);
                                                    if (!serverType->ParserAlreadyTested) // jen pokud jsme ho uz nezkouseli
                                                    {
                                                        // serverType je vybrany, jdeme vyzkouset jeho parser na listingu
                                                        // serverType->ParserAlreadyTested = TRUE;  // zbytecne, dale se nepouziva
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
                                                            needSimpleListing = err2; // uspesne jsme rozparsovali listing nebo doslo k chybe nedostatku pameti, koncime
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
                                        if (needSimpleListing) // neznamy format listingu
                                        {                      // dame do logu "Unknown Server Type"
                                            lstrcpyn(errText, LoadStr(listingServerType[0] == 0 ? IDS_LOGMSGUNKNOWNSRVTYPE : IDS_LOGMSGUNKNOWNSRVTYPE2),
                                                     199 + FTP_MAX_PATH);
                                            Logs.LogMessage(LogUID, errText, -1, TRUE);

                                            // chyba na polozce, zapiseme do ni tento stav
                                            Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_UNABLETOPARSELISTING, NO_ERROR, NULL, Oper);
                                            lookForNewWork = TRUE;
                                        }
                                        else // dame do logu cim jsme to rozparsovali
                                        {
                                            if (listingServerType[0] != 0) // "always true"
                                            {
                                                _snprintf_s(errText, 200 + FTP_MAX_PATH, _TRUNCATE, LoadStr(IDS_LOGMSGPARSEDBYSRVTYPE), listingServerType);
                                                Logs.LogMessage(LogUID, errText, -1, TRUE);
                                            }

                                            BOOL nonEmptyDirSkipOrAsk = FALSE;
                                            if (CurItem->Type == fqitDeleteExploreDir && ftpQueueItems->Count > 0 &&
                                                ((CFTPQueueItemDelExplore*)CurItem)->IsTopLevelDir)
                                            { // mazani neprazdneho adresare - zkontrolujeme jak si user preje, aby se to chovalo
                                                int confirmDelOnNonEmptyDir;
                                                Oper->GetParamsForDeleteOper(&confirmDelOnNonEmptyDir, NULL, NULL);
                                                switch (confirmDelOnNonEmptyDir)
                                                {
                                                case NONEMPTYDIRDEL_USERPROMPT:
                                                {
                                                    // chyba na polozce, zapiseme do ni tento stav
                                                    Queue->UpdateItemState(CurItem, sqisUserInputNeeded, ITEMPR_DIRISNOTEMPTY, NO_ERROR, NULL, Oper);
                                                    lookForNewWork = TRUE;
                                                    nonEmptyDirSkipOrAsk = TRUE;
                                                    break;
                                                }

                                                case NONEMPTYDIRDEL_DELETEIT:
                                                    break;

                                                case NONEMPTYDIRDEL_SKIP:
                                                {
                                                    // chyba na polozce, zapiseme do ni tento stav
                                                    Queue->UpdateItemState(CurItem, sqisSkipped, ITEMPR_DIRISNOTEMPTY, NO_ERROR, NULL, Oper);
                                                    lookForNewWork = TRUE;
                                                    nonEmptyDirSkipOrAsk = TRUE;
                                                    break;
                                                }
                                                }
                                            }
                                            if (!nonEmptyDirSkipOrAsk)
                                            {
                                                // je-li treba pridame jeste "parent" polozku (napr. pri mazani jde o polozku pro
                                                // vymaz adresare po dokonceni vymazu vsech souboru/linku/podadresaru v adresari)
                                                CFTPQueueItemType type = fqitNone;
                                                BOOL ok = TRUE;
                                                CFTPQueueItem* item = NULL;
                                                CFTPQueueItemState state = sqisWaiting;
                                                DWORD problemID = ITEMPR_OK;
                                                BOOL skip = TRUE; // TRUE pokud se vubec nema soubor/adresar/link zpracovavat
                                                switch (CurItem->Type)
                                                {
                                                case fqitDeleteExploreDir:   // explore adresare pro delete (pozn.: linky na adresare mazeme jako celek, ucel operace se splni a nesmaze se nic "navic") (objekt tridy CFTPQueueItemDelExplore)
                                                case fqitMoveExploreDir:     // explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
                                                case fqitMoveExploreDirLink: // explore linku na adresar pro presun (po dokonceni smaze link na adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
                                                {
                                                    skip = FALSE;
                                                    type = (CurItem->Type == fqitMoveExploreDir ? fqitMoveDeleteDir : (CurItem->Type == fqitDeleteExploreDir ? fqitDeleteDir : fqitMoveDeleteDirLink));

                                                    item = new CFTPQueueItemDir;
                                                    if (item != NULL && !((CFTPQueueItemDir*)item)->SetItemDir(0, 0, 0, 0))
                                                        ok = FALSE;
                                                    break;
                                                }

                                                case fqitChAttrsExploreDir: // explore adresare pro zmenu atributu (prida i polozku pro zmenu atributu adresare) (objekt tridy CFTPQueueItemChAttrExplore)
                                                {
                                                    if (selDirs) // resime jen pokud se maji nastavit atributy u zkoumaneho adresare
                                                    {
                                                        skip = FALSE;
                                                        type = fqitChAttrsDir;
                                                        const char* rights = ((CFTPQueueItemChAttrExplore*)CurItem)->OrigRights;

                                                        // vypocteme nova prava pro soubor/adresar
                                                        DWORD actAttr;
                                                        DWORD attrDiff = 0;
                                                        BOOL attrErr = FALSE;
                                                        if (rights != NULL && GetAttrsFromUNIXRights(&actAttr, &attrDiff, rights))
                                                        {
                                                            DWORD changeMask = (~attrAndMask | attrOrMask) & 0777;
                                                            if ((attrDiff & changeMask) == 0 &&                                                  // nemame zmenit zadny neznamy atribut
                                                                (actAttr & changeMask) == (((actAttr & attrAndMask) | attrOrMask) & changeMask)) // nemame zmenit zadny znamy atribut
                                                            {                                                                                    // neni co delat (zadna zmena atributu)
                                                                skip = TRUE;
                                                            }
                                                            else
                                                            {
                                                                if (((attrDiff & attrAndMask) & attrOrMask) != (attrDiff & attrAndMask))
                                                                {                        // prusvih, neznamy atribut ma byt zachovan, coz neumime
                                                                    actAttr |= attrDiff; // dame tam aspon 'x', kdyz uz neumime 's' nebo 't' nebo co to tam ted vlastne je (viz UNIXova prava)
                                                                    attrErr = TRUE;
                                                                }
                                                                actAttr = (actAttr & attrAndMask) | attrOrMask;
                                                            }
                                                        }
                                                        else // nezname prava
                                                        {
                                                            actAttr = attrOrMask; // predpokladame zadna prava (actAttr==0)
                                                            if (((~attrAndMask | attrOrMask) & 0777) != 0777)
                                                            { // prusvih, nezname prava a nejaky atribut ma byt zachovan (nezname jeho hodnotu -> neumime ho zachovat)
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
                                                                rights = NULL; // je-li vse OK, neni duvod si pamatovat puvodni prava

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

                                                    // zadna polozka se nemusi pridavat pro:
                                                    // case fqitCopyExploreDir:         // explore adresare nebo linku na adresar pro kopirovani (objekt tridy CFTPQueueItemCopyMoveExplore)
                                                    // case fqitChAttrsExploreDirLink:  // explore linku na adresar pro zmenu atributu (objekt tridy CFTPQueueItem)
                                                }

                                                BOOL parentItemAdded = FALSE;       // TRUE = na konci ftpQueueItems je "parent" polozka (napr. smazani adresare (Delete a Move), zmena atributu adresare (Change Attrs))
                                                int parentUID = CurItem->ParentUID; // parent-UID pro polozky vznikle expanzi adresare
                                                if (item != NULL)
                                                {
                                                    if (ok)
                                                    {
                                                        item->SetItem(CurItem->ParentUID, type, state, problemID, CurItem->Path, CurItem->Name);
                                                        ftpQueueItems->Add(item); // pridani operace do fronty
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
                                                    if (!skip) // jen pokud nejde o skipnuti polozky, ale o chybu nedostatku pameti
                                                    {
                                                        TRACE_E(LOW_MEMORY);
                                                        err2 = TRUE;
                                                    }
                                                }

                                                if (!err2)
                                                {
                                                    // pro polozky vznikle explorem adresare (netyka se pripadne "parent" polozky na konci pole):
                                                    // nastavime parenty + napocitame pocty ve stavech "Skipped", "Failed" a "jinych nez Done"
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

                                                    // pokud pridavame "parent" polozku, nastavime do ni pocty Skipped+Failed+NotDone
                                                    if (parentItemAdded)
                                                    {
                                                        CFTPQueueItemDir* parentItem = (CFTPQueueItemDir*)(ftpQueueItems->At(ftpQueueItems->Count - 1)); // musi byt nutne potomek CFTPQueueItemDir (kazda "parent" polozka ma pocty Skipped+Failed+NotDone)
                                                        parentItem->SetStateAndNotDoneSkippedFailed(childItemsNotDone, childItemsSkipped,
                                                                                                    childItemsFailed, childItemsUINeeded);
                                                        // nyni uz vsechny nove polozky reprezentuje jen "parent" polozka -> napocitame nove
                                                        // NotDone + Skipped + Failed + UINeeded pouze pro tuto polozku
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

                                                    // probiha vice operaci nad daty, ostatni musi pockat az se provedou vsechny,
                                                    // jinak budou pracovat s nekonzistentnimi daty
                                                    Queue->LockForMoreOperations();

                                                    if (Queue->ReplaceItemWithListOfItems(CurItem->UID, ftpQueueItems->GetData(),
                                                                                          ftpQueueItems->Count))
                                                    { // CurItem uz je dealokovana, byla nahrazena seznamem polozek ftpQueueItems
                                                        CurItem = NULL;
                                                        ftpQueueItems->DetachMembers(); // polozky uz jsou ve fronte, musime je vyhodit z pole, jinak se dealokuji

                                                        // tuto cestu budeme povazovat za uspesne projitou (sbirame cesty pro detekci jednoduchych cyklu)
                                                        if (HaveWorkingPath)
                                                            Oper->AddToExploredPaths(WorkingPath);

                                                        // polozce/operaci CurItem->ParentUID snizime o jednu NotDone (za CurItem ve stavu
                                                        // sqisProcessing) a zvysime NotDone + Skipped + Failed + UINeeded podle
                                                        // childItemsNotDone + childItemsSkipped + childItemsFailed + childItemsUINeeded
                                                        childItemsNotDone--; // snizeni o jednu za CurItem
                                                        Oper->AddToItemOrOperationCounters(curItemParent, childItemsNotDone,
                                                                                           childItemsSkipped, childItemsFailed,
                                                                                           childItemsUINeeded, FALSE);

                                                        Queue->UnlockForMoreOperations();

                                                        // zvysime celkovou velikost prenasenych dat o velikost z novych polozek
                                                        // (tyka se jen Copy a Move, jinak je velikost nulova)
                                                        Oper->AddToTotalSize(totalSize, sizeInBytes);

                                                        Oper->ReportItemChange(-1); // pozadame o redraw vsech polozek

                                                        // tento worker si bude muset najit dalsi praci
                                                        State = fwsLookingForWork; // neni nutne volat Oper->OperationStatusMaybeChanged(), nemeni stav operace (neni paused a nebude ani po teto zmene)
                                                        SubState = fwssNone;
                                                        postActivate = TRUE; // postneme si aktivaci pro dalsi stav workera
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
                                                        err2 = TRUE; // nedostatek pameti -> vepiseme chybu do polozky
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
                                    // chyba na polozce, zapiseme do ni tento stav
                                    Queue->UpdateItemState(CurItem, sqisFailed, ITEMPR_LOWMEM, NO_ERROR, NULL, Oper);
                                    lookForNewWork = TRUE;
                                }
                                if (welcomeReply != NULL)
                                    SalamanderGeneral->Free(welcomeReply);
                                if (systReply != NULL)
                                    SalamanderGeneral->Free(systReply);
                                if (allocatedListing != NULL)
                                {
                                    memset(allocatedListing, 0, allocatedListingLen); // muze jit o tajna data, radsi nulujeme
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

            // pokud jde o explore adresare pro mazani/change-attr nebo o upload-listing, musime
            // vynulovat merak rychlosti (rychlost explore ani upload-listingu se nemeri, tento
            // okamzik tedy muze byt zacatek mereni rychlosti operaci mazani/change-attr/uploadu),
            // aby se korektne ukazoval time-left v operation-dialogu
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
                postActivate = TRUE;       // impulz pro pokracovani v praci
                reportWorkerChange = TRUE; // potrebujeme schovat pripadny progres tahani listingu

                // vzhledem k tomu, ze uz v sekci CSocketsThread::CritSect jsme, je tohle volani
                // mozne i ze sekce CSocket::SocketCritSect a CFTPWorker::WorkerCritSect (nehrozi dead-lock)
                SocketsThread->DeleteTimer(UID, WORKER_STATUSUPDATETIMID); // zrusime pripadny timer z predchozi prace
            }
            break;
        }
        }
        if (!nextLoop)
            break;
    }
}
