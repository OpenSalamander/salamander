// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

CClosedCtrlConChecker ClosedCtrlConChecker; // handles informing the user about "control connection" closure outside of operations
CListingCache ListingCache;                 // cache of directory listings on servers (used when changing and listing directories)

int CLogData::NextLogUID = 0;          // global counter for log objects
int CLogData::OldestDisconnectNum = 0; // disconnect number of the oldest server log that disconnected
int CLogData::NextDisconnectNum = 0;   // disconnect number for the next server log that will disconnect

CLogs Logs; // logs of all connections to FTP servers

CControlConnectionSocket* LeftPanelCtrlCon = NULL;
CControlConnectionSocket* RightPanelCtrlCon = NULL;
CRITICAL_SECTION PanelCtrlConSect; // critical section for access to LeftPanelCtrlCon and RightPanelCtrlCon

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

    memset(Password, 0, PASSWORD_MAX_SIZE); // wipe the memory where the password appeared
    if (NewEvent != NULL)
        HANDLES(CloseHandle(NewEvent));
    HANDLES(DeleteCriticalSection(&EventCritSect));

    // Logs cannot touch the "control connection" (nested critical sections are forbidden),
    // this call synchronizes only the validity of the pointer to the "control connection" (not the object contents,
    // therefore it can be at the very end of the destructor)
    if (LogUID != -1)
        Logs.ClosingConnection(LogUID);
}

DWORD
CControlConnectionSocket::GetTimeFromStart()
{
    DWORD t = GetTickCount();
    return t - StartTime; // works even for t < StartTime (tick counter wraparound)
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
            e = Events[EventsUsedCount++]; // we already have preallocated space for the event
        else                               // we must allocate it
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
    if (e != NULL) // we have space for the event, store the data into it
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
        Events.Add(e); // the event object will be reused later, put it at the end of the queue
        if (!Events.IsGood())
        {
            delete e; // failed to add it, too bad
            Events.ResetState();
        }
        RewritableEvent = FALSE;
        ret = TRUE;
        if (EventsUsedCount > 0)
            SetEvent(NewEvent); // there is another one
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

    const DWORD cycleTime = 200; // period of testing the ESC key press in ms (200 = 5 times per second)
    DWORD timeStart = GetTickCount();
    DWORD restOfWaitTime = milliseconds; // remaining waiting time

    HANDLE watchedEvent;
    BOOL watchingUserIface;
    if (waitForUserIfaceFinish && userIface != NULL)
    {
        if ((watchedEvent = userIface->GetFinishedEvent()) == NULL)
        {
            TRACE_E("Unexpected situation in CControlConnectionSocket::WaitForEventOrESC(): userIface->GetFinishedEvent() returned NULL!");
            Sleep(200); // so the whole machine does not freeze...
            *event = ccsevUserIfaceFinished;
            *data1 = 0;
            *data2 = 0;
            return; // report completion of work in the user interface
        }
        watchingUserIface = TRUE;
    }
    else
    {
        watchingUserIface = FALSE;
        watchedEvent = NewEvent;
    }

    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help
    while (1)
    {
        DWORD waitTime;
        if (milliseconds != INFINITE)
            waitTime = min(cycleTime, restOfWaitTime);
        else
            waitTime = cycleTime;
        DWORD waitRes = MsgWaitForMultipleObjects(1, &watchedEvent, FALSE, waitTime, QS_ALLINPUT);

        // first check for ESC press (so we do not ignore it for the user)
        if (milliseconds != 0 &&                                                          // if the timeout is zero, we are only pumping messages, ignore ESC
            ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == parent || // ESC key pressed
             waitWnd != NULL && waitWnd->GetWindowClosePressed() ||                       // close button in the wait window
             userIface != NULL && userIface->GetWindowClosePressed()))                    // close button in the user interface
        {
            // cannot read the event now, leave it for next time
            if (waitRes == WAIT_OBJECT_0 && !watchingUserIface)
                SetEvent(NewEvent);

            MSG msg; // discard buffered ESC
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                ;
            *event = ccsevESC;
            *data1 = 0;
            *data2 = 0;
            break; // report ESC
        }
        if (waitRes == WAIT_OBJECT_0) // try to read a new event
        {
            if (!watchingUserIface)
            {
                if (GetEvent(event, data1, data2))
                    break; // report a new event
            }
            else // user interface reports completion
            {
                *event = ccsevUserIfaceFinished;
                *data1 = 0;
                *data2 = 0;
                break; // report completion of work in the user interface
            }
        }
        else
        {
            if (waitRes == WAIT_OBJECT_0 + 1) // process Windows messages
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
                    restOfWaitTime == waitTime) // not just the timeout of the ESC key test cycle, but the global timeout
                {
                    *event = ccsevTimeout; // no time remains
                    *data1 = 0;
                    *data2 = 0;
                    break; // report timeout
                }
            }
        }
        if (milliseconds != INFINITE) // recalculate the remaining waiting time (based on real time)
        {
            DWORD t = GetTickCount() - timeStart; // works even when the tick counter wraps around
            if (t < (DWORD)milliseconds)
                restOfWaitTime = (DWORD)milliseconds - t;
            else
                restOfWaitTime = 0; // let it report the timeout (we must not do it ourselves - the event has priority over the timeout)
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
        ProxyServer = Config.FTPProxyServerList.MakeCopyOfProxyServer(proxyServerUID, NULL); // ignore lack of memory (automatically "not used (direct connection)")
        if (ProxyServer != NULL)
        {
            if (ProxyServer->ProxyEncryptedPassword != NULL)
            {
                // decrypt the password into ProxyPlainPassword
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                char* plainPassword = NULL;
                if (!passwordManager->DecryptPassword(ProxyServer->ProxyEncryptedPassword, ProxyServer->ProxyEncryptedPasswordSize, &plainPassword))
                {
                    // at this point it should be verified that the password can be decrypted (in all branches calling SetConnectionParameters()
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

enum CStartCtrlConStates // states of the automaton for CControlConnectionSocket::StartControlConnection
{
    // obtain an IP address from the textual address of the FTP server
    sccsGetIP,

    // fatal error (resource ID of the text is in 'fatalErrorTextID' + if 'fatalErrorTextID' is -1, the string is directly in 'errBuf')
    sccsFatalError,

    // fatal operation error (resource ID of the text is in 'opFatalErrorTextID' and the Windows error number in
    // 'opFatalError' + if 'opFatalError' is -1, the string is directly in 'errBuf' + if
    // 'opFatalErrorTextID' is -1, the string is directly in 'formatBuf')
    sccsOperationFatalError,

    // connect to the FTP server (retrieved IP is in 'auxServerIP')
    sccsConnect,

    // retry the connection (based on Config.DelayBetweenConRetries + Config.ConnectRetries),
    // if it should not retry anymore, it transitions to the state from 'noRetryState' + if 'fastRetry' is TRUE, it must not
    // wait before the next attempt
    sccsRetry,

    // now connected, read the message from the server (expecting "220 Service ready for new user")
    sccsServerReady,

    // initialize sending of the login sequence of commands - according to the proxy server script
    sccsStartLoginScript,

    // gradually send the login sequence of commands - according to the proxy server script
    sccsProcessLoginScript,

    // retry the login (without waiting and losing the connection) - only transitions to sccsStartLoginScript - cannot be used with a proxy server!!!
    sccsRetryLogin,

    // end of the method (successful or unsuccessful - according to 'ret' TRUE/FALSE)
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
        return FALSE; // end of string, no more tokens
    char* dst = NULL;
    do
    {
        if (*t == ';')
        {
            if (*(t + 1) == ';') // escape sequence: ";;" -> ";"
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
        reconnect = TRUE; // in this case it certainly is a reconnect

    BOOL ret = FALSE;
    int fatalErrorTextID = 0;
    int opFatalErrorTextID = 0;
    int opFatalError = 0;
    CStartCtrlConStates noRetryState = sccsDone;
    BOOL retryLogError = TRUE;   // FALSE = do not print the error message to the log (reason: it has already been printed)
    BOOL fatalErrLogMsg = TRUE;  // FALSE = do not print the error message to the log (reason: it has already been printed)
    BOOL actionCanceled = FALSE; // TRUE = write "action canceled" to the log at 'sccsDone'

    int attemptNum = 1;
    if (totalAttemptNum != NULL)
        attemptNum = *totalAttemptNum; // initialize with the total number of attempts
    CDynString welcomeMessage;
    BOOL useWelcomeMessage = canShowWelcomeDlg && !reconnect && Config.ShowWelcomeMessage;
    BOOL retryLoginWithoutAsking = FALSE;
    BOOL fastRetry = FALSE;
    unsigned short port;
    in_addr srvAddr;
    int logUID = -1; // UID of the log for this connection: currently "invalid log"

    char proxyScriptText[PROXYSCRIPT_MAX_SIZE];
    char host[HOST_MAX_SIZE];
    char buf[1000];
    char errBuf[300];
    char errBuf2[300];
    char formatBuf[300];
    char retryBuf[700];

    const DWORD showWaitWndTime = WAITWND_STARTCON; // show time of the wait window
    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // at least one second

    // store the focus from 'parent' (if the focus is not from 'parent', store NULL)
    HWND focusedWnd = GetFocus();
    HWND hwnd = focusedWnd;
    while (hwnd != NULL && hwnd != parent)
        hwnd = GetParent(hwnd);
    if (hwnd != parent)
        focusedWnd = NULL;

    // disable 'parent', restore the focus when re-enabling
    EnableWindow(parent, FALSE);

    // set a wait cursor over the parent, unfortunately we cannot do it otherwise
    CSetWaitCursorWindow* winParent = new CSetWaitCursorWindow;
    if (winParent != NULL)
        winParent->AttachToWindow(parent);

    OurWelcomeMsgDlg = NULL; // no need to synchronize, accessed only from the main thread

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
            txt = GetProxyScriptText(fpstNotUsed, FALSE); // undefined script = "not used (direct connection)" script - SOCKS 4/4A/5, HTTP 1.1
        lstrcpyn(proxyScriptText, txt, PROXYSCRIPT_MAX_SIZE);
    }
    DWORD auxServerIP = ServerIP;
    srvAddr.s_addr = auxServerIP;
    ResetWorkingPathCache();         // after connecting to the server it is necessary to determine the working dir
    ResetCurrentTransferModeCache(); // after connecting to the server it is necessary to set the transfer mode (it should be ASCII, but we do not trust it)
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    const char* proxyScriptExecPoint = NULL;
    const char* proxyScriptStartExecPoint = NULL; // first script command (line following "connect to:")
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

    // prepare keep-alive for further use + set keep-alive to 'kamForbidden' (a normal command is in progress)
    ReleaseKeepAlive();
    WaitForEndOfKeepAlive(parent, 0); // cannot open the wait window (it is in the 'kamNone' state)

    CStartCtrlConStates state = (auxServerIP == INADDR_NONE ? sccsGetIP : sccsConnect);

    if (retryMsg != NULL) // simulate the state when the connection was interrupted directly in this method - "retry" connection
    {
        lstrcpyn(errBuf, retryMsg, 300); // store the retry message in errBuf (for sccsOperationFatalError)
        opFatalErrorTextID = reconnectErrResID != -1 ? reconnectErrResID : IDS_SENDCOMMANDERROR;
        opFatalError = -1;                      // the "error" (reply) is directly in errBuf
        noRetryState = sccsOperationFatalError; // if retry is not performed, execute sccsOperationFatalError
        retryLogError = FALSE;                  // the error is already in the log, do not add it again
        state = sccsRetry;
        // useWelcomeMessage = FALSE;  // we already printed it, repeating it makes no sense  -- "always FALSE"
        fastRetry = useFastReconnect;
    }

    if (ProcessProxyScript(proxyScriptText, &proxyScriptExecPoint, proxyLastCmdReply,
                           &proxyScriptParams, host, &port, NULL, NULL, errBuf2, NULL))
    {
        if (proxyScriptParams.NeedUserInput()) // theoretically should not happen
        {                                      // only proxyScriptParams->NeedProxyHost can be TRUE (otherwise ProcessProxyScript would return an error)
            strcpy(errBuf, LoadStr(IDS_PROXYSRVADREMPTY));
            lstrcpyn(errBuf, errBuf2, 300);
            opFatalError = -1; // error is directly in errBuf
            opFatalErrorTextID = IDS_ERRINPROXYSCRIPT;
            state = sccsOperationFatalError;
        }
        else
            proxyScriptStartExecPoint = proxyScriptExecPoint;
    }
    else // theoretically should never happen (saved scripts are validated)
    {
        lstrcpyn(errBuf, errBuf2, 300);
        opFatalError = -1; // error is directly in errBuf
        opFatalErrorTextID = IDS_ERRINPROXYSCRIPT;
        state = sccsOperationFatalError;
    }

RETRY_LABEL:

    while (state != sccsDone)
    {
        CALL_STACK_MESSAGE2("state = %d", state); // so we can see where it eventually crashed/froze
        switch (state)
        {
        case sccsGetIP: // obtain an IP address from the textual address of the FTP server
        {
            if (!GetHostByAddress(host, 0)) // must be outside the SocketCritSect section
            {                               // no chance of success -> report an error
                sprintf(formatBuf, LoadStr(IDS_GETIPERROR), host);
                opFatalErrorTextID = -1; // the text is in 'formatBuf'
                opFatalError = NO_ERROR; // unknown error
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
                    // wait for an event on the socket (receiving the resolved IP address) or ESC
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
                            SalamanderGeneral->WaitForESCRelease(); // measure to prevent interrupting the next action after every ESC in the previous message box
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

                    case ccsevIPReceived: // data1 == IP, data2 == error
                    {
                        if (data1 != INADDR_NONE) // we have an IP
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            auxServerIP = ServerIP = data1;
                            HANDLES(LeaveCriticalSection(&SocketCritSect));

                            state = sccsConnect;
                        }
                        else // error
                        {
                            sprintf(formatBuf, LoadStr(IDS_GETIPERROR), host);
                            opFatalErrorTextID = -1; // the text is in 'formatBuf'
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

        case sccsConnect: // connect to the FTP server (retrieved IP is in 'auxServerIP')
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

            // create a log and insert the header into the log
            if (!reconnect && LogUID == -1) // we do not have a log yet
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

            // add the header - Host + Port + IP + User
            Logs.LogMessage(logUID, buf, -1);

            ResetBuffersAndEvents(); // empty the buffers (discard old data) and discard old events
            if (useWelcomeMessage)
                welcomeMessage.Clear(); // clear the previous attempt (only makes sense after a "retry")

            if ((proxyType == fpstSocks5 || proxyType == fpstHTTP1_1) &&
                proxyScriptParams.ProxyUser[0] != 0 && proxyScriptParams.ProxyPassword[0] == 0)
            { // the user should enter the proxy password
                _snprintf_s(connectingToAs, _TRUNCATE, LoadStr(IDS_CONNECTINGTOAS2),
                            proxyScriptParams.ProxyHost, proxyScriptParams.ProxyUser);
                if (CEnterStrDlg(parent, LoadStr(IDS_ENTERPRXPASSTITLE), LoadStr(IDS_ENTERPRXPASSTEXT),
                                 proxyScriptParams.ProxyPassword, PASSWORD_MAX_SIZE, TRUE,
                                 connectingToAs, FALSE)
                        .Execute() != IDCANCEL)
                { // value change -> we must update the originals as well
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
                    // wait for an event on the socket (opening the connection to the server) or ESC
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
                            SalamanderGeneral->WaitForESCRelease(); // measure to prevent interrupting the next action after every ESC in the previous message box
                            waitWnd.Show(TRUE);
                        }
                        break;
                    }

                    case ccsevTimeout:
                    {
                        if (GetProxyTimeoutDescr(errBuf, 300))
                            fatalErrorTextID = -1; // the description is directly in 'errBuf'
                        else
                            fatalErrorTextID = IDS_OPENCONTIMEOUT;
                        noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
                        state = sccsRetry;
                        break;
                    }

                    case ccsevConnected: // data1 == error
                    {
                        if (data1 == NO_ERROR)
                            state = sccsServerReady; // we are connected
                        else                         // error
                        {
                            if (GetProxyError(errBuf, 300, formatBuf, 300, FALSE))
                                opFatalError = -1; // error while connecting through the proxy server: the error text is directly in 'errBuf'+'formatBuf'
                            else
                            {
                                opFatalError = data1;
                                sprintf(formatBuf, LoadStr(IDS_OPENCONERROR), host, inet_ntoa(srvAddr), port);
                            }
                            opFatalErrorTextID = -1;                  // the text is in 'formatBuf'
                            noRetryState = sccsOperationFatalError;   // if retry is not performed, execute sccsOperationFatalError
                            if (opFatalError == -1 && errBuf[0] == 0) // simple error -> convert it to sccsFatalError
                            {
                                fatalErrorTextID = -1;
                                lstrcpyn(errBuf, formatBuf, 300);
                                noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
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
                opFatalErrorTextID = -1;                // the text is in 'formatBuf'
                noRetryState = sccsOperationFatalError; // if retry is not performed, execute sccsOperationFatalError
                state = sccsRetry;
            }
            break;
        }

        case sccsRetry: // try connecting again (based on Config.DelayBetweenConRetries + Config.ConnectRetries),
        {               // if it should no longer try, transition to the state from 'noRetryState' + if 'fastRetry' is TRUE,
                        // do not wait before the next attempt
            // Resend AUTH TLS if needed
            if (EncryptControlConnection)
            {
                SSLInitSequence = sslisAUTH;
                strcpy(proxySendCmdBuf, "AUTH TLS\r\n");
                strcpy(proxyLogCmdBuf, proxySendCmdBuf);
            }
            bModeZSent = false;

            switch (noRetryState) // text of the last error for the log
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
                strcpy(s1, "\r\n");                     // CRLF at the end of the last error text
                Logs.LogMessage(logUID, buf, -1, TRUE); // add the last error text to the log
            }
            retryLogError = TRUE;
            *s1 = 0; // trim end-of-line characters from the last error text

            // when the server closes the control connection, keep-alive changes to 'kamNone', therefore:
            // prepare keep-alive for further use + set keep-alive to 'kamForbidden' (a normal command is in progress)
            ReleaseKeepAlive();
            WaitForEndOfKeepAlive(parent, 0); // cannot open the wait window (it is in the 'kamNone' state)

            if (fastRetry) // this "retry" may be due to user delay - we will not wait, it might succeed (ftp.novell.com - kills after 20s)
            {
                fastRetry = FALSE;

                // if necessary, close the socket; the system will attempt a graceful shutdown (we will not learn the result)
                CloseSocket(NULL);
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg(); // display "connection inactive"

                state = sccsConnect; // try to connect again (the IP is already known)
            }
            else
            {
                if (attemptNum < Config.GetConnectRetries() + 1) // try to connect again, but wait first
                {
                    attemptNum++; // increase the connection attempt number
                    if (totalAttemptNum != NULL)
                        *totalAttemptNum = attemptNum; // store the total number of attempts

                    // if necessary, close the socket; the system will attempt a graceful shutdown (we will not learn the result)
                    CloseSocket(NULL);
                    Logs.SetIsConnected(logUID, IsConnected());
                    Logs.RefreshListOfLogsInLogsDlg(); // display "connection inactive"

                    switch (noRetryState) // text of the last error for the wait window
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
                            if (*(s + 1) == '\n') // remove an empty line in the text
                                memmove(s, s + 1, strlen(s + 1) + 1);
                            s++;
                        }
                        s = s + strlen(s);
                        while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                            s--;
                        *s = 0; // trim the EOL at the end of the message
                        break;
                    }
                    }

                    // show a window with the waiting message
                    int delayBetweenConRetries = Config.GetDelayBetweenConRetries();
                    _snprintf_s(retryBuf, _TRUNCATE, LoadStr(IDS_WAITINGTORETRYSUF), buf, delayBetweenConRetries,
                                attemptNum, Config.GetConnectRetries() + 1);

                    waitWnd.SetText(retryBuf);
                    waitWnd.Create(0);

                    // wait for ESC or the waiting timeout
                    CControlConnectionSocketEvent event;
                    DWORD data1, data2;
                    BOOL run = TRUE;
                    DWORD start = GetTickCount();
                    while (run)
                    {
                        DWORD now = GetTickCount();
                        if (now - start < (DWORD)delayBetweenConRetries * 1000)
                        { // rebuild the text for the retry wait window (contains the countdown)
                            DWORD wait = delayBetweenConRetries * 1000 - (now - start);
                            if (now != start) // it makes no sense the first time
                            {
                                _snprintf_s(retryBuf, _TRUNCATE, LoadStr(IDS_WAITINGTORETRYSUF), buf, (1 + (wait - 1) / 1000),
                                            attemptNum, Config.GetConnectRetries() + 1);
                                waitWnd.SetText(retryBuf);
                            }
                            BOOL notTimeout = FALSE; // TRUE = it cannot be a timeout
                            if (wait > 1500)
                            {
                                wait = 1000; // wait at most 1.5 seconds, at least 0.5 seconds
                                notTimeout = TRUE;
                            }
                            WaitForEventOrESC(parent, &event, &data1, &data2,
                                              wait, &waitWnd, NULL, FALSE);
                            switch (event) // we only care about ESC or timeout, ignore events
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
                                /* used by the script export_mnu.py, which generates salmenu.mnu for Translator
   we let the message box buttons resolve hotkey collisions by simulating that this is a menu
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
                                { // gives up further login attempts
                                    state = sccsDone;
                                    run = FALSE;
                                    // actionCanceled = TRUE;   // do not log cancel in "retry"
                                }
                                else
                                {
                                    if (msgRes == IDNO)
                                    {
                                        event = ccsevTimeout;
                                        run = FALSE; // immediately try the next login
                                    }
                                    else
                                    {
                                        SalamanderGeneral->WaitForESCRelease(); // measure to prevent interrupting the next action after every ESC in the previous message box
                                        waitWnd.Show(TRUE);                     // wants to continue waiting
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
                        else // we will not try it anymore (timeout)
                        {
                            event = ccsevTimeout;
                            run = FALSE;
                        }
                    }

                    waitWnd.Destroy();

                    if (event == ccsevTimeout)
                        state = sccsConnect; // try to connect again (the IP is already known)
                }
                else
                {
                    if (noRetryState == sccsFatalError || noRetryState == sccsOperationFatalError)
                        fatalErrLogMsg = FALSE; // the message was already logged (if needed), do not log it again
                    state = noRetryState;       // no more attempts, continue with the state from 'noRetryState'
                }
            }
            break;
        }

        case sccsServerReady: // now connected, read the message from the server (expect "220 Service ready for new user")
        {
            waitWnd.SetText(LoadStr(IDS_WAITINGFORLOGIN));
            waitWnd.Create(GetWaitTime(showWaitWndTime));

            DWORD start = GetTickCount();
            while (state == sccsServerReady)
            {
                // wait for an event on the socket (server reply) or ESC
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
                        SalamanderGeneral->WaitForESCRelease(); // measure to prevent interrupting the next action after every ESC in the previous message box
                        waitWnd.Show(TRUE);
                    }
                    break;
                }

                case ccsevTimeout:
                {
                    fatalErrorTextID = IDS_WAITFORLOGTIMEOUT;
                    noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
                    state = sccsRetry;
                    break;
                }

                case ccsevClosed:       // possible unexpected connection loss (also handle that ccsevClosed could overwrite ccsevNewBytesRead)
                case ccsevNewBytesRead: // read new bytes
                {
                    char* reply;
                    int replySize;
                    int replyCode;

                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    while (ReadFTPReply(&reply, &replySize, &replyCode)) // while we have some server response
                    {
                        if (useWelcomeMessage)
                            welcomeMessage.Append(reply, replySize);
                        Logs.LogMessage(logUID, reply, replySize);

                        if (replyCode != -1)
                        {
                            if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&
                                FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION) // e.g. 220 - Service ready for new user
                            {
                                if (event != ccsevClosed) // if the connection is not closed yet
                                {
                                    state = sccsStartLoginScript; // send the login command sequence

                                    CopyStr(retryBuf, 700, reply, replySize); // store the first server reply (source of server version info)
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
                                    FTP_DIGIT_2(replyCode) == FTP_D2_CONNECTION) // e.g. 120 - Service ready in nnn minutes
                                {                                                // ignore this notification (we have only one timeout)
                                }
                                else
                                {
                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                        FTP_DIGIT_1(replyCode) == FTP_D1_ERROR) // e.g. 421 Service not available, closing control connection
                                    {
                                        if (state == sccsServerReady) // if we are not reporting another error yet
                                        {
                                            CopyStr(errBuf, 300, reply, replySize);
                                            fatalErrorTextID = -1;         // the error text is in 'errBuf'
                                            noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
                                            retryLogError = FALSE;         // the error is already in the log, do not add it again
                                            state = sccsRetry;
                                        }
                                    }
                                    else // unexpected response, ignore it
                                    {
                                        TRACE_E("Unexpected reply: " << CopyStr(errBuf, 300, reply, replySize));
                                    }
                                }
                            }
                        }
                        else // not an FTP server
                        {
                            if (state == sccsServerReady) // if we are not reporting another error yet
                            {
                                opFatalErrorTextID = IDS_NOTFTPSERVERERROR;
                                CopyStr(errBuf, 300, reply, replySize);
                                opFatalError = -1; // the "error" (reply) is directly in errBuf
                                state = sccsOperationFatalError;
                                fatalErrLogMsg = FALSE; // already in the log, no point adding it again
                            }
                        }
                        SkipFTPReply(replySize);
                    }
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    if (event == ccsevClosed)
                    {
                        if (state == sccsServerReady) // close without a reason
                        {
                            fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                            noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
                            state = sccsRetry;
                        }
                        if (data1 != NO_ERROR)
                        {
                            FTPGetErrorText(data1, buf, 1000 - 2); // (1000-2) so there is room for our CRLF
                            char* s = buf + strlen(buf);
                            while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                                s--;
                            strcpy(s, "\r\n"); // append our CRLF to the end of the error text line
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

        case sccsRetryLogin: // retry login (without waiting and losing the connection) - only transitions to sccsStartLoginScript - cannot be used with a proxy server!!!
        {
            state = sccsStartLoginScript;
            break;
        }

        case sccsStartLoginScript: // initialize sending of the login command sequence - according to the proxy server script
        {
            if (proxyScriptStartExecPoint == NULL)
                TRACE_E("CControlConnectionSocket::StartControlConnection(): proxyScriptStartExecPoint cannot be NULL here!");
            proxyScriptExecPoint = proxyScriptStartExecPoint;
            proxyLastCmdReply = -1;
            proxyLastCmdReplyText[0] = 0;
            state = sccsProcessLoginScript;
            break;
        }

        case sccsProcessLoginScript: // gradually send the login command sequence - according to the proxy server script
        {
            while (1)
            {
                if (sslisNone != SSLInitSequence ||
                    ProcessProxyScript(proxyScriptText, &proxyScriptExecPoint, proxyLastCmdReply,
                                       &proxyScriptParams, NULL, NULL, proxySendCmdBuf,
                                       proxyLogCmdBuf, errBuf, NULL))
                {
                    if (sslisNone == SSLInitSequence && proxyScriptParams.NeedUserInput()) // it is necessary to enter some data (user, password, etc.)
                    {
                        if (proxyScriptParams.NeedProxyHost)
                        {
                        ENTER_PROXYHOST_AGAIN:
                            if (CEnterStrDlg(parent, LoadStr(IDS_ENTERPRXHOSTTITLE), LoadStr(IDS_ENTERPRXHOSTTEXT),
                                             proxyScriptParams.ProxyHost, HOST_MAX_SIZE, FALSE, NULL, FALSE)
                                    .Execute() == IDCANCEL)
                            {
                                state = sccsDone; // user canceled -> finish
                                actionCanceled = TRUE;
                                break;
                            }
                            else // we have entered "proxyhost:port"
                            {
                                char* s = strchr(proxyScriptParams.ProxyHost, ':');
                                int portNum = 0;
                                if (s != NULL) // the port is also specified
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
                                state = sccsDone; // user canceled -> finish
                                actionCanceled = TRUE;
                                break;
                            }
                            else // values changed -> we must update the originals
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
                                state = sccsDone; // user canceled -> finish
                                actionCanceled = TRUE;
                                break;
                            }
                            else // values changed -> we must update the originals
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
                                state = sccsDone; // user canceled -> finish
                                actionCanceled = TRUE;
                                break;
                            }
                            else // values changed -> we must update the originals
                            {
                                if (proxyScriptParams.Password[0] == 0)
                                    proxyScriptParams.AllowEmptyPassword = TRUE; // empty password at the user's request (we will not ask again)
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
                                state = sccsDone; // user canceled -> finish
                                actionCanceled = TRUE;
                                break;
                            }
                            else // values changed -> we must update the originals
                            {
                                HANDLES(EnterCriticalSection(&SocketCritSect));
                                lstrcpyn(Account, proxyScriptParams.Account, ACCOUNT_MAX_SIZE);
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                        }
                        // repaint the main window (so the user does not stare at the remainder after the dialog during the whole connect)
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
                        if (proxySendCmdBuf[0] == 0) // end of the login script
                        {
                            if (proxyLastCmdReply == -1) // the script contains no command sent to the server - e.g. commands skipped because they contain optional variables
                            {
                                fatalErrorTextID = IDS_INCOMPLETEPRXSCR2;
                                state = sccsFatalError;
                            }
                            else
                            {
                                if (FTP_DIGIT_1(proxyLastCmdReply) == FTP_D1_SUCCESS) // e.g. 230 User logged in, proceed
                                {
                                    state = sccsDone;
                                    ret = TRUE; // SUCCESS, we are logged in!
                                }
                                else // FTP_DIGIT_1(proxyLastCmdReply) == FTP_D1_PARTIALSUCCESS  // e.g. 331 User name okay, need password
                                {
                                    lstrcpyn(errBuf, proxyLastCmdReplyText, 300);
                                    opFatalError = -1; // error is directly in errBuf
                                    opFatalErrorTextID = IDS_INCOMPLETEPRXSCR;
                                    state = sccsOperationFatalError;
                                }
                            }
                        }
                        else // we have a command to send to the server
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
                                    // wait for an event on the socket (server reply) or ESC
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
                                            allBytesWritten = TRUE; // no longer important, the socket will be closed
                                        }
                                        else
                                        {
                                            SalamanderGeneral->WaitForESCRelease(); // measure to prevent interrupting the next action after every ESC in the previous message box
                                            waitWnd.Show(TRUE);
                                        }
                                        break;
                                    }

                                    case ccsevTimeout:
                                    {
                                        fatalErrorTextID = IDS_SNDORABORCMDTIMEOUT;
                                        noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
                                        state = sccsRetry;
                                        allBytesWritten = TRUE; // no longer important, the socket will be closed
                                        break;
                                    }

                                    case ccsevWriteDone:
                                        allBytesWritten = TRUE; // all bytes have been sent (also handle that ccsevWriteDone could overwrite ccsevNewBytesRead)
                                    case ccsevClosed:           // possible unexpected connection loss (also handle that ccsevClosed could overwrite ccsevNewBytesRead)
                                    case ccsevNewBytesRead:     // read new bytes
                                    {
                                        char* reply;
                                        int replySize;
                                        int replyCode;

                                        HANDLES(EnterCriticalSection(&SocketCritSect));
                                        BOOL sectLeaved = FALSE;
                                        while (ReadFTPReply(&reply, &replySize, &replyCode)) // while we have some server response
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
                                                if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS ||      // e.g. 230 User logged in, proceed
                                                    FTP_DIGIT_1(replyCode) == FTP_D1_PARTIALSUCCESS) // e.g. 331 User name okay, need password
                                                {                                                    // we have a successful reply to the command, store it and continue executing the login script
                                                    fastRetry = FALSE;                               // any further "retry" will no longer be due to user delay
                                                    if (replyReceived)
                                                        TRACE_E("CControlConnectionSocket::StartControlConnection(): unexpected situation: more replies to one command!");
                                                    else
                                                    {
                                                        replyReceived = TRUE; // take only the first server reply to the command (another reply probably relates to the control connection state, e.g. "timeout")
                                                        proxyLastCmdReply = replyCode;
                                                        CopyStr(proxyLastCmdReplyText, 300, reply, replySize);
                                                    }
                                                    if (event == ccsevClosed)
                                                        state = sccsProcessLoginScript; // ensure an error is reported later
                                                }
                                                else
                                                {
                                                    if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // e.g. 421 Service not available (too many users), closing control connection
                                                        FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // e.g. 530 Not logged in (invalid password)
                                                    {
                                                        if (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR)
                                                        { // convenient handling of the "too many users" error - no questions, immediately "retry"
                                                            // drawback: with this code comes a message that may require changing user/password
                                                            retryLoginWithoutAsking = TRUE;
                                                        }

                                                        CopyStr(errBuf, 300, reply, replySize);
                                                        SkipFTPReply(replySize);
                                                        if (!retryLoginWithoutAsking)
                                                        {
                                                            fastRetry = TRUE; // user-induced delay, some servers do not like it (ftp.novell.com - kills after 20s)
                                                            BOOL proxyUsed = ProxyServer != NULL;

                                                            HANDLES(LeaveCriticalSection(&SocketCritSect)); // must leave the section - opening a dialog will take a while...
                                                            sectLeaved = TRUE;

                                                            waitWnd.Show(FALSE);
                                                            if (replyCode == 534 && EncryptDataConnection ||
                                                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR && SSLInitSequence == sslisAUTH)
                                                            {
                                                                // 534 comes after PROT when data connection encryption is requested but not supported
                                                                // 530 comes after AUTH when AUTH is not recognized
                                                                SalamanderGeneral->SalMessageBox(parent, LoadStr((SSLInitSequence == sslisAUTH) ? IDS_SSL_ERR_CONTRENCUNSUP : IDS_SSL_ERR_DATAENCUNSUP),
                                                                                                 LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONSTOP);
                                                                state = sccsDone;
                                                                allBytesWritten = TRUE;      // no longer important, the socket is going to be closed
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
                                                                        proxyScriptParams.AllowEmptyPassword = TRUE; // empty password at the user's request (we will not ask again)
                                                                    // update the originals (overwriting possible changes in another thread) + the copy in 'value'
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
                                                                    { // retry login (without waiting and closing the connection) - handles responses such as "invalid user/password"
                                                                        state = sccsRetryLogin;
                                                                        // allBytesWritten = TRUE;   // the connection will not be closed, we must wait (besides, once a reply arrived, it is likely the whole command was sent)
                                                                    }
                                                                }
                                                                else // cancel
                                                                {
                                                                    state = sccsDone;
                                                                    actionCanceled = TRUE;
                                                                    allBytesWritten = TRUE; // no longer important, the socket will be closed
                                                                }
                                                                // repaint the main window (so the user does not stare at the remainder after the dialog during the whole connect)
                                                            }
                                                            UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
                                                        }

                                                        if (state == sccsProcessLoginScript)
                                                        { // standard retry (connection closure + waiting) - handles responses like "too many users"
                                                            opFatalErrorTextID = IDS_LOGINERROR;
                                                            // CopyStr(errBuf, 300, reply, replySize);   // done earlier - before leaving the critical section
                                                            opFatalError = -1;                      // the error text is in 'errBuf'
                                                            noRetryState = sccsOperationFatalError; // if retry is not performed, execute sccsOperationFatalError
                                                            retryLogError = FALSE;                  // the error is already in the log, do not add it again
                                                            state = sccsRetry;
                                                            allBytesWritten = TRUE; // no longer important, the socket will be closed
                                                        }
                                                        // SkipFTPReply(replySize);  // done earlier - before leaving the critical section
                                                        break;
                                                    }
                                                    else // unexpected response, ignore it
                                                        TRACE_E("Unexpected reply: " << CopyStr(errBuf, 300, reply, replySize));
                                                }
                                            }
                                            else // not an FTP server
                                            {
                                                if (state == sccsProcessLoginScript) // if we are not reporting another error yet
                                                {
                                                    opFatalErrorTextID = IDS_NOTFTPSERVERERROR;
                                                    CopyStr(errBuf, 300, reply, replySize);
                                                    opFatalError = -1; // the "error" (reply) is directly in errBuf
                                                    state = sccsOperationFatalError;
                                                    fatalErrLogMsg = FALSE; // already in the log, no point adding it again
                                                    allBytesWritten = TRUE; // no longer important, the socket will be closed
                                                }
                                            }
                                            SkipFTPReply(replySize);
                                        }
                                        if (!sectLeaved)
                                            HANDLES(LeaveCriticalSection(&SocketCritSect));

                                        if (event == ccsevClosed)
                                        {
                                            allBytesWritten = TRUE;              // no longer important, the socket has been closed
                                            if (state == sccsProcessLoginScript) // close without a reason
                                            {
                                                fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                                                noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
                                                state = sccsRetry;
                                            }
                                            if (data1 != NO_ERROR)
                                            {
                                                FTPGetErrorText(data1, buf, 1000 - 2); // (1000-2) so there is room for our CRLF
                                                char* s2 = buf + strlen(buf);
                                                while (s2 > buf && (*(s2 - 1) == '\n' || *(s2 - 1) == '\r'))
                                                    s2--;
                                                strcpy(s2, "\r\n"); // append our CRLF to the end of the error text line
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
                                                                       NULL /* it's always NULL for the control connection */))
                                                    {
                                                        allBytesWritten = TRUE; // no longer important, the socket will be closed
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
                                                            noRetryState = state; // if retry is not performed, execute sccsFatalError or sccsOperationFatalError
                                                            state = sccsRetry;
                                                            retryLogError = FALSE;
                                                        }
                                                        else
                                                            fatalErrLogMsg = FALSE;
                                                    }
                                                    else
                                                    {
                                                        if (unverifiedCert != NULL) // socket is encrypted, but the server certificate is not verified; ask the user whether to trust it
                                                        {
                                                            fastRetry = TRUE; // user-induced delay, some servers do not like it (ftp.novell.com - kills after 20s)
                                                            waitWnd.Show(FALSE);

                                                            INT_PTR dlgRes;
                                                            do
                                                            {
                                                                dlgRes = CCertificateErrDialog(parent, errBuf).Execute();
                                                                switch (dlgRes)
                                                                {
                                                                case IDOK: // accept once
                                                                {
                                                                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTACCEPTED), -1, TRUE);
                                                                    SetCertificate(unverifiedCert);
                                                                    break;
                                                                }

                                                                case IDCANCEL:
                                                                {
                                                                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTREJECTED), -1, TRUE);
                                                                    allBytesWritten = TRUE; // no longer important, the socket will be closed
                                                                    state = sccsDone;
                                                                    break;
                                                                }

                                                                case IDB_CERTIFICATE_VIEW:
                                                                {
                                                                    unverifiedCert->ShowCertificate(parent);
                                                                    if (unverifiedCert->CheckCertificate(errBuf, 300))
                                                                    { // the server certificate is already trusted (the user probably imported it manually)
                                                                        Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTVERIFIED), -1, TRUE);
                                                                        dlgRes = -1; // only to terminate the loop
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
                                                    allBytesWritten = TRUE; // no longer important, the socket will be closed
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
                            else // Write error (low memory, disconnected, non-blocking "send" failure)
                            {
                                while (state == sccsProcessLoginScript)
                                {
                                    // pick an event on the socket
                                    CControlConnectionSocketEvent event;
                                    DWORD data1, data2;
                                    WaitForEventOrESC(parent, &event, &data1, &data2, 0, NULL, NULL, FALSE); // do not wait, just receive events
                                    switch (event)
                                    {
                                    // case ccsevESC:   // (the user cannot press ESC during a 0 ms timeout)
                                    case ccsevTimeout: // no message pending -> show the error from Write directly
                                    {
                                        opFatalErrorTextID = IDS_SENDCOMMANDERROR;
                                        opFatalError = error;
                                        noRetryState = sccsOperationFatalError; // if retry is not performed, execute sccsOperationFatalError
                                        state = sccsRetry;
                                        break;
                                    }

                                    case ccsevClosed:       // unexpected loss of connection (also handle that ccsevClosed could overwrite ccsevNewBytesRead)
                                    case ccsevNewBytesRead: // read new bytes (possibly an error description leading to disconnect)
                                    {
                                        char* reply;
                                        int replySize;
                                        int replyCode;

                                        HANDLES(EnterCriticalSection(&SocketCritSect));
                                        while (ReadFTPReply(&reply, &replySize, &replyCode)) // while we have some server response
                                        {
                                            Logs.LogMessage(logUID, reply, replySize);

                                            if (replyCode == -1 ||                                 // not an FTP reply
                                                FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // description of a temporary error
                                                FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // description of an error
                                            {
                                                opFatalErrorTextID = IDS_SENDCOMMANDERROR;
                                                CopyStr(errBuf, 300, reply, replySize);
                                                SkipFTPReply(replySize);
                                                opFatalError = -1;                      // the "error" (reply) is directly in errBuf
                                                noRetryState = sccsOperationFatalError; // if retry is not performed, execute sccsOperationFatalError
                                                retryLogError = FALSE;                  // the error is already in the log, do not add it again
                                                state = sccsRetry;
                                                break; // no need to read another message
                                            }
                                            SkipFTPReply(replySize);
                                        }
                                        HANDLES(LeaveCriticalSection(&SocketCritSect));

                                        if (event == ccsevClosed)
                                        {
                                            if (state == sccsProcessLoginScript) // close without a reason
                                            {
                                                fatalErrorTextID = IDS_CONNECTIONLOSTERROR;
                                                noRetryState = sccsFatalError; // if retry is not performed, execute sccsFatalError
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
                else // theoretically should never happen (saved scripts are validated)
                {
                    opFatalError = -1; // error is directly in errBuf
                    opFatalErrorTextID = IDS_ERRINPROXYSCRIPT;
                    state = sccsOperationFatalError;
                    break;
                }
            }
            break;
        }

        case sccsFatalError: // fatal error (resource ID of the text is in 'fatalErrorTextID' + if 'fatalErrorTextID' is -1, the string is directly in 'errBuf')
        {
            lstrcpyn(buf, GetFatalErrorTxt(fatalErrorTextID, errBuf), 1000);
            char* s = buf + strlen(buf);
            while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                s--;
            if (fatalErrLogMsg)
            {
                strcpy(s, "\r\n");                      // CRLF at the end of the last error text
                Logs.LogMessage(logUID, buf, -1, TRUE); // add the last error text to the log
            }
            fatalErrLogMsg = TRUE;
            *s = 0;
            SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            state = sccsDone;
            break;
        }

        case sccsOperationFatalError: // fatal operation error (resource ID of the text is in 'opFatalErrorTextID' and
        {                             // the Windows error number in 'opFatalError' + if 'opFatalError' is -1,
                                      // the string is directly in 'errBuf' + if 'opFatalErrorTextID' is -1, the
                                      // string is directly in 'formatBuf')
            const char* e = GetOperationFatalErrorTxt(opFatalError, errBuf);
            if (fatalErrLogMsg)
            {
                lstrcpyn(buf, e, 1000);
                char* s = buf + strlen(buf);
                while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                strcpy(s, "\r\n");                      // CRLF at the end of the last error text
                Logs.LogMessage(logUID, buf, -1, TRUE); // add the last error text to the log
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
        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE); // ESC (cancel) to the log

    // drop the wait cursor over the parent
    if (winParent != NULL)
    {
        winParent->DetachWindow();
        delete winParent;
    }

    // enable 'parent' again
    EnableWindow(parent, TRUE);
    // if 'parent' is active, restore the focus as well
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

    if (ret) // we are successfully connected to the FTP server
    {
        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGLOGINSUCCESS), -1, TRUE);

        if (useWelcomeMessage && welcomeMessage.Length > 0)
        { // display the "welcome message"
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
                        DestroyWindow(w->HWindow); // also deallocates 'w'
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

        // send the initial FTP commands
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
                    s++;     // strip only the first space (so the command can start with spaces)
                if (*s != 0) // if there is anything, send it to the server
                {
                    _snprintf_s(retryBuf, _TRUNCATE, "%s\r\n", s);
                    int ftpReplyCode;
                    if (!SendFTPCommand(parent, retryBuf, retryBuf, NULL, GetWaitTime(showWaitWndTime),
                                        NULL, &ftpReplyCode, NULL, 0, FALSE, TRUE, TRUE, &canRetry,
                                        errBuf, 300, NULL))
                    {
                        ret = FALSE; // we are no longer connected
                        break;       // go perform the "retry"
                    }
                }
            }
        }

        // find out the server operating system
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
                ret = FALSE; // error -> connection closed - go perform the "retry"
        }

        if (ret && workDir != NULL &&
            !GetCurrentWorkingPath(parent, workDir, workDirBufSize, FALSE, &canRetry, errBuf, 300))
        {
            ret = FALSE; // error -> connection closed - go perform the "retry"
        }

        if (canRetry && !ret) // assumes 'errBuf' has been set
        {
            opFatalErrorTextID = IDS_SENDCOMMANDERROR;
            opFatalError = -1;                      // the "error" (reply) is directly in errBuf
            noRetryState = sccsOperationFatalError; // if retry is not performed, execute sccsOperationFatalError
            retryLogError = FALSE;                  // the error is already in the log, do not add it again
            state = sccsRetry;
            useWelcomeMessage = FALSE; // we already printed it, repeating it makes no sense

            // disable 'parent' again, restore the focus when re-enabling
            EnableWindow(parent, FALSE);

            // set a wait cursor over the parent again, unfortunately we cannot do it otherwise
            winParent = new CSetWaitCursorWindow;
            if (winParent != NULL)
                winParent->AttachToWindow(parent);

            goto RETRY_LABEL; // proceed to the next attempt
        }
    }
    else
    {
        CloseSocket(NULL); // close the socket (if open); the system attempts a "graceful" shutdown (we will not learn the result)
        Logs.SetIsConnected(logUID, IsConnected());
        Logs.RefreshListOfLogsInLogsDlg(); // display "connection inactive"
    }
    if (ret)
        SetupKeepAliveTimer(); // if everything is OK, set the timer for keep-alive
    else
        ReleaseKeepAlive(); // on error release keep-alive (cannot be used without an established connection)
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
    if (retryMsg == NULL && IsConnected()) // unknown reason for disconnect + the connection is not interrupted
    {
        if (setStartTimeIfConnected)
            SetStartTime();
        return TRUE;
    }
    else
    {
        if (retryMsg == NULL) // connection interrupted for an unknown reason - display it and ask about reconnecting
        {
            // if we have not yet displayed what led to the connection closing, do it now
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
                return FALSE; // the user does not even want to try reopening the "control connection"
        }
        else // connection interrupted for a known reason - run "retry" inside StartControlConnection()
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
        for (i = ModelessDlgs.Count - 1; i >= 0; i--) // search from the end (the last window is the last in the array)
        {
            if (ModelessDlgs[i]->HWindow == OurWelcomeMsgDlg) // the window is still open, activate it
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
