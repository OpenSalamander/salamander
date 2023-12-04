// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

CClosedCtrlConChecker ClosedCtrlConChecker; // resi informovani uzivatele o zavreni "control connection" mimo operace
CListingCache ListingCache;                 // cache listingu cest na serverech (pouziva se pri zmene a listovani cest)

int CLogData::NextLogUID = 0;          // globalni pocitadlo pro objekty logu
int CLogData::OldestDisconnectNum = 0; // disconnect-cislo nejstarsiho logu serveru, ktery se disconnectnul
int CLogData::NextDisconnectNum = 0;   // disconnect-cislo pro dalsi log serveru, ktery se disconnectne

CLogs Logs; // logy vsech pripojeni na FTP servery

CControlConnectionSocket* LeftPanelCtrlCon = NULL;
CControlConnectionSocket* RightPanelCtrlCon = NULL;
CRITICAL_SECTION PanelCtrlConSect; // kriticka sekce pro pristup k LeftPanelCtrlCon a RightPanelCtrlCon

//
// ****************************************************************************
// CControlConnectionSocket
//

CControlConnectionSocket::CControlConnectionSocket() : Events(5, 5)
{
    HANDLES(InitializeCriticalSection(&EventCritSect));
    EventsUsedCount = 0;
    NewEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL)); // auto, nonsignaled
    if (NewEvent == NULL)
        TRACE_E("Unable to create synchronization event object needed for handling of socket events.");
    RewritableEvent = FALSE;

    ProxyServer = NULL;
    Host[0] = 0;
    Port = 0;
    User[0] = 0;
    Password[0] = 0;
    Account[0] = 0;
    UseListingsCache = TRUE;
    InitFTPCommands = NULL;
    UsePassiveMode = TRUE;
    ListCommand = NULL;
    UseLIST_aCommand = FALSE;

    ServerIP = INADDR_NONE;
    CanSendOOBData = TRUE;
    ServerSystem = NULL;
    ServerFirstReply = NULL;
    HaveWorkingPath = FALSE;
    WorkingPath[0] = 0;
    CurrentTransferMode = ctrmUnknown;

    EventConnectSent = FALSE;

    BytesToWrite = NULL;
    BytesToWriteCount = 0;
    BytesToWriteOffset = 0;
    BytesToWriteAllocatedSize = 0;

    ReadBytes = NULL;
    ReadBytesCount = 0;
    ReadBytesOffset = 0;
    ReadBytesAllocatedSize = 0;

    StartTime = 0;

    LogUID = -1;

    ConnectionLostMsg = NULL;

    OurWelcomeMsgDlg = NULL;

    KeepAliveMode = kamNone;
    KeepAliveEnabled = FALSE;
    KeepAliveSendEvery = Config.KeepAliveSendEvery;
    KeepAliveStopAfter = Config.KeepAliveStopAfter;
    KeepAliveCommand = Config.KeepAliveCommand;
    KeepAliveStart = 0;
    KeepAliveCmdAllBytesWritten = TRUE;
    KeepAliveFinishedEvent = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL)); // auto, nonsignaled
    if (KeepAliveFinishedEvent == NULL)
        TRACE_E("Unable to create synchronization event object needed for handling of keep alive commands.");
    KeepAliveDataCon = NULL;
    KeepAliveDataConState = kadcsNone;
    EncryptControlConnection = EncryptDataConnection = 0;
    CompressData = 0;
}

CControlConnectionSocket::~CControlConnectionSocket()
{
    if (KeepAliveFinishedEvent != NULL)
        HANDLES(CloseHandle(KeepAliveFinishedEvent));

    if (ConnectionLostMsg != NULL)
        SalamanderGeneral->Free(ConnectionLostMsg);

    if (InitFTPCommands != NULL)
        SalamanderGeneral->Free(InitFTPCommands);
    if (ListCommand != NULL)
        SalamanderGeneral->Free(ListCommand);

    if (ServerSystem != NULL)
        SalamanderGeneral->Free(ServerSystem);
    if (ServerFirstReply != NULL)
        SalamanderGeneral->Free(ServerFirstReply);

    if (BytesToWrite != NULL)
        free(BytesToWrite);
    if (ReadBytes != NULL)
        free(ReadBytes);

    if (ProxyServer != NULL)
        delete ProxyServer;

    memset(Password, 0, PASSWORD_MAX_SIZE); // mazeme pamet, ve ktere se objevil password
    if (NewEvent != NULL)
        HANDLES(CloseHandle(NewEvent));
    HANDLES(DeleteCriticalSection(&EventCritSect));

    // Logs nemuze sahat na "control connection" (zakazane vnoreni kritickych sekci),
    // toto volani synchronizuje jen platnost ukazatele na "control connection" (ne obsah objektu,
    // proto muze byt az na konci destkruktoru)
    if (LogUID != -1)
        Logs.ClosingConnection(LogUID);
}

DWORD
CControlConnectionSocket::GetTimeFromStart()
{
    DWORD t = GetTickCount();
    return t - StartTime; // funguje i pro t < StartTime (preteceni tick-counteru)
}

DWORD
CControlConnectionSocket::GetWaitTime(DWORD showTime)
{
    DWORD waitTime = GetTimeFromStart();
    if (waitTime < showTime)
        return showTime - waitTime;
    else
        return 0;
}

BOOL CControlConnectionSocket::AddEvent(CControlConnectionSocketEvent event, DWORD data1,
                                        DWORD data2, BOOL rewritable)
{
    CALL_STACK_MESSAGE5("CControlConnectionSocket::AddEvent(%d, %u, %u, %d)",
                        (int)event, data1, data2, rewritable);
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&EventCritSect));
    CControlConnectionSocketEventData* e = NULL;
    if (RewritableEvent && EventsUsedCount > 0)
        e = Events[EventsUsedCount - 1];
    else
    {
        if (EventsUsedCount < Events.Count)
            e = Events[EventsUsedCount++]; // uz mame predalokovane misto pro udalost
        else                               // musime alokovat
        {
            e = new CControlConnectionSocketEventData;
            if (e != NULL)
            {
                Events.Add(e);
                if (Events.IsGood())
                    EventsUsedCount++;
                else
                {
                    Events.ResetState();
                    delete e;
                    e = NULL;
                }
            }
            else
                TRACE_E(LOW_MEMORY);
        }
    }
    if (e != NULL) // mame misto pro udalost, ulozime do nej data
    {
        e->Event = event;
        e->Data1 = data1;
        e->Data2 = data2;
        RewritableEvent = rewritable;
        ret = TRUE;
    }
    HANDLES(LeaveCriticalSection(&EventCritSect));
    if (ret)
        SetEvent(NewEvent);
    return ret;
}

BOOL CControlConnectionSocket::GetEvent(CControlConnectionSocketEvent* event, DWORD* data1, DWORD* data2)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GetEvent(,,)");
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&EventCritSect));
    if (EventsUsedCount > 0)
    {
        CControlConnectionSocketEventData* e = Events[0];
        *event = e->Event;
        *data1 = e->Data1;
        *data2 = e->Data2;
        EventsUsedCount--;
        Events.Detach(0);
        if (!Events.IsGood())
            Events.ResetState();
        Events.Add(e); // objekt udalosti se casem znovu pouzije, dame ho na konec fronty
        if (!Events.IsGood())
        {
            delete e; // neporadilo se ho pridat, smula
            Events.ResetState();
        }
        RewritableEvent = FALSE;
        ret = TRUE;
        if (EventsUsedCount > 0)
            SetEvent(NewEvent); // je tam jeste dalsi
    }
    HANDLES(LeaveCriticalSection(&EventCritSect));
    return ret;
}

void CControlConnectionSocket::WaitForEventOrESC(HWND parent, CControlConnectionSocketEvent* event,
                                                 DWORD* data1, DWORD* data2, int milliseconds,
                                                 CWaitWindow* waitWnd, CSendCmdUserIfaceAbstract* userIface,
                                                 BOOL waitForUserIfaceFinish)
{
    CALL_STACK_MESSAGE3("CControlConnectionSocket::WaitForEventOrESC(, , , , %d, , , %d)",
                        milliseconds, waitForUserIfaceFinish);

    const DWORD cycleTime = 200; // perioda testovani stisku ESC v ms (200 = 5x za sekundu)
    DWORD timeStart = GetTickCount();
    DWORD restOfWaitTime = milliseconds; // zbytek cekaci doby

    HANDLE watchedEvent;
    BOOL watchingUserIface;
    if (waitForUserIfaceFinish && userIface != NULL)
    {
        if ((watchedEvent = userIface->GetFinishedEvent()) == NULL)
        {
            TRACE_E("Unexpected situation in CControlConnectionSocket::WaitForEventOrESC(): userIface->GetFinishedEvent() returned NULL!");
            Sleep(200); // at nevytuhne cela masina...
            *event = ccsevUserIfaceFinished;
            *data1 = 0;
            *data2 = 0;
            return; // ohlasime dokonceni prace v user-ifacu
        }
        watchingUserIface = TRUE;
    }
    else
    {
        watchingUserIface = FALSE;
        watchedEvent = NewEvent;
    }

    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help
    while (1)
    {
        DWORD waitTime;
        if (milliseconds != INFINITE)
            waitTime = min(cycleTime, restOfWaitTime);
        else
            waitTime = cycleTime;
        DWORD waitRes = MsgWaitForMultipleObjects(1, &watchedEvent, FALSE, waitTime, QS_ALLINPUT);

        // nejdrive zkontrolujeme stisk ESC (abychom ho userovi nevyignorovali)
        if (milliseconds != 0 &&                                                          // je-li nulovy timeout, jde jen o pumpovani zprav, ESC neresime
            ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == parent || // stisk ESC
             waitWnd != NULL && waitWnd->GetWindowClosePressed() ||                       // close button ve wait-okenku
             userIface != NULL && userIface->GetWindowClosePressed()))                    // close button v uziv. rozhrani
        {
            // ted nelze udalost precist, nechame to na priste
            if (waitRes == WAIT_OBJECT_0 && !watchingUserIface)
                SetEvent(NewEvent);

            MSG msg; // vyhodime nabufferovany ESC
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;
            *event = ccsevESC;
            *data1 = 0;
            *data2 = 0;
            break; // ohlasime ESC
        }
        if (waitRes == WAIT_OBJECT_0) // zkusime precist novou udalost
        {
            if (!watchingUserIface)
            {
                if (GetEvent(event, data1, data2))
                    break; // ohlasime novou udalost
            }
            else // user-iface hlasi dokonceni prace
            {
                *event = ccsevUserIfaceFinished;
                *data1 = 0;
                *data2 = 0;
                break; // ohlasime dokonceni prace v user-ifacu
            }
        }
        else
        {
            if (waitRes == WAIT_OBJECT_0 + 1) // zpracujeme Windows message
            {
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
            else
            {
                if (waitRes == WAIT_TIMEOUT &&
                    restOfWaitTime == waitTime) // neni to jen timeout cyklu testu klavesy ESC, ale globalni timeout
                {
                    *event = ccsevTimeout; // nezbyva jiz zadny cas
                    *data1 = 0;
                    *data2 = 0;
                    break; // ohlasime timeout
                }
            }
        }
        if (milliseconds != INFINITE) // napocitame znovu zbytek cekaci doby (podle realneho casu)
        {
            DWORD t = GetTickCount() - timeStart; // funguje i pri preteceni tick-counteru
            if (t < (DWORD)milliseconds)
                restOfWaitTime = (DWORD)milliseconds - t;
            else
                restOfWaitTime = 0; // nechame ohlasit timeout (sami nesmime - prioritu ma event pred timeoutem)
        }
    }
}

void CControlConnectionSocket::SetConnectionParameters(const char* host, unsigned short port, const char* user,
                                                       const char* password, BOOL useListingsCache,
                                                       const char* initFTPCommands, BOOL usePassiveMode,
                                                       const char* listCommand, BOOL keepAliveEnabled,
                                                       int keepAliveSendEvery, int keepAliveStopAfter,
                                                       int keepAliveCommand, int proxyServerUID,
                                                       int encryptControlConnection, int encryptDataConnection,
                                                       int compressData)
{
    CALL_STACK_MESSAGE14("CControlConnectionSocket::SetConnectionParameters(%s, %u, %s, %s, %d, %s, %d, %s, %d, %d, %d, %d, %d)",
                         host, (unsigned)port, user, password, useListingsCache, initFTPCommands,
                         usePassiveMode, listCommand, keepAliveEnabled, keepAliveSendEvery,
                         keepAliveStopAfter, keepAliveCommand, proxyServerUID);
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (ProxyServer != NULL)
        delete ProxyServer;
    if (proxyServerUID == -2)
        proxyServerUID = Config.DefaultProxySrvUID;
    if (proxyServerUID == -1)
        ProxyServer = NULL;
    else
    {
        ProxyServer = Config.FTPProxyServerList.MakeCopyOfProxyServer(proxyServerUID, NULL); // nedostatek pameti ignorujeme (automaticky "not used (direct connection)")
        if (ProxyServer != NULL)
        {
            if (ProxyServer->ProxyEncryptedPassword != NULL)
            {
                // rozsifrujeme heslo do ProxyPlainPassword
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                char* plainPassword = NULL;
                if (!passwordManager->DecryptPassword(ProxyServer->ProxyEncryptedPassword, ProxyServer->ProxyEncryptedPasswordSize, &plainPassword))
                {
                    // v tuto chvili ma byt overeno, ze lze heslo rozsifrovat (ve vsech vetvich volajicich SetConnectionParameters()
                    TRACE_E("CControlConnectionSocket::SetConnectionParameters(): internal error, cannot decrypt password!");
                    ProxyServer->SetProxyPassword(NULL);
                }
                else
                {
                    ProxyServer->SetProxyPassword(plainPassword[0] == 0 ? NULL : plainPassword);
                    memset(plainPassword, 0, lstrlen(plainPassword));
                    SalamanderGeneral->Free(plainPassword);
                }
            }
            else
                ProxyServer->SetProxyPassword(NULL);
        }
    }
    lstrcpyn(Host, host, HOST_MAX_SIZE);
    Port = port;
    lstrcpyn(User, user, USER_MAX_SIZE);
    lstrcpyn(Password, password, PASSWORD_MAX_SIZE);
    UseListingsCache = useListingsCache;
    if (InitFTPCommands != NULL)
        SalamanderGeneral->Free(InitFTPCommands);
    InitFTPCommands = SalamanderGeneral->DupStr(initFTPCommands);
    UsePassiveMode = usePassiveMode;
    if (ListCommand != NULL)
        SalamanderGeneral->Free(ListCommand);
    ListCommand = SalamanderGeneral->DupStr(listCommand);
    UseLIST_aCommand = FALSE;
    KeepAliveEnabled = keepAliveEnabled;
    KeepAliveSendEvery = keepAliveSendEvery;
    KeepAliveStopAfter = keepAliveStopAfter;
    KeepAliveCommand = keepAliveCommand;
    EncryptControlConnection = encryptControlConnection;
    EncryptDataConnection = encryptDataConnection;
    CompressData = compressData;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

enum CStartCtrlConStates // stavy automatu pro CControlConnectionSocket::StartControlConnection
{
    // ziskani IP adresy z textove adresy FTP serveru
    sccsGetIP,

    // fatalni chyba (res-id textu je v 'fatalErrorTextID' + je-li 'fatalErrorTextID' -1, je
    // string rovnou v 'errBuf')
    sccsFatalError,

    // fatalni chyba operace (res-id textu je v 'opFatalErrorTextID' a cislo Windows chyby v
    // 'opFatalError' + je-li 'opFatalError' -1, je string rovnou v 'errBuf' + je-li
    // 'opFatalErrorTextID' -1, je string rovnou v 'formatBuf')
    sccsOperationFatalError,

    // pripojeni na FTP server (ziskane IP je v 'auxServerIP')
    sccsConnect,

    // zkusi znovu pripojeni (podle Config.DelayBetweenConRetries + Config.ConnectRetries),
    // pokud jiz nema zkouset, prejde do stavu z 'noRetryState' + je-li 'fastRetry' TRUE, nema
    // pred dalsim pokusem cekat
    sccsRetry,

    // ted pripojeno, precteme zpravu od serveru (ocekavame "220 Service ready for new user")
    sccsServerReady,

    // inicializace posilani login sekvence prikazu - podle proxy server skriptu
    sccsStartLoginScript,

    // postupne posilani login sekvence prikazu - podle proxy server skriptu
    sccsProcessLoginScript,

    // zkusi znovu login (bez cekani a ztraty connectiony) - jen prejde do stavu sccsStartLoginScript - nelze pouzit s proxy serverem!!!
    sccsRetryLogin,

    // konec metody (uspesny i neuspesny - podle 'ret' TRUE/FALSE)
    sccsDone
};

const char* GetFatalErrorTxt(int fatalErrorTextID, char* errBuf)
{
    return fatalErrorTextID == -1 ? errBuf : LoadStr(fatalErrorTextID);
}

const char* GetOperationFatalErrorTxt(int opFatalError, char* errBuf)
{
    char* e;
    if (opFatalError != NO_ERROR)
    {
        if (opFatalError != -1)
            e = FTPGetErrorText(opFatalError, errBuf, 300);
        else
            e = errBuf;
    }
    else
        e = LoadStr(IDS_UNKNOWNERROR);
    return e;
}

BOOL GetToken(char** s, char** next)
{
    char* t = *next;
    *s = t;
    if (*t == 0)
        return FALSE; // konec retezce, uz zadny token
    char* dst = NULL;
    do
    {
        if (*t == ';')
        {
            if (*(t + 1) == ';') // escape sekvence: ";;" -> ";"
            {
                if (dst == NULL)
                    dst = t;
                t++;
            }
            else
            {
                if (dst == NULL)
                    *t++ = 0;
                else
                    t++;
                break;
            }
        }
        if (dst != NULL)
            *dst++ = *t;
        t++;
    } while (*t != 0);
    if (dst != NULL)
        *dst = 0;
    *next = t;
    return TRUE;
}

typedef enum eSSLInit
{
    sslisAUTH,
    sslisPBSZ,
    sslisPROT,
    sslisNone,
} eSSLInit;

HWND FindPopupParent(HWND wnd)
{
    HWND win = wnd;
    for (;;)
    {
        HWND w = (GetWindowLong(win, GWL_STYLE) & WS_CHILD) ? GetParent(win) : NULL;
        if (w == NULL)
            break;
        win = w;
    }
    //  if (win != wnd) TRACE_E("FindPopupParent(): found! (" << win << " for " << wnd << ")");
    return win;
}

BOOL CControlConnectionSocket::StartControlConnection(HWND parent, char* user, int userSize, BOOL reconnect,
                                                      char* workDir, int workDirBufSize, int* totalAttemptNum,
                                                      const char* retryMsg, BOOL canShowWelcomeDlg,
                                                      int reconnectErrResID, BOOL useFastReconnect)
{
    CALL_STACK_MESSAGE8("CControlConnectionSocket::StartControlConnection(, , %d, %d, , %d, , %s, %d, %d, %d)",
                        userSize, reconnect, workDirBufSize, retryMsg, canShowWelcomeDlg, reconnectErrResID,
                        useFastReconnect);

    parent = FindPopupParent(parent);
    if (workDirBufSize > 0)
        workDir[0] = 0;
    if (retryMsg != NULL)
        reconnect = TRUE; // v tomto pripade jde jiste o reconnect

    BOOL ret = FALSE;
    int fatalErrorTextID = 0;
    int opFatalErrorTextID = 0;
    int opFatalError = 0;
    CStartCtrlConStates noRetryState = sccsDone;
    BOOL retryLogError = TRUE;   // FALSE = nevypise error hlasku do logu (duvod: uz tam je vypsana)
    BOOL fatalErrLogMsg = TRUE;  // FALSE = nevypise error hlasku do logu (duvod: uz tam je vypsana)
    BOOL actionCanceled = FALSE; // TRUE = vypsat u 'sccsDone' do logu "action canceled"

    int attemptNum = 1;
    if (totalAttemptNum != NULL)
        attemptNum = *totalAttemptNum; // init na celkovy pocet pokusu
    CDynString welcomeMessage;
    BOOL useWelcomeMessage = canShowWelcomeDlg && !reconnect && Config.ShowWelcomeMessage;
    BOOL retryLoginWithoutAsking = FALSE;
    BOOL fastRetry = FALSE;
    unsigned short port;
    in_addr srvAddr;
    int logUID = -1; // UID logu teto connectiony: zatim "invalid log"

    char proxyScriptText[PROXYSCRIPT_MAX_SIZE];
    char host[HOST_MAX_SIZE];
    char buf[1000];
    char errBuf[300];
    char errBuf2[300];
    char formatBuf[300];
    char retryBuf[700];

    const DWORD showWaitWndTime = WAITWND_STARTCON; // show time wait-okenka
    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // aspon sekundu

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

    OurWelcomeMsgDlg = NULL; // neni treba synchronizovat, pristup jen z hl. threadu

    CWaitWindow waitWnd(parent, TRUE);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    lstrcpyn(user, User, userSize);
    CProxyScriptParams proxyScriptParams(ProxyServer, Host, Port, User, Password, Account, Password[0] == 0 && reconnect);
    CFTPProxyServerType proxyType = fpstNotUsed;
    if (ProxyServer != NULL)
        proxyType = ProxyServer->ProxyType;
    if (proxyType == fpstOwnScript)
        lstrcpyn(proxyScriptText, HandleNULLStr(ProxyServer->ProxyScript), PROXYSCRIPT_MAX_SIZE);
    else
    {
        const char* txt = GetProxyScriptText(proxyType, FALSE);
        if (txt[0] == 0)
            txt = GetProxyScriptText(fpstNotUsed, FALSE); // nedefinovany skript = "not used (direct connection)" skript - SOCKS 4/4A/5, HTTP 1.1
        lstrcpyn(proxyScriptText, txt, PROXYSCRIPT_MAX_SIZE);
    }
    DWORD auxServerIP = ServerIP;
    srvAddr.s_addr = auxServerIP;
    ResetWorkingPathCache();         // po pripojeni na server je nutne zjistit working-dir
    ResetCurrentTransferModeCache(); // po pripojeni na server je nutne nastavit prenosovy rezim (mel by byt ascii, ale neverime tomu)
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    const char* proxyScriptExecPoint = NULL;
    const char* proxyScriptStartExecPoint = NULL; // prvni prikaz skriptu (radek nasledujici za "connect to:")
    int proxyLastCmdReply = -1;
    char proxyLastCmdReplyText[300];
    proxyLastCmdReplyText[0] = 0;
    char proxySendCmdBuf[FTPCOMMAND_MAX_SIZE];
    proxySendCmdBuf[0] = 0;
    eSSLInit SSLInitSequence = EncryptControlConnection ? sslisAUTH : sslisNone;
    char proxyLogCmdBuf[FTPCOMMAND_MAX_SIZE];
    proxyLogCmdBuf[0] = 0;
    char tmpCmdBuf[FTPCOMMAND_MAX_SIZE];
    tmpCmdBuf[0] = 0;
    char connectingToAs[200];
    bool bModeZSent = false;

    if (sslisAUTH == SSLInitSequence)
    {
        strcpy(proxySendCmdBuf, "AUTH TLS\r\n");
        strcpy(proxyLogCmdBuf, proxySendCmdBuf);
    }

    // pripravime na dalsi pouziti keep-alive + nastavime keep-alive na 'kamForbidden' (probiha normalni prikaz)
    ReleaseKeepAlive();
    WaitForEndOfKeepAlive(parent, 0); // nemuze otevrit wait-okenko (je ve stavu 'kamNone')

    CStartCtrlConStates state = (auxServerIP == INADDR_NONE ? sccsGetIP : sccsConnect);

    if (retryMsg != NULL) // simulace stavu, kdy preruseni spojeni nastalo primo v teto metode - "retry" pripojeni
    {
        lstrcpyn(errBuf, retryMsg, 300); // ulozime retry message do errBuf (pro sccsOperationFatalError)
        opFatalErrorTextID = reconnectErrResID != -1 ? reconnectErrResID : IDS_SENDCOMMANDERROR;
        opFatalError = -1;                      // "error" (reply) je primo v errBuf
        noRetryState = sccsOperationFatalError; // pokud se neprovede retry, provede se sccsOperationFatalError
        retryLogError = FALSE;                  // chyba uz v logu je, nepridavat znovu
        state = sccsRetry;
        // useWelcomeMessage = FALSE;  // uz jsme ji vypsali, znovu uz nema smysl  -- "always FALSE"
        fastRetry = useFastReconnect;
    }

    if (ProcessProxyScript(proxyScriptText, &proxyScriptExecPoint, proxyLastCmdReply,
                           &proxyScriptParams, host, &port, NULL, NULL, errBuf2, NULL))
    {
        if (proxyScriptParams.NeedUserInput()) // teoreticky by nemelo nastat
        {                                      // jen proxyScriptParams->NeedProxyHost muze byt TRUE (jinak by ProcessProxyScript vratil chybu)
            strcpy(errBuf, LoadStr(IDS_PROXYSRVADREMPTY));
            lstrcpyn(errBuf, errBuf2, 300);
            opFatalError = -1; // error je primo v errBuf
            opFatalErrorTextID = IDS_ERRINPROXYSCRIPT;
            state = sccsOperationFatalError;
        }
        else
            proxyScriptStartExecPoint = proxyScriptExecPoint;
    }
    else // teoreticky by nikdy nemelo nastat (ulozene skripty jsou validovane)
    {
        lstrcpyn(errBuf, errBuf2, 300);
        opFatalError = -1; // error je primo v errBuf
        opFatalErrorTextID = IDS_ERRINPROXYSCRIPT;
        state = sccsOperationFatalError;
    }

RETRY_LABEL:

    while (state != sccsDone)
    {
        CALL_STACK_MESSAGE2("state = %d", state); // at je pripadne videt kde to spadlo/vytuhlo
        switch (state)
        {
        case sccsGetIP: // ziskani IP adresy z textove adresy FTP serveru
        {
            if (!GetHostByAddress(host, 0)) // musi byt mimo sekci SocketCritSect
            {                               // neni sance na uspech -> ohlasime chybu
                sprintf(formatBuf, LoadStr(IDS_GETIPERROR), host);
                opFatalErrorTextID = -1; // text je v 'formatBuf'
                opFatalError = NO_ERROR; // neznama chyba
                state = sccsOperationFatalError;
            }
            else
            {
                sprintf(buf, LoadStr(IDS_GETTINGIPOFSERVER), host);
                waitWnd.SetText(buf);
                waitWnd.Create(GetWaitTime(showWaitWndTime));

                DWORD start = GetTickCount();
                while (state == sccsGetIP)
                {
                    // pockame na udalost na socketu (prijem zjistene IP adresy) nebo ESC
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
                        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_GETIPESC),
                                                             LoadStr(IDS_FTPPLUGINTITLE),
                                                             MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                 MB_ICONQUESTION) == IDYES)
                        { // cancel
                            state = sccsDone;
                            actionCanceled = TRUE;
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
                        fatalErrorTextID = IDS_GETIPTIMEOUT;
                        state = sccsFatalError;
                        break;
                    }

                    case ccsevIPReceived: // data1 == ip, data2 == error
                    {
                        if (data1 != INADDR_NONE) // mame IP
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            auxServerIP = ServerIP = data1;
                            HANDLES(LeaveCriticalSection(&SocketCritSect));

                            state = sccsConnect;
                        }
                        else // chyba
                        {
                            sprintf(formatBuf, LoadStr(IDS_GETIPERROR), host);
                            opFatalErrorTextID = -1; // text je v 'formatBuf'
                            opFatalError = data2;
                            state = sccsOperationFatalError;
                        }
                        break;
                    }

                    default:
                        TRACE_E("Unexpected event = " << event);
                        break;
                    }
                }
                waitWnd.Destroy();
            }
            break;
        }

        case sccsConnect: // pripojeni na FTP server (ziskane IP je v 'auxServerIP')
        {
            srvAddr.s_addr = auxServerIP;

            SYSTEMTIME st;
            GetLocalTime(&st);
            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, errBuf, 50) == 0)
                sprintf(errBuf, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
            strcat(errBuf, " - ");
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, errBuf + strlen(errBuf), 50) == 0)
                sprintf(errBuf + strlen(errBuf), "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);

            HANDLES(EnterCriticalSection(&SocketCritSect));

            // zalozime log a vlozime do logu header
            if (!reconnect && LogUID == -1) // jeste nemame log
            {
                if (Config.EnableLogging)
                    Logs.CreateLog(&LogUID, Host, Port, User, this, FALSE, FALSE);
                if (ProxyServer != NULL)
                {
                    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_PRXSRVLOGHEADER), Host, Port, User,
                                ProxyServer->ProxyName, GetProxyTypeName(ProxyServer->ProxyType),
                                proxyScriptParams.ProxyHost, proxyScriptParams.ProxyPort,
                                proxyScriptParams.ProxyUser, host, inet_ntoa(srvAddr), port, LogUID, errBuf);
                }
                else
                {
                    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_LOGHEADER), Host, inet_ntoa(srvAddr), Port, LogUID, errBuf);
                }
                if (Config.AlwaysShowLogForActPan &&
                    (!Config.UseConnectionDataFromConfig || !Config.ChangingPathInInactivePanel))
                {
                    Logs.ActivateLog(LogUID);
                }
            }
            else
            {
                if (ProxyServer != NULL)
                {
                    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_PRXSRVRECONLOGHEADER), Host, Port, User,
                                ProxyServer->ProxyName, GetProxyTypeName(ProxyServer->ProxyType),
                                proxyScriptParams.ProxyHost, proxyScriptParams.ProxyPort,
                                proxyScriptParams.ProxyUser, host, inet_ntoa(srvAddr), port, attemptNum, errBuf);
                }
                else
                {
                    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_RECONLOGHEADER), Host, inet_ntoa(srvAddr), Port, attemptNum, errBuf);
                }
            }
            logUID = LogUID;

            HANDLES(LeaveCriticalSection(&SocketCritSect));

            // pridame header - Host + Port + IP + User
            Logs.LogMessage(logUID, buf, -1);

            ResetBuffersAndEvents(); // vyprazdnime buffery (zahodime stara data) a zahodime stare udalosti
            if (useWelcomeMessage)
                welcomeMessage.Clear(); // vycistime predchozi pokus (ma smysl jen po "retry")

            if ((proxyType == fpstSocks5 || proxyType == fpstHTTP1_1) &&
                proxyScriptParams.ProxyUser[0] != 0 && proxyScriptParams.ProxyPassword[0] == 0)
            { // user by mel zadat proxy-password
                _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS2),
                            proxyScriptParams.ProxyHost, proxyScriptParams.ProxyUser);
                if (CEnterStrDlg(parent, LoadStr(IDS_ENTERPRXPASSTITLE), LoadStr(IDS_ENTERPRXPASSTEXT),
                                 proxyScriptParams.ProxyPassword, PASSWORD_MAX_SIZE, TRUE,
                                 connectingToAs, FALSE)
                        .Execute() != IDCANCEL)
                { // zmena hodnot -> musime zmenit i originaly
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    if (ProxyServer != NULL)
                        ProxyServer->SetProxyPassword(proxyScriptParams.ProxyPassword);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                }
            }

            DWORD error;
            BOOL conRes = ConnectWithProxy(auxServerIP, port, proxyType,
                                           &error, proxyScriptParams.Host, proxyScriptParams.Port,
                                           proxyScriptParams.ProxyUser, proxyScriptParams.ProxyPassword,
                                           INADDR_NONE);
            Logs.SetIsConnected(logUID, IsConnected());
            Logs.RefreshListOfLogsInLogsDlg();
            if (conRes)
            {
                if (proxyType == fpstNotUsed)
                    sprintf(buf, LoadStr(IDS_OPENINGCONTOSERVER), host, inet_ntoa(srvAddr), port);
                else
                    sprintf(buf, LoadStr(IDS_OPENINGCONTOSERVER2), proxyScriptParams.Host, proxyScriptParams.Port);
                waitWnd.SetText(buf);
                waitWnd.Create(GetWaitTime(showWaitWndTime));

                DWORD start = GetTickCount();
                while (state == sccsConnect)
                {
                    // pockame na udalost na socketu (otevreni spojeni na server) nebo ESC
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
                        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_OPENCONESC),
                                                             LoadStr(IDS_FTPPLUGINTITLE),
                                                             MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                 MB_ICONQUESTION) == IDYES)
                        { // cancel
                            state = sccsDone;
                            actionCanceled = TRUE;
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
                        if (GetProxyTimeoutDescr(errBuf, 300))
                            fatalErrorTextID = -1; // popis je primo v 'errBuf'
                        else
                            fatalErrorTextID = IDS_OPENCONTIMEOUT;
                        noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                        state = sccsRetry;
                        break;
                    }

                    case ccsevConnected: // data1 == error
                    {
                        if (data1 == NO_ERROR)
                            state = sccsServerReady; // jsme pripojeny
                        else                         // chyba
                        {
                            if (GetProxyError(errBuf, 300, formatBuf, 300, FALSE))
                                opFatalError = -1; // chyba behem pripojovani pres proxy server: text chyby je primo v 'errBuf'+'formatBuf'
                            else
                            {
                                opFatalError = data1;
                                sprintf(formatBuf, LoadStr(IDS_OPENCONERROR), host, inet_ntoa(srvAddr), port);
                            }
                            opFatalErrorTextID = -1;                  // text je v 'formatBuf'
                            noRetryState = sccsOperationFatalError;   // pokud se neprovede retry, provede se sccsOperationFatalError
                            if (opFatalError == -1 && errBuf[0] == 0) // jednoducha chyba -> prevedeme ji na sccsFatalError
                            {
                                fatalErrorTextID = -1;
                                lstrcpyn(errBuf, formatBuf, 300);
                                noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                            }
                            state = sccsRetry;
                        }
                        break;
                    }

                    default:
                        TRACE_E("Unexpected event = " << event);
                        break;
                    }
                }

                waitWnd.Destroy();
            }
            else
            {
                opFatalError = error;
                sprintf(formatBuf, LoadStr(IDS_OPENCONERROR), host, inet_ntoa(srvAddr), port);
                opFatalErrorTextID = -1;                // text je v 'formatBuf'
                noRetryState = sccsOperationFatalError; // pokud se neprovede retry, provede se sccsOperationFatalError
                state = sccsRetry;
            }
            break;
        }

        case sccsRetry: // zkusi znovu pripojeni (podle Config.DelayBetweenConRetries + Config.ConnectRetries),
        {               // pokud jiz nema zkouset, prejde do stavu z 'noRetryState' + je-li 'fastRetry' TRUE,
                        // nema pred dalsim pokusem cekat
            // Resend AUTH TLS if needed
            if (EncryptControlConnection)
            {
                SSLInitSequence = sslisAUTH;
                strcpy(proxySendCmdBuf, "AUTH TLS\r\n");
                strcpy(proxyLogCmdBuf, proxySendCmdBuf);
            }
            bModeZSent = false;

            switch (noRetryState) // text posledni chyby pro log
            {
            case sccsFatalError:
            {
                lstrcpyn(buf, GetFatalErrorTxt(fatalErrorTextID, errBuf), 1000);
                break;
            }

            case sccsOperationFatalError:
            {
                lstrcpyn(buf, GetOperationFatalErrorTxt(opFatalError, errBuf), 1000);
                break;
            }

            default:
            {
                buf[0] = 0;
                TRACE_E("CControlConnectionSocket::StartControlConnection(): Unexpected value "
                        "of 'noRetryState': "
                        << noRetryState);
                break;
            }
            }
            char* s1 = buf + strlen(buf);
            while (s1 > buf && (*(s1 - 1) == '\n' || *(s1 - 1) == '\r'))
                s1--;
            if (retryLogError)
            {
                strcpy(s1, "\r\n");                     // CRLF na konec textu posledni chyby
                Logs.LogMessage(logUID, buf, -1, TRUE); // text posledni chyby pridame do logu
            }
            retryLogError = TRUE;
            *s1 = 0; // oriznuti znaku konce radky z textu posledni chyby

            // pri zavreni control-connection ze strany serveru dojde ke zmene keep-alive na 'kamNone', proto:
            // pripravime na dalsi pouziti keep-alive + nastavime keep-alive na 'kamForbidden' (probiha normalni prikaz)
            ReleaseKeepAlive();
            WaitForEndOfKeepAlive(parent, 0); // nemuze otevrit wait-okenko (je ve stavu 'kamNone')

            if (fastRetry) // mozna je toto "retry" kvuli zpozdeni uzivatele - nebudeme cekat, ma to sanci vyjit (ftp.novell.com - po 20s kill)
            {
                fastRetry = FALSE;

                // je-li potreba, zavreme socket, system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
                CloseSocket(NULL);
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"

                state = sccsConnect; // zkusime se znovu pripojit (IP uz je zname)
            }
            else
            {
                if (attemptNum < Config.GetConnectRetries() + 1) // zkusime se pripojit znovu, nejdrive pockame
                {
                    attemptNum++; // zvysime cislo pokusu o pripojeni
                    if (totalAttemptNum != NULL)
                        *totalAttemptNum = attemptNum; // ulozime celkovy pocet pokusu

                    // je-li potreba, zavreme socket, system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
                    CloseSocket(NULL);
                    Logs.SetIsConnected(logUID, IsConnected());
                    Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"

                    switch (noRetryState) // text posledni chyby pro wait okenko
                    {
                    case sccsFatalError:
                    {
                        if (proxyType == fpstNotUsed)
                        {
                            _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_WAITINGTORETRY), host, inet_ntoa(srvAddr),
                                        port, GetFatalErrorTxt(fatalErrorTextID, errBuf));
                        }
                        else
                        {
                            _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_WAITINGTORETRY2), proxyScriptParams.Host,
                                        proxyScriptParams.Port, GetFatalErrorTxt(fatalErrorTextID, errBuf));
                        }
                        break;
                    }

                    case sccsOperationFatalError:
                    {
                        const char* e = GetOperationFatalErrorTxt(opFatalError, errBuf);
                        char* f;
                        if (opFatalErrorTextID != -1)
                            f = LoadStr(opFatalErrorTextID);
                        else
                            f = formatBuf;
                        _snprintf_s(buf, _TRUNCATE, f, e);

                        char* s = buf;
                        while (*s != 0 && *s != '\n')
                            s++;
                        if (*s == '\n')
                        {
                            if (*(s + 1) == '\n') // zruseni prazdneho radku v textu
                                memmove(s, s + 1, strlen(s + 1) + 1);
                            s++;
                        }
                        s = s + strlen(s);
                        while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                            s--;
                        *s = 0; // orez EOLu na konci zpravy
                        break;
                    }
                    }

                    // ukazeme okenko s hlaskou o cekani
                    int delayBetweenConRetries = Config.GetDelayBetweenConRetries();
                    _snprintf_s(retryBuf, _TRUNCATE, LoadStr(IDS_WAITINGTORETRYSUF), buf, delayBetweenConRetries,
                                attemptNum, Config.GetConnectRetries() + 1);

                    waitWnd.SetText(retryBuf);
                    waitWnd.Create(0);

                    // pockame na ESC nebo timeout cekani
                    CControlConnectionSocketEvent event;
                    DWORD data1, data2;
                    BOOL run = TRUE;
                    DWORD start = GetTickCount();
                    while (run)
                    {
                        DWORD now = GetTickCount();
                        if (now - start < (DWORD)delayBetweenConRetries * 1000)
                        { // sestavime znovu text pro retry wait okenko (obsahuje countdown)
                            DWORD wait = delayBetweenConRetries * 1000 - (now - start);
                            if (now != start) // poprve to nema smysl
                            {
                                _snprintf_s(retryBuf, _TRUNCATE, LoadStr(IDS_WAITINGTORETRYSUF), buf, (1 + (wait - 1) / 1000),
                                            attemptNum, Config.GetConnectRetries() + 1);
                                waitWnd.SetText(retryBuf);
                            }
                            BOOL notTimeout = FALSE; // TRUE = nemuze jit o timeout
                            if (wait > 1500)
                            {
                                wait = 1000; // cekame max. 1.5 sekundy, min. 0.5 sekundy
                                notTimeout = TRUE;
                            }
                            WaitForEventOrESC(parent, &event, &data1, &data2,
                                              wait, &waitWnd, NULL, FALSE);
                            switch (event) // zajima nas jen ESC nebo timeout, udalosti ignorujeme
                            {
                            case ccsevESC:
                            {
                                waitWnd.Show(FALSE);
                                MSGBOXEX_PARAMS params;
                                memset(&params, 0, sizeof(params));
                                params.HParent = parent;
                                params.Flags = MB_YESNOCANCEL | MB_ICONQUESTION;
                                params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                                params.Text = LoadStr(IDS_WAITRETRESC);
                                char aliasBtnNames[300];
                                /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_WAITRETRESCABORTBTN
  {MNTT_IT, IDS_WAITRETRESCRETRYBTN
  {MNTT_IT, IDS_WAITRETRESCWAITBTN
  {MNTT_PE, 0
};
*/
                                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                                        DIALOG_YES, LoadStr(IDS_WAITRETRESCABORTBTN),
                                        DIALOG_NO, LoadStr(IDS_WAITRETRESCRETRYBTN),
                                        DIALOG_CANCEL, LoadStr(IDS_WAITRETRESCWAITBTN));
                                params.AliasBtnNames = aliasBtnNames;
                                int msgRes = SalamanderGeneral->SalMessageBoxEx(&params);
                                if (msgRes == IDYES)
                                { // vzdava dalsi pokusy o login
                                    state = sccsDone;
                                    run = FALSE;
                                    // actionCanceled = TRUE;   // cancel v "retry" nebudeme logovat
                                }
                                else
                                {
                                    if (msgRes == IDNO)
                                    {
                                        event = ccsevTimeout;
                                        run = FALSE; // rovnou zkusime dalsi login
                                    }
                                    else
                                    {
                                        SalamanderGeneral->WaitForESCRelease(); // opatreni, aby se neprerusovala dalsi akce po kazdem ESC v predeslem messageboxu
                                        waitWnd.Show(TRUE);                     // chce pokracovat v cekani
                                    }
                                }
                                break;
                            }

                            case ccsevTimeout:
                                if (!notTimeout)
                                    run = FALSE;
                                break;
                            }
                        }
                        else // uz to zkouset nebudeme (timeout)
                        {
                            event = ccsevTimeout;
                            run = FALSE;
                        }
                    }

                    waitWnd.Destroy();

                    if (event == ccsevTimeout)
                        state = sccsConnect; // zkusime se znovu pripojit (IP uz je zname)
                }
                else
                {
                    if (noRetryState == sccsFatalError || noRetryState == sccsOperationFatalError)
                        fatalErrLogMsg = FALSE; // hlasku jsme uz do logu vypsali (bylo-li potreba), znova se vypisovat nebude
                    state = noRetryState;       // dalsi pokus jiz nebude, pokracujeme na stav z 'noRetryState'
                }
            }
            break;
        }

        case sccsServerReady: // ted pripojeno, precteme zpravu od serveru (ocekavame "220 Service ready for new user")
        {
            waitWnd.SetText(LoadStr(IDS_WAITINGFORLOGIN));
            waitWnd.Create(GetWaitTime(showWaitWndTime));

            DWORD start = GetTickCount();
            while (state == sccsServerReady)
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
                    if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_WAITFORLOGESC),
                                                         LoadStr(IDS_FTPPLUGINTITLE),
                                                         MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                             MB_ICONQUESTION) == IDYES)
                    { // cancel
                        state = sccsDone;
                        actionCanceled = TRUE;
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
                    fatalErrorTextID = IDS_WAITFORLOGTIMEOUT;
                    noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                    state = sccsRetry;
                    break;
                }

                case ccsevClosed:       // mozna necekana ztrata pripojeni (osetrime take to, ze ccsevClosed mohla prepsat ccsevNewBytesRead)
                case ccsevNewBytesRead: // nacetli jsme nove byty
                {
                    char* reply;
                    int replySize;
                    int replyCode;

                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                    {
                        if (useWelcomeMessage)
                            welcomeMessage.Append(reply, replySize);
                        Logs.LogMessage(logUID, reply, replySize);

                        if (replyCode != -1)
                        {
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&
                                FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION) // napr. 220 - Service ready for new user
                            {
                                if (event != ccsevClosed) // pokud uz neni zavrena connectiona
                                {
                                    state = sccsStartLoginScript; // posleme login sekvenci prikazu

                                    CopyStr(retryBuf, 700, reply, replySize); // ulozime prvni odpoved serveru (zdroj informaci o verzi serveru)
                                    if (ServerFirstReply != NULL)
                                        SalamanderGeneral->Free(ServerFirstReply);
                                    ServerFirstReply = SalamanderGeneral->DupStr(retryBuf);

                                    SkipFTPReply(replySize);
                                    break;
                                }
                            }
                            else
                            {
                                if (FTP_DIGIT_1(replyCode) == FTP_D1_MAYBESUCCESS &&
                                    FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION) // napr. 120 - Service ready in nnn minutes
                                {                                                // tuto notifikaci ignorujeme (mame jen jeden timeout)
                                }
                                else
                                {
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                        FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) // napr. 421 Service not available, closing control connection
                                    {
                                        if (state == sccsServerReady) // pokud jiz nehlasime jinou chybu
                                        {
                                            CopyStr(errBuf, 300, reply, replySize);
                                            fatalErrorTextID = -1;         // text chyby je v 'errBuf'
                                            noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                                            retryLogError = FALSE;         // chyba uz v logu je, nepridavat znovu
                                            state = sccsRetry;
                                        }
                                    }
                                    else // necekana reakce, ignorujeme ji
                                    {
                                        TRACE_E("Unexpected reply: " << CopyStr(errBuf, 300, reply, replySize));
                                    }
                                }
                            }
                        }
                        else // neni FTP server
                        {
                            if (state == sccsServerReady) // pokud jiz nehlasime jinou chybu
                            {
                                opFatalErrorTextID = IDS_NOTFTPSERVERERROR;
                                CopyStr(errBuf, 300, reply, replySize);
                                opFatalError = -1; // "error" (reply) je primo v errBuf
                                state = sccsOperationFatalError;
                                fatalErrLogMsg = FALSE; // uz je v logu, nema smysl ji tam davat znovu
                            }
                        }
                        SkipFTPReply(replySize);
                    }
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    if (event == ccsevClosed)
                    {
                        if (state == sccsServerReady) // close bez priciny
                        {
                            fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                            noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                            state = sccsRetry;
                        }
                        if (data1 != NO_ERROR)
                        {
                            FTPGetErrorText(data1, buf, 1000 - 2); // (1000-2) aby zbylo na nase CRLF
                            char* s = buf + strlen(buf);
                            while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                                s--;
                            strcpy(s, "\r\n"); // dosazeni naseho CRLF na konec radky z textem chyby
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

            waitWnd.Destroy();
            break;
        }

        case sccsRetryLogin: // zkusi znovu login (bez cekani a ztraty connectiony) - jen prejde do stavu sccsStartLoginScript - nelze pouzit s proxy serverem!!!
        {
            state = sccsStartLoginScript;
            break;
        }

        case sccsStartLoginScript: // inicializace posilani login sekvence prikazu - podle proxy server skriptu
        {
            if (proxyScriptStartExecPoint == NULL)
                TRACE_E("CControlConnectionSocket::StartControlConnection(): proxyScriptStartExecPoint cannot be NULL here!");
            proxyScriptExecPoint = proxyScriptStartExecPoint;
            proxyLastCmdReply = -1;
            proxyLastCmdReplyText[0] = 0;
            state = sccsProcessLoginScript;
            break;
        }

        case sccsProcessLoginScript: // postupne posilani login sekvence prikazu - podle proxy server skriptu
        {
            while (1)
            {
                if (sslisNone != SSLInitSequence ||
                    ProcessProxyScript(proxyScriptText, &proxyScriptExecPoint, proxyLastCmdReply,
                                       &proxyScriptParams, NULL, NULL, proxySendCmdBuf,
                                       proxyLogCmdBuf, errBuf, NULL))
                {
                    if (sslisNone == SSLInitSequence && proxyScriptParams.NeedUserInput()) // je potreba zadat nejake udaje (user, password, atd.)
                    {
                        if (proxyScriptParams.NeedProxyHost)
                        {
                        ENTER_PROXYHOST_AGAIN:
                            if (CEnterStrDlg(parent, LoadStr(IDS_ENTERPRXHOSTTITLE), LoadStr(IDS_ENTERPRXHOSTTEXT),
                                             proxyScriptParams.ProxyHost, HOST_MAX_SIZE, FALSE, NULL, FALSE)
                                    .Execute() == IDCANCEL)
                            {
                                state = sccsDone; // dal cancel -> koncime
                                actionCanceled = TRUE;
                                break;
                            }
                            else // mame zadany "proxyhost:port"
                            {
                                char* s = strchr(proxyScriptParams.ProxyHost, ':');
                                int portNum = 0;
                                if (s != NULL) // je zadany i port
                                {
                                    char* hostEnd = s++;
                                    while (*s != 0 && *s >= '0' && *s <= '9')
                                    {
                                        portNum = 10 * portNum + (*s - '0');
                                        s++;
                                    }
                                    if (*s != 0 || portNum < 1 || portNum > 65535)
                                    {
                                        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_PORTISUSHORT), LoadStr(IDS_FTPERRORTITLE),
                                                                         MB_OK | MB_ICONEXCLAMATION);
                                        goto ENTER_PROXYHOST_AGAIN;
                                    }
                                    *hostEnd = 0;
                                    proxyScriptParams.ProxyPort = portNum;
                                }

                                HANDLES(EnterCriticalSection(&SocketCritSect));
                                if (ProxyServer != NULL)
                                {
                                    ProxyServer->SetProxyHost(proxyScriptParams.ProxyHost);
                                    if (portNum != 0)
                                        ProxyServer->SetProxyPort(portNum);
                                }
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                        }
                        if (proxyScriptParams.NeedProxyPassword)
                        {
                            _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS2),
                                        proxyScriptParams.ProxyHost, proxyScriptParams.ProxyUser);
                            if (CEnterStrDlg(parent, LoadStr(IDS_ENTERPRXPASSTITLE), LoadStr(IDS_ENTERPRXPASSTEXT),
                                             proxyScriptParams.ProxyPassword, PASSWORD_MAX_SIZE, TRUE,
                                             connectingToAs, FALSE)
                                    .Execute() == IDCANCEL)
                            {
                                state = sccsDone; // dal cancel -> koncime
                                actionCanceled = TRUE;
                                break;
                            }
                            else // zmena hodnot -> musime zmenit i originaly
                            {
                                HANDLES(EnterCriticalSection(&SocketCritSect));
                                if (ProxyServer != NULL)
                                    ProxyServer->SetProxyPassword(proxyScriptParams.ProxyPassword);
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                        }
                        if (proxyScriptParams.NeedUser)
                        {
                            _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS1), proxyScriptParams.Host);
                            if (CEnterStrDlg(parent, NULL, NULL, proxyScriptParams.User, USER_MAX_SIZE, FALSE,
                                             connectingToAs, FALSE)
                                    .Execute() == IDCANCEL)
                            {
                                state = sccsDone; // dal cancel -> koncime
                                actionCanceled = TRUE;
                                break;
                            }
                            else // zmena hodnot -> musime zmenit i originaly
                            {
                                HANDLES(EnterCriticalSection(&SocketCritSect));
                                lstrcpyn(User, proxyScriptParams.User, USER_MAX_SIZE);
                                lstrcpyn(user, proxyScriptParams.User, userSize);
                                Logs.ChangeUser(logUID, User);
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                        }
                        if (proxyScriptParams.NeedPassword)
                        {
                            _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS2), proxyScriptParams.Host,
                                        proxyScriptParams.User);
                            if (CEnterStrDlg(parent, LoadStr(IDS_ENTERPASSTITLE), LoadStr(IDS_ENTERPASSTEXT),
                                             proxyScriptParams.Password, PASSWORD_MAX_SIZE, TRUE,
                                             connectingToAs, TRUE)
                                    .Execute() == IDCANCEL)
                            {
                                state = sccsDone; // dal cancel -> koncime
                                actionCanceled = TRUE;
                                break;
                            }
                            else // zmena hodnot -> musime zmenit i originaly
                            {
                                if (proxyScriptParams.Password[0] == 0)
                                    proxyScriptParams.AllowEmptyPassword = TRUE; // prazdne heslo na prani uzivatele (uz se nebudeme znovu ptat)
                                HANDLES(EnterCriticalSection(&SocketCritSect));
                                lstrcpyn(Password, proxyScriptParams.Password, PASSWORD_MAX_SIZE);
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                        }
                        if (proxyScriptParams.NeedAccount)
                        {
                            _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS2), proxyScriptParams.Host,
                                        proxyScriptParams.User);
                            if (CEnterStrDlg(parent, LoadStr(IDS_ENTERACCTTITLE), LoadStr(IDS_ENTERACCTTEXT),
                                             proxyScriptParams.Account, ACCOUNT_MAX_SIZE, TRUE,
                                             connectingToAs, FALSE)
                                    .Execute() == IDCANCEL)
                            {
                                state = sccsDone; // dal cancel -> koncime
                                actionCanceled = TRUE;
                                break;
                            }
                            else // zmena hodnot -> musime zmenit i originaly
                            {
                                HANDLES(EnterCriticalSection(&SocketCritSect));
                                lstrcpyn(Account, proxyScriptParams.Account, ACCOUNT_MAX_SIZE);
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                        }
                        // nechame prekreslit hlavni okno (aby user cely connect nekoukal na zbytek po dialogu)
                        UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                    }
                    else
                    {
                        if (proxySendCmdBuf[0] == 0 && CompressData && !bModeZSent)
                        {
                            strcpy(proxySendCmdBuf, "MODE Z\r\n");
                            strcpy(proxyLogCmdBuf, proxySendCmdBuf);
                            bModeZSent = true;
                        }
                        if (proxySendCmdBuf[0] == 0) // konec login skriptu
                        {
                            if (proxyLastCmdReply == -1) // skript neobsahuje zadny prikaz, ktery by se poslal na server - napr. pokud se prikazy preskoci kvuli tomu, ze obsahuji nepovinne promenne
                            {
                                fatalErrorTextID = IDS_INCOMPLETEPRXSCR2;
                                state = sccsFatalError;
                            }
                            else
                            {
                                if (FTP_DIGIT_1(proxyLastCmdReply) == FTP_D1_SUCCESS) // napr. 230 User logged in, proceed
                                {
                                    state = sccsDone;
                                    ret = TRUE; // USPECH, jsme zalogovany!
                                }
                                else // FTP_DIGIT_1(proxyLastCmdReply) == FTP_D1_PARTIALSUCCESS  // napr. 331 User name okay, need password
                                {
                                    lstrcpyn(errBuf, proxyLastCmdReplyText, 300);
                                    opFatalError = -1; // error je primo v errBuf
                                    opFatalErrorTextID = IDS_INCOMPLETEPRXSCR;
                                    state = sccsOperationFatalError;
                                }
                            }
                        }
                        else // mame prikaz k odeslani na server
                        {
                            DWORD error;
                            BOOL allBytesWritten;
                            if (Write(proxySendCmdBuf, -1, &error, &allBytesWritten))
                            {
                                if (useWelcomeMessage)
                                    welcomeMessage.Append(proxyLogCmdBuf, -1);
                                Logs.LogMessage(logUID, proxyLogCmdBuf, -1);

                                lstrcpyn(tmpCmdBuf, proxyLogCmdBuf, FTPCOMMAND_MAX_SIZE);
                                char* s = strchr(tmpCmdBuf, '\r');
                                if (s != NULL)
                                    *s = 0;
                                _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_SENDINGLOGINCMD), tmpCmdBuf);
                                waitWnd.SetText(buf);
                                waitWnd.Create(GetWaitTime(showWaitWndTime));

                                DWORD start = GetTickCount();
                                BOOL replyReceived = FALSE;
                                while (!allBytesWritten || state == sccsProcessLoginScript && !replyReceived)
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
                                        if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SENDCOMMANDESC2),
                                                                             LoadStr(IDS_FTPPLUGINTITLE),
                                                                             MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                                                 MB_ICONQUESTION) == IDYES)
                                        { // cancel
                                            state = sccsDone;
                                            actionCanceled = TRUE;
                                            allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
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
                                        fatalErrorTextID = IDS_SNDORABORCMDTIMEOUT;
                                        noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                                        state = sccsRetry;
                                        allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
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
                                        BOOL sectLeaved = FALSE;
                                        while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                                        {
                                            if (useWelcomeMessage)
                                                welcomeMessage.Append(reply, replySize);
                                            Logs.LogMessage(logUID, reply, replySize);

                                            if (replyCode != -1)
                                            {
                                                if ((FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) && CompressData && !_strnicmp(proxySendCmdBuf, "MODE Z", sizeof("MODE Z") - 1))
                                                {
                                                    // Server does not support compression -> swallow the error, disable compression and go on
                                                    replyCode = 200; // Emulate Full success
                                                    CompressData = FALSE;
                                                    Logs.LogMessage(logUID, LoadStr(IDS_MODEZ_LOG_UNSUPBYSERVER), -1);
                                                }
                                                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||      // napr. 230 User logged in, proceed
                                                    FTP_DIGIT_1(replyCode) == FTP_D1_PARTIALSUCCESS) // napr. 331 User name okay, need password
                                                {                                                    // mame uspesnou odpoved na prikaz, ulozime ji a budeme pokracovat v provadeni login skriptu
                                                    fastRetry = FALSE;                               // pripadne dalsi "retry" jiz nebude kvuli zpozdeni uzivatele
                                                    if (replyReceived)
                                                        TRACE_E("CControlConnectionSocket::StartControlConnection(): unexpected situation: more replies to one command!");
                                                    else
                                                    {
                                                        replyReceived = TRUE; // bereme jen prvni odpoved serveru na prikaz (dalsi odpoved uz se nejspis nevztahuje k prikazu, ale ke stavu control-connectiony - napr. "timeout")
                                                        proxyLastCmdReply = replyCode;
                                                        CopyStr(proxyLastCmdReplyText, 300, reply, replySize);
                                                    }
                                                    if (event == ccsevClosed)
                                                        state = sccsProcessLoginScript; // aby se nize jiste vypsala chyba
                                                }
                                                else
                                                {
                                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // napr. 421 Service not available (too many users), closing control connection
                                                        FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // napr. 530 Not logged in (invalid password)
                                                    {
                                                        if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR)
                                                        { // pohodlne reseni chyby "too many users" - zadne dotazy, hned "retry"
                                                            // muze vadit: s timto kodem prijde zprava, na kterou je treba reagovat zmenou user/passwd
                                                            retryLoginWithoutAsking = TRUE;
                                                        }

                                                        CopyStr(errBuf, 300, reply, replySize);
                                                        SkipFTPReply(replySize);
                                                        if (!retryLoginWithoutAsking)
                                                        {
                                                            fastRetry = TRUE; // zpozdeni kvuli uzivateli, to nektere servery nesnasi dobre (ftp.novell.com - po 20s kill)
                                                            BOOL proxyUsed = ProxyServer != NULL;

                                                            HANDLES(LeaveCriticalSection(&SocketCritSect)); // musime opustit sekci - jdeme vybalit dialog, to bude na dlouho...
                                                            sectLeaved = TRUE;

                                                            waitWnd.Show(FALSE);
                                                            if (replyCode == 534 && EncryptDataConnection ||
                                                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR && SSLInitSequence == sslisAUTH)
                                                            {
                                                                // 534 comes after PROT when data connection encyption is requested but not supported
                                                                // 530 comes after AUTH when AUTH is not recoginized
                                                                SalamanderGeneral->SalMessageBox(parent, LoadStr((SSLInitSequence == sslisAUTH) ? IDS_SSL_ERR_CONTRENCUNSUP : IDS_SSL_ERR_DATAENCUNSUP),
                                                                                                 LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONSTOP);
                                                                state = sccsDone;
                                                                allBytesWritten = TRUE;      // no longer important, the socket is gonna be closed
                                                                SSLInitSequence = sslisNone; // do not attempt to load OpenSSL libs
                                                            }
                                                            else
                                                            {
                                                                _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS1), proxyScriptParams.Host);
                                                                CLoginErrorDlg dlg(parent, errBuf, &proxyScriptParams, connectingToAs,
                                                                                   NULL, NULL, NULL, FALSE, TRUE, proxyUsed);
                                                                if (dlg.Execute() == IDOK)
                                                                {
                                                                    if (proxyScriptParams.Password[0] == 0)
                                                                        proxyScriptParams.AllowEmptyPassword = TRUE; // prazdne heslo na prani uzivatele (uz se nebudeme znovu ptat)
                                                                    // zmenime originaly (premaze pripadne zmeny v jinem threadu) + kopii ve 'value'
                                                                    HANDLES(EnterCriticalSection(&SocketCritSect));
                                                                    if (ProxyServer != NULL)
                                                                    {
                                                                        ProxyServer->SetProxyUser(proxyScriptParams.ProxyUser);
                                                                        ProxyServer->SetProxyPassword(proxyScriptParams.ProxyPassword);
                                                                    }
                                                                    lstrcpyn(User, proxyScriptParams.User, USER_MAX_SIZE);
                                                                    lstrcpyn(user, proxyScriptParams.User, userSize);
                                                                    Logs.ChangeUser(logUID, User);
                                                                    lstrcpyn(Password, proxyScriptParams.Password, PASSWORD_MAX_SIZE);
                                                                    lstrcpyn(Account, proxyScriptParams.Account, ACCOUNT_MAX_SIZE);
                                                                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                                                                    retryLoginWithoutAsking = dlg.RetryWithoutAsking;
                                                                    if (dlg.LoginChanged && event != ccsevClosed && !proxyUsed)
                                                                    { // zkusi znovu login (bez cekani a zavreni connectiony) - resi reakce typu "invalid user/password"
                                                                        state = sccsRetryLogin;
                                                                        // allBytesWritten = TRUE;   // nedojde k zavreni connectiony, musime pockat (navic, kdyz uz prisla odpoved, je snad jiste ze odesel cely prikaz)
                                                                    }
                                                                }
                                                                else // cancel
                                                                {
                                                                    state = sccsDone;
                                                                    actionCanceled = TRUE;
                                                                    allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                                                }
                                                                // nechame prekreslit hlavni okno (aby user cely connect nekoukal na zbytek po dialogu)
                                                            }
                                                            UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                                                        }

                                                        if (state == sccsProcessLoginScript)
                                                        { // std. retry (zavreni connectiony + cekani) - resi reakce typu "too many users"
                                                            opFatalErrorTextID = IDS_LOGINERROR;
                                                            // CopyStr(errBuf, 300, reply, replySize);   // dela se drive - pred opustenim kriticke sekce
                                                            opFatalError = -1;                      // text chyby je v 'errBuf'
                                                            noRetryState = sccsOperationFatalError; // pokud se neprovede retry, provede se sccsOperationFatalError
                                                            retryLogError = FALSE;                  // chyba uz v logu je, nepridavat znovu
                                                            state = sccsRetry;
                                                            allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                                        }
                                                        // SkipFTPReply(replySize);  // dela se drive - pred opustenim kriticke sekce
                                                        break;
                                                    }
                                                    else // necekana reakce, ignorujeme ji
                                                        TRACE_E("Unexpected reply: " << CopyStr(errBuf, 300, reply, replySize));
                                                }
                                            }
                                            else // neni FTP server
                                            {
                                                if (state == sccsProcessLoginScript) // pokud jiz nehlasime jinou chybu
                                                {
                                                    opFatalErrorTextID = IDS_NOTFTPSERVERERROR;
                                                    CopyStr(errBuf, 300, reply, replySize);
                                                    opFatalError = -1; // "error" (reply) je primo v errBuf
                                                    state = sccsOperationFatalError;
                                                    fatalErrLogMsg = FALSE; // uz je v logu, nema smysl ji tam davat znovu
                                                    allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                                }
                                            }
                                            SkipFTPReply(replySize);
                                        }
                                        if (!sectLeaved)
                                            HANDLES(LeaveCriticalSection(&SocketCritSect));

                                        if (event == ccsevClosed)
                                        {
                                            allBytesWritten = TRUE;              // ted uz to neni dulezite, doslo k zavreni socketu
                                            if (state == sccsProcessLoginScript) // close bez priciny
                                            {
                                                fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                                                noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                                                state = sccsRetry;
                                            }
                                            if (data1 != NO_ERROR)
                                            {
                                                FTPGetErrorText(data1, buf, 1000 - 2); // (1000-2) aby zbylo na nase CRLF
                                                char* s2 = buf + strlen(buf);
                                                while (s2 > buf && (*(s2 - 1) == '\n' || *(s2 - 1) == '\r'))
                                                    s2--;
                                                strcpy(s2, "\r\n"); // dosazeni naseho CRLF na konec radky z textem chyby
                                                Logs.LogMessage(logUID, buf, -1);
                                            }
                                        }
                                        else
                                        {
                                            switch (SSLInitSequence)
                                            {
                                            case sslisAUTH:
                                            {
                                                int errID;
                                                if (InitSSL(logUID, &errID))
                                                {
                                                    int err;
                                                    CCertificate* unverifiedCert;
                                                    if (!EncryptSocket(logUID, &err, &unverifiedCert, &errID, errBuf, 300,
                                                                       NULL /* pro control connection je to vzdy NULL */))
                                                    {
                                                        allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                                        if (errBuf[0] == 0)
                                                        {
                                                            state = sccsFatalError;
                                                            fatalErrorTextID = errID;
                                                        }
                                                        else
                                                        {
                                                            state = sccsOperationFatalError;
                                                            opFatalErrorTextID = errID;
                                                            opFatalError = -1;
                                                        }
                                                        if (err == SSLCONERR_CANRETRY)
                                                        {
                                                            noRetryState = state; // pokud se neprovede retry, provede se sccsFatalError nebo sccsOperationFatalError
                                                            state = sccsRetry;
                                                            retryLogError = FALSE;
                                                        }
                                                        else
                                                            fatalErrLogMsg = FALSE;
                                                    }
                                                    else
                                                    {
                                                        if (unverifiedCert != NULL) // socket je zasifrovany, ale certifikat serveru neni overeny, musime se zeptat uzivatele, jestli mu duveruje
                                                        {
                                                            fastRetry = TRUE; // zpozdeni kvuli uzivateli, to nektere servery nesnasi dobre (ftp.novell.com - po 20s kill)
                                                            waitWnd.Show(FALSE);

                                                            INT_PTR dlgRes;
                                                            do
                                                            {
                                                                dlgRes = CCertificateErrDialog(parent, errBuf).Execute();
                                                                switch (dlgRes)
                                                                {
                                                                case IDOK: // Accept once
                                                                {
                                                                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTACCEPTED), -1, TRUE);
                                                                    SetCertificate(unverifiedCert);
                                                                    break;
                                                                }

                                                                case IDCANCEL:
                                                                {
                                                                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTREJECTED), -1, TRUE);
                                                                    allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                                                    state = sccsDone;
                                                                    break;
                                                                }

                                                                case IDB_CERTIFICATE_VIEW:
                                                                {
                                                                    unverifiedCert->ShowCertificate(parent);
                                                                    if (unverifiedCert->CheckCertificate(errBuf, 300))
                                                                    { // certifikat serveru uz je duveryhodny (uzivatel ho nejspis rucne importoval)
                                                                        Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTVERIFIED), -1, TRUE);
                                                                        dlgRes = -1; // jen pro ukonceni cyklu
                                                                        unverifiedCert->SetVerified(true);
                                                                        SetCertificate(unverifiedCert);
                                                                    }
                                                                    break;
                                                                }
                                                                }
                                                            } while (dlgRes == IDB_CERTIFICATE_VIEW);
                                                            unverifiedCert->Release();
                                                        }
                                                    }
                                                }
                                                else // Error! OpenSSL libs not found? Or not W2K+?
                                                {
                                                    allBytesWritten = TRUE; // ted uz to neni dulezite, dojde k zavreni socketu
                                                    state = sccsFatalError;
                                                    fatalErrorTextID = errID;
                                                    fatalErrLogMsg = FALSE;
                                                }
                                                if (EncryptDataConnection)
                                                {
                                                    strcpy(proxySendCmdBuf, "PBSZ 0\r\n");
                                                    SSLInitSequence = sslisPBSZ;
                                                }
                                                else
                                                {
                                                    proxySendCmdBuf[0] = 0;
                                                    SSLInitSequence = sslisNone;
                                                }
                                                break;
                                            }
                                            case sslisPBSZ:
                                                strcpy(proxySendCmdBuf, "PROT P\r\n");
                                                SSLInitSequence = sslisPROT;
                                                break;
                                            case sslisPROT:
                                                proxySendCmdBuf[0] = 0;
                                                SSLInitSequence = sslisNone;
                                                break;
                                            case sslisNone:
                                                break;
                                            }
                                        }
                                        if (sslisNone != SSLInitSequence)
                                            strcpy(proxyLogCmdBuf, proxySendCmdBuf);
                                        break;
                                    }

                                    default:
                                        TRACE_E("Unexpected event = " << event);
                                        break;
                                    }
                                }

                                waitWnd.Destroy();
                            }
                            else // chyba Write (low memory, disconnected, chyba neblokujiciho "send")
                            {
                                while (state == sccsProcessLoginScript)
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
                                        opFatalErrorTextID = IDS_SENDCOMMANDERROR;
                                        opFatalError = error;
                                        noRetryState = sccsOperationFatalError; // pokud se neprovede retry, provede se sccsOperationFatalError
                                        state = sccsRetry;
                                        break;
                                    }

                                    case ccsevClosed:       // slo o necekanou ztratu pripojeni (osetrime take to, ze ccsevClosed mohla prepsat ccsevNewBytesRead)
                                    case ccsevNewBytesRead: // nacetli jsme nove byty (mozna popis chyby vedouci k disconnectu)
                                    {
                                        char* reply;
                                        int replySize;
                                        int replyCode;

                                        HANDLES(EnterCriticalSection(&SocketCritSect));
                                        while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                                        {
                                            Logs.LogMessage(logUID, reply, replySize);

                                            if (replyCode == -1 ||                                 // neni FTP odpoved
                                                FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // popis docasne chyby
                                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // popis chyby
                                            {
                                                opFatalErrorTextID = IDS_SENDCOMMANDERROR;
                                                CopyStr(errBuf, 300, reply, replySize);
                                                SkipFTPReply(replySize);
                                                opFatalError = -1;                      // "error" (reply) je primo v errBuf
                                                noRetryState = sccsOperationFatalError; // pokud se neprovede retry, provede se sccsOperationFatalError
                                                retryLogError = FALSE;                  // chyba uz v logu je, nepridavat znovu
                                                state = sccsRetry;
                                                break; // dalsi hlasku uz neni nutne cist
                                            }
                                            SkipFTPReply(replySize);
                                        }
                                        HANDLES(LeaveCriticalSection(&SocketCritSect));

                                        if (event == ccsevClosed)
                                        {
                                            if (state == sccsProcessLoginScript) // close bez priciny
                                            {
                                                fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                                                noRetryState = sccsFatalError; // pokud se neprovede retry, provede se sccsFatalError
                                                state = sccsRetry;
                                            }
                                            if (data1 != NO_ERROR)
                                            {
                                                FTPGetErrorTextForLog(data1, buf, 1000);
                                                Logs.LogMessage(logUID, buf, -1);
                                            }
                                        }
                                        break;
                                    }
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
                else // teoreticky by nikdy nemelo nastat (ulozene skripty jsou validovane)
                {
                    opFatalError = -1; // error je primo v errBuf
                    opFatalErrorTextID = IDS_ERRINPROXYSCRIPT;
                    state = sccsOperationFatalError;
                    break;
                }
            }
            break;
        }

        case sccsFatalError: // fatalni chyba (res-id textu je v 'fatalErrorTextID' + je-li 'fatalErrorTextID' -1, je string rovnou v 'errBuf')
        {
            lstrcpyn(buf, GetFatalErrorTxt(fatalErrorTextID, errBuf), 1000);
            char* s = buf + strlen(buf);
            while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                s--;
            if (fatalErrLogMsg)
            {
                strcpy(s, "\r\n");                      // CRLF na konec textu posledni chyby
                Logs.LogMessage(logUID, buf, -1, TRUE); // text posledni chyby pridame do logu
            }
            fatalErrLogMsg = TRUE;
            *s = 0;
            SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            state = sccsDone;
            break;
        }

        case sccsOperationFatalError: // fatalni chyba operace (res-id textu je v 'opFatalErrorTextID' a
        {                             // cislo Windows chyby v 'opFatalError' + je-li 'opFatalError' -1,
                                      // je string rovnou v 'errBuf' + je-li 'opFatalErrorTextID' -1, je
                                      // string rovnou v 'formatBuf')
            const char* e = GetOperationFatalErrorTxt(opFatalError, errBuf);
            if (fatalErrLogMsg)
            {
                lstrcpyn(buf, e, 1000);
                char* s = buf + strlen(buf);
                while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                strcpy(s, "\r\n");                      // CRLF na konec textu posledni chyby
                Logs.LogMessage(logUID, buf, -1, TRUE); // text posledni chyby pridame do logu
            }
            fatalErrLogMsg = TRUE;
            char* f;
            if (opFatalErrorTextID != -1)
                f = LoadStr(opFatalErrorTextID);
            else
                f = formatBuf;
            sprintf(buf, f, e);
            SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            state = sccsDone;
            break;
        }

        default: // (always false)
        {
            TRACE_E("Enexpected situation in CControlConnectionSocket::StartControlConnection(): state = " << state);
            state = sccsDone;
            break;
        }
        }
    }

    if (actionCanceled)
        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE); // ESC (cancel) do logu

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

    if (ret) // jsme uspesne pripojeni na FTP server
    {
        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGLOGINSUCCESS), -1, TRUE);

        if (useWelcomeMessage && welcomeMessage.Length > 0)
        { // zobrazime "welcome message"
            CWelcomeMsgDlg* w = new CWelcomeMsgDlg(SalamanderGeneral->GetMainWindowHWND(),
                                                   welcomeMessage.GetString());
            if (w != NULL)
            {
                if (w->Create() == NULL)
                {
                    delete w;
                    w = NULL;
                }
                else
                {
                    ModelessDlgs.Add(w);
                    if (!ModelessDlgs.IsGood())
                    {
                        DestroyWindow(w->HWindow); // zaroven dealokace 'w'
                        ModelessDlgs.ResetState();
                    }
                    else
                    {
                        OurWelcomeMsgDlg = w->HWindow;
                        SalamanderGeneral->PostMenuExtCommand(FTPCMD_ACTIVWELCOMEMSG, TRUE);
                    }
                }
            }
        }

        // posleme inicializacni FTP prikazy
        HANDLES(EnterCriticalSection(&SocketCritSect));
        char cmdBuf[FTP_MAX_PATH];
        if (InitFTPCommands != NULL && *InitFTPCommands != 0)
            lstrcpyn(cmdBuf, InitFTPCommands, FTP_MAX_PATH);
        else
            cmdBuf[0] = 0;
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        BOOL canRetry = TRUE;
        if (cmdBuf[0] != 0)
        {
            char* next = cmdBuf;
            char* s;
            while (GetToken(&s, &next))
            {
                if (*s != 0 && *s <= ' ')
                    s++;     // odbourame jen prvni mezeru (aby na zacatek prikazu bylo mozne dat i mezery)
                if (*s != 0) // je-li co, posleme to na server
                {
                    _snprintf_s(retryBuf, _TRUNCATE, "%s\r\n", s);
                    int ftpReplyCode;
                    if (!SendFTPCommand(parent, retryBuf, retryBuf, NULL, GetWaitTime(showWaitWndTime),
                                        NULL, &ftpReplyCode, NULL, 0, FALSE, TRUE, TRUE, &canRetry,
                                        errBuf, 300, NULL))
                    {
                        ret = FALSE; // uz nejsme pripojeni
                        break;       // jdeme provest "retry"
                    }
                }
            }
        }

        // zjistime operacni system serveru
        if (ret && PrepareFTPCommand(buf, 1000, formatBuf, 300, ftpcmdSystem, NULL))
        {
            int ftpReplyCode;
            if (SendFTPCommand(parent, buf, formatBuf, NULL, GetWaitTime(showWaitWndTime), NULL,
                               &ftpReplyCode, retryBuf, 700, FALSE, FALSE, FALSE, &canRetry,
                               errBuf, 300, NULL))
            {
                HANDLES(EnterCriticalSection(&SocketCritSect));
                if (ServerSystem != NULL)
                    SalamanderGeneral->Free(ServerSystem);
                ServerSystem = SalamanderGeneral->DupStr(retryBuf);
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            }
            else
                ret = FALSE; // chyba -> zavrena connectiona - jdeme provest "retry"
        }

        if (ret && workDir != NULL &&
            !GetCurrentWorkingPath(parent, workDir, workDirBufSize, FALSE, &canRetry, errBuf, 300))
        {
            ret = FALSE; // chyba -> zavrena connectiona - jdeme provest "retry"
        }

        if (canRetry && !ret) // predpoklada nastaveny 'errBuf'
        {
            opFatalErrorTextID = IDS_SENDCOMMANDERROR;
            opFatalError = -1;                      // "error" (reply) je primo v errBuf
            noRetryState = sccsOperationFatalError; // pokud se neprovede retry, provede se sccsOperationFatalError
            retryLogError = FALSE;                  // chyba uz v logu je, nepridavat znovu
            state = sccsRetry;
            useWelcomeMessage = FALSE; // uz jsme ji vypsali, znovu uz nema smysl

            // znovu disablujeme 'parent', pri enablovani obnovime i fokus
            EnableWindow(parent, FALSE);

            // nahodime cekaci kurzor nad parentem, bohuzel to jinak neumime
            winParent = new CSetWaitCursorWindow;
            if (winParent != NULL)
                winParent->AttachToWindow(parent);

            goto RETRY_LABEL; // jdeme na dalsi pokus
        }
    }
    else
    {
        CloseSocket(NULL); // zavreme socket (je-li otevreny), system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
        Logs.SetIsConnected(logUID, IsConnected());
        Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
    }
    if (ret)
        SetupKeepAliveTimer(); // pokud je vse OK, nastavime timer pro keep-alive
    else
        ReleaseKeepAlive(); // pri chybe uvolnime keep-alive (nelze pouzivat bez navazaneho spojeni)
    return ret;
}

BOOL CControlConnectionSocket::ReconnectIfNeeded(BOOL notInPanel, BOOL leftPanel, HWND parent,
                                                 char* userBuf, int userBufSize, BOOL* reconnected,
                                                 BOOL setStartTimeIfConnected, int* totalAttemptNum,
                                                 const char* retryMsg, BOOL* userRejectsReconnect,
                                                 int reconnectErrResID, BOOL useFastReconnect)
{
    CALL_STACK_MESSAGE8("CControlConnectionSocket::ReconnectIfNeeded(%d, %d, , , %d, , %d, , %s, , %d, %d)",
                        notInPanel, leftPanel, userBufSize, setStartTimeIfConnected, retryMsg,
                        reconnectErrResID, useFastReconnect);
    if (reconnected != NULL)
        *reconnected = FALSE;
    if (userRejectsReconnect != NULL)
        *userRejectsReconnect = FALSE;
    if (retryMsg == NULL && IsConnected()) // nezname duvod preruseni spojeni + spojeni neni preruseno
    {
        if (setStartTimeIfConnected)
            SetStartTime();
        return TRUE;
    }
    else
    {
        if (retryMsg == NULL) // preruseni spojeni z neznameho duvodu - vypiseme + zeptame se na reconnect
        {
            // pokud jsme jeste nestihli vypsat co vedlo k zavreni connectiny, udelame to ted
            CheckCtrlConClose(notInPanel, leftPanel, parent, FALSE);

            BOOL reconnectToSrv = FALSE;
            if (!Config.AlwaysReconnect)
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = parent;
                params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_HINT;
                params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                params.Text = LoadStr(IDS_RECONNECTTOSRV);
                params.CheckBoxText = LoadStr(IDS_ALWAYSRECONNECT);
                params.CheckBoxValue = &Config.AlwaysReconnect;
                reconnectToSrv = SalamanderGeneral->SalMessageBoxEx(&params) == IDYES;
                if (userRejectsReconnect != NULL)
                    *userRejectsReconnect = !reconnectToSrv;
            }

            if (Config.AlwaysReconnect || reconnectToSrv)
            {
                SetStartTime();
                BOOL ret = StartControlConnection(parent, userBuf, userBufSize, TRUE, NULL, 0,
                                                  totalAttemptNum, retryMsg, FALSE,
                                                  reconnectErrResID, useFastReconnect);
                if (ret && reconnected != NULL)
                    *reconnected = TRUE;
                return ret;
            }
            else
                return FALSE; // uzivatel nechce ani zkusit znovu otevrit "control connection"
        }
        else // preruseni spojeni ze znameho duvodu - pustime "retry" uvnitr StartControlConnection()
        {
            BOOL ret = StartControlConnection(parent, userBuf, userBufSize, TRUE, NULL, 0,
                                              totalAttemptNum, retryMsg, FALSE,
                                              reconnectErrResID, useFastReconnect);
            if (ret && reconnected != NULL)
                *reconnected = TRUE;
            return ret;
        }
    }
}

void CControlConnectionSocket::ActivateWelcomeMsg()
{
    if (OurWelcomeMsgDlg != NULL && GetForegroundWindow() == SalamanderGeneral->GetMainWindowHWND())
    {
        int i;
        for (i = ModelessDlgs.Count - 1; i >= 0; i--) // hledame od zadu (posledni okno je posledni v poli)
        {
            if (ModelessDlgs[i]->HWindow == OurWelcomeMsgDlg) // okno je jeste otevrene, aktivujeme ho
            {
                SetForegroundWindow(OurWelcomeMsgDlg);
                break;
            }
        }
    }
}

char* CControlConnectionSocket::AllocServerSystemReply()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    char* ret = SalamanderGeneral->DupStr(HandleNULLStr(ServerSystem));
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

char* CControlConnectionSocket::AllocServerFirstReply()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    char* ret = SalamanderGeneral->DupStr(HandleNULLStr(ServerFirstReply));
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}
