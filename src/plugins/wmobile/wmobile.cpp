// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// plugin interface object; Salamander calls its methods
CPluginInterface PluginInterface;
// additional parts of the CPluginInterface interface
CPluginInterfaceForFS InterfaceForFS;

// ConfigVersion: 0 - no configuration was read from the Registry (plugin installation or a version without configuration - up to and including 2.5 beta 7),
//                1 - first configuration version (since 2.5 beta 8; introduced to automatically disable the Alt+F1/F2 menu item when rapi.dll is not installed)

int ConfigVersion = 0;           // version of the configuration loaded from the registry (see description above)
#define CURRENT_CONFIG_VERSION 1 // current configuration version (stored in the registry when the plugin unloads)
const char* CONFIG_VERSION = "Version";

// global data

// pointers to lower/upper case mapping tables
unsigned char* LowerCase = NULL;
unsigned char* UpperCase = NULL;

HINSTANCE DLLInstance = NULL; // handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to SLG - language-dependent resources

// Salamander general interface - valid from startup until the plugin terminates
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// variable definition for "spl_com.h"
int SalamanderVersion = 0;

// interface providing customized Windows controls used in Salamander
CSalamanderGUIAbstract* SalamanderGUI = NULL;

char TitleWMobile[100] = "Windows Mobile Plugin";                  // replaced with IDS_WMPLUGINTITLE in the entry point
char TitleWMobileError[100] = "Windows Mobile Plugin Error";       // replaced with IDS_WMPLUGINTITLE_ERROR in the entry point
char TitleWMobileQuestion[100] = "Windows Mobile Plugin Question"; // replaced with IDS_WMPLUGINTITLE_QUESTION in the entry point

// ****************************************************************************

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;

        INITCOMMONCONTROLSEX initCtrls;
        initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
        initCtrls.dwICC = ICC_BAR_CLASSES;
        if (!InitCommonControlsEx(&initCtrls))
        {
            MessageBox(NULL, "InitCommonControlsEx failed!", "Error", MB_OK | MB_ICONERROR);
            return FALSE; // DLL won't start
        }
    }

    return TRUE; // DLL can be loaded
}

void OnAbout(HWND hParent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

#ifdef __BORLANDC__
extern "C"
{
    int WINAPI SalamanderPluginGetReqVer();
    CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander);
};
#endif // __BORLANDC__

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is built for the current Salamander version and newer - perform a check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Windows Mobile Plugin" /* do not translate! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Windows Mobile Plugin" /* do not translate! */);
    if (HLanguage == NULL)
        return NULL;

    // obtain Salamander's general interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderGeneral->GetLowerAndUpperCase(&LowerCase, &UpperCase);

    strncpy_s(TitleWMobile, LoadStr(IDS_WMPLUGINTITLE), _TRUNCATE);
    strncpy_s(TitleWMobileError, LoadStr(IDS_WMPLUGINTITLE_ERROR), _TRUNCATE);
    strncpy_s(TitleWMobileQuestion, LoadStr(IDS_WMPLUGINTITLE_QUESTION), _TRUNCATE);

    // obtain the interface that provides customized Windows controls used in Salamander
    SalamanderGUI = salamander->GetSalamanderGUI();

    // set the help file name
    SalamanderGeneral->SetHelpFileName("wmobile.chm");

    if (!InitializeWinLib("WMOBILE" /* do not translate! */, DLLInstance))
        return FALSE;
    SetupWinLibHelp(HTMLHelpCallback);

    if (!InitFS())
        return NULL; // error

    // configure the basic plugin information
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_FILESYSTEM | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "WMOBILE" /* do not translate! */, NULL, "CE");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // obtain our FS name (it may not be "cefs"; Salamander can adjust it)
    SalamanderGeneral->GetPluginFSName(AssignedFSName, 0);

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void WINAPI
CPluginInterface::About(HWND parent)
{
    OnAbout(parent);
}

BOOL WINAPI
CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = TRUE;

    if (ret)
    {
        ReleaseFS();

        ReleaseWinLib(DLLInstance);

        // remove all copies of FS files from the disk cache (theoretically redundant, every FS should delete its own copies)
        char uniqueFileName[MAX_PATH];
        strcpy(uniqueFileName, AssignedFSName);
        strcat(uniqueFileName, ":");
        // disk names are case-insensitive while the disk cache is case-sensitive; converting
        // to lowercase makes the disk cache behave case-insensitively as well
        SalamanderGeneral->ToLowerCase(uniqueFileName);
        SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
    }
    if (ret && InterfaceForFS.GetActiveFSCount() != 0)
    {
        TRACE_E("Some FS interfaces were not closed (count=" << InterfaceForFS.GetActiveFSCount() << ")");
    }
    return ret;
}

void WINAPI
CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    if (regKey != NULL) // load from the registry
    {
        registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD));
    }
}

void WINAPI
CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
}

void WINAPI
CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
}

void WINAPI
CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // switch to icons with alpha-channel support
    CGUIIconListAbstract* iconList = SalamanderGUI->CreateIconList();
    iconList->Create(16, 16, 1);
    HICON hIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS), IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
    iconList->ReplaceIcon(0, hIcon);
    DestroyIcon(hIcon);
    salamander->SetIconListForGUI(iconList); // Salamander takes care of destroying the icon list
    salamander->SetChangeDriveMenuItem("\tMobile Device", 0);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    if (ConfigVersion < 1) // do this only during plugin installation or upgrade from 2.5 beta 7 or older (to keep the user's settings)
    {
        // if rapi is not installed, hide the icon so it does not get in the way
        HINSTANCE hLib = LoadLibrary("rapi.dll");
        if (hLib != NULL)
            FreeLibrary(hLib);
        else
            SalamanderGeneral->SetChangeDriveMenuItemVisibility(FALSE);
    }
}

void WINAPI
CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    //JR the Windows Mobile plugin does not use a dedicated data interface
}

void WINAPI
CPluginInterface::ClearHistory(HWND parent)
{
}

CPluginInterfaceForFSAbstract* WINAPI
CPluginInterface::GetInterfaceForFS()
{
    return &InterfaceForFS;
}
