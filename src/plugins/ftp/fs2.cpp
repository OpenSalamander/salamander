// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

int CSimpleListPluginDataInterface::ListingColumnWidth = 0;      // LO/HI-WORD: levy/pravy panel: sirka sloupce Raw Listing
int CSimpleListPluginDataInterface::ListingColumnFixedWidth = 0; // LO/HI-WORD: levy/pravy panel: ma sloupec Raw Listing pevnou sirku?

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
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
        memset(PathListing, 0, PathListingLen); // muze jit o tajna data, radsi nulujeme
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
    if (strcmp(User, FTP_ANONYMOUS) != 0) // anonymniho uzivatele ignorujeme
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
    if (Port != IPPORT_FTP) // standardni port FTP (port IPPORT_FTP) ignorujeme
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
            *s++ = slash; // aspon root cesta tam bude vzdycky
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
        return FALSE; // preklad neni mozny (FS jeste nebylo pripojene), at si chybu ohlasi sam Salamander

    char errBuf[900 + FTP_MAX_PATH];
    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_LOGMSGCHANGINGPATH), path);
    ControlConnection->LogMessage(errBuf, -1, TRUE);

    int panel;
    BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
    char replyBuf[700];
    TotalConnectAttemptNum = 1; // zahajeni uzivatelem pozadovane akce -> je-li treba znovu pripojit, jde o 1. pokus reconnect
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
                lstrcpyn(path, replyBuf, pathSize); // uspesne jsme ziskali novou cestu na serveru
            }
            else
            {
                if (canRetry) // "retry" je povolen
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
            ChangePathOnlyGetCurPathTime = GetTickCount(); // pro optimalizaci ChangePath() volane tesne po ziskani pracovni cesty
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
    MakeUserPart(buf, bufSize); // pokud se nevejde cesta, jmeno se urcite take nevejde (ohlasi chybu)
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
        return FALSE; // nevejde se, nelze porovnat
}

BOOL CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    if (Config.UseConnectionDataFromConfig)
    { // uzivatel otevira novou connectionu z Connect dialogu - musime vratit FALSE (uzivatel muze
        // pozadovat druhy connect na stejny server - situace, kdy by se pri change-path vracelo TRUE)
        return FALSE;
    }

    if (currentFSNameIndex != fsNameIndex)
        return FALSE; // nelze michat FTP a FTPS

    char actUserPart[FTP_USERPART_SIZE];
    if (MakeUserPart(actUserPart, FTP_USERPART_SIZE))
    {
        return FTPHasTheSameRootPath(actUserPart, userPart, FTPGetUserLength(User));
    }
    else
        return FALSE; // nevejde se, nelze porovnat
}

void CPluginFSInterface::ClearHostFromListingCacheIfFirstCon(const char* host, int port, const char* user)
{
    int i;
    for (i = 0; i < FTPConnections.Count; i++) // zkusime najit FS s danou kombinaci host+port+user
    {
        CPluginFSInterface* fs = FTPConnections[i];
        if (fs != this && fs->ContainsHost(host, port, user))
            return; // nasli jsme, cache se nebude promazavat
    }

    // vycisti z cache listingy vsechny cest s kombinaci host+port+user
    ListingCache.RefreshOnPath(host, port, user, ftpsptEmpty, "", TRUE);

    // smazeme soubory z diskcache (FTP i FTPS)
    char path[FTP_USERPART_SIZE + 50]; // +50 je rezerva pro FS name
    strcpy(path, AssignedFSName);      // sestavime jmeno pro FTP
    strcat(path, ":");
    if (MakeUserPart(path + AssignedFSNameLen + 1, FTP_USERPART_SIZE + 49 - (AssignedFSNameLen + 1), NULL, TRUE))
    {
        char* end = path + strlen(path);
        strcpy(end, "/");
        SalamanderGeneral->RemoveFilesFromCache(path); // za ftp://user@host:port je vzdy / nebo \, promazneme obe varianty
        strcpy(end, "\\");
        SalamanderGeneral->RemoveFilesFromCache(path);
        *end = 0;

        if (strlen(path) - AssignedFSNameLen + AssignedFSNameLenFTPS < FTP_USERPART_SIZE + 49)
        {
            char path2[FTP_USERPART_SIZE + 50]; // +50 je rezerva pro FS name
            strcpy(path2, AssignedFSNameFTPS);  // sestavime jmeno pro FTPS
            strcpy(path2 + AssignedFSNameLenFTPS, path + AssignedFSNameLen);
            end = path2 + strlen(path2);
            strcpy(end, "/");
            SalamanderGeneral->RemoveFilesFromCache(path2); // za ftps://user@host:port je vzdy / nebo \, promazneme obe varianty
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
    ErrorState = fesOK; // pro pristi volani jsme zase OK

    if (lastErrorState == fesFatal)
    {
        TargetPanelPath[0] = 0; // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
        return FALSE;           // fatal error, koncime
    }

    // tvrdy refresh bereme jako projev neduvery k ceste, zahodime z disk-cache vsechny soubory
    // downloadle pro View (vcetne souboru z podcest a to jak FTP, tak FTPS)
    if (mode == 1 && forceRefresh && !NextRefreshWontClearCache)
    {
        char uniqueFileName[FTP_USERPART_SIZE + 50]; // +50 je rezerva pro FS name; jmena v cache jsou case-sensitive
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
    if (ControlConnection == NULL) // otevreni spojeni (otevreni cesty na FTP server)
    {
        TargetPanelPath[0] = 0;
        TotalConnectAttemptNum = 1; // otevirame spojeni = 1. pokus o otevreni spojeni
        InformAboutUnknownSrvType = TRUE;

        BOOL parsedPath = TRUE; // TRUE = cesta ziskana z user-part, je potreba zjistit jestli se ma ze zacatku oriznout '/' nebo '\\'
        ControlConnection = new CControlConnectionSocket;
        if (ControlConnection == NULL || !ControlConnection->IsGood())
        {
            if (ControlConnection != NULL) // nedostatek systemovych prostredku pro alokaci objektu
            {
                DeleteSocket(ControlConnection);
                ControlConnection = NULL;
            }
            else
                TRACE_E(LOW_MEMORY);
            TargetPanelPath[0] = 0; // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
            return FALSE;           // fatal error
        }

        char anonymousPasswd[PASSWORD_MAX_SIZE];
        Config.GetAnonymousPasswd(anonymousPasswd, PASSWORD_MAX_SIZE);
        if (*userPart == 0 && Config.UseConnectionDataFromConfig) // data z dialogu Connect
        {
            // vytahneme data o vybranem serveru z dialogu Connect z konfigurace
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
                    TargetPanelPath[0] = 0; // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
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
            parsedPath = FALSE; // cesta zadana userem (ze zacatku se urcite neorezava '/' ani '\\')

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
                if (server->KeepConnectionAlive == 1) // vlastni hodnoty
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
                    memset(plainPassword, 0, lstrlen(plainPassword)); // vynulujeme buffer s heslem
                    SalamanderGeneral->Free(plainPassword);
                }
                else
                    password[0] = 0;
            }

            // osetreni rozsifrovatelnosti pripadneho proxy hesla se provadi v connect dialogu
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
            memset(password, 0, lstrlen(password)); // vynulujeme buffer s heslem
        }
        else // pripojeni na zaklade zmeny cesty na FTP file-system (napr. Shift+F7 + "ftp://ftp.altap.cz/")
        {
            if (Config.UseConnectionDataFromConfig)
                TRACE_E("Unexpected situation in CPluginFSInterface::ChangePath() - UseConnectionDataFromConfig + nonempty userpart.");

            int encControlAndDataConn = currentFSNameIndex == AssignedFSNameIndexFTPS;

            // overime, zda bude mozne rozsifrovat pripadne heslo pro default proxy (do SetConnectionParameters() smime vstoupit pouze v pripade, ze je to mozne)
            if (!Config.FTPProxyServerList.EnsurePasswordCanBeDecrypted(SalamanderGeneral->GetMsgBoxParent(), Config.DefaultProxySrvUID))
            {
                TargetPanelPath[0] = 0; // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
                return FALSE;           // fatal error
            }

            AutodetectSrvType = TRUE; // pouzivame automatickou detekci typu serveru
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
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
                TargetPanelPath[0] = 0;                        // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
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
                    t++; // kontrola jestli jde o cislo
                port = atoi(p);
                if (*t != 0 || port < 1 || port > 65535) // invalidni cislo portu
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_INVALIDPORT),
                                                      LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                    memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
                    TargetPanelPath[0] = 0;                        // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
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

            // password - neni-li NULL, obsahuje heslo pro pripojeni
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
        }

        ControlConnection->SetStartTime();
        if (!ControlConnection->StartControlConnection(SalamanderGeneral->GetMsgBoxParent(),
                                                       User, USER_MAX_SIZE, FALSE, RescuePath,
                                                       FTP_MAX_PATH, &TotalConnectAttemptNum,
                                                       NULL, TRUE, -1, FALSE))
        {                                            // nepodarilo se pripojit, uvolnime objekt socketu (signalizuje stav "nikdy nebylo pripojeno")
            ControlConnection->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
            DeleteSocket(ControlConnection);
            ControlConnection = NULL;
            Logs.RefreshListOfLogsInLogsDlg();
            TargetPanelPath[0] = 0; // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
            return FALSE;
        }
        lstrcpyn(HomeDir, RescuePath, FTP_MAX_PATH); // ulozime si aktualni cestu po loginu na server (home-dir)
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

        if (pathListing != NULL) // listing byl v cache
        {
            OverwritePathListing = FALSE;
            if (PathListing != NULL)
            {
                memset(PathListing, 0, PathListingLen); // muze jit o tajna data, radsi nulujeme
                free(PathListing);                      // uvolnime stary listing
            }
            PathListing = pathListing;
            PathListingLen = pathListingLen;
            PathListingDate = pathListingDate;
            PathListingIsIncomplete = FALSE; // v cache jsou jen kompletni listingy
            PathListingIsBroken = FALSE;     // v cache jsou jen neporusene listingy
            PathListingMayBeOutdated = FALSE;
            PathListingStartTime = pathListingStartTime;
        }

        if (ret && TargetPanelPath[0] != 0)
        {
            TargetPanelPathPanel = SalamanderGeneral->GetSourcePanel();
            TargetPanelPathPanel = (TargetPanelPathPanel == PANEL_RIGHT) ? PANEL_LEFT : PANEL_RIGHT;
            SalamanderGeneral->PostMenuExtCommand(FTPCMD_CHANGETGTPANELPATH, TRUE); // posleme az v "idle"
        }
        if (!ret)
            TargetPanelPath[0] = 0;              // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
        ControlConnection->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
        return ret;
    }
    else // zmena cesty
    {
        ControlConnection->DetachWelcomeMsg(); // dale uz nebude dochazet k aktivacim welcome-msg okna (user uz se asi prepnul to panelu)

        BOOL skipFirstReconnectIfNeeded = TRUE;
        int backupTotalConnectAttemptNum = TotalConnectAttemptNum;
        if (lastErrorState == fesOK) // pri prvnim volani (v ramci jedne zmeny cesty + pokud nejde o connect)
        {                            // zadna zachrana cesta - Salamander pri neuspechu zajisti navrat na puvodni cestu v panelu sam
            RescuePath[0] = 0;
            TotalConnectAttemptNum = 1;         // zahajeni zmeny cesty = pripadny 1. pokus o otevreni spojeni
            skipFirstReconnectIfNeeded = FALSE; // pri prvnim volani neni na co navazovat -> test spojeni nutny
        }

        BOOL ret = TRUE;
        if (!NextRefreshCanUseOldListing || PathListing == NULL /* always false */)
        {
            NextRefreshCanUseOldListing = FALSE;

            // vytahneme novou cestu
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
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password

            if (IsDetached && !ControlConnection->IsConnected())
            { // pripojeni odpojeneho FS se zavrenou "control connection" - hlasime userovi co uz
                // jsme nejspis napsali do logu (v "quiet" modu CheckCtrlConClose())
                ControlConnection->CheckCtrlConClose(TRUE, FALSE /* leftPanel - ignored */,
                                                     SalamanderGeneral->GetMsgBoxParent(), FALSE);
            }

            BOOL showChangeInLog = TRUE;
            if (lastErrorState == fesOK && // pri prvnim volani (v ramci jedne zmeny cesty + pokud nejde o connect)
                GetTickCount() - ChangePathOnlyGetCurPathTime <= 1000)
            { // neubehla jeste ani sekunda od ziskani pracovni cesty - optimalizujeme ChangeWorkingPath()
                ChangePathOnlyGetCurPathTime = 0;
                Path[0] = 0;                                           // nasledne volani ChangeWorkingPath() jen vytahne pracovni cestu z cache (neprobehne dalsi zmena cesty)
                showChangeInLog = FALSE;                               // hlasku do logu uz jsme ukazali v GetFullFSPath()
                skipFirstReconnectIfNeeded = TRUE;                     // navazujeme na predchozi praci s "control connection"
                TotalConnectAttemptNum = backupTotalConnectAttemptNum; // navazeme na predchozi pocet reconnectu
            }
            else
            {
                BOOL old = SalamanderGeneral->GetMainWindowHWND() != GetForegroundWindow(); // je-li hlavni okno neaktivni, musime zobrazovat wait okna, jinak hlavni okno nepujde aktivovat a napr. po automatickem zavreni okna operace (po dokonceni operace) dojde k aktivaci jine aplikace misto hl. okna Salama
                ControlConnection->SetStartTime(old);                                       // cas nastavujeme jen jde-li o zacatek operace (a ne o pokracovani GetFullFSPath())
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
                                                       lastErrorState == fesInaccessiblePath, /* neda-li se vylistovat, zkratime ji */
                                                       lastErrorState == fesInaccessiblePath ? NULL : cutFileName,
                                                       pathWasCut, RescuePath, showChangeInLog,
                                                       &pathListing, &pathListingLen, &pathListingDate,
                                                       &pathListingStartTime, &TotalConnectAttemptNum,
                                                       skipFirstReconnectIfNeeded);
            if (pathListing != NULL) // listing byl v cache
            {
                OverwritePathListing = FALSE;
                if (PathListing != NULL)
                {
                    memset(PathListing, 0, PathListingLen); // muze jit o tajna data, radsi nulujeme
                    free(PathListing);                      // uvolnime stary listing
                }
                PathListing = pathListing;
                PathListingLen = pathListingLen;
                PathListingDate = pathListingDate;
                PathListingIsIncomplete = FALSE; // v cache jsou jen kompletni listingy
                PathListingIsBroken = FALSE;     // v cache jsou jen neporusene listingy
                PathListingMayBeOutdated = FALSE;
                PathListingStartTime = pathListingStartTime;
            }
            if (!ret)
                TargetPanelPath[0] = 0; // pripojeni se nezdarilo, zadna zmena cesty v cilovem panelu
        }
        else // pri refreshi z duvodu zmeny konfigurace server-types + sloupcu zustavame na stejne ceste se stejnym textem listingu (i kdyz je NULL)
        {
            ControlConnection->SetStartTime(); // nastavime cas pro listovani
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

            // doplnime prazdne hodnoty do prazdnych sloupcu
            FillEmptyValues(err, &updir, TRUE, dataIface, columns, NULL, NULL, 0, 0, 0);

            // pridame ho do 'dir'
            if (!err && !dir->AddDir(NULL, updir, NULL))
                err = TRUE;
            if (err)
            {
                // uvolnime data up-diru
                dataIface->ReleasePluginData(updir, TRUE);
                SalamanderGeneral->Free(updir.Name);
            }
        }
        else
            err = TRUE; // malo pameti
    }
    else
        err = TRUE; // malo pameti
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
        // vytvorime lokalni kopii dat sloupcu (typ serveru neni zablokovany po celou dobu existence listingu v panelu)
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

        if (!err) // mame kopii sloupcu
        {
            CFTPListingPluginDataInterface* dataIface = new CFTPListingPluginDataInterface(columns, TRUE,
                                                                                           validDataMask, isVMS);
            if (dataIface != NULL)
            {
                validDataMask |= dataIface->GetPLValidDataMask();
                DWORD* emptyCol = new DWORD[columns->Count]; // pomocne predalokovane pole pro GetNextItemFromListing
                if (dataIface->IsGood() && emptyCol != NULL)
                {
                    CFTPParser* parser = serverType->CompiledParser;
                    if (parser == NULL)
                    {
                        parser = CompileParsingRules(HandleNULLStr(serverType->RulesForParsing), columns,
                                                     NULL, NULL, NULL);
                        serverType->CompiledParser = parser; // 'parser' nebudeme dealokovat, uz je v 'serverType'
                    }
                    if (parser != NULL)
                    {
                        BOOL needUpDir = FTPIsValidAndNotRootPath(GetFTPServerPathType(Path), Path); // TRUE dokud je potreba vlozit up-dir ("..")

                        CFileData file;
                        const char* listing = PathListing;
                        const char* listingEnd = PathListing + (PathListingIsBroken ? 0 /* neni-li listing OK, bereme radsi prazdny listing */ : PathListingLen);
                        BOOL isDir = FALSE;

                        if (dir != NULL)
                        {
                            dir->SetValidData(validDataMask);
                            dir->SetFlags(SALDIRFLAG_CASESENSITIVE | SALDIRFLAG_IGNOREDUPDIRS); // asi neni k nicemu, kazdopadne vsechno bereme case-sensitive, takze timhle snad nic nezkazime
                        }
                        parser->BeforeParsing(listing, listingEnd, PathListingDate.Year, PathListingDate.Month,
                                              PathListingDate.Day, PathListingIsIncomplete); // init parseru
                        while (parser->GetNextItemFromListing(&file, &isDir, dataIface, columns, &listing,
                                                              listingEnd, NULL, &err, emptyCol))
                        {
                            BOOL dealloc = TRUE;
                            if (isDir && file.NameLen <= 2 &&
                                file.Name[0] == '.' && (file.Name[1] == 0 || file.Name[1] == '.')) // adresare "." a ".."
                            {
                                if (needUpDir && file.Name[1] == '.') // adresar ".."
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
                            else // pridame do 'dir' normalni soubor/adresar
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
                                        // pridani adresare do 'dir'
                                        if (dir->AddDir(NULL, file, NULL))
                                            dealloc = FALSE;
                                        else
                                            err = TRUE;
                                    }
                                    else
                                    {
                                        // pridani souboru do 'dir'
                                        if (dir->AddFile(NULL, file, NULL))
                                            dealloc = FALSE;
                                        else
                                            err = TRUE;
                                    }
                                }
                            }
                            if (dealloc)
                            {
                                // uvolnime data souboru nebo adresare
                                dataIface->ReleasePluginData(file, isDir);
                                SalamanderGeneral->Free(file.Name);
                            }
                            if (err)
                                break;
                        }
                        if (needUpDir && dir != NULL) // jeste je nutne vlozit up-dir ("..")
                            AddUpdir(err, needUpDir, dataIface, columns, dir);
                        if (!err && listing == listingEnd) // parsovani probehlo uspesne
                        {
                            if (pluginData != NULL)
                            {
                                *pluginData = dataIface;
                                dataIface = NULL; // pujde ven jako navratova hodnota
                            }
                            ret = TRUE; // uspech
                        }
                        else
                        {
                            if (dir != NULL)
                                dir->Clear(dataIface); // jinak uvolnime vsechna alokovana data
                        }
                    }
                    else
                        err = TRUE; // muze byt jen nedostatek pameti
                }
                else
                {
                    if (emptyCol == NULL)
                        TRACE_E(LOW_MEMORY);
                    err = TRUE; // low memory
                }
                if (emptyCol != NULL)
                    delete[] emptyCol;
                columns = NULL; // jsou v 'dataIface'
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

    if (OverwritePathListing) // mame stary listing, uvolnime ho (je ho nutne prepsat novym)
    {
        OverwritePathListing = FALSE;
        if (PathListing != NULL)
        {
            memset(PathListing, 0, PathListingLen); // muze jit o tajna data, radsi nulujeme
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

    //  if (PathListing != NULL && forceRefresh && !NextRefreshCanUseOldListing)  // muze se stat kdyz da user tvrdy refresh a odmitne reconnect - pak se pouzije listing z cache (je-li k dispozici)
    //    TRACE_E("Unexpected situation in CPluginFSInterface::ListCurrentPath() - cached refresh!");
    BOOL fatalError = FALSE;
    BOOL listingIsNotFromCache = PathListing == NULL;
    if (PathListing != NULL || // jdeme zpracovat listing z cache, jinak jdeme listovat na server...
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
        ControlConnection->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
        if (PathListing != NULL)                 // mame aspon cast listingu, jdeme ho parsovat
        {
            CFTPServerPathType pathType = GetFTPServerPathType(Path);
            BOOL isVMS = pathType == ftpsptOpenVMS; // zjistime jestli nejde nahodou o VMS listing

            BOOL needSimpleListing = TRUE;
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            // at user nekouka cely parsing na pozustatek list-wait-okna
            UpdateWindow(SalamanderGeneral->GetMsgBoxParent());

            // po 3 sekundach ukazeme wait-okenko "parsing listing, please wait..."
            SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAITWNDPARSINGLST), LoadStr(IDS_FTPPLUGINTITLE),
                                                    WAITWND_PARSINGLST, FALSE,
                                                    SalamanderGeneral->GetMsgBoxParent());

            char* welcomeReply = ControlConnection->AllocServerFirstReply();
            char* systReply = ControlConnection->AllocServerSystemReply();

            if (listingIsNotFromCache && !PathListingIsIncomplete && !PathListingIsBroken &&
                welcomeReply != NULL && systReply != NULL)
            { // updatneme listing v cache upload listingu (nahradime aproximovany listing realnym)
                UploadListingCache.AddOrUpdateListing(User, Host, Port, Path, pathType,
                                                      PathListing, PathListingLen,
                                                      PathListingDate, PathListingStartTime,
                                                      TRUE /* jen update listingu */, welcomeReply, systReply,
                                                      AutodetectSrvType ? NULL : LastServerType);
            }

            // nulovani pomocne promenne pro urceni ktery typ serveru jiz byl (neuspesne) vyzkousen
            CServerTypeList* serverTypeList = Config.LockServerTypeList();
            int serverTypeListCount = serverTypeList->Count;
            int j;
            for (j = 0; j < serverTypeListCount; j++)
                serverTypeList->At(j)->ParserAlreadyTested = FALSE;

            // hledani LastServerType
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
                        // serverType je vybrany, jdeme vyzkouset jeho parser na listingu
                        serverType->ParserAlreadyTested = TRUE;
                        if (ParseListing(dir, &pluginData, serverType, &err, isVMS, NULL, FALSE, NULL, NULL))
                        {
                            needSimpleListing = FALSE; // uspesne jsme rozparsovali listing
                        }
                        break; // nasli jsme pozadovany typ serveru, koncime
                    }
                }
                if (i == serverTypeListCount) // LastServerType neexistuje -> probehne autodetekce
                {
                    AutodetectSrvType = TRUE;
                    LastServerType[0] = 0;
                }
            }
            else
                AutodetectSrvType = TRUE; // nejspis zbytecne, jen pro sychr...

            // autodetekce - vyber typu serveru se splnenou autodetekcni podminkou
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
                        if (!serverType->ParserAlreadyTested) // jen pokud jsme ho uz nezkouseli
                        {
                            if (serverType->CompiledAutodetCond == NULL)
                            {
                                serverType->CompiledAutodetCond = CompileAutodetectCond(HandleNULLStr(serverType->AutodetectCond),
                                                                                        NULL, NULL, NULL, NULL, 0);
                                if (serverType->CompiledAutodetCond == NULL) // muze byt jen chyba nedostatku pameti
                                {
                                    err = TRUE;
                                    break;
                                }
                            }
                            if (serverType->CompiledAutodetCond->Evaluate(welcomeReply, welcomeReplyLen,
                                                                          systReply, systReplyLen))
                            {
                                // serverType je vybrany, jdeme vyzkouset jeho parser na listingu
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
                                    needSimpleListing = err; // uspesne jsme rozparsovali listing nebo doslo k chybe nedostatku pameti, koncime
                                    break;
                                }
                            }
                        }
                    }
                }

                // autodetekce - vyber zbyvajicich typu serveru
                if (!err && needSimpleListing)
                {
                    int i;
                    for (i = 0; i < serverTypeListCount; i++)
                    {
                        serverType = serverTypeList->At(i);
                        if (!serverType->ParserAlreadyTested) // jen pokud jsme ho uz nezkouseli
                        {
                            // serverType je vybrany, jdeme vyzkouset jeho parser na listingu
                            // serverType->ParserAlreadyTested = TRUE;  // zbytecne, dale se nepouziva
                            if (ParseListing(dir, &pluginData, serverType, &err, isVMS, NULL, FALSE, NULL, NULL) || err)
                            {
                                if (!err)
                                {
                                    const char* s = serverType->TypeName;
                                    if (*s == '*')
                                        s++;
                                    lstrcpyn(LastServerType, s, SERVERTYPE_MAX_SIZE);
                                }
                                needSimpleListing = err; // uspesne jsme rozparsovali listing nebo doslo k chybe nedostatku pameti, koncime
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
                if (needSimpleListing) // neznamy listing, vypiseme hlasku o poslani informace do
                {                      // ALTAPu + dame do logu "Unknown Server Type"
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
                else // dame do logu cim jsme to rozparsovali
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
                pluginData = &SimpleListPluginDataInterface; // POZOR: zmena se muze tykat i ziskavani data iface v CPluginFSInterface::ChangeAttributes!
                dir->SetValidData(VALID_DATA_NONE);
                dir->SetFlags(SALDIRFLAG_CASESENSITIVE | SALDIRFLAG_IGNOREDUPDIRS); // asi neni k nicemu, kazdopadne vsechno bereme case-sensitive, takze timhle snad nic nezkazime
                if (!PathListingIsBroken &&                                         // neni-li listing OK, bereme radsi prazdny listing
                    PathListingLen > 0)
                {
                    char* beg = PathListing;
                    char* end = beg + PathListingLen;
                    char* s = beg;
                    int lines = 0;
                    while (s < end)
                    {
                        while (s < end && *s != '\r' && *s != '\n')
                            s++; // hledame konec radky
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
                            s++; // hledame konec radky

                        // generovani poradoveho cisla radky vypisu listingu
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

                        // zpracovani radky ('beg' az 's')
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

                        // preskok na zacatek dalsi radky
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

            // schovame wait-okenko "parsing listing, please wait..."
            SalamanderGeneral->DestroySafeWaitWindow();

            SetCursor(oldCur);
            ControlConnection->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
            return !err;
        }
        else
            ErrorState = fesFatal; // low memory, do trace jiz hlaseno
    }
    else
        ErrorState = fatalError ? fesFatal : fesInaccessiblePath; // server hlasi fatal error nebo jen ze cestu nelze listovat
    NextRefreshWontClearCache = FALSE;
    ControlConnection->ActivateWelcomeMsg(); // pokud nejaky msg-box deaktivoval okno welcome-msg, aktivujeme ho zpet
    return FALSE;
}
