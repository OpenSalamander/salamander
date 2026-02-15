// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// enable dumping blocks left allocated on the heap when the plug-in ends
// #define DUMP_MEM

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to SLG - language-dependent resources
BOOL WindowsVistaAndLater;

// plug-in interface object whose methods Salamander calls
CPluginInterface PluginInterface;
CPluginInterfaceForMenuExt InterfaceForMenuExt;
CPluginInterfaceForFS InterfaceForFS;

// general Salamander interface - valid from plug-in startup until shutdown
CSalamanderGeneralAbstract* SG = NULL;

// define the variable for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// define the variable for "spl_com.h"
int SalamanderVersion = 0;

CSalamanderGUIAbstract* SalGUI = NULL;

const char* CONFIG_RECENTPATH = "Recent Path";
const char* CONFIG_COPYORMOVEHISTORY = "Copy Or Move History %d";
const char* CONFIG_PATTERNHISTORY = "Pattern History %d";
const char* CONFIG_LOOKINHISTORY = "Look In History %d";
const char* CONFIG_WIDTH = "Window Width";
const char* CONFIG_HEIGHT = "Window Height";
const char* CONFIG_MAXIMIZED = "Maximized";
const char* CONFIG_COMMAND = "Command";
const char* CONFIG_ARGUMENTS = "Arguments";
const char* CONFIG_INITDIR = "InitDir";
const char* CONFIG_EXPORTDIR = "Last Export Directory";
const char* CONFIG_TYPEDATECOLFW = "TypeDateColFW";
const char* CONFIG_TYPEDATECOLW = "TypeDateColW";
const char* CONFIG_DATATIMECOLFW = "DataTimeColFW";
const char* CONFIG_DATATIMECOLW = "DataTimeColW";
const char* CONFIG_SIZECOLFW = "SizeColFW";
const char* CONFIG_SIZECOLW = "SizeColW";

// FS name assigned by Salamander after loading the plug-in
char AssignedFSName[MAX_PATH] = "";

CThreadQueue ThreadQueue("RegEdt Find Dialogs, Workers, and Changes Monitor");
CWindowQueueEx WindowQueue;

BOOL AlwaysOnTop = FALSE;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    CALL_STACK_MESSAGE_NONE
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
    return SG->LoadStr(HLanguage, resID);
}

LPWSTR LoadStrW(int resID)
{
    return SG->LoadStrW(HLanguage, resID);
}

BOOL ErrorHelper(HWND parent, const char* message, int lastError, va_list arglist)
{
    CALL_STACK_MESSAGE3("ErrorHelper(, %s, %d, )", message, lastError);
    char buf[2048]; //temp variable
    *buf = 0;
    vsprintf(buf, message, arglist);
    if (lastError != ERROR_SUCCESS)
    {
        // insert a space between our message and the system error description
        if (*buf != 0 && *(buf + strlen(buf) - 1) != ' ')
            strcat(buf, " ");
        int l = lstrlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 2048 - l, NULL);
    }
    if (SG)
    {
        if ((DWORD_PTR)parent == -1)
            parent = SG->GetMsgBoxParent();
        SG->SalMessageBox(parent, buf, LoadStr(IDS_REGEDTERR), MB_ICONERROR);
    }
    else
    {
        if ((DWORD_PTR)parent == -1)
            parent = 0;
        MessageBox(parent, buf, "RegEdit - Error", MB_OK | MB_ICONERROR);
    }
    return FALSE;
}

BOOL Error(HWND parent, int error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastError = GetLastError();
    CALL_STACK_MESSAGE2("Error(, %d, )", error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(parent, LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL Error(HWND parent, const char* error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastError = GetLastError();
    CALL_STACK_MESSAGE2("Error(, %s, )", error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(parent, error, lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL Error(int error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastError = GetLastError();
    CALL_STACK_MESSAGE2("Error(%d, )", error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(DialogStackPeek(), LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL ErrorL(int lastError, HWND parent, int error, ...)
{
    CALL_STACK_MESSAGE3("ErrorL(%d, , %d, )", lastError, error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(parent, LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

BOOL ErrorL(int lastError, int error, ...)
{
    CALL_STACK_MESSAGE3("ErrorL(%d, %d, )", lastError, error);
    va_list arglist;
    va_start(arglist, error);
    BOOL ret = ErrorHelper(DialogStackPeek(), LoadStr(error), lastError, arglist);
    va_end(arglist);
    return ret;
}

int SalPrintf(char* buffer, unsigned count, const char* format, ...)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE3("SalPrintf(, 0x%X, %s, )", count, format);
    va_list arglist;
    va_start(arglist, format);
    int ret = _vsnprintf_s(buffer, count, _TRUNCATE, format, arglist);
    va_end(arglist);
    return ret;
}

int SalPrintfW(LPWSTR buffer, unsigned count, LPCWSTR format, ...)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("SalPrintfW(, 0x%X, , )", count);
    va_list arglist;
    va_start(arglist, format);
    int ret = _vsnwprintf_s(buffer, count, _TRUNCATE, format, arglist);
    va_end(arglist);
    return ret;
}

#ifdef DUMP_MEM
_CrtMemState ___CrtMemState;
#endif //DUMP_MEM

int WINAPI SalamanderPluginGetReqVer()
{
    CALL_STACK_MESSAGE_NONE
    return LAST_VERSION_OF_SALAMANDER;
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    CALL_STACK_MESSAGE_NONE
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

#ifdef DUMP_MEM
    _CrtMemCheckpoint(&___CrtMemState);
#endif //DUMP_MEM

    // this plug-in targets the current Salamander version and newer - verify that
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // cannot call Error here because it uses SG->SalMessageBox (SG not initialized + incompatible interface)
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Registry Editor" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Registry Editor" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // obtain Salamander's general interface
    SG = salamander->GetSalamanderGeneral();
    SalGUI = salamander->GetSalamanderGUI();

    // set the help file name
    SG->SetHelpFileName("regedt.chm");

    // detect which OS we run on
    WindowsVistaAndLater = SalIsWindowsVersionOrGreater(6, 0, 0);

    if (!InitDialogs())
        return NULL;

    if (!InitFS())
    {
        ReleaseDialogs();
        return NULL;
    }

    // set the basic plug-in information
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_FILESYSTEM | FUNCTION_LOADSAVECONFIGURATION | FUNCTION_CONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_DESCRIPTION),
                                   "RegEdit",
                                   NULL,
                                   "reg");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // obtain our FS name (it may differ from "reg"; Salamander can adjust it)
    SG->GetPluginFSName(AssignedFSName, 0);

    return &PluginInterface;
}

// ****************************************************************************
//
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_DESCRIPTION));
    SG->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);

    BOOL ret = WindowQueue.Empty();
    if (!ret)
    {
        ret = WindowQueue.CloseAllWindows(force) || force;
    }
    if (ret)
    {
        ChangeMonitor.Stop();

        if (!ThreadQueue.KillAll(force) && !force)
            ret = FALSE; // Petr: it's OK that the change monitor stopped; our FS should be gone
        else             // the FS should no longer exist (plug-in unload = removed from panel)
        {
            ReleaseDialogs();
            ReleaseFS();

#ifdef DUMP_MEM
            _CrtMemDumpAllObjectsSince(&___CrtMemState);
#endif //DUMP_MEM
        }
    }
    return ret;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, , )");
    // default values
    DialogWidth = DialogHeight = -1;
    wcscpy(RecentFullPath, L"\\");
    strcpy(Command, "");
    strcpy(Arguments, "\"$(Name)\"");
    strcpy(InitDir, "$(FullPath)");
    strcpy(LastExportPath, "");
    if (regKey)
    {
        registry->GetValue(regKey, CONFIG_RECENTPATH, REG_BINARY, RecentFullPath, MAX_FULL_KEYNAME * 2);

        WCHAR buffer[max(MAX_FULL_KEYNAME, 1024)];
        // history of opened files
        LoadHistory(regKey, CONFIG_COPYORMOVEHISTORY, CopyOrMoveHistory, buffer, MAX_FULL_KEYNAME, registry);
        // history of searched patterns
        LoadHistory(regKey, CONFIG_PATTERNHISTORY, PatternHistory, buffer, MAX_KEYNAME, registry);
        // history of searched paths
        LoadHistory(regKey, CONFIG_LOOKINHISTORY, LookInHistory, buffer, 1024, registry);

        // window position
        if (!registry->GetValue(regKey, CONFIG_WIDTH, REG_DWORD, &DialogWidth, sizeof(int)) ||
            !registry->GetValue(regKey, CONFIG_HEIGHT, REG_DWORD, &DialogHeight, sizeof(int)))
        {
            DialogWidth = DialogHeight = -1;
        }
        registry->GetValue(regKey, CONFIG_MAXIMIZED, REG_DWORD, &Maximized, sizeof(BOOL));
        registry->GetValue(regKey, CONFIG_COMMAND, REG_SZ, Command, MAX_PATH);
        registry->GetValue(regKey, CONFIG_ARGUMENTS, REG_SZ, Arguments, MAX_PATH);
        registry->GetValue(regKey, CONFIG_INITDIR, REG_SZ, InitDir, MAX_PATH);
        registry->GetValue(regKey, CONFIG_EXPORTDIR, REG_SZ, LastExportPath, MAX_PATH);

        registry->GetValue(regKey, CONFIG_TYPEDATECOLFW, REG_DWORD, &TypeDateColFW, sizeof(int));
        registry->GetValue(regKey, CONFIG_TYPEDATECOLW, REG_DWORD, &TypeDateColW, sizeof(int));
        registry->GetValue(regKey, CONFIG_DATATIMECOLFW, REG_DWORD, &DataTimeColFW, sizeof(int));
        registry->GetValue(regKey, CONFIG_DATATIMECOLW, REG_DWORD, &DataTimeColW, sizeof(int));
        registry->GetValue(regKey, CONFIG_SIZECOLFW, REG_DWORD, &SizeColFW, sizeof(int));
        registry->GetValue(regKey, CONFIG_SIZECOLW, REG_DWORD, &SizeColW, sizeof(int));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, , )");
    registry->SetValue(regKey, CONFIG_RECENTPATH, REG_BINARY, RecentFullPath, (int)wcslen(RecentFullPath) * 2 + 2);

    BOOL b;
    if (SG->GetConfigParameter(SALCFG_SAVEHISTORY, &b, sizeof(BOOL), NULL) && b)
    {
        SaveHistory(regKey, CONFIG_COPYORMOVEHISTORY, CopyOrMoveHistory, registry);
        SaveHistory(regKey, CONFIG_PATTERNHISTORY, PatternHistory, registry);
        SaveHistory(regKey, CONFIG_LOOKINHISTORY, LookInHistory, registry);
    }
    else
    {
        // trim the histories
        char buf[32];
        int i;
        for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
        {
            SalPrintf(buf, 32, CONFIG_COPYORMOVEHISTORY, i);
            registry->DeleteValue(regKey, buf);
            SalPrintf(buf, 32, CONFIG_PATTERNHISTORY, i);
            registry->DeleteValue(regKey, buf);
            SalPrintf(buf, 32, CONFIG_LOOKINHISTORY, i);
            registry->DeleteValue(regKey, buf);
        }
    }
    // window position
    HWND lastWnd = WindowQueue.GetLastWnd();
    if (lastWnd != NULL)
    {
        WINDOWPLACEMENT wndpl;
        wndpl.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(lastWnd, &wndpl))
        {
            DialogWidth = wndpl.rcNormalPosition.right - wndpl.rcNormalPosition.left;
            DialogHeight = wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top;
            Maximized = wndpl.showCmd == SW_SHOWMAXIMIZED;
        }
    }

    registry->SetValue(regKey, CONFIG_WIDTH, REG_DWORD, &DialogWidth, sizeof(int));
    registry->SetValue(regKey, CONFIG_HEIGHT, REG_DWORD, &DialogHeight, sizeof(int));
    registry->SetValue(regKey, CONFIG_MAXIMIZED, REG_DWORD, &Maximized, sizeof(BOOL));
    registry->SetValue(regKey, CONFIG_COMMAND, REG_SZ, Command, -1);
    registry->SetValue(regKey, CONFIG_ARGUMENTS, REG_SZ, Arguments, -1);
    registry->SetValue(regKey, CONFIG_INITDIR, REG_SZ, InitDir, -1);
    registry->SetValue(regKey, CONFIG_EXPORTDIR, REG_SZ, LastExportPath, -1);

    registry->SetValue(regKey, CONFIG_TYPEDATECOLFW, REG_DWORD, &TypeDateColFW, sizeof(int));
    registry->SetValue(regKey, CONFIG_TYPEDATECOLW, REG_DWORD, &TypeDateColW, sizeof(int));
    registry->SetValue(regKey, CONFIG_DATATIMECOLFW, REG_DWORD, &DataTimeColFW, sizeof(int));
    registry->SetValue(regKey, CONFIG_DATATIMECOLW, REG_DWORD, &DataTimeColW, sizeof(int));
    registry->SetValue(regKey, CONFIG_SIZECOLFW, REG_DWORD, &SizeColFW, sizeof(int));
    registry->SetValue(regKey, CONFIG_SIZECOLW, REG_DWORD, &SizeColW, sizeof(int));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    CConfigDialog(parent).Execute();
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* used by the export_mnu.py script that generates salmenu.mnu for Translator
   keep synchronized with the salamander->AddMenuItem() calls below...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_FIND
	{MNTT_IT, IDS_MENUNEWKEY
	{MNTT_IT, IDS_MENUNEWVAL
	{MNTT_IT, IDS_EXPORT
	{MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_FIND), 0, MID_FIND, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL);
    /*
  char name[256];
  // append Salamander's shortcut to the command name
  SG->GetSalamanderCommand(SALCMD_CREATEDIRECTORY, name, 256, NULL, NULL);
  const char * str = LoadStr(IDS_MENUNEWKEY);
  int len = strlen(str);
  char * tab = strrchr(name, '\t');
  if (tab)
  {
    memmove(name + len, tab, strlen(tab)+1);
    memmove(name, str, len);
  }
  else strcpy(name, str);
  salamander->AddMenuItem(-1, name, 0, MID_NEWKEY, FALSE,
                          MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_SUBDIR,
                          MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_SUBDIR,
                          MENU_SKILLLEVEL_ALL);
  */
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUNEWKEY), 0, MID_NEWKEY, FALSE,
                            MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_SUBDIR,
                            MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_SUBDIR,
                            MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENUNEWVAL), 0, MID_NEWVAL, FALSE,
                            MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_SUBDIR,
                            MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_SUBDIR,
                            MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_EXPORT), 0, MID_EXPORT, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_REGEDT),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetChangeDriveMenuItem(LoadStr(IDS_DRIVEMENUTEXT), 0);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
}

void CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    CALL_STACK_MESSAGE1("CPluginInterface::ReleasePluginDataInterface()");
    delete ((CPluginDataInterface*)pluginData);
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    CALL_STACK_MESSAGE1("CPluginInterface::GetInterfaceForMenuExt()");
    return &InterfaceForMenuExt;
}

CPluginInterfaceForFSAbstract*
CPluginInterface::GetInterfaceForFS()
{
    CALL_STACK_MESSAGE1("CPluginInterface::GetInterfaceForFS()");
    return &InterfaceForFS;
}

void CPluginInterface::Event(int event, DWORD param)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Event(, 0x%X)", param);
    if (event == PLUGINEVENT_COLORSCHANGED)
    {
        // ImageList must not be NULL, otherwise the entry point would fail
        COLORREF bkColor = SG->GetCurrentColor(SALCOL_ITEM_BK_NORMAL);
        if (ImageList_GetBkColor(ImageList) != bkColor)
            ImageList_SetBkColor(ImageList, bkColor);
    }
}

void CPluginInterface::ClearHistory(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::ClearHistory()");
    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (CopyOrMoveHistory[i])
        {
            free(CopyOrMoveHistory[i]);
            CopyOrMoveHistory[i] = NULL;
        }
        if (PatternHistory[i])
        {
            free(PatternHistory[i]);
            PatternHistory[i] = NULL;
        }
        if (LookInHistory[i])
        {
            free(LookInHistory[i]);
            LookInHistory[i] = NULL;
        }
    }
}
