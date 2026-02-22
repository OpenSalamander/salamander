// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

int CSimpleListPluginDataInterface::ListingColumnWidth = 0;      // LO/HI-WORD: left/right panel: width of the Raw Listing column
int CSimpleListPluginDataInterface::ListingColumnFixedWidth = 0; // LO/HI-WORD: left/right panel: does the Raw Listing column have a fixed width?

// Global variables where I store pointers to Salamander's global variables
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;

CSimpleListPluginDataInterface SimpleListPluginDataInterface;

//
// ****************************************************************************
// CPluginFSInterface
//

CPluginFSInterface::CPluginFSInterface()
{
    Host[0] = 0;
    Port = -1;
    User[0] = 0;
    Path[0] = 0;

    ErrorState = fesOK;
    IsDetached = FALSE;

    ControlConnection = NULL;
    RescuePath[0] = 0;
    HomeDir[0] = 0;
    OverwritePathListing = FALSE;
    PathListing = NULL;
    PathListingLen = 0;
    memset(&PathListingDate, 0, sizeof(PathListingDate));
    PathListingIsIncomplete = FALSE;
    PathListingIsBroken = FALSE;
    PathListingMayBeOutdated = FALSE;
    PathListingStartTime = 0;

    DirLineHotPathType = ftpsptEmpty;
    DirLineHotPathUserLength = 0;

    ChangePathOnlyGetCurPathTime = 0;

    TotalConnectAttemptNum = 1;

    AutodetectSrvType = TRUE;
    LastServerType[0] = 0;

    InformAboutUnknownSrvType = TRUE;
    NextRefreshCanUseOldListing = FALSE;
    NextRefreshWontClearCache = FALSE;

    TransferMode = Config.TransferMode;

    CalledFromDisconnectDialog = FALSE;

    RefreshPanelOnActivation = FALSE;
}

CPluginFSInterface::~CPluginFSInterface()
{
    if (PathListing != NULL)
    {
        memset(PathListing, 0, PathListingLen); // might contain confidential data, better to zero it
        free(PathListing);
    }
    if (ControlConnection != NULL)
        TRACE_E("Unexpected situation in CPluginFSInterface::~CPluginFSInterface(): ControlConnection is not closed!");
}

BOOL CPluginFSInterface::MakeUserPart(char* buffer, int bufferSize, char* path, BOOL ignorePath)
{
    char* end = buffer + bufferSize;
    char* s = buffer;
    if (s < end)
        *s++ = '/';
    if (s < end)
        *s++ = '/';
    int l;
    if (strcmp(User, FTP_ANONYMOUS) != 0) // ignore the anonymous user
    {
        l = (int)strlen(User);
        if (l > end - s)
            l = (int)(end - s);
        memmove(s, User, l);
        s += l;
        if (s < end)
            *s++ = '@';
    }
    l = (int)strlen(Host);
    if (l > end - s)
        l = (int)(end - s);
    memmove(s, Host, l);
    s += l;
    if (Port != IPPORT_FTP) // ignore the standard FTP port (IPPORT_FTP)
    {
        if (s < end)
            *s++ = ':';
        char buf[20];
        l = (int)strlen(_itoa(Port, buf, 10));
        if (l > end - s)
            l = (int)(end - s);
        memmove(s, buf, l);
        s += l;
    }
    if (!ignorePath)
    {
        if (path == NULL)
            path = Path;
        char slash = '/';
        if (*path == '/')
            path++;
        else
        {
            if (*path == '\\')
            {
                slash = '\\';
                path++;
            }
        }
        if (s < end)
            *s++ = slash; // at least the root path will always be there
        l = (int)strlen(path);
        if (l > end - s)
            l = (int)(end - s);
        memmove(s, path, l);
        s += l;
    }
    if (s < end)
    {
        *s = 0;
        return TRUE;
    }
    else
    {
        if (bufferSize > 0)
            *(end - 1) = 0;
        return FALSE;
    }
}

void CPluginFSInterface::CheckCtrlConClose(HWND parent)
{
    if (ControlConnection != NULL && !ControlConnection->IsConnected())
    {
        int panel;
        BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
        ControlConnection->CheckCtrlConClose(notInPanel, panel == PANEL_LEFT, parent, notInPanel);
    }
}

CFTPServerPathType
CPluginFSInterface::GetFTPServerPathType(const char* path)
{
    if (ControlConnection != NULL)
        return ControlConnection->GetFTPServerPathType(path);
    return ftpsptEmpty;
}

BOOL CPluginFSInterface::ReconnectIfNeeded(HWND parent, BOOL* reconnected, BOOL setStartTimeIfConnected,
                                           int* totalAttemptNum, const char* retryMsg)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ReconnectIfNeeded(, , %d, , %s)",
                        setStartTimeIfConnected, retryMsg);
    if (ControlConnection != NULL)
    {
        int panel;
        BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
        return ControlConnection->ReconnectIfNeeded(notInPanel, panel == PANEL_LEFT, parent,
                                                    User, USER_MAX_SIZE, reconnected,
                                                    setStartTimeIfConnected, totalAttemptNum,
                                                    retryMsg, NULL, -1, FALSE);
    }
    return FALSE;
}

BOOL CPluginFSInterface::GetRootPath(char* userPart)
{
    static char buff[] = "/";
    return MakeUserPart(userPart, MAX_PATH, buff);
}

BOOL CPluginFSInterface::GetCurrentPath(char* userPart)
{
    return MakeUserPart(userPart, MAX_PATH);
}

BOOL CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    if (ControlConnection == NULL)
        return FALSE; // translation is not possible (the FS has not been connected yet); let Salamander report the error

    char errBuf[900 + FTP_MAX_PATH];
    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_LOGMSGCHANGINGPATH), path);
    ControlConnection->LogMessage(errBuf, -1, TRUE);

    int panel;
    BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
    char replyBuf[700];
    TotalConnectAttemptNum = 1; // start of a user-requested action -> if reconnection is needed, this is the first attempt
    const char* retryMsgAux = NULL;
    BOOL canRetry = FALSE;
    char retryMsgBuf[300];
    ControlConnection->SetStartTime();
    BOOL retErr = TRUE;
    while (ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                    SalamanderGeneral->GetMsgBoxParent(),
                                                    path, User, USER_MAX_SIZE, &success, replyBuf,
                                                    700, Path, &TotalConnectAttemptNum,
                                                    retryMsgAux, FALSE, NULL))
    {
        BOOL run = FALSE;
        if (!success)
        {
            _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR), path, replyBuf);
            SalamanderGeneral->SalMessageBox(parent, errBuf, LoadStr(IDS_FTPERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
        }
        else
        {
            if (ControlConnection->GetCurrentWorkingPath(SalamanderGeneral->GetMsgBoxParent(),
                                                         replyBuf, 700, TRUE, &canRetry, retryMsgBuf, 300))
            {
                lstrcpyn(path, replyBuf, pathSize); // successfully obtained a new path on the server
            }
            else
            {
                if (canRetry) // the "retry" option is allowed
                {
                    run = TRUE;
                    retryMsgAux = retryMsgBuf;
                }
                else
                    success = FALSE;
            }
        }
        if (!run)
        {
            retErr = FALSE;
            break;
        }
    }
    if (retErr)
        success = FALSE;
    if (success)
    {
        char root[2 * MAX_PATH];
        sprintf(root, "%s:", fsName);
        int len = (int)strlen(root);
        if (MakeUserPart(root + len, 2 * MAX_PATH - len, path) &&
            (int)strlen(root) < pathSize)
        {
            strcpy(path, root);
            ChangePathOnlyGetCurPathTime = GetTickCount(); // optimization for ChangePath() called right after obtaining the working path
        }
        else
        {
            success = FALSE;
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_TOOLONGPATH),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        }
    }
    return TRUE;
}

BOOL CPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
{
    MakeUserPart(buf, bufSize); // if the path does not fit, the name certainly will not either (an error will be reported)
    CFTPServerPathType type = GetFTPServerPathType(Path);
    if (isDir == 2) // up-dir
    {
        char tmpPath[FTP_MAX_PATH];
        lstrcpyn(tmpPath, Path, FTP_MAX_PATH);
        if (FTPCutDirectory(type, tmpPath, FTP_MAX_PATH, NULL, 0, NULL))
            return MakeUserPart(buf, bufSize, tmpPath);
        else
            return FALSE;
    }
    else
        return FTPPathAppend(type, buf, bufSize, file.Name, isDir);
}

BOOL CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    char actUserPart[FTP_USERPART_SIZE];
    if (currentFSNameIndex == fsNameIndex &&
        MakeUserPart(actUserPart, FTP_USERPART_SIZE))
    {
        CFTPServerPathType type = GetFTPServerPathType(Path);
        return FTPIsTheSamePath(type, actUserPart, userPart, TRUE, FTPGetUserLength(User));
    }
    else
        return FALSE; // does not fit, cannot compare
}

BOOL CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    if (Config.UseConnectionDataFromConfig)
    { // the user is opening a new connection from the Connect dialog - we must return FALSE (the user might
        // request a second connection to the same server - a situation where ChangePath would otherwise return TRUE)
        return FALSE;
    }

    if (currentFSNameIndex != fsNameIndex)
        return FALSE; // cannot mix FTP and FTPS

    char actUserPart[FTP_USERPART_SIZE];
    if (MakeUserPart(actUserPart, FTP_USERPART_SIZE))
    {
        return FTPHasTheSameRootPath(actUserPart, userPart, FTPGetUserLength(User));
    }
    else
        return FALSE; // does not fit, cannot compare
}

void CPluginFSInterface::ClearHostFromListingCacheIfFirstCon(const char* host, int port, const char* user)
{
    int i;
    for (i = 0; i < FTPConnections.Count; i++) // try to find an FS with the given host+port+user combination
    {
        CPluginFSInterface* fs = FTPConnections[i];
        if (fs != this && fs->ContainsHost(host, port, user))
            return; // found one, the cache will not be cleared
    }

    // Clear cached listings for all paths with the given host+port+user combination
    ListingCache.RefreshOnPath(host, port, user, ftpsptEmpty, "", TRUE);

    // Remove files from the disk cache (FTP and FTPS)
    char path[FTP_USERPART_SIZE + 50]; // +50 is reserved for the FS name
    strcpy(path, AssignedFSName);      // compose the name for FTP
    strcat(path, ":");
    if (MakeUserPart(path + AssignedFSNameLen + 1, FTP_USERPART_SIZE + 49 - (AssignedFSNameLen + 1), NULL, TRUE))
    {
        char* end = path + strlen(path);
        strcpy(end, "/");
        SalamanderGeneral->RemoveFilesFromCache(path); // after ftp://user@host:port there is always / or \\, clear both variants
        strcpy(end, "\\");
        SalamanderGeneral->RemoveFilesFromCache(path);
        *end = 0;

        if (strlen(path) - AssignedFSNameLen + AssignedFSNameLenFTPS < FTP_USERPART_SIZE + 49)
        {
            char path2[FTP_USERPART_SIZE + 50]; // +50 is reserved for the FS name
            strcpy(path2, AssignedFSNameFTPS);  // compose the name for FTPS
            strcpy(path2 + AssignedFSNameLenFTPS, path + AssignedFSNameLen);
            end = path2 + strlen(path2);
            strcpy(end, "/");
            SalamanderGeneral->RemoveFilesFromCache(path2); // after ftps://user@host:port there is always / or \\, clear both variants
            strcpy(end, "\\");
            SalamanderGeneral->RemoveFilesFromCache(path2);
        }
    }
}

BOOL CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                    const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                    BOOL forceRefresh, int mode)
{
    if (forceRefresh && RefreshPanelOnActivation)
        RefreshPanelOnActivation = FALSE;

    if (mode != 3 && (pathWasCut != NULL || cutFileName != NULL))
    {
        TRACE_E("Incorrect value of 'mode' in CPluginFSInterface::ChangePath().");
        mode = 3;
    }
    OverwritePathListing = TRUE;
    if (cutFileName != NULL)
        *cutFileName = 0;
    if (pathWasCut != NULL)
        *pathWasCut = FALSE;
    CFTPErrorState lastErrorState = ErrorState;
    ErrorState = fesOK; // ready for the next call again

    if (lastErrorState == fesFatal)
    {
        TargetPanelPath[0] = 0; // the connection failed, no path change in the target panel
        return FALSE;           // fatal error, stop
    }

    // Treat a hard refresh as distrust of the path; drop from the disk cache all files
    // downloaded for View (including files from subpaths for both FTP and FTPS)
    if (mode == 1 && forceRefresh && !NextRefreshWontClearCache)
    {
        char uniqueFileName[FTP_USERPART_SIZE + 50]; // +50 is reserved for the FS name; cache names are case-sensitive
        int i;
        for (i = 0; i < 2; i++)
        {
            strcpy(uniqueFileName, i == 0 ? AssignedFSName : AssignedFSNameFTPS);
            strcat(uniqueFileName, ":");
            int len = (int)strlen(uniqueFileName);
            lstrcpyn(uniqueFileName + len, userPart, FTP_USERPART_SIZE + 50 - len);
            SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
        }
    }

    char newUserPart[FTP_USERPART_SIZE + 1];
    if (ControlConnection == NULL) // opening the connection (opening the path on the FTP server)
    {
        TargetPanelPath[0] = 0;
        TotalConnectAttemptNum = 1; // opening the connection = first attempt to open it
        InformAboutUnknownSrvType = TRUE;

        BOOL parsedPath = TRUE; // TRUE = path obtained from the user part; need to decide whether to trim '/' or '\\' at the start
        ControlConnection = new CControlConnectionSocket;
        if (ControlConnection == NULL || !ControlConnection->IsGood())
        {
            if (ControlConnection != NULL) // insufficient system resources for allocating the object
            {
                DeleteSocket(ControlConnection);
                ControlConnection = NULL;
            }
            else
                TRACE_E(LOW_MEMORY);
            TargetPanelPath[0] = 0; // the connection failed, no path change in the target panel
            return FALSE;           // fatal error
        }

        char anonymousPasswd[PASSWORD_MAX_SIZE];
        Config.GetAnonymousPasswd(anonymousPasswd, PASSWORD_MAX_SIZE);
        if (*userPart == 0 && Config.UseConnectionDataFromConfig) // data from the Connect dialog
        {
            // Pull server data selected in the Connect dialog from the configuration
            CFTPServer* server;
            if (Config.LastBookmark == 0)
                server = &Config.QuickConnectServer;
            else
            {
                if (Config.LastBookmark - 1 >= 0 && Config.LastBookmark - 1 < Config.FTPServerList.Count)
                {
                    server = Config.FTPServerList[Config.LastBookmark - 1];
                }
                else
                {
                    TRACE_E("Unexpected situation in CPluginFSInterface::ChangePath().");
                    TargetPanelPath[0] = 0; // the connection failed, no path change in the target panel
                    return FALSE;
                }
            }

            AutodetectSrvType = server->ServerType == NULL;
            lstrcpyn(LastServerType,
                     HandleNULLStr(server->ServerType != NULL ? (server->ServerType +
                                                                 (server->ServerType[0] == '*' ? 1 : 0))
                                                              : NULL),
                     SERVERTYPE_MAX_SIZE);

            lstrcpyn(Host, HandleNULLStr(server->Address), HOST_MAX_SIZE);
            Port = server->Port;
            if (server->AnonymousConnection)
                strcpy(User, FTP_ANONYMOUS);
            else
            {
                lstrcpyn(User, HandleNULLStr(server->UserName), USER_MAX_SIZE);
            }
            lstrcpyn(Path, HandleNULLStr(server->InitialPath), FTP_MAX_PATH);
            parsedPath = FALSE; // path entered by the user (never trim '/' or '\\' at the beginning)

            if (server->TargetPanelPath != NULL)
                lstrcpyn(TargetPanelPath, server->TargetPanelPath, MAX_PATH);

            BOOL useListingsCache = Config.UseListingsCache;
            if (server->UseListingsCache != 2)
                useListingsCache = server->UseListingsCache;
            BOOL usePassiveMode = Config.PassiveMode;
            if (server->UsePassiveMode != 2)
                usePassiveMode = server->UsePassiveMode;
            BOOL keepConnectionAlive = Config.KeepAlive;
            int keepAliveSendEvery = Config.KeepAliveSendEvery;
            int keepAliveStopAfter = Config.KeepAliveStopAfter;
            int keepAliveCommand = Config.KeepAliveCommand;
            if (server->KeepConnectionAlive != 2)
            {
                keepConnectionAlive = server->KeepConnectionAlive;
                if (server->KeepConnectionAlive == 1) // custom values
                {
                    keepAliveSendEvery = server->KeepAliveSendEvery;
                    keepAliveStopAfter = server->KeepAliveStopAfter;
                    keepAliveCommand = server->KeepAliveCommand;
                }
            }
            TransferMode = Config.TransferMode;
            if (server->TransferMode != 0)
            {
                switch (server->TransferMode)
                {
                case 1:
                    TransferMode = trmBinary;
                    break;
                case 2:
                    TransferMode = trmASCII;
                    break;
                default:
                    TransferMode = trmAutodetect;
                    break;
                }
            }
            int encControlConn = server->EncryptControlConnection == 1;
            lstrcpyn(fsName, encControlConn ? AssignedFSNameFTPS : AssignedFSName, MAX_PATH);
            int encDataConn = encControlConn && server->EncryptDataConnection == 1;
            int compressData = (server->CompressData >= 0) ? server->CompressData : Config.CompressData;

            ClearHostFromListingCacheIfFirstCon(Host, Port, User);

            char password[PASSWORD_MAX_SIZE];
            if (server->AnonymousConnection)
            {
                lstrcpyn(password, anonymousPasswd, PASSWORD_MAX_SIZE);
            }
            else
            {
                char* plainPassword;
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                if (server->EncryptedPassword != NULL && passwordManager->DecryptPassword(server->EncryptedPassword, server->EncryptedPasswordSize, &plainPassword))
                {
                    lstrcpyn(password, plainPassword, PASSWORD_MAX_SIZE);
                    memset(plainPassword, 0, lstrlen(plainPassword)); // clear the buffer with the password
                    SalamanderGeneral->Free(plainPassword);
                }
                else
                    password[0] = 0;
            }

            // Handling decryptability of a possible proxy password is done in the connect dialog
            ControlConnection->SetConnectionParameters(Host, Port, User,
                                                       password,
                                                       useListingsCache,
                                                       server->InitFTPCommands,
                                                       usePassiveMode,
                                                       server->ListCommand,
                                                       keepConnectionAlive,
                                                       keepAliveSendEvery,
                                                       keepAliveStopAfter,
                                                       keepAliveCommand,
                                                       server->ProxyServerUID,
                                                       encControlConn,
                                                       encDataConn,
                                                       compressData);
            memset(password, 0, lstrlen(password)); // clear the buffer with the password
        }
        else // connection based on changing the path in the FTP file system (e.g. Shift+F7 + "ftp://ftp.altap.cz/")
        {
            if (Config.UseConnectionDataFromConfig)
                TRACE_E("Unexpected situation in CPluginFSInterface::ChangePath() - UseConnectionDataFromConfig + nonempty userpart.");

            int encControlAndDataConn = currentFSNameIndex == AssignedFSNameIndexFTPS;

            // Verify that any password for the default proxy can be decrypted (we may call SetConnectionParameters() only if it can)
            if (!Config.FTPProxyServerList.EnsurePasswordCanBeDecrypted(SalamanderGeneral->GetMsgBoxParent(), Config.DefaultProxySrvUID))
            {
                TargetPanelPath[0] = 0; // the connection failed, no path change in the target panel
                return FALSE;           // fatal error
            }

            AutodetectSrvType = TRUE; // we are using automatic server type detection
            LastServerType[0] = 0;

            lstrcpyn(newUserPart, userPart, FTP_USERPART_SIZE);
            char *u, *host, *p, *path, *password;
            char firstCharOfPath = '/';
            FTPSplitPath(newUserPart, &u, &password, &host, &p, &path, &firstCharOfPath, 0);
            if (password != NULL && *password == 0)
                password = NULL;
            if (host == NULL || *host == 0)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_HOSTNAMEMISSING),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // erase the memory that contained the password
                TargetPanelPath[0] = 0;                        // the connection failed, no path change in the target panel
                return FALSE;                                  // fatal error
            }
            char user[USER_MAX_SIZE];
            if (u == NULL || u != NULL && *u == 0)
                strcpy(user, FTP_ANONYMOUS);
            else
                lstrcpyn(user, u, USER_MAX_SIZE);
            int port = IPPORT_FTP;
            if (p != NULL && *p != 0)
            {
                char* t = p;
                while (*t >= '0' && *t <= '9')
                    t++; // verify that it is a number
                port = atoi(p);
                if (*t != 0 || port < 1 || port > 65535) // invalid port number
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_INVALIDPORT),
                                                      LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                    memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // erase the memory that contained the password
                    TargetPanelPath[0] = 0;                        // the connection failed, no path change in the target panel
                    return FALSE;                                  // fatal error
                }
            }

            lstrcpyn(Host, host, HOST_MAX_SIZE);
            Port = port;
            lstrcpyn(User, user, USER_MAX_SIZE);
            if (path != NULL)
            {
                Path[0] = firstCharOfPath;
                lstrcpyn(Path + 1, path, FTP_MAX_PATH - 1);
            }
            else
                Path[0] = 0;

            ClearHostFromListingCacheIfFirstCon(Host, Port, User);

            if (strcmp(user, FTP_ANONYMOUS) == 0 && password == NULL)
                password = anonymousPasswd;
            ControlConnection->SetConnectionParameters(Host, Port, User, HandleNULLStr(password),
                                                       Config.UseListingsCache, NULL, Config.PassiveMode,
                                                       NULL, Config.KeepAlive, Config.KeepAliveSendEvery,
                                                       Config.KeepAliveStopAfter, Config.KeepAliveCommand,
                                                       -2 /* default proxy server */,
                                                       encControlAndDataConn, encControlAndDataConn, Config.CompressData);
            TransferMode = Config.TransferMode;

            // password - if not NULL, contains the password for the connection
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // erase the memory that contained the password
        }

        ControlConnection->SetStartTime();
        if (!ControlConnection->StartControlConnection(SalamanderGeneral->GetMsgBoxParent(),
                                                       User, USER_MAX_SIZE, FALSE, RescuePath,
                                                       FTP_MAX_PATH, &TotalConnectAttemptNum,
                                                       NULL, TRUE, -1, FALSE))
        {                                            // failed to connect, release the socket object (signals the "never connected" state)
            ControlConnection->ActivateWelcomeMsg(); // if any message box deactivated the welcome-msg window, activate it again
            DeleteSocket(ControlConnection);
            ControlConnection = NULL;
            Logs.RefreshListOfLogsInLogsDlg();
            TargetPanelPath[0] = 0; // the connection failed, no path change in the target panel
            return FALSE;
        }
        lstrcpyn(HomeDir, RescuePath, FTP_MAX_PATH); // save the current path after logging into the server (home dir)
        char* pathListing = NULL;
        int pathListingLen = 0;
        CFTPDate pathListingDate;
        memset(&pathListingDate, 0, sizeof(pathListingDate));
        DWORD pathListingStartTime = 0;
        BOOL ret = ControlConnection->ChangeWorkingPath(TRUE, FALSE, SalamanderGeneral->GetMsgBoxParent(),
                                                        Path, FTP_MAX_PATH, User, USER_MAX_SIZE,
                                                        parsedPath, forceRefresh, mode, FALSE,
                                                        cutFileName, pathWasCut, RescuePath, TRUE,
                                                        &pathListing, &pathListingLen, &pathListingDate,
                                                        &pathListingStartTime, &TotalConnectAttemptNum, TRUE);

        if (pathListing != NULL) // the listing was in the cache
        {
            OverwritePathListing = FALSE;
            if (PathListing != NULL)
            {
                memset(PathListing, 0, PathListingLen); // might contain confidential data, better to zero it
                free(PathListing);                      // release the old listing
            }
            PathListing = pathListing;
            PathListingLen = pathListingLen;
            PathListingDate = pathListingDate;
            PathListingIsIncomplete = FALSE; // only complete listings are stored in the cache
            PathListingIsBroken = FALSE;     // only intact listings are stored in the cache
            PathListingMayBeOutdated = FALSE;
            PathListingStartTime = pathListingStartTime;
        }

        if (ret && TargetPanelPath[0] != 0)
        {
            TargetPanelPathPanel = SalamanderGeneral->GetSourcePanel();
            TargetPanelPathPanel = (TargetPanelPathPanel == PANEL_RIGHT) ? PANEL_LEFT : PANEL_RIGHT;
            SalamanderGeneral->PostMenuExtCommand(FTPCMD_CHANGETGTPANELPATH, TRUE); // send later in "idle"
        }
        if (!ret)
            TargetPanelPath[0] = 0;              // the connection failed, no path change in the target panel
        ControlConnection->ActivateWelcomeMsg(); // if any message box deactivated the welcome-msg window, activate it again
        return ret;
    }
    else // path change
    {
        ControlConnection->DetachWelcomeMsg(); // the welcome-msg window will no longer be activated (the user likely switched to the panel)

        BOOL skipFirstReconnectIfNeeded = TRUE;
        int backupTotalConnectAttemptNum = TotalConnectAttemptNum;
        if (lastErrorState == fesOK) // on the first call (within a single path change and if it is not a connect)
        {                            // no rescue path - Salamander itself restores the original panel path on failure
            RescuePath[0] = 0;
            TotalConnectAttemptNum = 1;         // starting the path change = potential first attempt to open the connection
            skipFirstReconnectIfNeeded = FALSE; // nothing to resume on the first call -> connection test required
        }

        BOOL ret = TRUE;
        if (!NextRefreshCanUseOldListing || PathListing == NULL /* always false */)
        {
            NextRefreshCanUseOldListing = FALSE;

            // retrieve the new path
            lstrcpyn(newUserPart, userPart, FTP_USERPART_SIZE);
            char* path;
            char firstCharOfPath;
            FTPSplitPath(newUserPart, NULL, NULL, NULL, NULL, &path, &firstCharOfPath, FTPGetUserLength(User));
            if (path != NULL)
            {
                Path[0] = firstCharOfPath;
                lstrcpyn(Path + 1, path, FTP_MAX_PATH - 1);
            }
            else
                Path[0] = 0;
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // erase the memory that contained the password

            if (IsDetached && !ControlConnection->IsConnected())
            { // reconnecting a detached FS with a closed control connection - inform the user about what was
                // most likely written to the log already (in the "quiet" mode of CheckCtrlConClose())
                ControlConnection->CheckCtrlConClose(TRUE, FALSE /* leftPanel - ignored */,
                                                     SalamanderGeneral->GetMsgBoxParent(), FALSE);
            }

            BOOL showChangeInLog = TRUE;
            if (lastErrorState == fesOK && // on the first call (within a single path change and if it is not a connect)
                GetTickCount() - ChangePathOnlyGetCurPathTime <= 1000)
            { // less than a second has passed since obtaining the working path - optimize ChangeWorkingPath()
                ChangePathOnlyGetCurPathTime = 0;
                Path[0] = 0;                                           // the next ChangeWorkingPath() call only pulls the working path from cache (no further path change)
                showChangeInLog = FALSE;                               // the log message was already shown in GetFullFSPath()
                skipFirstReconnectIfNeeded = TRUE;                     // continue from the previous control connection work
                TotalConnectAttemptNum = backupTotalConnectAttemptNum; // continue with the previous reconnect count
            }
            else
            {
                BOOL old = SalamanderGeneral->GetMainWindowHWND() != GetForegroundWindow(); // if the main window is inactive, we must show wait windows; otherwise it cannot be activated and another application could gain focus after the operation window closes automatically
                ControlConnection->SetStartTime(old);                                       // set the time only when it is the start of the operation (and not a continuation of GetFullFSPath())
            }

            int panel;
            BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
            char* pathListing = NULL;
            int pathListingLen = 0;
            CFTPDate pathListingDate;
            memset(&pathListingDate, 0, sizeof(pathListingDate));
            DWORD pathListingStartTime = 0;
            ret = ControlConnection->ChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                       SalamanderGeneral->GetMsgBoxParent(), Path,
                                                       FTP_MAX_PATH, User, USER_MAX_SIZE, TRUE,
                                                       forceRefresh, mode,
                                                       lastErrorState == fesInaccessiblePath, /* if it cannot be listed, shorten it */
                                                       lastErrorState == fesInaccessiblePath ? NULL : cutFileName,
                                                       pathWasCut, RescuePath, showChangeInLog,
                                                       &pathListing, &pathListingLen, &pathListingDate,
                                                       &pathListingStartTime, &TotalConnectAttemptNum,
                                                       skipFirstReconnectIfNeeded);
            if (pathListing != NULL) // the listing was in the cache
            {
                OverwritePathListing = FALSE;
                if (PathListing != NULL)
                {
                    memset(PathListing, 0, PathListingLen); // might contain confidential data, better to zero it
                    free(PathListing);                      // release the old listing
                }
                PathListing = pathListing;
                PathListingLen = pathListingLen;
                PathListingDate = pathListingDate;
                PathListingIsIncomplete = FALSE; // only complete listings are stored in the cache
                PathListingIsBroken = FALSE;     // only intact listings are stored in the cache
                PathListingMayBeOutdated = FALSE;
                PathListingStartTime = pathListingStartTime;
            }
            if (!ret)
                TargetPanelPath[0] = 0; // the connection failed, no path change in the target panel
        }
        else // when refreshing because of server-type or column configuration changes we stay on the same path with the same listing text (even if it is NULL)
        {
            ControlConnection->SetStartTime(); // set the time for listing
            OverwritePathListing = FALSE;
        }
        return ret;
    }
}

void CPluginFSInterface::AddUpdir(BOOL& err, BOOL& needUpDir, CFTPListingPluginDataInterface* dataIface,
                                  TIndirectArray<CSrvTypeColumn>* columns, CSalamanderDirectoryAbstract* dir)
{
    needUpDir = FALSE;
    CFileData updir;
    memset(&updir, 0, sizeof(updir));
    if (dataIface->AllocPluginData(updir))
    {
        updir.Name = SalamanderGeneral->DupStr("..");
        if (updir.Name != NULL)
        {
            updir.NameLen = strlen(updir.Name);
            updir.Ext = updir.Name + updir.NameLen;

            // fill empty values into empty columns
            FillEmptyValues(err, &updir, TRUE, dataIface, columns, NULL, NULL, 0, 0, 0);

            // add it to 'dir'
            if (!err && !dir->AddDir(NULL, updir, NULL))
                err = TRUE;
            if (err)
            {
                // release the up-dir data
                dataIface->ReleasePluginData(updir, TRUE);
                SalamanderGeneral->Free(updir.Name);
            }
        }
        else
            err = TRUE; // low memory
    }
    else
        err = TRUE; // low memory
}

BOOL CPluginFSInterface::ParseListing(CSalamanderDirectoryAbstract* dir,
                                      CPluginDataInterfaceAbstract** pluginData,
                                      CServerType* serverType, BOOL* lowMem, BOOL isVMS,
                                      const char* findName, BOOL caseSensitive,
                                      BOOL* fileExists, BOOL* dirExists)
{
    BOOL ret = FALSE;
    *lowMem = FALSE;
    if (pluginData != NULL)
        *pluginData = NULL;
    if (findName != NULL)
    {
        if (fileExists != NULL)
            *fileExists = FALSE;
        if (dirExists != NULL)
            *dirExists = FALSE;
    }

    TIndirectArray<CSrvTypeColumn>* columns = new TIndirectArray<CSrvTypeColumn>(5, 5);
    DWORD validDataMask = VALID_DATA_HIDDEN | VALID_DATA_ISLINK; // Name + NameLen + Hidden + IsLink
    BOOL err = FALSE;
    if (columns != NULL && columns->IsGood())
    {
        // Create a local copy of the column data (the server type is not locked for the whole lifetime of the listing in the panel)
        int i;
        for (i = 0; i < serverType->Columns.Count; i++)
        {
            CSrvTypeColumn* c = serverType->Columns[i]->MakeCopy();
            if (c != NULL)
            {
                switch (c->Type)
                {
                case stctExt:
                    validDataMask |= VALID_DATA_EXTENSION;
                    break;
                case stctSize:
                    validDataMask |= VALID_DATA_SIZE;
                    break;
                case stctDate:
                    validDataMask |= VALID_DATA_DATE;
                    break;
                case stctTime:
                    validDataMask |= VALID_DATA_TIME;
                    break;
                case stctType:
                    validDataMask |= VALID_DATA_TYPE;
                    break;
                }
                columns->Add(c);
                if (!columns->IsGood())
                {
                    err = TRUE;
                    delete c;
                    columns->ResetState();
                    break;
                }
            }
            else
            {
                err = TRUE;
                break;
            }
        }

        if (!err) // we have a copy of the columns
        {
            CFTPListingPluginDataInterface* dataIface = new CFTPListingPluginDataInterface(columns, TRUE,
                                                                                           validDataMask, isVMS);
            if (dataIface != NULL)
            {
                validDataMask |= dataIface->GetPLValidDataMask();
                DWORD* emptyCol = new DWORD[columns->Count]; // helper pre-allocated array for GetNextItemFromListing
                if (dataIface->IsGood() && emptyCol != NULL)
                {
                    CFTPParser* parser = serverType->CompiledParser;
                    if (parser == NULL)
                    {
                        parser = CompileParsingRules(HandleNULLStr(serverType->RulesForParsing), columns,
                                                     NULL, NULL, NULL);
                        serverType->CompiledParser = parser; // do not deallocate 'parser'; it is now in 'serverType'
                    }
                    if (parser != NULL)
                    {
                        BOOL needUpDir = FTPIsValidAndNotRootPath(GetFTPServerPathType(Path), Path); // TRUE while an up-dir ("..") still needs to be inserted

                        CFileData file;
                        const char* listing = PathListing;
                        const char* listingEnd = PathListing + (PathListingIsBroken ? 0 /* if the listing is not OK, use an empty listing instead */ : PathListingLen);
                        BOOL isDir = FALSE;

                        if (dir != NULL)
                        {
                            dir->SetValidData(validDataMask);
                            dir->SetFlags(SALDIRFLAG_CASESENSITIVE | SALDIRFLAG_IGNOREDUPDIRS); // probably unnecessary, but everything is treated as case-sensitive so this should be safe
                        }
                        parser->BeforeParsing(listing, listingEnd, PathListingDate.Year, PathListingDate.Month,
                                              PathListingDate.Day, PathListingIsIncomplete); // initialize the parser
                        while (parser->GetNextItemFromListing(&file, &isDir, dataIface, columns, &listing,
                                                              listingEnd, NULL, &err, emptyCol))
                        {
                            BOOL dealloc = TRUE;
                            if (isDir && file.NameLen <= 2 &&
                                file.Name[0] == '.' && (file.Name[1] == 0 || file.Name[1] == '.')) // directories "." and ".."
                            {
                                if (needUpDir && file.Name[1] == '.') // directory ".."
                                {
                                    file.Hidden = FALSE;
                                    needUpDir = FALSE;
                                    if (dir != NULL)
                                    {
                                        if (dir->AddDir(NULL, file, NULL))
                                            dealloc = FALSE;
                                        else
                                            err = TRUE;
                                    }
                                }
                            }
                            else // add a normal file/directory to 'dir'
                            {
                                if (findName != NULL)
                                {
                                    if (caseSensitive && strcmp(findName, file.Name) == 0 ||
                                        !caseSensitive && SalamanderGeneral->StrICmp(findName, file.Name) == 0)
                                    {
                                        if (isDir)
                                        {
                                            if (dirExists != NULL)
                                                *dirExists = TRUE;
                                        }
                                        else
                                        {
                                            if (fileExists != NULL)
                                                *fileExists = TRUE;
                                        }
                                    }
                                }

                                if (dir != NULL)
                                {
                                    if (isDir)
                                    {
                                        // add a directory to 'dir'
                                        if (dir->AddDir(NULL, file, NULL))
                                            dealloc = FALSE;
                                        else
                                            err = TRUE;
                                    }
                                    else
                                    {
                                        // add a file to 'dir'
                                        if (dir->AddFile(NULL, file, NULL))
                                            dealloc = FALSE;
                                        else
                                            err = TRUE;
                                    }
                                }
                            }
                            if (dealloc)
                            {
                                // release the file or directory data
                                dataIface->ReleasePluginData(file, isDir);
                                SalamanderGeneral->Free(file.Name);
                            }
                            if (err)
                                break;
                        }
                        if (needUpDir && dir != NULL) // still need to insert the up-dir ("..")
                            AddUpdir(err, needUpDir, dataIface, columns, dir);
                        if (!err && listing == listingEnd) // parsing finished successfully
                        {
                            if (pluginData != NULL)
                            {
                                *pluginData = dataIface;
                                dataIface = NULL; // will leave as the return value
                            }
                            ret = TRUE; // success
                        }
                        else
                        {
                            if (dir != NULL)
                                dir->Clear(dataIface); // otherwise release all allocated data
                        }
                    }
                    else
                        err = TRUE; // can only be a lack of memory
                }
                else
                {
                    if (emptyCol == NULL)
                        TRACE_E(LOW_MEMORY);
                    err = TRUE; // low memory
                }
                if (emptyCol != NULL)
                    delete[] emptyCol;
                columns = NULL; // they now live in 'dataIface'
                if (dataIface != NULL)
                    delete dataIface;
            }
            else
            {
                TRACE_E(LOW_MEMORY);
                err = TRUE; // low memory
            }
        }
    }
    else
    {
        if (columns == NULL)
            TRACE_E(LOW_MEMORY);
        err = TRUE; // low memory
    }
    *lowMem = err;
    if (columns != NULL)
        delete columns;
    return ret;
}

BOOL CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                         CPluginDataInterfaceAbstract*& pluginData,
                                         int& iconsType, BOOL forceRefresh)
{
    if (ControlConnection == NULL) // "always false"
    {
        TRACE_E("Unexpected situation in CPluginFSInterface::ListCurrentPath().");
        ErrorState = fesFatal;
        return FALSE; // fatal error
    }

    if (OverwritePathListing) // we have an old listing, free it (it must be overwritten by a new one)
    {
        OverwritePathListing = FALSE;
        if (PathListing != NULL)
        {
            memset(PathListing, 0, PathListingLen); // might contain confidential data, better to zero it
            free(PathListing);
        }
        PathListing = NULL;
        PathListingLen = 0;
        memset(&PathListingDate, 0, sizeof(PathListingDate));
        PathListingIsIncomplete = FALSE;
        PathListingIsBroken = FALSE;
        PathListingMayBeOutdated = FALSE;
        PathListingStartTime = 0;
    }

    char logBuf[200 + FTP_MAX_PATH];
    _snprintf_s(logBuf, _TRUNCATE, LoadStr(PathListing != NULL ? IDS_LOGMSGLISTINGCACHEDPATH : IDS_LOGMSGLISTINGPATH), Path);
    ControlConnection->LogMessage(logBuf, -1, TRUE);

    //  if (PathListing != NULL && forceRefresh && !NextRefreshCanUseOldListing)  // may happen when the user requests a hard refresh and refuses to reconnect - then the cached listing is used (if available)
    //    TRACE_E("Unexpected situation in CPluginFSInterface::ListCurrentPath() - cached refresh!");
    BOOL fatalError = FALSE;
    BOOL listingIsNotFromCache = PathListing == NULL;
    if (PathListing != NULL || // process the cached listing; otherwise list on the server...
        ControlConnection->ListWorkingPath(SalamanderGeneral->GetMsgBoxParent(),
                                           Path, User, USER_MAX_SIZE, &PathListing,
                                           &PathListingLen, &PathListingDate,
                                           &PathListingIsIncomplete, &PathListingIsBroken,
                                           &PathListingMayBeOutdated, &PathListingStartTime,
                                           forceRefresh, &TotalConnectAttemptNum, &fatalError,
                                           NextRefreshWontClearCache))
    {
        NextRefreshCanUseOldListing = FALSE;
        NextRefreshWontClearCache = FALSE;
        ControlConnection->ActivateWelcomeMsg(); // if any message box deactivated the welcome-msg window, activate it again
        if (PathListing != NULL)                 // we have at least part of the listing, go parse it
        {
            CFTPServerPathType pathType = GetFTPServerPathType(Path);
            BOOL isVMS = pathType == ftpsptOpenVMS; // find out whether this might be a VMS listing

            BOOL needSimpleListing = TRUE;
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            // so the user does not stare at the remains of the list-wait window during parsing
            UpdateWindow(SalamanderGeneral->GetMsgBoxParent());

            // after 3 seconds show the wait window "parsing listing, please wait..."
            SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAITWNDPARSINGLST), LoadStr(IDS_FTPPLUGINTITLE),
                                                    WAITWND_PARSINGLST, FALSE,
                                                    SalamanderGeneral->GetMsgBoxParent());

            char* welcomeReply = ControlConnection->AllocServerFirstReply();
            char* systReply = ControlConnection->AllocServerSystemReply();

            if (listingIsNotFromCache && !PathListingIsIncomplete && !PathListingIsBroken &&
                welcomeReply != NULL && systReply != NULL)
            { // update the cached listing uploaded earlier (replace the approximate listing with the real one)
                UploadListingCache.AddOrUpdateListing(User, Host, Port, Path, pathType,
                                                      PathListing, PathListingLen,
                                                      PathListingDate, PathListingStartTime,
                                                      TRUE /* just update the listing */, welcomeReply, systReply,
                                                      AutodetectSrvType ? NULL : LastServerType);
            }

            // Reset the helper variable indicating which server type has already been (unsuccessfully) tested
            CServerTypeList* serverTypeList = Config.LockServerTypeList();
            int serverTypeListCount = serverTypeList->Count;
            int j;
            for (j = 0; j < serverTypeListCount; j++)
                serverTypeList->At(j)->ParserAlreadyTested = FALSE;

            // look for LastServerType
            CServerType* serverType = NULL;
            BOOL err = FALSE;
            if (LastServerType[0] != 0)
            {
                int i;
                for (i = 0; i < serverTypeListCount; i++)
                {
                    serverType = serverTypeList->At(i);
                    const char* s = serverType->TypeName;
                    if (*s == '*')
                        s++;
                    if (SalamanderGeneral->StrICmp(LastServerType, s) == 0)
                    {
                        // serverType is selected, try its parser on the listing
                        serverType->ParserAlreadyTested = TRUE;
                        if (ParseListing(dir, &pluginData, serverType, &err, isVMS, NULL, FALSE, NULL, NULL))
                        {
                            needSimpleListing = FALSE; // successfully parsed the listing
                        }
                        break; // found the desired server type, stop
                    }
                }
                if (i == serverTypeListCount) // LastServerType does not exist -> fall back to autodetection
                {
                    AutodetectSrvType = TRUE;
                    LastServerType[0] = 0;
                }
            }
            else
                AutodetectSrvType = TRUE; // probably redundant, just to be safe...

            // Autodetection - choose the server types whose autodetection condition is satisfied
            if (!err && needSimpleListing && AutodetectSrvType)
            {
                if (welcomeReply == NULL || systReply == NULL)
                    err = TRUE;
                else
                {
                    int welcomeReplyLen = (int)strlen(welcomeReply);
                    int systReplyLen = (int)strlen(systReply);
                    int i;
                    for (i = 0; i < serverTypeListCount; i++)
                    {
                        serverType = serverTypeList->At(i);
                        if (!serverType->ParserAlreadyTested) // only if we have not tried it yet
                        {
                            if (serverType->CompiledAutodetCond == NULL)
                            {
                                serverType->CompiledAutodetCond = CompileAutodetectCond(HandleNULLStr(serverType->AutodetectCond),
                                                                                        NULL, NULL, NULL, NULL, 0);
                                if (serverType->CompiledAutodetCond == NULL) // can only fail due to lack of memory
                                {
                                    err = TRUE;
                                    break;
                                }
                            }
                            if (serverType->CompiledAutodetCond->Evaluate(welcomeReply, welcomeReplyLen,
                                                                          systReply, systReplyLen))
                            {
                                // serverType is selected, try its parser on the listing
                                serverType->ParserAlreadyTested = TRUE;
                                if (ParseListing(dir, &pluginData, serverType, &err, isVMS, NULL, FALSE, NULL, NULL) || err)
                                {
                                    if (!err)
                                    {
                                        const char* s = serverType->TypeName;
                                        if (*s == '*')
                                            s++;
                                        lstrcpyn(LastServerType, s, SERVERTYPE_MAX_SIZE);
                                    }
                                    needSimpleListing = err; // successfully parsed the listing or ran into low memory, stop
                                    break;
                                }
                            }
                        }
                    }
                }

                // Autodetection - pick the remaining server types
                if (!err && needSimpleListing)
                {
                    int i;
                    for (i = 0; i < serverTypeListCount; i++)
                    {
                        serverType = serverTypeList->At(i);
                        if (!serverType->ParserAlreadyTested) // only if we have not tried it yet
                        {
                            // serverType is selected, try its parser on the listing
                            // serverType->ParserAlreadyTested = TRUE;  // unnecessary, not used afterwards
                            if (ParseListing(dir, &pluginData, serverType, &err, isVMS, NULL, FALSE, NULL, NULL) || err)
                            {
                                if (!err)
                                {
                                    const char* s = serverType->TypeName;
                                    if (*s == '*')
                                        s++;
                                    lstrcpyn(LastServerType, s, SERVERTYPE_MAX_SIZE);
                                }
                                needSimpleListing = err; // successfully parsed the listing or ran into low memory, stop
                                break;
                            }
                        }
                    }
                }
            }
            Config.UnlockServerTypeList();

            if (welcomeReply != NULL)
                SalamanderGeneral->Free(welcomeReply);
            if (systReply != NULL)
                SalamanderGeneral->Free(systReply);

            if (!err)
            {
                if (needSimpleListing) // unknown listing; show a message about sending the information to ALTAP
                {                      // and log "Unknown Server Type"
                    lstrcpyn(logBuf, LoadStr(AutodetectSrvType ? IDS_LOGMSGUNKNOWNSRVTYPE : IDS_LOGMSGUNKNOWNSRVTYPE2),
                             200 + FTP_MAX_PATH);
                    ControlConnection->LogMessage(logBuf, -1, TRUE);
                    if (InformAboutUnknownSrvType)
                    {
                        SalamanderGeneral->ShowMessageBox(LoadStr(AutodetectSrvType ? IDS_UNKNOWNSRVTYPEINFO : IDS_UNKNOWNSRVTYPEINFO2),
                                                          LoadStr(IDS_FTPPLUGINTITLE), MSGBOX_INFO);
                        InformAboutUnknownSrvType = FALSE;
                    }
                }
                else // log which parser handled it
                {
                    if (LastServerType[0] != 0) // "always true"
                    {
                        _snprintf_s(logBuf, _TRUNCATE, LoadStr(IDS_LOGMSGPARSEDBYSRVTYPE), LastServerType);
                        ControlConnection->LogMessage(logBuf, -1, TRUE);
                    }
                }
            }

            if (!err && needSimpleListing)
            {
                CFileData file;
                pluginData = &SimpleListPluginDataInterface; // ATTENTION: the change may also affect obtaining the data interface in CPluginFSInterface::ChangeAttributes!
                dir->SetValidData(VALID_DATA_NONE);
                dir->SetFlags(SALDIRFLAG_CASESENSITIVE | SALDIRFLAG_IGNOREDUPDIRS); // probably unnecessary, but everything is treated as case-sensitive so this should be safe
                if (!PathListingIsBroken &&                                         // if the listing is not OK, rather use an empty listing
                    PathListingLen > 0)
                {
                    char* beg = PathListing;
                    char* end = beg + PathListingLen;
                    char* s = beg;
                    int lines = 0;
                    while (s < end)
                    {
                        while (s < end && *s != '\r' && *s != '\n')
                            s++; // look for the end of the line
                        lines++;
                        if (s < end && *s == '\r')
                            s++;
                        if (s < end && *s == '\n')
                            s++;
                    }
                    char buf[30];
                    sprintf(buf, "%d", lines);
                    int width = (int)strlen(buf);

                    int line = 1;
                    s = beg;
                    while (beg < end)
                    {
                        while (s < end && *s != '\r' && *s != '\n')
                            s++; // look for the end of the line

                        // generate the line number for the listing output
                        sprintf(buf, "%0*d", width, line);
                        line++;
                        file.NameLen = strlen(buf);
                        file.Name = (char*)malloc(file.NameLen + 1);
                        if (file.Name == NULL)
                        {
                            err = TRUE;
                            break;
                        }
                        strcpy(file.Name, buf);

                        // process the line ('beg' to 's')
                        char* row = (char*)malloc(s - beg + 1);
                        if (row == NULL)
                        {
                            free(file.Name);
                            err = TRUE;
                            break;
                        }
                        memcpy(row, beg, s - beg);
                        row[s - beg] = 0;
                        file.PluginData = (DWORD_PTR)row;
                        if (!dir->AddFile(NULL, file, NULL))
                        {
                            SalamanderGeneral->Free(file.Name);
                            free(row);
                            err = TRUE;
                            break;
                        }

                        // move to the start of the next line
                        if (s < end && *s == '\r')
                            s++;
                        if (s < end && *s == '\n')
                            s++;
                        beg = s;
                    }
                }

                if (!err && FTPIsValidAndNotRootPath(pathType, Path))
                {
                    file.Name = SalamanderGeneral->DupStr("..");
                    if (file.Name == NULL)
                        err = TRUE;
                    else
                    {
                        file.NameLen = strlen(file.Name);
                        file.PluginData = NULL;
                        if (!dir->AddDir(NULL, file, NULL))
                        {
                            SalamanderGeneral->Free(file.Name);
                            err = TRUE;
                        }
                    }
                }
                if (err)
                    dir->Clear(pluginData);
                iconsType = pitSimple;
            }
            else
                iconsType = pitFromRegistry;

            if (err)
                ErrorState = fesFatal;

            // hide the wait window "parsing listing, please wait..."
            SalamanderGeneral->DestroySafeWaitWindow();

            SetCursor(oldCur);
            ControlConnection->ActivateWelcomeMsg(); // if any message box deactivated the welcome-msg window, activate it again
            return !err;
        }
        else
            ErrorState = fesFatal; // low memory, already reported to trace
    }
    else
        ErrorState = fatalError ? fesFatal : fesInaccessiblePath; // the server reports a fatal error or simply that the path cannot be listed
    NextRefreshWontClearCache = FALSE;
    ControlConnection->ActivateWelcomeMsg(); // if any message box deactivated the welcome-msg window, activate it again
    return FALSE;
}
