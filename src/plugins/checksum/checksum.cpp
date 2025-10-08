// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "checksum.rh"
#include "checksum.rh2"
#include "lang\lang.rh"
#include "checksum.h"
#include "misc.h"
#include "dialogs.h"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle to the SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to the SLG - language-dependent resources

// plugin interface object, its methods are called from Salamander
CPluginInterface PluginInterface;
// the CPluginInterface part used for menu extensions
CPluginInterfaceForMenuExt InterfaceForMenuExt;
// Salamander general interface - valid from startup until the plugin shuts down
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
// SHA1 interface from Salamander
CSalamanderCryptAbstract* SalamanderCrypt = NULL;
// interface for convenient file work
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;
// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;
// variable definition for "spl_com.h"
int SalamanderVersion = 0;
// Salamander GUI interface
CSalamanderGUIAbstract* SalamanderGUI;

SConfig Config = {
    HT_MD5,                                      // HashType - the default format for SaveAs
    {662, 301, 230, 80, 80, 229, 279, 430, 860}, // CalcDlgWidths
    {433, 301, 230, 80, 80},                     // VerDlgWidths
    {
        // Register all known algorthms here
        {HT_CRC, true, IDS_COLUMN_CRC, IDS_COPYTOCBOARD_CRC, IDS_SAVE_FILTER_CRC, IDS_VERIFY_CRC, _T(".sfv"), "CRC", CRCFactory},
        {HT_MD5, true, IDS_COLUMN_MD5, IDS_COPYTOCBOARD_MD5, IDS_SAVE_FILTER_MD5, IDS_VERIFY_MD5, _T(".md5"), "MD5", MD5Factory},
        {HT_SHA1, true, IDS_COLUMN_SHA1, IDS_COPYTOCBOARD_SHA1, IDS_SAVE_FILTER_SHA1, IDS_VERIFY_SHA1, _T(".sha1"), "SHA1", SHA1Factory},
        {HT_SHA256, true, IDS_COLUMN_SHA256, IDS_COPYTOCBOARD_SHA256, IDS_SAVE_FILTER_SHA256, IDS_VERIFY_SHA256, _T(".sha256"), "SHA256", SHA256Factory},
        {HT_SHA512, true, IDS_COLUMN_SHA512, IDS_COPYTOCBOARD_SHA512, IDS_SAVE_FILTER_SHA512, IDS_VERIFY_SHA512, _T(".sha512"), "SHA512", SHA512Factory}}};

// Current config version
#define CURRENT_CONFIG_VERSION 1 // AS 2.52b1 with CRC/MD5/SHA1/SHA256 columns

static const char* CONFIG_VERSION = "Version";
static const char* CONFIG_HASHTYPE = "Hash Type";
static const char* CONFIG_CALCDLGWIDTHS = "Dialog Size 1";
static const char* CONFIG_VERDLGWIDTHS = "Dialog Size 2";

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

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // configure SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // configure SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    HANDLES_CAN_USE_TRACE();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is designed for the current Salamander version and newer - verify that
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Checksum" /* do not translate! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Checksum" /* do not translate! */);
    if (HLanguage == NULL)
        return NULL;

    // obtain the general Salamander interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();
    SalamanderGUI = salamander->GetSalamanderGUI();
    SalamanderCrypt = SalamanderGeneral->GetSalamanderCrypt();

    // set the help file name
    SalamanderGeneral->SetHelpFileName("checksum.chm");

    InitializeWinLib("Checksum" /* do not translate! */, DLLInstance);

    SetupWinLibHelp(HTMLHelpCallback);

    // set the basic information about the plugin
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "Checksum" /* do not translate! */);

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

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = ModelessQueue.Empty();
    if (!ret)
    {
        ret = ModelessQueue.CloseAllWindows(force) || force;
    }
    if (ret)
    {
        if (!ThreadQueue.KillAll(force) && !force)
            ret = FALSE;
        else
            ReleaseWinLib(DLLInstance);
    }
    return ret;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    if (regKey != NULL)
    {
        DWORD v;

        v = HT_MD5;
        if (registry->GetValue(regKey, CONFIG_HASHTYPE, REG_DWORD, &v, sizeof(DWORD)))
        {
            Config.HashType = (eHASH_TYPE)v;
            if (Config.HashType >= HT_COUNT)
                Config.HashType = HT_MD5; // Sanity check
        }

        // Ignore old config from the non-SHA1 era
        if (registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD)))
        {
            registry->GetValue(regKey, CONFIG_CALCDLGWIDTHS, REG_BINARY, Config.CalcDlgWidths, sizeof(Config.CalcDlgWidths));
        }
        registry->GetValue(regKey, CONFIG_VERDLGWIDTHS, REG_BINARY, Config.VerDlgWidths, sizeof(Config.VerDlgWidths));

        int i;
        for (i = 0; i < HT_COUNT; i++)
            registry->GetValue(regKey, Config.HashInfo[i].sRegID, REG_DWORD, &Config.HashInfo[i].bCalculate, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    v = Config.HashType;
    registry->SetValue(regKey, CONFIG_HASHTYPE, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CALCDLGWIDTHS, REG_BINARY, Config.CalcDlgWidths, sizeof(Config.CalcDlgWidths));
    registry->SetValue(regKey, CONFIG_VERDLGWIDTHS, REG_BINARY, Config.VerDlgWidths, sizeof(Config.VerDlgWidths));

    int i;
    for (i = 0; i < HT_COUNT; i++)
        registry->SetValue(regKey, Config.HashInfo[i].sRegID, REG_DWORD, &Config.HashInfo[i].bCalculate, sizeof(DWORD));
}

INT_PTR
OnConfiguration(HWND hParent)
{
    static bool bInConfiguration = false;

    if (bInConfiguration)
    {
        SalamanderGeneral->SalMessageBox(hParent, LoadStr(IDS_CONFIG_CONFLICT),
                                         LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION | MB_OK);
        return IDCANCEL;
    }

    bInConfiguration = true;
    CConfigurationDialog dlg(hParent);
    INT_PTR ret = dlg.Execute();
    bInConfiguration = false;

    return ret;
}

void CPluginInterface::Configuration(HWND hParent)
{
    OnConfiguration(hParent);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
   keep it in sync with the salamander->AddMenuItem() calls below...
MENU_TEMPLATE_ITEM PluginMenu[] =
{
        {MNTT_PB, 0
        {MNTT_IT, IDS_MENU_VERIFY
        {MNTT_IT, IDS_MENU_CALCULATE
        {MNTT_PE, 0
};
*/

    salamander->AddMenuItem(-1, LoadStr(IDS_MENU_VERIFY), SALHOTKEY('V', HOTKEYF_CONTROL | HOTKEYF_SHIFT), CMD_VERIFY, FALSE, MENU_EVENT_TRUE,
                            MENU_EVENT_FILE_FOCUSED | MENU_EVENT_DISK, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_MENU_CALCULATE), 0, CMD_CALCULATE, FALSE, MENU_EVENT_FILE_FOCUSED | MENU_EVENT_FILES_SELECTED | MENU_EVENT_DIR_FOCUSED | MENU_EVENT_DIRS_SELECTED, MENU_EVENT_DISK, MENU_SKILLLEVEL_ALL);

    // set the plugin icon
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_CHECKSUM),
                                      IMAGE_BITMAP, 16, 16, SalamanderGeneral->GetIconLRFlags());
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

char Focus_Path[MAX_PATH] = "";

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                                 HWND parent, int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem( , , %ld, %X)", id, eventMask);

    SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // treat all commands as path work (shows up in Alt+F12)

    switch (id)
    {
    case CMD_VERIFY:
    {
        OpenVerifyDialog(parent);
        return FALSE; // we manipulate the focus, so do not clear the panel selection
    }

    case CMD_CALCULATE:
    {
        OpenCalculateDialog(parent);
        return TRUE;
    }

    case CMD_FOCUSFILE:
    {
        if (Focus_Path[0] != 0) // only if we were lucky enough not to hit the start of Salamander's BUSY mode
        {
            char* fname;
            if (SalamanderGeneral->CutDirectory(Focus_Path, &fname))
            {
                SalamanderGeneral->SkipOneActivateRefresh(); // prevent the main window from refreshing when switching from the Verify dialog
                SalamanderGeneral->FocusNameInPanel(PANEL_SOURCE, Focus_Path, fname);
                Focus_Path[0] = 0;
            }
        }
        return TRUE;
    }
    }
    return FALSE;
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case 1:
        helpID = IDH_VERIFYCHKSUM;
        break;
    case 2:
        helpID = IDH_CALCCHKSUM;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}
