// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <tchar.h>
#include "splitcbn.rh"
#include "splitcbn.rh2"
#include "lang\lang.rh"
#include "splitcbn.h"
#include "split.h"
#include "combine.h"
#include "dialogs.h"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro menu extensions
CPluginInterfaceForMenuExt InterfaceForMenuExt;
// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;
// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;
// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

BOOL configIncludeFileExt;
BOOL configCreateBatchFile;
BOOL configSplitToOther;
BOOL configCombineToOther;
BOOL configSplitToSubdir;

static const char* KEY_INCLUDEFILEEXT = "Include Original Extension";
static const char* KEY_CREATEBATCHFILE = "Create Batch File";
static const char* KEY_SPLITTOOTHER = "Split To Other Panel";
static const char* KEY_COMBINETOOTHER = "Combine To Other Panel";
static const char* KEY_SPLITTOSUBDIR = "Split To Subdirectory";

// ****************************************************************************

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;
        InitCommonControls();
    }
    return TRUE; // DLL can be loaded
}

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

//****************************************************************************

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

// ****************************************************************************

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (salamander->GetVersion() < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Split & Combine" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Split & Combine" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();
    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

    // nastavime jmeno souboru s helpem
    SalamanderGeneral->SetHelpFileName("splitcbn.chm");

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "SplitCombine");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

// ****************************************************************************
//
//  CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUTTITLE), MB_OK | MB_ICONINFORMATION);
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    configIncludeFileExt = TRUE;
    configCreateBatchFile = TRUE;
    SalamanderGeneral->GetConfigParameter(SALCFG_ARCOTHERPANELFORUNPACK, &configSplitToOther, sizeof(BOOL), NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_ARCOTHERPANELFORPACK, &configCombineToOther, sizeof(BOOL), NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_ARCSUBDIRBYARCFORUNPACK, &configSplitToSubdir, sizeof(BOOL), NULL);
    if (regKey != NULL)
    {
        registry->GetValue(regKey, KEY_INCLUDEFILEEXT, REG_DWORD, &configIncludeFileExt, sizeof(DWORD));
        registry->GetValue(regKey, KEY_CREATEBATCHFILE, REG_DWORD, &configCreateBatchFile, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SPLITTOOTHER, REG_DWORD, &configSplitToOther, sizeof(DWORD));
        registry->GetValue(regKey, KEY_COMBINETOOTHER, REG_DWORD, &configCombineToOther, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SPLITTOSUBDIR, REG_DWORD, &configSplitToSubdir, sizeof(DWORD));
    }
    //if (!configSplitToOther) configSplitToSubdir = FALSE;
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
    registry->SetValue(regKey, KEY_INCLUDEFILEEXT, REG_DWORD, &configIncludeFileExt, sizeof(DWORD));
    registry->SetValue(regKey, KEY_CREATEBATCHFILE, REG_DWORD, &configCreateBatchFile, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SPLITTOOTHER, REG_DWORD, &configSplitToOther, sizeof(DWORD));
    registry->SetValue(regKey, KEY_COMBINETOOTHER, REG_DWORD, &configCombineToOther, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SPLITTOSUBDIR, REG_DWORD, &configSplitToSubdir, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration( )");
    ConfigDialog(parent);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_MENU1
	{MNTT_IT, IDS_MENU2
	{MNTT_PE, 0
};
*/

    salamander->AddMenuItem(-1, LoadStr(IDS_MENU1), 0, 1, FALSE, MENU_EVENT_TRUE,
                            MENU_EVENT_FILE_FOCUSED | MENU_EVENT_DISK, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENU2), 0, 2, FALSE, MENU_EVENT_FILES_SELECTED | MENU_EVENT_FILE_FOCUSED, MENU_EVENT_DISK, MENU_SKILLLEVEL_ALL);

    // nastavime ikonku pluginu
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_SPLIT),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

// ****************************************************************************
//
//  CPluginInterfaceForMenuExt
//

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                                 HWND parent, int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem( , , %ld, %X)", id, eventMask);

    SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // vsechny prikazy povazujeme za praci s cestou (objevi se v Alt+F12)

    switch (id)
    {
    case 1:
    {
        return SplitCommand(parent, salamander);
    }

    case 2:
    {
        return CombineCommand(eventMask, parent, salamander);
    }
    }
    return FALSE;
}

BOOL CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case 1:
        helpID = IDH_SPLIT;
        break;
    case 2:
        helpID = IDH_COMBINE;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

// ****************************************************************************
//
//  Pomocne funkce
//

void CenterWindow(HWND hWnd)
{
    CALL_STACK_MESSAGE1("CenterWindow()");
    HWND hParent = GetParent(hWnd);
    if (hParent != NULL)
        SalamanderGeneral->MultiMonCenterWindow(hWnd, hParent, TRUE);
}

void GetInfo(char* buffer, CQuadWord& size)
{
    CALL_STACK_MESSAGE2("GetInfo(, %I64u)", size.Value);
    SYSTEMTIME st;
    GetLocalTime(&st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, size), date, time);
}

void StripExtension(LPTSTR fileName)
{
    CALL_STACK_MESSAGE2("StripExtension(%s)", fileName);
    LPTSTR dot = _tcsrchr(fileName, '.');
    if (dot != NULL)
        *dot = 0; // ".cvspass" ve Windows je pripona
}

BOOL Error(int title, int error, ...)
{
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE3("Error(%d, %d, ...)", title, error);
    char buf[1024];
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        strcat(buf, " ");
        DWORD l = (DWORD)strlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    SalamanderGeneral->ShowMessageBox(buf, LoadStr(title), MSGBOX_ERROR);

    return FALSE;
}

BOOL Error2(HWND hParent, int title, int error, ...)
{
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE3("Error2( , %d, %d, ...)", title, error);
    char buf[1024];
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        strcat(buf, " ");
        DWORD l = (DWORD)strlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(title), MSGBOXEX_OK | MSGBOXEX_ICONEXCLAMATION);

    return FALSE;
}

void GetTargetDir(LPTSTR targetDir, LPTSTR subdirName, BOOL bSplit)
{
    // Tato fce vrati cilovy adresar pro split nebo combine, bere v uvahu konfiguraci
    // configSplitToOther/configCombineToOther. Pokud by cilova cesta vedla do archivu
    // nebo do FS, bez ohledu na konfiguraci bude nabidnuta cesta do zdrojoveho panelu,
    // ktera je vzdy zarucene PATH_TYPE_WINDOWS (diky menu enablerum).

    int type;
    SalamanderGeneral->GetPanelPath(
        (bSplit ? configSplitToOther : configCombineToOther) ? PANEL_TARGET : PANEL_SOURCE,
        targetDir, MAX_PATH, &type, NULL);

    if (type != PATH_TYPE_WINDOWS)
        SalamanderGeneral->GetPanelPath(PANEL_SOURCE, targetDir, MAX_PATH, NULL, NULL);

    if (bSplit && configSplitToSubdir && subdirName != NULL)
    {
        if (SalamanderGeneral->SalPathAppend(targetDir, subdirName, MAX_PATH))
            SalamanderGeneral->SalPathRemoveExtension(targetDir);
    }
}

BOOL MakePathAbsolute(char* path, BOOL pathIsDir, char* absRoot, BOOL activePreferred, int errorTitle)
{
    int type;
    char* secondPart;
    BOOL isDir;

    SalamanderGeneral->SalUpdateDefaultDir(!configCombineToOther);
    if (!SalamanderGeneral->SalParsePath(SalamanderGeneral->GetMsgBoxParent(), path, type, isDir, secondPart,
                                         LoadStr(IDS_PATHERROR), NULL, TRUE, absRoot, NULL, NULL, MAX_PATH))
        return FALSE;

    if (type != PATH_TYPE_WINDOWS) // podporujeme jen windowsove cesty
        return Error(errorTitle, IDS_WINPATH);

    if (isDir)
    {
        char* s = secondPart;
        if (!pathIsDir)
            while (*s != 0 && *s != '\\')
                s++;
        if (*s != 0) // obsahuje i podadresare, dotazeme se na vytvoreni
            if (SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                                 LoadStr(IDS_TARGETPATHEXIST), LoadStr(errorTitle), MB_YESNO | MB_ICONQUESTION) == IDNO)
                return FALSE;
    }

    return TRUE;
}
