// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "..\7zip\7za\c\7zVersion.h"

#include "7zip.h"
#include "dialogs.h"
#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"

#define INITGUID

#include "7zclient.h"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro archivator
CPluginInterfaceForArchiver InterfaceForArchiver;

// cast interfacu CPluginInterface pro viewer
//CPluginInterfaceForViewer InterfaceForViewer;

// interface pro menu
CPluginInterfaceForMenuExt InterfaceForMenuExt;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;

int ConfigVersion = 0;

// CURRENT_CONFIG_VERSION history
// 1: ?
// 2: ?
// 3: Igor zmenil default hodnoty pro LZMA kompresi (velikost slovniku, atd). Zmen je vice
//    takze jsme se Honzou Paterou dohodli, ze pri importu starych konfiguraci budeme
//    nastaveni komprese ignorovat a pouziji se tako nove defaulty.
#define CURRENT_CONFIG_VERSION 3
const char* CONFIG_VERSION = "Version";

CConfig Config;

int SortByExtDirsAsFiles = FALSE; // aktualni hodnota konfiguracni promenne Salamandera SALCFG_SORTBYEXTDIRSASFILES

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
// pro archiv i pro FS - promenne se sdileji
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;

// nazvy polozek v registrech
const char* CONFIG_SHOW_EXTENDED_OPTIONS = "Show Extended Options";
const char* CONFIG_EXTENDED_LIST_INFO = "Extended List Info";
const char* CONFIG_LIST_INFO_PACKED_SIZE = "List Info Packed Size";
const char* CONFIG_LIST_INFO_METHOD = "List Info Method";

const char* CONFIG_COL_PACKEDSIZE_FIXEDWIDTH = "Column PackedSize FixedWidth";
const char* CONFIG_COL_PACKEDSIZE_WIDTH = "Column PackedSize Width";
const char* CONFIG_COL_METHOD_FIXEDWIDTH = "Column Method FixedWidth";
const char* CONFIG_COL_METHOD_WIDTH = "Column Method Width";

const char* CONFIG_COMPRESS_LEVEL = "Compression Level";
const char* CONFIG_COMPRESS_METHOD = "Compression Method";
const char* CONFIG_DICT_SIZE = "Dictionary Size";
const char* CONFIG_WORD_SIZE = "Word Size";
const char* CONFIG_SOLID_ARCHIVE = "Solid Archive";

// menu id
#define IDM_TESTARCHIVE 1

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DLLInstance = hinstDLL;
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        break;
    }
    }
    return TRUE; // DLL can be loaded
}

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "7-Zip" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "7-Zip" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();
    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    // nastavime jmeno souboru s helpem
    SalamanderGeneral->SetHelpFileName("7zip.chm");

    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                          sizeof(SortByExtDirsAsFiles), NULL);

    if (!InterfaceForArchiver.Init())
        return NULL;

    if (!InitializeWinLib("7zip", DLLInstance))
        return NULL;
    SetWinLibStrings("Invalid number!", LoadStr(IDS_PLUGINNAME));
    SetupWinLibHelp(HTMLHelpCallback);

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_PANELARCHIVEREDIT | FUNCTION_CUSTOMARCHIVERPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM, VERSINFO_COPYRIGHT, LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "7zip", "7z");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

BOOL Warning(int resID, BOOL quiet, ...)
{
    if (!quiet)
    {
        char buf[1024];
        buf[0] = 0;
        va_list arglist;
        va_start(arglist, quiet);
        vsprintf(buf, LoadStr(resID), arglist);
        va_end(arglist);

        SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_WARNING);
    }
    return FALSE;
}

BOOL Error(int resID, BOOL quiet, ...)
{
    if (!quiet)
    {
        char buf[1024];
        buf[0] = 0;
        va_list arglist;
        va_start(arglist, quiet);
        vsprintf(buf, LoadStr(resID), arglist);
        va_end(arglist);

        SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
    }
    return FALSE;
}

BOOL Error(HWND hParent, int resID, BOOL quiet, ...)
{
    if (!quiet)
    {
        char buf[1024];
        buf[0] = 0;
        va_list arglist;
        va_start(arglist, quiet);
        vsprintf(buf, LoadStr(resID), arglist);
        va_end(arglist);

        SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(IDS_PLUGINNAME), MB_OK);
    }
    return FALSE;
}

/*
BOOL
SysError(char *msg, DWORD err, BOOL quiet)
{
  if (!quiet)
    if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)  // jde o chybu
    {
      char buf[1024];
      sprintf(buf, "%s\n\n%s", msg, SalamanderGeneral->GetErrorText(err));
      SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
    }

  return FALSE;
}
*/

BOOL SysError(int resID, DWORD err, BOOL quiet, ...)
{
    if (!quiet)
    {
        char msg[1024];
        msg[0] = 0;
        va_list arglist;
        va_start(arglist, quiet);
        vsprintf(msg, LoadStr(resID), arglist);
        va_end(arglist);

        char buf[2048 + 4];
        sprintf(buf, "%s\n\n%s", msg, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
    }

    return FALSE;
}
/*
BOOL SysError(int title, int error, ...)
{
  int lastErr = GetLastError();
  CALL_STACK_MESSAGE3("SysError(%d, %d, ...)", title, error);
  char buf[1024];
  *buf = 0;
  va_list arglist;
  va_start(arglist, error);
  vsprintf(buf, LoadStr(error), arglist);
  va_end(arglist);
  if (lastErr != ERROR_SUCCESS)
  {
    strcat(buf, " ");
    int l = strlen(buf);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
  }
  SalamanderGeneral->ShowMessageBox(buf, LoadStr(title), MSGBOX_ERROR);
  return FALSE;
}
*/

void GetInfo(char* buffer, const FILETIME* lastWrite, CQuadWord size)
{
    CALL_STACK_MESSAGE2("GetInfo(, , 0x%I64X)", size.Value);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, size), date, time);
}

void GetInfo(char* buffer, const FILETIME* lastWrite, UINT64 size)
{
    CALL_STACK_MESSAGE2("GetInfo(, , 0x%I64X)", size);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    CQuadWord qwsize;
    qwsize.Value = size;

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, qwsize), date, time);
}

BOOL SafeDeleteFile(const char* fileName, BOOL& silent)
{
    int mbRet;

    do
    {
        mbRet = DIALOG_OK;
        if (!::DeleteFile(fileName) && !silent)
        {
            DWORD err = ::GetLastError();
            mbRet = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL,
                                                   fileName, SalamanderGeneral->GetErrorText(err), LoadStr(IDS_DELETE_ERROR));
        }
    } while (mbRet == DIALOG_RETRY);

    BOOL ret = FALSE;
    switch (mbRet)
    {
    case DIALOG_OK:
        ret = TRUE;
        break;
    case DIALOG_CANCEL:
        ret = FALSE;
        break;
    case DIALOG_SKIP:
        ret = TRUE;
        break;
    case DIALOG_SKIPALL:
        ret = TRUE;
        silent = TRUE;
        break;
    }

    return ret;
}

BOOL SafeRemoveDirectory(const char* fileName, BOOL& silent)
{
    int mbRet;

    do
    {
        mbRet = DIALOG_OK;
        if (!::RemoveDirectory(fileName) && !silent)
        {
            DWORD err = ::GetLastError();
            mbRet = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL,
                                                   fileName, SalamanderGeneral->GetErrorText(err), LoadStr(IDS_DELETE_ERROR));
        }
    } while (mbRet == DIALOG_RETRY);

    BOOL ret = FALSE;
    switch (mbRet)
    {
    case DIALOG_OK:
        ret = TRUE;
        break;
    case DIALOG_CANCEL:
        ret = FALSE;
        break;
    case DIALOG_SKIP:
        ret = TRUE;
        break;
    case DIALOG_SKIPALL:
        ret = TRUE;
        silent = TRUE;
        break;
    }

    return ret;
}

// ****************************************************************************
//
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n"
                "7za.dll " MY_VERSION "\n\n"                   // 7-Zip engine version
                VERSINFO_COPYRIGHT "\n" MY_COPYRIGHT_CR "\n\n" // Igor Pavlov copyright
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);

    ReleaseWinLib(DLLInstance);

    return TRUE;
}

void CPluginInterface::SetDefaultConfiguration()
{
    Config.ExtendedListInfo = FALSE;
    Config.ListInfoMethod = FALSE;
    Config.ListInfoPackedSize = FALSE;

    Config.ColumnPackedSizeFixedWidth = 0;
    Config.ColumnPackedSizeWidth = 0;
    Config.ColumnMethodFixedWidth = 0;
    Config.ColumnMethodWidth = 0;

    Config.ShowExtendedOptions = TRUE;

    Config.CompressParams.CompressLevel = COMPRESS_LEVEL_NORMAL;
    Config.CompressParams.Method = CCompressParams::LZMA;
    Config.CompressParams.DictSize = 16 * 1024; // Dictionary Size in KB
    Config.CompressParams.WordSize = 32;
    Config.CompressParams.SolidArchive = TRUE;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
            ConfigVersion = 0; // default configuration
    }
    else
    {
        ConfigVersion = 0; // default configuration
    }

    // set config defaults
    SetDefaultConfiguration();

    if (regKey != NULL) // load z registry
    {
        registry->GetValue(regKey, CONFIG_SHOW_EXTENDED_OPTIONS, REG_DWORD, &Config.ShowExtendedOptions, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_EXTENDED_LIST_INFO, REG_DWORD, &Config.ExtendedListInfo, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_LIST_INFO_PACKED_SIZE, REG_DWORD, &Config.ListInfoPackedSize, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_LIST_INFO_METHOD, REG_DWORD, &Config.ListInfoMethod, sizeof(DWORD));

        registry->GetValue(regKey, CONFIG_COL_PACKEDSIZE_FIXEDWIDTH, REG_DWORD, &Config.ColumnPackedSizeFixedWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_PACKEDSIZE_WIDTH, REG_DWORD, &Config.ColumnPackedSizeWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_METHOD_FIXEDWIDTH, REG_DWORD, &Config.ColumnMethodFixedWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_METHOD_WIDTH, REG_DWORD, &Config.ColumnMethodWidth, sizeof(DWORD));

        if (ConfigVersion >= 1)
        {
            // compress params
            registry->GetValue(regKey, CONFIG_SOLID_ARCHIVE, REG_DWORD, &Config.CompressParams.SolidArchive, sizeof(DWORD));
            if (registry->GetValue(regKey, CONFIG_COMPRESS_METHOD, REG_DWORD, &Config.CompressParams.Method, sizeof(DWORD)) &&
                (Config.CompressParams.Method != CCompressParams::LZMA || ConfigVersion >= 3)) // na konfiguraci verze 3 jsme udelali reset defaultu pro LZMA, protoze jsou jine
            {
                registry->GetValue(regKey, CONFIG_COMPRESS_LEVEL, REG_DWORD, &Config.CompressParams.CompressLevel, sizeof(DWORD));
                registry->GetValue(regKey, CONFIG_DICT_SIZE, REG_DWORD, &Config.CompressParams.DictSize, sizeof(DWORD));
                registry->GetValue(regKey, CONFIG_WORD_SIZE, REG_DWORD, &Config.CompressParams.WordSize, sizeof(DWORD));
            }
        }
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_SHOW_EXTENDED_OPTIONS, REG_DWORD, &Config.ShowExtendedOptions, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_EXTENDED_LIST_INFO, REG_DWORD, &Config.ExtendedListInfo, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_LIST_INFO_PACKED_SIZE, REG_DWORD, &Config.ListInfoPackedSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_LIST_INFO_METHOD, REG_DWORD, &Config.ListInfoMethod, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_COL_PACKEDSIZE_FIXEDWIDTH, REG_DWORD, &Config.ColumnPackedSizeFixedWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_PACKEDSIZE_WIDTH, REG_DWORD, &Config.ColumnPackedSizeWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_METHOD_FIXEDWIDTH, REG_DWORD, &Config.ColumnMethodFixedWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_METHOD_WIDTH, REG_DWORD, &Config.ColumnMethodWidth, sizeof(DWORD));

    // config version == 2
    // compress params
    registry->SetValue(regKey, CONFIG_COMPRESS_LEVEL, REG_DWORD, &Config.CompressParams.CompressLevel, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COMPRESS_METHOD, REG_DWORD, &Config.CompressParams.Method, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_DICT_SIZE, REG_DWORD, &Config.CompressParams.DictSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_WORD_SIZE, REG_DWORD, &Config.CompressParams.WordSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SOLID_ARCHIVE, REG_DWORD, &Config.CompressParams.SolidArchive, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CConfigurationDialog dlg(parent);

    dlg.Cfg = Config;
    if (dlg.Execute() == IDOK)
    {
        Config = dlg.Cfg;
        if (SalamanderGeneral->GetPanelPluginData(PANEL_LEFT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_LEFT);
        if (SalamanderGeneral->GetPanelPluginData(PANEL_RIGHT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_RIGHT);
    }
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // pri pridavani dalsich pripon, je treba zvedat CURRENT_CONFIG_VERSION

    // ZAKLADNI CAST
    // AddViewer a AddPanelArchiver budou podlehat CASTI PRO UPGRADY
    //  salamander->AddViewer("*.7z", FALSE); // default (install pluginu), jinak Salam ignoruje

    salamander->AddPanelArchiver("7z", TRUE, FALSE);

    salamander->AddCustomPacker("7-Zip (Plugin)", "7z", ConfigVersion < 1);
    salamander->AddCustomUnpacker("7-Zip (Plugin)", "*.7z", ConfigVersion < 1);

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_TESTARCHIVE
  {MNTT_PE, 0
};
*/

    // do menu dame ladici polozku, ktera umozni uzivatelum snadno snadno zaslat prvni 1MB z ISO image
    // do menu dame polozku pro testovani integrity archivu
    salamander->AddMenuItem(-1, LoadStr(IDS_TESTARCHIVE), 0, IDM_TESTARCHIVE, FALSE, MENU_EVENT_ARCHIVE_FOCUSED,
                            MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    // nastavime ikonku pluginu
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_7ZIP),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    /*
  // CAST PRO UPGRADY
  if (ConfigVersion < 2) // pridani nrg, pdi, cdi, cif, ncd
  {
    salamander->AddViewer("*.nrg;*.pdi;*.cdi;*.cif;*.ncd", TRUE);
    salamander->AddPanelArchiver("nrg;pdi;cdi;cif;ncd", FALSE, TRUE);
  }

  if (ConfigVersion < 3) // pridani c2d
  {
    salamander->AddViewer("*.c2d", TRUE);
    salamander->AddPanelArchiver("c2d", FALSE, TRUE);
  }
*/
}

void CPluginInterface::Event(int event, DWORD param)
{
    if (event == PLUGINEVENT_CONFIGURATIONCHANGED)
    {
        SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                              sizeof(SortByExtDirsAsFiles), NULL);
    }
}

void CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    delete ((CPluginDataInterface*)pluginData);
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    return &InterfaceForArchiver;
}

/*
CPluginInterfaceForViewerAbstract *
CPluginInterface::GetInterfaceForViewer()
{
  return &InterfaceForViewer;
}
*/

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

// ****************************************************************************
//
// CPluginInterfaceForArchiver
//

CPluginInterfaceForArchiver::CPluginInterfaceForArchiver()
{
}

BOOL CPluginInterfaceForArchiver::Init()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Init()");

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander,
                                              const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginDataPar)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);

    C7zClient* client = new C7zClient();
    if (client == NULL)
        return Error(IDS_INSUFFICIENT_MEMORY);

    CPluginDataInterface* pluginData;
    pluginDataPar = pluginData = new CPluginDataInterface(client);
    if (pluginData == NULL)
    {
        delete client;
        return Error(IDS_INSUFFICIENT_MEMORY);
    }

    // otevrit archiv
    // predat kompletni listing jadru Salamandera
    if (!client->ListArchive(fileName, dir, pluginData, pluginData->Password))
    {
        delete pluginData;
        return FALSE;
    }

    return TRUE;
}

void FreeString(void* strig)
{
    free(strig);
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginDataPar, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    if (pluginDataPar == NULL)
    {
        TRACE_E("Internal error");
        return FALSE;
    }

    CPluginDataInterface* pluginData = (CPluginDataInterface*)pluginDataPar;
    C7zClient* client = pluginData->Get7zClient();
    if (client == NULL)
    {
        TRACE_E("Internal error");
        return FALSE;
    }

    BOOL ret = FALSE;
    // spocitat 'totalSize' pro progress dialog
    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    int itemCount = 0; // pocet zpracovavanych polozek
    const char* name;
    const CFileData* fileData;
    int errorOccured;

    // nejdriv si spocitame, kolik toho budeme zpracovavat a jak to bude veliky
    while ((name = next(SalamanderGeneral->GetMsgBoxParent(), 1, &isDir, &size, &fileData, nextParam, &errorOccured)) != NULL)
    {
        if (fileData->PluginData != 0)
            itemCount++;

        totalSize += size;
    }
    // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci
    if (errorOccured == SALENUM_CANCEL)
        return FALSE;

    BOOL delTempDir = TRUE;
    if (itemCount > 0 &&
        SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                         targetDir, totalSize, LoadStr(IDS_UNPACKING_ARCHIVE)))
    {
        salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), FALSE, NULL, FALSE);
        salamander->ProgressDialogAddText(LoadStr(IDS_READING_ARCHIVEITEMS), FALSE);

        // ulozime si zpracovavane polozky do pole
        TIndirectArray<CArchiveItemInfo> itemList(itemCount, 10, dtDelete);
        next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);
        while ((name = next(NULL /* podruhe uz chyby nepiseme */, 1, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
        {
            if (fileData->PluginData != 0)
            {
                CArchiveItemInfo* aii = new CArchiveItemInfo(name, fileData, isDir == TRUE);
                if (aii == NULL)
                    return Error(IDS_INSUFFICIENT_MEMORY);
                itemList.Add(aii);
            }
        }

        salamander->ProgressDialogAddText(LoadStr(IDS_UNPACKING), FALSE);
        ret = client->Decompress(salamander, fileName, targetDir, &itemList, pluginData->Password) != OPER_CANCEL;
        salamander->CloseProgressDialog();
    }

    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginDataPar,
                                                const char* nameInArchive, const CFileData* fileData,
                                                const char* targetDir, const char* newFileName,
                                                BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, , %s, , %s, ,)", fileName,
                        nameInArchive, targetDir);

    if (newFileName != NULL)
    {
        *renamingNotSupported = TRUE;
        return FALSE;
    }

    if (pluginDataPar == NULL)
    {
        TRACE_E("Internal error");
        return FALSE;
    }

    CPluginDataInterface* pluginData = (CPluginDataInterface*)pluginDataPar;
    C7zClient* client = pluginData->Get7zClient();
    if (client == NULL)
    {
        TRACE_E("Internal error");
        return FALSE;
    }

    BOOL ret = FALSE;
    TIndirectArray<CArchiveItemInfo> archiveItems(1, 10, dtDelete);
    if (fileData->PluginData != 0)
    {
        CArchiveItemInfo* aii = new CArchiveItemInfo(fileData->Name, fileData, FALSE);
        if (aii == NULL)
            return Error(IDS_INSUFFICIENT_MEMORY);
        archiveItems.Add(aii);

        if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                             targetDir, CQuadWord(fileData->Size), LoadStr(IDS_UNPACKING_ARCHIVE)))
        {
            salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), FALSE, NULL, FALSE);
            ret = client->Decompress(salamander, fileName, targetDir, &archiveItems, pluginData->Password, TRUE) != OPER_CANCEL;
            //      ret = client->Decompress(salamander, fileName, targetDir, &archiveItems) == OPER_OK;
            salamander->CloseProgressDialog();
        }
    }

    return ret;
}

// NOTE: it is assumed itemCount is inited on entry
void CalcSize(CSalamanderDirectoryAbstract const* dir, const char* mask, CQuadWord& size, int& itemCount)
{
    int count = dir->GetFilesCount();
    int i;
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetFile(i);
        //    TRACE_I("CalcSize(): file: " << path << (path[0] != 0 ? "\\" : "") << file->Name);

        if (SalamanderGeneral->AgreeMask(file->Name, mask, file->Ext[0] != 0))
        {
            size += file->Size + CQuadWord(1, 0);
            if (file->PluginData != 0)
                itemCount++;
        }
    }

    count = dir->GetDirsCount();
    int j;
    for (j = 0; j < count; j++)
    {
        CFileData const* file = dir->GetDir(j);
        if (file->PluginData != 0)
            itemCount++;
        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(j);
        CalcSize(subDir, mask, size, itemCount);
    }
}

int GatherItems(CSalamanderDirectoryAbstract const* dir, const char* mask, TIndirectArray<CArchiveItemInfo>* archiveItems, char* archivePath)
{
    int archivePathLen = lstrlen(archivePath);
    // soubory
    int count = dir->GetFilesCount();
    int i;
    for (i = 0; i < count; i++)
    {
        CFileData const* fileData = dir->GetFile(i);
        if (SalamanderGeneral->AgreeMask(fileData->Name, mask, fileData->Ext[0] != 0) &&
            fileData->PluginData != 0 &&
            SalamanderGeneral->SalPathAppend(archivePath, fileData->Name, MAX_PATH)) // zneuzijeme archivePath, pro nazev souboru v archivu
        {
            CArchiveItemInfo* aii = new CArchiveItemInfo(archivePath, fileData, FALSE);
            archivePath[archivePathLen] = '\0';
            if (aii == NULL)
            {
                Error(IDS_INSUFFICIENT_MEMORY);
                return OPER_CANCEL;
            }
            archiveItems->Add(aii);
        }
    } // for

    count = dir->GetDirsCount();
    int j;
    for (j = 0; j < count; j++)
    {
        CFileData const* fileData = dir->GetDir(j);
        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(j);
        if (SalamanderGeneral->SalPathAppend(archivePath, fileData->Name, MAX_PATH))
        {
            // do zpracovani pridat i adresare (ale jen ty co maji definovano PluginData)
            if (SalamanderGeneral->AgreeMask(fileData->Name, mask,
                                             strchr(fileData->Name, '.') != NULL) && // u adresare v Ext nemusi byt pripona, proto hledame tecku pres strchr
                fileData->PluginData != 0)
            {
                CArchiveItemInfo* aii = new CArchiveItemInfo(archivePath, fileData, TRUE);
                if (aii == NULL)
                {
                    Error(IDS_INSUFFICIENT_MEMORY);
                    return OPER_CANCEL;
                }
                archiveItems->Add(aii);
            }

            if (GatherItems(subDir, mask, archiveItems, archivePath) == OPER_CANCEL)
                return OPER_CANCEL;
        }
        archivePath[archivePathLen] = '\0';
    }

    return OPER_OK;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    if (delArchiveWhenDone)
        archiveVolumes->Add(fileName, -2); // FIXME: az 7-zip plugin doucime multi-volume archivy (.7z.001, .7z.002, atd.), musime sem napridavat vsechny svazky archivu (aby se smazal kompletni archiv)
    CSalamanderDirectoryAbstract* dir = SalamanderGeneral->AllocSalamanderDirectory(FALSE);
    if (dir == NULL)
        return Error(IDS_INSUFFICIENT_MEMORY);

    BOOL ret = FALSE;

    C7zClient* client = new C7zClient();
    if (client)
    {
        CPluginDataInterface* pluginData = new CPluginDataInterface(client);
        if (pluginData)
        {
            // otevrit archiv
            if (client->ListArchive(fileName, dir, pluginData, pluginData->Password))
            {
                CQuadWord totalSize(0, 0);
                int itemCount = 0;
                CalcSize(dir, mask, totalSize, itemCount);

                //
                BOOL delTempDir = TRUE;
                if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                                     targetDir, totalSize, LoadStr(IDS_UNPACKING_ARCHIVE)))
                {
                    salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), FALSE, NULL, FALSE);
                    salamander->ProgressDialogAddText(LoadStr(IDS_READING_ARCHIVEITEMS), FALSE);

                    char modmask[256];
                    SalamanderGeneral->PrepareMask(modmask, mask);

                    char archivePath[MAX_PATH_LEN];
                    archivePath[0] = '\0';

                    TIndirectArray<CArchiveItemInfo> archiveItems(itemCount, 10, dtDelete);
                    if (GatherItems(dir, modmask, &archiveItems, archivePath) != OPER_CANCEL)
                    {
                        C7zClient* client2 = ((CPluginDataInterface*)pluginData)->Get7zClient();

                        salamander->ProgressDialogAddText(LoadStr(IDS_UNPACKING), FALSE);
                        ret = client2->Decompress(salamander, fileName, targetDir, &archiveItems, pluginData->Password) != OPER_CANCEL;
                    }
                    salamander->CloseProgressDialog();
                }

                dir->Clear(pluginData);
                if (pluginData != NULL)
                    PluginInterface.ReleasePluginDataInterface(pluginData);

                int panel = -1;
                CanCloseArchive(salamander, fileName, TRUE, panel);
            }
            else
            {
                delete (CPluginDataInterface*)pluginData;
            }
        }
        else
        {
            ret = Error(IDS_INSUFFICIENT_MEMORY);
            delete client;
        }
    }
    else
    {
        ret = Error(IDS_INSUFFICIENT_MEMORY);
    }

    SalamanderGeneral->FreeSalamanderDirectory(dir);

    return ret;
}

BOOL CPluginInterfaceForArchiver::CanCloseArchive(CSalamanderForOperationsAbstract* salamander,
                                                  const char* fileName,
                                                  BOOL force, int panel)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::CanCloseArchive(, %s, %d, %d)", fileName, force, panel);

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                const char* archiveRoot, BOOL move, const char* sourcePath,
                                                SalEnumSelection2 next, void* nextParam)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::PackToArchive(, %s, %s, %d, %s, ,)", fileName,
                        archiveRoot, move, sourcePath);

    // otestovat existenci archivu (potrebujeme rozlisit update a createnew archive)
    BOOL isNewArchive = FALSE;
    HANDLE hArchive = ::CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hArchive == INVALID_HANDLE_VALUE)
    {
        isNewArchive = TRUE; // soubor neexistuje, je to novy archiv

        // test, zda lze zapisovat do cilove cesty
        hArchive = INVALID_HANDLE_VALUE;
        hArchive = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hArchive == INVALID_HANDLE_VALUE)
            return SysError(IDS_CANT_CREATE_ARCHIVE, ::GetLastError());
        else
        {
            ::CloseHandle(hArchive);
            ::DeleteFile(fileName);
        }
    }
    else
    {
        ::CloseHandle(hArchive); // soubor existuje, zavreme ho, budeme aktualizovat

        // test na read-only attribute
        DWORD attrs = SalamanderGeneral->SalGetFileAttributes(fileName);
        if (attrs != -1)
        {
            if (attrs & FILE_ATTRIBUTE_READONLY)
                return Error(IDS_READONLY_ARCHIVE);
        }
        // tady soubor nema READ-ONLY attribut, nebo se nepodarilo ziskat atributy a mel by zabrat nasledujici test

        // test, zda lze do souboru psat
        hArchive = INVALID_HANDLE_VALUE;
        hArchive = ::CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hArchive == INVALID_HANDLE_VALUE)
            return SysError(IDS_CANT_UPDATE_ARCHIVE, ::GetLastError(), FALSE, fileName);
        else
            ::CloseHandle(hArchive); // soubor existuje, zavreme ho, budeme aktualizovat
    }

    CCompressParams compressParams;
    bool passwordDefined = false;
    char password[PASSWORD_LEN];
    if (Config.ShowExtendedOptions)
    {
        // zobrazit dialog box s exteneded options
        CExtOptionsDialog dlg(SalamanderGeneral->GetMsgBoxParent());
        if (isNewArchive)
            dlg.SetTitle(LoadStr(IDS_CREATE_NEW_ARCHIVE));
        else
            dlg.SetTitle(LoadStr(IDS_ADD_FILES_TO_ARCHIVE));
        dlg.SetArchiveName(fileName);
        dlg.CompressParams = Config.CompressParams;
        int res = (int)dlg.Execute();
        if (res == IDCANCEL)
            return FALSE;

        if (res == IDOK)
        {
            passwordDefined = dlg.IsPasswordDefined() == TRUE;
            strcpy(password, dlg.GetPassword());

            Config.ShowExtendedOptions = dlg.GetNotAgain() == FALSE;

            // vzit config z ext. dialogu
            compressParams = dlg.CompressParams;
        }
    }
    else
    {
        compressParams = Config.CompressParams;
    }

    // open progress dialog
    if (isNewArchive)
        salamander->OpenProgressDialog(LoadStr(IDS_PACKING_ARCHIVE), FALSE, NULL, FALSE);
    else
        salamander->OpenProgressDialog(LoadStr(IDS_UPDATING_ARCHIVE), FALSE, NULL, FALSE);
    salamander->ProgressDialogAddText(LoadStr(IDS_READING_DIRTREE), FALSE);

    BOOL isDir;
    const char* name;
    const char* dosName; // dummy
    CQuadWord size;
    DWORD attr;
    FILETIME lastWrite;
    int errorOccured;

    // spocitat kolik polozek budeme komprimovat
    // (mohli bysme sice rovnou pouzit nafukovaci pole, ale pri velkym poctu bysme fragmentovali pamet. a 2x iterovat
    // seznamem nas nezabije)
    int itemCount = 0;
    while ((name = next(SalamanderGeneral->GetMsgBoxParent(), 3, &dosName, &isDir, &size,
                        &attr, &lastWrite, nextParam, &errorOccured)) != NULL)
    {
        itemCount++;
    }
    // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci
    if (errorOccured == SALENUM_CANCEL)
    {
        salamander->CloseProgressDialog();
        return FALSE;
    }

    // pripravit seznam komprimovanych polozek
    TIndirectArray<CFileItem> fileList(itemCount, 20, dtDelete);
    next(NULL, -1, NULL, NULL, NULL, NULL, NULL, nextParam, NULL);
    while ((name = next(NULL /* podruhe uz chyby nepiseme */, 3, &dosName, &isDir, &size,
                        &attr, &lastWrite, nextParam, &errorOccured)) != NULL)
    {
        if (errorOccured == SALENUM_ERROR)
            TRACE_I("Not all files and directories from disk will be packed.");

        // vytvareni listu souboru, ktery se maji zapakovat
        CFileItem* fi = new CFileItem(sourcePath, archiveRoot, name, attr, size.Value, lastWrite, isDir == TRUE);
        if (fi == NULL)
        {
            salamander->CloseProgressDialog();
            Error(IDS_INSUFFICIENT_MEMORY);
            return FALSE;
        }
        if (move)
            fi->CanDelete = TRUE;
        fileList.Add(fi);
    }
    if (errorOccured != SALENUM_SUCCESS)
        TRACE_I("Not all files and directories from disk will be packed.");

    // vytvorit archiv
    C7zClient client;
    if (isNewArchive)
        salamander->ProgressDialogAddText(LoadStr(IDS_PACKING), FALSE);
    else
        salamander->ProgressDialogAddText(LoadStr(IDS_UPDATING), FALSE);
    BOOL ret = client.Update(salamander, fileName, sourcePath, isNewArchive, &fileList, &compressParams, passwordDefined,
                             GetUnicodeString(password)) == OPER_OK;

    // smazat po sobe soubory, pokud presouvame do archivu
    if (move && ret)
    { // nejprve zamkneme soubor archivu, abysme si ho nemohli sami smazat (bug: https://forum.altap.cz/viewtopic.php?f=3&t=3859)
        while (1)
        {
            hArchive = ::CreateFile(fileName, GENERIC_READ /* zkousel jsem 0, ale system pak dovolil smazani souboru */,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            if (hArchive != INVALID_HANDLE_VALUE)
                break;
            DWORD err = ::GetLastError();
            if (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(),
                                               BUTTONS_RETRYCANCEL, fileName,
                                               SalamanderGeneral->GetErrorText(err),
                                               LoadStr(IDS_ARCLOCK_ERROR)) != DIALOG_RETRY)
            {
                break;
            }
        }
        if (hArchive != INVALID_HANDLE_VALUE)
        {
            BOOL fileSilent = FALSE;
            BOOL dirSilent = FALSE;

            salamander->ProgressDialogAddText(LoadStr(IDS_REMOVING_FILES), FALSE);
            salamander->ProgressSetTotalSize(CQuadWord(itemCount, 0), CQuadWord(-1, -1));
            salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), FALSE);

            // nejdriv smazneme soubory (ty co maji CanDelete)
            int i;
            for (i = 0; i < fileList.Count; i++)
            {
                CFileItem* fi = fileList[i];
                if (fi->CanDelete && !fi->IsDir)
                {
                    // smazat soubor
                    CSysString name2 = GetAnsiString(fi->FullPath);

                    // shodit prip read-only attribut
                    SalamanderGeneral->ClearReadOnlyAttr(name2);

                    if (!SafeDeleteFile(name2, fileSilent))
                    {
                        // nepodarilo se smazat soubor a user dal cancel
                        ret = FALSE;
                        break;
                    }

                    if (!salamander->ProgressAddSize(1, TRUE))
                    {
                        salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                        salamander->ProgressEnableCancel(FALSE);
                        // pri mazani dal user cancel
                        ret = FALSE;
                        break;
                    }
                }
            }

            // soubory smazany (zadny cancel), pokracujeme v mazani, nyni jsou na rade prazdne adresare
            if (ret)
            {
                // priprava bufferu pro jmena
                char sourceName[MAX_PATH + 1]; // buffer pro plne jmeno na disku
                strcpy(sourceName, sourcePath);
                char* endSource = sourceName + strlen(sourceName); // misto pro jmena z enumerace 'next'
                if (endSource > sourceName && *(endSource - 1) != '\\')
                {
                    *endSource++ = '\\';
                    *endSource = 0;
                }
                int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // max. pocet znaku pro jmeno z enumerace 'next'

                // smazeme adresare, pokud v nich neco zbylo, tak se nesmazou a to je dobre :)
                // protoze se iteruje od listu ke koreni, tak muzeme mazat timto zpusobem
                next(NULL, -1, NULL, NULL, NULL, NULL, NULL, nextParam, NULL);
                while ((name = next(NULL /* podruhe uz chyby nepiseme */, 3, &dosName, &isDir, &size,
                                    &attr, &lastWrite, nextParam, NULL)) != NULL)
                {
                    if (isDir)
                    {
                        if ((int)strlen(name) < endSourceSize)
                        {
                            strcpy_s(endSource, endSourceSize, name);
                            // shodit prip read-only attribut
                            SalamanderGeneral->ClearReadOnlyAttr(sourceName, attr);
                            ::RemoveDirectory(sourceName);
                        }
                        else
                            TRACE_E("Unable to delete directory, its full name is too long: " << sourcePath << " : " << name);
                        if (!salamander->ProgressAddSize(1, TRUE))
                        {
                            salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                            salamander->ProgressEnableCancel(FALSE);
                            // pri mazani dal user cancel
                            ret = FALSE;
                            break;
                        }
                    }
                }
            }
            ::CloseHandle(hArchive);
        }
    }

    salamander->CloseProgressDialog();

    return ret;
}

BOOL CPluginInterfaceForArchiver::DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                    CPluginDataInterfaceAbstract* pluginDataPar, const char* archiveRoot,
                                                    SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DeleteFromArchive(, %s, , %s, ,)",
                        fileName, archiveRoot);

    //  char password[PASSWORD_LEN];
    bool passwordDefined = false;

    // open progress dialog
    salamander->OpenProgressDialog(LoadStr(IDS_UPDATING_ARCHIVE), FALSE, NULL, FALSE);
    salamander->ProgressDialogAddText(LoadStr(IDS_READING_ARCHIVEITEMS), FALSE);

    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    const char* name;
    const CFileData* fileData;
    int errorOccured;
    int itemCount = 0;
    while ((name = next(SalamanderGeneral->GetMsgBoxParent(), 1, &isDir, &size, &fileData, nextParam, &errorOccured)) != NULL)
    {
        if (fileData->PluginData != 0)
            itemCount++;
    }
    // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci
    BOOL ret = TRUE;
    if (errorOccured == SALENUM_CANCEL)
        ret = FALSE;
    if (ret && itemCount > 0)
    {
        TIndirectArray<CArchiveItemInfo> archiveItems(itemCount, 20, dtDelete);
        next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);
        while ((name = next(NULL /* podruhe uz chyby nepiseme */, 1, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
        {
            if (fileData->PluginData != 0)
            {
                CArchiveItemInfo* aii = new CArchiveItemInfo(name, fileData, isDir == TRUE);
                if (aii == NULL)
                    return Error(IDS_INSUFFICIENT_MEMORY);
                archiveItems.Add(aii);
            }
        }

        salamander->ProgressDialogAddText(LoadStr(IDS_REMOVING), FALSE);
        CPluginDataInterface* pluginData = (CPluginDataInterface*)pluginDataPar;
        C7zClient* client = pluginData->Get7zClient();
        ret = client->Delete(salamander, fileName, &archiveItems, passwordDefined, pluginData->Password) == OPER_OK;
    }
    salamander->CloseProgressDialog();
    return ret;
}

//
// ****************************************************************************
// CPluginInterfaceForViewer
//
/*
BOOL
CPluginInterfaceForViewer::ViewFile(const char *name, int left, int top, int width,
                                    int height, UINT showCmd, BOOL alwaysOnTop,
                                    BOOL returnLock, HANDLE *lock, BOOL *lockOwner,
                                    CSalamanderPluginViewerData *viewerData,
                                    int enumFilesSourceUID, int enumFilesCurrentIndex)
{
  CALL_STACK_MESSAGE11("CPluginInterfaceForViewer::ViewFile(%s, %d, %d, %d, %d, "
                      "0x%X, %d, %d, , , , %d, %d)", name, left, top, width, height,
                      showCmd, alwaysOnTop, returnLock, enumFilesSourceUID, enumFilesCurrentIndex);

  // 'lock' ani 'lockOwner' nenastavujeme, staci nam platnost souboru 'name' jen
  // v ramci teto metody

  HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

  CISOImage *image;
  if ((image = new CISOImage()) == NULL)
  {
    SetCursor(hOldCur);
    return Error(IDS_INSUFFICIENT_MEMORY);
  }

  if (!image->Open(name))
  {
    delete image;
    SetCursor(hOldCur);
    return FALSE;
  }

  char tempFileName[MAX_PATH];
  if (SalamanderGeneral->SalGetTempFileName(NULL, "ISO", tempFileName, TRUE, NULL))
  {
    char caption[2000];
    int err;
    CSalamanderPluginInternalViewerData viewerData;

    // vytvorim docasny soubor a naleju do nej dump modulu
    FILE *outStream = fopen(tempFileName, "w");
    if (!image->DumpInfo(outStream))
    {
      // ?muze vubec nastat?
    }
    fclose(outStream);
    delete image;

    // soubor predam Salamanderovi - ten si jej presune do cache a az ho prestane
    // pouzivat, smaze ho
    viewerData.Size = sizeof(viewerData);
    viewerData.FileName = tempFileName;
    viewerData.Mode = 0;  // text mode
    sprintf(caption, "%s - %s", name, LoadStr(IDS_UNISO));
    viewerData.Caption = caption;
    viewerData.WholeCaption = TRUE;
    if (!SalamanderGeneral->ViewFileInPluginViewer(NULL, &viewerData, TRUE, "iso_dump.txt", err))
    {
      // soubor je smazan i v pripade neuspechu
    }
  }
  else
  {
    SetCursor(hOldCur);
    SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(), LoadStr(IDS_ERR_TMP),
                                     LoadStr(IDS_UNISO), MB_OK | MB_ICONEXCLAMATION);
    delete image;
  }

  SetCursor(hOldCur);
  return TRUE;
}

BOOL
CPluginInterfaceForViewer::CanViewFile(const char *name)
{
  BOOL canView = FALSE;

  CISOImage *image;
  if ((image = new CISOImage()) != NULL)
  {
//    if (image->ReadSessions(name))
//      canView = TRUE;
//    else if (image->Open(name, TRUE))
    if (image->Open(name, TRUE))
      canView = TRUE;

    delete image;
  }

  return canView;
}
*/

//
// ****************************************************************************
// Menu Handlers
//

static BOOL TestArchive(CSalamanderForOperationsAbstract* salamander, HWND hParent)
{
    // vytahneme cestu na FOCUSED 7z archiv
    const CFileData* cfd = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, NULL);
    if (cfd == NULL)
        return FALSE;
    char fileName[2 * MAX_PATH];

    C7zClient client;

    salamander->OpenProgressDialog(LoadStr(IDS_TESTING_ARCHIVE), FALSE, NULL, FALSE);
    sprintf(fileName, LoadStr(IDS_TESTING_ARCHIVE_NAME), cfd->Name);
    salamander->ProgressDialogAddText(fileName, FALSE);

    SalamanderGeneral->GetPanelPath(PANEL_SOURCE, fileName, 2 * MAX_PATH, NULL, NULL);
    SalamanderGeneral->SalPathAppend(fileName, cfd->Name, 2 * MAX_PATH);
    int ret = client.TestArchive(salamander, fileName);
    salamander->CloseProgressDialog();

    if (ret == OPER_OK)
    {
        char text[1024];
        text[0] = '\0';
        sprintf(text, LoadStr(IDS_TESTARCHIVEOK), fileName);
        SalamanderGeneral->SalMessageBox(hParent, text, LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
        ret = TRUE;
    }
    else if (ret == OPER_CONTINUE)
    {
        char text[1024];
        text[0] = '\0';
        sprintf(text, LoadStr(IDS_TESTARCHIVECORRUPTED), fileName);
        SalamanderGeneral->SalMessageBox(hParent, text, LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
        ret = FALSE;
    }

    return ret;
}

//
// ****************************************************************************
// CPluginInterfaceForMenuExt
//
BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* SalOp,
                                                 HWND parent, int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem( , , %ld, %X)", id, eventMask);

    switch (id)
    {
    case IDM_TESTARCHIVE:
    {
        return TestArchive(SalOp, parent);
    }

    default:
    {
        TRACE_E("Wrong menu ID");
    }
    }
    return FALSE;
}

BOOL CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case IDM_TESTARCHIVE:
        helpID = IDH_TESTARCHIVE;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

// ****************************************************************************
//
// CPluginDataInterface
//

// callback volany ze Salamandera pro ziskani textu
// popis viz. spl_com.h / FColumnGetText
void WINAPI GetPackedSizeText()
{
    if (*TransferIsDir)
    {
        *TransferLen = 0;
    }
    else
    {
        C7zClient::CItemData* itemData = (C7zClient::CItemData*)(*TransferFileData)->PluginData;
        if (itemData->PackedSize != 0)
        {
            SalamanderGeneral->NumberToStr(TransferBuffer, CQuadWord().SetUI64(itemData->PackedSize));
            *TransferLen = (int)_tcslen(TransferBuffer);
        }
        else
        {
            *TransferLen = 0;
        }
    }
}

void WINAPI GetMethodText()
{
    if (*TransferIsDir)
    {
        *TransferLen = 0;
    }
    else
    {
        C7zClient::CItemData* itemData = (C7zClient::CItemData*)(*TransferFileData)->PluginData;
        *TransferLen = sprintf(TransferBuffer, "%s", itemData->Method);
    }
}

CPluginDataInterface::CPluginDataInterface(C7zClient* client)
{
    Client = client;
}

CPluginDataInterface::~CPluginDataInterface()
{
    Password = L"empty";
    delete Client;
    Client = NULL;
}

void CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    delete (C7zClient::CItemData*)file.PluginData;
}

void WINAPI
CPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                const CFileData* upperDir)
{
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    // sloupce upravujeme jen v detailed rezimu
    if (view->GetViewMode() == VIEW_MODE_DETAILED)
    {
        // zkusime najit std. Size a zaradit se za nej; pokud ho nenajdeme,
        // zaradime se na konec
        int sizeIndex = view->GetColumnsCount();
        int i;
        for (i = 0; i < sizeIndex; i++)
            if (view->GetColumn(i)->ID == COLUMN_ID_SIZE)
            {
                sizeIndex = i + 1;
                break;
            }

        CColumn column;
        if (Config.ListInfoPackedSize)
        {
            // sloupec pro zobrazeni komprimovane velikosti
            lstrcpy(column.Name, LoadStr(IDS_LISTINFO_PAKEDSIZE));
            lstrcpy(column.Description, LoadStr(IDS_LISTINFO_PAKEDSIZE_DESC));
            column.GetText = GetPackedSizeText;
            column.SupportSorting = 0;
            column.LeftAlignment = 0;
            column.ID = COLUMN_ID_CUSTOM;
            column.CustomData = 0;
            column.Width = leftPanel ? LOWORD(Config.ColumnPackedSizeWidth) : HIWORD(Config.ColumnPackedSizeWidth);
            column.FixedWidth = leftPanel ? LOWORD(Config.ColumnPackedSizeFixedWidth) : HIWORD(Config.ColumnPackedSizeFixedWidth);
            view->InsertColumn(sizeIndex, &column);
        }

        if (Config.ListInfoMethod)
        {
            // sloupec pro zobrazeni metody
            sizeIndex = view->GetColumnsCount();
            lstrcpy(column.Name, LoadStr(IDS_LISTINFO_METHOD));
            lstrcpy(column.Description, LoadStr(IDS_LISTINFO_METHOD_DESC));
            column.GetText = GetMethodText;
            column.SupportSorting = 0;
            column.LeftAlignment = 1;
            column.ID = COLUMN_ID_CUSTOM;
            column.CustomData = 1;
            column.Width = leftPanel ? LOWORD(Config.ColumnMethodWidth) : HIWORD(Config.ColumnMethodWidth);
            column.FixedWidth = leftPanel ? LOWORD(Config.ColumnMethodFixedWidth) : HIWORD(Config.ColumnMethodFixedWidth);
            view->InsertColumn(sizeIndex, &column);
        }
    }
}

void CPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (column->CustomData == 1)
    {
        if (leftPanel)
            Config.ColumnMethodFixedWidth = MAKELONG(newFixedWidth, HIWORD(Config.ColumnMethodFixedWidth));
        else
            Config.ColumnMethodFixedWidth = MAKELONG(LOWORD(Config.ColumnMethodFixedWidth), newFixedWidth);
    }
    else
    {
        if (leftPanel)
            Config.ColumnPackedSizeFixedWidth = MAKELONG(newFixedWidth, HIWORD(Config.ColumnPackedSizeFixedWidth));
        else
            Config.ColumnPackedSizeFixedWidth = MAKELONG(LOWORD(Config.ColumnPackedSizeFixedWidth), newFixedWidth);
    }
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void CPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (column->CustomData == 1)
    {
        if (leftPanel)
            Config.ColumnMethodWidth = MAKELONG(newWidth, HIWORD(Config.ColumnMethodWidth));
        else
            Config.ColumnMethodWidth = MAKELONG(LOWORD(Config.ColumnMethodWidth), newWidth);
    }
    else
    {
        if (leftPanel)
            Config.ColumnPackedSizeWidth = MAKELONG(newWidth, HIWORD(Config.ColumnPackedSizeWidth));
        else
            Config.ColumnPackedSizeWidth = MAKELONG(LOWORD(Config.ColumnPackedSizeWidth), newWidth);
    }
}
