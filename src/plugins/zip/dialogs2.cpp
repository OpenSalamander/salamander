// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <stdio.h>

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
#include "prevsfx.h"

#include "iosfxset.h"

TIndirectArray2<CFavoriteSfx> Favorities(8);
CFavoriteSfx LastUsedSfxSet;

//******************************************************************************
//
// CAdvancedSEDialog
//

INT_PTR WINAPI AdvancedSEDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("AdvancedSEDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CAdvancedSEDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CAdvancedSEDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

int CAdvancedSEDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::Proceed()");

    EnableWindow(Parent, FALSE);

    CreateDialogParam(HLanguage, MAKEINTRESOURCE(IDD_ADVANCEDSE),
                      Parent, AdvancedSEDlgProc, (LPARAM)this);
    if (!Dlg)
        return -1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) // && IsWindow(Dlg))
    {
        if (!TranslateAccelerator(Dlg, Accel, &msg) &&
            !IsDialogMessage(Dlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(Parent, TRUE);

    return Result;
}

INT_PTR
CAdvancedSEDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CAdvancedSEDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        //controls
        /*
        case IDC_CURRENT:
          return OnCurrent(HIWORD(wParam), LOWORD(wParam), (HWND) lParam);
        case IDC_TEMP:
          return OnTemp(HIWORD(wParam), LOWORD(wParam), (HWND) lParam);
        */
        case IDC_REMOVE:
            return OnRemove(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_WAITFOR:
            return OnWaitFor(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_TEXTS:
            return OnTexts(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_TARGETDIR:
            return OnTargetDir(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_SPECDIR:
            return OnSpecDir(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_AUTO:
            return OnAuto(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_CHANGEICON:
            return OnChangeIcon(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_LANGUAGE:
            return OnChangeLanguage(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDCANCEL:
            EndDialog(IDCANCEL);
            return TRUE;
        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_ADVANCEDSE, FALSE);
            return TRUE;

        //spec dir menu
        case CM_TEMP:
        case CM_PROGFILES:
        case CM_WINDIR:
        case CM_SYSDIR:
        case CM_ENVVAR:
        case CM_REGENTRY:
            return OnSpecDirMenu(LOWORD(wParam));
        //menu
        case CM_SFX_EXPORT:
            return OnExport();
        case CM_SFX_IMPORT:
            return OnImport();
        case CM_SFX_PREVIEW:
            return OnPreview();
        case CM_SFX_RESETALL:
            return OnResetAll();
        case CM_SFX_RESETVALUES:
            return OnResetValues();
        case CM_SFX_RESETTEXTS:
            return OnResetTexts();
        case CM_SFX_ADD:
            return OnAddFavorite();
        case CM_SFX_FAVORITIES:
            return OnFavorities();
        case CM_SFX_MANAGE:
            return OnManageFavorities();
        case CM_SFX_LASTUSED:
            return OnLastUsed();

        default:
        {
            if (LOWORD(wParam) >= CM_SFX_FAVORITE &&
                LOWORD(wParam) < CM_SFX_FAVORITE + Favorities.Count)
                return OnFavoriteOption(LOWORD(wParam));
        }
        }
        break;

    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_ADVANCEDSE, FALSE);
        return TRUE;
        /*
    case WM_CONTEXTMENU:
    {
      HMENU  menu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_FAVMANMENU));
      if (menu)
      {
        HMENU subMenu = GetSubMenu(menu, 0);
        if (subMenu)
        {
          int ret = TrackPopupMenuEx(subMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NONOTIFY | TPM_RETURNCMD,
                           LOWORD(lParam), HIWORD(lParam), NULL, NULL);
          ret = GetLastError();
        }
        DestroyMenu(menu);
      }
      break;
    }
    */

    case WM_DESTROY:
        if (SpecDirMenu)
            DestroyMenu(SpecDirMenu);
        if (LargeIcon)
            DestroyIcon(LargeIcon);
        if (SmallIcon)
            DestroyIcon(SmallIcon);
        SubClassSmallIcon(IDC_EXEICON, false);
        PostQuitMessage(0);
        break;

    case WM_USER_GETICON:
        SetWindowLongPtr(Dlg, DWLP_MSGRESULT, (LONG_PTR)SmallIcon);
        return TRUE;
    }
    return FALSE;
}

BOOL CAdvancedSEDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnInit(0x%IX, 0x%IX)", wParam, lParam);
    ExtractIconEx(TmpSfxSettings.IconFile, TmpSfxSettings.IconIndex,
                  &LargeIcon, &SmallIcon, 1);
    SubClassSmallIcon(IDC_EXEICON, true);

    ResetValueControls();

    if (!LoadSfxLangs(Dlg, TmpSfxSettings.SfxFile, false))
    {
        EndDialog(IDCANCEL);
        return TRUE;
    }

    if (!InitMenu())
    {
        EndDialog(IDCANCEL);
        return TRUE;
    }

    LRESULT i = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        CurrentSfxLang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, i, 0);
        if ((LRESULT)CurrentSfxLang == CB_ERR)
        {
            TRACE_E("nepodarilo se ziskat aktualni jazyk z comboboxu");
            CurrentSfxLang = NULL;
        }
    }
    else
    {
        TRACE_E("nepodarilo se ziskat aktualni jazyk z comboboxu");
        CurrentSfxLang = NULL;
    }

    CenterDlgToParent();
    return TRUE;
}

void CAdvancedSEDialog::ResetValueControls()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::ResetValueControls()");
    HWND wnd = GetFocus();
    SetFocus(GetDlgItem(Dlg, IDC_TARGETDIR));
    if (wnd)
    {
        int id = GetDlgCtrlID(wnd);
        if (id == IDC_SPECDIR || id == IDC_WAITFOR || id == IDC_TEXTS || id == IDC_CHANGEICON ||
            id == IDCANCEL || id == IDHELP)
        {
            LONG style = GetWindowLong(wnd, GWL_STYLE);
            SetWindowLong(wnd, GWL_STYLE, (style & ~BS_DEFPUSHBUTTON) | BS_PUSHBUTTON);
            wnd = GetDlgItem(Dlg, IDOK);
            style = GetWindowLong(wnd, GWL_STYLE);
            if (style)
                SetWindowLong(wnd, GWL_STYLE, (style & ~BS_PUSHBUTTON) | BS_DEFPUSHBUTTON);
        }
    }
    SendDlgItemMessage(Dlg, IDC_TARGETDIR, EM_SETLIMITTEXT, 2 * MAX_PATH - 1, 0);
    SendDlgItemMessage(Dlg, IDC_TARGETDIR, WM_SETTEXT, 0, (LPARAM)TmpSfxSettings.TargetDir);
    if (lstrcmpi(TmpSfxSettings.TargetDir, SFX_TDTEMP))
        EnableWindow(GetDlgItem(Dlg, IDC_REMOVE), FALSE);
    if (!(TmpSfxSettings.Flags & SE_NOTALLOWCHANGE))
        SendDlgItemMessage(Dlg, IDC_ALLOWUSER, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_ALLOWUSER, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    if (TmpSfxSettings.Flags & SE_REMOVEAFTER)
    {
        EnableWindow(GetDlgItem(Dlg, IDC_WAITFOR), TRUE);
        SendDlgItemMessage(Dlg, IDC_REMOVE, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    }
    else
    {
        EnableWindow(GetDlgItem(Dlg, IDC_WAITFOR), FALSE);
        SendDlgItemMessage(Dlg, IDC_REMOVE, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    }
    if (TmpSfxSettings.Flags & SE_AUTO)
    {
        SendDlgItemMessage(Dlg, IDC_AUTO, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
        EnableWindow(GetDlgItem(Dlg, IDC_HIDEMAINDLG), TRUE);
    }
    else
    {
        SendDlgItemMessage(Dlg, IDC_AUTO, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
        EnableWindow(GetDlgItem(Dlg, IDC_HIDEMAINDLG), FALSE);
    }
    if (TmpSfxSettings.Flags & SE_HIDEMAINDLG)
        SendDlgItemMessage(Dlg, IDC_HIDEMAINDLG, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_HIDEMAINDLG, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    if (TmpSfxSettings.Flags & SE_SHOWSUMARY)
        SendDlgItemMessage(Dlg, IDC_SUMDLG, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_SUMDLG, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    if (TmpSfxSettings.Flags & SE_OVEWRITEALL)
        SendDlgItemMessage(Dlg, IDC_OVEWRITE, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_OVEWRITE, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    if (TmpSfxSettings.Flags & SE_AUTODIR)
        SendDlgItemMessage(Dlg, IDC_AUTODIR, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_AUTODIR, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    if (TmpSfxSettings.Flags & SE_REQUIRESADMIN)
        SendDlgItemMessage(Dlg, IDC_REQSADMIN, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
    else
        SendDlgItemMessage(Dlg, IDC_REQSADMIN, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
    SendDlgItemMessage(Dlg, IDC_EXECUTE, EM_SETLIMITTEXT, SE_MAX_COMMANDLINE - 1, 0);
    SendDlgItemMessage(Dlg, IDC_EXECUTE, WM_SETTEXT, 0, (LPARAM)TmpSfxSettings.Command);
}

void CAdvancedSEDialog::EndDialog(int result)
{
    CALL_STACK_MESSAGE2("CAdvancedSEDialog::EndDialog(%d)", result);
    EnableWindow(Parent, TRUE);
    DestroyWindow(Dlg);
    Result = result;
}

/*
BOOL CAdvancedSEDialog::OnCurrent(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
  if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    //PackOptions->Flags = (PackOptions->Flags & ~SE_DIRFLAGSMASK) | SE_CURRENTDIR;
    SendDlgItemMessage(Dlg, IDC_REMOVE, BM_SETCHECK, (WPARAM) BST_UNCHECKED, 0);
    EnableWindow(GetDlgItem(Dlg, IDC_REMOVE), FALSE);
  }
  return TRUE;
}

BOOL CAdvancedSEDialog::OnTemp(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
  if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
  {
    //PackOptions->Flags = (PackOptions->Flags & ~SE_DIRFLAGSMASK) | SE_TEMPDIR;
    EnableWindow(GetDlgItem(Dlg, IDC_REMOVE), TRUE);
  }
  return TRUE;
}
*/

BOOL CAdvancedSEDialog::OnTargetDir(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnTargetDir(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    if (wNotifyCode == EN_UPDATE)
    {
        char buffer[2 * MAX_PATH];
        SendDlgItemMessage(Dlg, IDC_TARGETDIR, WM_GETTEXT, 2 * MAX_PATH, (LPARAM)buffer);
        //orezeme mezery na konci
        char* sour = buffer + lstrlen(buffer);
        while (--sour >= buffer && *sour == ' ')
            ;
        sour[1] = 0;
        if (lstrcmpi(buffer, SFX_TDTEMP) == 0)
        {
            EnableWindow(GetDlgItem(Dlg, IDC_REMOVE), TRUE);
        }
        else
        {
            SendDlgItemMessage(Dlg, IDC_REMOVE, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
            EnableWindow(GetDlgItem(Dlg, IDC_REMOVE), FALSE);
            EnableWindow(GetDlgItem(Dlg, IDC_WAITFOR), FALSE);
        }
        return TRUE;
    }
    return FALSE;
}

BOOL CAdvancedSEDialog::OnSpecDir(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnSpecDir()");

    if (!SpecDirMenu)
        SpecDirMenu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SPECDIR));
    if (!SpecDirMenu)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ERRLOADMENU), LoadStr(IDS_ERROR),
                                         MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    if (!SpecDirMenuPopup)
        SpecDirMenuPopup = GetSubMenu(SpecDirMenu, 0);
    if (SpecDirMenuPopup)
    {
        RECT r;
        GetWindowRect(hwndCtl, &r);
        TrackPopupMenuEx(SpecDirMenuPopup, TPM_LEFTALIGN | TPM_TOPALIGN, r.right, r.top, Dlg, NULL);
    }
    return TRUE;
}

BOOL CAdvancedSEDialog::OnSpecDirMenu(WORD itemID)
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnSpecDirMenu()");

    const char* string;
    switch (itemID)
    {
    case CM_TEMP:
        string = SFX_TDTEMP;
        break;
    case CM_PROGFILES:
        string = SFX_TDPROGFILES;
        break;
    case CM_WINDIR:
        string = SFX_TDWINDIR;
        break;
    case CM_SYSDIR:
        string = SFX_TDSYSDIR;
        break;
    case CM_ENVVAR:
        string = SFX_TDENVVAR;
        break;
    case CM_REGENTRY:
        string = SFX_TDREGVAL;
        break;
    }
    SendDlgItemMessage(Dlg, IDC_TARGETDIR, EM_REPLACESEL, TRUE, (LPARAM)string);
    if (itemID == CM_ENVVAR || itemID == CM_REGENTRY)
    {
        DWORD start, end;
        SendDlgItemMessage(Dlg, IDC_TARGETDIR, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        SendDlgItemMessage(Dlg, IDC_TARGETDIR, EM_SETSEL, (WPARAM)end - 1, (LPARAM)end - 1);
    }
    SetFocus(GetDlgItem(Dlg, IDC_TARGETDIR));
    HWND button = GetDlgItem(Dlg, IDC_SPECDIR);
    LONG style = GetWindowLong(button, GWL_STYLE);
    if (style)
        SetWindowLong(button, GWL_STYLE, (style & ~BS_DEFPUSHBUTTON) | BS_PUSHBUTTON);
    button = GetDlgItem(Dlg, IDOK);
    style = GetWindowLong(button, GWL_STYLE);
    if (style)
        SetWindowLong(button, GWL_STYLE, (style & ~BS_PUSHBUTTON) | BS_DEFPUSHBUTTON);
    return TRUE;
}

BOOL CAdvancedSEDialog::OnRemove(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnRemove(0x%X, 0x%X, )", wNotifyCode,
                        wID);
    if (SendMessage(hwndCtl, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        //PackOptions->Flags = (PackOptions->Flags & ~SE_DIRFLAGSMASK) | SE_TEMPDIR;
        EnableWindow(GetDlgItem(Dlg, IDC_WAITFOR), TRUE);
    }
    else
    {
        SendDlgItemMessage(Dlg, IDC_HIDEMAINDLG, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
        EnableWindow(GetDlgItem(Dlg, IDC_WAITFOR), FALSE);
    }
    return TRUE;
}

BOOL CAdvancedSEDialog::OnWaitFor(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnWaitFor(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    WaitForDialog(Dlg, TmpSfxSettings.WaitFor);
    return TRUE;
}

BOOL CAdvancedSEDialog::OnTexts(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnText()");
    LRESULT i = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, i, 0);
        if ((LRESULT)lang != CB_ERR)
        {
            CSfxTextsDialog dlg(Dlg, &TmpSfxSettings, lang);
            dlg.Proceed();
        }
        else
            TRACE_E("nepodarilo se ziskat aktualni jazyk z comboboxu");
    }
    else
        TRACE_E("nepodarilo se ziskat aktualni jazyk z comboboxu");

    return TRUE;
}

BOOL CAdvancedSEDialog::OnAuto(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnAuto(0x%X, 0x%X, )", wNotifyCode,
                        wID);
    if (SendDlgItemMessage(Dlg, wID, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        //PackOptions->Flags = (PackOptions->Flags & ~SE_DIRFLAGSMASK) | SE_TEMPDIR;
        EnableWindow(GetDlgItem(Dlg, IDC_HIDEMAINDLG), TRUE);
    }
    else
    {
        SendDlgItemMessage(Dlg, IDC_HIDEMAINDLG, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
        EnableWindow(GetDlgItem(Dlg, IDC_HIDEMAINDLG), FALSE);
    }

    return TRUE;
}

typedef BOOL(WINAPI* FPickIconDlg)(HWND hwndOwner, LPSTR lpstrFile,
                                   DWORD nMaxFile, LPDWORD lpdwIconIndex);

BOOL CAdvancedSEDialog::OnChangeIcon(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnChangeIcon(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    int errorID = 0;
    HINSTANCE Shell32DLL = LoadLibrary("shell32.dll");
    if (Shell32DLL)
    {
        FPickIconDlg PickIconDlg = (FPickIconDlg)GetProcAddress(Shell32DLL, (LPCSTR)62); // Min: XP (shell32.dll version 6.0)
        if (PickIconDlg)
        {
            char file[MAX_PATH];
            DWORD index = TmpSfxSettings.IconIndex;
            WCHAR wfile[MAX_PATH];

            lstrcpy(file, TmpSfxSettings.IconFile);
            MultiByteToWideChar(CP_ACP, 0, file, -1, wfile, MAX_PATH);
            wfile[MAX_PATH - 1] = 0;

            if (PickIconDlg(Dlg, (LPSTR)wfile, MAX_PATH, &index))
            {
                WideCharToMultiByte(CP_ACP, 0, wfile, -1, file, MAX_PATH, NULL, NULL);
                file[MAX_PATH - 1] = 0;

                /*
        if (file[0] == '%')
        {
          char buf[MAX_PATH];
          char * c = strchr(file + 1, '%');
          if (c)
          {
            *c = 0;
            int i = GetEnvironmentVariable(file + 1, buf, MAX_PATH);
            if (i && i <= MAX_PATH)
            {
              if (PathAppend(buf, ++c)) lstrcpy(file, buf);
            }
          }
        }
        */
                char buf[MAX_PATH];
                DWORD ret = ExpandEnvironmentStrings(file, buf, MAX_PATH);
                if (ret != 0 && ret <= MAX_PATH)
                    lstrcpy(file, buf);

                HICON iconLarge, iconSmall;
                CIcon* icons;
                int count;
                switch (LoadIcons(file, index, &icons, &count))
                {
                case 1:
                    errorID = IDS_ERROPENICO;
                    break;
                case 2:
                    errorID = IDS_ERRLOADLIB;
                    break;
                case 3:
                    errorID = IDS_ERRLOADLIB2;
                    break;
                case 4:
                    errorID = IDS_ERRLOADICON;
                    break;
                }
                if (!errorID)
                {
                    if (ExtractIconEx(file, index, &iconLarge, &iconSmall, 1))
                    {
                        lstrcpy(TmpSfxSettings.IconFile, file);
                        TmpSfxSettings.IconIndex = index;
                        if (SmallIcon)
                            DestroyIcon(SmallIcon);
                        if (LargeIcon)
                            DestroyIcon(LargeIcon);
                        SmallIcon = iconSmall;
                        LargeIcon = iconLarge;
                        if (PackOptions->Icons)
                            DestroyIcons(PackOptions->Icons, PackOptions->IconsCount);
                        PackOptions->Icons = icons;
                        PackOptions->IconsCount = count;
                        InvalidateRect(GetDlgItem(Dlg, IDC_EXEICON), NULL, TRUE);
                        UpdateWindow(GetDlgItem(Dlg, IDC_EXEICON));
                    }
                    else
                    {
                        DestroyIcons(icons, count);
                        errorID = IDS_ERRLOADICON;
                    }
                }
            }
        }
        else
            errorID = IDS_ERRGETPROCADDRESS;
    }
    else
        errorID = IDS_ERRLAODSHELLDLL;
    if (errorID)
    {
        char buffer[1024];
        int e = GetLastError();
        SalamanderGeneral->SalMessageBox(Dlg, FormatMessage(buffer, errorID, e),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
    }
    if (Shell32DLL)
        FreeLibrary(Shell32DLL);
    return TRUE;
}

BOOL CAdvancedSEDialog::OnChangeLanguage(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnChangeLanguage(0x%X, 0x%X, )",
                        wNotifyCode, wID);

    if (wNotifyCode == CBN_SELENDOK)
    {
        LRESULT i = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
        if (i != CB_ERR)
        {
            CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, i, 0);
            if ((LRESULT)lang != CB_ERR)
            {
                if (CurrentSfxLang && CurrentSfxLang != lang)
                {
                    if (lstrcmpi(CurrentSfxLang->DlgTitle, TmpSfxSettings.Title) == 0 &&
                        lstrcmpi(CurrentSfxLang->DlgText, TmpSfxSettings.Text) == 0 &&
                        lstrcmpi(CurrentSfxLang->ButtonText, TmpSfxSettings.ExtractBtnText) == 0 &&
                        lstrcmpi(CurrentSfxLang->Vendor, TmpSfxSettings.Vendor) == 0 &&
                        lstrcmpi(CurrentSfxLang->WWW, TmpSfxSettings.WWW) == 0 &&
                        (TmpSfxSettings.MBoxText == NULL || *TmpSfxSettings.MBoxText == 0) &&
                        *TmpSfxSettings.MBoxTitle == 0)
                    {
                        goto REPLACE;
                    }
                    else
                    {
                        switch (*ChangeLangReaction)
                        {
                        case CLR_ASK:
                            if (ChangeTextsDialog(Dlg, ChangeLangReaction) != IDOK)
                                break;
                        case CLR_REPLACE:
                        {
                        REPLACE:
                            strcpy(TmpSfxSettings.Title, lang->DlgTitle);
                            strcpy(TmpSfxSettings.Text, lang->DlgText);
                            strcpy(TmpSfxSettings.ExtractBtnText, lang->ButtonText);
                            strcpy(TmpSfxSettings.Vendor, lang->Vendor);
                            strcpy(TmpSfxSettings.WWW, lang->WWW);
                            break;
                        }
                        }
                    }
                }
                CurrentSfxLang = lang;
            }
        }
    }

    return TRUE;
}

BOOL CAdvancedSEDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CAdvancedSEDialog::OnOK(0x%X, 0x%X, )", wNotifyCode, wID);
    if (GetSettings(&TmpSfxSettings))
    {
        LRESULT i = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
        if (i != CB_ERR)
        {
            CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, i, 0);
            if ((LRESULT)lang != CB_ERR)
            {
                char buffer[2048];
                buffer[0] = 0;
                if (strcmp(TmpSfxSettings.Vendor, lang->Vendor) != 0 || strcmp(TmpSfxSettings.WWW, lang->WWW) != 0)
                    sprintf(buffer, "%s\r\n%s\r\n\r\n", lang->Vendor, lang->WWW);
                strcat_s(buffer, lang->AboutLicenced);
                lstrcpyn(PackOptions->About, buffer, SE_MAX_ABOUT);
            }
        }

        PackOptions->SfxSettings = TmpSfxSettings;

        EndDialog(IDOK);
    }

    return TRUE;
}

BOOL CAdvancedSEDialog::GetSettings(CSfxSettings* sfxSettings)
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::GetSettings()");
    CSfxSettings settings = TmpSfxSettings; // aby se zkopirovalo jmeno
    //settings = *sfxSettings;// to abychom si neprepsaly IconFile a IconIndex
    settings.Flags = 0;
    SendDlgItemMessage(Dlg, IDC_TARGETDIR, WM_GETTEXT, 2 * MAX_PATH, (LPARAM)settings.TargetDir);
    //orezeme mezery na konci
    char* sour = settings.TargetDir + lstrlen(settings.TargetDir);
    while (--sour >= settings.TargetDir && *sour == ' ')
        ;
    sour[1] = 0;
    //overime syntax
    DWORD ret = ParseTargetDir(settings.TargetDir, NULL, NULL, NULL, NULL, NULL);
    if (ret)
    {
        switch (LOWORD(ret))
        {
        case 1:
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADTEMP), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            break;

        case 2:
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_MISBAR), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            break;

        case 3:
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADVAR), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            break;

        case 4:
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADKEY), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            break;
        }
        SendDlgItemMessage(Dlg, IDC_TARGETDIR, EM_SETSEL, HIWORD(ret), HIWORD(ret));
        SetFocus(GetDlgItem(Dlg, IDC_TARGETDIR));
        return FALSE;
    }
    if (SendDlgItemMessage(Dlg, IDC_ALLOWUSER, BM_GETCHECK, 0, 0) != BST_CHECKED)
    {
        settings.Flags |= SE_NOTALLOWCHANGE;
    }
    SendDlgItemMessage(Dlg, IDC_EXECUTE, WM_GETTEXT, SE_MAX_COMMANDLINE, (LPARAM)settings.Command);
    if (SendDlgItemMessage(Dlg, IDC_REMOVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_REMOVEAFTER;
    }
    LRESULT i = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, i, 0);
        if ((LRESULT)lang != CB_ERR)
        {
            lstrcpy(settings.SfxFile, lang->FileName);
        }
    }
    if (SendDlgItemMessage(Dlg, IDC_HIDEMAINDLG, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_HIDEMAINDLG;
    }
    if (SendDlgItemMessage(Dlg, IDC_AUTO, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_AUTO;
    }
    if (SendDlgItemMessage(Dlg, IDC_SUMDLG, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_SHOWSUMARY;
    }
    if (SendDlgItemMessage(Dlg, IDC_OVEWRITE, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_OVEWRITEALL;
    }
    if (SendDlgItemMessage(Dlg, IDC_AUTODIR, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_AUTODIR;
    }
    if (SendDlgItemMessage(Dlg, IDC_REQSADMIN, BM_GETCHECK, 0, 0) == BST_CHECKED)
    {
        settings.Flags |= SE_REQUIRESADMIN;
    }
    *sfxSettings = settings;
    return TRUE;
}

// porovna dva stringy a ignoruje sigle '&'
int CompareMenuItems(char* name1, char* name2)
{
    CALL_STACK_MESSAGE1("CompareMenuItems(, )");
    char buf1[MAX_FAVNAME];
    char buf2[MAX_FAVNAME];

    //odstranime single '&'
    char* sour = name1;
    char* dest = buf1;

    while (*(*sour == '&' ? sour++ : sour))
    {
        *dest++ = *sour++;
    }
    *dest = 0;

    sour = name2;
    dest = buf2;
    while (*(*sour == '&' ? sour++ : sour))
    {
        *dest++ = *sour++;
    }
    *dest = 0;

    return SalamanderGeneral->StrICmp(buf1, buf2);

    /* tohle bylo case sensitive, ale jiank OK
  int ret = 0 ;

  while (!(ret = *(unsigned char *)(*name1 == '&' ? name1++ : name1) -
                 *(unsigned char *)(*name2 == '&' ? name2++ : name2)) && *name2)
      ++name1, ++name2;

  if (ret < 0)
    ret = -1;
  else
  {
    if (ret > 0)
      ret = 1;
  }

  return ret;
*/
}

void SortFavoriteSettings(int left, int right)
{
    CALL_STACK_MESSAGE_NONE
    int i = left, j = right;
    char* pivot = Favorities[(i + j) / 2]->Name;
    do
    {
        while (CompareMenuItems(Favorities[i]->Name, pivot) < 0 && i < right)
            i++;
        while (CompareMenuItems(pivot, Favorities[j]->Name) < 0 && j > left)
            j--;
        if (i <= j)
        {
            CFavoriteSfx* tmp = Favorities[i];
            Favorities[i] = Favorities[j];
            Favorities[j] = tmp;
            i++;
            j--;
        }
    } while (i <= j); //musej bejt shodny?
    if (left < j)
        SortFavoriteSettings(left, j);
    if (i < right)
        SortFavoriteSettings(i, right);
}

BOOL CAdvancedSEDialog::InitMenu()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::InitMenu()");
    Accel = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_SFXACCELS));
    if (!Accel)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ERRLOADACCELS), LoadStr(IDS_ERROR),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    Menu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SFXMENU));
    if (!Menu)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ERRLOADMENU), LoadStr(IDS_ERROR),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    CreateFavoritesMenu();

    MENUITEMINFO mi;
    if (!*LastUsedSfxSet.Name)
    {
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_STATE;
        mi.fState = MFS_DISABLED;
        SetMenuItemInfo(Menu, CM_SFX_LASTUSED, FALSE, &mi);
    }

    // zajistime aby mnelo okno spravnou velikost i po pridani menu
    RECT wr, cr1, cr2;
    GetClientRect(Dlg, &cr1);

    SetMenu(Dlg, Menu);

    GetClientRect(Dlg, &cr2);
    GetWindowRect(Dlg, &wr);
    LONG w = wr.right - wr.left;
    LONG h = wr.bottom - wr.top + cr1.bottom - cr2.bottom;
    SetWindowPos(Dlg, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);

    return TRUE;
}

BOOL CAdvancedSEDialog::CreateFavoritesMenu()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::CreateFavoritesMenu()");
    MENUITEMINFO mi;
    if (FavoritiesMenu)
    {
        // odebereme submenu polozce 'Favorities'
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_SUBMENU;
        mi.hSubMenu = NULL;
        SetMenuItemInfo(Menu, CM_SFX_FAVORITIES, FALSE, &mi);
        DestroyMenu(FavoritiesMenu);
    }
    FavoritiesMenu = CreatePopupMenu();
    if (!FavoritiesMenu)
    {
        TRACE_E("Nedostali jsme FavoritiesMenu ve funkci InitMenu.");
        return FALSE;
    }
    if (Favorities.Count == 0)
    {
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mi.fType = MFT_STRING;
        mi.fState = MFS_DISABLED;
        mi.wID = CM_SFX_FAVORITE;
        mi.dwTypeData = LoadStr(IDS_EMPTY);
        mi.cch = lstrlen(mi.dwTypeData);
        InsertMenuItem(FavoritiesMenu, 0, TRUE, &mi);
    }
    else
    {
        SortFavoriteSettings(0, Favorities.Count - 1);
        int i;
        for (i = 0; i < Favorities.Count; i++)
        {
            CFavoriteSfx* fav;

            fav = Favorities[i];
            memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(mi);
            mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_DATA;
            mi.fType = MFT_STRING;
            mi.wID = CM_SFX_FAVORITE + i;
            mi.dwItemData = (ULONG_PTR)fav;
            mi.dwTypeData = fav->Name;
            mi.cch = lstrlen(mi.dwTypeData);
            InsertMenuItem(FavoritiesMenu, i, TRUE, &mi);
        }
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE;
        mi.fType = MFT_SEPARATOR;
        InsertMenuItem(FavoritiesMenu, Favorities.Count /*i++*/, TRUE, &mi);

        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID;
        mi.fType = MFT_STRING;
        mi.wID = CM_SFX_MANAGE;
        mi.dwTypeData = LoadStr(IDS_MANAGE);
        mi.cch = lstrlen(mi.dwTypeData);
        InsertMenuItem(FavoritiesMenu, Favorities.Count + 1 /*i*/, TRUE, &mi);
    }

    // nastavime submenu polozce 'Favorities'
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_SUBMENU;
    mi.hSubMenu = FavoritiesMenu;
    SetMenuItemInfo(Menu, CM_SFX_FAVORITIES, FALSE, &mi);

    return TRUE;
}

#define SFX_SET_SIGNATURE 0x53584653
#define SFX_SET_CURRENTVERSION 1

struct CSettingsHeader
{
    DWORD Signature;
    DWORD Version;
    DWORD DataSize;
};

BOOL CAdvancedSEDialog::OnImport()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnImport()");
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = Dlg;
    char buf[128];
    sprintf(buf, "%s%c*.set%c"
                 "%s%c%s%c",
            LoadStr(IDS_SETTINGSFILE), 0, 0,
            LoadStr(IDS_ALLFILES), 0, "*.*", 0);
    ofn.lpstrFilter = buf;
    ofn.nFilterIndex = 1;
    char fileName[MAX_PATH];
    fileName[0] = 0;
    ofn.lpstrFile = fileName;
    if (PackObject->Config.LastExportPath[0])
        ofn.lpstrInitialDir = PackObject->Config.LastExportPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "set";

    if (GetOpenFileName(&ofn))
    {
        CFile* file;
        int ret = PackObject->CreateCFile(&file, fileName, GENERIC_READ, FILE_SHARE_READ,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                                          false, false);
        if (ret)
        {
            if (ret == ERR_LOWMEM)
            {
                SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
            }
            return TRUE;
        }

        char* buffer = (char*)malloc((unsigned)file->Size + 1);
        if (!buffer)
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        }
        else
        {
            if (!PackObject->Read(file, buffer, (unsigned)file->Size, NULL, NULL))
            {
                buffer[file->Size] = 0;
                CSfxSettings settings;

                memset(&settings, 0, sizeof(CSfxSettings));
                settings.Flags = SE_SHOWSUMARY;
                if (DefLanguage)
                    lstrcpy(settings.SfxFile, DefLanguage->FileName);
                else
                {
                    TRACE_E("CAdvancedSEDialog::OnImport(), neni naloadena DefLanguage");
                    settings.SfxFile[0] = 0;
                }

                char zip2sfxDir[MAX_PATH];
                if (GetModuleFileName(DLLInstance, zip2sfxDir, MAX_PATH - 1)) // -1 je rozdil delek "zip2sfx\\" a "zip.spl"
                {
                    char* name = strrchr(zip2sfxDir, '\\');
                    if (name != NULL)
                        strcpy(name + 1, "zip2sfx\\");
                }
                else
                    zip2sfxDir[0] = 0;

                //nacteme je poprve abychom ziskaly jmeno sfx balicku (neni to poviny parameter)
                ret = ImportSFXSettings(buffer, &settings, zip2sfxDir);
                if (ret == 0)
                {
                    SalamanderGeneral->SalPathStripPath(settings.SfxFile);
                    CSfxLang* lang = NULL;
                    int i;
                    for (i = 0; i < SfxLanguages->Count; i++)
                    {
                        lang = (*SfxLanguages)[i];
                        if (lstrcmpi(lang->FileName, settings.SfxFile) == 0)
                            break;
                        lang = NULL;
                    }
                    if (!lang)
                    {
                        char err[1024];
                        sprintf(err, LoadStr(IDS_NOLANGFILE), settings.SfxFile);
                        SalamanderGeneral->SalMessageBox(Dlg, err, LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                    {
                        settings.Flags = SE_SHOWSUMARY;
                        lstrcpy(settings.Text, lang->DlgText);
                        lstrcpy(settings.Title, lang->DlgTitle);
                        lstrcpy(settings.ExtractBtnText, lang->ButtonText);
                        lstrcpy(settings.Vendor, lang->Vendor);
                        lstrcpy(settings.WWW, lang->WWW);
                        GetModuleFileName(DLLInstance, settings.IconFile, MAX_PATH);
                        settings.IconIndex = -IDI_SFXICON;
                        //nacteme je podruhe abychom ziskali texty (nejsou to povinne parametry)
                        //kdyz nejsou zadany pouziji se texty ze sfx balicku, budto zadaneho
                        //nebo defaultniho
                        ImportSFXSettings(buffer, &settings, zip2sfxDir);

                        SalamanderGeneral->SalPathStripPath(settings.SfxFile);

                        char buff[MAX_PATH];
                        int rootLen = SalamanderGeneral->GetRootPath(buff, settings.IconFile);
                        int iconFileLen = (int)strlen(settings.IconFile);
                        if (iconFileLen < rootLen)
                            rootLen = iconFileLen;
                        SalamanderGeneral->SalRemovePointsFromPath(settings.IconFile + rootLen);

                        if (LoadFavSettings(&settings))
                        {
                            ResetValueControls();
                            lstrcpy(PackObject->Config.LastExportPath, fileName);
                            SalamanderGeneral->CutDirectory(PackObject->Config.LastExportPath);
                        }
                    }
                }
                else
                {
                    int errID;
                    switch (ret)
                    {
                    case 1:
                        errID = IDS_BADTEMP2;
                        break;
                    case 2:
                        errID = IDS_MISBAR2;
                        break;
                    case 3:
                        errID = IDS_BADVAR2;
                        break;
                    case 4:
                        errID = IDS_BADKEY2;
                        break;
                    case 5:
                        errID = IDS_MISSINGVERSION;
                        break;
                    case 6:
                        errID = IDS_BADVERSION;
                        break;
                    case 8:
                        errID = IDS_BADMSGBOXTYPE;
                        break;
                    case 7:
                    default:
                        errID = IDS_ERRFORMAT;
                        break;
                    }

                    SalamanderGeneral->SalMessageBox(Dlg, LoadStr(errID), LoadStr(IDS_ERROR),
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
            }
            free(buffer);
        }
        PackObject->CloseCFile(file);
    }

    return TRUE;
}

BOOL CAdvancedSEDialog::OnExport()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnExport()");
    CSfxSettings settings;
    if (!GetSettings(&settings))
        return TRUE;
    char fullPath[MAX_PATH];
    GetModuleFileName(DLLInstance, fullPath, MAX_PATH);
    SalamanderGeneral->CutDirectory(fullPath);
    SalamanderGeneral->SalPathAppend(fullPath, "sfx", MAX_PATH);
    SalamanderGeneral->SalPathAppend(fullPath, settings.SfxFile, MAX_PATH);
    lstrcpy(settings.SfxFile, fullPath);
    //lstrcpy(settings.IconFile, TmpSfxSettings.IconFile);
    //settings.IconIndex = TmpSfxSettings.IconIndex;

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = Dlg;
    char buf[128];
    sprintf(buf, "%s%c*.set%c", LoadStr(IDS_SETTINGSFILE), 0, 0);
    ofn.lpstrFilter = buf;
    ofn.nFilterIndex = 1;
    char fileName[MAX_PATH];
    fileName[0] = 0;
    ofn.lpstrFile = fileName;
    if (PackObject->Config.LastExportPath[0])
        ofn.lpstrInitialDir = PackObject->Config.LastExportPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | /*OFN_FILEMUSTEXIST | */ OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "set";

    if (SalamanderGeneral->SafeGetSaveFileName(&ofn))
    {
        //otestujem jestli uz existuje
        if (SalamanderGeneral->SalGetFileAttributes(fileName) == 0xFFFFFFFF ||
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_EXPORTOVEWRITE), LoadStr(IDS_PLUGINNAME),
                                             MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            CFile* file;
            int ret = PackObject->CreateCFile(&file, fileName, GENERIC_WRITE, FILE_SHARE_READ,
                                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                                              false, false);
            if (ret)
            {
                if (ret == ERR_LOWMEM)
                {
                    SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR),
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
                return TRUE;
            }

            if (PackObject->ExportSFXSettings(file, &settings))
            {
                PackObject->CloseCFile(file);
                lstrcpy(PackObject->Config.LastExportPath, fileName);
                SalamanderGeneral->CutDirectory(PackObject->Config.LastExportPath);
                // ohlasime zmenu na ceste
                SalamanderGeneral->PostChangeOnPathNotification(PackObject->Config.LastExportPath, FALSE);
            }
            else
            {
                PackObject->CloseCFile(file);
                DeleteFile(ofn.lpstrFile);
            }
        }
    }

    return TRUE;
}

BOOL CAdvancedSEDialog::OnPreview()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnPreview()");
    CSfxSettings settings;

    if (!GetSettings(&settings))
        return TRUE;

    LRESULT retL = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (retL != CB_ERR)
    {
        CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, retL, 0);
        if ((LRESULT)lang != CB_ERR)
        {
            char buffer[2048];
            buffer[0] = 0;
            if (strcmp(TmpSfxSettings.Vendor, lang->Vendor) != 0 || strcmp(TmpSfxSettings.WWW, lang->WWW) != 0)
                sprintf(buffer, "%s\r\n%s\r\n\r\n", lang->Vendor, lang->WWW);
            strcat_s(buffer, lang->AboutLicenced);
            lstrcpyn(PackOptions->About, buffer, SE_MAX_ABOUT);
        }
    }

    char tmpName[MAX_PATH];
    DWORD e;
    if (!SalamanderGeneral->SalGetTempFileName(NULL, "Sal", tmpName, TRUE, &e))
    {
        char buffer[1024];
        SalamanderGeneral->SalMessageBox(Dlg, FormatMessage(buffer, IDS_ERRGETTEMP, e),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
    }

    int ret = PackObject->CreateCFile(&PackObject->TempFile, tmpName, GENERIC_WRITE, FILE_SHARE_READ,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, PE_NOSKIP, NULL,
                                      false, false);
    if (ret)
    {
        if (ret == ERR_LOWMEM)
        {
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        }
        return TRUE;
    }

    ret = PackObject->WriteSfxExecutable(tmpName, settings.SfxFile, TRUE, 0);
    PackObject->CloseCFile(PackObject->TempFile);
    PackObject->TempFile = NULL;
    if (ret)
    {
        if (ret != IDS_NODISPLAY)
            SalamanderGeneral->SalMessageBox(Dlg, LoadStr(ret), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        DeleteFile(tmpName);
        return TRUE;
    }

    HINSTANCE sfxHInstance = LoadLibraryEx(tmpName, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (sfxHInstance)
    {
        CALL_STACK_MESSAGE1("Preview SFX Dialog");
        CPreviewInitData data;
        data.Settings = &settings;
        data.About = PackOptions->About;
        data.AboutButton1 = LoadStr(IDS_SFXABOUTBTN1);
        data.AboutButton2 = LoadStr(IDS_SFXABOUTBTN2);
        data.LargeIcon = LargeIcon;
        data.SmallIcon = SmallIcon;
        data.SfxHInstance = sfxHInstance;
        HRSRC hrsrc = FindResource(sfxHInstance, MAKEINTRESOURCE(SE_IDD_SFXDIALOG), RT_DIALOG);
        if (hrsrc != NULL)
        {
            HGLOBAL hGlobal = LoadResource(sfxHInstance, hrsrc);
            if (hGlobal != NULL)
            {
                INT_PTR ret2 = DialogBoxIndirectParam(HLanguage, (LPCDLGTEMPLATE)hGlobal, Dlg, SfxPreviewDlgProc,
                                                      (LPARAM)&data);
                if (ret2 == 0 || ret2 == -1)
                    TRACE_E("DialogBoxIndirectParam failed");
            }
            else
                TRACE_E("LoadResource failed");
        }
        else
            TRACE_E("Dialog template SE_IDD_SFXDIALOG was not found.");

        FreeLibrary(sfxHInstance);
    }
    else
        TRACE_E("LoadLibraryEx failed");

    DeleteFile(tmpName);

    return TRUE;
}

BOOL CAdvancedSEDialog::OnResetAll()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnResetAll()");
    OnResetValues();
    OnResetTexts();

    return TRUE;
}

BOOL CAdvancedSEDialog::OnResetValues()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnResetValues()");

    //nemuzeme pouzit primo TmpSfxSettings = DefOptions.SfxSetting, protoze by nam to preplaclo
    //ikonu, ktery by razem mohla byt neplatna
    TmpSfxSettings.Flags = DefOptions.SfxSettings.Flags;
    *TmpSfxSettings.Command = 0;
    lstrcpy(TmpSfxSettings.TargetDir, DefOptions.SfxSettings.TargetDir);
    TmpSfxSettings.MBoxStyle = DefOptions.SfxSettings.MBoxStyle;
    *TmpSfxSettings.MBoxTitle = 0;
    TmpSfxSettings.SetMBoxText("");
    *TmpSfxSettings.WaitFor = 0;

    if (!DefLanguage)
    {
        //MessageBox(Dlg, LoadStr(IDS_NODEFSFX), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        TRACE_E("Neni naloudena DefLanguage pro sfx advanced dialog a je volano reset values.");
    }
    else
    {
        CurrentSfxLang = DefLanguage;
        char langName[128];
        if (GetLocaleInfo(MAKELCID(MAKELANGID(DefLanguage->LangID, SUBLANG_NEUTRAL), SORT_DEFAULT), LOCALE_SLANGUAGE, langName, 128))
        {
            char* c = strchr(langName, ' ');
            if (c)
                *c = 0;
            if (SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_SELECTSTRING, -1, (LPARAM)langName) == CB_ERR)
            {
                SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_SETCURSEL, 0, 0);
            }
        }
        lstrcpy(TmpSfxSettings.SfxFile, DefLanguage->FileName);

        HICON iconLarge, iconSmall;
        CIcon* icons;
        int count;
        char file[MAX_PATH];
        GetModuleFileName(DLLInstance, file, MAX_PATH);
        int errorID = 0;
        switch (LoadIcons(file, -IDI_SFXICON, &icons, &count))
        {
        case 1:
            errorID = IDS_ERROPENICO;
            break;
        case 2:
            errorID = IDS_ERRLOADLIB;
            break;
        case 3:
            errorID = IDS_ERRLOADLIB2;
            break;
        case 4:
            errorID = IDS_ERRLOADICON;
            break;
        }
        if (!errorID)
        {
            if (ExtractIconEx(file, -IDI_SFXICON, &iconLarge, &iconSmall, 1))
            {
                lstrcpy(TmpSfxSettings.IconFile, file);
                TmpSfxSettings.IconIndex = -IDI_SFXICON;
                if (SmallIcon)
                    DestroyIcon(SmallIcon);
                if (LargeIcon)
                    DestroyIcon(LargeIcon);
                SmallIcon = iconSmall;
                LargeIcon = iconLarge;
                if (PackOptions->Icons)
                    DestroyIcons(PackOptions->Icons, PackOptions->IconsCount);
                PackOptions->Icons = icons;
                PackOptions->IconsCount = count;
                InvalidateRect(GetDlgItem(Dlg, IDC_EXEICON), NULL, TRUE);
                UpdateWindow(GetDlgItem(Dlg, IDC_EXEICON));
            }
            else
            {
                DestroyIcons(icons, count);
                errorID = IDS_ERRLOADICON;
            }
        }
        if (errorID)
        {
            char buffer[1024];
            int e = GetLastError();
            SalamanderGeneral->SalMessageBox(Dlg, FormatMessage(buffer, errorID, e),
                                             LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        }
    }

    ResetValueControls();

    return TRUE;
}

BOOL CAdvancedSEDialog::OnResetTexts()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnResetTexts()");
    LRESULT i = SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        CSfxLang* lang = (CSfxLang*)SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_GETITEMDATA, i, 0);
        if ((LRESULT)lang != CB_ERR)
        {
            TmpSfxSettings.MBoxStyle = MB_OK;
            TmpSfxSettings.SetMBoxText("");
            *TmpSfxSettings.MBoxTitle = 0;
            lstrcpy(TmpSfxSettings.Text, lang->DlgText);
            lstrcpy(TmpSfxSettings.Title, lang->DlgTitle);
            lstrcpy(TmpSfxSettings.ExtractBtnText, lang->ButtonText);
            lstrcpy(TmpSfxSettings.Vendor, lang->Vendor);
            lstrcpy(TmpSfxSettings.WWW, lang->WWW);
        }
    }

    return TRUE;
}

BOOL CAdvancedSEDialog::OnAddFavorite()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnAddFavorite()");
    if (!FavoritiesMenu)
        return TRUE;
    CFavoriteSfx* newFav = new CFavoriteSfx;

    if (!newFav)
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }

    if (!GetSettings(&newFav->Settings))
    {
        delete newFav;
        return TRUE;
    }

    //lstrcpy(newFav->Settings.IconFile, TmpSfxSettings.IconFile);
    //newFav->Settings.IconIndex = TmpSfxSettings.IconIndex;

    *newFav->Name = 0;
    CRenFavDialog renFav(Dlg, newFav->Name, false);

    if (renFav.Proceed() != IDOK)
    {
        delete newFav;
        return TRUE;
    }

    /*
  //najdeme si index na, ktery ho pridame
  int index;
  for (index = 0; index < Favorities.Count; index++)
  {
    if (CompareMenuItems(newFav->Name, Favorities[index]->Name) < 0) break;
  }
  */

    if (!Favorities.Add(newFav))
    {
        delete newFav;
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_LOWMEM), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    // tridi se az v CreateFavoritiesMenu
    //SortFavoriteSettings(0, Favorities.Count - 1);

    CreateFavoritesMenu();

    /*
  //odstranime "empty" v pripade ze pridavame prvni polozku
  if (Favorities.Count == 1) DeleteMenu(FavoritiesMenu, 0, MF_BYPOSITION);

  MENUITEMINFO mi;

  memset(&mi, 0, sizeof(mi));
  mi.cbSize = sizeof(mi);
  mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_DATA;
  mi.fType = MFT_STRING;
  mi.wID = CM_SFX_FAVORITE + Favorities.Count - 1;
  mi.dwItemData = (ULONG_PTR) newFav;
  mi.dwTypeData = newFav->Name;
  mi.cch = lstrlen(mi.dwTypeData);
  InsertMenuItem(FavoritiesMenu, index, TRUE, &mi);
  */

    return TRUE;
}

BOOL CAdvancedSEDialog::OnRenameFavorite()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnRenameFavorite()");
    SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_UNDERCOSTRUCT), LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
    return TRUE;
}

BOOL CAdvancedSEDialog::OnDeleteteFavorite()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnDeleteteFavorite()");
    SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_UNDERCOSTRUCT), LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
    return TRUE;
}

BOOL CAdvancedSEDialog::OnRemoveAllFavorities()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnRemoveAllFavorities()");
    SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_UNDERCOSTRUCT), LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONINFORMATION);
    return TRUE;
}

BOOL CAdvancedSEDialog::OnManageFavorities()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnManageFavorities()");
    ManageFavoritiesDialog(Dlg);
    CreateFavoritesMenu();
    return TRUE;
}

BOOL CAdvancedSEDialog::OnFavorities()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnFavorities()");
    if (!FavoritiesMenu)
        return TRUE;

    POINT p;

    p.x = 10;
    p.y = 10;

    ClientToScreen(Dlg, &p);
    TrackPopupMenuEx(FavoritiesMenu, TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, Dlg, NULL);

    return TRUE;
}

BOOL CAdvancedSEDialog::OnFavoriteOption(WORD itemID)
{
    CALL_STACK_MESSAGE2("CAdvancedSEDialog::OnFavoriteOption(0x%X)", itemID);
    MENUITEMINFO mi;

    mi.cbSize = sizeof(MENUITEMINFO);
    mi.fMask = MIIM_DATA;
    if (!GetMenuItemInfo(FavoritiesMenu, itemID, FALSE, &mi))
    {
        TRACE_E("Nejde ziskat item data vubrane polozky z menu favorities.");
        return TRUE;
    }
    CFavoriteSfx* fav = (CFavoriteSfx*)mi.dwItemData;

    if (LoadFavSettings(&fav->Settings))
    {
        ResetValueControls();
    }

    return TRUE;
}

BOOL CAdvancedSEDialog::OnLastUsed()
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::OnLastUsed()");
    if (*LastUsedSfxSet.Name)
    {
        if (LoadFavSettings(&LastUsedSfxSet.Settings))
        {
            ResetValueControls();
        }
    }
    return TRUE;
}

BOOL CAdvancedSEDialog::LoadFavSettings(CSfxSettings* sfxSettings)
{
    CALL_STACK_MESSAGE1("CAdvancedSEDialog::LoadFavSettings()");
    CSfxLang* lang = NULL;
    int i;
    for (i = 0; i < SfxLanguages->Count; i++)
    {
        lang = (*SfxLanguages)[i];
        if (lstrcmpi(lang->FileName, sfxSettings->SfxFile) == 0)
            break;
        lang = NULL;
    }
    if (!lang)
    {
        char err[1024];
        sprintf(err, LoadStr(IDS_NOLANGFILE), sfxSettings->SfxFile);
        SalamanderGeneral->SalMessageBox(Dlg, err, LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    char langName[128];
    if (GetLocaleInfo(MAKELCID(MAKELANGID(lang->LangID, SUBLANG_NEUTRAL), SORT_DEFAULT), LOCALE_SLANGUAGE, langName, 128))
    {
        char* c = strchr(langName, ' ');
        if (c)
            *c = 0;
        CurrentSfxLang = lang;
        SendDlgItemMessage(Dlg, IDC_LANGUAGE, CB_SELECTSTRING, -1, (LPARAM)langName);
    }

    TmpSfxSettings.Flags = sfxSettings->Flags;
    lstrcpy(TmpSfxSettings.Command, sfxSettings->Command);
    lstrcpy(TmpSfxSettings.SfxFile, sfxSettings->SfxFile);
    lstrcpy(TmpSfxSettings.Text, sfxSettings->Text);
    lstrcpy(TmpSfxSettings.Title, sfxSettings->Title);
    TmpSfxSettings.MBoxStyle = sfxSettings->MBoxStyle;
    TmpSfxSettings.SetMBoxText(sfxSettings->MBoxText);
    lstrcpy(TmpSfxSettings.MBoxTitle, sfxSettings->MBoxTitle);
    lstrcpy(TmpSfxSettings.TargetDir, sfxSettings->TargetDir);
    lstrcpy(TmpSfxSettings.ExtractBtnText, sfxSettings->ExtractBtnText);
    lstrcpy(TmpSfxSettings.Vendor, sfxSettings->Vendor);
    lstrcpy(TmpSfxSettings.WWW, sfxSettings->WWW);
    lstrcpy(TmpSfxSettings.WaitFor, sfxSettings->WaitFor);

    HICON iconLarge, iconSmall;
    CIcon* icons;
    int count;
    int errorID = 0;
    switch (LoadIcons(sfxSettings->IconFile, sfxSettings->IconIndex, &icons, &count))
    {
    case 1:
        errorID = IDS_ERROPENICO;
        break;
    case 2:
        errorID = IDS_ERRLOADLIB;
        break;
    case 3:
        errorID = IDS_ERRLOADLIB2;
        break;
    case 4:
        errorID = IDS_ERRLOADICON;
        break;
    }
    if (!errorID)
    {
        if (ExtractIconEx(sfxSettings->IconFile, sfxSettings->IconIndex, &iconLarge, &iconSmall, 1))
        {
            lstrcpy(TmpSfxSettings.IconFile, sfxSettings->IconFile);
            TmpSfxSettings.IconIndex = sfxSettings->IconIndex;
            if (SmallIcon)
                DestroyIcon(SmallIcon);
            if (LargeIcon)
                DestroyIcon(LargeIcon);
            SmallIcon = iconSmall;
            LargeIcon = iconLarge;
            if (PackOptions->Icons)
                DestroyIcons(PackOptions->Icons, PackOptions->IconsCount);
            PackOptions->Icons = icons;
            PackOptions->IconsCount = count;
            InvalidateRect(GetDlgItem(Dlg, IDC_EXEICON), NULL, TRUE);
            UpdateWindow(GetDlgItem(Dlg, IDC_EXEICON));
        }
        else
        {
            DestroyIcons(icons, count);
            errorID = IDS_ERRLOADICON;
        }
    }
    if (errorID)
    {
        char buffer[1024];
        int err = GetLastError();
        SalamanderGeneral->SalMessageBox(Dlg, FormatMessage(buffer, errorID, err),
                                         LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
    }
    return TRUE;
}

//******************************************************************************
//
// CSfxTextsDialog
//

INT_PTR WINAPI SfxTextsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("SfxTextsDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    static CSfxTextsDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CSfxTextsDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CSfxTextsDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CSfxTextsDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_SFXTEXTS),
                          Parent, SfxTextsDlgProc, (LPARAM)this);
}

INT_PTR CSfxTextsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSfxTextsDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        //controls
        case IDOK:
            return OnOK(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;

        case IDC_RESET:
            return OnReset();

        case IDHELP:
            SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_SFXTEXTS, FALSE);
            return TRUE;
        }
        break;

    case WM_HELP:
        SalamanderGeneral->OpenHtmlHelp(Dlg, HHCDisplayContext, IDD_SFXTEXTS, FALSE);
        return TRUE;

        //case WM_DESTROY: break;
    }
    return FALSE;
}

BOOL CSfxTextsDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE1("CSfxTextsDialog::OnInit");

    //!bacha nize se odkazuji tvrde na poradi
    int i = 0;
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBOK));
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_SETITEMDATA, i++, (LPARAM)MB_OK);
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBOKCANCEL));
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_SETITEMDATA, i++, MB_OKCANCEL);
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBYESNO));
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_SETITEMDATA, i++, MB_YESNO);
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBAGREEDISAGREE));
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_SETITEMDATA, i++, SE_MBAGREEDISAGREE);
    i = 0;
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBNOICON));
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_SETITEMDATA, i++, (LPARAM)0);
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBEXCLAMATION));
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_SETITEMDATA, i++, MB_ICONEXCLAMATION);
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBINFORMATION));
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_SETITEMDATA, i++, MB_ICONINFORMATION);
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_MBQUESTION));
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_SETITEMDATA, i++, MB_ICONQUESTION);
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_LONGMESSAGE));
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_SETITEMDATA, i++, SE_LONGMESSAGE);

    ResetControls(SfxSettings->MBoxStyle, SfxSettings->MBoxTitle, SfxSettings->MBoxText,
                  SfxSettings->Title, SfxSettings->Text, SfxSettings->ExtractBtnText,
                  SfxSettings->Vendor, SfxSettings->WWW);

    CenterDlgToParent();
    return TRUE;
}

void CSfxTextsDialog::ResetControls(UINT mboxStyle, const char* mboxTitle, const char* mboxText,
                                    const char* title, const char* text, const char* button,
                                    const char* vendor, const char* www)
{
    CALL_STACK_MESSAGE1("CSfxTextsDialog::ResetControls");
    int i = 0;
    if ((int)mboxStyle < 0)
    {
        switch (mboxStyle)
        {
        case SE_MBOK:
            i = 0;
            break;
        case SE_MBOKCANCEL:
            i = 1;
            break;
        case SE_MBYESNO:
            i = 2;
            break;
        case SE_MBAGREEDISAGREE:
            i = 3;
            break;
        }
    }
    else
    {
        switch (mboxStyle & 0x0F)
        {
        case MB_OK:
            i = 0;
            break;
        case MB_OKCANCEL:
            i = 1;
            break;
        case MB_YESNO:
            i = 2;
            break;
        }
    }
    SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_SETCURSEL, i, 0);
    if ((int)mboxStyle < 0)
        i = 4;
    else
    {
        switch (mboxStyle & 0xF0)
        {
        case 0:
            i = 0;
            break;
        case MB_ICONEXCLAMATION:
            i = 1;
            break;
        case MB_ICONINFORMATION:
            i = 2;
            break;
        case MB_ICONQUESTION:
            i = 3;
            break;
        }
    }
    SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_SETCURSEL, i, 0);
    SendDlgItemMessage(Dlg, IDC_MBOXTEXT, EM_SETLIMITTEXT, SE_MAX_MBOXTEXT, 0);
    SendDlgItemMessage(Dlg, IDC_MBOXTEXT, WM_SETTEXT, 0, (LPARAM)mboxText);
    SendDlgItemMessage(Dlg, IDC_MBOXTITLE, EM_SETLIMITTEXT, SE_MAX_TITLE - 1, 0);
    SendDlgItemMessage(Dlg, IDC_MBOXTITLE, WM_SETTEXT, 0, (LPARAM)mboxTitle);
    SendDlgItemMessage(Dlg, IDC_TEXT, EM_SETLIMITTEXT, SE_MAX_TEXT - 1, 0);
    SendDlgItemMessage(Dlg, IDC_TEXT, WM_SETTEXT, 0, (LPARAM)text);
    SendDlgItemMessage(Dlg, IDC_TITLE, EM_SETLIMITTEXT, SE_MAX_TITLE - 1, 0);
    SendDlgItemMessage(Dlg, IDC_TITLE, WM_SETTEXT, 0, (LPARAM)title);
    SendDlgItemMessage(Dlg, IDC_BUTTONTEXT, EM_SETLIMITTEXT, SE_MAX_EXTRBTN - 1, 0);
    SendDlgItemMessage(Dlg, IDC_BUTTONTEXT, WM_SETTEXT, 0, (LPARAM)button);
    SendDlgItemMessage(Dlg, IDE_VENDOR, WM_SETTEXT, 0, (LPARAM)vendor);
    SendDlgItemMessage(Dlg, IDE_WWW, WM_SETTEXT, 0, (LPARAM)www);
}

BOOL CSfxTextsDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE1("CSfxTextsDialog::OnOK()");
    CSfxSettings settings = *SfxSettings;

    settings.MBoxStyle = 0;
    // nacteme typ message boxu
    LRESULT i = SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        UINT ui = (UINT)(SendDlgItemMessage(Dlg, IDC_MBOXICON, CB_GETITEMDATA, i, 0) & 0xffffffff); // X64 - ITEMDATA obsahuji DWORD
        if (ui != CB_ERR)
            settings.MBoxStyle |= ui;
    }
    // nacteme nastaveni tlacitek
    i = SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_GETCURSEL, 0, 0);
    if (i != CB_ERR)
    {
        UINT ui = (UINT)(SendDlgItemMessage(Dlg, IDC_MBOXBUTTONS, CB_GETITEMDATA, i, 0) & 0xffffffff); // X64 - ITEMDATA obsahuji DWORD
        if (ui != CB_ERR)
        {
            if ((int)settings.MBoxStyle < 0)
            {
                switch (i)
                {
                case 0:
                    ui = SE_MBOK;
                    break;
                case 1:
                    ui = SE_MBOKCANCEL;
                    break;
                case 2:
                    ui = SE_MBYESNO;
                    break;
                case 3:
                    ui = SE_MBAGREEDISAGREE;
                    break;
                }
            }
            else
            {
                if ((int)ui < 0)
                {
                    SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADMSGBOXTYPE), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
                    return TRUE;
                }
            }
            settings.MBoxStyle |= ui;
        }
    }

    int l = (int)SendDlgItemMessage(Dlg, IDC_MBOXTEXT, WM_GETTEXTLENGTH, 0, 0) + 1;
    settings.MBoxText = (char*)realloc(settings.MBoxText, l);
    SendDlgItemMessage(Dlg, IDC_MBOXTEXT, WM_GETTEXT, l, (LPARAM)settings.MBoxText);
    SendDlgItemMessage(Dlg, IDC_MBOXTITLE, WM_GETTEXT, SE_MAX_TITLE, (LPARAM)settings.MBoxTitle);
    if (!lstrlen(settings.MBoxTitle) && lstrlen(settings.MBoxText))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ERRBADMBOXTITLE), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }

    SendDlgItemMessage(Dlg, IDC_TITLE, WM_GETTEXT, SE_MAX_TITLE, (LPARAM)settings.Title);
    if (!lstrlen(settings.Title))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_ERRBADTITLE), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    SendDlgItemMessage(Dlg, IDC_TEXT, WM_GETTEXT, SE_MAX_TEXT, (LPARAM)settings.Text);
    SendDlgItemMessage(Dlg, IDC_BUTTONTEXT, WM_GETTEXT, SE_MAX_EXTRBTN, (LPARAM)settings.ExtractBtnText);
    if (!lstrlen(settings.ExtractBtnText))
    {
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_BADBUTTONTEXT), LoadStr(IDS_ERROR), MB_OK | MB_ICONEXCLAMATION);
        return TRUE;
    }
    SendDlgItemMessage(Dlg, IDE_VENDOR, WM_GETTEXT, SE_MAX_VENDOR, (LPARAM)settings.Vendor);
    SendDlgItemMessage(Dlg, IDE_WWW, WM_GETTEXT, SE_MAX_WWW, (LPARAM)settings.WWW);

    *SfxSettings = settings;

    EndDialog(Dlg, IDOK);
    return TRUE;
}

BOOL CSfxTextsDialog::OnReset()
{
    CALL_STACK_MESSAGE1("CSfxTextsDialog::OnReset()");
    ResetControls(MB_OK, "", "", CurrentSfxLang->DlgTitle, CurrentSfxLang->DlgText,
                  CurrentSfxLang->ButtonText, CurrentSfxLang->Vendor,
                  CurrentSfxLang->WWW);
    return TRUE;
}

//******************************************************************************
//
// CManageFavoritiesDialog
//

INT_PTR WINAPI ManageFavoritiesDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ManageFavoritiesDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    static CManageFavoritiesDialog* dlg = NULL;

    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hDlg);
        dlg = (CManageFavoritiesDialog*)lParam;
        dlg->Dlg = hDlg;
        return dlg->DialogProc(uMsg, wParam, lParam);

    default:
        if (dlg)
            return dlg->DialogProc(uMsg, wParam, lParam);
    }
    return FALSE;
}

INT_PTR CManageFavoritiesDialog::Proceed()
{
    CALL_STACK_MESSAGE1("CManageFavoritiesDialog::Proceed()");
    return DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_MANFAVS),
                          Parent, ManageFavoritiesDlgProc, (LPARAM)this);
}

INT_PTR CManageFavoritiesDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CManageFavoritiesDialog::DialogProc(0x%X, 0x%IX, 0x%IX)",
                        uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        return OnInit(wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        //controls
        case IDOK:
            EndDialog(Dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(Dlg, IDCANCEL);
            return TRUE;

        case IDC_FAVORITIES:
            return OnFavorities(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
        case IDC_RENAME:
            return OnRenameFavorite();
        case IDC_REMOVE:
            return OnRemoveFavorite();
        case IDC_REMOVEALL:
            return OnRemoveAllFavorities();
        }
        break;

        //case WM_DESTROY: break;
    }
    return FALSE;
}

BOOL CManageFavoritiesDialog::OnInit(WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE1("CManageFavoritiesDialog::OnInit");

    HWND wnd = GetDlgItem(Dlg, IDC_FAVORITIES);

    int i;
    for (i = 0; i < Favorities.Count; i++)
    {
        SendMessage(wnd, LB_ADDSTRING, 0, (LPARAM)Favorities[i]->Name);
    }
    SendMessage(wnd, LB_SETCURSEL, 0, 0);

    CenterDlgToParent();
    return TRUE;
}

BOOL CManageFavoritiesDialog::OnFavorities(WORD wNotifyCode, WORD wID, HWND hwndCtl)
{
    CALL_STACK_MESSAGE3("CManageFavoritiesDialog::OnFavorities(0x%X, 0x%X, )",
                        wNotifyCode, wID);
    switch (wNotifyCode)
    {
    case LBN_KILLFOCUS:
    {
        /*
      SendMessage(hwndCtl, LB_SETCURSEL, -1, 0);
      FavFocus = FALSE;
      */
        break;
    }
    case LBN_SETFOCUS:
    {
        /*
      int i = SendMessage(hwndCtl, LB_GETCARETINDEX, 0, 0);
      if (i != LB_ERR) SendMessage(hwndCtl, LB_SETCURSEL, i, 0);
      FavFocus = TRUE;
      */
        break;
    }
    }
    return FALSE;
}

BOOL CManageFavoritiesDialog::OnRenameFavorite()
{
    CALL_STACK_MESSAGE1("CManageFavoritiesDialog::OnRenameFavorite()");
    HWND wnd = GetDlgItem(Dlg, IDC_FAVORITIES);
    int i = (int)SendMessage(wnd, LB_GETCARETINDEX, 0, 0);
    if (i != LB_ERR && i >= 0 && i < Favorities.Count)
    {
        CRenFavDialog renFav(Dlg, Favorities[i]->Name, true);

        if (renFav.Proceed() == IDOK)
        {
            CFavoriteSfx* fav = Favorities[i];
            SortFavoriteSettings(0, Favorities.Count - 1);
            SendMessage(wnd, LB_RESETCONTENT, 0, 0);
            int j = 0;
            for (i = 0; i < Favorities.Count; i++)
            {
                SendMessage(wnd, LB_ADDSTRING, 0, (LPARAM)Favorities[i]->Name);
                if (strcmp(fav->Name, Favorities[i]->Name) == 0)
                    j = i;
            }
            /*
      SendMessage(wnd, LB_SETCARETINDEX, j, FALSE);
      if (FavFocus) SendMessage(wnd, LB_SETCURSEL, j, 0);
      */
            SendMessage(wnd, LB_SETCURSEL, j, 0);
        }
    }
    return TRUE;
}

BOOL CManageFavoritiesDialog::OnRemoveFavorite()
{
    CALL_STACK_MESSAGE1("CManageFavoritiesDialog::OnRemoveFavorite()");
    int i = (int)SendDlgItemMessage(Dlg, IDC_FAVORITIES, LB_GETCARETINDEX, 0, 0);
    if (i != LB_ERR && i >= 0 && i < Favorities.Count)
    {
        char buf[500];
        sprintf(buf, LoadStr(IDS_REMOVEWARN), Favorities[i]->Name);
        if (SalamanderGeneral->SalMessageBox(Dlg, buf, LoadStr(IDS_REMOVEWARNTITLE), MB_YESNO) == IDYES)
        {
            SendDlgItemMessage(Dlg, IDC_FAVORITIES, LB_DELETESTRING, i, 0);
            Favorities.Delete(i);
            if (Favorities.Count > i + 1)
                SortFavoriteSettings(i, Favorities.Count - 1);
        }
        if (Favorities.Count)
        {
            int j = Favorities.Count > i ? i : Favorities.Count - 1;
            /*
      SendDlgItemMessage(Dlg, IDC_FAVORITIES, LB_SETCARETINDEX, j, FALSE);
      if (FavFocus) SendDlgItemMessage(Dlg, IDC_FAVORITIES, LB_SETCURSEL, j, 0);
      */
            SendDlgItemMessage(Dlg, IDC_FAVORITIES, LB_SETCURSEL, j, 0);
        }
    }
    return TRUE;
}

BOOL CManageFavoritiesDialog::OnRemoveAllFavorities()
{
    CALL_STACK_MESSAGE1("CManageFavoritiesDialog::OnRemoveAllFavorities()");
    if (Favorities.Count &&
        SalamanderGeneral->SalMessageBox(Dlg, LoadStr(IDS_REMOVEALLWARN),
                                         LoadStr(IDS_REMOVEWARNTITLE), MB_YESNO) == IDYES)
    {
        SendDlgItemMessage(Dlg, IDC_FAVORITIES, LB_RESETCONTENT, 0, 0);
        Favorities.Destroy();
    }
    return TRUE;
}

INT_PTR ManageFavoritiesDialog(HWND parent)
{
    CALL_STACK_MESSAGE1("ManageFavoritiesDialog()");
    CManageFavoritiesDialog dlg(parent);
    return dlg.Proceed();
}
