// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

//#define DEBUGLOGPACKETSIZEANDWRITESIZE    // enabled = logs the current block sizes sent during upload to the socket and, additionally, every second how much data was successfully sent

// ****************************************************************************
// constants:

#define DATACON_BYTESTOREADONSOCKET 8192         // minimum number of bytes to read from the socket (also allocate the buffer for the read data)
#define DATACON_BYTESTOREADONSOCKETPREALLOC 8192 // how many bytes to preallocate for reading (so the next read does not immediately allocate again)

#define KEEPALIVEDATACON_READBUFSIZE 8192 // how many bytes to read from the socket (data are discarded - keep-alive only)

#define DATACON_FLUSHBUFFERSIZE 65536     // buffer size for handing data over for verification/write to disk (see CDataConnectionSocket::FlushBuffer)
#define DATACON_FLUSHTIMEOUT 1000         // time in milliseconds after which data are passed on for verification/write to disk if the flush buffer did not already trigger it (see CDataConnectionSocket::FlushBuffer)
#define DATACON_TESTNODATATRTIMEOUT 10000 // time in milliseconds between periodic checks of the no-data-transfer timeout

#define DATACON_FLUSHTIMERID 50        // ID of the timer in the data connection that ensures periodic data flushing (see DATACON_FLUSHTIMEOUT)
#define DATACON_TESTNODATATRTIMERID 51 // ID of the timer in the data connection that ensures periodic testing of the no-data-transfer timeout (see DATACON_TESTNODATATRTIMEOUT)

#define DATACON_DISKWORKWRITEFINISHED 40 // ID of the message posted to the data connection when FTPDiskThread finishes the requested disk work - write (only for direct flushing of data to the file in the data connection, see CDataConnectionSocket::SetDirectFlushParams)

// WARNING: besides DATACON_UPLOADFLUSHBUFFERSIZE the estimated "packet" size during writes is also important (see CUploadDataConnectionSocket::LastPacketSizeEstimation)
#define DATACON_UPLOADFLUSHBUFFERSIZE 65536 // buffer size for handing data over for writing to the data connection (see CUploadDataConnectionSocket::FlushBuffer)

//
// *********************************************************************************
// CDataConnectionBaseSocket + CDataConnectionSocket + CUploadDataConnectionSocket
//
// socket object that serves as the "data connection" to an FTP server
// use the ::DeleteSocket() function for deallocation!

class CDataConnectionBaseSocket : public CSocket
{
protected:
    // the critical section for accessing the object's data is CSocket::SocketCritSect
    // WARNING: SocketsThread->CritSect must not be entered while inside this section (do not call SocketsThread methods)

    BOOL UsePassiveMode; // TRUE = passive mode (PASV), otherwise active mode (PORT)
    BOOL EncryptConnection;
    int CompressData;                 // 0 = MODE Z is not used; if not 0, MODE Z is used (transfer of compressed data)
    CFTPProxyForDataCon* ProxyServer; // NULL = "not used (direct connection)" (read-only, initialized in the constructor, therefore accessible without a critical section)
    DWORD ServerIP;                   // IP of the server we connect to in passive mode
    unsigned short ServerPort;        // server port we connect to in passive mode

    int LogUID; // log UID for the corresponding "control connection" (-1 until the log is created)

    DWORD NetEventLastError; // code of the last error reported to ReceiveNetEvent() or that occurred in PassiveConnect()
    int SSLErrorOccured;     // see constants SSLCONERR_XXX
    BOOL ReceivedConnected;  // TRUE = the "data connection" opened (connect or accept); does not describe the current state
    DWORD LastActivityTime;  // GetTickCount() from when we last worked with the "data connection" (tracks initialization (SetPassive() and SetActive()), successful connect, and read)
    DWORD SocketCloseTime;   // GetTickCount() from the moment the "data connection" socket closed

    CSynchronizedDWORD* GlobalLastActivityTime; // object for storing the last activity time of a group of data connections

    BOOL PostMessagesToWorker;        // TRUE = send messages to the owner (worker - see below for its identification)
    int WorkerSocketMsg;              // identifies the owner of the data connection
    int WorkerSocketUID;              // identifies the owner of the data connection
    DWORD WorkerMsgConnectedToServer; // ID of the message the data connection owner wants to receive after connecting to the server (the UID of this object goes into the 'param' parameter of PostSocketMessage)
    DWORD WorkerMsgConnectionClosed;  // ID of the message the data connection owner wants to receive after the connection to the server closes/breaks (the UID of this object goes into the 'param' parameter of PostSocketMessage)
    DWORD WorkerMsgListeningForCon;   // ID of the message the data connection owner wants to receive after the port for "listen" opens (the UID of this object goes into the 'param' parameter of PostSocketMessage)

    BOOL WorkerPaused;         // TRUE = the worker is in the "paused" state, so we must not transfer more data
    int DataTransferPostponed; // 1 = while WorkerPaused==TRUE a request to transfer data (FD_READ/FD_WRITE) was postponed, 2 = FD_CLOSE was postponed

    CTransferSpeedMeter TransferSpeedMeter;        // object for measuring the data transfer speed in this data connection
    CTransferSpeedMeter ComprTransferSpeedMeter;   // object for measuring the speed of compressed data transfer in this data connection (used only when MODE Z is enabled)
    CTransferSpeedMeter* GlobalTransferSpeedMeter; // object for measuring the overall data transfer speed (all data connection workers of the FTP operation); if NULL, it does not exist

    DWORD ListenOnIP;            // IP on which the proxy server listens (waits for a connection); if INADDR_NONE, the ListeningForConnection() method has not been called yet or the proxy server reported an error
    unsigned short ListenOnPort; // port on which the proxy server listens (waits for a connection)
    CSalZLIB ZLIBInfo;           // Control structure for ZLIB compression

    // SSLConForReuse: if not NULL, this is the socket whose SSL session should be reused
    // this is called "SSL session reuse", see for example http://vincent.bernat.im/en/blog/2011-ssl-session-reuse-rfc5077.html
    // and it is used from the control connection for all of its data connections
    // WARNING: used without synchronization, it relies on the assumption that SSLConForReuse
    //          (the control connection) definitely exists longer than this socket (the data connection)
    CSocket* SSLConForReuse;

public:
    CDataConnectionBaseSocket(CFTPProxyForDataCon* proxyServer, int encryptConnection,
                              CCertificate* certificate, int compressData, CSocket* conForReuse);
    virtual ~CDataConnectionBaseSocket();

    // description provided in the derived classes of this object
    virtual BOOL CloseSocketEx(DWORD* error) = 0;

    // sets parameters for the passive mode of the "data connection"; these parameters are used
    // in the PassiveConnect() method; 'ip' + 'port' are the connection parameters obtained from the server;
    // 'logUID' is the log UID of the "control connection" to which this "data connection" belongs
    void SetPassive(DWORD ip, unsigned short port, int logUID);

    // cleanup of variables before connecting the data socket
    virtual void ClearBeforeConnect() {}

    // calls Connect() for the stored parameters for passive mode
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called from any thread
    BOOL PassiveConnect(DWORD* error);

    // sets the object for storing the last activity time of a group of data connections
    // (all data connection workers of the FTP operation)
    // can be called from any thread
    void SetGlobalLastActivityTime(CSynchronizedDWORD* globalLastActivityTime);

    // sets the active mode of the "data connection"; socket opening see CSocket::OpenForListening();
    // 'logUID' is the log UID of the "control connection" to which this "data connection" belongs
    // can be called from any thread
    void SetActive(int logUID);

    // returns TRUE if the "data connection" is open for data transfer (the server connection has already been established);
    // in 'transferFinished' (if not NULL) it returns TRUE if the data transfer has already taken place (FALSE = the server connection has not been established yet)
    BOOL IsTransfering(BOOL* transferFinished);

    // returns LastActivityTime (uses a critical section)
    DWORD GetLastActivityTime();

    // called at the moment when the data transfer should start (for example after sending a listing command);
    // in passive mode it checks whether the first attempt to establish the connection was rejected
    // and, if so, it performs another attempt; in active mode nothing happens (allowing the connection via "accept"
    // could be placed here, but to keep things more general establishing the connection is always allowed);
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called from any thread
    virtual void ActivateConnection();

    // returns SocketCloseTime (uses a critical section)
    DWORD GetSocketCloseTime();

    // returns LogUID (within a critical section)
    int GetLogUID();

    // if this is passive mode and encryption of the data connection is enabled, attempts to encrypt it
    void EncryptPassiveDataCon();

    // sets the object for measuring overall data transfer speed (all data connection workers of the FTP operation)
    // can be called from any thread
    void SetGlobalTransferSpeedMeter(CTransferSpeedMeter* globalTransferSpeedMeter);

    // just wraps a call to CSocket::OpenForListeningWithProxy() with parameters from 'ProxyServer'
    BOOL OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                   BOOL* listenError, DWORD* err);

    // remembers the "listen" IP+port (where we wait for the connection) and whether an error occurred,
    // then posts WorkerMsgListeningForCon to the owner of the data connection
    // WARNING: without a proxy server this method is called directly from OpenForListeningWithProxy(),
    //          so it may not run in the "sockets" thread
    // WARNING: must not be called from the CSocket::SocketCritSect critical section
    virtual void ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                        BOOL proxyError);

    // if the "listen" port was opened successfully, returns TRUE + the "listen" IP+port
    // in 'listenOnIP'+'listenOnPort'; returns FALSE on a "listen" error
    BOOL GetListenIPAndPort(DWORD* listenOnIP, unsigned short* listenOnPort);

protected:
    // if PostMessagesToWorker==TRUE, posts message 'msgID' to the worker;
    // WARNING: must be called from the SocketCritSect critical section and only with a single nesting
    //          into this section
    void DoPostMessageToWorker(int msgID);

    // writes the NetEventLastError failure to the log
    void LogNetEventLastError(BOOL canBeProxyError);

private:
    // hides the CSocket::CloseSocket() method
    BOOL CloseSocket(DWORD* error) { TRACE_E("Use CloseSocketEx() instead of CloseSocket()!"); }
};

class CDataConnectionSocket : public CDataConnectionBaseSocket
{
protected:
    // the critical section for accessing the object's data is CSocket::SocketCritSect
    // WARNING: SocketsThread->CritSect must not be entered while inside this section (do not call SocketsThread methods)

    char* ReadBytes;              // buffer for bytes read from the socket (read after receiving FD_READ)
    int ValidBytesInReadBytesBuf; // number of valid bytes in the 'ReadBytes' buffer
    int ReadBytesAllocatedSize;   // allocated size of the 'ReadBytes' buffer
    BOOL ReadBytesLowMemory;      // TRUE = insufficient memory for the read data

    CQuadWord TotalReadBytesCount; // total number of bytes read from the data connection; with compression this holds the size of the decompressed data read from the data connection

    // shared among all threads - set to "signaled" after the "data connection" closes (after receiving all bytes) and also after the flush to disk is completed (see TgtDiskFileName)
    HANDLE TransferFinished;

    HWND WindowWithStatus;  // handle of the window that should receive notifications about changes in the "data connection" state (NULL = do not send anything)
    UINT StatusMessage;     // message sent when the "data connection" state changes
    BOOL StatusMessageSent; // TRUE = the previous message has not been delivered yet, so there is no point sending another one

    DWORD WorkerMsgFlushData; // ID of the message the data connection owner wants to receive once data are ready in the flush buffer for verification/write to disk (the UID of this object goes into the 'param' parameter of PostSocketMessage)

    BOOL FlushData;              // TRUE if data should be passed on for verification/write to disk, FALSE = data go entirely into the 'ReadBytes' memory
    char* FlushBuffer;           // flush buffer used to hand data over for verification/write to disk (NULL if it is not allocated (ValidBytesInFlushBuffer==0) or if it has been handed over to the worker i.e. disk thread)
    int ValidBytesInFlushBuffer; // number of valid bytes in the 'FlushBuffer' (WARNING: 'FlushBuffer' can also be NULL if it has been handed to the worker i.e. disk thread); if > 0 we wait until the worker finishes passing data on for verification/write to disk (starts by posting 'WorkerMsgFlushData' to the worker, ends by invoking this object's FlushDataFinished method from the worker)

    char* DecomprDataBuffer;             // decompressed data (from the flush buffer) for verification/write to disk (NULL if not allocated (DecomprDataAllocatedSize==0) or if handed over to the worker i.e. disk thread)
    int DecomprDataAllocatedSize;        // allocated size of the 'DecomprDataBuffer'
    int DecomprDataDelayedBytes;         // number of bytes in 'DecomprDataBuffer' whose verification/write to disk was postponed to the next GiveFlushData & FlushDataFinished cycle
    int AlreadyDecomprPartOfFlushBuffer; // where the next round of decompression from 'FlushBuffer' to 'DecomprDataBuffer' should start

    int NeedFlushReadBuf; // 0 = nothing; 1, 2, and 3 = needs to flush data; if 2 or 3, it then needs to post: 2 = FD_READ, 3 = FD_CLOSE
    BOOL FlushTimerAdded; // TRUE if a timer for flushing data exists (used only if the flush did not already happen because the read buffer filled up)

    CQuadWord DataTotalSize; // total size of the transferred data in bytes: -1 = unknown

    char TgtDiskFileName[MAX_PATH];           // if not "", this is the full name of the disk file to which data should be flushed (the file is overwritten; no resumes are performed here)
    HANDLE TgtDiskFile;                       // target disk file for flushing data (NULL = we have not opened it yet)
    BOOL TgtDiskFileCreated;                  // TRUE if the target disk file for flushing data was created
    DWORD TgtFileLastError;                   // code of the last error reported by the disk thread when writing to the TgtDiskFileName file
    CQuadWord TgtDiskFileSize;                // current size of the disk file used for flushing data
    CCurrentTransferMode CurrentTransferMode; // current transfer mode (ASCII/binary)
    BOOL AsciiTrModeForBinFileProblemOccured; // TRUE = the "ASCII mode for binary file" error was detected
    int AsciiTrModeForBinFileHowToSolve;      // how to solve the "ASCII mode for binary file" problem: 0 = ask the user, 1 = download again in binary mode, 2 = interrupt the file download (cancel), 3 = ignore (finish the download in ASCII mode)
    BOOL TgtDiskFileClosed;                   // TRUE = the file used for flushing data is already closed (no additional writing will occur)
    int TgtDiskFileCloseIndex;                // file close index in the disk-work thread (-1 = unused)
    BOOL DiskWorkIsUsed;                      // TRUE if DiskWork is queued in FTPDiskThread

    BOOL NoDataTransTimeout;  // TRUE = a no-data-transfer timeout occurred - we closed the data connection
    BOOL DecomprErrorOccured; // TRUE = a data decompression error occurred (MODE Z) - corrupted data arrived
    //BOOL DecomprMissingStreamEnd;   // unfortunately this test is not viable, for example Serv-U 7 and 8 simply do not terminate the stream // TRUE = decompression has not encountered the end of the stream yet (incomplete decompression) - used only when FlushData==TRUE (not for obtaining listings, there the decompression happens at once in GiveData and a stream without an end raises an error)

    // data that do not require a critical section (written only in the sockets thread and disk thread, synchronization via FTPDiskThread and DiskWorkIsUsed):
    CFTPDiskWork DiskWork; // work data submitted to the FTPDiskThread object (thread performing disk operations)

public:
    // 'flushData' is TRUE if data should be passed on for verification/write to disk, FALSE = data go entirely into the 'ReadBytes' memory
    CDataConnectionSocket(BOOL flushData, CFTPProxyForDataCon* proxyServer, int encryptConnection,
                          CCertificate* certificate, int compressData, CSocket* conForReuse);
    virtual ~CDataConnectionSocket();

    BOOL IsGood() { return TransferFinished != NULL; } // no critical section required

    HANDLE GetTransferFinishedEvent() { return TransferFinished; } // no critical section required

    // just calls CloseSocket() + sets SocketCloseTime and the TransferFinished event
    virtual BOOL CloseSocketEx(DWORD* error);

    // cleanup of variables before connecting the data socket
    virtual void ClearBeforeConnect();

    // returns NetEventLastError, ReadBytesLowMemory, TgtFileLastError, NoDataTransTimeout,
    // SSLErrorOccured, DecomprErrorOccured
    // (these use a critical section)
    void GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr, BOOL* noDataTransTimeout,
                  int* sslErrorOccured, BOOL* decomprErrorOccured);

    // returns DecomprMissingStreamEnd (uses a critical section)
    //BOOL GetDecomprMissingStreamEnd();

    // handing over data from the "data connection"; returns an allocated buffer with data and in 'length' (must not be
    // NULL) their length; if a decompression error occurs (MODE Z) it returns TRUE in 'decomprErr' and if
    // it is a data error, it returns an empty buffer; it returns NULL only when memory is insufficient
    char* GiveData(int* length, BOOL* decomprErr);

    // returns the status of the "data connection" (parameters must not be NULL): 'downloaded' (how much has already been
    // read/downloaded), 'total' (total size of the read/download - if unknown, returns -1),
    // 'connectionIdleTime' (time in seconds since data were last received), 'speed' (connection speed
    // in bytes per second)
    // can be called from any thread
    void GetStatus(CQuadWord* downloaded, CQuadWord* total, DWORD* connectionIdleTime, DWORD* speed);

    // sets the handle of the window that should receive (post-msg) notification messages
    // when the object's state changes (see GetStatus); 'hwnd' is the handle of such a window;
    // 'msg' is the message code the window reacts to; another message is sent only
    // after the StatusMessageReceived method is called (prevents sending redundant messages);
    // if 'hwnd' is NULL, it means no further messages should be sent
    void SetWindowWithStatus(HWND hwnd, UINT msg);

    // the window reports that it received the state-change message, so upon the next change
    // a message should be sent again
    void StatusMessageReceived();

    // enables/disables sending data connection messages to the worker; parameter meaning see
    // PostMessagesToWorker, WorkerSocketMsg, WorkerSocketUID, WorkerMsgConnectedToServer,
    // WorkerMsgConnectionClosed, WorkerMsgFlushData, and WorkerMsgListeningForCon
    void SetPostMessagesToWorker(BOOL post, int msg, int uid, DWORD msgIDConnected,
                                 DWORD msgIDConClosed, DWORD msgIDFlushData,
                                 DWORD msgIDListeningForCon);

    // sets TgtDiskFileName and CurrentTransferMode, while also enabling flushing data
    // directly into the TgtDiskFileName file (uses FTPDiskThread)
    void SetDirectFlushParams(const char* tgtFileName, CCurrentTransferMode currentTransferMode);

    // returns the state of the target file when flushing data directly to the TgtDiskFileName file;
    // in 'fileCreated' it returns TRUE if the file was created; in 'fileSize' it returns the size
    // of the file
    void GetTgtFileState(BOOL* fileCreated, CQuadWord* fileSize);

    // called after the data transfer finishes (successful or not); meaningful only when flushing
    // data directly into the TgtDiskFileName file
    void CloseTgtFile();

    // called by the worker after data flushing finishes; 'flushBuffer' is the buffer that was just flushed
    // (it is therefore returned to this object - retrieve it via GiveFlushData();
    // WARNING: it can also be a buffer from 'DecomprDataBuffer'); if there is more data to flush,
    // the buffers are swapped and WorkerMsgFlushData is posted to the worker; if necessary it will also post
    // FD_READ or FD_CLOSE to this socket (see NeedFlushReadBuf);
    // 'enterSocketCritSect' is TRUE except for the exception described below
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread), exception: if we are in CSocketsThread::CritSect and
    //          SocketCritSect, the call is possible if we set 'enterSocketCritSect' to FALSE
    void FlushDataFinished(char* flushBuffer, BOOL enterSocketCritSect);

    // if new data are present in FlushBuffer it returns TRUE and the data in 'flushBuffer'
    // (must not be NULL) and 'validBytesInFlushBuffer' (must not be NULL); after handing the data over
    // it nulls FlushBuffer (without MODE Z) or DecomprDataBuffer (with MODE Z) (returning it
    // via FlushDataFinished() or deallocating the buffer is up to the caller); in 'deleteTgtFile'
    // (must not be NULL) it returns TRUE if we discovered that data in the file may be
    // corrupted and the file must therefore be deleted; returns FALSE if no data flush is needed
    //
    BOOL GiveFlushData(char** flushBuffer, int* validBytesInFlushBuffer, BOOL* deleteTgtFile);

    // only when FlushData==TRUE: releases the loaded data from the buffer; serves only as
    // a safeguard against an error trace in the object's destructor (which reports that not all data
    // were flushed)
    void FreeFlushData();

    // called after this data connection closes; returns TRUE if all data are
    // flushed (meaning the data connection no longer contains any data); if 'onlyTest'
    // is FALSE and necessary, it swaps buffers and posts WorkerMsgFlushData
    // to the worker (flush the data); it returns TRUE only after FlushDataFinished is called for
    // the last flushed data
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    BOOL AreAllDataFlushed(BOOL onlyTest);

    // ensures an immediate forced termination of the data connection: calls CloseSocketEx and, when
    // using direct flushing of data from the data connection into a file, stops flushing
    // data and clears the flush buffer
    void CancelConnectionAndFlushing();

    // only when using direct flushing of data from the data connection into a file: returns TRUE
    // if data are being flushed; otherwise returns FALSE
    BOOL IsFlushingDataToDisk();

    // returns TRUE if the "ASCII transfer mode for binary file" problem needs to be handled, then
    // in 'howToSolve' it returns how to handle the problem: 0 = ask the user, 1 = download
    // again in binary mode, 2 = interrupt the file download (cancel); returns FALSE if
    // the problem did not occur or the user chose: ignore (finish the download in ASCII
    // mode)
    BOOL IsAsciiTrForBinFileProblem(int* howToSolve);

    // sets AsciiTrModeForBinFileHowToSolve to howToSolve; called after the user's answer
    // to the question how to solve the "ASCII transfer mode for binary file" problem; constants see
    // AsciiTrModeForBinFileHowToSolve (0, 1, 2, 3)
    void SetAsciiTrModeForBinFileHowToSolve(int howToSolve);

    // waits for the file into which data were flushed directly to close, or for a timeout
    // ('timeout' in milliseconds or the value INFINITE = no timeout); returns TRUE
    // if the closure occurred, FALSE on timeout
    BOOL WaitForFileClose(DWORD timeout);

    // sets the total size of the transferred data in bytes (-1 = unknown); uses a critical section and
    // will inform the "user-iface" object (just like about the size downloaded so far
    // of data, download speed, and connection idle time)
    // can be called from any thread
    void SetDataTotalSize(CQuadWord const& size);

    // called when the user switches the worker into/out of the "paused" state; in the "paused" state the data connection
    // should stop transferring data, and after the state ends the transfer should continue
    void UpdatePauseStatus(BOOL pause);

protected:
    // the "data connection" calls this when its state changes (evaluates whether
    // a status message should be sent to the window - see SetWindowWithStatus)
    void StatusHasChanged();

    // only when using direct flushing of data from the data connection into a file (see
    // SetDirectFlushParams), otherwise does nothing: ensures the flushed data are passed
    // to the disk-work thread
    // WARNING: must be called from the SocketCritSect critical section
    void DirectFlushData();

    // called after the connection with the server is established
    // WARNING: call only from the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    void JustConnected();

    // moves a filled ReadBytes buffer into FlushBuffer
    // WARNING: must be called from the SocketCritSect critical section
    void MoveReadBytesToFlushBuffer();

    // ******************************************************************************************
    // methods called in the "sockets" thread (based on messages received from the system or other threads)
    //
    // WARNING: called inside SocketsThread->CritSect, they should be executed as quickly as possible (no
    //        waiting for user input, etc.)
    // ******************************************************************************************

    // receiving events for this socket (FD_READ, FD_WRITE, FD_CLOSE, etc.); 'index' is
    // the socket index in the SocketsThread->Sockets array (used for repeatedly posting
    // messages for the socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // receiving the result of ReceiveNetEvent(FD_CLOSE) - if 'error' is not NO_ERROR it is
    // a Windows error code (came with FD_CLOSE or occurred while handling FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // receiving a timer with ID 'id' and parameter 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // receiving a posted message with ID 'id' and parameter 'param'
    virtual void ReceivePostMessage(DWORD id, void* param);

    // called after FD_ACCEPT is received and processed (assuming CSocket::ReceiveNetEvent is used for FD_ACCEPT)
    // CSocket::ReceiveNetEvent): 'success' indicates accept succeeded; on failure 'winError' contains
    // the Windows error code and 'proxyError' is TRUE if the proxy server reported the error
    // (retrieve the error text via GetProxyError())
    // WARNING: call only after a single entry into the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError);
};

class CUploadDataConnectionSocket : public CDataConnectionBaseSocket
{
protected:
    // the critical section for accessing the object's data is CSocket::SocketCritSect
    // WARNING: SocketsThread->CritSect must not be entered while inside this section (do not call SocketsThread methods)

    char* BytesToWrite;     // buffer for data that will be written to the socket (written after receiving FD_WRITE)
    int BytesToWriteCount;  // number of valid bytes in the 'BytesToWrite' buffer
    int BytesToWriteOffset; // number of bytes already sent from the 'BytesToWrite' buffer

    CQuadWord TotalWrittenBytesCount; // total number of bytes written to the data connection (WARNING: with MODE Z this is the size of the decompressed data)

    char* FlushBuffer;           // flush buffer for data prepared to be written to the socket (NULL if handed to the worker i.e. disk thread - data from disk are loaded into it)
    int ValidBytesInFlushBuffer; // number of valid bytes in the 'FlushBuffer'; if > 0, we wait until the socket write finishes and then refill BytesToWrite and try to read more data from the file
    BOOL EndOfFileReached;       // TRUE = no more data can be prepared for writing to the socket (we have reached the end of the file on disk)
    BOOL WaitingForWriteEvent;   // FALSE = when new data arrive for writing, we must post FD_WRITE; TRUE = currently waiting until more data can be written, then FD_WRITE will arrive (no need to post it)
    BOOL ConnectionClosedOnEOF;  // TRUE = we closed the data connection because all data were written up to end-of-file

    char* ComprDataBuffer;             // data that, after compression, are written to the flush buffer (for writing to the socket) (NULL if not allocated (ComprDataAllocatedSize==0) or if handed to the worker i.e. disk thread - data from disk are read into it)
    int ComprDataAllocatedSize;        // allocated size of the 'ComprDataBuffer'
    int ComprDataDelayedOffset;        // count of already compressed bytes in 'ComprDataBuffer'
    int ComprDataDelayedCount;         // number of valid bytes in 'ComprDataBuffer'; compression of bytes from 'ComprDataDelayedOffset' to 'ComprDataDelayedCount' is postponed to the next GiveBufferForData & DataBufferPrepared cycle
    int AlreadyComprPartOfFlushBuffer; // where to store the next batch of compressed data from 'ComprDataBuffer' into 'FlushBuffer'
    int DecomprBytesInBytesToWrite;    // number of bytes the data from BytesToWrite (from BytesToWriteOffset to BytesToWriteCount) decompress into
    int DecomprBytesInFlushBuffer;     // number of bytes the data from FlushBuffer decompress into

    DWORD WorkerMsgPrepareData; // ID of the message the data connection owner wants to receive once more data need to be prepared (read from disk) into the flush buffer for sending to the server (the UID of this object goes into the 'param' parameter of PostSocketMessage)
    BOOL PrepareDataMsgWasSent; // TRUE = WorkerMsgPrepareData has already been sent and we are waiting for the owner's (worker's) response; FALSE = if more data are needed, we send WorkerMsgPrepareData

    CQuadWord DataTotalSize; // total size of the transferred data in bytes (-1 = uninitialized value)

    BOOL NoDataTransTimeout; // TRUE = a no-data-transfer timeout occurred - we closed the data connection

    BOOL FirstWriteAfterConnect;      // TRUE = after this connection to the server no data have been written to the socket yet
    DWORD FirstWriteAfterConnectTime; // time of the first write to the socket (start of filling the local buffers) after this connection to the server
    DWORD SkippedWriteAfterConnect;   // number of bytes excluded from the upload speed calculation because they most likely went into a local buffer (artificially and incorrectly increases the initial upload speed)
    DWORD LastSpeedTestTime;          // time of the last connection speed test (to estimate how many bytes to write to the socket at once)
    DWORD LastPacketSizeEstimation;   // last estimate of the optimal "packet" size (how many bytes to write to the socket at once)
    DWORD PacketSizeChangeTime;       // time of the last change to LastPacketSizeEstimation
    DWORD BytesSentAfterPckSizeCh;    // number of bytes sent within one second after the last change to LastPacketSizeEstimation
    DWORD PacketSizeChangeSpeed;      // transfer speed before the last change to LastPacketSizeEstimation
    DWORD TooBigPacketSize;           // packet size at which transfer speed starts to degrade (e.g. from 5MB/s to 160KB/s); -1 if no such size was detected

    BOOL Activated; // Data not sent over socket until ActivateConnection is called

#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
    // auxiliary data section for DebugLogPacketSizeAndWriteSize
    DWORD DebugLastWriteToLog;        // time of the last log write (we write once per second to avoid flooding the log)
    DWORD DebugSentButNotLoggedBytes; // number of bytes skipped because of DebugLastWriteToLog
    DWORD DebugSentButNotLoggedCount; // number of written blocks skipped because of DebugLastWriteToLog
#endif                                // DEBUGLOGPACKETSIZEANDWRITESIZE

public:
    CUploadDataConnectionSocket(CFTPProxyForDataCon* proxyServer, int encryptConnection,
                                CCertificate* certificate, int compressData, CSocket* conForReuse);
    virtual ~CUploadDataConnectionSocket();

    BOOL IsGood() { return BytesToWrite != NULL && FlushBuffer != NULL; }

    // returns NetEventLastError, NoDataTransTimeout, and SSLErrorOccured (use a critical section)
    void GetError(DWORD* netErr, BOOL* noDataTransTimeout, int* sslErrorOccured);

    // just calls CloseSocket() + sets SocketCloseTime
    virtual BOOL CloseSocketEx(DWORD* error);

    // enables/disables sending upload data connection messages to the worker; parameter meaning see
    // PostMessagesToWorker, WorkerSocketMsg, WorkerSocketUID, WorkerMsgConnectedToServer,
    // WorkerMsgConnectionClosed, WorkerMsgPrepareData, and WorkerMsgListeningForCon
    void SetPostMessagesToWorker(BOOL post, int msg, int uid, DWORD msgIDConnected,
                                 DWORD msgIDConClosed, DWORD msgIDPrepareData,
                                 DWORD msgIDListeningForCon);

    // cleanup of variables before connecting the data socket
    virtual void ClearBeforeConnect();

    void ActivateConnection();

    // sets the total size of the transferred data in bytes; uses a critical section
    // can be called from any thread
    void SetDataTotalSize(CQuadWord const& size);

    // returns the actual number of bytes transferred by the data connection
    void GetTotalWrittenBytesCount(CQuadWord* uploadSize);

    // returns TRUE if all data were transferred successfully
    BOOL AllDataTransferred();

    // if FlushBuffer (ComprDataBuffer when compression is used) is empty and has not yet been handed
    // to the worker, returns TRUE and the FlushBuffer (ComprDataBuffer when compression is used) in 'flushBuffer'
    // (must not be NULL); after handing over the buffer, FlushBuffer (ComprDataBuffer when compression is used)
    // is cleared (returning it via FlushDataFinished() or deallocating the buffer is up to the caller);
    // returns FALSE if no data flush is needed
    BOOL GiveBufferForData(char** flushBuffer);

    // releases prepared data from both buffers (data for sending to the server); serves only as
    // a safeguard against an error trace in the object's destructor (which reports that not all data
    // were flushed)
    void FreeBufferedData();

    // called by the worker after finishing reading data from disk; 'flushBuffer' is the buffer that was just filled;
    // if the second data buffer is free and we have not reached the end of the file yet,
    // the buffers are swapped and WorkerMsgPrepareData is posted to the worker; if necessary it will also post
    // FD_WRITE to this socket (see WaitingForWriteEvent); with compression it ensures
    // gradually fills the FlushBuffer with compressed data (until it is full and we are not
    // at the end of the file, posts WorkerMsgPrepareData to the worker)
    void DataBufferPrepared(char* flushBuffer, int validBytesInFlushBuffer, BOOL enterCS);

    // called by the worker after receiving the response to the STOR/APPE command from the server (upload is finished on the server side)
    // (upload is finished on the server side)
    void UploadFinished();

    // returns the status of the "data connection" (parameters must not be NULL): 'uploaded' (how much has already been
    // written/uploaded), 'total' (total size of the write/upload),
    // 'connectionIdleTime' (time in seconds since data were last written), 'speed' (connection speed
    // in bytes per second)
    // can be called from any thread
    void GetStatus(CQuadWord* uploaded, CQuadWord* total, DWORD* connectionIdleTime, DWORD* speed);

    // called when the user switches the worker into/out of the "paused" state; the data connection should stop transferring data in the "paused" state
    // and after the state ends the transfer should continue
    void UpdatePauseStatus(BOOL pause);

protected:
    // called after the connection with the server is established
    // WARNING: call only from the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    void JustConnected();

#ifdef DEBUGLOGPACKETSIZEANDWRITESIZE
    // called to log values related to changes in the block size of data sent via 'send'
    // WARNING: call only from the CSocket::SocketCritSect critical section
    void DebugLogPacketSizeAndWriteSize(int size, BOOL noChangeOfLastPacketSizeEstimation = FALSE);
#endif // DEBUGLOGPACKETSIZEANDWRITESIZE

    // moves a filled FlushBuffer into BytesToWrite
    // WARNING: must be called from the SocketCritSect critical section
    void MoveFlushBufferToBytesToWrite();

    // ******************************************************************************************
    // methods called in the "sockets" thread (based on messages received from the system or other threads)
    //
    // WARNING: called inside SocketsThread->CritSect, they should be executed as quickly as possible (no
    //        waiting for user input, etc.)
    // ******************************************************************************************

    // receiving events for this socket (FD_READ, FD_WRITE, FD_CLOSE, etc.); 'index' is
    // the socket index in the SocketsThread->Sockets array (used for repeatedly posting
    // messages for the socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // receiving the result of ReceiveNetEvent(FD_CLOSE) - if 'error' is not NO_ERROR it is
    // a Windows error code (came with FD_CLOSE or occurred while handling FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // receiving a timer with ID 'id' and parameter 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // called after FD_ACCEPT is received and processed (assuming CSocket::ReceiveNetEvent is used for FD_ACCEPT)
    // CSocket::ReceiveNetEvent): 'success' indicates accept succeeded; on failure 'winError' contains
    // the Windows error code and 'proxyError' is TRUE if the proxy server reported the error
    // (retrieve the error text via GetProxyError())
    // WARNING: call only after a single entry into the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError);
};

//
// ****************************************************************************
// CKeepAliveDataConSocket
//
// socket object that serves as the "data connection" to an FTP server; all data
// are simply ignored, the data transfer is just "keep-alive" theatre for the server
// use the ::DeleteSocket() function for deallocation!

class CKeepAliveDataConSocket : public CSocket
{
protected:
    // the critical section for accessing the object's data is CSocket::SocketCritSect
    // WARNING: SocketsThread->CritSect must not be entered while inside this section (do not call SocketsThread methods)

    BOOL UsePassiveMode; // TRUE = passive mode (PASV), otherwise active mode (PORT)
    BOOL EncryptConnection;
    CFTPProxyForDataCon* ProxyServer; // NULL = "not used (direct connection)" (read-only, initialized in the constructor, therefore accessible without a critical section)
    DWORD ServerIP;                   // IP of the server we connect to in passive mode
    unsigned short ServerPort;        // server port we connect to in passive mode

    int LogUID; // log UID for the corresponding "control connection" (-1 until the log is created)

    DWORD NetEventLastError; // code of the last error reported to ReceiveNetEvent() or that occurred in PassiveConnect()
    int SSLErrorOccured;     // see constants SSLCONERR_XXX
    BOOL ReceivedConnected;  // TRUE = the "data connection" opened (connect or accept); does not describe the current state
    DWORD LastActivityTime;  // GetTickCount() from when we last worked with the "data connection" (tracks initialization (SetPassive() and SetActive()), successful connect, and read)
    DWORD SocketCloseTime;   // GetTickCount() from the moment the "data connection" socket closed

    // used when closing the "data connection" (evaluates whether the keep-alive command has finished)
    // WARNING: ParentControlSocket->SocketCritSect must not be entered (do not call ParentControlSocket methods)
    //        from this object's SocketCritSect (the opposite order of entry is already used, see
    //        ccsevTimeout in CControlConnectionSocket::WaitForEndOfKeepAlive())
    // NOTE: used the same way as CDataConnectionBaseSocket::SSLConForReuse, see its comment
    CControlConnectionSocket* ParentControlSocket;
    BOOL CallSetupNextKeepAliveTimer; // TRUE if SetupNextKeepAliveTimer() of ParentControlSocket should be called after closing the socket (indirectly)

    DWORD ListenOnIP;            // IP on which the proxy server listens (waits for a connection); if INADDR_NONE, ListeningForConnection() has not been called yet or the proxy server reported an error
    unsigned short ListenOnPort; // port on which the proxy server listens (waits for a connection)

public:
    CKeepAliveDataConSocket(CControlConnectionSocket* parentControlSocket, CFTPProxyForDataCon* proxyServer, int encryptConnection, CCertificate* certificate);
    virtual ~CKeepAliveDataConSocket();

    // called when the "control connection" receives the server's response ('replyCode')
    // announcing the end of the data transfer; returns TRUE if the transfer really finished (the
    // "data connection" can be released); returns FALSE if the transfer has not finished yet (the "data connection"
    // cannot be released); once the transfer actually ends, it indirectly calls SetupNextKeepAliveTimer()
    // of the ParentControlSocket object
    BOOL FinishDataTransfer(int replyCode);

    // returns TRUE if the "data connection" is open for data transfer (the connection to the server has already been established)
    // in 'transferFinished' (if not NULL) it returns TRUE if the transfer has already finished
    // (FALSE = the connection to the server has not been established yet)
    BOOL IsTransfering(BOOL* transferFinished);

    // just calls CloseSocket() + sets SocketCloseTime and the TransferFinished event
    BOOL CloseSocketEx(DWORD* error);

    // returns SocketCloseTime (uses a critical section)
    DWORD GetSocketCloseTime();

    // sets parameters for the passive mode of the "data connection"; these parameters are used
    // in the PassiveConnect() method; 'ip' + 'port' are the connection parameters obtained from the server;
    // 'logUID' is the log UID of the "control connection" to which this "data connection" belongs
    void SetPassive(DWORD ip, unsigned short port, int logUID);

    // calls Connect() for the stored parameters for passive mode
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //        SocketsThread)
    // can be called from any thread
    BOOL PassiveConnect(DWORD* error);

    // sets the active mode of the "data connection"; socket opening see CSocket::OpenForListening();
    // 'logUID' is the log UID of the "control connection" to which this "data connection" belongs
    // can be called from any thread
    void SetActive(int logUID);

    // called when the data transfer should start (for example after sending a listing command);
    // in passive mode it checks whether the first attempt to establish the connection was rejected
    // and, if so, performs another attempt; in active mode nothing happens
    // (accepting the connection via "accept" could be done here, but to keep things more general establishing the connection is always allowed)
    //
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //        SocketsThread)
    // can be called from any thread
    void ActivateConnection();

    // just wraps a call to CSocket::OpenForListeningWithProxy() with parameters from 'ProxyServer'
    BOOL OpenForListeningWithProxy(DWORD listenOnIP, unsigned short listenOnPort,
                                   BOOL* listenError, DWORD* err);

    // remembers the "listen" IP+port (where we wait for the connection) and whether an error occurred,
    // then posts CTRLCON_KALISTENFORCON to the owner of the data connection
    // WARNING: without a proxy server this method is called directly from OpenForListeningWithProxy(),
    //        so it may not run in the "sockets" thread
    // WARNING: must not be called from the CSocket::SocketCritSect critical section
    virtual void ListeningForConnection(DWORD listenOnIP, unsigned short listenOnPort,
                                        BOOL proxyError);

    // if the "listen" port was opened successfully, returns TRUE + the "listen" IP+port
    // in 'listenOnIP'+'listenOnPort'; returns FALSE on a "listen" error
    BOOL GetListenIPAndPort(DWORD* listenOnIP, unsigned short* listenOnPort);

    // if this is passive mode and encryption of the data connection is enabled, tries to encrypt it
    void EncryptPassiveDataCon();

protected:
    // called after the connection with the server is established
    // WARNING: call only from the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    void JustConnected();

    // writes the NetEventLastError failure to the log
    void LogNetEventLastError(BOOL canBeProxyError);

    // ******************************************************************************************
    // methods called in the "sockets" thread (based on messages received from the system or other threads)
    //
    // WARNING: called inside SocketsThread->CritSect, they should be executed as quickly as possible (no
    //        waiting for user input, etc.)
    // ******************************************************************************************

    // receiving events for this socket (FD_READ, FD_WRITE, FD_CLOSE, etc.); 'index' is
    // the socket index in the SocketsThread->Sockets array (used for repeatedly posting
    // messages for the socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // receiving the result of ReceiveNetEvent(FD_CLOSE) - if 'error' is not NO_ERROR it is
    // a Windows error code (came with FD_CLOSE or occurred while handling FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // timer reception with ID 'id' and parameter 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // called after FD_ACCEPT is received and processed (assuming CSocket::ReceiveNetEvent is used for FD_ACCEPT)
    // CSocket::ReceiveNetEvent): 'success' is the accept success flag; on failure it is in 'winError'
    // the Windows error code and 'proxyError' is TRUE if the proxy server reported the error
    // (retrieve the error text via GetProxyError())
    // WARNING: call only after a single entry into the CSocket::SocketCritSect and CSocketsThread::CritSect critical sections
    virtual void ConnectionAccepted(BOOL success, DWORD winError, BOOL proxyError);

private:
    // hides the CSocket::CloseSocket() method
    BOOL CloseSocket(DWORD* error) { TRACE_E("Use CloseSocketEx() instead of CloseSocket()!"); }
};
