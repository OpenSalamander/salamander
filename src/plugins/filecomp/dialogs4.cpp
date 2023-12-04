// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

DWORD LastCfgPage;

// ****************************************************************************
//
// CPropPageGeneral
//

CPropPageGeneral::CPropPageGeneral(CConfiguration* configuration, DWORD* changeFlag)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGGENERAL, IDD_CFGGENERAL, PSP_HASHELP, NULL)
{
    CALL_STACK_MESSAGE1("CPropPageGeneral::CPropPageGeneral(, , , , )");
    HFont = NULL;
    Configuration = configuration;
    LogFont = Configuration->FileViewLogFont;
    UseViewerFont = Configuration->UseViewerFont;
    ChangeFlag = changeFlag;
}

CPropPageGeneral::~CPropPageGeneral()
{
    CALL_STACK_MESSAGE1("CPropPageGeneral::~CPropPageGeneral()");
    if (HFont)
        DeleteObject(HFont);
}

void CPropPageGeneral::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPropPageGeneral::Validate()");
    int i;
    ti.EditLine(IDE_CONTEXT, i);
    if (i < 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_POSITIVEVALUE), LoadStr(IDS_ERROR), MB_ICONERROR);
        ti.ErrorOn(IDE_CONTEXT);
        return;
    }
    ti.EditLine(IDE_TABSIZE, i);
    if (i <= 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_POSITIVEVALUE), LoadStr(IDS_ERROR), MB_ICONERROR);
        ti.ErrorOn(IDE_TABSIZE);
        return;
    }
}

void CPropPageGeneral::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPropPageGeneral::Transfer()");
    CConfiguration oldCfg = *Configuration;
    ti.CheckBox(IDC_CONFIRM, Configuration->ConfirmSelection);
    ti.EditLine(IDE_CONTEXT, Configuration->Context);
    ti.EditLine(IDE_TABSIZE, Configuration->TabSize);
    ti.CheckBox(IDC_DETAILDIFFS, Configuration->DetailedDifferences);
    ti.CheckBox(IDC_VIEW_HORIZONTAL, Configuration->HorizontalView);
    ti.CheckBox(IDC_LOADONSTART, LoadOnStart);
    HWND whiteSpace = GetDlgItem(HWindow, IDC_WHITESPACE);
    if (ti.Type == ttDataFromWindow)
    {
        if (memcmp(&Configuration->FileViewLogFont, &LogFont, sizeof(LogFont)) != 0 ||
            Configuration->UseViewerFont != UseViewerFont)
        {
            Configuration->FileViewLogFont = LogFont;
            Configuration->UseViewerFont = UseViewerFont;
            *ChangeFlag |= CC_FONT;
        }
        Configuration->WhiteSpace = (unsigned char)SendMessage(whiteSpace, CB_GETCURSEL, 0, 0) + 1;
        if (oldCfg.Context != Configuration->Context)
            *ChangeFlag |= CC_CONTEXT;
        if (oldCfg.TabSize != Configuration->TabSize)
            *ChangeFlag |= CC_TABSIZE;
        if (oldCfg.WhiteSpace != Configuration->WhiteSpace)
            *ChangeFlag |= CC_CHAR;
    }
    else
    {
        SendMessage(whiteSpace, CB_SETCURSEL, (int)Configuration->WhiteSpace - 1, 0);
    }
}

void CPropPageGeneral::LoadControls(BOOL initCombo)
{
    CALL_STACK_MESSAGE2("CPropPageGeneral::LoadControls(%d)", initCombo);

    // inicializuje edit linu pro ukazku fontu
    HWND hEdit = GetDlgItem(HWindow, IDE_FONT);
    LOGFONT logFont;
    logFont = LogFont;
    logFont.lfHeight = SalGUI->GetWindowFontHeight(hEdit); // pro prezentaci fontu v edit line pouzijeme jeji velikost fontu
    if (HFont)
        DeleteObject(HFont);
    HFont = CreateFontIndirect(&logFont);

    HDC hDC = GetDC(HWindow);

    SendMessage(hEdit, WM_SETFONT, (WPARAM)HFont, MAKELPARAM(TRUE, 0));
    sprintf(logFont.lfFaceName, LoadStr(IDS_FONTDESCRIPTION),
            MulDiv(-LogFont.lfHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY)),
            LogFont.lfFaceName);
    SetWindowText(hEdit, logFont.lfFaceName);

    // inicializuje comba pro vyber znaku
    HWND whiteSpace = GetDlgItem(HWindow, IDC_WHITESPACE);
    SendMessage(whiteSpace, WM_SETFONT, (WPARAM)HFont, FALSE);
    if (initCombo)
    {
        SendMessage(whiteSpace, WM_SETREDRAW, FALSE, 0);
        SendMessage(whiteSpace, CB_RESETCONTENT, 0, 0);
        char buf[20];
        int i;
        for (i = 1; i < 256; i++)
        {
            sprintf(buf, "%-3d - %c", i, (char)i);
            SendMessage(whiteSpace, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        SendMessage(whiteSpace, WM_SETREDRAW, TRUE, 0);
    }

    ReleaseDC(HWindow, hDC);
}

INT_PTR
CPropPageGeneral::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPropPageGeneral::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalGUI->AttachButton(HWindow, IDB_FONT, BTF_RIGHTARROW);
        LoadControls(TRUE);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_FONT:
        {
            HMENU hMenu, hSubMenu;

            hMenu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_CTX_FONT_MENU));
            hSubMenu = GetSubMenu(hMenu, 0);
            CheckMenuItem(hSubMenu, CM_CUSTOMFONT, MF_BYCOMMAND | (UseViewerFont ? MF_UNCHECKED : MF_CHECKED));
            CheckMenuItem(hSubMenu, CM_AUTOFONT, MF_BYCOMMAND | (UseViewerFont ? MF_CHECKED : MF_UNCHECKED));

            RECT r;
            GetWindowRect(GetDlgItem(HWindow, IDB_FONT), &r);

            TPMPARAMS tpmPar;
            tpmPar.cbSize = sizeof(tpmPar);
            tpmPar.rcExclude = r;
            switch (TrackPopupMenuEx(hSubMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, r.right, r.top, HWindow, &tpmPar))
            {
            case CM_CUSTOMFONT:
            {
                CHOOSEFONT cf;
                LOGFONT logFont = LogFont;
                memset(&cf, 0, sizeof(CHOOSEFONT));
                cf.lStructSize = sizeof(CHOOSEFONT);
                cf.hwndOwner = HWindow;
                cf.hDC = NULL;
                cf.lpLogFont = &logFont;
                cf.iPointSize = 10;
                cf.Flags = CF_NOVERTFONTS | CF_FIXEDPITCHONLY | CF_SCREENFONTS |
                           CF_INITTOLOGFONTSTRUCT | CF_ENABLEHOOK;
                cf.lpfnHook = ComDlgHookProc;
                if (ChooseFont(&cf) != 0)
                {
                    LogFont = logFont;
                    UseViewerFont = FALSE;
                }
                break;
            }

            case CM_AUTOFONT:
            {
                LogFont = Configuration->InternalViewerFont;
                UseViewerFont = TRUE;
                break;
            }
            }
            DestroyMenu(hMenu);
            LoadControls(FALSE);
            return 0;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CConfigurationDialog
//

// pomocny objekt pro centrovani konfiguracniho dialogu k parentovi
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
                CenterWindow(HWindow);
            break;
        }

        case WM_APP + 1000: // mame se odpojit od dialogu (uz je vycentrovano)
        {
            DetachWindow();
            delete this; // trochu prasarna, ale uz se 'this' nikdo ani nedotkne, takze pohoda
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

// pomocny call-back pro centrovani konfiguracniho dialogu k parentovi a vyhozeni '?' buttonku z captionu
int CALLBACK CenterCallback(HWND HWindow, UINT uMsg, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CenterCallback(, 0x%X, 0x%IX)", uMsg, lParam);
    if (uMsg == PSCB_INITIALIZED) // pripojime se na dialog
    {
        CCenteredPropertyWindow* wnd = new CCenteredPropertyWindow;
        if (wnd != NULL)
        {
            wnd->AttachToWindow(HWindow);
            if (wnd->HWindow == NULL)
                delete wnd; // okno neni pripojeny, zrusime ho uz tady
            else
            {
                PostMessage(wnd->HWindow, WM_APP + 1000, 0, 0); // pro odpojeni CCenteredPropertyWindow od dialogu
            }
        }
    }
    if (uMsg == PSCB_PRECREATE) // odstraneni '?' buttonku z headeru property sheetu
    {
        // Remove the DS_CONTEXTHELP style from the dialog box template
        if (((LPDLGTEMPLATEEX)lParam)->signature == 0xFFFF)
            ((LPDLGTEMPLATEEX)lParam)->style &= ~DS_CONTEXTHELP;
        else
            ((LPDLGTEMPLATE)lParam)->style &= ~DS_CONTEXTHELP;
    }
    return 0;
}

CConfigurationDialog::CConfigurationDialog(HWND parent, CConfiguration* configuration,
                                           SALCOLOR* colors, CCompareOptions* options, DWORD* changeFlag)
    : CPropertyDialog(parent, HLanguage, LoadStr(IDS_CONFIGURATIONTITLE),
                      LastCfgPage, PSH_HASHELP | PSH_NOAPPLYNOW | PSH_USECALLBACK, NULL, &LastCfgPage,
                      CenterCallback),
      Page1(configuration, changeFlag),
      Page2(colors, changeFlag, &Page1),
      Page3(options, changeFlag)
{
    CALL_STACK_MESSAGE1("CConfigurationDialog::CConfigurationDialog(, , , , )");
    Add(&Page1); // Genereal
    Add(&Page2); // Default Compare Options
    Add(&Page3); // Colors

    *changeFlag = 0;
}
