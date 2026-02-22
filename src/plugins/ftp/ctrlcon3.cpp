// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

//
// ****************************************************************************
// CControlConnectionSocket
//

enum CSendFTPCmdStates // states of the automaton for CControlConnectionSocket::SendFTPCommand
{
    // sending an FTP command
    sfcsSendCommand,

    // aborting an FTP command (sending the "ABOR" command)
    sfcsAbortCommand,

    // sending the abort of an FTP command again without OOB data (sending the "ABOR" command)
    sfcsResendAbortCommand,

    // fatal error (the resource ID of the text is in 'fatalErrorTextID' + if 'fatalErrorTextID' is -1,
    // the string is directly in 'errBuf')
    sfcsFatalError,

    // fatal error of the operation (the resource ID of the text is in 'opFatalErrorTextID' and the Windows error number in
    // 'opFatalError' + if 'opFatalError' is -1, the string is directly in 'errBuf')
    sfcsOperationFatalError,

    // method finished (success or failure indicated by the TRUE/FALSE value of 'ret')
    sfcsDone
};

// **************************************************************************************
// helper object CSendCmdUserIfaceWaitWnd for CControlConnectionSocket::SendFTPCommand()

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
    if (waitWndText == NULL) // standard text of the wait window
    {
        lstrcpyn(errBuf, logCmd, 300); // trim the CRLF from the command
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
        SalamanderGeneral->WaitForESCRelease(); // prevent the next action from being interrupted by a lingering ESC state from the previous message box
        WaitWnd.Show(TRUE);
    }
    return esc;
}

// *********************************************************************************

void WriteUnexpReplyToLog(int logUID, char* unexpReply, int unexpReplyBufSize)
{ // helper function - write "unexpected reply: %s" to the log
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
    DWORD startTime = GetTickCount(); // start time of the operation
    if (canRetry != NULL)
        *canRetry = FALSE;
    if (retryMsgBufSize > 0)
        retryMsg[0] = 0;

    char buf[500];
    char errBuf[300];

    // if the CSendCmdUserIfaceWaitWnd user interface should be used, create it here
    CSendCmdUserIfaceAbstract* userIface = specialUserInterface;
    CSendCmdUserIfaceWaitWnd objSendCmdUserIfaceWaitWnd(parent); // do not allocate it (unnecessary error handling)
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
    if (parentIsEnabled) // we cannot leave the parent enabled (the wait window is not modal)
    {
        // store the focus from 'parent' (if the focus is not from 'parent', store NULL)
        focusedWnd = GetFocus();
        HWND hwnd = focusedWnd;
        while (hwnd != NULL && hwnd != parent)
            hwnd = GetParent(hwnd);
        if (hwnd != parent)
            focusedWnd = NULL;
        // disable the 'parent'; when enabling it restore the focus as well
        EnableWindow(parent, FALSE);

        // set the wait cursor over the parent, unfortunately we do not know another way
        winParent = new CSetWaitCursorWindow;
        if (winParent != NULL)
            winParent->AttachToWindow(parent);
    }

    BOOL ret = FALSE;
    int fatalErrorTextID = 0;
    int opFatalErrorTextID = 0;
    int opFatalError = 0;
    BOOL fatalErrLogMsg = TRUE; // FALSE = do not print the error message to the log (reason: it has already been printed there)
    BOOL aborting = FALSE;
    BOOL cmdReplyReceived = FALSE; // only when aborting: TRUE = the reply to the command has already arrived (we can wait for the abort reply)
    BOOL donotRetry = FALSE;       // TRUE = retry makes no sense for this error

    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // at least one second

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID; // log UID of this connection
    BOOL auxCanSendOOBData = CanSendOOBData;
    BOOL handleKeepAlive = KeepAliveMode != kamForbidden; // TRUE if keep-alive is not handled at a higher level (it must be handled here)
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    if (handleKeepAlive)
    {
        // wait for the keep-alive command to finish (if it is in progress) + set
        // keep-alive to 'kamForbidden' (a normal command is running)
        DWORD waitTime = GetTickCount() - startTime;
        WaitForEndOfKeepAlive(parent, waitTime < (DWORD)waitWndTime ? waitWndTime - waitTime : 0);
    }

    CSendFTPCmdStates state = sfcsSendCommand;
    while (state != sfcsDone)
    {
        CALL_STACK_MESSAGE2("state = %d", state); // aids debugging by showing where the automaton stalled or failed
        switch (state)
        {
        case sfcsResendAbortCommand:
        {
            state = sfcsAbortCommand; // continue processing as sfcsAbortCommand
                                      // break intentionally omitted
        }
        case sfcsAbortCommand:
        {
            if (!PrepareFTPCommand(buf, 500, errBuf, 300, ftpcmdAbort, NULL))
            { // unexpected error ("always false")
                state = sfcsDone;
                userIface->CancelDataCon();
                break;
            }

            userIface->BeforeAborting();
            aborting = TRUE;
            // break intentionally omitted (continue processing as sfcsSendCommand)
        }
        case sfcsSendCommand:
        {
            CSendFTPCmdStates sendState = state;

            DWORD error;
            BOOL allBytesWritten;

            if (aborting && auxCanSendOOBData)
            {
                // notify the server about the abort (see RFC 959 - sending the ABOR command)
                HANDLES(EnterCriticalSection(&SocketCritSect));
                if (Socket != INVALID_SOCKET &&
                    BytesToWriteCount == 0) // "always true" (all data should have been sent)
                {
                    // send the "TELNET IP" sequence (interrupt process)
                    char errTxt[100];
                    int sentLen;
                    if ((sentLen = send(Socket, "\xff\xf4" /* IAC+IP */, 2, 0)) != 2)
                    {                     // almost "always false", log the error; it might be useful for debugging problems
                        if (sentLen == 1) // the second byte was not sent, add it to 'buf' (it will be sent afterwards)
                        {
                            int ll = (int)strlen(buf);
                            if (ll > 498)
                                ll = 498;
                            memmove(buf + 1, buf, ll);
                            buf[0] = '\xf4'; // IP (IAC has already been sent)
                            buf[ll + 1] = 0;
                        }
                        DWORD err = WSAGetLastError();
                        sprintf(errTxt, "Unable to send TELNET-IP: error = %u (%d)\r\n", err, sentLen);
                        Logs.LogMessage(logUID, errTxt, -1, TRUE);
                    }

                    // send the TELNET "Synch" signal - the socket is non-blocking, so the only risk is that the TELNET
                    // "Synch" is not sent; we will not handle this error (it is unlikely and unimportant)
                    if (sentLen == 2 && // only if IAC+IP was sent successfully (otherwise it makes no sense)
                        (sentLen = send(Socket, "\xF2" /* DM */, 1, MSG_OOB)) != 1)
                    { // almost "always false", log the error; it might be useful for debugging problems
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
            if (!aborting) // try to skip extra server replies (they should not exist at all, but unfortunately they do - WarFTPD generates "550 access denied" twice after listing a directory the user cannot access)
            {
                char* reply;
                int replySize;
                int replyCode;

                HANDLES(EnterCriticalSection(&SocketCritSect));
                while (ReadFTPReply(&reply, &replySize, &replyCode)) // as long as we have any server reply
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
                if (unexpReply[0] != 0) // the write succeeded, we no longer need the unexpected reply -> write it to the log and discard it
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
                    // wait for an event on the socket (server reply) or ESC
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
                            if (allowCmdAbort && state == sfcsSendCommand) // cancel for the command -> start aborting the command
                            {
                                state = sfcsAbortCommand;
                                // allBytesWritten = TRUE;   // we must wait until the command is sent before starting to send the abort
                                // we will not display the wait window again; in theory sending should never stall -> ignore it (the user will wait without the wait window)
                            }
                            else // cannot use abort (we must close the connection) or cancel while aborting the command
                            {
                                state = sfcsDone;
                                isCanceled = TRUE;
                                allBytesWritten = TRUE;                                               // no longer important now, the socket will be closed
                                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGACTIONCANCELED), -1, TRUE); // ESC (cancel) to the log
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
                            allBytesWritten = TRUE; // no longer important now, the socket will be closed
                        }
                        break;
                    }

                    case ccsevWriteDone:
                        allBytesWritten = TRUE; // all bytes have already been sent (also handle that ccsevWriteDone could overwrite ccsevNewBytesRead)
                    case ccsevClosed:           // possible unexpected loss of connection (also handle that ccsevClosed could overwrite ccsevNewBytesRead)
                    case ccsevNewBytesRead:     // new bytes have been read
                    {
                        char* reply;
                        int replySize;
                        int replyCode;

                        HANDLES(EnterCriticalSection(&SocketCritSect));
                        while (ReadFTPReply(&reply, &replySize, &replyCode)) // as long as we have any server reply
                        {
                            Logs.LogMessage(logUID, reply, replySize);

                            if (state != sfcsFatalError && state != sfcsOperationFatalError && // only if we do not already have another error
                                replyCode == -1)                                               // not an FTP reply, we are done
                            {
                                opFatalErrorTextID = IDS_NOTFTPSERVERERROR;
                                allBytesWritten = TRUE; // no longer important now, the socket will be closed
                                CopyStr(errBuf, 300, reply, replySize);
                                opFatalError = -1; // the "error" (reply) is directly in errBuf
                                state = sfcsOperationFatalError;
                                fatalErrLogMsg = FALSE; // it is already in the log, no point adding it again
                                donotRetry = TRUE;      // retry makes no sense
                                SkipFTPReply(replySize);
                                break;
                            }

                            if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS)
                            { // replies of type FTP_D1_MAYBESUCCESS are logged only (we are waiting for the server's "last word")
                                if (event != ccsevClosed)
                                {
                                    if (!aborting) // send command
                                    {              // state can also be sfcsAbortCommand; then we must abort the command even if it succeeded just now (the user ordered an abort)
                                        if (state != sfcsAbortCommand)
                                        {
                                            state = sfcsDone;
                                            *ftpReplyCode = replyCode;
                                            CopyStr(ftpReplyBuf, ftpReplyBufSize, reply, replySize);
                                            ret = TRUE; // SUCCESS, we have the server's reply! (for sending we only care about a single server reply)
                                        }
                                        // else; // continue waiting for the abort command (we have not sent ABOR yet)
                                        SkipFTPReply(replySize);
                                        break;
                                    }
                                    else // abort command
                                    {
                                        if (auxCanSendOOBData &&                      // we were sending OOB data
                                            FTP_DIGIT_1(replyCode) == FTP_D1_ERROR && // the reply is a syntax error
                                            FTP_DIGIT_2(replyCode) == FTP_D2_SYNTAX)
                                        {                                               // the server probably does not understand OOB data and inserted them directly into the data stream (the server does not know the "\xF2ABOR" command)
                                            auxCanSendOOBData = CanSendOOBData = FALSE; // do not try OOB again on this "control connection"
                                            state = sfcsResendAbortCommand;
                                            SkipFTPReply(replySize);
                                            break;
                                        }
                                        else
                                        {
                                            // this is the reply for the sent or aborted command; we will still try to read
                                            // additional server replies (some servers send one more for ABOR)
                                            if (!cmdReplyReceived) // return the first reply (it should belong to the command but it may be
                                            {                      // for ABOR as well) - ignore any possible second reply (to ABOR)
                                                state = sfcsDone;
                                                *ftpReplyCode = replyCode;
                                                CopyStr(ftpReplyBuf, ftpReplyBufSize, reply, replySize);
                                                if (cmdAborted != NULL)
                                                    *cmdAborted = TRUE;
                                                ret = TRUE; // SUCCESS, we have the server's reply to the command/abort
                                                cmdReplyReceived = TRUE;
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    if (state != sfcsFatalError && state != sfcsOperationFatalError &&
                                        (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                                         FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)) // e.g. 421 Service not available, closing control connection
                                    {                                             // discard all server replies one by one, log the first error we find
                                        CopyStr(errBuf, 300, reply, replySize);
                                        fatalErrorTextID = -1; // the error text is in 'errBuf'
                                        state = sfcsFatalError;
                                        allBytesWritten = TRUE; // no longer important now, the socket will be closed
                                        fatalErrLogMsg = FALSE; // it is already in the log, no point adding it again
                                    }
                                }
                            }
                            else
                            {
                                userIface->MaybeSuccessReplyReceived(reply, replySize); // passive mode: try to encrypt the data connection (only if the command uses it)
                            }
                            SkipFTPReply(replySize);
                        }
                        HANDLES(LeaveCriticalSection(&SocketCritSect));

                        if (event == ccsevClosed)
                        {
                            allBytesWritten = TRUE; // no longer important now, the socket was closed
                            if (state == sfcsSendCommand || state == sfcsAbortCommand || state == sfcsResendAbortCommand)
                            { // close without a cause (whether during/after send or before/during/after abort)
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
                { // the server reports "done", we will still wait for the user interface ("data connection") to close
                    BOOL calledBeforeWaitingForFinish = FALSE;
                    BOOL useTimeout = FALSE;    // TRUE = use the 'serverTimeout2' timeout while waiting for the data connection to finish
                    int serverTimeout2 = 10000; // timeout for finishing the data connection when LIST returns an error or the connection has not been opened yet is 10 seconds
                    DWORD start2 = GetTickCount();
                    while (!userIface->CanFinishSending(*ftpReplyCode, &useTimeout))
                    {
                        if (!calledBeforeWaitingForFinish) // call only the first time
                        {
                            DWORD waitTime2 = GetTickCount() - startTime;
                            userIface->BeforeWaitingForFinish(*ftpReplyCode, &useTimeout);
                            calledBeforeWaitingForFinish = TRUE;
                        }

                        // wait for the user interface to close, for a timeout, or for ESC
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
                            break; // CanFinishSending() will hopefully return TRUE now

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

                    // check whether the control connection closed while finishing reading the data connection
                    if (!IsConnected())
                    {
                        HANDLES(EnterCriticalSection(&EventCritSect));
                        DWORD error2 = NO_ERROR;
                        int i;
                        for (i = 0; i < EventsUsedCount; i++) // check whether the ccsevClosed event is present
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
                        ret = FALSE; // the control connection is closed
                    }
                }
                else
                {
                    if (state != sfcsResendAbortCommand && state != sfcsAbortCommand)
                        userIface->CancelDataCon();
                }

                userIface->SendingFinished();
            }
            else // Write error (low memory, disconnected, non-blocking "send" error)
            {
                if (aborting)
                    userIface->CancelDataCon();
                while (state == sendState)
                {
                    // pick an event on the socket
                    CControlConnectionSocketEvent event;
                    DWORD data1, data2;
                    WaitForEventOrESC(parent, &event, &data1, &data2, 0, NULL, NULL, FALSE); // do not wait, just collect events
                    switch (event)
                    {
                    // case ccsevESC:   // (the user cannot press ESC during a 0 ms timeout)
                    case ccsevTimeout: // no message is waiting -> display the error from Write directly
                    {
                        opFatalErrorTextID = !aborting ? IDS_SENDCOMMANDERROR : IDS_ABORTCOMMANDERROR;
                        opFatalError = error;
                        state = sfcsOperationFatalError;
                        break;
                    }

                    case ccsevClosed:       // unexpected loss of connection (also handle that ccsevClosed could overwrite ccsevNewBytesRead)
                    case ccsevNewBytesRead: // read new bytes (possibly the error description that caused the disconnect)
                    {
                        char* reply;
                        int replySize;
                        int replyCode;

                        BOOL done = FALSE;
                        if (unexpReply[0] != 0)
                        {
                            Logs.LogMessage(logUID, unexpReply, -1);

                            if (unexpReplyCode == -1 ||                                 // not an FTP reply
                                FTP_DIGIT_1(unexpReplyCode) == FTP_D1_TRANSIENTERROR || // description of a temporary error
                                FTP_DIGIT_1(unexpReplyCode) == FTP_D1_ERROR)            // description of an error
                            {
                                opFatalErrorTextID = !aborting ? IDS_SENDCOMMANDERROR : IDS_ABORTCOMMANDERROR;
                                lstrcpyn(errBuf, unexpReply, 300);
                                opFatalError = -1;      // the "error" (reply) is directly in errBuf
                                fatalErrLogMsg = FALSE; // the error is already in the log, do not add it again
                                state = sfcsOperationFatalError;
                                done = TRUE; // no need to read another message
                            }
                        }

                        if (!done)
                        {
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            while (ReadFTPReply(&reply, &replySize, &replyCode)) // as long as we have any server reply
                            {
                                Logs.LogMessage(logUID, reply, replySize);

                                if (replyCode == -1 ||                                 // not an FTP reply
                                    FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR || // description of a temporary error
                                    FTP_DIGIT_1(replyCode) == FTP_D1_ERROR)            // description of an error
                                {
                                    opFatalErrorTextID = !aborting ? IDS_SENDCOMMANDERROR : IDS_ABORTCOMMANDERROR;
                                    CopyStr(errBuf, 300, reply, replySize);
                                    SkipFTPReply(replySize);
                                    opFatalError = -1;      // the "error" (reply) is directly in errBuf
                                    fatalErrLogMsg = FALSE; // the error is already in the log, do not add it again
                                    state = sfcsOperationFatalError;
                                    break; // no need to read another message
                                }
                                SkipFTPReply(replySize);
                            }
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }

                        if (event == ccsevClosed)
                        {
                            if (state == sendState) // close without a cause
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

        case sfcsFatalError: // fatal error (the resource ID of the text is in 'fatalErrorTextID' + if 'fatalErrorTextID' is -1, the string is directly in 'errBuf')
        {
            lstrcpyn(buf, GetFatalErrorTxt(fatalErrorTextID, errBuf), 500);
            char* s = buf + strlen(buf);
            while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                s--;
            if (fatalErrLogMsg)
            {
                strcpy(s, "\r\n");                      // CRLF at the end of the last error text
                Logs.LogMessage(logUID, buf, -1, TRUE); // add the last error text to the log
            }
            fatalErrLogMsg = TRUE;
            if (canRetry == NULL || donotRetry) // "retry" is not possible or does not make sense
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

        case sfcsOperationFatalError: // fatal error of the operation (the resource ID of the text is in 'opFatalErrorTextID' and
        {                             // the Windows error number is in 'opFatalError' + if 'opFatalError' is -1,
                                      // the string is directly in 'errBuf'
            const char* e = GetOperationFatalErrorTxt(opFatalError, errBuf);
            if (fatalErrLogMsg)
            {
                lstrcpyn(buf, e, 500);
                char* s = buf + strlen(buf);
                while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                strcpy(s, "\r\n");                      // CRLF at the end of the last error text
                Logs.LogMessage(logUID, buf, -1, TRUE); // add the last error text to the log
            }
            fatalErrLogMsg = TRUE;

            if (canRetry == NULL || donotRetry) // "retry" is not possible or does not make sense
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

    if (parentIsEnabled) // if we disabled the parent, enable it again
    {
        // remove the wait cursor over the parent
        if (winParent != NULL)
        {
            winParent->DetachWindow();
            delete winParent;
        }

        // enable the 'parent'
        EnableWindow(parent, TRUE);
        // if the 'parent' is active, restore focus as well
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
        ResetWorkingPathCache(); // if a change to the working path is likely, reset the cache
    if (resetCurrentTransferModeCache)
        ResetCurrentTransferModeCache(); // if a change to the transfer mode is likely, reset the cache

    if (ret) // the connection is OK, no timeout occurred
    {
        if (handleKeepAlive)
        {
            // if everything is OK, set up the keep-alive timer
            SetupKeepAliveTimer();
        }
    }
    else // connection interrupted or timeout (the socket cannot be used anymore)
    {
        CloseSocket(NULL); // close the socket (if it is open); the system will attempt a "graceful" shutdown (we will not learn the result)
        Logs.SetIsConnected(logUID, IsConnected());
        Logs.RefreshListOfLogsInLogsDlg(); // "connection inactive" notification

        if (handleKeepAlive)
        {
            // release keep-alive, it is no longer needed (the connection is no longer established)
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

        // determine the working directory on the server
        PrepareFTPCommand(cmdBuf, 50, logBuf, 50, ftpcmdPrintWorkingPath, NULL); // cannot fail
        int ftpReplyCode;
        if (SendFTPCommand(parent, cmdBuf, logBuf, NULL, GetWaitTime(WAITWND_COMOPER), NULL,
                           &ftpReplyCode, replyBuf, 700, FALSE, FALSE, FALSE, canRetry,
                           retryMsg, retryMsgBufSize, NULL))
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS && // success is returned (should be 257)
                    FTPGetDirectoryFromReply(replyBuf, (int)strlen(replyBuf), WorkingPath, FTP_MAX_PATH) ||
                FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS) // failure returned (e.g. "not defined; use CWD to set the working directory") -> temporarily use an empty path
            {
                if (FTP_DIGIT_1(ftpReplyCode) != FTP_D1_SUCCESS)
                    WorkingPath[0] = 0; // temporarily use an empty path
                leaveSect = TRUE;
                HaveWorkingPath = TRUE; // we have the working directory
            }
            else // fatal error, cannot determine the working directory; close the connection and return an error
            {
                int logUID = LogUID; // log UID of this connection
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                CloseSocket(NULL); // close the socket (if it is open); the system will attempt a "graceful" shutdown (we will not learn the result)
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg(); // "connection inactive" notification
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGFATALERROR), -1, TRUE);

                ReleaseKeepAlive(); // on error release keep-alive (cannot be used without an established connection)

                sprintf(errBuf, LoadStr(IDS_GETCURWORKPATHERROR), replyBuf);
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
        }
        // else; // error -> connection closed
    }

    if (leaveSect) // HaveWorkingPath == TRUE at the same time
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

            BOOL needChangeDir = i == 0 && reconnected && startPath != NULL; // after reconnect try to set 'startPath' again
            if (i == 0 && !reconnected && startPath != NULL)                 // we have been connected for a while, check
            {                                                                // whether the working directory matches 'startPath'
                // use the cache; under normal circumstances the path should be there
                if (GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, FALSE, &canRetry, retryMsgBuf, 300))
                {
                    if (strcmp(newPath, startPath) != 0) // the working directory on the server differs - change required
                        needChangeDir = TRUE;            // (assumption: the server always returns the same working path string)
                }
                else
                {
                    if (canRetry) // "retry" is allowed
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    break; // connection closed, terminate the inner loop
                }
            }

            if (needChangeDir)
            { // restored connection + relative path -> first change to the absolute path we base it on
                p = startPath;
            }
            else
            {
                p = path;
                i = 1; // end of the loop
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
                        {               // failure (the absolute path we base it on does not exist)
                            ret = TRUE; // the change happened (with an error)
                            lstrcpyn(ftpReplyBuf, replyBuf, ftpReplyBufSize);
                            break; // failure -> finish
                        }
                    }
                    else
                    {
                        ret = TRUE; // the change happened (successfully or with an error)
                        lstrcpyn(ftpReplyBuf, replyBuf, ftpReplyBufSize);
                        if (FTP_DIGIT_1(ftpReplyCode) == FTP_D1_SUCCESS) // success is returned (should be 250)
                            *success = TRUE;
                    }
                }
                else
                {
                    if (canRetry) // "retry" is allowed
                    {
                        run = TRUE;
                        retryMsgAux = retryMsgBuf;
                    }
                    break; // connection closed, terminate the inner loop
                }
            }
            else // unexpected error ("always false") -> close the connection
            {
                TRACE_E("Unexpected situation in CControlConnectionSocket::SendChangeWorkingPath() - small buffer for command!");

                HANDLES(EnterCriticalSection(&SocketCritSect));
                int logUID = LogUID; // log UID of this connection
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                CloseSocket(NULL); // close the socket (if it is open); the system will attempt a "graceful" shutdown (we will not learn the result)
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.RefreshListOfLogsInLogsDlg(); // "connection inactive" notification
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGFATALERROR2), -1, TRUE);

                ReleaseKeepAlive(); // on error release keep-alive (cannot be used without an established connection)

                break;
            }
        }
        if (!run)
            break; // end the loop if there was no connection interruption that needs to be restored
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

    // first phase: estimate the path text
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
        if (path[0] == 0) // only right after connecting - otherwise the path change (Shift+F7) to
        {                 // a relative path (e.g. "ftp://localhost") is ignored
                          // + when optimizing ChangePath() called right after obtaining the working directory
            // forceRefresh should be only FALSE + it is always preceded by a GetCurrentWorkingPath() call -> it should always
            // take the path from the cache (does not touch the connection)
            ret = GetCurrentWorkingPath(parent, path, pathBufSize, forceRefresh, NULL, NULL, 0);
            if (ret)
            {
                donotTestPath = TRUE; // no point testing the path obtained from the server - list it right away
                pathType = GetFTPServerPathType(path);
            }
        }
        else
        {
            if (parsedPath &&                    // except for connecting from the "Connect to FTP Server" dialog it is always TRUE
                (*path == '/' || *path == '\\')) // 'path' always starts with '/' or '\\' ("always true")
            {
                pathType = GetFTPServerPathType(path + 1);
                if (pathType == ftpsptOpenVMS || pathType == ftpsptMVS || pathType == ftpsptIBMz_VM ||
                    pathType == ftpsptOS2 && GetFTPServerPathType("") == ftpsptOS2) // OS/2 paths get confused with the Unix path "/C:/path", so distinguish OS/2 paths even just by the SYST reply
                {                                                                   // VMS + MVS + IBM_z/VM + OS/2 do not have '/' or '\\' at the start of the path
                    memmove(path, path + 1, strlen(path) + 1);                      // remove the '/' or '\\' character from the start of the path
                    if (path[0] == 0)                                               // generic root -> supplement it according to the system type
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
                                        lstrcpyn(path, "/", pathBufSize); // the tested server supported the Unix root "/"; maybe someone will report it, then we will handle it further...
                                    }
                                }
                                else
                                {
                                    if (pathType == ftpsptOS2)
                                    {
                                        if (rescuePath[0] == 0 || !FTPGetOS2RootPath(path, pathBufSize, rescuePath))
                                        {
                                            lstrcpyn(path, "/", pathBufSize); // try at least the Unix root "/", we know nothing else; maybe someone will report it, then we will handle it further...
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
                        { // try whether it is a file name (with VMS it is distinguished by syntax)
                            lstrcpyn(prevUsedPath, path, FTP_MAX_PATH);
                            BOOL fileNameCouldBeCut;
                            if (FTPCutDirectory(pathType, prevUsedPath, FTP_MAX_PATH, newPath,
                                                FTP_MAX_PATH, &fileNameCouldBeCut) &&
                                fileNameCouldBeCut) // with VMS, 'fileNameCouldBeCut' == TRUE means it is definitely a file
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
    if (ret && showChangeInLog && // if it should be shown in the log
        !cutDirectory)            // only on the first pass (do not report shortening due to a faulty listing)
    {
        _snprintf_s(errBuf, _TRUNCATE, LoadStr(forceRefresh ? IDS_LOGMSGREFRESHINGPATH : IDS_LOGMSGCHANGINGPATH), path);
        LogMessage(errBuf, -1, TRUE);
    }

    prevUsedPath[0] = 0;
    if (ret && cutDirectory) // still OK + the path should be shortened before use (if the path could not be listed)
    {
        if (donotTestPath)
            donotTestPath = FALSE;                  // path change - we must test the modified one
        lstrcpyn(prevUsedPath, path, FTP_MAX_PATH); // remember the previous path on the server (returned by the server)
        if (!FTPCutDirectory(pathType, path, pathBufSize, NULL, 0, NULL))
        {
            if (rescuePath[0] != 0) // try the rescue path as well
            {
                lstrcpyn(path, rescuePath, pathBufSize);
                pathType = GetFTPServerPathType(path);
                rescuePath[0] = 0; // do not try it next time (avoid loops)
            }
            else // no need to report any error (listing already reported an error); we quietly tried
            {    // to find an accessible path (which did not work)
                ret = FALSE;
            }
        }
        if (ret) // either shortened or another path, in any case it is not the requested path
        {
            fileNameAlreadyCut = TRUE;
            if (pathWasCut != NULL)
                *pathWasCut = TRUE;
        }
    }

    // second phase: find on the server the requested or the closest matching path that
    //               is either cached or accessible
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
        BOOL resuscitateKeepAlive = (IsConnected() && KeepAliveEnabled && KeepAliveMode == kamNone); // if keep-alive has already turned off (revival time expired), we must restart it
        KeepAliveStart = GetTickCount();                                                             // beware, it is not enough to do it simply; 'resuscitateKeepAlive' must be used
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (donotTestPath)
        {
            if (useListingsCacheAux && !forceRefresh && // the user wants to use the cache and this is not a hard refresh
                ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType, path, pathBufSize,
                                            listCmd, isFTPS, cachedListing, cachedListingLen,
                                            cachedListingDate, cachedListingStartTime) &&
                *cachedListing == NULL)
            {
                ret = FALSE; // the listing is in the cache, but there is not enough memory to allocate it -> fatal error
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
                BOOL inCache = useListingsCacheAux && !forceRefresh && // the user wants to use the cache and this is not a hard refresh
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
                    ret = FALSE;   // the listing is in the cache, but there is not enough memory to allocate it
                    errBuf[0] = 0; // any possible message is unnecessary; the printed error would only be confusing
                    break;         // fatal error
                }
                if (!inCache) // the listing is not in the cache (or we must not use it)
                {
                    resuscitateKeepAlive = FALSE; // the connection will be touched, keep-alive will revive automatically

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
                        if (success) // the server reports success - retrieve the new path
                        {
                            if (GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, forceRefresh,
                                                      &canRetry, retryMsgBuf, 300))
                            {
                                if (cutDirectory && strcmp(newPath, prevUsedPath) == 0)
                                { // the server is making fools of us (does not change the path but claims it did)
                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
                                }
                                else
                                {
                                    lstrcpyn(path, newPath, pathBufSize);       // take over the new path from the server
                                    if (useListingsCacheAux && !forceRefresh && // the user wants to use the cache and this is not a hard refresh
                                        strcmp(path, pathSearchedInCache) != 0) // if we have not searched for this path in the cache yet, try it
                                    {
                                        pathType = GetFTPServerPathType(path);
                                        if (ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType,
                                                                        path, pathBufSize, listCmd, isFTPS,
                                                                        cachedListing, cachedListingLen,
                                                                        cachedListingDate,
                                                                        cachedListingStartTime) &&
                                            *cachedListing == NULL)
                                        {                  // fatal error
                                            ret = FALSE;   // the listing is in the cache, but there is not enough memory to allocate it
                                            errBuf[0] = 0; // any possible message is unnecessary; the printed error would only be confusing
                                        }
                                    }
                                    break; // fatal error or success (path changed) + possibly: the path is cached, take the listing from the cache
                                }
                            }
                            else
                            {
                                if (canRetry) // "retry" is allowed
                                {
                                    retryMsgAux = retryMsgBuf;

                                    goto TRY_CHANGE_AGAIN;
                                }

                                ret = FALSE;
                                errBuf[0] = 0; // the user has already received the fatal error message; another message is unnecessary
                                break;         // fatal error - the connection is already closed
                            }
                        }
                        else // error, generate a message
                        {
                            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
                        }

                        if (strcmp(rescuePath, path) == 0)
                            rescuePath[0] = 0; // path error identical to 'rescuePath' -> 'rescuePath' is no longer meaningful

                        // try to shorten the path (a successful path change would not reach this point)
                        BOOL fileNameCouldBeCut;
                        if (!FTPCutDirectory(pathType, path, pathBufSize, newPath, FTP_MAX_PATH, &fileNameCouldBeCut))
                        {
                            if (rescuePath[0] != 0) // try the rescue path as well
                            {
                                lstrcpyn(path, rescuePath, pathBufSize);
                                fileNameAlreadyCut = TRUE;
                                pathType = GetFTPServerPathType(path);
                                rescuePath[0] = 0; // do not try it next time (avoid loops)
                            }
                            else
                            {
                                // even if we did not find any accessible path, we do not want a disconnect, therefore do the following:
                                if (GetCurrentWorkingPath(parent, newPath, FTP_MAX_PATH, TRUE,
                                                          &canRetry, retryMsgBuf, 300))
                                {
                                    if (newPath[0] == 0) // we care only about the case when there is no current path on the server (otherwise we should not get here - rescuePath would be used)
                                    {
                                        if (pathWasCut != NULL)
                                            *pathWasCut = TRUE; // we are on a path other than the requested one
                                        if (pathBufSize > 0)
                                            path[0] = 0; // there is no current path on the server
                                        break;           // go try listing...
                                    }
                                }
                                else
                                {
                                    if (canRetry) // "retry" is allowed
                                    {
                                        retryMsgAux = retryMsgBuf;

                                        goto TRY_CHANGE_AGAIN;
                                    }

                                    ret = FALSE;
                                    errBuf[0] = 0; // the user has already received the fatal error message; another message is unnecessary
                                    break;         // fatal error - the connection is already closed
                                }

                                ret = FALSE; // report the last error (in all types of 'mode')
                                break;       // fatal error (no accessible path exists on the FS) - leave the connection open
                            }
                        }
                        if (fileNameCouldBeCut && !fileNameAlreadyCut && mode == 3) // first shortening -> it may be a file name
                        {
                            errBuf[0] = 0; // in 'mode' 3 this is not reported as an error (we are trying to focus on the file)
                            if (cutFileName != NULL)
                                lstrcpyn(cutFileName, newPath, MAX_PATH);
                        }
                        else
                        {
                            if (cutFileName != NULL)
                                *cutFileName = 0; // it can no longer be a file name
                        }
                        fileNameAlreadyCut = TRUE;
                        if (pathWasCut != NULL)
                            *pathWasCut = TRUE;
                        if (mode == 1)
                            errBuf[0] = 0; // in 'mode' 1 only root errors are reported
                    }
                    else
                    {
                        // if this is a hard refresh and the user refused reconnect and the path has a cached listing,
                        // use the cached listing (the user knows they answered "NO" to reconnect, so they will not expect
                        // a refreshed listing)
                        if (firstRound && userRejectsReconnect && useListingsCacheAux && forceRefresh)
                        {
                            inCache = ListingCache.GetPathListing(hostTmp, portTmp, userBuf, pathType,
                                                                  path, pathBufSize, listCmd, isFTPS,
                                                                  cachedListing, cachedListingLen,
                                                                  cachedListingDate,
                                                                  cachedListingStartTime);
                            if (inCache && *cachedListing == NULL)
                            {
                                ret = FALSE;   // the listing is in the cache, but there is not enough memory to allocate it
                                errBuf[0] = 0; // any possible message is unnecessary; the printed error would only be confusing
                                break;         // fatal error
                            }
                            if (inCache)
                                break; // the path is cached - do not check whether it still exists, take the listing from the cache
                        }

                        ret = FALSE;
                        errBuf[0] = 0; // the user has already received the fatal error message; another message is unnecessary
                        break;         // fatal error - the connection is already closed
                    }
                    skipFirstReconnectIfNeeded = TRUE; // the next SendChangeWorkingPath() call follows the previous successful SendChangeWorkingPath() call
                }
                else
                    break; // the path is cached - do not check whether it still exists, take the listing from the cache
            }
            if (totalAttemptNum != NULL)
                *totalAttemptNum = attemptNum;
            if (errBuf[0] != 0) // if we have an error message, display it here
            {
                SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
            }
        }
        // it should be revived and the listing is from the cache (otherwise it must be revived on the first sent command)
        if (resuscitateKeepAlive && *cachedListing != NULL)
        {
            WaitForEndOfKeepAlive(parent, 0); // the wait window cannot be shown
            SetupKeepAliveTimer(TRUE);
        }
    }

    if (ret && strcmp(rescuePath, path) == 0)
        rescuePath[0] = 0; // trying a path identical to 'rescuePath' -> 'rescuePath' is no longer meaningful
    return ret;
}

void CControlConnectionSocket::GiveConnectionToWorker(CFTPWorker* newWorker, HWND parent)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GiveConnectionToWorker(,)");

    parent = FindPopupParent(parent);

    if (IsConnected()) // only if the connection is open
    {
        // first stop keep-alive:
        // store the focus from 'parent' (if the focus is not from 'parent', store NULL)
        HWND focusedWnd = GetFocus();
        HWND hwnd = focusedWnd;
        while (hwnd != NULL && hwnd != parent)
            hwnd = GetParent(hwnd);
        if (hwnd != parent)
            focusedWnd = NULL;
        // disable the 'parent'; when enabling it restore the focus as well
        EnableWindow(parent, FALSE);

        // set the wait cursor over the parent, unfortunately we do not know another way
        CSetWaitCursorWindow* winParent = new CSetWaitCursorWindow;
        if (winParent != NULL)
            winParent->AttachToWindow(parent);

        // wait for the keep-alive command to finish (if it is currently running) + set
        // keep-alive to 'kamForbidden' (normal commands will run)
        WaitForEndOfKeepAlive(parent, WAITWND_CONTOOPER);

        // remove the wait cursor over the parent
        if (winParent != NULL)
        {
            winParent->DetachWindow();
            delete winParent;
        }

        // enable the 'parent'
        EnableWindow(parent, TRUE);
        // if the 'parent' is active, restore the focus as well
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

        // after stopping keep-alive we can hand over the active "control connection" to the worker
        // (the timer and post-socket message associated with keep-alive have been cleared/delivered)
        if (IsConnected()) // only if the connection is open even after the keep-alive command finishes
        {
            // swap sockets and the object's internal data related to the socket (read/write buffers, etc.)
            SocketsThread->BeginSocketsSwap(this, newWorker);
            // This check is rather complicated for sanity purposes: pCertificate is always NULL
            if (pCertificate)
                pCertificate->Release();
            pCertificate = newWorker->GetCertificate(); // Keep the certificate
            newWorker->RefreshCopiesOfUIDAndMsg();      // refresh copies of UID+Msg (they changed)
            BOOL ok = newWorker->IsConnected();
            if (ok) // paranoid check: the connection might still drop between IsConnected() and SocketsThread->BeginSocketsSwap(), swapping would make no sense then
            {
                HANDLES(EnterCriticalSection(&newWorker->WorkerCritSect));
                newWorker->SocketClosed = FALSE;      // the socket is no longer closed; we are taking over the socket from the panel
                newWorker->ConnectAttemptNumber = 1;  // the connection is established, so this must be one
                int workerLogUID = newWorker->LogUID; // log UID of this worker
                newWorker->ErrorDescr[0] = 0;         // start collecting error messages
                HANDLES(LeaveCriticalSection(&newWorker->WorkerCritSect));

                HANDLES(EnterCriticalSection(&SocketCritSect));
                HANDLES(EnterCriticalSection(&newWorker->SocketCritSect));

                // pass the worker information about the connection and the socket data
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
                int logUID = LogUID; // log UID of this connection

                HANDLES(LeaveCriticalSection(&newWorker->SocketCritSect));
                HANDLES(LeaveCriticalSection(&SocketCritSect));

                ResetBuffersAndEvents(); // clear the event queue (it should contain only ccsevNewBytesRead)

                // inform the log that the "control connection" has been handed to the worker (and is thus "inactive")
                Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGCONINWORKER), -1, TRUE);
                Logs.SetIsConnected(logUID, IsConnected());
                Logs.LogMessage(workerLogUID, LoadStr(IDS_LOGMSGWORKERUSECON), -1, TRUE);
                Logs.SetIsConnected(workerLogUID, newWorker->IsConnected());
                Logs.RefreshListOfLogsInLogsDlg();
            }
            else // if swapping makes no sense, restore the objects to their original state
            {
                SocketsThread->BeginSocketsSwap(this, newWorker);
                newWorker->RefreshCopiesOfUIDAndMsg(); // refresh copies of UID+Msg (they changed)
                SocketsThread->EndSocketsSwap();
            }
            SocketsThread->EndSocketsSwap();
        }

        // release keep-alive, it is no longer needed (the connection is no longer established)
        ReleaseKeepAlive();
    }
}

void CControlConnectionSocket::GetConnectionFromWorker(CFTPWorker* workerWithCon)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GetConnectionFromWorker()");

    if (!IsConnected() && workerWithCon->IsConnected()) // only if the connection is not open and the worker has an open connection
    {
        // swap sockets and the object's internal data related to the socket (read/write buffers, etc.)
        SocketsThread->BeginSocketsSwap(this, workerWithCon);
        workerWithCon->RefreshCopiesOfUIDAndMsg(); // refresh copies of UID+Msg (they changed)
        BOOL ok = IsConnected();
        if (ok) // paranoid check: the connection might still drop between workerWithCon->IsConnected() and SocketsThread->BeginSocketsSwap(); swapping would make no sense then
        {
            HANDLES(EnterCriticalSection(&workerWithCon->WorkerCritSect));
            workerWithCon->SocketClosed = TRUE;       // the socket is no longer open; we are taking over the closed socket from the panel
            int workerLogUID = workerWithCon->LogUID; // log UID of this worker
            workerWithCon->ErrorDescr[0] = 0;         // handing over the connection is not an error
            HANDLES(LeaveCriticalSection(&workerWithCon->WorkerCritSect));

            HANDLES(EnterCriticalSection(&SocketCritSect));
            HANDLES(EnterCriticalSection(&workerWithCon->SocketCritSect));

            // take over from the worker the information about the connection and the socket data
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
            int logUID = LogUID; // log UID of this connection

            HANDLES(LeaveCriticalSection(&workerWithCon->SocketCritSect));
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            workerWithCon->ResetBuffersAndEvents(); // clear the event queue (it should contain only ccsevNewBytesRead)

            // inform the log that the "control connection" has been taken from the worker (and is therefore "active" again)
            Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGCONFROMWORKER), -1, TRUE);
            Logs.SetIsConnected(logUID, IsConnected());
            Logs.LogMessage(workerLogUID, LoadStr(IDS_LOGMSGWORKERRETCON), -1, TRUE);
            Logs.SetIsConnected(workerLogUID, workerWithCon->IsConnected());
            Logs.RefreshListOfLogsInLogsDlg(); // "connection active" notification
        }
        else // if swapping makes no sense, restore the objects to their original state
        {
            SocketsThread->BeginSocketsSwap(this, workerWithCon);
            workerWithCon->RefreshCopiesOfUIDAndMsg(); // refresh copies of UID+Msg (they changed)
            SocketsThread->EndSocketsSwap();
        }
        SocketsThread->EndSocketsSwap();

        if (ok)
        {
            // restart keep-alive
            ReleaseKeepAlive();
            WaitForEndOfKeepAlive(SalamanderGeneral->GetMsgBoxParent(), 0); // cannot open the wait window (it is in state 'kamNone')
            SetupKeepAliveTimer(TRUE);                                      // set up the keep-alive timer; trigger the keep-alive command right away (we do not know how long the connection was inactive, so let us avoid losing it)
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
