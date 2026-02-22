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

//****************************************************************************
//
// CCommonDialog
//

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, hParent, origin)
{
}

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, helpID, hParent, origin)
{
}

INT_PTR
CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // horizontally and vertically center the dialog relative to the parent
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // want focus from DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//
// ****************************************************************************
// CCommonPropSheetPage
//

void CCommonPropSheetPage::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//
// ****************************************************************************
// CConfigPageViewer
//

CConfigPageViewer::CConfigPageViewer()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGEVIEWER, IDD_CFGPAGEVIEWER, PSP_HASHELP, NULL)
{
}

void CConfigPageViewer::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_CFG_SAVEPOSONCLOSE, 1, CfgSavePosition);
    ti.RadioButton(IDC_CFG_SETBYMAINWINDOW, 0, CfgSavePosition);
}

//
// ****************************************************************************
// CConfigDialog
//

// helper object for centering the configuration dialog to the parent
class CCenteredPropertyWindow : public CWindow
{
protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_WINDOWPOSCHANGING:
        {
            WINDOWPOS* pos = (WINDOWPOS*)lParam;
            if (pos->flags & SWP_SHOWWINDOW)
            {
                HWND hParent = GetParent(HWindow);
                if (hParent != NULL)
                    SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);
            }
            break;
        }

        case WM_APP + 1000: // we should detach from the dialog (already centered)
        {
            DetachWindow();
            delete this; // a bit of a hack, but nobody will touch 'this' anymore, so it's fine
            return 0;
        }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

#ifndef LPDLGTEMPLATEEX
#include <pshpack1.h>
typedef struct DLGTEMPLATEEX
{
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x;
    short y;
    short cx;
    short cy;
} DLGTEMPLATEEX, *LPDLGTEMPLATEEX;
#include <poppack.h>
#endif // LPDLGTEMPLATEEX

// helper callback for centering the configuration dialog to the parent and removing the '?' button from the caption
int CALLBACK CenterCallback(HWND HWindow, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED) // attach to the dialog
    {
        CCenteredPropertyWindow* wnd = new CCenteredPropertyWindow;
        if (wnd != NULL)
        {
            wnd->AttachToWindow(HWindow);
            if (wnd->HWindow == NULL)
                delete wnd; // window is not attached, destroy it right here
            else
            {
                PostMessage(wnd->HWindow, WM_APP + 1000, 0, 0); // to detach CCenteredPropertyWindow from the dialog
            }
        }
    }
    if (uMsg == PSCB_PRECREATE) // remove the '?' button from the property sheet header
    {
        // Remove the DS_CONTEXTHELP style from the dialog box template
        if (((LPDLGTEMPLATEEX)lParam)->signature == 0xFFFF)
            ((LPDLGTEMPLATEEX)lParam)->style &= ~DS_CONTEXTHELP;
        else
            ((LPDLGTEMPLATE)lParam)->style &= ~DS_CONTEXTHELP;
    }
    return 0;
}

CConfigDialog::CConfigDialog(HWND parent)
    : CPropertyDialog(parent, HLanguage, LoadStr(IDS_CFG_TITLE),
                      LastCfgPage, PSH_USECALLBACK | PSH_NOAPPLYNOW | PSH_HASHELP,
                      NULL, &LastCfgPage, CenterCallback)
{
    Add(&PageViewer);
}
