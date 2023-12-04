// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "array2.h"

#include "fdi.h"
#include "uncab.h"
#include "dialogs.h"

#include "uncab.rh"
#include "uncab.rh2"
#include "lang\lang.rh"

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
        OrigTextControlProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LONG_PTR)TextControlProc);
    else
        SetWindowLongPtr(GetDlgItem(Dlg, wID), GWLP_WNDPROC, (LONG_PTR)OrigTextControlProc);
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
    char buf[1024];
    sprintf(buf, LoadStr(IDS_NEXTVOLTEXT), CabNumber);
    SendDlgItemMessage(Dlg, IDC_TEXT, WM_SETTEXT, 0, (LPARAM)buf);
    SendDlgItemMessage(Dlg, IDC_DISKNAME, WM_SETTEXT, 0, (LPARAM)DiskName);
    SendDlgItemMessage(Dlg, IDC_CABNAME, WM_SETTEXT, 0, (LPARAM)VolumeName);
    SendDlgItemMessage(Dlg, IDC_FILENAME, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    SendDlgItemMessage(Dlg, IDC_FILENAME, WM_SETTEXT, 0, (LPARAM)VolumePath);

    CenterDlgToParent();
    return TRUE;
}

int CALLBACK DirectoryBrowse(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    CALL_STACK_MESSAGE4("DirectoryBrowse(, 0x%X, 0x%IX, 0x%IX)", uMsg, lParam,
                        lpData);
    if (uMsg == BFFM_INITIALIZED)
    {
        SetWindowText(hwnd, LoadStr(IDS_BROWSEARCHIVETITLE));
        char buf[MAX_PATH];
        SalamanderGeneral->GetRootPath(buf, (char*)lpData);
        SalamanderGeneral->SalPathRemoveBackslash(buf);
        SalamanderGeneral->SalPathRemoveBackslash((char*)lpData);
        if (lstrlen(buf) == lstrlen((char*)lpData)) // je to root-dir
            SalamanderGeneral->SalPathAddBackslash((char*)lpData, MAX_PATH);
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }
    return 0;
}

BOOL CNextVolumeDialog::OnBrowse(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CNextVolumeDialog::OnBrowse(0x%X, 0x%X, )", wNotifyCode,
                        wID);
    char path[MAX_PATH];

    GetDlgItemText(Dlg, IDC_FILENAME, VolumePath, MAX_PATH);

    BROWSEINFO bi;
    bi.hwndOwner = Dlg;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = path;
    char buf[1024];
    sprintf(buf, LoadStr(IDS_BROWSEFOLDERTEXT), VolumeName);
    bi.lpszTitle = buf;
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    bi.lpfn = DirectoryBrowse;
    bi.lParam = (LPARAM)VolumePath;
    LPITEMIDLIST res = SHBrowseForFolder(&bi);
    if (res != NULL)
    {
        SHGetPathFromIDList(res, VolumePath);
        SetDlgItemText(Dlg, IDC_FILENAME, VolumePath);
    }
    // uvolneni item-id-listu
    IMalloc* alloc;
    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
    {
        if (alloc->DidAlloc(res) == 1)
            alloc->Free(res);
        alloc->Release();
    }

    return TRUE;
}

BOOL CNextVolumeDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CNextVolumeDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    GetDlgItemText(Dlg, IDC_FILENAME, VolumePath, MAX_PATH);
    SalamanderGeneral->SalPathAddBackslash(VolumePath, MAX_PATH);

    char fullName[MAX_PATH];
    strcpy(fullName, VolumePath);
    SalamanderGeneral->SalPathAppend(fullName, VolumeName, MAX_PATH);

    SalamanderGeneral->SalUpdateDefaultDir(TRUE);
    int err;
    if (!SalamanderGeneral->SalGetFullName(fullName, &err, CurrentPath))
    {
        char buffer[100];
        SalamanderGeneral->SalMessageBox(Dlg, SalamanderGeneral->GetGFNErrorText(err, buffer, 100),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        return TRUE;
    }

    DWORD attr = SalamanderGeneral->SalGetFileAttributes(fullName);
    if (attr == 0xFFFFFFFF || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_NOTFOUND),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONERROR);
        return TRUE;
    }

    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR NextVolumeDialog(HWND parent, char* volumeName, char* volumePath, char* diskName, int cabNumber)
{
    CALL_STACK_MESSAGE1("NextVolumeDialog(, )");
    CNextVolumeDialog dlg(parent, volumeName, volumePath, diskName, cabNumber);
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
        case IDSKIP:
            if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
                Options |= OP_SKIPCONTINUED;
            EndDialog(Dlg, IDSKIP);
            return FALSE;

        case IDALL:
            if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
                Options |= OP_SKIPCONTINUED;
            EndDialog(Dlg, IDALL);
            return FALSE;

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

/*
BOOL 
CContinuedFileDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
  if (SendDlgItemMessage(Dlg, IDC_DONTSHOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
    Options |= OP_SKIPCONTINUED;
  
  EndDialog(Dlg, IDOK);
  return TRUE;
}
*/

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
    SendDlgItemMessage(Dlg, IDC_SKIPCONTINUED, BM_SETCHECK, Options & OP_SKIPCONTINUED ? BST_CHECKED : BST_UNCHECKED, 0);
    SendDlgItemMessage(Dlg, IDC_NOVOLATTENTION, BM_SETCHECK, Options & OP_NO_VOL_ATTENTION ? BST_CHECKED : BST_UNCHECKED, 0);

    CenterDlgToParent();
    return TRUE;
}

BOOL CConfigDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CConfigDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    if (SendDlgItemMessage(Dlg, IDC_SKIPCONTINUED, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Options |= OP_SKIPCONTINUED;
    else
        Options &= ~OP_SKIPCONTINUED;
    if (SendDlgItemMessage(Dlg, IDC_NOVOLATTENTION, BM_GETCHECK, 0, 0) == BST_CHECKED)
        Options |= OP_NO_VOL_ATTENTION;
    else
        Options &= ~OP_NO_VOL_ATTENTION;

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
        Options |= OP_NO_VOL_ATTENTION;

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
