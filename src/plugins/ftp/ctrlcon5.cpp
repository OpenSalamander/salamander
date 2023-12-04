// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CControlConnectionSocket
//

void CControlConnectionSocket::DownloadOneFile(HWND parent, const char* fileName,
                                               CQuadWord const& fileSizeInBytes,
                                               BOOL asciiMode, const char* workPath,
                                               const char* tgtFileName, BOOL* newFileCreated,
                                               BOOL* newFileIncomplete, CQuadWord* newFileSize,
                                               int* totalAttemptNum, int panel, BOOL notInPanel,
                                               char* userBuf, int userBufSize)
{
    CALL_STACK_MESSAGE10("CControlConnectionSocket::DownloadOneFile(, %s, , %d, %s, %s, , , , %d, %d, %d, %s, %d)",
                         fileName, asciiMode, workPath, tgtFileName, *totalAttemptNum, panel,
                         notInPanel, userBuf, userBufSize);

    *newFileCreated = FALSE;
    *newFileIncomplete = FALSE;
    newFileSize->Set(0, 0);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL usePassiveModeAux = UsePassiveMode;
    int logUID = LogUID; // UID logu teto connectiony
    char errBuf[900 + FTP_MAX_PATH];
    char hostBuf[HOST_MAX_SIZE];
    char userBuffer[USER_MAX_SIZE];
    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
    unsigned short portBuf = Port;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    int lockedFileUID; // UID zamceneho souboru (v FTPOpenedFiles) - soubor zamykame pro download
    if (FTPOpenedFiles.OpenFile(userBuffer, hostBuf, portBuf, workPath,
                                GetFTPServerPathType(workPath),
                                fileName, &lockedFileUID, ffatRead))
    { // soubor na serveru jeste neni otevreny, muzeme s nim pracovat, alokujeme objekt pro "data connection"
        HANDLES(EnterCriticalSection(&SocketCritSect));
        CFTPProxyForDataCon* dataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
        BOOL dataConProxyServerOK = ProxyServer == NULL || dataConProxyServer != NULL;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        CDataConnectionSocket* dataConnection = dataConProxyServerOK ? new CDataConnectionSocket(TRUE, dataConProxyServer, EncryptDataConnection, pCertificate, CompressData, this) : NULL;
        if (dataConnection == NULL || !dataConnection->IsGood())
        {
            if (dataConnection != NULL)
                DeleteSocket(dataConnection); // bude se jen dealokovat
            else
            {
                if (dataConProxyServer != NULL)
                    delete dataConProxyServer;
            }
            dataConnection = NULL;
            TRACE_E(LOW_MEMORY);
        }
        else
        {
            char cmdBuf[50 + FTP_MAX_PATH];
            char logBuf[50 + FTP_MAX_PATH];
            const char* retryMsgAux = NULL;
            BOOL canRetry = FALSE;
            char retryMsgBuf[300];
            BOOL reconnected = FALSE;
            char replyBuf[700];
            BOOL setStartTimeIfConnected = TRUE;
            BOOL sslErrReconnect = FALSE;     // TRUE = reconnect kvuli chybam SSL
            BOOL fastSSLErrReconnect = FALSE; // TRUE = jde o zmenu certifikatu serveru, zadouci je okamzity reconnect (bez 20 vterin cekani)
            while (ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                                     userBuf, userBufSize, &reconnected,
                                     setStartTimeIfConnected, totalAttemptNum,
                                     retryMsgAux, NULL,
                                     sslErrReconnect ? IDS_DOWNLOADONEFILEERROR : -1,
                                     fastSSLErrReconnect)) // bude-li potreba, reconnectneme se
            {
                if (pCertificate) // certifikat control-connectiony se mohl zmenit, predame pripadny novy do data-connectiony
                    dataConnection->SetCertificate(pCertificate);
                sslErrReconnect = FALSE;
                fastSSLErrReconnect = FALSE;
                setStartTimeIfConnected = TRUE;
                BOOL run = FALSE;
                BOOL ok = TRUE;
                char newPath[FTP_MAX_PATH];
                BOOL needChangeDir = reconnected; // po reconnectu zkusime opet nastavit pracovni adresar
                if (!reconnected)                 // jsme jiz dele pripojeni, zkontrolujeme jestli pracovni adresar odpovida 'workPath'
                {
                    // vyuzijeme cache, v normalnich pripadech by tam cesta mela byt
                    ok = GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300);
                    if (!ok && canRetry) // "retry" je povolen
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    if (ok && strcmp(newPath, workPath) != 0) // nesedi pracovni adresar na serveru - nutna zmena
                        needChangeDir = TRUE;                 // (predpoklad: server vraci stale stejny retezec pracovni cesty)
                }
                if (ok && needChangeDir) // je-li potreba zmenit pracovni adresar
                {
                    BOOL success;
                    // v SendChangeWorkingPath() je pri vypadku spojeni ReconnectIfNeeded(), nastesti to
                    // nevadi, protoze kod predchazejici tomuto volani se provadi jen pokud k reconnectu
                    // nedojde - "if (!reconnected)" - pokud dojde k reconnectu, jsou oba kody stejne
                    ok = SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, workPath,
                                               userBuf, userBufSize, &success,
                                               replyBuf, 700, NULL,
                                               totalAttemptNum, NULL, TRUE, NULL);
                    if (ok && !success && workPath[0] != 0) // send se povedl, ale server hlasi nejakou chybu (+ignorujeme chybu pri prazdne ceste) -> soubor nelze
                    {                                       // downloadnout (je na akt. ceste v panelu)
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), workPath, replyBuf);
                        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        ok = FALSE;
                    }
                }

                ReuseSSLSessionFailed = FALSE;
                if (ok && usePassiveModeAux) // pasivni rezim (PASV)
                {
                    PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdPassive, NULL); // nemuze selhat
                    int ftpReplyCode;
                    if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                       &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                       retryMsgBuf, 300, NULL))
                    {
                        DWORD ip;
                        unsigned short port;
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS &&      // uspech (melo by byt 227)
                            FTPGetIPAndPortFromReply(replyBuf, -1, &ip, &port)) // podarilo se ziskat IP+port
                        {
                            dataConnection->SetPassive(ip, port, logUID);
                            dataConnection->PassiveConnect(NULL); // prvni pokus, vysledek nas nezajima (testuje se pozdeji)
                        }
                        else // pasivni rezim neni podporovan
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            UsePassiveMode = usePassiveModeAux = FALSE; // zkusime to jeste v normalnim rezimu (PORT)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));

                            Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGPASVNOTSUPPORTED), -1);
                        }
                    }
                    else // chyba -> zavrena connectiona
                    {
                        ok = FALSE;
                        if (canRetry) // "retry" je povolen
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (ok && !usePassiveModeAux) // normalni rezim (PORT)
                {
                    DWORD localIP;
                    GetLocalIP(&localIP, NULL);   // snad ani nemuze vratit chybu
                    unsigned short localPort = 0; // listen on any port
                    dataConnection->SetActive(logUID);
                    if (OpenForListeningAndWaitForRes(parent, dataConnection, &localIP, &localPort, &canRetry,
                                                      retryMsgBuf, 300, GetWaitTime(WAITWND_COMOPER),
                                                      errBuf, 900 + FTP_MAX_PATH))
                    {
                        PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                          ftpcmdSetPort, NULL, localIP, localPort); // nemuze selhat
                        int ftpReplyCode;
                        if (!SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                            &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                            retryMsgBuf, 300, NULL)) // odpoved serveru ignorujeme, chyba se objevi dale (timeout pri listovani)
                        {                                            // chyba -> zavrena connectiona
                            ok = FALSE;
                            if (canRetry) // "retry" je povolen
                            {
                                run = TRUE;
                                retryMsgAux = retryMsgBuf;
                            }
                        }
                    }
                    else // nepodarilo se otevrit "listen" socket pro prijem datoveho spojeni ze serveru ->
                    {    // zavrena connectiona (aby se dalo pouzit standardni Retry)
                        ok = FALSE;
                        if (canRetry) // "retry" je povolen, jdeme na dalsi reconnect
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (ok) // jsme-li jeste pripojeni, zmenime rezim prenosu podle 'asciiMode' (uspesnost ignorujeme)
                {
                    if (!SetCurrentTransferMode(parent, asciiMode, NULL, NULL, 0, FALSE, &canRetry,
                                                retryMsgBuf, 300))
                    { // chyba -> zavrena connectiona
                        ok = FALSE;
                        if (canRetry) // "retry" je povolen
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (ok)
                {
                    // nastavime jmeno ciloveho souboru do data-connectiony, flush dat se bude provadet
                    // do tohoto souboru (vzdy dojde k jeho prepisu, zadny resume zde delat nebudeme)
                    dataConnection->SetDirectFlushParams(tgtFileName, asciiMode ? ctrmASCII : ctrmBinary);

                    if (fileSizeInBytes != CQuadWord(-1, -1)) // je-li znama velikost souboru v bytech, nastavime ji
                        dataConnection->SetDataTotalSize(fileSizeInBytes);

                    int ftpReplyCode;
                    CSendCmdUserIfaceForListAndDownload userIface(TRUE, parent, dataConnection, logUID);

                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                    CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, workPath);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdRetrieveFile, NULL, fileName); // nemuze nahlasit chybu
                    BOOL fileIncomplete = TRUE;
                    BOOL tgtFileError = FALSE;
                    userIface.InitWnd(fileName, hostBuf, workPath, pathType);
                    BOOL sendCmdRes = SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                                     &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                                     retryMsgBuf, 300, &userIface);
                    int asciiTrForBinFileHowToSolve = 0;
                    if (dataConnection->IsAsciiTrForBinFileProblem(&asciiTrForBinFileHowToSolve))
                    {                                         // byl detekovan problem "ascii transfer mode for binary file"
                        if (asciiTrForBinFileHowToSolve == 0) // mame se zeptat uzivatele
                        {
                            INT_PTR res = CViewErrAsciiTrForBinFileDlg(parent).Execute();
                            if (res == IDOK)
                                asciiTrForBinFileHowToSolve = 1;
                            else
                            {
                                if (res == IDIGNORE)
                                    asciiTrForBinFileHowToSolve = 3;
                                else
                                    asciiTrForBinFileHowToSolve = 2;
                            }
                            dataConnection->SetAsciiTrModeForBinFileHowToSolve(asciiTrForBinFileHowToSolve);
                            // nechame prekreslit hlavni okno
                            UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                        }
                        if (asciiTrForBinFileHowToSolve == 1) // downloadnout znovu v binary rezimu
                        {
                            // pockame si na zavreni souboru v diskcache, jinak nepujde soubor smazat
                            dataConnection->WaitForFileClose(5000); // max. 5 sekund

                            SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL);
                            DeleteFile(tgtFileName);
                            asciiMode = FALSE; // downloadnout znovu v binary rezimu

                            ok = FALSE; // zopakujeme download
                            run = TRUE;
                            retryMsgAux = NULL;
                            setStartTimeIfConnected = FALSE;
                        }
                        else
                        {
                            if (asciiTrForBinFileHowToSolve == 2) // prerusit download souboru (cancel)
                            {
                                // pockame si na zavreni souboru v diskcache, jinak nepujde soubor smazat
                                dataConnection->WaitForFileClose(5000); // max. 5 sekund

                                SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL);
                                DeleteFile(tgtFileName);
                                ok = FALSE; // zadnou hlasku vypisovat nebudeme, user jiz cancel potvrdil
                            }
                        }
                    }
                    else
                        asciiTrForBinFileHowToSolve = 3;
                    if (asciiTrForBinFileHowToSolve == 3) // problem "ascii transfer mode for binary file" nenastal nebo ma byt ignorovan
                    {
                        if (sendCmdRes)
                        {
                            if (userIface.WasAborted()) // uzivatel download abortoval - koncime s chybou (nekompletnim souborem)
                                ok = FALSE;             // zadnou hlasku vypisovat nebudeme, user potvrdil dotaz pri abortu
                            else
                            {
                                if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS || // server hlasi chybu downloadu
                                    userIface.HadError() ||                        // datova connectiona zaznamenala chybu hlasenou systemem
                                    //dataConnection->GetDecomprMissingStreamEnd() || // bohuzel tenhle test je nepruchozi, napr. Serv-U 7 a 8 proste stream neukoncuje // pokud jsou data komprimovana (MODE Z), musime dostat kompletni data stream, jinak jde o chybu
                                    userIface.GetDatConCancelled()) // datova connectiona byla prerusena (bud nedoslo k otevreni nebo zavreni po chybe hlasene serverem v odpovedi na RETR)
                                {
                                    ok = FALSE;

                                    DWORD netErr, tgtFileErr;
                                    BOOL lowMem, noDataTrTimeout;
                                    int sslErrorOccured;
                                    BOOL decomprErrorOccured;
                                    userIface.GetError(&netErr, &lowMem, &tgtFileErr, &noDataTrTimeout, &sslErrorOccured,
                                                       &decomprErrorOccured);
                                    //BOOL decomprMissingStreamEnd = dataConnection->GetDecomprMissingStreamEnd();
                                    BOOL sslReuseErr = ReuseSSLSessionFailed &&
                                                       (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_TRANSIENTERROR ||
                                                        FTP_DIGIT_1(ftpReplyCode) == FTP_D1_ERROR);
                                    if (sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslErrorOccured == SSLCONERR_CANRETRY ||
                                        sslReuseErr)
                                    {                                                                       // potrebujeme provest reconnect
                                        CloseControlConnection(parent);                                     // zavreme soucasnou control connectionu
                                        lstrcpyn(retryMsgBuf, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 300); // nastavime text chyby pro reconnect wait okenko
                                        retryMsgAux = retryMsgBuf;
                                        sslErrReconnect = TRUE;
                                        run = TRUE;
                                        fastSSLErrReconnect = sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslReuseErr;
                                    }
                                    else
                                    {
                                        // vypiseme hlasku "Unable to download file from server"
                                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERROR), fileName, workPath);
                                        int len = (int)strlen(errBuf);
                                        BOOL isNetErr = TRUE;
                                        int useSuffixResID = IDS_DOWNLOADFILEERRORSUFIX2; // server reply:  (prefix pro hlasku v 'replyBuf')
                                        if (tgtFileErr != NO_ERROR || FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS ||
                                            noDataTrTimeout || sslErrorOccured != SSLCONERR_NOERROR ||
                                            decomprErrorOccured /*|| decomprMissingStreamEnd*/)
                                        {                                                 // pokud nemame popis chyby ze serveru, spokojime se se systemovym popisem
                                            useSuffixResID = IDS_DOWNLOADFILEERRORSUFIX1; // error:
                                            isNetErr = sslErrorOccured != SSLCONERR_NOERROR || netErr != NO_ERROR ||
                                                       tgtFileErr == NO_ERROR || decomprErrorOccured /*|| decomprMissingStreamEnd*/;
                                            if (!isNetErr)
                                            {
                                                netErr = tgtFileErr;
                                                tgtFileError = TRUE;
                                            }
                                            if (sslErrorOccured != SSLCONERR_NOERROR || decomprErrorOccured)
                                                lstrcpyn(replyBuf, LoadStr(decomprErrorOccured ? IDS_ERRDATACONDECOMPRERROR : IDS_ERRDATACONSSLCONNECTERROR), 700);
                                            else
                                            {
                                                if (noDataTrTimeout)
                                                    lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNODATATRTIMEOUT), 700);
                                                else
                                                {
                                                    if (netErr != NO_ERROR)
                                                    {
                                                        if (!dataConnection->GetProxyError(replyBuf, 700, NULL, 0, TRUE))
                                                        {
                                                            if (!isNetErr)
                                                                useSuffixResID = IDS_DOWNLOADFILEERRORSUFIX3; // error writing target file:
                                                            FTPGetErrorText(netErr, replyBuf, 700);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        if (userIface.GetDatConCancelled())
                                                            lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNOTOPENED), 700);
                                                        else
                                                        {
                                                            /*if (decomprMissingStreamEnd) lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONDECOMPRERROR), 700);
                              else*/
                                                            lstrcpyn(replyBuf, LoadStr(IDS_UNKNOWNERROR), 700);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(useSuffixResID), replyBuf);
                                        SalamanderGeneral->SalMessageBox(parent, errBuf,
                                                                         LoadStr(IDS_FTPERRORTITLE),
                                                                         MB_OK | MB_ICONEXCLAMATION);
                                    }
                                }
                                else
                                    fileIncomplete = FALSE; // vse OK, download se povedl
                            }
                        }
                        else // zavrena connectiona
                        {
                            if (userIface.WasAborted()) // uzivatel download abortoval - cimz se terminovala connectiona (dela napr. sunsolve.sun.com (Sun Unix) nebo ftp.chg.ru) - koncime s chybou (nekompletnim listingem)
                            {
                                ok = FALSE;   // hlasku "file can be incomplete" vypisovat nebudeme, user byl varovan pri abortu
                                if (canRetry) // prevezmeme hlaseni do messageboxu, ktery oznami preruseni spojeni
                                {
                                    HANDLES(EnterCriticalSection(&SocketCritSect));
                                    if (ConnectionLostMsg != NULL)
                                        SalamanderGeneral->Free(ConnectionLostMsg);
                                    ConnectionLostMsg = SalamanderGeneral->DupStr(retryMsgBuf);
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                }
                            }
                            else
                            {
                                // chyba -> 'ok' zustava FALSE, jdeme na dalsi reconnect
                                ok = FALSE;
                                if (canRetry) // "retry" je povolen
                                {
                                    run = TRUE;
                                    retryMsgAux = retryMsgBuf;
                                }
                            }
                        }

                        // pockame si na zavreni souboru v diskcache, jinak nepujde soubor smazat a
                        // nemusi ho ani otevrit viewery (bez SHARE_WRITE nejde otevrit)
                        dataConnection->WaitForFileClose(5000); // max. 5 sekund

                        if (run) // jdeme na dalsi pokus, pro jistotu vycistime cilovy soubor (mohl se vytvorit pred chybou/prerusenim)
                        {        // nejsme v zadne kriticke sekci, takze i kdyz se diskova operace na cas zakousne, nic se nedeje
                            SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL);
                            DeleteFile(tgtFileName);
                        }
                        else // koncime download
                        {
                            dataConnection->GetTgtFileState(newFileCreated, newFileSize); // zjistime jestli soubor existuje + jak je veliky
                            *newFileIncomplete = fileIncomplete;                          // ulozime tez jestli je soubor kompletni
                            if (!*newFileCreated &&                                       // prazdne soubory data-connectiona neumi vytvorit (vytvari soubor jen tesne pred zapisem), musime je tedy vytvorit zde
                                !fileIncomplete &&                                        // nejde jen o chybu pri ziskavani souboru
                                !tgtFileError)                                            // jen pokus nedoslo k nejake chybe ciloveho souboru (at k ni nedojde podruhe zde)
                            {
                                if (*newFileSize != CQuadWord(0, 0))
                                    TRACE_E("CControlConnectionSocket::DownloadOneFile(): unexpected situation: file was not created, but its size is not null!");

                                // nejsme v zadne kriticke sekci, takze i kdyz se diskova operace na cas zakousne, nic se nedeje
                                SetFileAttributes(tgtFileName, FILE_ATTRIBUTE_NORMAL); // aby sel prepsat i read-only soubor
                                HANDLE file = HANDLES_Q(CreateFile(tgtFileName, GENERIC_WRITE,
                                                                   FILE_SHARE_READ, NULL,
                                                                   CREATE_ALWAYS,
                                                                   FILE_FLAG_SEQUENTIAL_SCAN,
                                                                   NULL));
                                if (file != INVALID_HANDLE_VALUE)
                                {
                                    *newFileCreated = TRUE;
                                    HANDLES(CloseHandle(file));
                                }
                                else
                                {
                                    DWORD err = GetLastError();

                                    // vypiseme hlasku "Unable to download file from server"
                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERROR), fileName, workPath);
                                    int len = (int)strlen(errBuf);
                                    if (err != NO_ERROR)
                                        FTPGetErrorText(err, replyBuf, 700);
                                    else
                                        lstrcpyn(replyBuf, LoadStr(IDS_UNKNOWNERROR), 700);
                                    _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERRORSUFIX3), replyBuf);
                                    SalamanderGeneral->SalMessageBox(parent, errBuf,
                                                                     LoadStr(IDS_FTPERRORTITLE),
                                                                     MB_OK | MB_ICONEXCLAMATION);
                                }
                            }
                        }
                    }
                }

                if (dataConnection->IsConnected())       // pripadne zavreme "data connection", system se pokusi o "graceful"
                    dataConnection->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)

                if (!run)
                    break;
            }

            // uvolnime "data connection"
            DeleteSocket(dataConnection);
        }
        FTPOpenedFiles.CloseFile(lockedFileUID);
    }
    else
    {
        // vypiseme hlasku "Unable to download file from server - file is locked by another operation"
        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERROR), fileName, workPath);
        int len = (int)strlen(errBuf);
        _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(IDS_DOWNLOADFILEERRORSUFIX4));
        SalamanderGeneral->SalMessageBox(parent, errBuf,
                                         LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
    }
}

BOOL CControlConnectionSocket::CreateDir(char* changedPath, HWND parent, char* newName,
                                         const char* workPath, int* totalAttemptNum, int panel,
                                         BOOL notInPanel, char* userBuf, int userBufSize)
{
    CALL_STACK_MESSAGE8("CControlConnectionSocket::CreateDir(, , %s, , %s, %d, %d, %d, %s, %d)",
                        newName, workPath, *totalAttemptNum, panel, notInPanel, userBuf, userBufSize);

    changedPath[0] = 0;

    BOOL retSuccess = FALSE;
    BOOL reconnected = FALSE;
    BOOL setStartTimeIfConnected = TRUE;
    BOOL canRetry = FALSE;
    const char* retryMsgAux = NULL;
    char retryMsgBuf[300];
    char replyBuf[700];
    char errBuf[900 + FTP_MAX_PATH];
    char cmdBuf[50 + 2 * MAX_PATH];
    char logBuf[50 + 2 * MAX_PATH];
    char hostBuf[HOST_MAX_SIZE];
    char userBuffer[USER_MAX_SIZE];
    while (ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                             userBuf, userBufSize, &reconnected,
                             setStartTimeIfConnected, totalAttemptNum,
                             retryMsgAux, NULL, -1, FALSE)) // bude-li potreba, reconnectneme se
    {
        setStartTimeIfConnected = TRUE;
        BOOL run = FALSE;
        BOOL ok = TRUE;
        char newPath[FTP_MAX_PATH];
        BOOL needChangeDir = reconnected; // po reconnectu zkusime opet nastavit pracovni adresar
        if (!reconnected)                 // jsme jiz dele pripojeni, zkontrolujeme jestli pracovni adresar odpovida 'workPath'
        {
            // vyuzijeme cache, v normalnich pripadech by tam cesta mela byt
            ok = GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300);
            if (!ok && canRetry) // "retry" je povolen
            {
                run = TRUE;
                retryMsgAux = retryMsgBuf;
            }
            if (ok && strcmp(newPath, workPath) != 0) // nesedi pracovni adresar na serveru - nutna zmena
                needChangeDir = TRUE;                 // (predpoklad: server vraci stale stejny retezec pracovni cesty)
        }
        if (ok && needChangeDir) // je-li potreba zmenit pracovni adresar
        {
            BOOL success;
            // v SendChangeWorkingPath() je pri vypadku spojeni ReconnectIfNeeded(), nastesti to
            // nevadi, protoze kod predchazejici tomuto volani se provadi jen pokud k reconnectu
            // nedojde - "if (!reconnected)" - pokud dojde k reconnectu, jsou oba kody stejne
            ok = SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, workPath,
                                       userBuf, userBufSize, &success,
                                       replyBuf, 700, NULL,
                                       totalAttemptNum, NULL, TRUE, NULL);
            if (ok && !success && workPath[0] != 0) // send se povedl, ale server hlasi nejakou chybu (+ignorujeme chybu pri prazdne ceste) -> soubor nelze
            {                                       // downloadnout (je na akt. ceste v panelu)
                _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), workPath, replyBuf);
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
        }

        if (ok)
        {
            // vytvorime pozadovany adresar
            PrepareFTPCommand(cmdBuf, 50 + 2 * MAX_PATH, logBuf, 50 + 2 * MAX_PATH,
                              ftpcmdCreateDir, NULL, newName); // nemuze selhat
            BOOL refreshWorkingPath = TRUE;
            int ftpReplyCode;
            if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                               &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                               retryMsgBuf, 300, NULL))
            {
                retSuccess = FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS;
                if (retSuccess && workPath[0] != 0) // pokud se adresar(e) vytvoril(y), zmenime listing(y) (pokud FTP prikaz nevraci uspech, predpokladame zjednodusene, ze zadny adresar proste nebyl vytvoren - na VMS lze tvorit vic adresaru najednou, asi muze dojit pouze k castecnemu vytvoreni, to ale neresime)
                {
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                    unsigned short portBuf = Port;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    UploadListingCache.ReportCreateDirs(hostBuf, userBuffer, portBuf, workPath,
                                                        GetFTPServerPathType(workPath), newName, FALSE);
                }
                if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS && // vraci se uspech (melo by byt 257)
                    FTPGetDirectoryFromReply(replyBuf, (int)strlen(replyBuf), newPath, FTP_MAX_PATH))
                {                   // prave byl vytvoren adresar 'newPath'
                    newName[0] = 0; // zatim se zadny focus po refreshi nekona
                    CFTPServerPathType pathType = GetFTPServerPathType(newPath);
                    char cutDir[FTP_MAX_PATH];
                    if (pathType != ftpsptUnknown &&
                        FTPCutDirectory(pathType, newPath, FTP_MAX_PATH, cutDir, FTP_MAX_PATH, NULL))
                    {
                        if (!FTPIsPrefixOfServerPath(pathType, workPath, newPath))
                        {
                            lstrcpyn(changedPath, newPath, FTP_MAX_PATH);
                            refreshWorkingPath = FALSE;
                        }
                        if (FTPIsTheSameServerPath(pathType, newPath, workPath))
                            lstrcpyn(newName, cutDir, 2 * MAX_PATH); // jmeno adresare pro focus po refreshi
                    }
                    else // nejspis server vraci v odpovedi "257" relativni jmeno adresare (napr. warftpd)
                    {
                        lstrcpyn(newName, newPath, 2 * MAX_PATH); // jmeno adresare pro focus po refreshi
                    }
                }
                else // chyba (i neocekavany format odpovedi "257")
                {
                    if (!retSuccess) // pri uspesne odpovedi nebudeme vypisovat error hlasku
                    {
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CREATEDIRERROR), newName, replyBuf);
                        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                         MB_OK | MB_ICONEXCLAMATION);
                    }
                }
            }
            else // chyba -> zavrena connectiona
            {
                if (workPath[0] != 0) // nevime, jestli se adresar(e) vytvoril(y), zneplatnime listing(y)
                {
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                    unsigned short portBuf = Port;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    UploadListingCache.ReportCreateDirs(hostBuf, userBuffer, portBuf, workPath,
                                                        GetFTPServerPathType(workPath), newName, TRUE);
                }
                if (canRetry) // "retry" je povolen
                {
                    run = TRUE;
                    retryMsgAux = retryMsgBuf;
                }
            }
            if (refreshWorkingPath)
                lstrcpyn(changedPath, workPath, FTP_MAX_PATH);
        }

        if (!run)
            break;
    }
    return retSuccess;
}

BOOL CControlConnectionSocket::QuickRename(char* changedPath, HWND parent, const char* fromName,
                                           char* newName, const char* workPath, int* totalAttemptNum,
                                           int panel, BOOL notInPanel, char* userBuf, int userBufSize,
                                           BOOL isVMS, BOOL isDir)
{
    CALL_STACK_MESSAGE11("CControlConnectionSocket::QuickRename(, , %s, %s, , %s, %d, %d, %d, %s, %d, %d, %d)",
                         fromName, newName, workPath, *totalAttemptNum, panel, notInPanel, userBuf,
                         userBufSize, isVMS, isDir);

    changedPath[0] = 0;
    if (strcmp(fromName, newName) == 0)
    {
        newName[0] = 0;
        return TRUE; // neni co delat, nedochazi ke zmene jmena
    }

    char fromNameBuf[MAX_PATH + 10];
    const char* fromNameForSrv = fromName;
    if (isVMS && isDir)
    {
        FTPMakeVMSDirName(fromNameBuf, MAX_PATH + 10, fromName);
        fromNameForSrv = fromNameBuf;
    }

    HANDLES(EnterCriticalSection(&SocketCritSect));
    char hostBuf[HOST_MAX_SIZE];
    char userBuffer[USER_MAX_SIZE];
    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
    unsigned short portBuf = Port;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    BOOL retSuccess = FALSE;
    BOOL srcLocked = FALSE;
    BOOL tgtLocked = FALSE;
    char errBuf[900 + FTP_MAX_PATH];
    CFTPServerPathType pathType = GetFTPServerPathType(workPath);
    int lockedFromFileUID; // UID zamceneho souboru (v FTPOpenedFiles) - soubor zamykame pro prejmenovani
    if (isDir || FTPOpenedFiles.OpenFile(userBuffer, hostBuf, portBuf, workPath, pathType,
                                         fromNameForSrv, &lockedFromFileUID, ffatRename))
    {                        // soubor na serveru jeste neni otevreny, muzeme s nim pracovat, alokujeme objekt pro "data connection"
        int lockedToFileUID; // UID zamceneho souboru (v FTPOpenedFiles) - soubor zamykame pro prejmenovani
        if (isDir || FTPOpenedFiles.OpenFile(userBuffer, hostBuf, portBuf, workPath, pathType,
                                             newName, &lockedToFileUID, ffatRename))
        { // soubor na serveru jeste neni otevreny, muzeme s nim pracovat, alokujeme objekt pro "data connection"
            BOOL reconnected = FALSE;
            BOOL setStartTimeIfConnected = TRUE;
            BOOL canRetry = FALSE;
            const char* retryMsgAux = NULL;
            char retryMsgBuf[300];
            char replyBuf[700];
            char cmdBuf[50 + 2 * MAX_PATH];
            char logBuf[50 + 2 * MAX_PATH];
            while (ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                                     userBuf, userBufSize, &reconnected,
                                     setStartTimeIfConnected, totalAttemptNum,
                                     retryMsgAux, NULL, -1, FALSE)) // bude-li potreba, reconnectneme se
            {
                setStartTimeIfConnected = TRUE;
                BOOL run = FALSE;
                BOOL ok = TRUE;
                char newPath[FTP_MAX_PATH];
                BOOL needChangeDir = reconnected; // po reconnectu zkusime opet nastavit pracovni adresar
                if (!reconnected)                 // jsme jiz dele pripojeni, zkontrolujeme jestli pracovni adresar odpovida 'workPath'
                {
                    // vyuzijeme cache, v normalnich pripadech by tam cesta mela byt
                    ok = GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300);
                    if (!ok && canRetry) // "retry" je povolen
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    if (ok && strcmp(newPath, workPath) != 0) // nesedi pracovni adresar na serveru - nutna zmena
                        needChangeDir = TRUE;                 // (predpoklad: server vraci stale stejny retezec pracovni cesty)
                }
                if (ok && needChangeDir) // je-li potreba zmenit pracovni adresar
                {
                    BOOL success;
                    // v SendChangeWorkingPath() je pri vypadku spojeni ReconnectIfNeeded(), nastesti to
                    // nevadi, protoze kod predchazejici tomuto volani se provadi jen pokud k reconnectu
                    // nedojde - "if (!reconnected)" - pokud dojde k reconnectu, jsou oba kody stejne
                    ok = SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT, parent, workPath,
                                               userBuf, userBufSize, &success,
                                               replyBuf, 700, NULL,
                                               totalAttemptNum, NULL, TRUE, NULL);
                    if (ok && !success && workPath[0] != 0) // send se povedl, ale server hlasi nejakou chybu (+ignorujeme chybu pri prazdne ceste) -> soubor nelze
                    {                                       // downloadnout (je na akt. ceste v panelu)
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), workPath, replyBuf);
                        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        ok = FALSE;
                    }
                }

                if (ok)
                {
                    // posleme nejprve "rename from" prikaz (pozdeji posleme navazujici "rename to")
                    PrepareFTPCommand(cmdBuf, 50 + 2 * MAX_PATH, logBuf, 50 + 2 * MAX_PATH,
                                      ftpcmdRenameFrom, NULL, fromNameForSrv); // nemuze selhat
                    int ftpReplyCode;
                    if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                       &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                       retryMsgBuf, 300, NULL))
                    {
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_PARTIALSUCCESS) // 350 Requested file action pending further information
                        {                                                       // mame poslat "rename to"
                            PrepareFTPCommand(cmdBuf, 50 + 2 * MAX_PATH, logBuf, 50 + 2 * MAX_PATH,
                                              ftpcmdRenameTo, NULL, newName); // nemuze selhat
                            if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                               &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                               retryMsgBuf, 300, NULL))
                            {
                                if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // vraci se uspech (melo by byt 250)
                                {                                                // quick rename uspesne probehl - v 'newName' nechame nove jmeno, aby se mohlo focusnout pri naslednem refreshi
                                    retSuccess = TRUE;
                                    if (workPath[0] != 0)
                                    {
                                        HANDLES(EnterCriticalSection(&SocketCritSect));
                                        lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                                        lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                                        portBuf = Port;
                                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                                        UploadListingCache.ReportRename(hostBuf, userBuffer, portBuf, workPath,
                                                                        pathType, fromName, newName, FALSE);
                                    }
                                }
                                else // chyba
                                {
                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_QUICKRENAMEERROR), fromName, newName, replyBuf);
                                    SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                                     MB_OK | MB_ICONEXCLAMATION);
                                }
                            }
                            else // chyba -> zavrena connectiona
                            {
                                if (workPath[0] != 0)
                                {
                                    HANDLES(EnterCriticalSection(&SocketCritSect));
                                    lstrcpyn(hostBuf, Host, HOST_MAX_SIZE);
                                    lstrcpyn(userBuffer, User, USER_MAX_SIZE);
                                    portBuf = Port;
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                    UploadListingCache.ReportRename(hostBuf, userBuffer, portBuf, workPath,
                                                                    pathType, fromName, newName, TRUE);
                                }
                                if (canRetry) // "retry" je povolen
                                {
                                    run = TRUE;
                                    retryMsgAux = retryMsgBuf;
                                }
                            }
                            lstrcpyn(changedPath, workPath, FTP_MAX_PATH);
                        }
                        else // chyba (i neocekavana odpoved)
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_QUICKRENAMEERROR), fromName, newName, replyBuf);
                            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                             MB_OK | MB_ICONEXCLAMATION);
                        }
                    }
                    else // chyba -> zavrena connectiona
                    {
                        if (canRetry) // "retry" je povolen
                        {
                            run = TRUE;
                            retryMsgAux = retryMsgBuf;
                        }
                    }
                }

                if (!run)
                    break;
            }
            if (!isDir)
                FTPOpenedFiles.CloseFile(lockedToFileUID);
        }
        else
            tgtLocked = TRUE; // rename-to-file uz je otevreny
        if (!isDir)
            FTPOpenedFiles.CloseFile(lockedFromFileUID);
    }
    else
        srcLocked = TRUE; // rename-from-file uz je otevreny
    if (srcLocked || tgtLocked)
    {
        // vypiseme hlasku "Unable to rename file on server - src or tgt file is locked by another operation"
        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_QUICKRENAMEFILEERR), fromNameForSrv, newName);
        int len = (int)strlen(errBuf);
        _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE,
                    LoadStr(srcLocked ? IDS_QUICKRENAMEFILEERRSUF1 : IDS_QUICKRENAMEFILEERRSUF2));
        SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
    }
    return retSuccess;
}

BOOL CControlConnectionSocket::OpenForListeningAndWaitForRes(HWND parent, CDataConnectionSocket* dataConnection,
                                                             DWORD* listenOnIP, unsigned short* listenOnPort,
                                                             BOOL* canRetry, char* retryMsg, int retryMsgBufSize,
                                                             int waitWndTime, char* errBuf, int errBufSize)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::OpenForListeningAndWaitForRes()");

    char buf[300];

    parent = FindPopupParent(parent);
    DWORD startTime = GetTickCount(); // cas zacatku operace
    *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    CWaitWindow waitWnd(parent, TRUE);
    waitWnd.SetText(LoadStr(IDS_PREPARINGACTDATACON));

    HWND focusedWnd = NULL;
    BOOL parentIsEnabled = IsWindowEnabled(parent);
    CSetWaitCursorWindow* winParent = NULL;
    if (parentIsEnabled) // parenta nemuzeme nechat enablovaneho (wait okenko neni modalni)
    {
        // schovame si fokus z 'parent' (neni-li fokus z 'parent', ulozime NULL)
        focusedWnd = GetFocus();
        HWND hwnd = focusedWnd;
        while (hwnd != NULL && hwnd != parent)
            hwnd = GetParent(hwnd);
        if (hwnd != parent)
            focusedWnd = NULL;
        // disablujeme 'parent', pri enablovani obnovime i fokus
        EnableWindow(parent, FALSE);

        // nahodime cekaci kurzor nad parentem, bohuzel to jinak neumime
        winParent = new CSetWaitCursorWindow;
        if (winParent != NULL)
            winParent->AttachToWindow(parent);
    }

    BOOL ret = FALSE;

    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // aspon sekundu

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID;                                  // UID logu teto connectiony
    BOOL handleKeepAlive = KeepAliveMode != kamForbidden; // TRUE pokud se keep-alive neresi o uroven vyse (je nutne ho resit zde)
    DWORD auxServerIP = ServerIP;
    int proxyPort = ProxyServer != NULL ? ProxyServer->ProxyPort : 0;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (handleKeepAlive)
    {
        // pockame na dokonceni keep-alive prikazu (pokud prave probiha) + nastavime
        // keep-alive na 'kamForbidden' (probiha normalni prikaz)
        DWORD waitTime = GetTickCount() - startTime;
        WaitForEndOfKeepAlive(parent, waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);
    }

    dataConnection->SetPostMessagesToWorker(TRUE, GetMsg(), GetUID(), -1, -1, -1, CTRLCON_LISTENFORCON);

    BOOL doNotCloseCon = FALSE; // TRUE = pri chybe se nema zavrit control-connectiona
    BOOL listenError;
    DWORD err;
    if (dataConnection->OpenForListeningWithProxy(*listenOnIP, *listenOnPort, &listenError, &err))
    {
        DWORD start = GetTickCount();
        DWORD waitTime = start - startTime;
        waitWnd.Create(waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);

        BOOL newBytesReadReceived = FALSE;
        DWORD newBytesReadData1 = 0;
        BOOL closedReceived = FALSE;
        DWORD closedData1 = 0;
        BOOL run = TRUE;
        while (run)
        {
            // pockame na udalost na socketu (odpoved serveru) nebo ESC
            CControlConnectionSocketEvent event;
            DWORD data1, data2;
            DWORD now = GetTickCount();
            if (now - start > (DWORD)serverTimeout)
                now = start + (DWORD)serverTimeout;
            WaitForEventOrESC(parent, &event, &data1, &data2, serverTimeout - (now - start),
                              &waitWnd, NULL, FALSE);
            switch (event)
            {
            case ccsevESC:
            {
                waitWnd.Show(FALSE);
                BOOL esc = SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_PREPACTDATACONESC),
                                                            LoadStr(IDS_FTPPLUGINTITLE),
                                                            MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
                if (esc)
                {
                    run = FALSE;
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE); // ESC (cancel) do logu
                }
                else
                {
                    SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu
                    waitWnd.Show(TRUE);
                }
                break;
            }

            case ccsevTimeout:
            {
                if (!dataConnection->GetProxyTimeoutDescr(buf, 300))
                    lstrcpyn(buf, LoadStr(IDS_PREPACTDATACONTIMEOUT), 300);
                if (retryMsgBufSize > 0)
                {
                    _snprintf_s(retryMsg, retryMsgBufSize, _TRUNCATE, "%s\r\n", buf);
                    Logs.LogMessage(logUID, retryMsg, -1, TRUE);
                }
                *canRetry = TRUE;
                run = FALSE;
                break;
            }

            case ccsevListenForCon:
            {
                if ((int)data1 == dataConnection->GetUID()) // zpravu zpracujeme jen pokud je pro nasi data-connectionu
                {
                    if (!dataConnection->GetListenIPAndPort(listenOnIP, listenOnPort)) // chyba "listen"
                    {
                        if (dataConnection->GetProxyError(buf, 300, NULL, 0, TRUE) &&
                            errBufSize > 0)
                        { // vypiseme chybu do logu
                            _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), buf);
                            Logs.LogMessage(logUID, errBuf, -1, TRUE);
                        }
                        *canRetry = TRUE;
                        lstrcpyn(retryMsg, LoadStr(IDS_PROXYERROPENACTDATA), retryMsgBufSize);
                    }
                    else
                        ret = TRUE; // uspech, vratime "listen" IP+port
                    run = FALSE;
                }
                break;
            }

            case ccsevClosed: // ztratu spojeni posleme znovu az to tady dokoncime
            {
                closedReceived = TRUE;
                closedData1 = data1;
                break;
            }

            case ccsevNewBytesRead: // zpravu o nacteni novych bytu posleme znovu az to tady dokoncime
            {
                newBytesReadReceived = TRUE;
                newBytesReadData1 = data1;
                break;
            }

            default:
                TRACE_E("CControlConnectionSocket::OpenForListeningAndWaitForRes(): unexpected event = " << event);
                break;
            }
        }
        waitWnd.Destroy();

        if (newBytesReadReceived)
            AddEvent(ccsevNewBytesRead, newBytesReadData1, 0, newBytesReadData1 == NO_ERROR); // prepsatelna jen pokud nejde o chybu
        if (closedReceived)
            AddEvent(ccsevClosed, closedData1, 0);
    }
    else
    {
        if (listenError) // selhalo volani CSocket::OpenForListening() - "retry" nema smysl (slo o lokalni operaci)
        {
            if (errBufSize > 0)
            {
                _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_OPENACTDATACONERROR),
                            (err != NO_ERROR ? FTPGetErrorText(err, buf, 300) : LoadStr(IDS_UNKNOWNERROR)));
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
            doNotCloseCon = TRUE;
        }
        else // chyba connectu na proxy server, vypiseme chybu do logu, zavreme control-connectionu a provedeme "retry"
        {
            in_addr srvAddr;
            srvAddr.s_addr = auxServerIP;
            if (err != NO_ERROR)
            {
                FTPGetErrorText(err, errBuf, errBufSize);
                char* s = errBuf + strlen(errBuf);
                while (s > errBuf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                *s = 0; // oriznuti znaku konce radky z textu chyby
                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOGMSGUNABLETOCONPRX2), inet_ntoa(srvAddr), proxyPort, errBuf);
            }
            else
                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOGMSGUNABLETOCONPRX), inet_ntoa(srvAddr), proxyPort);
            Logs.LogMessage(logUID, buf, -1, TRUE);

            *canRetry = TRUE;
            lstrcpyn(retryMsg, LoadStr(IDS_PROXYERRUNABLETOCON), retryMsgBufSize);
        }
    }

    if (parentIsEnabled) // pokud jsme disablovali parenta, zase ho enablujeme
    {
        // shodime cekaci kurzor nad parentem
        if (winParent != NULL)
        {
            winParent->DetachWindow();
            delete winParent;
        }

        // enablujeme 'parent'
        EnableWindow(parent, TRUE);
        // pokud je aktivni 'parent', obnovime i fokus
        if (GetForegroundWindow() == parent)
        {
            if (parent == SalamanderGeneral->GetMainWindowHWND())
                SalamanderGeneral->RestoreFocusInSourcePanel();
            else
            {
                if (focusedWnd != NULL)
                    SetFocus(focusedWnd);
            }
        }
    }

    if (ret || doNotCloseCon) // uspech nebo chyba, pri ktere se spojeni nema zavirat
    {
        if (handleKeepAlive)
        {
            // pokud je vse OK, nastavime timer pro keep-alive
            SetupKeepAliveTimer();
        }
    }
    else // chyba, po ktere se provede "retry", musime zavrit spojeni (standardni "retry" zacina connectem...)
    {
        CloseSocket(NULL); // zavreme socket (je-li otevreny), system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
        Logs.SetIsConnected(logUID, IsConnected());
        Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"

        if (handleKeepAlive)
        {
            // uvolnime keep-alive, ted uz nebude potreba (uz neni navazane spojeni)
            ReleaseKeepAlive();
        }
    }

    return ret;
}
