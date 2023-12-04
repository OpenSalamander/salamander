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

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

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
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc
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
// CConfigPageFirst
//

CConfigPageFirst::CConfigPageFirst()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGEFIRST, IDD_CFGPAGEFIRST, PSP_HASHELP, NULL)
{
}

void CConfigPageFirst::Validate(CTransferInfo& ti)
{
    int dummy;                          // jen testovaci hodnota (Validate se vola jen pri odchodu s okna pres OK)
    ti.EditLine(IDC_TESTNUMBER, dummy); // test jde-li o cislo
    if (ti.IsGood() && dummy >= 10)     // test neni-li cislo >= nez 10 (jen jde-li o cislo)
    {
        SalamanderGeneral->SalMessageBox(HWindow, "Number must be less then 10.", "Error",
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_TESTNUMBER);
        // PostMessage(GetDlgItem(HWindow, IDC_TESTNUMBER), EM_SETSEL, errorPos1, errorPos2);  // oznaceni pozice chyby
    }
}

void CConfigPageFirst::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDC_TESTSTRING, Str, MAX_PATH);
    ti.EditLine(IDC_TESTNUMBER, Number);

    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_COMBO))
    {
        if (ti.Type == ttDataToWindow) // Transfer() volany pri otevirani okna (data -> okno)
        {
            SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
            SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM) "first");
            SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM) "second");
            SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM) "third");
            SendMessage(hWnd, CB_SETCURSEL, Selection, 0);
        }
        else // ttDataFromWindow; Transfer() volany pri stisku OK (okno -> data)
        {
            Selection = (int)SendMessage(hWnd, CB_GETCURSEL, 0, 0);
        }
    }
}

//
// ****************************************************************************
// CConfigPageSecond
//

CConfigPageSecond::CConfigPageSecond()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGESECOND, 0, NULL) // second config page has no help
{
}

void CConfigPageSecond::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_CHECK, CheckBox);

    ti.RadioButton(IDC_RADIO1, 10, RadioBox);
    ti.RadioButton(IDC_RADIO2, 13, RadioBox);
    ti.RadioButton(IDC_RADIO3, 20, RadioBox);
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
            {
                HWND hParent = GetParent(HWindow);
                if (hParent != NULL)
                    SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);
            }
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

CConfigDialog::CConfigDialog(HWND parent)
    : CPropertyDialog(parent, HLanguage, LoadStr(IDS_CFG_TITLE),
                      LastCfgPage, PSH_USECALLBACK | PSH_NOAPPLYNOW | PSH_HASHELP,
                      NULL, &LastCfgPage, CenterCallback)
{
    Add(&PageFirst);
    Add(&PageSecond);
    Add(&PageViewer);
}

//
// ****************************************************************************
// CPathDialog
//

CPathDialog::CPathDialog(HWND parent, char* path, BOOL* filePath)
    : CCommonDialog(HLanguage, IDD_PATHDLG, IDD_PATHDLG, parent)
{
    Path = path;
    FilePath = filePath;
}

void CPathDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDC_PATHSTRING, Path, MAX_PATH);
    ti.CheckBox(IDC_FILECHECK, *FilePath);
}

INT_PTR
CPathDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPathDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        /*
      SetFocus(GetDlgItem(HWindow, IDOK));  // chceme svuj vlastni fokus
      CDialog::DialogProc(uMsg, wParam, lParam);
      return FALSE;
*/
        break; // chci focus od DefDlgProc
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CToolTipExample
//

class CToolTipExample : public CWindow
{
public:
    CToolTipExample(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_MOUSEMOVE:
        {
            DWORD mousePos = GetMessagePos();
            POINT p;
            p.x = GET_X_LPARAM(mousePos);
            p.y = GET_Y_LPARAM(mousePos);

            BOOL hit = (WindowFromPoint(p) == HWindow);

            if (GetCapture() == HWindow)
            {
                if (!hit)
                {
                    ReleaseCapture();
                }
            }
            else
            {
                if (hit)
                {
                    SalamanderGUI->SetCurrentToolTip(HWindow, 0);
                    SetCapture(HWindow);
                }
            }
            break;
        }

        case WM_CAPTURECHANGED:
        {
            SalamanderGUI->SetCurrentToolTip(NULL, 0);
            break;
        }

        case WM_USER_TTGETTEXT:
        {
            DWORD id = (DWORD)wParam;
            char* text = (char*)lParam;
            lstrcpyn(text, "ToolTip", TOOLTIP_TEXT_MAX);
            return 0;
        }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

//****************************************************************************
//
// CCtrlExampleDialog
//

CCtrlExampleDialog::CCtrlExampleDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_CTRLEXAMPLE, hParent)
{
    TimerStarted = FALSE;
    StringTemplate[0] = 0;
    Text = NULL;
    CachedText = NULL;
    Progress = NULL;
    ProgressNumber = 0;
    LastTickCount = 0;
}

BOOL CCtrlExampleDialog::CreateChilds()
{
    CGUIStaticTextAbstract* text;

    Text = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STNONE, 0);
    if (Text == NULL)
        return FALSE;

    CachedText = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STCACHE, STF_CACHED_PAINT);
    if (CachedText == NULL)
        return FALSE;

    text = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STBOLD, STF_BOLD | STF_HANDLEPREFIX);
    if (text == NULL)
        return FALSE;

    text = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STUNDERLINE, STF_UNDERLINE);
    if (text == NULL)
        return FALSE;

    text = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STEND, STF_END_ELLIPSIS);
    if (text == NULL)
        return FALSE;

    text = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STPATH, STF_PATH_ELLIPSIS);
    if (text == NULL)
        return FALSE;

    text = SalamanderGUI->AttachStaticText(HWindow, IDC_CE_STPATH2, STF_PATH_ELLIPSIS);
    if (text == NULL)
        return FALSE;
    text->SetPathSeparator('/');

    CGUIHyperLinkAbstract* hl;

    // HyperLink lze v dialogu navstivit z klavesnice, pokud mu priradime v .RC styl WS_TABSTOP.
    hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_CE_HLOPEN, STF_UNDERLINE | STF_HYPERLINK_COLOR);
    if (hl == NULL)
        return FALSE;
    hl->SetActionOpen("https://www.altap.cz");

    hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_CE_HLCOMMAND, STF_UNDERLINE | STF_HYPERLINK_COLOR);
    if (hl == NULL)
        return FALSE;
    hl->SetActionPostCommand(CM_POSTEDCOMMAND);

    hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_CE_HLHINT, STF_DOTUNDERLINE);
    if (hl == NULL)
        return FALSE;
    hl->SetActionShowHint("text 1 text 1 text 1 text 1\ntext 2 text 2 text 2 ");

    Progress = SalamanderGUI->AttachProgressBar(HWindow, IDC_CE_PROGRESS);
    if (Progress == NULL)
        return FALSE;

    Progress2 = SalamanderGUI->AttachProgressBar(HWindow, IDC_CE_PROGRESS2);
    if (Progress2 == NULL)
        return FALSE;

    SalamanderGUI->ChangeToArrowButton(HWindow, IDC_CE_PB);

    if (!SalamanderGUI->AttachButton(HWindow, IDC_CE_PBTEXT, BTF_RIGHTARROW))
        return FALSE;

    if (!SalamanderGUI->AttachButton(HWindow, IDC_CE_PBDROP, BTF_DROPDOWN))
        return FALSE;

    CGUIColorArrowButtonAbstract* colorArrowButton;
    colorArrowButton = SalamanderGUI->AttachColorArrowButton(HWindow, IDC_CE_PBCOLOR, TRUE);
    if (colorArrowButton == NULL)
        return FALSE;
    colorArrowButton->SetColor(RGB(0, 128, 255), RGB(0, 128, 255));

    colorArrowButton = SalamanderGUI->AttachColorArrowButton(HWindow, IDC_CE_PBCOLOR2, TRUE);
    if (colorArrowButton == NULL)
        return FALSE;
    colorArrowButton->SetColor(RGB(0, 0, 0), RGB(255, 255, 0));

    new CToolTipExample(HWindow, IDC_CE_TOOLTIP);

    CGUIToolbarHeaderAbstract* toolbarHeader = NULL;
    toolbarHeader = SalamanderGUI->AttachToolbarHeader(HWindow, IDC_LIST_HEADER, GetDlgItem(HWindow, IDC_LIST), TLBHDRMASK_MODIFY | TLBHDRMASK_UP | TLBHDRMASK_DOWN);
    if (toolbarHeader != NULL)
    {
        //    toolbarHeader->EnableToolbar(TLBHDRMASK_UP | TLBHDRMASK_DOWN);
        //    toolbarHeader->CheckToolbar(TLBHDRMASK_UP);
    }

    return TRUE;
}

INT_PTR
CCtrlExampleDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPathDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (!CreateChilds())
        {
            DestroyWindow(HWindow); // chyba -> neotevreme dialog
            return FALSE;           // konec zpracovani
        }
        GetDlgItemText(HWindow, IDC_CE_ST, StringTemplate, 300);
        TimerStarted = SetTimer(HWindow, 1, 20, NULL) != 0;
        //Progress2->SetProgress(450, NULL);

        CQuadWord current(123456, 0);
        CQuadWord total(543267, 0);
        Progress2->SetProgress2(current, total, NULL);

        //Progress2->SetSelfMoveTime(0); // progress se posune pri kazdem WM_TIMER
        Progress2->SetSelfMoveTime(1000); // samovolne posouvani po jednu vterinu
        Progress2->SetSelfMoveSpeed(100); // rychlost: 10 pohybu za vterinu
        break;
    }

    case WM_TIMER:
    {
        Progress2->SetProgress(-1, NULL);
        char buff[300];
        DWORD ticks = GetTickCount();
        wsprintf(buff, StringTemplate, ticks);
        int i;
        for (i = 0; i < 50; i++) // zduraznime blikaci efekt
        {
            SetDlgItemText(HWindow, IDC_CE_ST, buff);
            Text->SetText(buff);
            CachedText->SetText(buff);
        }

        ProgressNumber += 1;

        // progress bar aktualizujeme kazdych 100ms
        if (ticks - LastTickCount > 100)
        {
            LastTickCount = ticks;

            if (ProgressNumber > 1200) // 0..1000 se zvetsuje; 1001 - 1200 ceka na 100%
            {
                ProgressNumber = 0;
            }
            Progress->SetProgress(ProgressNumber, NULL);

            // unknown progress, posouvame obdelnik zleva doprava a zpet
            //        Progress2->SetProgress(-1);
        }

        break;
    }

    case WM_DESTROY:
    {
        if (TimerStarted)
            KillTimer(HWindow, 1);
        break;
    }

    case WM_LBUTTONDOWN:
    {
        SetCapture(HWindow);
        break;
    }

    case WM_LBUTTONUP:
    {
        ReleaseCapture();
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (GetCapture() == HWindow)
        {
            DWORD pos = GetMessagePos();
            short xPos = GET_X_LPARAM(pos);

            HWND hWnd = GetDlgItem(HWindow, IDC_CE_STNONE);
            RECT r;
            GetWindowRect(hWnd, &r);
            if (xPos >= r.left && xPos < r.right)
                r.right = xPos;
            int width = r.right - r.left;
            int height = r.bottom - r.top;

            SetWindowPos(GetDlgItem(HWindow, IDC_CE_STEND), NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);
            SetWindowPos(GetDlgItem(HWindow, IDC_CE_STPATH), NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);
            SetWindowPos(GetDlgItem(HWindow, IDC_CE_STPATH2), NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);
        }
        break;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
        if (popup != NULL)
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);

            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
            mii.Type = MENU_TYPE_STRING;

            char buffDrop[] = "Drop";
            mii.String = buffDrop;
            mii.ID = 1;
            popup->InsertItem(-1, TRUE, &mii);

            char buffDropSpecial[] = "Drop Special";
            mii.String = buffDropSpecial;
            mii.ID = 2;
            popup->InsertItem(-1, TRUE, &mii);

            // using salamander popup menu
            //popup->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
            //             r.left, r.bottom, HWindow, &r);

            // using windows popup menu
            HMENU hMenu = CreatePopupMenu();
            popup->FillMenuHandle(hMenu);
            TPMPARAMS tpm;
            tpm.cbSize = sizeof(tpm);
            tpm.rcExclude = r;
            DWORD flags = TPM_RETURNCMD | TPM_LEFTALIGN;
            if (LOWORD(wParam) == IDC_CE_PB)
                flags |= TPM_HORIZONTAL;
            else
                flags |= TPM_VERTICAL;
            TrackPopupMenuEx(hMenu, flags, r.left, r.bottom, HWindow, &tpm);
            DestroyMenu(hMenu);

            SalamanderGUI->DestroyMenuPopup(popup);
        }

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case CM_POSTEDCOMMAND:
        {
            SalamanderGeneral->SalMessageBox(HWindow, "SOMETHING", "Demo plugin",
                                             MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case IDC_CE_PBCOLOR2:
        case IDC_CE_PBCOLOR:
        case IDC_CE_PBTEXT:
        case IDC_CE_PB:
        {
            CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
            if (popup != NULL)
            {
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
                mii.Type = MENU_TYPE_STRING;

                char buffItem1[] = "Item 1";
                mii.String = buffItem1;
                mii.ID = 1;
                popup->InsertItem(-1, TRUE, &mii);

                char buffItem2[] = "Item xxxxx 2";
                mii.String = buffItem2;
                mii.ID = 2;
                popup->InsertItem(-1, TRUE, &mii);

                char buffItem3[] = "Item 3";
                mii.String = buffItem3;
                mii.ID = 3;
                popup->InsertItem(-1, TRUE, &mii);

                RECT r;
                GetWindowRect(GetDlgItem(HWindow, LOWORD(wParam)), &r);

                DWORD cmd;
                if (LOWORD(wParam) == IDC_CE_PB || LOWORD(wParam) == IDC_CE_PBTEXT)
                {
                    // pouzijeme standardni menu
                    HMENU hMenu = CreatePopupMenu();
                    popup->FillMenuHandle(hMenu);
                    TPMPARAMS tpm;
                    tpm.cbSize = sizeof(tpm);
                    tpm.rcExclude = r;
                    DWORD flags = TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN;
                    if (LOWORD(wParam) == IDC_CE_PB)
                        flags |= TPM_HORIZONTAL;
                    else
                        flags |= TPM_VERTICAL;
                    cmd = TrackPopupMenuEx(hMenu, flags, r.right, r.top, HWindow, &tpm);
                    DestroyMenu(hMenu);
                }
                else
                {
                    // pouzijeme nase menu
                    DWORD flags = MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON /*| MENU_TRACK_LEFTALIGN*/;
                    if (LOWORD(wParam) == IDC_CE_PBCOLOR)
                        flags |= /*MENU_TRACK_HORIZONTAL*/ 0;
                    else
                        flags |= MENU_TRACK_VERTICAL;
                    cmd = popup->Track(flags, r.right, r.top, HWindow, &r);
                }
                if (cmd != 0)
                {
                    // do something
                }
                SalamanderGUI->DestroyMenuPopup(popup);
            }
            return 0;
        }

        case IDC_LIST_HEADER:
        {
            if (GetFocus() != GetDlgItem(HWindow, IDC_LIST))
                SetFocus(GetDlgItem(HWindow, IDC_LIST));
            switch (HIWORD(wParam))
            {
            case TLBHDR_MODIFY:
                MessageBox(HWindow, "Modify", "ToolbarHeader", MB_OK);
                break;
            case TLBHDR_UP:
                MessageBox(HWindow, "Up", "ToolbarHeader", MB_OK);
                break;
            case TLBHDR_DOWN:
                MessageBox(HWindow, "Down", "ToolbarHeader", MB_OK);
                break;
            }
            return 0;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
