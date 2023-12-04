// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CServerTypeList
//

BOOL CServerTypeList::AddServerType(const char* typeName, const char* autodetectCond, int columnsCount,
                                    const char* columnsStr[], const char* rulesForParsing)
{
    CServerType* n = new CServerType;
    if (n != NULL)
    {
        if (n->Set(typeName, autodetectCond, columnsCount, columnsStr, rulesForParsing))
        {
            Add(n);
            if (!IsGood())
                ResetState();
            else
                return TRUE;
        }
        delete n;
    }
    else
        TRACE_E(LOW_MEMORY);
    return FALSE;
}

BOOL CServerTypeList::AddServerType(const char* typeName, CServerType* copyFrom)
{
    CServerType* n = new CServerType;
    if (n != NULL)
    {
        if (n->Set(typeName, copyFrom))
        {
            Add(n);
            if (!IsGood())
                ResetState();
            else
                return TRUE;
        }
        delete n;
    }
    else
        TRACE_E(LOW_MEMORY);
    return FALSE;
}

void CServerTypeList::AddNamesToCombo(HWND combo, const char* serverType, int& index)
{
    index = -1;
    char buf[SERVERTYPE_MAX_SIZE + 101];
    if (serverType != NULL && *serverType == '*')
        serverType++;
    int i;
    for (i = 0; i < Count; i++)
    {
        CServerType* s = At(i);
        SendMessage(combo, CB_ADDSTRING, 0,
                    (LPARAM)GetTypeNameForUser(s->TypeName, buf, SERVERTYPE_MAX_SIZE + 101));
        if (serverType != NULL && index == -1 &&
            SalamanderGeneral->StrICmp(serverType, s->TypeName[0] == '*' ? s->TypeName + 1 : s->TypeName) == 0)
        {
            index = i;
        }
    }
}

void CServerTypeList::AddNamesToListbox(HWND listbox)
{
    char buf[SERVERTYPE_MAX_SIZE + 101];
    int i;
    for (i = 0; i < Count; i++)
        SendMessage(listbox, LB_ADDSTRING, 0, (LPARAM)GetTypeNameForUser(At(i)->TypeName, buf, SERVERTYPE_MAX_SIZE + 101));
}

void CServerTypeList::Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HKEY actKey;
    if (registry->OpenKey(regKey, CONFIG_SERVERTYPES, actKey))
    {
        HKEY subKey;
        char buf[30];
        int i = 0;
        DestroyMembers();
        while (registry->OpenKey(actKey, _itoa(++i, buf, 10), subKey))
        {
            CServerType* item = new CServerType;
            if (item == NULL)
            {
                TRACE_E(LOW_MEMORY);
                break;
            }
            if (!item->Load(parent, subKey, registry)) // nepovedl se load, na tuhle polozku kasleme
            {
                delete item;
            }
            else
            {
                Add(item);
                if (!IsGood())
                {
                    ResetState();
                    delete item;
                    break;
                }
            }
            registry->CloseKey(subKey);
        }
        registry->CloseKey(actKey);
    }
}

void CServerTypeList::Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HKEY actKey;
    if (registry->CreateKey(regKey, CONFIG_SERVERTYPES, actKey))
    {
        registry->ClearKey(actKey);
        HKEY subKey;
        char buf[30];
        int i;
        for (i = 0; i < Count; i++)
        {
            if (registry->CreateKey(actKey, _itoa(i + 1, buf, 10), subKey))
            {
                At(i)->Save(parent, subKey, registry);
                registry->CloseKey(subKey);
            }
            else
                break;
        }
        registry->CloseKey(actKey);
    }
}

BOOL CServerTypeList::CopyItemsFrom(CServerTypeList* list)
{
    BOOL ret = TRUE;
    int i;
    for (i = 0; i < list->Count; i++)
    {
        CServerType* item = list->At(i)->MakeCopy();
        if (item != NULL)
        {
            Add(item);
            if (!IsGood())
            {
                ret = FALSE;
                delete item;
                ResetState();
                break;
            }
        }
        else
        {
            ret = FALSE;
            break;
        }
    }
    if (!ret)
        DestroyMembers(); // obrana proti pripadnemu prehlednuti chyby
    return ret;
}

BOOL CServerTypeList::ContainsTypeName(const char* typeName, CServerType* exclude, int* index)
{
    // porovnani bez ohledu na "user defined" (jinak by neslo editovat server type -> po
    // editaci se meni na "user defined" a kdyby uz existoval nastal by konflikt)
    if (*typeName == '*')
        typeName++;
    int i;
    for (i = 0; i < Count; i++)
    {
        CServerType* s = At(i);
        if (s != exclude &&
            SalamanderGeneral->StrICmp(typeName, s->TypeName[0] == '*' ? s->TypeName + 1 : s->TypeName) == 0)
        {
            if (index != NULL)
                *index = i;
            return TRUE;
        }
    }
    if (index != NULL)
        *index = -1;
    return FALSE;
}

//
// ****************************************************************************
// CFTPServerList
//

BOOL CFTPServerList::CopyMembersToList(CFTPServerList& dstList)
{
    dstList.DestroyMembers();
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPServer* n = At(i)->MakeCopy();
        if (n != NULL)
        {
            dstList.Add(n);
            if (!dstList.IsGood())
            {
                dstList.ResetState();
                delete n;
                return FALSE;
            }
        }
        else
            return FALSE; // chyba
    }
    return TRUE;
}

BOOL CFTPServerList::AddServer(const char* itemName,
                               const char* address,
                               const char* initialPath,
                               int anonymousConnection,
                               const char* userName,
                               const BYTE* encryptedPassword,
                               int encryptedPasswordSize,
                               int savePassword,
                               int proxyServerUID,
                               const char* targetPanelPath,
                               const char* serverType,
                               int transferMode,
                               int port,
                               int usePassiveMode,
                               int keepConnectionAlive,
                               int useMaxConcurrentConnections,
                               int maxConcurrentConnections,
                               int useServerSpeedLimit,
                               double serverSpeedLimit,
                               int useListingsCache,
                               const char* initFTPCommands,
                               const char* listCommand,
                               int keepAliveSendEvery,
                               int keepAliveStopAfter,
                               int keepAliveCommand,
                               int encryptControlConnection,
                               int encryptDataConnection,
                               int compressData)
{
    if (keepAliveSendEvery == -1)
        keepAliveSendEvery = Config.KeepAliveSendEvery;
    if (keepAliveStopAfter == -1)
        keepAliveStopAfter = Config.KeepAliveStopAfter;
    if (keepAliveCommand == -1)
        keepAliveCommand = Config.KeepAliveCommand;

    CFTPServer* n = new CFTPServer;
    if (n != NULL)
    {
        if (n->Set(itemName,
                   address,
                   initialPath,
                   anonymousConnection,
                   userName,
                   encryptedPassword,
                   encryptedPasswordSize,
                   savePassword,
                   proxyServerUID,
                   targetPanelPath,
                   serverType,
                   transferMode,
                   port,
                   usePassiveMode,
                   keepConnectionAlive,
                   useMaxConcurrentConnections,
                   maxConcurrentConnections,
                   useServerSpeedLimit,
                   serverSpeedLimit,
                   useListingsCache,
                   initFTPCommands,
                   listCommand,
                   keepAliveSendEvery,
                   keepAliveStopAfter,
                   keepAliveCommand,
                   encryptControlConnection,
                   encryptDataConnection,
                   compressData))
        {
            Add(n);
            if (!IsGood())
                ResetState();
            else
                return TRUE;
        }
        delete n;
    }
    else
        TRACE_E(LOW_MEMORY);
    return FALSE;
}

void CFTPServerList::AddNamesToListbox(HWND list)
{
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPServer* s = At(i);
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(s->ItemName == NULL ? "" : s->ItemName));
    }
}

void CFTPServerList::CheckProxyServersUID(CFTPProxyServerList& ftpProxyServerList)
{
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPServer* s = At(i);
        if (s->ProxyServerUID != -1 && s->ProxyServerUID != -2 &&
            !ftpProxyServerList.IsValidUID(s->ProxyServerUID))
        {
            s->ProxyServerUID = -2; // zvalidnime UID prechodem na "default"
        }
    }
}

BOOL CFTPServerList::ContainsUnsecuredPassword()
{
    // POZOR, stejna metoda exisuje pro CFTPProxyServerList
    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPServer* s = At(i);
        if (s->SavePassword && s->EncryptedPassword != NULL)
        {
            if (!passwordManager->IsPasswordEncrypted(s->EncryptedPassword, s->EncryptedPasswordSize))
                return TRUE;
        }
    }
    return FALSE;
}

BOOL EncryptPasswordAux(BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL savePassword, BOOL encrypt)
{
    BOOL ret = TRUE;
    if (*encryptedPassword != NULL && (savePassword || !encrypt))
    {
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        if (encrypt)
        {
            if (!passwordManager->IsPasswordEncrypted(*encryptedPassword, *encryptedPasswordSize))
            {
                // scrambled -> plain
                char* plainPassword;
                if (passwordManager->DecryptPassword(*encryptedPassword, *encryptedPasswordSize, &plainPassword))
                {
                    // plain -> AES
                    BYTE* buffEncryptedPassword = NULL;
                    int buffEncryptedPasswordSize = 0;
                    if (plainPassword[0] == 0 ||
                        passwordManager->EncryptPassword(plainPassword, &buffEncryptedPassword, &buffEncryptedPasswordSize, TRUE))
                    {
                        UpdateEncryptedPassword(encryptedPassword, encryptedPasswordSize, buffEncryptedPassword, buffEncryptedPasswordSize);
                        if (buffEncryptedPassword != NULL) // uvolnime buffer alokovany v EncryptPassword()
                        {
                            memset(buffEncryptedPassword, 0, buffEncryptedPasswordSize);
                            SalamanderGeneral->Free(buffEncryptedPassword);
                        }
                    }
                    // vynulujeme a uvolnime docasny plain buffer
                    memset(plainPassword, 0, lstrlen(plainPassword));
                    SalamanderGeneral->Free(plainPassword);
                }
            }
        }
        else
        {
            if (passwordManager->IsPasswordEncrypted(*encryptedPassword, *encryptedPasswordSize))
            {
                // encrypted -> plain
                char* plainPassword;
                if (passwordManager->DecryptPassword(*encryptedPassword, *encryptedPasswordSize, &plainPassword)) // muze vratit FALSE
                {
                    // plain -> scrambled
                    BYTE* scrambledPassword = NULL;
                    int scrambledPasswordSize = 0;
                    if (plainPassword[0] == 0 ||
                        passwordManager->EncryptPassword(plainPassword, &scrambledPassword, &scrambledPasswordSize, FALSE))
                    {
                        UpdateEncryptedPassword(encryptedPassword, encryptedPasswordSize, scrambledPassword, scrambledPasswordSize);
                        if (scrambledPassword != NULL) // uvolnime buffer alokovany v EncryptPassword()
                        {
                            memset(scrambledPassword, 0, scrambledPasswordSize);
                            SalamanderGeneral->Free(scrambledPassword);
                        }
                    }
                    // vynulujeme a uvolnime docasny plain buffer
                    memset(plainPassword, 0, lstrlen(plainPassword));
                    SalamanderGeneral->Free(plainPassword);
                }
                else
                {
                    // encrypted heslo se nepodarilo rozsifrovat, zrejme je zasifrovano pomoci jineho master password; dame o tom vedet pomoci navratove hodnoty
                    ret = FALSE;
                }
            }
        }
    }
    return ret;
}

BOOL CFTPServerList::EncryptPasswords(HWND hParent, BOOL encrypt)
{
    // POZOR, stejna metoda exisuje pro CFTPProxyServerList
    BOOL ret = TRUE;
    int i;
    for (i = 0; i < Count; i++)
    {
        CFTPServer* s = At(i);
        ret &= EncryptPasswordAux(&s->EncryptedPassword, &s->EncryptedPasswordSize, s->SavePassword, encrypt);
    }
    return ret;
}

void CFTPServerList::Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HKEY actKey;
    if (registry->OpenKey(regKey, CONFIG_FTPSERVERLIST, actKey))
    {
        HKEY subKey;
        char buf[30];
        int i = 0;
        DestroyMembers();
        while (registry->OpenKey(actKey, _itoa(++i, buf, 10), subKey))
        {
            CFTPServer* item = new CFTPServer;
            if (item == NULL)
            {
                TRACE_E(LOW_MEMORY);
                break;
            }
            if (!item->Load(parent, subKey, registry)) // nepovedl se load, na tuhle polozku kasleme
            {
                delete item;
            }
            else
            {
                Add(item);
                if (!IsGood())
                {
                    ResetState();
                    delete item;
                    break;
                }
            }
            registry->CloseKey(subKey);
        }
        registry->CloseKey(actKey);
    }
}

void CFTPServerList::Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HKEY actKey;
    if (registry->CreateKey(regKey, CONFIG_FTPSERVERLIST, actKey))
    {
        registry->ClearKey(actKey);
        HKEY subKey;
        char buf[30];
        int i;
        for (i = 0; i < Count; i++)
        {
            if (registry->CreateKey(actKey, _itoa(i + 1, buf, 10), subKey))
            {
                At(i)->Save(parent, subKey, registry);
                registry->CloseKey(subKey);
            }
            else
                break;
        }
        registry->CloseKey(actKey);
    }
}

//
// ****************************************************************************
// CConfiguration
//

CConfiguration::CConfiguration()
{
    HANDLES(InitializeCriticalSection(&ServerTypeListCS));
    HANDLES(InitializeCriticalSection(&ConParamsCS));

    LastCfgPage = 0;
    ShowWelcomeMessage = TRUE;
    PriorityToPanelConnections = TRUE;
    EnableTotalSpeedLimit = FALSE;
    TotalSpeedLimit = 2;
    strcpy(AnonymousPasswd, "name@someserver.com");

    PassiveMode = FALSE;
    KeepAlive = TRUE;
    UseMaxConcurrentConnections = FALSE;
    MaxConcurrentConnections = 1;
    UseServerSpeedLimit = FALSE;
    ServerSpeedLimit = 2;
    UseListingsCache = TRUE;
    TransferMode = trmAutodetect;
    ASCIIFileMasks = NULL;

    ServerRepliesTimeout = 30;
    NoDataTransferTimeout = 300;
    DelayBetweenConRetries = 20;
    ConnectRetries = 60;
    ResumeOverlap = 1024;
    ResumeMinFileSize = 32768;
    KeepAliveSendEvery = 90;
    KeepAliveStopAfter = 30;
    KeepAliveCommand = 0;

    CompressData = 0;

    LastBookmark = 0;

    DefaultProxySrvUID = -1; // "not used"

    AlwaysNotCloseCon = FALSE;
    AlwaysDisconnect = FALSE;

    EnableLogging = TRUE;
    UseLogMaxSize = TRUE;
    LogMaxSize = 50;
    UseMaxClosedConLogs = TRUE;
    MaxClosedConLogs = 10;
    AlwaysShowLogForActPan = TRUE;
    DisableLoggingOfWorkers = FALSE;

    LogsDlgPlacement.length = 0; // zatim neplatna pozice
    OperDlgPlacement.length = 0; // zatim neplatna pozice
    OperDlgSplitPos = 0.5;
    CloseOperationDlgIfSuccessfullyFinished = TRUE;
    CloseOperationDlgWhenOperFinishes = FALSE;
    OpenSolveErrIfIdle = TRUE;

    // nasledujici data se neukladaji (drzi se jen v pameti, pak se zahodi):
    UseConnectionDataFromConfig = FALSE;
    ChangingPathInInactivePanel = FALSE;
    DisconnectCommandUsed = FALSE;

    int i;
    for (i = 0; i < COMMAND_HISTORY_SIZE; i++)
        CommandHistory[i] = NULL;
    SendSecretCommand = FALSE;

    for (i = 0; i < HOSTADDRESS_HISTORY_SIZE; i++)
        HostAddressHistory[i] = NULL;
    for (i = 0; i < INITIALPATH_HISTORY_SIZE; i++)
        InitPathHistory[i] = NULL;

    AlwaysReconnect = FALSE;
    WarnWhenConLost = TRUE;
    HintListHiddenFiles = TRUE;
    AlwaysOverwrite = FALSE;

    ConvertHexEscSeq = TRUE;

    CacheMaxSize = CQuadWord(4 * 1024 * 1024, 0);

    DownloadAddToQueue = FALSE;
    DeleteAddToQueue = FALSE;
    ChAttrAddToQueue = FALSE;

    OperationsCannotCreateFile = CANNOTCREATENAME_USERPROMPT;
    OperationsCannotCreateDir = CANNOTCREATENAME_USERPROMPT;
    OperationsFileAlreadyExists = FILEALREADYEXISTS_USERPROMPT;
    OperationsDirAlreadyExists = DIRALREADYEXISTS_JOIN;
    OperationsRetryOnCreatedFile = RETRYONCREATFILE_RES_OVRWR;
    OperationsRetryOnResumedFile = RETRYONRESUMFILE_RESUME;
    OperationsAsciiTrModeButBinFile = ASCIITRFORBINFILE_USERPROMPT;
    OperationsUnknownAttrs = UNKNOWNATTRS_USERPROMPT;
    OperationsNonemptyDirDel = NONEMPTYDIRDEL_DELETEIT;
    OperationsHiddenFileDel = HIDDENFILEDEL_USERPROMPT;
    OperationsHiddenDirDel = HIDDENDIRDEL_DELETEIT;

    UploadCannotCreateFile = CANNOTCREATENAME_USERPROMPT;
    UploadCannotCreateDir = CANNOTCREATENAME_USERPROMPT;
    UploadFileAlreadyExists = FILEALREADYEXISTS_USERPROMPT;
    UploadDirAlreadyExists = DIRALREADYEXISTS_JOIN;
    UploadRetryOnCreatedFile = RETRYONCREATFILE_RES_OVRWR;
    UploadRetryOnResumedFile = RETRYONRESUMFILE_RESUME;
    UploadAsciiTrModeButBinFile = ASCIITRFORBINFILE_USERPROMPT;

    TestParserDlgWidth = -1;
    TestParserDlgHeight = -1;
}

CConfiguration::~CConfiguration()
{
    int i;
    for (i = 0; i < COMMAND_HISTORY_SIZE; i++)
        if (CommandHistory[i] != NULL)
            free(CommandHistory[i]);

    for (i = 0; i < HOSTADDRESS_HISTORY_SIZE; i++)
        if (HostAddressHistory[i] != NULL)
            free(HostAddressHistory[i]);
    for (i = 0; i < INITIALPATH_HISTORY_SIZE; i++)
        if (InitPathHistory[i] != NULL)
            free(InitPathHistory[i]);

    HANDLES(DeleteCriticalSection(&ConParamsCS));
    HANDLES(DeleteCriticalSection(&ServerTypeListCS));
}

BOOL CConfiguration::InitWithSalamanderGeneral()
{
    // alokuje se pres SalamanderGeneral, proto musi byt zde
    FTPServerList.AddServer("ALTAP",
                            "ftp.altap.cz",
                            "/pub/altap/salamand");

    // popis retezce v poli: "visible,ID,nameStrID,nameStr,descrStrID,descrStr,colType,emptyValue,leftAlignment,fixedWidth,width"
    const char* unix1Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",    // name
                                  "1,ext,1,\\0,1,\\0,2,\\0",     // extension
                                  "1,size,2,\\0,2,\\0,3,\\0",    // size
                                  "0,type,5,\\0,5,\\0,6,\\0",    // type
                                  "1,date,3,\\0,3,\\0,4,\\0",    // date
                                  "1,time,4,\\0,4,\\0,9,\\0,0",  // time
                                  "1,rights,6,\\0,6,\\0,7,\\0",  // rights
                                  "1,link,12,\\0,12,\\0,7,\\0",  // link target
                                  "1,user,7,\\0,7,\\0,7,\\0",    // user
                                  "1,group,8,\\0,8,\\0,7,\\0",   // group
                                  "0,device,9,\\0,9,\\0,7,\\0"}; // device minor/major number
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("UNIX1", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                          "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 11, unix1Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>),\r\n"
                                 "  white_spaces(), year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), month_3(<date>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), month_3(<date>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"),\r\n"
                                 "  unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"total ????\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("UNIX2", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                          "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 11, unix1Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), month_3(<date>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"),\r\n"
                                 "  unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"total ????\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* unix3Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",    // name
                                  "1,ext,1,\\0,1,\\0,2,\\0",     // extension
                                  "1,size,2,\\0,2,\\0,3,\\0",    // size
                                  "0,type,5,\\0,5,\\0,6,\\0",    // type
                                  "1,date,3,\\0,3,\\0,4,\\0",    // date
                                  "1,time,4,\\0,4,\\0,9,\\0,0",  // time
                                  "1,rights,6,\\0,6,\\0,7,\\0",  // rights
                                  "1,link,12,\\0,12,\\0,7,\\0",  // link target
                                  "1,user,7,\\0,7,\\0,7,\\0",    // user
                                  "0,device,9,\\0,9,\\0,7,\\0"}; // device minor/major number
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("UNIX3",
                                 "(not welcome_contains(\" NW \") or not welcome_contains(\" HellSoft.\")) "
                                 "and not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                 "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 10, unix3Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  positive_number(<size>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), unix_device(<device>), white_spaces(), month_3(<date>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>),\r\n"
                                 "  white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), month_3(<date>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"),\r\n"
                                 "  unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"total ????\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("UNIX4", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                          "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 11, unix1Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  day(<date>), all(1), skip_white_spaces(), month_txt(<date>), all(1),\r\n"
                                 "  skip_white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), day(<date>), all(1), skip_white_spaces(),\r\n"
                                 "  month_txt(<date>), all(1), skip_white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"),\r\n"
                                 "  unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), skip_white_spaces(), month_txt(<date>),\r\n"
                                 "  all(1), skip_white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), skip_white_spaces(), month_txt(<date>),\r\n"
                                 "  all(1), skip_white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), day(<date>), all(1),\r\n"
                                 "  skip_white_spaces(), month_txt(<date>), all(1), skip_white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), day(<date>), all(1),\r\n"
                                 "  skip_white_spaces(), month_txt(<date>), all(1), skip_white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"total ????\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("UNIX5", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                          "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 11, unix1Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), year(<date>), white_spaces(2),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"total ????\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* xbox360Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",   // name
                                    "1,ext,1,\\0,1,\\0,2,\\0",    // extension
                                    "1,size,2,\\0,2,\\0,3,\\0",   // size
                                    "0,type,5,\\0,5,\\0,6,\\0",   // type
                                    "1,date,3,\\0,3,\\0,4,\\0",   // date
                                    "1,time,4,\\0,4,\\0,5,\\0,0", // time
                                    "1,rights,6,\\0,6,\\0,7,\\0", // rights
                                    "1,user,7,\\0,7,\\0,7,\\0",   // user
                                    "1,group,8,\\0,8,\\0,7,\\0"}; // group
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("Xbox 360", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                             "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 9, xbox360Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  year(<date>), all(1), month(<date>), all(1), day(<date>), all(1), time(<time>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* unix6Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",    // name
                                  "1,ext,1,\\0,1,\\0,2,\\0",     // extension
                                  "1,size,2,\\0,2,\\0,3,\\0",    // size
                                  "0,type,5,\\0,5,\\0,6,\\0",    // type
                                  "1,rights,6,\\0,6,\\0,7,\\0",  // rights
                                  "1,link,12,\\0,12,\\0,7,\\0",  // link target
                                  "1,user,7,\\0,7,\\0,7,\\0",    // user
                                  "1,group,8,\\0,8,\\0,7,\\0",   // group
                                  "0,device,9,\\0,9,\\0,7,\\0"}; // device minor/major number
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("UNIX6", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                          "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 9, unix6Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>), white_spaces(),\r\n"
                                 "  word(<group>), white_spaces(), positive_number(<size>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(<user>), white_spaces(), word(<group>),\r\n"
                                 "  white_spaces(), word(), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices\r\n"
                                 "* word(<rights>), white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), word(<group>), white_spaces(), unix_device(<device>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files and directories (skipping user+group with more spaces)\r\n"
                                 "* if(next_char!=\"l\"), assign(<is_dir>, next_char==\"d\"), word(<rights>),\r\n"
                                 "  white_spaces(), word(), skip_to_number(), positive_number(<size>),\r\n"
                                 "  white_spaces(1), assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with links (skipping user+group with more spaces)\r\n"
                                 "* if(next_char==\"l\"), assign(<is_link>, true), word(<rights>), white_spaces(),\r\n"
                                 "  word(), skip_to_number(), word(), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), unix_link(<is_dir>, <name>, <link>);\r\n"
                                 "\r\n"
                                 "# parse lines with devices (skipping user+group with more spaces)\r\n"
                                 "* word(<rights>), white_spaces(), word(), skip_to_number(),\r\n"
                                 "  unix_device(<device>), white_spaces(1),\r\n"
                                 "  assign(<is_hidden>, next_char==\".\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"total ????\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* netwareColumns[] = {"1,name,0,\\0,0,\\0,1,\\0",   // name
                                    "1,ext,1,\\0,1,\\0,2,\\0",    // extension
                                    "1,size,2,\\0,2,\\0,3,\\0",   // size
                                    "0,type,5,\\0,5,\\0,6,\\0",   // type
                                    "1,date,3,\\0,3,\\0,4,\\0",   // date
                                    "1,time,4,\\0,4,\\0,9,\\0,0", // time
                                    "1,rights,6,\\0,6,\\0,7,\\0", // rights
                                    "1,user,7,\\0,7,\\0,7,\\0"};  // user
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("Netware", "not syst_contains(\"z/VM \") and not syst_contains(\"OS/2 \") "
                                            "and not syst_contains(\" MACOS \") and not syst_contains(\"Windows_NT\")",
                                 8, netwareColumns,
                                 "# parse lines from NETWARE FTP server\r\n"
                                 "* assign(<is_dir>, next_char==\"d\"), all(1), white_spaces(1), all_to(\"[\"),\r\n"
                                 "  all_up_to(<rights>, \"]\"), white_spaces(1), all(<user>, 28),\r\n"
                                 "  cut_white_spaces_end(<user>), white_spaces(), positive_number(<size>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(), day(<date>),\r\n"
                                 "  white_spaces(), year_or_time(<date>, <time>), white_spaces(1),\r\n"
                                 "  rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines from Hellsoft FTP server\r\n"
                                 "* assign(<is_dir>, next_char==\"d\"), all_to(\"[\"), all_up_to(<rights>, \"]\"),\r\n"
                                 "  white_spaces(), word(), white_spaces(), word(<user>),\r\n"
                                 "  white_spaces(), positive_number(<size>), white_spaces(), month_3(<date>),\r\n"
                                 "  white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip first line e.g. \"total ???\" or \"--- listing ---\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* vms1Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",         // name
                                 "1,ext,1,\\0,1,\\0,2,\\0",          // extension
                                 "1,blksize,10,\\0,10,\\0,10,\\0,0", // block size
                                 "0,type,5,\\0,5,\\0,6,\\0",         // type
                                 "1,date,3,\\0,3,\\0,8,\\0,0",       // date
                                 "1,time,4,\\0,4,\\0,9,\\0,0",       // time
                                 "1,rights,6,\\0,6,\\0,7,\\0",       // rights
                                 "1,user,7,\\0,7,\\0,7,\\0"};        // user
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("VMS1", "syst_contains(\" VMS \")",
                                 8, vms1Columns,
                                 "# parse lines with directories\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>), all(1),\r\n"
                                 "  year(<date>), white_spaces(), time(<time>), all_to(\"[\"),\r\n"
                                 "  all_up_to(<user>, \"]\"), all_to(\"(\"), all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with directories with corrupted user string\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>), all(1),\r\n"
                                 "  year(<date>), white_spaces(), time(<time>), all_to(\"[\"),\r\n"
                                 "  all_up_to(<user>, \"(\"), cut_white_spaces_end(<user>),\r\n"
                                 "  all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>),\r\n"
                                 "  all(1), year(<date>), white_spaces(), time(<time>), all_to(\"[\"),\r\n"
                                 "  all_up_to(<user>, \"]\"), all_to(\"(\"), all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with files with corrupted user string\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>),\r\n"
                                 "  all(1), year(<date>), white_spaces(), time(<time>), all_to(\"[\"),\r\n"
                                 "  all_up_to(<user>, \"(\"), cut_white_spaces_end(<user>),\r\n"
                                 "  all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# skip first line e.g. \"Directory ???:[???]\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip last line e.g. \"Total of ??? files, ??? blocks.\"\r\n"
                                 "* if(last_nonempty_line), if(first_nonempty_line == false), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip last line containing \"Total of ???\"\r\n"
                                 "* if(last_nonempty_line), skip_white_spaces(), if(next_word eq \"Total\"),\r\n"
                                 "  word(), skip_white_spaces(), if(next_word eq \"of\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on char \"%\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), if(next_char==\"%\"), rest_of_line(<rights>);\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on word \"privilege\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), all_to(<rights>, \"privilege\"),\r\n"
                                 "  add_string_to_column(<rights>, rest_of_line), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on char \"%\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), if(next_char==\"%\"),\r\n"
                                 "  rest_of_line(<rights>);\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on word \"privilege\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), all_to(<rights>, \"privilege\"),\r\n"
                                 "  add_string_to_column(<rights>, rest_of_line), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* vms2Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",            // name
                                 "1,ext,1,\\0,1,\\0,2,\\0",             // extension
                                 "1,blksize,10,\\0,10,\\0,10,\\0,0",    // block size
                                 "0,type,5,\\0,5,\\0,6,\\0",            // type
                                 "1,date,3,\\0,3,\\0,8,\\0,0",          // date
                                 "1,time,4,\\0,4,\\0,9,\\0,0",          // time
                                 "1,rights,6,\\0,6,\\0,7,\\0",          // rights
                                 "1,user,7,\\0,7,\\0,7,\\0",            // user
                                 "1,allocblks,11,\\0,11,\\0,10,\\0,0"}; // allocated blocks
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("VMS2", "syst_contains(\" VMS \")",
                                 9, vms2Columns,
                                 "# parse lines with directories\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  all(1), number(<allocblks>), white_spaces(), day(<date>), all(1),\r\n"
                                 "  month_3(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  all_to(\"[\"), all_up_to(<user>, \"]\"), all_to(\"(\"), all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with directories with corrupted user string\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  all(1), number(<allocblks>), white_spaces(), day(<date>), all(1),\r\n"
                                 "  month_3(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  all_to(\"[\"), all_up_to(<user>, \"(\"), cut_white_spaces_end(<user>),\r\n"
                                 "  all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  all(1), number(<allocblks>), white_spaces(), day(<date>), all(1),\r\n"
                                 "  month_3(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  all_to(\"[\"), all_up_to(<user>, \"]\"), all_to(\"(\"), all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with files with corrupted user string\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  all(1), number(<allocblks>), white_spaces(), day(<date>), all(1),\r\n"
                                 "  month_3(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  all_to(\"[\"), all_up_to(<user>, \"(\"), cut_white_spaces_end(<user>),\r\n"
                                 "  all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# skip first line e.g. \"Directory ???:[???]\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip last line e.g. \"Total of ??? files, ??? blocks.\"\r\n"
                                 "* if(last_nonempty_line), if(first_nonempty_line == false), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip last line containing \"Total of ???\"\r\n"
                                 "* if(last_nonempty_line), skip_white_spaces(), if(next_word eq \"Total\"),\r\n"
                                 "  word(), skip_white_spaces(), if(next_word eq \"of\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on char \"%\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), if(next_char==\"%\"), rest_of_line(<rights>);\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on word \"privilege\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), all_to(<rights>, \"privilege\"),\r\n"
                                 "  add_string_to_column(<rights>, rest_of_line), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on char \"%\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), if(next_char==\"%\"),\r\n"
                                 "  rest_of_line(<rights>);\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on word \"privilege\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), all_to(<rights>, \"privilege\"),\r\n"
                                 "  add_string_to_column(<rights>, rest_of_line), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* vms3Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",          // name
                                 "1,ext,1,\\0,1,\\0,2,\\0",           // extension
                                 "1,size,2,\\0,2,\\0,3,\\0",          // size
                                 "0,type,5,\\0,5,\\0,6,\\0",          // type
                                 "1,date,3,\\0,3,\\0,8,\\0,0",        // date
                                 "1,time,4,\\0,4,\\0,9,\\0,0",        // time
                                 "1,rights,6,\\0,6,\\0,7,\\0",        // rights
                                 "1,blksize,10,\\0,10,\\0,10,\\0,0"}; // block size
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("VMS3", "syst_contains(\" VMS \")",
                                 8, vms3Columns,
                                 "# parse lines with directories\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), day(<date>), all(1),\r\n"
                                 "  month_3(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  white_spaces(), positive_number(<size>), all(1), number(<blksize>), all_to(\"(\"),\r\n"
                                 "  all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), day(<date>),\r\n"
                                 "  all(1), month_3(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  white_spaces(), positive_number(<size>), all(1), number(<blksize>), all_to(\"(\"),\r\n"
                                 "  all_up_to(<rights>, \")\");\r\n"
                                 "\r\n"
                                 "# skip first line e.g. \"Directory ???:[???]\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip last line e.g. \"Total of ??? files, ??? blocks.\"\r\n"
                                 "* if(last_nonempty_line), if(first_nonempty_line == false), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip last line containing \"Total of ???\"\r\n"
                                 "* if(last_nonempty_line), skip_white_spaces(), if(next_word eq \"Total\"),\r\n"
                                 "  word(), skip_white_spaces(), if(next_word eq \"of\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on char \"%\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), if(next_char==\"%\"), rest_of_line(<rights>);\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on word \"privilege\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), all_to(<rights>, \"privilege\"),\r\n"
                                 "  add_string_to_column(<rights>, rest_of_line), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on char \"%\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), if(next_char==\"%\"),\r\n"
                                 "  rest_of_line(<rights>);\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on word \"privilege\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), all_to(<rights>, \"privilege\"),\r\n"
                                 "  add_string_to_column(<rights>, rest_of_line), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* vms4Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",         // name
                                 "1,ext,1,\\0,1,\\0,2,\\0",          // extension
                                 "1,blksize,10,\\0,10,\\0,10,\\0,0", // block size
                                 "0,type,5,\\0,5,\\0,6,\\0",         // type
                                 "1,date,3,\\0,3,\\0,8,\\0,0",       // date
                                 "1,time,4,\\0,4,\\0,9,\\0,0"};      // time
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("VMS4", "syst_contains(\" VMS \")",
                                 6, vms4Columns,
                                 "# parse lines with directories\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>), all(1),\r\n"
                                 "  year(<date>), white_spaces(), time(<time>);\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), number(<blksize>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>),\r\n"
                                 "  all(1), year(<date>), white_spaces(), time(<time>);\r\n"
                                 "\r\n"
                                 "# skip first line e.g. \"Directory ???:[???]\" - up to 3 words\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), word(), skip_white_spaces(),\r\n"
                                 "  word(), skip_white_spaces(), word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip last line e.g. \"Total of ??? files, ??? blocks.\"\r\n"
                                 "* if(last_nonempty_line), if(first_nonempty_line == false), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip last line containing \"Total of ???\"\r\n"
                                 "* if(last_nonempty_line), skip_white_spaces(), if(next_word eq \"Total\"),\r\n"
                                 "  word(), skip_white_spaces(), if(next_word eq \"of\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on char \"%\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), if(next_char==\"%\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible directories (based on word \"privilege\")\r\n"
                                 "* all_up_to(<name>, \".DIR;1\"), assign(<is_dir>, true),\r\n"
                                 "  white_spaces_and_line_ends(), all_to(\"privilege\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on char \"%\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), if(next_char==\"%\"),\r\n"
                                 "  rest_of_line();\r\n"
                                 "\r\n"
                                 "# parse lines with inaccessible files (based on word \"privilege\")\r\n"
                                 "* word(<name>), white_spaces_and_line_ends(), all_to(\"privilege\"),\r\n"
                                 "  rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* netPrezColumns[] = {"1,name,0,\\0,0,\\0,1,\\0",    // name
                                    "1,ext,1,\\0,1,\\0,2,\\0",     // extension
                                    "1,size,2,\\0,2,\\0,3,\\0",    // size
                                    "0,type,5,\\0,5,\\0,6,\\0",    // type
                                    "1,date,3,\\0,3,\\0,4,\\0",    // date
                                    "1,time,4,\\0,4,\\0,9,\\0,0",  // time
                                    "1,rights,6,\\0,6,\\0,7,\\0"}; // rights
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("NetPresenz", "welcome_contains(\" NetPresenz \") and syst_contains(\" MACOS \")",
                                 7, netPrezColumns,
                                 "# parse lines with files\r\n"
                                 "* if(next_char!=\"d\"), all(<rights>, 10), white_spaces(), word(),\r\n"
                                 "  white_spaces(), word(), white_spaces(), positive_number(<size>),\r\n"
                                 "  white_spaces(), month_3(<date>), white_spaces(),\r\n"
                                 "  day(<date>), white_spaces(), year_or_time(<date>, <time>),\r\n"
                                 "  white_spaces(1), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with directories\r\n"
                                 "* if(next_char==\"d\"), assign(<is_dir>, true), all(<rights>, 10), white_spaces(),\r\n"
                                 "  word(), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  month_3(<date>), white_spaces(), day(<date>), white_spaces(),\r\n"
                                 "  year_or_time(<date>, <time>), white_spaces(1), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* IBMz_VM1Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",         // name
                                     "1,ext,5,\\0,5,\\0,2,\\0",          // type (extension)
                                     "1,format,13,\\0,13,\\0,7,\\0",     // format
                                     "1,lrecl,14,\\0,14,\\0,10,\\0,0",   // lrecl
                                     "1,records,15,\\0,15,\\0,10,\\0,0", // records
                                     "1,blocks,10,\\0,10,\\0,10,\\0,0",  // blocks
                                     "1,date,3,\\0,3,\\0,8,\\0",         // date
                                     "1,time,4,\\0,4,\\0,9,\\0,0"};      // time
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("IBM z/VM (CMS) 1", NULL,
                                 8, IBMz_VM1Columns,
                                 "# parse lines with directories\r\n"
                                 "* word(<name>), all_to(\" DIR\"), assign(<is_dir>, true), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(), white_spaces(), word(), white_spaces(),\r\n"
                                 "  year(<date>), all(1), month(<date>), all(1), day(<date>),\r\n"
                                 "  white_spaces(), time(<time>), white_spaces(), word();\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces(), add_string_to_column(<name>, \".\"),\r\n"
                                 "  add_string_to_column(<name>, next_word), word(), white_spaces(),\r\n"
                                 "  word(<format>), white_spaces(), number(<lrecl>), white_spaces(),\r\n"
                                 "  number(<records>), white_spaces(), number(<blocks>), white_spaces(),\r\n"
                                 "  year(<date>), all(1), month(<date>), all(1), day(<date>),\r\n"
                                 "  white_spaces(), time(<time>), white_spaces(), word();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* IBMz_VM2Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",         // name
                                     "1,ext,5,\\0,5,\\0,2,\\0",          // type (extension)
                                     "1,format,13,\\0,13,\\0,7,\\0",     // format
                                     "1,lrecl,14,\\0,14,\\0,10,\\0,0",   // lrecl
                                     "1,records,15,\\0,15,\\0,10,\\0,0", // records
                                     "1,blocks,10,\\0,10,\\0,10,\\0,0",  // blocks
                                     "1,date,3,\\0,3,\\0,8,\\0",         // date
                                     "1,time,4,\\0,4,\\0,9,\\0,0",       // time
                                     "1,user,7,\\0,7,\\0,7,\\0"};        // user
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("IBM z/VM (CMS) 2", NULL,
                                 9, IBMz_VM2Columns,
                                 "# parse lines with directories\r\n"
                                 "* word(<name>), all_to(\" DISK\"), assign(<is_dir>, true), white_spaces(),\r\n"
                                 "  word(<format>), white_spaces(), number(<lrecl>), white_spaces(),\r\n"
                                 "  number(<records>), white_spaces(), number(<blocks>), white_spaces(),\r\n"
                                 "  month(<date>), all(1), day(<date>), all(1), year(<date>),\r\n"
                                 "  white_spaces(), time(<time>), white_spaces(), word(<user>);\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces(), add_string_to_column(<name>, \".\"),\r\n"
                                 "  add_string_to_column(<name>, next_word), word(), white_spaces(),\r\n"
                                 "  word(<format>), white_spaces(), number(<lrecl>), white_spaces(),\r\n"
                                 "  number(<records>), white_spaces(), number(<blocks>), white_spaces(),\r\n"
                                 "  month(<date>), all(1), day(<date>), all(1), year(<date>),\r\n"
                                 "  white_spaces(), time(<time>), white_spaces(), word(<user>);\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* IBMz_VM3Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",         // name
                                     "1,ext,5,\\0,5,\\0,2,\\0",          // type (extension)
                                     "1,fm,16,\\0,16,\\0,7,\\0",         // filemode
                                     "1,format,13,\\0,13,\\0,7,\\0",     // format
                                     "1,lrecl,14,\\0,14,\\0,10,\\0,0",   // lrecl
                                     "1,records,15,\\0,15,\\0,10,\\0,0", // records
                                     "1,blocks,10,\\0,10,\\0,10,\\0,0",  // blocks
                                     "1,date,3,\\0,3,\\0,8,\\0",         // date
                                     "1,time,4,\\0,4,\\0,9,\\0,0"};      // time
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("IBM z/VM (CMS) 3", "syst_contains(\"z/VM\")",
                                 9, IBMz_VM3Columns,
                                 "# parse lines with directories\r\n"
                                 "* word(<name>), white_spaces(), word(<fm>), all_to(\" DIR\"),\r\n"
                                 "  assign(<is_dir>, true), white_spaces(), word(), white_spaces(),\r\n"
                                 "  word(), white_spaces(), word(), white_spaces(), month(<date>),\r\n"
                                 "  all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>);\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces(), add_string_to_column(<name>, \".\"),\r\n"
                                 "  add_string_to_column(<name>, next_word), word(), white_spaces(),\r\n"
                                 "  word(<fm>), white_spaces(), word(<format>), white_spaces(),\r\n"
                                 "  number(<lrecl>), white_spaces(), number(<records>), white_spaces(),\r\n"
                                 "  number(<blocks>), white_spaces(), month(<date>), all(1),\r\n"
                                 "  day(<date>), all(1), year(<date>), white_spaces(), time(<time>);\r\n"
                                 "\r\n"
                                 "# skip first line \"Filename FileType Fm ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"Filename\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"FileType\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"Fm\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* mvs1Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",         // name
                                 "1,dsorg,23,\\0,23,\\0,7,\\0",      // dsorg
                                 "1,date,3,\\0,3,\\0,8,\\0,0",       // date
                                 "1,volume,17,\\0,17,\\0,7,\\0",     // volume
                                 "1,unit,18,\\0,18,\\0,7,\\0",       // unit
                                 "1,extents,19,\\0,19,\\0,10,\\0,0", // extents
                                 "1,used,20,\\0,20,\\0,10,\\0,0",    // used
                                 "1,recfm,21,\\0,21,\\0,7,\\0",      // recfm
                                 "1,lrecl,14,\\0,14,\\0,10,\\0,0",   // lrecl
                                 "1,blksz,22,\\0,22,\\0,10,\\0,0"};  // blksz
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("MVS1", "syst_contains(\" MVS \")",
                                 10, mvs1Columns,
                                 "# data sets\r\n"
                                 "* word(<volume>), white_spaces(), word(<unit>), white_spaces(), year(<date>),\r\n"
                                 "  all(1), month(<date>), all(1), day(<date>), white_spaces(), number(<extents>),\r\n"
                                 "  white_spaces(), number(<used>), white_spaces(), word(<recfm>), white_spaces(),\r\n"
                                 "  number(<lrecl>), white_spaces(), number(<blksz>), white_spaces(),\r\n"
                                 "  word(<dsorg>), assign(<is_dir>, <dsorg> eq \"PO\"), white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# data sets with \"**NONE**\" date\r\n"
                                 "* word(<volume>), white_spaces(), word(<unit>), white_spaces(),\r\n"
                                 "  if(next_word eq \"**NONE**\"), word(), white_spaces(), number(<extents>),\r\n"
                                 "  white_spaces(), number(<used>), white_spaces(), word(<recfm>),\r\n"
                                 "  white_spaces(), number(<lrecl>), white_spaces(), number(<blksz>),\r\n"
                                 "  white_spaces(), word(<dsorg>), assign(<is_dir>, <dsorg> eq \"PO\"),\r\n"
                                 "  white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"Migrated\" data sets\r\n"
                                 "* if(next_word eq \"Migrated\"), word(<unit>), white_spaces(),\r\n"
                                 "  word(<name>);\r\n"
                                 "\r\n"
                                 "# \"MIGRAT\" lines\r\n"
                                 "* if(next_word eq \"MIGRAT\"), all(<unit>, 55), white_spaces(),\r\n"
                                 "  cut_white_spaces_end(<unit>), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"ARCIVE\" lines\r\n"
                                 "* if(next_word eq \"ARCIVE\"), all(<unit>, 55), white_spaces(),\r\n"
                                 "  cut_white_spaces_end(<unit>), word(<name>);\r\n"
                                 "\r\n"
                                 "# data sets with joined numbers from \"Ext\" and \"Used\" columns\r\n"
                                 "* word(<volume>), white_spaces(), word(<unit>), white_spaces(), year(<date>),\r\n"
                                 "  all(1), month(<date>), all(1), day(<date>), white_spaces(), word(),\r\n"
                                 "  white_spaces(), word(<recfm>), white_spaces(), number(<lrecl>), white_spaces(),\r\n"
                                 "  number(<blksz>), white_spaces(), word(<dsorg>), assign(<is_dir>, <dsorg> eq \"PO\"),\r\n"
                                 "  white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# data sets with \"**NONE**\" date and with joined numbers from \"Ext\" and \"Used\" columns\r\n"
                                 "* word(<volume>), white_spaces(), word(<unit>), white_spaces(),\r\n"
                                 "  if(next_word eq \"**NONE**\"), word(), white_spaces(), word(), white_spaces(),\r\n"
                                 "  word(<recfm>), white_spaces(), number(<lrecl>), white_spaces(), number(<blksz>),\r\n"
                                 "  white_spaces(), word(<dsorg>), assign(<is_dir>, <dsorg> eq \"PO\"),\r\n"
                                 "  white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"Pseudo directories\"\r\n"
                                 "* if(next_word eq \"Pseudo\"), word(), white_spaces(1), if(next_word eq \"Directory\"),\r\n"
                                 "  assign(<is_dir>, true), word(), white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"Error determining attributes\"\r\n"
                                 "* if(next_word eq \"Error\"), word(), white_spaces(1), if(next_word eq \"determining\"),\r\n"
                                 "  word(), white_spaces(1), if(next_word eq \"attributes\"), word(), white_spaces(),\r\n"
                                 "  assign(<unit>, \"Error determining attributes\"), word(<name>);\r\n"
                                 "\r\n"
                                 "# Volume + \"Error determining attributes\"\r\n"
                                 "* word(<volume>), white_spaces(), if(next_word eq \"Error\"), word(),\r\n"
                                 "  white_spaces(1), if(next_word eq \"determining\"), word(), white_spaces(1),\r\n"
                                 "  if(next_word eq \"attributes\"), word(), white_spaces(),\r\n"
                                 "  assign(<unit>, \"Error determining attributes\"), word(<name>);\r\n"
                                 "\r\n"
                                 "# Volume + \"Not Mounted\"\r\n"
                                 "* word(<volume>), white_spaces(), if(next_word eq \"Not\"), word(),\r\n"
                                 "  white_spaces(1), if(next_word eq \"Mounted\"), word(), white_spaces(),\r\n"
                                 "  assign(<unit>, \"Not Mounted\"), word(<name>);\r\n"
                                 "\r\n"
                                 "# Volume + Unit + \"<\" + any error message + \" > \" + Dsname\r\n"
                                 "* all(<volume>, 6), white_spaces(1), all(<unit>, 6), white_spaces(),\r\n"
                                 "  if(next_word == \"<\"), all_to(<name>, \" > \"), cut_white_spaces(<volume>),\r\n"
                                 "  cut_white_spaces(<unit>), add_string_to_column(<unit>, \" \"),\r\n"
                                 "  add_string_to_column(<unit>, <name>), cut_white_spaces_end(<unit>),\r\n"
                                 "  skip_white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# optional columns: Volume + Unit + Recfm + Dsorg; Dsname is necessary\r\n"
                                 "* all(<volume>, 6), white_spaces(1), all(<unit>, 6), white_spaces(20),\r\n"
                                 "  all(<recfm>, 5), white_spaces(12), all(<dsorg>, 5), white_spaces(),\r\n"
                                 "  word(<name>), cut_white_spaces(<volume>), cut_white_spaces(<unit>),\r\n"
                                 "  cut_white_spaces(<recfm>), cut_white_spaces(<dsorg>);\r\n"
                                 "\r\n"
                                 "# skip first line \"Volume Unit Referred ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"Volume\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"Unit\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"Referred\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("MVS2", "syst_contains(\" MVS \")",
                                 10, mvs1Columns,
                                 "# data sets\r\n"
                                 "* word(<volume>), white_spaces(), word(<unit>), white_spaces(), month(<date>),\r\n"
                                 "  all(1), day(<date>), all(1), year(<date>), white_spaces(), number(<extents>),\r\n"
                                 "  white_spaces(), number(<used>), white_spaces(), word(<recfm>), white_spaces(),\r\n"
                                 "  number(<lrecl>), white_spaces(), number(<blksz>), white_spaces(),\r\n"
                                 "  word(<dsorg>), assign(<is_dir>, <dsorg> eq \"PO\"), white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# data sets with \"**NONE**\" date\r\n"
                                 "* word(<volume>), white_spaces(), word(<unit>), white_spaces(),\r\n"
                                 "  if(next_word eq \"**NONE**\"), word(), white_spaces(), number(<extents>),\r\n"
                                 "  white_spaces(), number(<used>), white_spaces(), word(<recfm>),\r\n"
                                 "  white_spaces(), number(<lrecl>), white_spaces(), number(<blksz>),\r\n"
                                 "  white_spaces(), word(<dsorg>), assign(<is_dir>, <dsorg> eq \"PO\"),\r\n"
                                 "  white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"Migrated\" data sets\r\n"
                                 "* if(next_word eq \"Migrated\"), word(<unit>), white_spaces(),\r\n"
                                 "  word(<name>);\r\n"
                                 "\r\n"
                                 "# \"MIGRAT\" lines\r\n"
                                 "* if(next_word eq \"MIGRAT\"), all(<unit>, 53), white_spaces(),\r\n"
                                 "  cut_white_spaces_end(<unit>), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"ARCIVE\" lines\r\n"
                                 "* if(next_word eq \"ARCIVE\"), all(<unit>, 53), white_spaces(),\r\n"
                                 "  cut_white_spaces_end(<unit>), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"Pseudo directories\"\r\n"
                                 "* if(next_word eq \"Pseudo\"), word(), white_spaces(1), if(next_word eq \"Directory\"),\r\n"
                                 "  assign(<is_dir>, true), word(), white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# \"Error determining attributes\"\r\n"
                                 "* if(next_word eq \"Error\"), word(), white_spaces(1), if(next_word eq \"determining\"),\r\n"
                                 "  word(), white_spaces(1), if(next_word eq \"attributes\"), word(), white_spaces(),\r\n"
                                 "  assign(<unit>, \"Error determining attributes\"), word(<name>);\r\n"
                                 "\r\n"
                                 "# Volume + \"Error determining attributes\"\r\n"
                                 "* word(<volume>), white_spaces(), if(next_word eq \"Error\"), word(),\r\n"
                                 "  white_spaces(1), if(next_word eq \"determining\"), word(), white_spaces(1),\r\n"
                                 "  if(next_word eq \"attributes\"), word(), white_spaces(),\r\n"
                                 "  assign(<unit>, \"Error determining attributes\"), word(<name>);\r\n"
                                 "\r\n"
                                 "# Volume + \"Not Mounted\"\r\n"
                                 "* word(<volume>), white_spaces(), if(next_word eq \"Not\"), word(),\r\n"
                                 "  white_spaces(1), if(next_word eq \"Mounted\"), word(), white_spaces(),\r\n"
                                 "  assign(<unit>, \"Not Mounted\"), word(<name>);\r\n"
                                 "\r\n"
                                 "# Volume + Unit + \"<\" + any error message + \" > \" + Dsname\r\n"
                                 "* all(<volume>, 6), white_spaces(1), all(<unit>, 4), white_spaces(),\r\n"
                                 "  if(next_word == \"<\"), all_to(<name>, \" > \"), cut_white_spaces(<volume>),\r\n"
                                 "  cut_white_spaces(<unit>), add_string_to_column(<unit>, \" \"),\r\n"
                                 "  add_string_to_column(<unit>, <name>), cut_white_spaces_end(<unit>),\r\n"
                                 "  skip_white_spaces(), word(<name>);\r\n"
                                 "\r\n"
                                 "# optional columns: Volume + Unit + Recfm + Dsorg; Dsname is necessary\r\n"
                                 "* all(<volume>, 6), white_spaces(1), all(<unit>, 4), white_spaces(19),\r\n"
                                 "  all(<recfm>, 5), white_spaces(13), all(<dsorg>, 5), white_spaces(),\r\n"
                                 "  word(<name>), cut_white_spaces(<volume>), cut_white_spaces(<unit>),\r\n"
                                 "  cut_white_spaces(<recfm>), cut_white_spaces(<dsorg>);\r\n"
                                 "\r\n"
                                 "# skip first line \"Volume Unit Referred ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"Volume\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"Unit\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"Referred\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* mvsPO1Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",        // name
                                   "1,size,15,\\0,15,\\0,10,\\0,0",   // size
                                   "1,vvmm,24,\\0,24,\\0,7,\\0",      // VV.MM
                                   "1,created,25,\\0,25,\\0,8,\\0,0", // created
                                   "1,ch_date,3,\\0,3,\\0,8,\\0,0",   // changed date
                                   "1,ch_time,4,\\0,4,\\0,9,\\0,0",   // changed time
                                   "1,init,26,\\0,26,\\0,10,\\0,0",   // init size
                                   "1,mod,27,\\0,27,\\0,10,\\0,0",    // mod
                                   "1,id,28,\\0,28,\\0,7,\\0"};       // id
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("MVS PO 1", "syst_contains(\" MVS \")",
                                 9, mvsPO1Columns,
                                 "# members\r\n"
                                 "* word(<name>), white_spaces(), word(<vvmm>), white_spaces(), year(<created>),\r\n"
                                 "  all(1), month(<created>), all(1), day(<created>), white_spaces(),\r\n"
                                 "  year(<ch_date>), all(1), month(<ch_date>), all(1), day(<ch_date>),\r\n"
                                 "  white_spaces(), time(<ch_time>), white_spaces(), number(<size>),\r\n"
                                 "  white_spaces(), number(<init>), white_spaces(), number(<mod>),\r\n"
                                 "  white_spaces(), word(<id>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# members without any other info\r\n"
                                 "* word(<name>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip first line \"Name VV.MM Created ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"Name\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"VV.MM\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"Created\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("MVS PO 2", "syst_contains(\" MVS \")",
                                 9, mvsPO1Columns,
                                 "# members\r\n"
                                 "* word(<name>), white_spaces(), word(<vvmm>), white_spaces(),\r\n"
                                 "  month(<created>), all(1), day(<created>), all(1), year(<created>),\r\n"
                                 "  white_spaces(), month(<ch_date>), all(1), day(<ch_date>), all(1),\r\n"
                                 "  year(<ch_date>), white_spaces(), time(<ch_time>), white_spaces(),\r\n"
                                 "  number(<size>), white_spaces(), number(<init>), white_spaces(),\r\n"
                                 "  number(<mod>), white_spaces(), word(<id>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# members without any other info\r\n"
                                 "* word(<name>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip first line \"Name VV.MM Created ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"Name\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"VV.MM\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"Created\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* mvsPO3Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",      // name
                                   "1,size,15,\\0,15,\\0,10,\\0,0", // size
                                   "1,vvmm,24,\\0,24,\\0,7,\\0",    // VV.MM
                                   "1,ch_date,3,\\0,3,\\0,8,\\0,0", // changed date
                                   "1,ch_time,4,\\0,4,\\0,9,\\0,0", // changed time
                                   "1,init,26,\\0,26,\\0,10,\\0,0", // init size
                                   "1,mod,27,\\0,27,\\0,10,\\0,0",  // mod
                                   "1,id,28,\\0,28,\\0,7,\\0"};     // id
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("MVS PO 3", "syst_contains(\" MVS \")",
                                 8, mvsPO3Columns,
                                 "# members\r\n"
                                 "* word(<name>), white_spaces(), word(<vvmm>), white_spaces(),\r\n"
                                 "  year(<ch_date>), all(1), month(<ch_date>), all(1), day(<ch_date>),\r\n"
                                 "  white_spaces(), time(<ch_time>), white_spaces(), number(<size>),\r\n"
                                 "  white_spaces(), number(<init>), white_spaces(), number(<mod>),\r\n"
                                 "  white_spaces(), word(<id>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# members without any other info\r\n"
                                 "* word(<name>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip first line \"NAME VV.MM CHANGED ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"NAME\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"VV.MM\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"CHANGED\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* mvsPO4Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",     // name
                                   "1,size,15,\\0,15,\\0,7,\\0",   // size
                                   "1,ttr,29,\\0,29,\\0,7,\\0",    // TTR
                                   "1,alias,30,\\0,30,\\0,7,\\0",  // alias-of
                                   "1,ac,31,\\0,31,\\0,7,\\0",     // AC
                                   "1,attr,32,\\0,32,\\0,7,\\0",   // attr
                                   "1,amode,33,\\0,33,\\0,7,\\0",  // amode
                                   "1,rmode,34,\\0,34,\\0,7,\\0"}; // rmode
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("MVS PO 4", "syst_contains(\" MVS \")",
                                 8, mvsPO4Columns,
                                 "#datasets\r\n"
                                 "* word(<name>), white_spaces(), word(<size>), white_spaces(), word(<ttr>),\r\n"
                                 "  white_spaces(1), all(<alias>, 8), white_spaces(1), all(<ac>, 2), white_spaces(1),\r\n"
                                 "  all(<attr>, 30), cut_white_spaces_end(<alias>), cut_white_spaces_end(<ac>),\r\n"
                                 "  cut_white_spaces_end(<attr>), white_spaces(), word(<amode>), white_spaces(),\r\n"
                                 "  word(<rmode>), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip first line \"Name Size TTR ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"Name\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"Size\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"TTR\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* IISColumns[] = {"1,name,0,\\0,0,\\0,1,\\0",  // name
                                "1,ext,1,\\0,1,\\0,2,\\0",   // extension
                                "1,size,2,\\0,2,\\0,3,\\0",  // size
                                "0,type,5,\\0,5,\\0,6,\\0",  // type
                                "1,date,3,\\0,3,\\0,4,\\0",  // date
                                "1,time,4,\\0,4,\\0,5,\\0"}; // time
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("Microsoft IIS", NULL,
                                 6, IISColumns,
                                 "# parse lines with files\r\n"
                                 "* month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), white_spaces(), positive_number(<size>), white_spaces(),\r\n"
                                 "  rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with directories\r\n"
                                 "* month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), white_spaces(), if(next_word eq \"<dir>\"), all(5),\r\n"
                                 "  assign(<is_dir>, true), white_spaces(), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip lines with files without name\r\n"
                                 "* month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), white_spaces(), positive_number(<size>), white_spaces();\r\n"
                                 "\r\n"
                                 "# skip lines with directories without name\r\n"
                                 "* month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), white_spaces(), if(next_word eq \"<dir>\"), all(5),\r\n"
                                 "  assign(<is_dir>, true), white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* os2Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",     // name
                                "1,ext,1,\\0,1,\\0,2,\\0",      // extension
                                "1,size,2,\\0,2,\\0,3,\\0",     // size
                                "0,type,5,\\0,5,\\0,6,\\0",     // type
                                "1,date,3,\\0,3,\\0,4,\\0",     // date
                                "1,time,4,\\0,4,\\0,9,\\0,0",   // time
                                "1,attrs,32,\\0,32,\\0,7,\\0"}; // attributes
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("OS/2", NULL,
                                 7, os2Columns,
                                 "# parse lines with files and directories\r\n"
                                 "* skip_white_spaces(), positive_number(<size>), white_spaces(1), all(<attrs>, 9),\r\n"
                                 "  cut_white_spaces(<attrs>), white_spaces(1),\r\n"
                                 "  assign(<is_dir>, next_word eq \"DIR\"), all(3), white_spaces(),\r\n"
                                 "  month(<date>), all(1), day(<date>), all(1), year(<date>),\r\n"
                                 "  white_spaces(), time(<time>), white_spaces(), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    const char* vxWorksColumns[] = {"1,name,0,\\0,0,\\0,1,\\0",    // name
                                    "1,ext,1,\\0,1,\\0,2,\\0",     // extension
                                    "1,size,2,\\0,2,\\0,3,\\0",    // size
                                    "0,type,5,\\0,5,\\0,6,\\0",    // type
                                    "1,date,3,\\0,3,\\0,4,\\0",    // date
                                    "1,time,4,\\0,4,\\0,9,\\0,0"}; // time
    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("VxWorks", "welcome_contains(\" VxWorks \")",
                                 6, vxWorksColumns,
                                 "# parse lines with directories\r\n"
                                 "* skip_white_spaces(), positive_number(<size>), white_spaces(), month_3(<date>),\r\n"
                                 "  all(1), day(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  white_spaces(), all_up_to(<name>, \"<\"), cut_white_spaces_end(<name>),\r\n"
                                 "  if(next_word eq \"DIR>\"), all(4), assign(<is_dir>, true), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# parse lines with files\r\n"
                                 "* skip_white_spaces(), positive_number(<size>), white_spaces(), month_3(<date>),\r\n"
                                 "  all(1), day(<date>), all(1), year(<date>), white_spaces(), time(<time>),\r\n"
                                 "  white_spaces(), rest_of_line(<name>), cut_white_spaces_end(<name>),\r\n"
                                 "  if(\"<\" not_in <name>);\r\n"
                                 "\r\n"
                                 "# skip first line \"size date time ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"size\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"date\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"time\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip second line \"--------  ------ ...\"\r\n"
                                 "* if(\"----\" in next_word), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // popis retezce v poli: "visible,ID,nameStrID,nameStr,descrStrID,descrStr,colType,emptyValue,leftAlignment,fixedWidth,width"
    const char* tandemColumns[] = {"1,name,0,\\0,0,\\0,1,\\0",      // name
                                   "1,code,35,\\0,35,\\0,10,\\0,0", // code
                                   "1,size,2,\\0,2,\\0,3,\\0",      // size
                                   "1,date,3,\\0,3,\\0,4,\\0",      // date
                                   "1,time,4,\\0,4,\\0,5,\\0",      // time
                                   "1,user,7,\\0,7,\\0,7,\\0",      // user
                                   "1,RWEP,36,\\0,36,\\0,7,\\0"};   // RWEP

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("Tandem", NULL, 7, tandemColumns,
                                 "# parse lines with files\r\n"
                                 "* word(<name>), white_spaces(), number(<code>), white_spaces(), positive_number(<size>),\r\n"
                                 "  white_spaces(), day(<date>), all(1), month_3(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), white_spaces(), all_up_to(<user>, \"\\\"\"), cut_white_spaces_end(<user>),\r\n"
                                 "  all_up_to(<RWEP>, \"\\\"\");\r\n"
                                 "\r\n"
                                 "# skip first line \"File Code EOF ...\"\r\n"
                                 "* if(first_nonempty_line), skip_white_spaces(), if (next_word eq \"file\"), word(),\r\n"
                                 "  skip_white_spaces(), if (next_word eq \"code\"), word(), skip_white_spaces(),\r\n"
                                 "  if (next_word eq \"eof\"), rest_of_line();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // popis retezce v poli: "visible,ID,nameStrID,nameStr,descrStrID,descrStr,colType,emptyValue,leftAlignment,fixedWidth,width"
    const char* IBM_AS_400Columns[] = {"1,name,0,\\0,0,\\0,1,\\0",     // name
                                       "1,ext,1,\\0,1,\\0,2,\\0",      // extension
                                       "1,size,2,\\0,2,\\0,10,\\0,0",  // size
                                       "0,win_type,5,\\0,5,\\0,6,\\0", // windows file type
                                       "1,date,3,\\0,3,\\0,8,\\0,0",   // date
                                       "1,time,4,\\0,4,\\0,9,\\0,0",   // time
                                       "1,type,5,\\0,5,\\0,7,\\0",     // AS/400 file type
                                       "1,user,7,\\0,7,\\0,7,\\0"};    // user

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("IBM iSeries/i5, AS/400 (EN)", "syst_contains(\" OS/400 \")", 8, IBM_AS_400Columns,
                                 "# parse lines with files\r\n"
                                 "* word(<user>), skip_white_spaces(), positive_number(<size>), skip_white_spaces(),\r\n"
                                 "  month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line not_end_with \"/\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files without size+date+time\r\n"
                                 "* word(<user>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line not_end_with \"/\"), if(\" \" not_in rest_of_line),\r\n"
                                 "  rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files without user+type+size+date+time\r\n"
                                 "* white_spaces(40), skip_white_spaces(), if(rest_of_line not_end_with \"/\"),\r\n"
                                 "  rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with directories\r\n"
                                 "* word(<user>), skip_white_spaces(), positive_number(<size>), skip_white_spaces(),\r\n"
                                 "  month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line end_with \"/\"), assign(<is_dir>, true), rest_of_line(<name>),\r\n"
                                 "  cut_end_of_string(<name>, 1);\r\n"
                                 "\r\n"
                                 "# parse lines with directories without user\r\n"
                                 "* skip_white_spaces(), positive_number(<size>), skip_white_spaces(),\r\n"
                                 "  month(<date>), all(1), day(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line end_with \"/\"), assign(<is_dir>, true), rest_of_line(<name>),\r\n"
                                 "  cut_end_of_string(<name>, 1);\r\n"
                                 "\r\n"
                                 "# parse lines with directories without user+type+size+date+time\r\n"
                                 "* white_spaces(40), skip_white_spaces(), if(rest_of_line end_with \"/\"),\r\n"
                                 "  assign(<is_dir>, true), rest_of_line(<name>), cut_end_of_string(<name>, 1);\r\n"
                                 "\r\n"
                                 "# parse lines with file without name (only on the first row)\r\n"
                                 "* if(first_nonempty_line), word(), skip_white_spaces(), positive_number(<size>),\r\n"
                                 "  skip_white_spaces(), month(<date>), all(1), day(<date>), all(1), year(<date>),\r\n"
                                 "  white_spaces(), time(<time>), skip_white_spaces(), if(next_word eq \"*FILE\"),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    // neni treba pouzivat LockServerTypeList() a UnlockServerTypeList(), jeste zadne dalsi thready nebezi
    ServerTypeList.AddServerType("IBM iSeries/i5, AS/400 (CZ)", "syst_contains(\" OS/400 \")", 8, IBM_AS_400Columns,
                                 "# parse lines with files\r\n"
                                 "* word(<user>), skip_white_spaces(), positive_number(<size>), skip_white_spaces(),\r\n"
                                 "  day(<date>), all(1), month(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line not_end_with \"/\"), rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files without size+date+time\r\n"
                                 "* word(<user>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line not_end_with \"/\"), if(\" \" not_in rest_of_line),\r\n"
                                 "  rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with files without user+type+size+date+time\r\n"
                                 "* white_spaces(40), skip_white_spaces(), if(rest_of_line not_end_with \"/\"),\r\n"
                                 "  rest_of_line(<name>);\r\n"
                                 "\r\n"
                                 "# parse lines with directories\r\n"
                                 "* word(<user>), skip_white_spaces(), positive_number(<size>), skip_white_spaces(),\r\n"
                                 "  day(<date>), all(1), month(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line end_with \"/\"), assign(<is_dir>, true), rest_of_line(<name>),\r\n"
                                 "  cut_end_of_string(<name>, 1);\r\n"
                                 "\r\n"
                                 "# parse lines with directories without user\r\n"
                                 "* skip_white_spaces(), positive_number(<size>), skip_white_spaces(),\r\n"
                                 "  day(<date>), all(1), month(<date>), all(1), year(<date>), white_spaces(),\r\n"
                                 "  time(<time>), skip_white_spaces(), word(<type>), skip_white_spaces(),\r\n"
                                 "  if(rest_of_line end_with \"/\"), assign(<is_dir>, true), rest_of_line(<name>),\r\n"
                                 "  cut_end_of_string(<name>, 1);\r\n"
                                 "\r\n"
                                 "# parse lines with directories without user+type+size+date+time\r\n"
                                 "* white_spaces(40), skip_white_spaces(), if(rest_of_line end_with \"/\"),\r\n"
                                 "  assign(<is_dir>, true), rest_of_line(<name>), cut_end_of_string(<name>, 1);\r\n"
                                 "\r\n"
                                 "# parse lines with file without name (only on the first row)\r\n"
                                 "* if(first_nonempty_line), word(), skip_white_spaces(), positive_number(<size>),\r\n"
                                 "  skip_white_spaces(), day(<date>), all(1), month(<date>), all(1), year(<date>),\r\n"
                                 "  white_spaces(), time(<time>), skip_white_spaces(), if(next_word eq \"*FILE\"),\r\n"
                                 "  word(), skip_white_spaces();\r\n"
                                 "\r\n"
                                 "# skip empty lines anywhere\r\n"
                                 "* skip_white_spaces();\r\n");

    if ((CommandHistory[0] = _strdup("HELP")) != NULL)
        CommandHistory[1] = _strdup("CDUP");

    ASCIIFileMasks = SalamanderGeneral->AllocSalamanderMaskGroup();
    if (ASCIIFileMasks != NULL)
    {
        ASCIIFileMasks->SetMasksString("*.txt;*.*htm;*.*html;*.pl;*.php;*.php3;*.asp;*.cgi;*.css;*.bat;*.tcl;"
                                       "*.diz;*.nfo;*.ini;*.mak;*.cpp;*.c;*.h;*.bas;*.pas;*.tex;*.log",
                                       FALSE);
        return TRUE;
    }
    else
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
}

void CConfiguration::ReleaseDataFromSalamanderGeneral()
{
    SalamanderGeneral->FreeSalamanderMaskGroup(ASCIIFileMasks);
    FTPServerList.DestroyMembers();      // pro jistotu (kdyby se dealokovalo pres SalamanderGeneral)
    FTPProxyServerList.DestroyMembers(); // pro jistotu (kdyby se dealokovalo pres SalamanderGeneral)
}

void CConfiguration::GetAnonymousPasswd(char* buf, int bufSize)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    lstrcpyn(buf, AnonymousPasswd, bufSize);
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

void CConfiguration::SetAnonymousPasswd(const char* passwd)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    lstrcpyn(AnonymousPasswd, passwd, PASSWORD_MAX_SIZE);
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

int CConfiguration::GetServerRepliesTimeout()
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    int ret = ServerRepliesTimeout;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
    return ret;
}

int CConfiguration::GetNoDataTransferTimeout()
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    int ret = NoDataTransferTimeout;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
    return ret;
}

int CConfiguration::GetDelayBetweenConRetries()
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    if (DelayBetweenConRetries < 1)
        DelayBetweenConRetries = 1;
    int ret = DelayBetweenConRetries;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
    return ret;
}

int CConfiguration::GetConnectRetries()
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    int ret = ConnectRetries;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
    return ret;
}

int CConfiguration::GetResumeOverlap()
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    int ret = ResumeOverlap;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
    return ret;
}

int CConfiguration::GetResumeMinFileSize()
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    int ret = ResumeMinFileSize;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
    return ret;
}

void CConfiguration::SetServerRepliesTimeout(int value)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    ServerRepliesTimeout = value;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

void CConfiguration::SetNoDataTransferTimeout(int value)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    NoDataTransferTimeout = value;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

void CConfiguration::SetDelayBetweenConRetries(int value)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    DelayBetweenConRetries = value;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

void CConfiguration::SetConnectRetries(int value)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    ConnectRetries = value;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

void CConfiguration::SetResumeOverlap(int value)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    ResumeOverlap = value;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}

void CConfiguration::SetResumeMinFileSize(int value)
{
    HANDLES(EnterCriticalSection(&ConParamsCS));
    ResumeMinFileSize = value;
    HANDLES(LeaveCriticalSection(&ConParamsCS));
}
