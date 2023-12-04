// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	entry.cpp
	Plugin entry points.
*/

#include "precomp.h"
#include "wpdplug.h"
#include "wpdplugfs.h"
#include "globals.h"
#include "lang/lang.rh"

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)

/// Entry point of the SPL module.
/// \param hModule A handle to the DLL module.
/// \param dwReason The reason code that indicates why the DLL entry-point
///        function is being called.
/// \param lpvReserved This parameter is ignored.
/// \return The function returns TRUE if it succeeds or FALSE if it fails.
BOOL WINAPI
DllMain(
    _In_ HINSTANCE hModule,
    _In_ DWORD dwReason,
    _In_ LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    if (dwReason == DLL_PROCESS_ATTACH)
    {
#ifdef _DLL
        // Linking to the dynamic CRT.
        DisableThreadLibraryCalls(hModule);
#else
#pragma message(__FILE__ "(" STRINGIZE(__LINE__) "): Not linking to the shared dynamic version of C run-time library (SALRTL).")
        // We don't call DisableThreadLibraryCalls when linking to the
        // static C run-time, since MSDN warns:
        // Do not call this function from a DLL that is linked to the
        // static C run-time library (CRT). The static CRT requires
        // DLL_THREAD_ATTACH and DLL_THREAD_DETATCH notifications to
        // function properly.
#endif
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }

    return TRUE;
}

/// Returns the object representing the plugin.
/// \param salamander Pointer to the CSalamanderPluginEntryAbstract interface.
/// \return If the function succeeds, the return value is pointer to the
///         plugin object. If the function fails, the return value is NULL.
CPluginInterfaceAbstract* WINAPI
SalamanderPluginEntry(
    _In_ CSalamanderPluginEntryAbstract* salamander)
{
    // Initialize global variables.
    SalamanderDebug = salamander->GetSalamanderDebug();
    SalamanderVersion = salamander->GetVersion();
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderGUI = salamander->GetSalamanderGUI();

    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    CFxString invariantName;
    g_oPlugin.GetPluginEnglishName(invariantName);

    // Check Salamander version.
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    {
        // Reject older versions.
        MessageBox(
            salamander->GetParentWindow(),
            REQUIRE_LAST_VERSION_OF_SALAMANDER,
            invariantName,
            MB_OK | MB_ICONERROR);
        return nullptr;
    }

    if (!g_oPlugin.Initialize(salamander))
    {
        return nullptr;
    }

    CFxString name, description, configKey;
    DWORD functions;

    g_oPlugin.GetPluginName(name);
    g_oPlugin.GetPluginDescription(description);
    g_oPlugin.GetPluginConfigKey(configKey);
    functions = g_oPlugin.GetSupportedFunctions();

    PCTSTR suggestedFSName = nullptr;
    CFxPluginInterfaceForFS* pluginInterfaceForFS;
    if (functions & FUNCTION_FILESYSTEM)
    {
        pluginInterfaceForFS = static_cast<CFxPluginInterfaceForFS*>(g_oPlugin.GetInterfaceForFS());
        _ASSERTE(pluginInterfaceForFS != nullptr);
        suggestedFSName = pluginInterfaceForFS->GetSuggestedFSName();
    }

    // Setup basic plugin information.
    if (!salamander->SetBasicPluginData(
            name,
            functions,
            VERSINFO_VERSION_NO_PLATFORM,
            VERSINFO_COPYRIGHT,
            description,
            configKey,
            NULL,
            suggestedFSName))
    {
        return nullptr;
    }

    if (functions & FUNCTION_FILESYSTEM)
    {
        // Retrieve the file system name assigned to plugin by Salamander
        // (may be different from suggested name).
        pluginInterfaceForFS->RetrieveAssignedFSName();
    }

    // Return plugin interface.
    return &g_oPlugin;
}

/// Returns version of Open Salamander required to run this plugin.
/// \return The return value is required version of Open Salamander.
///         For list of version values see \e spl_vers.h.
int WINAPI
SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}
