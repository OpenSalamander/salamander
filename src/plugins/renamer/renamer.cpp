// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// zapina vypis bloku ktere zustaly alokovane na heapu po skonceni pluginu
// #define DUMP_MEM

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SG = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

CSalamanderGUIAbstract* SalGUI = NULL;

const char* CONFIG_WIDTH = "Window Width";
const char* CONFIG_HEIGHT = "Window Height";
const char* CONFIG_MAXIMIZED = "Maximized";
const char* CONFIG_CUSTOMFONT = "Custom Font";
const char* CONFIG_MANUALFONT = "Manual Mode Font";
const char* CONFIG_MASKHISTORY = "Mask History %d";
const char* CONFIG_NEWNAMEHISTORY = "New Name History %d";
const char* CONFIG_SEARCHHISTORY = "Search History %d";
const char* CONFIG_REPLACEHISTORY = "Replace History %d";
const char* CONFIG_COMMANDHISTORY = "Command History %d";
const char* CONFIG_COMMAND = "Command";
const char* CONFIG_ARGUMENTS = "Arguments";
const char* CONFIG_INITDIR = "InitDir";
const char* CONFIG_LASTUSED = "Last Used Options";
const char* CONFIG_LASTMASK = "Last Last Mask";
const char* CONFIG_LASTSUBDIRS = "Last Subdirs";
const char* CONFIG_LASTREMOVESOURCE = "Last Remove Source Path";
const char* CONFIG_CONFIRMESCCLOSE = "Confirm ESC Close";

CThreadQueue ThreadQueue("Renamer Dialogs");
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

BOOL ErrorHelper(HWND parent, const char* message, int lastError, va_list arglist)
{
    CALL_STACK_MESSAGE3("ErrorHelper(, %s, %d, )", message, lastError);
    char buf[1024]; //temp variable
    *buf = 0;
    vsprintf(buf, message, arglist);
    if (lastError != ERROR_SUCCESS)
    {
        int l = lstrlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    if (SG)
    {
        if (parent == (HWND)-1)
            parent = SG->GetMsgBoxParent();
        SG->SalMessageBox(parent, buf, LoadStr(IDS_RENAMERERR), MB_ICONERROR);
    }
    else
    {
        if (parent == (HWND)-1)
            parent = 0;
        MessageBox(parent, buf, "Renamer - Error", MB_OK | MB_ICONERROR);
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

// BOOL
// ErrorL(int lastError, HWND parent, int error, ...)
// {
//   CALL_STACK_MESSAGE3("ErrorL(%d, , %d, )", lastError, error);
//   va_list arglist;
//   va_start(arglist, error);
//   BOOL ret = ErrorHelper(parent, LoadStr(error), lastError, arglist);
//   va_end(arglist);
//   return ret;
// }

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
    CALL_STACK_MESSAGE3("SalPrintf(, 0x%X, %s, )", count, format);
    va_list arglist;
    va_start(arglist, format);
    int ret = _vsnprintf_s(buffer, count, _TRUNCATE, format, arglist);
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
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

#ifdef DUMP_MEM
    _CrtMemCheckpoint(&___CrtMemState);
#endif //DUMP_MEM

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // tady nelze volat Error, protoze pouziva SG->SalMessageBox (SG neni inicializovane + jde o nekompatibilni rozhrani)
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Renamer" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Renamer" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SG = salamander->GetSalamanderGeneral();
    SalGUI = salamander->GetSalamanderGUI();

    // nastavime jmeno souboru s helpem
    SG->SetHelpFileName("renamer.chm");

    if (!InitDialogs())
        return NULL;

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_LOADSAVECONFIGURATION | FUNCTION_CONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_DESCRIPTION),
                                   "Renamer" /* neprekladat! */);

    salamander->SetPluginHomePageURL("www.altap.cz");

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
        if (force)
            UpdateWindow(SG->GetMainWindowHWND());
        if (force)
            SG->CreateSafeWaitWindow(LoadStr(IDS_WAITINGFORWINDOWS), NULL, 0, FALSE, NULL);
        ret = WindowQueue.CloseAllWindows(force, 1000, INFINITE) || force;
        if (force)
            SG->DestroySafeWaitWindow();
    }
    if (ret)
    {
        if (!ThreadQueue.KillAll(force) && !force)
            ret = FALSE;
        else
        {
            ReleaseDialogs();

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
    // nastavime default hodnoty
    DialogWidth = DialogHeight = -1;
    LastOptions.Reset(FALSE);
    strcpy(LastMask, "*.*");
    LastSubdirs = FALSE;
    LastRemoveSourcePath = FALSE;
    UseCustomFont = FALSE;
    ConfirmESCClose = TRUE;
    strcpy(Command, "notepad");
    strcpy(Arguments, "\"$(Name)\"");
    strcpy(InitDir, "$(FullPath)");
    if (regKey)
    {
        char buffer[4096];

        // historie
        LoadHistory(regKey, CONFIG_MASKHISTORY, MaskHistory, buffer, MAX_GROUPMASK, registry);
        LoadHistory(regKey, CONFIG_NEWNAMEHISTORY, NewNameHistory, buffer, 2 * MAX_PATH, registry);
        LoadHistory(regKey, CONFIG_SEARCHHISTORY, SearchHistory, buffer, MAX_PATH, registry);
        LoadHistory(regKey, CONFIG_REPLACEHISTORY, ReplaceHistory, buffer, MAX_PATH, registry);
        LoadHistory(regKey, CONFIG_COMMANDHISTORY, CommandHistory, buffer, MAX_PATH, registry);

        // load z registry
        registry->GetValue(regKey, CONFIG_CUSTOMFONT, REG_DWORD, &UseCustomFont, sizeof(int));
        UseCustomFont = UseCustomFont &&
                        registry->GetValue(regKey, CONFIG_MANUALFONT, REG_BINARY, &ManualModeLogFont, sizeof(LOGFONT));

        // pozice okna
        if (!registry->GetValue(regKey, CONFIG_WIDTH, REG_DWORD, &DialogWidth, sizeof(int)) ||
            !registry->GetValue(regKey, CONFIG_HEIGHT, REG_DWORD, &DialogHeight, sizeof(int)))
        {
            DialogWidth = DialogHeight = -1;
        }
        registry->GetValue(regKey, CONFIG_MAXIMIZED, REG_DWORD, &Maximized, sizeof(BOOL));

        // posledni nastaveni voleb
        HKEY subKey;
        if (registry->OpenKey(regKey, CONFIG_LASTUSED, subKey))
        {
            LastOptions.Load(subKey, registry);
            registry->GetValue(subKey, CONFIG_LASTMASK, REG_SZ, LastMask, MAX_GROUPMASK);
            registry->GetValue(subKey, CONFIG_LASTSUBDIRS, REG_DWORD, &LastSubdirs, sizeof(BOOL));
            registry->GetValue(subKey, CONFIG_LASTREMOVESOURCE, REG_DWORD,
                               &LastRemoveSourcePath, sizeof(BOOL));
            registry->CloseKey(subKey);
        }

        registry->GetValue(regKey, CONFIG_COMMAND, REG_SZ, Command, MAX_PATH);
        registry->GetValue(regKey, CONFIG_ARGUMENTS, REG_SZ, Arguments, MAX_PATH);
        registry->GetValue(regKey, CONFIG_INITDIR, REG_SZ, InitDir, MAX_PATH);

        registry->GetValue(regKey, CONFIG_CONFIRMESCCLOSE, REG_DWORD, &ConfirmESCClose, sizeof(BOOL));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, , )");

    BOOL b;

    if (SG->GetConfigParameter(SALCFG_SAVEHISTORY, &b, sizeof(BOOL), NULL) && b)
    {
        SaveHistory(regKey, CONFIG_MASKHISTORY, MaskHistory, registry);
        SaveHistory(regKey, CONFIG_NEWNAMEHISTORY, NewNameHistory, registry);
        SaveHistory(regKey, CONFIG_SEARCHHISTORY, SearchHistory, registry);
        SaveHistory(regKey, CONFIG_REPLACEHISTORY, ReplaceHistory, registry);
        SaveHistory(regKey, CONFIG_COMMANDHISTORY, CommandHistory, registry);
    }
    else
    {
        //podrizneme historie
        char buf[32];
        int i;
        for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
        {
            SalPrintf(buf, 32, CONFIG_MASKHISTORY, i);
            registry->DeleteValue(regKey, buf);
            SalPrintf(buf, 32, CONFIG_NEWNAMEHISTORY, i);
            registry->DeleteValue(regKey, buf);
            SalPrintf(buf, 32, CONFIG_SEARCHHISTORY, i);
            registry->DeleteValue(regKey, buf);
            SalPrintf(buf, 32, CONFIG_REPLACEHISTORY, i);
            registry->DeleteValue(regKey, buf);
            SalPrintf(buf, 32, CONFIG_COMMANDHISTORY, i);
            registry->DeleteValue(regKey, buf);
        }
    }

    registry->SetValue(regKey, CONFIG_CUSTOMFONT, REG_DWORD, &UseCustomFont, sizeof(int));
    if (UseCustomFont)
        registry->SetValue(regKey, CONFIG_MANUALFONT, REG_BINARY, &ManualModeLogFont, sizeof(LOGFONT));

    // pozice okna
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
        // CRenamerDialog * dlg = (CRenamerDialog*) WindowsManager.GetWindowPtr(lastWnd);
        // if (dlg && dlg->Is(otDialog))
        // {
        //   LastOptions = dlg->RenamerOptions;
        //   LastSubdirs = dlg->Subdirs;
        //   LastRemoveSourcePath = dlg->RemoveSourcePath;
        // }
    }
    registry->SetValue(regKey, CONFIG_WIDTH, REG_DWORD, &DialogWidth, sizeof(int));
    registry->SetValue(regKey, CONFIG_HEIGHT, REG_DWORD, &DialogHeight, sizeof(int));
    registry->SetValue(regKey, CONFIG_MAXIMIZED, REG_DWORD, &Maximized, sizeof(BOOL));
    // posledni nastaveni voleb
    HKEY subKey;
    if (registry->CreateKey(regKey, CONFIG_LASTUSED, subKey))
    {
        LastOptions.Save(subKey, registry);
        registry->SetValue(subKey, CONFIG_LASTMASK, REG_SZ, LastMask, -1);
        registry->SetValue(subKey, CONFIG_LASTSUBDIRS, REG_DWORD, &LastSubdirs, sizeof(BOOL));
        registry->SetValue(subKey, CONFIG_LASTREMOVESOURCE, REG_DWORD,
                           &LastRemoveSourcePath, sizeof(BOOL));
        registry->CloseKey(subKey);
    }
    registry->SetValue(regKey, CONFIG_COMMAND, REG_SZ, Command, -1);
    registry->SetValue(regKey, CONFIG_ARGUMENTS, REG_SZ, Arguments, -1);
    registry->SetValue(regKey, CONFIG_INITDIR, REG_SZ, InitDir, -1);
    registry->SetValue(regKey, CONFIG_CONFIRMESCCLOSE, REG_DWORD, &ConfirmESCClose, sizeof(BOOL));
}

void OnConfiguration(HWND hParent)
{
    CALL_STACK_MESSAGE1("OnConfiguration");

    static BOOL InConfiguration = FALSE;
    if (InConfiguration)
    {
        SG->SalMessageBox(hParent, LoadStr(IDS_CFG_CONFLICT), LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
        return;
    }
    InConfiguration = TRUE;

    if (CConfigDialog(hParent).Execute() == IDOK)
    {
        WindowQueue.BroadcastMessage(WM_USER_CFGCHNG, 0, 0);
    }
    InConfiguration = FALSE;
}

void CPluginInterface::Configuration(HWND parent)
{
    OnConfiguration(parent);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_PLUGMENU_RENAME
  {MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_PLUGMENU_RENAME), SALHOTKEY('R', HOTKEYF_CONTROL | HOTKEYF_SHIFT),
                            MID_RENAME, FALSE, MENU_EVENT_FILE_FOCUSED | MENU_EVENT_DIR_FOCUSED | MENU_EVENT_FILES_SELECTED | MENU_EVENT_DIRS_SELECTED,
                            MENU_EVENT_DISK, MENU_SKILLLEVEL_ALL);
    // salamander->AddMenuItem(-1, LoadStr(IDS_PLUGMENU_UNDO), 0, MID_UNDO, FALSE,
    //     MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    // nastavime ikonku pluginu
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_RENAMER),
                                      IMAGE_BITMAP, 16, 16, SG->GetIconLRFlags());
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    CALL_STACK_MESSAGE1("CPluginInterface::GetInterfaceForMenuExt()");
    return &InterfaceForMenuExt;
}

void CPluginInterface::Event(int event, DWORD param)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Event(, 0x%X)", param);
    switch (event)
    {
    case PLUGINEVENT_COLORSCHANGED:
    {
        // nutne HSymbolsImageList != NULL, jinak by entry-point vratil chybu
        COLORREF bkColor = GetSysColor(COLOR_WINDOW);
        if (ImageList_GetBkColor(HSymbolsImageList) != bkColor)
            ImageList_SetBkColor(HSymbolsImageList, bkColor);
        break;
    }

    case PLUGINEVENT_CONFIGURATIONCHANGED:
    {
        // nacteme confirmations z konfigurace
        Silent = 0;
        BOOL b;
        if (SG->GetConfigParameter(SALCFG_CNFRMFILEOVER, &b, sizeof(b), NULL) && !b)
            Silent |= SILENT_OVERWRITE_FILE_EXIST;
        if (SG->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &b, sizeof(b), NULL) && !b)
            Silent |= SILENT_OVERWRITE_FILE_SYSHID;

        SG->GetConfigParameter(SALCFG_MINBEEPWHENDONE, &MinBeepWhenDone, sizeof(BOOL), NULL);
        break;
    }

    case PLUGINEVENT_SETTINGCHANGE:
    {
        WindowQueue.BroadcastMessage(WM_USER_SETTINGCHANGE, 0, 0);
        break;
    }
    }
}

void CPluginInterface::ClearHistory(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::ClearHistory()");

    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (MaskHistory[i])
        {
            free(MaskHistory[i]);
            MaskHistory[i] = NULL;
        }
        if (NewNameHistory[i])
        {
            free(NewNameHistory[i]);
            NewNameHistory[i] = NULL;
        }
        if (SearchHistory[i])
        {
            free(SearchHistory[i]);
            SearchHistory[i] = NULL;
        }
        if (ReplaceHistory[i])
        {
            free(ReplaceHistory[i]);
            ReplaceHistory[i] = NULL;
        }
        if (CommandHistory[i])
        {
            free(CommandHistory[i]);
            CommandHistory[i] = NULL;
        }
    }
}
