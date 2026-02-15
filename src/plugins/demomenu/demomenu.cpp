// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

// Plugin interface object whose methods are called by Salamander
CPluginInterface PluginInterface;
// Additional interfaces exposed by CPluginInterface
CPluginInterfaceForMenuExt InterfaceForMenuExt;

// Global data
const char* PluginNameEN = "DemoMenu";    // Non-translated plugin name, used before loading the language module + for debugging
const char* PluginNameShort = "DEMOMENU"; // Plugin name (short, without spaces)

HINSTANCE DLLInstance = NULL; // Handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // Handle to SLG - language-dependent resources

// Salamander general interface - available from Salamander launch until the plugin shuts down
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// Variable required by "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// Variable required by "spl_com.h"
int SalamanderVersion = 0;

// Interface providing customized Windows controls used in Salamander
//CSalamanderGUIAbstract *SalamanderGUI = NULL;

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

// ****************************************************************************

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
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

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // Set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // Set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // Verify Salamander is at the minimum supported version before continuing
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // Reject older versions
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   PluginNameEN, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // Load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), PluginNameEN);
    if (HLanguage == NULL)
        return NULL;

    // Acquire Salamander's general interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    // Acquire the interface providing customized Windows controls used in Salamander
    //  SalamanderGUI = salamander->GetSalamanderGUI();

    // Register the name of the help file
    SalamanderGeneral->SetHelpFileName("demomenu.chm");

    // Provide the basic plugin metadata
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME), 0, VERSINFO_VERSION_NO_PLATFORM, VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION), PluginNameShort,
                                   NULL, NULL);

    // Register the plugin home page URL
    salamander->SetPluginHomePageURL(LoadStr(IDS_PLUGIN_HOME));

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

void WINAPI
CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // Register the basic menu item:
    salamander->AddMenuItem(-1, LoadStr(IDS_TESTCMD), SALHOTKEY('M', HOTKEYF_CONTROL | HOTKEYF_SHIFT),
                            MENUCMD_TESTCMD, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    /*
  CGUIIconListAbstract *iconList = SalamanderGUI->CreateIconList();
  iconList->Create(16, 16, 1);
  HICON hIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_PLUGINICON), IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
  iconList->ReplaceIcon(0, hIcon);
  DestroyIcon(hIcon);
  salamander->SetIconListForGUI(iconList); // Salamander takes care of destroying the icon list

  salamander->SetPluginIcon(0);
  salamander->SetPluginMenuAndToolbarIcon(0);
*/
}

CPluginInterfaceForMenuExtAbstract* WINAPI
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}
