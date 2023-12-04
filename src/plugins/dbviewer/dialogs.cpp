// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "data.h"
#include "renderer.h"
#include "dialogs.h"
#include "dbviewer.h"
#include "dbviewer.rh"
#include "dbviewer.rh2"
#include "lang\lang.rh"

// from windowsx.h
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

#define ListView_SetItemTextW(hwndLV, i, iSubItem_, pszText_) \
    { \
        LV_ITEMW _ms_lvi; \
        _ms_lvi.iSubItem = iSubItem_; \
        _ms_lvi.pszText = pszText_; \
        SNDMSG((hwndLV), LVM_SETITEMTEXTW, (WPARAM)(i), (LPARAM)(LV_ITEM*)&_ms_lvi); \
    }

char* FindHistory[FIND_HISTORY_SIZE];

//****************************************************************************
//
// HistoryComboBox
//

void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* Text,
                     int textLen, int historySize, char* history[])
{
    CALL_STACK_MESSAGE4("HistoryComboBox(, , %d, , %d, %d, )",
                        ctrlID, textLen, historySize);
    HWND hwnd;
    if (ti.GetControl(hwnd, ctrlID))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
            SendMessage(hwnd, CB_LIMITTEXT, textLen - 1, 0);
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)Text);
        }
        else
        {
            SendMessage(hwnd, WM_GETTEXT, textLen, (LPARAM)Text);
            SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
            SendMessage(hwnd, CB_LIMITTEXT, textLen - 1, 0);
            SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)Text);

            // vse o.k. zalozime do historie
            if (ti.IsGood())
            {
                if (Text[0] != 0)
                {
                    BOOL insert = TRUE;
                    int i;
                    for (i = 0; i < historySize; i++)
                    {
                        if (history[i] != NULL)
                        {
                            if (strcmp(history[i], Text) == 0) // je-li uz v historii
                            {                                  // pujde na 0. pozici
                                if (i > 0)
                                {
                                    char* swap = history[i];
                                    memmove(history + 1, history, i * sizeof(char*));
                                    history[0] = swap;
                                }
                                insert = FALSE;
                                break;
                            }
                        }
                        else
                            break;
                    }

                    if (insert)
                    {
                        char* newText = _tcsdup(Text);
                        if (newText != NULL)
                        {
                            if (history[historySize - 1] != NULL)
                                free(history[historySize - 1]);
                            memmove(history + 1, history,
                                    (historySize - 1) * sizeof(char*));
                            history[0] = newText;
                        }
                        else
                            TRACE_E("Low memory");
                    }
                }
            }
        }

        int i;
        for (i = 0; i < historySize; i++) // naplneni listu combo-boxu
            if (history[i] != NULL)
                SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)history[i]);
            else
                break;
    }
}

//****************************************************************************
//
// CCommonDialog
//

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent)
    : CDialog(hInstance, resID, hParent)
{
}

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent)
    : CDialog(hInstance, resID, helpID, hParent)
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
            SalGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//****************************************************************************
//
// CGoToDialog
//

CGoToDialog::CGoToDialog(HWND hParent, int* record, int recordCount)
    : CCommonDialog(HLanguage, IDD_GOTO, hParent)
{
    pRecord = record;
    RecordCount = recordCount;
}

void CGoToDialog::Transfer(CTransferInfo& ti)
{
    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_GOTO_RECORD))
    {
        TCHAR buff[20];
        if (ti.Type == ttDataToWindow)
        {
            wsprintf(buff, _T("%d"), (*pRecord) + 1);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)buff);
        }
        else
        {
            GetWindowText(hWnd, buff, SizeOf(buff));
            int val = _ttoi(buff) - 1;
            if (val < 0)
                val = 0;
            if (val >= RecordCount)
                val = RecordCount - 1;
            *pRecord = val;
        }
    }
}

INT_PTR
CGoToDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CGoToDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(HWindow, IDC_GOTO_RECORD, EM_SETLIMITTEXT, 10, 0);
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCSVOptionsDialog
//

// slouzi pro neustale prepisovani jednoho povoleneho znaku
class COneCharEditLine : public CWindow
{
public:
    COneCharEditLine(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        /*
      if (uMsg == WM_CHAR)
      {
        char buff[2];
        buff[0] = wParam;
        buff[1] = 0;
        SetWindowText(HWindow, buff);
        SendMessage(HWindow, EM_SETSEL, 0, 1);
        return 0;
      }
      */
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

CCSVOptionsDialog::CCSVOptionsDialog(HWND hParent, CCSVConfig* cfgCSV, CCSVConfig* defaultCfgCSV)
    : CCommonDialog(HLanguage, IDD_CSV_CONFIG, IDD_CSV_CONFIG, hParent)
{
    CfgCSV = cfgCSV;
    DefaultCfgCSV = defaultCfgCSV;
}

void CCSVOptionsDialog::MyTransfer(CTransferInfo& ti, CCSVConfig* cfg)
{
    ti.RadioButton(IDC_CSV_AS_SEP, 0, cfg->ValueSeparator);
    ti.RadioButton(IDC_CSV_TABULATOR, 1, cfg->ValueSeparator);
    ti.RadioButton(IDC_CSV_SEMICOLON, 2, cfg->ValueSeparator);
    ti.RadioButton(IDC_CSV_COMMA, 3, cfg->ValueSeparator);
    ti.RadioButton(IDC_CSV_SPACE, 4, cfg->ValueSeparator);
    ti.RadioButton(IDC_CSV_OTHER, 5, cfg->ValueSeparator);

    ti.RadioButton(IDC_CSV_AS_TQ, 0, cfg->TextQualifier);
    ti.RadioButton(IDC_CSV_DOUBLE, 1, cfg->TextQualifier);
    ti.RadioButton(IDC_CSV_QUOTES, 2, cfg->TextQualifier);
    ti.RadioButton(IDC_CSV_NONE, 3, cfg->TextQualifier);
    char buff[2];
    buff[0] = ' ';
    if (ti.Type == ttDataToWindow)
    {
        buff[0] = cfg->ValueSeparatorChar;
        buff[1] = 0;
    }
    ti.EditLine(IDE_CSV_OTHER, buff, 2);
    if (ti.Type == ttDataFromWindow)
        cfg->ValueSeparatorChar = buff[0]; // kdyz bude prazdny retezec, bude to '\0'

    ti.RadioButton(IDC_CSV_AS_FR, 0, cfg->FirstRowAsName);
    ti.RadioButton(IDC_CSV_ASHEADER, 1, cfg->FirstRowAsName);
    ti.RadioButton(IDC_CSV_ASDATA, 2, cfg->FirstRowAsName);
}

void CCSVOptionsDialog::EnableControls()
{
    BOOL other = IsDlgButtonChecked(HWindow, IDC_CSV_OTHER) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDE_CSV_OTHER), other);
}

void CCSVOptionsDialog::Transfer(CTransferInfo& ti)
{
    MyTransfer(ti, CfgCSV);
    if (ti.Type == ttDataToWindow)
        EnableControls();
}

INT_PTR
CCSVOptionsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        COneCharEditLine* edit = new COneCharEditLine(HWindow, IDE_CSV_OTHER);
        if (!DefaultCfgCSV)
        {
            ShowWindow(GetDlgItem(HWindow, IDC_CSV_SETDEF), SW_HIDE);
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_CSV_SETDEF:
        {
            CTransferInfo ti(HWindow, ttDataFromWindow);
            MyTransfer(ti, DefaultCfgCSV);
            return 0;
        }

        case IDC_CSV_TABULATOR:
        case IDC_CSV_SEMICOLON:
        case IDC_CSV_COMMA:
        case IDC_CSV_SPACE:
        case IDC_CSV_OTHER:
        {
            EnableControls();
            return 0;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPropertiesDialog
//

CPropertiesDialog::CPropertiesDialog(HWND hParent, CRendererWindow* renderer)
    : CCommonDialog(HLanguage, IDD_PROPERTIES, hParent)
{
    Renderer = renderer;
}

void CPropertiesDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow && Renderer != NULL && Renderer->Database.IsOpened())
    {
        Renderer->Database.GetFileInfo(GetDlgItem(HWindow, IDC_PROP_INFO));
    }
}

//****************************************************************************
//
// CConfigurationDialog
//

CConfigurationDialog::CConfigurationDialog(HWND hParent, BOOL enableCSVOptions)
    : CCommonDialog(HLanguage, IDD_CONFIGURATION, IDD_CONFIGURATION, hParent)
{
    bEnableCSVOptions = enableCSVOptions;
    HFont = NULL;
    UseCustomFont = CfgUseCustomFont;
    memcpy(&LogFont, &CfgLogFont, sizeof(LOGFONT));
}

CConfigurationDialog::~CConfigurationDialog()
{
    if (HFont != NULL)
        DeleteObject(HFont);
}

void CConfigurationDialog::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_CFG_SAVEPOSONCLOSE, 1, CfgSavePosition);
    ti.RadioButton(IDC_CFG_SETBYMAINWINDOW, 0, CfgSavePosition);
    if (ti.Type == ttDataToWindow)
    {
        SetFontText();
    }
    else
    {
        CfgUseCustomFont = UseCustomFont;
        memcpy(&CfgLogFont, &LogFont, sizeof(LOGFONT));
        if (CfgSavePosition)
            CfgWindowPlacement.length = 0;
    }
}

void CConfigurationDialog::SetFontText()
{
    LOGFONT logFont;
    if (UseCustomFont)
        logFont = LogFont;
    else
        GetDefaultLogFont(&logFont);

    HWND hEdit = GetDlgItem(HWindow, IDE_CFG_FONT);
    int oldHeight = logFont.lfHeight;
    logFont.lfHeight = SalamanderGUI->GetWindowFontHeight(hEdit); // pro prezentaci fontu v edit line pouzijeme jeji velikost fontu
    if (HFont != NULL)
        DeleteObject(HFont);
    HFont = CreateFontIndirect(&logFont);

    HDC hDC = GetDC(HWindow);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)HFont, MAKELPARAM(TRUE, 0));
    char buf[LF_FACESIZE + 200];
    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_FONTDESCRIPTION),
                MulDiv(-oldHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY)),
                logFont.lfFaceName,
                LoadStr(UseCustomFont ? IDS_CUSTOMFONT : IDS_DEFAULTFONT));
    SetWindowText(hEdit, buf);
    ReleaseDC(HWindow, hDC);
}

INT_PTR
CConfigurationDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->AttachButton(HWindow, IDB_CFG_FONT, BTF_RIGHTARROW);
        if (!bEnableCSVOptions)
        {
            ShowWindow(GetDlgItem(HWindow, IDB_CFG_CSVOPTIONS), SW_HIDE);
        }
        break;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_CFG_FONT:
        {
            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenu() dole...
MENU_TEMPLATE_ITEM ConfigurationDialogMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_USEDEFAULTFONT
  {MNTT_IT, IDS_USECUSTOMFONT
  {MNTT_PE, 0
};
*/
            HMENU hMenu = CreatePopupMenu();
            BOOL cstFont = UseCustomFont;
            InsertMenu(hMenu, 0xFFFFFFFF, cstFont ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_USEDEFAULTFONT));
            InsertMenu(hMenu, 0xFFFFFFFF, cstFont ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING, 2, LoadStr(IDS_USECUSTOMFONT));

            TPMPARAMS tpmPar;
            tpmPar.cbSize = sizeof(tpmPar);
            GetWindowRect((HWND)lParam, &tpmPar.rcExclude);
            DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, tpmPar.rcExclude.right, tpmPar.rcExclude.top,
                                         HWindow, &tpmPar);
            if (cmd == 1)
            {
                // default font
                UseCustomFont = FALSE;
                SetFontText();
            }
            if (cmd == 2)
            {
                // custom font
                CHOOSEFONT cf;
                memset(&cf, 0, sizeof(CHOOSEFONT));
                cf.lStructSize = sizeof(CHOOSEFONT);
                cf.hwndOwner = HWindow;
                cf.lpLogFont = &LogFont;
                cf.iPointSize = 10;
                cf.Flags = CF_NOVERTFONTS | CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
                if (ChooseFont(&cf) != 0)
                {
                    UseCustomFont = TRUE;
                    SetFontText();
                }
            }
            return 0;
        }
        case IDB_CFG_CSVOPTIONS:
        {
            CCSVOptionsDialog(HWindow, &CfgDefaultCSV, NULL).Execute();
            return 0;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CColumnsDialog
//

CColumnsDialog::CColumnsDialog(HWND hParent, CRendererWindow* renderer)
    : CCommonDialog(HLanguage, IDD_COLUMNS, IDD_COLUMNS, hParent), MyColumns(50, 50)
{
    Renderer = renderer;
    HListView = NULL;
    DisableNotification = FALSE;
    // natahneme si sloupce k nam, abychom nad nima mohli konfigurovat
    int i;
    for (i = 0; i < Renderer->Database.GetColumnCount(); i++)
        MyColumns.Add(*Renderer->Database.GetColumn(i));
}

void CColumnsDialog::SetLVTexts(int index)
{
    CDatabaseColumn* column = &MyColumns[index];
    char buff[100];

    // name
    if (!Renderer->Database.GetIsUnicode())
    {
        ListView_SetItemText(HListView, index, 0, column->Name);
    }
    else
    {
        ListView_SetItemTextW(HListView, index, 0, (LPWSTR)column->Name);
    }

    // type
    ListView_SetItemText(HListView, index, 1, column->Type);

    // len
    buff[0] = 0;
    if (column->FieldLen != -1)
        SalGeneral->PrintDiskSize(buff, CQuadWord(column->FieldLen, 0), 2);
    ListView_SetItemText(HListView, index, 2, buff);

    // digits
    buff[0] = 0;
    if (column->Decimals != -1)
        sprintf(buff, "%d", column->Decimals);
    ListView_SetItemText(HListView, index, 3, buff);

    // visibility
    UINT state = INDEXTOSTATEIMAGEMASK((column->Visible ? 2 : 1));
    ListView_SetItemState(HListView, index, state, LVIS_STATEIMAGEMASK);
}

void CColumnsDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        DisableNotification = TRUE;
        int i;
        for (i = 0; i < MyColumns.Count; i++)
        {
            char emptyBuff[] = "";
            LVITEM lvi;
            lvi.mask = LVIF_TEXT | LVIF_STATE;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.state = 0;
            lvi.pszText = emptyBuff;
            ListView_InsertItem(HListView, &lvi);

            SetLVTexts(i);
        }
        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, 0, state, state);
        DisableNotification = FALSE;
    }
    else
    {
        // vratime zmenene sloupce do okna
        int i;
        for (i = 0; i < MyColumns.Count; i++)
            Renderer->Database.SetColumn(i, &MyColumns[i]);
        Renderer->ColumnsWasChanged();
    }
}

void CColumnsDialog::Validate(CTransferInfo& ti)
{
    // alespon jeden sloupec musi zustat zobrazeny
    BOOL visible = FALSE;
    int i;
    for (i = 0; i < MyColumns.Count; i++)
    {
        if (MyColumns[i].Visible)
        {
            visible = TRUE;
            break;
        }
    }
    if (!visible)
    {
        SalGeneral->SalMessageBox(HWindow, LoadStr(IDS_NOVISIBLE_FIELD), LoadStr(IDS_PLUGINNAME),
                                  MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_COL_LV);
    }
}

void CColumnsDialog::OnMove(BOOL up)
{
    DisableNotification = TRUE;
    int index1 = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    int index2 = index1;
    if (index1 == -1)
        return;
    if (up && index1 > 0)
        index2 = index1 - 1;
    if (!up && index1 < MyColumns.Count - 1)
        index2 = index1 + 1;
    if (index2 != index1)
    {
        CDatabaseColumn tmp = MyColumns[index1];
        MyColumns[index1] = MyColumns[index2];
        MyColumns[index2] = tmp;
        SetLVTexts(index1);
        SetLVTexts(index2);
        ListView_SetItemState(HListView, index2, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    }
    DisableNotification = FALSE;
}

void CColumnsDialog::RecalcLayout(DWORD NewDlgW, DWORD NewDlgH)
{
    static int moveIDs[] = {IDOK, IDCANCEL, IDHELP};
    RECT r;
    HWND hControl;
    HDWP hdwp = BeginDeferWindowPos(SizeOf(moveIDs) + 3);

    int i;
    for (i = 0; i < SizeOf(moveIDs); i++)
    {
        hControl = GetDlgItem(HWindow, moveIDs[i]);
        GetWindowRect(hControl, &r);
        ScreenToClient(HWindow, (LPPOINT)&r);
        DeferWindowPos(hdwp, hControl, NULL, r.left + NewDlgW - PrevDlgW, r.top + NewDlgH - PrevDlgH,
                       0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
    hControl = GetDlgItem(HWindow, IDC_COL_LV);
    GetWindowRect(hControl, &r);
    ScreenToClient(HWindow, (LPPOINT)&r);
    ScreenToClient(HWindow, (LPPOINT)&r.right);
    DeferWindowPos(hdwp, hControl, NULL, 0, 0, r.right - r.left + NewDlgW - PrevDlgW,
                   r.bottom - r.top + NewDlgH - PrevDlgH, SWP_NOZORDER | SWP_NOMOVE);

    hControl = GetDlgItem(HWindow, IDC_STATIC_1);
    GetWindowRect(hControl, &r);
    ScreenToClient(HWindow, (LPPOINT)&r);
    ScreenToClient(HWindow, (LPPOINT)&r.right);
    DeferWindowPos(hdwp, hControl, NULL, r.left, r.top + NewDlgH - PrevDlgH, r.right - r.left + NewDlgW - PrevDlgW,
                   r.bottom - r.top, SWP_NOZORDER);

    hControl = GetDlgItem(HWindow, IDC_COL_RESTORE);
    GetWindowRect(hControl, &r);
    ScreenToClient(HWindow, (LPPOINT)&r);
    ScreenToClient(HWindow, (LPPOINT)&r.right);
    DeferWindowPos(hdwp, hControl, NULL, r.left, r.top + NewDlgH - PrevDlgH, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    EndDeferWindowPos(hdwp);
    PrevDlgW = NewDlgW;
    PrevDlgH = NewDlgH;
}

INT_PTR
CColumnsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HListView = GetDlgItem(HWindow, IDC_COL_LV);

        DWORD exFlags = LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // zjistim velikost listview
        RECT r;
        GetClientRect(HListView, &r);
        int colWidth = (int)(r.right / 4);

        // naleju do listview sloupce Name
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.pszText = LoadStr(IDS_COLUMNS_NAME);
        lvc.cx = r.right - 2 * colWidth - (int)(colWidth / 1.4) - GetSystemMetrics(SM_CXVSCROLL);
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        ListView_InsertColumn(HListView, 0, &lvc);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_COLUMNS_TYPE);
        lvc.cx = colWidth;
        lvc.iSubItem = 1;
        ListView_InsertColumn(HListView, 1, &lvc);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_COLUMNS_LENGTH);
        lvc.cx = colWidth;
        lvc.iSubItem = 2;
        ListView_InsertColumn(HListView, 2, &lvc);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_COLUMNS_DECIMALS);
        lvc.cx = (int)(colWidth / 1.4);
        lvc.iSubItem = 3;
        ListView_InsertColumn(HListView, 3, &lvc);
        GetClientRect(HWindow, &r);
        MinDlgW = PrevDlgW = r.right;
        MinDlgH = PrevDlgH = r.bottom;

        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_COL_RESTORE)
        {
            // nahazime originalni hodnoty
            int i;
            for (i = 0; i < MyColumns.Count; i++)
            {
                int j;
                for (j = 0; j < MyColumns.Count; j++)
                {
                    if (Renderer->Database.GetColumn(j)->OriginalIndex == i)
                    {
                        MyColumns[i] = *Renderer->Database.GetColumn(j);
                        MyColumns[i].Visible = TRUE; // na zacatku byly vsechny sloupce viditelne
                        break;
                    }
                }
                // promitneme do LV
                SetLVTexts(i);
            }
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (DisableNotification)
            break;

        if (wParam == IDC_COL_LV)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case NM_DBLCLK:
            {
                LVHITTESTINFO ht;
                DWORD pos = GetMessagePos();
                ht.pt.x = GET_X_LPARAM(pos);
                ht.pt.y = GET_Y_LPARAM(pos);
                ScreenToClient(HListView, &ht.pt);
                ListView_HitTest(HListView, &ht);
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                if (index != -1 && ht.iItem == index && ht.flags != LVHT_ONITEMICON)
                {
                    UINT state = ListView_GetItemState(HListView, index, LVIS_STATEIMAGEMASK);
                    if (state == INDEXTOSTATEIMAGEMASK(2))
                        state = INDEXTOSTATEIMAGEMASK(1);
                    else
                        state = INDEXTOSTATEIMAGEMASK(2);
                    ListView_SetItemState(HListView, index, state, LVIS_STATEIMAGEMASK);
                }
                break;
            }

            case LVN_KEYDOWN:
            {
                LPNMLVKEYDOWN nmhk = (LPNMLVKEYDOWN)nmh;
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                if ((GetKeyState(VK_MENU) & 0x8000) &&
                    (nmhk->wVKey == VK_UP || nmhk->wVKey == VK_DOWN))
                {
                    OnMove(nmhk->wVKey == VK_UP);
                }
                break;
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if ((nmhi->uOldState & 0xF000) != (nmhi->uNewState & 0xF000))
                {
                    // Visibility changed
                    int index = nmhi->iItem;
                    if (index != -1)
                    {
                        DWORD state = ListView_GetItemState(HListView, index, LVIS_STATEIMAGEMASK);
                        MyColumns[index].Visible = state == INDEXTOSTATEIMAGEMASK(2);
                    }
                }
                break;
            }
            }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        break;

    case WM_SIZE:
        RecalcLayout(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;

        if (mmi)
        {
            mmi->ptMinTrackSize.x = MinDlgW;
            mmi->ptMinTrackSize.y = MinDlgH;
            return 0; // processed
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CFindDialog
//

void CFindDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_FIND_REGEXP, Regular);
    HistoryComboBox(HWindow, ti, IDC_FIND_TEXT, Text, SizeOf(Text),
                    FIND_HISTORY_SIZE, FindHistory);
    /*
  if (ti.Type == ttDataToWindow)
  { // inicializace hledaneho textu podle oznaceni ve viewru (parent tohoto dialogu)
    CWindowsObject *win = WindowsManager.GetWindowPtr(Parent);
    if (win != NULL && win->Is(otViewerWindow))  // pro jistotu test je-li to okno viewru
    {
      CViewerWindow *view = (CViewerWindow *)win;
      char buf[FIND_TEXT_LEN];
      char hexBuf[FIND_TEXT_LEN];
      int len;
      if (view->GetFindText(buf, len))
      {
        if (HexMode)
        {
          if (len * 3 > FIND_TEXT_LEN) len = (FIND_TEXT_LEN - 1) / 3;
          int i;
          for (i = 0; i < len; i++)
          {
            sprintf(hexBuf + i * 3, i == len - 1 ? "%02X" : "%02X ", (unsigned)buf[i]);
          }
          strcpy(buf, hexBuf);
        }
        SendMessage(GetDlgItem(HWindow, IDC_FINDTEXT), WM_SETTEXT, 0, (LPARAM)buf);
      }
    }
  }
  */
    ti.RadioButton(IDC_FIND_SBACKWARD, 0, Forward);
    ti.RadioButton(IDC_FIND_SFORWARD, 1, Forward);
    ti.CheckBox(IDC_FIND_WHOLEWORDS, WholeWords);
    ti.CheckBox(IDC_FIND_CASESENSITIVE, CaseSensitive);
}

INT_PTR
CFindDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_USER_CLEARHISTORY:
    {
        char buffer[MAX_PATH];
        HWND cb = GetDlgItem(HWindow, IDC_FIND_TEXT);
        SendMessage(cb, WM_GETTEXT, MAX_PATH, (LPARAM)buffer);
        SendMessage(cb, CB_RESETCONTENT, 0, 0);
        SendMessage(cb, WM_SETTEXT, 0, (LPARAM)buffer);
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
