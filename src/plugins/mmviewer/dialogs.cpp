// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dialogs.h"
#include "output.h"
#include "renderer.h"
#include "mmviewer.h"

#include "mmviewer.rh"
#include "mmviewer.rh2"
#include "lang\lang.rh"

//****************************************************************************
//
// CCommonDialog
//

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, hParent, origin)
{
}

INT_PTR
CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        if (Parent != NULL)
            SalGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//****************************************************************************
//
// CAboutDialog
//

CAboutDialog::CAboutDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_ABOUT, hParent)
{
}

INT_PTR
CAboutDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CAboutDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // dosadime aktualni verzi
        char buff[1000];
        char buff2[1000];
        GetDlgItemText(HWindow, IDC_ABOUT_TITLE, buff, 1000);
        wsprintf(buff2, buff, VERSINFO_VERSION);
        SetDlgItemText(HWindow, IDC_ABOUT_TITLE, buff2);
        SetDlgItemText(HWindow, IDC_ABOUT_COPYRIGHT, VERSINFO_COPYRIGHT);
        // jmeno pluginu a verze budou tucne
        SalamanderGUI->AttachStaticText(HWindow, IDC_ABOUT_TITLE, STF_BOLD);
        SalamanderGUI->AttachStaticText(HWindow, IDC_ABOUT_DEPRO, STF_BOLD);

        SendDlgItemMessage(HWindow, IDC_MAIN_ICON, STM_SETIMAGE, IMAGE_ICON,
                           (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_MAIN)));
        SendDlgItemMessage(HWindow, IDC_DISKEXPLORER_ICON, STM_SETIMAGE, IMAGE_ICON,
                           (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_DISKEXPLORER)));

        CGUIHyperLinkAbstract* hl;
        hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_ABOUT_EMAIL, STF_UNDERLINE | STF_HYPERLINK_COLOR);
        if (hl != NULL)
            hl->SetActionOpen("mailto:tomas@tjelinek.com");

        hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_ABOUT_WWW, STF_UNDERLINE | STF_HYPERLINK_COLOR);
        if (hl != NULL)
            hl->SetActionOpen("http://www.tjelinek.com/");

        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
