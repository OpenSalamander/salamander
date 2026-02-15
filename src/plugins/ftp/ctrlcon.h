// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ***************************************************************************
// functions:

// helper function for converting a string from a stream into a null-terminated string
// 'buf'+'bufSize' is the output buffer; 'txt'+'size' is the input string
// returns 'buf'
char* CopyStr(char* buf, int bufSize, const char* txt, int size);

// helper function for decomposing a string with initial FTP commands (separated by ';')
// into individual commands; returns TRUE if another command is available (the
// command is returned in 's'); 'next' is an IN/OUT variable, initialize it to
// the start of the string and leave it unchanged between calls to GetToken
BOOL GetToken(char** s, char** next);

// command codes for PrepareFTPCommand (parameters in [] are passed through the ellipsis)
enum CFtpCmdCode
{
    ftpcmdQuit,              // [] - logout from FTP server
    ftpcmdSystem,            // [] - determine the operating system on the server (may be just a simulation)
    ftpcmdAbort,             // [] - abort the command currently being executed
    ftpcmdPrintWorkingPath,  // [] - get the working (current) directory on the FTP server
    ftpcmdChangeWorkingPath, // [char *path] - change the working directory on the FTP server
    ftpcmdSetTransferMode,   // [BOOL ascii] - set the transfer mode (ASCII/BINARY(IMAGE))
    ftpcmdPassive,           // [] - request the server to use "listen" for the data connection (the client establishes the data connection)
    ftpcmdSetPort,           // [DWORD IP, unsigned short port] - set the IP and port for the data connection on the server
    ftpcmdNoOperation,       // [] - keep-alive command "no operation"
    ftpcmdDeleteFile,        // [char *filename] - delete the file 'filename'
    ftpcmdDeleteDir,         // [char *dirname] - delete the directory 'dirname'
    ftpcmdChangeAttrs,       // [int newAttr, char *name] - change the attributes (mode) of the file/directory 'name' to 'newAttr'
    ftpcmdChangeAttrsQuoted, // [int newAttr, char *nameToQuotes (insert '\\' before every '"' character in the name)] - change the attributes (mode) of the file/directory 'name' to 'newAttr' - the file/directory name is in quotation marks (Linux FTP requires this for names with spaces)
    ftpcmdRestartTransfer,   // [char *number] - REST command (resume / restart transfer)
    ftpcmdRetrieveFile,      // [char *filename] - download the file 'filename'
    ftpcmdCreateDir,         // [char *path] - create the directory 'path'
    ftpcmdRenameFrom,        // [char *fromName] - start renaming ("rename from")
    ftpcmdRenameTo,          // [char *newName] - finish renaming ("rename to")
    ftpcmdStoreFile,         // [char *filename] - upload the file 'filename'
    ftpcmdGetSize,           // [char *filename] - get the size of the file (may also be a link to a file)
    ftpcmdAppendFile,        // [char *filename] - upload: append the file 'filename'
};

// prepares the text of a command for the FTP server (including CRLF at the end), returns TRUE
// if the command fits into the buffer 'buf' (with the size 'bufSize'); 'ftpCmd'
// is the command code (see CFtpCmdCode); if 'cmdLen' is not NULL, it returns the length
// of the prepared command text (it is always null-terminated at the end);
// 'logBuf' with the length 'logBufSize' will contain a version of the command suitable for
// the log file (passwords replaced with asterisks, etc.) - this is a null-terminated string
BOOL PrepareFTPCommand(char* buf, int bufSize, char* logBuf, int logBufSize,
                       CFtpCmdCode ftpCmd, int* cmdLen, ...);

// helper function for preparing error texts
const char* GetFatalErrorTxt(int fatalErrorTextID, char* errBuf);
// helper function for preparing error texts
const char* GetOperationFatalErrorTxt(int opFatalError, char* errBuf);

// ****************************************************************************
// macros:

// macros for extracting digits from the FTP reply code (expects an int in the range 0-999)
#define FTP_DIGIT_1(n) ((n) / 100)       // 1st digit
#define FTP_DIGIT_2(n) (((n) / 10) % 10) // 2nd digit
#define FTP_DIGIT_3(n) ((n) % 10)        // 3rd digit

// ****************************************************************************
// constants:

#define WAITWND_STARTCON 2000   // start control connection: show wait window after 2 seconds
#define WAITWND_CLOSECON 2000   // close control connection: show wait window after 2 seconds
#define WAITWND_COMOPER 2000    // regular operation (sending a command): show wait window after 2 seconds
#define WAITWND_PARSINGLST 2000 // parsing listing: show wait window after 2 seconds
#define WAITWND_CONTOOPER 2000  // handing the active "control connection" to the worker of an operation: show wait window after 2 seconds
#define WAITWND_CLWORKCON 250   // close control connections of workers in operations: show wait window after a quarter of a second (originally after 2 seconds - users tried to click Add and other buttons, which is impossible, but it was not visualized - with the wait window open it is clear why it does not work)
#define WAITWND_CLOPERDLGS 2000 // close all operation dialogs (when the plugin is unloaded): show wait window after 2 seconds

#define CRTLCON_BYTESTOWRITEONSOCKETPREALLOC 512 // how many bytes to preallocate for writing (so the next write does not allocate unnecessarily, for example due to a 1-byte overflow)
#define CRTLCON_BYTESTOREADONSOCKET 1024         // the minimum number of bytes to read from the socket at once (also allocate the buffer for the read data)
#define CRTLCON_BYTESTOREADONSOCKETPREALLOC 512  // how many bytes to preallocate for reading (so the next read does not immediately allocate again)

// response codes for the 1st digit of the FTP reply code
#define FTP_D1_MAYBESUCCESS 1   // possible success (the client must wait for the next reply)
#define FTP_D1_SUCCESS 2        // success (the client may send the next command)
#define FTP_D1_PARTIALSUCCESS 3 // partial success (the client must send the next command in the sequence)
#define FTP_D1_TRANSIENTERROR 4 // temporary error (failure, but the same command might succeed next time)
#define FTP_D1_ERROR 5          // error (there is no point repeating the command; it needs to be changed)

// response codes for the 2nd digit of the FTP reply code
#define FTP_D2_SYNTAX 0         // in case of errors it is syntax; otherwise commands not classified elsewhere functionally
#define FTP_D2_INFORMATION 1    // information (status or help, etc.)
#define FTP_D2_CONNECTION 2     // related to the connection
#define FTP_D2_AUTHENTICATION 3 // related to login/authentication/accounting
#define FTP_D2_UNKNOWN 4        // not specified
#define FTP_D2_FILESYSTEM 5     // file system action

#define CTRLCON_KEEPALIVE_TIMERID 1 // CControlConnectionSocket: ID of the timer for keep-alive

#define CTRLCON_KAPOSTSETUPNEXT 2 // CControlConnectionSocket: ID of the posted message for executing SetupNextKeepAliveTimer()
#define CTRLCON_LISTENFORCON 3    // CControlConnectionSocket: ID of the posted message about opening the port for "listen" (on the proxy server)
#define CTRLCON_KALISTENFORCON 4  // CControlConnectionSocket: keep-alive: ID of the posted message about opening the port for "listen" (on the proxy server)

//
// ****************************************************************************
// CDynString
//
// helper object for a dynamically allocated string

struct CDynString
{
    char* Buffer;
    int Length;
    int Allocated;

    CDynString()
    {
        Buffer = NULL;
        Length = 0;
        Allocated = 0;
    }

    ~CDynString()
    {
        if (Buffer != NULL)
            free(Buffer);
    }

    void Clear()
    {
        Length = 0;
        if (Buffer != NULL)
            Buffer[0] = 0;
    }

    BOOL Append(const char* str, int len); // returns TRUE on success; if 'len' is -1, "len=strlen(str)" is used

    // skips at least 'len' characters, skipping entire lines; adds the counts of skipped characters/lines
    // to 'skippedChars'/'skippedLines'
    void SkipBeginning(DWORD len, int* skippedChars, int* skippedLines);

    const char* GetString() const { return Buffer; }
};

//
// ****************************************************************************
// CLogs
//
// log management for all connections (control connections in panels and workers in operations)

class CControlConnectionSocket;

class CLogData
{
protected:
    static int NextLogUID;          // global counter for log objects
    static int OldestDisconnectNum; // disconnect number of the oldest server log that disconnected
    static int NextDisconnectNum;   // disconnect number for the next server log that will disconnect

    // log identification:
    int UID;             // unique log number (value -1 is reserved for "invalid UID")
    char* Host;          // server address
    unsigned short Port; // port used on the server
    char* User;          // user name

    BOOL CtrlConOrWorker; // TRUE/FALSE = logging the "control connection" from the panel / from a worker
    BOOL WorkerIsAlive;   // TRUE/FALSE = the worker exists / no longer exists
    // the "control connection" we are logging (NULL == FS no longer exists);
    // WARNING: we must not access the "control connection" object - nesting critical sections is forbidden
    CControlConnectionSocket* CtrlCon;
    BOOL Connected;    // TRUE/FALSE == active/inactive "control connection" (panel and worker)
    int DisconnectNum; // if (CtrlCon==NULL && !WorkerIsAlive), holds the number describing how old the dead log is (so that we always delete starting from the longest dead log)

    CDynString Text;  // the actual log text
    int SkippedChars; // number of skipped characters since the last output to the edit window in Logs
    int SkippedLines; // number of skipped lines since the last output to the edit window in Logs

protected:
    CLogData(const char* host, unsigned short port, const char* user,
             CControlConnectionSocket* ctrlCon, BOOL connected, BOOL isWorker);
    ~CLogData();

    BOOL IsGood() { return Host != NULL && User != NULL; }

    // change user; returns TRUE on success (on failure leaves an empty string in User)
    BOOL ChangeUser(const char* user);

    friend class CLogs;
    friend TIndirectArray<CLogData>;
};

class CLogsDlg;

class CLogs
{
protected:
    CRITICAL_SECTION LogCritSect; // critical section of the object (WARNING: entering other sections inside this one is forbidden)

    TIndirectArray<CLogData> Data; // array of all logs
    int LastUID;                   // single-entry lookup cache - if LastUID==UID, it tries to use LastIndex
    int LastIndex;                 // helper value for the lookup cache (searching logs by UID in Data)

    CLogsDlg* LogsDlg; // holds a pointer to the open (NULL = closed) Logs dialog
    HANDLE LogsThread; // handle of the thread in which the last opened Logs dialog ran/is running

public:
    CLogs() : Data(5, 5)
    {
        HANDLES(InitializeCriticalSection(&LogCritSect));
        LastUID = -1;
        LastIndex = -1;
        LogsDlg = NULL;
        LogsThread = NULL;
    }

    ~CLogs() { HANDLES(DeleteCriticalSection(&LogCritSect)); }

    // creates a new log; returns TRUE on success and writes the new log's UID into 'uid' (must not be NULL);
    // WARNING: does not take Config.EnableLogging into account
    BOOL CreateLog(int* uid, const char* host, unsigned short port, const char* user,
                   CControlConnectionSocket* ctrlCon, BOOL connected, BOOL isWorker);

    // sets CLogData::Connected to 'isConnected' in the log with UID=='uid'
    void SetIsConnected(int uid, BOOL isConnected);

    // marks the log with UID=='uid' as a closed-connection log; used to announce the connection
    // closure on the FTP server; returns TRUE on success (UID found)
    BOOL ClosingConnection(int uid);

    // changes the user name (this can happen during connection); returns TRUE on success
    BOOL ChangeUser(int uid, const char* user);

    // adds the text 'str' (length 'len') to the log with UID=='uid'; if 'uid' is -1 ("invalid UID")
    // nothing is logged; if 'len' is -1, it uses "len=strlen(str)"; if 'addTimeToLog' is TRUE, the
    // current time is placed before the message; returns TRUE when the text is added to the log or
    // when 'uid' is -1
    BOOL LogMessage(int uid, const char* str, int len, BOOL addTimeToLog = FALSE);

    // returns TRUE if the log with UID=='uid' is in the log array (therefore the log can be viewed
    // in the Logs dialog)
    BOOL HasLogWithUID(int uid);

    // called after logging parameters change in the configuration
    void ConfigChanged();

    // adds logs to the combo box; returns the focus in 'focusIndex' (must not be NULL) based on
    // 'prevItemUID' (the UID of the item we want to select, it may also be -1); if 'prevItemUID'
    // is not found, it returns -1 in 'focusIndex'; in 'empty' (must not be NULL) it returns TRUE if
    // no logs exist
    void AddLogsToCombo(HWND combo, int prevItemUID, int* focusIndex, BOOL* empty);

    // sets the text of the log with UID 'logUID' into the edit control 'edit'; if 'update' is TRUE,
    // the text is being updated
    void SetLogToEdit(HWND edit, int logUID, BOOL update);

    // sets the value of 'LogsDlg' (inside the critical section)
    void SetLogsDlg(CLogsDlg* logsDlg);

    // returns TRUE if there is a chance the Logs dialog will be activated (in its own thread),
    // returns FALSE on failure; if 'showLogUID' is not -1, it specifies the UID of the log to
    // activate
    BOOL ActivateLogsDlg(int showLogUID);

    // if the Logs dialog is open, activates the log with UID 'showLogUID' in it
    void ActivateLog(int showLogUID);

    // if the Logs dialog is open, closes it and waits one second for its thread to finish (if it
    // does not finish in time, nothing happens)
    void CloseLogsDlg();

    // saves the current position of the Logs dialog into the configuration
    void SaveLogsDlgPos();

    // refreshes the list of logs in the Logs dialog (if it exists)
    void RefreshListOfLogsInLogsDlg();

    // saves the log to a file (letting the user choose); 'itemName' is the log name; 'uid' is the
    // log UID
    void SaveLog(HWND parent, const char* itemName, int uid);

    // copies the log to the clipboard; 'itemName' is the log name; 'uid' is the log UID
    void CopyLog(HWND parent, const char* itemName, int uid);

    // clears the log text; 'itemName' is the log name; 'uid' is the log UID
    void ClearLog(HWND parent, const char* itemName, int uid);

    // removes the log; 'itemName' is the log name; 'uid' is the log UID
    void RemoveLog(HWND parent, const char* itemName, int uid);

    // saves all logs to a file (letting the user choose)
    void SaveAllLogs(HWND parent);

    // copies all logs to the clipboard
    void CopyAllLogs(HWND parent);

    // removes all logs
    void RemoveAllLogs(HWND parent);

protected:
    // discards any redundant old logs;
    // WARNING: must be called from the critical section
    void LimitClosedConLogs();

    // returns in 'index' the index of the log with UID 'uid'; returns FALSE if the log is not
    // found (otherwise TRUE)
    // WARNING: must be called from the critical section
    BOOL GetLogIndex(int uid, int* index);
};

extern CLogs Logs; // logs of all connections to FTP servers

//
// ****************************************************************************
// CListingCache
//
// listing cache on FTP servers - used when changing or listing a path without accessing the server

struct CListingCacheItem
{
public:
    // connection parameters for the server:
    char* Host;          // host address (must not be NULL)
    unsigned short Port; // port on which the FTP server runs
    char* User;          // user name, NULL == anonymous
    int UserLength;      // optimization-only variable: length of 'User' if it contains "forbidden" characters (if 'User==NULL',
                         // this is zero)

    char* Path;                  // cached path (local on the server)
    CFTPServerPathType PathType; // type of the cached path

    char* ListCmd; // command that retrieved the listing (a different command may produce a different listing)
    BOOL IsFTPS;   // TRUE = FTPS, FALSE = FTP

    char* CachedListing;          // listing of the cached path
    int CachedListingLen;         // length of the listing of the cached path
    CFTPDate CachedListingDate;   // date when the listing was created (needed to evaluate "year_or_time" correctly)
    DWORD CachedListingStartTime; // IncListingCounter() value at the moment the "LIST" command was sent to obtain this listing

    CListingCacheItem(const char* host, unsigned short port, const char* user, const char* path,
                      const char* listCmd, BOOL isFTPS, const char* cachedListing, int cachedListingLen,
                      const CFTPDate& cachedListingDate, DWORD cachedListingStartTime,
                      CFTPServerPathType pathType);
    ~CListingCacheItem();

    BOOL IsGood() { return Host != NULL; }
};

class CListingCache
{
protected:
    CRITICAL_SECTION CacheCritSect;          // critical section of the object
    TIndirectArray<CListingCacheItem> Cache; // array of cached path listings
    CQuadWord TotalCacheSize;                // total size of items in the cache (will later be stored on disk, hence a quad word)

public:
    CListingCache();
    ~CListingCache();

    // returns TRUE if a usable listing of the path 'path' (of type 'pathType') is available on the
    // server 'host', where user 'user' is connected on port 'port'; the listing is returned in the
    // allocated string 'cachedListing' (must not be NULL; returning NULL means an allocation error),
    // the string length is returned in 'cachedListingLen' (must not be NULL); the caller is
    // responsible for deallocation; the date when the listing was captured is returned in
    // 'cachedListingDate' (must not be NULL); 'path' returns the exact text of the cached path (as
    // provided by the server when it was inserted into the cache); 'path' is a buffer of size
    // 'pathBufSize' bytes
    // can be called from any thread
    BOOL GetPathListing(const char* host, unsigned short port, const char* user,
                        CFTPServerPathType pathType, char* path, int pathBufSize,
                        const char* listCmd, BOOL isFTPS, char** cachedListing,
                        int* cachedListingLen, CFTPDate* cachedListingDate,
                        DWORD* cachedListingStartTime);

    // adds or refreshes (overwrites) the listing of the path 'path' (type 'pathType') on the server
    // 'host', where user 'user' is connected on port 'port'; the listing is in the string
    // 'cachedListing' (must not be NULL), the string length is in 'cachedListingLen'; the date when
    // the listing was captured is in 'cachedListingDate' (must not be NULL);
    // can be called from any thread
    void AddOrUpdatePathListing(const char* host, unsigned short port, const char* user,
                                CFTPServerPathType pathType, const char* path,
                                const char* listCmd, BOOL isFTPS,
                                const char* cachedListing, int cachedListingLen,
                                const CFTPDate* cachedListingDate,
                                DWORD cachedListingStartTime);

    // reports to the cache that the user issued a hard refresh on the path 'path' (type 'pathType')
    // on the server 'host', where user 'user' is connected on port 'port'; this expresses distrust in
    // the cache's freshness, so we remove the path including its subpaths from the cache; if
    // 'ignorePath' is TRUE, listings of all paths from server 'host', where user 'user' is connected
    // on port 'port', are removed from the cache
    // can be called from any thread
    void RefreshOnPath(const char* host, unsigned short port, const char* user,
                       CFTPServerPathType pathType, const char* path, BOOL ignorePath = FALSE);

    // reports to the cache that a change occurred on the path 'userPart' (FS user-part path
    // format); if 'includingSubdirs' is TRUE, changes in subdirectories of 'userPart' are included;
    // the changed paths are removed from the cache (so they will be loaded from the server next time)
    void AcceptChangeOnPathNotification(const char* userPart, BOOL includingSubdirs);

protected:
    // searches for an item in the cache; if found, returns TRUE and its index in 'index'
    // (must not be NULL); returns FALSE if the item does not exist in the cache;
    // WARNING: call only from the CacheCritSect critical section
    BOOL Find(const char* host, unsigned short port, const char* user,
              CFTPServerPathType pathType, const char* path, const char* listCmd,
              BOOL isFTPS, int* index);
};

//
// ****************************************************************************
// CControlConnectionSocket
//
// socket object that serves as the FTP server "control connection"
// use ::DeleteSocket! to deallocate it

// event codes for CControlConnectionSocket::WaitForEventOrESC
enum CControlConnectionSocketEvent
{
    ccsevESC,               // return value of WaitForEventOrESC only
    ccsevTimeout,           // return value of WaitForEventOrESC only
    ccsevWriteDone,         // buffer write finished (see the Write method)
    ccsevNewBytesRead,      // another block of data read into the socket buffer (see ReadFTPReply)
    ccsevClosed,            // the socket was closed
    ccsevIPReceived,        // IP received
    ccsevConnected,         // connection to the server opened
    ccsevUserIfaceFinished, // user interface reports completion ("data connection" closed or "keep alive" command finished)
    ccsevListenForCon,      // successful or failed opening of the "listen" port on the proxy server
};

struct CControlConnectionSocketEventData
{
    CControlConnectionSocketEvent Event; // event number (see ccsevXXX)
    DWORD Data1;
    DWORD Data2;

    // description of Data1 and Data2 usage for individual events:
    //
    // event ccsevWriteDone:
    //   Data1 - if NO_ERROR, the write completed successfully; otherwise contains the Windows error code
    //
    // event ccsevNewBytesRead:
    //   Data1 - if NO_ERROR, new bytes were read into the 'ReadBytes' buffer; otherwise contains the
    //           Windows error code
    //
    // event ccsevClosed:
    //   Data1 - if NO_ERROR, the socket closed gracefully; otherwise it closed abortively
    //
    // event ccsevIPReceived:
    //   Data1 - contains the 'ip' parameter (from ReceiveHostByAddress)
    //   Data2 - contains the 'error' parameter (from ReceiveHostByAddress)
    //
    // event ccsevConnected:
    //   Data1 - error code from FD_CONNECT (NO_ERROR = connected successfully)
    //
    // event ccsevUserIfaceFinished:
    //   neither Data1 nor Data2 is used
    //
    // event ccsevListenForCon:
    //   Data1 - UID of the data connection that is reporting back
};

// codes for the transfer mode in an open "control connection" (the server remembers the last setting)
enum CCurrentTransferMode
{
    ctrmUnknown, // unknown - we have not set it yet or the user might have changed it
    ctrmBinary,  // binary mode (image mode)
    ctrmASCII    // ASCII mode
};

// helper interface for CControlConnectionSocket::SendFTPCommand() - handles various user interfaces
// when sending FTP commands (e.g. PWD and LIST differ significantly)
class CSendCmdUserIfaceAbstract
{
public:
    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText) = 0;
    virtual void BeforeAborting() = 0;
    virtual void AfterWrite(BOOL aborting, DWORD showTime) = 0;
    virtual BOOL GetWindowClosePressed() = 0;
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort) = 0;
    virtual void SendingFinished() = 0;
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize) = 0;
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize) = 0; // FTP reply code: 1xx
    virtual void CancelDataCon() = 0;

    // collection of methods for waiting for the user interface ("data connection") to close when the
    // command finishes successfully on the server
    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout) = 0;       // if it returns FALSE, the
                                                                              // event obtained via
                                                                              // GetFinishedEvent() will be
                                                                              // waited on
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout) = 0; // called after the first
                                                                              // CanFinishSending() that
                                                                              // returns FALSE
    virtual void HandleDataConTimeout(DWORD* start) = 0;                      // called only if
                                                                              // BeforeWaitingForFinish
                                                                              // returns TRUE in
                                                                              // 'useTimeout'
    virtual HANDLE GetFinishedEvent() = 0;                                    // once it is signaled,
                                                                              // CanFinishSending() is
                                                                              // tested again
    virtual void HandleESCWhenWaitingForFinish(HWND parent) = 0;              // user pressed ESC while
                                                                              // waiting (after this method
                                                                              // CanFinishSending() is tested
                                                                              // again)
};

class CSendCmdUserIfaceForListAndDownload : public CSendCmdUserIfaceAbstract
{
protected:
    BOOL ForDownload;     // object usage: TRUE = download, FALSE = list
    BOOL DatConCancelled; // TRUE = the data connection was cancelled (either it never opened (accept)
                          // or it was closed after an error from the server was received)

    CListWaitWindow WaitWnd;

    CDataConnectionSocket* DataConnection;
    BOOL AlreadyAborted;
    int LogUID;

public:
    CSendCmdUserIfaceForListAndDownload(BOOL forDownload, HWND parent,
                                        CDataConnectionSocket* dataConnection,
                                        int logUID)
        : WaitWnd(parent, dataConnection, &AlreadyAborted)
    {
        ForDownload = forDownload;
        DatConCancelled = FALSE;
        DataConnection = dataConnection;
        AlreadyAborted = FALSE;
        LogUID = logUID;
    }

    BOOL WasAborted() { return AlreadyAborted; }
    BOOL HadError();
    void GetError(DWORD* netErr, BOOL* lowMem, DWORD* tgtFileErr, BOOL* noDataTrTimeout,
                  int* sslErrorOccured, BOOL* decomprErrorOccured);
    BOOL GetDatConCancelled() { return DatConCancelled; }

    void InitWnd(const char* fileName, const char* host, const char* path,
                 CFTPServerPathType pathType);
    virtual void Init(HWND parent, const char* logCmd, const char* waitWndText) {}
    virtual void BeforeAborting() { WaitWnd.SetText(LoadStr(IDS_ABORTINGCOMMAND)); }
    virtual void AfterWrite(BOOL aborting, DWORD showTime);
    virtual BOOL GetWindowClosePressed() { return WaitWnd.GetWindowClosePressed(); }
    virtual BOOL HandleESC(HWND parent, BOOL isSend, BOOL allowCmdAbort);
    virtual void SendingFinished();
    virtual BOOL IsTimeout(DWORD* start, DWORD serverTimeout, int* errorTextID, char* errBuf, int errBufSize);
    virtual void MaybeSuccessReplyReceived(const char* reply, int replySize);
    virtual void CancelDataCon();

    virtual BOOL CanFinishSending(int replyCode, BOOL* useTimeout);
    virtual void BeforeWaitingForFinish(int replyCode, BOOL* useTimeout);
    virtual void HandleDataConTimeout(DWORD* start);
    virtual HANDLE GetFinishedEvent();
    virtual void HandleESCWhenWaitingForFinish(HWND parent);
};

// states of keep-alive processing in the "control connection"
enum CKeepAliveMode
{
    kamNone,                      // keep-alive not handled (no connection established, disabled, etc.)
    kamWaiting,                   // waiting for the keep-alive period to elapse (before sending keep-alive commands)
    kamProcessing,                // keep-alive commands are currently being executed
    kamWaitingForEndOfProcessing, // keep-alive commands are running and regular commands already wait for them to finish
    kamForbidden,                 // a regular command is currently in progress, keep-alive is forbidden until it finishes
};

enum CKeepAliveDataConState
{
    kadcsNone,                // not set (the "data connection" is not being started yet)
    kadcsWaitForPassiveReply, // passive: waiting for the PASV reply (IP+port where the "data connection" socket must open)
    kadcsWaitForListen,       // active: waiting for the listen port to open (locally or on the proxy server) ("listen" mode of the
                              // "data connection")
    kadcsWaitForSetPortReply, // active: waiting for the reply to the PORT command ("listen" mode of the "data connection")
    kadcsWaitForListStart,    // active + passive: the "list" command was sent, waiting for the server to connect ("data connection"
                              // connect)
    kadcsDone,                // active + passive: the server has already reported the end of the listing
};

class CWaitWindow;
class CKeepAliveDataConSocket;
class CFTPOperation;
class CFTPWorker;

class CControlConnectionSocket : public CSocket
{
private:
    // event-related data (access them only via AddEvent and GetEvent):
    // events are generated by the "receive" part ("receive" methods running in the "sockets" thread),
    // events are consumed by the control part (waiting for ESC/timeout or finishing work with the
    // socket in the main thread)
    // NOTE: overwritable events are also supported; they remain in the queue only until another event
    //       is generated (to prevent unnecessary accumulation)
    //
    // critical section for handling events
    // WARNING: inside this section, do not nest into CSocket::SocketCritSect or
    // SocketsThread->CritSect (do not call SocketsThread methods)
    CRITICAL_SECTION EventCritSect;
    TIndirectArray<CControlConnectionSocketEventData> Events; // event queue
    int EventsUsedCount;                                      // number of array elements actually used in Events (elements are reused)
    HANDLE NewEvent;                                          // system event: signaled if Events contains an event
    BOOL RewritableEvent;                                     // TRUE if the new event may overwrite the last event in the queue

protected:
    // the critical section for accessing object data is CSocket::SocketCritSect
    // WARNING: inside this section, do not nest into SocketsThread->CritSect (do not call SocketsThread methods)

    // connection parameters for the FTP server
    CFTPProxyServer* ProxyServer; // NULL = "not used (direct connection)"
    char Host[HOST_MAX_SIZE];
    unsigned short Port;
    char User[USER_MAX_SIZE];
    char Password[PASSWORD_MAX_SIZE];
    char Account[ACCOUNT_MAX_SIZE];
    int UseListingsCache;
    char* InitFTPCommands;
    BOOL UsePassiveMode;
    char* ListCommand;
    BOOL UseLIST_aCommand; // TRUE = ignore ListCommand, use "LIST -a" (list hidden files (UNIX))

    DWORD ServerIP;                           // server IP address (==INADDR_NONE until the IP is known)
    BOOL CanSendOOBData;                      // FALSE if the server does not support OOB data (used when sending abort commands)
    char* ServerSystem;                       // server system (reply to SYST command) - may also be NULL
    char* ServerFirstReply;                   // first server reply (often contains the FTP server version) - may also be NULL
    BOOL HaveWorkingPath;                     // TRUE if WorkingPath is valid
    char WorkingPath[FTP_MAX_PATH];           // current working directory on the FTP server
    CCurrentTransferMode CurrentTransferMode; // current transfer mode on the FTP server (memory of the last FTP "TYPE" command)

    BOOL EventConnectSent; // TRUE only if the ccsevConnected event was already sent (handles FD_READ arriving before FD_CONNECT)

    char* BytesToWrite;            // buffer for bytes that were not written in Write (written after FD_WRITE is received)
    int BytesToWriteCount;         // number of valid bytes in the 'BytesToWrite' buffer
    int BytesToWriteOffset;        // number of bytes already sent from the 'BytesToWrite' buffer
    int BytesToWriteAllocatedSize; // allocated size of the 'BytesToWrite' buffer

    char* ReadBytes;            // buffer for bytes read from the socket (read after FD_READ is received)
    int ReadBytesCount;         // number of valid bytes in the 'ReadBytes' buffer
    int ReadBytesOffset;        // number of bytes already processed (skipped) in the 'ReadBytes' buffer
    int ReadBytesAllocatedSize; // allocated size of the 'ReadBytes' buffer

    DWORD StartTime; // start time of the tracked operation (used to compute timeouts and wait windows)

    int LogUID; // log UID for this connection (-1 until the log is created)

    // if not NULL, this is the error message explaining the disconnection reason in a detached FS
    // (displayed in a message box when the FS is connected into a panel)
    char* ConnectionLostMsg;

    HWND OurWelcomeMsgDlg; // handle of the welcome-message window (no synchronization needed, accessed only from the main
                           // thread) - NULL = not opened yet; note: the window may already be closed

    BOOL KeepAliveEnabled;            // TRUE if this "control connection" should prevent disconnects by sending keep-alive commands
    int KeepAliveSendEvery;           // interval for sending keep-alive commands (period) (copy for use across threads)
    int KeepAliveStopAfter;           // time after which keep-alive commands stop (copy for use across threads)
    int KeepAliveCommand;             // keep-alive command (0-NOOP, 1-PWD, 2-NLST, 3-LIST) (copy for use across threads)
    DWORD KeepAliveStart;             // GetTickCount() of the last executed command (excluding keep-alive) in the "control connection"
    CKeepAliveMode KeepAliveMode;     // current state of keep-alive processing in the "control connection"
    BOOL KeepAliveCmdAllBytesWritten; // FALSE = the last keep-alive command has not been fully sent yet (must wait for FD_WRITE)
    HANDLE KeepAliveFinishedEvent;    // signaled after the keep-alive command finishes (used by the main thread when waiting to
                                      // start a normal command)
    // "data connection" for the keep-alive command (NLST+LIST) - it only discards data (object destruction happens inside
    // SocketsThread->CritSect, so the object cannot disappear "under the feet" of "receive" methods; before destroying the
    // object, KeepAliveDataCon is set to NULL in this object's section (the object can only be "retrieved" for the first
    // destruction))
    CKeepAliveDataConSocket* KeepAliveDataCon;
    CKeepAliveDataConState KeepAliveDataConState; // state of the "data connection" for the keep-alive command (NLST+LIST) - for
                                                  // active and passive connections

    int EncryptControlConnection;
    int EncryptDataConnection;

    int CompressData;

public:
    CControlConnectionSocket();
    virtual ~CControlConnectionSocket();

    // ******************************************************************************************
    // methods for working with the control socket
    // ******************************************************************************************

    BOOL IsGood() { return NewEvent != NULL && KeepAliveFinishedEvent != NULL; }

    // sets the connection parameters for the FTP server; strings must not be NULL (except for
    // 'initFTPCommands' and 'listCommand' - they may be NULL)
    // can be called from any thread
    void SetConnectionParameters(const char* host, unsigned short port, const char* user,
                                 const char* password, BOOL useListingsCache,
                                 const char* initFTPCommands, BOOL usePassiveMode,
                                 const char* listCommand, BOOL keepAliveEnabled,
                                 int keepAliveSendEvery, int keepAliveStopAfter,
                                 int keepAliveCommand, int proxyServerUID,
                                 int encryptControlConnection, int encryptDataConnection,
                                 int compressData);

    // methods for tracking the duration of an operation with the socket:
    // WARNING: not synchronized - use from one thread or apply other synchronization
    //
    // sets the start time of the operation (time step approx. 10 ms - uses GetTickCount)
    void SetStartTime(BOOL setOldTime = FALSE) { StartTime = GetTickCount() - (setOldTime ? 60000 : 0); }
    // returns the number of ms since the operation started (maximum duration is roughly 50 days)
    DWORD GetTimeFromStart();
    // subtracts the time since the operation started from 'showTime', returns at least 0 ms (no negative numbers)
    DWORD GetWaitTime(DWORD showTime);

    int GetEncryptControlConnection() { return EncryptControlConnection; }
    int GetEncryptDataConnection() { return EncryptDataConnection; }
    int GetCompressData() { return CompressData; }

    // opens the "control connection" to the FTP server (configured by the preceding
    // SetConnectionParameters call); expects SetStartTime() to be set - shows a wait window using
    // GetWaitTime(); 'parent' is the thread's foreground window (after ESC is pressed it is used to
    // detect whether ESC was pressed in this window or, for example, in another application; in the
    // main thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin);
    // 'parent' is also the parent of any error message boxes; 'reconnect' is TRUE when reconnecting
    // a closed "control connection"; if 'workDir' is not NULL, the current working directory on the
    // server is determined (right after the connection) and stored in 'workDir' (buffer of size
    // 'workDirBufSize'); if 'totalAttemptNum' is not NULL, it is an in/out variable containing the
    // total number of connection attempts (initialize to 1 before the first call); if 'retryMsg' is
    // not NULL, the message 'retryMsg' (text for the retry wait window) is displayed before the next
    // connection attempt (provided not all attempts are exhausted) - this allows simulating a state
    // where the disconnect occurred inside this method; if 'reconnectErrResID' is not -1, it is used
    // as the text for the reconnect wait window (if it is -1, IDS_SENDCOMMANDERROR is used); if
    // 'useFastReconnect' is TRUE, reconnect is performed without waiting;
    // returns TRUE if the connection succeeded; the user name may change during connection (it does
    // not depend on the method's success) - the current name is returned in 'user' (maximum
    // 'userSize' bytes);
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL StartControlConnection(HWND parent, char* user, int userSize, BOOL reconnect,
                                char* workDir, int workDirBufSize, int* totalAttemptNum,
                                const char* retryMsg, BOOL canShowWelcomeDlg,
                                int reconnectErrResID, BOOL useFastReconnect);

    // changes the working directory in the "control connection"; expects SetStartTime() to be set -
    // if the connection does not need to be restored, shows a wait window using GetWaitTime();
    // 'notInPanel' is TRUE for a detached FS (not present in a panel); if 'notInPanel' is FALSE, the
    // connection is in a panel - if 'leftPanel' is TRUE it is the left panel, otherwise the right
    // panel; 'parent' is the thread's foreground window (after ESC is pressed it is used to detect
    // whether ESC was pressed in this window and not, for example, in another application; in the
    // main thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin);
    // 'parent' is also the parent of any error message boxes; 'path' is the new working directory;
    // if 'parsedPath' is TRUE, it is first necessary to determine whether the leading slash should
    // be trimmed from 'path' (e.g. for OpenVMS/MVS paths) - used when 'path' is obtained by parsing
    // the user-part of an FS path; 'path' returns (up to 'pathBufSize' bytes) the new working
    // directory; 'userBuf' + 'userBufSize' is an in/out buffer for the user name on the FTP server
    // (it may change during reconnect); if 'forceRefresh' is TRUE, no cached data may be used when
    // changing the path (the path must be changed directly on the server); 'mode' is the path change
    // mode (see CPluginFSInterfaceAbstract::ChangePath); if 'cutDirectory' is TRUE, 'path' must be
    // shortened before use (by one directory); if the path is shortened because it leads to a file
    // (a mere suspicion that it might be a file path is enough - after listing the path it checks
    // whether the file exists, otherwise an error is shown) and 'cutFileName' is not NULL (possible
    // only in 'mode' 3 and with 'cutDirectory' FALSE), the buffer 'cutFileName' (size MAX_PATH
    // characters) receives that file name (without path), otherwise 'cutFileName' receives an empty
    // string; if 'pathWasCut' is not NULL, it returns TRUE if the path was shortened; Salamander uses
    // 'cutFileName' and 'pathWasCut' in the Change Directory command (Shift+F7) when a file name is
    // entered - the file gains focus; 'rescuePath' contains the last accessible and listable path on
    // the server that should be used when everything else fails (better than disconnecting) - it is
    // an in/out string (maximum size FTP_MAX_PATH characters);
    // if 'showChangeInLog' is TRUE, the log should contain the message "Changing path to...";
    // if the listing is cached (no need to retrieve it from the server), it is returned in
    // 'cachedListing' (must not be NULL) as an allocated string, the string length is returned in
    // 'cachedListingLen' (must not be NULL), the caller is responsible for deallocation; the date the
    // listing was captured is returned in 'cachedListingDate' (must not be NULL);
    // 'totalAttemptNum' + 'skipFirstReconnectIfNeeded' are parameters for SendChangeWorkingPath();
    // returns FALSE if the path change failed (says nothing about the "control connection" - it may
    // remain open or closed)
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL ChangeWorkingPath(BOOL notInPanel, BOOL leftPanel, HWND parent, char* path,
                           int pathBufSize, char* userBuf, int userBufSize, BOOL parsedPath,
                           BOOL forceRefresh, int mode, BOOL cutDirectory,
                           char* cutFileName, BOOL* pathWasCut, char* rescuePath,
                           BOOL showChangeInLog, char** cachedListing, int* cachedListingLen,
                           CFTPDate* cachedListingDate, DWORD* cachedListingStartTime,
                           int* totalAttemptNum, BOOL skipFirstReconnectIfNeeded);

    // lists the working directory in the "control connection" (does not use any cache); expects
    // SetStartTime() to be set (shows a wait window using GetWaitTime()), the working directory set to
    // 'path' and an open connection (if it is closed, offers reconnect); 'parent' is the thread's
    // foreground window (after ESC is pressed it is used to detect whether ESC was pressed in this
    // window and not, for example, in another application; in the main thread this is
    // SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent' is also the
    // parent of any error message boxes; 'path' is the working directory; 'userBuf' + 'userBufSize'
    // is an in/out buffer for the user name on the FTP server (it may change during reconnect);
    // returns TRUE if at least part of the listing was obtained (empty string in the worst case); the
    // listing is returned in the allocated string 'allocatedListing' (must not be NULL; returning NULL
    // means an allocation error), the string length is returned in 'allocatedListingLen' (must not be
    // NULL), the caller is responsible for deallocation; 'listingDate' (must not be NULL) returns the
    // date when the listing was captured; 'pathListingIsIncomplete' (must not be NULL) returns TRUE if
    // the listing is incomplete (was interrupted) or FALSE if it is complete; 'pathListingIsBroken'
    // (must not be NULL) returns TRUE if the listing command returned an error (3xx, 4xx, or 5xx);
    // if 'forceRefresh' is TRUE, no cache may be used; everything must be executed directly on the
    // server; 'totalAttemptNum' are parameters for StartControlConnection(); returns FALSE if the
    // server refuses to list the path (says nothing about the "control connection" - it may remain
    // open or closed); returns FALSE and sets 'fatalError' (must not be NULL) to TRUE if a fatal error
    // occurred (the server is not responding like an FTP server, etc.); 'dontClearCache' is TRUE if
    // ListingCache.RefreshOnPath() should not be called to clear the cache for the current path (it is
    // cleared elsewhere);
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL ListWorkingPath(HWND parent, const char* path, char* userBuf, int userBufSize,
                         char** allocatedListing, int* allocatedListingLen,
                         CFTPDate* listingDate, BOOL* pathListingIsIncomplete,
                         BOOL* pathListingIsBroken, BOOL* pathListingMayBeOutdated,
                         DWORD* listingStartTime, BOOL forceRefresh, int* totalAttemptNum,
                         BOOL* fatalError, BOOL dontClearCache);

    // obtains the current working directory in the "control connection"; expects SetStartTime() to be
    // set - shows a wait window using GetWaitTime(); 'parent' is the thread's foreground window (after
    // ESC is pressed it is used to detect whether ESC was pressed in this window and not, for example,
    // in another application; in the main thread this is SalamanderGeneral->GetMsgBoxParent() or a
    // dialog opened by the plugin); 'parent' is also the parent of any error message boxes; 'path'
    // returns (up to 'pathBufSize' bytes) the current working directory, or an empty string if getting
    // the working directory fails (e.g. "not defined yet"); if 'forceRefresh' is TRUE, no cached data
    // may be used when retrieving the path (it must be fetched directly from the server);
    // returns FALSE if getting the path failed (e.g. invalid reply format from the server), the
    // connection was interrupted (on timeout it automatically closes the connection hard - sending
    // "QUIT" makes no sense); if 'canRetry' is not NULL, the error text may be returned in 'retryMsg'
    // (buffer of size 'retryMsgBufSize') - 'canRetry' returns TRUE; otherwise the error is shown in a
    // message box ('canRetry' is either NULL or FALSE is returned there);
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL GetCurrentWorkingPath(HWND parent, char* path, int pathBufSize, BOOL forceRefresh,
                               BOOL* canRetry, char* retryMsg, int retryMsgBufSize);

    // if needed, sets the transfer mode (ASCII/BINARY(IMAGE) - TRUE/FALSE in 'asciiMode') in the
    // "control connection"; expects SetStartTime() to be set - shows a wait window using
    // GetWaitTime(); 'parent' is the thread's foreground window (after ESC is pressed it is used to
    // detect whether ESC was pressed in this window and not, for example, in another application; in
    // the main thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin);
    // 'parent' is also the parent of any error message boxes; if 'success' is not NULL, it returns
    // TRUE when the server reports success; if the server reports failure, the server reply text is in
    // the buffer 'ftpReplyBuf' (maximum size 'ftpReplyBufSize'), null-terminated - if it is longer
    // than the buffer it is simply truncated; if 'forceRefresh' is TRUE, no cached data may be used
    // when setting the transfer mode (the mode is set even if theoretically unnecessary because it is
    // already set); returns FALSE if setting the transfer mode failed, the connection was interrupted
    // (on timeout it automatically closes the connection hard - sending "QUIT" makes no sense); if
    // 'canRetry' is not NULL, the error text may be returned in 'retryMsg' (buffer of size
    // 'retryMsgBufSize') - 'canRetry' returns TRUE; otherwise the error is displayed in a message box
    // ('canRetry' is either NULL or FALSE is returned there);
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL SetCurrentTransferMode(HWND parent, BOOL asciiMode, BOOL* success, char* ftpReplyBuf,
                                int ftpReplyBufSize, BOOL forceRefresh, BOOL* canRetry,
                                char* retryMsg, int retryMsgBufSize);

    // clears the cache for the current working directory: synchronized HaveWorkingPath=FALSE,
    void ResetWorkingPathCache();

    // invalidates the remembered transfer mode: synchronized CurrentTransferMode=ctrmUnknown
    void ResetCurrentTransferModeCache();

    // sends a command to change the working directory in the "control connection" to 'path'; expects
    // SetStartTime() to be set - if reconnect is unnecessary, shows a wait window using GetWaitTime();
    // 'parent' is the thread's foreground window (after ESC is pressed it is used to detect whether
    // ESC was pressed in this window and not, for example, in another application; in the main thread
    // this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent' is also
    // the parent of any error message boxes; 'success' (must not be NULL) returns TRUE when the
    // operation succeeds;
    // 'notInPanel' + 'leftPanel' + 'userBuf' + 'userBufSize' + 'totalAttemptNum' + 'retryMsg' are
    // parameters for ReconnectIfNeeded(); the server reply text is in the buffer 'ftpReplyBuf'
    // (maximum size 'ftpReplyBufSize'), null-terminated - if it is longer than the buffer it is simply
    // truncated; if 'startPath' is not NULL and the connection is restored, a command to change the
    // working directory to 'startPath' is sent first and then to 'path' (handles relative path
    // changes); if 'startPath' is not NULL and reconnect is unnecessary, GetCurrentWorkingPath (with
    // 'forceRefresh'=FALSE) verifies whether the working directory on the server is exactly
    // 'startPath'; if not, the command first changes the working directory to 'startPath' and only
    // then to 'path'; if 'skipFirstReconnectIfNeeded' is TRUE, the connection is assumed to be
    // established (used to pick up a possible "retry"); 'userRejectsReconnect' (if not NULL) returns
    // TRUE when the operation fails solely because the user refused reconnect; returns FALSE only when
    // the command could not be sent - the connection was interrupted (on timeout it automatically
    // closes the connection hard - sending "QUIT" makes no sense)
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL SendChangeWorkingPath(BOOL notInPanel, BOOL leftPanel, HWND parent, const char* path,
                               char* userBuf, int userBufSize, BOOL* success, char* ftpReplyBuf,
                               int ftpReplyBufSize, const char* startPath, int* totalAttemptNum,
                               const char* retryMsg, BOOL skipFirstReconnectIfNeeded,
                               BOOL* userRejectsReconnect);

    // determines the path type on the FTP server (calls ::GetFTPServerPathType() inside the critical
    // section with 'ServerSystem' and 'ServerFirstReply')
    CFTPServerPathType GetFTPServerPathType(const char* path);

    // checks whether 'ServerSystem' contains the name 'systemName'
    BOOL IsServerSystem(const char* systemName);

    // if the "control connection" is closed, offers the user to reopen it (WARNING: does not set the
    // working directory on the server); returns TRUE when the "control connection" is ready for use,
    // and in that case sets SetStartTime() (so further waiting can continue after a possible
    // reconnect); 'notInPanel' is TRUE for a detached FS (not in a panel); if 'notInPanel' is FALSE,
    // the connection is in a panel - if 'leftPanel' is TRUE it is the left panel, otherwise the right
    // panel; 'parent' is the thread's foreground window (after ESC is pressed it is used to detect
    // whether ESC was pressed in this window and not, for example, in another application; in the
    // main thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin);
    // 'parent' is also the parent of any error message boxes; 'userBuf' + 'userBufSize' is the buffer
    // for a new user name on the FTP server (it may change during reconnect); 'reconnected' (if not
    // NULL) returns TRUE when the connection was restored (the "control connection" was reopened); if
    // 'setStartTimeIfConnected' is FALSE and reconnecting is unnecessary, SetStartTime() is not set;
    // 'totalAttemptNum' + 'retryMsg' + 'reconnectErrResID' + 'useFastReconnect' are parameters for
    // StartControlConnection(); 'userRejectsReconnect' (if not NULL) returns TRUE if the user refuses
    // to perform a reconnect
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL ReconnectIfNeeded(BOOL notInPanel, BOOL leftPanel, HWND parent, char* userBuf,
                           int userBufSize, BOOL* reconnected, BOOL setStartTimeIfConnected,
                           int* totalAttemptNum, const char* retryMsg,
                           BOOL* userRejectsReconnect, int reconnectErrResID,
                           BOOL useFastReconnect);

    // sends a command to the FTP server and returns the server's reply (WARNING: does not return
    // replies of type FTP_D1_MAYBESUCCESS - it automatically waits for the next server reply);
    // 'parent' is the thread's foreground window (after ESC is pressed it is used to detect whether
    // ESC was pressed in this window and not, for example, in another application; in the main thread
    // this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent' is also
    // the parent of any error message boxes; 'ftpCmd' is the command being sent; 'logCmd' is the string
    // for the log (identical to 'ftpCmd' except for omitted passwords, etc.); 'waitWndText' is the text
    // for the wait window (NULL = standard text showing the message about sending 'logCmd');
    // 'waitWndTime' is the delay before displaying the wait window; if 'allowCmdAbort' is TRUE, the
    // first ESC sends "ABOR" (probably only makes sense for commands with a data connection) and the
    // connection is aborted only after the second ESC; if 'allowCmdAbort' is FALSE, the connection is
    // aborted after the first ESC; if 'resetWorkingPathCache' is TRUE, ResetWorkingPathCache() is
    // called after sending the command (used when the command may change the current working
    // directory); if 'resetCurrentTransferModeCache' is TRUE, ResetCurrentTransferModeCache() is called
    // after sending the command (used when the command may change the current transfer mode); returns
    // TRUE if sending the command or aborting it and receiving the reply succeeded; 'cmdAborted'
    // (if not NULL) returns TRUE when the command was successfully aborted; the reply code is returned
    // in 'ftpReplyCode' (must not be NULL) - it is valid (!=-1) even when 'cmdAborted' == TRUE; the
    // reply text is stored in the buffer 'ftpReplyBuf' (maximum size 'ftpReplyBufSize'),
    // null-terminated - if it is longer than the buffer, it is simply truncated; if
    // 'specialUserInterface' is NULL, the standard wait window is used for the user interface,
    // otherwise the object provided via 'specialUserInterface' should be used (for example when
    // listing the current path);
    // returns FALSE if the connection was interrupted (on timeout it automatically closes the
    // connection hard - sending "QUIT" makes no sense); if 'canRetry' is not NULL, the error text can
    // be returned in 'retryMsg' (buffer of size 'retryMsgBufSize') - 'canRetry' returns TRUE; otherwise
    // the error is shown in a message box ('canRetry' is either NULL or FALSE is returned there);
    // can be called only from the main thread (uses wait windows, etc.)
    //
    // WARNING: when aborting commands ('allowCmdAbort'==TRUE) the system for receiving server replies
    //          is not fully resolved (servers return either one reply just for ABOR or two replies
    //          (list + abort); unfortunately both use code 226, so it is impossible to tell which case
    //          occurred) - this is handled by trying to receive everything the server sends in one
    //          packet (it probably sends both replies together); any additional replies are ignored as
    //          "unexpected" before the next FTP command sent by this method
    BOOL SendFTPCommand(HWND parent, const char* ftpCmd, const char* logCmd, const char* waitWndText,
                        int waitWndTime, BOOL* cmdAborted, int* ftpReplyCode, char* ftpReplyBuf,
                        int ftpReplyBufSize, BOOL allowCmdAbort, BOOL resetWorkingPathCache,
                        BOOL resetCurrentTransferModeCache, BOOL* canRetry, char* retryMsg,
                        int retryMsgBufSize, CSendCmdUserIfaceAbstract* specialUserInterface);

    // closes the "control connection" to the FTP server (or performs a hard socket close after a
    // timeout); 'parent' is the thread's foreground window (after ESC is pressed it is used to detect
    // whether ESC was pressed in this window and not, for example, in another application; in the main
    // thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent'
    // is also the parent of any error message boxes
    // can be called only from the main thread (uses wait windows, etc.)
    void CloseControlConnection(HWND parent);

    // if the user has not yet been informed about the "control connection" closing in the panel, this
    // method informs them; 'notInPanel' is TRUE for a detached FS (not in a panel); if 'notInPanel' is
    // FALSE, the connection is in a panel - if 'leftPanel' is TRUE it is the left panel, otherwise the
    // right panel; 'parent' is the parent of any message boxes; if 'quiet' is TRUE, the message about
    // the "control connection" closing is written only to the log; if 'quiet' is FALSE, a message box
    // is also displayed
    // WARNING: relies on Salamander doing no work with the "control connection" while idle (nothing is
    //          waiting for events, etc.)
    void CheckCtrlConClose(BOOL notInPanel, BOOL leftPanel, HWND parent, BOOL quiet);

    // returns the log UID (-1 if the log does not exist)
    int GetLogUID();

    // adds the text 'str' (length 'len') to the log of this "control connection"; if 'len' is -1, it
    // uses "len=strlen(str)"; if 'addTimeToLog' is TRUE, the current time is placed before the
    // message; returns TRUE when the log is updated successfully or if the log does not exist at all
    BOOL LogMessage(const char* str, int len, BOOL addTimeToLog = FALSE);

    // if the welcome-message window is open and the main Salamander window becomes active, activates
    // the welcome-message window (e.g. after closing an error message box the main window would become
    // active instead of the welcome-message window)
    // WARNING: may only be called in the main thread
    void ActivateWelcomeMsg();

    // detaches the welcome-message window from this "control connection" (it will no longer be
    // activated via ActivateWelcomeMsg)
    // WARNING: may only be called in the main thread
    void DetachWelcomeMsg() { OurWelcomeMsgDlg = NULL; }

    // returns an allocated string with the server's reply to the SYST command (get operating system);
    // returns NULL if allocation fails
    char* AllocServerSystemReply();

    // returns an allocated string with the first server reply; returns NULL if allocation fails
    char* AllocServerFirstReply();

    // posts the message 'msgID' to the socket (CTRLCON_KAPOSTSETUPNEXT: to trigger
    // SetupNextKeepAliveTimer() when 'KeepAliveMode' is 'kamProcessing' or
    // 'kamWaitingForEndOfProcessing') (CTRLCON_KALISTENFORCON: the "listen" port on the proxy server
    // was opened)
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called from any thread
    void PostMsgToCtrlCon(int msgID, void* msgParam);

    // initializes the operation object - calls its SetConnection() and returns TRUE on success;
    // WARNING: the operation 'oper' must not be inserted in FTPOperationsList !!!
    BOOL InitOperation(CFTPOperation* oper);

    // hands the active "control connection" to the newly created worker 'newWorker' (cannot be NULL);
    // 'parent' is the thread's foreground window (after ESC is pressed it is used to detect whether
    // ESC was pressed in this window and not, for example, in another application; in the main thread
    // this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin)
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    void GiveConnectionToWorker(CFTPWorker* newWorker, HWND parent);

    // takes over the active "control connection" from the worker 'workerWithCon' (cannot be NULL);
    // if this object already has the connection open, the takeover does not happen;
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    void GetConnectionFromWorker(CFTPWorker* workerWithCon);

    // returns (inside the critical section) the value of UseListingsCache
    BOOL GetUseListingsCache();

    // downloads a single file in the working directory; the file name (without path) is 'fileName'; if
    // the file size in bytes is known, it is provided in 'fileSizeInBytes', otherwise it is
    // CQuadWord(-1, -1); 'asciiMode' is TRUE/FALSE for transferring the file in ASCII/binary transfer
    // mode; 'parent' is the thread's foreground window (after ESC is pressed it is used to detect
    // whether ESC was pressed in this window and not, for example, in another application; in the main
    // thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent'
    // is also the parent of any error message boxes; 'workPath' is the working directory; 'tgtFileName'
    // is the target file name (where the download is stored); 'newFileCreated' returns TRUE if at least
    // part of the file was downloaded (something exists on disk); 'newFileIncomplete' returns TRUE if
    // the file was not downloaded completely; if 'newFileCreated' is TRUE, 'newFileSize' returns the
    // file size on disk; 'totalAttemptNum', 'panel', 'notInPanel', 'userBuf', and 'userBufSize' are
    // parameters for ReconnectIfNeeded()
    void DownloadOneFile(HWND parent, const char* fileName, CQuadWord const& fileSizeInBytes,
                         BOOL asciiMode, const char* workPath, const char* tgtFileName,
                         BOOL* newFileCreated, BOOL* newFileIncomplete, CQuadWord* newFileSize,
                         int* totalAttemptNum, int panel, BOOL notInPanel, char* userBuf,
                         int userBufSize);

    // creates the directory 'newName' (any string from the user - treated as an absolute or relative
    // path on the server - Salamander path syntax is ignored); 'parent' is the parent of any message
    // boxes; 'newName' is the name of the directory being created; on success, 'newName' (buffer
    // 2 * MAX_PATH characters) returns the directory name to focus in the panel on the next refresh;
    // returns success (on failure it is assumed the user has already seen an error window); 'workPath'
    // is the working directory; 'totalAttemptNum', 'panel', 'notInPanel', 'userBuf', and 'userBufSize'
    // are parameters for ReconnectIfNeeded(); 'changedPath' (at least FTP_MAX_PATH characters)
    // returns the server path that needs to be refreshed (if 'changedPath' is empty, no refresh is
    // required)
    BOOL CreateDir(char* changedPath, HWND parent, char* newName, const char* workPath,
                   int* totalAttemptNum, int panel, BOOL notInPanel, char* userBuf,
                   int userBufSize);

    // renames the file/directory 'fromName' to 'newName'; 'parent' is the parent of any message
    // boxes; on success, 'newName' (buffer 2 * MAX_PATH characters) returns the file/directory name
    // to focus in the panel during the next refresh; returns success (on failure it is assumed the user
    // has already seen an error window); 'workPath' is the working directory; 'totalAttemptNum',
    // 'panel', 'notInPanel', 'userBuf', and 'userBufSize' are parameters for ReconnectIfNeeded();
    // 'changedPath' (at least FTP_MAX_PATH characters) returns the server path that needs to be
    // refreshed (if 'changedPath' is empty, no refresh is required)
    BOOL QuickRename(char* changedPath, HWND parent, const char* fromName, char* newName,
                     const char* workPath, int* totalAttemptNum, int panel, BOOL notInPanel,
                     char* userBuf, int userBufSize, BOOL isVMS, BOOL isDir);

    // sends a request to open a "listen" port (either on the local machine or on the proxy server) for
    // the data connection 'dataConnection'; inputs 'listenOnIP' + 'listenOnPort' specify the IP+port
    // where the "listen" port should be opened (makes sense only without a proxy server);
    // 'parent' is the thread's foreground window (after ESC is pressed it is used to detect whether ESC
    // was pressed in this window and not, for example, in another application; in the main thread this
    // is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent' is also the
    // parent of any error message boxes; 'waitWndTime' is the delay before displaying the wait window;
    // returns TRUE on success - 'listenOnIP' + 'listenOnPort' then contain the IP+port where we wait
    // for the FTP server to connect; returns FALSE if an interruption or error occurred; if retrying is
    // meaningful, the connection is forcibly closed (we could send "QUIT", but for now we simplify
    // our lives) and the error text is returned in 'retryMsg' (buffer of size 'retryMsgBufSize', must
    // not be 0) and 'canRetry' (must not be NULL) returns TRUE; if retrying makes no sense, the error
    // is shown in a message box and 'canRetry' returns FALSE (the connection is not interrupted);
    // 'errBuf' is a helper buffer of size 'errBufSize' (must not be 0) - used for texts displayed in
    // message boxes;
    // can be called only from the main thread (uses wait windows, etc.)
    BOOL OpenForListeningAndWaitForRes(HWND parent, CDataConnectionSocket* dataConnection,
                                       DWORD* listenOnIP, unsigned short* listenOnPort,
                                       BOOL* canRetry, char* retryMsg, int retryMsgBufSize,
                                       int waitWndTime, char* errBuf, int errBufSize);

    // returns TRUE if the "LIST -a" command is used for listing
    BOOL IsListCommandLIST_a();

    // if listing uses the "LIST -a" command, it switches to ListCommand or "LIST"; otherwise it uses
    // "LIST -a"
    void ToggleListCommandLIST_a();

protected:
    // ******************************************************************************************
    // helper methods - do not use outside this object
    // ******************************************************************************************

    // adds an event; if 'rewritable' is TRUE, this event will be overwritten when another event is
    // added; 'event' + 'data1' + 'data2' are the event data; returns TRUE if the event was added to
    // the queue successfully, returns FALSE on insufficient memory
    BOOL AddEvent(CControlConnectionSocketEvent event, DWORD data1, DWORD data2, BOOL rewritable = FALSE);

    // retrieves an event; the event is returned in 'event' + 'data1' + 'data2' (must not be NULL);
    // returns TRUE if an event is returned from the queue, returns FALSE when the queue is empty
    BOOL GetEvent(CControlConnectionSocketEvent* event, DWORD* data1, DWORD* data2);

    // waits for an event on the socket (event = the next phase of the started operation finished or
    // ESC); 'parent' is the thread's foreground window (after ESC is pressed it is used to detect
    // whether ESC was pressed in this window and not, for example, in another application; in the main
    // thread this is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin);
    // 'event' + 'data1' + 'data2' (must not be NULL) return the event (one of ccsevXXX; ccsevESC if the
    // user pressed ESC); 'milliseconds' is the time in ms to wait for the event; after that the method
    // returns 'event' == ccsevTimeout; if 'milliseconds' is INFINITE, wait without a time limit; if
    // 'waitWnd' is not NULL, it monitors the close button in the wait window 'waitWnd'; if 'userIface'
    // is not NULL, it monitors the close button in the user interface 'userIface'; additionally, if
    // 'waitForUserIfaceFinish' is TRUE, it watches for the event announcing the completion of the user
    // interface ("data connection" + "keep alive") instead of the socket event; can be called from any
    // thread
    void WaitForEventOrESC(HWND parent, CControlConnectionSocketEvent* event,
                           DWORD* data1, DWORD* data2, int milliseconds, CWaitWindow* waitWnd,
                           CSendCmdUserIfaceAbstract* userIface, BOOL waitForUserIfaceFinish);

    // helper method to detect whether the buffer 'ReadBytes' already contains the entire reply from the
    // FTP server; returns TRUE on success - 'reply' (must not be NULL) receives the pointer to the
    // beginning of the reply, 'replySize' (must not be NULL) receives the reply length, 'replyCode'
    // (if not NULL) receives the FTP reply code or -1 if the reply has no code (does not start with a
    // three-digit number); if the reply is not complete yet, returns FALSE - another call to
    // ReadFTPReply makes sense only after an event is received (ccsevNewBytesRead or another event that
    // may have overwritten ccsevNewBytesRead, because ccsevNewBytesRead is overwritable when the read
    // succeeds)
    // WARNING: must be called from the SocketCritSect critical section (otherwise the 'ReadBytes'
    // buffer could change arbitrarily)
    BOOL ReadFTPReply(char** reply, int* replySize, int* replyCode = NULL);

    // helper method to release the FTP server reply (length 'replySize') from the 'ReadBytes' buffer
    // WARNING: must be called from the SocketCritSect critical section (otherwise the 'ReadBytes'
    // buffer could change arbitrarily)
    void SkipFTPReply(int replySize);

    // writes bytes from the buffer 'buffer' of length 'bytesToWrite' to the socket (performs "send");
    // if 'bytesToWrite' is -1, it writes strlen(buffer) bytes; returns FALSE on error and, if the
    // Windows error code is known, returns it in 'error' (if not NULL); if it returns TRUE, at least
    // part of the buffer was written successfully; 'allBytesWritten' (must not be NULL) returns TRUE if
    // the entire buffer was written; if 'allBytesWritten' returns FALSE, you must wait for the
    // ccsevWriteDone event before calling Write again (once it arrives, the write has finished)
    // can be called from any thread
    BOOL Write(const char* buffer, int bytesToWrite, DWORD* error, BOOL* allBytesWritten);

    // clears the read and write buffers and the event queue (useful before reopening the connection)
    void ResetBuffersAndEvents();

    // if a keep-alive command is currently being sent in this "control connection", waits for it to
    // finish; if no keep-alive command is in progress, exits immediately; cancels the keep-alive timer
    // (if it exists); switches 'KeepAliveMode' to 'kamForbidden'; reports the connection loss to the log
    // and also shows an error message box leading to a disconnect (reconnect is handled by the next
    // regular command); keep-alive command errors are reported only in the log; 'parent' is the already
    // disabled foreground window of the thread (after ESC is pressed it is used to detect whether ESC
    // was pressed in this window and not, for example, in another application; in the main thread this
    // is SalamanderGeneral->GetMsgBoxParent() or a dialog opened by the plugin); 'parent' is also the
    // parent of any error message boxes; 'waitWndTime' is the delay before showing the wait window;
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called only from the main thread (uses wait windows, etc.)
    void WaitForEndOfKeepAlive(HWND parent, int waitWndTime);

    // called after a regular command finishes ('KeepAliveMode' must be 'kamForbidden' or 'kamNone')
    // when the keep-alive timer must be set; switches 'KeepAliveMode' to 'kamWaiting' (or 'kamNone' if
    // keep-alive is disabled); if 'immediate' is TRUE, the first keep-alive command is executed as soon
    // as possible
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called from any thread
    void SetupKeepAliveTimer(BOOL immediate = FALSE);

    // called after a keep-alive command finishes ('KeepAliveMode' should be 'kamProcessing' or
    // 'kamWaitingForEndOfProcessing' or 'kamNone' (after ReleaseKeepAlive())); if needed, sets a new
    // timer for the next keep-alive command; switches 'KeepAliveMode' to 'kamWaiting' (only when
    // 'KeepAliveMode' == 'kamProcessing')
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called from any thread
    void SetupNextKeepAliveTimer();

    // releases resources obtained for the keep-alive command, cancels the keep-alive timer (if it
    // exists), sets 'KeepAliveMode' to 'kamNone' and frees 'KeepAliveDataCon' (deleted by
    // SocketsThread inside its critical section, so during the execution of socket methods in this
    // thread the release cannot happen); may be called in any 'KeepAliveMode' state;
    // WARNING: this method must not be called from the SocketCritSect critical section (the method uses
    //          SocketsThread)
    // can be called from any thread
    void ReleaseKeepAlive();

    // sends the keep-alive command 'ftpCmd' to the server; 'logUID' is the log UID
    BOOL SendKeepAliveCmd(int logUID, const char* ftpCmd);

    // ******************************************************************************************
    // methods called in the "sockets" thread (based on messages received from the system or other threads)
    //
    // WARNING: called in SocketsThread->CritSect; they should run as quickly as possible (no waiting for
    //          user input, etc.)
    // ******************************************************************************************

    // receives the result of GetHostByAddress; if 'ip' == INADDR_NONE it is an error and 'err' may
    // contain the error code (if 'err' != 0)
    virtual void ReceiveHostByAddress(DWORD ip, int hostUID, int err);

    // receives events for this socket (FD_READ, FD_WRITE, FD_CLOSE, etc.); 'index' is the index of the
    // socket in SocketsThread->Sockets (used for re-sending messages for the socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // receives the result of ReceiveNetEvent(FD_CLOSE) - if 'error' is not NO_ERROR, it is the Windows
    // error code (came with FD_CLOSE or occurred while processing FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // receives a timer with ID 'id' and parameter 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // receives a posted message with ID 'id' and parameter 'param'
    virtual void ReceivePostMessage(DWORD id, void* param);
};

//
// ****************************************************************************
// CClosedCtrlConChecker
//
// handles informing the user about a "control connection" closing outside operations (e.g. timeout or
// "kick" leading to a disconnect from the FTP server)

class CClosedCtrlConChecker
{
protected:
    CRITICAL_SECTION DataSect; // critical section for accessing the object's data

    // array of closed sockets that we check (whenever FTPCMD_CLOSECONNOTIF is received)
    TIndirectArray<CControlConnectionSocket> CtrlConSockets;

    BOOL CmdNotPost; // FALSE if the FTPCMD_CLOSECONNOTIF command has already been posted

public:
    CClosedCtrlConChecker();
    ~CClosedCtrlConChecker();

    // adds a closed "control connection" to the test; returns TRUE on success
    BOOL Add(CControlConnectionSocket* sock);

    // informs the user about the "control connection" closing if it has not been reported yet;
    // called in the main thread while idle; 'parent' is the parent of any message boxes
    void Check(HWND parent);
};

extern CClosedCtrlConChecker ClosedCtrlConChecker; // handles informing the user about "control connection" closures outside operations
extern CListingCache ListingCache;                 // cache of path listings on servers (used when changing and listing paths)

// "control connection" from the left/right panel (NULL == our FS is not in the left/right panel)
// WARNING: do not touch the "control connection" object - only used to determine the FS position in the panel (left/right/disconnected)
extern CControlConnectionSocket* LeftPanelCtrlCon;
extern CControlConnectionSocket* RightPanelCtrlCon;
extern CRITICAL_SECTION PanelCtrlConSect; // critical section for accessing LeftPanelCtrlCon and RightPanelCtrlCon

void RefreshValuesOfPanelCtrlCon(); // refreshes LeftPanelCtrlCon and RightPanelCtrlCon values according to the current state
