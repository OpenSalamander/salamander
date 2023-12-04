// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CControlConnectionSocket
//

enum CSendFTPCmdStates // stavy automatu pro CControlConnectionSocket::SendFTPCommand
{
    // poslani FTP commandu
    sfcsSendCommand,

    // abort FTP commandu (poslani prikazu "ABOR")
    sfcsAbortCommand,

    // nove poslani abortu FTP commandu bez OOB dat (poslani prikazu "ABOR")
    sfcsResendAbortCommand,

    // fatalni chyba (res-id textu je v 'fatalErrorTextID' + je-li 'fatalErrorTextID' -1, je
    // string rovnou v 'errBuf')
    sfcsFatalError,

    // fatalni chyba operace (res-id textu je v 'opFatalErrorTextID' a cislo Windows chyby v
    // 'opFatalError' + je-li 'opFatalError' -1, je string rovnou v 'errBuf')
    sfcsOperationFatalError,

    // konec metody (uspesny i neuspesny - podle 'ret' TRUE/FALSE)
    sfcsDone
};

// **************************************************************************************
// pomocny objekt CSendCmdUserIfaceWaitWnd pro CControlConnectionSocket::SendFTPCommand()

class CSendCmdUserIfaceWaitWnd : public CSendCmdUserIfaceAbstract
{
protected:
    CWaitWindow WaitWnd;

public:
    CSendCmdUserIfaceWaitWnd(HWND parent) : WaitWnd(parent, TRUE) {}

    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText);
    virtual void BeforeAborting() { WaitWnd.SetText(LoadStr(IDS_ABORTINGCOMMAND)); }
    virtual void AfterWrite(BOOL aborting, DWORD showTime) { WaitWnd.Create(showTime); }
    virtual BOOL GetWindowClosePressed() { return WaitWnd.GetWindowClosePressed(); }
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort);
    virtual void SendingFinished() { WaitWnd.Destroy(); }
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize) { return TRUE; }
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize) {}
    virtual void CancelDataCon() {}

    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout) { return TRUE; }
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout) {}
    virtual void HandleDataConTimeout(DWORD* start) {}
    virtual HANDLE GetFinishedEvent() { return NULL; }
    virtual void HandleESCWhenWaitingForFinish(HWND parent) {}
};

void CSendCmdUserIfaceWaitWnd::Init(HWND parent, const char* logCmd, const char* waitWndText)
{
    char buf[500];
    char errBuf[300];
    if (waitWndText == NULL) // std. text wait okenka
    {
        lstrcpyn(errBuf, logCmd, 300); // orizneme CRLF z commandu
        char* s = errBuf + strlen(errBuf);
        while (s > errBuf && (*(s - 1) == '\r' || *(s - 1) == '\n'))
            s--;
        *s = 0;
        _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_SENDINGCOMMAND), errBuf);
        WaitWnd.SetText(buf);
    }
    else
        WaitWnd.SetText(waitWndText);
}

BOOL CSendCmdUserIfaceWaitWnd::HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort)
{
    WaitWnd.Show(FALSE);
    BOOL esc = SalamanderGeneral->SalMessageBox(parent,
                                                LoadStr(isSend ? (allowCmdAbort ? IDS_SENDCOMMANDESC : IDS_SENDCOMMANDESC2) : IDS_ABORTCOMMANDESC),
                                                LoadStr(IDS_FTPPLUGINTITLE),
                                                MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES;
    if (!esc)
    {
        SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu
        WaitWnd.Show(TRUE);
    }
    return esc;
}

// *********************************************************************************

void WriteUnexpReplyToLog(int logUID, char* unexpReply, int unexpReplyBufSize)
{ // pomocna funkce - zapis "unexpected reply: %s" do logu
    char* s = LoadStr(IDS_LOGMSGUNEXPREPLY);
    int ul = (int)strlen(s);
    int l = (int)strlen(unexpReply);
    if (l + ul + 1 > unexpReplyBufSize)
        l = unexpReplyBufSize - ul - 1;
    memmove(unexpReply + ul, unexpReply, l);
    memcpy(unexpReply, s, ul);
    unexpReply[ul + l] = 0;
    if (l >= 2)
        memcpy(unexpReply + ul + l - 2, "\r\n", 2);
    Logs.LogMessage(logUID, unexpReply, -1);
}

// *********************************************************************************

BOOL CControlConnectionSocket::SendFTPCommand(HWND parent, const char* ftpCmd, const char* logCmd,
                                              const char* waitWndText, int waitWndTime, BOOL* cmdAborted,
                                              int* ftpReplyCode, char* ftpReplyBuf, int ftpReplyBufSize,
                                              BOOL allowCmdAbort, BOOL resetWorkingPathCache,
                                              BOOL resetCurrentTransferModeCache, BOOL* canRetry,
                                              char* retryMsg, int retryMsgBufSize,
                                              CSendCmdUserIfaceAbstract* specialUserInterface)
{
    CALL_STACK_MESSAGE8("CControlConnectionSocket::SendFTPCommand(, , %s, , %d, , , , %d, %d, %d, %d, , , %d,)",
                        logCmd, waitWndTime, ftpReplyBufSize, allowCmdAbort, resetWorkingPathCache,
                        resetCurrentTransferModeCache, retryMsgBufSize);

    parent = FindPopupParent(parent);
    DWORD startTime = GetTickCount(); // cas zacatku operace
    if (canRetry != NULL)
        *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    char buf[500];
    char errBuf[300];

    // pokud se ma pouzivat uzivatelske rozhrani CSendCmdUserIfaceWaitWnd, vytvorime ho zde
    CSendCmdUserIfaceAbstract* userIface = specialUserInterface;
    CSendCmdUserIfaceWaitWnd objSendCmdUserIfaceWaitWnd(parent); // nebudeme ho alokovat (zbytecne osetrovani chyb)
    if (userIface == NULL)
        userIface = &objSendCmdUserIfaceWaitWnd;

    userIface->Init(parent, logCmd, waitWndText);

    if (cmdAborted != NULL)
        *cmdAborted = FALSE;
    *ftpReplyCode = -1;
    if (ftpReplyBufSize > 0)
        ftpReplyBuf[0] = 0;

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
    int fatalErrorTextID = 0;
    int opFatalErrorTextID = 0;
    int opFatalError = 0;
    BOOL fatalErrLogMsg = TRUE; // FALSE = nevypise error hlasku do logu (duvod: uz tam je vypsana)
    BOOL aborting = FALSE;
    BOOL cmdReplyReceived = FALSE; // jen pri abortovani: TRUE = odpoved na command uz prisla (muzeme cekat na odpoved na abort)
    BOOL donotRetry = FALSE;       // TRUE = na tuto chybu nema smysl zkouset retry

    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // aspon sekundu

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID; // UID logu teto connectiony
    BOOL auxCanSendOOBData = CanSendOOBData;
    BOOL handleKeepAlive = KeepAliveMode != kamForbidden; // TRUE pokud se keep-alive neresi o uroven vyse (je nutne ho resit zde)
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (handleKeepAlive)
    {
        // pockame na dokonceni keep-alive prikazu (pokud prave probiha) + nastavime
        // keep-alive na 'kamForbidden' (probiha normalni prikaz)
        DWORD waitTime = GetTickCount() - startTime;
        WaitForEndOfKeepAlive(parent, waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);
    }

    CSendFTPCmdStates state = sfcsSendCommand;
    while (state != sfcsDone)
    {
        CALL_STACK_MESSAGE2("state = %d", state); // at je pripadne videt kde to spadlo/vytuhlo
        switch (state)
        {
        case sfcsResendAbortCommand:
        {
            state = sfcsAbortCommand; // dale zpracovavame jako sfcsAbortCommand
                                      // break;  // zde byt nema
        }
        case sfcsAbortCommand:
        {
            if (!PrepareFTPCommand(buf, 500, errBuf, 300, ftpcmdAbort, NULL))
            { // necekana chyba ("always false")
                state = sfcsDone;
                userIface->CancelDataCon();
                break;
            }

            userIface->BeforeAborting();
            aborting = TRUE;
            // break;  // nema tu byt break (dale se zpracovava jako sfcsSendCommand)
        }
        case sfcsSendCommand:
        {
            CSendFTPCmdStates sendState = state;

            DWORD error;
            BOOL allBytesWritten;

            if (aborting && auxCanSendOOBData)
            {
                // upozornime server na abort (viz RFC 959 - posilani ABOR commandu)
                HANDLES(EnterCriticalSection(&SocketCritSect));
                if (Socket != INVALID_SOCKET &&
                    BytesToWriteCount == 0) // "always true" (vsechna data by mela byt poslana)
                {
                    // posleme sekvenci "TELNET IP" (interrupt process)
                    char errTxt[100];
                    int sentLen;
                    if ((sentLen = send(Socket, "\xff\xf4" /* IAC+IP */, 2, 0)) != 2)
                    {                     // temer "always false", chybu posleme do logu, pro ladeni problemu by se to mohlo hodit
                        if (sentLen == 1) // druhy byte se neposlal, pridame ho do 'buf' (to se bude nasledne posilat)
                        {
                            int ll = (int)strlen(buf);
                            if (ll > 498)
                                ll = 498;
                            memmove(buf + 1, buf, ll);
                            buf[0] = '\xf4'; // IP (IAC uz je poslane)
                            buf[ll + 1] = 0;
                        }
                        DWORD err = WSAGetLastError();
                        sprintf(errTxt, "Unable to send TELNET-IP: error = %u (%d)\r\n", err, sentLen);
                        Logs.LogMessage(logUID, errTxt, -1, TRUE);
                    }

                    // posleme TELNET "Synch" signal - socket je neblokujici, takze hrozi jen ze se TELNET
                    // "Synch" neposle, tuto chybu nebudeme osetrovat (je nepravdepodobna a nedulezita)
                    if (sentLen == 2 && // jen pokud se podarilo poslat IAC+IP (jinak nema smysl)
                        (sentLen = send(Socket, "\xF2" /* DM */, 1, MSG_OOB)) != 1)
                    { // temer "always false", chybu posleme do logu, pro ladeni problemu by se to mohlo hodit
                        DWORD err = WSAGetLastError();
                        sprintf(errTxt, "Unable to send TELNET \"Synch\" signal: error = %u (%d)\r\n", err, sentLen);
                        Logs.LogMessage(logUID, errTxt, -1, TRUE);
                    }
                }
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            }

            char unexpReply[700];
            unexpReply[0] = 0;
            int unexpReplyCode = -1;
            if (!aborting) // zkusime skipnout nadbytecne odpovedi serveru (nemely by vubec existovat, ale bohuzel existuji - WarFTPD po listovani adresare, kam user nema pristup vygeneruje dvakrat "550 access denied")
            {
                char* reply;
                int replySize;
                int replyCode;

                HANDLES(EnterCriticalSection(&SocketCritSect));
                while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                {
                    if (unexpReply[0] != 0)
                        WriteUnexpReplyToLog(logUID, unexpReply, 700);

                    CopyStr(unexpReply, 700, reply, replySize);
                    unexpReplyCode = replyCode;

                    SkipFTPReply(replySize);
                }
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            }

            if (Write(!aborting ? ftpCmd : buf, -1, &error, &allBytesWritten))
            {
                if (unexpReply[0] != 0) // zapis se podaril, pripadnou necekanou odpoved jiz nepotrebujeme -> vypiseme ji do logu a zahodime
                {
                    WriteUnexpReplyToLog(logUID, unexpReply, 700);
                    unexpReply[0] = 0;
                }
                Logs.LogMessage(logUID, !aborting ? logCmd : errBuf, -1);

                DWORD start = GetTickCount();
                DWORD waitTime = start - startTime;
                userIface->AfterWrite(aborting, waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);

                BOOL isCanceled = FALSE;
                while (!allBytesWritten || state == sendState)
                {
                    // pockame na udalost na socketu (odpoved serveru) nebo ESC
                    CControlConnectionSocketEvent event;
                    DWORD data1, data2;
                    DWORD now = GetTickCount();
                    if (now - start > (DWORD)serverTimeout)
                        now = start + (DWORD)serverTimeout;
                    WaitForEventOrESC(parent, &event, &data1, &data2, serverTimeout - (now - start),
                                      NULL, userIface, FALSE);
                    switch (event)
                    {
                    case ccsevESC:
                    {
                        if (userIface->HandleESC(parent, state == sfcsSendCommand, allowCmdAbort))
                        {                                                  // cancel
                            if (allowCmdAbort && state == sfcsSendCommand) // cancel pro command -> zacne abortovani commandu
                            {
                                state = sfcsAbortCommand;
                                // allBytesWritten = TRUE;   // musime pockat az se odesle prikaz nez zacneme s posilanim abortu
                                // nebudeme znovu ukazovat wait okenko, odeslani by teoreticky nemelo nikdy vaznout -> neresime (user bude cekat bez wait okna)
                            }
                            else // nelze pouzit abort (musime zavrit connectionu) nebo cancel pro abortovani commandu
                            {
                                state = sfcsDone;
                                isCanceled = TRUE;
                                allBytesWritten = TRUE;                                               // ted uz to neni dulezite, dojde k zavreni socketu
                                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE); // ESC (cancel) do logu
                            }
                        }
                        break;
                    }

                    case ccsevTimeout:
                    {
                        int errorTextID = IDS_SNDORABORCMDTIMEOUT;
                        if (userIface->IsTimeout(&start, serverTimeout, &errorTextID, errBuf, 300))
                        {
                            fatalErrorTextID = errorTextID;
                            state = sfcsFatalError;
                            allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                        }
                        break;
                    }

                    case ccsevWriteDone:
                        allBytesWritten = TRUE; // uz odesly vsechny byty (osetrime take to, ze ccsevWriteDone mohla prepsat ccsevNewBytesRead)
                    case ccsevClosed:           // mozna necekana ztrata pripojeni (osetrime take to, ze ccsevClosed mohla prepsat ccsevNewBytesRead)
                    case ccsevNewBytesRead:     // nacetli jsme nove byty
                    {
                        char* reply;
                        int replySize;
                        int replyCode;

                        HANDLES(EnterCriticalSection(&SocketCritSect));
                        while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                        {
                            Logs.LogMessage(logUID, reply, replySize);

                            if (state != sfcsFatalError && state != sfcsOperationFatalError && // jen pokud uz nemame jinou chybu
                                replyCode == -1)                                               // neni FTP odpoved, koncime
                            {
                                opFatalErrorTextID = IDS_NOTFTPSERVERERROR;
                                allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                CopyStr(errBuf, 300, reply, replySize);
                                opFatalError = -1; // "error" (reply) je primo v errBuf
                                state = sfcsOperationFatalError;
                                fatalErrLogMsg = FALSE; // uz je v logu, nema smysl ji tam davat znovu
                                donotRetry = TRUE;      // retry nema smysl
                                SkipFTPReply(replySize);
                                break;
                            }

                            if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS)
                            { // odpovedi typu FTP_D1_MAYBESUCCESS davame jen do logu (cekame na "posledni slovo" serveru)
                                if (event != ccsevClosed)
                                {
                                    if (!aborting) // send command
                                    {              // muze byt i state==sfcsAbortCommand, to pak musime prikaz abortnout, i kdyz prave uspesne probehl (user porucil abort)
                                        if (state != sfcsAbortCommand)
                                        {
                                            state = sfcsDone;
                                            *ftpReplyCode = replyCode;
                                            CopyStr(ftpReplyBuf, ftpReplyBufSize, reply, replySize);
                                            ret = TRUE; // USPECH, mame odpoved serveru! (u sendu nas zajima jen jedna odpoved serveru)
                                        }
                                        // else; // jdeme pokracovat v cekani na abort prikazu (jeste jsme neposlali ABOR)
                                        SkipFTPReply(replySize);
                                        break;
                                    }
                                    else // abort command
                                    {
                                        if (auxCanSendOOBData &&                      // posilali jsme OOB data
                                            FTP_DIGIT_1(replyCode) == FTP_D1_ERROR && // odpovedi je syntax error
                                            FTP_DIGIT_2(replyCode) == FTP_D2_SYNTAX)
                                        {                                               // nejspis server nerozumi OOB datum a vlozil je primo do datoveho streamu (prikaz "\xF2ABOR" server nezna)
                                            auxCanSendOOBData = CanSendOOBData = FALSE; // v teto "control connection" uz OOB znovu nebudeme zkouset
                                            state = sfcsResendAbortCommand;
                                            SkipFTPReply(replySize);
                                            break;
                                        }
                                        else
                                        {
                                            // tohle je odpoved za poslany nebo abortovany prikaz, jeste zkusime nacist
                                            // dalsi odpovedi serveru (nektere servery posilaji jeste jednu za ABOR)
                                            if (!cmdReplyReceived) // vracime prvni odpoved (mela by byt k prikazu, ale muze byt
                                            {                      // i na ABOR) - pripadnou druhou odpoved (na ABOR) ignorujeme
                                                state = sfcsDone;
                                                *ftpReplyCode = replyCode;
                                                CopyStr(ftpReplyBuf, ftpReplyBufSize, reply, replySize);
                                                if (cmdAborted != NULL)
                                                    *cmdAborted = TRUE;
                                                ret = TRUE; // USPECH, mame odpoved serveru na prikaz/abort
                                                cmdReplyReceived = TRUE;
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    if (state != sfcsFatalError && state != sfcsOperationFatalError &&
                                        (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                         FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)) // napr. 421 Service not available, closing control connection
                                    {                                             // postupne vyhazime vsechny odpovedi serveru, vypiseme prvni nalezenou chybu
                                        CopyStr(errBuf, 300, reply, replySize);
                                        fatalErrorTextID = -1; // text chyby je v 'errBuf'
                                        state = sfcsFatalError;
                                        allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                        fatalErrLogMsg = FALSE; // uz je v logu, nema smysl ji tam davat znovu
                                    }
                                }
                            }
                            else
                            {
                                userIface->MaybeSuccessReplyReceived(reply, replySize); // pasivni rezim: zkusime zasifrovat data-connectionu (jen pokud ji prikaz pouziva)
                            }
                            SkipFTPReply(replySize);
                        }
                        HANDLES(LeaveCriticalSection(&SocketCritSect));

                        if (event == ccsevClosed)
                        {
                            allBytesWritten = TRUE; // ted uz to neni dulezite, doslo k zavreni socketu
                            if (state == sfcsSendCommand || state == sfcsAbortCommand || state == sfcsResendAbortCommand)
                            { // close bez priciny (at uz behem/po sendu nebo pred/behem/po abortu)
                                fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                                state = sfcsFatalError;
                            }
                            if (data1 != NO_ERROR)
                            {
                                FTPGetErrorTextForLog(data1, buf, 500);
                                Logs.LogMessage(logUID, buf, -1);
                            }
                        }
                        break;
                    }

                    default:
                        TRACE_E("Unexpected event = " << event);
                        break;
                    }
                }

                if (!isCanceled && !aborting && state == sfcsDone)
                { // server hlasi "hotovo", budeme jeste cekat na zavreni user-ifacu ("data connection")
                    BOOL calledBeforeWaitingForFinish = FALSE;
                    BOOL useTimeout = FALSE;    // TRUE = pouzit timeout 'serverTimeout2' cekani na dokonceni data-connectiony
                    int serverTimeout2 = 10000; // timeout pro dokonceni data-connectiony v pripadech, ze LIST vraci error nebo ze jeste nedoslo k otevreni spojeni je 10 sekund
                    DWORD start2 = GetTickCount();
                    while (!userIface->CanFinishSending(*ftpReplyCode, &useTimeout))
                    {
                        if (!calledBeforeWaitingForFinish) // zavolame jen poprve
                        {
                            DWORD waitTime2 = GetTickCount() - startTime;
                            userIface->BeforeWaitingForFinish(*ftpReplyCode, &useTimeout);
                            calledBeforeWaitingForFinish = TRUE;
                        }

                        // pockame na zavreni user-ifacu, timeout nebo ESC
                        CControlConnectionSocketEvent event;
                        DWORD data1, data2;
                        DWORD now = GetTickCount();
                        if (now - start2 > (DWORD)serverTimeout2)
                            now = start2 + (DWORD)serverTimeout2;
                        WaitForEventOrESC(parent, &event, &data1, &data2,
                                          useTimeout ? serverTimeout2 - (now - start2) : INFINITE,
                                          NULL, userIface, TRUE);
                        switch (event)
                        {
                        case ccsevUserIfaceFinished:
                            break; // CanFinishSending() uz snad vrati TRUE

                        case ccsevTimeout:
                        {
                            if (useTimeout)
                                userIface->HandleDataConTimeout(&start2);
                            else
                                TRACE_E("Unexpected event ccsevTimeout!");
                            break;
                        }

                        case ccsevESC:
                        {
                            userIface->HandleESCWhenWaitingForFinish(parent);
                            break;
                        }

                        default:
                            TRACE_E("Unexpected event (waiting for closing of user-iface) = " << event);
                            break;
                        }
                    }

                    // zjistime jestli behem docitani data-connectiony nedoslo k zavreni control-connectiony
                    if (!IsConnected())
                    {
                        HANDLES(EnterCriticalSection(&EventCritSect));
                        DWORD error2 = NO_ERROR;
                        int i;
                        for (i = 0; i < EventsUsedCount; i++) // test pritomnosti udalosti ccsevClosed
                        {
                            if (Events[i]->Event == ccsevClosed)
                            {
                                error2 = Events[i]->Data1;
                                break;
                            }
                        }
                        HANDLES(LeaveCriticalSection(&EventCritSect));

                        fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                        state = sfcsFatalError;
                        if (error2 != NO_ERROR)
                        {
                            FTPGetErrorTextForLog(error2, buf, 500);
                            Logs.LogMessage(logUID, buf, -1);
                        }
                        ret = FALSE; // control-connectiona je zavrena
                    }
                }
                else
                {
                    if (state != sfcsResendAbortCommand && state != sfcsAbortCommand)
                        userIface->CancelDataCon();
                }

                userIface->SendingFinished();
            }
            else // chyba Write (low memory, disconnected, chyba neblokujiciho "send")
            {
                if (aborting)
                    userIface->CancelDataCon();
                while (state == sendState)
                {
                    // vybereme udalost na socketu
                    CControlConnectionSocketEvent event;
                    DWORD data1, data2;
                    WaitForEventOrESC(parent, &event, &data1, &data2, 0, NULL, NULL, FALSE); // necekame, jen prijimame udalosti
                    switch (event)
                    {
                    // case ccsevESC:   // (uzivatel nemuze stihnout ESC behem 0 ms timeoutu)
                    case ccsevTimeout: // zadna zprava neceka -> zobrazime primo error z metody Write
                    {
                        opFatalErrorTextID = !aborting ? IDS_SENDCOMMANDERROR : IDS_ABORTCOMMANDERROR;
                        opFatalError = error;
                        state = sfcsOperationFatalError;
                        break;
                    }

                    case ccsevClosed:       // slo o necekanou ztratu pripojeni (osetrime take to, ze ccsevClosed mohla prepsat ccsevNewBytesRead)
                    case ccsevNewBytesRead: // nacetli jsme nove byty (mozna popis chyby vedouci k disconnectu)
                    {
                        char* reply;
                        int replySize;
                        int replyCode;

                        BOOL done = FALSE;
                        if (unexpReply[0] != 0)
                        {
                            Logs.LogMessage(logUID, unexpReply, -1);

                            if (unexpReplyCode == -1 ||                                 // neni FTP odpoved
                                FTP_DIGIT_1(unexpReplyCode) == FTP_D1_TRANSIENTERROR || // popis docasne chyby
                                FTP_DIGIT_1(unexpReplyCode) == FTP_D1_ERROR)            // popis chyby
                            {
                                opFatalErrorTextID = !aborting ? IDS_SENDCOMMANDERROR : IDS_ABORTCOMMANDERROR;
                                lstrcpyn(errBuf, unexpReply, 300);
                                opFatalError = -1;      // "error" (reply) je primo v errBuf
                                fatalErrLogMsg = FALSE; // chyba uz v logu je, nepridavat znovu
                                state = sfcsOperationFatalError;
                                done = TRUE; // dalsi hlasku uz neni nutne cist
                            }
                        }

                        if (!done)
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                            {
                                Logs.LogMessage(logUID, reply, replySize);

                                if (replyCode == -1 ||                                 // neni FTP odpoved
                                    FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // popis docasne chyby
                                    FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // popis chyby
                                {
                                    opFatalErrorTextID = !aborting ? IDS_SENDCOMMANDERROR : IDS_ABORTCOMMANDERROR;
                                    CopyStr(errBuf, 300, reply, replySize);
                                    SkipFTPReply(replySize);
                                    opFatalError = -1;      // "error" (reply) je primo v errBuf
                                    fatalErrLogMsg = FALSE; // chyba uz v logu je, nepridavat znovu
                                    state = sfcsOperationFatalError;
                                    break; // dalsi hlasku uz neni nutne cist
                                }
                                SkipFTPReply(replySize);
                            }
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }

                        if (event == ccsevClosed)
                        {
                            if (state == sendState) // close bez priciny
                            {
                                fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                                state = sfcsFatalError;
                            }
                            if (data1 != NO_ERROR)
                            {
                                FTPGetErrorTextForLog(data1, buf, 500);
                                Logs.LogMessage(logUID, buf, -1);
                            }
                        }
                        break;
                    }
                    }
                }
            }
            break;
        }

        case sfcsFatalError: // fatalni chyba (res-id textu je v 'fatalErrorTextID' + je-li 'fatalErrorTextID' -1, je string rovnou v 'errBuf')
        {
            lstrcpyn(buf, GetFatalErrorTxt(fatalErrorTextID, errBuf), 500);
            char* s = buf + strlen(buf);
            while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                s--;
            if (fatalErrLogMsg)
            {
                strcpy(s, "\r\n");                      // CRLF na konec textu posledni chyby
                Logs.LogMessage(logUID, buf, -1, TRUE); // text posledni chyby pridame do logu
            }
            fatalErrLogMsg = TRUE;
            if (canRetry == NULL || donotRetry) // "retry" neni mozne nebo nema smysl
            {
                *s = 0;
                SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                *canRetry = TRUE;
                lstrcpyn(retryMsg, buf, retryMsgBufSize); // "retry"
            }
            state = sfcsDone;
            break;
        }

        case sfcsOperationFatalError: // fatalni chyba operace (res-id textu je v 'opFatalErrorTextID' a
        {                             // cislo Windows chyby v 'opFatalError' + je-li 'opFatalError' -1,
                                      // je string rovnou v 'errBuf'
            const char* e = GetOperationFatalErrorTxt(opFatalError, errBuf);
            if (fatalErrLogMsg)
            {
                lstrcpyn(buf, e, 500);
                char* s = buf + strlen(buf);
                while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                strcpy(s, "\r\n");                      // CRLF na konec textu posledni chyby
                Logs.LogMessage(logUID, buf, -1, TRUE); // text posledni chyby pridame do logu
            }
            fatalErrLogMsg = TRUE;

            if (canRetry == NULL || donotRetry) // "retry" neni mozne nebo nema smysl
            {
                sprintf(buf, LoadStr(opFatalErrorTextID), e);
                SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                *canRetry = TRUE;
                lstrcpyn(retryMsg, e, retryMsgBufSize); // "retry"
            }
            state = sfcsDone;
            break;
        }

        default: // (always false)
        {
            TRACE_E("Unexpected situation in CControlConnectionSocket::SendFTPCommand(): state = " << state);
            state = sfcsDone;
            break;
        }
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

    if (resetWorkingPathCache)
        ResetWorkingPathCache(); // pokud hrozi zmena pracovni cesty, resetneme cache
    if (resetCurrentTransferModeCache)
        ResetCurrentTransferModeCache(); // pokud hrozi zmena prenosoveho rezimu, resetneme cache

    if (ret) // spojeni je OK, timeout nenastal
    {
        if (handleKeepAlive)
        {
            // pokud je vse OK, nastavime timer pro keep-alive
            SetupKeepAliveTimer();
        }
    }
    else // spojeni preruseno nebo timeout (socket je dale nepouzitelny)
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

BOOL CControlConnectionSocket::GetCurrentWorkingPath(HWND parent, char* path, int pathBufSize,
                                                     BOOL forceRefresh, BOOL* canRetry,
                                                     char* retryMsg, int retryMsgBufSize)
{
    CALL_STACK_MESSAGE4("CControlConnectionSocket::GetCurrentWorkingPath(, , %d, %d, , , %d)",
                        pathBufSize, forceRefresh, retryMsgBufSize);

    if (canRetry != NULL)
        *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL leaveSect = TRUE;

    if (!HaveWorkingPath || forceRefresh)
    {
        char cmdBuf[50];
        char logBuf[50];
        char replyBuf[700];
        char errBuf[900];

        HANDLES(LeaveCriticalSection(&SocketCritSect));
        leaveSect = FALSE;

        // zjistime pracovni adresar na serveru
        PrepareFTPCommand(cmdBuf, 50, logBuf, 50, ftpcmdPrintWorkingPath, NULL); // nemuze selhat
        int ftpReplyCode;
        if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                           &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, canRetry,
                           retryMsg, retryMsgBufSize, NULL))
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS && // vraci se uspech (melo by byt 257)
                    FTPGetDirectoryFromReply(replyBuf, (int)strlen(replyBuf), WorkingPath, FTP_MAX_PATH) ||
                FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS) // vraci neuspech (napr. "not defined; use CWD to set the working directory") -> prozatimne pouzijeme prazdnou cestu
            {
                if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS)
                    WorkingPath[0] = 0; // prozatimne pouzijeme prazdnou cestu
                leaveSect = TRUE;
                HaveWorkingPath = TRUE; // mame pracovni adresar
            }
            else // fatalni chyba, nelze zjistit pracovni adresar, zavreme connectionu a vratime chybu
            {
                int logUID = LogUID; // UID logu teto connectiony
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                CloseSocket(NULL); // zavreme socket (je-li otevreny), system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGFATALERROR), -1, TRUE);

                ReleaseKeepAlive(); // pri chybe uvolnime keep-alive (nelze pouzivat bez navazaneho spojeni)

                sprintf(errBuf, LoadStr(IDS_GETCURWORKPATHERROR), replyBuf);
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
        }
        // else; // chyba -> zavrena connectiona
    }

    if (leaveSect) // zaroven HaveWorkingPath==TRUE
    {
        lstrcpyn(path, WorkingPath, pathBufSize);
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    }
    return leaveSect;
}

void CControlConnectionSocket::ResetWorkingPathCache()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::ResetWorkingPathCache()");
    HANDLES(EnterCriticalSection(&SocketCritSect));
    HaveWorkingPath = FALSE;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CControlConnectionSocket::ResetCurrentTransferModeCache()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::ResetCurrentTransferModeCache()");
    HANDLES(EnterCriticalSection(&SocketCritSect));
    CurrentTransferMode = ctrmUnknown;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CControlConnectionSocket::SendChangeWorkingPath(BOOL notInPanel, BOOL leftPanel, HWND parent,
                                                     const char* path, char* userBuf, int userBufSize,
                                                     BOOL* success, char* ftpReplyBuf, int ftpReplyBufSize,
                                                     const char* startPath, int* totalAttemptNum,
                                                     const char* retryMsg, BOOL skipFirstReconnectIfNeeded,
                                                     BOOL* userRejectsReconnect)
{
    CALL_STACK_MESSAGE9("CControlConnectionSocket::SendChangeWorkingPath(%d, %d, , %s, , %d, , , %d, %s, , %s, %d,)",
                        notInPanel, leftPanel, path, userBufSize, ftpReplyBufSize, startPath,
                        retryMsg, skipFirstReconnectIfNeeded);

    if (ftpReplyBufSize > 0)
        ftpReplyBuf[0] = 0;
    if (userRejectsReconnect != NULL)
        *userRejectsReconnect = FALSE;

    BOOL ret = FALSE;
    *success = FALSE;
    char cmdBuf[50 + FTP_MAX_PATH];
    char logBuf[50 + FTP_MAX_PATH];
    char replyBuf[700];
    char newPath[FTP_MAX_PATH];
    BOOL reconnected = FALSE;
    int attemptNum = 1;
    if (totalAttemptNum != NULL)
        attemptNum = *totalAttemptNum;
    const char* retryMsgAux = retryMsg;
    BOOL canRetry = FALSE;
    char retryMsgBuf[300];

    if (skipFirstReconnectIfNeeded && retryMsg != NULL)
    {
        TRACE_E("CControlConnectionSocket::SendChangeWorkingPath(): Invalid value (TRUE) of 'skipFirstReconnectIfNeeded' ('retryMsg' != NULL)!");
        skipFirstReconnectIfNeeded = FALSE;
    }

    BOOL firstRound = TRUE;
    while (skipFirstReconnectIfNeeded ||
           ReconnectIfNeeded(notInPanel, leftPanel, parent, userBuf, userBufSize, &reconnected, FALSE,
                             &attemptNum, retryMsgAux, firstRound ? userRejectsReconnect : NULL, -1, FALSE))
    {
        firstRound = FALSE;
        skipFirstReconnectIfNeeded = FALSE;
        BOOL run = FALSE;
        int i;
        for (i = 0; i < 2; i++)
        {
            const char* p;

            BOOL needChangeDir = i == 0 && reconnected && startPath != NULL; // po reconnectu zkusime opet nastavit 'startPath'
            if (i == 0 && !reconnected && startPath != NULL)                 // jsme jiz dele pripojeni, zkontrolujeme
            {                                                                // jestli pracovni adresar odpovida 'startPath'
                // vyuzijeme cache, v normalnich pripadech by tam cesta mela byt
                if (GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300))
                {
                    if (strcmp(newPath, startPath) != 0) // nesedi pracovni adresar na serveru - nutna zmena
                        needChangeDir = TRUE;            // (predpoklad: server vraci stale stejny retezec pracovni cesty)
                }
                else
                {
                    if (canRetry) // "retry" je povolen
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    break; // zavrena connectiona, prerusime vnitrni cyklus
                }
            }

            if (needChangeDir)
            { // obnovene spojeni + relativni cesta -> nejdrive zmenime cestu na absolutni, ze ktere vychazime
                p = startPath;
            }
            else
            {
                p = path;
                i = 1; // konec smycky
            }
            if (PrepareFTPCommand(cmdBuf, 50 + FTP_MAX_PATH, logBuf, 50 + FTP_MAX_PATH,
                                  ftpcmdChangeWorkingPath, NULL, p))
            {
                int ftpReplyCode;
                if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                                   &ftpReplyCode, replyBuf, 700, FALSE, TRUE, FALSE, &canRetry,
                                   retryMsgBuf, 300, NULL))
                {
                    if (p == startPath)
                    {
                        if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS)
                        {               // neuspech (neexistuje absolutni cesta, ze ktere vychazime)
                            ret = TRUE; // zmena probehla (s chybou)
                            lstrcpyn(ftpReplyBuf, replyBuf, ftpReplyBufSize);
                            break; // neuspech -> koncime
                        }
                    }
                    else
                    {
                        ret = TRUE; // zmena probehla (uspesne nebo s chybou)
                        lstrcpyn(ftpReplyBuf, replyBuf, ftpReplyBufSize);
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // vraci se uspech (melo by byt 250)
                            *success = TRUE;
                    }
                }
                else
                {
                    if (canRetry) // "retry" je povolen
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    break; // zavrena connectiona, prerusime vnitrni cyklus
                }
            }
            else // neocekavana chyba ("always false") -> zavreme connectionu
            {
                TRACE_E("Unexpected situation in CControlConnectionSocket::SendChangeWorkingPath() - small buffer for command!");

                HANDLES(EnterCriticalSection(&SocketCritSect));
                int logUID = LogUID; // UID logu teto connectiony
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                CloseSocket(NULL); // zavreme socket (je-li otevreny), system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGFATALERROR2), -1, TRUE);

                ReleaseKeepAlive(); // pri chybe uvolnime keep-alive (nelze pouzivat bez navazaneho spojeni)

                break;
            }
        }
        if (!run)
            break; // ukonceni cyklu v pripade, ze nenastalo preruseni spojeni, ktere se ma obnovit
    }
    if (totalAttemptNum != NULL)
        *totalAttemptNum = attemptNum;

    return ret;
}

CFTPServerPathType
CControlConnectionSocket::GetFTPServerPathType(const char* path)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::GetFTPServerPathType(%s)", path);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    CFTPServerPathType type = ::GetFTPServerPathType(ServerFirstReply, ServerSystem, path);
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return type;
}

BOOL CControlConnectionSocket::IsServerSystem(const char* systemName)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::IsServerSystem()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    char sysName[201];
    FTPGetServerSystem(ServerSystem, sysName);
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return _stricmp(sysName, systemName) == 0;
}

BOOL CControlConnectionSocket::ChangeWorkingPath(BOOL notInPanel, BOOL leftPanel, HWND parent, char* path,
                                                 int pathBufSize, char* userBuf, int userBufSize,
                                                 BOOL parsedPath, BOOL forceRefresh, int mode,
                                                 BOOL cutDirectory, char* cutFileName, BOOL* pathWasCut,
                                                 char* rescuePath, BOOL showChangeInLog, char** cachedListing,
                                                 int* cachedListingLen, CFTPDate* cachedListingDate,
                                                 DWORD* cachedListingStartTime, int* totalAttemptNum,
                                                 BOOL skipFirstReconnectIfNeeded)
{
    CALL_STACK_MESSAGE13("CControlConnectionSocket::ChangeWorkingPath(%d, %d, , %s, %d, , %d, %d, %d, %d, %d, , , %s, %d, , , , , , %d)",
                         notInPanel, leftPanel, path, pathBufSize, userBufSize, parsedPath,
                         forceRefresh, mode, cutDirectory, rescuePath, showChangeInLog,
                         skipFirstReconnectIfNeeded);

    // prvni faze: provedeme odhad textu cesty
    BOOL ret = TRUE;
    if (pathBufSize <= 0)
    {
        TRACE_E("Unexpected parameter value ('pathBufSize'<=0) in CControlConnectionSocket::ChangeWorkingPath().");
        ret = FALSE;
    }
    CFTPServerPathType pathType = ftpsptEmpty;
    BOOL donotTestPath = FALSE;
    char newPath[FTP_MAX_PATH];
    char prevUsedPath[FTP_MAX_PATH];
    BOOL fileNameAlreadyCut = FALSE;
    if (ret)
    {
        if (path[0] == 0) // jen tesne po pripojeni - jinak se zmena cesty (Shift+F7) na
        {                 // rel. cestu (napr. "ftp://localhost") ignoruje
                          // + pri optimalizaci ChangePath() volane tesne po ziskani pracovni cesty
            // forceRefresh by melo byt jen FALSE + vzdy predchazi volani GetCurrentWorkingPath() -> vzdy
            // by melo brat cestu z cache (nesaha na connectionu)
            ret = GetCurrentWorkingPath(parent, path, pathBufSize, forceRefresh, NULL, NULL, 0);
            if (ret)
            {
                donotTestPath = TRUE; // cestu ziskanou od serveru nema smysl testovat - rovnou ji vylistujeme
                pathType = GetFTPServerPathType(path);
            }
        }
        else
        {
            if (parsedPath &&                    // krome pripojeni z dialogu "Connect to FTP Server" je vzdy TRUE
                (*path == '/' || *path == '\\')) // 'path' ma za zacatku vzdy '/' nebo '\\' ("always true")
            {
                pathType = GetFTPServerPathType(path + 1);
                if (pathType == ftpsptOpenVMS || pathType == ftpsptMVS || pathType == ftpsptIBMz_VM ||
                    pathType == ftpsptOS2 && GetFTPServerPathType("") == ftpsptOS2) // OS/2 cesty se pletou s unixovou cestou "/C:/path", proto rozlisujeme OS/2 cesty i jen podle SYST-reply
                {                                                                   // VMS + MVS + IBM_z/VM + OS/2 nemaji '/' ani '\\' na zacatku cesty
                    memmove(path, path + 1, strlen(path) + 1);                      // vyhodime znak '/' nebo '\\' ze zacatku cesty
                    if (path[0] == 0)                                               // obecny root -> doplnime podle typu systemu
                    {
                        if (pathType == ftpsptOpenVMS)
                            lstrcpyn(path, "[000000]", pathBufSize);
                        else
                        {
                            if (pathType == ftpsptMVS)
                                lstrcpyn(path, "''", pathBufSize);
                            else
                            {
                                if (pathType == ftpsptIBMz_VM)
                                {
                                    if (rescuePath[0] == 0 || !FTPGetIBMz_VMRootPath(path, pathBufSize, rescuePath))
                                    {
                                        lstrcpyn(path, "/", pathBufSize); // testovany server podporoval Unixovy root "/", mozna se nekdo ozve, pak budeme resit dale...
                                    }
                                }
                                else
                                {
                                    if (pathType == ftpsptOS2)
                                    {
                                        if (rescuePath[0] == 0 || !FTPGetOS2RootPath(path, pathBufSize, rescuePath))
                                        {
                                            lstrcpyn(path, "/", pathBufSize); // zkusime aspon Unixovy root "/", nic jineho neumime, mozna se nekdo ozve, pak budeme resit dale...
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (pathType == ftpsptOpenVMS && mode == 3 && !fileNameAlreadyCut &&
                            cutFileName != NULL && !cutDirectory)
                        { // zkusime jestli nejde o jmeno souboru (u VMS je to rozlisene syntaxi)
                            lstrcpyn(prevUsedPath, path, FTP_MAX_PATH);
                            BOOL fileNameCouldBeCut;
                            if (FTPCutDirectory(pathType, prevUsedPath, FTP_MAX_PATH, newPath,
                                                FTP_MAX_PATH, &fileNameCouldBeCut) &&
                                fileNameCouldBeCut) // u VMS pri 'fileNameCouldBeCut'==TRUE vime jiste, ze se jedna o soubor
                            {
                                lstrcpyn(cutFileName, newPath, MAX_PATH);
                                lstrcpyn(path, prevUsedPath, pathBufSize);
                                fileNameAlreadyCut = TRUE;
                                if (pathWasCut != NULL)
                                    *pathWasCut = TRUE;
                            }
                        }
                    }
                }
                else
                    pathType = GetFTPServerPathType(path);
            }
            else
                pathType = GetFTPServerPathType(path);
        }
    }

    char replyBuf[700];
    char errBuf[900 + FTP_MAX_PATH];
    if (ret && showChangeInLog && // pokud se ma ukazat v logu
        !cutDirectory)            // jen pri prvnim pruchodu (zkracovani kvuli vadnemu listingu nehlasime)
    {
        _snprintf_s(errBuf, _TRUNCATE, LoadStr(forceRefresh ? IDS_LOGMSGREFRESHINGPATH : IDS_LOGMSGCHANGINGPATH), path);
        LogMessage(errBuf, -1, TRUE);
    }

    prevUsedPath[0] = 0;
    if (ret && cutDirectory) // zatim OK + ma se zkratit cesta jeste pred pouzitim (pokud cesta nesla vylistovat)
    {
        if (donotTestPath)
            donotTestPath = FALSE;                  // zmena cesty - zmenenou uz testovat musime
        lstrcpyn(prevUsedPath, path, FTP_MAX_PATH); // zapamatujeme si predchozi cestu na serveru (vracenou serverem)
        if (!FTPCutDirectory(pathType, path, pathBufSize, NULL, 0, NULL))
        {
            if (rescuePath[0] != 0) // jeste zkusime zachranou cestu
            {
                lstrcpyn(path, rescuePath, pathBufSize);
                pathType = GetFTPServerPathType(path);
                rescuePath[0] = 0; // priste uz ji zkouset nebudeme (nezacyklime se)
            }
            else // neni treba hlasit zadnou chybu (listovani hlasilo chybu), snazili jsme se v tichosti
            {    // nalezt nejakou pristupnou cestu (coz nevyslo)
                ret = FALSE;
            }
        }
        if (ret) // bud orizla nebo jina cesta, kazdopadne to neni pozadovana cesta
        {
            fileNameAlreadyCut = TRUE;
            if (pathWasCut != NULL)
                *pathWasCut = TRUE;
        }
    }

    // druha faze: najdeme na serveru pozadovanou nebo co nejblize odpovidajici cestu, ktera
    //             je bud cachovana nebo pristupna
    if (ret)
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        char hostTmp[HOST_MAX_SIZE];
        lstrcpyn(hostTmp, Host, HOST_MAX_SIZE);
        unsigned short portTmp = Port;
        char listCmd[FTPCOMMAND_MAX_SIZE + 2];
        lstrcpyn(listCmd, UseLIST_aCommand ? LIST_a_CMD_TEXT : (ListCommand != NULL && *ListCommand != 0 ? ListCommand : LIST_CMD_TEXT),
                 FTPCOMMAND_MAX_SIZE);
        strcat(listCmd, "\r\n");
        BOOL isFTPS = EncryptControlConnection == 1;
        int useListingsCacheAux = UseListingsCache;
        BOOL resuscitateKeepAlive = (IsConnected() && KeepAliveEnabled && KeepAliveMode == kamNone); // pokud uz se keep-alive vypnul (vyprsela doba ozivovani), musime ho znovu nastartovat
        KeepAliveStart = GetTickCount();                                                             // pozor, nestaci jen tak jednoduse, pouziti 'resuscitateKeepAlive' je nutne
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (donotTestPath)
        {
            if (useListingsCacheAux && !forceRefresh && // user chce cache pouzivat a nejde o tvrdy refresh
                ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType, path, pathBufSize,
                                            listCmd, isFTPS, cachedListing, cachedListingLen,
                                            cachedListingDate, cachedListingStartTime) &&
                *cachedListing == NULL)
            {
                ret = FALSE; // listing je v cache, ale neni dost pameti pro jeho alokaci -> fatalni chyba
            }
        }
        else
        {
            errBuf[0] = 0;

            int attemptNum = 1;
            if (totalAttemptNum != NULL)
                attemptNum = *totalAttemptNum;
            const char* retryMsgAux = NULL;
            BOOL canRetry = FALSE;
            char retryMsgBuf[300];
            BOOL firstRound = TRUE;

            while (1)
            {
                BOOL inCache = useListingsCacheAux && !forceRefresh && // user chce cache pouzivat a nejde o tvrdy refresh
                               ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType,
                                                           path, pathBufSize, listCmd, isFTPS,
                                                           cachedListing, cachedListingLen,
                                                           cachedListingDate,
                                                           cachedListingStartTime);
                char pathSearchedInCache[FTP_MAX_PATH];
                if (useListingsCacheAux && !forceRefresh)
                    lstrcpyn(pathSearchedInCache, path, FTP_MAX_PATH);
                else
                    pathSearchedInCache[0] = 0;

                if (inCache && *cachedListing == NULL)
                {
                    ret = FALSE;   // listing je v cache, ale neni dost pameti pro jeho alokaci
                    errBuf[0] = 0; // pripadne hlaseni je zbytecne, vypsana chyba by byla jen matouci
                    break;         // fatalni chyba
                }
                if (!inCache) // listing neni v cache (nebo ho nesmime pouzit)
                {
                    resuscitateKeepAlive = FALSE; // sahne se na connectionu, keep-alive se ozivi automaticky

                    BOOL success;
                    BOOL userRejectsReconnect;

                TRY_CHANGE_AGAIN:

                    if (SendChangeWorkingPath(notInPanel, leftPanel, parent, path, userBuf,
                                              userBufSize, &success, replyBuf, 700, NULL,
                                              &attemptNum, retryMsgAux, skipFirstReconnectIfNeeded,
                                              &userRejectsReconnect))
                    {
                        firstRound = FALSE;
                        skipFirstReconnectIfNeeded = FALSE;
                        retryMsgAux = NULL;
                        if (success) // server vraci uspech - vytahneme novou cestu
                        {
                            if (GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, forceRefresh,
                                                      &canRetry, retryMsgBuf, 300))
                            {
                                if (cutDirectory && strcmp(newPath, prevUsedPath) == 0)
                                { // server si z nas dela blazny (nemeni cestu a vraci ze ano)
                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
                                }
                                else
                                {
                                    lstrcpyn(path, newPath, pathBufSize);       // prevezmeme novou cestu ze serveru
                                    if (useListingsCacheAux && !forceRefresh && // user chce cache pouzivat a nejde o tvrdy refresh
                                        strcmp(path, pathSearchedInCache) != 0) // pokud jsme jeste v cache tuto cestu nehledali, zkusime to
                                    {
                                        pathType = GetFTPServerPathType(path);
                                        if (ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType,
                                                                        path, pathBufSize, listCmd, isFTPS,
                                                                        cachedListing, cachedListingLen,
                                                                        cachedListingDate,
                                                                        cachedListingStartTime) &&
                                            *cachedListing == NULL)
                                        {                  // fatalni chyba
                                            ret = FALSE;   // listing je v cache, ale neni dost pameti pro jeho alokaci
                                            errBuf[0] = 0; // pripadne hlaseni je zbytecne, vypsana chyba by byla jen matouci
                                        }
                                    }
                                    break; // fatal error nebo uspech (cesta zmenena) + pripadne: cesta je cachovana, listing bereme z cache
                                }
                            }
                            else
                            {
                                if (canRetry) // "retry" je povolen
                                {
                                    retryMsgAux = retryMsgBuf;

                                    goto TRY_CHANGE_AGAIN;
                                }

                                ret = FALSE;
                                errBuf[0] = 0; // user dostal uz hlaseni fatalni chyby, dalsi hlaska je zbytecna
                                break;         // fatalni chyba - uz je i zavrena connectiona
                            }
                        }
                        else // chyba, nagenerujeme hlasku
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
                        }

                        if (strcmp(rescuePath, path) == 0)
                            rescuePath[0] = 0; // chyba cesty shodne s 'rescuePath' -> 'rescuePath' uz dale nema vyznam

                        // zkusime zkratit cestu (pri uspesne zmene cesty to sem nedojde)
                        BOOL fileNameCouldBeCut;
                        if (!FTPCutDirectory(pathType, path, pathBufSize, newPath, FTP_MAX_PATH, &fileNameCouldBeCut))
                        {
                            if (rescuePath[0] != 0) // jeste zkusime zachranou cestu
                            {
                                lstrcpyn(path, rescuePath, pathBufSize);
                                fileNameAlreadyCut = TRUE;
                                pathType = GetFTPServerPathType(path);
                                rescuePath[0] = 0; // priste uz ji zkouset nebudeme (nezacyklime se)
                            }
                            else
                            {
                                // ani pokud jsme nenasli zadnou pristupnou cestu, nechceme disconnect, proto provedeme nasledujici:
                                if (GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, TRUE,
                                                          &canRetry, retryMsgBuf, 300))
                                {
                                    if (newPath[0] == 0) // zajima nas jen pripad, kdy na serveru neni zadna aktualni cesta (jinak to sem snad ani nemuze dojit - dojde k pouziti rescuePath)
                                    {
                                        if (pathWasCut != NULL)
                                            *pathWasCut = TRUE; // jsme na jine nez pozadovane ceste
                                        if (pathBufSize > 0)
                                            path[0] = 0; // zadna aktualni cesta na serveru neni
                                        break;           // jdeme zkusit listovani...
                                    }
                                }
                                else
                                {
                                    if (canRetry) // "retry" je povolen
                                    {
                                        retryMsgAux = retryMsgBuf;

                                        goto TRY_CHANGE_AGAIN;
                                    }

                                    ret = FALSE;
                                    errBuf[0] = 0; // user dostal uz hlaseni fatalni chyby, dalsi hlaska je zbytecna
                                    break;         // fatalni chyba - uz je i zavrena connectiona
                                }

                                ret = FALSE; // hlasime posledni chybu (ve vsech typech 'mode')
                                break;       // fatalni chyba (neexistuje zadna pristupna cesta na FS) - spojeni nechame klidne otevrene
                            }
                        }
                        if (fileNameCouldBeCut && !fileNameAlreadyCut && mode == 3) // prvni zkraceni -> muze byt jmeno souboru
                        {
                            errBuf[0] = 0; // v 'mode' 3 se tohle jako chyba nehlasi (snazime se o fokus souboru)
                            if (cutFileName != NULL)
                                lstrcpyn(cutFileName, newPath, MAX_PATH);
                        }
                        else
                        {
                            if (cutFileName != NULL)
                                *cutFileName = 0; // to uz byt jmeno souboru nemuze
                        }
                        fileNameAlreadyCut = TRUE;
                        if (pathWasCut != NULL)
                            *pathWasCut = TRUE;
                        if (mode == 1)
                            errBuf[0] = 0; // v 'mode' 1 se hlasi jen chyby rootu
                    }
                    else
                    {
                        // pokud jde o tvrdy refresh a user odmitl reconnect a cesta ma cachovany listing,
                        // pouzijeme cachovany listing (user vi, ze "NO" na reconnect, takze nebude ocekavat
                        // refreshnuty listing)
                        if (firstRound && userRejectsReconnect && useListingsCacheAux && forceRefresh)
                        {
                            inCache = ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType,
                                                                  path, pathBufSize, listCmd, isFTPS,
                                                                  cachedListing, cachedListingLen,
                                                                  cachedListingDate,
                                                                  cachedListingStartTime);
                            if (inCache && *cachedListing == NULL)
                            {
                                ret = FALSE;   // listing je v cache, ale neni dost pameti pro jeho alokaci
                                errBuf[0] = 0; // pripadne hlaseni je zbytecne, vypsana chyba by byla jen matouci
                                break;         // fatalni chyba
                            }
                            if (inCache)
                                break; // cesta je cachovana - nebudeme zkoumat jestli jeste existuje, listing bereme z cache
                        }

                        ret = FALSE;
                        errBuf[0] = 0; // user dostal uz hlaseni fatalni chyby, dalsi hlaska je zbytecna
                        break;         // fatalni chyba - uz je i zavrena connectiona
                    }
                    skipFirstReconnectIfNeeded = TRUE; // dalsi volani SendChangeWorkingPath() navazuje na predchozi uspesne volani SendChangeWorkingPath()
                }
                else
                    break; // cesta je cachovana - nebudeme zkoumat jestli jeste existuje, listing bereme z cache
            }
            if (totalAttemptNum != NULL)
                *totalAttemptNum = attemptNum;
            if (errBuf[0] != 0) // pokud mame nejake chybove hlaseni, vypiseme ho zde
            {
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
        }
        // ma se ozivit a listing je z cache (jinak se musi ozivit az na prvnim poslanem prikazu)
        if (resuscitateKeepAlive && *cachedListing != NULL)
        {
            WaitForEndOfKeepAlive(parent, 0); // wait-okno se nemuze zobrazit
            SetupKeepAliveTimer(TRUE);
        }
    }

    if (ret && strcmp(rescuePath, path) == 0)
        rescuePath[0] = 0; // zkousime cestu shodnou s 'rescuePath' -> 'rescuePath' uz dale nema vyznam
    return ret;
}

void CControlConnectionSocket::GiveConnectionToWorker(CFTPWorker* newWorker, HWND parent)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GiveConnectionToWorker(,)");

    parent = FindPopupParent(parent);

    if (IsConnected()) // jen je-li spojeni otevrene
    {
        // nejdrive zastavime keep-alive:
        // schovame si fokus z 'parent' (neni-li fokus z 'parent', ulozime NULL)
        HWND focusedWnd = GetFocus();
        HWND hwnd = focusedWnd;
        while (hwnd != NULL && hwnd != parent)
            hwnd = GetParent(hwnd);
        if (hwnd != parent)
            focusedWnd = NULL;
        // disablujeme 'parent', pri enablovani obnovime i fokus
        EnableWindow(parent, FALSE);

        // nahodime cekaci kurzor nad parentem, bohuzel to jinak neumime
        CSetWaitCursorWindow* winParent = new CSetWaitCursorWindow;
        if (winParent != NULL)
            winParent->AttachToWindow(parent);

        // pockame na dokonceni keep-alive prikazu (pokud prave probiha) + nastavime
        // keep-alive na 'kamForbidden' (budou probihat normalni prikazy)
        WaitForEndOfKeepAlive(parent, WAITWND_CONTOOPER);

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

        // po zastaveni keep-alive muzeme provest predani aktivni "control connection" do workera
        // (timer a post-socket-message spojene s keep-alive jsou smazane/dorucene)
        if (IsConnected()) // jen je-li spojeni otevrene i po dokonceni keep-alive prikazu
        {
            // prohodime sockety a interni data objektu tykajici se socketu (read/write buffery, atd.)
            SocketsThread->BeginSocketsSwap(this, newWorker);
            // This check is rather complicated for sanity purposes: pCertificate is always NULL
            if (pCertificate)
                pCertificate->Release();
            pCertificate = newWorker->GetCertificate(); // Keep the certificate
            newWorker->RefreshCopiesOfUIDAndMsg();      // obnovime kopie UID+Msg (doslo k jejich zmene)
            BOOL ok = newWorker->IsConnected();
            if (ok) // paranoid check: mezi IsConnected() a SocketsThread->BeginSocketsSwap() jeste mohlo vypadnout spojeni, to by zadne prohazovani nemelo smysl
            {
                HANDLES(EnterCriticalSection(&newWorker->WorkerCritSect));
                newWorker->SocketClosed = FALSE;      // socket uz neni zavreny, prebirame socket z panelu
                newWorker->ConnectAttemptNumber = 1;  // spojeni je navazane, tedy zde musi byt jednicka
                int workerLogUID = newWorker->LogUID; // UID logu workera
                newWorker->ErrorDescr[0] = 0;         // zacneme sbirat chybova hlaseni
                HANDLES(LeaveCriticalSection(&newWorker->WorkerCritSect));

                HANDLES(EnterCriticalSection(&SocketCritSect));
                HANDLES(EnterCriticalSection(&newWorker->SocketCritSect));

                // predame workerovi informace o connectione a data socketu
                newWorker->ControlConnectionUID = UID;
                if (HaveWorkingPath)
                {
                    newWorker->HaveWorkingPath = TRUE;
                    lstrcpyn(newWorker->WorkingPath, WorkingPath, FTP_MAX_PATH);
                }
                newWorker->CurrentTransferMode = CurrentTransferMode;
                newWorker->ResetBuffersAndEvents();
                newWorker->EventConnectSent = EventConnectSent;
                newWorker->ReadBytes = ReadBytes;
                newWorker->ReadBytesCount = ReadBytesCount;
                newWorker->ReadBytesOffset = ReadBytesOffset;
                newWorker->ReadBytesAllocatedSize = ReadBytesAllocatedSize;
                ReadBytes = NULL;
                ReadBytesCount = 0;
                ReadBytesOffset = 0;
                ReadBytesAllocatedSize = 0;
                int logUID = LogUID; // UID logu teto connectiony

                HANDLES(LeaveCriticalSection(&newWorker->SocketCritSect));
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                ResetBuffersAndEvents(); // vycistime frontu udalosti (mela by obsahovat jen ccsevNewBytesRead)

                // oznamime Logu, ze je "control connection" predana do workera (tudiz i "inactive")
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGCONINWORKER), -1, TRUE);
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.LogMessage(workerLogUID, LoadStr(IDS_LOGMSGWORKERUSECON), -1, TRUE);
                Logs.SetIsConnected(workerLogUID, newWorker->IsConnected());
                Logs.RefreshListOfLogsInLogsDlg();
            }
            else // pokud prohozeni nema smysl, vratime objekty do puvodniho stavu
            {
                SocketsThread->BeginSocketsSwap(this, newWorker);
                newWorker->RefreshCopiesOfUIDAndMsg(); // obnovime kopie UID+Msg (doslo k jejich zmene)
                SocketsThread->EndSocketsSwap();
            }
            SocketsThread->EndSocketsSwap();
        }

        // uvolnime keep-alive, ted uz nebude potreba (uz neni navazane spojeni)
        ReleaseKeepAlive();
    }
}

void CControlConnectionSocket::GetConnectionFromWorker(CFTPWorker* workerWithCon)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GetConnectionFromWorker()");

    if (!IsConnected() && workerWithCon->IsConnected()) // jen neni-li spojeni otevrene a worker ma otevrene spojeni
    {
        // prohodime sockety a interni data objektu tykajici se socketu (read/write buffery, atd.)
        SocketsThread->BeginSocketsSwap(this, workerWithCon);
        workerWithCon->RefreshCopiesOfUIDAndMsg(); // obnovime kopie UID+Msg (doslo k jejich zmene)
        BOOL ok = IsConnected();
        if (ok) // paranoid check: mezi workerWithCon->IsConnected() a SocketsThread->BeginSocketsSwap() jeste mohlo vypadnout spojeni, to by zadne prohazovani nemelo smysl
        {
            HANDLES(EnterCriticalSection(&workerWithCon->WorkerCritSect));
            workerWithCon->SocketClosed = TRUE;       // socket uz neni otevreny, prebirame zavreny socket z panelu
            int workerLogUID = workerWithCon->LogUID; // UID logu workera
            workerWithCon->ErrorDescr[0] = 0;         // predani connectiony neni zadna chyba
            HANDLES(LeaveCriticalSection(&workerWithCon->WorkerCritSect));

            HANDLES(EnterCriticalSection(&SocketCritSect));
            HANDLES(EnterCriticalSection(&workerWithCon->SocketCritSect));

            // prebirame od workera informace o connectione a data socketu
            workerWithCon->ControlConnectionUID = -1;
            if (workerWithCon->HaveWorkingPath)
            {
                HaveWorkingPath = TRUE;
                lstrcpyn(WorkingPath, workerWithCon->WorkingPath, FTP_MAX_PATH);
            }
            CurrentTransferMode = workerWithCon->CurrentTransferMode;
            ResetBuffersAndEvents();
            EventConnectSent = workerWithCon->EventConnectSent;
            if (ReadBytes != NULL)
                free(ReadBytes);
            ReadBytes = workerWithCon->ReadBytes;
            ReadBytesCount = workerWithCon->ReadBytesCount;
            ReadBytesOffset = workerWithCon->ReadBytesOffset;
            ReadBytesAllocatedSize = workerWithCon->ReadBytesAllocatedSize;
            workerWithCon->EventConnectSent = FALSE;
            workerWithCon->ReadBytes = NULL;
            workerWithCon->ReadBytesCount = 0;
            workerWithCon->ReadBytesOffset = 0;
            workerWithCon->ReadBytesAllocatedSize = 0;
            if (ConnectionLostMsg != NULL)
                SalamanderGeneral->Free(ConnectionLostMsg);
            ConnectionLostMsg = NULL;
            int logUID = LogUID; // UID logu teto connectiony

            HANDLES(LeaveCriticalSection(&workerWithCon->SocketCritSect));
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            workerWithCon->ResetBuffersAndEvents(); // vycistime frontu udalosti (mela by obsahovat jen ccsevNewBytesRead)

            // oznamime Logu, ze je "control connection" prevzata od workera (tudiz opet "active")
            Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGCONFROMWORKER), -1, TRUE);
            Logs.SetIsConnected(logUID, IsConnected());
            Logs.LogMessage(workerLogUID, LoadStr(IDS_LOGMSGWORKERRETCON), -1, TRUE);
            Logs.SetIsConnected(workerLogUID, workerWithCon->IsConnected());
            Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection active"
        }
        else // pokud prohozeni nema smysl, vratime objekty do puvodniho stavu
        {
            SocketsThread->BeginSocketsSwap(this, workerWithCon);
            workerWithCon->RefreshCopiesOfUIDAndMsg(); // obnovime kopie UID+Msg (doslo k jejich zmene)
            SocketsThread->EndSocketsSwap();
        }
        SocketsThread->EndSocketsSwap();

        if (ok)
        {
            // rozjedeme znovu keep-alive
            ReleaseKeepAlive();
            WaitForEndOfKeepAlive(SalamanderGeneral->GetMsgBoxParent(), 0); // nemuze otevrit wait-okenko (je ve stavu 'kamNone')
            SetupKeepAliveTimer(TRUE);                                      // nastavime timer pro keep-alive, nechame hned provest keep-alive prikaz (nevime jak dlouho nebyla connectiona v provozu, tak at o ni zbytecne neprijdeme)
        }
    }
}

BOOL CControlConnectionSocket::GetUseListingsCache()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GetConnectionFromWorker()");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = UseListingsCache;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}
