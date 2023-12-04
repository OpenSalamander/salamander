// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...

	Network plugin entry points.
*/

#include "precomp.h"
#include "nethood.h"
#include "nethoodfs.h"
#include "globals.h"
#include "config.h"
#include "nethood.rh"
#include "nethood.rh2"
#include "lang\lang.rh"

unsigned int _winmajor;
unsigned int _winminor;
unsigned int _osplatform;

/// Entry point of the SPL module.
/// \param hModule A handle to the DLL module.
/// \param dwReason The reason code that indicates why the DLL entry-point
///        function is being called.
/// \param lpvReserved This parameter is ignored.
/// \return The function returns TRUE if it succeeds or FALSE if it fails.
BOOL WINAPI
DllMain(
    __in HINSTANCE hModule,
    __in DWORD dwReason,
    __in LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hModule;
        g_hLangInstance = hModule;
        DisableThreadLibraryCalls(hModule);
//		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);  // Petr: tohle resi Salamander, sem to nepatri
#if (_MSC_VER >= 1500)
        //		OSVERSIONINFO osver = {sizeof(OSVERSIONINFO), };
        //		if (GetVersionEx(&osver))
        //		{
        //			_winmajor = osver.dwMajorVersion;
        //			_winminor = osver.dwMinorVersion;
        //			_osplatform = osver.dwPlatformId;
        //		}

        // Petr: potrebuji se obejit bez volani GetVersionEx (obsolete) + potrebuji _winmajor,
        // _winminor a _osplatform (pouziva je GetOsSpecificData), takze je pro ucely Nethoodu
        // ziskam zcela primitivne, viz nasledujici kod

        // Windows Versions supported by Open Salamander
        //
        // Name                        dwPlatformId  dwMajorVersion  dwMinorVersion  dwBuildNumber
        //----------------------------------------------------------------------------------------
        // Windows 2000                2             5               0
        // Windows XP                  2             5               1
        // Windows Vista               2             6               0
        // Windows 7                   2             6               1
        // Windows 8                   2             6               2
        // Windows 8.1                 2             6               3
        _winmajor = 5;
        _winminor = 0;
        _osplatform = 2; // VER_PLATFORM_WIN32_NT
        if (SalIsWindowsVersionOrGreater(6, 3, 0))
        {
            _winmajor = 6;
            _winminor = 3;
        }
        else if (SalIsWindowsVersionOrGreater(6, 2, 0))
        {
            _winmajor = 6;
            _winminor = 2;
        }
        else if (SalIsWindowsVersionOrGreater(6, 1, 0))
        {
            _winmajor = 6;
            _winminor = 1;
        }
        else if (SalIsWindowsVersionOrGreater(6, 0, 0))
        {
            _winmajor = 6;
            _winminor = 0;
        }
        else if (SalIsWindowsVersionOrGreater(5, 1, 0))
        {
            _winmajor = 5;
            _winminor = 1;
        }
#endif
    }

    return TRUE;
}

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

/// Returns the object representing the plugin.
/// \param salamander Pointer to the CSalamanderPluginEntryAbstract interface.
/// \return If the function succeeds, the return value is pointer to the
///         plugin object. If the function fails, the return value is NULL.
CPluginInterfaceAbstract* WINAPI
SalamanderPluginEntry(
    __in CSalamanderPluginEntryAbstract* salamander)
{
    HINSTANCE hLangInst;
    TCHAR szDescription[128];

    SalamanderDebug = salamander->GetSalamanderDebug();
    SalamanderVersion = salamander->GetVersion();

    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // Check Salamander version.
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    {
        // Reject older versions.
        MessageBox(
            salamander->GetParentWindow(),
            REQUIRE_LAST_VERSION_OF_SALAMANDER,
            PluginNameEN,
            MB_OK | MB_ICONERROR);
        return NULL;
    }

    // Initialize global variables.
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderGUI = salamander->GetSalamanderGUI();

    // setup help file name
    SalamanderGeneral->SetHelpFileName("nethood.chm");

#if 0
	// DBG: Fake OS version
	_winmajor = 5;
	_winminor = 1;
	//_osplatform = VER_PLATFORM_WIN32_NT;
#endif

    // Load language module.
    hLangInst = salamander->LoadLanguageModule(
        salamander->GetParentWindow(),
        PluginNameEN);
    if (hLangInst == NULL)
    {
        return NULL;
    }
    g_hLangInstance = hLangInst;

    if (!InitializeWinLib(PluginNameEN, g_hInstance))
    {
        return NULL;
    }
    SetupWinLibHelp(HTMLHelpCallback);

    LoadString(hLangInst, IDS_DESCRIPTION, szDescription, COUNTOF(szDescription));

    // Setup basic plugin information.
    salamander->SetBasicPluginData(
        SalamanderGeneral->LoadStr(GetLangInstance(), IDS_PLUGIN_NAME),
        FUNCTION_CONFIGURATION |
            FUNCTION_LOADSAVECONFIGURATION |
            FUNCTION_FILESYSTEM,
        VERSINFO_VERSION_NO_PLATFORM,
        VERSINFO_COPYRIGHT,
        szDescription,
        SuggestedConfigKey,
        NULL,
        SuggestedFSName);

    // Inform Salamander that this is Nethood plugin.
    SalamanderGeneral->SetPluginIsNethood();

    // Retrieve the file system name assigned to plugin by Salamander
    // (may be different from suggested name).
    SalamanderGeneral->GetPluginFSName(g_szAssignedFSName, 0);
    g_cchAssignedFSName = _tcslen(g_szAssignedFSName);

    // Setup plugin home page.
    salamander->SetPluginHomePageURL(HomePageUrl);

    // Return plugin interface.
    return &g_oNethoodPlugin;
}

/// Returns version of Open Salamander required to run this plugin.
/// \return The return value is required version of Open Salamander.
///         For list of version values see \e spl_vers.h.
int WINAPI
SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}
