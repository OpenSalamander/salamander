// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

WNDPROC OrigTextControlProc;

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
        HFONT hCurrentFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        HFONT hOldFont = (HFONT)SelectObject(ps.hdc, hCurrentFont);
        SetTextColor(ps.hdc, GetSysColor(COLOR_BTNTEXT));
        int prevBkMode = SetBkMode(ps.hdc, TRANSPARENT);
        int len = GetWindowText(hWnd, txt, MAX_PATH);
        DrawText(ps.hdc, txt, lstrlen(txt), &r, DT_SINGLELINE | /*DT_VCENTER*/ DT_BOTTOM | DT_NOPREFIX | DT_PATH_ELLIPSIS);
        SetBkMode(ps.hdc, prevBkMode);
        SelectObject(ps.hdc, hOldFont);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return CallWindowProc(OrigTextControlProc, hWnd, uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CDlgRoot
//

void CDlgRoot::CenterDlgToParent()
{
    CALL_STACK_MESSAGE1("CDlgRoot::CenterDlgToParent()");
    HWND hParent = GetParent(Dlg);
    if (hParent != NULL)
        SalamanderGeneral->MultiMonCenterWindow(Dlg, hParent, TRUE);
}

void CDlgRoot::SubClassStatic(DWORD wID, BOOL subclass)
{
    CALL_STACK_MESSAGE3("CDlgRoot::SubClassStatic(0x%X, %d)", wID, subclass);
    if (subclass)
        OrigTextControlProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LPARAM)TextControlProc);
    else
        SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LPARAM)OrigTextControlProc);
}

// ****************************************************************************
//
// CNextVolumeDialog
//

INT_PTR WINAPI NextVolumeDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("NextVolumeDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CNextVolumeDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        // SalamanderGUI->ArrangeHorizontalLines(hDlg); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary
        dlg = (CNextVolumeDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CNextVolumeDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CNextVolumeDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CHANGEDISK),
                          Parent, NextVolumeDlgProc, (LPARAM)this);
}

INT_PTR
CNextVolumeDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CNextVolumeDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
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

        case IDC_BROWSE:
            return OnBrowse(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        }
        break;
    }
    return FALSE;
}

BOOL CNextVolumeDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CNextVolumeDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SendDlgItemMessage(Dlg, IDC_FILENAME, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)VolumeName);
    if (Message)
    {
        SendDlgItemMessage(Dlg, IDS_NEXTDISK, WM_SETTEXT, 0, (LPARAM)Message);
        SetWindowText(Dlg, LoadStr(IDS_SELECTFIRSTTITLE));
    }

    CenterDlgToParent();
    return TRUE;
}

BOOL CNextVolumeDialog::OnBrowse(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CNextVolumeDialog::OnBrowse(0x%X, 0x%X, )", wNotifyCode,
                        wID);
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));
    TCHAR buf[128];

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = Dlg;
    ofn.hInstance = 0;
    _stprintf(buf, _T("%s%c%s%c"
                      "%s%c%s%c"),
              LoadStr(IDS_RARARCHIVES), 0, _T("*.r*"), 0,
              LoadStr(IDS_ALLFILES), 0, _T("*.*"), 0);
    ofn.lpstrFilter = buf;
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    GetDlgItemText(Dlg, IDC_FILENAME, VolumeName, MAX_PATH);
    ofn.lpstrFile = VolumeName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = LoadStr(IDS_BROWSEARCHIVETITLE);
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
    {
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)VolumeName);
    }
    return TRUE;
}

BOOL CNextVolumeDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CNextVolumeDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    SendDlgItemMessage(Dlg, IDC_FILENAME, WM_GETTEXT, MAX_PATH, (LPARAM)VolumeName);

    SalamanderGeneral->SalUpdateDefaultDir(TRUE);
    int err;
    if (!SalamanderGeneral->SalGetFullName(VolumeName, &err, CurrentPath))
    {
        char buffer[100];
        SalamanderGeneral->SalMessageBox(Dlg, SalamanderGeneral->GetGFNErrorText(err, buffer, 100),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        return TRUE;
    }

    DWORD attr = SalamanderGeneral->SalGetFileAttributes(VolumeName);
    if (attr == 0xFFFFFFFF || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_NOTFOUND),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)VolumeName);
        return TRUE;
    }

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR NextVolumeDialog(HWND parent, LPTSTR volumeName, LPCTSTR message)
{
    CALL_STACK_MESSAGE1("NextVolumeDialog(, )");
    CNextVolumeDialog dlg(parent, volumeName, message);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

// ****************************************************************************
//
// CContinuedFileDialog
//

INT_PTR WINAPI ContinuedFileDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ContinuedFileDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CContinuedFileDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        // SalamanderGUI->ArrangeHorizontalLines(hDlg); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary
        dlg = (CContinuedFileDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CContinuedFileDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CContinuedFileDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_ERROR),
                          Parent, ContinuedFileDlgProc, (LPARAM)this);
}

INT_PTR
CContinuedFileDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CContinuedFileDialog::DialogProc(0x%X, 0x%IX, 0x%IX)",
                        uMsg, wParam, lParam);
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
        }
        break;

    case WM_DESTROY:
        SubClassStatic(IDS_FILENAME, FALSE);
        return TRUE;
    }
    return FALSE;
}

BOOL CContinuedFileDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CContinuedFileDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SubClassStatic(IDS_FILENAME, TRUE);
    SendDlgItemMessage(Dlg, IDS_FILENAME, WM_SETTEXT, 0, (LPARAM)File);

    CenterDlgToParent();
    return TRUE;
}

BOOL CContinuedFileDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CContinuedFileDialog::OnOK(0x%X, 0x%X, )", wNotifyCode,
                        wID);
    if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config.Options |= OP_SKIPCONTINUED;

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR ContinuedFileDialog(HWND parent, const char* file)
{
    CALL_STACK_MESSAGE2("ContinuedFileDialog(, %s)", file);
    CContinuedFileDialog dlg(parent, file);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

// ****************************************************************************
//
// CConfigDialog
//

INT_PTR WINAPI ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ConfigDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static CConfigDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        // SalamanderGUI->ArrangeHorizontalLines(hDlg); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary
        dlg = (CConfigDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CConfigDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CConfigDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CONFIG),
                          Parent, ConfigDlgProc, (LPARAM)this);
}

INT_PTR
CConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            return FALSE;
        }
        break;
    }
    return FALSE;
}

BOOL CConfigDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CConfigDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SendDlgItemMessage(Dlg, IDC_SKIPCONTINUED, BM_SETCHECK, Config.Options & OP_SKIPCONTINUED ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessage(Dlg, IDC_NOVOLATTENTION, BM_SETCHECK, Config.Options & OP_NO_VOL_ATTENTION ? BST_CHECKED : BST_UNCHECKED, 0);

    SendDlgItemMessage(Dlg, IDC_CFG_LISTINFOPACKEDSIZE, BM_SETCHECK, Config.ListInfoPackedSize ? BST_CHECKED : BST_UNCHECKED, 0);

    CenterDlgToParent();
    return TRUE;
}

BOOL CConfigDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CConfigDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    if (SendDlgItemMessage(Dlg, IDC_SKIPCONTINUED, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config.Options |= OP_SKIPCONTINUED;
    else
        Config.Options &= ~OP_SKIPCONTINUED;
    if (SendDlgItemMessage(Dlg, IDC_NOVOLATTENTION, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config.Options |= OP_NO_VOL_ATTENTION;
    else
        Config.Options &= ~OP_NO_VOL_ATTENTION;

    if (SendDlgItemMessage(Dlg, IDC_CFG_LISTINFOPACKEDSIZE, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config.ListInfoPackedSize = true;
    else
        Config.ListInfoPackedSize = false;

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR ConfigDialog(HWND parent)
{
    CALL_STACK_MESSAGE1("ConfigDialog()");
    CConfigDialog dlg(parent);
    return dlg.Proceed();
}

// ****************************************************************************
//
// CPasswordDialog
//

INT_PTR WINAPI PasswordDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("PasswordDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CPasswordDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        // SalamanderGUI->ArrangeHorizontalLines(hDlg); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary
        dlg = (CPasswordDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CPasswordDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CPasswordDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_PASSWORD),
                          Parent, PasswordDlgProc, (LPARAM)this);
}

INT_PTR
CPasswordDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            *Password = 0;
            SendDlgItemMessage(Dlg, IDC_PASSWORD, WM_GETTEXT, MAX_PASSWORD, (LPARAM)Password);
            if (lstrlen(Password))
                EndDialog(Dlg, IDOK);
            else
                SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ENTERPWD), LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;

        case IDALL:
            *Password = 0;
            SendDlgItemMessage(Dlg, IDC_PASSWORD, WM_GETTEXT, MAX_PASSWORD, (LPARAM)Password);
            if (lstrlen(Password))
                EndDialog(Dlg, IDALL);
            else
                SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ENTERPWD), LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
            return TRUE;

        case IDSKIP:
            EndDialog(Dlg, IDSKIP);
            return TRUE;

        case IDSKIPALL:
            EndDialog(Dlg, IDSKIPALL);
            return TRUE;

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return FALSE;
        }
        break;

    case WM_DESTROY:
        SubClassStatic(IDS_FILENAME, FALSE);
        return TRUE;
    }
    return FALSE;
}

BOOL CPasswordDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CPasswordDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SendDlgItemMessage(Dlg, IDC_LOCK_ICON, STM_SETIMAGE, IMAGE_ICON,
                       (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_LOCK)));
    SubClassStatic(IDS_FILENAME, TRUE);
    SendDlgItemMessage(Dlg, IDS_FILENAME, WM_SETTEXT, 0, (LPARAM)FileName);
    SendDlgItemMessage(Dlg, IDC_PASSWORD, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    if (Flags & PD_NOSKIP)
        EnableWindow(GetDlgItem(Dlg, IDSKIP), FALSE);
    if (Flags & PD_NOSKIPALL)
        EnableWindow(GetDlgItem(Dlg, IDSKIPALL), FALSE);
    if (Flags & PD_NOALL)
    {
        HWND ok = GetDlgItem(Dlg, IDOK);
        HWND all = GetDlgItem(Dlg, IDALL);
        SendMessage(Dlg, DM_SETDEFID, IDOK, 0);
        SendMessage(all, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        SendMessage(ok, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        EnableWindow(all, FALSE);
    }

    CenterDlgToParent();
    return TRUE;
}

INT_PTR PasswordDialog(HWND parent, const char* fileName, char* password, DWORD flags)
{
    CALL_STACK_MESSAGE3("PasswordDialog(, %s, , 0x%X)", fileName, flags);
    CPasswordDialog dlg(parent, fileName, password, flags);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}

// ****************************************************************************
//
// CAttentionDialog
//

INT_PTR WINAPI AttentionDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("AttentionDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CAttentionDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        // SalamanderGUI->ArrangeHorizontalLines(hDlg); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary
        dlg = (CAttentionDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CAttentionDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CAttentionDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_WARNING),
                          Parent, AttentionDlgProc, (LPARAM)this);
}

INT_PTR
CAttentionDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CAttentionDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        CenterDlgToParent();
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return FALSE;
        }
        break;
    }
    return FALSE;
}

BOOL CAttentionDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAttentionDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Config.Options |= OP_NO_VOL_ATTENTION;

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR AttentionDialog(HWND parent)
{
    CALL_STACK_MESSAGE1("AttentionDialog()");
    CAttentionDialog dlg(parent);
    HWND mainWnd = SalamanderGeneral->GetWndToFlash(parent);
    INT_PTR ret = dlg.Proceed();
    if (mainWnd != NULL)
        FlashWindow(mainWnd, FALSE);
    return ret;
}
