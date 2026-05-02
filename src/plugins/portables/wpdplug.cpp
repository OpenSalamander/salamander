// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	wpdplug.cpp
	Salamander plugin interface.
*/

#include "precomp.h"
#include "wpdplug.h"
#include "wpdplugfs.h"
#include "globals.h"
#include "notifier.h"
#include "wpddeviceicons.h"
#include "lang/lang.rh"
#include "wpd.rh"

const TCHAR CWpdPluginInterface::c_szEnglishName[] = TEXT("Portable Devices");

CWpdNotifier* g_pNotifier;
extern CWpdDeviceIcons g_oDeviceIcons;

DWORD WINAPI CWpdPluginInterface::GetSupportedFunctions() const
{
    return ////FUNCTION_CONFIGURATION |
        ////FUNCTION_LOADSAVECONFIGURATION |
        FUNCTION_FILESYSTEM;
}

void WINAPI CWpdPluginInterface::GetPluginEnglishName(CFxString& name) const
{
    name = c_szEnglishName;
}

void WINAPI CWpdPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    __super::Connect(parent, salamander);

    g_oDeviceIcons.Initialize();

    g_pNotifier = new CWpdNotifier();
}

BOOL WINAPI CWpdPluginInterface::Release(HWND parent, BOOL force)
{
    BOOL res = __super::Release(parent, force);

    if (res)
    {
        delete g_pNotifier;
        g_pNotifier = nullptr;

        g_oDeviceIcons.Destroy();
    }

    return res;
}

CFxPluginInterfaceForFS* WINAPI CWpdPluginInterface::CreateInterfaceForFS()
{
    return new CWpdPluginInterfaceForFS(*this);
}

bool WINAPI CWpdPluginInterface::NeedsWinLib() const
{
    // No need for Salamander's WinLib, we rather use ATL/WTL combo.
    return false;
}
