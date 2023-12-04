// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

WSADATA WinSocketsData; // info o implementaci Windows Sockets

CThreadQueue SocketsThreadQueue("FTP Sockets"); // fronta vsech threadu pouzitych ve spojitosti se sockety

CSocketsThread* SocketsThread = NULL;   // thread obsluhy vsech socketu
CRITICAL_SECTION SocketsThreadCritSect; // pro synchronizaci ukonceni threadu SocketsThread

const char* SOCKETSWINDOW_CLASSNAME = "SocketsHiddenWindowClass";

int CSocket::NextSocketUID = 0;                  // globalni pocitadlo pro objekty socketu
CRITICAL_SECTION CSocket::NextSocketUIDCritSect; // kriticka sekce pocitadla (sockety se vytvari v ruznych threadech)

#ifdef _DEBUG
BOOL InDeleteSocket = FALSE; // TRUE pokud jsme uvnitr DeleteSocket (pro test primeho volani "delete socket")
#endif

DWORD CSocketsThread::LastWM_TIMER_Processing = 0; // GetTickCount() z okamziku posledniho zpracovani WM_TIMER (WM_TIMER chodi jen pri "idle" messageloopy, coz je pro nas nepripustne)

// ***************************************************************************************

BOOL InitSockets(HWND parent)
{
    // initialize Windows Sockets library
    int err;
    if ((err = WSAStartup(MAKEWORD(1, 1), &WinSocketsData)) != 0) // error
    {
        char buf[500];
        char errBuf[300];
        sprintf(buf, LoadStr(IDS_WINSOCKETSERROR), FTPGetErrorText(err, errBuf, 300));
        MessageBox(parent, buf, LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONERROR);
        return FALSE;
    }

    HANDLES(InitializeCriticalSection(&SocketsThreadCritSect));
    HANDLES(InitializeCriticalSection(&CSocket::NextSocketUIDCritSect));

    // start Sockets thread
    SocketsThread = new CSocketsThread;
    if (SocketsThread != NULL)
    {
        if (!SocketsThread->IsGood() ||
            SocketsThread->Create(SocketsThreadQueue) == NULL)
        {
            delete SocketsThread;
            SocketsThread = NULL;
        }
        else
        {
            if (!SocketsThread->IsRunning())
                SocketsThread = NULL; // nastala chyba, thread se dealokuje sam
        }
    }
    else
        TRACE_E(LOW_MEMORY);
    if (SocketsThread == NULL)
    {
        WSACleanup();
        return FALSE;
    }

    return TRUE;
}

void ReleaseSockets()
{
    if (SocketsThread != NULL)
    {
        // musime vynulovat SocketsThread, aby ostatni thready vedeli, ze uz neni komu poslat
        // svuj vysledek (pokud je drive nesejme SocketsThreadQueue.KillAll)
        CSocketsThread* s = SocketsThread;
        HANDLES(EnterCriticalSection(&SocketsThreadCritSect));
        SocketsThread = NULL;
        HANDLES(LeaveCriticalSection(&SocketsThreadCritSect));
        HANDLE thread = s->GetHandle();
        s->Terminate();
        // dealokace objektu je automaticka (v pripade normalniho ukonceni threadu) - nelze jiz dale
        // pouzivat 's'
        CALL_STACK_MESSAGE1("ReleaseSockets(): SocketsThreadQueue.WaitForExit()");
        SocketsThreadQueue.WaitForExit(thread, INFINITE); // pockame na ukonceni threadu
    }

    // pokud nedobehly "legalne", pozabijime je
    SocketsThreadQueue.KillAll(TRUE, 0, 0);

    HANDLES(DeleteCriticalSection(&CSocket::NextSocketUIDCritSect));
    HANDLES(DeleteCriticalSection(&SocketsThreadCritSect)); // vs. thready jsou mrtve, sekci uz nikdo neuzije

    if (WSACleanup() == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
#ifdef _DEBUG // jinak hlasi kompilator warning "errBuf - unreferenced local variable"
        char errBuf[300];
        TRACE_E("Unable to release Windows Sockets: " << FTPGetErrorText(err, errBuf, 300));
#endif
    }
}

void DeleteSocket(class CSocket* socket)
{
#ifdef _DEBUG
    InDeleteSocket = TRUE;
#endif
    if (SocketsThread != NULL)
        SocketsThread->DeleteSocket(socket);
    else
    {
        if (socket != NULL)
            delete socket;
    }
#ifdef _DEBUG
    InDeleteSocket = FALSE;
#endif
}

//
// ****************************************************************************
// CSocket
//

CSocket::CSocket()
{
    HANDLES(InitializeCriticalSection(&SocketCritSect));
    HANDLES(EnterCriticalSection(&NextSocketUIDCritSect));
    UID = NextSocketUID++;
    HANDLES(LeaveCriticalSection(&NextSocketUIDCritSect));
    Msg = -1;
    Socket = INVALID_SOCKET;
    SSLConn = NULL;
    ReuseSSLSession = 0 /* zkusit */;
    ReuseSSLSessionFailed = FALSE;
    pCertificate = NULL;
    OurShutdown = FALSE;
    IsDataConnection = FALSE;
    SocketState = ssNotOpened;
    HostAddress = NULL;
    HostIP = INADDR_NONE;
    HostPort = -1;
    ProxyUser = NULL;
    ProxyPassword = NULL;
    ProxyIP = INADDR_NONE;
    ProxyErrorCode = pecNoError;
    ProxyWinError = NO_ERROR;
    ShouldPostFD_WRITE = FALSE;
    HTTP11_FirstLineOfReply = NULL;
    HTTP11_EmptyRowCharsReceived = 0;
    IsSocketConnectedLastCallTime = 0;
}

CSocket::~CSocket()
{
    HANDLES(DeleteCriticalSection(&SocketCritSect));
    if (Socket != INVALID_SOCKET)
        TRACE_E("CSocket::~CSocket(): Associated Windows socket object was not closed!");
    if (SSLConn != NULL)
        TRACE_E("CSocket::~CSocket(): Associated SSL connection was not closed!");
#ifdef _DEBUG
    if (!InDeleteSocket)
        TRACE_E("CSocket::~CSocket(): Incorrect use of operator delete, use DeleteSocket() instead.");
#endif
    if (HostAddress != NULL)
        SalamanderGeneral->Free(HostAddress);
    if (ProxyUser != NULL)
        SalamanderGeneral->Free(ProxyUser);
    if (ProxyPassword != NULL)
        SalamanderGeneral->Free(ProxyPassword);
    if (HTTP11_FirstLineOfReply != NULL)
        free(HTTP11_FirstLineOfReply);
    if (pCertificate)
        pCertificate->Release();
}

BOOL CSocket::Shutdown(DWORD* error)
{
    CALL_STACK_MESSAGE1("CSocket::Shutdown()");
    if (error != NULL)
        *error = NO_ERROR;

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL ret = FALSE;
    if (Socket != INVALID_SOCKET) // socket je pripojeny (jinak ho nelze zavrit)
    {
        if (SSLConn)
        {
            int err = SSLLib.SSL_shutdown(SSLConn);
            SSLLib.SSL_free(SSLConn);
            SSLConn = NULL;
        }
        if (shutdown(Socket, SD_SEND) != SOCKET_ERROR)
        {
            ret = TRUE; // uspech
            OurShutdown = TRUE;
        }
        else // chyba, zjistime jaka
        {
            if (error != NULL)
                *error = WSAGetLastError();
        }
    }
    else
        TRACE_I("CSocket::Shutdown(): socket is already closed.");

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

BOOL CSocket::CloseSocket(DWORD* error)
{
    CALL_STACK_MESSAGE1("CSocket::CloseSocket()");
    if (error != NULL)
        *error = NO_ERROR;

    HANDLES(EnterCriticalSection(&SocketCritSect));

    BOOL ret = FALSE;
    if (Socket != INVALID_SOCKET) // socket je pripojeny (jinak ho nelze zavrit)
    {
        if (SSLConn)
        {
            int err = SSLLib.SSL_shutdown(SSLConn);
            SSLLib.SSL_free(SSLConn);
            SSLConn = NULL;
        }
        if (closesocket(Socket) != SOCKET_ERROR)
        {
            ret = TRUE; // uspech
            Socket = INVALID_SOCKET;
            OurShutdown = FALSE;
            SocketState = ssNotOpened;
            ReuseSSLSession = 0 /* zkusit */;
            ReuseSSLSessionFailed = FALSE;
        }
        else // chyba, zjistime jaka
        {
            if (error != NULL)
                *error = WSAGetLastError();
        }
    }
    else
        TRACE_I("CSocket::CloseSocket(): socket is already closed.");

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return ret;
}

BOOL CSocket::IsConnected()
{
    CALL_STACK_MESSAGE1("CSocket::IsConnected()");
    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = Socket != INVALID_SOCKET;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

BOOL CSocket::ConnectWithProxy(DWORD serverIP, unsigned short serverPort, CFTPProxyServerType proxyType,
                               DWORD* err, const char* host, unsigned short port, const char* proxyUser,
                               const char* proxyPassword, DWORD hostIP)
{
    CALL_STACK_MESSAGE8("CSocket::ConnectWithProxy(0x%X, %u, %d, , %s, %u, %s, , 0x%X)",
                        serverIP, serverPort, proxyType, host, port, proxyUser, hostIP);
    switch (proxyType)
    {
    case fpstSocks4:
    case fpstSocks4A:
    case fpstSocks5:
    case fpstHTTP1_1:
    {
        SocketsThread->LockSocketsThread();
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (err != NULL)
            *err = NO_ERROR;
        BOOL ret = FALSE;
        if (SocketState != ssNotOpened ||
            !SetProxyData(host, port, proxyUser, proxyPassword, hostIP, err, INADDR_NONE))
        {
            if (SocketState != ssNotOpened)
                TRACE_E("CSocket::ConnectWithProxy(): SocketState != ssNotOpened");
        }
        else
        {
            switch (proxyType)
            {
            case fpstSocks4:
                SocketState = ssSocks4_Connect;
                break;
            case fpstSocks4A:
                SocketState = ssSocks4A_Connect;
                break;
            case fpstSocks5:
                SocketState = ssSocks5_Connect;
                break;
            case fpstHTTP1_1:
                SocketState = ssHTTP1_1_Connect;
                break;
            }
            if (Connect(serverIP, serverPort, err, TRUE))
                ret = TRUE;
            else
                SocketState = ssNotOpened;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        SocketsThread->UnlockSocketsThread();
        return ret;
    }

    default:
        if (!HostAddress)
        { // Needed by Certificate verification
            HostAddress = SalamanderGeneral->DupStr(host);
        }
        return Connect(serverIP, serverPort, err);
    }
}

void CSocket::SetSndRcvBuffers()
{
#ifdef DATACON_USES_OUR_BUF_SIZES
    CALL_STACK_MESSAGE1("CSocket::SetSndRcvBuffers()");

    if (IsDataConnection) // mimo data connectiony nechame defaulty
    {
        int bufSize;
        int len = sizeof(bufSize);
        if (getsockopt(Socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, &len) != 0)
            bufSize = 0;
        else
        {
            if (len != sizeof(bufSize))
            {
                TRACE_E("CSocket::SetSndRcvBuffers(): getsockopt(SO_SNDBUF) did not return int-number");
                bufSize = 0;
            }
        }
        if (bufSize < DATACON_SNDBUF_SIZE)
        {
            bufSize = DATACON_SNDBUF_SIZE;
            if (setsockopt(Socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufSize, sizeof(bufSize)) != 0)
                TRACE_E("CSocket::SetSndRcvBuffers(): setsockopt(SO_SNDBUF) failed");
        }

        len = sizeof(bufSize);
        if (getsockopt(Socket, SOL_SOCKET, SO_RCVBUF, (char*)&bufSize, &len) != 0)
            bufSize = 0;
        else
        {
            if (len != sizeof(bufSize))
            {
                TRACE_E("CSocket::SetSndRcvBuffers(): getsockopt(SO_RCVBUF) did not return int-number");
                bufSize = 0;
            }
        }
        if (bufSize < DATACON_RCVBUF_SIZE)
        {
            bufSize = DATACON_RCVBUF_SIZE;
            if (setsockopt(Socket, SOL_SOCKET, SO_RCVBUF, (char*)&bufSize, sizeof(bufSize)) != 0)
                TRACE_E("CSocket::SetSndRcvBuffers(): setsockopt(SO_RCVBUF) failed");
        }
    }
#endif // DATACON_USES_OUR_BUF_SIZES
}

BOOL CSocket::Connect(DWORD ip, unsigned short port, DWORD* error, BOOL calledFromConnect)
{
    CALL_STACK_MESSAGE4("CSocket::Connect(0x%X, %u, , %d)", ip, port, calledFromConnect);

    if (!calledFromConnect)
    {
        SocketsThread->LockSocketsThread();
        HANDLES(EnterCriticalSection(&SocketCritSect));
    }

    HWND socketsWindow = SocketsThread->GetHiddenWindow();

    /*  // zakomentovano: pokud uz jsme v CSocketsThread::CritSect, neni vnoreni do sekce SocketCritSect problem
#ifdef _DEBUG
  if (SocketCritSect.RecursionCount > 1) TRACE_E("Incorrect call to CSocket::Connect: from section SocketCritSect!");
#endif
*/
    if (error != NULL)
        *error = NO_ERROR;

    if (!calledFromConnect && SocketState != ssNotOpened || Socket != INVALID_SOCKET)
    {
        if (!calledFromConnect && SocketState != ssNotOpened)
            TRACE_E("CSocket::Connect(): SocketState != ssNotOpened");
        else
            TRACE_E("Socket is already opened!");
        if (!calledFromConnect)
        {
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            SocketsThread->UnlockSocketsThread();
        }
        return FALSE;
    }
    if (!calledFromConnect)
        SocketState = ssNoProxyOrConnected;
    ProxyErrorCode = pecNoError;
    ProxyWinError = NO_ERROR;
    ShouldPostFD_WRITE = FALSE;

    BOOL addCalled = FALSE;
    if (Msg == -1) // socket neni v SocketsThread, pridame ho
    {
        if (!SocketsThread->AddSocket(this)) // jsme v sekci CSocketsThread::CritSect, takze je tohle volani mozne i ze sekce CSocket::SocketCritSect
        {
            if (!calledFromConnect)
            {
                SocketState = ssNotOpened;
                HANDLES(LeaveCriticalSection(&SocketCritSect));
                SocketsThread->UnlockSocketsThread();
            }
            return FALSE;
        }
        addCalled = TRUE;
    }

    BOOL ret = FALSE;
    Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (Socket != INVALID_SOCKET)
    {
        // disablujeme Nagle algoritmus, nechceme zadne zbytecne cekani
        // POZOR: pri downloadu na lokalni siti dochazelo k castym vypadkum, casto se
        //        restartoval prenos, navic zrychleni jsem nepozoroval, takze jsem
        //        od tohoto radsi upustil (bylo to jeste u accept())
        // BOOL noDelayOn = TRUE;
        // setsockopt(Socket, IPPROTO_TCP, TCP_NODELAY, (char *)&noDelayOn, sizeof(noDelayOn));

        SetSndRcvBuffers();

        if (WSAAsyncSelect(Socket, socketsWindow,
                           Msg, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE) != SOCKET_ERROR)
        {
            SOCKADDR_IN addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = ip;
            if (connect(Socket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
            {
                DWORD err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) // normalni reakce na connect neblokujiciho socketu
                {
                    ret = TRUE;
                }
                else // jina chyba connectu
                {
                    if (error != NULL)
                        *error = WSAGetLastError();
                }
            }
            else // vratila NO_ERROR, tvari se jako blokujici varianta "connect", nemelo by vubec nastat
            {
                TRACE_E("CSocket::Connect(): connect has returned unexpected value!");
            }
        }
        else
        {
            if (error != NULL)
                *error = WSAGetLastError();
        }

        if (!ret)
        {
            closesocket(Socket);
            Socket = INVALID_SOCKET;
            OurShutdown = FALSE;
        }
    }
    else
    {
        if (error != NULL)
            *error = WSAGetLastError();
    }
    if (!calledFromConnect && !ret)
        SocketState = ssNotOpened;
    if (!ret && addCalled)
        SocketsThread->DeleteSocket(this, TRUE); // odpojime objekt ze SocketsThread; jsme v sekci CSocketsThread::CritSect, takze je tohle volani mozne i ze sekce CSocket::SocketCritSect
    if (!calledFromConnect)
    {
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        SocketsThread->UnlockSocketsThread();
    }
    return ret;
}

BOOL CSocket::SetProxyData(const char* hostAddress, unsigned short hostPort,
                           const char* proxyUser, const char* proxyPassword,
                           DWORD hostIP, DWORD* error, DWORD proxyIP)
{
    if (HostAddress != NULL)
        SalamanderGeneral->Free(HostAddress);
    if (ProxyUser != NULL)
        SalamanderGeneral->Free(ProxyUser);
    if (ProxyPassword != NULL)
        SalamanderGeneral->Free(ProxyPassword);

    BOOL err = GetStrOrNULL(hostAddress) == NULL;
    if (err)
        TRACE_E("CSocket::SetProxyData(): hostAddress cannot be empty!");
    HostAddress = SalamanderGeneral->DupStr(GetStrOrNULL(hostAddress));
    HostIP = hostIP;
    HostPort = hostPort;
    ProxyUser = SalamanderGeneral->DupStr(GetStrOrNULL(proxyUser));
    ProxyPassword = SalamanderGeneral->DupStr(GetStrOrNULL(proxyPassword));
    ProxyIP = proxyIP;
    if (err && error != NULL)
        *error = ERROR_NOT_ENOUGH_MEMORY;
    return !err;
}

BOOL CSocket::GetLocalIP(DWORD* ip, DWORD* error)
{
    CALL_STACK_MESSAGE1("CSocket::GetLocalIP()");

    SOCKADDR_IN addr;
    memset(&addr, 0, sizeof(addr));
    int len = sizeof(addr);
    if (error != NULL)
        *error = NO_ERROR;

    HANDLES(EnterCriticalSection(&SocketCritSect));

    if (getsockname(Socket, (SOCKADDR*)&addr, &len) != SOCKET_ERROR)
        *ip = addr.sin_addr.s_addr;
    else
    {
        if (error != NULL)
            *error = WSAGetLastError();
        *ip = INADDR_ANY;
    }

    HANDLES(LeaveCriticalSection(&SocketCritSect));

    return *ip != INADDR_ANY; // INADDR_ANY neni platne IP
}

BOOL CSocket::OpenForListening(DWORD* listenOnIP, unsigned short* listenOnPort, DWORD* error)
{
    CALL_STACK_MESSAGE3("CSocket::OpenForListening(0x%X, %u,)", *listenOnIP, *listenOnPort);

    SocketsThread->LockSocketsThread();
    HANDLES(EnterCriticalSection(&SocketCritSect));

    HWND socketsWindow = SocketsThread->GetHiddenWindow();

#ifdef _DEBUG
    if (SocketCritSect.RecursionCount > 1)
        TRACE_E("Incorrect call to CSocket::OpenForListening: from section SocketCritSect!");
#endif

    if (error != NULL)
        *error = NO_ERROR;

    if (SocketState != ssNotOpened || Socket != INVALID_SOCKET)
    {
        if (SocketState != ssNotOpened)
            TRACE_E("CSocket::OpenForListening(): SocketState != ssNotOpened!");
        else
            TRACE_E("Socket is already opened!");
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        SocketsThread->UnlockSocketsThread();
        return FALSE;
    }
    SocketState = ssNoProxyOrConnected;
    ProxyErrorCode = pecNoError;
    ProxyWinError = NO_ERROR;

    BOOL addCalled = FALSE;
    if (Msg == -1) // socket neni v SocketsThread, pridame ho
    {
        if (!SocketsThread->AddSocket(this)) // jsme v sekci CSocketsThread::CritSect, takze je tohle volani mozne i ze sekce CSocket::SocketCritSect
        {
            SocketState = ssNotOpened;
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            SocketsThread->UnlockSocketsThread();
            return FALSE;
        }
        addCalled = TRUE;
    }

    BOOL ret = FALSE;
    Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (Socket != INVALID_SOCKET)
    {
        SetSndRcvBuffers();

        if (WSAAsyncSelect(Socket, socketsWindow,
                           Msg, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE) != SOCKET_ERROR)
        {
            SOCKADDR_IN addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(*listenOnPort); // 0 = use any port (choose between 1024 and 5000)
            addr.sin_addr.s_addr = INADDR_ANY;    // INADDR_ANY = choose appropriate local address
            if (bind(Socket, (SOCKADDR*)&addr, sizeof(addr)) != SOCKET_ERROR)
            {
                int len = sizeof(addr);
                if (getsockname(Socket, (SOCKADDR*)&addr, &len) != SOCKET_ERROR)
                {
                    if (addr.sin_addr.s_addr != INADDR_ANY)
                        *listenOnIP = addr.sin_addr.s_addr;
                    *listenOnPort = ntohs(addr.sin_port);
                    if (listen(Socket, 5) != SOCKET_ERROR)
                    {
                        ret = TRUE;
                    }
                }
            }
        }

        if (!ret)
        {
            if (error != NULL)
                *error = WSAGetLastError(); // zjisti chybu posledniho volani WSA (select, bind, get, listen)

            closesocket(Socket);
            Socket = INVALID_SOCKET;
            OurShutdown = FALSE;
        }
    }
    else
    {
        if (error != NULL)
            *error = WSAGetLastError();
    }
    if (!ret)
        SocketState = ssNotOpened;
    if (!ret && addCalled)
        SocketsThread->DeleteSocket(this, TRUE); // odpojime objekt ze SocketsThread; jsme v sekci CSocketsThread::CritSect, takze je tohle volani mozne i ze sekce CSocket::SocketCritSect
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    SocketsThread->UnlockSocketsThread();
    return ret;
}

BOOL CSocket::OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                        const char* host, DWORD hostIP, unsigned short hostPort,
                                        CFTPProxyServerType proxyType, DWORD proxyIP,
                                        unsigned short proxyPort, const char* proxyUser,
                                        const char* proxyPassword, BOOL* listenError, DWORD* err)
{
    CALL_STACK_MESSAGE10("CSocket::OpenForListeningWithProxy(0x%X, %u, %s, 0x%X, %u, %d, 0x%X, %u, %s, , ,)",
                         listenOnIP, listenOnPort, host, hostIP, hostPort,
                         (int)proxyType, proxyIP, proxyPort, proxyUser);

    switch (proxyType)
    {
    case fpstSocks4:
    case fpstSocks4A:
    case fpstSocks5:
    case fpstHTTP1_1:
    {
        if (listenError != NULL)
            *listenError = FALSE;
        SocketsThread->LockSocketsThread();
        HANDLES(EnterCriticalSection(&SocketCritSect));
        if (err != NULL)
            *err = NO_ERROR;
        BOOL ret = FALSE;
        if (SocketState != ssNotOpened ||
            !SetProxyData(host, hostPort, proxyUser, proxyPassword, hostIP, err, proxyIP))
        {
            if (SocketState != ssNotOpened)
                TRACE_E("CSocket::OpenForListeningWithProxy(): SocketState != ssNotOpened");
        }
        else
        {
            switch (proxyType)
            {
            case fpstSocks4:
                SocketState = ssSocks4_Listen;
                break;
            case fpstSocks4A:
                SocketState = ssSocks4A_Listen;
                break;
            case fpstSocks5:
                SocketState = ssSocks5_Listen;
                break;
            case fpstHTTP1_1:
                SocketState = ssHTTP1_1_Listen;
                break;
            }
            if (Connect(proxyIP, proxyPort, err, TRUE))
                ret = TRUE;
            else
                SocketState = ssNotOpened;
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        SocketsThread->UnlockSocketsThread();
        return ret;
    }

    default:
    {
        if (listenError != NULL)
            *listenError = TRUE;
        BOOL ret = OpenForListening(&listenOnIP, &listenOnPort, err);
        if (ret)
            ListeningForConnection(listenOnIP, listenOnPort, FALSE);
        return ret;
    }
    }
}

class CGetHostByNameThread : public CThread
{
protected:
    char* Address; // zjistovana adresa
    int HostUID;   // hostUID z volani GetHostByAddress, ktera zalozila tento thread
    int SocketMsg; // Msg z objektu socketu, ktery ma dostat vysledek
    int SocketUID; // UID z objektu socketu, ktery ma dostat vysledek

public:
    CGetHostByNameThread(const char* address, int hostUID, int socketMsg,
                         int socketUID) : CThread("GetHostByName")
    {
        Address = SalamanderGeneral->DupStr(address);
        HostUID = hostUID;
        SocketMsg = socketMsg;
        SocketUID = socketUID;
    }

    virtual ~CGetHostByNameThread()
    {
        if (Address != NULL)
            SalamanderGeneral->Free(Address);
    }

    virtual unsigned Body()
    {
        CALL_STACK_MESSAGE2("CGetHostByNameThread::Body(%s)", Address);
        // ziskame IP adresu volanim gethostbyname
        HOSTENT* host = NULL;
        int err = 0;
        DWORD ip = INADDR_NONE; // error
        if (Address != NULL)
        {
            host = gethostbyname(Address);
            if (host == NULL)
                err = WSAGetLastError();
            else
                ip = *(DWORD*)(host->h_addr);
        }
        // posleme vysledek do threadu "sockets"
        HANDLES(EnterCriticalSection(&SocketsThreadCritSect));
        if (SocketsThread != NULL)
        {
            SocketsThread->PostHostByAddressResult(SocketMsg, SocketUID, ip, HostUID, err); // post result
        }
        HANDLES(LeaveCriticalSection(&SocketsThreadCritSect));
        return 0;
    }
};

BOOL CSocket::GetHostByAddress(const char* address, int hostUID)
{
    CALL_STACK_MESSAGE3("CSocket::GetHostByAddress(%s, %d)", address, hostUID);

    SocketsThread->LockSocketsThread();
    HANDLES(EnterCriticalSection(&SocketCritSect));
    /*  // zakomentovano: pokud uz jsme v CSocketsThread::CritSect, neni vnoreni do sekce SocketCritSect problem
#ifdef _DEBUG
  if (SocketCritSect.RecursionCount > 1) TRACE_E("Incorrect call to CSocket::GetHostByAddress: from section SocketCritSect!");
#endif
*/
    BOOL addCalled = FALSE;
    if (Msg == -1) // socket neni v SocketsThread, pridame ho
    {
        if (!SocketsThread->AddSocket(this))
        {
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            SocketsThread->UnlockSocketsThread();
            return FALSE;
        }
        addCalled = TRUE;
    }

    BOOL postMsg = FALSE; // je-li TRUE, ma se zavolat SocketsThread->PostHostByAddressResult s nasl. parametry:
    int postSocketMsg = Msg;
    int postSocketUID = UID;
    DWORD postIP;
    int postHostUID = hostUID;
    int postErr = 0;

    BOOL maybeOK = FALSE;
    DWORD ip = inet_addr(address);
    if (ip == INADDR_NONE) // nejde o IP string (aa.bb.cc.dd), zkusime gethostbyname
    {
        CGetHostByNameThread* t = new CGetHostByNameThread(address, hostUID, Msg, UID);
        if (t != NULL)
        {
            if (t->Create(SocketsThreadQueue) == NULL)
                delete t; // thread se nepustil, error
            else
                maybeOK = TRUE; // thread je spusteny, mozna se to ziskani adresy povede
        }
        else
            TRACE_E(LOW_MEMORY); // malo pameti, error
        if (!maybeOK)
        {
            postMsg = TRUE; // post error to object
            postIP = INADDR_NONE;
        }
    }
    else
    {
        maybeOK = TRUE;
        postMsg = TRUE; // post IP
        postIP = ip;
    }
    if (!maybeOK && addCalled)
        SocketsThread->DeleteSocket(this, TRUE); // odpojime objekt ze SocketsThread

    HANDLES(LeaveCriticalSection(&SocketCritSect));
    SocketsThread->UnlockSocketsThread();

    if (postMsg) // aby bylo mimo kritickou sekci SocketCritSect
    {
        SocketsThread->PostHostByAddressResult(postSocketMsg, postSocketUID, postIP, postHostUID, postErr);
    }
    return maybeOK;
}

void CSocket::ReceiveHostByAddress(DWORD ip, int hostUID, int err)
{
    /*
  CALL_STACK_MESSAGE4("CSocket::ReceiveHostByAddress(0x%X, %d, %d)", ip, hostUID, err);
  if (ip != INADDR_NONE)
  {
    in_addr addr;
    addr.s_addr = ip;
    TRACE_I("CSocket::ReceiveHostByAddress(): received IP: " << inet_ntoa(addr));
  }
  else
  {
    char buf[300];
    if (err != 0) FTPGetErrorText(err, buf, 300);
    else buf[0] = 0;
    TRACE_I("CSocket::ReceiveHostByAddress(): error: " << buf);
  }
*/
}

void CSocket::ReceiveHostByAddressInt(DWORD ip, int hostUID, int err, int index)
{
    CALL_STACK_MESSAGE5("CSocket::ReceiveHostByAddressInt(0x%X, %d, %d, %d)", ip, hostUID, err, index);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (SocketState == ssSocks4_WaitForIP)
    {
        if (ip != INADDR_NONE) // mame IP adresu FTP serveru, posleme connect-request na proxy server
        {
            HostIP = ip;
            SocketState = ssSocks4_WaitForCon;
            BOOL csLeft;
            Socks4SendRequest(1 /* connect request */, index, &csLeft, TRUE /* connect */, FALSE /* Socks 4 */);
            if (!csLeft)
                HANDLES(LeaveCriticalSection(&SocketCritSect));
        }
        else // chyba pri ziskavani IP adresy FTP serveru
        {
            ProxyErrorCode = pecGettingHostIP;
            ProxyWinError = err;
            SocketState = ssConnectFailed;
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
        }
    }
    else
    {
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        ReceiveHostByAddress(ip, hostUID, err);
    }
}

BOOL CSocket::GetProxyError(char* errBuf, int errBufSize, char* formatBuf, int formatBufSize, BOOL oneLineText)
{
    CALL_STACK_MESSAGE1("CSocket::GetProxyError(, , ,)");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = FALSE; // FALSE = neni to chyba hlasena proxy serverem (ale treba chyba pripojovani na proxy server)
    if (ProxyErrorCode != pecNoError)
    {
        ret = TRUE;
        char errText[300];
        if (ProxyErrorCode != pecUnexpectedReply && ProxyErrorCode != pecProxySrvError &&
            ProxyErrorCode != pecNoAuthUnsup && ProxyErrorCode != pecUserPassAuthUnsup &&
            ProxyErrorCode != pecUserPassAuthFail && ProxyErrorCode != pecListenUnsup &&
            ProxyErrorCode != pecHTTPProxySrvError)
        {
            if (ProxyWinError == NO_ERROR)
                lstrcpyn(errText, LoadStr(ProxyErrorCode == pecReceivingBytes ? IDS_CONNECTIONLOSTERROR : IDS_UNKNOWNERROR), 300);
            else
            {
                FTPGetErrorText(ProxyWinError, errText, 300);
                char* s = errText + strlen(errText);
                while (s > errText && (*(s - 1) == '\n' || *(s - 1) == '\r'))
                    s--;
                *s = 0; // oriznuti znaku konce radky z textu chyby
            }
        }
        else
        {
            if (ProxyErrorCode == pecHTTPProxySrvError && HTTP11_FirstLineOfReply != NULL)
            {
                char* s = HTTP11_FirstLineOfReply;
                while (*s != 0 && *s > ' ')
                    s++;
                while (*s != 0 && *s <= ' ')
                    s++;
                if (*s != 0)
                {
                    int len = (int)strlen(s);
                    if (len > 0 && s[len - 1] == '\n')
                        len--;
                    if (len > 0 && s[len - 1] == '\r')
                        len--;
                    if (len > 299)
                        len = 299;
                    memcpy(errText, s, len);
                    errText[len] = 0;
                }
                else
                    errText[0] = 0;
            }
            else
            {
                if (ProxyErrorCode == pecProxySrvError)
                    lstrcpyn(errText, LoadStr(ProxyWinError), 300);
                else
                    errText[0] = 0;
            }
        }
        if (oneLineText)
        {
            if (formatBufSize > 0)
                formatBuf[0] = 0;
            if (errBufSize > 0)
            {
                switch (ProxyErrorCode)
                {
                case pecGettingHostIP:
                case pecUnexpectedReply:
                {
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE,
                                LoadStr(ProxyErrorCode == pecUnexpectedReply ? IDS_PROXYUNEXPECTEDREPLY : IDS_PROXYERRGETIP), HostAddress, errText);
                    break;
                }

                case pecSendingBytes:
                case pecReceivingBytes:
                {
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE,
                                LoadStr(ProxyErrorCode == pecSendingBytes ? IDS_PROXYERRSENDREQ : IDS_PROXYERRRECVREP), errText);
                    break;
                }

                case pecConPrxSrvError:
                {
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_PROXYERRUNABLETOCON2), errText);
                    break;
                }

                case pecNoAuthUnsup:
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_PROXYERRNOAUTHUNSUP));
                    break;
                case pecUserPassAuthUnsup:
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_PROXYERRUSERPASSAUTHUNSUP));
                    break;
                case pecUserPassAuthFail:
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_PROXYERRUSERPASSFAIL));
                    break;
                case pecListenUnsup:
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_PROXYERRLISTENUNSUP));
                    break;

                default: // pecProxySrvError, pecHTTPProxySrvError
                {
                    _snprintf_s(errBuf, errBufSize, _TRUNCATE, LoadStr(IDS_PROXYERROPENCON), HostAddress, HostPort, errText);
                    break;
                }
                }
            }
        }
        else
        {
            if (ProxyErrorCode == pecConPrxSrvError)
                TRACE_E("CSocket::GetProxyError(): unexpected value of ProxyErrorCode: pecConPrxSrvError!");
            if (formatBufSize > 0)
            {
                _snprintf_s(formatBuf, formatBufSize, _TRUNCATE,
                            LoadStr(ProxyErrorCode == pecGettingHostIP ? IDS_GETIPERROR : ProxyErrorCode == pecSendingBytes    ? IDS_PROXYSENDREQERROR
                                                                                      : ProxyErrorCode == pecReceivingBytes    ? IDS_PROXYRECVREPERROR
                                                                                      : ProxyErrorCode == pecUnexpectedReply   ? IDS_PROXYUNEXPECTEDREPLY
                                                                                      : ProxyErrorCode == pecNoAuthUnsup       ? IDS_PROXYERRNOAUTHUNSUP
                                                                                      : ProxyErrorCode == pecUserPassAuthUnsup ? IDS_PROXYERRUSERPASSAUTHUNSUP
                                                                                      : ProxyErrorCode == pecUserPassAuthFail  ? IDS_PROXYERRUSERPASSFAIL
                                                                                      : ProxyErrorCode == pecListenUnsup       ? IDS_PROXYERRLISTENUNSUP
                                                                                                                               : IDS_PROXYOPENCONERROR), // ProxyErrorCode == pecProxySrvError nebo pecHTTPProxySrvError
                            HostAddress, HostPort);
            }
            lstrcpyn(errBuf, errText, errBufSize);
        }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

BOOL CSocket::GetProxyTimeoutDescr(char* buf, int bufSize)
{
    CALL_STACK_MESSAGE1("CSocket::GetProxyTimeoutDescr(,)");

    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = FALSE; // FALSE = timeout pripojovani na FTP server
    switch (SocketState)
    {
    case ssSocks4_Connect:
    case ssSocks4A_Connect:
    case ssSocks5_Connect:
    case ssSocks5_WaitForMeth:
    case ssSocks5_WaitForLogin:
    case ssHTTP1_1_Connect:
    case ssSocks4_Listen:
    case ssSocks4A_Listen:
    case ssSocks5_Listen:
    case ssSocks5_ListenWaitForMeth:
    case ssSocks5_ListenWaitForLogin:
    case ssHTTP1_1_Listen:
    {
        lstrcpyn(buf, LoadStr(IDS_OPENPROXYSRVCONTIMEOUT), bufSize);
        ret = TRUE;
        break;
    }

    case ssSocks4_WaitForIP:
    {
        lstrcpyn(buf, LoadStr(IDS_GETIPTIMEOUT), bufSize);
        ret = TRUE;
        break;
    }
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CSocket::ProxySendBytes(const char* buf, int bufLen, int index, BOOL* csLeft, BOOL isConnect)
{
    *csLeft = FALSE;
    if (Socket != INVALID_SOCKET && bufLen > 0)
    { // zjednodusili jsme si tu situaci - pokud se nepodari 'buf' poslat najednou, ohlasime chybu a hotovo
        // (korektni by bylo cekat na FD_WRITE a poslat zbytek bytu, ale nejspis to tu nebude potreba,
        // 'bufLen' je male cislo (cca do 1000))
        int len = 0;
        if (!SSLConn)
        {
            while (1) // cyklus nutny kvuli funkci 'send' (pokud nastane chyba "would block", ohlasi ji az v dalsim kole)
            {
                int sentLen = send(Socket, buf + len, bufLen - len, 0);
                if (sentLen != SOCKET_ERROR) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                {
                    len += sentLen;
                    if (len >= bufLen)
                        break; // prestaneme posilat (jiz neni co)
                }
                else // nastala chyba pri posilani bytu proxy serveru -> hotovo: connect failed
                {
                    ProxyErrorCode = pecSendingBytes;
                    // je-li WSAEWOULDBLOCK -> nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space), zbytek poslat az po FD_WRITE (zatim neimplementovano, snad zbytecne)
                    ProxyWinError = WSAGetLastError();
                    SocketState = isConnect ? ssConnectFailed : ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    if (isConnect)
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                    else
                        ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                    *csLeft = TRUE;
                    break;
                }
            }
        }
        else
        {
            while (1) // cyklus nutny kvuli funkci 'send' (pokud nastane chyba "would block", ohlasi ji az v dalsim kole)
            {
                int sentLen = SSLLib.SSL_write(SSLConn, buf + len, bufLen - len);
                if (sentLen > 0) // aspon neco je uspesne odeslano (nebo spis prevzato Windowsama, doruceni je ve hvezdach)
                {
                    len += sentLen;
                    if (len >= bufLen)
                        break; // prestaneme posilat (jiz neni co)
                }
                else // nastala chyba pri posilani bytu proxy serveru -> hotovo: connect failed
                {
                    ProxyErrorCode = pecSendingBytes;
                    // je-li WSAEWOULDBLOCK -> nic dalsiho uz poslat nejde (Windowsy jiz nemaji buffer space), zbytek poslat az po FD_WRITE (zatim neimplementovano, snad zbytecne)
                    ProxyWinError = SSLtoWS2Error(SSLLib.SSL_get_error(SSLConn, sentLen));
                    SocketState = isConnect ? ssConnectFailed : ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    if (isConnect)
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                    else
                        ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                    *csLeft = TRUE;
                    break;
                }
            }
        }
    }
}

void CSocket::Socks4SendRequest(int request, int index, BOOL* csLeft, BOOL isConnect, BOOL isSocks4A)
{
    *csLeft = FALSE;
    char buf[300 + HOST_MAX_SIZE];
    buf[0] = 4; // 4 = verze
    buf[1] = request;
    *(unsigned short*)(buf + 2) = htons(HostPort); // port
    if (isSocks4A && HostIP == INADDR_NONE)
        HostIP = inet_addr(HostAddress); // pokud jde o IP string, ziskame IP (nektere proxy servery tento prevod neumi)
    if (isSocks4A && HostIP == INADDR_NONE)
        *(DWORD*)(buf + 4) = 0x01000000; // SOCKS 4A "IP address": 0.0.0.x (x ma byt nenulove, tak treba 1)
    else
        *(DWORD*)(buf + 4) = HostIP; // IP address
    int len = 8;
    if (ProxyUser != NULL)
    {
        int sl = (int)strlen(ProxyUser);
        if (sl + len + 1 <= 300)
        {
            memcpy(buf + len, ProxyUser, sl);
            len += sl;
        }
    }
    buf[len++] = 0; // koncova nula
    if (isSocks4A && HostIP == INADDR_NONE)
    {
        int sl = (int)strlen(HostAddress);
        if (sl + len + 1 <= 300 + HOST_MAX_SIZE)
        {
            memcpy(buf + len, HostAddress, sl);
            len += sl;
        }
        buf[len++] = 0; // koncova nula pro SOCKS 4A
    }
    ProxySendBytes(buf, len, index, csLeft, isConnect);
}

void CSocket::Socks5SendMethods(int index, BOOL* csLeft, BOOL isConnect)
{
    *csLeft = FALSE;
    char buf[10];
    buf[0] = 5;                         // 5 = verze
    buf[1] = ProxyUser == NULL ? 1 : 2; // pocet metod
    int off = 2;
    if (ProxyUser != NULL)
        buf[off++] = 2; // "user+password"
    buf[off] = 0;       // "none" (anonymous access)
    ProxySendBytes(buf, off + 1, index, csLeft, isConnect);
}

void CSocket::Socks5SendLogin(int index, BOOL* csLeft, BOOL isConnect)
{
    *csLeft = FALSE;
    char buf[600];
    buf[0] = 1; // 1 = verze
    int userLen = (int)strlen(HandleNULLStr(ProxyUser));
    if (userLen > 255)
        userLen = 255; // delsi jmeno proste nelze zadat do SOCKS 5 requestu
    int passLen = (int)strlen(HandleNULLStr(ProxyPassword));
    if (passLen > 255)
        passLen = 255; // delsi password proste nelze zadat do SOCKS 5 requestu
    buf[1] = userLen;
    memcpy(buf + 2, HandleNULLStr(ProxyUser), userLen);
    buf[2 + userLen] = passLen;
    memcpy(buf + 3 + userLen, HandleNULLStr(ProxyPassword), passLen);
    ProxySendBytes(buf, 3 + userLen + passLen, index, csLeft, isConnect);
}

void CSocket::Socks5SendRequest(int request, int index, BOOL* csLeft, BOOL isConnect)
{
    *csLeft = FALSE;
    char buf[300];
    buf[0] = 5; // 5 = verze
    buf[1] = request;
    buf[2] = 0; // reserved
    if (HostIP == INADDR_NONE)
        HostIP = inet_addr(HostAddress); // pokud jde o IP string, ziskame IP (nektere proxy servery tento prevod neumi)
    buf[3] = HostIP == INADDR_NONE ? 3 /* name address */ : 1 /* IP address */;
    int len;
    if (HostIP == INADDR_NONE) // nemame IP adresu, pouzijeme jmennou adresu
    {
        len = (int)strlen(HostAddress);
        if (len > 255)
            len = 255; // delsi jmenne adresy proste nelze zadat do SOCKS 5 requestu
        buf[4] = (unsigned char)len;
        memcpy(buf + 5, HostAddress, len);
        len++; // za byte s velikosti adresy
    }
    else
    {
        len = 4;
        *(DWORD*)(buf + 4) = HostIP; // IP address
    }
    *(unsigned short*)(buf + 4 + len) = htons(HostPort); // port
    ProxySendBytes(buf, 6 + len, index, csLeft, isConnect);
}

const char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void EncodeToBase64(char* buf, const char* txt)
{
    const unsigned char* str = (const unsigned char*)txt;
    int len = (int)strlen(txt);
    char* b = buf;
    //  int eol = 0;
    while (len > 2)
    {
        *b++ = Base64Table[str[0] >> 2];
        *b++ = Base64Table[((str[0] & 0x03) << 4) + (str[1] >> 4)];
        *b++ = Base64Table[((str[1] & 0x0F) << 2) + (str[2] >> 6)];
        *b++ = Base64Table[(str[2] & 0x3F)];
        /*  // zakomentovano, protoze napr. WinGate s user+pass na vice radek neumi delat
    if (len > 3 && ++eol % 18 == 0)  // 72 znaku na radek
    {
      *b++ = '\r';
      *b++ = '\n';
    }
*/
        str += 3;
        len -= 3;
    }
    if (len != 0)
    {
        *b++ = Base64Table[str[0] >> 2];
        *b++ = Base64Table[((str[0] & 0x03) << 4) + (str[1] >> 4)];
        if (len == 1)
            *b++ = '=';
        else
            *b++ = Base64Table[((str[1] & 0x0f) << 2)];
        *b++ = '=';
    }
    *b = 0;
}

void CSocket::HTTP11SendRequest(int index, BOOL* csLeft)
{
    *csLeft = FALSE;
    char buf[2200];
    char passwordPart[1500];
    if (ProxyUser != NULL || ProxyPassword != NULL)
    {
        char login[500];
        _snprintf_s(login, _TRUNCATE, "%s:%s", HandleNULLStr(ProxyUser), HandleNULLStr(ProxyPassword));
        char loginInBase64[700]; // 4/3 * 500 + ((4/3 * 500) / 72) * 2 = 686 (zvetseni je 4/3 + EOL po kazdych 72 znacich)
        EncodeToBase64(loginInBase64, login);

        _snprintf_s(passwordPart, _TRUNCATE, "Authorization: Basic %s\r\nProxy-Authorization: Basic %s\r\n\r\n",
                    loginInBase64, loginInBase64);
    }
    else
        strcpy(passwordPart, "\r\n");
    _snprintf_s(buf, _TRUNCATE, "CONNECT %s:%u HTTP/1.1\r\nHost: %s:%u\r\n%s",
                HostAddress, HostPort, HostAddress, HostPort, passwordPart);
    ProxySendBytes(buf, (int)strlen(buf), index, csLeft, TRUE /* connect */);
}

BOOL CSocket::ProxyReceiveBytes(LPARAM lParam, char* buf, int* read, int index, BOOL isConnect,
                                BOOL isListen, BOOL readOnlyToEOL)
{
    DWORD event = WSAGETSELECTEVENT(lParam);
    BOOL ret = FALSE;
    if (event == FD_READ || event == FD_CLOSE) // FD_CLOSE nekdy chodi pred poslednim FD_READ, nezbyva tedy nez napred zkusit FD_READ a pokud uspeje, poslat si FD_CLOSE znovu (muze pred nim znovu uspet FD_READ)
    {
        BOOL reportError = FALSE;
        DWORD err = WSAGETSELECTERROR(lParam);
        if (err == NO_ERROR)
        {
            if (Socket != INVALID_SOCKET) // socket je pripojeny
            {
                int len;
                if (!readOnlyToEOL)
                    len = recv(Socket, buf, *read, 0);
                else
                {
                    int i;
                    for (i = 0; i < *read; i++)
                    {
                        int r = recv(Socket, buf + i, 1, 0);
                        if (r != SOCKET_ERROR)
                        {
                            if (buf[i] == '\n')
                            {
                                len = i + 1;
                                break; // nactenim LF koncime
                            }
                        }
                        else
                        {
                            if (i > 0 && WSAGetLastError() == WSAEWOULDBLOCK)
                                len = i; // jen uz neni co dal cist
                            else
                                len = SOCKET_ERROR; // stala se nejaka chyba nebo jsme vubec nic neprecetli
                            break;
                        }
                    }
                    if (i == *read)
                        len = i; // cteni skoncilo kvuli omezene velikosti bufferu
                }
                if (len != SOCKET_ERROR) // mozna jsme neco precetli (0 = spojeni uz je zavrene)
                {
                    if (len > 0)
                    {
                        *read = len;
                        ret = TRUE;
                    }
                    else
                    {
                        if (event == FD_CLOSE)
                        {
                            ProxyErrorCode = pecReceivingBytes;
                            ProxyWinError = NO_ERROR; // text IDS_CONNECTIONLOSTERROR
                            SocketState = isConnect ? ssConnectFailed : ssListenFailed;
                            reportError = TRUE;
                        }
                    }
                }
                else
                {
                    err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK) // pokud nejde o chybu "neni co cist"
                    {
                        ProxyErrorCode = pecReceivingBytes;
                        ProxyWinError = err;
                        SocketState = isConnect ? ssConnectFailed : ssListenFailed;
                        reportError = TRUE;
                    }
                }
            }
            if (!ret && !reportError)
                HANDLES(LeaveCriticalSection(&SocketCritSect));
        }
        else
        {
            ProxyErrorCode = pecReceivingBytes;
            ProxyWinError = err;
            SocketState = isConnect ? ssConnectFailed : ssListenFailed;
            reportError = TRUE;
        }
        if (reportError)
        {
            if (isConnect)
            {
                HANDLES(LeaveCriticalSection(&SocketCritSect));
                ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
            }
            else
            {
                if (isListen)
                {
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                }
                else
                {
                    ConnectionAccepted(FALSE, NO_ERROR, TRUE /* proxy error */);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                }
            }
        }
    }
    else
        HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

DWORD GetSOCKS4ErrDescr(char replyCode)
{
    switch (replyCode)
    {
    case 92:
        return IDS_PROXYERRNEEDIDENTD; // request rejected becasue SOCKS server cannot connect to identd on the client
    case 93:
        return IDS_PROXYERRDIFFUSERS; // request rejected because the client program and identd report different user-ids
    // case 91: // rejected of failed
    default:
        return IDS_PROXYERRREJORFAIL;
    }
}

DWORD GetSOCKS5ErrDescr(char replyCode)
{
    switch (replyCode)
    {
    case 2:
        return IDS_PROXYERRRULESET; // connection not allowed by ruleset
    case 3:
        return IDS_PROXYERRNETUNR; // Network unreachable
    case 4:
        return IDS_PROXYERRHOSTUNR; // Host unreachable
    case 5:
        return IDS_PROXYERRCONREF; // Connection refused
    case 6:
        return IDS_PROXYERRTTLEXP; // TTL expired
    case 7:
        return IDS_PROXYERRCMDNOTSUP; // Command not supported
    case 8:
        return IDS_PROXYERRADRTYPEUNSUP; // Address type not supported
    // case 1: // general SOCKS server failure
    default:
        return IDS_PROXYERRGENFAIL;
    }
}

void CSocket::ReceiveNetEventInt(LPARAM lParam, int index)
{
    //  CALL_STACK_MESSAGE3("CSocket::ReceiveNetEventInt(0x%IX, %d)", lParam, index);

    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (SocketState == ssNoProxyOrConnected)
    {
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        ReceiveNetEvent(lParam, index); // bezny provoz presmerujeme do ReceiveNetEvent()
    }
    else
    {
        char buf[200];
        DWORD err = WSAGETSELECTERROR(lParam);
        DWORD event = WSAGETSELECTEVENT(lParam);
        if (event == FD_WRITE)
            ShouldPostFD_WRITE = TRUE;
        switch (SocketState)
        {
        case ssNotOpened: // Socket by se mel rovnat INVALID_SOCKET, takze by se to sem vubec nemelo dostat
        {                 // muze se stat: na zacatku metody je vstup do kriticke sekce, pred vstupem je v kriticke sekci jiny thread, ktery nastavi Socket na INVALID_SOCKET a SocketState na ssNotOpened
            // TRACE_E("CSocket::ReceiveNetEventInt(): unexpected situation: called when SocketState == ssNotOpened");
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            break;
        }

        case ssSocks4_Connect:
        case ssSocks4A_Connect:
        case ssSocks5_Connect:
        {
            if (event == FD_CONNECT)
            {
                if (err != NO_ERROR) // chyba pripojovani na proxy server => hotovo: connect failed
                {
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, err), index);
                }
                else // jsme pripojeny na proxy server
                {
                    if (SocketState == ssSocks5_Connect) // SOCKS 5
                    {
                        SocketState = ssSocks5_WaitForMeth;
                        BOOL csLeft;
                        Socks5SendMethods(index, &csLeft, TRUE /* connect */);
                        if (!csLeft)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else
                    {
                        if (SocketState == ssSocks4A_Connect) // SOCKS 4A
                        {
                            SocketState = ssSocks4A_WaitForCon;
                            BOOL csLeft;
                            Socks4SendRequest(1 /* connect request */, index, &csLeft, TRUE /* connect */, TRUE /* Socks 4A */);
                            if (!csLeft)
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                        else // SOCKS 4
                        {
                            if (HostIP != INADDR_NONE) // mame IP adresu FTP serveru, posleme CONNECT request
                            {
                                SocketState = ssSocks4_WaitForCon;
                                BOOL csLeft;
                                Socks4SendRequest(1 /* connect request */, index, &csLeft, TRUE /* connect */, FALSE /* Socks 4 */);
                                if (!csLeft)
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                            }
                            else // provedeme nejdrive preklad host-adresy na IP
                            {
                                // uz jsme v sekci CSocketsThread::CritSect, takze je mozne volat tuhle metodu i ze sekce SocketCritSect
                                if (!GetHostByAddress(HostAddress, 0)) // chyba zjistovani IP => hotovo: connect failed
                                {
                                    ProxyErrorCode = pecGettingHostIP;
                                    ProxyWinError = NO_ERROR;
                                    SocketState = ssConnectFailed;
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                                }
                                else
                                {
                                    SocketState = ssSocks4_WaitForIP;
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                }
                            }
                        }
                    }
                }
            }
            else
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            break;
        }

        case ssSocks4_WaitForIP:
        {
            if (event == FD_CLOSE) // spojeni s proxy serverm se zavrelo => hotovo: connect failed
            {
                SocketState = ssConnectFailed;
                HANDLES(LeaveCriticalSection(&SocketCritSect));
                if (err == NO_ERROR)
                    err = WSAECONNABORTED; // potrebujeme vypsat nejakou chybu, uspech connectu to proste byt nemuze
                ReceiveNetEvent(MAKELPARAM(FD_CONNECT, err), index);
            }
            else
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            break; // v tomto stavu cekame na IP v ReceiveHostByAddressInt (timeout neresime, staci globalni timeout pro prijeti FD_CONNECT)
        }

        case ssSocks4_WaitForCon:  // cekame na vysledek pozadavku na spojeni s FTP serverem (timeout neresime, staci globalni timeout pro prijeti FD_CONNECT)
        case ssSocks4A_WaitForCon: // cekame na vysledek pozadavku na spojeni s FTP serverem (timeout neresime, staci globalni timeout pro prijeti FD_CONNECT)
        {
            int read = 8;
            if (ProxyReceiveBytes(lParam, buf, &read, index, TRUE /* connect */, FALSE, FALSE))
            {                                 // zpracujeme odpoved SOCKS4 proxy serveru
                if (read == 8 && buf[0] == 0) // odpoved by mela mit 8 bytu + "version" by mela byt 0
                {
                    if (buf[1] == 90) // reply code == uspech
                    {
                        SocketState = ssNoProxyOrConnected;
                        if (ShouldPostFD_WRITE) // aby ReceiveNetEvent() dostal take FD_WRITE (FD_READ se vygeneroval/vygeneruje sam, ten postit nemusime)
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, MAKELPARAM(FD_WRITE, NO_ERROR));
                        }
                        if (event == FD_CLOSE) // tenhle close nastal az po uspesnem pripojeni na FTP server, ten se zpracuje pozdeji...
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, lParam);
                        }
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, NO_ERROR), index); // ohlasime uspesny connect
                    }
                    else // chyba
                    {
                        ProxyErrorCode = pecProxySrvError;
                        ProxyWinError = GetSOCKS4ErrDescr(buf[1] /* reply code */);
                        SocketState = ssConnectFailed;
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks5_WaitForMeth: // cekame jakou metodu autentifikace si server vybere
        {
            int read = 2;
            if (ProxyReceiveBytes(lParam, buf, &read, index, TRUE /* connect */, FALSE, FALSE))
            {                  // zpracujeme prvni odpoved SOCKS5 proxy serveru
                if (read == 2) // odpoved by mela mit 2 byty (prvni byte je verze, ale nepisou na co ma byt nastavena, tak ji ignorujeme)
                {
                    if (buf[1] == 0 /* anonymous */)
                    {
                        SocketState = ssSocks5_WaitForCon;
                        BOOL csLeft;
                        Socks5SendRequest(1 /* connect request */, index, &csLeft, TRUE /* connect */);
                        if (!csLeft)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else
                    {
                        if (buf[1] == 2 /* user+password */ && ProxyUser != NULL)
                        {
                            SocketState = ssSocks5_WaitForLogin;
                            BOOL csLeft;
                            Socks5SendLogin(index, &csLeft, TRUE /* connect */);
                            if (!csLeft)
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                        else // chyba
                        {
                            // WinGate neposila 0xFF, ale vzdy 0/2 podle toho jestli chce user+password,
                            // takze tenhle test skipneme: if (buf[1] == 0xFF)  // no acceptable method
                            ProxyErrorCode = ProxyUser == NULL ? pecNoAuthUnsup : pecUserPassAuthUnsup;
                            SocketState = ssConnectFailed;
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                            ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                        }
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks5_WaitForLogin: // cekame na vysledek loginu na proxy server (poslali jsme user+password)
        {
            int read = 2;
            if (ProxyReceiveBytes(lParam, buf, &read, index, TRUE /* connect */, FALSE, FALSE))
            {                  // zpracujeme odpoved SOCKS5 proxy serveru na user+password
                if (read == 2) // odpoved by mela mit 2 byty (prvni byte je verze, ale nepisou na co ma byt nastavena, tak ji ignorujeme)
                {
                    if (buf[1] == 0) // uspech
                    {
                        SocketState = ssSocks5_WaitForCon;
                        BOOL csLeft;
                        Socks5SendRequest(1 /* connect request */, index, &csLeft, TRUE /* connect */);
                        if (!csLeft)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else // server nas odmitnul, koncime
                    {
                        ProxyErrorCode = pecUserPassAuthFail;
                        SocketState = ssConnectFailed;
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks5_WaitForCon: // cekame na vysledek pozadavku na spojeni s FTP serverem
        {
            int read = 10;
            if (ProxyReceiveBytes(lParam, buf, &read, index, TRUE /* connect */, FALSE, FALSE))
            {                                                 // zpracujeme odpoved SOCKS5 proxy serveru na CONNECT request
                if (read == 10 && buf[0] == 5 && buf[3] == 1) // odpoved by mela mit 10 bytu + "version" by mela byt 5 + address type by mel byt 1 (IPv4)
                {
                    if (buf[1] == 0) // reply code == uspech
                    {
                        SocketState = ssNoProxyOrConnected;
                        if (ShouldPostFD_WRITE) // aby ReceiveNetEvent() dostal take FD_WRITE (FD_READ se vygeneroval/vygeneruje sam, ten postit nemusime)
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, MAKELPARAM(FD_WRITE, NO_ERROR));
                        }
                        if (event == FD_CLOSE) // tenhle close nastal az po uspesnem pripojeni na FTP server, ten se zpracuje pozdeji...
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, lParam);
                        }
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, NO_ERROR), index); // ohlasime uspesny connect
                    }
                    else // chyba
                    {
                        ProxyErrorCode = pecProxySrvError;
                        ProxyWinError = GetSOCKS5ErrDescr(buf[1] /* reply code */);
                        SocketState = ssConnectFailed;
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks4_Listen:
        case ssSocks4A_Listen:
        case ssSocks5_Listen:
        {
            if (event == FD_CONNECT)
            {
                if (err != NO_ERROR) // chyba pripojovani na proxy server => hotovo: listen failed
                {
                    ProxyErrorCode = pecConPrxSrvError;
                    ProxyWinError = err;
                    SocketState = ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                }
                else // jsme pripojeny na proxy server, posleme LISTEN request
                {
                    if (SocketState == ssSocks5_Listen) // SOCKS 5
                    {
                        SocketState = ssSocks5_ListenWaitForMeth;
                        BOOL csLeft;
                        Socks5SendMethods(index, &csLeft, FALSE /* listen */);
                        if (!csLeft)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else
                    {
                        if (SocketState == ssSocks4A_Listen) // SOCKS 4A
                        {
                            SocketState = ssSocks4A_WaitForListenRes;
                            BOOL csLeft;
                            Socks4SendRequest(2 /* bind (listen) request */, index, &csLeft, FALSE /* listen */, TRUE /* Socks 4A */);
                            if (!csLeft)
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                        else // SOCKS 4
                        {
                            SocketState = ssSocks4_WaitForListenRes;
                            BOOL csLeft;
                            Socks4SendRequest(2 /* bind (listen) request */, index, &csLeft, FALSE /* listen */, FALSE /* Socks 4 */);
                            if (!csLeft)
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                    }
                }
            }
            else
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            break;
        }

        case ssSocks5_ListenWaitForMeth: // cekame jakou metodu autentifikace si server vybere
        {
            int read = 2;
            if (ProxyReceiveBytes(lParam, buf, &read, index, FALSE, TRUE /* listen */, FALSE))
            {                  // zpracujeme prvni odpoved SOCKS5 proxy serveru
                if (read == 2) // odpoved by mela mit 2 byty (prvni byte je verze, ale nepisou na co ma byt nastavena, tak ji ignorujeme)
                {
                    if (buf[1] == 0 /* anonymous */)
                    {
                        SocketState = ssSocks5_WaitForListenRes;
                        BOOL csLeft;
                        Socks5SendRequest(2 /* bind (listen) request */, index, &csLeft, FALSE /* listen */);
                        if (!csLeft)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else
                    {
                        if (buf[1] == 2 /* user+password */ && ProxyUser != NULL)
                        {
                            SocketState = ssSocks5_ListenWaitForLogin;
                            BOOL csLeft;
                            Socks5SendLogin(index, &csLeft, FALSE /* listen */);
                            if (!csLeft)
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                        }
                        else // chyba
                        {
                            if (buf[1] == 0xFF) // no acceptable method
                                ProxyErrorCode = ProxyUser == NULL ? pecNoAuthUnsup : pecUserPassAuthUnsup;
                            else
                                ProxyErrorCode = pecUnexpectedReply; // neocekavana odpoved (server vybral jinou metodu nez jsme pozadovali)
                            SocketState = ssListenFailed;
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                            ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                        }
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks5_ListenWaitForLogin: // cekame na vysledek loginu na proxy server (poslali jsme user+password)
        {
            int read = 2;
            if (ProxyReceiveBytes(lParam, buf, &read, index, FALSE, TRUE /* listen */, FALSE))
            {                  // zpracujeme odpoved SOCKS5 proxy serveru na user+password
                if (read == 2) // odpoved by mela mit 2 byty (prvni byte je verze, ale nepisou na co ma byt nastavena, tak ji ignorujeme)
                {
                    if (buf[1] == 0) // uspech
                    {
                        SocketState = ssSocks5_WaitForListenRes;
                        BOOL csLeft;
                        Socks5SendRequest(2 /* bind (listen) request */, index, &csLeft, FALSE /* listen */);
                        if (!csLeft)
                            HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else // server nas odmitnul, koncime
                    {
                        ProxyErrorCode = pecUserPassAuthFail;
                        SocketState = ssListenFailed;
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks4_WaitForListenRes:  // cekame az proxy otevre port pro "listen" a vrati IP+port, kde posloucha nebo vrati chybu (timeout by se mel resit venku - timeout pro volani ListeningForConnection())
        case ssSocks4A_WaitForListenRes: // cekame az proxy otevre port pro "listen" a vrati IP+port, kde posloucha nebo vrati chybu (timeout by se mel resit venku - timeout pro volani ListeningForConnection())
        {
            int read = 8;
            if (ProxyReceiveBytes(lParam, buf, &read, index, FALSE, TRUE /* listen */, FALSE))
            {                                 // zpracujeme odpoved SOCKS4 proxy serveru
                if (read == 8 && buf[0] == 0) // odpoved by mela mit 8 bytu + "version" by mela byt 0
                {
                    if (buf[1] == 90) // reply code == uspech
                    {
                        if (SocketState == ssSocks4A_WaitForListenRes)
                            SocketState = ssSocks4A_WaitForAccept;
                        else
                            SocketState = ssSocks4_WaitForAccept;
                        DWORD ip = *(DWORD*)(buf + 4);
                        if (ip == 0)
                            ip = ProxyIP; // proxy server si preje, abysme zde pouzili jeho IP adresu
                        int port = ntohs(*(unsigned short*)(buf + 2));
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ListeningForConnection(ip, port, FALSE); // ohlasime ip+port, kde se ceka na spojeni z FTP serveru
                    }
                    else // chyba
                    {
                        ProxyErrorCode = pecProxySrvError;
                        ProxyWinError = GetSOCKS4ErrDescr(buf[1] /* reply code */);
                        SocketState = ssListenFailed;
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks5_WaitForListenRes: // cekame az proxy otevre port pro "listen" a vrati IP+port, kde posloucha nebo vrati chybu (timeout by se mel resit venku - timeout pro volani ListeningForConnection())
        {
            int read = 10;
            if (ProxyReceiveBytes(lParam, buf, &read, index, FALSE, TRUE /* listen */, FALSE))
            {                                                 // zpracujeme odpoved SOCKS5 proxy serveru
                if (read == 10 && buf[0] == 5 && buf[3] == 1) // odpoved by mela mit 10 bytu + "version" by mela byt 5 + address type by mel byt 1 (IPv4)
                {
                    if (buf[1] == 0) // reply code == uspech
                    {
                        SocketState = ssSocks5_WaitForAccept;
                        DWORD ip = *(DWORD*)(buf + 4);
                        if (ip == 0)
                            ip = ProxyIP; // neni v dokumentaci, ale "antinat" vraci nulu = zrejme ocekava stejne chovani jako SOCKS4/4A: proxy server si preje, abysme zde pouzili jeho IP adresu
                        int port = ntohs(*(unsigned short*)(buf + 8));
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ListeningForConnection(ip, port, FALSE); // ohlasime ip+port, kde se ceka na spojeni z FTP serveru
                    }
                    else // chyba
                    {
                        ProxyErrorCode = pecProxySrvError;
                        ProxyWinError = GetSOCKS5ErrDescr(buf[1] /* reply code */);
                        SocketState = ssListenFailed;
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                        ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssListenFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks4_WaitForAccept:  // cekame az proxy prijme spojeni z FTP serveru nebo vrati chybu (timeout neresime, staci globalni timeout pro metodu ConnectionAccepted())
        case ssSocks4A_WaitForAccept: // cekame az proxy prijme spojeni z FTP serveru nebo vrati chybu (timeout neresime, staci globalni timeout pro metodu ConnectionAccepted())
        {
            int read = 8;
            if (ProxyReceiveBytes(lParam, buf, &read, index, FALSE, FALSE /* accept */, FALSE))
            {                                 // zpracujeme odpoved SOCKS4 proxy serveru
                if (read == 8 && buf[0] == 0) // odpoved by mela mit 8 bytu + "version" by mela byt 0
                {
                    if (buf[1] == 90) // reply code == uspech
                    {
                        SocketState = ssNoProxyOrConnected;
                        if (ShouldPostFD_WRITE) // aby ReceiveNetEvent() dostal take FD_WRITE (FD_READ se vygeneroval/vygeneruje sam, ten postit nemusime)
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, MAKELPARAM(FD_WRITE, NO_ERROR));
                        }
                        if (event == FD_CLOSE) // tenhle close nastal az po uspesnem pripojeni z FTP serveru, ten se zpracuje pozdeji...
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, lParam);
                        }
                        ConnectionAccepted(TRUE, NO_ERROR, FALSE); // ohlasime uspesne navazani spojeni
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else // chyba - connectiona z neznameho IP
                    {
                        ProxyErrorCode = pecProxySrvError;
                        ProxyWinError = IDS_PROXYERRINVHOST;
                        SocketState = ssListenFailed;
                        ConnectionAccepted(FALSE, NO_ERROR, TRUE /* proxy error */);
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssListenFailed;
                    ConnectionAccepted(FALSE, NO_ERROR, TRUE /* proxy error */);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssSocks5_WaitForAccept: // cekame az proxy prijme spojeni z FTP serveru (nebo vrati chybu)
        {
            int read = 10;
            if (ProxyReceiveBytes(lParam, buf, &read, index, FALSE, FALSE /* accept */, FALSE))
            {                                                 // zpracujeme odpoved SOCKS5 proxy serveru
                if (read == 10 && buf[0] == 5 && buf[3] == 1) // odpoved by mela mit 10 bytu + "version" by mela byt 5 + address type by mel byt 1 (IPv4)
                {
                    if (buf[1] == 0) // reply code == uspech
                    {
                        SocketState = ssNoProxyOrConnected;
                        if (ShouldPostFD_WRITE) // aby ReceiveNetEvent() dostal take FD_WRITE (FD_READ se vygeneroval/vygeneruje sam, ten postit nemusime)
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, MAKELPARAM(FD_WRITE, NO_ERROR));
                        }
                        if (event == FD_CLOSE) // tenhle close nastal az po uspesnem pripojeni z FTP serveru, ten se zpracuje pozdeji...
                        {
                            PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                        (WPARAM)Socket, lParam);
                        }
                        ConnectionAccepted(TRUE, NO_ERROR, FALSE); // ohlasime uspesne navazani spojeni
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                    else // chyba
                    {
                        ProxyErrorCode = pecProxySrvError;
                        ProxyWinError = GetSOCKS5ErrDescr(buf[1] /* reply code */);
                        SocketState = ssListenFailed;
                        ConnectionAccepted(FALSE, NO_ERROR, TRUE /* proxy error */);
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                    }
                }
                else
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssListenFailed;
                    ConnectionAccepted(FALSE, NO_ERROR, TRUE /* proxy error */);
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                }
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssHTTP1_1_Connect:
        {
            if (event == FD_CONNECT)
            {
                if (err != NO_ERROR) // chyba pripojovani na proxy server => hotovo: connect failed
                {
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, err), index);
                }
                else // jsme pripojeny na proxy server
                {
                    SocketState = ssHTTP1_1_WaitForCon;
                    if (HTTP11_FirstLineOfReply != NULL)
                    {
                        free(HTTP11_FirstLineOfReply);
                        HTTP11_FirstLineOfReply = NULL;
                    }
                    HTTP11_EmptyRowCharsReceived = 0;
                    BOOL csLeft;
                    HTTP11SendRequest(index, &csLeft);
                    if (!csLeft)
                        HANDLES(LeaveCriticalSection(&SocketCritSect));
                }
            }
            else
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            break;
        }

        case ssHTTP1_1_WaitForCon: // HTTP 1.1 - CONNECT: cekame na vysledek pozadavku na spojeni s FTP serverem
        {
            int read = 200;
            if (ProxyReceiveBytes(lParam, buf, &read, index, TRUE /* connect */, FALSE,
                                  TRUE /* read only to first LF */) &&
                read > 0)
            { // zpracujeme dalsi cast odpovedi HTTP1.1 proxy serveru na CONNECT request
                if (HTTP11_EmptyRowCharsReceived > 0 && HTTP11_EmptyRowCharsReceived != 4)
                {
                    const char* endline = "\r\n\r\n" + HTTP11_EmptyRowCharsReceived;
                    int i;
                    for (i = 0; i < read; i++)
                    {
                        if (*endline++ == buf[i])
                        {
                            if (++HTTP11_EmptyRowCharsReceived == 4)
                                break;
                        }
                        else
                        {
                            HTTP11_EmptyRowCharsReceived = 0;
                            break;
                        }
                    }
                }
                if (HTTP11_EmptyRowCharsReceived == 0)
                {
                    if (buf[read - 1] == '\r')
                        HTTP11_EmptyRowCharsReceived = 1;
                    if (read > 1 && buf[read - 2] == '\r' && buf[read - 1] == '\n')
                        HTTP11_EmptyRowCharsReceived = 2;
                }
                BOOL unexpReply = FALSE;
                int len = HTTP11_FirstLineOfReply != NULL ? (int)strlen(HTTP11_FirstLineOfReply) : 0;
                if (len == 0 || HTTP11_FirstLineOfReply[len - 1] != '\n') // jen pokud jeste neni nactena cela prvni radka
                {
                    char* newStr = (char*)malloc(len + read + 1);
                    if (newStr != NULL)
                    {
                        if (len > 0)
                            memcpy(newStr, HTTP11_FirstLineOfReply, len);
                        memcpy(newStr + len, buf, read);
                        newStr[len + read] = 0;
                        len = len + read;
                        if (HTTP11_FirstLineOfReply != NULL)
                            free(HTTP11_FirstLineOfReply);
                        HTTP11_FirstLineOfReply = newStr;
                    }
                    else
                    {
                        TRACE_E(LOW_MEMORY);
                        unexpReply = TRUE; // nizka pravdepodobnost chyby, simulujeme spatnou odpoved serveru...
                    }
                }

                BOOL csLeft = FALSE;
                if (!unexpReply && len > 0 && HTTP11_FirstLineOfReply[len - 1] == '\n')
                { // pokud uz je nacteny cely prvni radek odpovedi
                    if (_strnicmp("HTTP/", HTTP11_FirstLineOfReply, 5) == 0)
                    {
                        char* s = HTTP11_FirstLineOfReply + 5;
                        while (*s != 0 && *s > ' ')
                            s++;
                        while (*s != 0 && *s <= ' ')
                            s++;
                        if (*s != 0)
                        {
                            if (*s == '2') // uspech
                            {
                                if (HTTP11_EmptyRowCharsReceived == 4) // uz jsme precetli i konec odpovedi (prazdny radek)
                                {
                                    // pri uspesnem pripojeni zahodime text prvniho radku odpovedi, neni dale uz k nicemu
                                    if (HTTP11_FirstLineOfReply != NULL)
                                    {
                                        free(HTTP11_FirstLineOfReply);
                                        HTTP11_FirstLineOfReply = NULL;
                                    }

                                    SocketState = ssNoProxyOrConnected;
                                    if (ShouldPostFD_WRITE) // aby ReceiveNetEvent() dostal take FD_WRITE (FD_READ se vygeneroval/vygeneruje sam, ten postit nemusime)
                                    {
                                        PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                                    (WPARAM)Socket, MAKELPARAM(FD_WRITE, NO_ERROR));
                                    }
                                    if (event == FD_CLOSE) // tenhle close nastal az po uspesnem pripojeni na FTP server, ten se zpracuje pozdeji...
                                    {
                                        PostMessage(SocketsThread->GetHiddenWindow(), WM_APP_SOCKET_MIN + index,
                                                    (WPARAM)Socket, lParam);
                                    }
                                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                                    csLeft = TRUE;
                                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, NO_ERROR), index); // ohlasime uspesny connect
                                }
                            }
                            else // proxy server hlasi chybu
                            {
                                ProxyErrorCode = pecHTTPProxySrvError; // hlasku si to vezme z HTTP11_FirstLineOfReply
                                SocketState = ssConnectFailed;
                                HANDLES(LeaveCriticalSection(&SocketCritSect));
                                csLeft = TRUE;
                                ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                            }
                        }
                        else
                            unexpReply = TRUE;
                    }
                    else
                        unexpReply = TRUE;
                }
                if (unexpReply)
                {
                    ProxyErrorCode = pecUnexpectedReply;
                    SocketState = ssConnectFailed;
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
                    csLeft = TRUE;
                    ReceiveNetEvent(MAKELPARAM(FD_CONNECT, ERROR_INVALID_FUNCTION /* jen nesmi byt NO_ERROR */), index);
                }
                if (!csLeft)
                    HANDLES(LeaveCriticalSection(&SocketCritSect));
            }
            // else HANDLES(LeaveCriticalSection(&SocketCritSect));  // ProxyReceiveBytes() vraci FALSE = sekce SocketCritSect uz byla opustena
            break;
        }

        case ssHTTP1_1_Listen:
        {
            if (event == FD_CONNECT)
            {
                ProxyErrorCode = pecListenUnsup;
                SocketState = ssListenFailed;
                HANDLES(LeaveCriticalSection(&SocketCritSect));
                ListeningForConnection(INADDR_NONE, 0, TRUE /* proxy error */);
            }
            else
                HANDLES(LeaveCriticalSection(&SocketCritSect));
            break;
        }

        case ssConnectFailed:
        case ssListenFailed:
        {
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            break; // chybu connectu jsme jiz ohlasili, uz nic nedelame
        }

        default:
        {
            TRACE_E("CSocket::ReceiveNetEventInt(): unknown SocketState: " << SocketState);
            HANDLES(LeaveCriticalSection(&SocketCritSect));
            break;
        }
        }
    }
}

void CSocket::ReceiveNetEvent(LPARAM lParam, int index)
{
    CALL_STACK_MESSAGE3("CSocket::ReceiveNetEvent(0x%IX, %d)", lParam, index);
    DWORD eventError = WSAGETSELECTERROR(lParam); // extract error code of event
    switch (WSAGETSELECTEVENT(lParam))
    {
    case FD_CLOSE:
    {
        DWORD error;
        if (eventError == NO_ERROR) // zatim to vypada na graceful close
        {
            // sekvence podle helpu: "Graceful Shutdown, Linger Options, and Socket Closure"

            HANDLES(EnterCriticalSection(&SocketCritSect));

            // podle helpu u "shutdown" se ma radsi volat jeste recv (nejspis zbytecne - data ignorujeme) +
            // shutdownuje server, muze pri spatnem zpracovani FD_CLOSE na socketu zbyt neprectena data (jen
            // vypiseme warning pro programatora)
            if (Socket != INVALID_SOCKET) // socket je pripojeny (jinak chybu "nepripojeny socket" oznami CloseSocket)
            {
                char buf[500];
                if (!SSLConn)
                {
                    while (1)
                    {
                        int r = recv(Socket, buf, 500, 0);
                        if (r == SOCKET_ERROR || r == 0)
                            break; // cyklime do chyby nebo nuly (0 = gracefully closed)
                        else
                        {
                            if (OurShutdown) // shutdown inicioval klient
                                TRACE_E("Unexpected: please inform Petr Solin: recv() read some bytes in FD_CLOSE on " << index);
                            else // shutdown inicioval server
                                TRACE_E("Probably invalid handling of FD_CLOSE: recv() read some bytes in FD_CLOSE on " << index);
                        }
                    }
                }
                else
                {
                    while (1)
                    {
                        int r = SSLLib.SSL_read(SSLConn, buf, 500);
                        if (r <= 0)
                            break; // cyklime do chyby nebo nuly (0 = gracefully closed)
                        else
                        {
                            if (OurShutdown) // shutdown inicioval klient
                                TRACE_E("Unexpected: please inform Petr Solin: SSL_read() read some bytes in FD_CLOSE on " << index);
                            else // shutdown inicioval server
                                TRACE_E("Probably invalid handling of FD_CLOSE: SSL_read() read some bytes in FD_CLOSE on " << index);
                        }
                    }
                }
            }
            BOOL ourShutdown = OurShutdown;
            HANDLES(LeaveCriticalSection(&SocketCritSect));

            if (!ourShutdown) // shutdown inicioval server
            {
                if (!Shutdown(&error))
                    eventError = error;
            }
        }
        if (!CloseSocket(&error) && eventError == NO_ERROR)
            eventError = error;

        SocketWasClosed(eventError); // ohlasime uzavreni socketu (pripadne i chybu)
        break;
    }

    case FD_ACCEPT:
    {
        HWND socketsWindow = SocketsThread->GetHiddenWindow(); // aby bylo mimo sekci SocketCritSect

        HANDLES(EnterCriticalSection(&SocketCritSect));

#ifdef _DEBUG
        if (SocketCritSect.RecursionCount > 1)
            TRACE_E("Incorrect call to CSocket::ReceiveNetEvent(FD_ACCEPT): from section SocketCritSect!");
#endif

        if (eventError == NO_ERROR) // bereme jen accept bez chyby, ostatni ignorujeme (spojeni se zkousi trvale az do timeoutu)
        {
            if (Socket != INVALID_SOCKET) // socket je pripojeny (jinak ignorujeme - uzivatel zrejme dal abort)
            {
                SOCKADDR_IN addr;
                memset(&addr, 0, sizeof(addr));
                int len = sizeof(addr);
                SOCKET sock = accept(Socket, (SOCKADDR*)&addr, &len);
                if (sock != INVALID_SOCKET &&
                    // volani WSAAsyncSelect podle helpu neni nutne, ale pry to nektere verze Windows Socketu
                    // nedelaji automaticky, takze to nutne je - vyskytla se anomalie: pri krokovani posilani
                    // prikazu LIST to generuje FD_XXX dvojmo (druha sada prijde po prvnim FD_CLOSE -> je nedorucitelna)
                    WSAAsyncSelect(sock, socketsWindow, Msg, FD_READ | FD_WRITE | FD_CLOSE) != SOCKET_ERROR)
                {
                    closesocket(Socket); // zavreme "listen" socket, uz nebude potreba - volani CloseSocketEx nezadouci
                    Socket = sock;
                    OurShutdown = FALSE;
                    ConnectionAccepted(TRUE, NO_ERROR, FALSE);
                }
                else // chyba - posleme ji aspon do logu
                {
                    DWORD err = WSAGetLastError();
                    if (sock != INVALID_SOCKET)
                        closesocket(sock);
                    ConnectionAccepted(FALSE, err, FALSE);
                }
            }
        }
        HANDLES(LeaveCriticalSection(&SocketCritSect));
        break;
    }
    }
}

void CSocket::SetMsgIndex(int index)
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    if (Msg != -1)
        TRACE_E("CSocket::SetMsgIndex(): message index has already beet set!");
    Msg = index + WM_APP_SOCKET_MIN;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

void CSocket::ResetMsgIndex()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    Msg = -1;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

int CSocket::GetMsgIndex()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int msg = Msg;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    if (msg != -1)
        msg -= WM_APP_SOCKET_MIN;
    return msg;
}

int CSocket::GetMsg()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int msg = Msg;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return msg;
}

int CSocket::GetUID()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    int uid = UID;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return uid;
}

SOCKET
CSocket::GetSocket()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    SOCKET sock = Socket;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return sock;
}

void CSocket::SetCertificate(CCertificate* certificate)
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    CCertificate* old = pCertificate; // duvodem je zajisteni volani AddRef pres Release (pro pripad, ze je pCertificate == certificate)
    pCertificate = certificate;
    if (pCertificate)
        pCertificate->AddRef();
    if (old)
        old->Release();
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

CCertificate*
CSocket::GetCertificate()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    CCertificate* ret = pCertificate;
    if (ret != NULL)
        ret->AddRef();
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CSocket::SwapSockets(CSocket* sock)
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    HANDLES(EnterCriticalSection(&sock->SocketCritSect)); // nezablokuje se, pokud se SwapSockets() nezavola soucasne s prohozenymi parametry (opacne poradi vstupu do sekci)
    int swapMsg = Msg;
    Msg = sock->Msg;
    sock->Msg = swapMsg;
    SOCKET swapSocket = Socket;
    Socket = sock->Socket;
    sock->Socket = swapSocket;
    SSL* swapSSLConn = SSLConn;
    SSLConn = sock->SSLConn;
    sock->SSLConn = swapSSLConn;
    int swapReuseSSLSession = ReuseSSLSession;
    ReuseSSLSession = sock->ReuseSSLSession;
    sock->ReuseSSLSession = swapReuseSSLSession;
    BOOL swapReuseSSLSessionFailed = ReuseSSLSessionFailed;
    ReuseSSLSessionFailed = sock->ReuseSSLSessionFailed;
    sock->ReuseSSLSessionFailed = swapReuseSSLSessionFailed;
    CCertificate* swapSSLCert = pCertificate;
    pCertificate = sock->pCertificate;
    sock->pCertificate = swapSSLCert;
    BOOL swapOurShutdown = OurShutdown;
    OurShutdown = sock->OurShutdown;
    sock->OurShutdown = swapOurShutdown;
    CSocketState swapSocketState = SocketState;
    SocketState = sock->SocketState;
    sock->SocketState = swapSocketState;
    char* swapHostAddress = HostAddress;
    HostAddress = sock->HostAddress;
    sock->HostAddress = swapHostAddress;
    DWORD swapHostIP = HostIP;
    HostIP = sock->HostIP;
    sock->HostIP = swapHostIP;
    unsigned short swapHostPort = HostPort;
    HostPort = sock->HostPort;
    sock->HostPort = swapHostPort;
    char* swapProxyUser = ProxyUser;
    ProxyUser = sock->ProxyUser;
    sock->ProxyUser = swapProxyUser;
    char* swapProxyPassword = ProxyPassword;
    ProxyPassword = sock->ProxyPassword;
    sock->ProxyPassword = swapProxyPassword;
    DWORD swapProxyIP = ProxyIP;
    ProxyIP = sock->ProxyIP;
    sock->ProxyIP = swapProxyIP;
    CProxyErrorCode swapProxyErrorCode = ProxyErrorCode;
    ProxyErrorCode = sock->ProxyErrorCode;
    sock->ProxyErrorCode = swapProxyErrorCode;
    DWORD swapProxyWinError = ProxyWinError;
    ProxyWinError = sock->ProxyWinError;
    sock->ProxyWinError = swapProxyWinError;
    HANDLES(LeaveCriticalSection(&sock->SocketCritSect));
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

BOOL CSocket::GetIsSocketConnectedLastCallTime(DWORD* lastCallTime)
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    BOOL ret = FALSE;
    if (IsSocketConnectedLastCallTime != 0)
    {
        ret = TRUE;
        *lastCallTime = IsSocketConnectedLastCallTime;
    }
    HANDLES(LeaveCriticalSection(&SocketCritSect));
    return ret;
}

void CSocket::SetIsSocketConnectedLastCallTime()
{
    HANDLES(EnterCriticalSection(&SocketCritSect));
    IsSocketConnectedLastCallTime = GetTickCount();
    if (IsSocketConnectedLastCallTime == 0)
        IsSocketConnectedLastCallTime = 1;
    HANDLES(LeaveCriticalSection(&SocketCritSect));
}

//
// ****************************************************************************
// CSocketsThread
//

CSocketsThread::CSocketsThread()
    : CThread("Sockets"), Sockets(100, 100), MsgData(10, 10), Timers(50, 100), PostMsgs(50, 100)
{
    HANDLES(InitializeCriticalSection(&CritSect));
    RunningEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual, non-signaled
    CanEndThread = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // manual, non-signaled
    Running = FALSE;
    HWindow = NULL;
    FirstFreeIndexInSockets = -1;
    LockedTimers = -1;
    Terminating = FALSE;
}

CSocketsThread::~CSocketsThread()
{
    if (MsgData.Count > 0)
        TRACE_E("FTP Client: Zustalo " << MsgData.Count << " nedorucenych zprav WM_APP_SOCKET_ADDR.");
    if (PostMsgs.Count > 0)
        TRACE_E("FTP Client: Zustalo " << PostMsgs.Count << " nedorucenych zprav WM_APP_SOCKET_POSTMSG.");
    if (RunningEvent != NULL)
        HANDLES(CloseHandle(RunningEvent));
    if (CanEndThread != NULL)
        HANDLES(CloseHandle(CanEndThread));
    HANDLES(DeleteCriticalSection(&CritSect));
    if (Sockets.Count > 0)
    {
        TRACE_I("FTP Client: Bylo otevreno nejvice " << Sockets.Count << " socketu zaroven.");
        int c = 0;
        int i;
        for (i = 0; i < Sockets.Count; i++)
            if (Sockets[i] != NULL)
                c++;
        if (c != 0)
            TRACE_E("FTP Client: Zustalo otevreno " << c << " socketu!");
    }
}

void CSocketsThread::Terminate()
{
    CALL_STACK_MESSAGE1("CSocketsThread::Terminate()");
    HANDLES(EnterCriticalSection(&CritSect));
    if (HWindow != NULL)
    {
        KillTimer(HWindow, SOCKETSTHREAD_TIMERID); // ukoncime timer
        PostMessage(HWindow, WM_CLOSE, 0, 0);      // zasleme oknu zpravu o ukonceni
    }
    Terminating = TRUE; // Windows timer uz nepujde nahodit
    HANDLES(LeaveCriticalSection(&CritSect));
}

BOOL CSocketsThread::AddSocket(CSocket* sock)
{
    CALL_STACK_MESSAGE1("CSocketsThread::AddSocket()");
    BOOL ret = TRUE;
    HANDLES(EnterCriticalSection(&CritSect));
    if (FirstFreeIndexInSockets != -1) // mame kam vlozit novy socket?
    {
        // vlozeni 'sock' do pole
        Sockets[FirstFreeIndexInSockets] = sock;
        sock->SetMsgIndex(FirstFreeIndexInSockets);

        // hledani dalsiho volneho mista v poli
        int i;
        for (i = FirstFreeIndexInSockets + 1; i < Sockets.Count; i++)
        {
            if (Sockets[i] == NULL)
            {
                FirstFreeIndexInSockets = i;
                break;
            }
        }
        if (i >= Sockets.Count)
            FirstFreeIndexInSockets = -1;
    }
    else
    {
        if (WM_APP_SOCKET_MAX - WM_APP_SOCKET_MIN >= Sockets.Count) // vejdeme se s novym socketem?
        {
            int index = Sockets.Add(sock);
            if (Sockets.IsGood())
                sock->SetMsgIndex(index);
            else
            {
                Sockets.ResetState();
                ret = FALSE;
            }
        }
        else
        {
            TRACE_E("CSocketsThread::AddSocket(): Unable to add new socket - limit reached (count=" << Sockets.Count << ")");
            ret = FALSE;
        }
    }
    HANDLES(LeaveCriticalSection(&CritSect));
    return ret;
}

void CSocketsThread::DeleteSocketFromIndex(int index, BOOL onlyDetach)
{
    CALL_STACK_MESSAGE3("CSocketsThread::DeleteSocketFromIndex(%d, %d)", index, onlyDetach);
    HANDLES(EnterCriticalSection(&CritSect));
    if (index >= 0 && index < Sockets.Count && Sockets[index] != NULL)
    {
        if (!onlyDetach)
        {
#ifdef _DEBUG
            BOOL old = InDeleteSocket;
            InDeleteSocket = TRUE; // sice nemusime byt primo v ::DeleteSocket, ale volani je korektni
#endif
            delete Sockets[index]; // dealokace objektu socketu
#ifdef _DEBUG
            InDeleteSocket = old;
#endif
        }
        else
            Sockets[index]->ResetMsgIndex(); // odpojeni objektu socketu
        Sockets[index] = NULL;               // uvolneni pozice v poli (pole nezkracujeme, neni s tim pocitano + je to zbytecne)
        if (FirstFreeIndexInSockets == -1 || index < FirstFreeIndexInSockets)
            FirstFreeIndexInSockets = index;
    }
    else
        TRACE_E("CSocketsThread::DeleteSocketFromIndex(): Pokus o vymaz neplatneho prvku pole!");
    HANDLES(LeaveCriticalSection(&CritSect));
}

void CSocketsThread::BeginSocketsSwap(CSocket* sock1, CSocket* sock2)
{
    CALL_STACK_MESSAGE1("CSocketsThread::BeginSocketsSwap(,)");

    HANDLES(EnterCriticalSection(&CritSect));
    int index1 = sock1->GetMsgIndex();
    int index2 = sock2->GetMsgIndex();
    if (index1 != -1 && index2 != -1)
    {
        // prohodime vnitrni data objektu socketu (jen data tridy CSocket, zbytek je na volajicim)
        sock1->SwapSockets(sock2);
        // prohodime objekty v poli obsluhovanych socketu
        Sockets[index1] = sock2;
        Sockets[index2] = sock1;
    }
    else
        TRACE_E("Incorrect use of CSocketsThread::BeginSocketsSwap(): at least one socket is not in Sockets array!");
}

void CSocketsThread::EndSocketsSwap()
{
    HANDLES(LeaveCriticalSection(&CritSect));
}

BOOL CSocketsThread::PostHostByAddressResult(int socketMsg, int socketUID, DWORD ip, int hostUID, int err)
{
    CALL_STACK_MESSAGE6("CSocketsThread::PostHostByAddressResult(%d, %d, 0x%X, %d, %d)",
                        socketMsg, socketUID, ip, hostUID, err);
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&CritSect));
    if (HWindow != NULL)
    {
        CMsgData* data = new CMsgData(socketMsg, socketUID, ip, hostUID, err);
        if (data != NULL)
        {
            MsgData.Add(data);
            if (MsgData.IsGood())
            {
                if (PostMessage(HWindow, WM_APP_SOCKET_ADDR, 0, 0))
                    ret = TRUE;
                else
                {
                    DWORD error = GetLastError();
                    TRACE_E("PostMessage has failed: err=" << error);
                    MsgData.Delete(MsgData.Count - 1);
                    if (!MsgData.IsGood())
                        MsgData.ResetState();
                }
            }
            else
            {
                MsgData.ResetState();
                delete data;
            }
        }
        else
            TRACE_E(LOW_MEMORY);
    }
    HANDLES(LeaveCriticalSection(&CritSect));
    return ret;
}

void CSocketsThread::ReceiveMsgData()
{
    HANDLES(EnterCriticalSection(&CritSect));
    if (MsgData.Count > 0)
    {
        CMsgData* data = MsgData[0]; // bereme prvni nezpracovana data vlozena do pole
        int index = data->SocketMsg - WM_APP_SOCKET_MIN;
        if (index >= 0 && index < Sockets.Count) // "always true"
        {
            CSocket* s = Sockets[index];
            if (s != NULL && s->GetUID() == data->SocketUID) // je to prijemce zpravy (dockal se)
                s->ReceiveHostByAddressInt(data->IP, data->HostUID, data->Err, index);
            else // jde o IP, ktere nedoslo z duvodu zruseni socketu nebo swapnuti socketu,
            {    // pri swapnuti socketu dohledame cilovy socket sekvencne a dorucime IP
                int i;
                for (i = 0; i < Sockets.Count; i++)
                {
                    CSocket* s2 = Sockets[i];
                    if (s2 != NULL && s2->GetUID() == data->SocketUID) // je to prijemce zpravy (dockal se)
                    {
                        s2->ReceiveHostByAddressInt(data->IP, data->HostUID, data->Err, i);
                        break;
                    }
                }
#ifdef _DEBUG
                if (i == Sockets.Count)
                {
                    // vypiseme warning, ze se ztratilo IP (nedoslo do objektu socketu)
                    TRACE_I("Lost IP-address with host-UID " << data->HostUID << " for UID " << data->SocketUID);
                }
#endif
            }
        }
        else
            TRACE_E("Unexpected situation in CSocketsThread::ReceiveMsgData()");
        MsgData.Delete(0);
        if (!MsgData.IsGood())
            MsgData.ResetState();
    }
    else
        TRACE_E("Unexpected situation in CSocketsThread::ReceiveMsgData(): no data in array!");
    HANDLES(LeaveCriticalSection(&CritSect));
}

BOOL CSocketsThread::PostSocketMessage(int socketMsg, int socketUID, DWORD id, void* param)
{
    CALL_STACK_MESSAGE4("CSocketsThread::PostSocketMessage(%d, %d, %u,)", socketMsg, socketUID, id);
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&CritSect));
    if (HWindow != NULL)
    {
        CPostMsgData* data = new CPostMsgData(socketMsg, socketUID, id, param);
        if (data != NULL)
        {
            PostMsgs.Add(data);
            if (PostMsgs.IsGood())
            {
                if (PostMessage(HWindow, WM_APP_SOCKET_POSTMSG, 0, 0))
                    ret = TRUE;
                else
                {
                    DWORD error = GetLastError();
                    TRACE_E("PostMessage has failed: err=" << error);
                    PostMsgs.Delete(PostMsgs.Count - 1);
                    if (!PostMsgs.IsGood())
                        PostMsgs.ResetState();
                }
            }
            else
            {
                PostMsgs.ResetState();
                delete data;
            }
        }
        else
            TRACE_E(LOW_MEMORY);
    }
    HANDLES(LeaveCriticalSection(&CritSect));
    return ret;
}

BOOL CSocketsThread::IsSocketConnected(int socketUID, BOOL* isConnected)
{
    CALL_STACK_MESSAGE2("CSocketsThread::IsSocketConnected(%d,)", socketUID);
    if (isConnected != NULL)
        *isConnected = FALSE;
    BOOL ret = FALSE;
    HANDLES(EnterCriticalSection(&CritSect));
    int i;
    for (i = 0; i < Sockets.Count; i++)
    {
        CSocket* s = Sockets[i];
        if (s != NULL && s->GetUID() == socketUID) // je to hledany socket
        {
            ret = TRUE;
            if (isConnected != NULL)
                *isConnected = s->IsConnected();
            s->SetIsSocketConnectedLastCallTime();
            break;
        }
    }
    HANDLES(LeaveCriticalSection(&CritSect));
    return ret;
}

void CSocketsThread::ReceivePostMessage()
{
    HANDLES(EnterCriticalSection(&CritSect));
    if (PostMsgs.Count > 0)
    {
        CPostMsgData* data = PostMsgs[0]; // bereme prvni nezpracovana data vlozena do pole
        int index = data->SocketMsg - WM_APP_SOCKET_MIN;
        if (index >= 0 && index < Sockets.Count) // "always true"
        {
            CSocket* s = Sockets[index];
            if (s != NULL && s->GetUID() == data->SocketUID) // je to prijemce zpravy (dockal se)
                s->ReceivePostMessage(data->ID, data->Param);
            else // jde o zpravu, ktera nedosla z duvodu zruseni socketu nebo swapnuti socketu,
            {    // pri swapnuti socketu dohledame cilovy socket sekvencne a dorucime zpravu
                int i;
                for (i = 0; i < Sockets.Count; i++)
                {
                    CSocket* s2 = Sockets[i];
                    if (s2 != NULL && s2->GetUID() == data->SocketUID) // je to prijemce zpravy (dockal se)
                    {
                        s2->ReceivePostMessage(data->ID, data->Param);
                        break;
                    }
                }
#ifdef _DEBUG
                if (i == Sockets.Count)
                {
                    // vypiseme warning, ze se ztratila zprava (nedosla do objektu socketu)
                    TRACE_I("Lost post-message " << data->ID << " for UID " << data->SocketUID);
                }
#endif
            }
        }
        else
            TRACE_E("Unexpected situation in CSocketsThread::ReceivePostMessage()");
        PostMsgs.Delete(0);
        if (!PostMsgs.IsGood())
            PostMsgs.ResetState();
    }
    else
        TRACE_E("Unexpected situation in CSocketsThread::ReceivePostMessage(): no data in array!");
    HANDLES(LeaveCriticalSection(&CritSect));
}

int CSocketsThread::FindIndexForNewTimer(DWORD timeoutAbs, int leftIndex)
{
    if (leftIndex >= Timers.Count)
        return leftIndex;

    // vsechny casy se musi vztahnout k nejblizsimu timeoutu, protoze jen tak se podari
    // seradit timeouty, ktere prekroci 0xFFFFFFFF
    DWORD timeoutAbsBase = Timers[leftIndex]->TimeoutAbs;
    if ((int)(timeoutAbs - timeoutAbsBase) < 0)
        timeoutAbsBase = timeoutAbs;
    timeoutAbs -= timeoutAbsBase;

    int l = leftIndex, r = Timers.Count - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        DWORD actTimeoutAbs = Timers[m]->TimeoutAbs - timeoutAbsBase;
        if (actTimeoutAbs == timeoutAbs)
        {
            while (++m < Timers.Count && Timers[m]->TimeoutAbs - timeoutAbsBase == timeoutAbs)
                ;     // vratime index za posledni stejny timer
            return m; // nalezeno
        }
        else if (actTimeoutAbs > timeoutAbs)
        {
            if (l == r || l > m - 1)
                return m; // nenalezeno, mel by byt na teto pozici
            r = m - 1;
        }
        else
        {
            if (l == r)
                return m + 1; // nenalezeno, mel by byt az za touto pozici
            l = m + 1;
        }
    }
}

BOOL CSocketsThread::AddTimer(int socketMsg, int socketUID, DWORD timeoutAbs, DWORD id, void* param)
{
    HANDLES(EnterCriticalSection(&CritSect));

    BOOL ret = FALSE;
    CTimerData* data = new CTimerData(socketMsg, socketUID, timeoutAbs, id, param);
    if (data != NULL)
    {
        // musime vlozit az za chraneny usek (timery v tomto useku se po zpracovani smazou)
        int i = FindIndexForNewTimer(timeoutAbs, (LockedTimers == -1 ? 0 : LockedTimers));
        Timers.Insert(i, data);
        if (Timers.IsGood())
        {
            if (i == 0 && !Terminating) // vlozeni timeru s nejkratsim casem do timeoutu
            {
                DWORD ti = timeoutAbs - GetTickCount();
                if ((int)ti > 0) // pokud jiz nenastal timeout noveho timeru (muze nastat i zaporny rozdil casu), zmenime nebo nahodime Windows timer
                    SetTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID, ti, NULL);
                else
                {
                    if ((int)ti < 0)
                        TRACE_E("CSocketsThread::AddTimer(): expired timer was added (" << (int)ti << " ms)");
                    KillTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID);                // zrusime pripadny Windowsovy timer, uz neni potreba
                    PostMessage(GetHiddenWindow(), WM_TIMER, SOCKETSTHREAD_TIMERID, 0); // co nejdrive zpracujeme dalsi timeout
                }
            }
            ret = TRUE;
        }
        else
        {
            Timers.ResetState();
            delete data;
        }
    }
    else
        TRACE_E(LOW_MEMORY);

    HANDLES(LeaveCriticalSection(&CritSect));
    return ret;
}

BOOL CSocketsThread::DeleteTimer(int socketUID, DWORD id)
{
    HANDLES(EnterCriticalSection(&CritSect));

    BOOL ret = FALSE;
    BOOL setTimer = FALSE;
    int last = (LockedTimers != -1 ? LockedTimers : 0); // muzeme mazat jen za chranenym usekem (timery v tomto useku se po zpracovani smazou)
    int i;
    for (i = Timers.Count - 1; i >= 0; i--)
    {
        CTimerData* timer = Timers[i];
        if (timer->SocketUID == socketUID && timer->ID == id) // smazeme vsechny odpovidajici timery
        {
            if (i >= last)
            {
                if (i == 0)
                    setTimer = TRUE;
                Timers.Delete(i);
                if (!Timers.IsGood())
                    Timers.ResetState(); // Delete se nemuze nepovest, ale nemusi se podarit zmenseni pole
            }
            else // vymaz timeru z uzamcene oblasti pole Timers = jen zmena SocketMsg na (WM_APP_SOCKET_MIN-1)
            {
                Timers[i]->SocketMsg = WM_APP_SOCKET_MIN - 1;
            }
            ret = TRUE;
        }
    }
    if (setTimer) // byl zrusen timer s nejblizsim timeoutem, musime prenastavit timeout
    {
        if (Timers.Count > 0 && !Terminating)
        {
            DWORD ti = Timers[0]->TimeoutAbs - GetTickCount();
            if ((int)ti > 0) // pokud jiz nenastal timeout noveho timeru (muze nastat i zaporny rozdil casu), zmenime nebo nahodime Windows timer
                SetTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID, ti, NULL);
            else
            {
                KillTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID);                // zrusime pripadny Windowsovy timer, uz neni potreba
                PostMessage(GetHiddenWindow(), WM_TIMER, SOCKETSTHREAD_TIMERID, 0); // co nejdrive zpracujeme dalsi timeout
            }
        }
        else
            KillTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID); // zrusime pripadny Windowsovy timer, uz neni potreba
    }

    HANDLES(LeaveCriticalSection(&CritSect));
    return ret;
}

void CSocketsThread::ReceiveTimer()
{
    // zrusime pripadny Windowsovy timer (aby se zbytecne neopakovalo volani)
    KillTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID);

    HANDLES(EnterCriticalSection(&CritSect));

    // obrana proti rekurzivnimu volani ReceiveTimer() - WM_TIMER nema cenu posilat, az dobehne
    // zpracovani prvniho volani ReceiveTimer(), bude nahozeny novy timer nebo postnuty WM_TIMER
    if (LockedTimers == -1)
    {
        DWORD ti = GetTickCount();
        int last = FindIndexForNewTimer(ti, 0);
        if (last > 0) // pokud nastal nejaky timeout timeru
        {
            LockedTimers = last; // budeme chranit zpracovavane timery proti mazani a rozesunuti pole
            int i;
            for (i = 0; i < last; i++)
            {
                CTimerData* timer = Timers[i];
                int index = timer->SocketMsg - WM_APP_SOCKET_MIN;
                if (index >= 0 && index < Sockets.Count) // "always true"
                {
                    CSocket* s = Sockets[index];
                    if (s != NULL && s->GetUID() == timer->SocketUID) // je to prijemce zpravy (dockal se)
                        s->ReceiveTimer(timer->ID, timer->Param);
                    else // jde o timer, ktery nedosel z duvodu zruseni socketu nebo swapnuti socketu,
                    {    // pri swapnuti socketu dohledame cilovy socket sekvencne a dorucime timer
                        int j;
                        for (j = 0; j < Sockets.Count; j++)
                        {
                            CSocket* s2 = Sockets[j];
                            if (s2 != NULL && s2->GetUID() == timer->SocketUID) // je to prijemce zpravy (dockal se)
                            {
                                s2->ReceiveTimer(timer->ID, timer->Param);
                                break;
                            }
                        }
#ifdef _DEBUG
                        if (j == Sockets.Count)
                        {
                            // vypiseme warning, ze se ztratil timer-event (nedosel do objektu socketu)
                            TRACE_I("Lost timer-event " << timer->ID << " for UID " << timer->SocketUID);
                        }
#endif
                    }
                }
                else
                {
                    if (index != -1)
                        TRACE_E("Unexpected situation in CSocketsThread::ReceiveTimer()");
                }
            }
            Timers.Delete(0, last);
            LockedTimers = -1; // uz neni co chranit
        }
        if (Timers.Count > 0 && !Terminating)
        {
            ti = Timers[0]->TimeoutAbs - GetTickCount();
            if ((int)ti > 0) // pokud jiz nenastal dalsi timeout (muze nastat i zaporny rozdil casu), nahodime znovu timer
                SetTimer(GetHiddenWindow(), SOCKETSTHREAD_TIMERID, ti, NULL);
            else
                PostMessage(GetHiddenWindow(), WM_TIMER, SOCKETSTHREAD_TIMERID, 0); // co nejdrive zpracujeme dalsi timeout
        }
    }
    HANDLES(LeaveCriticalSection(&CritSect));
}

LRESULT
CSocketsThread::ReceiveNetEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE1("CSocketsThread::ReceiveNetEvent()");

    HANDLES(EnterCriticalSection(&CritSect));
    int index = uMsg - WM_APP_SOCKET_MIN;
    if (index >= 0 && index < Sockets.Count) // "always true"
    {
        CSocket* s = Sockets[index];
        if (s != NULL && s->GetSocket() == (SOCKET)wParam) // je to prijemce zpravy (dockal se)
        {
            s->ReceiveNetEventInt(lParam, index);
        }
        else
        {
#ifdef _DEBUG
            // vypiseme warning, ze se ztratila zprava (nedosla do objektu socketu)
            switch (WSAGETSELECTEVENT(lParam))
            {
            case FD_READ: /*TRACE_I("Lost FD_READ on " << (uMsg - WM_APP_SOCKET_MIN));*/
                break;    // chodi po FD_CLOSE; chodi podruhe (po FD_CLOSE), pokud se po accept() pouzije WSAAsyncSelect()
            case FD_WRITE:
                TRACE_I("Lost FD_WRITE on " << (uMsg - WM_APP_SOCKET_MIN));
                break;     // chodi podruhe (po FD_CLOSE), pokud se po accept() pouzije WSAAsyncSelect()
            case FD_CLOSE: /*TRACE_I("Lost FD_CLOSE on " << (uMsg - WM_APP_SOCKET_MIN));*/
                break;     // chodi podruhe (po FD_CLOSE), pokud se po accept() pouzije WSAAsyncSelect()
            case FD_CONNECT:
                TRACE_I("Lost FD_CONNECT on " << (uMsg - WM_APP_SOCKET_MIN));
                break; // nastava u adresaru nepristupnych pro listovani na VMS (data-connection se zavre jeste nez dobehne jeji connect)
            case FD_ACCEPT:
                TRACE_I("Lost FD_ACCEPT on " << (uMsg - WM_APP_SOCKET_MIN));
                break;
            default:
                TRACE_E("Lost socket event (" << WSAGETSELECTEVENT(lParam) << ") on " << (uMsg - WM_APP_SOCKET_MIN));
                break;
            }
#endif
        }
    }
    else
        TRACE_E("Unexpected situation in CSocketsThread::ReceiveNetEvent()");
    HANDLES(LeaveCriticalSection(&CritSect));
    return 0;
}

LRESULT CALLBACK
CSocketsThread::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //  SLOW_CALL_STACK_MESSAGE5("CSocketsThread::WindowProc(0x%X, 0x%X, 0x%X, 0x%X)", hwnd, uMsg, wParam, lParam);

    if (GetTickCount() - LastWM_TIMER_Processing >= 500 && uMsg != WM_TIMER)
    {                                             // pokud ubehlo 1000ms od posledniho WM_TIMER, vlozime ho do zpracovani umele, protoze nejspis "jen"
                                                  // neni thread "idle", a proto system WM_TIMER neposila (je to bohuzel low-priority message)
        LastWM_TIMER_Processing = GetTickCount(); // ulozime kdy byl naposledy zpracovan WM_TIMER
        if (SocketsThread != NULL)
            SocketsThread->ReceiveTimer();
    }

    if (uMsg >= WM_APP_SOCKET_MIN && uMsg <= WM_APP_SOCKET_MAX) // jde o zpravu pro socket od Windows Sockets
    {
        if (SocketsThread != NULL)
            return SocketsThread->ReceiveNetEvent(uMsg, wParam, lParam);
    }
    else
    {
        switch (uMsg)
        {
        case WM_APP_SOCKET_ADDR:
        {
            if (SocketsThread != NULL)
                SocketsThread->ReceiveMsgData();
            return 0;
        }

        case WM_TIMER:
        {
            // ulozime kdy byl naposledy zpracovan WM_TIMER
            LastWM_TIMER_Processing = GetTickCount();
            if (SocketsThread != NULL)
                SocketsThread->ReceiveTimer();
            return 0;
        }

        case WM_APP_SOCKET_POSTMSG:
        {
            if (SocketsThread != NULL)
                SocketsThread->ReceivePostMessage();
            return 0;
        }

        case WM_DESTROY: // s ukoncenim okna ukoncime i smycku zprav
        {
            PostQuitMessage(0);
            return 0;
        }
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

BOOL CSocketsThread::IsRunning()
{
    CALL_STACK_MESSAGE1("CSocketsThread::IsRunning()");
    WaitForSingleObject(RunningEvent, INFINITE);
    BOOL run = Running;
    SetEvent(CanEndThread); // ted uz se muze thread ukoncit a tento objekt dealokovat
    return run;
}

unsigned
CSocketsThread::Body()
{
    TRACE_I("Begin");

    WNDCLASS hiddenWinCls;
    memset(&hiddenWinCls, 0, sizeof(hiddenWinCls));
    hiddenWinCls.lpfnWndProc = CSocketsThread::WindowProc;
    hiddenWinCls.hInstance = DLLInstance;
    hiddenWinCls.hCursor = LoadCursor(NULL, IDC_ARROW);
    hiddenWinCls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    hiddenWinCls.lpszClassName = SOCKETSWINDOW_CLASSNAME;
    if (RegisterClass(&hiddenWinCls) != 0)
    {
        HWindow = CreateWindow(SOCKETSWINDOW_CLASSNAME, "HiddenSocketsWindow", 0, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, DLLInstance, 0);
        if (HWindow != NULL)
        {
            // ohlasime uspesne nastartovani a inicializaci threadu
            Running = TRUE;
            SetEvent(RunningEvent);
            WaitForSingleObject(CanEndThread, INFINITE);
            LastWM_TIMER_Processing = GetTickCount();

            // smycka zprav
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // nulujeme handle okna - ted uz je zbytecne posilat message
            HANDLES(EnterCriticalSection(&CritSect));
            HWindow = NULL;
            HANDLES(LeaveCriticalSection(&CritSect));
        }

        if (!UnregisterClass(SOCKETSWINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(SOCKETSWINDOW_CLASSNAME) has failed");
    }
    if (!Running)
    {
        SetEvent(RunningEvent); // v pripade chyby ohlasime smrt threadu
        WaitForSingleObject(CanEndThread, INFINITE);
    }

    // Petr: pokud se v tomto threadu pracovalo s OpenSSL, musime uvolnit pamet nasledujicim volanim
    SSLThreadLocalCleanup();

    TRACE_I("End");
    return 0;
}
