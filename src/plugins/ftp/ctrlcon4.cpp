// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CControlConnectionSocket
//

BOOL CControlConnectionSocket::SetCurrentTransferMode(HWND parent, BOOL asciiMode, BOOL* success,
                                                      char* ftpReplyBuf, int ftpReplyBufSize,
                                                      BOOL forceRefresh, BOOL* canRetry,
                                                      char* retryMsg, int retryMsgBufSize)
{
    CALL_STACK_MESSAGE5("CControlConnectionSocket::SetCurrentTransferMode(, %d, , , %d, %d, , %d)",
                        asciiMode, ftpReplyBufSize, forceRefresh, retryMsgBufSize);

    if (success != NULL)
        *success = FALSE;
    if (ftpReplyBufSize > 0)
        ftpReplyBuf[0] = 0;
    if (canRetry != NULL)
        *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL leaveSect = TRUE;
    BOOL ret = TRUE;

    if (forceRefresh ||
        asciiMode && CurrentTransferMode != ctrmASCII ||
        !asciiMode && CurrentTransferMode != ctrmBinary)
    {
        char cmdBuf[50];
        char logBuf[50];

        HANDLES(LeaveCriticalSection(&SocketCritSect));
        leaveSect = FALSE;

        // zmenime prenosovy rezim na serveru
        PrepareFTPCommand(cmdBuf, 50, logBuf, 50, ftpcmdSetTransferMode, NULL, asciiMode); // nemuze selhat
        int ftpReplyCode;
        if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                           &ftpReplyCode, ftpReplyBuf, ftpReplyBufSize, FALSE, FALSE, TRUE,
                           canRetry, retryMsg, retryMsgBufSize, NULL))
        {
            if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // vraci se uspech (melo by byt 200)
            {
                HANDLES(EnterCriticalSection(&SocketCritSect));
                leaveSect = TRUE;
                CurrentTransferMode = (asciiMode ? ctrmASCII : ctrmBinary); // prenosovy rezim byl zmenen
            }
            else
                CurrentTransferMode = ctrmUnknown; // neznama chyba, nemusi vubec vadit, ale vratime chybu, at to posoudi volajici
        }
        else
            ret = FALSE; // chyba -> zavrena connectiona
    }

    if (leaveSect) // zaroven uz je uspesne nastaveny pozadovany prenosovy rezim
    {
        if (success != NULL)
            *success = TRUE;
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
    return ret;
}

//
// **************************************************************************************
// CSendCmdUserIfaceForListAndDownload
//

BOOL CSendCmdUserIfaceForListAndDownload::HadError()
{
    DWORD netErr, tgtFileErr;
    BOOL lowMem;
    int sslErrorOccured;
    BOOL decomprErrorOccured;
    DataConnection->GetError(&netErr, &lowMem, &tgtFileErr, NULL, &sslErrorOccured, &decomprErrorOccured);
    return netErr != NO_ERROR || lowMem || tgtFileErr != NO_ERROR || sslErrorOccured != SSLCONERR_NOERROR ||
           decomprErrorOccured;
}

void CSendCmdUserIfaceForListAndDownload::GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr,
                                                   BOOL* noDataTrTimeout, int* sslErrorOccured,
                                                   BOOL* decomprErrorOccured)
{
    DataConnection->GetError(netErr, lowMem, tgtFileErr, noDataTrTimeout, sslErrorOccured,
                             decomprErrorOccured);
}

HANDLE
CSendCmdUserIfaceForListAndDownload::GetFinishedEvent()
{
    return DataConnection->GetTransferFinishedEvent();
}

void CSendCmdUserIfaceForListAndDownload::InitWnd(const char* fileName, const char* host,
                                                  const char* path, CFTPServerPathType pathType)
{
    char buf[500];
    if (!ForDownload)
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LISTWNDDOWNLOADING), host);
    else
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LISTWNDDOWNLOADINGFILE), fileName, host);
    WaitWnd.SetText(buf);
    WaitWnd.SetPath(path, pathType);
}

void CSendCmdUserIfaceForListAndDownload::AfterWrite(BOOL aborting, DWORD showTime)
{
    WaitWnd.Create(showTime);
    if (!aborting)
        DataConnection->ActivateConnection();
}

BOOL CSendCmdUserIfaceForListAndDownload::HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort)
{
    BOOL offerAbort = isSend && allowCmdAbort ||           // pokud je abort mozny a jeste jsme ho neprovedli
                      DataConnection->IsTransfering(NULL); // pokud neprobiha prenos dat, neumime abort (jen terminate "control connection")

    WaitWnd.Show(FALSE);
    BOOL esc = SalamanderGeneral->SalMessageBox(parent,
                                                LoadStr(!AlreadyAborted ? (offerAbort ? (ForDownload ? IDS_LISTWNDDOWNLFILESENDCMDESC : IDS_LISTWNDSENDCOMMANDESC) : (ForDownload ? IDS_LISTWNDDOWNLFILESENDCMDESC2 : IDS_LISTWNDSENDCOMMANDESC2)) : IDS_LISTWNDABORTCOMMANDESC),
                                                LoadStr(IDS_FTPPLUGINTITLE),
                                                MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
    if (esc)
    {
        WaitWnd.SetText(LoadStr(ForDownload ? IDS_LISTWNDDOWNLFILEABORTING : IDS_LISTWNDABORTINGCOMMAND));
        if (!offerAbort)
            AlreadyAborted = TRUE;
        if (!AlreadyAborted)
        {
            if (DataConnection->IsTransfering(NULL) ||  // jen tak pro formu, treba uz je zavrena connectiona a zaroven
                DataConnection->IsFlushingDataToDisk()) // uz neni co flushnout, at nepiseme do logu bludy moc casto (stejne muze nastat, kaslem na to)
            {
                DataConnection->CancelConnectionAndFlushing(); // zavreme "data connection", system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
                Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONTERMINATED), -1, TRUE);
            }
            AlreadyAborted = TRUE;
            if (!allowCmdAbort)
                esc = FALSE; // to jeste neni ESC pro listovani
        }
    }
    else
        SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu
    if (!esc)
        WaitWnd.Show(TRUE); // nic se nestalo, pokracujeme v zobrazeni wait-okenka (behem abortu se zobrazi jiny text v okenku)
    return esc;
}

void CSendCmdUserIfaceForListAndDownload::SendingFinished()
{
    WaitWnd.Destroy();
}

BOOL CSendCmdUserIfaceForListAndDownload::IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID,
                                                    char* errBuf, int errBufSize)
{
    BOOL trFinished;
    BOOL ret = FALSE;
    if (DataConnection->IsTransfering(&trFinished))
        *start = GetTickCount(); // cekame na data, takze to neni timeout
    else
    {
        if (trFinished)
        {
            *start = DataConnection->GetSocketCloseTime();
            ret = (GetTickCount() - *start) >= serverTimeout; // timeout se meri od zavreni connectiony (okamzik odkdy muze server reagovat - take se dozvi o zavreni connectiony)
        }
        else
            ret = TRUE; // spojeni se jeste neotevrelo -> timeoutneme
    }
    if (ret)
    {
        char errText[300];
        if (DataConnection->GetProxyTimeoutDescr(errText, 300))
        {
            if (errBufSize > 0)
                _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), errText);
            *errorTextID = -1; // popis je primo v 'errBuf'
        }
        else
            *errorTextID = ForDownload ? IDS_LISTWNDDOWNLFILETIMEOUT : IDS_LISTCMDTIMEOUT;
    }
    return ret;
}

void CSendCmdUserIfaceForListAndDownload::CancelDataCon()
{
    DataConnection->CancelConnectionAndFlushing();
}

void CSendCmdUserIfaceForListAndDownload::MaybeSuccessReplyReceived(const char* reply, int replySize)
{
    DataConnection->EncryptPassiveDataCon();

    CQuadWord size;
    if (FTPGetDataSizeInfoFromSrvReply(size, reply, replySize))
    {
        // mame celkovou velikost listingu - 'size'
        DataConnection->SetDataTotalSize(size);
    }
}

BOOL CSendCmdUserIfaceForListAndDownload::CanFinishSending(int replyCode, BOOL* useTimeout)
{
    BOOL trFinished;
    BOOL ret = DataConnection->IsTransfering(&trFinished);
    if (!ret && !trFinished && DataConnection->IsConnected()) // zatim nebylo navazane spojeni a socket je otevreny
    {
        if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS)
        {
            *useTimeout = TRUE; // nebudeme zavirat socket, bypass bugy warftpd: umi vratit "success" jeste pred acceptnutim socketu data-connectiony (pred zahajenim data-transferu) - vypis chyby v pripade, ze v teto situaci dojde k timeoutu (warftpd neprovede data-transfer) neresime, list&view nejsou az tak dulezite a je mala sance, ze k tomu kdy dojde
            ret = TRUE;         // simulujeme, ze se zrovna prenasi data
        }
        else
            DataConnection->CloseSocketEx(NULL); // zavru socket (jen se ceka na spojeni), server zrejme hlasi chybu prikazu (listovani)
    }
    if (!ret && ForDownload)
    {
        SocketsThread->LockSocketsThread();
        ret = !DataConnection->AreAllDataFlushed(FALSE);
        SocketsThread->UnlockSocketsThread();
        if (!ret)
            DataConnection->CloseTgtFile(); // uspesne zavreni souboru po dokonceni flushe dat
    }
    return !ret; // bud nedoslo ke spojeni nebo uz je zavrene
}

void CSendCmdUserIfaceForListAndDownload::BeforeWaitingForFinish(int replyCode, BOOL* useTimeout)
{
    if (FTP_DIGIT_1(replyCode) != FTP_D1_SUCCESS) // LIST nevraci uspech - nemusi vubec zavrit
    {                                             // data connection (napr. WarFTPD) - pockame na zbyla data, ovsem radsi s timeoutem
        *useTimeout = TRUE;
        //    DataConnection->CloseSocketEx(NULL);   // aby bylo co ukazat v panelu na prikaz Show Raw Listing, musime dotahnout zbyla data
    }
}

void CSendCmdUserIfaceForListAndDownload::HandleDataConTimeout(DWORD* start)
{
    DWORD lastActTime = DataConnection->GetLastActivityTime();
    if (*start < lastActTime)
        *start = lastActTime;
    else
    {
        BOOL trFinished;
        if (!DataConnection->IsTransfering(&trFinished) && !trFinished &&
            DataConnection->IsConnected()) // zatim nebylo navazane spojeni a socket je otevreny
        {
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONNOTOPENED), -1, TRUE);
        }
        DatConCancelled = TRUE;
        DataConnection->CancelConnectionAndFlushing(); // ukoncime cekani na data, uz cekame prilis dlouho (nastal timeout)
    }
}

void CSendCmdUserIfaceForListAndDownload::HandleESCWhenWaitingForFinish(HWND parent)
{
    WaitWnd.Show(FALSE);
    BOOL esc = SalamanderGeneral->SalMessageBox(parent,
                                                LoadStr(ForDownload ? IDS_LISTWNDDOWNLFILESENDCMDESC : IDS_LISTWNDSENDCOMMANDESC),
                                                LoadStr(IDS_FTPPLUGINTITLE),
                                                MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
    if (esc)
    {
        // WaitWnd.SetText(LoadStr(IDS_LISTWNDABORTINGCOMMAND)); // zbytecne, okno se uz neukaze
        // user vymysli odpoved na abort a behem te doby muze data connection skoncit (proto je
        // v hlasce "listing muze byt nekompletni") - potom ma smysl abort ignorovat
        if (DataConnection->IsTransfering(NULL) || DataConnection->IsFlushingDataToDisk())
        {
            DataConnection->CancelConnectionAndFlushing(); // zavreme "data connection", system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGDATACONTERMINATED), -1, TRUE);
            AlreadyAborted = TRUE;
        }
    }
    else
        SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu
    if (!esc)
        WaitWnd.Show(TRUE);
}

// ***********************************************************************************

BOOL CControlConnectionSocket::IsListCommandLIST_a()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = _stricmp(UseLIST_aCommand ? LIST_a_CMD_TEXT : (ListCommand != NULL && *ListCommand != 0 ? ListCommand : LIST_CMD_TEXT),
                        LIST_a_CMD_TEXT) == 0;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CControlConnectionSocket::ToggleListCommandLIST_a()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (IsListCommandLIST_a())
    {
        if (UseLIST_aCommand)
            UseLIST_aCommand = FALSE;
        else
        {
            if (ListCommand != NULL)
                free(ListCommand);
            ListCommand = NULL;
        }
    }
    else
        UseLIST_aCommand = TRUE;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CControlConnectionSocket::ListWorkingPath(HWND parent, const char* path, char* userBuf,
                                               int userBufSize, char** allocatedListing,
                                               int* allocatedListingLen, CFTPDate* listingDate,
                                               BOOL* pathListingIsIncomplete, BOOL* pathListingIsBroken,
                                               BOOL* pathListingMayBeOutdated,
                                               DWORD* pathListingStartTime, BOOL forceRefresh,
                                               int* totalAttemptNum, BOOL* fatalError,
                                               BOOL dontClearCache)
{
    CALL_STACK_MESSAGE4("CControlConnectionSocket::ListWorkingPath(, %s, , %d, , , , , , , , %d, , ,)",
                        path, userBufSize, forceRefresh);

    *fatalError = FALSE;
    *pathListingIsBroken = FALSE;
    BOOL ok = TRUE;
    BOOL ret = TRUE;
    char cmdBuf[50 + FTP_MAX_PATH];
    char logBuf[50 + FTP_MAX_PATH];
    char replyBuf[700];
    char errBuf[900 + FTP_MAX_PATH];
    char listCmd[FTPCOMMAND_MAX_SIZE + 2];

    HANDLES(EnterCriticalSection(&SocketCritSect));
    lstrcpyn(listCmd, UseLIST_aCommand ? LIST_a_CMD_TEXT : (ListCommand != NULL && *ListCommand != 0 ? ListCommand : LIST_CMD_TEXT),
             FTPCOMMAND_MAX_SIZE);
    BOOL usePassiveModeAux = UsePassiveMode;
    int logUID = LogUID; // UID logu teto connectiony
    int useListingsCacheAux = UseListingsCache;
    CFTPProxyForDataCon* dataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
    BOOL dataConProxyServerOK = ProxyServer == NULL || dataConProxyServer != NULL;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    strcat(listCmd, "\r\n");

    int attemptNum = 1;
    if (totalAttemptNum != NULL)
        attemptNum = *totalAttemptNum;
    const char* retryMsgAux = NULL;
    BOOL canRetry = FALSE;
    char retryMsgBuf[300];
    char hostTmp[HOST_MAX_SIZE];

    // ziskame datum, kdy byl listing vytvoren (predpokladame, ze jej server nejdrive vytvori, pak az posle)
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD lstStTime = 0;

    // alokujeme objekt pro "data connection"
    CDataConnectionSocket* dataConnection = dataConProxyServerOK ? new CDataConnectionSocket(FALSE, dataConProxyServer, EncryptDataConnection, pCertificate, CompressData, this) : NULL;
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
        *fatalError = TRUE; // fatalni chyba
    }
    else
    {
        while (1)
        {
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
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" je povolen, jdeme na dalsi reconnect
                    else
                    {
                        *fatalError = TRUE; // fatalni chyba
                        break;
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
                        if (canRetry)
                            retryMsgAux = retryMsgBuf; // "retry" je povolen, jdeme na dalsi reconnect
                        else
                        {
                            *fatalError = TRUE; // fatalni chyba
                            break;
                        }
                    }
                }
                else // nepodarilo se otevrit "listen" socket pro prijem datoveho spojeni ze serveru ->
                {    // zavrena connectiona (aby se dalo pouzit standardni Retry)
                    ok = FALSE;
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" je povolen, jdeme na dalsi reconnect
                    else
                    {
                        *fatalError = TRUE; // fatalni chyba
                        break;
                    }
                }
            }

            if (ok) // jsme-li jeste pripojeni, zmenime rezim prenosu na ASCII (uspesnost ignorujeme)
            {
                ok = SetCurrentTransferMode(parent, TRUE, NULL, NULL, 0, forceRefresh, &canRetry,
                                            retryMsgBuf, 300);
                if (!ok) // chyba -> zavrena connectiona
                {
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" je povolen, jdeme na dalsi reconnect
                    else
                    {
                        *fatalError = TRUE; // fatalni chyba
                        break;
                    }
                }
            }

            BOOL sslErrReconnect = FALSE;     // TRUE = reconnect kvuli chybam SSL
            BOOL fastSSLErrReconnect = FALSE; // TRUE = jde o zmenu certifikatu serveru, zadouci je okamzity reconnect (bez 20 vterin cekani)
            if (ok)
            {
                int ftpReplyCode;
                CSendCmdUserIfaceForListAndDownload userIface(FALSE, parent, dataConnection, logUID);

                //        if (UsePassiveMode) {
                //          dataConnection->EncryptConnection();
                //        }
                HANDLES(EnterCriticalSection(&SocketCritSect));
                lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
                CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                userIface.InitWnd(NULL, hostTmp, path, pathType);
                lstStTime = IncListingCounter();
                if (SendFTPCommand(parent, listCmd, listCmd, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                   &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                                   retryMsgBuf, 300, &userIface))
                {
                    if (!userIface.GetDatConCancelled() && !userIface.WasAborted() && !userIface.HadError() &&
                        FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS &&
                        FTP_DIGIT_2(ftpReplyCode) != FTP_D2_CONNECTION) // neni to jen chyba spojeni (sitova)
                    {                                                   // server odmita listovat
                        BOOL skipMessage = FTPIsEmptyDirListErrReply(replyBuf);
                        if (!skipMessage)
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_LISTPATHERROR), path, replyBuf);
                            // pokud da uzivatel IDNO, koncime - cestu nelze listovat -> nutna zmena cesty
                            ret = SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                                   MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                       MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
                            if (!ret)
                                SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu

                            *pathListingIsBroken = TRUE; // aby se vedelo, ze vraceny listing neni OK (VxWorks: behem listovani umi vratit chybu "error reading entry: 16" + vratit "550 no files found or ...")
                        }
                        // VMS vraci 550 u prazdneho adresare: nemuzeme cestu opustit + listing klidne
                        // muzeme povazovat za OK (lze i cachovat - nebyl preruseny + server jiny listing
                        // nejspis jen tak nevrati)
                        // ret = FALSE;   // koncime - cestu nelze listovat -> nutna zmena cesty

                        break; // hlasime "uspesny listing" (umozni pracovat v prazdnem/nelistovatelnem adresari)
                    }
                    else
                    {
                        if (userIface.WasAborted()) // uzivatel listing abortoval - koncime s chybou (nekompletnim listingem)
                            ok = FALSE;             // hlasku "list can be incomplete" vypisovat nebudeme, user byl varovan pri abortu
                        else
                        {
                            if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS &&
                                    FTP_DIGIT_2(ftpReplyCode) == FTP_D2_CONNECTION || // jde jen o sitovou chybu
                                userIface.HadError() ||                               // datova connectiona zaznamenala chybu hlasenou systemem
                                userIface.GetDatConCancelled())                       // datova connectiona byla prerusena (bud nedoslo k otevreni nebo zavreni po chybe hlasene serverem v odpovedi na LIST)
                            {
                                ok = FALSE;

                                DWORD err;
                                BOOL lowMem, noDataTrTimeout;
                                int sslErrorOccured;
                                userIface.GetError(&err, &lowMem, NULL, &noDataTrTimeout, &sslErrorOccured, NULL);

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
                                    fastSSLErrReconnect = sslErrorOccured == SSLCONERR_UNVERIFIEDCERT || sslReuseErr;
                                }
                                else
                                {
                                    // vypiseme hlasku "list can be incomplete", user jeste nebyl varovan
                                    lstrcpyn(errBuf, LoadStr(IDS_UNABLETOREADLIST), 900 + FTP_MAX_PATH);
                                    int len = (int)strlen(errBuf);
                                    BOOL systErr = FALSE;
                                    BOOL trModeHint = FTP_DIGIT_1(ftpReplyCode) == FTP_D1_TRANSIENTERROR &&
                                                      FTP_DIGIT_2(ftpReplyCode) == FTP_D2_CONNECTION;

                                    if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS ||
                                        FTP_DIGIT_2(ftpReplyCode) != FTP_D2_CONNECTION ||
                                        noDataTrTimeout || sslErrorOccured != SSLCONERR_NOERROR)
                                    { // pokud nemame popis sitove chyby ze serveru, spokojime se se systemovym popisem
                                        systErr = TRUE;
                                        if (!trModeHint)
                                            trModeHint = err == WSAETIMEDOUT || sslErrorOccured != SSLCONERR_NOERROR;
                                        if (sslErrorOccured != SSLCONERR_NOERROR)
                                        {
                                            lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONSSLCONNECTERROR), 700);
                                            strcat(replyBuf, "\r\n");
                                        }
                                        else
                                        {
                                            if (noDataTrTimeout)
                                                lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNODATATRTIMEOUT), 700);
                                            else
                                            {
                                                if (err != NO_ERROR)
                                                {
                                                    if (!dataConnection->GetProxyError(replyBuf, 700, NULL, 0, TRUE))
                                                        FTPGetErrorText(err, replyBuf, 700);
                                                }
                                                else
                                                {
                                                    if (userIface.GetDatConCancelled())
                                                        lstrcpyn(replyBuf, LoadStr(IDS_ERRDATACONNOTOPENED), 700);
                                                    else
                                                        lstrcpyn(replyBuf, LoadStr(IDS_UNKNOWNERROR), 700);
                                                }
                                            }
                                        }
                                    }
                                    _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE,
                                                LoadStr(systErr ? (trModeHint ? IDS_UNABLETOREADLISTSUFFIX3 : IDS_UNABLETOREADLISTSUFFIX) : (trModeHint ? IDS_UNABLETOREADLISTSUFFIX4 : IDS_UNABLETOREADLISTSUFFIX2)),
                                                replyBuf);
                                    SalamanderGeneral->SalMessageBox(parent, errBuf,
                                                                     LoadStr(IDS_FTPERRORTITLE),
                                                                     MB_OK | MB_ICONEXCLAMATION);
                                }
                            }
                        }
                        if (!sslErrReconnect)
                            break; // abortovano nebo totalni uspech (vylistovano vse - 'ok'==TRUE)
                    }
                }
                else // zavrena connectiona
                {
                    if (userIface.WasAborted()) // uzivatel listing abortoval - cimz se terminovala connectiona (dela napr. sunsolve.sun.com (Sun Unix) nebo ftp.chg.ru) - koncime s chybou (nekompletnim listingem)
                    {
                        ok = FALSE;   // hlasku "list can be incomplete" vypisovat nebudeme, user byl varovan pri abortu
                        if (canRetry) // prevezmeme hlaseni do messageboxu, ktery oznami preruseni spojeni
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            if (ConnectionLostMsg != NULL)
                                SalamanderGeneral->Free(ConnectionLostMsg);
                            ConnectionLostMsg = SalamanderGeneral->DupStr(retryMsgBuf);
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                        break; // abortovano
                    }

                    // chyba -> 'ok' zustava FALSE, jdeme na dalsi reconnect
                    ok = FALSE;
                    if (canRetry)
                        retryMsgAux = retryMsgBuf; // "retry" je povolen
                    else
                    {
                        *fatalError = TRUE; // fatalni chyba
                        break;
                    }
                }
            }

            if (!ok) // spojeni bylo preruseno, optame se na reconnect
            {
                if (dataConnection->IsConnected())       // zavreme starou "data connection" (pro pripad, ze nedosel FD_CONNECT)
                    dataConnection->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)

                SetStartTime();
                BOOL startRet = StartControlConnection(parent, userBuf, userBufSize, TRUE, NULL, 0,
                                                       &attemptNum, retryMsgAux, FALSE, sslErrReconnect ? IDS_LISTCOMMANDERROR : -1,
                                                       fastSSLErrReconnect);
                retryMsgAux = NULL;
                if (totalAttemptNum != NULL)
                    *totalAttemptNum = attemptNum;
                if (startRet) // spojeni obnoveno
                {
                    if (pCertificate) // certifikat control-connectiony se mohl zmenit, predame pripadny novy do data-connectiony
                        dataConnection->SetCertificate(pCertificate);

                    // zmenime cestu na 'path' (cesta, kterou listujeme)
                    PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                      ftpcmdChangeWorkingPath, NULL, path);
                    int ftpReplyCode;
                    if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                       &ftpReplyCode, replyBuf, 700, FALSE, TRUE, FALSE, &canRetry,
                                       retryMsgBuf, 300, NULL))
                    {
                        BOOL pathError = TRUE;
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // je nadeje na uspech, jeste radsi cestu zkontrolujeme
                        {
                            if (GetCurrentWorkingPath(parent, cmdBuf, FTP_MAX_PATH, TRUE, &canRetry,
                                                      retryMsgBuf, 300))
                            {
                                if (strcmp(cmdBuf, path) == 0) // mame pozadovany pracovni adresar na serveru
                                                               // (predpoklad: server vraci stale stejny retezec pracovni cesty)
                                {
                                    pathError = FALSE;
                                    ok = TRUE; // uspesny reconnect, jdeme znovu listovat
                                }
                            }
                            else
                            {
                                pathError = FALSE; // chyba -> zavrena connectiona - 'ok' zustava FALSE, jdeme na dalsi reconnect
                                if (canRetry)
                                    retryMsgAux = retryMsgBuf; // "retry" je povolen
                                else
                                {
                                    *fatalError = TRUE; // fatalni chyba
                                    break;
                                }
                            }
                        }

                        if (pathError) // vypiseme chybu cesty a koncime
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
                            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                             MB_OK | MB_ICONEXCLAMATION);
                            ret = FALSE; // koncime - cestu nelze listovat -> nutna zmena cesty

                            // pokud jsme nenasli zadnou pristupnou cestu, na tomto miste provedeme disconnect,
                            // pri connectu je problem osetreny (v CControlConnectionSocket::ChangeWorkingPath()),
                            // zde uz to neni na me nervy ;-)

                            break;
                        }
                    }
                    else // chyba -> zavrena connectiona - 'ok' zustava FALSE, jdeme na dalsi reconnect
                    {
                        if (canRetry)
                            retryMsgAux = retryMsgBuf; // "retry" je povolen
                        else
                        {
                            *fatalError = TRUE; // fatalni chyba
                            break;
                        }
                    }
                }
                else // reconnect se nepodaril - koncime s chybou (nekompletnim listingem)
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_UNABLETOREADLIST),
                                                     LoadStr(IDS_FTPERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
            }
        }
    }

    if (ret && !*fatalError) // neni chyba cesty ani fatalni chyba
    {
        if (dataConnection->IsConnected()) // chyba: "data connection" uz ma byt davno zavrena
        {
            TRACE_E("Unexpected situation in CControlConnectionSocket::ListWorkingPath(): data connection has left opened!");
            dataConnection->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
        }

        // prevezmeme data z "data connection"
        BOOL decomprErr;
        *allocatedListing = dataConnection->GiveData(allocatedListingLen, &decomprErr);

        if (decomprErr && ok)
        {
            ok = FALSE;

            // vypiseme hlasku "list can be incomplete", user jeste nebyl varovan
            lstrcpyn(errBuf, LoadStr(IDS_UNABLETOREADLIST), 900 + FTP_MAX_PATH);
            int len = (int)strlen(errBuf);
            _snprintf_s(errBuf + len, 900 + FTP_MAX_PATH - len, _TRUNCATE, LoadStr(IDS_UNABLETOREADLISTSUFFIX),
                        LoadStr(IDS_ERRDATACONDECOMPRERROR));
            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }

        *pathListingIsIncomplete = !ok; // TRUE pri vypadku/preruseni/chybe spojeni
        *pathListingMayBeOutdated = FALSE;

        // ulozime datum, kdy byl listing vytvoren
        listingDate->Year = st.wYear;
        listingDate->Month = (BYTE)st.wMonth;
        listingDate->Day = (BYTE)st.wDay;
        *pathListingStartTime = lstStTime;

        char userTmp[USER_MAX_SIZE];
        if (forceRefresh &&  // tvrdy refresh bereme jako projev neduvery k ceste, zahodime ji z cache vcetne
            !dontClearCache) // vsech podcest (useListingsCacheAux ignorujeme, to na neduveru nema vliv)
        {                    // vola se az kdyz mame nahradni listing (do te doby bude user jiste radsi
                             // na neaktualnim listingu nez na zadnem)
            HANDLES(EnterCriticalSection(&SocketCritSect));
            lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
            unsigned short portTmp = Port;
            lstrcpyn(userTmp, User, USER_MAX_SIZE);
            CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            ListingCache.RefreshOnPath(hostTmp, portTmp, userTmp, pathType, path);
        }

        if (ok) // mame kompletni listing
        {
            if (!*pathListingIsBroken && useListingsCacheAux && *allocatedListing != NULL)
            { // user chce cache pouzivat -> pridame nove nacteny listing do cache
                HANDLES(EnterCriticalSection(&SocketCritSect));
                lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
                unsigned short portTmp = Port;
                lstrcpyn(userTmp, User, USER_MAX_SIZE);
                CFTPServerPathType pathType = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
                BOOL isFTPS = EncryptControlConnection == 1;
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                ListingCache.AddOrUpdatePathListing(hostTmp, portTmp, userTmp, pathType, path,
                                                    listCmd, isFTPS, *allocatedListing,
                                                    *allocatedListingLen, listingDate,
                                                    *pathListingStartTime);
            }
        }
        else // vypadek/preruseni/chyba spojeni = vratime aspon co mame (user uz vi, ze "list can be incomplete")
        {
            if (*allocatedListing != NULL)
            { // v bufferu neni kompletni listing -> zarizneme ho na poslednim konci radku (CRLF nebo LF), at se s nim lepe pracuje
                char* start = *allocatedListing;
                char* s = start + *allocatedListingLen;
                while (s > start && *(s - 1) != '\n')
                    s--;
                if (s < start + *allocatedListingLen) // je kam zapsat nulu terminujici retezec (jen pro snazsi debugging)
                    *s = 0;                           // bud je to na zacatku bufferu nebo za poslednim LF
                *allocatedListingLen = (int)(s - start);
            }
        }
    }
    if (dataConnection != NULL) // uvolnime a prip. i zavreme "data connection"
    {
        if (dataConnection->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
            dataConnection->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
        DeleteSocket(dataConnection);
    }
    if (*fatalError)
        ret = FALSE; // u fatalni chyby uspech vracet jiste nebudeme
    return ret;
}

class CFinishingKeepAliveUserIface : public CSendCmdUserIfaceAbstract
{
protected:
    CWaitWindow* WaitWnd;
    HANDLE FinishedEvent;

public:
    CFinishingKeepAliveUserIface(CWaitWindow* waitWnd, HANDLE finishedEvent)
    {
        WaitWnd = waitWnd;
        FinishedEvent = finishedEvent;
    }

    virtual BOOL GetWindowClosePressed() { return WaitWnd->GetWindowClosePressed(); }
    virtual HANDLE GetFinishedEvent() { return FinishedEvent; }

    // ostatni metody se nevyuzivaji
    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText) {}
    virtual void BeforeAborting() {}
    virtual void AfterWrite(BOOL aborting, DWORD showTime) {}
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort) { return FALSE; }
    virtual void SendingFinished() {}
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize) { return FALSE; }
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize) {}
    virtual void CancelDataCon() {}
    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout) { return FALSE; }
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout) {}
    virtual void HandleDataConTimeout(DWORD* start) {}
    virtual void HandleESCWhenWaitingForFinish(HWND parent) {}
};

void CControlConnectionSocket::WaitForEndOfKeepAlive(HWND parent, int waitWndTime)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::WaitForEndOfKeepAlive(, %d)", waitWndTime);

    DWORD startTime = GetTickCount(); // cas zacatku operace

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::WaitForEndOfKeepAlive(): from section SocketCritSect!");
#endif

    if (!KeepAliveEnabled && KeepAliveMode != kamNone)
        TRACE_E("CControlConnectionSocket::WaitForEndOfKeepAlive(): Keep-Alive is disabled, but Mode == " << (int)KeepAliveMode);

    if (KeepAliveEnabled &&
        (KeepAliveMode == kamProcessing ||               // provadi se keep-alive prikaz, musime cekat na dokonceni
         KeepAliveMode == kamWaitingForEndOfProcessing)) // uz cekame na dokonceni (nemelo by nastat)
    {
        KeepAliveMode = kamWaitingForEndOfProcessing;
        HANDLE finishedEvent = KeepAliveFinishedEvent;
        int logUID = LogUID;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        // ukazeme wait-okenko, ze cekame na konec keep-alive prikazu
        CWaitWindow waitWnd(parent, TRUE);
        waitWnd.SetText(LoadStr(IDS_FINISHINGKEEPALIVECMD));
        DWORD start = GetTickCount();
        DWORD waitTime = start - startTime;
        waitWnd.Create(waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);

        // pockame na dokonceni nebo preruseni (ESC/timeout) keep-alive prikazu
        int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
        if (serverTimeout < 1000)
            serverTimeout = 1000; // aspon sekundu
        CFinishingKeepAliveUserIface userIface(&waitWnd, finishedEvent);
        BOOL wait = TRUE;
        while (wait)
        {
            CControlConnectionSocketEvent event;
            DWORD data1, data2;
            DWORD now = GetTickCount();
            if (now - start > (DWORD)serverTimeout)
                now = start + (DWORD)serverTimeout;
            WaitForEventOrESC(parent, &event, &data1, &data2, serverTimeout - (now - start),
                              NULL, &userIface, TRUE);
            switch (event)
            {
            case ccsevESC:
            {
                waitWnd.Show(FALSE);
                if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_KEEPALIVECMDESC),
                                                     LoadStr(IDS_FTPPLUGINTITLE),
                                                     MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                         MB_ICONQUESTION) == IDYES)
                { // cancel
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE);
                    ReleaseKeepAlive(); // uvolnime keep-alive
                    CloseSocket(NULL);  // zavreme spojeni
                    Logs.SetIsConnected(logUID, IsConnected());
                    Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
                    wait = FALSE;
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
                BOOL isTimeout = TRUE;

                HANDLES(EnterCriticalSection(&SocketCritSect));
                if (KeepAliveDataCon != NULL)
                {
                    BOOL trFinished;
                    if (KeepAliveDataCon->IsTransfering(&trFinished))
                    { // cekame na data, takze to neni timeout
                        start = GetTickCount();
                        isTimeout = FALSE;
                    }
                    else
                    {
                        if (trFinished)
                        {
                            start = KeepAliveDataCon->GetSocketCloseTime();
                            isTimeout = (GetTickCount() - start) >= (DWORD)serverTimeout; // timeout se meri od zavreni connectiony (okamzik odkdy muze server reagovat - take se dozvi o zavreni connectiony)
                        }
                        // else isTimeout = TRUE;  // spojeni se jeste neotevrelo -> timeoutneme
                    }
                }
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                if (isTimeout)
                {
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGKEEPALIVECMDTIMEOUT), -1, TRUE);
                    ReleaseKeepAlive(); // uvolnime keep-alive
                    CloseSocket(NULL);  // zavreme spojeni
                    Logs.SetIsConnected(logUID, IsConnected());
                    Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
                    wait = FALSE;
                }
                break;
            }

            case ccsevNewBytesRead:
                break; // ignorujeme (hrozi max. nejaky stary event + po teto metode nasleduje stejne zapis prikazu, pak teprve server odpovi)

            case ccsevClosed: // zavreni spojeni, jen zabalime keep-alive a nechame to vyresit nekoho jineho
            {
                ReleaseKeepAlive();
                AddEvent(ccsevClosed, data1, data2);
                wait = FALSE;
                break;
            }

            case ccsevUserIfaceFinished:
                wait = FALSE;
                break; // keep-alive prikaz se dokoncil

            default:
            {
                TRACE_E("CControlConnectionSocket::WaitForEndOfKeepAlive: Unexpected event (" << (int)event << ").");
                break;
            }
            }
        }
        waitWnd.Destroy();

        // keep-alive prikaz uz se dokoncil nebo byl prerusen (ESC/timeout)
        HANDLES(EnterCriticalSection(&SocketCritSect));
        KeepAliveMode = kamForbidden;
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
    else
    {
        if (KeepAliveEnabled)
        {
            BOOL deleteTimer = FALSE;
            int uid;
            if (KeepAliveMode == kamWaiting)
            {
                deleteTimer = TRUE;
                uid = UID;
            }
            KeepAliveMode = kamForbidden;
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            if (deleteTimer)
            {
                // v modu 'kamForbidden' nema keep-alive timer smysl (pokud dojde k jeho timeoutu,
                // jen se vyignoruje), vymazeme ho
                SocketsThread->DeleteTimer(uid, CTRLCON_KEEPALIVE_TIMERID);
            }
        }
        else
            HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
}

void CControlConnectionSocket::SetupKeepAliveTimer(BOOL immediate)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::SetupKeepAliveTimer(%d)", immediate);

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::SetupKeepAliveTimer(): from section SocketCritSect!");
#endif

    if (!KeepAliveEnabled && KeepAliveMode != kamNone)
        TRACE_E("CControlConnectionSocket::SetupKeepAliveTimer(): Keep-Alive is disabled, but Mode == " << (int)KeepAliveMode);
    BOOL timer = FALSE;
    int msg;
    int uid;
    DWORD ti;
    if (KeepAliveEnabled && KeepAliveMode == kamForbidden) // volano po dokonceni normalniho prikazu
    {
        KeepAliveMode = kamWaiting;
        timer = TRUE;
        msg = Msg;
        uid = UID;
        KeepAliveStart = GetTickCount();                                   // cas posledniho provedeneho normalniho prikazu v "control connection"
        ti = KeepAliveStart + (immediate ? 0 : KeepAliveSendEvery * 1000); // cas, kdy by se mel poslat prvni keep-alive prikaz
    }
    else
    {
        if (KeepAliveMode != kamNone)
            TRACE_E("CControlConnectionSocket::SetupKeepAliveTimer(): unexpected Mode == " << (int)KeepAliveMode);
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (timer) // mame nahodit keep-alive timer
        SocketsThread->AddTimer(msg, uid, ti, CTRLCON_KEEPALIVE_TIMERID, NULL);
}

void CControlConnectionSocket::SetupNextKeepAliveTimer()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::SetupNextKeepAliveTimer()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::SetupNextKeepAliveTimer(): from section SocketCritSect!");
#endif

    if (!KeepAliveCmdAllBytesWritten)
    { // nemelo by nikdy nastat, protoze odpoved ze serveru prijde az po zapsani kompletniho
        // prikazu (navic prikaz se vzdy zapise najednou, je to par bytu)
        TRACE_E("Unexpected situation in CControlConnectionSocket::SetupNextKeepAliveTimer(): KeepAliveCmdAllBytesWritten==FALSE!");
        KeepAliveCmdAllBytesWritten = TRUE;
    }

    if (KeepAliveDataCon != NULL || KeepAliveDataConState != kadcsNone)
        TRACE_E("Unexpected situation in CControlConnectionSocket::SetupNextKeepAliveTimer(): KeepAliveDataCon!=NULL or KeepAliveDataConState!=kadcsNone!");

    BOOL timer = FALSE;
    int msg;
    int uid;
    DWORD ti;
    if (KeepAliveMode == kamProcessing) // keep-alive normalne dobehl, vyhodnotime jestli znovu nastavime keep-alive timer
    {
        ti = GetTickCount() + KeepAliveSendEvery * 1000; // cas, kdy by se mel poslat dalsi keep-alive prikaz
        if ((int)((ti - KeepAliveStart) / 60000) < KeepAliveStopAfter)
        {
            KeepAliveMode = kamWaiting;
            timer = TRUE;
            msg = Msg;
            uid = UID;
        }
        else
        {
            KeepAliveMode = kamNone;                                         // dale uz keep-alive nemame provadet (uz nema cenu hajit connectionu)
            Logs.LogMessage(LogUID, LoadStr(IDS_LOGMSGKASTOPPED), -1, TRUE); // upozornime usera na zastaveni keep-alive rezimu
        }
    }
    else
    {
        if (KeepAliveMode == kamWaitingForEndOfProcessing) // hl. thread ceka na dokonceni keep-alive prikazu
        {
            SetEvent(KeepAliveFinishedEvent);
        }
        else
        {
            if (KeepAliveMode != kamNone) // kamNone = doslo k volani ReleaseKeepAlive()
                TRACE_E("CControlConnectionSocket::SetupNextKeepAliveTimer(): unexpected Mode == " << (int)KeepAliveMode);
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (timer) // mame nahodit keep-alive timer
        SocketsThread->AddTimer(msg, uid, ti, CTRLCON_KEEPALIVE_TIMERID, NULL);
}

void CControlConnectionSocket::ReleaseKeepAlive()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::ReleaseKeepAlive()");

    HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CControlConnectionSocket::ReleaseKeepAlive(): from section SocketCritSect!");
#endif

    if (KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing)
        SetEvent(KeepAliveFinishedEvent); // pustime dal hl. thread
    BOOL deleteTimer = FALSE;
    int uid;
    if (KeepAliveMode == kamWaiting)
    {
        deleteTimer = TRUE;
        uid = UID;
    }
    KeepAliveMode = kamNone; // reinicializace keep-alive
    KeepAliveCmdAllBytesWritten = TRUE;
    CKeepAliveDataConSocket* closeDataCon = KeepAliveDataCon;
    KeepAliveDataCon = NULL;
    KeepAliveDataConState = kadcsNone;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    // je-li otevrena "data connection", zavreme ji, ted uz jiste nebude potreba
    if (closeDataCon != NULL)
    {
        if (closeDataCon->IsConnected())       // zavreme "data connection", system se pokusi o "graceful"
            closeDataCon->CloseSocketEx(NULL); // shutdown (nedozvime se o vysledku)
        DeleteSocket(closeDataCon);            // uvonime "data connection" pres volani metody SocketsThread
    }

    if (deleteTimer)
    {
        // v modu 'kamNone' nema keep-alive timer smysl (pokud dojde k jeho timeoutu,
        // jen se vyignoruje), vymazeme ho
        SocketsThread->DeleteTimer(uid, CTRLCON_KEEPALIVE_TIMERID);
    }
}

void CControlConnectionSocket::PostMsgToCtrlCon(int msgID, void* msgParam)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::PostMsgToCtrlCon()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int msg = Msg;
    int uid = UID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    SocketsThread->PostSocketMessage(msg, uid, msgID, msgParam);
}

void CControlConnectionSocket::ReceiveTimer(DWORD id, void* param)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::ReceiveTimer(%u,)", id);
    if (id == CTRLCON_KEEPALIVE_TIMERID)
    {
        BOOL sendKACmd = FALSE;
        int cmd;
        HANDLES(EnterCriticalSection(&SocketCritSect));
        int logUID = LogUID;
        BOOL usePassiveModeAux;
        if (KeepAliveEnabled && KeepAliveMode == kamWaiting) // nic nebrani v poslani keep-alive prikazu
        {
            KeepAliveMode = kamProcessing;
            ResetEvent(KeepAliveFinishedEvent); // priprava pro pouziti eventu pro blokovani hl.threadu az do dokonceni keep-alive prikazu
            sendKACmd = TRUE;
            usePassiveModeAux = UsePassiveMode;

            if (KeepAliveCommand == 2 /* NLST */ || KeepAliveCommand == 3 /* LIST */)
            {
                // alokujeme objekt pro "data connection"
                if (KeepAliveDataCon != NULL)
                    TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveTimer(): KeepAliveDataCon is not NULL!");
                CFTPProxyForDataCon* dataConProxyServer = ProxyServer == NULL ? NULL : ProxyServer->AllocProxyForDataCon(ServerIP, Host, HostIP, Port);
                BOOL dataConProxyServerOK = ProxyServer == NULL || dataConProxyServer != NULL;
                KeepAliveDataCon = dataConProxyServerOK ? new CKeepAliveDataConSocket(this, dataConProxyServer, EncryptDataConnection, pCertificate) : NULL;
                if (KeepAliveDataCon == NULL)
                {
                    if (dataConProxyServer != NULL)
                        delete dataConProxyServer;
                    if (dataConProxyServerOK)
                        TRACE_E(LOW_MEMORY);
                    KeepAliveCommand = 0; // posleme misto toho "NOOP"
                }
                else
                    KeepAliveDataConState = usePassiveModeAux ? kadcsWaitForPassiveReply : kadcsWaitForListen;
            }
            cmd = KeepAliveCommand;
        }
        CKeepAliveDataConSocket* keepAliveDataConAux = KeepAliveDataCon;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (sendKACmd)
        {
            // poslani keep-alive prikazu (zadne casove prodlevy, nesmime na nic cekat)
            char ftpCmd[200];
            ftpCmd[0] = 0;
            BOOL waitForListen = FALSE;
            switch (cmd)
            {
            case 0: // NOOP
            {
                PrepareFTPCommand(ftpCmd, 200, NULL, 0, ftpcmdNoOperation, NULL);
                break;
            }

            case 1: // PWD
            {
                PrepareFTPCommand(ftpCmd, 200, NULL, 0, ftpcmdPrintWorkingPath, NULL);
                break;
            }

            case 2: // NLST
            case 3: // LIST
            {
                if (usePassiveModeAux) // pasivni mod "data connection"
                {
                    PrepareFTPCommand(ftpCmd, 200, NULL, 0, ftpcmdPassive, NULL);
                }
                else // aktivni mod "data connection"
                {
                    DWORD localIP;
                    GetLocalIP(&localIP, NULL);   // snad ani nemuze vratit chybu
                    unsigned short localPort = 0; // listen on any port
                    DWORD error;
                    keepAliveDataConAux->SetActive(logUID);
                    BOOL listenError;
                    if (!keepAliveDataConAux->OpenForListeningWithProxy(localIP, localPort, &listenError, &error))
                    { // nepodarilo se otevrit "listen" socket pro prijem datoveho spojeni ze
                        // serveru (lokalni operace, nejspis nikdy nenastane) + muze byt
                        // i chyba pri connectu na proxy server
                        Logs.LogMessage(logUID, LoadStr(listenError ? IDS_LOGMSGOPENACTDATACONERROR : IDS_LOGMSGOPENACTDATACONERROR2), -1, TRUE);
                    }
                    else
                        waitForListen = TRUE;
                }
                break;
            }

            default:
            {
                TRACE_E("CControlConnectionSocket::ReceiveTimer(): unknown keep-alive command!");
                ftpCmd[0] = 0;
                break;
            }
            }

            if (ftpCmd[0] != 0 || waitForListen)
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGKEEPALIVE), -1, TRUE);
            if (ftpCmd[0] != 0)
                SendKeepAliveCmd(logUID, ftpCmd); // posleme keep-alive prikaz
            else
            {
                if (!waitForListen)
                    ReleaseKeepAlive(); // nic jsme neposlali (nema smysl pokracovat v keep-alive), zrusime keep-alive
            }
        }
    }
}

void CControlConnectionSocket::ReceivePostMessage(DWORD id, void* param)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::ReceivePostMessage(%u,)", id);
    switch (id)
    {
    case CTRLCON_KAPOSTSETUPNEXT: // prave byla ukoncena "data connection" keep-alive prikazu, odpoved o ukonceni listingu od serveru uz prisla, zavolame SetupNextKeepAliveTimer()
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        BOOL call = (KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing); // nestihlo se stat nic neocekavaneho?
        CKeepAliveDataConSocket* closeDataCon = KeepAliveDataCon;
        if (call)
        {
            KeepAliveDataCon = NULL;
            KeepAliveDataConState = kadcsNone;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (call)
        {
            if (closeDataCon != NULL)
                DeleteSocket(closeDataCon);
            SetupNextKeepAliveTimer();
        }
        break;
    }

    case CTRLCON_LISTENFORCON: // zprava o otevreni portu pro "listen" (na proxy serveru)
    {
        AddEvent(ccsevListenForCon, (DWORD)(DWORD_PTR)param, 0);
        break;
    }

    case CTRLCON_KALISTENFORCON: // keep-alive: zprava o otevreni portu pro "listen" (na proxy serveru)
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if ((KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing) &&
            KeepAliveDataConState == kadcsWaitForListen)
        {
            CKeepAliveDataConSocket* kaDataConnection = KeepAliveDataCon;
            int logUID = LogUID; // UID logu teto connectiony
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            if ((int)(INT_PTR)param == kaDataConnection->GetUID()) // zpravu zpracujeme jen pokud je pro nasi data-connectionu
            {
                DWORD listenOnIP;
                unsigned short listenOnPort;
                char buf[300];
                char errBuf[500];
                if (!kaDataConnection->GetListenIPAndPort(&listenOnIP, &listenOnPort)) // chyba "listen"
                {
                    if (kaDataConnection->GetProxyError(buf, 300, NULL, 0, TRUE))
                    { // vypiseme chybu do logu
                        _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_LOGMSGDATCONERROR), buf);
                        Logs.LogMessage(logUID, errBuf, -1, TRUE);
                    }
                    ReleaseKeepAlive(); // koncime...
                }
                else // uspech, posleme prikaz "PORT"
                {
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    KeepAliveDataConState = kadcsWaitForSetPortReply;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    PrepareFTPCommand(buf, 300, NULL, 0, ftpcmdSetPort, NULL, listenOnIP, listenOnPort);
                    SendKeepAliveCmd(logUID, buf);
                }
            }
        }
        else
            HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }
    }
}

BOOL CControlConnectionSocket::InitOperation(CFTPOperation* oper)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::InitOperation()");
    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = oper->SetConnection(ProxyServer, Host, Port, User, Password, Account,
                                   InitFTPCommands, UsePassiveMode,
                                   UseLIST_aCommand ? LIST_a_CMD_TEXT : ListCommand,
                                   ServerIP, ServerSystem, ServerFirstReply,
                                   UseListingsCache, HostIP);
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

//
// ****************************************************************************
// CListingCacheItem
//

CListingCacheItem::CListingCacheItem(const char* host, unsigned short port, const char* user,
                                     const char* path, const char* listCmd, BOOL isFTPS,
                                     const char* cachedListing, int cachedListingLen,
                                     const CFTPDate& cachedListingDate,
                                     DWORD cachedListingStartTime, CFTPServerPathType pathType)
{
    // zkopirujeme data
    BOOL err = (host == NULL || path == NULL || listCmd == NULL);
    Host = SalamanderGeneral->DupStr(host);
    Port = port;
    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    User = SalamanderGeneral->DupStr(user); // je-li NULL, zustane NULL, ale 'err' se nezmeni
    Path = SalamanderGeneral->DupStr(path);
    ListCmd = SalamanderGeneral->DupStr(listCmd);
    IsFTPS = isFTPS;
    CachedListing = (char*)malloc(cachedListingLen + 1); // +1 jako reseni listingu s nulovou delkou
    if (CachedListing != NULL && cachedListing != NULL)
    {
        memcpy(CachedListing, cachedListing, cachedListingLen);
        CachedListing[cachedListingLen] = 0; // kdyz uz tam je naalokovano, tak pro debugovaci ucely to udelame null-terminated
    }
    else
        err = TRUE;
    CachedListingLen = cachedListingLen;
    CachedListingDate = cachedListingDate;
    CachedListingStartTime = cachedListingStartTime;
    PathType = pathType;

    // pri chybe uvolnime a nulujeme data
    if (err)
    {
        if (User != NULL)
            SalamanderGeneral->Free(User);
        if (Host != NULL)
            SalamanderGeneral->Free(Host);
        if (Path != NULL)
            SalamanderGeneral->Free(Path);
        if (ListCmd != NULL)
            SalamanderGeneral->Free(ListCmd);
        if (CachedListing != NULL)
        {
            memset(CachedListing, 0, CachedListingLen); // muze jit o tajna data, pro jistotu vynulujeme
            free(CachedListing);
        }
        User = NULL;
        Host = NULL;
        Path = NULL;
        ListCmd = NULL;
        CachedListing = NULL;
    }
    UserLength = FTPGetUserLength(User);
}

CListingCacheItem::~CListingCacheItem()
{
    if (User != NULL)
        SalamanderGeneral->Free(User);
    if (Host != NULL)
        SalamanderGeneral->Free(Host);
    if (Path != NULL)
        SalamanderGeneral->Free(Path);
    if (ListCmd != NULL)
        SalamanderGeneral->Free(ListCmd);
    if (CachedListing != NULL)
    {
        memset(CachedListing, 0, CachedListingLen); // muze jit o tajna data, pro jistotu vynulujeme
        free(CachedListing);
    }
}

//
// ****************************************************************************
// CListingCache
//

CListingCache::CListingCache() : Cache(100, 50), TotalCacheSize(0, 0)
{
    HANDLES(InitializeCriticalSection(&CacheCritSect));
}

CListingCache::~CListingCache()
{
#ifdef _DEBUG
    int i;
    for (i = 0; i < Cache.Count; i++)
        TotalCacheSize -= CQuadWord(Cache[i]->CachedListingLen, 0);
    if (TotalCacheSize != CQuadWord(0, 0))
        TRACE_E("CListingCache::~CListingCache(): TotalCacheSize is not zero when cache is empty!");
#endif
    HANDLES(DeleteCriticalSection(&CacheCritSect));
}

BOOL CListingCache::Find(const char* host, unsigned short port, const char* user,
                         CFTPServerPathType pathType, const char* path, const char* listCmd,
                         BOOL isFTPS, int* index)
{
    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    int i;
    for (i = 0; i < Cache.Count; i++)
    {
        CListingCacheItem* item = Cache[i];
        if (SalamanderGeneral->StrICmp(host, item->Host) == 0 &&
            (user == NULL && item->User == NULL ||
             item->User != NULL && user != NULL && strcmp(user, item->User) == 0) &&
            port == item->Port &&
            FTPIsTheSameServerPath(pathType, path, item->Path) &&
            isFTPS == item->IsFTPS &&
            SalamanderGeneral->StrICmp(listCmd, item->ListCmd) == 0)
        {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CListingCache::GetPathListing(const char* host, unsigned short port, const char* user,
                                   CFTPServerPathType pathType, char* path, int pathBufSize,
                                   const char* listCmd, BOOL isFTPS, char** cachedListing,
                                   int* cachedListingLen, CFTPDate* cachedListingDate,
                                   DWORD* cachedListingStartTime)
{
    HANDLES(EnterCriticalSection(&CacheCritSect));

    BOOL found = FALSE;
    int index;
    if (Find(host, port, user, pathType, path, listCmd, isFTPS, &index)) // updatneme polozku cache
    {
        found = TRUE;
        CListingCacheItem* item = Cache[index];
        *cachedListing = (char*)malloc(item->CachedListingLen + 1); // +1 jako reseni listingu s nulovou delkou
        if (*cachedListing != NULL)
        {
            memcpy(*cachedListing, item->CachedListing, item->CachedListingLen);
            (*cachedListing)[item->CachedListingLen] = 0; // kdyz uz tam je naalokovano, tak pro debugovaci ucely to udelame null-terminated
            *cachedListingLen = item->CachedListingLen;
        }
        else
            TRACE_E(LOW_MEMORY); // *cachedListingLen zustava 0 + chybu pameti zpracuje volajici
        *cachedListingDate = item->CachedListingDate;
        *cachedListingStartTime = item->CachedListingStartTime;
        lstrcpyn(path, item->Path, pathBufSize);
    }

    HANDLES(LeaveCriticalSection(&CacheCritSect));
    return found;
}

void CListingCache::AddOrUpdatePathListing(const char* host, unsigned short port, const char* user,
                                           CFTPServerPathType pathType, const char* path,
                                           const char* listCmd, BOOL isFTPS,
                                           const char* cachedListing, int cachedListingLen,
                                           const CFTPDate* cachedListingDate,
                                           DWORD cachedListingStartTime)
{
    HANDLES(EnterCriticalSection(&CacheCritSect));

    // je-li uz polozka v cache, smazeme ji (nema cenu se patlat s updatovanim jejich dat)
    int index;
    if (Find(host, port, user, pathType, path, listCmd, isFTPS, &index))
    {
        TotalCacheSize -= CQuadWord(Cache[index]->CachedListingLen, 0);
        Cache.Delete(index);
        if (!Cache.IsGood())
            Cache.ResetState();
    }

    // vlozime novou polozku do cache
    CListingCacheItem* item = new CListingCacheItem(host, port, user, path, listCmd, isFTPS,
                                                    cachedListing, cachedListingLen,
                                                    *cachedListingDate,
                                                    cachedListingStartTime, pathType);
    if (item != NULL && item->IsGood())
    {
        Cache.Add(item);
        if (Cache.IsGood())
        {
            TotalCacheSize += CQuadWord(item->CachedListingLen, 0);
            item = NULL; // je uspesne vlozena, nema se uvolnit pozdeji v teto metode

            // pokud uz je v cache moc polozek, uvolnime je od nejstarsi, alespon posledne
            // pridanou polozku v cache nechame
            int count = 0; // kolik polozek je potreba smazat (mazeme najednou, jinak slozitost O(N*N))
            while (Cache.Count > count + 1 && TotalCacheSize > Config.CacheMaxSize)
                TotalCacheSize -= CQuadWord(Cache[count++]->CachedListingLen, 0);
            if (count > 0)
            {
                Cache.Delete(0, count);
                if (!Cache.IsGood())
                    Cache.ResetState();
            }
        }
        else
            Cache.ResetState();
    }
    if (item != NULL)
        delete item;

    HANDLES(LeaveCriticalSection(&CacheCritSect));
}

void CListingCache::RefreshOnPath(const char* host, unsigned short port, const char* user,
                                  CFTPServerPathType pathType, const char* path, BOOL ignorePath)
{
    HANDLES(EnterCriticalSection(&CacheCritSect));

    if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
        user = NULL;
    int delIndex = 0; // promenne pro mazani po blocich (sesuv pole ma slozitost O(N*N), optimalizujeme)
    int delCount = 0;
    int i;
    for (i = 0; i < Cache.Count; i++)
    {
        CListingCacheItem* item = Cache[i];
        if (SalamanderGeneral->StrICmp(host, item->Host) == 0 &&
            (user == NULL && item->User == NULL ||
             item->User != NULL && user != NULL && strcmp(user, item->User) == 0) &&
            port == item->Port &&
            (ignorePath || FTPIsPrefixOfServerPath(pathType, path, item->Path, FALSE))) // bereme cestu vcetne jejich podcest
        {
            // smazneme polozku z cache
            TotalCacheSize -= CQuadWord(item->CachedListingLen, 0);
            if (delIndex + delCount == i)
                delCount++; // navazuje na mazany blok, jen ho rozsirime
            else            // musime vytvorit novy blok a stary smazat
            {
                if (delCount > 0)
                {
                    Cache.Delete(delIndex, delCount);
                    if (!Cache.IsGood())
                        Cache.ResetState();
                    i -= delCount; // uprava indexu na zaklade vymazu stareho bloku (musi lezet cely pred 'i')
                }
                delIndex = i;
                delCount = 1;
            }
        }
    }
    if (delCount > 0)
    {
        Cache.Delete(delIndex, delCount);
        if (!Cache.IsGood())
            Cache.ResetState();
    }

    HANDLES(LeaveCriticalSection(&CacheCritSect));
}

void CListingCache::AcceptChangeOnPathNotification(const char* userPart, BOOL includingSubdirs)
{
    char buf[FTP_USERPART_SIZE];
    const char* pathPart = NULL;
    char *user, *host, *portStr, *pathStr;
    int port;
    int userLength = -1;

    HANDLES(EnterCriticalSection(&CacheCritSect));

    int delIndex = 0; // promenne pro mazani po blocich (sesuv pole ma slozitost O(N*N), optimalizujeme)
    int delCount = 0;
    int i;
    for (i = 0; i < Cache.Count; i++)
    {
        CListingCacheItem* item = Cache[i];
        if (userLength == -1 || userLength != item->UserLength)
        {
            userLength = item->UserLength;
            lstrcpyn(buf, userPart, FTP_USERPART_SIZE);
            FTPSplitPath(buf, &user, NULL, &host, &portStr, &pathStr, NULL, userLength);
            if (pathStr != NULL && pathStr > buf)
                pathPart = userPart + (pathStr - buf) - 1;
            port = portStr != NULL ? atoi(portStr) : IPPORT_FTP;
            if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
                user = NULL;
            if (host == NULL || pathPart == NULL)
            { // tohle jeste muze byt jen shoda nahod, musime to zkusit pro neznamou delku username
                lstrcpyn(buf, userPart, FTP_USERPART_SIZE);
                FTPSplitPath(buf, &user, NULL, &host, &portStr, &pathStr, NULL, 0);
                if (pathStr != NULL && pathStr > buf)
                    pathPart = userPart + (pathStr - buf) - 1;
                port = portStr != NULL ? atoi(portStr) : IPPORT_FTP;
                if (user != NULL && strcmp(user, FTP_ANONYMOUS) == 0)
                    user = NULL;
                if (host == NULL || pathPart == NULL)
                {
                    TRACE_E("CListingCache::AcceptChangeOnPathNotification(): invalid (or relative) path received: " << userPart);
                    HANDLES(LeaveCriticalSection(&CacheCritSect));
                    return; // takove polozky v cache nejsou, neni co delat
                }
            }
        }
        if (SalamanderGeneral->StrICmp(host, item->Host) == 0 &&
            (user == NULL && item->User == NULL ||
             item->User != NULL && user != NULL && strcmp(user, item->User) == 0) &&
            port == item->Port &&
            FTPIsPrefixOfServerPath(item->PathType, FTPGetLocalPath(pathPart, item->PathType),
                                    item->Path, !includingSubdirs))
        { // polozka odpovida zmenene ceste nebo jejimu podadresari, smazneme ji z cache
            TotalCacheSize -= CQuadWord(item->CachedListingLen, 0);
            if (delIndex + delCount == i)
                delCount++; // navazuje na mazany blok, jen ho rozsirime
            else            // musime vytvorit novy blok a stary smazat
            {
                if (delCount > 0)
                {
                    Cache.Delete(delIndex, delCount);
                    if (!Cache.IsGood())
                        Cache.ResetState();
                    i -= delCount; // uprava indexu na zaklade vymazu stareho bloku (musi lezet cely pred 'i')
                }
                delIndex = i;
                delCount = 1;
            }
        }
    }
    if (delCount > 0)
    {
        Cache.Delete(delIndex, delCount);
        if (!Cache.IsGood())
            Cache.ResetState();
    }

    HANDLES(LeaveCriticalSection(&CacheCritSect));
}
