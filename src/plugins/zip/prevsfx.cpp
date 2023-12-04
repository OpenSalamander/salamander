// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
//#include <stdio.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr\\comdefs.h"
#include "selfextr\\resource.h"
#include "config.h"
#include "typecons.h"
#include "chicon.h"
#include "common.h"
#include "prevsfx.h"

WNDPROC OrigLinkControlProc;

#define PGC_FONT RGB(0, 0, 192)

LRESULT CALLBACK LinkControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("LinkControlProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static HCURSOR hand;

    switch (uMsg)
    {
    case WM_PAINT:
    {
        RECT r;
        PAINTSTRUCT ps;
        char txt[MAX_PATH];
        DWORD c;
        UINT format = DT_SINGLELINE | DT_BOTTOM | DT_NOPREFIX;

        GetClientRect(hWnd, &r);
        BeginPaint(hWnd, &ps);
        HBRUSH DialogBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        if (DialogBrush)
        {
            FillRect(ps.hdc, &r, DialogBrush);
            DeleteObject(DialogBrush);
        }
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        LOGFONT lf;
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfUnderline = TRUE;
        hFont = CreateFontIndirect(&lf);
        c = PGC_FONT;
        format |= DT_RIGHT;

        HFONT hOldFont = (HFONT)SelectObject(ps.hdc, hFont);
        SetTextColor(ps.hdc, c);
        int prevBkMode = SetBkMode(ps.hdc, TRANSPARENT);
        int len = GetWindowText(hWnd, txt, MAX_PATH);
        DrawText(ps.hdc, txt, len, &r, format);
        SetBkMode(ps.hdc, prevBkMode);
        SelectObject(ps.hdc, hOldFont);
        DeleteObject(hFont);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(hand);
        return TRUE;

    case WM_LBUTTONDOWN:
    {
        char link[MAX_PATH];
        GetWindowText(hWnd, link, MAX_PATH);
        ShellExecute(hWnd, "open", link, NULL, NULL, SW_SHOWNORMAL);
        break;
    }

    case WM_USER_SETHCURSOR:
    {
        hand = (HCURSOR)wParam;
        return TRUE;
    }
    }
    return CallWindowProc(OrigLinkControlProc, hWnd, uMsg, wParam, lParam);
}

INT_PTR WINAPI SfxPreviewDlgProc(HWND dlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("SfxPreviewDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static bool processingCommand = false;
    static bool aboutShowed = false;
    static int dlgWinWidth;
    static int dlgWinAboutHeigth;
    static int dlgWinHeigth;
    static CPreviewInitData* data;

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGUI->ArrangeHorizontalLines(dlg);
        data = (CPreviewInitData*)lParam;
        LinkControlProc(GetDlgItem(dlg, IDC_WEBLINK), WM_USER_SETHCURSOR,
                        (WPARAM)LoadCursor(data->SfxHInstance, MAKEINTRESOURCE(IDCURSOR_HAND)), 0);
        OrigLinkControlProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(dlg, IDC_WEBLINK), GWLP_WNDPROC, (LONG_PTR)LinkControlProc);
        if (data->SmallIcon)
            SendMessage(dlg, WM_SETICON, ICON_BIG, (LPARAM)data->SmallIcon);
        if (data->LargeIcon)
            SendMessage(GetDlgItem(dlg, IDC_ANIMATION), STM_SETICON, (WPARAM)data->LargeIcon, 0);
        SetWindowText(dlg, data->Settings->Title);
        char path[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, path);
        SetDlgItemText(dlg, IDC_PATH, path);
        SetDlgItemText(dlg, IDOK, data->Settings->ExtractBtnText);
        SetDlgItemText(dlg, IDC_ABOUT, data->AboutButton1);
        SetDlgItemText(dlg, IDC_VENDOR, data->Settings->Vendor);
        SetDlgItemText(dlg, IDC_WEBLINK, data->Settings->WWW);
        ShowWindow(GetDlgItem(dlg, IDC_FILE), SW_HIDE);
        ShowWindow(GetDlgItem(dlg, IDC_PROGRESS), SW_HIDE);
        ShowWindow(GetDlgItem(dlg, IDC_FILENAME), SW_HIDE);
        SetDlgItemText(dlg, IDC_TEXT, data->Settings->Text);
        LONG style = GetWindowLong(GetDlgItem(dlg, IDC_WEBLINK), GWL_STYLE);
        if (style)
        {
            style = style | SS_NOTIFY;
            SetWindowLong(GetDlgItem(dlg, IDC_WEBLINK), GWL_STYLE, style);
        }
        SetDlgItemText(dlg, IDC_ABOUTTEXT, data->About);
        SalamanderGeneral->MultiMonCenterWindow(dlg, NULL, FALSE); // centrujeme k desktopu
        SetFocus(GetDlgItem(dlg, IDOK));

        RECT r;
        GetWindowRect(dlg, &r);
        dlgWinWidth = r.right - r.left;
        dlgWinAboutHeigth = r.bottom - r.top;
        RECT r2;
        GetWindowRect(GetDlgItem(dlg, IDC_SEPARATOR), &r2);
        // aplikace prelozena s novejsim platform toolset pracuje jinak s velikosti oken, vice viz
        // https://social.msdn.microsoft.com/Forums/vstudio/en-US/7ca548b5-8931-41dc-ac1d-ed9aed223d7a/different-dialog-box-position-and-size-with-visual-c-2012
        // https://connect.microsoft.com/VisualStudio/feedback/details/768135/different-dialog-box-size-and-position-when-compiled-in-visual-c-2012-vs-2010-2008
        // proto nefunguje puvodni reseni (ve vc2012+ je okno nizsi nez ve vc2010-): dlgWinHeigth = r2.top - r.top + 1;
        // reseni: dlgWinHeigth = plna vyska okna minus rozdil vysky client-area dialogu a umisteni separatoru
        //         v ramci client-area dialogu
        POINT p;
        p.x = r2.left;
        p.y = r2.top;
        ScreenToClient(dlg, &p);
        GetClientRect(dlg, &r2);
        dlgWinHeigth = dlgWinAboutHeigth - (r2.bottom - p.y) - 2; // -2 pro dosazeni puvodni vysky z vc2010-

        SetWindowPos(dlg, 0, 0, 0, dlgWinWidth, dlgWinHeigth, SWP_NOMOVE | SWP_NOZORDER);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return FALSE;

        case IDC_ABOUT:
            if (!processingCommand)
            {
                processingCommand = true;
                if (aboutShowed)
                {
                    SetWindowPos(dlg, 0, 0, 0, dlgWinWidth, dlgWinHeigth, SWP_NOMOVE | SWP_NOZORDER);
                    SetDlgItemText(dlg, IDC_ABOUT, data->AboutButton1);
                    aboutShowed = false;
                }
                else
                {
                    SetWindowPos(dlg, 0, 0, 0, dlgWinWidth, dlgWinAboutHeigth, SWP_NOMOVE | SWP_NOZORDER);
                    SetDlgItemText(dlg, IDC_ABOUT, data->AboutButton2);
                    aboutShowed = true;
                }
                processingCommand = false;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        SetWindowLongPtr(GetDlgItem(dlg, IDC_WEBLINK), GWLP_WNDPROC, (LONG_PTR)OrigLinkControlProc);
        return TRUE;
    }
    return FALSE;
}
