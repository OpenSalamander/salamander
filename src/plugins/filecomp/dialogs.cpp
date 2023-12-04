// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

UINT_PTR CALLBACK
ComDlgHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ComDlgHookProc(, 0x%X, 0x%IX, 0x%IX)", uiMsg, wParam,
                        lParam);
    if (uiMsg == WM_INITDIALOG)
    {
        // SalamanderGUI->ArrangeHorizontalLines(hdlg);  // pro Windows common dialogy tohle nedelame
        CenterWindow(hdlg);
        return 1;
    }
    return 0;
}

// ****************************************************************************
//
// CCompareFilesDialog
//

// historie pro kombace

TCHAR CBHistory[MAX_HISTORY_ENTRIES][MAX_PATH];
int CBHistoryEntries;

void AddToHistory(LPCTSTR path)
{
    CALL_STACK_MESSAGE2(_T("AddToHistory(%s)"), path);
    int toMove = __min(CBHistoryEntries, MAX_HISTORY_ENTRIES - 1);
    int enlarge = 1;
    // podivame jestli uz stejna cesta neni v historii
    int i;
    for (i = 0; i < CBHistoryEntries; i++)
    {
        if (SG->IsTheSamePath(CBHistory[i], path))
        {
            toMove = i;
            enlarge = 0;
            break;
        }
    }
    // vytvorime misto pro cestu kterou budeme ukladat
    int j;
    for (j = toMove; j > 0; j--)
        _tcscpy(CBHistory[j], CBHistory[j - 1]);
    // And store the path...
    _tcscpy(CBHistory[0], path);
    CBHistoryEntries = __min(CBHistoryEntries + enlarge, MAX_HISTORY_ENTRIES);
}

CCompareFilesDialog::CCompareFilesDialog(HWND parent, LPTSTR path1, LPTSTR path2,
                                         BOOL& succes, CCompareOptions* options)
    : CCommonDialog(IDD_COMPAREFILES, parent), Succes(succes)
{
    CALL_STACK_MESSAGE_NONE
    Path1 = path1;
    Path2 = path2;
    Succes = succes;
    Options = options;
}

BOOL FileExists(LPCTSTR path)
{
    CALL_STACK_MESSAGE2(_T("FileExists(%s)"), path);
    DWORD attr = SG->SalGetFileAttributes(path);
    int i = GetLastError();
    return ((attr != 0xffffffff) && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) ||
           ((attr == 0xffffffff) && ((i != ERROR_FILE_NOT_FOUND) && (i != ERROR_PATH_NOT_FOUND)));
}

void CCompareFilesDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCompareFilesDialog::Validate()");
    TCHAR buffer[MAX_PATH];

    int i;
    for (i = 0; i < 2; i++)
    {
        ti.EditLine(IDE_PATH1 + i, buffer, MAX_PATH);
        if (!*buffer)
        {
            SG->SalMessageBox(HWindow, LoadStr(IDS_MISSINGPATH), LoadStr(IDS_ERROR), MB_ICONERROR);
            ti.ErrorOn(IDE_PATH1 + i);
            return;
        }
        if (!FileExists(buffer))
        {
            TCHAR buf2[300 + MAX_PATH];

            wsprintf(buf2, LoadStr(IDS_FILEDOESNOTEXIST), buffer);
            SG->SalMessageBox(HWindow, buf2, LoadStr(IDS_ERROR), MB_ICONERROR);
            ti.ErrorOn(IDE_PATH1 + i);
            return;
        }
    }
}

void CCompareFilesDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCompareFilesDialog::Transfer()");
    ti.EditLine(IDE_PATH1, Path1, MAX_PATH);
    ti.EditLine(IDE_PATH2, Path2, MAX_PATH);
    if (ti.Type == ttDataFromWindow)
    {
        AddToHistory(Path2);
        AddToHistory(Path1);
        Succes = TRUE;
    }
}

/*
UINT CALLBACK 
OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("OFNHookProc(, 0x%X, 0x%IX, 0x%IX)", uiMsg, wParam, lParam);
  if (uiMsg == WM_INITDIALOG)
  {
    // SalamanderGUI->ArrangeHorizontalLines(hdlg);  // pro Windows common dialogy tohle nedelame
    HWND hwnd = GetParent(hdlg);
    CenterWindow(hdlg);
    return 1;
  }
  return 0;
}
*/

LRESULT CCompareFilesDialog::DragDropEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CCompareFilesDialog* pParent = (CCompareFilesDialog*)WindowsManager.GetWindowPtr(GetParent(hWnd));

    if (!pParent)
        return NULL; // What's wrong?

    if (WM_DROPFILES == uMsg)
    {
        HDROP hDrop = (HDROP)wParam;
        TCHAR buffer[MAX_PATH];
        int nFilesDropped = DragQueryFile(hDrop, 0, buffer, MAX_PATH);

        if (nFilesDropped)
        {
            SetWindowText(hWnd, buffer);
        }

        DragFinish(hDrop);
        return 0;
    }

    return CallWindowProc(hWnd == GetDlgItem(pParent->HWindow, IDE_PATH1) ? pParent->OldEditProc1 : pParent->OldEditProc2, hWnd, uMsg, wParam, lParam);
}

INT_PTR
CCompareFilesDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCompareFilesDialog::DialogProc(0x%X, 0x%IX, 0x%IX)",
                        uMsg, wParam, lParam);
    UINT idCB;
    UINT idTitle;

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HWND hWnd1 = GetDlgItem(HWindow, IDE_PATH1), hWnd2 = GetDlgItem(HWindow, IDE_PATH2);

        SG->InstallWordBreakProc(hWnd1); // instalujeme WordBreakProc do comboboxu
        SG->InstallWordBreakProc(hWnd2); // instalujeme WordBreakProc do comboboxu

        // I believe OldEditProc1 and OldEditProc2 are equal. But I am rather paranoic...
        OldEditProc1 = (WNDPROC)GetWindowLongPtr(hWnd1, GWLP_WNDPROC);
        OldEditProc2 = (WNDPROC)GetWindowLongPtr(hWnd2, GWLP_WNDPROC);
        SetWindowLongPtr(hWnd1, GWLP_WNDPROC, (LONG_PTR)DragDropEditProc);
        SetWindowLongPtr(hWnd2, GWLP_WNDPROC, (LONG_PTR)DragDropEditProc);
        DragAcceptFiles(hWnd1, TRUE);
        DragAcceptFiles(hWnd2, TRUE);

        SetWindowPos(HWindow, AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // incializujeme historii
        int i = 0;
        if (CBHistoryEntries > 1)
        {
            // v druhem kombaci ulozime prvni dve cesty v obracenem poradi
            for (; i < 2; i++)
            {
                SendMessage(GetDlgItem(HWindow, IDE_PATH1), CB_ADDSTRING, 0, (LPARAM)CBHistory[i]);
                SendMessage(GetDlgItem(HWindow, IDE_PATH2), CB_ADDSTRING, 0, (LPARAM)CBHistory[1 - i]);
            }
        }
        for (; i < CBHistoryEntries; i++)
        {
            SendMessage(GetDlgItem(HWindow, IDE_PATH1), CB_ADDSTRING, 0, (LPARAM)CBHistory[i]);
            SendMessage(GetDlgItem(HWindow, IDE_PATH2), CB_ADDSTRING, 0, (LPARAM)CBHistory[i]);
        }

        SendMessage(HWindow, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FCICO)));

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDADVANCED:
        {
            BOOL setDefault = FALSE;
            CAdvancedOptionsDialog dlg(HWindow, Options, &setDefault);
            if (dlg.Execute() == IDOK && setDefault)
            {
                if (memcmp(Options, &DefCompareOptions, sizeof(*Options)) != 0)
                {
                    DefCompareOptions = *Options;
                    MainWindowQueue.BroadcastMessage(WM_USER_CFGCHNG, CC_DEFOPTIONS | CC_HAVEHWND, (LPARAM)GetParent(HWindow));
                }
            }
            return 0;
        }

        case IDB_BROWSE1:
        case IDB_BROWSE2:
        {
            if (IDB_BROWSE1 == LOWORD(wParam))
            {
                idCB = IDE_PATH1;
                idTitle = IDS_SELECTFIRST;
            }
            else
            {
                idCB = IDE_PATH2;
                idTitle = IDS_SELECTSECOND;
            }

            OPENFILENAME ofn;
            TCHAR path[MAX_PATH];
            TCHAR dir[MAX_PATH];
            TCHAR buf[128];

            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            _stprintf(buf, _T("%s%c*.*%c"), LoadStr(IDS_ALLFILES), 0, 0);
            ofn.lpstrFilter = buf;
            if (!GetDlgItemText(HWindow, idCB, path, SizeOf(path)))
            {
                SendDlgItemMessage(HWindow, idCB, CB_GETLBTEXT, 0, LPARAM(dir));
                SG->CutDirectory(dir);
                ofn.lpstrInitialDir = dir;
            }
            ofn.lpstrFile = path;
            ofn.nMaxFile = SizeOf(path);
            ofn.lpstrTitle = LoadStr(idTitle);
            //ofn.lpfnHook = OFNHookProc;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR /*| OFN_ENABLEHOOK*/;

            if (SG->SafeGetOpenFileName(&ofn))
                SetDlgItemText(HWindow, idCB, path);

            return 0;
        }
        }
        break;
    }

    case WM_USER_CLEARHISTORY:
    {
        TCHAR buffer[MAX_PATH];
        HWND cb = GetDlgItem(HWindow, IDE_PATH1);
        SendMessage(cb, WM_GETTEXT, SizeOf(buffer), (LPARAM)buffer);
        SendMessage(cb, CB_RESETCONTENT, 0, 0);
        SendMessage(cb, WM_SETTEXT, 0, (LPARAM)buffer);
        cb = GetDlgItem(HWindow, IDE_PATH2);
        SendMessage(cb, WM_GETTEXT, SizeOf(buffer), (LPARAM)buffer);
        SendMessage(cb, CB_RESETCONTENT, 0, 0);
        SendMessage(cb, WM_SETTEXT, 0, (LPARAM)buffer);
        break;
    }

    case WM_DESTROY:
        DragAcceptFiles(GetDlgItem(HWindow, IDE_PATH1), FALSE);
        DragAcceptFiles(GetDlgItem(HWindow, IDE_PATH2), FALSE);
        break;
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CCommonPropSheetPage
//

void CCommonPropSheetPage::NotifDlgJustCreated()
{
    SalGUI->ArrangeHorizontalLines(HWindow);
}
