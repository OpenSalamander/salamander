// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

const char* LogsSeparator = "\r\n=================\r\n\r\n";

// ****************************************************************************

char* CopyStr(char* buf, int bufSize, const char* txt, int size)
{
    if (bufSize <= 0)
        return NULL;
    if (size >= bufSize)
        size = bufSize - 1;
    memcpy(buf, txt, size);
    buf[size] = 0;

    // je-li to nutne, provedeme konverzi LF na CRLF
    int insBytes = 0;
    char* s = buf;
    while (*s != 0)
    {
        if (*s == '\n' && (s == buf || *(s - 1) != '\r'))
        {
            if (++size >= bufSize && --size == (s - buf) + 1) // maly buffer
            {
                *s = 0; // oriznuti posledniho LF (CRLF se nevejde)
                break;
            }
            memmove(s + 1, s, size - (s - buf) - 1);
            *s++ = '\r';
            buf[size] = 0;
        }
        s++;
    }

    return buf;
}

// ****************************************************************************

BOOL PrepareFTPCommand(char* buf, int bufSize, char* logBuf, int logBufSize,
                       CFtpCmdCode ftpCmd, int* cmdLen, ...)
{
    va_list args;
    va_start(args, cmdLen);

    BOOL ret = TRUE;
    int len = 0;
    if (logBufSize > 0)
        logBuf[0] = 0;
    if (bufSize > 0)
    {
        switch (ftpCmd)
        {
        case ftpcmdQuit:
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "QUIT");
            break;
        case ftpcmdSystem:
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "SYST");
            break;
        case ftpcmdAbort:
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "ABOR");
            break;
        case ftpcmdPrintWorkingPath:
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "PWD");
            break;
        case ftpcmdNoOperation:
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "NOOP");
            break;
        case ftpcmdChangeWorkingPath:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "CWD %s", args);
            break;

        case ftpcmdSetTransferMode:
        {
            BOOL ascii = va_arg(args, BOOL);
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "TYPE %c", ascii ? 'A' : 'I');
            break;
        }

        case ftpcmdPassive:
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "PASV");
            break;

        case ftpcmdSetPort:
        {
            DWORD ip = va_arg(args, DWORD);
            unsigned short port = va_arg(args, unsigned short);
            len = _snprintf_s(buf, bufSize, _TRUNCATE, "PORT %u,%u,%u,%u,%d,%d",
                              (ip & 0xff),
                              ((ip >> 8) & 0xff),
                              ((ip >> 16) & 0xff),
                              ((ip >> 24) & 0xff),
                              ((port >> 8) & 0xff),
                              (port & 0xff));
            break;
        }

        case ftpcmdDeleteFile:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "DELE %s", args);
            break;
        case ftpcmdDeleteDir:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "RMD %s", args);
            break;
        case ftpcmdChangeAttrs:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "SITE CHMOD %03o %s", args);
            break;
        case ftpcmdChangeAttrsQuoted:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "SITE CHMOD %03o \"%s\"", args);
            break;
        case ftpcmdRestartTransfer:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "REST %s", args);
            break;
        case ftpcmdRetrieveFile:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "RETR %s", args);
            break;
        case ftpcmdStoreFile:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "STOR %s", args);
            break;
        case ftpcmdAppendFile:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "APPE %s", args);
            break;
        case ftpcmdCreateDir:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "MKD %s", args);
            break;
        case ftpcmdRenameFrom:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "RNFR %s", args);
            break;
        case ftpcmdRenameTo:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "RNTO %s", args);
            break;
        case ftpcmdGetSize:
            len = _vsnprintf_s(buf, bufSize, _TRUNCATE, "SIZE %s", args);
            break;

        default:
        {
            TRACE_E("Unknown command code in PrepareFTPCommand(): " << ftpCmd);
            ret = FALSE;
            break;
        }
        }
    }
    else
        len = -1; // asi nehrozi, jen prevence
    // hromadny test na vysledek _vsnprintf_s + pridame CRLF na konec
    if (len < 0 || len + 2 >= bufSize)
    {
        TRACE_E("PrepareFTPCommand(): Insufficient buffer size: " << bufSize);
        len = 0;
        ret = FALSE;
    }
    else
    {
        buf[len++] = '\r';
        buf[len++] = '\n';
        buf[len] = 0;
    }

    va_end(args);
    if (cmdLen != NULL)
        *cmdLen = len;
    if (logBufSize > 0 && logBuf[0] == 0)
    {
        if (len >= logBufSize)
            len = logBufSize - 1;
        memmove(logBuf, buf, len);
        logBuf[len] = 0;
    }
    return ret;
}

//
// ****************************************************************************
// CDynString
//

BOOL CDynString::Append(const char* str, int len)
{
    if (len == -1)
        len = (int)strlen(str);
    if (Length + len >= Allocated)
    {
        int size = Length + len + 1 + 256; // +256 znaku do foroty, at se tak casto nealokuje
        char* newBuf = (char*)realloc(Buffer, size);
        if (newBuf != NULL)
        {
            Buffer = newBuf;
            Allocated = size;
        }
        else // nedostatek pameti, smula...
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    memmove(Buffer + Length, str, len);
    Length += len;
    Buffer[Length] = 0;
    return TRUE;
}

void CDynString::SkipBeginning(DWORD len, int* skippedChars, int* skippedLines)
{
    if (Length > 0)
    {
        if (len > (DWORD)Length)
            len = Length;
        char* skipEnd = Buffer + len;
        char* end = Buffer + Length;
        int lines = 0;
        char* s = Buffer;
        while (s < end)
        {
            if (*s == '\r' && s + 1 < end && *(s + 1) == '\n')
            {
                lines++;
                s += 2;
                if (s >= skipEnd)
                    break;
            }
            else
            {
                if (*s == '\n')
                {
                    lines++;
                    if (++s >= skipEnd)
                        break;
                }
                else
                    s++;
            }
        }
        *skippedLines += lines;
        *skippedChars += (int)(s - Buffer);
        Length -= (int)(s - Buffer);
        memmove(Buffer, s, Length);
        Buffer[Length] = 0;
    }
}

//
// ****************************************************************************
// CControlConnectionSocket
//

void CControlConnectionSocket::CloseControlConnection(HWND parent)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::CloseControlConnection()");

    parent = FindPopupParent(parent);
    const DWORD showWaitWndTime = WAITWND_CLOSECON; // show time wait-okenka
    int serverTimeout = Config.GetServerRepliesTimeout() * 1000;
    if (serverTimeout < 1000)
        serverTimeout = 1000; // aspon sekundu
    SetStartTime();

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
    // keep-alive na 'kamForbidden' (probiha normalni prikaz)
    WaitForEndOfKeepAlive(parent, GetWaitTime(showWaitWndTime));

    CWaitWindow waitWnd(parent, TRUE);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));

    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGDISCONNECT), -1, TRUE);

    BOOL socketClosed = FALSE;
    int cmdLen;
    char buf[500];
    char errBuf[300];
    if (PrepareFTPCommand(buf, 500, errBuf, 300, ftpcmdQuit, &cmdLen))
    {
        DWORD error;
        BOOL allBytesWritten;
        if (Write(buf, cmdLen, &error, &allBytesWritten))
        {
            // sestavime text pro wait okenko
            HANDLES(EnterCriticalSection(&SocketCritSect));
            Logs.LogMessage(logUID, errBuf, -1);
            int l = (int)strlen(Host);
            if (l > 22) // zkratime text (do 22 znaku bereme, jinak jen 20 znaku + "...")
            {
                memcpy(errBuf, Host, 20);
                strcpy(errBuf + 20, "...");
            }
            else
                memcpy(errBuf, Host, l + 1);
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            sprintf(buf, LoadStr(IDS_CLOSINGCONNECTION), errBuf);

            waitWnd.SetText(buf);
            waitWnd.Create(GetWaitTime(showWaitWndTime));

            DWORD start = GetTickCount();
            BOOL shutdownCalled = FALSE;
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
                                  NULL, NULL, FALSE);
                switch (event)
                {
                case ccsevESC:
                {
                    waitWnd.Show(FALSE);
                    if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_CLOSECONESC),
                                                         LoadStr(IDS_FTPPLUGINTITLE),
                                                         MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                                             MB_ICONQUESTION) == IDYES)
                    { // user si preje tvrde terminovani connectiony
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
                    Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGDISCONTIMEOUT), -1, TRUE); // timeout disconnectu do logu
                    run = FALSE;
                    break;
                }

                case ccsevWriteDone:    // uz odesly vsechny byty (osetrime to, ze ccsevWriteDone mohla prepsat ccsevNewBytesRead)
                case ccsevClosed:       // mozna necekana ztrata pripojeni (osetrime take to, ze ccsevClosed mohla prepsat ccsevNewBytesRead)
                case ccsevNewBytesRead: // nacetli jsme nove byty
                {
                    char* reply;
                    int replySize;

                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    while (ReadFTPReply(&reply, &replySize)) // dokud mame nejakou odpoved serveru
                    {
                        Logs.LogMessage(logUID, reply, replySize);

                        if (!shutdownCalled) // prisla odpoved (at uspech ci chyba), shutdowneme socket
                        {
                            shutdownCalled = TRUE;
                            run = Shutdown(NULL); // v behu pokracujeme jen pri uspesnem zahajeni shutdownu
                        }
                        SkipFTPReply(replySize);
                    }
                    HANDLES(LeaveCriticalSection(&SocketCritSect));

                    if (event == ccsevClosed)
                    {
                        run = FALSE;
                        socketClosed = TRUE; // doslo k zavreni spojeni, koncime
                    }
                    break;
                }
                }
            }
            waitWnd.Destroy();
        }
    }

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

    if (!socketClosed)
    {
        CloseSocket(NULL); // zavreme socket (je-li otevreny), system se pokusi o "graceful" shutdown (nedozvime se o vysledku)
        Logs.SetIsConnected(logUID, IsConnected());
        Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"
    }

    // uvolnime keep-alive, ted uz nebude potreba (uz neni navazane spojeni)
    ReleaseKeepAlive();
}

void CControlConnectionSocket::CheckCtrlConClose(BOOL notInPanel, BOOL leftPanel, HWND parent, BOOL quiet)
{
    CALL_STACK_MESSAGE4("CControlConnectionSocket::CheckCtrlConClose(%d, %d, , %d)",
                        notInPanel, leftPanel, quiet);

    // socket je jiste zavreny + aktivne s nim nic nepracuje (nesleduje udalosti, atd.)

    HANDLES(EnterCriticalSection(&EventCritSect));
    DWORD error = NO_ERROR;
    BOOL found = FALSE;
    int i;
    for (i = 0; i < EventsUsedCount; i++) // test pritomnosti udalosti ccsevClosed
    {
        if (Events[i]->Event == ccsevClosed)
        {
            error = Events[i]->Data1;
            found = TRUE;
            break;
        }
    }
    int logUID = LogUID;
    char* auxConnectionLostMsg = ConnectionLostMsg;
    HANDLES(LeaveCriticalSection(&EventCritSect));

    if (found ||                      // ceka na nas udalost ccsevClosed -> user jeste nevi o tom, ze je zavrena connectiona
        auxConnectionLostMsg != NULL) // mame ulozeny text z okamziku uzavreni connectiony (tehdy sla jen do logu)
    {
        char errBuf[300];
        errBuf[0] = 0;
        char buf[500];

        if (found)
        {
            char* reply;
            int replySize;
            int replyCode;
            BOOL haveErr = FALSE;

            HANDLES(EnterCriticalSection(&SocketCritSect));
            while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
            {
                Logs.LogMessage(logUID, reply, replySize, TRUE);

                if (replyCode != -1 &&
                    (FTP_DIGIT_1(replyCode) == FTP_D1_TRANSIENTERROR ||
                     FTP_DIGIT_1(replyCode) == FTP_D1_ERROR))
                {
                    CopyStr(errBuf, 300, reply, replySize); // chybova hlaska (bereme posledni v rade)
                    haveErr = TRUE;
                }
                if (!haveErr)
                    CopyStr(errBuf, 300, reply, replySize); // pokud nemame chybu, bereme kazdou hlasku (posledni v rade)
                SkipFTPReply(replySize);
            }
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            if (errBuf[0] == 0)
            {
                if (auxConnectionLostMsg != NULL) // mame ulozeny text z okamziku uzavreni connectiony (tehdy sla jen do logu)
                {                                 // zobrazime ji jeste v messageboxu
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    if (ConnectionLostMsg != NULL)
                        lstrcpyn(errBuf, ConnectionLostMsg, 300);
                    else
                        errBuf[0] = 0;
                    SalamanderGeneral->Free(ConnectionLostMsg); // uz nebude potreba
                    ConnectionLostMsg = NULL;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                }
                else
                {
                    if (error == NO_ERROR)
                        strcpy(errBuf, LoadStr(IDS_NONEREPLY));
                    else
                        FTPGetErrorText(error, errBuf, 300);
                }
            }
            if (error != NO_ERROR)
            {
                FTPGetErrorTextForLog(error, buf, 500);
                Logs.LogMessage(logUID, buf, -1, TRUE);
            }
        }
        else // mame ulozeny text z okamziku uzavreni connectiony (tehdy sla jen do logu)
        {    // zobrazime ji jeste v messageboxu
            HANDLES(EnterCriticalSection(&SocketCritSect));
            if (ConnectionLostMsg != NULL)
                lstrcpyn(errBuf, ConnectionLostMsg, 300);
            else
                errBuf[0] = 0;
            SalamanderGeneral->Free(ConnectionLostMsg); // uz nebude potreba
            ConnectionLostMsg = NULL;
            HANDLES(LeaveCriticalSection(&SocketCritSect));
        }

        if (quiet)
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            if (ConnectionLostMsg != NULL)
                SalamanderGeneral->Free(ConnectionLostMsg);
            ConnectionLostMsg = SalamanderGeneral->DupStr(errBuf);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
        }
        else
        {
            if (Config.WarnWhenConLost)
            {
                BOOL actWelcomeMsg = (OurWelcomeMsgDlg != NULL && GetForegroundWindow() == OurWelcomeMsgDlg);
                sprintf(buf, LoadStr(notInPanel ? IDS_DCONLOSTFORMATERROR : (leftPanel ? IDS_LCONLOSTFORMATERROR : IDS_RCONLOSTFORMATERROR)), errBuf);
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = parent;
                params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_HINT;
                params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                params.Text = buf;
                params.CheckBoxText = LoadStr(IDS_WARNWHENCONLOST);
                int doNotWarnWhenConLost = !Config.WarnWhenConLost;
                params.CheckBoxValue = &doNotWarnWhenConLost;
                SalamanderGeneral->SalMessageBoxEx(&params);
                Config.WarnWhenConLost = !doNotWarnWhenConLost;
                if (actWelcomeMsg)
                    ActivateWelcomeMsg();
            }
        }
        // demolice zbytku dat (schvalne az po boxu - zpozdeni, dojdou vsechny udalosti), uz nebudou potreba
        ResetBuffersAndEvents();
    }
}

int CControlConnectionSocket::GetLogUID()
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::GetLogUID()");
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return logUID;
}

BOOL CControlConnectionSocket::LogMessage(const char* str, int len, BOOL addTimeToLog)
{
    CALL_STACK_MESSAGE4("CControlConnectionSocket::LogMessage(%s, %d, %d)", str, len, addTimeToLog);
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return Logs.LogMessage(logUID, str, len, addTimeToLog);
}

BOOL CControlConnectionSocket::ReadFTPReply(char** reply, int* replySize, int* replyCode)
{
    CALL_STACK_MESSAGE1("CControlConnectionSocket::ReadFTPReply(, ,)");

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CControlConnectionSocket::ReadFTPReply: not from section SocketCritSect!");
#endif

    return FTPReadFTPReply(ReadBytes, ReadBytesCount, ReadBytesOffset, reply, replySize, replyCode);
}

void CControlConnectionSocket::SkipFTPReply(int replySize)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::SkipFTPReply(%d)", replySize);

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount == 0 /* nechytne situaci, kdy
      sekci pouziva jiny thread */
    )
        TRACE_E("Incorrect call to CControlConnectionSocket::SkipFTPReply: not from section SocketCritSect!");
#endif

    ReadBytesOffset += replySize;
    if (ReadBytesOffset >= ReadBytesCount) // uz jsme precetli vse - resetneme buffer
    {
        if (ReadBytesOffset > ReadBytesCount)
            TRACE_E("Error in call to CControlConnectionSocket::SkipFTPReply(): trying to skip more bytes than is read");
        ReadBytesOffset = 0;
        ReadBytesCount = 0;
    }
}

BOOL CControlConnectionSocket::Write(const char* buffer, int bytesToWrite, DWORD* error, BOOL* allBytesWritten)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::Write(, %d, ,)", bytesToWrite);
    if (bytesToWrite == -1)
        bytesToWrite = (int)strlen(buffer);
    if (error != NULL)
        *error = NO_ERROR;
    if (allBytesWritten != NULL)
        *allBytesWritten = FALSE;

    if (bytesToWrite == 0) // zapis prazdneho bufferu
    {
        if (allBytesWritten != NULL)
            *allBytesWritten = TRUE;
        return TRUE;
    }

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL ret = FALSE;
    if (Socket != INVALID_SOCKET) // socket je pripojeny
    {
        if (BytesToWriteCount == BytesToWriteOffset) // nic neceka na poslani, muzeme posilat
        {
            if (BytesToWriteCount != 0)
                TRACE_E("Unexpected value of BytesToWriteCount.");

            int len = 0;
            if (!SSLConn)
                while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                {
                    // POZOR: pokud se nekdy zase bude zavadet TELNET protokol, je nutne predelat posilani IAC+IP
                    // pred abortovanim prikazu v metode SendFTPCommand()

                    int sentLen = send(Socket, buffer + len, bytesToWrite - len, 0);
                    if (sentLen != SOCKET_ERROR) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                    {
                        len += sentLen;
                        if (len >= bytesToWrite) // uz je poslano vsechno?
                        {
                            ret = TRUE;
                            break; // prestaneme posilat (jiz neni co)
                        }
                    }
                    else
                    {
                        DWORD err = WSAGetLastError();
                        if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                        {
                            ret = TRUE;
                            break; // prestaneme posilat (dodela se po FD_WRITE)
                        }
                        else // chyba posilani
                        {
                            if (error != NULL)
                                *error = err;
                            break; // vratime chybu
                        }
                    }
                }
            else
                while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                {
                    // POZOR: pokud se nekdy zase bude zavadet TELNET protokol, je nutne predelat posilani IAC+IP
                    // pred abortovanim prikazu v metode SendFTPCommand()

                    int sentLen = SSLLib.SSL_write(SSLConn, buffer + len, bytesToWrite - len);
                    if (sentLen >= 0) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                    {
                        len += sentLen;
                        if (len >= bytesToWrite) // uz je poslano vsechno?
                        {
                            ret = TRUE;
                            break; // prestaneme posilat (jiz neni co)
                        }
                    }
                    else
                    {
                        DWORD err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                        if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                        {
                            ret = TRUE;
                            break; // prestaneme posilat (dodela se po FD_WRITE)
                        }
                        else // chyba posilani
                        {
                            if (error != NULL)
                                *error = err;
                            break; // vratime chybu
                        }
                    }
                }

            if (ret) // uspesne odeslani, v 'len' je pocet poslanych bytu (zbytek posleme po prijeti FD_WRITE)
            {
                if (allBytesWritten != NULL)
                    *allBytesWritten = (len >= bytesToWrite);
                if (len < bytesToWrite) // zbytek vlozime do bufferu 'BytesToWrite'
                {
                    const char* buf = buffer + len;
                    int size = bytesToWrite - len;

                    if (BytesToWriteAllocatedSize - BytesToWriteCount < size) // malo mista v bufferu 'BytesToWrite'
                    {
                        int newSize = BytesToWriteCount + size + CRTLCON_BYTESTOWRITEONSOCKETPREALLOC;
                        char* newBuf = (char*)realloc(BytesToWrite, newSize);
                        if (newBuf != NULL)
                        {
                            BytesToWrite = newBuf;
                            BytesToWriteAllocatedSize = newSize;
                        }
                        else // nedostatek pameti pro ulozeni dat v nasem bufferu (chybu hlasi jen TRACE)
                        {
                            TRACE_E(LOW_MEMORY);
                            ret = FALSE;
                        }
                    }

                    if (ret) // muzeme zapsat (v bufferu je dost mista)
                    {
                        memcpy(BytesToWrite + BytesToWriteCount, buf, size);
                        BytesToWriteCount += size;
                    }
                }
            }
        }
        else // jeste nebylo odeslano vse -> chybne pouziti Write
        {
            TRACE_E("Incorrect use of CControlConnectionSocket::Write(): called again before waiting for ccsevWriteDone event.");
        }
    }
    else
        TRACE_I("CControlConnectionSocket::Write(): Socket is already closed.");

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

void CControlConnectionSocket::ResetBuffersAndEvents()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));

    EventConnectSent = FALSE;

    BytesToWriteCount = 0;
    BytesToWriteOffset = 0;

    ReadBytesCount = 0;
    ReadBytesOffset = 0;

    HANDLES(EnterCriticalSection(&EventCritSect));
    EventsUsedCount = 0;
    RewritableEvent = FALSE;
    ResetEvent(NewEvent);
    HANDLES(LeaveCriticalSection(&EventCritSect));

    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CControlConnectionSocket::ReceiveHostByAddress(DWORD ip, int hostUID, int err)
{
    CALL_STACK_MESSAGE4("CControlConnectionSocket::ReceiveHostByAddress(0x%X, %d, %d)", ip, hostUID, err);

    AddEvent(ccsevIPReceived, ip, err);
}

BOOL CControlConnectionSocket::SendKeepAliveCmd(int logUID, const char* ftpCmd)
{
    char errBuf[300];
    char buf[500];
    DWORD error;
    BOOL allBytesWritten;
    if (Write(ftpCmd, -1, &error, &allBytesWritten))
    {
        Logs.LogMessage(logUID, ftpCmd, -1);
        if (!allBytesWritten) // dost nepravdepodobne, ale stejne resime
        {
            HANDLES(EnterCriticalSection(&SocketCritSect));
            KeepAliveCmdAllBytesWritten = FALSE;
            HANDLES(LeaveCriticalSection(&SocketCritSect));
        }
        return TRUE;
    }
    else // chyba Write (low memory, disconnected, chyba neblokujiciho "send")
    {
        // pridame hlasku do logu, o zbytek by se mel postarat ClosedCtrlConChecker (hlaska userovi o vypadku spojeni)
        const char* e = GetOperationFatalErrorTxt(error, errBuf);
        lstrcpyn(buf, e, 500);
        char* s = buf + strlen(buf);
        while (s > buf && (*(s - 1) == '\n' || *(s - 1) == '\r'))
            s--;
        strcpy(s, "\r\n");                      // CRLF na konec textu posledni chyby
        Logs.LogMessage(logUID, buf, -1, TRUE); // text posledni chyby pridame do logu

        ReleaseKeepAlive(); // nic jsme neposlali (chyba spojeni, nema smysl pokracovat v keep-alive), zrusime keep-alive
        return FALSE;
    }
}

void CControlConnectionSocket::ReceiveNetEvent(LPARAM lParam, int index)
{
    CALL_STACK_MESSAGE3("CControlConnectionSocket::ReceiveNetEvent(0x%IX, %d)", lParam, index);
    DWORD eventError = WSAGETSELECTERROR(lParam); // extract error code of event
    switch (WSAGETSELECTEVENT(lParam))            // extract event
    {
    case FD_CLOSE: // nekdy chodi pred poslednim FD_READ, nezbyva tedy nez napred zkusit FD_READ a pokud uspeje, poslat si FD_CLOSE znovu (muze pred nim znovu uspet FD_READ)
    case FD_READ:
    {
        BOOL sendFDCloseAgain = FALSE; // TRUE = prisel FD_CLOSE + bylo co cist (provedl se jako FD_READ) => posleme si znovu FD_CLOSE (soucasny FD_CLOSE byl plany poplach)
        HANDLES(EnterCriticalSection(&SocketCritSect));

        if (!EventConnectSent) // pokud prisla FD_READ pred FD_CONNECT, posleme ccsevConnected jeste pred ctenim
        {
            EventConnectSent = TRUE;
            AddEvent(ccsevConnected, eventError, 0); // posleme si udalost s vysledkem pripojeni
        }

        BOOL ret = FALSE;
        DWORD err = NO_ERROR;
        BOOL genEvent = FALSE;
        if (eventError == NO_ERROR)
        {
            if (Socket != INVALID_SOCKET) // socket je pripojeny
            {
                BOOL lowMem = FALSE;
                if (ReadBytesAllocatedSize - ReadBytesCount < CRTLCON_BYTESTOREADONSOCKET) // maly buffer 'ReadBytes'
                {
                    if (ReadBytesOffset > 0) // je mozny sesun dat v bufferu?
                    {
                        memmove(ReadBytes, ReadBytes + ReadBytesOffset, ReadBytesCount - ReadBytesOffset);
                        ReadBytesCount -= ReadBytesOffset;
                        ReadBytesOffset = 0;
                    }

                    if (ReadBytesAllocatedSize - ReadBytesCount < CRTLCON_BYTESTOREADONSOCKET) // stale maly buffer 'ReadBytes'
                    {
                        int newSize = ReadBytesCount + CRTLCON_BYTESTOREADONSOCKET +
                                      CRTLCON_BYTESTOREADONSOCKETPREALLOC;
                        char* newBuf = (char*)realloc(ReadBytes, newSize);
                        if (newBuf != NULL)
                        {
                            ReadBytes = newBuf;
                            ReadBytesAllocatedSize = newSize;
                        }
                        else // nedostatek pameti pro ulozeni dat v nasem bufferu (chybu hlasi jen TRACE)
                        {
                            TRACE_E(LOW_MEMORY);
                            lowMem = TRUE;
                        }
                    }
                }

                if (!lowMem)
                { // precteme co nejvice bytu do bufferu, necteme cyklicky, aby se data zpracovavala postupne
                    // (staci mensi buffery); je-li jeste neco ke cteni, dostaneme znovu FD_READ
                    if (!SSLConn)
                    {
                        int len = recv(Socket, ReadBytes + ReadBytesCount, ReadBytesAllocatedSize - ReadBytesCount, 0);
                        if (len != SOCKET_ERROR) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                        {
                            if (len > 0)
                            {
                                ReadBytesCount += len; // upravime pocet jiz nactenych bytu o nove nactene
                                ret = TRUE;
                                genEvent = TRUE;
                                if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                    sendFDCloseAgain = TRUE;
                            }
                        }
                        else
                        {
                            err = WSAGetLastError();
                            if (err != WSAEWOULDBLOCK)
                                genEvent = TRUE; // budeme generovat udalost s chybou
                        }
                    }
                    else
                    {
                        if (SSLLib.SSL_pending(SSLConn) > 0) // je-li neprazdny interni SSL buffer nedojde vubec k volani recv() a tudiz neprijde dalsi FD_READ, tedy musime si ho poslat sami, jinak se prenos dat zastavi
                            PostMessage(SocketsThread->GetHiddenWindow(), Msg, (WPARAM)Socket, FD_READ);
                        int len = SSLLib.SSL_read(SSLConn, ReadBytes + ReadBytesCount, ReadBytesAllocatedSize - ReadBytesCount);
                        if (len >= 0) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                        {
                            if (len > 0)
                            {
                                ReadBytesCount += len; // upravime pocet jiz nactenych bytu o nove nactene
                                ret = TRUE;
                                genEvent = TRUE;
                                if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
                                    sendFDCloseAgain = TRUE;
                            }
                        }
                        else
                        {
                            err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, len));
                            ;
                            if (err != WSAEWOULDBLOCK)
                                genEvent = TRUE; // budeme generovat udalost s chybou
                        }
                    }
                }
            }
            else
            {
                // muze nastat: hl. thread stihne zavolat CloseSocket() pred dorucenim FD_READ
                // TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveNetEvent(FD_READ): Socket is not connected.");
                // udalost s touto necekanou chybou nebudeme zavadet (reseni: user pouzije ESC)
            }
        }
        else // hlaseni chyby v FD_READ (podle helpu jen WSAENETDOWN)
        {
            if (WSAGETSELECTEVENT(lParam) != FD_CLOSE) // chybu si osetri FD_CLOSE sam
            {
                genEvent = TRUE;
                err = eventError;
            }
        }
        if (genEvent && (KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing))
        {
            char* reply;
            int replySize;
            int replyCode;
            while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
            {
                Logs.LogMessage(LogUID, reply, replySize);
                BOOL run = TRUE;
                BOOL leave = TRUE;
                BOOL setupNextKA = TRUE;
                BOOL sendList = FALSE;
                int logUID = LogUID; // UID logu teto connectiony
                int listCmd = KeepAliveCommand;
                CKeepAliveDataConSocket* kaDataConnection = KeepAliveDataCon;
                switch (KeepAliveDataConState)
                {
                case kadcsWaitForPassiveReply: // odpoved na PASV (otevreme "data connection" + posleme listovaci prikaz)
                {
                    setupNextKA = FALSE;
                    DWORD ip;
                    unsigned short port;
                    if (FTP_DIGIT_1(replyCode) == FTP_D1_SUCCESS &&             // uspech (melo by byt 227)
                        FTPGetIPAndPortFromReply(reply, replySize, &ip, &port)) // podarilo se ziskat IP+port
                    {
                        KeepAliveDataConState = kadcsWaitForListStart;
                        SkipFTPReply(replySize);
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        leave = FALSE;

                        kaDataConnection->SetPassive(ip, port, logUID);
                        kaDataConnection->PassiveConnect(NULL); // prvni pokus, vysledek nas nezajima (testuje se pozdeji)

                        sendList = TRUE;
                    }
                    else // pasivni rezim neni podporovan, koncime...
                    {
                        SkipFTPReply(replySize);
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        leave = FALSE;

                        Logs.LogMessage(logUID, LoadStr(IDS_LOGMSGKAPASVNOTSUPPORTED), -1, TRUE);
                        ReleaseKeepAlive();
                        run = FALSE; // koncime...
                    }
                    break;
                }

                case kadcsWaitForSetPortReply:
                {
                    KeepAliveDataConState = kadcsWaitForListStart;
                    setupNextKA = FALSE;
                    sendList = TRUE;
                    break;
                }

                case kadcsWaitForListStart:
                {
                    if (FTP_DIGIT_1(replyCode) != FTP_D1_MAYBESUCCESS)
                    {
                        KeepAliveDataConState = kadcsDone;
                        SkipFTPReply(replySize);
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        leave = FALSE;

                        if (kaDataConnection->FinishDataTransfer(replyCode))
                        { // uvolnime "data connection" pres volani metody SocketsThread
                            HANDLES(EnterCriticalSection(&SocketCritSect));
                            kaDataConnection = KeepAliveDataCon; // opatreni proti dvojite destrukci (pokud hl.thread ceka na destrukci, je zde uz NULL)
                            KeepAliveDataCon = NULL;
                            KeepAliveDataConState = kadcsNone;
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                            if (kaDataConnection != NULL)
                                DeleteSocket(kaDataConnection);
                        }
                        else
                            setupNextKA = FALSE; // "data connection" jeste neskoncila, cekame na jeji konec
                        run = FALSE;             // koncime...
                    }
                    else
                    {
                        setupNextKA = FALSE;
                        SkipFTPReply(replySize);
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        leave = FALSE;

                        kaDataConnection->EncryptPassiveDataCon();
                    }
                    break;
                }
                }
                if (leave)
                {
                    SkipFTPReply(replySize);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                }

                if (sendList) // posleme prikaz "list" ('listCmd' ('KeepAliveCommand') musi byt 2 nebo 3, jinak by to sem nedoslo)
                {
                    char ftpCmd[200];
                    const char* s = (listCmd == 3 ? LIST_CMD_TEXT : NLST_CMD_TEXT);
                    sprintf(ftpCmd, "%s\r\n", s);

                    if (SendKeepAliveCmd(logUID, ftpCmd))
                        kaDataConnection->ActivateConnection();
                    else
                        run = FALSE; // koncime...
                }

                if (setupNextKA)
                {
                    // pri prijmu odpovedi pustime dale hl.thread (pokud ceka) nebo nastavime dalsi
                    // keep-alive timer (hl.thread neceka) nebo uplne uvolnime keep-alive (uz vyprsela
                    // doba posilani keep-alive prikazu)
                    SetupNextKeepAliveTimer();

                    // odcerpame nadbytecne zpravy od serveru (napr. WarFTPD je generuje pri LIST v nepristupnem adresari)
                    HANDLES(EnterCriticalSection(&SocketCritSect));
                    while (ReadFTPReply(&reply, &replySize, &replyCode)) // dokud mame nejakou odpoved serveru
                    {
                        Logs.LogMessage(LogUID, reply, replySize);
                        SkipFTPReply(replySize);
                    }
                    break; // koncime...
                }
                HANDLES(EnterCriticalSection(&SocketCritSect));
                if (!run)
                    break;
            }
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            genEvent = FALSE;
        }
        else
            HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (genEvent) // vygenerujeme udalost ccsevNewBytesRead
        {
            AddEvent(ccsevNewBytesRead, (!ret ? err : NO_ERROR), 0, ret); // prepsatelna jen pokud nejde o chybu
        }

        // ted zpracujeme FD_CLOSE
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            if (sendFDCloseAgain) // FD_CLOSE byl misto FD_READ => posleme FD_CLOSE znovu
            {
                PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                            (WPARAM)GetSocket(), lParam);
            }
            else // korektni FD_CLOSE
            {
                CSocket::ReceiveNetEvent(lParam, index); // zavolame metodu predka
            }
        }
        break;
    }

    case FD_WRITE:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));

        BOOL ret = FALSE;
        DWORD err = NO_ERROR;
        BOOL genEvent = FALSE;
        if (eventError == NO_ERROR)
        {
            if (BytesToWriteCount > BytesToWriteOffset) // mame nejaky restiky, budeme posilat zbyla data z bufferu 'BytesToWrite'
            {
                if (Socket != INVALID_SOCKET) // socket je pripojeny
                {
                    int len = 0;
                    if (!SSLConn)
                    {
                        while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                        {
                            int sentLen = send(Socket, BytesToWrite + BytesToWriteOffset + len,
                                               BytesToWriteCount - BytesToWriteOffset - len, 0);
                            if (sentLen != SOCKET_ERROR) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                            {
                                len += sentLen;
                                if (len >= BytesToWriteCount - BytesToWriteOffset) // uz je poslano vsechno?
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (jiz neni co)
                                }
                            }
                            else
                            {
                                err = WSAGetLastError();
                                if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (dodela se po FD_WRITE)
                                }
                                else // jina chyba - resetneme buffer
                                {
                                    BytesToWriteOffset = 0;
                                    BytesToWriteCount = 0;
                                    break; // vratime chybu
                                }
                            }
                        }
                    }
                    else
                    {
                        while (1) // cyklus nutny kvuli funkci 'send' (neposila FD_WRITE pri 'sentLen' < 'bytesToWrite')
                        {
                            int sentLen = SSLLib.SSL_write(SSLConn, BytesToWrite + BytesToWriteOffset + len,
                                                           BytesToWriteCount - BytesToWriteOffset - len);
                            if (sentLen >= 0) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                            {
                                len += sentLen;
                                if (len >= BytesToWriteCount - BytesToWriteOffset) // uz je poslano vsechno?
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (jiz neni co)
                                }
                            }
                            else
                            {
                                err = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                                if (err == WSAEWOULDBLOCK) // nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space)
                                {
                                    ret = TRUE;
                                    break; // prestaneme posilat (dodela se po FD_WRITE)
                                }
                                else // jina chyba - resetneme buffer
                                {
                                    BytesToWriteOffset = 0;
                                    BytesToWriteCount = 0;
                                    break; // vratime chybu
                                }
                            }
                        }
                    }

                    if (ret && len > 0) // aspon neco bylo odeslano -> zmena 'BytesToWriteOffset'
                    {
                        BytesToWriteOffset += len;
                        if (BytesToWriteOffset >= BytesToWriteCount) // vsechno poslano, reset bufferu
                        {
                            BytesToWriteOffset = 0;
                            BytesToWriteCount = 0;
                        }
                    }

                    genEvent = (!ret || BytesToWriteCount == BytesToWriteOffset); // chyba nebo se jiz podarilo poslat vse
                }
                else
                {
                    // muze nastat: hl. thread stihne zavolat CloseSocket() pred dorucenim FD_WRITE
                    //TRACE_E("Unexpected situation in CControlConnectionSocket::ReceiveNetEvent(FD_WRITE): Socket is not connected.");
                    BytesToWriteCount = 0; // chyba -> resetneme buffer
                    BytesToWriteOffset = 0;
                    // udalost s touto necekanou chybou nebudeme zavadet (reseni: user pouzije ESC)
                }
            }
        }
        else // hlaseni chyby v FD_WRITE (podle helpu jen WSAENETDOWN)
        {
            genEvent = TRUE;
            err = eventError;
            BytesToWriteCount = 0; // chyba -> resetneme buffer
            BytesToWriteOffset = 0;
        }
        if (genEvent && (KeepAliveMode == kamProcessing || KeepAliveMode == kamWaitingForEndOfProcessing))
        { // dokoncen zapis prikazu pro keep-alive: misto udalosti ccsevWriteDone jen zapiseme do KeepAliveCmdAllBytesWritten
            KeepAliveCmdAllBytesWritten = TRUE;
            genEvent = FALSE;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));

        if (genEvent) // vygenerujeme udalost ccsevWriteDone
        {
            AddEvent(ccsevWriteDone, (!ret ? err : NO_ERROR), 0);
        }
        break;
    }

    case FD_CONNECT:
    {
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (!EventConnectSent)
        {
            EventConnectSent = TRUE;
            AddEvent(ccsevConnected, eventError, 0); // posleme si udalost s vysledkem pripojeni
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }
    }
}

void CControlConnectionSocket::SocketWasClosed(DWORD error)
{
    CALL_STACK_MESSAGE2("CControlConnectionSocket::SocketWasClosed(%u)", error);

    AddEvent(ccsevClosed, error, 0);

    // informujeme uzivatele o zavreni "control connection", pokud se tak nestane
    // behem operace se socketem (informuje uzivatele napr. o timeoutu nebo "kick"
    // vedoucimu k odpojeni z FTP serveru)
    ClosedCtrlConChecker.Add(this);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    int logUID = LogUID; // UID logu teto connectiony
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    Logs.SetIsConnected(logUID, IsConnected());
    Logs.RefreshListOfLogsInLogsDlg(); // hlaseni "connection inactive"

    // uvolnime keep-alive, ted uz nebude potreba (uz neni navazane spojeni)
    ReleaseKeepAlive();
}

//
// ****************************************************************************
// CClosedCtrlConChecker
//

CClosedCtrlConChecker::CClosedCtrlConChecker() : CtrlConSockets(2, 5, dtNoDelete)
{
    HANDLES(InitializeCriticalSection(&DataSect));
    CmdNotPost = TRUE;
}

CClosedCtrlConChecker::~CClosedCtrlConChecker()
{
    HANDLES(DeleteCriticalSection(&DataSect));
}

BOOL CClosedCtrlConChecker::Add(CControlConnectionSocket* sock)
{
    CALL_STACK_MESSAGE1("CClosedCtrlConChecker::Add()");
    HANDLES(EnterCriticalSection(&DataSect));

    BOOL ret = FALSE;
    CtrlConSockets.Add(sock);
    if (CtrlConSockets.IsGood())
    {
        ret = TRUE;
        if (CmdNotPost)
        {
            CmdNotPost = FALSE;
            SalamanderGeneral->PostMenuExtCommand(FTPCMD_CLOSECONNOTIF, TRUE);
        }
    }
    else
        CtrlConSockets.ResetState();

    HANDLES(LeaveCriticalSection(&DataSect));
    return ret;
}

void CClosedCtrlConChecker::Check(HWND parent)
{
    CALL_STACK_MESSAGE1("CClosedCtrlConChecker::Check()");
    HANDLES(EnterCriticalSection(&DataSect));

    CmdNotPost = TRUE;

    int k;
    for (k = 0; k < CtrlConSockets.Count; k++) // pro vsechny ulozene zavrene "control connection" (muzou byt i dealokovane)
    {
        CControlConnectionSocket* ctrlCon = CtrlConSockets[k];
        int i;
        for (i = 0; i < FTPConnections.Count; i++) // zkusime najit FS vyuzivajici 'ctrlCon' (FS uz ale muze byt zavreny)
        {
            CPluginFSInterface* fs = FTPConnections[i];
            if (fs->Contains(ctrlCon)) // nasli jsme FS s zavrenou "control connection"
            {
                fs->CheckCtrlConClose(parent); // pokud se jeste user nedozvedel o zavreni, dozvi se ted
                break;
            }
        }
    }
    CtrlConSockets.DestroyMembers();

    HANDLES(LeaveCriticalSection(&DataSect));
}

//
// ****************************************************************************
// CLogData
//

CLogData::CLogData(const char* host, unsigned short port, const char* user,
                   CControlConnectionSocket* ctrlCon, BOOL connected, BOOL isWorker)
{
    UID = NextLogUID++;
    if (UID == -1)
        UID = NextLogUID++; // -1 je vyhrazeno
    Host = SalamanderGeneral->DupStr(host);
    Port = port;
    User = SalamanderGeneral->DupStr(user);
    CtrlConOrWorker = !isWorker;
    WorkerIsAlive = isWorker;
    CtrlCon = ctrlCon;
    Connected = connected;
    DisconnectNum = -1;
    SkippedChars = 0;
    SkippedLines = 0;
}

CLogData::~CLogData()
{
    if (Host != NULL)
        SalamanderGeneral->Free(Host);
    if (User != NULL)
        SalamanderGeneral->Free(User);
}

BOOL CLogData::ChangeUser(const char* user)
{
    char* u = SalamanderGeneral->DupStr(user);
    if (u != NULL)
    {
        if (User != NULL)
            SalamanderGeneral->Free(User);
        User = u;
    }
    else
    {
        if (User != NULL)
            User[0] = 0;
    }
    return u != NULL;
}

//
// ****************************************************************************
// CLogsDlgThread
//

class CLogsDlgThread : public CThread
{
protected:
    CLogsDlg* LogsDlg;
    BOOL AlwaysOnTop;

public:
    CLogsDlgThread(CLogsDlg* logsDlg) : CThread("Logs Dialog")
    {
        LogsDlg = logsDlg;
        AlwaysOnTop = FALSE;
        SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &AlwaysOnTop, sizeof(AlwaysOnTop), NULL);
    }

    virtual unsigned Body()
    {
        CALL_STACK_MESSAGE1("CLogsDlgThread::Body()");

        // 'sendWMClose': dialog nastavi na TRUE v pripade, ze doslo k prijeti WM_CLOSE
        // v okamziku, kdy je otevreny modalni dialog nad dialogem logu - az se tento
        // modalni dialog ukonci, posle se WM_CLOSE znovu do dialogu logu
        BOOL sendWMClose = FALSE;
        LogsDlg->SendWMClose = &sendWMClose;

        if (LogsDlg->Create() == NULL || LogsDlg->CloseDlg)
        {
            if (!LogsDlg->CloseDlg)
                Logs.SetLogsDlg(NULL);
            if (LogsDlg->HWindow != NULL)
                DestroyWindow(LogsDlg->HWindow); // WM_CLOSE nemuze dojit, protoze ho nema co dorucit (message-loopa zatim nebezi)
        }
        else
        {
            HWND dlg = LogsDlg->HWindow;
            if (AlwaysOnTop) // always-on-top osetrime aspon "staticky" (neni v system menu)
                SetWindowPos(dlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            SetForegroundWindow(dlg);

            // message-loopa - pockame do ukonceni nemodalniho dialogu
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (sendWMClose)
                {
                    sendWMClose = FALSE;
                    PostMessage(dlg, WM_CLOSE, 0, 0);
                }
            }
        }
        delete LogsDlg;
        return 0;
    }
};

//
// ****************************************************************************
// CLogs
//

void CLogs::AddLogsToCombo(HWND combo, int prevItemUID, int* focusIndex, BOOL* empty)
{
    *focusIndex = -1; // nenalezen

    SendMessage(combo, CB_RESETCONTENT, 0, 0);

    HANDLES(EnterCriticalSection(&LogCritSect));
    *empty = (Data.Count <= 0);
    if (!*empty)
    {
        HANDLES(EnterCriticalSection(&PanelCtrlConSect));
        char buf[300];
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CLogData* d = Data[i];
            sprintf(buf, "%d: ", d->UID);
            if (d->User != NULL && d->User[0] != 0 && strcmp(d->User, FTP_ANONYMOUS) != 0)
                sprintf(buf + strlen(buf), "%s@", d->User);
            sprintf(buf + strlen(buf), "%s", d->Host);
            if (d->Port != IPPORT_FTP)
                sprintf(buf + strlen(buf), ":%u", d->Port);

            int fsPosID = 0;
            if (d->CtrlConOrWorker) // connection in panel
            {
                if (d->CtrlCon != NULL)
                {
                    if (LeftPanelCtrlCon == d->CtrlCon)
                        fsPosID = IDS_FTPINLEFTPANEL; // FS in left panel
                    else
                    {
                        if (RightPanelCtrlCon == d->CtrlCon)
                            fsPosID = IDS_FTPINRIGHTPANEL; // FS in right panel
                        else
                            fsPosID = IDS_FTPNOTINPANEL; // detached FS
                    }
                }
                else
                    fsPosID = IDS_FTPDISCONNECTED; // closed FS
            }
            else // connection in worker used for operation (copy/move/delete)
            {
                if (d->WorkerIsAlive)
                    fsPosID = IDS_FTPOPERATION; // living worker
                else
                    fsPosID = IDS_OPERSTOPPED; // stopped worker
            }

            if ((d->CtrlCon != NULL || d->WorkerIsAlive) && !d->Connected)
            {
                sprintf(buf + strlen(buf), " (%s, %s)", LoadStr(fsPosID), LoadStr(IDS_FTPINACTIVE));
            }
            else
                sprintf(buf + strlen(buf), " (%s)", LoadStr(fsPosID));

            // pridame sestavene jmeno + UID logu
            if (i == SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)buf))
                SendMessage(combo, CB_SETITEMDATA, i, d->UID);
            if (d->UID == prevItemUID)
                *focusIndex = i;
        }
        HANDLES(LeaveCriticalSection(&PanelCtrlConSect));
    }
    else
    {
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_NOLOGS));
        SendMessage(combo, CB_SETITEMDATA, 0, -1);
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

BOOL CLogs::GetLogIndex(int uid, int* index)
{
    if (LastUID != uid || LastIndex < 0 || LastIndex >= Data.Count || Data[LastIndex]->UID != uid)
    {
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            if (Data[i]->UID == uid)
            {
                LastUID = uid;
                LastIndex = i;
                break;
            }
        }
        if (i == Data.Count) // log neexistuje
        {
            *index = -1;
            return FALSE;
        }
    }
    *index = LastIndex;
    return TRUE;
}

void CLogs::SetLogToEdit(HWND edit, int logUID, BOOL update)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    int index;
    if (logUID != -1 && GetLogIndex(logUID, &index))
    {
        BOOL lockUpdate = GetForegroundWindow() == GetParent(edit);
        CLogData* d = Data[index];
        if (!update) // nastaveni textu logu
        {
            if (lockUpdate)
                LockWindowUpdate(edit);
            SetWindowText(edit, d->Text.GetString());
            SendMessage(edit, EM_SETSEL, d->Text.Length, d->Text.Length);
            SendMessage(edit, EM_SCROLLCARET, 0, 0);
            if (lockUpdate)
                LockWindowUpdate(NULL);
            d->SkippedChars = 0;
            d->SkippedLines = 0;
        }
        else // update textu logu
        {
            if (lockUpdate)
                LockWindowUpdate(edit);
            int firstLine = (int)SendMessage(edit, EM_GETFIRSTVISIBLELINE, 0, 0);
            int pos;
            SendMessage(edit, EM_GETSEL, 0, (LPARAM)&pos);
            if (pos == SendMessage(edit, WM_GETTEXTLENGTH, 0, 0))
                pos = d->Text.Length; // drzime konec textu
            else                      // drzime pozici caretu
            {
                pos -= d->SkippedChars;
                if (pos < 0)
                    pos = 0;
                firstLine -= d->SkippedLines;
                if (firstLine < 0)
                    firstLine = 0;
            }

            // zjistime jestli scrollovat na caret nebo na prvni viditelnou radku z minuleho obsahu editu
            BOOL scrollCaret = pos == d->Text.Length;
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
            if (scrollCaret && GetScrollInfo(edit, SB_VERT, &si) != 0)
                scrollCaret = (si.nMax == (int)si.nPage + si.nPos - 1);

            SetWindowText(edit, d->Text.GetString());
            SendMessage(edit, EM_SETSEL, pos, pos);
            if (scrollCaret)
                SendMessage(edit, EM_SCROLLCARET, 0, 0);
            else
                SendMessage(edit, EM_LINESCROLL, 0, firstLine);

            if (lockUpdate)
                LockWindowUpdate(NULL);
            d->SkippedChars = 0;
            d->SkippedLines = 0;
        }
    }
    else
        SetWindowText(edit, ""); // neznamy log -> cistime edit
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

void CLogs::ConfigChanged()
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    if (!Config.EnableLogging) // demolice dat
    {
        Data.DestroyMembers();
        if (!Data.IsGood())
            Data.ResetState();
        if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
            PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
    }
    else
    {
        if (Config.DisableLoggingOfWorkers) // demolice dat logu workeru
        {
            BOOL change = FALSE;
            int i;
            for (i = Data.Count - 1; i >= 0; i--)
            {
                if (!Data[i]->CtrlConOrWorker) // jde o log workera
                {
                    change = TRUE;
                    Data.Delete(i);
                    if (!Data.IsGood())
                        Data.ResetState();
                }
            }
            if (change && LogsDlg != NULL && LogsDlg->HWindow != NULL)
                PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
        }

        LimitClosedConLogs(); // osetri zmenu limitu pro pocet logu zavrenych spojeni

        if (Config.UseLogMaxSize) // existuje limit pro velikost logu
        {
            DWORD size = Config.LogMaxSize * 1024; // preteceni se resi uz pri zadani
            int i;
            for (i = 0; i < Data.Count; i++)
            {
                CLogData* d = Data[i];
                if ((DWORD)(d->Text.Length) > size) // log je prilis velky
                {
                    d->Text.SkipBeginning((DWORD)(d->Text.Length) - size, &(d->SkippedChars), &(d->SkippedLines));
                    if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
                        PostMessage(LogsDlg->HWindow, WM_APP_UPDATELOG, d->UID, 0);
                }
            }
        }
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

BOOL CLogs::CreateLog(int* uid, const char* host, unsigned short port, const char* user,
                      CControlConnectionSocket* ctrlCon, BOOL connected, BOOL isWorker)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL ok = FALSE;
    *uid = -1;
    CLogData* d = new CLogData(host, port, user, ctrlCon, connected, isWorker);
    if (d != NULL && d->IsGood())
    {
        Data.Add(d);
        if (Data.IsGood())
        {
            if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
                PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
            ok = TRUE;
            *uid = d->UID;
            d = NULL;
        }
        else
            Data.ResetState();
    }
    if (d != NULL)
        delete d;
    HANDLES(LeaveCriticalSection(&LogCritSect));
    return ok;
}

BOOL CLogs::ChangeUser(int uid, const char* user)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL ret = FALSE;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CLogData* d = Data[i];
        if (d->UID == uid)
        {
            ret = d->ChangeUser(user);
            if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
                PostMessage(LogsDlg->HWindow, WM_APP_UPDATELOG, d->UID, 0);
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
    return ret;
}

void CLogs::LimitClosedConLogs()
{
    if (Config.UseMaxClosedConLogs) // existuje limit pro pocet logu zavrenych spojeni
    {
        int firstSurvival = CLogData::NextDisconnectNum - Config.MaxClosedConLogs;
        if (CLogData::OldestDisconnectNum < firstSurvival) // je co rusit
        {
            int i;
            for (i = Data.Count - 1; i >= 0; i--)
            {
                CLogData* d = Data[i];
                if (d->CtrlCon == NULL && !d->WorkerIsAlive && // log zavreneho spojeni (mrtvy log)
                    d->DisconnectNum < firstSurvival)          // jde o prilis stary log
                {
                    Data.Delete(i);
                    if (!Data.IsGood())
                        Data.ResetState();
                }
            }
            if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
                PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
            CLogData::OldestDisconnectNum = firstSurvival;
        }
    }
}

void CLogs::SetIsConnected(int uid, BOOL isConnected)
{
    if (uid != -1)
    {
        HANDLES(EnterCriticalSection(&LogCritSect));
        int index;
        if (GetLogIndex(uid, &index))
        {
            CLogData* d = Data[index];
            d->Connected = isConnected;
        }
        else
            TRACE_I("CLogs::SetIsConnected(): uid (" << uid << ") not found");
        HANDLES(LeaveCriticalSection(&LogCritSect));
    }
}

BOOL CLogs::ClosingConnection(int uid)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL ret = FALSE;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CLogData* d = Data[i];
        if (d->UID == uid)
        {
            ret = TRUE;
            d->WorkerIsAlive = FALSE;
            d->CtrlCon = NULL;
            d->Connected = FALSE;
            d->DisconnectNum = CLogData::NextDisconnectNum++;
            LimitClosedConLogs();
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
    return ret;
}

BOOL CLogs::LogMessage(int uid, const char* str, int len, BOOL addTimeToLog)
{
    if (uid == -1)
        return TRUE; // "invalid UID" -> nemame nic logovat

    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL ret = FALSE;
    int index;
    if (GetLogIndex(uid, &index))
    {
        char timeBuf[20];
        int timeLen = 0;
        if (addTimeToLog)
        {
            SYSTEMTIME st;
            GetLocalTime(&st); // pouzijeme std. casovy format, at zbytecne nebrzdime s GetTimeFormat
            timeLen = sprintf(timeBuf, "(%u:%02u:%02u): ", st.wHour, st.wMinute, st.wSecond);
        }

        if (len == -1)
            len = (int)strlen(str);
        CLogData* d = Data[index];
        if (Config.UseLogMaxSize) // existuje limit pro velikost logu
        {
            DWORD size = Config.LogMaxSize * 1024; // preteceni se resi uz pri zadani
            if ((DWORD)(d->Text.Length + len + timeLen) > size)
            {
                d->Text.SkipBeginning((DWORD)(d->Text.Length + len + timeLen) - size, &(d->SkippedChars), &(d->SkippedLines));
            }
        }

        // zapiseme hlasku do logu (v nejslozitejsim pripade zapiseme nejprve CR+LF pred textem, pak
        // aktualni cas, a pak zbytek textu (za jiz zapsanymi CR+LF))
        const char* s = str;
        if (timeLen > 0)
        {
            const char* end = str + len;
            while (s < end && (*s == '\r' || *s == '\n'))
                s++;
        }
        if (s == str || (ret = d->Text.Append(str, (int)(s - str))) != 0)
            if (timeLen == 0 || (ret = d->Text.Append(timeBuf, timeLen)) != 0)
                ret = d->Text.Append(s, (int)(len - (s - str)));

        if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
            PostMessage(LogsDlg->HWindow, WM_APP_UPDATELOG, d->UID, 0);
    }
    else
        TRACE_I("CLogs::LogMessage(): uid (" << uid << ") not found");
    HANDLES(LeaveCriticalSection(&LogCritSect));
    return ret;
}

BOOL CLogs::HasLogWithUID(int uid)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL ret = FALSE;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        if (Data[i]->UID == uid)
        {
            ret = TRUE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
    return ret;
}

void CLogs::SetLogsDlg(CLogsDlg* logsDlg)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    LogsDlg = logsDlg;
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

void CLogs::ActivateLog(int showLogUID)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
        PostMessage(LogsDlg->HWindow, WM_APP_ACTIVATELOG, showLogUID, 0);
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

BOOL CLogs::ActivateLogsDlg(int showLogUID)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL ret = FALSE;
    if (LogsDlg != NULL)
    {
        if (LogsDlg->HWindow != NULL) // "always true" (jinak: dialog se teprve otevre, cimz se sam aktivuje)
        {
            if (IsIconic(LogsDlg->HWindow))
                ShowWindow(LogsDlg->HWindow, SW_RESTORE);
            SetForegroundWindow(LogsDlg->HWindow);
            PostMessage(LogsDlg->HWindow, WM_APP_ACTIVATELOG, showLogUID, 0);
        }
        ret = TRUE;
    }
    else
    {
        LogsDlg = new CLogsDlg(NULL, SalamanderGeneral->GetMainWindowHWND(), GlobalShowLogUID);
        if (LogsDlg != NULL)
        {
            CLogsDlgThread* t = new CLogsDlgThread(LogsDlg);
            if (t != NULL)
            {
                if ((LogsThread = t->Create(AuxThreadQueue)) == NULL)
                { // thread se nepustil, error
                    delete t;
                    delete LogsDlg;
                    LogsDlg = NULL;
                }
                else
                    ret = TRUE; // uspech
            }
            else // malo pameti, error
            {
                delete LogsDlg;
                LogsDlg = NULL;
                TRACE_E(LOW_MEMORY);
            }
        }
        else
            TRACE_E(LOW_MEMORY); // malo pameti, error
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
    return ret;
}

void CLogs::CloseLogsDlg()
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    if (LogsDlg != NULL)
    {
        LogsDlg->CloseDlg = TRUE; // k CloseDlg neni synchronizovany pristup, snad zbytecne
        if (LogsDlg->HWindow != NULL)
            PostMessage(LogsDlg->HWindow, WM_CLOSE, 0, 0);
        LogsDlg = NULL; // dealokaci zajisti thread dialogu
    }
    HANDLE t = LogsThread;
    HANDLES(LeaveCriticalSection(&LogCritSect));
    if (t != NULL)
    {
        CALL_STACK_MESSAGE1("CLogs::CloseLogsDlg(): AuxThreadQueue.WaitForExit()");
        AuxThreadQueue.WaitForExit(t, INFINITE);
    }
}

void CLogs::SaveLogsDlgPos()
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
    {
        Config.LogsDlgPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(LogsDlg->HWindow, &Config.LogsDlgPlacement);
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

void CLogs::RefreshListOfLogsInLogsDlg()
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
        PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
    HANDLES(LeaveCriticalSection(&LogCritSect));
}

void CLogs::SaveLog(HWND parent, const char* itemName, int uid)
{ // itemName == NULL - "save all as..."
    static char initDir[MAX_PATH] = "";
    if (initDir[0] == 0)
        GetMyDocumentsPath(initDir);
    char fileName[MAX_PATH];
    strcpy(fileName, "ftp.log");

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = parent;
    char* s = LoadStr(IDS_SAVELOGFILTER);
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrDefExt = "log";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = LoadStr(itemName == NULL ? IDS_SAVEALLASTITLE : IDS_SAVEASTITLE);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT |
                OFN_NOTESTFILECREATE | OFN_HIDEREADONLY;

    char buf[200 + MAX_PATH];
    if (SalamanderGeneral->SafeGetSaveFileName(&ofn))
    {
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

        s = strrchr(fileName, '\\');
        if (s != NULL)
        {
            memcpy(initDir, fileName, s - fileName);
            initDir[s - fileName] = 0;
        }

        if (SalamanderGeneral->SalGetFileAttributes(fileName) != 0xFFFFFFFF) // aby sel prepsat i read-only soubor
            SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);
        HANDLE file = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE,
                                           FILE_SHARE_READ, NULL,
                                           CREATE_ALWAYS,
                                           FILE_FLAG_SEQUENTIAL_SCAN,
                                           NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            int sepLen = (int)strlen(LogsSeparator);

            HANDLES(EnterCriticalSection(&LogCritSect));
            DWORD err = NO_ERROR;
            int i;
            for (i = 0; i < Data.Count; i++)
            {
                CLogData* d = Data[i];
                if (uid == -1 || d->UID == uid)
                {
                    // zapis logu
                    ULONG written;
                    BOOL success;
                    if ((success = WriteFile(file, d->Text.GetString(), d->Text.Length, &written, NULL)) == 0 ||
                        written != (DWORD)d->Text.Length)
                    {
                        if (!success)
                            err = GetLastError();
                        else
                            err = ERROR_DISK_FULL;
                        break;
                    }

                    if (uid != -1)
                        break; // zapisujeme jen jeden log, koncime...

                    if (i + 1 < Data.Count) // zapis separatoru
                    {
                        if ((success = WriteFile(file, LogsSeparator, sepLen, &written, NULL)) == 0 ||
                            written != (DWORD)sepLen)
                        {
                            if (!success)
                                err = GetLastError();
                            else
                                err = ERROR_DISK_FULL;
                            break;
                        }
                    }
                }
            }
            HANDLES(LeaveCriticalSection(&LogCritSect));

            HANDLES(CloseHandle(file));
            SetCursor(oldCur);
            if (err != NO_ERROR) // vypis chyby
            {
                sprintf(buf, LoadStr(IDS_SAVELOGERROR), SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                DeleteFile(fileName); // pri chybe soubor smazem
            }

            // ohlasime zmenu na ceste (mozna pribyl nas soubor)
            SalamanderGeneral->CutDirectory(fileName);
            SalamanderGeneral->PostChangeOnPathNotification(fileName, FALSE);
        }
        else
        {
            DWORD err = GetLastError();
            SetCursor(oldCur);
            sprintf(buf, LoadStr(IDS_SAVELOGERROR), SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }
    }
}

void CLogs::CopyLog(HWND parent, const char* itemName, int uid)
{
    HANDLES(EnterCriticalSection(&LogCritSect));
    BOOL err = FALSE, found = FALSE;
    int i;
    for (i = 0; i < Data.Count; i++)
    {
        CLogData* d = Data[i];
        if (d->UID == uid)
        {
            err = !SalamanderGeneral->CopyTextToClipboard(d->Text.GetString(), d->Text.Length, FALSE, NULL);
            found = TRUE;
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&LogCritSect));
    if (found)
    {
        if (!err)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_TEXTCOPIEDTOCLIPBOARD),
                                             LoadStr(IDS_FTPPLUGINTITLE),
                                             MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_COPYTOCLIPBOARDERROR),
                                             LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }
    }
}

void CLogs::ClearLog(HWND parent, const char* itemName, int uid)
{
    char buf[500];
    sprintf(buf, LoadStr(IDS_CLEARLOGQUESTION), itemName);
    MSGBOXEX_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.HParent = parent;
    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
    params.Text = buf;
    if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
    {
        HANDLES(EnterCriticalSection(&LogCritSect));
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CLogData* d = Data[i];
            if (d->UID == uid)
            {
                d->Text.Clear();
                if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
                    PostMessage(LogsDlg->HWindow, WM_APP_UPDATELOG, d->UID, 0);
                break;
            }
        }
        HANDLES(LeaveCriticalSection(&LogCritSect));
    }
}

void CLogs::RemoveLog(HWND parent, const char* itemName, int uid)
{
    char buf[500];
    sprintf(buf, LoadStr(IDS_REMOVELOGQUESTION), itemName);
    MSGBOXEX_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.HParent = parent;
    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
    params.Text = buf;
    if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
    {
        HANDLES(EnterCriticalSection(&LogCritSect));
        int i;
        for (i = 0; i < Data.Count; i++)
        {
            CLogData* d = Data[i];
            if (d->UID == uid)
            {
                Data.Delete(i);
                if (!Data.IsGood())
                    Data.ResetState();
                if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
                    PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
                break;
            }
        }
        HANDLES(LeaveCriticalSection(&LogCritSect));
    }
}

void CLogs::SaveAllLogs(HWND parent)
{
    SaveLog(parent, NULL, -1);
}

void CLogs::CopyAllLogs(HWND parent)
{
    int sepLen = (int)strlen(LogsSeparator);

    HANDLES(EnterCriticalSection(&LogCritSect));
    int len = 0, i;
    for (i = 0; i < Data.Count; i++)
    {
        CLogData* d = Data[i];
        len += d->Text.Length;
    }
    if (Data.Count > 1)
        len += (Data.Count - 1) * sepLen;
    BOOL err = TRUE;
    char* txt = (char*)malloc(len + 1);
    if (txt != NULL)
    {
        char* s = txt;
        for (i = 0; i < Data.Count; i++)
        {
            CLogData* d = Data[i];
            memcpy(s, d->Text.GetString(), d->Text.Length);
            s += d->Text.Length;
            if (i + 1 < Data.Count)
            {
                memcpy(s, LogsSeparator, sepLen);
                s += sepLen;
            }
        }
        *s = 0;
        err = !SalamanderGeneral->CopyTextToClipboard(txt, len, FALSE, NULL);
        free(txt);
    }
    else
        TRACE_E(LOW_MEMORY);
    HANDLES(LeaveCriticalSection(&LogCritSect));

    if (!err)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_TEXTCOPIEDTOCLIPBOARD),
                                         LoadStr(IDS_FTPPLUGINTITLE),
                                         MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_COPYTOCLIPBOARDERROR),
                                         LoadStr(IDS_FTPERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
    }
}

void CLogs::RemoveAllLogs(HWND parent)
{
    MSGBOXEX_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.HParent = parent;
    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
    params.Text = LoadStr(IDS_REMOVEALLLOGSQUESTION);
    if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
    {
        HANDLES(EnterCriticalSection(&LogCritSect));
        Data.DestroyMembers();
        if (!Data.IsGood())
            Data.ResetState();
        if (LogsDlg != NULL && LogsDlg->HWindow != NULL)
            PostMessage(LogsDlg->HWindow, WM_APP_UPDATELISTOFLOGS, 0, 0);
        HANDLES(LeaveCriticalSection(&LogCritSect));
    }
}
