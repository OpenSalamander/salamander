// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>
#include <tchar.h>

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr/comdefs.h"
#include "config.h"
#include "typecons.h"
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"
#include "chicon.h"
#include "common.h"
#include "add_del.h"
#include "dialogs.h"

WNDPROC OrigTextControlProc;
WNDPROC OrigCBEditCtrlProc;
WNDPROC OrigSmallIconProc;

TIndirectArray2<CSfxLang>* SfxLanguages = NULL;
CSfxLang* DefLanguage = NULL;

LRESULT CALLBACK TextControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("TextControlProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    switch (uMsg)
    {
    case WM_PAINT:
    {
        RECT r;
        PAINTSTRUCT ps;
        char txt[MAX_PATH];

        GetClientRect(hWnd, &r);
        BeginPaint(hWnd, &ps);
        HBRUSH DialogBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        if (DialogBrush)
        {
            FillRect(ps.hdc, &r, DialogBrush);
            DeleteObject(DialogBrush);
        }
        UINT format = DT_SINGLELINE | DT_BOTTOM | DT_NOPREFIX;
        DWORD color = GetSysColor(COLOR_BTNTEXT);
        HFONT hCurrentFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        int ID = GetDlgCtrlID(hWnd);
        format |= DT_PATH_ELLIPSIS;
        HFONT hOldFont = (HFONT)SelectObject(ps.hdc, hCurrentFont);
        SetTextColor(ps.hdc, color);
        int prevBkMode = SetBkMode(ps.hdc, TRANSPARENT);
        int len = GetWindowText(hWnd, txt, MAX_PATH);
        DrawText(ps.hdc, txt, lstrlen(txt), &r, format);
        SetBkMode(ps.hdc, prevBkMode);
        SelectObject(ps.hdc, hOldFont);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return FALSE;
    }
    return CallWindowProc(OrigTextControlProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK SmallIconProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("SmallIconProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_PAINT:
    {
        HICON icon = (HICON)SendMessage(GetParent(hWnd), WM_USER_GETICON, (WPARAM)hWnd, 0);

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT r;
        GetClientRect(hWnd, &r);

        HBRUSH DialogBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        if (DialogBrush)
        {
            FillRect(ps.hdc, &r, DialogBrush);
            DeleteObject(DialogBrush);
        }
        HRGN region = CreateRectRgn(r.left, r.top, r.right, r.bottom);
        if (region != NULL)
        {
            SelectClipRgn(ps.hdc, region);
        }
        if (icon)
            DrawIconEx(hdc, (r.right - 16) / 2, (r.bottom - 16) / 2, icon, 0, 0, 0, NULL, DI_NORMAL);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return CallWindowProc(OrigSmallIconProc, hWnd, uMsg, wParam, lParam);
}

//CDlgRoot

void CDlgRoot::CenterDlgToParent()
{
    CALL_STACK_MESSAGE1("CDlgRoot::CenterDlgToParent()");
    HWND hParent = GetParent(Dlg);
    if (hParent != NULL)
        SalamanderGeneral->MultiMonCenterWindow(Dlg, hParent, TRUE);
}

void CDlgRoot::SubClassStatic(DWORD wID, bool subclass)
{
    CALL_STACK_MESSAGE3("CDlgRoot::SubClassStatic(0x%X, %d)", wID, subclass);
    if (subclass)
        OrigTextControlProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LONG_PTR)TextControlProc);
    else
        SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LONG_PTR)OrigTextControlProc);
}

void CDlgRoot::SubClassSmallIcon(DWORD wID, bool subclass)
{
    CALL_STACK_MESSAGE3("CDlgRoot::SubClassSmallIcon(0x%X, %d)", wID, subclass);
    if (subclass)
        OrigSmallIconProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LONG_PTR)SmallIconProc);
    else
        SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LONG_PTR)OrigSmallIconProc);
}

//CPackDialog

LRESULT CALLBACK CBEditCtrlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CBEditCtrlProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_SETTEXT:
    {
        char* c = strchr((char*)lParam, ' ');
        if (c)
            *c = 0;
    }
    }
    return CallWindowProc(OrigCBEditCtrlProc, hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
    CALL_STACK_MESSAGE_NONE
    *(HWND*)lParam = hwnd;
    return FALSE;
}

void CPackDialog::SubClassComboBox(DWORD wID, bool subclass)
{
    CALL_STACK_MESSAGE3("CPackDialog::SubClassComboBox(0x%X, %d)", wID, subclass);
    //CBEditCtrl = GetDlgItem(GetDlgItem(Dlg, wID), 0x03E9);
    if (subclass)
    {
        HWND cb = GetDlgItem(Dlg, wID);
        if (cb)
            EnumChildWindows(cb, EnumChildProc, (LPARAM)&CBEditCtrl);
        if (CBEditCtrl)
        {
            OrigCBEditCtrlProc = (WNDPROC)SetWindowLongPtr(CBEditCtrl, GWLP_WNDPROC, (LONG_PTR)CBEditCtrlProc);
        }
        //SetWindowPos(Dlg, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    else
    {
        if (CBEditCtrl)
        {
            SetWindowLongPtr(CBEditCtrl, GWLP_WNDPROC, (LONG_PTR)OrigCBEditCtrlProc);
        }
    }
}

INT_PTR WINAPI PackDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("PackDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static CPackDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CPackDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CPackDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CPackDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_EXPACKOPTIONS),
                          Parent, PackDlgProc, (LPARAM)this);
}

INT_PTR CPackDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPackDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return FALSE;

        case IDC_MULTIVOL:
            return OnMultiVol(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_SEQNAME:
            return OnSeqName(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_SELFEXTR:
            return OnSelfExtr(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_ENCRYPT:
            return OnEncrypt(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_VOLSIZE:
            return OnVolSize(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_ADVANCED:
            return OnAdvanced(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_CONFIG:
            return OnConfig(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_EXPACKOPTIONS, FALSE);
            return TRUE;
        }
        break;

    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_EXPACKOPTIONS, FALSE);
        return TRUE;

    case WM_DESTROY:
        SubClassStatic(IDC_ARCHIVE, false);
        SubClassComboBox(IDC_VOLSIZE, false);
        break;

    case WM_USER_SETVOLSIZE:
        return SetVolSize(wParam, lParam);
    }
    return FALSE;
}

BOOL CPackDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    int i;
    char buf[MAX_VOL_STR + 4];

    SubClassStatic(IDC_ARCHIVE, true);
    SubClassComboBox(IDC_VOLSIZE, true);

    if ((DecimalSeparatorLen = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, DecimalSeparator, 5)) == 0 ||
        DecimalSeparatorLen > 5)
    {
        strcpy(DecimalSeparator, ".");
        DecimalSeparatorLen = 1;
    }
    else
    {
        DecimalSeparatorLen--;
        DecimalSeparator[DecimalSeparatorLen] = 0; // posychrujeme nulu na konci
    }

    SendDlgItemMessage(Dlg, IDC_UNITS, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SIZE_KB));
    SendDlgItemMessage(Dlg, IDC_UNITS, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SIZE_MB));
    for (i = 0; i < 5 && Config->VolSizeCache[i][0] != 0; i++)
    {
        sprintf(buf, "%s %s", Config->VolSizeCache[i], LoadStr(Config->VolSizeUnits[i] == 0 ? IDS_SIZE_KB : IDS_SIZE_MB));
        SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_AUTO));

    SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_LIMITTEXT, MAX_VOL_STR - 1, 0);
    SendDlgItemMessage(Dlg, IDC_PASSWORD1, EM_SETLIMITTEXT, MAX_PASSWORD - 1, 0);
    SendDlgItemMessage(Dlg, IDC_PASSWORD2, EM_SETLIMITTEXT, MAX_PASSWORD - 1, 0);

    if (Config->EncryptMethod == EM_ZIP20)
        SendDlgItemMessage(Dlg, IDC_ENC_ZIP20, BM_SETCHECK, BST_CHECKED, 0);
    else if (Config->EncryptMethod == EM_AES128)
        SendDlgItemMessage(Dlg, IDC_ENC_AES128, BM_SETCHECK, BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_ENC_AES256, BM_SETCHECK, BST_CHECKED, 0);

    if (!*Config->DefSfxFile)
        EnableWindow(GetDlgItem(Dlg, IDC_SELFEXTR), FALSE);

    if (Config->LastUsedAuto)
    {
        SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_SELECTSTRING, -1, (LPARAM)LoadStr(IDS_AUTO));
        SendDlgItemMessage(Dlg, IDC_UNITS, CB_SETCURSEL, 0, 0);
    }
    else
    {
        SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_SETCURSEL, 0, 0);
        SendDlgItemMessage(Dlg, IDC_UNITS, CB_SETCURSEL, Config->VolSizeUnits[0] == 0 ? 0 : 1, 0);
    }

    ResetControls();

    CenterDlgToParent();
    return TRUE;
}

BOOL CPackDialog::OnMultiVol(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnMultiVol(0x%X, 0x%X, )", wNotifyCode, wID);
    char name[MAX_PATH];

    switch (wNotifyCode)
    {
    case BN_CLICKED:
        if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            EnableWindow(GetDlgItem(Dlg, IDC_VOLSIZE), TRUE);
            EnableWindow(GetDlgItem(Dlg, IDC_UNITS), TRUE);
            if ((Flags & PD_REMOVALBE))
                EnableWindow(GetDlgItem(Dlg, IDC_SEQNAME), TRUE);
            if (SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
            {
                MakeFileName(1, SendDlgItemMessage(Dlg, IDC_SEQNAME, BM_GETCHECK, 0, 0) == BST_CHECKED,
                             ZipFile, name, Config->WinZipNames && SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
                SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)name);
                InvalidateRect(GetDlgItem(Dlg, IDC_ARCHIVE), NULL, TRUE);
                UpdateWindow(GetDlgItem(Dlg, IDC_ARCHIVE));
            }
            SetWindowText(Dlg, LoadStr(IDS_CREAETARCH));
        }
        else
        {
            EnableWindow(GetDlgItem(Dlg, IDC_VOLSIZE), FALSE);
            EnableWindow(GetDlgItem(Dlg, IDC_UNITS), FALSE);
            EnableWindow(GetDlgItem(Dlg, IDC_SEQNAME), FALSE);
            if (SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
            {
                SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)ZipFile);
                InvalidateRect(GetDlgItem(Dlg, IDC_ARCHIVE), NULL, TRUE);
                UpdateWindow(GetDlgItem(Dlg, IDC_ARCHIVE));
            }
            if ((Flags & PD_NEWARCHIVE) == 0 &&
                SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
            {
                SetWindowText(Dlg, LoadStr(IDS_ADDTOARCHIVE));
            }
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CPackDialog::OnSeqName(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnSeqName(0x%X, 0x%X, )", wNotifyCode, wID);
    char name[MAX_PATH];

    switch (wNotifyCode)
    {
    case BN_CLICKED:
        if (SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
        {
            MakeFileName(1, SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED,
                         ZipFile, name, Config->WinZipNames && SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
            SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)name);
            InvalidateRect(GetDlgItem(Dlg, IDC_ARCHIVE), NULL, TRUE);
            UpdateWindow(GetDlgItem(Dlg, IDC_ARCHIVE));
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CPackDialog::OnSelfExtr(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnSelfExtr(0x%X, 0x%X, )", wNotifyCode, wID);
    char name[MAX_PATH];
    switch (wNotifyCode)
    {
    case BN_CLICKED:
        if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            EnableWindow(GetDlgItem(Dlg, IDC_ADVANCED), TRUE);
            char path[MAX_PATH + 4], ext[MAX_PATH];
            SplitPath2(ZipFile, path, name, ext);
            strcat(path, name);
            strcat(path, ".exe");
            *(path + MAX_PATH - 1) = 0; //jen tak pro jistotu
            SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)path);
            InvalidateRect(GetDlgItem(Dlg, IDC_ARCHIVE), NULL, TRUE);
            UpdateWindow(GetDlgItem(Dlg, IDC_ARCHIVE));
            SetWindowText(Dlg, LoadStr(IDS_CREAETARCH));
        }
        else
        {
            EnableWindow(GetDlgItem(Dlg, IDC_ADVANCED), FALSE);
            if (SendDlgItemMessage(Dlg, IDC_MULTIVOL, BM_GETCHECK, 0, 0) == BST_CHECKED)
            {
                MakeFileName(1, SendDlgItemMessage(Dlg, IDC_SEQNAME, BM_GETCHECK, 0, 0) == BST_CHECKED,
                             ZipFile, name, Config->WinZipNames);
                SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)name);
            }
            else
            {
                SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)ZipFile);
            }
            InvalidateRect(GetDlgItem(Dlg, IDC_ARCHIVE), NULL, TRUE);
            UpdateWindow(GetDlgItem(Dlg, IDC_ARCHIVE));
            if ((Flags & PD_NEWARCHIVE) == 0 &&
                SendDlgItemMessage(Dlg, IDC_MULTIVOL, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
            {
                SetWindowText(Dlg, LoadStr(IDS_ADDTOARCHIVE));
            }
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CPackDialog::OnEncrypt(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnEncrypt(0x%X, 0x%X, )", wNotifyCode, wID);
    switch (wNotifyCode)
    {
    case BN_CLICKED:
    {
        BOOL bChecked = SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED;

        EnableWindow(GetDlgItem(Dlg, IDC_PASSWORD1), bChecked);
        EnableWindow(GetDlgItem(Dlg, IDC_PASSWORD2), bChecked);
        EnableWindow(GetDlgItem(Dlg, IDC_ENC_ZIP20), bChecked);
        EnableWindow(GetDlgItem(Dlg, IDC_ENC_AES128), bChecked);
        EnableWindow(GetDlgItem(Dlg, IDC_ENC_AES256), bChecked);
        if (bChecked)
            SetFocus(GetDlgItem(Dlg, IDC_PASSWORD1));
        return TRUE;
    }
    }
    return FALSE;
}

BOOL CPackDialog::SetVolSize(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CPackDialog::SetVolSize(0x%IX, 0x%IX)", wParam, lParam);
    TCHAR buf[32];
    _stprintf(buf, _T("%Iu"), wParam);
    SendDlgItemMessage(Dlg, IDC_VOLSIZE, WM_SETTEXT, 0, (LPARAM)buf);
    SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_SETEDITSEL, 0, MAKELPARAM((0), (-1)));
    return FALSE;
}

BOOL CPackDialog::OnVolSize(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnVolSize(0x%X, 0x%X, )", wNotifyCode, wID);
    switch (wNotifyCode)
    {
    case CBN_SELCHANGE:
    {
        LRESULT curSel = SendDlgItemMessage(Dlg, wID, CB_GETCURSEL, 0, 0);
        if (curSel != CB_ERR)
        {
            if (curSel < 5 && Config->VolSizeCache[curSel][0] != 0)
            {
                SendDlgItemMessage(Dlg, IDC_UNITS, CB_SETCURSEL,
                                   Config->VolSizeUnits[curSel] == 0 ? 0 : 1, 0);
            }
        }
        return TRUE;
    }
    }
    return FALSE;
}

BOOL CPackDialog::OnAdvanced(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnAdvanced(0x%X, 0x%X, )", wNotifyCode, wID);
    CAdvancedSEDialog dlg(Dlg, PackObject, PackOptions, &Config->ChangeLangReaction);
    dlg.Proceed();
    return TRUE;
}

BOOL CPackDialog::OnConfig(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnConfig(0x%X, 0x%X, )", wNotifyCode, wID);
    bool old = Config->ShowExOptions;
    if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config->ShowExOptions = false;
    else
        Config->ShowExOptions = true;

    CConfigDialog dlg(Dlg, Config);

    if (dlg.Proceed() == IDOK)
    {
        if (Config->ShowExOptions)
            SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
        else
            SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
        if (SalamanderGeneral->GetPanelPluginData(PANEL_LEFT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_LEFT);
        if (SalamanderGeneral->GetPanelPluginData(PANEL_RIGHT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_RIGHT);
    }
    else
        Config->ShowExOptions = old;

    return TRUE;
}

BOOL CPackDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPackDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    PackOptions->Action = PA_NORMAL;
    if (SendDlgItemMessage(Dlg, IDC_MULTIVOL, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        if (Config->Level < 1)
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_MULTISTORED), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }

        bool mb = SendDlgItemMessage(Dlg, IDC_UNITS, CB_GETCURSEL, 0, 0) == 1;
        bool ok = false;
        CQuadWord size;
        char volSize[MAX_VOL_STR];

        PackOptions->Action |= PA_MULTIVOL;

        *volSize = 0;
        SendDlgItemMessage(Dlg, IDC_VOLSIZE, WM_GETTEXT, MAX_VOL_STR, (LPARAM)volSize);
        char* sour = volSize;
        while (*sour && *sour == ' ')
            sour++;
        const char* a = LoadStr(IDS_AUTO);
        if (!_strnicmp(sour, a, lstrlen(a)))
        {
            PackOptions->VolumeSize = -1;
            ok = true;
        }
        else
        {
            if (lstrlen(sour) < 12)
            {
                double aux;
                if (Atod(sour, DecimalSeparator, &aux))
                {
                    size.SetDouble(aux);
                    if (size < CQuadWord((mb ? 40960 : 41943040), 0) && size > CQuadWord(0, 0))
                    {
                        QWORD u = size.Value * (mb ? 1024 * 1024 : 1024);

                        if (u >= 1024)
                        {
                            PackOptions->VolumeSize = u;
                            ok = 1;
                        }
                    }
                }
            }
        }
        if (!ok)
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADVOLSIZE), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }
        //update volume size cache
        if (PackOptions->VolumeSize == -1)
            Config->LastUsedAuto = true;
        else
        {
            //LRESULT cs = SendDlgItemMessage(Dlg, IDC_VOLSIZE, CB_GETCURSEL, 0, 0);
            LRESULT i;
            /*
      if (cs == CB_ERR || cs < 5 && Config->VolSizeUnits[cs] != (mb ? 1 : 0))
      {*/
            double d1, d2;
            Atod(volSize, DecimalSeparator, &d1);
            if (mb)
                d1 *= 1024;
            for (i = 0; i < 4 && Config->VolSizeCache[i][0] != 0; i++)
            {
                Atod(Config->VolSizeCache[i], DecimalSeparator, &d2);
                d2 *= Config->VolSizeUnits[i] == 1 ? 1024 : 1;
                if (d1 == d2)
                    break;
            }
            for (; i > 0; i--)
            {
                lstrcpy(Config->VolSizeCache[i], Config->VolSizeCache[i - 1]);
                //Config->VolSizeCache[i] = Config->VolSizeCache[i - 1];
                Config->VolSizeUnits[i] = Config->VolSizeUnits[i - 1];
            }
            lstrcpy(Config->VolSizeCache[0], volSize);
            Config->VolSizeUnits[0] = mb ? 1 : 0;
            Config->LastUsedAuto = false;
            /*
      }
      else
      {
        if (cs < 5 )//&& Config->VolSizeCache[cs])
        {
          for (i = cs; i > 0; i--)
          {
            lstrcpy(Config->VolSizeCache[i], Config->VolSizeCache[i - 1]);
            Config->VolSizeUnits[i] = Config->VolSizeUnits[i - 1];
          }
          lstrcpy(Config->VolSizeCache[0], volSize);
          Config->VolSizeUnits[0] = mb ? 1 : 0;
          Config->LastUsedAuto = false;
        }
      }
      */
        }

        PackOptions->SeqNames = SendDlgItemMessage(Dlg, IDC_SEQNAME, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    if (SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        PackOptions->Action |= PA_SELFEXTRACT;
    }

    if (SendDlgItemMessage(Dlg, IDC_ENCRYPT, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        if (Config->Level < 1)
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ECRYPTSTORED), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }

        if ((PackOptions->Action & PA_SELFEXTRACT) && (SendDlgItemMessage(Dlg, IDC_ENC_ZIP20, BM_GETCHECK, 0, 0) != BST_CHECKED))
        {
            // SFX doesn't support AES...
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ECRYPTSFX), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }

        char pwd1[MAX_PASSWORD];
        char pwd2[MAX_PASSWORD];

        PackOptions->Encrypt = true;
        if (GetDlgItemText(Dlg, IDC_PASSWORD1, pwd1, MAX_PASSWORD - 1) > 0 &&
            GetDlgItemText(Dlg, IDC_PASSWORD2, pwd2, MAX_PASSWORD - 1) > 0)
            if (!lstrcmp(pwd1, pwd2))
                lstrcpy(PackOptions->Password, pwd1);
            else
            {
                SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_PWDDONTMATCH), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
                return TRUE;
            }
        else
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_PWDTOOSHORT), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }
        if (SendDlgItemMessage(Dlg, IDC_ENC_ZIP20, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->EncryptMethod = EM_ZIP20;
        else if (SendDlgItemMessage(Dlg, IDC_ENC_AES128, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->EncryptMethod = EM_AES128;
        else
            Config->EncryptMethod = EM_AES256;
    }
    else
        PackOptions->Encrypt = false;

    if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config->ShowExOptions = false;
    else
        Config->ShowExOptions = true;

    EndDialog(Dlg, IDOK);
    return TRUE;
}

void CPackDialog::ResetControls()
{
    CALL_STACK_MESSAGE1("CPackDialog::ResetControls()");
    char archive[MAX_PATH + 4]; //aby to neprelezlo u  SE

    if (PackOptions->Action & PA_MULTIVOL)
        SendDlgItemMessage(Dlg, IDC_MULTIVOL, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    if (PackOptions->SeqNames || !(Flags & PD_REMOVALBE))
        SendDlgItemMessage(Dlg, IDC_SEQNAME, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    if (PackOptions->Action & PA_SELFEXTRACT)
        SendDlgItemMessage(Dlg, IDC_SELFEXTR, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    if (PackOptions->Encrypt)
        SendDlgItemMessage(Dlg, IDC_ENCRYPT, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);

    SetWindowText(Dlg, Flags & PD_NEWARCHIVE || PackOptions->Action & (PA_MULTIVOL | PA_SELFEXTRACT)
                           ? LoadStr(IDS_CREAETARCH)
                           : LoadStr(IDS_ADDTOARCHIVE));
    lstrcpy(archive, ZipFile);
    if (PackOptions->Action & PA_MULTIVOL)
    {
        MakeFileName(1, PackOptions->SeqNames, ZipFile, archive,
                     Config->WinZipNames && !(PackOptions->Action & PA_SELFEXTRACT));
    }
    if (PackOptions->Action & PA_SELFEXTRACT)
    {
        char name[MAX_PATH], ext[MAX_PATH];
        SplitPath2(ZipFile, archive, name, ext);
        lstrcat(archive, name);
        lstrcat(archive, ".exe");
        *(archive + MAX_PATH - 1) = 0; //jen tak pro jistotu
    }
    SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)archive);
}

INT_PTR PackDialog(HWND parent, CZipPack* packObject, CConfiguration* config,
                   CExtendedOptions* packOptions, const char* zipFile, unsigned flags)
{
    CALL_STACK_MESSAGE3("PackDialog(, , , , %s, 0x%X)", zipFile, flags);
    CPackDialog dlg(parent, packObject, config, packOptions, zipFile, flags);
    return dlg.Proceed();
}

// CConfigDialog

INT_PTR WINAPI ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ConfigDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static CConfigDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CConfigDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CConfigDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CConfigDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CONFIG),
                          Parent, ConfigDlgProc, (LPARAM)this);
}

INT_PTR CConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;

        case IDC_DEFAULT:
            return OnDefault(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_CONFIG, FALSE);
            return TRUE;
        }
        break;

    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_CONFIG, FALSE);
        return TRUE;
    }
    return FALSE;
}

BOOL CConfigDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CConfigDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    if (Config->AutoExpandMV)
        SendDlgItemMessage(Dlg, IDC_AUTOEXPANDMV, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_AUTOEXPANDMV, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (Config->BackupZip)
        SendDlgItemMessage(Dlg, IDC_BACKUP, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_BACKUP, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (Config->NoEmptyDirs)
        SendDlgItemMessage(Dlg, IDC_NOEMPTYDIRS, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_NOEMPTYDIRS, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (Config->WinZipNames)
        SendDlgItemMessage(Dlg, IDC_WINZIPNAMES, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_WINZIPNAMES, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    SendDlgItemMessage(Dlg, IDC_LEVEL, EM_LIMITTEXT, 1, 0);
    SetDlgItemInt(Dlg, IDC_LEVEL, Config->Level, TRUE);

    if (Config->TimeToNewestFile)
        SendDlgItemMessage(Dlg, IDC_SETFILETIME, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_SETFILETIME, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (Config->ShowExOptions)
        SendDlgItemMessage(Dlg, IDC_SHOWEXOPTIONS, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_SHOWEXOPTIONS, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (!*Config->DefSfxFile || !LoadSfxLangs(Dlg, Config->DefSfxFile, false))
        EnableWindow(GetDlgItem(Dlg, IDC_LANGUAGE), FALSE);

    if (Config->ChangeLangReaction == CLR_ASK)
        SendDlgItemMessage(Dlg, IDC_ASK, BM_SETCHECK, BST_CHECKED, 0);
    if (Config->ChangeLangReaction == CLR_REPLACE)
        SendDlgItemMessage(Dlg, IDC_REPLACE, BM_SETCHECK, BST_CHECKED, 0);
    if (Config->ChangeLangReaction == CLR_PRESERVE)
        SendDlgItemMessage(Dlg, IDC_LEAVE, BM_SETCHECK, BST_CHECKED, 0);

    SendDlgItemMessage(Dlg, IDC_CFG_LISTINFOPACKEDSIZE, BM_SETCHECK, Config->ListInfoPackedSize ? BST_CHECKED : BST_UNCHECKED, 0);

    CenterDlgToParent();
    return TRUE;
}

BOOL CConfigDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CConfigDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    BOOL sucess;

    int i = (int)GetDlgItemInt(Dlg, IDC_LEVEL, &sucess, FALSE);

    if (!sucess || i < 0 || i > 9)
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADLEVEL), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
    else
    {
        Config->Level = i;
        if (SendDlgItemMessage(Dlg, IDC_AUTOEXPANDMV, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->AutoExpandMV = true;
        else
            Config->AutoExpandMV = false;

        if (SendDlgItemMessage(Dlg, IDC_BACKUP, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->BackupZip = true;
        else
            Config->BackupZip = false;

        if (SendDlgItemMessage(Dlg, IDC_NOEMPTYDIRS, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->NoEmptyDirs = true;
        else
            Config->NoEmptyDirs = false;

        if (SendDlgItemMessage(Dlg, IDC_SETFILETIME, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->TimeToNewestFile = true;
        else
            Config->TimeToNewestFile = false;

        if (SendDlgItemMessage(Dlg, IDC_WINZIPNAMES, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->WinZipNames = TRUE;
        else
            Config->WinZipNames = FALSE;

        if (SendDlgItemMessage(Dlg, IDC_SHOWEXOPTIONS, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->ShowExOptions = true;
        else
            Config->ShowExOptions = false;

        LRESULT j = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
        if (j != CB_ERR)
        {
            CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, j, 0);
            if ((LRESULT)lang != CB_ERR)
            {
                /*char * file = strrchr(lang->FileName, '\\');
        if (!file) file = lang->FileName;
        else file++;
        lstrcpy(Config->DefSfxFile, file);
        */
                lstrcpy(Config->DefSfxFile, lang->FileName);
            }
        }

        if (SendDlgItemMessage(Dlg, IDC_ASK, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->ChangeLangReaction = CLR_ASK;
        if (SendDlgItemMessage(Dlg, IDC_REPLACE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->ChangeLangReaction = CLR_REPLACE;
        if (SendDlgItemMessage(Dlg, IDC_LEAVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->ChangeLangReaction = CLR_PRESERVE;

        if (SendDlgItemMessage(Dlg, IDC_CFG_LISTINFOPACKEDSIZE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            Config->ListInfoPackedSize = true;
        else
            Config->ListInfoPackedSize = false;

        EndDialog(Dlg, IDOK);
    }
    return TRUE;
}

BOOL CConfigDialog::OnDefault(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CConfigDialog::OnDefault(0x%X, 0x%X, )", wNotifyCode, wID);
    //Config->Level = DefConfig.Level;
    //Config->NoEmptyDirs = DefConfig.NoEmptyDirs;
    //Config->BackupZip = DefConfig.BackupZip;
    //Config->ShowExOptions = DefConfig.ShowExOptions;
    //Config->LastUsedAuto = DefConfig.LastUsedAuto;
    //Config->AutoExpandMV = DefConfig.AutoExpandMV;

    if (DefConfig.AutoExpandMV)
        SendDlgItemMessage(Dlg, IDC_AUTOEXPANDMV, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_AUTOEXPANDMV, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (DefConfig.BackupZip)
        SendDlgItemMessage(Dlg, IDC_BACKUP, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_BACKUP, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (DefConfig.NoEmptyDirs)
        SendDlgItemMessage(Dlg, IDC_NOEMPTYDIRS, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_NOEMPTYDIRS, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    if (DefConfig.TimeToNewestFile)
        SendDlgItemMessage(Dlg, IDC_SETFILETIME, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_SETFILETIME, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    //SendDlgItemMessage(Dlg, IDC_LEVEL, CB_LIMITTEXT, 1, 0);
    SetDlgItemInt(Dlg, IDC_LEVEL, DefConfig.Level, FALSE);

    if (DefConfig.ShowExOptions)
        SendDlgItemMessage(Dlg, IDC_SHOWEXOPTIONS, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_SHOWEXOPTIONS, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);

    //jeste vybereme spravny sfx balicek
    if (IsWindowEnabled(GetDlgItem(Dlg, IDC_LANGUAGE)))
    {
        CSfxLang* lang;
        BOOL ok = FALSE;
        int i;
        for (i = 0; i < SfxLanguages->Count; i++)
        {
            lang = (*SfxLanguages)[i];
            if (lstrcmpi(lang->FileName, DefConfig.DefSfxFile) == 0)
            {
                char langName[128];
                if (GetLocaleInfo(MAKELCID(MAKELANGID(lang->LangID, SUBLANG_NEUTRAL), SORT_DEFAULT), LOCALE_SLANGUAGE, langName, 128))
                {
                    char* c = strchr(langName, ' ');
                    if (c)
                        *c = 0;
                    if (SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_SELECTSTRING, -1, (LPARAM)langName) != CB_ERR)
                        ok = TRUE;
                }
                break;
            }
        }
        if (!ok)
            SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_SETCURSEL, 0, 0);
    }

    return TRUE;
}

// CPasswordDialog

INT_PTR WINAPI PasswordDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("PasswordDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CPasswordDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CPasswordDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CPasswordDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CPasswordDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_PASSWORD),
                          Parent, PasswordDlgProc, (LPARAM)this);
}

INT_PTR CPasswordDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPasswordDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        case IDC_SKIP:
            EndDialog(Dlg, IDC_SKIP);
            return TRUE;
        case IDC_SKIPALL:
            EndDialog(Dlg, IDC_SKIPALL);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        SubClassStatic(IDC_FILE, false);
        return TRUE;
    }
    return FALSE;
}

BOOL CPasswordDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CPasswordDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    Lock = LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_LOCK));
    if (Lock)
        SendDlgItemMessage(Dlg, IDC_LOCK, STM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)Lock);
    SubClassStatic(IDC_FILE, true);
    SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File);
    SendDlgItemMessage(Dlg, IDC_PASSWORD, EM_SETLIMITTEXT, MAX_PASSWORD - 1, 0);
    CenterDlgToParent();
    return TRUE;
}

BOOL CPasswordDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CPasswordDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    if (GetDlgItemText(Dlg, IDC_PASSWORD, Password, MAX_PASSWORD - 1) == 0)
        *Password = 0;
    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR PasswordDialog(HWND parent, const char* file, char* password)
{
    CALL_STACK_MESSAGE2("PasswordDialog(, %s, )", file);
    CPasswordDialog dlg(parent, file, password);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//CLowDiskSpaceDialog

INT_PTR WINAPI LowDiskSpaceDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("LowDiskSpaceDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CLowDiskSpaceDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CLowDiskSpaceDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CLowDiskSpaceDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CLowDiskSpaceDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_LOWFREESPACE),
                          Parent, LowDiskSpaceDlgProc, (LPARAM)this);
}

INT_PTR CLowDiskSpaceDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CLowDiskSpaceDialog::DialogProc(0x%X, 0x%IX, 0x%IX)",
                        uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_IGNORE:
            EndDialog(Dlg, IDC_IGNORE);
            return TRUE;

        case IDC_ALL:
            EndDialog(Dlg, IDC_ALL);
            return TRUE;

        case IDC_RETRY:
            EndDialog(Dlg, IDC_RETRY);
            return TRUE;

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        SubClassStatic(IDC_FILE, false);
        return TRUE;
    }
    return FALSE;
}

BOOL CLowDiskSpaceDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CLowDiskSpaceDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    char buf[32];

    if (Flags & LSD_NOIGNORE)
    {
        EnableWindow(GetDlgItem(Dlg, IDC_IGNORE), FALSE);
        EnableWindow(GetDlgItem(Dlg, IDC_ALL), FALSE);
    }
    SubClassStatic(IDC_PATH, true);
    SendDlgItemMessage(Dlg, IDC_TEXT, WM_SETTEXT, 0, (LPARAM)Text);
    SendDlgItemMessage(Dlg, IDC_PATH, WM_SETTEXT, 0, (LPARAM)Path);
    FormatNumber(FreeSpace, buf, LoadStr(IDS_BYTES));
    SendDlgItemMessage(Dlg, IDC_FREESPACE, WM_SETTEXT, 0, (LPARAM)buf);
    if (VolumeSize != -1)
    {
        FormatNumber(VolumeSize, buf, LoadStr(IDS_BYTES));
    }
    else
    {
        lstrcpy(buf, LoadStr(IDS_AUTO));
    }
    SendDlgItemMessage(Dlg, IDC_VOLUMESIZE, WM_SETTEXT, 0, (LPARAM)buf);
    CenterDlgToParent();
    return TRUE;
}

INT_PTR LowDiskSpaceDialog(HWND parent, const char* text, const char* path,
                           __INT64 freeSpace, __INT64 volumeSize, int flags)
{
    CALL_STACK_MESSAGE6("LowDiskSpaceDialog(, %s, %s, 0x%I64X, 0x%I64X, %d)", text,
                        path, freeSpace, volumeSize, flags);
    CLowDiskSpaceDialog dlg(parent, text, path, freeSpace, volumeSize, flags);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//CChangeDiskDialog

INT_PTR WINAPI ChangeDiskDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ChangeDiskDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CChangeDiskDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CChangeDiskDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CChangeDiskDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CChangeDiskDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CHANGEDISK),
                          Parent, ChangeDiskDlgProc, (LPARAM)this);
}

INT_PTR CChangeDiskDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeDiskDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            EndDialog(Dlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL CChangeDiskDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SendDlgItemMessage(Dlg, IDC_CHDISKTEXT, WM_SETTEXT, 0, (LPARAM)Text);
    CenterDlgToParent();
    return TRUE;
}

INT_PTR ChangeDiskDialog(HWND parent, const char* text)
{
    CALL_STACK_MESSAGE2("ChangeDiskDialog(, %s)", text);
    CChangeDiskDialog dlg(parent, text);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//CChangeDiskDialog2

INT_PTR WINAPI ChangeDiskDlgProc2(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ChangeDiskDlgProc2(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CChangeDiskDialog2* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CChangeDiskDialog2*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CChangeDiskDialog2::Proceed()
{
    CALL_STACK_MESSAGE1("CChangeDiskDialog2::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CHANGEDISK2),
                          Parent, ChangeDiskDlgProc2, (LPARAM)this);
}

INT_PTR CChangeDiskDialog2::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeDiskDialog2::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BROWSE:
            return OnBrowse(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL CChangeDiskDialog2::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog2::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    //char buf[128];

    //sprintf(buf, LoadStr(IDS_CHDISKTEXT2), VolumeNumber);
    //SendDlgItemMessage(Dlg, IDC_CHDISKTEXT, WM_SETTEXT, 0, (LPARAM) Text);
    SendDlgItemMessage(Dlg, IDC_FILENAME, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)FileName);

    CenterDlgToParent();
    return TRUE;
}

BOOL CChangeDiskDialog2::OnBrowse(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog2::OnBrowse(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));
    char buf[128];

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = Dlg;
    ofn.hInstance = 0;
    sprintf(buf, "%s%c%s%c"
                 "%s%c%s%c",
            LoadStr(IDS_ZIPARCHIVES), 0, LoadStr(IDS_ZIPARCHIVESEXTS), 0,
            LoadStr(IDS_ALLFILES), 0, "*.*", 0);
    ofn.lpstrFilter = buf;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    GetDlgItemText(Dlg, IDC_FILENAME, FileName, MAX_PATH - 1);
    ofn.lpstrFile = FileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = LoadStr(IDS_BROWSEARCHIVETITLE);
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "zip";
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
    {
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)FileName);
    }
    /*
  else
  {
    DWORD err = CommDlgExtendedError();
  }
  */
    return TRUE;
}

BOOL CChangeDiskDialog2::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog2::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    GetDlgItemText(Dlg, IDC_FILENAME, FileName, MAX_PATH - 1);

    SalamanderGeneral->SalUpdateDefaultDir(TRUE);
    int err;
    if (!SalamanderGeneral->SalGetFullName(FileName, &err, CurrentPath))
    {
        char buffer[100];
        SalamanderGeneral->SalMessageBox(Dlg, SalamanderGeneral->GetGFNErrorText(err, buffer, 100),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        return TRUE;
    }

    DWORD attr = SalamanderGeneral->SalGetFileAttributes(FileName);
    if (attr == 0xFFFFFFFF || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_NOTFOUND),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)FileName);
        return TRUE;
    }

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR ChangeDiskDialog2(HWND parent, /*int volNum,*/ char* fileName)
{
    CALL_STACK_MESSAGE1("ChangeDiskDialog2(, )");
    CChangeDiskDialog2 dlg(parent, fileName);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//CChangeDiskDialog3

INT_PTR WINAPI ChangeDiskDlgProc3(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ChangeDiskDlgProc3(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CChangeDiskDialog3* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CChangeDiskDialog3*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CChangeDiskDialog3::Proceed()
{
    CALL_STACK_MESSAGE1("CChangeDiskDialog3::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CHANGEDISK3),
                          Parent, ChangeDiskDlgProc3, (LPARAM)this);
}

INT_PTR CChangeDiskDialog3::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeDiskDialog3::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SEQNAME:
            return OnSeqNames(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_WINZIP:
            return OnWinZip(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDC_BROWSE:
            return OnBrowse(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL CChangeDiskDialog3::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog3::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    char buf[128];
    char buf2[MAX_PATH + 1];

    sprintf(buf, LoadStr(IDS_CHDISKTEXT2), VolumeNumber);
    SendDlgItemMessage(Dlg, IDC_CHDISKTEXT, WM_SETTEXT, 0, (LPARAM)buf);
    SendDlgItemMessage(Dlg, IDC_FILENAME, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    if (*Flags & CHD_SEQNAMES)
    {
        SendDlgItemMessage(Dlg, IDC_SEQNAME, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
        RenumberName(VolumeNumber, OldName, buf2, Last, *Flags & CHD_WINZIP);
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)buf2);
    }
    else
    {
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)OldName);
        EnableWindow(GetDlgItem(Dlg, IDC_WINZIP), FALSE);
    }
    if (*Flags & CHD_WINZIP)
        SendDlgItemMessage(Dlg, IDC_WINZIP, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    if (!(*Flags & CHD_FIRST))
    {
        EnableWindow(GetDlgItem(Dlg, IDC_SEQNAME), FALSE);
    }
    CenterDlgToParent();
    return TRUE;
}

BOOL CChangeDiskDialog3::OnSeqNames(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog3::OnSeqNames(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    char buf[MAX_PATH + 1];
    char buf2[MAX_PATH + 1];

    if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        GetDlgItemText(Dlg, IDC_FILENAME, buf, MAX_PATH - 1);
        *Flags |= CHD_SEQNAMES;
        RenumberName(VolumeNumber, buf, buf2, Last, *Flags & CHD_WINZIP);
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)buf2);
        EnableWindow(GetDlgItem(Dlg, IDC_WINZIP), TRUE);
    }
    else
    {
        *Flags &= ~CHD_SEQNAMES;
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)OldName);
        EnableWindow(GetDlgItem(Dlg, IDC_WINZIP), FALSE);
    }
    return TRUE;
}

BOOL CChangeDiskDialog3::OnWinZip(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog3::OnWinZip(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    char buf[MAX_PATH + 1];

    if (SendDlgItemMessage(Dlg, IDC_SEQNAME, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
            *Flags |= CHD_WINZIP;
        else
            *Flags &= ~CHD_WINZIP;
        RenumberName(VolumeNumber, OldName, buf, Last, *Flags & CHD_WINZIP);
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)buf);
    }
    return TRUE;
}

BOOL CChangeDiskDialog3::OnBrowse(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog3::OnBrowse(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));
    char buf[128];
    char buf2[MAX_PATH + 1];

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = Dlg;
    ofn.hInstance = 0;
    sprintf(buf, "%s%c%s%c"
                 "%s%c%s%c",
            LoadStr(IDS_ZIPARCHIVES), 0, LoadStr(IDS_ZIPARCHIVESEXTS), 0,
            LoadStr(IDS_ALLFILES), 0, "*.*", 0);
    ofn.lpstrFilter = buf;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    GetDlgItemText(Dlg, IDC_FILENAME, buf2, MAX_PATH - 1);
    ofn.lpstrFile = buf2;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = LoadStr(IDS_BROWSEARCHIVETITLE);
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "zip";
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
    {
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)buf2);
    }
    /*
  else
  {
    DWORD err = CommDlgExtendedError();
  }
  */
    return TRUE;
}

BOOL CChangeDiskDialog3::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CChangeDiskDialog3::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    GetDlgItemText(Dlg, IDC_FILENAME, OldName, MAX_PATH - 1);

    SalamanderGeneral->SalUpdateDefaultDir(TRUE);
    int err;
    if (!SalamanderGeneral->SalGetFullName(OldName, &err, CurrentPath))
    {
        char buffer[100];
        SalamanderGeneral->SalMessageBox(Dlg, SalamanderGeneral->GetGFNErrorText(err, buffer, 100),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        return TRUE;
    }

    DWORD attr = SalamanderGeneral->SalGetFileAttributes(OldName);
    if (attr == 0xFFFFFFFF || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_NOTFOUND),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)OldName);
        return TRUE;
    }

    if (SendDlgItemMessage(Dlg, IDC_PROCEEDALL, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        *Flags |= CHD_ALL;
    }
    else
    {
        *Flags &= ~CHD_ALL;
    }

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR ChangeDiskDialog3(HWND parent, int volNum, bool last, char* fileName, unsigned* flags)
{
    CALL_STACK_MESSAGE2("ChangeDiskDialog3(, %d, , )", volNum);
    CChangeDiskDialog3 dlg(parent, volNum, last, fileName, flags);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//COverwriteDialog

INT_PTR WINAPI OverwriteDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("OverwriteDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static COverwriteDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (COverwriteDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR COverwriteDialog::Proceed()
{
    CALL_STACK_MESSAGE1("COverwriteDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_OVERWRITE),
                          Parent, OverwriteDlgProc, (LPARAM)this);
}

INT_PTR COverwriteDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("COverwriteDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_YES:
            EndDialog(Dlg, IDC_YES);
            return TRUE;

        case IDC_ALL:
            EndDialog(Dlg, IDC_ALL);
            return TRUE;

        case IDC_RETRY:
            EndDialog(Dlg, IDC_RETRY);
            return TRUE;

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        SubClassStatic(IDC_FILE, false);
        return TRUE;
    }
    return FALSE;
}

BOOL COverwriteDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("COverwriteDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SubClassStatic(IDC_FILE, true);
    SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File);
    SendDlgItemMessage(Dlg, IDC_FILEATTR, WM_SETTEXT, 0, (LPARAM)Attr);
    CenterDlgToParent();
    return TRUE;
}

INT_PTR OverwriteDialog(HWND parent, const char* file, const char* attr)
{
    CALL_STACK_MESSAGE3("OverwriteDialog(, %s, %s)", file, attr);
    COverwriteDialog dlg(parent, file, attr);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//COverwriteDialog2

INT_PTR WINAPI OverwriteDlgProc2(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("OverwriteDlgProc2(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static COverwriteDialog2* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (COverwriteDialog2*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR COverwriteDialog2::Proceed()
{
    CALL_STACK_MESSAGE1("COverwriteDialog2::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_OVERWRITE2),
                          Parent, OverwriteDlgProc2, (LPARAM)this);
}

INT_PTR COverwriteDialog2::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("COverwriteDialog2::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_YES:
            EndDialog(Dlg, IDC_YES);
            return TRUE;

        case IDC_NO_:
            EndDialog(Dlg, IDC_NO_);
            return TRUE;

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        SubClassStatic(IDC_FILE, false);
        return TRUE;
    }
    return FALSE;
}

BOOL COverwriteDialog2::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("COverwriteDialog2::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SubClassStatic(IDC_FILE, true);
    SendDlgItemMessage(Dlg, IDC_FILE, WM_SETTEXT, 0, (LPARAM)File);
    SendDlgItemMessage(Dlg, IDC_FILEATTR, WM_SETTEXT, 0, (LPARAM)Attr);
    CenterDlgToParent();
    return TRUE;
}

INT_PTR OverwriteDialog2(HWND parent, const char* file, const char* attr)
{
    CALL_STACK_MESSAGE3("OverwriteDialog2(, %s, %s)", file, attr);
    COverwriteDialog2 dlg(parent, file, attr);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

//******************************************************************************
//
// CRenFavDialog
//

INT_PTR WINAPI RenFavDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("RenFavDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static CRenFavDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CRenFavDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CRenFavDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CRenFavDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_RENAMEFAVSET),
                          Parent, RenFavDlgProc, (LPARAM)this);
}

INT_PTR
CRenFavDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRenFavDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL CRenFavDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CRenFavDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SendDlgItemMessage(Dlg, IDC_NAME, WM_SETTEXT, 0, (LPARAM)Name);
    SendDlgItemMessage(Dlg, IDC_NAME, EM_SETLIMITTEXT, MAX_FAVNAME - 1, 0);
    if (Rename)
        SetWindowText(Dlg, LoadStr(IDS_RENAMEFAVSET));
    CenterDlgToParent();
    return TRUE;
}

BOOL CRenFavDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CRenFavDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    char buf[MAX_FAVNAME];
    GetDlgItemText(Dlg, IDC_NAME, buf, MAX_FAVNAME - 1);
    if (lstrlen(TrimTralingSpaces(buf)) == 0)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_NONTEXT), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    int index;
    for (index = 0; index < Favorities.Count; index++)
    {
        if (CompareMenuItems(buf, Favorities[index]->Name) == 0)
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_USEDNAME), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }
    }
    strcpy(Name, buf);
    EndDialog(Dlg, IDOK);
    return TRUE;
}

//******************************************************************************
//
// CCreateSFXDialog
//

INT_PTR WINAPI CreateSFXDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CreateSFXDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CCreateSFXDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CCreateSFXDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CCreateSFXDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CCreateSFXDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CREATESFX),
                          Parent, CreateSFXDlgProc, (LPARAM)this);
}

INT_PTR
CCreateSFXDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCreateSFXDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;

        case IDC_ADVANCED:
            return OnAdvanced(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_CREATESFX, FALSE);
            return TRUE;
        }
        break;

    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_CREATESFX, FALSE);
        return TRUE;
    }
    return FALSE;
}

BOOL CCreateSFXDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CCreateSFXDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SendDlgItemMessage(Dlg, IDC_ARCHIVE, WM_SETTEXT, 0, (LPARAM)ZipName);
    SendDlgItemMessage(Dlg, IDC_ARCHIVE, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    SendDlgItemMessage(Dlg, IDC_NEWARCHIVE, WM_SETTEXT, 0, (LPARAM)ExeName);
    SendDlgItemMessage(Dlg, IDC_NEWARCHIVE, EM_SETLIMITTEXT, MAX_PATH - 1, 0);

    if (!PackObject)
    {
        EnableWindow(GetDlgItem(Dlg, IDC_ADVANCED), FALSE);
        SetWindowText(GetDlgItem(Dlg, IDC_SOURCE), LoadStr(IDS_SOURCEHEADING));
        SetWindowText(GetDlgItem(Dlg, IDC_TARGET), LoadStr(IDS_TARGETHEADING));
    }

    CenterDlgToParent();
    return TRUE;
}

BOOL CCreateSFXDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CCreateSFXDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    if (GetDlgItemText(Dlg, IDC_ARCHIVE, ZipName, MAX_PATH - 1) <= 0)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_NOARCHIVETYPED), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    if (GetDlgItemText(Dlg, IDC_NEWARCHIVE, ExeName, MAX_PATH - 1) <= 0)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(PackObject ? IDS_NOEXETYPED : IDS_NOARCHIVETYPED), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }

    EndDialog(Dlg, IDOK);
    return TRUE;
}

BOOL CCreateSFXDialog::OnAdvanced(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CCreateSFXDialog::OnAdvanced(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    CAdvancedSEDialog dlg(Dlg, PackObject, PackOptions, &Config.ChangeLangReaction);
    dlg.Proceed();
    return TRUE;
}
