// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#include "precomp.h"
#include "nethood.h"
#include "cache.h"
#include "nethoodfs.h"
#include "nethoodfs2.h"
#include "nethooddata.h"
#include "nethoodmenu.h"
#include "globals.h"
#include "config.h"
#include "icons.h"
#include "nethood.rh"
#include "nethood.rh2"
#include "lang\lang.rh"
#include "cfgdlg.h"

extern CNethoodCache g_oNethoodCache;
extern CNethoodIcons g_oIcons;
extern CNethoodPluginInterfaceForMenuExt g_oMenuExt;

static const TCHAR CONFIG_VERSION[] = TEXT("Version");
#define CURRENT_CONFIG_VERSION 1
static const TCHAR CONFIG_SYSSHARES[] = TEXT("ShowSystemShares");
static const TCHAR CONFIG_NETSHORTCUTS[] = TEXT("ShowShortcuts");
static const TCHAR CONFIG_SHOWSERVERS[] = TEXT("ShowServers");
static const TCHAR CONFIG_PRELOAD[] = TEXT("Preload");
static const TCHAR CONFIG_TSCVOLUMES[] = TEXT("ShowRDSVolumes"); // Remote Desktop Services

void WINAPI
CNethoodPluginInterface::About(
    __in HWND hwndParent)
{
    TCHAR szMessage[256];

    StringCchPrintf(szMessage, COUNTOF(szMessage),
                    TEXT("%s ") TEXT(VERSINFO_VERSION) TEXT("\n\n")
                        TEXT(VERSINFO_COPYRIGHT) TEXT("\n\n")
                            TEXT("%s"),
                    SalamanderGeneral->LoadStr(GetLangInstance(), IDS_PLUGIN_NAME),
                    SalamanderGeneral->LoadStr(GetLangInstance(), IDS_DESCRIPTION));

    SalamanderGeneral->SalMessageBox(
        hwndParent,
        szMessage,
        SalamanderGeneral->LoadStr(GetLangInstance(), IDS_ABOUT),
        MB_OK | MB_ICONINFORMATION);
}

BOOL WINAPI
CNethoodPluginInterface::Release(
    __in HWND parent,
    __in BOOL force)
{
    g_oNethoodCache.Destroy();
    ReleaseWinLib(g_hInstance);
    return TRUE;
}

void WINAPI
CNethoodPluginInterface::LoadConfiguration(
    __in HWND hwndParent,
    __in HKEY hKey,
    __in CSalamanderRegistryAbstract* registry)
{
    DWORD dwVersion;
    DWORD dwArbitrary;
    bool bDisplayShortcuts;

    CALL_STACK_MESSAGE1("CNethoodPluginInterface::LoadConfiguration(, ,)");

    if (!hKey || !registry->GetValue(hKey, CONFIG_VERSION, REG_DWORD, &dwVersion, sizeof(DWORD)))
        dwVersion = CURRENT_CONFIG_VERSION;

    if (hKey && registry->GetValue(hKey, CONFIG_SYSSHARES, REG_DWORD, &dwArbitrary, sizeof(DWORD)) &&
        dwArbitrary < 3)
    {
        g_oNethoodCache.SetDisplaySystemShares(static_cast<CNethoodCache::SystemSharesDisplayMode>(dwArbitrary));
    }
    else
    {
        g_oNethoodCache.SetDisplaySystemShares(CNethoodCache::SysShareNone);
    }

    if (hKey && registry->GetValue(hKey, CONFIG_NETSHORTCUTS, REG_DWORD, &dwArbitrary, sizeof(DWORD)))
    {
        bDisplayShortcuts = (dwArbitrary != FALSE);
    }
    else
    {
        // Display My Network Places on Win2K/XP/2003 by default.
        bDisplayShortcuts = (_winmajor == 5);
    }
    g_oNethoodCache.SetDisplayNetworkShortcuts(bDisplayShortcuts);

    if (hKey && registry->GetValue(hKey, CONFIG_SHOWSERVERS, REG_DWORD, &dwArbitrary, sizeof(DWORD)))
    {
        CNethoodFSInterface::SetHideServersInRoot(dwArbitrary == 0);
    }
    else
    {
        CNethoodFSInterface::SetHideServersInRoot(bDisplayShortcuts);
    }

    if (hKey && registry->GetValue(hKey, CONFIG_PRELOAD, REG_DWORD, &dwArbitrary, sizeof(DWORD)))
    {
        m_bPreload = (dwArbitrary != 0);
    }
    else
    {
        m_bPreload = false;
    }

    bool bTSAvailable = g_oNethoodCache.AreTSAvailable();
    if (hKey && registry->GetValue(hKey, CONFIG_TSCVOLUMES, REG_DWORD, &dwArbitrary, sizeof(DWORD)) && bTSAvailable && dwArbitrary < 3)
    {
        g_oNethoodCache.SetDisplayTSClientVolumes(static_cast<CNethoodCache::TSCDisplayMode>(dwArbitrary));
    }
    else
    {
        g_oNethoodCache.SetDisplayTSClientVolumes(bTSAvailable ? CNethoodCache::TSCDisplayFolder : CNethoodCache::TSCDisplayNone);
    }
}

void WINAPI
CNethoodPluginInterface::SaveConfiguration(
    __in HWND hwndParent,
    __in HKEY hKey,
    __in CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CNethoodPluginInterface::SaveConfiguration(, ,)");

    if (hKey != NULL)
    {
        DWORD dwArbitrary;

        dwArbitrary = CURRENT_CONFIG_VERSION;
        registry->SetValue(hKey, CONFIG_VERSION, REG_DWORD, &dwArbitrary, sizeof(DWORD));

        dwArbitrary = g_oNethoodCache.GetDisplaySystemShares();
        registry->SetValue(hKey, CONFIG_SYSSHARES, REG_DWORD, &dwArbitrary, sizeof(DWORD));

        dwArbitrary = g_oNethoodCache.GetDisplayNetworkShortcuts();
        registry->SetValue(hKey, CONFIG_NETSHORTCUTS, REG_DWORD, &dwArbitrary, sizeof(DWORD));

        dwArbitrary = !CNethoodFSInterface::GetHideServersInRoot();
        registry->SetValue(hKey, CONFIG_SHOWSERVERS, REG_DWORD, &dwArbitrary, sizeof(DWORD));

        dwArbitrary = m_bPreload;
        registry->SetValue(hKey, CONFIG_PRELOAD, REG_DWORD, &dwArbitrary, sizeof(DWORD));

        dwArbitrary = g_oNethoodCache.GetDisplayTSClientVolumes();
        registry->SetValue(hKey, CONFIG_TSCVOLUMES, REG_DWORD, &dwArbitrary, sizeof(DWORD));
    }
}

void WINAPI
CNethoodPluginInterface::Configuration(
    __in HWND hwndParent)
{
    CALL_STACK_MESSAGE1("CNethoodPluginInterface::Configuration()");

    CNethoodConfigDialog(hwndParent).Execute();
}

void WINAPI
CNethoodPluginInterface::Connect(
    __in HWND parent,
    __in CSalamanderConnectAbstract* salamander)
{
    TCHAR szFileMenu[MAX_PATH];
    int iIcon = -1;

    szFileMenu[0] = TEXT(',');
    szFileMenu[1] = TEXT('\t');

    LoadString(GetLangInstance(), IDS_MENUITEM, &szFileMenu[2], COUNTOF(szFileMenu) - 2);

    g_oIcons.Load();

    HICON hIcon = g_oIcons.GetIcon(SALICONSIZE_16, CNethoodIcons::IconMain);
    assert(hIcon != NULL);
    if (hIcon != NULL)
    {
        CGUIIconListAbstract* pIconList;

        pIconList = SalamanderGUI->CreateIconList();
        assert(pIconList != NULL);
        if (pIconList->Create(16, 16, 1))
        {
            if (pIconList->ReplaceIcon(0, hIcon))
            {
                salamander->SetIconListForGUI(pIconList);
                iIcon = 0;
            }
            else
            {
                assert(0);
                SalamanderGUI->DestroyIconList(pIconList);
            }
        }
        else
        {
            assert(0);
            SalamanderGUI->DestroyIconList(pIconList);
        }

        DestroyIcon(hIcon);
    }

    assert(iIcon >= 0);
    salamander->SetPluginIcon(iIcon);
    salamander->SetChangeDriveMenuItem(szFileMenu, iIcon);
    salamander->SetPluginMenuAndToolbarIcon(-1);

    if (m_bPreload)
    {
        g_oNethoodCache.GetPathStatus(TEXT("\\"), NULL, NULL);
    }
}

void WINAPI
CNethoodPluginInterface::ReleasePluginDataInterface(
    __in CPluginDataInterfaceAbstract* pluginData)
{
    CNethoodPluginDataInterface* pNethoodData;

    TRACE_I("ReleasePluginDataInterface(" << pluginData << ")");

    // Typecast to correct destructor be called.
    pNethoodData = static_cast<CNethoodPluginDataInterface*>(pluginData);
    delete pNethoodData;
}

CPluginInterfaceForArchiverAbstract* WINAPI
CNethoodPluginInterface::GetInterfaceForArchiver()
{
    return NULL;
}

CPluginInterfaceForViewerAbstract* WINAPI
CNethoodPluginInterface::GetInterfaceForViewer()
{
    return NULL;
}

CPluginInterfaceForMenuExtAbstract* WINAPI
CNethoodPluginInterface::GetInterfaceForMenuExt()
{
    return &g_oMenuExt;
}

CPluginInterfaceForFSAbstract* WINAPI
CNethoodPluginInterface::GetInterfaceForFS()
{
    return &g_oNethoodFS;
}

CPluginInterfaceForThumbLoaderAbstract* WINAPI
CNethoodPluginInterface::GetInterfaceForThumbLoader()
{
    return NULL;
}

void WINAPI
CNethoodPluginInterface::Event(
    __in int event,
    __in DWORD param)
{
}

void WINAPI
CNethoodPluginInterface::ClearHistory(
    __in HWND parent)
{
}

void WINAPI
CNethoodPluginInterface::AcceptChangeOnPathNotification(
    __in const char* path,
    __in BOOL includingSubdirs)
{
}

void WINAPI
CNethoodPluginInterface::PasswordManagerEvent(
    __in HWND parent,
    __in int event)
{
}

void CNethoodPluginInterface::SetPreloadFlag(bool bPreload)
{
    m_bPreload = bPreload;
    SalamanderGeneral->SetFlagLoadOnSalamanderStart(bPreload);
}
