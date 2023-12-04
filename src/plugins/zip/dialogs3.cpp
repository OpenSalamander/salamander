// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <stdio.h>

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "dbg.h"

#include "array2.h"

#include "selfextr\\comdefs.h"
#include "config.h"
#include "typecons.h"
#include "zip.rh"
#include "zip.rh2"
#include "lang\lang.rh"
#include "chicon.h"
#include "common.h"
#include "add_del.h"
#include "dialogs.h"
//#include "prevsfx.h"

//******************************************************************************
//
// CCommentDialog
//

WNDPROC OrigEditControlProc;

LRESULT CALLBACK EditControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("EditControlProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        TEXTMETRIC tm;
        HGDIOBJ hOldFont = SelectObject((HDC)wParam, (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0));
        GetTextMetrics((HDC)wParam, &tm);
        SelectObject((HDC)wParam, hOldFont);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int clwidth = rc.right - rc.left;
        rc.top = (LONG)((SendMessage(hWnd, EM_GETLINECOUNT, 0, 0) -
                         SendMessage(hWnd, EM_GETFIRSTVISIBLELINE, 0, 0)) *
                            tm.tmHeight +
                        1);
        if (rc.top < rc.bottom)
        {
            FillRect((HDC)wParam, &rc, (HBRUSH)(COLOR_WINDOW + 1));
            rc.bottom = rc.top;
        }
        else
        {
            rc.top = rc.bottom - (rc.bottom - 1) % tm.tmHeight;
            if (rc.top < rc.bottom)
            {
                FillRect((HDC)wParam, &rc, (HBRUSH)(COLOR_WINDOW + 1));
                rc.bottom = rc.top;
            }
        }
        rc.top = 1;
        int m = (int)SendMessage(hWnd, EM_GETMARGINS, 0, 0);
        rc.left = 0;
        rc.right = LOWORD(m);
        FillRect((HDC)wParam, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        rc.left = clwidth - HIWORD(m);
        rc.right = clwidth;
        FillRect((HDC)wParam, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        rc.left = 0;
        rc.right = clwidth;
        rc.top = 0;
        rc.bottom = 1;
        FillRect((HDC)wParam, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        return TRUE;
    }
    }
    return CallWindowProc(OrigEditControlProc, hWnd, uMsg, wParam, lParam);
}

INT_PTR WINAPI CommentDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CommentDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static CCommentDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CCommentDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CCommentDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CCommentDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_COMMENT),
                          Parent, CommentDlgProc, (LPARAM)this);
}

INT_PTR
CCommentDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCommentDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);

    case WM_SHOWWINDOW:
        if (FirstShow)
        {
            // zrusime vyber celeho textu
            SendMessage(CommentHWnd, EM_SETSEL, 0, 0);
            FirstShow = FALSE;
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            return OnCancel();

        //menu
        case CM_SAVE:
            Save();
            return TRUE;

        case CM_CLOSE:
            return OnCancel();
        }
        break;

    case WM_SIZE:
    {
        switch (wParam)
        {
        case SIZE_MAXIMIZED:
        case SIZE_RESTORED:
            SetWindowPos(CommentHWnd, 0, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE | SWP_NOZORDER);
        }
        return TRUE;
    }

    case WM_DESTROY:
    {
        SetWindowLongPtr(CommentHWnd, GWLP_WNDPROC, (LONG_PTR)OrigEditControlProc);
    }
    }
    return FALSE;
}

BOOL CCommentDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CCommentDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    CommentHWnd = GetDlgItem(Dlg, IDC_COMMENT);

    OrigEditControlProc = (WNDPROC)SetWindowLongPtr(CommentHWnd, GWLP_WNDPROC, (LONG_PTR)EditControlProc);

    SetDlgItemText(Dlg, IDC_COMMENT, PackObject->Comment);
    SendDlgItemMessage(Dlg, IDC_COMMENT, EM_SETLIMITTEXT, MAX_ZIPCOMMENT - 1, 0);

    char title[MAX_PATH + 128];
    sprintf(title, LoadStr(IDS_COMMENTDLGTITLE), PackObject->ZipName);
    SetWindowText(Dlg, title);
    SendMessage(Dlg, WM_SETICON, ICON_SMALL, (WPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_COMMENT)));

    // zadisablime polozku 'save'
    if (PackObject->MultiVol)
    {
        HMENU menu = GetMenu(Dlg);
        if (menu)
        {
            MENUITEMINFO mi;
            memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(mi);
            mi.fMask = MIIM_STATE;
            mi.fState = MFS_DISABLED;
            SetMenuItemInfo(menu, CM_SAVE, FALSE, &mi);
        }
    }
    // nastavime tenky ramecek
    LONG style = GetWindowLong(CommentHWnd, GWL_EXSTYLE);
    if (style)
    {
        style = style & ~(LONG)WS_EX_CLIENTEDGE | WS_EX_STATICEDGE;
        SetWindowLong(CommentHWnd, GWL_EXSTYLE, style);
        //SetWindowPos(hwndPath, 0, 0, 0, w, ShortPathControlHeigth, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    RECT r;
    if (GetWindowRect(Parent, &r))
    {
        SetWindowPos(Dlg, 0, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER);
    }

    //CenterDlgToParent();
    return TRUE;
}

BOOL CCommentDialog::OnCancel()
{
    CALL_STACK_MESSAGE1("CCommentDialog::OnCancel()");
    if (!PackObject->MultiVol && SendMessage(CommentHWnd, EM_GETMODIFY, 0, 0))
    {
        switch (SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_COMMENTMODIFIED), LoadStr(IDS_PLUGINNAME), MB_YESNOCANCEL | MB_ICONEXCLAMATION))
        {
        case IDYES:
            if (!Save())
                return TRUE;
        case IDNO:
            break;
        default:
            return TRUE;
        }
    }

    EndDialog(Dlg, IDOK);
    return TRUE;
}

BOOL CCommentDialog::Save()
{
    CALL_STACK_MESSAGE1("CCommentDialog::Save()");
    if (PackObject->MultiVol)
        return FALSE;
    if (GetDlgItemText(Dlg, IDC_COMMENT, PackObject->Comment, MAX_ZIPCOMMENT - 1) <= 0)
    {
        PackObject->Comment[0] = 0;
    }
    int ret = PackObject->SaveComment();
    if (ret)
    {
        if (ret != IDS_NODISPLAY)
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(ret), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    SendMessage(CommentHWnd, EM_SETMODIFY, FALSE, 0);

    return TRUE;
}

//******************************************************************************
//
// CWaitForDialog
//

INT_PTR WINAPI WaitForDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("WaitForDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static CWaitForDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CWaitForDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CWaitForDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CWaitForDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_WAITFOR),
                          Parent, WaitForDlgProc, (LPARAM)this);
}

INT_PTR
CWaitForDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CWaitForDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            return OnOK();

        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;

        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_WAITFOR, FALSE);
            return TRUE;
        }
        break;

    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_WAITFOR, FALSE);
        return TRUE;
    }
    return FALSE;
}

BOOL CWaitForDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CWaitForDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    SetDlgItemText(Dlg, IDC_FILENAME, WaitFor);
    SendDlgItemMessage(Dlg, IDC_FILENAME, EM_SETLIMITTEXT, MAX_PATH - 1, 0);

    CenterDlgToParent();
    return TRUE;
}

BOOL CWaitForDialog::OnOK()
{
    CALL_STACK_MESSAGE1("CWaitForDialog::OnOK()");
    GetDlgItemText(Dlg, IDC_FILENAME, WaitFor, MAX_PATH - 1);
    EndDialog(Dlg, IDOK);
    return TRUE;
}

INT_PTR
WaitForDialog(HWND parent, char* waitFor)
{
    CALL_STACK_MESSAGE1("WaitForDialog(, )");
    CWaitForDialog dlg(parent, waitFor);
    return dlg.Proceed();
}

//******************************************************************************
//
// CChangeTextDialog
//

INT_PTR WINAPI ChangeTextDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ChangeTextDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CChangeTextsDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CChangeTextsDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR
CChangeTextsDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CChangeTextDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CHANGELANG),
                          Parent, ChangeTextDlgProc, (LPARAM)this);
}

INT_PTR
CChangeTextsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeTextsDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(Dlg, IDC_ASK, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
        CenterDlgToParent();
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (SendDlgItemMessage(Dlg, IDC_ASK, BM_GETCHECK, 0, 0) == BST_CHECKED)
                *ChangeLangReaction = CLR_ASK;
            if (SendDlgItemMessage(Dlg, IDC_REPLACE, BM_GETCHECK, 0, 0) == BST_CHECKED)
                *ChangeLangReaction = CLR_REPLACE;
            if (SendDlgItemMessage(Dlg, IDC_LEAVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
                *ChangeLangReaction = CLR_PRESERVE;
            EndDialog(Dlg, IDOK);
            return TRUE;

        case IDCANCEL:
            if (SendDlgItemMessage(Dlg, IDC_ASK, BM_GETCHECK, 0, 0) == BST_CHECKED)
                *ChangeLangReaction = CLR_ASK;
            if (SendDlgItemMessage(Dlg, IDC_REPLACE, BM_GETCHECK, 0, 0) == BST_CHECKED)
                *ChangeLangReaction = CLR_REPLACE;
            if (SendDlgItemMessage(Dlg, IDC_LEAVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
                *ChangeLangReaction = CLR_PRESERVE;
            EndDialog(Dlg, IDCANCEL);
            return TRUE;

        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_CHANGELANG, FALSE);
            return TRUE;
        }
        break;
    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_CHANGELANG, FALSE);
        return TRUE;
    }
    return FALSE;
}

INT_PTR
ChangeTextsDialog(HWND parent, int* changeLangReaction)
{
    CALL_STACK_MESSAGE1("ChangeTextsDialog(, )");
    CChangeTextsDialog dlg(parent, changeLangReaction);
    return dlg.Proceed();
}

//******************************************************************************
//
// other functions
//

BOOL LoadSfxLangs(HWND dlg, char* selectedSfxFile, bool isConfig)
{
    CALL_STACK_MESSAGE2("LoadSfxLangs(, , %d)", isConfig);
    if (!SfxLanguages)
    {
        if (!LoadLangChache(dlg) && !isConfig)
            return FALSE;
    }

    CSfxLang* lang;
    int index;
    char langName[128];
    char selLang[128];
    selLang[0] = 0;

    int i;
    for (i = 0; i < SfxLanguages->Count; i++)
    {
        lang = (*SfxLanguages)[i];
        if (GetLocaleInfo(MAKELCID(MAKELANGID(lang->LangID, SUBLANG_NEUTRAL), SORT_DEFAULT), LOCALE_SLANGUAGE, langName, 128))
        {
            char* c = strchr(langName, ' ');
            if (c)
                *c = 0;
            index = (int)SendDlgItemMessage(dlg, IDC_LANGUAGE, CB_ADDSTRING, 0, (LPARAM)langName);
            if (index != CB_ERR && index != CB_ERRSPACE)
            {
                SendDlgItemMessage(dlg, IDC_LANGUAGE, CB_SETITEMDATA, index, (LPARAM)lang);

                if (lstrcmpi(lang->FileName, selectedSfxFile) == 0)
                {
                    lstrcpy(selLang, langName);
                }
            }
        }
    }

    if (!*selLang ||
        SendDlgItemMessage(dlg, IDC_LANGUAGE, CB_SELECTSTRING, -1, (LPARAM)selLang) == CB_ERR)
    {
        SendDlgItemMessage(dlg, IDC_LANGUAGE, CB_SETCURSEL, 0, 0);
    }

    return TRUE;
}

BOOL LoadLangChache(HWND parent)
{
    CALL_STACK_MESSAGE1("LoadLangChache()");
    char path[MAX_PATH];
    char* file;
    GetModuleFileName(DLLInstance, path, MAX_PATH);
    SalamanderGeneral->CutDirectory(path);
    SalamanderGeneral->SalPathAppend(path, "sfx\\*.sfx", MAX_PATH);
    WIN32_FIND_DATA fd;
    HANDLE find = FindFirstFile(path, &fd);
    if (find == INVALID_HANDLE_VALUE)
    {
        int le = GetLastError();
        if (le == ERROR_FILE_NOT_FOUND || le == ERROR_NO_MORE_FILES)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NOSFXINSTALLED), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        }
        else
        {
            char buffer[512];
            lstrcpy(buffer, LoadStr(IDS_UNABLEREADSFXDIR));
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, le,
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer + lstrlen(buffer),
                          512 - lstrlen(buffer), NULL);
            SalamanderGeneral->SalMessageBox(parent, buffer, LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }

    SfxLanguages = new TIndirectArray2<CSfxLang>(8);
    if (!SfxLanguages)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    SalamanderGeneral->CutDirectory(path);
    SalamanderGeneral->SalPathAddBackslash(path, MAX_PATH);
    file = path + lstrlen(path);

    CSfxLang* lang;
    BOOL ret;
    int le;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            lstrcpy(file, fd.cFileName);
            int r = LoadSfxFileData(path, &lang);
            if (r || !SfxLanguages->Add(lang))
            {
                if (!r)
                {
                    delete lang;
                    r = IDS_LOWMEM;
                }
                if (r == IDS_LOWMEM)
                {
                    SfxLanguages->Destroy();
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
                else
                {
                    char err[512];
                    sprintf(err, LoadStr(r), file);
                    if (SalamanderGeneral->SalMessageBox(parent, err, LoadStr(IDS_ERROR), MB_OKCANCEL | MB_ICONEXCLAMATION) != IDOK)
                    {
                        SfxLanguages->Destroy();
                        break;
                    }
                }
            }
        }

        ret = FindNextFile(find, &fd);
        le = GetLastError();
        if (!ret && le != ERROR_NO_MORE_FILES)
        {
            char buffer[512];
            lstrcpy(buffer, LoadStr(IDS_UNABLEREADSFXDIR));
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, le,
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer + lstrlen(buffer),
                          512 - lstrlen(buffer), NULL);
            SalamanderGeneral->SalMessageBox(parent, buffer, LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            break;
        }
    } while (ret);

    FindClose(find);
    if (SfxLanguages->Count == 0)
    {
        delete SfxLanguages;
        SfxLanguages = NULL;
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_UNABLEREADALLSFX), LoadStr(IDS_ERROR),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    return TRUE;
}

int FormatNumber(__UINT64 number, char* buffer, const char* text)
{
    CALL_STACK_MESSAGE3("FormatNumber(0x%I64X, , %s)", number, text);
    __UINT64 i;
    char buf1[32];
    char* dest;
    char* dest2;
    int digits = 0;

    dest = buf1;
    i = number;
    if (!i)
        *dest++ = '0';
    while (i)
    {
        if (!((digits) % 3) && digits)
            *dest++ = ' ';
        *dest++ = (char)(i % 10 + 0x30);
        digits++;
        i /= 10;
    }
    dest2 = buffer;
    dest--;
    while (dest >= buf1)
        *dest2++ = *dest--;
    *dest2++ = ' ';
    lstrcpy(dest2, text);
    return lstrlen(buffer);
}
