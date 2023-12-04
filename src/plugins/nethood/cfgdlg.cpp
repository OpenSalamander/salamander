// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#include "precomp.h"
#include "cache.h"
#include "nethood.h"
#include "nethoodfs.h"
#include "nethoodfs2.h"
#include "globals.h"
#include "nethood.rh"
#include "nethood.rh2"
#include "lang\lang.rh"
#include "cfgdlg.h"

/// Cache of network.
extern CNethoodCache g_oNethoodCache;

CNethoodConfigDialog::CNethoodConfigDialog(__in HWND hwndParent)
    : CDialog(GetLangInstance(), IDD_CONFIG, IDD_CONFIG, hwndParent)
{
}

INT_PTR CNethoodConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // Transfer variables first.
        INT_PTR res = CDialog::DialogProc(uMsg, wParam, lParam);
        if (Parent != NULL)
        {
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        }
        EnableDisableCheckBox();
        if (!g_oNethoodCache.AreTSAvailable())
        {
            EnableWindow(GetDlgItem(HWindow, IDC_SHOWTSC), FALSE);
        }
        return res;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_SYSSHARES)
        {
            EnableDisableCheckBox();
        }
        break;
    }
    }

    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CNethoodConfigDialog::Transfer(CTransferInfo& ti)
{
    int bSysShares;
    int bHideSysShares;
    int bShortcuts;
    CNethoodCache::SystemSharesDisplayMode sysSharesMode;
    int bShowServersInRoot;
    int bPreload;
    CNethoodCache::TSCDisplayMode tscMode;
    int bShowTSC;

    if (ti.Type == ttDataToWindow)
    {
        sysSharesMode = g_oNethoodCache.GetDisplaySystemShares();
        bSysShares = (sysSharesMode != CNethoodCache::SysShareNone);
        bHideSysShares = (sysSharesMode == CNethoodCache::SysShareDisplayHidden);
        bShortcuts = g_oNethoodCache.GetDisplayNetworkShortcuts();
        bShowServersInRoot = !CNethoodFSInterface::GetHideServersInRoot();
        bPreload = g_oNethoodPlugin.GetPreloadFlag();
        tscMode = g_oNethoodCache.GetDisplayTSClientVolumes();
        bShowTSC = IsWindowEnabled(GetDlgItem(HWindow, IDC_SHOWTSC)) &&
                   tscMode != CNethoodCache::TSCDisplayNone;
    }

    ti.CheckBox(IDC_SYSSHARES, bSysShares);
    ti.CheckBox(IDC_HIDESYSSHARES, bHideSysShares);
    ti.CheckBox(IDC_NETSHORTCUTS, bShortcuts);
    ti.CheckBox(IDC_SHOWWORKGRP, bShowServersInRoot);
    ti.CheckBox(IDC_PRELOAD, bPreload);
    ti.CheckBox(IDC_SHOWTSC, bShowTSC);

    if (ti.Type == ttDataFromWindow)
    {
        if (bSysShares)
        {
            if (bHideSysShares)
            {
                sysSharesMode = CNethoodCache::SysShareDisplayHidden;
            }
            else
            {
                sysSharesMode = CNethoodCache::SysShareDisplayNormal;
            }
        }
        else
        {
            sysSharesMode = CNethoodCache::SysShareNone;
        }

        if (bShowTSC)
        {
            tscMode = CNethoodCache::TSCDisplayFolder;
        }
        else
        {
            tscMode = CNethoodCache::TSCDisplayNone;
        }

        g_oNethoodCache.SetDisplaySystemShares(sysSharesMode);
        g_oNethoodCache.SetDisplayNetworkShortcuts(!!bShortcuts);
        CNethoodFSInterface::SetHideServersInRoot(!bShowServersInRoot);
        g_oNethoodPlugin.SetPreloadFlag(!!bPreload);
        g_oNethoodCache.SetDisplayTSClientVolumes(tscMode);

        RefreshPanels();
    }
}

void CNethoodConfigDialog::EnableDisableCheckBox()
{
    EnableWindow(
        GetDlgItem(HWindow, IDC_HIDESYSSHARES),
        IsDlgButtonChecked(HWindow, IDC_SYSSHARES) == BST_CHECKED);
}

void CNethoodConfigDialog::RefreshPanel(int iPanel)
{
    CNethoodFSInterface* pFS;

    pFS = static_cast<CNethoodFSInterface*>(SalamanderGeneral->GetPanelPluginFS(iPanel));
    if (pFS != NULL)
    {
        pFS->ConfigurationChanged();
    }
}
