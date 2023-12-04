// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	entry.cpp
	Automation plugin entry point.
*/

#include "precomp.h"
#include "automationplug.h"
#include "automation.rh2"
#include "lang\lang.rh"
#include "versinfo.rh2"

/// Plugin module instance handle.
HINSTANCE g_hInstance;

/// Language module instance handle.
HINSTANCE g_hLangInst;

/// Salamander general interface.
CSalamanderGeneralAbstract* SalamanderGeneral;

/// Salamander version (see \e spl_vers.h).
int SalamanderVersion;

/// Salamander debug interface.
CSalamanderDebugAbstract* SalamanderDebug;

/// Salamander GUI interface.
CSalamanderGUIAbstract* SalamanderGUI;

/// Instance of our plugin.
CAutomationPluginInterface g_oAutomationPlugin;

/// Caption for the dialog boxes before we load the language module.
_TCHAR MSGBOX_CAPTION[] = _T("Automation");

/// Entry point of the SPL module.
/// \param hModule A handle to the DLL module.
/// \param dwReason The reason code that indicates why the DLL entry-point
///        function is being called.
/// \param lpvReserved This parameter is ignored.
/// \return The function returns TRUE if it succeeds or FALSE if it fails.
BOOL WINAPI DllMain(
    HINSTANCE hModule,
    DWORD dwReason,
    LPVOID lpvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = /*g_hLangInst =*/hModule;
        DisableThreadLibraryCalls(hModule);
    }

    return TRUE;
}

/// HTML help callback, needed by WinLib.
static void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

/// Returns the object representing the plugin.
/// \param salamander Pointer to the CSalamanderPluginEntryAbstract interface.
/// \return If the function succeeds, the return value is pointer to the
///         plugin object. If the function fails, the return value is NULL.
CPluginInterfaceAbstract*
    WINAPI
    SalamanderPluginEntry(
        CSalamanderPluginEntryAbstract* salamander)
{
    SalamanderDebug = salamander->GetSalamanderDebug();
    SalamanderVersion = salamander->GetVersion();

    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderGUI = salamander->GetSalamanderGUI();

    // requires latest version of Salamander
    if (salamander->GetVersion() < SalamanderPluginGetReqVer())
    {
        // reject older versions
        SalamanderGeneral->SalMessageBox(
            salamander->GetParentWindow(),
            REQUIRE_LAST_VERSION_OF_SALAMANDER,
            MSGBOX_CAPTION,
            MB_OK | MB_ICONERROR);
        return NULL;
    }

    g_hLangInst = salamander->LoadLanguageModule(
        salamander->GetParentWindow(),
        MSGBOX_CAPTION);
    if (!g_hLangInst)
        return NULL;

    if (!InitializeWinLib(MSGBOX_CAPTION, g_hInstance))
    {
        return NULL;
    }
    SetupWinLibHelp(HTMLHelpCallback);

    // setup help file name
    SalamanderGeneral->SetHelpFileName("automation.chm");

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(
        SalamanderGeneral->LoadStr(g_hLangInst, IDS_PLUGINNAME),
        FUNCTION_CONFIGURATION |
            FUNCTION_LOADSAVECONFIGURATION |
            FUNCTION_DYNAMICMENUEXT,
        VERSINFO_VERSION_NO_PLATFORM,
        VERSINFO_COPYRIGHT,
        SalamanderGeneral->LoadStr(g_hLangInst, IDS_DESCRIPTION),
        MSGBOX_CAPTION,
        NULL,
        NULL);

    // Setup plugin home page.
    salamander->SetPluginHomePageURL("www.altap.cz");

    return &g_oAutomationPlugin;
}

/// Returns version of Open Salamander required to run this plugin.
/// \return The return value is required version of Open Salamander.
///         For list of version values see \e spl_vers.h.
int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}
