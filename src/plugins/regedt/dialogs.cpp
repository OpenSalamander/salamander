// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

TDirectArray<DWORD_PTR> DialogStack(4, 4);
CCS DialogStackCS;

BOOL MinBeepWhenDone;

LPWSTR CopyOrMoveHistory[MAX_HISTORY_ENTRIES];

HFONT EnvFont = NULL; // font prostredi (edit, toolbar, header, status)
int EnvFontHeight;    // vyska fontu

//BOOL HaveForMicrosoftSanfSerif; // v systemu je nainstalovany font Microsoft Sans Serif

BOOL CreateEnvFont()
{
    CALL_STACK_MESSAGE1("CreateEnvFont()");
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    LOGFONT* lf = &ncm.lfMenuFont;

    if (EnvFont != NULL)
        DeleteObject(EnvFont);
    EnvFont = CreateFontIndirect(lf);
    if (EnvFont == NULL)
    {
        TRACE_E("Font se nepovedl vytvorit.");
        return FALSE;
    }

    // zjistime si vysku
    HDC dc = GetDC(NULL);
    HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    EnvFontHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(dc, oldFont);
    ReleaseDC(NULL, dc);
    return TRUE;
}

int CALLBACK
EnumFontsProc(CONST LOGFONT* lplf, CONST TEXTMETRIC* lptm, DWORD dwType, LPARAM lpData)
{
    CALL_STACK_MESSAGE3("EnumFontsProc(, , 0x%X, 0x%IX)", dwType, lpData);
    *(LPBOOL)lpData = TRUE;
    return 0;
}

/*
BOOL
CheckForMicrosoftSanfSerif()
{
  CALL_STACK_MESSAGE1("CheckForMicrosoftSanfSerif()");
  BOOL ret = FALSE;
  HDC hdc = GetDC(NULL);
  EnumFonts(hdc, "Microsoft Sans Serif", EnumFontsProc, (LPARAM) &ret);
  ReleaseDC(NULL, hdc);
  return ret;
}
*/

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SG->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL InitDialogs()
{
    CALL_STACK_MESSAGE1("InitDialogs()");
    if (!InitializeWinLib("RegEdt", DLLInstance))
        return FALSE;
    SetupWinLibHelp(HTMLHelpCallback);

    if (!CreateEnvFont())
    {
        ReleaseWinLib(DLLInstance);
        return FALSE;
    }

    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        CopyOrMoveHistory[i] = NULL;
        PatternHistory[i] = NULL;
        LookInHistory[i] = NULL;
    }

    MinBeepWhenDone = TRUE;
    SG->GetConfigParameter(SALCFG_MINBEEPWHENDONE, &MinBeepWhenDone, sizeof(BOOL), NULL);

    //  HaveForMicrosoftSanfSerif = CheckForMicrosoftSanfSerif();
    //  TRACE_I("HaveForMicrosoftSanfSerif = " << HaveForMicrosoftSanfSerif);

    return TRUE;
}

void ReleaseDialogs()
{
    CALL_STACK_MESSAGE1("ReleaseDialogs()");
    ReleaseWinLib(DLLInstance);

    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (CopyOrMoveHistory[i])
            delete[] CopyOrMoveHistory[i];
        if (PatternHistory[i])
            delete[] PatternHistory[i];
        if (LookInHistory[i])
            delete[] LookInHistory[i];
    }

    if (EnvFont)
        DeleteObject(EnvFont);
}

void DialogStackPush(HWND hWindow)
{
    CALL_STACK_MESSAGE1("DialogStackPush()");
    DialogStackCS.Enter();
    DialogStack.Add((DWORD_PTR)hWindow);
    DialogStack.Add(GetCurrentThreadId());
    DialogStackCS.Leave();
}

void DialogStackPop()
{
    CALL_STACK_MESSAGE1("DialogStackPop()");
    DialogStackCS.Enter();
    int i = DialogStack.Count - 1;
    while (i > 0)
    {
        if (DialogStack[i] == GetCurrentThreadId())
        {
            DialogStack.Delete(i--);
            DialogStack.Delete(i);
            break;
        }
        i -= 2;
    }
    DialogStackCS.Leave();
}

HWND DialogStackPeek()
{
    CALL_STACK_MESSAGE1("DialogStackPeek()");
    DialogStackCS.Enter();
    HWND ret = (HWND)-1;
    int i = DialogStack.Count - 1;
    while (i > 0)
    {
        if (DialogStack[i] == GetCurrentThreadId())
        {
            ret = (HWND)DialogStack[--i];
            break;
        }
        i -= 2;
    }
    DialogStackCS.Leave();
    return ret;
}

void HistoryComboBox(CTransferInfoEx& ti, int id, LPWSTR text, int textMax,
                     int historySize, LPWSTR* history)
{
    CALL_STACK_MESSAGE4("HistoryComboBox(, %d, , %d, %d, )", id, textMax,
                        historySize);
    HWND combo;

    if (!ti.GetControl(combo, id))
        return;

    if (ti.Type == ttDataFromWindow)
    {
        ti.EditLineW(id, text, textMax);

        int toMove = historySize - 1;

        // podivame jestli uz stejna polozka neni v historii
        int i;
        for (i = 0; i < historySize; i++)
        {
            if (history[i] == NULL)
                break;
            if (CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                               history[i], -1,
                               text, -1) == CSTR_EQUAL)
            {
                toMove = i;
                break;
            }
        }
        // alokujeme si pamet pro novou polozku
        LPWSTR ptr = new WCHAR[wcslen(text) + 1];
        if (ptr)
        {
            // uvolnime pamet vymazavane polozky
            if (history[toMove])
                delete[] history[toMove];
            // vytvorime misto pro cestu kterou budeme ukladat
            for (i = toMove; i > 0; i--)
                history[i] = history[i - 1];
            // ulozime cestu
            wcscpy(ptr, text);
            history[0] = ptr;
        }
    }
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; i < historySize; i++)
    {
        if (history[i] == NULL)
            break;
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)history[i]);
    }
    if (ti.Type == ttDataFromWindow)
        SendMessage(combo, CB_SETCURSEL, 0, 0);
    else
    {
        SendMessage(combo, CB_LIMITTEXT, textMax - 1, 0);
        SendMessageW(combo, WM_SETTEXT, 0, (LPARAM)text);
        SendMessage(combo, CB_SETEDITSEL, 0, -1);
    }
}

// ****************************************************************************
//
// CTransferInfoEx
//
//

void CTransferInfoEx::EditLineW(int ctrlID, LPWSTR buffer, DWORD bufferSize, BOOL select)
{
    CALL_STACK_MESSAGE4("CTransferInfoEx::EditLineW(%d, , 0x%X, %d)", ctrlID,
                        bufferSize, select);
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessageW(HWindow, EM_LIMITTEXT, bufferSize - 1, 0);
            SendMessageW(HWindow, WM_SETTEXT, 0, (LPARAM)buffer);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessageW(HWindow, WM_GETTEXT, bufferSize, (LPARAM)buffer);
            break;
        }
        }
    }
}

// ****************************************************************************
//
// CDialogEx
//
//

BOOL CDialogEx::ValidateData()
{
    CALL_STACK_MESSAGE1("CDialogEx::ValidateData()");
    CTransferInfoEx ti(HWindow, ttDataFromWindow);
    Validate(ti);
    if (!ti.IsGood())
    {
        HWND ctrl;
        if (ti.GetControl(ctrl, ti.FailCtrlID, TRUE))
        {
            HWND wnd = GetFocus();
            while (wnd != NULL && wnd != ctrl)
                wnd = GetParent(wnd);
            if (wnd == NULL) // fokusime jen pokud neni ctrl predek GetFocusu
            {                // jako napr. edit-line v combo-boxu
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)ctrl, TRUE);
            }
        }
        return FALSE;
    }
    else
        return TRUE;
}

BOOL CDialogEx::TransferData(CTransferType type)
{
    CALL_STACK_MESSAGE1("CDialogEx::TransferData()");
    CTransferInfoEx ti(HWindow, type);
    Transfer(ti);
    if (!ti.IsGood())
    {
        HWND ctrl;
        if (ti.GetControl(ctrl, ti.FailCtrlID, TRUE))
        {
            HWND wnd = GetFocus();
            while (wnd != NULL && wnd != ctrl)
                wnd = GetParent(wnd);
            if (wnd == NULL) // fokusime jen pokud neni ctrl predek GetFocusu
            {                // jako napr. edit-line v combo-boxu
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)ctrl, TRUE);
            }
        }
        return FALSE;
    }
    else
        return TRUE;
}

INT_PTR
CDialogEx::Execute()
{
    CALL_STACK_MESSAGE1("CDialogEx::Execute()");
    Modal = TRUE;

    //  if (HaveForMicrosoftSanfSerif)
    //  {
    return DialogBoxParamW(Modul, MAKEINTRESOURCEW(ResID), Parent,
                           (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
    //  }

    // nemame Microsoft Sans Serif musime ho v template vymenit
    // za MS Shell Dlg
    /*
  DWORD size;
  LPDLGTEMPLATE templ = LoadDlgTemplate(ResID, size);
  if (!templ) return -1;

  int ret = DialogBoxIndirectParamW(
    Modul, ReplaceDlgTemplateFont(templ, size, L"MS Shell Dlg"),  // dialog box template
    Parent, (DLGPROC)CDialog::CDialogProc, (LPARAM)this);

  free(templ);

  return ret;
*/
}

HWND CDialogEx::Create()
{
    CALL_STACK_MESSAGE1("CDialogEx::Create()");
    Modal = FALSE;

    //  if (HaveForMicrosoftSanfSerif)
    //  {
    return CreateDialogParamW(Modul, MAKEINTRESOURCEW(ResID), Parent,
                              (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
    //  }

    // nemame Microsoft Sans Serif musime ho v template vymenit
    // za MS Shell Dlg
    /*
  DWORD size;
  LPDLGTEMPLATE templ = LoadDlgTemplate(ResID, size);
  if (!templ) return NULL;

  HWND ret = CreateDialogIndirectParamW(
    Modul, ReplaceDlgTemplateFont(templ, size, L"MS Shell Dlg"),  // dialog box template
    Parent, (DLGPROC)CDialog::CDialogProc, (LPARAM)this);

  free(templ);

  return ret;
*/
}

INT_PTR
CDialogEx::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CDialogEx::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                             lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        DialogStackPush(HWindow);
        SG->MultiMonCenterWindow(HWindow, CenterToHWnd, FALSE);
        break;
    }

    case WM_DESTROY:
    {
        DialogStackPop();
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CDialogEx::NotifDlgJustCreated()
{
    SalGUI->ArrangeHorizontalLines(HWindow);
}

// ****************************************************************************
//
// CNewKeyDialog
//
//

void CNewKeyDialog::Validate(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CNewKeyDialog::Validate()");
    WCHAR buf[MAX_FULL_KEYNAME];
    ti.EditLineW(IDE_NAME, buf, MAX_FULL_KEYNAME);
    if (wcslen(buf) == 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_EMPTY), LoadStr(IDS_ERROR), MB_ICONERROR);
        ti.ErrorOn(IDE_NAME);
    }
}

void CNewKeyDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CNewKeyDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        if (Title)
            SendMessageW(HWindow, WM_SETTEXT, 0, (LPARAM)Title);
        if (Text)
            SendMessageW(GetDlgItem(HWindow, IDS_TEXT), WM_SETTEXT, 0, (LPARAM)Text);
    }
    ti.EditLineW(IDE_NAME, KeyName, MAX_FULL_KEYNAME);
    if (Direct)
        ti.CheckBox(IDC_DIRECT, *Direct);
}

// ****************************************************************************
//
// CNewValDialog
//
//

void CNewValDialog::Validate(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CNewValDialog::Validate()");
    CNewKeyDialog::Validate(ti);
}

void CNewValDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CNewValDialog::Transfer()");
    HWND combo = GetDlgItem(HWindow, IDC_TYPE);
    if (ti.Type == ttDataToWindow)
    {
        int sel = 0;
        CRegTypeText* tt = RegTypeTexts;
        while (tt->Text != NULL)
        {
            if (DlgType == vdtNewValDialog && tt->CanCreate ||
                DlgType == vdtEditValDialog && tt->CanEdit)
            {
                int ret = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)tt->Text);
                if (ret != CB_ERR)
                {
                    SendMessage(combo, CB_SETITEMDATA, ret, (LPARAM)tt->Type);
                    if (tt->Type == *Type)
                        sel = ret;
                }
            }
            tt++;
        }
        SendMessage(combo, CB_SETCURSEL, sel, 0);
    }
    else
    {
        int ret = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
        if (ret == CB_ERR)
        {
            ti.ErrorOn(IDC_TYPE);
            return;
        }
        *Type = (DWORD)SendMessage(combo, CB_GETITEMDATA, ret, 0); // x64 - Type je DWORD
    }
    CNewKeyDialog::Transfer(ti);
}

// ****************************************************************************
//
// CEditValDialog
//
//

void CEditValDialog::Validate(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CEditValDialog::Validate()");
    CNewValDialog::Validate(ti);
    if (!ti.IsGood())
        return;

    HWND combo = GetDlgItem(HWindow, IDC_TYPE);
    int ret = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (ret == CB_ERR)
    {
        ti.ErrorOn(IDC_TYPE);
        return;
    }
    *Type = (DWORD)SendMessage(combo, CB_GETITEMDATA, ret, 0); // x64 - Type je DWORD

    if (*Type == REG_DWORD || *Type == REG_DWORD_BIG_ENDIAN || *Type == REG_QWORD)
    {
        char buffer[21];
        HWND dataHWnd = GetDlgItem(HWindow, IDE_DATA);
        int i = (int)SendMessage(dataHWnd, WM_GETTEXT, 21, (LPARAM)buffer);
        QWORD d;
        if (Hex)
        {
            if (i == 0 || i > 16 || !HexStringToNumber(buffer, d))
            {
                Error(IDS_INVALIDHEXVALUE);
                ti.ErrorOn(IDE_DATA);
                return;
            }
        }
        else
        {
            if (i == 0 || !DecStringToNumber(buffer, d))
            {
                Error(IDS_INVALIDDECVALUE);
                ti.ErrorOn(IDE_DATA);
                return;
            }
        }
        if (*Type != REG_QWORD && HIDWORD(d) != 0)
        {
            Error(IDS_NOTDWORD);
            ti.ErrorOn(IDE_DATA);
        }
    }
}

void CEditValDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CEditValDialog::Transfer()");
    CNewValDialog::Transfer(ti);
    if (!ti.IsGood())
        return;

    HWND dataHWnd = GetDlgItem(HWindow, IDE_DATA);
    if (ti.Type == ttDataToWindow)
    {
        //SendMessageW(GetDlgItem(HWindow, IDE_NAME), WM_SETTEXT, 0, (LPARAM) KeyName);
        SendMessage(GetDlgItem(HWindow, IDR_HEX), BM_SETCHECK, BST_CHECKED, 0);
        RECT r;
        GetWindowRect(dataHWnd, &r);
        EditWidth = r.right - r.left;
        EditHeight = r.bottom - r.top;

        if (*Type != REG_DWORD && *Type != REG_DWORD_BIG_ENDIAN && *Type != REG_QWORD)
        {
            ShowWindow(GetDlgItem(HWindow, IDR_HEX), SW_HIDE);
            ShowWindow(GetDlgItem(HWindow, IDR_DEC), SW_HIDE);
            ShowWindow(GetDlgItem(HWindow, IDS_BASE), SW_HIDE);

            SendMessageW(dataHWnd, EM_LIMITTEXT, 0, 0);
            SendMessageW(dataHWnd, WM_SETTEXT, 0, (LPARAM)Data);
        }
        else
        {
            SendMessageW(dataHWnd, EM_LIMITTEXT, *Type == REG_QWORD ? 16 : 8, 0);
            SetWindowPos(dataHWnd, NULL, 0, 0, (int)(EditWidth * 0.46), EditHeight, SWP_NOZORDER | SWP_NOMOVE);

            char buffer[17];
            QWORD d = *Type == REG_QWORD ? *(LPQWORD)Data : *(LPDWORD)Data;
            if (*Type == REG_DWORD_BIG_ENDIAN)
                d = d >> 24 | (d & 0x00FF0000) >> 8 | (d & 0x0000FF00) << 8 | (d & 0x000000FF) << 24;
            _ui64toa(d, buffer, 16);
            SendMessage(dataHWnd, WM_SETTEXT, 0, (LPARAM)buffer);
        }

        SetFocus(GetDlgItem(HWindow, IDE_DATA));
    }
    else
    {
        if (*Type == REG_DWORD || *Type == REG_DWORD_BIG_ENDIAN || *Type == REG_QWORD)
        {
            char buffer[21];
            int i = (int)SendMessage(dataHWnd, WM_GETTEXT, 21, (LPARAM)buffer);
            QWORD d;
            if (Hex)
                HexStringToNumber(buffer, d);
            else
                DecStringToNumber(buffer, d);
            if (*Type == REG_DWORD_BIG_ENDIAN) // otocime endiany
            {
                d = d >> 24 | (d & 0x00FF0000) >> 8 | (d & 0x0000FF00) << 8 | (d & 0x000000FF) << 24;
            }
            DWORD s = *Type == REG_QWORD ? 8 : 4;
            if (s > Allocated)
            {
                void* ptr = realloc(Data, s);
                if (!ptr)
                {
                    Error(IDS_LOWMEM);
                    ti.ErrorOn(IDE_DATA);
                    return;
                }
                Data = (LPBYTE)ptr;
                Allocated = s;
            }
            Size = s;
            if (*Type == REG_QWORD)
                *(LPQWORD)Data = d;
            else
                *(LPDWORD)Data = (DWORD)d;
        }
        else
        {
            DWORD l = (DWORD)SendMessageW(dataHWnd, WM_GETTEXTLENGTH, 0, 0) * 2 + 2;
            if (l > Allocated)
            {
                void* ptr = realloc(Data, l);
                if (!ptr)
                {
                    Error(IDS_LOWMEM);
                    ti.ErrorOn(IDE_DATA);
                    return;
                }
                Data = (LPBYTE)ptr;
                Allocated = l;
            }
            Size = l;
            SendMessageW(dataHWnd, WM_GETTEXT, Size / 2, (LPARAM)Data);
        }
    }
}

INT_PTR
CEditValDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CEditValDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
        /* j.r. pokud nastavime spravne tab-order, neni toto treba a navic bude edit selected
    case WM_INITDIALOG:
    {
      CNewValDialog::DialogProc(uMsg, wParam, lParam);
      SetFocus(GetDlgItem(HWindow, IDE_DATA));
      return FALSE;
    }
    */

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_TYPE:
        {
            HWND combo = (HWND)lParam;
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int ret = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                if (ret == CB_ERR)
                    break;
                *Type = (DWORD)SendMessage(combo, CB_GETITEMDATA, ret, 0); // x64 - Type je DWORD

                DWORD show = *Type == REG_DWORD || *Type == REG_DWORD_BIG_ENDIAN || *Type == REG_QWORD ? SW_SHOW : SW_HIDE;
                ShowWindow(GetDlgItem(HWindow, IDR_HEX), show);
                ShowWindow(GetDlgItem(HWindow, IDR_DEC), show);
                ShowWindow(GetDlgItem(HWindow, IDS_BASE), show);
                int max;
                int width;
                if (show == SW_HIDE)
                {
                    max = 0;
                    width = EditWidth;
                }
                else
                {

                    max = Hex ? 8 : 10;
                    if (*Type == REG_QWORD)
                        max *= 2;
                    width = (int)(EditWidth * 0.46);
                }
                SetWindowPos(GetDlgItem(HWindow, IDE_DATA), NULL, 0, 0, width, EditHeight, SWP_NOZORDER | SWP_NOMOVE);
                SendMessageW(GetDlgItem(HWindow, IDE_DATA), EM_LIMITTEXT, max, 0);
            }
            break;
        }

        case IDR_DEC:
        case IDR_HEX:
        {
            BOOL hex = SendMessage(GetDlgItem(HWindow, IDR_HEX), BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (hex != Hex)
            {
                HWND combo = (HWND)GetDlgItem(HWindow, IDC_TYPE);
                int ret = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                if (ret == CB_ERR)
                    break;
                *Type = (DWORD)SendMessage(combo, CB_GETITEMDATA, ret, 0); // x64 - Type je DWORD

                char buffer[21];
                HWND dataHWnd = GetDlgItem(HWindow, IDE_DATA);
                int i = (int)SendMessage(dataHWnd, WM_GETTEXT, 21, (LPARAM)buffer);
                QWORD d;
                BOOL success = TRUE;
                if (hex)
                {
                    if (i == 0 || !DecStringToNumber(buffer, d))
                    {
                        if (i != 0)
                            Error(IDS_INVALIDDECVALUE);
                        success = FALSE;
                    }
                    else
                        _ui64toa(d, buffer, 16);
                }
                else
                {
                    if (i == 0 || i > 16 || !HexStringToNumber(buffer, d))
                    {
                        if (i != 0)
                            Error(IDS_INVALIDHEXVALUE);
                        success = FALSE;
                    }
                    else
                        _ui64toa(d, buffer, 10);
                }
                if (success)
                {
                    Hex = hex;
                    int max = Hex ? 8 : 10;
                    if (*Type == REG_QWORD)
                        max *= 2;
                    if (success)
                        SendMessage(dataHWnd, WM_SETTEXT, 0, (LPARAM)buffer);
                    SendMessageW(dataHWnd, EM_LIMITTEXT, max, 0);
                }
                else
                {
                    SendMessage(GetDlgItem(HWindow, IDR_HEX), BM_SETCHECK, Hex ? BST_CHECKED : BST_UNCHECKED, 0);
                    SendMessage(GetDlgItem(HWindow, IDR_DEC), BM_SETCHECK, Hex ? BST_UNCHECKED : BST_CHECKED, 0);
                }
            }
            break;
        }
        }
        break;
    }
    }
    return CNewValDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CRawEditValDialog
//
//

BOOL CRawEditValDialog::ExportToTempFile()
{
    CALL_STACK_MESSAGE1("CRawEditValDialog::ExportToTempFile()");
    // uz je vyexportovany
    if (*TempFile)
        return TRUE;

    // vytvorime jmeno tmp soubor
    if (!SG->SalGetTempFileName(NULL, "SAL", TempDir, FALSE, NULL))
        return Error(IDS_CREATETEMP);

    SG->SalPathAddBackslash(strcpy(TempFile, TempDir), MAX_PATH);
    int tlen = (int)strlen(TempFile);
    TempFile[tlen++] = '_';
    int len = min(MAX_PATH - tlen - 1, (int)wcslen(KeyName));
    WStrToStr(TempFile + tlen, MAX_PATH, KeyName, len);
    TempFile[len + tlen] = 0;
    ReplaceUnsafeCharacters(TempFile + tlen);

    // vytvorime/otevreme tmp soubor
    HANDLE file = CreateFile(TempFile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        SG->RemoveTemporaryDir(TempDir);
        *TempDir = *TempFile = 0;
        return Error(IDS_CREATETEMP);
    }

    DWORD written;
    BOOL b = WriteFile(file, Data, Size, &written, NULL) || written != Size;

    CloseHandle(file);

    if (!b)
    {
        SG->RemoveTemporaryDir(TempDir);
        *TempDir = *TempFile = 0;
        Error(IDS_WRITETEMP);
    }

    return b;
}

BOOL CRawEditValDialog::ImportFromTempFile()
{
    CALL_STACK_MESSAGE1("CRawEditValDialog::ImportFromTempFile()");
    // vytvorime/otevreme tmp soubor
    HANDLE file = CreateFile(TempFile, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return Error(IDS_OPENTEMP);

    CQuadWord size;
    DWORD err;
    if (!SG->SalGetFileSize(file, size, err))
    {
        CloseHandle(file);
        return ErrorL(err, GetParent(), IDS_SIZEOFTEMP);
    }

    if (size.HiDWord > 0)
    {
        CloseHandle(file);
        return Error(IDS_LONGDATA);
    }

    LPBYTE newData = (LPBYTE)malloc(size.LoDWord);
    if (!newData)
    {
        CloseHandle(file);
        return Error(IDS_LOWMEM);
    }

    DWORD read;
    BOOL b = ReadFile(file, newData, size.LoDWord, &read, NULL) || read != size.LoDWord;

    CloseHandle(file);

    if (b)
    {
        if (Data)
            free(Data);
        Data = newData;
        Size = Allocated = size.LoDWord;
    }
    else
        Error(IDS_READTEMP);

    return b;
}

void CRawEditValDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CRawEditValDialog::Transfer()");
    CNewValDialog::Transfer(ti);
    if (!ti.IsGood())
        return;

    // nacteme nova data z tempu
    if (Edit && !ImportFromTempFile())
    {
        ti.ErrorOn(IDB_EDIT);
        return;
    }
}

INT_PTR
CRawEditValDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRawEditValDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
        /* j.r. pokud nastavime spravne tab-order, neni toto treba a navic bude edit selected
    case WM_INITDIALOG:
    {
      CNewValDialog::DialogProc(uMsg, wParam, lParam);
      SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDB_EDIT), TRUE);
      return FALSE;
    }
    */

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_EDIT:
        {
            if (ExportToTempFile() && ExecuteEditor(TempFile))
            {
                Edit = TRUE;
                ShowWindow(GetDlgItem(HWindow, IDS_MESSAGE), SW_SHOW);
            }
            break;
        }
        }
        break;
    }

    case WM_DESTROY:
    {
        if (*TempDir)
            SG->RemoveTemporaryDir(TempDir);
        break;
    }
    }
    return CNewValDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CCopyOrMoveDialog
//
//

void CCopyOrMoveDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CCopyOrMoveDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        if (Title)
            SendMessageW(HWindow, WM_SETTEXT, 0, (LPARAM)Title);
        if (Text)
            SendMessageW(GetDlgItem(HWindow, IDS_TEXT), WM_SETTEXT, 0, (LPARAM)Text);
    }
    HistoryComboBox(ti, IDE_NAME, KeyName, MAX_FULL_KEYNAME, MAX_HISTORY_ENTRIES, CopyOrMoveHistory);
    if (Direct)
        ti.CheckBox(IDC_DIRECT, *Direct);
}

// ****************************************************************************
//
// CConfigDialog
//
//

void CConfigDialog::Validate(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CConfigDialog::Validate()");
    int e1, e2;
    char buffer[MAX_PATH];

    ti.EditLine(IDE_COMMAND, buffer, MAX_PATH);
    if (!SG->ValidateVarString(HWindow, buffer, e1, e2, ExpCommandVariables))
    {
        ti.ErrorOn(IDE_COMMAND);
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_COMMAND), TRUE);
        SendDlgItemMessage(HWindow, IDE_COMMAND, EM_SETSEL, e1, e2);
        return;
    }

    ti.EditLine(IDE_ARGUMENTS, buffer, MAX_PATH);
    if (!SG->ValidateVarString(HWindow, buffer, e1, e2, ExpArgumentsVariables))
    {
        ti.ErrorOn(IDE_ARGUMENTS);
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_ARGUMENTS), TRUE);
        SendDlgItemMessage(HWindow, IDE_ARGUMENTS, EM_SETSEL, e1, e2);
        return;
    }

    ti.EditLine(IDE_INITDIR, buffer, MAX_PATH);
    if (!SG->ValidateVarString(HWindow, buffer, e1, e2, ExpInitDirVariables))
    {
        ti.ErrorOn(IDE_INITDIR);
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_INITDIR), TRUE);
        SendDlgItemMessage(HWindow, IDE_INITDIR, EM_SETSEL, e1, e2);
        return;
    }
}

void CConfigDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CConfigDialog::Transfer()");
    ti.EditLine(IDE_COMMAND, Command, MAX_PATH);
    ti.EditLine(IDE_ARGUMENTS, Arguments, MAX_PATH);
    ti.EditLine(IDE_INITDIR, InitDir, MAX_PATH);
}

INT_PTR
CConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalGUI->ChangeToArrowButton(HWindow, IDB_BROWSE);
        SalGUI->ChangeToArrowButton(HWindow, IDB_ARGHELP);
        SalGUI->ChangeToArrowButton(HWindow, IDB_INITDIRHELP);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_BROWSE:
        {
            RECT r;
            GetWindowRect((HWND)lParam, &r);

            // vytvorime menu
            MENU_TEMPLATE_ITEM templ[] =
                {
                    {MNTT_PB, 0, 0, 0, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_BROWSE, MNTS_ALL, 1, -1, 0, NULL},
                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},

                    {MNTT_IT, IDS_EXP_WINDIR, MNTS_ALL, 2, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_SYSDIR, MNTS_ALL, 3, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_SALDIR, MNTS_ALL, 4, -1, 0, NULL},

                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},

                    {MNTT_IT, IDS_EXP_ENVVAR, MNTS_ALL, 30, -1, 0, NULL},

                    {MNTT_PE, 0, 0, 0, -1, 0, NULL}};

            CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
            if (!menu)
                return TRUE;
            if (!menu->LoadFromTemplate(HLanguage, templ, NULL, NULL, NULL))
            {
                SalGUI->DestroyMenuPopup(menu);
                return TRUE;
            }

            DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                                    r.right, r.top, HWindow, NULL);

            if (cmd > 0)
            {
                if (cmd == 1)
                {
                    char path[MAX_PATH];
                    path[0] = 0;
                    GetDlgItemText(HWindow, IDE_COMMAND, path, MAX_PATH);
                    if (GetOpenFileName(HWindow, NULL, LoadStr(IDS_EXEFILES), path))
                        SetDlgItemText(HWindow, IDE_COMMAND, path);
                }
                else if (cmd == 30)
                {
                    SendDlgItemMessage(HWindow, IDE_COMMAND, EM_REPLACESEL, TRUE, (LPARAM) "$[]");
                }
                else
                {
                    // jeste pro jistotu overime
                    if (cmd < 30)
                    {
                        char var[100];
                        sprintf(var, "$(%s)", ExpCommandVariables[cmd - 2].Name);
                        SendDlgItemMessage(HWindow, IDE_COMMAND, EM_REPLACESEL, TRUE, (LPARAM)var);
                    }
                }
            }

            SalGUI->DestroyMenuPopup(menu);
            return TRUE;
        }

        case IDB_ARGHELP:
        {
            RECT r;
            GetWindowRect((HWND)lParam, &r);

            // vytvorime menu
            MENU_TEMPLATE_ITEM templ[] =
                {
                    {MNTT_PB, 0, 0, 0, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_FULLNAME, MNTS_ALL, 1, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DRIVE, MNTS_ALL, 2, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_PATH, MNTS_ALL, 3, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_NAME, MNTS_ALL, 4, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_NAMEPART, MNTS_ALL, 5, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_EXTPART, MNTS_ALL, 6, -1, 0, NULL},

                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},

                    {MNTT_IT, IDS_EXP_FULLPATH, MNTS_ALL, 7, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_WINDIR, MNTS_ALL, 8, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_SYSDIR, MNTS_ALL, 9, -1, 0, NULL},

                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},

                    {MNTT_IT, IDS_EXP_DOSFULLNAME, MNTS_ALL, 10, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSDRIVE, MNTS_ALL, 11, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSPATH, MNTS_ALL, 12, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSNAME, MNTS_ALL, 13, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSNAMEPART, MNTS_ALL, 14, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSEXTPART, MNTS_ALL, 15, -1, 0, NULL},

                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},

                    {MNTT_IT, IDS_EXP_DOSFULLPATH, MNTS_ALL, 16, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSWINDIR, MNTS_ALL, 17, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DOSSYSDIR, MNTS_ALL, 18, -1, 0, NULL},

                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},

                    {MNTT_IT, IDS_EXP_ENVVAR, MNTS_ALL, 30, -1, 0, NULL},

                    {MNTT_PE, 0, 0, 0, -1, 0, NULL}};

            CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
            if (!menu)
                return TRUE;
            if (!menu->LoadFromTemplate(HLanguage, templ, NULL, NULL, NULL))
            {
                SalGUI->DestroyMenuPopup(menu);
                return TRUE;
            }

            DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                                    r.right, r.top, HWindow, NULL);

            if (cmd > 0)
            {
                if (cmd == 30)
                {
                    SendDlgItemMessage(HWindow, IDE_ARGUMENTS, EM_REPLACESEL, TRUE, (LPARAM) "$[]");
                }
                else
                {
                    // jeste pro jistotu overime
                    if (cmd < 19)
                    {
                        char var[100];
                        sprintf(var, "$(%s)", ExpArgumentsVariables[cmd - 1].Name);
                        SendDlgItemMessage(HWindow, IDE_ARGUMENTS, EM_REPLACESEL, TRUE, (LPARAM)var);
                    }
                }
            }

            SalGUI->DestroyMenuPopup(menu);
            return TRUE;
        }

        case IDB_INITDIRHELP:
        {
            RECT r;
            GetWindowRect((HWND)lParam, &r);

            // vytvorime menu
            MENU_TEMPLATE_ITEM templ[] =
                {
                    {MNTT_PB, 0, 0, 0, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_DRIVE, MNTS_ALL, 1, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_PATH, MNTS_ALL, 2, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_FULLPATH, MNTS_ALL, 3, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_WINDIR, MNTS_ALL, 4, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_SYSDIR, MNTS_ALL, 5, -1, 0, NULL},
                    {MNTT_SP, 0, MNTS_ALL, 0, -1, 0, NULL},
                    {MNTT_IT, IDS_EXP_ENVVAR, MNTS_ALL, 30, -1, 0, NULL},
                    {MNTT_PE, 0, 0, 0, -1, 0, NULL}};

            CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
            if (!menu)
                return TRUE;
            if (!menu->LoadFromTemplate(HLanguage, templ, NULL, NULL, NULL))
            {
                SalGUI->DestroyMenuPopup(menu);
                return TRUE;
            }

            DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                                    r.right, r.top, HWindow, NULL);

            if (cmd > 0)
            {
                if (cmd == 30)
                {
                    SendDlgItemMessage(HWindow, IDE_INITDIR, EM_REPLACESEL, TRUE, (LPARAM) "$[]");
                }
                else
                {
                    // jeste pro jistotu overime
                    if (cmd < 6)
                    {
                        char var[100];
                        sprintf(var, "$(%s)", ExpInitDirVariables[cmd - 1].Name);
                        SendDlgItemMessage(HWindow, IDE_INITDIR, EM_REPLACESEL, TRUE, (LPARAM)var);
                    }
                }
            }

            SalGUI->DestroyMenuPopup(menu);
            return TRUE;
        }
        }
        break;
    }
    }
    return CDialogEx::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CExportDialog
//
//

void CExportDialog::Validate(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CExportDialog::Validate()");
    WCHAR buffer[MAX_FULL_KEYNAME];
    ti.EditLineW(IDE_NAME, buffer, MAX_FULL_KEYNAME);
    if (wcslen(buffer) == 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_EMPTY), LoadStr(IDS_ERROR), MB_ICONERROR);
        ti.ErrorOn(IDE_NAME);
    }

    char buffer2[MAX_PATH];
    ti.EditLine(IDE_FILE, buffer2, MAX_PATH);
    if (strlen(buffer2) == 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_EMPTY), LoadStr(IDS_ERROR), MB_ICONERROR);
        ti.ErrorOn(IDE_FILE);
    }
}

void CExportDialog::Transfer(CTransferInfoEx& ti)
{
    CALL_STACK_MESSAGE1("CExportDialog::Transfer()");
    ti.EditLineW(IDE_NAME, Path, MAX_FULL_KEYNAME);
    ti.EditLine(IDE_FILE, File, MAX_PATH);
}

INT_PTR
CExportDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CExportDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_BROWSE:
        {
            char path[MAX_PATH];
            path[0] = 0;
            GetDlgItemText(HWindow, IDE_FILE, path, MAX_PATH);
            SG->SalPathRemoveBackslash(path);
            if (GetOpenFileName(HWindow, NULL, LoadStr(IDS_REGFILES), path, TRUE))
            {
                SG->SalPathAddExtension(path, ".reg", MAX_PATH);
                SetDlgItemText(HWindow, IDE_FILE, path);
            }
            return TRUE;
        }
        }
    }
    }
    return CDialogEx::DialogProc(uMsg, wParam, lParam);
}
