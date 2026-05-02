// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: noexpandtab:sw=8:ts=8

/*
	PE File Viewer Plugin for Open Salamander

	Copyright (c) 2015-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2015-2023 Open Salamander Authors

	cfgdlg.cpp
	Configuration dialog box.
*/

#include "precomp.h"
#include "lang\lang.rh"
#include "cfgdlg.h"
#include "pefile.h"
#include "peviewer.h"
#include "cfg.h"

CPeViewerConfigDialog::CPeViewerConfigDialog(HWND hwndParent)
    : CDialog(HLanguage, IDD_CONFIG, IDD_CONFIG, hwndParent)
{
}

void EnableButton(HWND hWindow, int btn1, BOOL enable, int btn2, BOOL prefer3, int btn3)
{
    if (!enable && GetFocus() == GetDlgItem(hWindow, btn1))
    {
        HWND nextBtn = GetDlgItem(hWindow, prefer3 ? btn3 : btn2);
        if (!IsWindowEnabled(nextBtn))
        {
            EnableWindow(nextBtn, TRUE);
        }
        SendMessage(hWindow, WM_NEXTDLGCTL, (WPARAM)nextBtn, TRUE);
    }
    EnableWindow(GetDlgItem(hWindow, btn1), enable);
}

void CPeViewerConfigDialog::EnableControls()
{
    CALL_STACK_MESSAGE1("CPeViewerConfigDialog::EnableControls()");
    int aIndex = (int)SendDlgItemMessage(HWindow, IDC_AVAILABLELB, LB_GETCURSEL, 0, 0);
    int cIndex = (int)SendDlgItemMessage(HWindow, IDC_SELECTEDLB, LB_GETCURSEL, 0, 0);
    int aCount = (int)SendDlgItemMessage(HWindow, IDC_AVAILABLELB, LB_GETCOUNT, 0, 0);
    int cCount = (int)SendDlgItemMessage(HWindow, IDC_SELECTEDLB, LB_GETCOUNT, 0, 0);

    EnableButton(HWindow, IDC_ADD, aCount > 0 && aIndex >= 0, IDC_REMOVE, FALSE, -1);
    EnableButton(HWindow, IDC_REMOVE, cCount > 0 && cIndex >= 0, IDC_ADD, FALSE, -1);
    EnableButton(HWindow, IDC_UP, cCount > 0 && cIndex > 0, IDC_DOWN, cCount <= 1, IDC_ADD);
    EnableButton(HWindow, IDC_DOWN, cCount > 0 && cIndex >= 0 && cIndex < cCount - 1,
                 IDC_UP, cCount <= 1, IDC_ADD);
}

INT_PTR CPeViewerConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // Transfer variables first.
        INT_PTR res = CDialog::DialogProc(uMsg, wParam, lParam);
        if (Parent != NULL)
        {
            SalGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        }
        return res;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == LBN_SELCHANGE)
        {
            EnableControls();
            return 0;
        }
        if (HIWORD(wParam) == LBN_DBLCLK)
        {
            if ((HWND)lParam == GetDlgItem(HWindow, IDC_SELECTEDLB))
                wParam = IDC_REMOVE;
            if ((HWND)lParam == GetDlgItem(HWindow, IDC_AVAILABLELB))
                wParam = IDC_ADD;
            // pass wParam to OnCommand()
        }
        OnCommand(LOWORD(wParam));
        break;
    }
    }

    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CPeViewerConfigDialog::OnCommand(int nCmd)
{
    switch (nCmd)
    {
    case IDC_ADD:
        MoveItem(GetDlgItem(HWindow, IDC_AVAILABLELB), GetDlgItem(HWindow, IDC_SELECTEDLB));
        break;
    case IDC_REMOVE:
        MoveItem(GetDlgItem(HWindow, IDC_SELECTEDLB), GetDlgItem(HWindow, IDC_AVAILABLELB));
        break;
    case IDC_UP:
        MoveItem(GetDlgItem(HWindow, IDC_SELECTEDLB), -1);
        break;
    case IDC_DOWN:
        MoveItem(GetDlgItem(HWindow, IDC_SELECTEDLB), +1);
        break;
    case IDC_RESET:
        SendDlgItemMessage(HWindow, IDC_AVAILABLELB, LB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(HWindow, IDC_SELECTEDLB, LB_RESETCONTENT, 0, 0);
        for (int i = 0; i < AVAILABLE_PE_DUMPERS; i++)
            AddItem(GetDlgItem(HWindow, IDC_SELECTEDLB), g_cfgChainDefault[i].pDumperCfg);
        SendMessage(GetDlgItem(HWindow, IDC_SELECTEDLB), LB_SETCURSEL, 0, 0);
        break;
    }
    EnableControls();
}

void CPeViewerConfigDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        FillListBoxes();
    }
    else if (ti.Type == ttDataFromWindow)
    {
        BuildConfigurationChain();
    }
}

void CPeViewerConfigDialog::FillListBoxes()
{
    HWND hwndAvailableLB = GetDlgItem(HWindow, IDC_AVAILABLELB);
    HWND hwndSelectedLB = GetDlgItem(HWindow, IDC_SELECTEDLB);

    unsigned i;

    for (i = 0; i < g_cfgChainLength; i++)
    {
        AddItem(hwndSelectedLB, g_cfgChain[i].pDumperCfg);
    }
    if (i > 0)
        SendMessage(hwndSelectedLB, LB_SETCURSEL, 0, 0);

    for (i = 0; i < AVAILABLE_PE_DUMPERS; i++)
    {
        int iDumperInChain = FindDumperInChain(&g_cfgDumpers[i]);
        if (iDumperInChain < 0)
        {
            AddItem(hwndAvailableLB, &g_cfgDumpers[i]);
        }
    }
    if (i > 0)
        SendMessage(hwndAvailableLB, LB_SETCURSEL, 0, 0);

    EnableControls();
}

void CPeViewerConfigDialog::BuildConfigurationChain()
{
    HWND hwndSelectedLB = GetDlgItem(HWindow, IDC_SELECTEDLB);

    LRESULT count = SendMessage(hwndSelectedLB, LB_GETCOUNT, 0, 0);

    g_cfgChainLength = (unsigned)count;

    for (int i = 0; i < count; i++)
    {
        g_cfgChain[i].pDumperCfg = (const CFG_DUMPER*)SendMessage(hwndSelectedLB, LB_GETITEMDATA, i, 0);
    }
}

int CPeViewerConfigDialog::InsertItem(HWND hwndLB, const CFG_DUMPER* pDumperCfg, int index, bool bSelect)
{
    PCTSTR pszDumperTitle = GetDumperTitleStr(pDumperCfg->nTitleId);
    LRESULT iItem = SendMessage(hwndLB, LB_INSERTSTRING, index, (LPARAM)pszDumperTitle);
    SendMessage(hwndLB, LB_SETITEMDATA, iItem, (LPARAM)pDumperCfg);

    if (bSelect)
    {
        SendMessage(hwndLB, LB_SETCURSEL, iItem, 0);
    }

    return (int)iItem;
}

void CPeViewerConfigDialog::MoveItem(HWND hwndSourceLB, HWND hwndDestinationLB)
{
    LRESULT iSource = SendMessage(hwndSourceLB, LB_GETCURSEL, 0, 0);
    if (iSource == LB_ERR)
    {
        return;
    }

    const CFG_DUMPER* pDumperCfg = (const CFG_DUMPER*)SendMessage(hwndSourceLB, LB_GETITEMDATA, iSource, 0);
    SendMessage(hwndSourceLB, LB_DELETESTRING, iSource, 0);

    LRESULT count = SendMessage(hwndSourceLB, LB_GETCOUNT, 0, 0);
    if (count > 0)
    {
        if (iSource >= count)
        {
            --iSource;
        }
        SendMessage(hwndSourceLB, LB_SETCURSEL, iSource, 0);
    }

    AddItem(hwndDestinationLB, pDumperCfg, true);
}

void CPeViewerConfigDialog::MoveItem(HWND hwndLB, int nRelativeIndex)
{
    LRESULT iSel = SendMessage(hwndLB, LB_GETCURSEL, 0, 0);
    if (iSel == LB_ERR)
    {
        return;
    }

    int iNew = (int)iSel + nRelativeIndex;
    if (iNew < 0)
    {
        return;
    }

    LRESULT count = SendMessage(hwndLB, LB_GETCOUNT, 0, 0);
    if (iNew >= count)
    {
        iNew = -1;
    }

    const CFG_DUMPER* pDumperCfg = (const CFG_DUMPER*)SendMessage(hwndLB, LB_GETITEMDATA, iSel, 0);
    SendMessage(hwndLB, LB_DELETESTRING, iSel, 0);

    InsertItem(hwndLB, pDumperCfg, iNew, true);
}
