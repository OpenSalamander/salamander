// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************
// CONSTANTS
// ****************************************************************************

// if defined, DATACON_SNDBUF_SIZE and DATACON_RCVBUF_SIZE are set for data connections
// solves the problem: https://forum.altap.cz/viewtopic.php?f=6&t=31923
// help for the forum: ver1 = 8k (version without setting buffers), ver2 = 4m (4MB RECV buffer, 256KB SEND buffer)
#define DATACON_USES_OUR_BUF_SIZES

#define DATACON_SNDBUF_SIZE (256 * 1024)      // value taken from WinSCP (download 2.2MB/s instead of 1.6MB/s)
#define DATACON_RCVBUF_SIZE (4 * 1024 * 1024) // value taken from WinSCP (download 2.2MB/s instead of 1.6MB/s)

#define WM_APP_SOCKET_POSTMSG (WM_APP + 98) // [0, 0] - ReceivePostMessage should be called
#define WM_APP_SOCKET_ADDR (WM_APP + 99)    // [0, 0] - ReceiveHostByAddress should be called
#define WM_APP_SOCKET_MIN (WM_APP + 100)    // first message number used when receiving events on sockets
#define WM_APP_SOCKET_MAX (WM_APP + 16099)  // last message number used when receiving events on sockets

#define SD_SEND 0x01 // they somehow forgot this constant in winsock.h (it is in winsock2.h)

// ****************************************************************************
// GLOBAL VARIABLES
// ****************************************************************************

extern WSADATA WinSocketsData; // info about the implementation of Windows Sockets

// ****************************************************************************
// GLOBAL FUNCTIONS
// ****************************************************************************

// initialization of the sockets module; 'parent' is the parent of the message box,
// returns TRUE if initialization is successful
BOOL InitSockets(HWND parent);
// release the sockets module
void ReleaseSockets();

// frees the socket object; use instead of calling the delete operator ("delete socket;");
// (stops monitoring events on the socket and removes it from the SocketsThread object)
void DeleteSocket(class CSocket* socket);

#ifdef _DEBUG
extern BOOL InDeleteSocket; // TRUE if we are inside ::DeleteSocket (to test direct calls of "delete socket")
#endif

//
// ****************************************************************************
// CSocket
//
// basic socket object, used only to define further objects
// use the ::DeleteSocket function for deallocation!

enum CSocketState // socket state
{
    ssNotOpened, // the socket is not yet open (Socket == INVALID_SOCKET)

    ssSocks4_Connect,    // SOCKS 4 - CONNECT: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssSocks4_WaitForIP,  // SOCKS 4 - CONNECT: waiting for the FTP server IP
    ssSocks4_WaitForCon, // SOCKS 4 - CONNECT: waiting for the result of the request to connect to the FTP server

    ssSocks4A_Connect,    // SOCKS 4A - CONNECT: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssSocks4A_WaitForCon, // SOCKS 4A - CONNECT: waiting for the result of the request to connect to the FTP server

    ssSocks5_Connect,      // SOCKS 5 - CONNECT: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssSocks5_WaitForMeth,  // SOCKS 5 - CONNECT: waiting to see which authentication method the server chooses
    ssSocks5_WaitForLogin, // SOCKS 5 - CONNECT: waiting for the result of the login to the proxy server (we sent user+password)
    ssSocks5_WaitForCon,   // SOCKS 5 - CONNECT: waiting for the result of the request to connect to the FTP server

    ssHTTP1_1_Connect,    // HTTP 1.1 - CONNECT: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssHTTP1_1_WaitForCon, // HTTP 1.1 - CONNECT: waiting for the result of the request to connect to the FTP server

    ssSocks4_Listen,           // SOCKS 4 - LISTEN: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssSocks4_WaitForListenRes, // SOCKS 4 - LISTEN: waiting for the proxy to open a "listen" port and return the IP+port where it listens (or return an error)
    ssSocks4_WaitForAccept,    // SOCKS 4 - LISTEN: waiting for the proxy to accept a connection from the FTP server (or return an error)

    ssSocks4A_Listen,           // SOCKS 4A - LISTEN: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssSocks4A_WaitForListenRes, // SOCKS 4A - LISTEN: waiting for the proxy to open a "listen" port and return the IP+port where it listens (or return an error)
    ssSocks4A_WaitForAccept,    // SOCKS 4A - LISTEN: waiting for the proxy to accept a connection from the FTP server (or return an error)

    ssSocks5_Listen,             // SOCKS 5 - LISTEN: waiting for FD_CONNECT (connecting to the proxy server is in progress)
    ssSocks5_ListenWaitForMeth,  // SOCKS 5 - LISTEN: waiting to see which authentication method the server chooses
    ssSocks5_ListenWaitForLogin, // SOCKS 5 - LISTEN: waiting for the result of the login to the proxy server (we sent user+password)
    ssSocks5_WaitForListenRes,   // SOCKS 5 - LISTEN: waiting for the proxy to open a "listen" port and return the IP+port where it listens (or return an error)
    ssSocks5_WaitForAccept,      // SOCKS 5 - LISTEN: waiting for the proxy to accept a connection from the FTP server (or return an error)

    ssHTTP1_1_Listen, // HTTP 1.1 - LISTEN: waiting for FD_CONNECT (connecting to the proxy server is in progress)

    ssConnectFailed, // CONNECT: just waiting for the response to the reported error (via FD_CONNECT with an error)
    ssListenFailed,  // LISTEN: just waiting for the response to the reported error (via ListeningForConnection() or ConnectionAccepted())

    ssNoProxyOrConnected, // connection without a proxy server or we are already connected (the proxy server is transparent for us)
};

enum CProxyErrorCode
{
    pecNoError,           // no error has occurred yet
    pecGettingHostIP,     // CONNECT: error obtaining the FTP server IP (SOCKS4)
    pecSendingBytes,      // CONNECT+LISTEN: error while sending data to the proxy server
    pecReceivingBytes,    // CONNECT+LISTEN: error while receiving data from the proxy server
    pecUnexpectedReply,   // CONNECT+LISTEN: received an unexpected response - report an error; ProxyWinError is not used
    pecProxySrvError,     // CONNECT+LISTEN: the proxy server reports an error when connecting to the FTP server; ProxyWinError contains the text resource ID
    pecNoAuthUnsup,       // CONNECT+LISTEN: the proxy server does not support access without authentication
    pecUserPassAuthUnsup, // CONNECT+LISTEN: the proxy server does not support authentication via user+password
    pecUserPassAuthFail,  // CONNECT+LISTEN: the proxy server did not accept our user+password
    pecConPrxSrvError,    // LISTEN: error when connecting to the proxy server
    pecListenUnsup,       // HTTP 1.1: LISTEN: not supported
    pecHTTPProxySrvError, // HTTP 1.1: CONNECT: the proxy server returned an error, its textual description is in HTTP11_FirstLineOfReply
};

enum CFTPProxyServerType;

class CSocket
{
public:
    static CRITICAL_SECTION NextSocketUIDCritSect; // critical section for the counter (sockets are created in different threads)

private:
    static int NextSocketUID; // global counter for socket objects

protected:
    // critical section for accessing the object's data
    // WARNING: this section must not nest into SocketsThread->CritSect (SocketsThread methods must not be called)
    CRITICAL_SECTION SocketCritSect;

    int UID; // unique number of this object (it is not moved in SwapSockets())

    int Msg;                    // message number used to receive events for this object (-1 == the object is not yet connected)
    SOCKET Socket;              // encapsulated Windows Sockets socket; if INVALID_SOCKET, the socket is not open
    SSL* SSLConn;               // SSL connection, use instead of Socket if non-NULL
    int ReuseSSLSession;        // reuse the SSL session of this control connection for all its data connections: 0 = try, 1 = yes, 2 = no
    BOOL ReuseSSLSessionFailed; // TRUE = reusing the SSL session for the last data connection opened failed: if we cannot do without it, we have to reconnect this control connection
    CCertificate* pCertificate; // non-NULL on FTPS connections
    BOOL OurShutdown;           // TRUE if this side (the FTP client) initiated the shutdown
    BOOL IsDataConnection;      // TRUE = socket for data transfer, set larger buffers (speed up listing, downloads and uploads)

    CSocketState SocketState; // socket state

    // data for connecting through proxy servers (firewalls)
    char* HostAddress;       // name address of the target machine we want to connect to
    DWORD HostIP;            // IP address of 'HostAddress' (==INADDR_NONE until the IP is known)
    unsigned short HostPort; // port of the target machine we want to connect to
    char* ProxyUser;         // username for the proxy server
    char* ProxyPassword;     // password for the proxy server
    DWORD ProxyIP;           // IP address of the proxy server (used only for LISTEN - otherwise ==INADDR_NONE)

    CProxyErrorCode ProxyErrorCode; // error code that occurred when connecting to the FTP server through the proxy server
    DWORD ProxyWinError;            // whether it is used depends on the value of ProxyErrorCode: Windows error code (NO_ERROR = use IDS_UNKNOWNERROR text)

    BOOL ShouldPostFD_WRITE; // TRUE = FD_WRITE arrived while connecting through the proxy server, so after the FTP connection is established we will have to forward it to ReceiveNetEvent()

    char* HTTP11_FirstLineOfReply;    // if not NULL, contains the first line of the reply from the HTTP 1.1 proxy server (to the CONNECT request)
    int HTTP11_EmptyRowCharsReceived; // the server response ends with CRLFCRLF, here we store how many characters of that sequence have already arrived

    DWORD IsSocketConnectedLastCallTime; // 0 if CSocketsThread::IsSocketConnected() has not yet been called for this socket, otherwise the GetTickCount() of the last call

public:
    CSocket();
    virtual ~CSocket();

    // ******************************************************************************************
    // methods used by the SocketsThread object
    // ******************************************************************************************

    // used by the SocketsThread object to pass the message number (index in the array of handled
    // sockets) that this object should use for receiving events; other methods of the object can
    // be called only after the object has been added to SocketsThread (by calling the SocketsThread->AddSocket method)
    // callable from any thread
    void SetMsgIndex(int index);

    // called when disconnecting the object from SocketsThread
    // callable from any thread
    void ResetMsgIndex();

    // returns the index of the object in the array of handled sockets; returns -1 if the object is not in this array
    // callable from any thread
    int GetMsgIndex();

    // returns the current value of Msg (inside the critical section)
    // callable from any thread
    int GetMsg();

    // used by the SocketsThread object to uniquely identify this object (message numbers are reused -
    // after the object is deallocated its message number is assigned to a newly created one)
    // callable from any thread
    int GetUID();

    // used by the SocketsThread object to uniquely identify this object (message numbers are reused -
    // after the object is deallocated its message number is assigned to a newly created one)
    // callable from any thread
    SOCKET GetSocket();

    CCertificate* GetCertificate(); // WARNING: returns the certificate only after calling its AddRef(), so the caller is responsible for releasing the certificate by calling Release()
    void SetCertificate(CCertificate* certificate);

    // used by the SocketsThread object when swapping socket objects, see
    // CSocketsThread::BeginSocketsSwap(); swaps Msg and Socket
    // callable from any thread
    void SwapSockets(CSocket* sock);

    // returns TRUE if CSocketsThread::IsSocketConnected() has been called for this socket -
    // returns the call time in 'lastCallTime'; returns FALSE if CSocketsThread::IsSocketConnected()
    // has not been called yet
    BOOL GetIsSocketConnectedLastCallTime(DWORD* lastCallTime);

    // sets IsSocketConnectedLastCallTime to the current GetTickCount()
    void SetIsSocketConnectedLastCallTime();

    // ******************************************************************************************
    // non-blocking methods for working with the object (they are asynchronous, this object
    // receives the results in the "sockets" thread - the "receive" methods of this object are called there)
    // ******************************************************************************************

    // obtains an IP address from a host name (or even the textual IP address itself); 'hostUID' is used
    // to identify the result when this method is called multiple times; the result (including 'hostUID')
    // will be in the parameters of the ReceiveHostByAddress method called in the "sockets" thread;
    // returns TRUE if there is a chance of success (if the thread retrieving the IP address can be started),
    // returns FALSE on failure; if it returns TRUE and this object was not connected to
    // SocketsThread, it connects it (see the AddSocket method)
    // WARNING: this method cannot be called from the SocketCritSect critical section (the method uses
    //          SocketsThread); the exception is when we are already inside the CSocketsThread::CritSect critical section
    // callable from any thread
    BOOL GetHostByAddress(const char* address, int hostUID = 0);

    // connects to a SOCKS 4/4A/5 or HTTP 1.1 (the proxy type is in 'proxyType') proxy server
    // 'serverIP' on port 'serverPort' + if it is not one of these proxy servers, it works the same as
    // the Connect method; creates a Windows socket and sets it as non-blocking - it sends messages
    // to the SocketsThread object, which, according to the proxy server protocol, performs
    // the connection to address 'host' port 'port' with proxy-user-name 'proxyUser' and proxy-password
    // 'proxyPassword' (only SOCKS 5 and HTTP 1.1); 'hostIP' (only SOCKS 4) is the IP address of 'host'
    // (if unknown, use INADDR_NONE); returns TRUE if there is a chance of success (the result of
    // connecting to 'host' is received by the ReceiveNetEvent method - FD_CONNECT), returns FALSE on failure
    // and if the Windows error code is known, returns it in 'error' (if not NULL); if it returns TRUE
    // and this object was not connected to SocketsThread, it connects it (see the AddSocket method)
    // WARNING: this method cannot be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // callable from any thread
    BOOL ConnectWithProxy(DWORD serverIP, unsigned short serverPort, CFTPProxyServerType proxyType,
                          DWORD* err, const char* host, unsigned short port, const char* proxyUser,
                          const char* proxyPassword, DWORD hostIP);

    // connects to IP address 'ip' on port 'port'; creates a Windows socket and sets it as non-blocking -
    // it sends messages to the SocketsThread object, which based on these messages calls the
    // ReceiveNetEvent method of this object; returns TRUE if there is a chance of success
    // (the result is received by the ReceiveNetEvent method - FD_CONNECT), returns FALSE on failure
    // and if the Windows error code is known, returns it in 'error' (if not NULL); if it returns
    // TRUE and this object was not connected to SocketsThread, it connects it (see the AddSocket method);
    // 'calledFromConnect' is TRUE only when called from one of the ConnectUsingXXX methods;
    // WARNING: this method cannot be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // callable from any thread
    BOOL Connect(DWORD ip, unsigned short port, DWORD* error, BOOL calledFromConnect = FALSE);

    // if an error occurred while connecting through the proxy server, this method returns
    // TRUE + the error in 'errBuf'+'formatBuf'; otherwise the method returns FALSE; 'errBuf' is
    // the buffer for the error text with a length of at least 'errBufSize' characters; 'formatBuf' is a buffer
    // (with a length of at least 'formatBufSize' characters) for the formatting string (for sprintf)
    // describing when the error occurred; if 'oneLineText' is TRUE, only 'errBuf' is filled
    // and only with a single line of text (without CR+LF)
    BOOL GetProxyError(char* errBuf, int errBufSize, char* formatBuf, int formatBufSize,
                       BOOL oneLineText);

    // returns a description of the timeout that occurred when connecting through the proxy server; returns FALSE
    // if it is a timeout while connecting to the FTP server; 'buf' is a buffer for the timeout description
    // with a length of at least 'bufSize' characters
    BOOL GetProxyTimeoutDescr(char* buf, int bufSize);

    // returns TRUE if the socket is not closed (INVALID_SOCKET)
    BOOL IsConnected();

    // initiates a socket shutdown - after successfully sending + confirming the unsent data,
    // FD_CLOSE arrives and closes the socket (releases Windows socket resources); after starting
    // the shutdown, no data can be written to the socket anymore (if Write does not write the whole buffer
    // at once, you must wait for the write to finish - event ccsevWriteDone);
    // returns TRUE on success, FALSE on error - if the Windows error code is known, returns it
    // in 'error' (if not NULL)
    // NOTE: after receiving FD_CLOSE, the SocketWasClosed method is called (information about the socket closure)
    BOOL Shutdown(DWORD* error);

    // hard socket closure (only when Shutdown timed out) - calls closesocket (deallocation of
    // the Windows socket); returns TRUE on success, FALSE on error - if the Windows error
    // code is known, returns it in 'error' (if not NULL)
    BOOL CloseSocket(DWORD* error);

    // encrypts the socket, returns TRUE on success; if the server certificate cannot be
    // verified and the user has not previously accepted it as trusted: if 'unverifiedCert'
    // is NULL, it returns failure and SSLCONERR_UNVERIFIEDCERT in 'sslErrorOccured'; if
    // 'unverifiedCert' is not NULL, it returns success and the server certificate in 'unverifiedCert',
    // the caller is responsible for releasing it by calling unverifiedCert->Release() and
    // returns the reason why the certificate cannot be verified in 'errorBuf' (of size 'errorBufLen',
    // if it is 0, 'errorBuf' can be NULL); in 'errorID' (if not NULL) it returns the resource-id
    // of the error text or -1 if no error should be displayed; for other errors
    // (except an untrusted certificate): it returns NULL in 'unverifiedCert', returns supplementary
    // text for the error in 'errorBuf' (of size 'errorBufLen', if it is 0, 'errorBuf' can be NULL) (it is inserted
    // into 'errorID' at position %s via sprintf); in 'sslErrorOccured' (if not NULL) it returns the error code (one of SSLCONERR_XXX);
    // 'logUID' is the log UID; 'conForReuse' (if not NULL) is the socket whose SSL session should be reused (called
    // "SSL session reuse", see for example http://vincent.bernat.im/en/blog/2011-ssl-session-reuse-rfc5077.html
    // and it is used from the control connection for all its data connections)
    BOOL EncryptSocket(int logUID, int* sslErrorOccured, CCertificate** unverifiedCert,
                       int* errorID, char* errorBuf, int errorBufLen, CSocket* conForReuse);

    // connects to a SOCKS 4/4A/5 or HTTP 1.1 (the proxy type is in 'proxyType') proxy server
    // 'proxyIP' on port 'proxyPort' and opens a port for "listen" on it; the IP+port where it
    // listens is received by the socket in the ListeningForConnection() method; if it is not one of these
    // proxy servers, it works like OpenForListening(), except that it also passes the result
    // through ListeningForConnection() - it is called directly from the OpenForListeningWithProxy() method;
    // 'listenOnIP'+'listenOnPort' is used only when it is not a connection through these proxy
    // servers - 'listenOnIP' is the IP of this machine (when binding the socket on multi-home machines
    // the IP may not be detectable, for FTP we take the IP from the "control connection"), 'listenOnPort' is
    // the port on which to listen; if it does not matter, value 0 is used;
    // it creates a Windows socket and sets it as non-blocking - it sends messages to the
    // SocketsThread object, which according to the proxy server protocol requests the opening of a "listen" port
    // for connections from address 'host' (IP 'hostIP') port 'hostPort' with proxy-user-name
    // 'proxyUser' and proxy-password 'proxyPassword' (only SOCKS 5 and HTTP 1.1); 'hostIP'
    // (used only for SOCKS 4, otherwise INADDR_NONE) is the IP address of 'host' (the IP must be
    // known - a connection to 'hostIP' must be open, otherwise it is impossible to request this opening of a
    // "listen" port); returns TRUE if there is a chance of success (the "listen" IP+port is received by the
    // ListeningForConnection() method, and then the result of the connection from 'host' is received by the
    // ConnectionAccepted() method - provided that the CSocket::ReceiveNetEvent method is called for FD_ACCEPT);
    // returns FALSE on failure and if the Windows error code is known, returns it in 'err' (if not NULL), and
    // if 'listenError' is not NULL, returns TRUE/FALSE in it depending on whether it is a LISTEN error (listen error without a proxy server)
    // or CONNECT error (error connecting to the proxy server); if it returns TRUE and this object was not connected
    // to SocketsThread, it connects it (see the AddSocket method)
    // WARNING: this method cannot be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // callable from any thread
    BOOL OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                   const char* host, DWORD hostIP, unsigned short hostPort,
                                   CFTPProxyServerType proxyType, DWORD proxyIP,
                                   unsigned short proxyPort, const char* proxyUser,
                                   const char* proxyPassword, BOOL* listenError, DWORD* err);

    // opens a socket and waits for a connection on it (listens); 'listenOnIP' (must not be NULL) is on
    // input the IP of this machine (when binding the socket on multi-home machines the IP may not be detectable,
    // for FTP we take the IP from the "control connection"), on output it is the IP where the connection is awaited;
    // 'listenOnPort' (must not be NULL) is on input the port on which to wait for a connection,
    // if it does not matter, value 0 is used; on output it is the port where the connection is awaited;
    // it creates a Windows socket and sets it as non-blocking - it sends messages to the
    // SocketsThread object, which based on these messages calls the ReceiveNetEvent method of this object
    // (the incoming connection is announced by calling the ConnectionAccepted() method - provided that the
    // CSocket::ReceiveNetEvent method is called for FD_ACCEPT); returns TRUE when the socket is opened successfully,
    // returns FALSE on failure and if the Windows error code is known, returns it in 'error' (if not NULL);
    // if it returns TRUE and this object was not connected to SocketsThread, it connects it (see the AddSocket method)
    // WARNING: this method cannot be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // callable from any thread
    BOOL OpenForListening(DWORD* listenOnIP, unsigned short* listenOnPort, DWORD* error);

    // returns the local IP address, the socket must be connected (see the getsockname() function);
    // returns the discovered IP address in 'ip' (must not be NULL); returns TRUE on success;
    // returns FALSE on failure and if the Windows error code is known, returns it in 'error'
    // (if not NULL)
    // callable from any thread
    BOOL GetLocalIP(DWORD* ip, DWORD* error);

    // ******************************************************************************************
    // methods called in the "sockets" thread (based on receiving messages from the system or other threads)
    //
    // WARNING: called inside SocketsThread->CritSect, they should be performed as quickly as possible (no
    //          waiting for user input, etc.)
    // ******************************************************************************************

    // receives the result of calling GetHostByAddress; if 'ip' == INADDR_NONE it is an error and 'err'
    // may contain the error code (if 'err' != 0)
    virtual void ReceiveHostByAddress(DWORD ip, int hostUID, int err);

    // receives events for this socket (FD_READ, FD_WRITE, FD_CLOSE, etc.); 'index' is
    // the index of the socket in the SocketsThread->Sockets array (used for repeated sending of
    // messages for the socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // receives the result of ReceiveNetEvent(FD_CLOSE) - if 'error' is not NO_ERROR, it is
    // the Windows error code (arrived with FD_CLOSE or occurred during FD_CLOSE processing)
    virtual void SocketWasClosed(DWORD error) {}

    // receives a timer with ID 'id' and parameter 'param'
    virtual void ReceiveTimer(DWORD id, void* param) {}

    // receives a posted message with ID 'id' and parameter 'param'
    virtual void ReceivePostMessage(DWORD id, void* param) {}

    // internal method of this object: handles the connection to the proxy server, after connecting or failing to connect
    // it redirects events to the virtual ReceiveNetEvent() method
    void ReceiveNetEventInt(LPARAM lParam, int index);

    // internal method of this object: handles connecting to the proxy server - specifically for Socks4 waiting
    // for the FTP server IP address, everything else is redirected to the ReceiveHostByAddress() method;
    // 'index' is the index of the socket in the SocketsThread->Sockets array (used when calling ReceiveNetEvent())
    void ReceiveHostByAddressInt(DWORD ip, int hostUID, int err, int index);

    // called after opening the socket for "listen": 'listenOnIP'+'listenOnPort' is the IP+port where
    // listening takes place; 'proxyError' is TRUE if the proxy server reports an error when opening the socket for
    // "listen" (the error text can be obtained via GetProxyError())
    // WARNING: when connecting without a proxy server this method is called directly from OpenForListeningWithProxy(),
    //          so it does not have to be from the "sockets" thread
    // WARNING: must not be called from the CSocket::SocketCritSect critical section
    virtual void ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                        BOOL proxyError) {}

    // called after receiving and processing FD_ACCEPT (provided that the CSocket::ReceiveNetEvent method is called for FD_ACCEPT):
    // 'success' indicates whether accept succeeded; on failure 'winError' contains the Windows error code (used only when connecting without a proxy server)
    // and 'proxyError' is TRUE if the error is reported by the proxy server (the error text can be obtained via GetProxyError())
    // WARNING: call only after a single entry into the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError) {}

protected:
    // helper method for setting data for connecting through a proxy server
    // WARNING: call only from the SocketCritSect section
    BOOL SetProxyData(const char* hostAddress, unsigned short hostPort,
                      const char* proxyUser, const char* proxyPassword,
                      DWORD hostIP, DWORD* error, DWORD proxyIP);

    // helper method: sends bytes from 'buf' to the socket; 'index' is the socket index in the
    // SocketsThread->Sockets array (used when calling ReceiveNetEvent()); inside the method
    // the SocketCritSect critical section may be left, in which case it returns TRUE in 'csLeft';
    // 'isConnect' is TRUE/FALSE for CONNECT/LISTEN
    // WARNING: call only with a single nesting level in the SocketCritSect section
    void ProxySendBytes(const char* buf, int bufLen, int index, BOOL* csLeft, BOOL isConnect);

    // helper methods: 'index' is the socket index in the SocketsThread->Sockets array (used
    // when calling ReceiveNetEvent()); inside the method the SocketCritSect critical section may be left,
    // in which case it returns TRUE in 'csLeft'
    // WARNING: call only with a single nesting level in the SocketCritSect section
    //
    // sends request 'request' (1=CONNECT, 2=LISTEN) to the SOCKS4 proxy server; 'isConnect' is
    // TRUE/FALSE for CONNECT/LISTEN; 'isSocks4A' is TRUE/FALSE for SOCKS 4A/4
    void Socks4SendRequest(int request, int index, BOOL* csLeft, BOOL isConnect, BOOL isSocks4A);
    // sends the list of supported authentication methods to the SOCKS5 proxy server; 'isConnect'
    // is TRUE/FALSE for CONNECT/LISTEN
    void Socks5SendMethods(int index, BOOL* csLeft, BOOL isConnect);
    // sends request 'request' (1=CONNECT, 2=LISTEN) to the SOCKS5 proxy server; 'isConnect' is
    // TRUE/FALSE for CONNECT/LISTEN
    void Socks5SendRequest(int request, int index, BOOL* csLeft, BOOL isConnect);
    // sends username+password to the SOCKS5 proxy server; 'isConnect' is TRUE/FALSE for CONNECT/LISTEN
    void Socks5SendLogin(int index, BOOL* csLeft, BOOL isConnect);
    // sends a request (to open a connection to the FTP server) to the HTTP 1.1 proxy server
    void HTTP11SendRequest(int index, BOOL* csLeft);

    // helper method: receives a reply from the proxy server; returns TRUE if any data appears on
    // the socket (if FD_CLOSE arrived, it returns the data + further FD_CLOSE processing is up to the caller);
    // WARNING: if it returns FALSE, it has left the SocketCritSect section; 'buf'
    // is the buffer for this response ('read' is on input the size of the 'buf' buffer, on
    // output it is the number of bytes read); 'index' is the socket index in the
    // SocketsThread->Sockets array (used when calling ReceiveNetEvent()); 'isConnect' is
    // TRUE/FALSE for CONNECT/LISTEN_or_ACCEPT; 'isListen' only makes sense when
    // 'isConnect'==FALSE and is TRUE/FALSE for LISTEN/ACCEPT; if 'readOnlyToEOL' is TRUE,
    // the socket is read byte by byte only until the first LF occurs (reads at most
    // one line including the trailing LF)
    // WARNING: call only with a single nesting in the SocketCritSect section and at least one
    //          nesting in the CSocketsThread::CritSect section
    BOOL ProxyReceiveBytes(LPARAM lParam, char* buf, int* read, int index,
                           BOOL isConnect, BOOL isListen, BOOL readOnlyToEOL);

    // helper method: sets larger buffers on the data connection socket (speeding up
    // listing, downloads and uploads)
    void SetSndRcvBuffers();
};

//
// ****************************************************************************
// CSocketsThread
//
// thread handling all sockets (contains an invisible window for receiving messages),
// used by socket objects; outside the sockets modules it is not needed

struct CMsgData // data for WM_APP_SOCKET_ADDR
{
    int SocketMsg;
    int SocketUID;
    DWORD IP;
    int HostUID;
    int Err;

    CMsgData(int socketMsg, int socketUID, DWORD ip, int hostUID, int err)
    {
        SocketMsg = socketMsg;
        SocketUID = socketUID;
        IP = ip;
        HostUID = hostUID;
        Err = err;
    }
};

#define SOCKETSTHREAD_TIMERID 666 // Windows timer ID of CSocketsThread::Timers (see CSocketsThread::AddTimer())

struct CTimerData // data for WM_TIMER (see CSocketsThread::AddTimer())
{
    int SocketMsg;    // message number used to receive events for the informed socket; if (WM_APP_SOCKET_MIN-1), it is a deleted timer in the locked section of the Timers array
    int SocketUID;    // UID of the informed socket
    DWORD TimeoutAbs; // absolute time in milliseconds; the timer fires only when GetTickCount() returns at least this value
    DWORD ID;         // timer id
    void* Param;      // timer parameter

    CTimerData(int socketMsg, int socketUID, DWORD timeoutAbs, DWORD id, void* param)
    {
        SocketMsg = socketMsg;
        SocketUID = socketUID;
        TimeoutAbs = timeoutAbs;
        ID = id;
        Param = param;
    }
};

struct CPostMsgData // data for WM_APP_SOCKET_POSTMSG
{
    int SocketMsg; // message number used to receive events for the informed socket
    int SocketUID; // UID of the informed socket
    DWORD ID;      // event id
    void* Param;   // event parameter

    CPostMsgData(int socketMsg, int socketUID, DWORD id, void* param)
    {
        SocketMsg = socketMsg;
        SocketUID = socketUID;
        ID = id;
        Param = param;
    }
};

class CSocketsThread : public CThread
{
protected:
    CRITICAL_SECTION CritSect; // critical section for accessing the object's data
    HANDLE RunningEvent;       // signaled only after the message loop in the thread starts
    HANDLE CanEndThread;       // signaled only after IsRunning() is called - the main thread has already read 'Running'
    BOOL Running;              // TRUE after the message loop in the thread starts successfully (otherwise it reports an error)
    HWND HWindow;              // handle of the invisible window for receiving socket messages
    BOOL Terminating;          // TRUE only after Terminate() is called

    // array of handled objects (descendants of CSocket); positions in the array correspond to received
    // messages (index zero == WM_APP_SOCKET_MIN), so ARRAY ELEMENTS MUST NOT BE MOVED (removal
    // is handled by rewriting the index to NULL)
    TIndirectArray<CSocket> Sockets;
    int FirstFreeIndexInSockets; // lowest free index inside the Sockets array (-1 = none)

    TIndirectArray<CMsgData> MsgData; // data for WM_APP_SOCKET_ADDR (when receiving the message we distribute data from the array)

    TIndirectArray<CTimerData> Timers;    // array of timers for sockets
    int LockedTimers;                     // section of the Timers array (number of elements from the start of the array) that must not change (used during CSocketsThread::ReceiveTimer()); -1 = such a section does not exist
    static DWORD LastWM_TIMER_Processing; // GetTickCount() from the time of the last WM_TIMER processing (WM_TIMER arrives only during an "idle" message loop, which is unacceptable for us)

    TIndirectArray<CPostMsgData> PostMsgs; // data for WM_APP_SOCKET_POSTMSG (when receiving the message we distribute data from the array)

public:
    CSocketsThread();
    ~CSocketsThread();

    // returns the state of the object (TRUE=OK); must be called after the constructor to determine the result of construction
    BOOL IsGood() { return Sockets.IsGood() && RunningEvent != NULL && CanEndThread != NULL; }

    // returns the handle of the invisible window (window for receiving socket messages)
    HWND GetHiddenWindow() const { return HWindow; }

    void LockSocketsThread() { HANDLES(EnterCriticalSection(&CritSect)); }
    void UnlockSocketsThread() { HANDLES(LeaveCriticalSection(&CritSect)); }

    // called by the main thread - waits until it is clear whether the thread started, then returns TRUE
    // for successful execution or FALSE on error
    BOOL IsRunning();

    // called by the main thread if the sockets thread needs to terminate
    void Terminate();

    // adds a timer to the timers array with timeout 'timeoutAbs' (absolute time in milliseconds,
    // the timer will fire only after GetTickCount() returns at least this value);
    // after the timer fires it is removed from the timers array; 'socketMsg'+'socketUID' identifies
    // the socket that should be notified of the added timer timeout (see the CSocket::ReceiveTimer() method);
    // 'id' is the timer ID; 'param' is an optional timer parameter, if it contains any allocated
    // value, the CSocket object must take care of deallocation when receiving the timer, when adding the timer fails,
    // or when unloading the plugin; returns TRUE when the timer is added successfully, otherwise returns FALSE
    // (the only error is lack of memory)
    // callable from any thread
    BOOL AddTimer(int socketMsg, int socketUID, DWORD timeoutAbs, DWORD id, void* param);

    // finds and removes from the timers array the timer with ID 'id' for the socket with UID 'socketUID';
    // if there are multiple timers in the array that match the criteria, all of them are removed;
    // returns TRUE if at least one timer was found and removed
    // callable from any thread
    BOOL DeleteTimer(int socketUID, DWORD id);

    // queues a posted message - indirectly invokes the CSocket::ReceivePostMessage method;
    // the message might not be accepted for delivery (lack of memory or PostMessage error) - then it returns FALSE;
    // 'socketMsg' and 'socketUID' identify the socket object that should receive the message;
    // 'id' and 'param' are parameters passed to CSocket::ReceivePostMessage; if
    // 'param' holds any allocated value, the CSocket object must take care of deallocation
    // when receiving the message, when PostSocketMessage fails, or when unloading the plugin
    // callable from any thread
    BOOL PostSocketMessage(int socketMsg, int socketUID, DWORD id, void* param);

    // checks whether a socket object with UID 'socketUID' exists;
    // if it exists returns TRUE (otherwise FALSE); if the socket exists it returns
    // TRUE in 'isConnected' (if not NULL) when the socket is not closed (INVALID_SOCKET),
    // otherwise it returns FALSE in 'isConnected'; sets the time of the last IsSocketConnected()
    // call in the socket - IsSocketConnectedLastCallTime
    BOOL IsSocketConnected(int socketUID, BOOL* isConnected);

    // adds 'sock' (must be allocated) to the array of handled objects; returns TRUE on success
    // ('sock' will be deallocated automatically when the socket closes); if it returns FALSE,
    // the object 'sock' needs to be deallocated
    // callable from any thread
    BOOL AddSocket(CSocket* sock);

    // deallocates/detaches ('onlyDetach' is FALSE/TRUE) the socket object 'sock' from the array of handled
    // objects (writes NULL to the position and sets FirstFreeIndexInSockets); if the object is not in the array
    // and 'onlyDetach' is TRUE, it is deallocated
    // callable from any thread
    void DeleteSocket(CSocket* sock, BOOL onlyDetach = FALSE)
    {
        if (sock != NULL)
        {
            int i = sock->GetMsgIndex();
            if (i != -1)
                DeleteSocketFromIndex(i, onlyDetach); // if it is in the array of handled objects
            else
            {
                if (!onlyDetach)
                {
#ifdef _DEBUG
                    BOOL old = InDeleteSocket;
                    InDeleteSocket = TRUE; // we might not be directly in ::DeleteSocket, but the call is correct
#endif
                    delete sock; // not in the array, release it directly
#ifdef _DEBUG
                    InDeleteSocket = old;
#endif
                }
            }
        }
    }

    // deallocates/detaches ('onlyDetach' is FALSE/TRUE) the socket object at position 'index' in the
    // array of handled objects (writes NULL to the position and sets FirstFreeIndexInSockets)
    // callable from any thread
    void DeleteSocketFromIndex(int index, BOOL onlyDetach = FALSE);

    // pair of methods that allow socket objects to be swapped - undelivered and new events on the
    // socket, results of GetHostByAddress, timers and posted messages are delivered
    // to the swapped socket object; BeginSocketsSwap() ensures entering the sockets thread critical
    // section (CritSect) and swapping the socket objects; after calling
    // BeginSocketsSwap() it is possible to swap the internal data of the socket object (except for data of the
    // CSocket class, those are already swapped) without the risk of delivering any message to the
    // socket objects in the sockets thread; to complete the swap you must call
    // EndSocketsSwap(), which leaves the sockets thread critical section (CritSect), thus
    // allowing the normal operation of the sockets thread; 'sock1' and 'sock2' (must not be NULL and
    // must be in the array of handled objects - see the AddSocket() method) are the swapped socket objects
    void BeginSocketsSwap(CSocket* sock1, CSocket* sock2);
    void EndSocketsSwap();

    // ******************************************************************************************
    // private methods of the object, do not call from outside (outside sockets.cpp)
    // ******************************************************************************************

    // queues a message with the result of calling CSocket::GetHostByAddress - indirectly invokes
    // the CSocket::ReceiveHostByAddress method; the message might not be accepted for delivery (lack of
    // memory or PostMessage error) - then it returns FALSE; 'socketMsg' and 'socketUID' identify
    // the socket object that should receive the message; 'ip', 'hostUID' and 'err' are parameters passed
    // to CSocket::ReceiveHostByAddress
    // callable from any thread
    BOOL PostHostByAddressResult(int socketMsg, int socketUID, DWORD ip, int hostUID, int err);

    // called by CSocketsThread::WindowProc when receiving WM_APP_SOCKET_ADDR
    void ReceiveMsgData();

    // called by CSocketsThread::WindowProc when receiving WM_TIMER
    void ReceiveTimer();

    // called by CSocketsThread::WindowProc when receiving WM_APP_SOCKET_POSTMSG
    void ReceivePostMessage();

    // called by CSocketsThread::WindowProc when receiving WM_APP_SOCKET_MIN to WM_APP_SOCKET_MAX
    LRESULT ReceiveNetEvent(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // this method contains the body of the thread
    virtual unsigned Body();

protected:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // finds the index in the Timers array where a new timer with timeout 'timeoutAbs' should be inserted
    // if timers with this timeout already exist in the array, the new timer is inserted after them
    // (preserves chronological order); 'leftIndex' is the first index where the new timer can
    // be inserted (used when the beginning of the Timers array is locked)
    // WARNING: call only from the 'CritSect' section
    int FindIndexForNewTimer(DWORD timeoutAbs, int leftIndex);
};

extern CSocketsThread* SocketsThread; // thread handling all sockets, intended only for internal use in the sockets modules
