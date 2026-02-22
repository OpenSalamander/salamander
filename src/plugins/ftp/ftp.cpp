// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

// plugin interface object, its methods are called from Salamander
CPluginInterface PluginInterface;
// other parts of the CPluginInterface interface
CPluginInterfaceForFS InterfaceForFS;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

// ConfigVersion: 0 - default (without load),
//                1 - work version before listing parsing support (from this version on there are sensible lists of server-types and ftp-servers)
//                2 - work version before the first MVS adjustments according to tester Michael Knigge (server-type list adjustment)
//                3 - work version before the second MVS adjustments according to tester Michael Knigge (server-type list adjustment)
//                4 - work version before the third MVS adjustments according to tester Michael Knigge (server-type list adjustment)
//                5 - work version before the first VMS adjustments (server-type list adjustment)
//                6 - work version before the fourth MVS adjustments according to tester Michael Knigge (server-type list adjustment)
//                7 - work version before modifying values in the configuration on the Operations page
//                8 - work version before Servant Salamander 2.1 beta 1
//                9 - Servant Salamander 2.5 beta 6 (+adjustments to server-type autodetect conditions)
//                10 - added parser for Unix systems with two spaces before the file/directory name in the listing (Filezilla + AIX)
//                11 - fixed parser for Unix systems with two spaces before the file/directory name in the listing (Filezilla + AIX)
//                12 - priority change: the parser for Unix systems with two spaces after the year before names comes before the parser with one space after the year before names (the variant without spaces at the beginning of names is more likely) + enriched MVS1 and MVS2 parsers
//                13 - fixed column descriptions in the MVS PO parser (all four): used "Number of Records" instead of "Size"
//                14 - fixed the MVS PO 4 parser: the value can be missing both in column "AC" and in column "Alias" - now reading them directly by offsets + trimming trailing spaces, there is no other way + fixed the MVS1 and MVS2 parsers (volume OK + followed by "Error determining attributes")
//                     + fixed the column names in the MVS PO parser (all four): used "Records" instead of "Size" - the size is not in bytes, nonsensical progress was shown during download
//                15 - added parser for OS/2 FTP server
//                16 - added parser for VxWorks FTP server
//                17 - fixed the MVS parser (another type of error message in the listing + another combination of skipped data in the listing) + fixed the VxWorks parser
//                18 - added alignment to parser columns - we usually align dates+times+numbers to the right
//                19 - adjusted the z/VM parser: Date is of type GeneralDate so that item ".." shows an empty string instead of "1.1.1602"
//                20 - added the "is_link" identifier (TRUE = the panel icon has a link overlay)
//                21 - added the UNIX4 parser (German dates - months longer than 3 letters), added functions "month_txt"
//                22 - modified values in the configuration on the Operations 2 (Upload) page
//                23 - adjusted parser autodetection conditions that skip the first or last rows of the listing (if Microsoft IIS or Netprezenz returned a listing with one or two rows, the VMS parser simply ignored it and an empty listing was used)
//                24 - adjusted skipping listing headers in parsers, now an invalid row is not skipped when a header is missing
//                25 - adjusted the Microsoft IIS parser: directories containing spaces instead of names are ignored (I do not understand why they are even shown when they have no name, but they are)
//                26 - added UNIX5 (IBM AIX - German version) parser (the month and day columns are swapped)
//                27 - adjusted the VMS1-4 parsers, novelty on cs.felk.cvut.cz: an empty directory returns a listing containing "Total of 0 files, 0/0 blocks" (the listing previously returned the error "no files found")
//                28 - added parser for Tandem
//                29 - default change: "Resume or Overwrite" instead of "Overwrite" for Config.UploadRetryOnCreatedFile; Config.DisableLoggingOfWorkers changed to FALSE (I am tired of writing to everyone to enable logging, the logging overhead is minimal)
//                30 - added parser for IBM AS/400
//                31 - adjusted parser for IBM AS/400
//                32 - renamed parser for IBM AS/400 to "IBM iSeries/i5, AS/400"
//                33 - adjusted parser for IBM AS/400: names must not contain spaces (otherwise it nonsensically parses, for example, unparsable Unix listings)
//                34 - adjusted all five UNIX parsers: user+group can contain more spaces (in that case they are ignored because we do not know how to separate user from group)
//                35 - added UNIX parser for MOXA FTP server (missing times + dates); change in all UNIX parsers: reading the <rights> column is no longer done by fixed 10 characters but by a word (reason: ACL on Unix introduced '+' after those ten characters, e.g. "drwxrwxr-x+")
//                36 - change in the CZ+EN parser for IBM AS/400: ignore the first row with an empty file name
//                37 - added parser for Xbox 360

int ConfigVersion = 0;
#define CURRENT_CONFIG_VERSION 37
#define RELOAD_PARSERS_BEFORE_CONFIG_VERSION 37 // parsers are not read from configurations before this version; defaults are used (primitive parser update for users)

// names of values in the configuration (in the registry)
const char* CONFIG_VERSION = "Version";
const char* CONFIG_LASTCFGPAGE = "Last Config Page";

const char* CONFIG_SHOWWELCOMEMESSAGE = "Show Welcome Message";
const char* CONFIG_PRIORITYTOPANELCON = "Priority to Panel Connections";
const char* CONFIG_ENABLETOTALSPEEDLIM = "Enable Total Speed Limit";
const char* CONFIG_TOTALSPEEDLIMIT = "Total Speed Limit";
const char* CONFIG_ANONYMOUSPASSWD = "Anonymous Password";
const char* CONFIG_OPERDLGPOSITION = "OperDlg Position";
const char* CONFIG_OPERDLGSPLITPOS = "OperDlg Split Pos.";
const char* CONFIG_OPERDLGCLOSEIFSUCCESS = "Close OperDlg If Successfully Finished";
const char* CONFIG_OPERDLGCLOSEWHENFINISHES = "Close OperDlg When Oper Finishes";
const char* CONFIG_OPENSOLVEERRIFIDLE = "Open SolveErrDlg If Idle";
const char* CONFIG_SIMPLELSTCOLFIXEDWIDTH = "Simple Listing Fixed Column Width";
const char* CONFIG_SIMPLELSTCOLWIDTH = "Simple Listing Column Width";

const char* CONFIG_PASSIVEMODE = "Passive Mode";
const char* CONFIG_KEEPALIVE = "Keep Alive";
const char* CONFIG_USEMAXCON = "Max. Connections";
const char* CONFIG_SPEEDLIM = "Speed Limit";
const char* CONFIG_TRANSFERMODE = "Transfer Mode";
const char* CONFIG_USELISTINGSCACHE = "Use Listings Cache";
const char* CONFIG_ASCIIMASKS = "ASCII File Masks";
const char* CONFIG_COMPRESSDATA = "Compress Data";

const char* CONFIG_SRVREPTIMEOUT = "Server Replies Timeout";
const char* CONFIG_NODATATRTIMEOUT = "No Data Transfer Timeout";
const char* CONFIG_DELBETWCONRETR = "Delay Connect Retries";
const char* CONFIG_CONATTEMPTS = "Connect Attempts";
const char* CONFIG_RESUMEOVERLAP = "Resume Overlap";
const char* CONFIG_RESUMEMINFILESIZE = "Resume Min File Size";
const char* CONFIG_KASENDEVERY = "Keep Alive - Every";
const char* CONFIG_KASTOPAFTER = "Keep Alive - Stop After";
const char* CONFIG_KACOMMAND = "Keep Alive - Command";
const char* CONFIG_CACHEMAXSIZE = "Mem Cache Max Size";

const char* CONFIG_LASTBOOKMARK = "Last Bookmark";

const char* CONFIG_DOWNLOADADDTOQUEUE = "Download Add To Queue";
const char* CONFIG_DELETEADDTOQUEUE = "Delete Add To Queue";
const char* CONFIG_CHATTRADDTOQUEUE = "ChngAttr Add To Queue";

const char* CONFIG_OPERCANNOTCREATEFILE = "If Cannot Create File";
const char* CONFIG_OPERCANNOTCREATEDIR = "If Cannot Create Dir";
const char* CONFIG_OPERFILEALREADYEXISTS = "If File Already Exists";
const char* CONFIG_OPERDIRALREADYEXISTS = "If Dir Already Exists";
const char* CONFIG_OPERRETRYONCREATFILE = "If Retry On Created";
const char* CONFIG_OPERRETRYONRESUMFILE = "If Retry On Resumed";
const char* CONFIG_OPERASCIITRMODEFORBIN = "If Ascii Mode For Binary File";
const char* CONFIG_OPERUNKNOWNATTRS = "If Unknown Attrs";
const char* CONFIG_OPERNONEMPTYDIRDEL = "If Directory Is Not Empty";
const char* CONFIG_OPERHIDDENFILEDEL = "If File Is Hidden";
const char* CONFIG_OPERHIDDENDIRDEL = "If Dir Is Hidden";

const char* CONFIG_UPLOADCANNOTCREATEFILE = "Upload - If Cannot Create File";
const char* CONFIG_UPLOADCANNOTCREATEDIR = "Upload - If Cannot Create Dir";
const char* CONFIG_UPLOADFILEALREADYEXISTS = "Upload - If File Already Exists";
const char* CONFIG_UPLOADDIRALREADYEXISTS = "Upload - If Dir Already Exists";
const char* CONFIG_UPLOADRETRYONCREATFILE = "Upload - If Retry On Created";
const char* CONFIG_UPLOADRETRYONRESUMFILE = "Upload - If Retry On Resumed";
const char* CONFIG_UPLOADASCIITRMODEFORBIN = "Upload - If Ascii Mode For Binary File";

const char* CONFIG_SERVERTYPES = "Server Types";
const char* CONFIG_STNAME = "Name";
const char* CONFIG_STADCOND = "Autodetect Condition";
const char* CONFIG_STCOLUMNS = "Columns";
const char* CONFIG_STRULESFORPARS = "Rules For Parsing";

const char* CONFIG_FTPSERVERLIST = "Bookmarks";
const char* CONFIG_FTPSRVNAME = "Name";
const char* CONFIG_FTPSRVADDRESS = "Address";
const char* CONFIG_FTPSRVPATH = "Initial Path";
const char* CONFIG_FTPSRVANONYM = "Anonymous";
const char* CONFIG_FTPSRVUSER = "User";
const char* CONFIG_FTPSRVPASSWD_OLD = "Password"; // import only, scrambled by FTP plugin
const char* CONFIG_FTPSRVPASSWD_SCRAMBLED = "PasswordS";
const char* CONFIG_FTPSRVPASSWD_ENCRYPTED = "PasswordE";
const char* CONFIG_FTPSRVSAVEPASSWD = "Save Password";
const char* CONFIG_FTPSRVPROXYSRVUID = "Proxy Server UID";
const char* CONFIG_FTPSRVTGTPATH = "Target Path";
const char* CONFIG_FTPSRVTYPE = "Server Type";
const char* CONFIG_FTPSRVTRANSFMODE = "Transfer Mode";
const char* CONFIG_FTPSRVPORT = "Port";
const char* CONFIG_FTPSRVPASV = "Passive Mode";
const char* CONFIG_FTPSRVKALIVE = "Keep Alive";
const char* CONFIG_FTPSRVKASENDEVERY = "Keep Alive - Every";
const char* CONFIG_FTPSRVKASTOPAFTER = "Keep Alive - Stop After";
const char* CONFIG_FTPSRVKACOMMAND = "Keep Alive - Command";
const char* CONFIG_FTPSRVUSEMAXCON = "Max. Connections";
const char* CONFIG_FTPSRVSPDLIM = "Speed Limit";
const char* CONFIG_FTPSRVUSELISTINGSCACHE = "Use Listings Cache";
const char* CONFIG_FTPSRVINITFTPCMDS = "Initial FTP Commands";
const char* CONFIG_FTPSRVLISTCMD = "List Command";
const char* CONFIG_FTPSRVENCRYPTCONTROLCONNECTION = "Encrypt Control Connection";
const char* CONFIG_FTPSRVENCRYPTDATACONNECTION = "Encrypt Data Connection";
const char* CONFIG_FTPSRVCOMPRESSDATA = "Compress Data";

const char* CONFIG_FTPPROXYLIST = "Proxy Servers";
const char* CONFIG_FTPPRXUID = "Unique ID";
const char* CONFIG_FTPPRXNAME = "Name";
const char* CONFIG_FTPPRXTYPE = "Type";
const char* CONFIG_FTPPRXHOST = "Host";
const char* CONFIG_FTPPRXPORT = "Port";
const char* CONFIG_FTPPRXUSER = "User";
const char* CONFIG_FTPPRXPASSWD_OLD = "Password"; // import only, scrambled by FTP plugin
const char* CONFIG_FTPPRXPASSWD_SCRAMBLED = "PasswordS";
const char* CONFIG_FTPPRXPASSWD_ENCRYPTED = "PasswordE";
const char* CONFIG_FTPPRXSCRIPT = "Script";

const char* CONFIG_DEFAULTFTPPRXUID = "Default Proxy UID";

const char* CONFIG_ALWAYSNOTCLOSECON = "Always Detach";
const char* CONFIG_ALWAYSDISCONNECT = "Always Disconnect";
const char* CONFIG_ALWAYSRECONNECT = "Always Reconnect";
const char* CONFIG_ALWAYSOVEWRITE = "Always Overwrite";
const char* CONFIG_CONVERTHEXESCSEQ = "Convert Hex-esc-sequences";
const char* CONFIG_WARNWHENCONLOST = "Connection Lost Message";
const char* CONFIG_HINTLISTHIDDENFILES = "List Hidden Files Hint";

const char* CONFIG_ENABLELOGGING = "Enable Logging";
const char* CONFIG_LOGMAXSIZE = "Log Max. Size";
const char* CONFIG_MAXCLOSEDCONLOGS = "Max. Closed Connections Logs";
const char* CONFIG_LOGSDLGPOSITION = "Logs Position";
const char* CONFIG_ALWAYSSHOWLOGFORACTPAN = "Always Show Panel Log";
const char* CONFIG_DISABLELOGWORKERS = "Disable Log for Operations";

const char* CONFIG_COMMANDHISTORY = "Command History";
const char* CONFIG_SENDSECRETCOMMAND = "Send Secret Command";

const char* CONFIG_HOSTADDRESSHISTORY = "Host Address History";
const char* CONFIG_INITPATHHISTORY = "Init Path History";

// frequently used error message
const char* LOW_MEMORY = "Low memory";

const char* LIST_CMD_TEXT = "LIST";      // text of the FTP command "LIST"
const char* NLST_CMD_TEXT = "NLST";      // text of the FTP command "NLST"
const char* LIST_a_CMD_TEXT = "LIST -a"; // text of the FTP command "LIST -a"

int SortByExtDirsAsFiles = FALSE; // current value of the Salamander configuration variable SALCFG_SORTBYEXTDIRSASFILES
int InactiveBeepWhenDone = TRUE;  // current value of the Salamander configuration variable SALCFG_MINBEEPWHENDONE

HINSTANCE DLLInstance = NULL; // handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to SLG - language-dependent resources

// general interface of Salamander - valid from startup until the plug-in is closed
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// ZLIB compression/decompression interface;
CSalamanderZLIBAbstract* SalZLIB = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// variable definition for "spl_com.h"
int SalamanderVersion = 0;

// interface providing customized Windows controls used in Salamander
CSalamanderGUIAbstract* SalamanderGUI = NULL;

// pointers to the mapping tables for lower/upper case
unsigned char* LowerCase = NULL;
unsigned char* UpperCase = NULL;

CConfiguration Config; // global configuration of the FTP client

// array with all open FS (use only from the main thread - unsynchronized)
TIndirectArray<CPluginFSInterface> FTPConnections(5, 10, dtNoDelete);

BOOL WindowsVistaAndLater = FALSE; // Windows Vista or later from the NT family

// global variables for the FTPCMD_CHANGETGTPANELPATH command
int TargetPanelPathPanel = PANEL_LEFT;
char TargetPanelPath[MAX_PATH] = "";

char UserDefinedSuffix[100] = ""; // preloaded string for marking user-defined "server type"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;

        INITCOMMONCONTROLSEX initCtrls;
        initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
        initCtrls.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
        if (!InitCommonControlsEx(&initCtrls))
        {
            MessageBox(NULL, "InitCommonControlsEx failed!", "Error", MB_OK | MB_ICONERROR);
            return FALSE; // DLL won't start
        }

        WindowsVistaAndLater = SalIsWindowsVersionOrGreater(6, 0, 0);
    }

    return TRUE; // DLL can be loaded
}

//
// ****************************************************************************
// LoadStr
//

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plug-in is made for the current version of Salamander and higher - perform a check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(), REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "FTP Client" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // let the language module (.slg) load
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "FTP Client" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // get the general interface of Salamander
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalZLIB = SalamanderGeneral->GetSalamanderZLIB();

    // set the help file name
    SalamanderGeneral->SetHelpFileName("ftp.chm");

    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                          sizeof(SortByExtDirsAsFiles), NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_MINBEEPWHENDONE, &InactiveBeepWhenDone,
                                          sizeof(InactiveBeepWhenDone), NULL);
    SalamanderGeneral->GetLowerAndUpperCase(&LowerCase, &UpperCase);
    lstrcpyn(UserDefinedSuffix, LoadStr(IDS_SRVTYPEUSERDEF), 100);
    if (!Config.InitWithSalamanderGeneral())
        return NULL; // error

    // obtain the interface providing customized Windows controls used in Salamander
    SalamanderGUI = salamander->GetSalamanderGUI();

    if (!InitSockets(salamander->GetParentWindow()))
    {
        Config.ReleaseDataFromSalamanderGeneral();
        return NULL; // error
    }

    if (!InitFS())
    {
        ReleaseSockets();
        Config.ReleaseDataFromSalamanderGeneral();
        return NULL; // error
    }

    // set the basic information about the plug-in
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION |
                                       FUNCTION_FILESYSTEM,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGINDESCR),
                                   "FTP", NULL, "ftp");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // we want to receive messages about creation/change/removal of the master password
    SalamanderGeneral->SetPluginUsesPasswordManager();

    // obtain our FS-name (it does not have to be "FTP", Salamander may adjust it)
    SalamanderGeneral->GetPluginFSName(AssignedFSName, 0);
    AssignedFSNameLen = (int)strlen(AssignedFSName);

    // also add a name for FTPS (FTP over SSL)
    if (salamander->AddFSName("ftps", &AssignedFSNameIndexFTPS))
        SalamanderGeneral->GetPluginFSName(AssignedFSNameFTPS, AssignedFSNameIndexFTPS);
    else
        strcpy(AssignedFSNameFTPS, AssignedFSName); // probably "dead code"
    AssignedFSNameLenFTPS = (int)strlen(AssignedFSNameFTPS);

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_FTPPLUGINTITLE),
                LoadStr(IDS_PLUGINDESCR));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUTPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);

    BOOL ret = FALSE;
    if (force ||
        FTPOperationsList.IsEmpty() ||
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_CANCELEXISTINGOPER),
                                         LoadStr(IDS_FTPPLUGINTITLE),
                                         MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) == IDYES)
    { // any cancellation of all operations is performed in ReleaseFS()
        ret = TRUE;
    }

    if (ret)
    {
        ReleaseFS();

        if (InterfaceForFS.GetActiveFSCount() != 0)
        {
            TRACE_E("Some FS interfaces were not closed (count=" << InterfaceForFS.GetActiveFSCount() << ")");
        }

        // remove all copies of files from FTP in the disk cache (they exist because they survive connection close and are unnecessary)
        char uniqueFileName[MAX_PATH];
        strcpy(uniqueFileName, AssignedFSName);
        strcat(uniqueFileName, ":");
        SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
        strcpy(uniqueFileName, AssignedFSNameFTPS);
        strcat(uniqueFileName, ":");
        SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);

        ReleaseSockets();
        FreeSSL();
        Config.ReleaseDataFromSalamanderGeneral();
    }
    return ret;
}

BOOL LoadHistory(CSalamanderRegistryAbstract* registry, HKEY hKey, const char* name, char* history[], int maxCount)
{
    HKEY historyKey;
    if (registry->OpenKey(hKey, name, historyKey))
    {
        int j;
        for (j = 0; j < maxCount; j++)
            if (history[j] != NULL)
            {
                free(history[j]);
                history[j] = NULL;
            }

        char buf[10];
        int i;
        for (i = 0; i < maxCount; i++)
        {
            _itoa(i + 1, buf, 10);
            DWORD bufferSize;
            if (registry->GetSize(historyKey, buf, REG_SZ, bufferSize))
            {
                history[i] = (char*)malloc(bufferSize);
                if (history[i] == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                    break;
                }
                if (!registry->GetValue(historyKey, buf, REG_SZ, history[i], bufferSize))
                    break;
            }
        }
        registry->CloseKey(historyKey);
    }
    return TRUE;
}

BOOL SaveHistory(CSalamanderRegistryAbstract* registry, HKEY hKey, const char* name, char* history[], int maxCount)
{
    HKEY historyKey;
    if (registry->CreateKey(hKey, name, historyKey))
    {
        registry->ClearKey(historyKey);

        BOOL saveHistory;
        if (SalamanderGeneral->GetConfigParameter(SALCFG_SAVEHISTORY, &saveHistory, sizeof(saveHistory), NULL) &&
            saveHistory)
        {
            char buf[10];
            int i;
            for (i = 0; i < maxCount; i++)
            {
                if (history[i] != NULL)
                {
                    _itoa(i + 1, buf, 10);
                    registry->SetValue(historyKey, buf, REG_SZ, history[i], (DWORD)strlen(history[i]) + 1);
                }
                else
                    break;
            }
        }
        registry->CloseKey(historyKey);
    }
    return TRUE;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    if (regKey != NULL) // load from the registry
    {
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_LASTCFGPAGE, REG_DWORD, &Config.LastCfgPage, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SHOWWELCOMEMESSAGE, REG_DWORD, &Config.ShowWelcomeMessage, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_PRIORITYTOPANELCON, REG_DWORD, &Config.PriorityToPanelConnections, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_ENABLETOTALSPEEDLIM, REG_DWORD, &Config.EnableTotalSpeedLimit, sizeof(DWORD));
        char num[30];
        if (registry->GetValue(regKey, CONFIG_TOTALSPEEDLIMIT, REG_SZ, num, 30))
        {
            Config.TotalSpeedLimit = atof(num);
        }
        char anonymousPasswd[PASSWORD_MAX_SIZE];
        if (registry->GetValue(regKey, CONFIG_ANONYMOUSPASSWD, REG_SZ, anonymousPasswd, PASSWORD_MAX_SIZE))
            Config.SetAnonymousPasswd(anonymousPasswd);

        registry->GetValue(regKey, CONFIG_PASSIVEMODE, REG_DWORD, &Config.PassiveMode, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_KEEPALIVE, REG_DWORD, &Config.KeepAlive, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COMPRESSDATA, REG_DWORD, &Config.CompressData, sizeof(DWORD));

        DWORD dw;
        if (registry->GetValue(regKey, CONFIG_USEMAXCON, REG_DWORD, &dw, sizeof(DWORD)) &&
            dw != -1)
        {
            Config.MaxConcurrentConnections = dw;
            Config.UseMaxConcurrentConnections = TRUE;
        }
        else
            Config.UseMaxConcurrentConnections = FALSE;
        if (registry->GetValue(regKey, CONFIG_SPEEDLIM, REG_SZ, num, 30) &&
            atof(num) != -1)
        {
            Config.ServerSpeedLimit = atof(num);
            Config.UseServerSpeedLimit = TRUE;
        }
        else
            Config.UseServerSpeedLimit = FALSE;
        registry->GetValue(regKey, CONFIG_USELISTINGSCACHE, REG_DWORD, &Config.UseListingsCache, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_TRANSFERMODE, REG_DWORD, &Config.TransferMode, sizeof(DWORD));
        char masks[MAX_GROUPMASK];
        if (registry->GetValue(regKey, CONFIG_ASCIIMASKS, REG_SZ, masks, MAX_GROUPMASK))
            Config.ASCIIFileMasks->SetMasksString(masks, FALSE);

        if (registry->GetValue(regKey, CONFIG_SRVREPTIMEOUT, REG_DWORD, &dw, sizeof(DWORD)))
            Config.SetServerRepliesTimeout(dw);
        if (registry->GetValue(regKey, CONFIG_NODATATRTIMEOUT, REG_DWORD, &dw, sizeof(DWORD)))
            Config.SetNoDataTransferTimeout(dw);
        if (registry->GetValue(regKey, CONFIG_DELBETWCONRETR, REG_DWORD, &dw, sizeof(DWORD)))
            Config.SetDelayBetweenConRetries(dw);
        if (registry->GetValue(regKey, CONFIG_CONATTEMPTS, REG_DWORD, &dw, sizeof(DWORD)))
            Config.SetConnectRetries(dw);
        if (registry->GetValue(regKey, CONFIG_RESUMEOVERLAP, REG_DWORD, &dw, sizeof(DWORD)))
        {
            if (dw > 1024 * 1024 * 1024)
                dw = 1024 * 1024 * 1024;
            Config.SetResumeOverlap(dw);
        }
        if (registry->GetValue(regKey, CONFIG_RESUMEMINFILESIZE, REG_DWORD, &dw, sizeof(DWORD)))
            Config.SetResumeMinFileSize(dw);
        registry->GetValue(regKey, CONFIG_KASENDEVERY, REG_DWORD, &Config.KeepAliveSendEvery, sizeof(DWORD));
        if (Config.KeepAliveSendEvery < 0 || Config.KeepAliveSendEvery > 10000)
            Config.KeepAliveSendEvery = 60;
        registry->GetValue(regKey, CONFIG_KASTOPAFTER, REG_DWORD, &Config.KeepAliveStopAfter, sizeof(DWORD));
        if (Config.KeepAliveStopAfter < 0 || Config.KeepAliveStopAfter > 10000)
            Config.KeepAliveStopAfter = 30;
        registry->GetValue(regKey, CONFIG_KACOMMAND, REG_DWORD, &Config.KeepAliveCommand, sizeof(DWORD));
        DWORD cacheMaxSize;
        if (registry->GetValue(regKey, CONFIG_CACHEMAXSIZE, REG_DWORD, &cacheMaxSize, sizeof(DWORD)))
        {
            Config.CacheMaxSize = CQuadWord(cacheMaxSize, 0); // storing it as a DWORD is sufficient for now
            if (Config.CacheMaxSize < CQuadWord(100 * 1024, 0))
                Config.CacheMaxSize = CQuadWord(100 * 1024, 0);
        }
        registry->GetValue(regKey, CONFIG_DOWNLOADADDTOQUEUE, REG_DWORD, &Config.DownloadAddToQueue, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_DELETEADDTOQUEUE, REG_DWORD, &Config.DeleteAddToQueue, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CHATTRADDTOQUEUE, REG_DWORD, &Config.ChAttrAddToQueue, sizeof(DWORD));

        if (ConfigVersion > 7) // older versions use default values
        {
            registry->GetValue(regKey, CONFIG_OPERCANNOTCREATEFILE, REG_DWORD, &Config.OperationsCannotCreateFile, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERCANNOTCREATEDIR, REG_DWORD, &Config.OperationsCannotCreateDir, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERFILEALREADYEXISTS, REG_DWORD, &Config.OperationsFileAlreadyExists, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERDIRALREADYEXISTS, REG_DWORD, &Config.OperationsDirAlreadyExists, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERRETRYONCREATFILE, REG_DWORD, &Config.OperationsRetryOnCreatedFile, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERRETRYONRESUMFILE, REG_DWORD, &Config.OperationsRetryOnResumedFile, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERASCIITRMODEFORBIN, REG_DWORD, &Config.OperationsAsciiTrModeButBinFile, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERUNKNOWNATTRS, REG_DWORD, &Config.OperationsUnknownAttrs, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERNONEMPTYDIRDEL, REG_DWORD, &Config.OperationsNonemptyDirDel, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERHIDDENFILEDEL, REG_DWORD, &Config.OperationsHiddenFileDel, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_OPERHIDDENDIRDEL, REG_DWORD, &Config.OperationsHiddenDirDel, sizeof(DWORD));
        }

        if (ConfigVersion >= 22) // older versions use default values
        {
            registry->GetValue(regKey, CONFIG_UPLOADCANNOTCREATEFILE, REG_DWORD, &Config.UploadCannotCreateFile, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_UPLOADCANNOTCREATEDIR, REG_DWORD, &Config.UploadCannotCreateDir, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_UPLOADFILEALREADYEXISTS, REG_DWORD, &Config.UploadFileAlreadyExists, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_UPLOADDIRALREADYEXISTS, REG_DWORD, &Config.UploadDirAlreadyExists, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_UPLOADRETRYONCREATFILE, REG_DWORD, &Config.UploadRetryOnCreatedFile, sizeof(DWORD));

            // adjustment of a poorly chosen default - based on user feedback I changed "Overwrite" to "Resume or Overwrite"
            // (they were annoyed that after an hour of upload and a broken connection the file was overwritten instead of resumed, the risk of
            // Resume is hopefully small enough, so I changed it to "Resume or Overwrite")
            if (ConfigVersion < 29 && Config.UploadRetryOnCreatedFile == RETRYONCREATFILE_OVERWRITE)
                Config.UploadRetryOnCreatedFile = RETRYONCREATFILE_RES_OVRWR;

            registry->GetValue(regKey, CONFIG_UPLOADRETRYONRESUMFILE, REG_DWORD, &Config.UploadRetryOnResumedFile, sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_UPLOADASCIITRMODEFORBIN, REG_DWORD, &Config.UploadAsciiTrModeButBinFile, sizeof(DWORD));
        }

        registry->GetValue(regKey, CONFIG_LASTBOOKMARK, REG_DWORD, &Config.LastBookmark, sizeof(DWORD));

        if (ConfigVersion != 1) // when moving from version 1 to 2 the server-types and ftp-servers lists in the registry are ignored
        {
            if (ConfigVersion < 2 || ConfigVersion >= RELOAD_PARSERS_BEFORE_CONFIG_VERSION)
            { // when upgrading to the latest version the server-types list is adjusted (ignore the old one)
                Config.LockServerTypeList()->Load(parent, regKey, registry);
                Config.UnlockServerTypeList();
            }
            Config.FTPProxyServerList.Load(parent, regKey, registry);
            Config.FTPServerList.Load(parent, regKey, registry);
        }

        registry->GetValue(regKey, CONFIG_DEFAULTFTPPRXUID, REG_DWORD, &Config.DefaultProxySrvUID, sizeof(DWORD));
        if (Config.DefaultProxySrvUID != -1 &&
            !Config.FTPProxyServerList.IsValidUID(Config.DefaultProxySrvUID))
        {
            Config.DefaultProxySrvUID = -1; // "not used"
        }

        registry->GetValue(regKey, CONFIG_ALWAYSNOTCLOSECON, REG_DWORD, &Config.AlwaysNotCloseCon, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_ALWAYSDISCONNECT, REG_DWORD, &Config.AlwaysDisconnect, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_ENABLELOGGING, REG_DWORD, &Config.EnableLogging, sizeof(DWORD));
        if (registry->GetValue(regKey, CONFIG_LOGMAXSIZE, REG_DWORD, &dw, sizeof(DWORD)))
        {
            if (dw != -1)
            {
                Config.LogMaxSize = dw;
                Config.UseLogMaxSize = TRUE;
            }
            else
                Config.UseLogMaxSize = FALSE;
        }
        if (registry->GetValue(regKey, CONFIG_MAXCLOSEDCONLOGS, REG_DWORD, &dw, sizeof(DWORD)))
        {
            if (dw != -1)
            {
                Config.MaxClosedConLogs = dw;
                Config.UseMaxClosedConLogs = TRUE;
            }
            else
                Config.UseMaxClosedConLogs = FALSE;
        }
        registry->GetValue(regKey, CONFIG_ALWAYSSHOWLOGFORACTPAN, REG_DWORD, &Config.AlwaysShowLogForActPan, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_DISABLELOGWORKERS, REG_DWORD, &Config.DisableLoggingOfWorkers, sizeof(DWORD));

        // adjustment of a poorly chosen default - logging will be enabled in workers by default (I am tired of constantly pointing people to how to enable logging + the logging overhead is minimal)
        if (ConfigVersion < 29 && Config.DisableLoggingOfWorkers)
            Config.DisableLoggingOfWorkers = FALSE;

        char buf[100];
        if (registry->GetValue(regKey, CONFIG_LOGSDLGPOSITION, REG_SZ, buf, 100) &&
            buf[0] != 0)
        {
            Config.LogsDlgPlacement.length = sizeof(WINDOWPLACEMENT);
            RECT rect;
            sscanf(buf, "%d, %d, %d, %d, %u", &rect.left, &rect.top, &rect.right, &rect.bottom, &Config.LogsDlgPlacement.showCmd);

            // ensure the window does not exceed the working area of the monitor where its major part lies
            RECT clipRect;
            SalamanderGeneral->MultiMonGetClipRectByRect(&rect, &clipRect, NULL);
            IntersectRect(&Config.LogsDlgPlacement.rcNormalPosition, &rect, &clipRect);
        }

        if (registry->GetValue(regKey, CONFIG_OPERDLGPOSITION, REG_SZ, buf, 100) &&
            buf[0] != 0)
        {
            Config.OperDlgPlacement.length = sizeof(WINDOWPLACEMENT);
            RECT rect;
            sscanf(buf, "%d, %d, %u", &rect.right, &rect.bottom, &Config.OperDlgPlacement.showCmd);
            rect.left = 0;
            rect.top = 0;

            // ensure the window does not exceed the working area of the monitor where its major part lies
            RECT clipRect;
            SalamanderGeneral->MultiMonGetClipRectByRect(&rect, &clipRect, NULL);
            IntersectRect(&Config.OperDlgPlacement.rcNormalPosition, &rect, &clipRect);
        }

        if (registry->GetValue(regKey, CONFIG_OPERDLGSPLITPOS, REG_DWORD, &dw, sizeof(DWORD)))
        {
            Config.OperDlgSplitPos = dw / 100000.0;
            if (Config.OperDlgSplitPos < 0 || Config.OperDlgSplitPos > 1)
                Config.OperDlgSplitPos = 0.5;
        }

        LoadHistory(registry, regKey, CONFIG_COMMANDHISTORY, Config.CommandHistory, COMMAND_HISTORY_SIZE);
        registry->GetValue(regKey, CONFIG_SENDSECRETCOMMAND, REG_DWORD, &Config.SendSecretCommand, sizeof(DWORD));

        LoadHistory(registry, regKey, CONFIG_HOSTADDRESSHISTORY, Config.HostAddressHistory, HOSTADDRESS_HISTORY_SIZE);
        LoadHistory(registry, regKey, CONFIG_INITPATHHISTORY, Config.InitPathHistory, INITIALPATH_HISTORY_SIZE);

        registry->GetValue(regKey, CONFIG_ALWAYSRECONNECT, REG_DWORD, &Config.AlwaysReconnect, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_WARNWHENCONLOST, REG_DWORD, &Config.WarnWhenConLost, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_HINTLISTHIDDENFILES, REG_DWORD, &Config.HintListHiddenFiles, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_ALWAYSOVEWRITE, REG_DWORD, &Config.AlwaysOverwrite, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_CONVERTHEXESCSEQ, REG_DWORD, &Config.ConvertHexEscSeq, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_OPERDLGCLOSEIFSUCCESS, REG_DWORD,
                           &Config.CloseOperationDlgIfSuccessfullyFinished, sizeof(DWORD));

        // commented out because remembering this checkbox feels odd to me - I now see the meaning of the checkbox
        // in having the option to let the dialog close after the operation completes (which does not depend on the previous operation)
        //    registry->GetValue(regKey, CONFIG_OPERDLGCLOSEWHENFINISHES, REG_DWORD,
        //                       &Config.CloseOperationDlgWhenOperFinishes, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_OPENSOLVEERRIFIDLE, REG_DWORD,
                           &Config.OpenSolveErrIfIdle, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_SIMPLELSTCOLFIXEDWIDTH, REG_DWORD,
                           &CSimpleListPluginDataInterface::ListingColumnFixedWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SIMPLELSTCOLWIDTH, REG_DWORD,
                           &CSimpleListPluginDataInterface::ListingColumnWidth, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));

    char num[30];
    registry->SetValue(regKey, CONFIG_LASTCFGPAGE, REG_DWORD, &Config.LastCfgPage, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SHOWWELCOMEMESSAGE, REG_DWORD, &Config.ShowWelcomeMessage, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_PRIORITYTOPANELCON, REG_DWORD, &Config.PriorityToPanelConnections, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_ENABLETOTALSPEEDLIM, REG_DWORD, &Config.EnableTotalSpeedLimit, sizeof(DWORD));
    sprintf(num, "%g", Config.TotalSpeedLimit);
    registry->SetValue(regKey, CONFIG_TOTALSPEEDLIMIT, REG_SZ, num, -1);
    char anonymousPasswd[PASSWORD_MAX_SIZE];
    Config.GetAnonymousPasswd(anonymousPasswd, PASSWORD_MAX_SIZE);
    registry->SetValue(regKey, CONFIG_ANONYMOUSPASSWD, REG_SZ, anonymousPasswd, -1);

    registry->SetValue(regKey, CONFIG_PASSIVEMODE, REG_DWORD, &Config.PassiveMode, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_KEEPALIVE, REG_DWORD, &Config.KeepAlive, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COMPRESSDATA, REG_DWORD, &Config.CompressData, sizeof(DWORD));

    DWORD dw = Config.UseMaxConcurrentConnections ? Config.MaxConcurrentConnections : -1;
    registry->SetValue(regKey, CONFIG_USEMAXCON, REG_DWORD, &dw, sizeof(DWORD));
    sprintf(num, "%g", (Config.UseServerSpeedLimit ? Config.ServerSpeedLimit : -1.0));
    registry->SetValue(regKey, CONFIG_SPEEDLIM, REG_SZ, num, -1);
    registry->SetValue(regKey, CONFIG_USELISTINGSCACHE, REG_DWORD, &Config.UseListingsCache, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_TRANSFERMODE, REG_DWORD, &Config.TransferMode, sizeof(DWORD));
    char masks[MAX_GROUPMASK];
    Config.ASCIIFileMasks->GetMasksString(masks);
    registry->SetValue(regKey, CONFIG_ASCIIMASKS, REG_SZ, masks, -1);

    dw = Config.GetServerRepliesTimeout();
    registry->SetValue(regKey, CONFIG_SRVREPTIMEOUT, REG_DWORD, &dw, sizeof(DWORD));
    dw = Config.GetNoDataTransferTimeout();
    registry->SetValue(regKey, CONFIG_NODATATRTIMEOUT, REG_DWORD, &dw, sizeof(DWORD));
    dw = Config.GetDelayBetweenConRetries();
    registry->SetValue(regKey, CONFIG_DELBETWCONRETR, REG_DWORD, &dw, sizeof(DWORD));
    dw = Config.GetConnectRetries();
    registry->SetValue(regKey, CONFIG_CONATTEMPTS, REG_DWORD, &dw, sizeof(DWORD));
    dw = Config.GetResumeOverlap();
    registry->SetValue(regKey, CONFIG_RESUMEOVERLAP, REG_DWORD, &dw, sizeof(DWORD));
    dw = Config.GetResumeMinFileSize();
    registry->SetValue(regKey, CONFIG_RESUMEMINFILESIZE, REG_DWORD, &dw, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_KASENDEVERY, REG_DWORD, &Config.KeepAliveSendEvery, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_KASTOPAFTER, REG_DWORD, &Config.KeepAliveStopAfter, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_KACOMMAND, REG_DWORD, &Config.KeepAliveCommand, sizeof(DWORD));
    DWORD cacheMaxSize = (DWORD)Config.CacheMaxSize.Value; // storing it as a DWORD is sufficient for now
    registry->SetValue(regKey, CONFIG_CACHEMAXSIZE, REG_DWORD, &cacheMaxSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_DOWNLOADADDTOQUEUE, REG_DWORD, &Config.DownloadAddToQueue, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_DELETEADDTOQUEUE, REG_DWORD, &Config.DeleteAddToQueue, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CHATTRADDTOQUEUE, REG_DWORD, &Config.ChAttrAddToQueue, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_OPERCANNOTCREATEFILE, REG_DWORD, &Config.OperationsCannotCreateFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERCANNOTCREATEDIR, REG_DWORD, &Config.OperationsCannotCreateDir, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERFILEALREADYEXISTS, REG_DWORD, &Config.OperationsFileAlreadyExists, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERDIRALREADYEXISTS, REG_DWORD, &Config.OperationsDirAlreadyExists, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERRETRYONCREATFILE, REG_DWORD, &Config.OperationsRetryOnCreatedFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERRETRYONRESUMFILE, REG_DWORD, &Config.OperationsRetryOnResumedFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERASCIITRMODEFORBIN, REG_DWORD, &Config.OperationsAsciiTrModeButBinFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERUNKNOWNATTRS, REG_DWORD, &Config.OperationsUnknownAttrs, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERNONEMPTYDIRDEL, REG_DWORD, &Config.OperationsNonemptyDirDel, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERHIDDENFILEDEL, REG_DWORD, &Config.OperationsHiddenFileDel, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_OPERHIDDENDIRDEL, REG_DWORD, &Config.OperationsHiddenDirDel, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_UPLOADCANNOTCREATEFILE, REG_DWORD, &Config.UploadCannotCreateFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_UPLOADCANNOTCREATEDIR, REG_DWORD, &Config.UploadCannotCreateDir, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_UPLOADFILEALREADYEXISTS, REG_DWORD, &Config.UploadFileAlreadyExists, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_UPLOADDIRALREADYEXISTS, REG_DWORD, &Config.UploadDirAlreadyExists, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_UPLOADRETRYONCREATFILE, REG_DWORD, &Config.UploadRetryOnCreatedFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_UPLOADRETRYONRESUMFILE, REG_DWORD, &Config.UploadRetryOnResumedFile, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_UPLOADASCIITRMODEFORBIN, REG_DWORD, &Config.UploadAsciiTrModeButBinFile, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_LASTBOOKMARK, REG_DWORD, &Config.LastBookmark, sizeof(DWORD));

    Config.LockServerTypeList()->Save(parent, regKey, registry);
    Config.UnlockServerTypeList();

    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
    if (passwordManager->IsUsingMasterPassword())
    {
        // determine whether any of the passwords is stored in an unencrypted form
        BOOL containsUnsecurePassword = Config.FTPServerList.ContainsUnsecuredPassword() || Config.FTPProxyServerList.ContainsUnsecuredPassword();
        if (containsUnsecurePassword)
        {
            if (!passwordManager->IsMasterPasswordSet())
            {
                // we do not know the master password, ask the user
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_PASSWORD_UNSECURED), LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONEXCLAMATION);
                passwordManager->AskForMasterPassword(parent);
            }
            if (passwordManager->IsMasterPasswordSet()) // if we already know the master password, we can encrypt
            {
                Config.FTPServerList.EncryptPasswords(parent, TRUE);
                Config.FTPProxyServerList.EncryptPasswords(parent, TRUE);
            }
        }
    }
    Config.FTPProxyServerList.Save(parent, regKey, registry);
    Config.FTPServerList.Save(parent, regKey, registry);

    registry->SetValue(regKey, CONFIG_DEFAULTFTPPRXUID, REG_DWORD, &Config.DefaultProxySrvUID, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_ALWAYSNOTCLOSECON, REG_DWORD, &Config.AlwaysNotCloseCon, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_ALWAYSDISCONNECT, REG_DWORD, &Config.AlwaysDisconnect, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_ENABLELOGGING, REG_DWORD, &Config.EnableLogging, sizeof(DWORD));
    dw = Config.UseLogMaxSize ? Config.LogMaxSize : -1;
    registry->SetValue(regKey, CONFIG_LOGMAXSIZE, REG_DWORD, &dw, sizeof(DWORD));
    dw = Config.UseMaxClosedConLogs ? Config.MaxClosedConLogs : -1;
    registry->SetValue(regKey, CONFIG_MAXCLOSEDCONLOGS, REG_DWORD, &dw, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_ALWAYSSHOWLOGFORACTPAN, REG_DWORD, &Config.AlwaysShowLogForActPan, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_DISABLELOGWORKERS, REG_DWORD, &Config.DisableLoggingOfWorkers, sizeof(DWORD));

    char buf[100];
    Logs.SaveLogsDlgPos(); // also store the position of the currently opened window
    if (Config.LogsDlgPlacement.length != 0)
    {
        sprintf(buf, "%d, %d, %d, %d, %u",
                Config.LogsDlgPlacement.rcNormalPosition.left,
                Config.LogsDlgPlacement.rcNormalPosition.top,
                Config.LogsDlgPlacement.rcNormalPosition.right,
                Config.LogsDlgPlacement.rcNormalPosition.bottom,
                Config.LogsDlgPlacement.showCmd);
    }
    else
        buf[0] = 0;
    registry->SetValue(regKey, CONFIG_LOGSDLGPOSITION, REG_SZ, buf, -1);

    if (Config.OperDlgPlacement.length != 0)
    {
        sprintf(buf, "%d, %d, %u",
                Config.OperDlgPlacement.rcNormalPosition.right -
                    Config.OperDlgPlacement.rcNormalPosition.left,
                Config.OperDlgPlacement.rcNormalPosition.bottom -
                    Config.OperDlgPlacement.rcNormalPosition.top,
                Config.OperDlgPlacement.showCmd);
    }
    else
        buf[0] = 0;
    registry->SetValue(regKey, CONFIG_OPERDLGPOSITION, REG_SZ, buf, -1);

    dw = (DWORD)(Config.OperDlgSplitPos * 100000);
    registry->SetValue(regKey, CONFIG_OPERDLGSPLITPOS, REG_DWORD, &dw, sizeof(DWORD));

    SaveHistory(registry, regKey, CONFIG_COMMANDHISTORY, Config.CommandHistory, COMMAND_HISTORY_SIZE);
    registry->SetValue(regKey, CONFIG_SENDSECRETCOMMAND, REG_DWORD, &Config.SendSecretCommand, sizeof(DWORD));

    SaveHistory(registry, regKey, CONFIG_HOSTADDRESSHISTORY, Config.HostAddressHistory, HOSTADDRESS_HISTORY_SIZE);
    SaveHistory(registry, regKey, CONFIG_INITPATHHISTORY, Config.InitPathHistory, INITIALPATH_HISTORY_SIZE);

    registry->SetValue(regKey, CONFIG_ALWAYSRECONNECT, REG_DWORD, &Config.AlwaysReconnect, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WARNWHENCONLOST, REG_DWORD, &Config.WarnWhenConLost, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_HINTLISTHIDDENFILES, REG_DWORD, &Config.HintListHiddenFiles, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_ALWAYSOVEWRITE, REG_DWORD, &Config.AlwaysOverwrite, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_CONVERTHEXESCSEQ, REG_DWORD, &Config.ConvertHexEscSeq, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_OPERDLGCLOSEIFSUCCESS, REG_DWORD,
                       &Config.CloseOperationDlgIfSuccessfullyFinished, sizeof(DWORD));

    // commented out because remembering this checkbox feels odd to me - I now see the meaning of the checkbox
    // in having the option to let the dialog close after the operation completes (which does not depend on the previous operation)
    //  registry->SetValue(regKey, CONFIG_OPERDLGCLOSEWHENFINISHES, REG_DWORD,
    //                     &Config.CloseOperationDlgWhenOperFinishes, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_OPENSOLVEERRIFIDLE, REG_DWORD,
                       &Config.OpenSolveErrIfIdle, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SIMPLELSTCOLFIXEDWIDTH, REG_DWORD,
                       &CSimpleListPluginDataInterface::ListingColumnFixedWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SIMPLELSTCOLWIDTH, REG_DWORD,
                       &CSimpleListPluginDataInterface::ListingColumnWidth, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    CConfigDlg(parent).Execute();
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    HBITMAP hBmp = (HBITMAP)HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDC_FTPICONBMP),
                                              IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR));
    salamander->SetBitmapWithIcons(hBmp);
    HANDLES(DeleteObject(hBmp));
    salamander->SetChangeDriveMenuItem(LoadStr(IDS_FTPCHNGDRVITEM), 0);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    /* used by the export_mnu.py script that generates salmenu.mnu for Translator
   keep synchronized with the salamander->AddMenuItem() calls below...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_CONNECTFTPSERVER
	{MNTT_IT, IDS_ORGBOOKMARKSCMD
	{MNTT_IT, IDS_SHOWLOGS
	{MNTT_IT, IDS_DISCONNECT
	{MNTT_IT, IDS_SHOWCERT
	{MNTT_PB, IDS_MENUTRANSFERMODE
	{MNTT_IT, IDS_MENUTRMODEAUTO
	{MNTT_IT, IDS_MENUTRMODEASCII
	{MNTT_IT, IDS_MENUTRMODEBINARY
	{MNTT_PE, 0
	{MNTT_IT, IDS_REFRESHPATH
	{MNTT_IT, IDS_ADDBOOKMARK
	{MNTT_IT, IDS_SENDFTPCOMMAND
	{MNTT_IT, IDS_SHOWRAWLISTING
	{MNTT_IT, IDS_LISTHIDDENFILES
	{MNTT_PE, 0
};
*/

    salamander->AddMenuItem(-1, LoadStr(IDS_CONNECTFTPSERVER), SALHOTKEY('F', HOTKEYF_CONTROL | HOTKEYF_SHIFT),
                            FTPCMD_CONNECTFTPSERVER, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_ORGBOOKMARKSCMD), 0, FTPCMD_ORGANIZEBOOKMARKS, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_SHOWLOGS), 0, FTPCMD_SHOWLOGS, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                            MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
    salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
    salamander->AddMenuItem(-1, LoadStr(IDS_DISCONNECT), SALHOTKEY_HINT,
                            FTPCMD_DISCONNECT_F12, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_SHOWCERT), 0, FTPCMD_SHOWCERT, TRUE,
                            0, 0, MENU_SKILLLEVEL_ALL);
    // start of the Transfer Mode submenu
    salamander->AddSubmenuStart(-1, LoadStr(IDS_MENUTRANSFERMODE), FTPCMD_TRMODESUBMENU, FALSE,
                                MENU_EVENT_THIS_PLUGIN_FS, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUTRMODEAUTO), 0, FTPCMD_TRMODEAUTO, TRUE,
                            0, 0, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUTRMODEASCII), 0, FTPCMD_TRMODEASCII, TRUE,
                            0, 0, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUTRMODEBINARY), 0, FTPCMD_TRMODEBINARY, TRUE,
                            0, 0, MENU_SKILLLEVEL_ALL);
    salamander->AddSubmenuEnd();
    // end of the Transfer Mode submenu
    salamander->AddMenuItem(-1, LoadStr(IDS_REFRESHPATH), 0, FTPCMD_REFRESHPATH, FALSE,
                            MENU_EVENT_THIS_PLUGIN_FS, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
    salamander->AddMenuItem(-1, LoadStr(IDS_ADDBOOKMARK), 0, FTPCMD_ADDBOOKMARK, FALSE,
                            MENU_EVENT_THIS_PLUGIN_FS, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_SENDFTPCOMMAND), 0, FTPCMD_SENDFTPCOMMAND, FALSE,
                            MENU_EVENT_THIS_PLUGIN_FS, MENU_EVENT_TRUE,
                            MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
    salamander->AddMenuItem(-1, LoadStr(IDS_SHOWRAWLISTING), 0, FTPCMD_SHOWRAWLISTING, FALSE,
                            MENU_EVENT_THIS_PLUGIN_FS, MENU_EVENT_TRUE,
                            MENU_SKILLLEVEL_ADVANCED);
    salamander->AddMenuItem(-1, LoadStr(IDS_LISTHIDDENFILES), 0, FTPCMD_LISTHIDDENFILES, TRUE,
                            0, 0, MENU_SKILLLEVEL_ALL);
}

CPluginInterfaceForFSAbstract*
CPluginInterface::GetInterfaceForFS()
{
    return &InterfaceForFS;
}

void RefreshValuesOfPanelCtrlCon()
{
    HANDLES(EnterCriticalSection(&PanelCtrlConSect));
    CPluginFSInterface* leftFS = (CPluginFSInterface*)(SalamanderGeneral->GetPanelPluginFS(PANEL_LEFT));
    if (leftFS != NULL)
        LeftPanelCtrlCon = leftFS->GetControlConnection();
    else
        LeftPanelCtrlCon = NULL;
    CPluginFSInterface* rightFS = (CPluginFSInterface*)(SalamanderGeneral->GetPanelPluginFS(PANEL_RIGHT));
    if (rightFS != NULL)
        RightPanelCtrlCon = rightFS->GetControlConnection();
    else
        RightPanelCtrlCon = NULL;
    HANDLES(LeaveCriticalSection(&PanelCtrlConSect));
}

void CPluginInterface::Event(int event, DWORD param)
{
    if (event == PLUGINEVENT_CONFIGURATIONCHANGED)
    {
        SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                              sizeof(SortByExtDirsAsFiles), NULL);
        SalamanderGeneral->GetConfigParameter(SALCFG_MINBEEPWHENDONE, &InactiveBeepWhenDone,
                                              sizeof(InactiveBeepWhenDone), NULL);
    }
    if (event == PLUGINEVENT_PANELSSWAPPED)
    {
        RefreshValuesOfPanelCtrlCon();
        Logs.RefreshListOfLogsInLogsDlg();
    }
    if (event == PLUGINEVENT_PANELACTIVATED)
    {
        CPluginFSInterface* fs = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(PANEL_SOURCE);
        if (fs != NULL)
            fs->ActivateWelcomeMsg(); // activate the welcome message window (cannot be done from the keyboard so the user can close it at all)

        if (Config.AlwaysShowLogForActPan)
        {
            CPluginFSInterface* fs2 = (CPluginFSInterface*)(SalamanderGeneral->GetPanelPluginFS(param));
            if (fs2 != NULL)
                Logs.ActivateLog(fs2->GetLogUID());
        }
    }
}

void CPluginInterface::ClearHistory(HWND parent)
{
    int i;
    for (i = 0; i < COMMAND_HISTORY_SIZE; i++)
        if (Config.CommandHistory[i] != NULL)
        {
            free(Config.CommandHistory[i]);
            Config.CommandHistory[i] = NULL;
        }
    for (i = 0; i < HOSTADDRESS_HISTORY_SIZE; i++)
        if (Config.HostAddressHistory[i] != NULL)
        {
            free(Config.HostAddressHistory[i]);
            Config.HostAddressHistory[i] = NULL;
        }
    for (i = 0; i < INITIALPATH_HISTORY_SIZE; i++)
        if (Config.InitPathHistory[i] != NULL)
        {
            free(Config.InitPathHistory[i]);
            Config.InitPathHistory[i] = NULL;
        }
}

void CPluginInterface::AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    // WARNING: in 'includingSubdirs' for FTP paths the value is ORed with 0x02 if it is a "soft refresh"

    BOOL isFTP = SalamanderGeneral->StrNICmp(path, AssignedFSName, AssignedFSNameLen) == 0 &&          // this is our FS-name for FTP
                 path[AssignedFSNameLen] == ':';                                                       // our FS-name is not just a prefix
    BOOL isFTPS = SalamanderGeneral->StrNICmp(path, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 && // this is our FS-name for FTPS
                  path[AssignedFSNameLenFTPS] == ':';                                                  // our FS-name is not just a prefix

    if (isFTP || isFTPS)
    {
        ListingCache.AcceptChangeOnPathNotification(path + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS) + 1,
                                                    (includingSubdirs & 0x01)); // drop listings for both FTP and FTPS paths

        SalamanderGeneral->RemoveFilesFromCache(path);

        char path2[2 * MAX_PATH]; // build the name for the second FS-name (remove the same name from cache on the other FS-name as a precaution)
        strcpy(path2, isFTPS ? AssignedFSName : AssignedFSNameFTPS);
        lstrcpyn(path2 + (isFTPS ? AssignedFSNameLen : AssignedFSNameLenFTPS),
                 path + (isFTP ? AssignedFSNameLen : AssignedFSNameLenFTPS),
                 2 * MAX_PATH - (isFTPS ? AssignedFSNameLen : AssignedFSNameLenFTPS));
        SalamanderGeneral->RemoveFilesFromCache(path2);
    }
}

void CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    if (pluginData != &SimpleListPluginDataInterface) // it is global, no need to release it
    {
        delete ((CFTPListingPluginDataInterface*)pluginData);
    }
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

void CPluginInterface::PasswordManagerEvent(HWND parent, int event)
{
    BOOL allPasswordsDecrypted = TRUE; // whether all passwords were successfully decrypted

    if (event == PME_MASTERPASSWORDCREATED || event == PME_MASTERPASSWORDCHANGED || event == PME_MASTERPASSWORDREMOVED)
    {
        // if the master password is being created, changed, or removed, try to convert encrypted passwords to scrambled form
        allPasswordsDecrypted &= Config.FTPServerList.EncryptPasswords(parent, FALSE);
        allPasswordsDecrypted &= Config.FTPProxyServerList.EncryptPasswords(parent, FALSE);
    }

    if (event == PME_MASTERPASSWORDCREATED || event == PME_MASTERPASSWORDCHANGED)
    {
        // if the master password is being created or changed, we must use it to encrypt the passwords
        Config.FTPServerList.EncryptPasswords(parent, TRUE);
        Config.FTPProxyServerList.EncryptPasswords(parent, TRUE);
    }

    if (!allPasswordsDecrypted)
    {
        // if at least one password could not be decrypted, inform the user about it
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_CANNOT_DECRYPT_SOMEPASSWORDS),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
}

//
// ****************************************************************************
// CFTPServer
//

void CFTPServer::Init()
{
    ItemName = NULL;
    Address = NULL;
    InitialPath = NULL;
    AnonymousConnection = TRUE;
    UserName = NULL;
    EncryptedPassword = NULL;
    EncryptedPasswordSize = 0;
    SavePassword = FALSE;
    ProxyServerUID = -2;
    TargetPanelPath = NULL;
    ServerType = NULL;
    TransferMode = 0;
    Port = IPPORT_FTP;
    UsePassiveMode = 2;
    KeepConnectionAlive = 2;
    KeepAliveSendEvery = Config.KeepAliveSendEvery;
    KeepAliveStopAfter = Config.KeepAliveStopAfter;
    KeepAliveCommand = Config.KeepAliveCommand;
    UseMaxConcurrentConnections = 2;
    MaxConcurrentConnections = 1;
    UseServerSpeedLimit = 2;
    ServerSpeedLimit = 2;
    UseListingsCache = 2;
    InitFTPCommands = NULL;
    ListCommand = NULL;
    EncryptControlConnection = EncryptDataConnection = 0;
    CompressData = -1;
    // WARNING: default values here must match the default values used in the Save() method
}

void CFTPServer::Release()
{
    if (ItemName != NULL)
        SalamanderGeneral->Free(ItemName);
    if (Address != NULL)
        SalamanderGeneral->Free(Address);
    if (InitialPath != NULL)
        SalamanderGeneral->Free(InitialPath);
    if (UserName != NULL)
        SalamanderGeneral->Free(UserName);
    if (EncryptedPassword != NULL)
    {
        memset(EncryptedPassword, 0, EncryptedPasswordSize); // clean memory containing the password
        SalamanderGeneral->Free(EncryptedPassword);
    }
    if (TargetPanelPath != NULL)
        SalamanderGeneral->Free(TargetPanelPath);
    if (InitFTPCommands != NULL)
        SalamanderGeneral->Free(InitFTPCommands);
    if (ServerType != NULL)
        SalamanderGeneral->Free(ServerType);
    if (ListCommand != NULL)
        SalamanderGeneral->Free(ListCommand);
    Init();
}

CFTPServer*
CFTPServer::MakeCopy()
{
    CFTPServer* n = new CFTPServer;
    if (n != NULL)
    {
        n->ItemName = SalamanderGeneral->DupStr(ItemName);
        n->Address = SalamanderGeneral->DupStr(Address);
        n->InitialPath = SalamanderGeneral->DupStr(InitialPath);
        n->AnonymousConnection = AnonymousConnection;
        if (!AnonymousConnection)
        {
            n->UserName = SalamanderGeneral->DupStr(UserName);
            n->EncryptedPassword = DupEncryptedPassword(EncryptedPassword, EncryptedPasswordSize);
            n->EncryptedPasswordSize = EncryptedPasswordSize;
            n->SavePassword = SavePassword;
        }
        n->ProxyServerUID = ProxyServerUID;
        n->TargetPanelPath = SalamanderGeneral->DupStr(TargetPanelPath);
        n->ServerType = SalamanderGeneral->DupStr(ServerType);
        n->TransferMode = TransferMode;
        n->Port = Port;
        n->UsePassiveMode = UsePassiveMode;
        n->KeepConnectionAlive = KeepConnectionAlive;
        n->KeepAliveSendEvery = KeepAliveSendEvery;
        n->KeepAliveStopAfter = KeepAliveStopAfter;
        n->KeepAliveCommand = KeepAliveCommand;
        n->UseMaxConcurrentConnections = UseMaxConcurrentConnections;
        n->MaxConcurrentConnections = MaxConcurrentConnections;
        n->UseServerSpeedLimit = UseServerSpeedLimit;
        n->ServerSpeedLimit = ServerSpeedLimit;
        n->InitFTPCommands = SalamanderGeneral->DupStr(InitFTPCommands);
        n->UseListingsCache = UseListingsCache;
        n->ListCommand = SalamanderGeneral->DupStr(ListCommand);
        n->EncryptControlConnection = EncryptControlConnection;
        n->EncryptDataConnection = EncryptDataConnection;
        n->CompressData = CompressData;
    }
    else
        TRACE_E(LOW_MEMORY);
    return n;
}

void UpdateStr(char*& str, const char* newStr, BOOL* err, BOOL clearMem)
{
    if (str == NULL) // there is no previous version of the string
    {
        str = SalamanderGeneral->DupStr(newStr);
    }
    else // an older version of the string exists
    {
        if (newStr != NULL) // the new string is not NULL
        {
            if (strcmp(str, newStr) != 0) // the strings differ
            {
                if (strlen(str) < strlen(newStr)) // the new string is longer (not enough space, reallocation needed)
                {
                    char* old = str;
                    str = SalamanderGeneral->DupStr(newStr);
                    if (str != NULL)
                    {
                        if (clearMem)
                            memset(old, 0, strlen(old)); // clean memory before deallocation because of passwords
                        SalamanderGeneral->Free(old);
                    }
                    else
                        str = old; // allocation failed, keep the older version of the string
                }
                else
                {
                    if (clearMem)
                        memset(str, 0, strlen(str)); // clean memory before shortening because of passwords
                    strcpy(str, newStr);
                }
            }
        }
        else
        {
            if (clearMem)
                memset(str, 0, strlen(str)); // clean memory before deallocation because of passwords
            free(str);
            str = NULL;
        }
    }
}

BOOL CFTPServer::Set(const char* itemName,
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
    BOOL err = FALSE;
    UpdateStr(ItemName, itemName, &err);
    UpdateStr(Address, address, &err);
    UpdateStr(InitialPath, initialPath, &err);
    AnonymousConnection = anonymousConnection;
    UpdateStr(UserName, userName, &err);
    UpdateEncryptedPassword(&EncryptedPassword, &EncryptedPasswordSize, encryptedPassword, encryptedPasswordSize);
    SavePassword = savePassword;
    ProxyServerUID = proxyServerUID;
    UpdateStr(TargetPanelPath, targetPanelPath, &err);
    UpdateStr(ServerType, serverType, &err);
    TransferMode = transferMode;
    Port = port;
    UsePassiveMode = usePassiveMode;
    KeepConnectionAlive = keepConnectionAlive;
    KeepAliveSendEvery = keepAliveSendEvery;
    KeepAliveStopAfter = keepAliveStopAfter;
    KeepAliveCommand = keepAliveCommand;
    UseMaxConcurrentConnections = useMaxConcurrentConnections;
    MaxConcurrentConnections = maxConcurrentConnections;
    UseServerSpeedLimit = useServerSpeedLimit;
    ServerSpeedLimit = serverSpeedLimit;
    UpdateStr(InitFTPCommands, initFTPCommands, &err);
    UseListingsCache = useListingsCache;
    UpdateStr(ListCommand, listCommand, &err);
    EncryptControlConnection = encryptControlConnection;
    EncryptDataConnection = encryptDataConnection;
    CompressData = compressData;
    return !err;
}

const char* GetStrOrNULL(const char* s)
{
    return (s != NULL && s[0] != 0) ? s : NULL;
}

unsigned char ScrambleTable[256] =
    {
        0, 223, 235, 233, 240, 185, 88, 102, 22, 130, 27, 53, 79, 125, 66, 201,
        90, 71, 51, 60, 134, 104, 172, 244, 139, 84, 91, 12, 123, 155, 237, 151,
        192, 6, 87, 32, 211, 38, 149, 75, 164, 145, 52, 200, 224, 226, 156, 50,
        136, 190, 232, 63, 129, 209, 181, 120, 28, 99, 168, 94, 198, 40, 238, 112,
        55, 217, 124, 62, 227, 30, 36, 242, 208, 138, 174, 231, 26, 54, 214, 148,
        37, 157, 19, 137, 187, 111, 228, 39, 110, 17, 197, 229, 118, 246, 153, 80,
        21, 128, 69, 117, 234, 35, 58, 67, 92, 7, 132, 189, 5, 103, 10, 15,
        252, 195, 70, 147, 241, 202, 107, 49, 20, 251, 133, 76, 204, 73, 203, 135,
        184, 78, 194, 183, 1, 121, 109, 11, 143, 144, 171, 161, 48, 205, 245, 46,
        31, 72, 169, 131, 239, 160, 25, 207, 218, 146, 43, 140, 127, 255, 81, 98,
        42, 115, 173, 142, 114, 13, 2, 219, 57, 56, 24, 126, 3, 230, 47, 215,
        9, 44, 159, 33, 249, 18, 93, 95, 29, 113, 220, 89, 97, 182, 248, 64,
        68, 34, 4, 82, 74, 196, 213, 165, 179, 250, 108, 254, 59, 14, 236, 175,
        85, 199, 83, 106, 77, 178, 167, 225, 45, 247, 163, 158, 8, 221, 61, 191,
        119, 16, 253, 105, 186, 23, 170, 100, 216, 65, 162, 122, 150, 176, 154, 193,
        206, 222, 188, 152, 210, 243, 96, 41, 86, 180, 101, 177, 166, 141, 212, 116};

BOOL InitUnscrambleTable = TRUE;
BOOL InitSRand = TRUE;
unsigned char UnscrambleTable[256];

void ScramblePassword(char* password)
{
    // padding + units of length + tens of length + hundreds of length + password
    char buf[PASSWORD_MAX_SIZE + 50];
    int len = (int)strlen(password);
    if (InitSRand)
    {
        srand((unsigned)time(NULL));
        InitSRand = FALSE;
    }
    int padding = (((len + 3) / 17) * 17 + 17) - 3 - len;
    int i;
    for (i = 0; i < padding; i++)
    {
        int p = 0;
        while (p <= 0 || p > 255 || p >= '0' && p <= '9')
            p = (int)((double)rand() / ((double)RAND_MAX / 256.0));
        buf[i] = (unsigned char)p;
    }
    buf[padding] = '0' + (len % 10);
    buf[padding + 1] = '0' + ((len / 10) % 10);
    buf[padding + 2] = '0' + ((len / 100) % 10);
    strcpy(buf + padding + 3, password);
    char* s = buf;
    int last = 31;
    while (*s != 0)
    {
        last = (last + (unsigned char)*s) % 255 + 1;
        *s = ScrambleTable[last];
        s++;
    }
    strcpy(password, buf);
    memset(buf, 0, PASSWORD_MAX_SIZE + 50); // clean memory containing the password
}

void UnscramblePassword(char* password)
{
    if (InitUnscrambleTable)
    {
        int i;
        for (i = 0; i < 256; i++)
        {
            UnscrambleTable[ScrambleTable[i]] = i;
        }
        InitUnscrambleTable = FALSE;
    }

    char backup[PASSWORD_MAX_SIZE + 50]; // backup for TRACE_E
    lstrcpyn(backup, password, PASSWORD_MAX_SIZE + 50);

    char* s = password;
    int last = 31;
    while (*s != 0)
    {
        int x = (int)UnscrambleTable[(unsigned char)*s] - 1 - (last % 255);
        if (x <= 0)
            x += 255;
        *s = (char)x;
        last = (last + x) % 255 + 1;
        s++;
    }

    s = password;
    while (*s != 0 && (*s < '0' || *s > '9'))
        s++; // find the length of the password
    BOOL ok = FALSE;
    if (strlen(s) >= 3)
    {
        int len = (s[0] - '0') + 10 * (s[1] - '0') + 100 * (s[2] - '0');
        int total = (((len + 3) / 17) * 17 + 17);
        int passwordLen = (int)strlen(password);
        if (len >= 0 && total == passwordLen && total - (s - password) - 3 == len)
        {
            memmove(password, password + passwordLen - len, len + 1);
            ok = TRUE;
        }
    }
    if (!ok)
    {
        password[0] = 0; // some error occurred, discard the password
        TRACE_E("Unable to unscramble password! scrambled=" << backup);
    }
    memset(backup, 0, PASSWORD_MAX_SIZE + 50); // clean memory containing the password
}

void LoadPassword(HKEY regKey, CSalamanderRegistryAbstract* registry, const char* oldPwdName, const char* scrambledPwdName, const char* encryptedPwdName, BYTE** encryptedPassword, int* encryptedPasswordSize)
{
    *encryptedPassword = NULL;
    *encryptedPasswordSize = 0;
    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
    BOOL passwordFound = FALSE; // do we have a password?
    // in the first step try to fetch the AES-encrypted or scrambled version of the password
    DWORD gotType;
    DWORD bufferSize;
    const char* keyName = encryptedPwdName;
    LONG res = SalamanderGeneral->SalRegQueryValueEx(regKey, keyName, 0, &gotType, NULL, &bufferSize);
    if (res != ERROR_SUCCESS || gotType != REG_BINARY || bufferSize == 0)
    {
        keyName = scrambledPwdName;
        res = SalamanderGeneral->SalRegQueryValueEx(regKey, keyName, 0, &gotType, NULL, &bufferSize);
    }
    if (res == ERROR_SUCCESS && gotType == REG_BINARY && bufferSize != 0)
    {
        BYTE* passwordReg = (BYTE*)SalamanderGeneral->Alloc(bufferSize);
        if (registry->GetValue(regKey, keyName, REG_BINARY, passwordReg, bufferSize))
        {
            *encryptedPassword = passwordReg;
            *encryptedPasswordSize = bufferSize;
            passwordFound = TRUE;
        }
        else
            SalamanderGeneral->Free(passwordReg);
    }

    // this may be the original FTP-scrambled version of the password
    if (!passwordFound)
    {
        char passwordReg[PASSWORD_MAX_SIZE + 50];
        if (registry->GetValue(regKey, oldPwdName, REG_SZ, passwordReg, PASSWORD_MAX_SIZE + 50))
        {
            char password[PASSWORD_MAX_SIZE + 50]; // 50 is a reserve for scrambling (the password gets longer)

            // obtain the plain password using the original FTP-scrambled method
            ConvertStringRegToTxt(password, PASSWORD_MAX_SIZE, passwordReg);
            UnscramblePassword(password);

            if (password[0] != 0)
            {
                // at this moment it is possible that the master password usage is enabled and MP is entered, so we could
                // keep the password encrypted in that case, but we will not complicate matters and postpone possible AES
                // encryption until saving the plug-in configuration; for now we keep the password only scrambled
                passwordManager->EncryptPassword(password, encryptedPassword, encryptedPasswordSize, FALSE);
            }
        }
    }
}

BOOL CFTPServer::Load(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    char itemName[BOOKMARKNAME_MAX_SIZE];
    char address[HOST_MAX_SIZE];
    char initialPath[FTP_MAX_PATH];
    int anonymousConnection;
    char userName[USER_MAX_SIZE];
    BYTE* encryptedPassword;
    int encryptedPasswordSize;
    int savePassword;
    int proxyServerUID;
    char targetPanelPath[MAX_PATH];
    char serverType[SERVERTYPE_MAX_SIZE];
    int transferMode;
    int port;
    int usePassiveMode;
    int keepConnectionAlive;
    int keepAliveSendEvery;
    int keepAliveStopAfter;
    int keepAliveCommand;
    int useMaxConcurrentConnections;
    int maxConcurrentConnections;
    int useServerSpeedLimit;
    double serverSpeedLimit;
    int useListingsCache;
    char initFTPCommands[FTP_MAX_PATH];
    char listCommand[FTPCOMMAND_MAX_SIZE];
    int encryptControlConnection, encryptDataConnection;
    int compressData;

    // take over default values (the object is clean, just initialized)
    strcpy(itemName, HandleNULLStr(ItemName));
    strcpy(address, HandleNULLStr(Address));
    strcpy(initialPath, HandleNULLStr(InitialPath));
    anonymousConnection = AnonymousConnection;
    strcpy(userName, HandleNULLStr(UserName));
    encryptedPassword = NULL;
    encryptedPasswordSize = 0;
    savePassword = SavePassword;
    proxyServerUID = ProxyServerUID;
    strcpy(targetPanelPath, HandleNULLStr(TargetPanelPath));
    strcpy(serverType, HandleNULLStr(ServerType));
    transferMode = TransferMode;
    port = Port;
    usePassiveMode = UsePassiveMode;
    keepConnectionAlive = KeepConnectionAlive;
    keepAliveSendEvery = KeepAliveSendEvery;
    keepAliveStopAfter = KeepAliveStopAfter;
    keepAliveCommand = KeepAliveCommand;
    useMaxConcurrentConnections = UseMaxConcurrentConnections;
    maxConcurrentConnections = MaxConcurrentConnections;
    useServerSpeedLimit = UseServerSpeedLimit;
    serverSpeedLimit = ServerSpeedLimit;
    useListingsCache = UseListingsCache;
    strcpy(initFTPCommands, HandleNULLStr(InitFTPCommands));
    strcpy(listCommand, HandleNULLStr(ListCommand));
    encryptControlConnection = EncryptControlConnection;
    encryptDataConnection = EncryptDataConnection;
    compressData = CompressData;

    if (!registry->GetValue(regKey, CONFIG_FTPSRVNAME, REG_SZ, itemName, BOOKMARKNAME_MAX_SIZE))
        return FALSE; // the name is mandatory
    registry->GetValue(regKey, CONFIG_FTPSRVADDRESS, REG_SZ, address, HOST_MAX_SIZE);
    registry->GetValue(regKey, CONFIG_FTPSRVPATH, REG_SZ, initialPath, FTP_MAX_PATH);
    registry->GetValue(regKey, CONFIG_FTPSRVANONYM, REG_DWORD, &anonymousConnection, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVUSER, REG_SZ, userName, USER_MAX_SIZE);

    LoadPassword(regKey, registry, CONFIG_FTPSRVPASSWD_OLD, CONFIG_FTPSRVPASSWD_SCRAMBLED, CONFIG_FTPSRVPASSWD_ENCRYPTED, &encryptedPassword, &encryptedPasswordSize);

    registry->GetValue(regKey, CONFIG_FTPSRVSAVEPASSWD, REG_DWORD, &savePassword, sizeof(DWORD));
    if (!savePassword && encryptedPassword != NULL && encryptedPasswordSize != 0)
        savePassword = TRUE;
    registry->GetValue(regKey, CONFIG_FTPSRVPROXYSRVUID, REG_DWORD, &proxyServerUID, sizeof(DWORD));
    if (proxyServerUID != -1 && proxyServerUID != -2 &&
        !Config.FTPProxyServerList.IsValidUID(proxyServerUID))
    {
        proxyServerUID = -2; // "default"
    }
    registry->GetValue(regKey, CONFIG_FTPSRVTGTPATH, REG_SZ, targetPanelPath, MAX_PATH);
    registry->GetValue(regKey, CONFIG_FTPSRVTYPE, REG_SZ, serverType, SERVERTYPE_MAX_SIZE);
    registry->GetValue(regKey, CONFIG_FTPSRVTRANSFMODE, REG_DWORD, &transferMode, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVPORT, REG_DWORD, &port, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVPASV, REG_DWORD, &usePassiveMode, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVKALIVE, REG_DWORD, &keepConnectionAlive, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVKASENDEVERY, REG_DWORD, &keepAliveSendEvery, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVKASTOPAFTER, REG_DWORD, &keepAliveStopAfter, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVKACOMMAND, REG_DWORD, &keepAliveCommand, sizeof(DWORD));
    if (registry->GetValue(regKey, CONFIG_FTPSRVUSEMAXCON, REG_DWORD, &maxConcurrentConnections, sizeof(DWORD)))
    {
        useMaxConcurrentConnections = (maxConcurrentConnections == -1 ? 0 : 1);
        if (maxConcurrentConnections == -1)
            maxConcurrentConnections = MaxConcurrentConnections; // do not leave -1 there -> use the default value
    }
    char num[30];
    if (registry->GetValue(regKey, CONFIG_FTPSRVSPDLIM, REG_SZ, num, 30))
    {
        serverSpeedLimit = atof(num);
        useServerSpeedLimit = (serverSpeedLimit == -1 ? 0 : 1);
        if (serverSpeedLimit == -1)
            serverSpeedLimit = ServerSpeedLimit; // do not leave -1 there -> use the default value
    }
    registry->GetValue(regKey, CONFIG_FTPSRVUSELISTINGSCACHE, REG_DWORD, &useListingsCache, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVINITFTPCMDS, REG_SZ, initFTPCommands, FTP_MAX_PATH);
    registry->GetValue(regKey, CONFIG_FTPSRVLISTCMD, REG_SZ, listCommand, FTPCOMMAND_MAX_SIZE);
    if (strcmp(listCommand, LIST_CMD_TEXT) == 0)
        listCommand[0] = 0;
    registry->GetValue(regKey, CONFIG_FTPSRVENCRYPTCONTROLCONNECTION, REG_DWORD, &encryptControlConnection, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVENCRYPTDATACONNECTION, REG_DWORD, &encryptDataConnection, sizeof(DWORD));
    registry->GetValue(regKey, CONFIG_FTPSRVCOMPRESSDATA, REG_DWORD, &compressData, sizeof(DWORD));

    BOOL ret = Set(itemName,
                   GetStrOrNULL(address),
                   GetStrOrNULL(initialPath),
                   anonymousConnection,
                   GetStrOrNULL(userName),
                   encryptedPassword, encryptedPasswordSize,
                   savePassword,
                   proxyServerUID,
                   GetStrOrNULL(targetPanelPath),
                   GetStrOrNULL(serverType),
                   transferMode,
                   port,
                   usePassiveMode,
                   keepConnectionAlive,
                   useMaxConcurrentConnections,
                   maxConcurrentConnections,
                   useServerSpeedLimit,
                   serverSpeedLimit,
                   useListingsCache,
                   GetStrOrNULL(initFTPCommands),
                   GetStrOrNULL(listCommand),
                   keepAliveSendEvery,
                   keepAliveStopAfter,
                   keepAliveCommand,
                   encryptControlConnection,
                   encryptDataConnection,
                   compressData);
    if (encryptedPassword != NULL)
    {
        memset(encryptedPassword, 0, encryptedPasswordSize);
        SalamanderGeneral->Free(encryptedPassword);
    }
    return ret;
}

BOOL IsNotEmptyStr(const char* s)
{
    return s != NULL && *s != 0;
}

BYTE* DupEncryptedPassword(const BYTE* password, int size)
{
    if (password == NULL || size == 0)
        return NULL;

    BYTE* buf = (BYTE*)SalamanderGeneral->Alloc(size);
    memcpy(buf, password, size);
    return buf;
}

void UpdateEncryptedPassword(BYTE** password, int* passwordSize, const BYTE* newPassword, int newPasswordSize)
{
    if (*password != NULL)
    {
        if (newPassword == *password)
            return; // assigning the same password (no change should occur)
        memset(*password, 0, *passwordSize);
        SalamanderGeneral->Free(*password);
        *password = NULL;
    }
    *passwordSize = 0;
    if (newPassword != NULL)
    {
        *password = (BYTE*)SalamanderGeneral->Alloc(newPasswordSize);
        memcpy(*password, newPassword, newPasswordSize);
        *passwordSize = newPasswordSize;
    }
}

void CFTPServer::Save(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    registry->SetValue(regKey, CONFIG_FTPSRVNAME, REG_SZ, HandleNULLStr(ItemName), -1);
    if (IsNotEmptyStr(Address))
        registry->SetValue(regKey, CONFIG_FTPSRVADDRESS, REG_SZ, Address, -1);
    if (IsNotEmptyStr(InitialPath))
        registry->SetValue(regKey, CONFIG_FTPSRVPATH, REG_SZ, InitialPath, -1);
    registry->SetValue(regKey, CONFIG_FTPSRVANONYM, REG_DWORD, &AnonymousConnection, sizeof(DWORD));
    if (!AnonymousConnection)
    {
        if (IsNotEmptyStr(UserName))
            registry->SetValue(regKey, CONFIG_FTPSRVUSER, REG_SZ, UserName, -1);

        if (SavePassword)
        {
            if (EncryptedPassword != NULL && EncryptedPasswordSize > 0)
            {
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                BOOL encrypted = passwordManager->IsPasswordEncrypted(EncryptedPassword, EncryptedPasswordSize);
                registry->SetValue(regKey, encrypted ? CONFIG_FTPSRVPASSWD_ENCRYPTED : CONFIG_FTPSRVPASSWD_SCRAMBLED, REG_BINARY, EncryptedPassword, EncryptedPasswordSize);
            }
            registry->SetValue(regKey, CONFIG_FTPSRVSAVEPASSWD, REG_DWORD, &SavePassword, sizeof(DWORD));
        }
    }

    if (ProxyServerUID != -2)
        registry->SetValue(regKey, CONFIG_FTPSRVPROXYSRVUID, REG_DWORD, &ProxyServerUID, sizeof(DWORD));
    if (IsNotEmptyStr(TargetPanelPath))
        registry->SetValue(regKey, CONFIG_FTPSRVTGTPATH, REG_SZ, TargetPanelPath, -1);
    if (IsNotEmptyStr(ServerType))
        registry->SetValue(regKey, CONFIG_FTPSRVTYPE, REG_SZ, ServerType, -1);
    if (TransferMode != 0)
        registry->SetValue(regKey, CONFIG_FTPSRVTRANSFMODE, REG_DWORD, &TransferMode, sizeof(DWORD));
    if (Port != IPPORT_FTP)
        registry->SetValue(regKey, CONFIG_FTPSRVPORT, REG_DWORD, &Port, sizeof(DWORD));
    if (UsePassiveMode != 2)
        registry->SetValue(regKey, CONFIG_FTPSRVPASV, REG_DWORD, &UsePassiveMode, sizeof(DWORD));
    if (KeepConnectionAlive != 2)
        registry->SetValue(regKey, CONFIG_FTPSRVKALIVE, REG_DWORD, &KeepConnectionAlive, sizeof(DWORD));
    if (KeepConnectionAlive == 1) // store only when explicitly configured
    {
        registry->SetValue(regKey, CONFIG_FTPSRVKASENDEVERY, REG_DWORD, &KeepAliveSendEvery, sizeof(DWORD));
        registry->SetValue(regKey, CONFIG_FTPSRVKASTOPAFTER, REG_DWORD, &KeepAliveStopAfter, sizeof(DWORD));
        registry->SetValue(regKey, CONFIG_FTPSRVKACOMMAND, REG_DWORD, &KeepAliveCommand, sizeof(DWORD));
    }
    if (UseMaxConcurrentConnections != 2)
    {
        DWORD dw;
        if (UseMaxConcurrentConnections == 1)
            dw = MaxConcurrentConnections;
        else
            dw = -1;
        registry->SetValue(regKey, CONFIG_FTPSRVUSEMAXCON, REG_DWORD, &dw, sizeof(DWORD));
    }
    if (UseServerSpeedLimit != 2)
    {
        char num[30];
        sprintf(num, "%g", (UseServerSpeedLimit == 1 ? ServerSpeedLimit : -1.0));
        registry->SetValue(regKey, CONFIG_FTPSRVSPDLIM, REG_SZ, num, -1);
    }
    if (UseListingsCache != 2)
        registry->SetValue(regKey, CONFIG_FTPSRVUSELISTINGSCACHE, REG_DWORD, &UseListingsCache, sizeof(DWORD));
    if (IsNotEmptyStr(InitFTPCommands))
        registry->SetValue(regKey, CONFIG_FTPSRVINITFTPCMDS, REG_SZ, InitFTPCommands, -1);
    if (IsNotEmptyStr(ListCommand))
        registry->SetValue(regKey, CONFIG_FTPSRVLISTCMD, REG_SZ, ListCommand, -1);

    if (EncryptControlConnection != 0)
        registry->SetValue(regKey, CONFIG_FTPSRVENCRYPTCONTROLCONNECTION, REG_DWORD, &EncryptControlConnection, sizeof(DWORD));
    if (EncryptDataConnection != 0)
        registry->SetValue(regKey, CONFIG_FTPSRVENCRYPTDATACONNECTION, REG_DWORD, &EncryptDataConnection, sizeof(DWORD));
    if (CompressData != -1)
        registry->SetValue(regKey, CONFIG_FTPSRVCOMPRESSDATA, REG_DWORD, &CompressData, sizeof(DWORD));
}

BOOL CFTPServer::EnsurePasswordCanBeDecrypted(HWND hParent)
{
    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
    if (!AnonymousConnection && EncryptedPassword != NULL &&
        passwordManager->IsPasswordEncrypted(EncryptedPassword, EncryptedPasswordSize))
    {
        // verify whether a master password is needed to decrypt the password
        if (passwordManager->IsUsingMasterPassword() && !passwordManager->IsMasterPasswordSet())
        {
            if (!passwordManager->AskForMasterPassword(hParent))
                return FALSE; // the user did not enter the correct master password
        }
        // verify that this is the correct master password for this password
        if (!passwordManager->DecryptPassword(EncryptedPassword, EncryptedPasswordSize, NULL))
        {
            int ret = SalamanderGeneral->SalMessageBox(hParent, LoadStr(IDS_CANNOT_DECRYPT_PASSWORD_DELETE),
                                                       LoadStr(IDS_FTPERRORTITLE), MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_DEFBUTTON2 | MB_ICONEXCLAMATION);
            if (ret == IDNO)
                return FALSE; // failed to decrypt the password

            // the user wanted to delete the password
            UpdateEncryptedPassword(&EncryptedPassword, &EncryptedPasswordSize, NULL, 0);
            // clear the save password checkbox
            SavePassword = FALSE;
        }
    }
    return TRUE;
}
