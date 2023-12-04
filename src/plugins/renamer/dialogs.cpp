// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

TDirectArray<DWORD_PTR> DialogStack(4, 4);
CCS DialogStackCS;

// TODO: pouzivat!
BOOL MinBeepWhenDone;

HICON
GetSH32DirIcon()
{
    CALL_STACK_MESSAGE_NONE
    SHFILEINFO shi;
    shi.hIcon = NULL;
    char systemDir[MAX_PATH];
    GetSystemDirectory(systemDir, MAX_PATH);
    __try
    {
        SHGetFileInfo(systemDir, 0, &shi, sizeof(shi),
                      SHGFI_SMALLICON |
                          SHGFI_ICON | /*SHGFI_SYSICONINDEX |*/
                          SHGFI_SHELLICONSIZE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        shi.hIcon = NULL;
    }

    return shi.hIcon;
}

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SG->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL InitDialogs()
{
    CALL_STACK_MESSAGE1("InitDialogs()");

    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        MaskHistory[i] = NULL;
        NewNameHistory[i] = NULL;
        SearchHistory[i] = NULL;
        ReplaceHistory[i] = NULL;
        CommandHistory[i] = NULL;
    }

    if (!InitializeWinLib("Renamer" /* neprekladat! */, DLLInstance))
        return FALSE;
    SetupWinLibHelp(HTMLHelpCallback);

    MinBeepWhenDone = TRUE;
    SG->GetConfigParameter(SALCFG_MINBEEPWHENDONE, &MinBeepWhenDone, sizeof(BOOL), NULL);

    HINSTANCE shell32DLL = LoadLibraryEx("shell32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (shell32DLL == NULL) // to se snad vubec nemuze stat (zaklad win 4.0)
    {
        TRACE_E("Nelze otevrit shell32.dll");
        ReleaseDialogs();
        return FALSE;
    }

    HSymbolsImageList = ImageList_Create(16, 16, ILC_MASK | SG->GetImageListColorFlags(), 3, 0);
    if (HSymbolsImageList == NULL)
    {
        FreeLibrary(shell32DLL);
        ReleaseDialogs();
        return FALSE;
    }
    ImageList_SetImageCount(HSymbolsImageList, 3);                      // inicializace
    ImageList_SetBkColor(HSymbolsImageList, GetSysColor(COLOR_WINDOW)); // aby pod XP chodily pruhledne ikonky

    // vytahnu z shell 32 ikony:
    int indexes[] = {ILS_DIRECTORY, ILS_FILE, -1};
    int resID[] = {4, 1, -1};
    HICON hIcon;
    int j;
    for (j = 0; indexes[j] != -1; j++)
    {
        hIcon = (HICON)LoadImage(shell32DLL, MAKEINTRESOURCE(resID[j]),
                                 IMAGE_ICON, 16, 16,
                                 SG->GetIconLRFlags());
        if (hIcon != NULL)
        {
            ImageList_ReplaceIcon(HSymbolsImageList, indexes[j], hIcon);
            DestroyIcon(hIcon);
        }
    }

    // nacteni ikonky adresare "system32"
    HICON sh32DirIcon = GetSH32DirIcon();

    // vykreslime ikonku adresare "system32" jako simple-iconku vsech adresaru
    if (sh32DirIcon != NULL) // pokud ikonku neziskame, je tam porad jeste 4-rka z shell32.dll
    {
        ImageList_ReplaceIcon(HSymbolsImageList, ILS_DIRECTORY, sh32DirIcon);
        DestroyIcon(sh32DirIcon);
    }

    FreeLibrary(shell32DLL);
    shell32DLL = NULL;

    // nacteme ikonku s vykricnikem
    hIcon = (HICON)LoadImage(DLLInstance, IDI_WARNING,
                             IMAGE_ICON, 16, 16,
                             SG->GetIconLRFlags());
    //  TRACE_I("ICON" << hIcon << "err" << GetLastError());
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(HSymbolsImageList, ILS_WARNING, hIcon);
        DestroyIcon(hIcon);
    }

    strcpy(DirText, LoadStr(IDS_DIRTEXT));

    HAccels = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_ACCELS));

    Silent = 0;
    BOOL b;
    if (SG->GetConfigParameter(SALCFG_CNFRMFILEOVER, &b, sizeof(b), NULL) && !b)
        Silent |= SILENT_OVERWRITE_FILE_EXIST;
    if (SG->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &b, sizeof(b), NULL) && !b)
        Silent |= SILENT_OVERWRITE_FILE_SYSHID;

    return TRUE;
}

void ReleaseDialogs()
{
    CALL_STACK_MESSAGE1("ReleaseDialogs()");

    ReleaseWinLib(DLLInstance);

    if (HSymbolsImageList)
        ImageList_Destroy(HSymbolsImageList);

    int i;
    for (i = 0; i < MAX_HISTORY_ENTRIES; i++)
    {
        if (MaskHistory[i])
        {
            free(MaskHistory[i]);
            MaskHistory[i] = NULL;
        }
        if (NewNameHistory[i])
        {
            free(NewNameHistory[i]);
            NewNameHistory[i] = NULL;
        }
        if (SearchHistory[i])
        {
            free(SearchHistory[i]);
            SearchHistory[i] = NULL;
        }
        if (ReplaceHistory[i])
        {
            free(ReplaceHistory[i]);
            ReplaceHistory[i] = NULL;
        }
        if (CommandHistory[i])
        {
            free(CommandHistory[i]);
            CommandHistory[i] = NULL;
        }
    }
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

UINT_PTR CALLBACK
ComDlgHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ComDlgHookProc(, 0x%X, 0x%IX, 0x%IX)", uiMsg, wParam,
                        lParam);
    if (uiMsg == WM_INITDIALOG)
    {
        // SalamanderGUI->ArrangeHorizontalLines(hdlg);  // pro Windows common dialogy tohle nedelame
        SG->MultiMonCenterWindow(hdlg, GetParent(hdlg), FALSE);
        return 1;
    }
    return 0;
}

void HistoryComboBox(CTransferInfo& ti, int id, char* text, int textMax,
                     int historySize, char** history)
{
    CALL_STACK_MESSAGE4("HistoryComboBox(, %d, , %d, %d, )", id, textMax,
                        historySize);
    HWND combo;

    if (!ti.GetControl(combo, id))
        return;

    if (ti.Type == ttDataFromWindow)
    {
        ti.EditLine(id, text, textMax);

        int toMove = historySize - 1;

        // podivame jestli uz stejna polozka neni v historii
        int j;
        for (j = 0; j < historySize; j++)
        {
            if (history[j] == NULL)
                break;
            if (SG->StrICmp(history[j], text) == 0)
            {
                toMove = j;
                break;
            }
        }
        // alokujeme si pamet pro novou polozku
        LPTSTR ptr = _tcsdup(text);
        if (ptr)
        {
            // uvolnime pamet vymazavane polozky
            if (history[toMove])
                free(history[toMove]);
            // vytvorime misto pro cestu kterou budeme ukladat
            int i;
            for (i = toMove; i > 0; i--)
                history[i] = history[i - 1];
            // ulozime cestu
            history[0] = ptr;
        }
    }
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; i < historySize; i++)
    {
        if (history[i] == NULL)
            break;
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)history[i]);
    }
    if (ti.Type == ttDataFromWindow)
        SendMessage(combo, CB_SETCURSEL, 0, 0);
    else
    {
        SendMessage(combo, CB_LIMITTEXT, textMax - 1, 0);
        SendMessage(combo, WM_SETTEXT, 0, (LPARAM)text);
        SendMessage(combo, CB_SETEDITSEL, 0, -1);
    }
}

void TransferCombo(CTransferInfo& ti, int id, int* comboContent, int& value)
{
    CALL_STACK_MESSAGE3("TransferCombo(, %d, , %d)", id, value);
    HWND combo;

    if (!ti.GetControl(combo, id))
        return;

    if (ti.Type == ttDataToWindow)
    {
        SendMessage(combo, CB_RESETCONTENT, 0, 0);
        int i;
        for (i = 0; comboContent[i + 1] != -1; i += 2)
        {
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(comboContent[i + 1]));
            if (value == comboContent[i])
                SendMessage(combo, CB_SETCURSEL, i / 2, 0);
        }
    }
    else
    {
        int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR)
            i = 0;
        value = comboContent[i * 2];
    }
}

//******************************************************************************
//
// CComboboxEdit
//

CComboboxEdit::CComboboxEdit(BOOL helperButton)
    : CWindow(ooAllocated)
{
    CALL_STACK_MESSAGE_NONE
    SelStart = 0;
    SelEnd = -1;
    SetSelOnFocus = FALSE;
    HelperButton = helperButton;
    SkipCharacter = FALSE;
}

LRESULT
CComboboxEdit::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CComboboxEdit::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CHAR:
    {
        if (SkipCharacter)
            return 0;
        break;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        SkipCharacter = FALSE;
        BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if (HelperButton && wParam == VK_RIGHT && altPressed)
        {
            HWND combo = GetParent(HWindow);
            HWND dialog = GetParent(combo);
            HWND next = GetNextDlgTabItem(dialog, combo, FALSE);
            BOOL nextIsButton;
            if (next != NULL)
            {
                char className[30];
                WORD wl = LOWORD(GetWindowLong(next, GWL_STYLE)); // jen BS_...
                nextIsButton = GetClassName(next, className, 30) != 0 &&
                               SG->StrICmp(className, "BUTTON") == 0; // &&
                                                                      // (wl == BS_PUSHBUTTON || wl == BS_DEFPUSHBUTTON ||
                                                                      // wl == BS_ICON));
            }
            else
                nextIsButton = FALSE;
            if (nextIsButton)
            {
                SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
                PostMessage(dialog, WM_COMMAND, MAKELONG(GetDlgCtrlID(next), BN_CLICKED),
                            LPARAM(next));
            }
            return 0;
        }

        if (ctrlPressed && !shiftPressed && !altPressed && wParam == 'A')
        {
            // od Windows Vista uz SelectAll standardne funguje, takze tam nechame select all na nich
            if (!SalIsWindowsVersionOrGreater(6, 0, 0))
            {
                SendMessage(HWindow, EM_SETSEL, 0, -1);
                SkipCharacter = TRUE; // zamezime pipnuti
                return TRUE;
            }
        }

        break;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        SkipCharacter = FALSE;
        break;
    }

    case WM_KILLFOCUS:
    {
        SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
        break;
    }

    case WM_SETFOCUS:
    {
        if (SetSelOnFocus)
        {
            LRESULT res = CWindow::WindowProc(uMsg, wParam, lParam);
            SendMessage(HWindow, EM_SETSEL, (WPARAM)SelStart, (LPARAM)SelEnd);
            SetSelOnFocus = FALSE;
            return res;
        }
        break;
    }

    case EM_REPLACESEL:
    {
        LRESULT res = CWindow::WindowProc(uMsg, wParam, lParam);
        SendMessage(HWindow, EM_GETSEL, (WPARAM)&SelStart, (LPARAM)&SelEnd);
        return res;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void CComboboxEdit::GetSel(DWORD* start, DWORD* end)
{
    CALL_STACK_MESSAGE_NONE
    if (GetFocus() == HWindow)
        SendMessage(HWindow, EM_GETSEL, (WPARAM)start, (LPARAM)end);
    else
    {
        *start = SelStart;
        *end = SelEnd;
    }
}

void CComboboxEdit::SetSel(DWORD start, DWORD end)
{
    CALL_STACK_MESSAGE_NONE
    if (GetFocus() == HWindow)
        SendMessage(HWindow, EM_SETSEL, (WPARAM)start, (LPARAM)end);
    else
    {
        SelStart = start;
        SelEnd = end;
        SetSelOnFocus = TRUE;
    }
}

void CComboboxEdit::ReplaceText(const char* text)
{
    CALL_STACK_MESSAGE_NONE
    // musime ozivit selection, protoze dementni combobox ji zapomel
    SendMessage(HWindow, EM_SETSEL, SelStart, SelEnd);
    SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)text);
}

//******************************************************************************
//
// CNotifyEdit
//

CNotifyEdit::CNotifyEdit()
{
    SkipCharacter = FALSE;
}

LRESULT
CNotifyEdit::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CNotifyEdit::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        SkipCharacter = FALSE;
        BOOL ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if (ctrlPressed && !shiftPressed && !altPressed && wParam == 'A')
        {
            // ani pod Windows Vista nefunguje v tomto pripade Ctrl+A, takze resime ve vsech pripadech
            SendMessage(HWindow, EM_SETSEL, 0, -1);
            SkipCharacter = TRUE; // zamezime pipnuti
            return TRUE;
        }

        if (!ctrlPressed && !shiftPressed && !altPressed && wParam == VK_ESCAPE)
        {
            PostMessage(GetParent(HWindow), WM_COMMAND, IDCANCEL, 0);
            return TRUE;
        }
    }
    case WM_SETFOCUS:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        SendMessage(GetParent(HWindow), WM_USER_CARETMOVE, 0, 0);
        return ret;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        SkipCharacter = FALSE;
        break;
    }

    case WM_CHAR:
    {
        if (SkipCharacter)
            return 0;
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
//******************************************************************************
//
// CAddCutDialog
//

void CAddCutDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CAddCutDialog::Validate()");
    int i;
    ti.EditLine(IDE_START, i);
    if (i < 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_BADCUTOFFSET), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_START);
        return;
    }
    ti.EditLine(IDE_END, i);
    if (i < 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_BADCUTOFFSET), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_END);
        return;
    }
}

void CAddCutDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CAddCutDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);

    const char* cutFromVals[7] =
        {
            VarOriginalName,
            VarDrive,
            VarPath,
            VarRelativePath,
            VarName,
            VarNamePart,
            VarExtPart};
    int fromCombo[] =
        {
            1, IDS_VAR_ORIGINALNAME,
            2, IDS_VAR_DRIVE,
            3, IDS_VAR_PATH,
            4, IDS_VAR_RELATIVEPATH,
            5, IDS_VAR_NAME,
            6, IDS_VAR_NAMEPART,
            7, IDS_VAR_EXTPART,
            0, -1};
    int cutFrom = 0;
    for (int i = 0; i < _countof(cutFromVals); i++)
    {
        if (cutFromVals[i] == *CutFrom)
        {
            cutFrom = i + 1;
            break;
        }
    }
    TransferCombo(ti, IDC_CUTFROM, fromCombo, cutFrom);
    if (cutFrom - 1 >= 0 && cutFrom - 1 < _countof(cutFromVals))
        *CutFrom = cutFromVals[cutFrom - 1];
    ti.EditLine(IDE_START, *Left);
    ti.EditLine(IDE_END, *Right);
    int signCombos[] =
        {
            1, IDS_LEFT,
            -1, IDS_RIGHT,
            0, -1};
    TransferCombo(ti, IDC_START, signCombos, *LeftSign);
    TransferCombo(ti, IDC_END, signCombos, *RightSign);
}

//******************************************************************************
//
// CAddCounterDialog
//

void CAddCounterDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CAddCounterDialog::Validate()");
    int i;
    double f;
    ti.EditLine(IDE_START, i);
    char buff[] = "%.f";
    ti.EditLine(IDE_STEP, f, buff);
    ti.EditLine(IDE_MINWIDTH, i);
    if (i < 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_BADCUTOFFSET), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_MINWIDTH);
        return;
    }
}

void CAddCounterDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CAddCounterDialog::Transfer()");
    int start = 0, base = 'd', minwidth = 0, fill = ' ';
    double step = 1;
    BOOL left = FALSE;
    char stepStr[100];

    ti.EditLine(IDE_START, start);
    char buff[] = "%.f";
    ti.EditLine(IDE_STEP, step, buff);
    if (ti.Type == ttDataFromWindow)
    {
        ti.EditLine(IDE_STEP, stepStr, 100);
        char* comma = strchr(stepStr, ',');
        if (comma)
            *comma = '.';
    }
    int baseCombos[] =
        {
            'd', IDS_DECIMAL,
            'x', IDS_HEX_LOWER,
            'X', IDS_HEX_UPPER,
            0, -1};
    TransferCombo(ti, IDC_BASE, baseCombos, base);
    ti.EditLine(IDE_MINWIDTH, minwidth);
    int fillCombos[] =
        {
            ' ', IDS_FILL_SPACES,
            '0', IDS_FILL_ZEROS,
            '_', IDS_FILL_UNDERSCORES,
            0, -1};
    TransferCombo(ti, IDC_FILL, fillCombos, fill);
    ti.CheckBox(IDC_LEFT, left);

    if (ti.Type == ttDataToWindow)
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);
    else
    {
        int i = 0;
        if (start > 0 || step != 1 || base != 'd' || minwidth > 0)
            i += sprintf(Buffer, "%d", start);
        if (step != 1 || base != 'd' || minwidth > 0)
            i += sprintf(Buffer + i, ",%s", stepStr);
        if (base != 'd' || minwidth > 0)
            i += sprintf(Buffer + i, ",%c", (char)base);
        if (minwidth > 0)
        {
            if (left)
                i += sprintf(Buffer + i, ",l");
            i += sprintf(Buffer + i, ",%d", minwidth);
            if (fill != ' ')
                i += sprintf(Buffer + i, ",%c", (char)fill);
        }
        Buffer[i] = 0;
    }
}

//******************************************************************************
//
// CDefineClassDialog
//

void CDefineClassDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CDefineClassDialog::Validate()");
    char buffer[512];
    ti.EditLine(IDE_SET, buffer, 512);
    if (strlen(buffer) == 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_EMPTYSTR), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_SET);
        return;
    }
}

void CDefineClassDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CDefineClassDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);

    ti.EditLine(IDE_SET, Buffer, 512);
}

//******************************************************************************
//
// CSubexpressionDialog
//

void CSubexpressionDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CSubexpressionDialog::Validate()");
    int i;
    ti.EditLine(IDC_NUMBER, i);
    if (i < 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_BADCUTOFFSET), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_NUMBER);
        return;
    }
}

void CSubexpressionDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CSubexpressionDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);

        char buf[4];
        int i;
        for (i = 0; i < 256; i++)
        {
            sprintf(buf, "%d", i);
            SendDlgItemMessage(HWindow, IDC_NUMBER, CB_ADDSTRING, 0, LPARAM(buf));
        }
        SendDlgItemMessage(HWindow, IDC_NUMBER, CB_SETCURSEL, 0, 0);
        SendDlgItemMessage(HWindow, IDC_NUMBER, CB_LIMITTEXT, 3, 0);
    }
    else
        ti.EditLine(IDC_NUMBER, *Number);

    int caseCombos[] =
        {
            ccDontChange, IDS_CASE_DONTCHANGE,
            ccLower, IDS_CASE_LOWER,
            ccUpper, IDS_CASE_UPPER,
            ccMixed, IDS_CASE_MIXED,
            -1, -1};
    int c = (int)*Case;
    TransferCombo(ti, IDC_CASE, caseCombos, c);
    *Case = (CChangeCase)c;
}

//******************************************************************************
//
// CCommandDialog
//

void CCommandDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCommandDialog::Validate()");
    char buffer[4096];
    ti.EditLine(IDC_COMMAND, buffer, 4096);
    if (strlen(buffer) <= 0)
    {
        SG->SalMessageBox(HWindow, LoadStr(IDS_EXP_EMPTYSTR), LoadStr(IDS_ERROR),
                          MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_COMMAND);
        return;
    }
}

void CCommandDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCommandDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);

    HistoryComboBox(ti, IDC_COMMAND, Command, 4096, MAX_HISTORY_ENTRIES, CommandHistory);
}

//******************************************************************************
//
// CCommandErrorDialog
//

void CCommandErrorDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCommandErrorDialog::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        SetDlgItemText(HWindow, IDE_COMMAND, Command);
        char buf[100];
        sprintf(buf, "%u (0x%x)", ExitCode, ExitCode);
        SetDlgItemText(HWindow, IDS_EXITCODE, buf);
        SetDlgItemText(HWindow, IDE_STDERR, StdError);
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);
    }
}

// ****************************************************************************
//
// CProgressDialog
//

BOOL CProgressDialog::Update(DWORD current, BOOL force)
{
    CALL_STACK_MESSAGE_NONE
    if (force || 0 < (int)(GetTickCount() - NextUpdate) ||
        abs((__int64)(current - Current)) > 100 ||
        current == 0 || current == 1000)
    {
        Progress->SetProgress(current, NULL);
        Current = current;
        NextUpdate = GetTickCount() + 100;
    }
    // vyprazdnime message loopu
    EmptyMessageLoop();
    return Cancel;
}

void CProgressDialog::CancelOperation()
{
    CALL_STACK_MESSAGE_NONE
    if (!Cancel)
    {
        Cancel = SG->SalMessageBox(HWindow, LoadStr(IDS_CANCELCNFRM), LoadStr(IDS_QUESTION),
                                   MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND) == IDYES;
        if (Cancel)
        {
            SetDlgItemText(HWindow, IDS_MESSAGE, LoadStr(IDS_CANCELING));
            // zrusime close button
            LONG l = GetWindowLong(HWindow, GWL_STYLE);
            l &= ~WS_SYSMENU;
            SetWindowLong(HWindow, GWL_STYLE, l);
            SetWindowPos(HWindow, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            EmptyMessageLoop();
        }
    }
}

void CProgressDialog::SetText(const char* text)
{
    CALL_STACK_MESSAGE_NONE
    SetDlgItemText(HWindow, IDS_MESSAGE, text);
}

void CProgressDialog::EmptyMessageLoop()
{
    CALL_STACK_MESSAGE_NONE
    // vyprazdnime message loopu
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (!IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

INT_PTR
CProgressDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CProgressDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        DialogStackPush(HWindow);
        Progress = SalGUI->AttachProgressBar(HWindow, IDS_PROGRESS);
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);
        NextUpdate = GetTickCount();
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        {
            CancelOperation();
            return TRUE;
        }
        }
        break;
    }

    case WM_CLOSE:
    {
        CancelOperation();
        return TRUE;
    }

    case WM_DESTROY:
        DialogStackPop();
        break;
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CConfigDialog
//
//

void CConfigDialog::Validate(CTransferInfo& ti)
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

void CConfigDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CConfigDialog::Transfer()");
    ti.EditLine(IDE_COMMAND, Command, MAX_PATH);
    ti.EditLine(IDE_ARGUMENTS, Arguments, MAX_PATH);
    ti.EditLine(IDE_INITDIR, InitDir, MAX_PATH);
    ti.CheckBox(IDC_CONFIRMESCCLOSE, ConfirmESCClose);
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
        SalGUI->AttachButton(HWindow, IDB_FONT, BTF_RIGHTARROW);
        SG->MultiMonCenterWindow(HWindow, Parent, FALSE);
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

        case IDB_FONT:
        {
            RECT r;
            GetWindowRect((HWND)lParam, &r);

            // vytvorime menu
            MENU_TEMPLATE_ITEM templ[] =
                {
                    {MNTT_PB, 0, 0, 0, -1, 0, NULL},
                    {MNTT_IT, IDS_MENU_AUTOFONT, MNTS_ALL, CMD_AUTOFONT, -1, 0, NULL},
                    {MNTT_IT, IDS_MENU_CUSTOMFONT, MNTS_ALL, CMD_FONT, -1, 0, NULL},
                    {MNTT_PE, 0, 0, 0, -1, 0, NULL}};

            CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
            if (!menu)
                return TRUE;
            if (!menu->LoadFromTemplate(HLanguage, templ, NULL, NULL, NULL))
            {
                SalGUI->DestroyMenuPopup(menu);
                return TRUE;
            }
            menu->CheckItem(CMD_AUTOFONT, FALSE, !UseCustomFont);
            menu->CheckItem(CMD_FONT, FALSE, UseCustomFont);

            DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                                    r.right, r.top, HWindow, NULL);

            switch (cmd)
            {
            case CMD_FONT:
            {
                CHOOSEFONT cf;
                LOGFONT logFont = ManualModeLogFont;
                memset(&cf, 0, sizeof(CHOOSEFONT));
                cf.lStructSize = sizeof(CHOOSEFONT);
                cf.hwndOwner = HWindow;
                cf.hDC = NULL;
                cf.lpLogFont = &logFont;
                cf.iPointSize = 10;
                cf.Flags = CF_SCREENFONTS | CF_ENABLEHOOK | CF_FORCEFONTEXIST;
                if (UseCustomFont)
                    cf.Flags |= CF_INITTOLOGFONTSTRUCT;
                cf.lpfnHook = ComDlgHookProc;
                if (ChooseFont(&cf) != 0)
                {
                    ManualModeLogFont = logFont;
                    UseCustomFont = TRUE;
                }
                break;
            }

            case CMD_AUTOFONT:
            {
                UseCustomFont = FALSE;
                break;
            }
            }

            SalGUI->DestroyMenuPopup(menu);
            return TRUE;
        }
        }
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}
