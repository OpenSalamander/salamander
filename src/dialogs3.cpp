// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "usermenu.h"
#include "execute.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "zip.h"
#include "gui.h"
#include "codetbl.h"
#include "worker.h"
#include "menu.h"

//
// ****************************************************************************
// CChangeCaseDlg
//

CChangeCaseDlg::CChangeCaseDlg(HWND parent, BOOL selectionContainsDirectory)
    : CCommonDialog(HLanguage, IDD_CHANGECASE, IDD_CHANGECASE, parent, ooStandard)
{
    SelectionContainsDirectory = selectionContainsDirectory;
    FileNameFormat = 2;
    Change = 0;
    SubDirs = FALSE;
}

void CChangeCaseDlg::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_CAPITALIZE, 1, FileNameFormat);
    ti.RadioButton(IDC_LOWERCASE, 2, FileNameFormat);
    ti.RadioButton(IDC_UPPERCASE, 3, FileNameFormat);
    ti.RadioButton(IDC_PARTMIXEDCASE, 7, FileNameFormat);

    ti.RadioButton(IDC_WHOLENAME, 0, Change);
    ti.RadioButton(IDC_ONLYNAME, 1, Change);
    ti.RadioButton(IDC_ONLYEXT, 2, Change);

    ti.CheckBox(IDC_RECURSESUBDIRS, SubDirs);
    if (ti.Type == ttDataToWindow)
        EnableWindow(GetDlgItem(HWindow, IDC_RECURSESUBDIRS), SelectionContainsDirectory);
}

//
// ****************************************************************************
// CConvertFilesDlg
//

CConvertFilesDlg::CConvertFilesDlg(HWND parent, BOOL selectionContainsDirectory)
    : CCommonDialog(HLanguage, IDD_CHANGECODING, IDD_CHANGECODING, parent, ooStandard)
{
    CodeTables.Init(HWindow);

    SelectionContainsDirectory = selectionContainsDirectory;
    strcpy(Mask, "*.*");
    Change = 0;
    SubDirs = FALSE;
    CodeType = 0;
    EOFType = 0;
}

void CConvertFilesDlg::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CConvertFilesDlg::Validate()");
    HWND hWnd;
    if (ti.GetControl(hWnd, IDE_FILEMASK))
    {
        if (ti.Type == ttDataFromWindow)
        {
            char buf[MAX_PATH];
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)buf);
            CMaskGroup mask(buf);
            int errorPos;
            if (!mask.PrepareMasks(errorPos))
            {
                SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                SetFocus(hWnd);
                SendMessage(hWnd, CB_SETEDITSEL, 0, MAKELPARAM(errorPos, errorPos + 1));
                ti.ErrorOn(IDE_FILEMASK);
            }
        }
    }

    if (ti.IsGood())
    {
        int noneEOF = IsDlgButtonChecked(HWindow, IDC_CHC_EOFNONE) == BST_CHECKED;
        if (noneEOF && CodeType == 0)
        {
            SalMessageBox(HWindow, LoadStr(IDS_SPECIFY_CONVERT_ACTION), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_CHC_CHANGECODING);
        }
    }
}

void CConvertFilesDlg::Transfer(CTransferInfo& ti)
{
    char** history = Configuration.ConvertHistory;
    HWND hWnd;
    if (ti.GetControl(hWnd, IDE_FILEMASK))
    {
        if (ti.Type == ttDataToWindow)
        {
            LoadComboFromStdHistoryValues(hWnd, history, CONVERT_HISTORY_SIZE);
            SendMessage(hWnd, CB_LIMITTEXT, MAX_PATH - 1, 0);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Mask);
        }
        else
        {
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)Mask);
            AddValueToStdHistoryValues(history, CONVERT_HISTORY_SIZE, Mask, FALSE);
        }
    }

    ti.RadioButton(IDC_CHC_EOFNONE, 0, EOFType);
    ti.RadioButton(IDC_CHC_EOFCRLF, 1, EOFType);
    ti.RadioButton(IDC_CHC_EOFLF, 2, EOFType);
    ti.RadioButton(IDC_CHC_EOFCR, 3, EOFType);

    ti.CheckBox(IDC_RECURSESUBDIRS, SubDirs);

    if (ti.Type == ttDataToWindow)
        EnableWindow(GetDlgItem(HWindow, IDC_RECURSESUBDIRS), SelectionContainsDirectory);
}

void CConvertFilesDlg::UpdateCodingText()
{
    char buff[1024];
    CodeTables.GetCodeName(CodeType, buff, 1024);

    // dig &
    RemoveAmpersands(buff);

    SetDlgItemText(HWindow, IDC_CHC_CODING, buff);
}
/*  int CEOFTypes[4] =
{
  IDS_EOF_NONE,
  IDS_EOF_CRLF,
  IDS_EOF_LF,
  IDS_EOF_CR
};

void
CConvertFilesDlg::UpdateEOFText()
{
  char *p = LoadStr(CEOFTypes[EOFType]);
  // remove &
  RemoveAmpersands(p);

  SetDlgItemText(HWindow, IDC_CHC_EOF, p);
}*/

INT_PTR
CConvertFilesDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        new CButton(HWindow, IDC_CHC_CHANGECODING, BTF_RIGHTARROW);
        UpdateCodingText();
        //      UpdateEOFText();

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_CHC_CHANGECODING)
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, IDC_CHC_CHANGECODING), &r);
            HMENU hMenu = CreatePopupMenu();
            CodeTables.InitMenu(hMenu, CodeType);
            TPMPARAMS tpmPar;
            tpmPar.cbSize = sizeof(tpmPar);
            tpmPar.rcExclude = r;
            DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, r.right, r.top, HWindow, &tpmPar);
            if (cmd != 0)
            {
                CodeType = cmd - CM_CODING_MIN;
                UpdateCodingText();
            }
            DestroyMenu(hMenu);
            return 0;
        }
        /*        if (LOWORD(wParam) == IDC_CHC_CHANGEEOF)
      {
        RECT r;
        GetWindowRect(GetDlgItem(HWindow, IDC_CHC_CHANGEEOF), &r);
        POINT p;
        p.x = r.right;
        p.y = r.top;
        HMENU hMenu = CreatePopupMenu();

        MENUITEMINFO mi;
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);

        int i;
        for (i = 0; i < 4; i++)
        {
          char *p = LoadStr(CEOFTypes[i]);
          mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
          mi.fType = MFT_STRING;
          mi.wID = i + 1;                   // +1 kvuli 'None'
          mi.dwTypeData = p;
          mi.cch = strlen(p);
          mi.fState = (i == EOFType ? MFS_CHECKED : MFS_UNCHECKED);
          InsertMenuItem(hMenu, i, TRUE, &mi);
          SetMenuItemBitmaps(hMenu, mi.wID, MF_BYCOMMAND, NULL, HMenuCheckDot);
        }

        // za none vlozim separator
        mi.fMask = MIIM_TYPE;
        mi.fType = MFT_SEPARATOR;
        InsertMenuItem(hMenu, 1, TRUE, &mi);

        DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN |
                                     TPM_RIGHTBUTTON, p.x, p.y,
                                     HWindow, NULL);
        if (cmd != 0)
        {
          EOFType = cmd - 1;
          UpdateEOFText();
        }
        DestroyMenu(hMenu);
        return 0;

      }
*/
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CFilterDialog
//

CFilterDialog::CFilterDialog(HWND parent, CMaskGroup* filter, char** filterHistory,
                             BOOL* use /*, BOOL *inverse*/)
    : CCommonDialog(HLanguage, IDD_CHANGEFILTER, IDD_CHANGEFILTER, parent)
{
    Filter = filter;
    UseFilter = use;
    //  Inverse = inverse;
    FilterHistory = filterHistory;
}

void CFilterDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFilterDialog::Validate()");
    BOOL useFilter;
    ti.RadioButton(IDC_DONTUSEFILTER, FALSE, useFilter);
    ti.RadioButton(IDC_USEFILTER, TRUE, useFilter);
    if (useFilter)
    {
        char buf[MAX_PATH];
        lstrcpyn(buf, Filter->GetMasksString(), MAX_PATH); // backup
        // we provide a buffer for MasksString, there is a size check, nothing to worry about
        ti.EditLine(IDE_FILTER, Filter->GetWritableMasksString(), MAX_PATH);
        int errorPos;
        if (!Filter->PrepareMasks(errorPos))
        {
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(HWindow, IDE_FILTER));
            SendMessage(GetDlgItem(HWindow, IDE_FILTER), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(IDE_FILTER);
        }
        Filter->SetMasksString(buf); // recovery
    }
}

void CFilterDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CFilterDialog::Transfer()");
    ti.RadioButton(IDC_DONTUSEFILTER, FALSE, *UseFilter);
    ti.RadioButton(IDC_USEFILTER, TRUE, *UseFilter);
    //  ti.CheckBox(IDC_INVERSEFILTER, *Inverse);

    if (ti.Type == ttDataToWindow)
        EnableControls();
    /*    ti.EditLine(IDE_FILTER, Filter->MasksString, MAX_PATH);
  int errorPos;
  Filter->PrepareMasks(errorPos);*/
    char** history = FilterHistory;
    HWND hWnd;
    if (ti.GetControl(hWnd, IDE_FILTER))
    {
        if (ti.Type == ttDataToWindow)
        {
            LoadComboFromStdHistoryValues(hWnd, history, FILTER_HISTORY_SIZE);
            SendMessage(hWnd, CB_LIMITTEXT, MAX_PATH - 1, 0);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Filter->GetMasksString());
        }
        else
        {
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)Filter->GetWritableMasksString());
            AddValueToStdHistoryValues(history, FILTER_HISTORY_SIZE, Filter->GetMasksString(), FALSE);
        }
    }
    int errorPos;
    Filter->PrepareMasks(errorPos);
}

void CFilterDialog::EnableControls()
{
    //  BOOL filter = IsDlgButtonChecked(HWindow, IDC_USEFILTER) == BST_CHECKED;
    //  EnableWindow(GetDlgItem(HWindow, IDC_INVERSEFILTER), filter);
    //  if (!filter)
    //    CheckDlgButton(HWindow, IDC_INVERSEFILTER, BST_UNCHECKED);
}

INT_PTR
CFilterDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_FILTER)); // Installing WordBreakProc into the combobox

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        if (*UseFilter)
        { // we want our own focus in the filter editbox
            CCommonDialog::DialogProc(uMsg, wParam, lParam);
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_FILTER), TRUE);
            return FALSE;
        }

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_EDITCHANGE || HIWORD(wParam) == CBN_SELCHANGE)
        {
            if (IsDlgButtonChecked(HWindow, IDC_DONTUSEFILTER))
            {
                CheckDlgButton(HWindow, IDC_USEFILTER, BST_CHECKED);
                CheckDlgButton(HWindow, IDC_DONTUSEFILTER, BST_UNCHECKED);
                EnableControls();
            }
        }
        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_DONTUSEFILTER || LOWORD(wParam) == IDC_USEFILTER))
        {
            EnableControls();
            if (LOWORD(wParam) == IDC_USEFILTER)
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_FILTER), TRUE);
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCopyMoveDialog
//

CCopyMoveDialog::CCopyMoveDialog(HWND parent, char* path, int pathBufSize, char* title,
                                 CTruncatedString* subject, DWORD helpID,
                                 char* history[], int historyCount, BOOL directoryHelper)
    : CCommonDialog(HLanguage, history ? IDD_COPYMOVEDIALOG_CB : IDD_COPYMOVEDIALOG, parent)
{
    DirectoryHelper = FALSE;
    if (directoryHelper)
    {
        if (history != NULL)
        {
            ResID = IDD_COPYMOVEDIALOG_CB_BT;
            DirectoryHelper = TRUE;
        }
        else
            TRACE_E("CCopyMoveDialog without history and with directoryHelper is not supported.");
    }
    Title = title;
    Subject = subject;
    Path = path;
    PathBufSize = pathBufSize;
    History = history;
    HistoryCount = historyCount;
    SetHelpID(helpID); // dialog is used for multiple purposes - let's set the correct helpID
    SelectionEnd = -1; // -1 = select all
}

void CCopyMoveDialog::SetSelectionEnd(int selectionEnd)
{
    SelectionEnd = selectionEnd;
}

void CCopyMoveDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCopyMoveDialog::Transfer()");
    if (History != NULL)
    {
        HWND hWnd;
        if (ti.GetControl(hWnd, IDE_PATH))
        {
            if (ti.Type == ttDataToWindow)
            {
                LoadComboFromStdHistoryValues(hWnd, History, HistoryCount);
                SendMessage(hWnd, CB_LIMITTEXT, PathBufSize - 1, 0);
                SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Path);
            }
            else
            {
                SendMessage(hWnd, WM_GETTEXT, PathBufSize, (LPARAM)Path);
                AddValueToStdHistoryValues(History, HistoryCount, Path, FALSE);
            }
        }
    }
    else
    {
        ti.EditLine(IDE_PATH, Path, PathBufSize);
    }
}

INT_PTR
CCopyMoveDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_PATH)); // Installing WordBreakProc into the combobox

        CreateKeyForwarder(HWindow, IDE_PATH); // to receive WM_USER_KEYDOWN
        if (DirectoryHelper)
        {
            ChangeToIconButton(HWindow, IDB_BROWSE, IDI_DIRECTORY);   // the button will have a folder icon and an arrow pointing to the right
            VerticalAlignChildToChild(HWindow, IDB_BROWSE, IDE_PATH); // place the button right behind the editline
        }

        SetWindowText(HWindow, Title);
        HWND hSubject = GetDlgItem(HWindow, IDS_SUBJECT);
        if (Subject->TruncateText(hSubject))
            SetWindowText(hSubject, Subject->Get());

        INT_PTR ret = CCommonDialog::DialogProc(uMsg, wParam, lParam);
        // we can only select the name without the dot and extension
        PostMessage(GetDlgItem(HWindow, IDE_PATH), CB_SETEDITSEL, 0,
                    MAKELPARAM(0, SelectionEnd));
        return FALSE;
    }

    case WM_USER_KEYDOWN:
    {
        BOOL processed = FALSE;
        if (DirectoryHelper)
            processed = OnDirectoryKeyDown((DWORD)lParam, HWindow, IDE_PATH, PathBufSize, IDB_BROWSE);
        if (!processed)
            processed = OnKeyDownHandleSelectAll((DWORD)lParam, HWindow, IDE_PATH);
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, processed);
        return processed;
    }

    case WM_USER_BUTTON:
    {
        OnDirectoryButton(HWindow, IDE_PATH, PathBufSize, IDB_BROWSE, wParam, lParam);
        return 0;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CEditNewFileDialog
//

CEditNewFileDialog::CEditNewFileDialog(HWND parent, char* path, int pathBufSize, CTruncatedString* subject, char* history[], int historyCount)
    : CCopyMoveDialog(parent, path, pathBufSize, LoadStr(IDS_EDITNEWFILE), subject, IDD_EDITNEWDIALOG, history, historyCount, FALSE)
{
    ResID = IDD_COPYMOVEDIALOG_CB_BTSML;
}

INT_PTR
CEditNewFileDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        ChangeToArrowButton(HWindow, IDB_BROWSE);
        VerticalAlignChildToChild(HWindow, IDB_BROWSE, IDE_PATH); // place the button right behind the editline
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_BROWSE)
        {
            /* used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() call below...
MENU_TEMPLATE_ITEM EditNewFileDialogMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_EDITNEWFILE_SAVEASDEFAULT
  {MNTT_IT, IDS_EDITNEWFILE_REVERTDEFAULT
  {MNTT_PE, 0
};*/
            HMENU hMenu = CreatePopupMenu();
            InsertMenu(hMenu, 0xFFFFFFFF, MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_EDITNEWFILE_SAVEASDEFAULT));
            char buff[2 * MAX_PATH];
            wsprintf(buff, LoadStr(IDS_EDITNEWFILE_REVERTDEFAULT), LoadStr(IDS_EDITNEWFILE_DEFAULTNAME));
            InsertMenu(hMenu, 0xFFFFFFFF, MF_BYCOMMAND | MF_STRING, 2, buff);

            TPMPARAMS tpmPar;
            tpmPar.cbSize = sizeof(tpmPar);
            GetWindowRect((HWND)lParam, &tpmPar.rcExclude);
            DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, tpmPar.rcExclude.right, tpmPar.rcExclude.top,
                                         HWindow, &tpmPar);
            if (cmd == 1)
            {
                Configuration.UseEditNewFileDefault = TRUE;
                SendDlgItemMessage(HWindow, IDE_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)Configuration.EditNewFileDefault);
            }
            if (cmd == 2)
            {
                Configuration.UseEditNewFileDefault = FALSE;
                Configuration.EditNewFileDefault[0] = 0;
            }
            return 0;
        }
        break;
    }
    }
    return CCopyMoveDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCopyMoveMoreDialog
//

CCopyMoveMoreDialog::CCopyMoveMoreDialog(HWND parent, char* path, int pathBufSize, char* title,
                                         CTruncatedString* subject, DWORD helpID,
                                         char* history[], int historyCount, CCriteriaData* criteriaInOut,
                                         BOOL havePermissions, BOOL supportsADS)
    : CCommonDialog(HLanguage, IDD_COPYMOVEMOREDIALOG, helpID, parent)
{
    if (history == NULL)
        TRACE_E("CCopyMoveMoreDialog without history is not supported.");

    Title = title;
    Subject = subject;
    Path = path;
    PathBufSize = pathBufSize;
    History = history;
    HistoryCount = historyCount;
    CriteriaInOut = criteriaInOut;
    Criteria = new CCriteriaData();
    *Criteria = *CriteriaInOut;
    Expanded = TRUE;
    HavePermissions = havePermissions;
    SupportsADS = supportsADS;
    MoreButton = NULL;
}

CCopyMoveMoreDialog::~CCopyMoveMoreDialog()
{
    if (Criteria != NULL)
    {
        delete Criteria;
        Criteria = NULL;
    }
}

void CCopyMoveMoreDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCopyMoveMoreDialog::Transfer()");
    if (History != NULL)
    {
        HWND hWnd;
        if (ti.GetControl(hWnd, IDE_PATH))
        {
            if (ti.Type == ttDataToWindow)
            {
                LoadComboFromStdHistoryValues(hWnd, History, HistoryCount);
                SendMessage(hWnd, CB_LIMITTEXT, PathBufSize - 1, 0);
                SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Path);
            }
            else
            {
                SendMessage(hWnd, WM_GETTEXT, PathBufSize, (LPARAM)Path);
                AddValueToStdHistoryValues(History, HistoryCount, Path, FALSE);
            }
        }
    }
    else
    {
        ti.EditLine(IDE_PATH, Path, PathBufSize);
    }
    TransferCriteriaControls(ti);
}

BOOL GetSpeedLimit(int sel, char* speedLimitText, DWORD* returnSpeedLimit)
{
    if (sel >= 0 && sel <= 3)
    {
        __int64 speedLimit = 0;
        char* s = speedLimitText;
        while (*s != 0 && *s <= ' ')
            s++;
        while (*s >= '0' && *s <= '9')
        {
            speedLimit = 10 * speedLimit + (*s - '0');
            if (speedLimit - 1 > 0xFFFFFFFF)
                break;
            s++;
        }
        while (*s != 0 && *s <= ' ')
            s++;
        if (*s == 0)
        {
            switch (sel)
            {
            case 1:
                speedLimit *= 1024;
                break;
            case 2:
                speedLimit *= 1024 * 1024;
                break;
            case 3:
                speedLimit *= 1024 * 1024 * 1024;
                break;
            }
            if (speedLimit - 1 == 0xFFFFFFFF)
                speedLimit--; // We consider 4GB as 0xFFFFFFFF, otherwise we will not store that number
            if (speedLimit > 0 && speedLimit <= 0xFFFFFFFF)
            {
                if (returnSpeedLimit != NULL)
                    *returnSpeedLimit = (DWORD)speedLimit;
                return TRUE;
            }
        }
    }
    return FALSE;
}

void CCopyMoveMoreDialog::TransferCriteriaControls(CTransferInfo& ti)
{
    ti.CheckBox(IDC_CM_NEWER, Criteria->OverwriteOlder);
    ti.CheckBox(IDC_CM_STARTONIDLE, Criteria->StartOnIdle);
    ti.CheckBox(IDC_CM_SECURITY, Criteria->CopySecurity);
    ti.CheckBox(IDC_CM_COPYATTRS, Criteria->CopyAttrs);
    ti.CheckBox(IDC_CM_DIRTIME, Criteria->PreserveDirTime);
    ti.CheckBox(IDC_CM_IGNADS, Criteria->IgnoreADS);
    ti.CheckBox(IDC_CM_EMPTY, Criteria->SkipEmptyDirs);
    ti.CheckBox(IDC_CM_NAMED, Criteria->UseMasks);
    ti.CheckBox(IDC_CM_SPEEDLIMIT, Criteria->UseSpeedLimit);
    char masks[MAX_PATH];
    if (ti.Type == ttDataToWindow)
        strcpy(masks, Criteria->Masks.GetMasksString());
    ti.EditLine(IDC_CM_NAMED_MASK, masks, MAX_PATH - 1);
    if (ti.Type == ttDataFromWindow)
    {
        Criteria->Masks.SetMasksString(masks);
        int errpos = 0;
        // masks must go out in Prepared state
        if (!Criteria->Masks.PrepareMasks(errpos)) // incorrect mask, this should not occur thanks to validation
            Criteria->UseMasks = FALSE;
        char dummy[200];
        Criteria->Advanced.GetAdvancedDescription(dummy, 200, Criteria->UseAdvanced);
        // Advance must also be prepared
        Criteria->Advanced.PrepareForTest();

        if (Criteria->UseSpeedLimit)
        {
            int sel = (int)SendDlgItemMessage(HWindow, IDC_CM_SPEEDLIMITUNITS, CB_GETCURSEL, 0, 0);
            char speedLimitText[20];
            GetDlgItemText(HWindow, IDE_CM_SPEEDLIMIT, speedLimitText, 20);
            if (GetSpeedLimit(sel, speedLimitText, &Criteria->SpeedLimit))
                Configuration.LastUsedSpeedLimit = Criteria->SpeedLimit;
            else
                Criteria->UseSpeedLimit = FALSE;
        }
    }
    if (ti.Type == ttDataToWindow)
    {
        DWORD speedLimNum = Configuration.LastUsedSpeedLimit;
        if (Criteria->UseSpeedLimit)
            speedLimNum = Criteria->SpeedLimit;
        int speedLimUnits = 0;
        if (speedLimNum == 0xFFFFFFFF)
        {
            speedLimNum = 4;
            speedLimUnits = 3;
        }
        else
        {
            while (speedLimNum % 1024 == 0)
            {
                speedLimNum /= 1024;
                speedLimUnits++;
                if (speedLimNum == 0 || speedLimUnits > 3) // cannot happen, just for peace of mind
                {
                    TRACE_E("CCopyMoveMoreDialog::TransferCriteriaControls(): unexpected situation!");
                    speedLimNum = 4;
                    speedLimUnits = 3;
                    break;
                }
            }
        }

        HWND speedLimitUnits = GetDlgItem(HWindow, IDC_CM_SPEEDLIMITUNITS);
        SendMessage(speedLimitUnits, CB_RESETCONTENT, 0, 0);
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_B_per_s));
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_KB_per_s));
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_MB_per_s));
        SendMessage(speedLimitUnits, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SPEED_GB_per_s));
        SendMessage(speedLimitUnits, CB_SETCURSEL, speedLimUnits, 0);

        HWND speedLimit = GetDlgItem(HWindow, IDE_CM_SPEEDLIMIT);
        char num[20];
        sprintf(num, "%u", speedLimNum);
        SetWindowText(speedLimit, num);
        SendMessage(speedLimit, EM_LIMITTEXT, 19, 0);

        UpdateAdvancedText();
        EnableControls();
    }
}

void CCopyMoveMoreDialog::UpdateAdvancedText()
{
    char buff[200];
    BOOL dirty;
    Criteria->Advanced.GetAdvancedDescription(buff, 200, dirty);
    SetDlgItemText(HWindow, IDC_CM_ADVANCED_INFO, buff);
    EnableWindow(GetDlgItem(HWindow, IDC_CM_ADVANCED_INFO), dirty);
}

void CCopyMoveMoreDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCopyMoveMoreDialog::Validate()");

    BOOL useSpeedLimit;
    ti.CheckBox(IDC_CM_SPEEDLIMIT, useSpeedLimit);
    if (useSpeedLimit)
    {
        int sel = (int)SendDlgItemMessage(HWindow, IDC_CM_SPEEDLIMITUNITS, CB_GETCURSEL, 0, 0);
        char speedLimitText[20];
        GetDlgItemText(HWindow, IDE_CM_SPEEDLIMIT, speedLimitText, 20);
        if (!GetSpeedLimit(sel, speedLimitText, NULL))
        {
            SalMessageBox(HWindow, LoadStr(IDS_SPEEDLIMITSIZE), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_CM_SPEEDLIMIT);
            return;
        }
    }

    BOOL useMasks;
    ti.CheckBox(IDC_CM_NAMED, useMasks);
    if (useMasks)
    {
        char buf[MAX_PATH];
        ti.EditLine(IDC_CM_NAMED_MASK, buf, MAX_PATH - 1);
        CMaskGroup masks(buf);
        int errorPos;
        if (!masks.PrepareMasks(errorPos))
        {
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(HWindow, IDC_CM_NAMED_MASK));
            SendMessage(GetDlgItem(HWindow, IDC_CM_NAMED_MASK), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(IDC_CM_NAMED_MASK);
        }
    }
}

HDWP CCopyMoveMoreDialog::OffsetControl(HDWP hdwp, int id, int yOffset)
{
    HWND hCtrl = GetDlgItem(HWindow, id);
    RECT r;
    GetWindowRect(hCtrl, &r);
    ScreenToClient(HWindow, (LPPOINT)&r);

    hdwp = HANDLES(DeferWindowPos(hdwp, hCtrl, NULL, r.left, r.top + yOffset, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
    return hdwp;
}

void CCopyMoveMoreDialog::SetOptionsButtonState(BOOL more)
{
    CheckDlgButton(HWindow, IDC_MORE, more ? BST_CHECKED : BST_UNCHECKED);

    // if the button is pressed (options expanded), it is not MORE, but it is DROPDOWN (and vice versa)
    DWORD btnFlags = MoreButton->GetFlags();
    if (more)
    {
        btnFlags &= ~BTF_MORE;
        btnFlags |= BTF_DROPDOWN;
    }
    else
    {
        btnFlags |= BTF_MORE;
        btnFlags &= ~BTF_DROPDOWN;
    }
    MoreButton->SetFlags(btnFlags, TRUE);
}

void CCopyMoveMoreDialog::DisplayMore(BOOL more, BOOL fast)
{
    // Hidden controls must be hidden in order to exclude them from the tab order
    int controls[] = {IDC_CM_NEWER, IDC_CM_STARTONIDLE, IDC_CM_SPEEDLIMIT, IDE_CM_SPEEDLIMIT,
                      IDC_CM_SPEEDLIMITUNITS, IDC_CM_SECURITY, IDC_CM_COPYATTRS,
                      IDC_CM_DIRTIME, IDC_CM_IGNADS, IDC_CM_EMPTY, IDC_CM_NAMED_MASK, IDC_CM_NAMED,
                      IDC_FILEMASK_HINT, IDC_CM_ADVANCED, IDC_CM_ADVANCED_INFO,
                      IDC_CM_SEPARATOR, -1};

    int wndHeight = OriginalHeight;
    if (!more)
        wndHeight -= SpacerHeight;
    SetWindowPos(HWindow, NULL, 0, 0, OriginalWidth, wndHeight,
                 SWP_NOZORDER | SWP_NOMOVE);

    HWND hFocus = GetFocus();
    int i;
    for (i = 0; controls[i] != -1; i++)
    {
        HWND hCtrl = GetDlgItem(HWindow, controls[i]);
        if (!more && hCtrl == hFocus)
        {
            SendMessage(HWindow, DM_SETDEFID, IDOK, 0);
            SetFocus(GetDlgItem(HWindow, IDE_PATH));
        }
        ShowWindow(hCtrl, more ? SW_SHOW : SW_HIDE);
    }

    int yOffset = more ? SpacerHeight : -SpacerHeight;

    HDWP hdwp = HANDLES(BeginDeferWindowPos(4));
    if (hdwp != NULL)
    {
        hdwp = OffsetControl(hdwp, IDOK, yOffset);
        hdwp = OffsetControl(hdwp, IDCANCEL, yOffset);
        hdwp = OffsetControl(hdwp, IDC_MORE, yOffset);
        hdwp = OffsetControl(hdwp, IDHELP, yOffset);
        HANDLES(EndDeferWindowPos(hdwp));
    }

    SetOptionsButtonState(more);

    if (!more && !fast) // fast is TRUE if the controls contain default values and there is no need to reset them
    {
        Criteria->Reset();
        CTransferInfo ti(HWindow, ttDataToWindow);
        TransferCriteriaControls(ti);
    }
    Expanded = more;
}

void CCopyMoveMoreDialog::EnableControls()
{
    BOOL named = IsDlgButtonChecked(HWindow, IDC_CM_NAMED);
    EnableWindow(GetDlgItem(HWindow, IDC_CM_NAMED_MASK), named);
    BOOL speedLimit = IsDlgButtonChecked(HWindow, IDC_CM_SPEEDLIMIT);
    EnableWindow(GetDlgItem(HWindow, IDE_CM_SPEEDLIMIT), speedLimit);
    EnableWindow(GetDlgItem(HWindow, IDC_CM_SPEEDLIMITUNITS), speedLimit);
}

/*  BOOL
CCopyMoveMoreDialog::ManageHiddenShortcuts(const MSG *msg)
{
  if (msg->message == WM_SYSKEYDOWN)
  {
    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (!controlPressed && altPressed && !shiftPressed)
    {
      // if Alt+? is pressed and the Options section is collapsed, it makes sense to investigate further
      if (!IsDlgButtonChecked(HWindow, IDC_FIND_GREP))
      {
        // check the hotkeys of monitored elements
        int resID[] = {IDC_FIND_CONTAINING_TEXT, IDC_FIND_HEX, IDC_FIND_CASE,
                       IDC_FIND_WHOLE, IDC_FIND_REGULAR, -1}; // (terminate -1)
        int i;
        for (i = 0; resID[i] != -1; i++)
        {
          char key = GetControlHotKey(HWindow, resID[i]);
          if (key != 0 && (WPARAM)key == msg->wParam)
          {
            // expand the Options section
            CheckDlgButton(HWindow, IDC_FIND_GREP, BST_CHECKED);
            SendMessage(HWindow, WM_COMMAND, MAKEWPARAM(IDC_FIND_GREP, BN_CLICKED), 0);
            return FALSE; // expanded, IsDialogMessage will take care of the rest upon our return
          }
        }
      }
    }
  }
  return FALSE; // not our message
}*/

INT_PTR
CCopyMoveMoreDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_PATH)); // Installing WordBreakProc into the combobox

        // Starting from version 2.53, we can save options, so IDC_CM_STARTONIDLE must always be enabled for the user to pre-set the choice
        // EnableWindow(GetDlgItem(HWindow, IDC_CM_STARTONIDLE), !OperationsQueue.IsEmpty());
        EnableWindow(GetDlgItem(HWindow, IDC_CM_SECURITY), HavePermissions);
        EnableWindow(GetDlgItem(HWindow, IDC_CM_IGNADS), SupportsADS);

        MoreButton = new CButton(HWindow, IDC_MORE, BTF_MORE | BTF_CHECKBOX);
        SetOptionsButtonState(TRUE);

        CreateKeyForwarder(HWindow, IDE_PATH);                  // to receive WM_USER_KEYDOWN
        ChangeToIconButton(HWindow, IDB_BROWSE, IDI_DIRECTORY); // the button will have a folder icon and an arrow pointing to the right

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        SetWindowText(HWindow, Title);
        HWND hSubject = GetDlgItem(HWindow, IDS_SUBJECT);
        if (Subject->TruncateText(hSubject))
        {
            char buff[MAX_PATH];
            lstrcpyn(buff, Subject->Get(), MAX_PATH - 1);
            DuplicateAmpersands(buff, MAX_PATH - 1, TRUE);
            SetWindowText(hSubject, buff);
        }

        // now we are in full size => we will measure the dialog
        RECT r;
        GetWindowRect(HWindow, &r);
        OriginalWidth = r.right - r.left;
        OriginalHeight = r.bottom - r.top;
        // original position of buttons
        GetWindowRect(GetDlgItem(HWindow, IDOK), &r);
        ScreenToClient(HWindow, (LPPOINT)&r);
        OriginalButtonsY = r.top;
        // height of the delimiter
        GetWindowRect(GetDlgItem(HWindow, IDC_CM_SPACER), &r);
        SpacerHeight = r.bottom - r.top;

        if (!Criteria->IsDirty()) // Wrap the dialog in case Criteria does not contain any data
            DisplayMore(FALSE, TRUE);
        break;
    }

    case WM_USER_KEYDOWN:
    {
        BOOL processed = OnDirectoryKeyDown((DWORD)lParam, HWindow, IDE_PATH, PathBufSize, IDB_BROWSE);
        if (!processed)
            processed = OnKeyDownHandleSelectAll((DWORD)lParam, HWindow, IDE_PATH);
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, processed);
        return processed;
    }

    case WM_USER_BUTTON:
    {
        OnDirectoryButton(HWindow, IDE_PATH, PathBufSize, IDB_BROWSE, wParam, lParam);
        return 0;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDC_MORE)
        {
            HWND hCtrl = GetDlgItem(HWindow, (int)wParam);
            RECT r;
            GetWindowRect(hCtrl, &r);

            CMenuPopup* popup = new CMenuPopup;
            if (popup != NULL)
            {
                /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertItem() dole...
MENU_TEMPLATE_ITEM CopyMoveMoreDialogMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_COPYMOVE_RESETHIDE
  {MNTT_IT, IDS_COPYMOVE_SAVEASDEF
  {MNTT_IT, IDS_COPYMOVE_RESETDEFS
  {MNTT_PE, 0
};
*/
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
                mii.Type = MENU_TYPE_STRING;

                mii.String = LoadStr(IDS_COPYMOVE_RESETHIDE);
                mii.ID = 1;
                popup->InsertItem(-1, TRUE, &mii);

                mii.String = LoadStr(IDS_COPYMOVE_SAVEASDEF);
                mii.ID = 2;
                popup->InsertItem(-1, TRUE, &mii);

                mii.String = LoadStr(IDS_COPYMOVE_RESETDEFS);
                mii.ID = 3;
                popup->InsertItem(-1, TRUE, &mii);

                BOOL selectMenuItem = LOWORD(lParam);
                DWORD flags = MENU_TRACK_RETURNCMD;
                if (selectMenuItem)
                {
                    popup->SetSelectedItemIndex(0);
                    flags |= MENU_TRACK_SELECT;
                }
                switch (popup->Track(flags, r.left, r.bottom, HWindow, &r))
                {
                case 1: // Reset and hide options
                {
                    PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDC_MORE, BN_CLICKED), 0);
                    break;
                }

                case 2: // Save options as defaults
                {
                    // to save options, validation must be passed
                    if (ValidateData())
                    {
                        if (TransferData(ttDataFromWindow))
                            CopyMoveOptions.Set(Criteria->IsDirty() ? Criteria : NULL); // save new default
                    }
                    break;
                }

                case 3: // Reset options and defaults
                {
                    CopyMoveOptions.Set(NULL);

                    // clean up the dialog
                    Criteria->Reset();
                    TransferData(ttDataToWindow);
                    break;
                }
                }
                delete popup;
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_CM_STARTONIDLE:
            case IDC_CM_NEWER:
            case IDC_CM_SPEEDLIMIT:
            case IDC_CM_COPYATTRS:
            case IDC_CM_SECURITY:
            case IDC_CM_DIRTIME:
            case IDC_CM_IGNADS:
            case IDC_CM_EMPTY:
            case IDC_CM_NAMED:
            case IDC_CM_ADVANCED:
            {
                if (!Expanded)
                    DisplayMore(TRUE, FALSE);
                break;
            }
            }

            EnableControls();

            // If the user clicks on the checkbox to enable the mask, they probably want to edit it
            if (LOWORD(wParam) == IDC_CM_NAMED)
            {
                if (IsDlgButtonChecked(HWindow, IDC_CM_NAMED))
                    SendMessage(HWindow, WM_NEXTDLGCTL, FALSE, FALSE); // focus on the mask
                else
                    SetDlgItemText(HWindow, IDC_CM_NAMED_MASK, "*.*"); // default value into the mask
            }

            // if the user clicks on the speed limit checkbox, they probably want to edit it
            if (LOWORD(wParam) == IDC_CM_SPEEDLIMIT)
            {
                if (IsDlgButtonChecked(HWindow, IDC_CM_SPEEDLIMIT))
                    SendMessage(HWindow, WM_NEXTDLGCTL, FALSE, FALSE); // focus to editbox
            }

            if (LOWORD(wParam) == IDC_MORE)
            {
                DisplayMore(!Expanded, FALSE);
                return 0;
            }

            if (LOWORD(wParam) == IDC_CM_ADVANCED)
            {
                CFilterCriteriaDialog dlg(HWindow, &Criteria->Advanced, FALSE);
                if (dlg.Execute() == IDOK)
                    UpdateAdvancedText();
                return 0;
            }

            if (LOWORD(wParam) == IDOK)
            {
                // Custom OK handling -- I need to propagate Criteria outside
                if (!ValidateData() ||
                    !TransferData(ttDataFromWindow))
                    return TRUE;
                *CriteriaInOut = *Criteria;
                EndDialog(HWindow, wParam);
                return TRUE;
            }
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CChangeDirDlg
//

CChangeDirDlg::CChangeDirDlg(HWND parent, char* path, BOOL* sendDirectlyToPlugin) : CCommonDialog(HLanguage, IDD_CHANGEDIR, IDD_CHANGEDIR, parent)
{
    Path = path;
    SendDirectlyToPlugin = sendDirectlyToPlugin;
}

void CChangeDirDlg::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CChangeDirDlg::Transfer()");
    char** history = Configuration.ChangeDirHistory;
    HWND hWnd;
    if (ti.GetControl(hWnd, IDE_PATH))
    {
        if (ti.Type == ttDataToWindow)
        {
            LoadComboFromStdHistoryValues(hWnd, history, CHANGEDIR_HISTORY_SIZE);
            SendMessage(hWnd, CB_LIMITTEXT, 2 * MAX_PATH - 1, 0);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Path);
        }
        else
        {
            SendMessage(hWnd, WM_GETTEXT, 2 * MAX_PATH, (LPARAM)Path);
            AddValueToStdHistoryValues(history, CHANGEDIR_HISTORY_SIZE, Path, FALSE);
        }
    }
    if (SendDirectlyToPlugin != NULL)
        ti.CheckBox(IDC_SENDDIRECTTOPLG, *SendDirectlyToPlugin);
}

INT_PTR
CChangeDirDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeDirDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (SendDirectlyToPlugin == NULL)
            EnableWindow(GetDlgItem(HWindow, IDC_SENDDIRECTTOPLG), FALSE);
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_PATH));    // Installing WordBreakProc into the combobox
        CreateKeyForwarder(HWindow, IDE_PATH);                  // to receive WM_USER_KEYDOWN
        ChangeToIconButton(HWindow, IDB_BROWSE, IDI_DIRECTORY); // the button will have a folder icon and an arrow pointing to the right

        CHyperLink* hl = new CHyperLink(HWindow, IDC_CHANGEDIR_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_CHANGEDIR_HINT));

        break;
    }

    case WM_USER_KEYDOWN:
    {
        BOOL processed = OnDirectoryKeyDown((DWORD)lParam, HWindow, IDE_PATH, 2 * MAX_PATH, IDB_BROWSE);
        if (!processed)
            processed = OnKeyDownHandleSelectAll((DWORD)lParam, HWindow, IDE_PATH);
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, processed);
        return processed;
    }

    case WM_USER_BUTTON:
    {
        OnDirectoryButton(HWindow, IDE_PATH, 2 * MAX_PATH, IDB_BROWSE, wParam, lParam);
        return 0;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CDriveInfo
//

CDriveInfo::CDriveInfo(HWND parent, const char* path, CObjectOrigin origin)
    : CCommonDialog(HLanguage, IDD_DRIVEINFO, IDD_DRIVEINFO, parent, origin)
{
    lstrcpyn(VolumePath, path, MAX_PATH);
    OldVolumeName[0] = 0;
    HDriveIcon = NULL;
}

void CDriveInfo::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CDriveInfo::Validate()");
    HWND edit;
    if (ti.GetControl(edit, IDE_VOLNAME) && ti.Type == ttDataFromWindow)
    {
        char newName[MAX_PATH];
        SendMessage(edit, WM_GETTEXT, MAX_PATH, (LPARAM)newName);

        if (strcmp(OldVolumeName, newName) != 0)
        {
            char volumePathWithBackslash[MAX_PATH];
            lstrcpyn(volumePathWithBackslash, VolumePath, MAX_PATH);
            SalPathAddBackslash(volumePathWithBackslash, MAX_PATH);
            BOOL handsOffLeft = SalPathIsPrefix(volumePathWithBackslash, MainWindow->LeftPanel->GetPath());
            BOOL handsOffRight = SalPathIsPrefix(volumePathWithBackslash, MainWindow->RightPanel->GetPath());
            if (handsOffLeft)
                MainWindow->LeftPanel->HandsOff(TRUE);
            if (handsOffRight)
                MainWindow->RightPanel->HandsOff(TRUE);
            //      SAD_SetUACParentWindow(HWindow);
            //      BOOL res = SAD_SetVolumeLabel(volumePathWithBackslash, newName);
            //      DWORD err = SAD_GetLastError();
            BOOL res = SetVolumeLabel(volumePathWithBackslash, newName);
            DWORD err = GetLastError();
            if (handsOffLeft)
                MainWindow->LeftPanel->HandsOff(FALSE);
            if (handsOffRight)
                MainWindow->RightPanel->HandsOff(FALSE);
            if (!res)
            {
                char buf[MAX_PATH + 100];
                sprintf(buf, LoadStr(IDS_UNABLETOCHANGEDRIVELABEL), GetErrorText(err));
                SalMessageBox(HWindow, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDE_VOLNAME);
            }
        }
    }
}

void CDriveInfo::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CDriveInfo::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        BOOL err;
        //---  GetVolumeInformation
        char volumeName[1000]; // used further as a buffer
        char buff[300];
        char volumePathWithBackslash[MAX_PATH];
        DWORD volumeSerialNumber;
        DWORD maximumComponentLength;
        DWORD fileSystemFlags;
        char fileSystemNameBuffer[100];
        char junctionOrSymlinkTgt[MAX_PATH];
        int linkType;
        err = (MyGetVolumeInformation(VolumePath, volumePathWithBackslash, junctionOrSymlinkTgt, &linkType,
                                      volumeName, 200, &volumeSerialNumber, &maximumComponentLength,
                                      &fileSystemFlags, fileSystemNameBuffer, 100) == 0);
        lstrcpyn(VolumePath, volumePathWithBackslash, MAX_PATH);
        SalPathAddBackslash(volumePathWithBackslash, MAX_PATH);
        //--- GetVolumeInformation - display
        if (!err)
        {
            SetWindowText(GetDlgItem(HWindow, IDE_VOLNAME), volumeName);
            strcpy(OldVolumeName, volumeName);

            char mountPoint[MAX_PATH];
            char guidPath[MAX_PATH];
            mountPoint[0] = 0;
            guidPath[0] = 0;
            if (GetResolvedPathMountPointAndGUID(VolumePath, mountPoint, guidPath))
            {
                SetWindowText(GetDlgItem(HWindow, IDT_MOUNTPOINT), mountPoint);
                SetWindowText(GetDlgItem(HWindow, IDT_GUIDPATH), guidPath);
            }

            strcpy(volumeName, VolumePath);
            if (volumeName[strlen(volumeName) - 1] == '\\')
                volumeName[strlen(volumeName) - 1] = 0; // shortening by the last character ('\\')
            sprintf(buff, "(%s) ", volumeName);
            GetWindowText(HWindow, buff + strlen(buff), 100);

            SetWindowText(HWindow, buff);

            sprintf(volumeName, "%04X-%04X", HIWORD(volumeSerialNumber), LOWORD(volumeSerialNumber));
            SetWindowText(GetDlgItem(HWindow, IDT_VOLSERNUM), volumeName);

            strcpy(volumeName, (maximumComponentLength > 100) ? LoadStr(IDS_INFODLGYES) : LoadStr(IDS_INFODLGNO));
            SetWindowText(GetDlgItem(HWindow, IDT_LONGNAMES), volumeName);

            volumeName[0] = 0;
            BOOL first = TRUE;
            if (fileSystemFlags & FS_CASE_IS_PRESERVED)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG1));
                first = FALSE;
            }
            if (fileSystemFlags & FS_CASE_SENSITIVE)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG2));
                first = FALSE;
            }
            if (fileSystemFlags & FS_UNICODE_STORED_ON_DISK)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG3));
                first = FALSE;
            }
            if (fileSystemFlags & FS_PERSISTENT_ACLS)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG4));
                first = FALSE;
            }
            if (fileSystemFlags & FS_FILE_COMPRESSION)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG5));
                first = FALSE;
            }
            if (fileSystemFlags & FS_VOL_IS_COMPRESSED)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG6));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_NAMED_STREAMS)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG7));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_READ_ONLY_VOLUME)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG8));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_SUPPORTS_ENCRYPTION)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG9));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_SUPPORTS_OBJECT_IDS)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG10));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_SUPPORTS_REPARSE_POINTS)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG11));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_SUPPORTS_SPARSE_FILES)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG12));
                first = FALSE;
            }
            if (fileSystemFlags & FILE_VOLUME_QUOTAS)
            {
                if (!first)
                    strcat(volumeName, ", ");
                strcat(volumeName, LoadStr(IDS_INFODLGFLAG13));
                first = FALSE;
            }
            SetWindowText(GetDlgItem(HWindow, IDT_FILESYSTEMFLAGS), volumeName);

            SetWindowText(GetDlgItem(HWindow, IDT_FILESYSTEMNAME), fileSystemNameBuffer);
        }
        //---  GetDiskFreeSpace
        DWORD sectorsPerCluster;
        DWORD bytesPerSector;
        DWORD numberOfFreeClusters;
        DWORD totalNumberOfClusters;
        err = (MyGetDiskFreeSpace(volumePathWithBackslash, &sectorsPerCluster,
                                  &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters) == 0);

        CQuadWord diskTotalBytes = CQuadWord(-1, -1), diskFreeBytes;
        ULARGE_INTEGER availBytes, totalBytes, freeBytes;
        if (GetDiskFreeSpaceEx(volumePathWithBackslash, &availBytes, &totalBytes, &freeBytes))
        {
            diskTotalBytes.Value = (unsigned __int64)totalBytes.QuadPart;
            diskFreeBytes.Value = (unsigned __int64)availBytes.QuadPart;
        }
        if (diskTotalBytes == CQuadWord(-1, -1) && !err)
        {
            diskTotalBytes = CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) *
                             CQuadWord(totalNumberOfClusters, 0);
            diskFreeBytes = CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) *
                            CQuadWord(numberOfFreeClusters, 0);
        }
        //--- GetDiskFreeSpace - display
        if (!err)
        {
            NumberToStr(volumeName, CQuadWord(sectorsPerCluster, 0));
            SetWindowText(GetDlgItem(HWindow, IDT_SPC), volumeName);

            NumberToStr(volumeName, CQuadWord(bytesPerSector, 0));
            SetWindowText(GetDlgItem(HWindow, IDT_BPS), volumeName);

            if (CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) != CQuadWord(0, 0))
                NumberToStr(volumeName, diskTotalBytes / (CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0)));
            else
                volumeName[0] = 0;
            SetWindowText(GetDlgItem(HWindow, IDT_NOC), volumeName);

            if (CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0) != CQuadWord(0, 0))
                NumberToStr(volumeName, CQuadWord(bytesPerSector, 0) * CQuadWord(sectorsPerCluster, 0));
            else
                volumeName[0] = 0;
            SetWindowText(GetDlgItem(HWindow, IDT_BPC), volumeName);
        }
        if (diskTotalBytes != CQuadWord(-1, -1))
        {
            double used = 1 - (diskFreeBytes <= diskTotalBytes && diskTotalBytes > CQuadWord(0, 0) ? diskFreeBytes.GetDouble() / diskTotalBytes.GetDouble() : 1);
            Graph->SetUsed(used);

            int spaceForLongAndShort = 0;
            RECT tmpR1;
            RECT tmpR2;
            GetWindowRect(GetDlgItem(HWindow, IDT_CAPACITY), &tmpR1);
            GetWindowRect(GetDlgItem(HWindow, IDB_GRAPH), &tmpR2);
            spaceForLongAndShort = tmpR2.left - tmpR1.left;

            SetWindowText(GetDlgItem(HWindow, IDT_CAPACITY), PrintDiskSize(volumeName, diskTotalBytes, 2));
            SetWindowText(GetDlgItem(HWindow, IDT_CAPACITY_SHORT), PrintDiskSize(volumeName, diskTotalBytes, 0));
            SetWindowText(GetDlgItem(HWindow, IDT_FREESPACE), PrintDiskSize(volumeName, diskFreeBytes, 2));
            SetWindowText(GetDlgItem(HWindow, IDT_FREESPACE_SHORT), PrintDiskSize(volumeName, diskFreeBytes, 0));
            if (diskTotalBytes >= diskFreeBytes)
                diskTotalBytes -= diskFreeBytes;
            else
                diskTotalBytes.SetUI64(0); // rather zero than complete nonsense
            SetWindowText(GetDlgItem(HWindow, IDT_USEDSPACE), PrintDiskSize(volumeName, diskTotalBytes, 2));
            SetWindowText(GetDlgItem(HWindow, IDT_USEDSPACE_SHORT), PrintDiskSize(volumeName, diskTotalBytes, 0));
            // place the statics
            int height;
            RECT r;
            GetClientRect(GetDlgItem(HWindow, IDT_CAPACITY), &r);
            height = r.bottom - r.top;
            int longWidth = 0;
            GrowWidth(IDT_CAPACITY, longWidth);
            GrowWidth(IDT_FREESPACE, longWidth);
            GrowWidth(IDT_USEDSPACE, longWidth);
            longWidth++; // Switching to editline caused the right sides to be slightly scattered

            int y1, y2, y3;
            int x;

            GetWindowRect(GetDlgItem(HWindow, IDT_CAPACITY), &r);
            ScreenToClient(HWindow, (LPPOINT)&r);
            x = r.left;

            y1 = r.top;
            SetWindowPos(GetDlgItem(HWindow, IDT_CAPACITY), NULL, 0, 0, longWidth, height,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);

            GetWindowRect(GetDlgItem(HWindow, IDT_FREESPACE), &r);
            ScreenToClient(HWindow, (LPPOINT)&r);
            y2 = r.top;
            SetWindowPos(GetDlgItem(HWindow, IDT_FREESPACE), NULL, 0, 0, longWidth, height,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);

            GetWindowRect(GetDlgItem(HWindow, IDT_USEDSPACE), &r);
            ScreenToClient(HWindow, (LPPOINT)&r);
            y3 = r.top;
            SetWindowPos(GetDlgItem(HWindow, IDT_USEDSPACE), NULL, 0, 0, longWidth, height,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);

            int shortWidth = 0;
            GrowWidth(IDT_CAPACITY_SHORT, shortWidth);
            GrowWidth(IDT_FREESPACE_SHORT, shortWidth);
            GrowWidth(IDT_USEDSPACE_SHORT, shortWidth);
            shortWidth++;                                                                 // Switching to editline caused the right sides to be slightly scattered
            x = r.left + longWidth + (spaceForLongAndShort - longWidth - shortWidth) / 2; // Center the SHORT between LONG and GRAPH
            if (x < r.left + longWidth)
                x = r.left + longWidth + height;
            SetWindowPos(GetDlgItem(HWindow, IDT_CAPACITY_SHORT), NULL, x, y1, shortWidth, height,
                         SWP_NOZORDER | SWP_NOOWNERZORDER);
            SetWindowPos(GetDlgItem(HWindow, IDT_FREESPACE_SHORT), NULL, x, y2, shortWidth, height,
                         SWP_NOZORDER | SWP_NOOWNERZORDER);
            SetWindowPos(GetDlgItem(HWindow, IDT_USEDSPACE_SHORT), NULL, x, y3, shortWidth, height,
                         SWP_NOZORDER | SWP_NOOWNERZORDER);
        }
        //--- GetDriveType
        UINT driveType;
        char remoteName[MAX_PATH];
        BOOL remoteNameValid = FALSE;
        char userName[100];
        BOOL userNameValid = FALSE;
        driveType = MyGetDriveType(volumePathWithBackslash);
        err = (driveType == 0 || driveType == 1);
        if (driveType == DRIVE_REMOTE)
        {
            // GetRootPath(buff, volumePathWithBackslash);
            lstrcpyn(buff, volumePathWithBackslash, 300);
            if (buff[0] != 0 && buff[1] == ':' && strlen(buff) <= 3)
                buff[2] = 0; // "x:\\" -> "x:"
            DWORD l = MAX_PATH;
            remoteNameValid = (WNetGetConnection(buff, remoteName, &l) == NO_ERROR);
            l = 100;
            userNameValid = (WNetGetUser(buff, userName, &l) == NO_ERROR);
        }
        //--- GetDriveType - display
        if (!err)
        {
            switch (driveType)
            {
            case DRIVE_REMOVABLE:
                strcpy(volumeName, LoadStr(IDS_INFODLGTYPE1));
                break;
            case DRIVE_FIXED:
                strcpy(volumeName, LoadStr(IDS_INFODLGTYPE2));
                break;
            case DRIVE_REMOTE:
            {
                strcpy(volumeName, LoadStr(IDS_INFODLGTYPE3));
                if (remoteNameValid || userNameValid)
                {
                    strcat(volumeName, " ");
                    sprintf(volumeName + strlen(volumeName), LoadStr(IDS_INFODLGTYPE8),
                            remoteNameValid ? remoteName : "",
                            userNameValid ? userName : "");
                }
                break;
            }
            case DRIVE_CDROM:
                strcpy(volumeName, LoadStr(IDS_INFODLGTYPE4));
                break;
            case DRIVE_RAMDISK:
                strcpy(volumeName, LoadStr(IDS_INFODLGTYPE5));
                break;
            default:
                sprintf(volumeName, LoadStr(IDS_INFODLGTYPE6), driveType);
                break;
            }
            BOOL substInfo = FALSE;
            if (volumePathWithBackslash[0] != '\\' && strlen(volumePathWithBackslash) <= 3)
            {
                char drive = toupper(volumePathWithBackslash[0]);
                if (GetSubstInformation(drive - 'A', buff, 300))
                {
                    substInfo = TRUE;
                    strcat(volumeName, " ");
                    sprintf(volumeName + strlen(volumeName), LoadStr(IDS_INFODLGTYPE7), buff);
                }
            }
            if (!substInfo && junctionOrSymlinkTgt[0] != 0)
            {
                strcat(volumeName, " ");
                sprintf(volumeName + strlen(volumeName), LoadStr(linkType == 2 ? IDS_INFODLGTYPE9 : IDS_INFODLGTYPE10),
                        junctionOrSymlinkTgt);
            }
            SetWindowText(GetDlgItem(HWindow, IDT_DRIVETYPE), volumeName);
        }
        //--- GetDriveIcon
        HDriveIcon = GetDriveIcon(volumePathWithBackslash, driveType, TRUE, TRUE);
        SendDlgItemMessage(HWindow, IDI_DI_DRIVE, STM_SETIMAGE, IMAGE_ICON, (LPARAM)HDriveIcon);
    }
}

void CDriveInfo::GrowWidth(int resID, int& width)
{
    char buff[200];
    int minWidth = 0;

    HWND hItem = GetDlgItem(HWindow, resID);
    GetWindowText(hItem, buff, 200);
    strcat(buff, "M"); // editbox has some borders, we compensate for this by adding this "M"
    HFONT hFont = (HFONT)SendMessage(hItem, WM_GETFONT, 0, 0);

    SIZE sz;
    HDC hDC = HANDLES(GetDC(HWindow));
    HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
    GetTextExtentPoint32(hDC, buff, (int)strlen(buff), &sz);
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(HWindow, hDC));

    if (width < sz.cx)
        width = sz.cx;
}

INT_PTR
CDriveInfo::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CDriveInfo::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        COLORREF FreeLight, FreeDark, UsedLight, UsedDark;
        HDC hdc = HANDLES(GetDC(HWindow));
        int devCaps = GetDeviceCaps(hdc, NUMCOLORS);
        HANDLES(ReleaseDC(HWindow, hdc));
        if (devCaps == -1) // more than 256 colors
        {
            FreeLight = RGB(35, 245, 156);
            FreeDark = RGB(9, 159, 96);
            UsedLight = RGB(74, 163, 234);
            UsedDark = RGB(18, 95, 156);
        }
        else
        {
            FreeLight = RGB(0, 255, 0);
            FreeDark = RGB(0, 128, 0);
            UsedLight = RGB(0, 0, 255);
            UsedDark = RGB(0, 0, 128);
        }

        CColorRectangle* cr;
        cr = new CColorRectangle(HWindow, IDB_FREESPACE);
        if (cr != NULL)
            cr->SetColor(FreeLight);
        cr = new CColorRectangle(HWindow, IDB_USEDSPACE);
        if (cr != NULL)
            cr->SetColor(UsedLight);

        Graph = new CColorGraph(HWindow, IDB_GRAPH); // JRYFIXME - rewrite to W10 look, see properties for disk, it wouldn't be convenient to use GDI+, maybe our SVG?
        if (Graph != NULL)
            Graph->SetColor(FreeLight, FreeDark, UsedLight, UsedDark);

        break;
    }

    case WM_DESTROY:
    {
        if (HDriveIcon != NULL)
            HANDLES(DestroyIcon(HDriveIcon));
        break;
    }

        //    case WM_COMMAND:
        //    {
        //      if (HIWORD(wParam) == EN_CHANGE)
        //        Dirty = TRUE;
        //      break;
        //    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CEnterPasswdDialog
//

CEnterPasswdDialog::CEnterPasswdDialog(HWND parent, const char* path, const char* user,
                                       CObjectOrigin origin)
    : CCommonDialog(HLanguage, IDD_ENTERPASSWD, IDD_ENTERPASSWD, parent, origin)
{
    Path = path;
    if (user != NULL)
        lstrcpyn(User, user, USERNAME_MAXLEN);
    else
        User[0] = 0;
    Passwd[0] = 0;
}

void CEnterPasswdDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CEnterPasswdDialog::Validate()");
    /*  // empty user-name = default username
  HWND edit;
  if (ti.GetControl(edit, IDE_NETUSER) && ti.Type == ttDataFromWindow)
  {
    if (SendMessage(edit, WM_GETTEXTLENGTH, 0, 0) == 0)
    {
      SalMessageBox(HWindow, LoadStr(IDS_EMPTYUSERNAME),
                    LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
      ti.ErrorOn(IDE_NETUSER);
    }
  }*/
}

void CEnterPasswdDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDE_NETPASSWD, Passwd, PASSWORD_MAXLEN);
    ti.EditLine(IDE_NETUSER, User, USERNAME_MAXLEN);
}

INT_PTR
CEnterPasswdDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowText(GetDlgItem(HWindow, IDS_NETPATH), Path);
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CPackDialog
//

CPackDialog::CPackDialog(HWND parent, char* path, const char* pathAlt,
                         CTruncatedString* subject, CPackerConfig* config)
    : CCommonDialog(HLanguage, IDD_PACK, IDD_PACK, parent)
{
    Subject = subject;
    Path = path;
    PathAlt = pathAlt;
    PackerConfig = config;
    SelectionEnd = -1;
}

void CPackDialog::SetSelectionEnd(int selectionEnd)
{
    SelectionEnd = selectionEnd;
}

void CPackDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPackDialog::Transfer()");
    HWND combo;
    if (ti.GetControl(combo, IDC_PACKER))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            int i;
            for (i = 0; i < PackerConfig->GetPackersCount(); i++)
            {
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)PackerConfig->GetPackerTitle(i));
            }
            // set position in combu, preferedPacker == -1 -> no selection
            SendMessage(combo, CB_SETCURSEL, (WPARAM)PackerConfig->GetPreferedPacker(), 0);

            i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR)
            {
                BOOL supMove = TRUE;
                if (PackerConfig->GetPackerType(i) == CUSTOMPACKER_EXTERNAL)
                {
                    supMove = PackerConfig->GetPackerSupMove(i);
                }
                EnableWindow(GetDlgItem(HWindow, IDC_MOVEFILES), supMove);
            }
        }
        else // ttDataFromWindow
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, (WPARAM)PackerConfig->GetPreferedPacker(), 0);
            if (i != CB_ERR)
                PackerConfig->SetPreferedPacker(i);
            else
                PackerConfig->SetPreferedPacker(-1);
        }
    }

    if (ti.Type == ttDataToWindow)
    {
        // !!! WARNING: the code must be consistent with CPackDialog::DialogProc/WM_COMMAND
        ti.GetControl(combo, IDE_PATH);
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)Path);
        // if the alternative path is the same as the first one, we will not add it (target is not ptDisk)
        if (StrICmp(Path, PathAlt) != 0)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)PathAlt);
        SendMessage(combo, CB_SETCURSEL, 0, 0);
    }
    else
        ti.EditLine(IDE_PATH, Path, MAX_PATH);

    if (ti.Type == ttDataFromWindow) // if the entered name does not have an extension, we will add it automatically
    {
        if (PackerConfig->GetPreferedPacker() != -1) // if we have an extension, otherwise we do not change the entered name
        {
            const char* ext = PackerConfig->GetPackerExt(PackerConfig->GetPreferedPacker());
            char* s = strrchr(Path, '.');
            char* s2 = strrchr(Path, '\\');
            int nameLen = (int)strlen(Path);
            if ((s == NULL || s2 != NULL && s2 > s) && // ".cvspass" in Windows is an extension ...
                (s2 == NULL || (s2 - Path + 1) < nameLen) &&
                nameLen > 0 &&
                nameLen + 1 + 1 + strlen(ext) < MAX_PATH)
            {
                strcpy(Path + nameLen, ".");
                strcpy(Path + nameLen + 1, ext);
            }
        }
    }

    HWND move;
    if (ti.GetControl(move, IDC_MOVEFILES))
    {
        if (ti.Type == ttDataToWindow)
        {
            ti.CheckBox(IDC_MOVEFILES, PackerConfig->Move);
        }
        else // ttDataFromWindow
        {
            if (IsWindowEnabled(move))
            {
                ti.CheckBox(IDC_MOVEFILES, PackerConfig->Move);
            }
            else
            {
                PackerConfig->Move = FALSE;
            }
        }
    }
}

BOOL CPackDialog::ChangeExtension(char* name, const char* ext)
{
    char* s = strrchr(name, '.');
    char* s2 = strrchr(name, '\\');
    if (s != NULL && // ".cvspass" in Windows is an extension ...
                     //if (s != NULL && s > name &&
        (s2 == NULL || s > s2) &&
        strlen(ext) + 1 + ((s + 1) - name) < MAX_PATH)
    {
        strcpy(s + 1, ext);
        return TRUE;
    }
    else
    {
        int nameLen = (int)strlen(name);
        if ((s == NULL || s2 != NULL && s2 > s) &&
            (s2 == NULL || (s2 - name + 1) < nameLen) &&
            nameLen > 0 &&
            nameLen + 1 + 1 + strlen(ext) < MAX_PATH)
        {
            strcpy(name + nameLen, ".");
            strcpy(name + nameLen + 1, ext);
            return TRUE;
        }
    }
    return FALSE;
}

INT_PTR
CPackDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPackDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_PATH)); // installing WordBreakProc into editline

        HWND hSubject = GetDlgItem(HWindow, IDS_SUBJECT);
        if (Subject->TruncateText(hSubject))
            SetWindowText(hSubject, Subject->Get());

        INT_PTR ret = CCommonDialog::DialogProc(uMsg, wParam, lParam);
        // we can only select the name without the dot and extension
        PostMessage(GetDlgItem(HWindow, IDE_PATH), CB_SETEDITSEL, 0, MAKELPARAM(0, SelectionEnd));
        return FALSE;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_PACKER)
        {
            int i = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR)
            {
                // change extensions
                char name[MAX_PATH];
                char name2[MAX_PATH];

                int curSel = (int)SendDlgItemMessage(HWindow, IDE_PATH, CB_GETCURSEL, 0, 0);
                if (curSel == CB_ERR) // I need to pull out the text here already, because CB_RESETCONTENT shoots it down
                    GetWindowText(GetDlgItem(HWindow, IDE_PATH), name2, MAX_PATH);

                // !!! WARNING: the code must be consistent with CPackDialog::Transfer
                // change the extensions in the combobox
                SendDlgItemMessage(HWindow, IDE_PATH, CB_RESETCONTENT, 0, 0);
                strcpy(name, Path);
                if (ChangeExtension(name, PackerConfig->GetPackerExt(i)))
                    SendDlgItemMessage(HWindow, IDE_PATH, CB_ADDSTRING, 0, (LPARAM)name);
                else
                    SendDlgItemMessage(HWindow, IDE_PATH, CB_ADDSTRING, 0, (LPARAM)Path);

                // if the alternative path is the same as the first one, we will not add it (target is not ptDisk)
                if (StrICmp(Path, PathAlt) != 0)
                {
                    strcpy(name, PathAlt);
                    if (ChangeExtension(name, PackerConfig->GetPackerExt(i)))
                        SendDlgItemMessage(HWindow, IDE_PATH, CB_ADDSTRING, 0, (LPARAM)name);
                    else
                        SendDlgItemMessage(HWindow, IDE_PATH, CB_ADDSTRING, 0, (LPARAM)PathAlt);
                }

                if (curSel != CB_ERR)
                    SendDlgItemMessage(HWindow, IDE_PATH, CB_SETCURSEL, (WPARAM)curSel, 0);
                else
                {
                    // if the editline is modified, I will also change the extension in it
                    if (ChangeExtension(name2, PackerConfig->GetPackerExt(i)))
                        SetWindowText(GetDlgItem(HWindow, IDE_PATH), name2);
                }

                BOOL supMove = TRUE;
                if (PackerConfig->GetPackerType(i) == CUSTOMPACKER_EXTERNAL)
                {
                    supMove = PackerConfig->GetPackerSupMove(i);
                }
                EnableWindow(GetDlgItem(HWindow, IDC_MOVEFILES), supMove);
            }
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CUnpackDialog
//

CUnpackDialog::CUnpackDialog(HWND parent, char* path, const char* pathAlt, char* mask,
                             CTruncatedString* subject, CUnpackerConfig* config,
                             BOOL* delArchiveWhenDone)
    : CCommonDialog(HLanguage, IDD_UNPACK, IDD_UNPACK, parent)
{
    Subject = subject;
    Path = path;
    PathAlt = pathAlt;
    Mask = mask;
    UnpackerConfig = config;
    DelArchiveWhenDone = delArchiveWhenDone;
}

void CUnpackDialog::EnableDelArcCheckbox()
{
    int i = (int)SendDlgItemMessage(HWindow, IDC_PACKER, CB_GETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(HWindow, IDC_DELETEARCHIVEFILES),
                 i != CB_ERR && UnpackerConfig->GetUnpackerType(i) != CUSTOMUNPACKER_EXTERNAL);
}

void CUnpackDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CUnpackDialog::Transfer()");
    HWND combo;
    if (ti.GetControl(combo, IDC_PACKER))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            int i;
            for (i = 0; i < UnpackerConfig->GetUnpackersCount(); i++)
            {
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)UnpackerConfig->GetUnpackerTitle(i));
            }
            // set position in combu, preferedUnpacker == -1 -> no selection
            SendMessage(combo, CB_SETCURSEL, (WPARAM)UnpackerConfig->GetPreferedUnpacker(), 0);
            EnableDelArcCheckbox();
        }
        else // ttDataFromWindow
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR)
                UnpackerConfig->SetPreferedUnpacker(i);
            else
                UnpackerConfig->SetPreferedUnpacker(-1);
        }
    }
    if (ti.Type == ttDataToWindow)
    {
        ti.GetControl(combo, IDE_PATH);
        SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)Path);
        // if the alternative path is the same as the first one, we will not add it (target is not ptDisk)
        if (StrICmp(Path, PathAlt) != 0)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)PathAlt);
        SendMessage(combo, CB_SETCURSEL, 0, 0);
        ti.CheckBox(IDC_DELETEARCHIVEFILES, *DelArchiveWhenDone);
    }
    else
    {
        ti.EditLine(IDE_PATH, Path, MAX_PATH);
        if (IsWindowEnabled(GetDlgItem(HWindow, IDC_DELETEARCHIVEFILES)))
            ti.CheckBox(IDC_DELETEARCHIVEFILES, *DelArchiveWhenDone);
        else
            *DelArchiveWhenDone = FALSE;
    }

    ti.EditLine(IDE_MASK, Mask, MAX_PATH);
}

INT_PTR
CUnpackDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_PATH)); // installing WordBreakProc into editline
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_MASK)); // installing WordBreakProc into editline

        HWND hSubject = GetDlgItem(HWindow, IDS_SUBJECT);
        if (Subject->TruncateText(hSubject))
            SetWindowText(hSubject, Subject->Get());

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_PACKER)
            EnableDelArcCheckbox();
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CZIPSizeResultsDlg
//

CZIPSizeResultsDlg::CZIPSizeResultsDlg(HWND parent, const CQuadWord& size, int files, int dirs)
    : CCommonDialog(HLanguage, IDD_ZIPSIZERESULTS, parent)
{
    Size = size;
    Files = files;
    Dirs = dirs;
}

INT_PTR
CZIPSizeResultsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        /*        RECT r1;                        // horizontal centering of the dialog
      GetWindowRect(HWindow, &r1);
      RECT r2;
      GetWindowRect(MainWindow->GetActivePanelHWND(), &r2);
      int width = r1.right - r1.left;
      r1.left = (r2.right + r2.left - width) / 2;
      MoveWindow(HWindow, r1.left, r1.top, width, r1.bottom - r1.top, FALSE);*/
        char buf[50];
        SetWindowText(GetDlgItem(HWindow, IDS_SIZE), PrintDiskSize(buf, Size, 1));
        SetWindowText(GetDlgItem(HWindow, IDS_FILESCOUNT), NumberToStr(buf, CQuadWord(Files, 0)));
        SetWindowText(GetDlgItem(HWindow, IDS_DIRSCOUNT), NumberToStr(buf, CQuadWord(Dirs, 0)));
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//***************************************************************************
//
// ChangeIconDialog
//

CChangeIconDialog::CChangeIconDialog(HWND hParent, char* iconFile, int* iconIndex)
    : CCommonDialog(HLanguage, IDD_CHANGEICON, IDD_CHANGEICON, hParent)
{
    IconFile = iconFile;
    IconIndex = iconIndex;
    Dirty = FALSE;
    Icons = NULL;
    IconsCount = 0;
}

CChangeIconDialog::~CChangeIconDialog()
{
    DestroyIcons();
}

void CChangeIconDialog::GetShell32(char* fileName)
{
    GetSystemDirectory(fileName, MAX_PATH);
    SalPathAppend(fileName, "SHELL32.DLL", MAX_PATH);
    SetDlgItemText(HWindow, IDE_CHI_FILENAME, fileName);
}

void CChangeIconDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDE_CHI_FILENAME, IconFile, MAX_PATH);

    if (ti.Type == ttDataToWindow)
    {
        LoadIcons();
        Dirty = FALSE;
    }
    else
    {
        int curSel = (int)SendDlgItemMessage(HWindow, IDL_CHI_LIST, LB_GETCURSEL, 0, 0);
        if (curSel == LB_ERR || curSel >= (int)IconsCount)
        {
            char fileName[MAX_PATH];
            GetShell32(fileName);
            LoadIcons();
            *IconIndex = 0;
        }
        else
        {
            *IconIndex = curSel;
        }
    }
}

BOOL CChangeIconDialog::LoadIcons()
{
    char fileName[MAX_PATH];
    GetDlgItemText(HWindow, IDE_CHI_FILENAME, fileName, MAX_PATH);
    int counter = 0;

AGAIN:
    counter++;
    DestroyIcons();
    SendDlgItemMessage(HWindow, IDL_CHI_LIST, LB_SETCOUNT, 0, 0);

    if (MainWindow->GetActivePanel()->CheckPath(FALSE, fileName) != ERROR_SUCCESS)
    {
        char buff[1024];
        sprintf(buff, LoadStr(IDS_CANNONTFINDFILE), fileName);
        SalMessageBox(HWindow, buff, LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);

        // set default
        GetShell32(fileName);
    }

    // Enumerating icons from files *.ICO, *.EXE, *.DLL, including 16-bit PE
    int iconsCount = ExtractIconEx(fileName, -1, NULL, NULL, 0);
    if (iconsCount > 0)
    {
        Icons = new HICON[iconsCount];
        if (Icons != NULL)
        {
            IconsCount = ExtractIconEx(fileName, 0, Icons, NULL, iconsCount);
            // add handle to 'HIcon' to HANDLES
            for (DWORD i = 0; i < IconsCount; i++)
                HANDLES_ADD(__htIcon, __hoLoadImage, Icons[i]);
        }
        else
            TRACE_E(LOW_MEMORY);
    }

    if (IconsCount == 0)
    {
        char buff[1024];
        sprintf(buff, LoadStr(IDS_NOICONS), fileName);
        SalMessageBox(HWindow, buff, LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);

        // set default
        GetShell32(fileName);
        if (counter < 2) // fuse
            goto AGAIN;
    }

    SendDlgItemMessage(HWindow, IDL_CHI_LIST, LB_SETCOUNT, IconsCount, 0);

    if (IconsCount > 0 && *IconIndex < (int)IconsCount)
        SendDlgItemMessage(HWindow, IDL_CHI_LIST, LB_SETCURSEL, *IconIndex, 0);

    return TRUE;
}

void CChangeIconDialog::DestroyIcons()
{
    int i;
    for (i = 0; i < (int)IconsCount; i++)
        HANDLES(DestroyIcon(Icons[i]));
    delete[] Icons;
    Icons = NULL;
    IconsCount = 0;
}

INT_PTR
CChangeIconDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HWND hList = GetDlgItem(HWindow, IDL_CHI_LIST);
        SendMessage(hList, LB_SETCOLUMNWIDTH, 32 + 8, 0);
        SendMessage(hList, LB_SETITEMHEIGHT, 0, MAKELPARAM(32 + 8, 0));

        // adjust the size of the listbox
        RECT wRect;
        RECT cRect;
        GetWindowRect(hList, &wRect);
        GetClientRect(hList, &cRect);

        int deltaH = cRect.bottom - 4 * (32 + 8);

        SetWindowPos(hList, NULL, 0, 0,
                     wRect.right - wRect.left,
                     wRect.bottom - wRect.top - deltaH,
                     SWP_NOMOVE | SWP_NOZORDER);
        break;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if ((int)lpdis->itemID >= 0 && (int)lpdis->itemID < (int)IconsCount)
        {
            RECT r = lpdis->rcItem;

            // draw background
            DWORD bkColor = (lpdis->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW;
            FillRect(lpdis->hDC, &r, (HBRUSH)(DWORD_PTR)(bkColor + 1));

            // draw an icon
            DrawIconEx(lpdis->hDC, r.left + 4, r.top + 4, Icons[lpdis->itemID], 32, 32, 0, NULL, DI_NORMAL);

            if (lpdis->itemState & ODS_FOCUS)
                DrawFocusRect(lpdis->hDC, &r);
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDL_CHI_LIST && HIWORD(wParam) == LBN_DBLCLK)
        {
            PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDOK, BN_CLICKED), 0);
            break;
        }

        if (LOWORD(wParam) == IDOK)
        {
            if (Dirty)
            {
                Dirty = FALSE;
                LoadIcons();
                return 0;
            }
            break;
        }

        if (LOWORD(wParam) == IDE_CHI_FILENAME)
        {
            if (HIWORD(wParam) == EN_CHANGE)
                Dirty = TRUE;
            if (HIWORD(wParam) == EN_KILLFOCUS)
            {
                if (Dirty)
                {
                    LoadIcons();
                    Dirty = FALSE;
                }
            }
            break;
        }

        if (LOWORD(wParam) == IDL_CHI_BROWSE)
        {
            if (BrowseCommand(HWindow, IDE_CHI_FILENAME, IDS_ICOFILTER))
            {
                LoadIcons();
                Dirty = FALSE;
            }
            return 0;
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CWaitWindow
//

CWaitWindow::CWaitWindow(HWND hParent, int textResID, BOOL showCloseButton, CObjectOrigin origin, BOOL showProgressBar)
    : CWindow(origin)
{
    HParent = hParent;
    HForegroundWnd = NULL;
    if (textResID != 0)
    {
        char* t = LoadStr(textResID);
        Text = DupStr(t);
    }
    else
        Text = NULL;
    Caption = NULL;
    ShowCloseButton = showCloseButton;
    ShowProgressBar = showProgressBar;
    BarMax = 0;
    BarPos = 0;
    NeedWrap = FALSE;
    CacheBitmap = NULL;
}

CWaitWindow::~CWaitWindow()
{
    if (Text != NULL)
        free(Text);
    if (Caption != NULL)
        free(Caption);
    if (CacheBitmap != NULL)
        delete (CacheBitmap);
}

void CWaitWindow::SetText(const char* text)
{
    if (Text != NULL)
        free(Text);
    Text = DupStr(text);
    if (HWindow != NULL && IsWindowVisible(HWindow))
    {
        HDC hDC = GetDC(HWindow);
        if (CacheBitmap == NULL) // only when the text changes will we go through the bitmap cache
        {
            CacheBitmap = new CBitmap();
            if (CacheBitmap != NULL)
            {
                RECT r;
                GetClientRect(HWindow, &r);
                if (!CacheBitmap->CreateBmp(hDC, r.right, r.bottom))
                {
                    delete (CacheBitmap);
                    CacheBitmap = NULL;
                }
            }
        }
        PaintText(hDC);
        ReleaseDC(HWindow, hDC);
    }
}

void CWaitWindow::SetCaption(const char* text)
{
    if (Caption != NULL)
        free(Caption);
    Caption = DupStr(text);
}

#define WAITWINDOW_HMARGIN 21
#define WAITWINDOW_VMARGIN 14

HWND CWaitWindow::Create(HWND hForegroundWnd)
{
    if (Text == NULL)
    {
        TRACE_E("CWaitWindow::Create(): you must set text for wait-wnd first!");
        return NULL;
    }
    HForegroundWnd = hForegroundWnd;

    if (HForegroundWnd != NULL)
        HForegroundWnd = GetTopVisibleParent(HForegroundWnd);

    // calculate the position of the window; prioritize centering to HForegroundWindow; secondarily to MainWindow
    HWND hCenterWnd = NULL;
    if (HForegroundWnd != NULL)
        hCenterWnd = HForegroundWnd;
    else if (MainWindow != NULL)
        hCenterWnd = MainWindow->HWindow;

    RECT clipRect;
    MultiMonGetClipRectByWindow(hCenterWnd, &clipRect, NULL);

    int scrW = clipRect.right - clipRect.left;
    int scrH = clipRect.bottom - clipRect.top;

    // calculate the size of the text => window size
    NeedWrap = FALSE;
    HDC dc = HANDLES(GetDC(NULL));
    if (dc != NULL)
    {
        HFONT old = (HFONT)SelectObject(dc, EnvFont);

        RECT tR;
        tR.left = 0;
        tR.top = 0;
        tR.right = 1;
        tR.bottom = 1;
        DrawText(dc, Text, -1, &tR, DT_CALCRECT | DT_LEFT | DT_NOPREFIX);
        if (tR.right + 2 * WAITWINDOW_HMARGIN >= scrW)
        {
            tR.right = (int)(scrW / 1.8);
            tR.bottom = 1;
            DrawText(dc, Text, -1, &tR, DT_CALCRECT | DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
            NeedWrap = TRUE;
        }
        TextSize.cx = tR.right;
        TextSize.cy = tR.bottom;
        SelectObject(dc, old);
        HANDLES(ReleaseDC(NULL, dc));
    }

    // Application compiled with a newer platform toolset behaves differently with window sizes, see more at
    // https://social.msdn.microsoft.com/Forums/vstudio/en-US/7ca548b5-8931-41dc-ac1d-ed9aed223d7a/different-dialog-box-position-and-size-with-visual-c-2012
    // https://connect.microsoft.com/VisualStudio/feedback/details/768135/different-dialog-box-size-and-position-when-compiled-in-visual-c-2012-vs-2010-2008
    // So we will use a hack where we first create the window, then measure the client area, and then adjust the window size
    // Note: AdjustWindowRectEx is unusable because it's nonsense; the original frame addition is also unusable, see the links above

    int width = TextSize.cx + 2 * WAITWINDOW_HMARGIN;
    int height = TextSize.cy + 2 * WAITWINDOW_VMARGIN;

    if (ShowProgressBar)
    {
        height += 8; // create space for progress bar

        BarRect.left = WAITWINDOW_HMARGIN;
        BarRect.top = WAITWINDOW_VMARGIN + TextSize.cy + 7;
        BarRect.right = BarRect.left + TextSize.cx;
        BarRect.bottom = BarRect.top + 4;
    }

    CreateEx(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
             SAVEBITS_CLASSNAME,
             Caption == NULL ? "Open Salamander" : Caption,
             WS_BORDER | WS_OVERLAPPED | (ShowCloseButton ? WS_SYSMENU : 0),
             0, 0, width, height,
             HParent,
             NULL,
             HInstance,
             this);

    // hack: real-time window resizing to work with both old (5 / XP compatible) and new toolset
    RECT clientR;
    GetClientRect(HWindow, &clientR);
    width += width - (clientR.right - clientR.left);
    height += height - (clientR.bottom - clientR.top);

    SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, width, height, SWP_NOMOVE | SWP_NOACTIVATE);
    MultiMonCenterWindow(HWindow, hCenterWnd, TRUE);

    // if HForegroundWnd != NULL, the display will be handled elsewhere
    if (HForegroundWnd == NULL)
        SetWindowPos(HWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    return HWindow;
}

void CWaitWindow::SetProgressMax(DWORD max)
{
    if (ShowProgressBar)
    {
        BarMax = max;
        if (HWindow != NULL)
            PaintProgressBar(NULL);
    }
    else
        TRACE_E("CWaitWindow::SetProgressMax() The progress bar must be enabled in the CWaitWindow constructor");
}
void CWaitWindow::SetProgressPos(DWORD pos)
{
    if (ShowProgressBar)
    {
        BarPos = pos;
        if (HWindow != NULL)
            PaintProgressBar(NULL);
    }
    else
        TRACE_E("CWaitWindow::SetProgressPos() The progress bar must be enabled in the CWaitWindow constructor");
}

extern BOOL SafeWaitWindowClosePressed;

void CWaitWindow::PaintProgressBar(HDC dc)
{
    BOOL releaseDC = FALSE;
    if (dc == NULL)
    {
        if (HWindow == NULL)
            return;
        dc = GetDC(HWindow);
        releaseDC = TRUE;
    }
    MoveToEx(dc, BarRect.left, BarRect.top, NULL);
    LineTo(dc, BarRect.right, BarRect.top);
    LineTo(dc, BarRect.right, BarRect.bottom);
    LineTo(dc, BarRect.left, BarRect.bottom);
    LineTo(dc, BarRect.left, BarRect.top);

    int width = BarRect.right - BarRect.left;
    if (width > 2)
    {
        int done = 0;
        if (BarMax > 0)
            done = ((width - 1) * BarPos) / BarMax;
        if (done > width - 1)
            done = width - 1;
        RECT r = BarRect;
        r.left++;
        r.top++;
        RECT r2 = r;
        r2.right = r2.left + done;
        FillRect(dc, &r2, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        r2 = r;
        r2.left = r2.left + done;
        FillRect(dc, &r2, (HBRUSH)(COLOR_WINDOW + 1));
    }
    if (releaseDC)
        ReleaseDC(HWindow, dc);
    GdiFlush();
}

void CWaitWindow::PaintText(HDC hDC)
{
    RECT r;
    GetClientRect(HWindow, &r);

    r.left = (r.right - TextSize.cx) / 2;
    r.top = (r.bottom - TextSize.cy) / 2;
    r.right = r.left + TextSize.cx;
    r.bottom = r.top + TextSize.cy;

    if (ShowProgressBar)
    {
        r.top -= 4;
        r.bottom -= 4;
    }

    if (Text != NULL)
    {

        HDC hDestDC = hDC;
        if (CacheBitmap != NULL && CacheBitmap->HMemDC != NULL)
            hDestDC = CacheBitmap->HMemDC;

        FillRect(hDestDC, &r, (HBRUSH)(COLOR_BTNFACE + 1));

        HFONT hOldFont = (HFONT)SelectObject(hDestDC, EnvFont);
        int prevBkMode = SetBkMode(hDestDC, TRANSPARENT);
        SetTextColor(hDestDC, GetSysColor(COLOR_BTNTEXT));
        // we will not clip to achieve a slight extension of the text
        // can occur during the SetText call
        DrawText(hDestDC, Text, (int)strlen(Text), &r, DT_LEFT | DT_NOPREFIX | DT_NOCLIP | (NeedWrap ? DT_WORDBREAK : 0));
        SetBkMode(hDestDC, prevBkMode);
        SelectObject(hDestDC, hOldFont);

        if (CacheBitmap != NULL && CacheBitmap->HMemDC != NULL)
        {
            BitBlt(hDC, r.left, r.top, r.right - r.left, r.bottom - r.top,
                   CacheBitmap->HMemDC, r.left, r.top, SRCCOPY);
        }
    }
    GdiFlush();
}

LRESULT
CWaitWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        HDC hDC = (HDC)wParam;

        RECT r;
        GetClientRect(HWindow, &r);
        FillRect(hDC, &r, (HBRUSH)(COLOR_BTNFACE + 1));

        PaintText(hDC);

        if (ShowProgressBar)
            PaintProgressBar(hDC);

        GdiFlush();

        return TRUE; // background is erased
    }

    case WM_NCHITTEST:
    {
        // prevent window from being moved by dragging the title bar
        // at the same time we will block the tooltip above the Close button
        return HTCLIENT;
    }

    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        HWND hActivate;
        if (HForegroundWnd != NULL)
            hActivate = HForegroundWnd;
        else
            hActivate = MainWindow->HWindow;
        SetForegroundWindow(hActivate);
        SetActiveWindow(hActivate);
        // Let's have one more round...
        SetForegroundWindow(hActivate);
        SetActiveWindow(hActivate);

        // JR: Originally, we were only catching WM_LBUTTONDOWN, but Manison reported that the detection of clicking on the cross from the Automation plugin was not working for him
        // I found out that WM_NCLBUTTONDOWN is triggered when clicking
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_NCLBUTTONDOWN)
        {
            // if the user clicked on the Close button, we set a global variable
            if (CWindow::WindowProc(WM_NCHITTEST, NULL, GetMessagePos()) == HTCLOSE)
                SafeWaitWindowClosePressed = TRUE;
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CConversionTablesDialog
//

CConversionTablesDialog::CConversionTablesDialog(HWND parent, char* dirName)
    : CCommonDialog(HLanguage, IDD_CONVERSION_TABLES, IDD_CONVERSION_TABLES, parent)
{
    HListView = NULL;
    DirName = dirName;
}

CConversionTablesDialog::~CConversionTablesDialog()
{
    CodeTables.FreePreloadedConversions();
}

void CConversionTablesDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        // stretch all available conversions
        CodeTables.PreloadAllConversions();

        HListView = GetDlgItem(HWindow, IDC_CT_LIST);

        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // I will pour the columns Name, Mode, and HotKey into the listview
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_SUBITEM | LVCF_FMT;
        lvc.pszText = LoadStr(IDS_CONVERSION_DESCRIPTION);
        lvc.iSubItem = 0;
        lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(HListView, 0, &lvc);

        lvc.pszText = LoadStr(IDS_CONVERSION_CODEPAGE);
        lvc.iSubItem = 1;
        lvc.fmt = LVCFMT_RIGHT;
        ListView_InsertColumn(HListView, 1, &lvc);

        lvc.pszText = LoadStr(IDS_CONVERSION_PATH);
        lvc.iSubItem = 2;
        lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(HListView, 2, &lvc);

        RECT r;
        GetClientRect(HListView, &r);
        ListView_SetColumnWidth(HListView, 0, r.right / 2.3);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(HListView, 2, LVSCW_AUTOSIZE_USEHEADER);

        int index = 0;
        const char* winCodePage;
        DWORD winCodePageIdentifier;
        const char* winCodePageDescription;
        const char* dirName;
        char buff[MAX_PATH + 120];

        char bestDirName[MAX_PATH];
        CodeTables.GetBestPreloadedConversion(DirName, bestDirName);
        int bestIndex = -1;

        while (CodeTables.EnumPreloadedConversions(&index, &winCodePage, &winCodePageIdentifier,
                                                   &winCodePageDescription, &dirName))
        {
            if (bestIndex == -1 && stricmp(bestDirName, dirName) == 0)
                bestIndex = index - 1;
            LVITEM lvi;
            lvi.mask = 0;
            lvi.iItem = index - 1;
            lvi.iSubItem = 0;
            ListView_InsertItem(HListView, &lvi);

            ListView_SetItemText(HListView, index - 1, 0, (char*)winCodePageDescription);
            sprintf(buff, "%u", winCodePageIdentifier);
            ListView_SetItemText(HListView, index - 1, 1, buff);
            sprintf(buff, "convert\\%s\\convert.cfg", dirName);
            ListView_SetItemText(HListView, index - 1, 2, buff);
        }
        sprintf(buff, "%u", GetACP());
        SetDlgItemText(HWindow, IDC_CT_CODEPAGE, buff);
        if (bestIndex != -1)
        {
            DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItemState(HListView, bestIndex, state, state);
            ListView_EnsureVisible(HListView, bestIndex, FALSE);
        }
    }
    else
    {
        int index = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
        if (index != -1)
        {
            const char* winCodePage;
            DWORD winCodePageIdentifier;
            const char* winCodePageDescription;
            const char* dirName;
            if (CodeTables.EnumPreloadedConversions(&index, &winCodePage, &winCodePageIdentifier,
                                                    &winCodePageDescription, &dirName))
            {
                strcpy(DirName, dirName);
            }
        }
    }
}

INT_PTR
CConversionTablesDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(GetDlgItem(HWindow, IDC_CT_LIST), GetSysColor(COLOR_WINDOW));
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
