// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dialogimpl.h"

extern HINSTANCE g_hLangInst;

static INT_PTR CALLBACK DialogProc(
    HWND hDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    CDialogImpl* dlg;
    INT_PTR res = FALSE;

    if (uMsg == WM_INITDIALOG)
    {
        ASSERT(lParam);
        dlg = (CDialogImpl*)lParam;
        dlg->m_hWnd = hDlg;
#pragma warning(push)
#pragma warning(disable : 4244)
        SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)lParam);
#pragma warning(pop)
    }
    else
    {
#pragma warning(push)
#pragma warning(disable : 4312)
        dlg = reinterpret_cast<CDialogImpl*>(GetWindowLongPtr(hDlg, DWLP_USER));
#pragma warning(pop)
    }

    if (dlg)
        res = dlg->DialogProc(uMsg, wParam, lParam);

    if (uMsg == WM_NCDESTROY)
    {
        SetWindowLongPtr(hDlg, DWLP_USER, NULL);
    }

    return res;
}

INT_PTR CDialogImpl::DoModal(int nIdResourceTemplate, HWND hwndParent)
{
    m_bModal = true;
    return ::DialogBoxParam(g_hLangInst, MAKEINTRESOURCE(nIdResourceTemplate),
                            hwndParent, ::DialogProc, (LPARAM)this);
}

HWND CDialogImpl::Create(int nIdResourceTemplate, HWND hwndParent)
{
    m_bModal = false;
    return ::CreateDialogParam(g_hLangInst, MAKEINTRESOURCE(nIdResourceTemplate),
                               hwndParent, ::DialogProc, (LPARAM)this);
}
