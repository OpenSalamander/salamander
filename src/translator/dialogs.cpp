// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "config.h"
#include "dialogs.h"
#include "translator.h"
#include "wndlayt.h"

const char* FILTER_EXECUTABLE = "Translator Project (*.atp)|*.atp|";
const char* FILTER_INCLUDE = "Resource Symbols (*.inc)|*.inc|";
const char* ERROR_TITLE = "Error";

void CenterWindowToWindow(HWND hWnd, HWND hBaseWnd)
{
    // centrujeme k hlavnimu oknu
    RECT r1;
    GetWindowRect(hWnd, &r1);
    int w = r1.right - r1.left;
    int h = r1.bottom - r1.top;
    int x;
    int y;

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, FALSE);

    RECT r2;
    // pri minimalizovanem okne se centrujeme k obrazovce
    if (hBaseWnd != NULL && !IsIconic(hBaseWnd))
    {
        GetWindowRect(hBaseWnd, &r2);
        x = (r2.right + r2.left - w) / 2;
        y = (r2.bottom + r2.top - h) / 2;

        if (x < workArea.left)
            x = workArea.left;
        if (y < workArea.top)
            y = workArea.top;
        if (x + w > workArea.right)
            x = workArea.right - w;
        if (y + h > workArea.bottom)
            y = workArea.bottom - h;
    }
    else
    {
        x = workArea.left + (workArea.right - workArea.left - w) / 2;
        y = workArea.top + (workArea.bottom - workArea.top - h) / 2;
    }
    MoveWindow(hWnd, x, y, w, h, TRUE);
}

//*****************************************************************************
//
// CCommonDialog
//

BOOL CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (CenterToWindow && !IsIconic(HWindow) && !IsZoomed(HWindow))
        {
            // centrujeme k hlavnimu oknu
            RECT r1;
            GetWindowRect(HWindow, &r1);
            int w = r1.right - r1.left;
            int h = r1.bottom - r1.top;
            int x;
            int y;

            RECT workArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, FALSE);

            HWND hFrame = GetMsgParent();

            RECT r2;
            // pri minimalizovanem okne se centrujeme k obrazovce
            if (hFrame != NULL && !IsIconic(hFrame))
            {
                GetWindowRect(hFrame, &r2);
                x = (r2.right + r2.left - w) / 2;
                y = (r2.bottom + r2.top - h) / 2;

                if (x < workArea.left)
                    x = workArea.left;
                if (y < workArea.top)
                    y = workArea.top;
                if (x + w > workArea.right)
                    x = workArea.right - w;
                if (y + h > workArea.bottom)
                    y = workArea.bottom - h;
            }
            else
            {
                x = workArea.left + (workArea.right - workArea.left - w) / 2;
                y = workArea.top + (workArea.bottom - workArea.top - h) / 2;
            }
            MoveWindow(HWindow, x, y, w, h, TRUE);
        }
        else
        {
            // centrujeme k obrazovce
            RECT r;
            GetWindowRect(HWindow, &r);
            RECT workArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, FALSE);
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            int x = workArea.left + (workArea.right - workArea.left - w) / 2;
            int y = workArea.top + (workArea.bottom - workArea.top - h) / 2;
            MoveWindow(HWindow, x, y, w, h, FALSE);
        }
        break;
    }
    }

    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void BrowseFileName(HWND hParent, int editlineResID, const char* filter)
{
    char file[MAX_PATH];
    GetDlgItemText(hParent, editlineResID, file, MAX_PATH);
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hParent;
    char buf[200];
    strcpy_s(buf, filter);
    char* s = buf;
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    //  ofn.lpstrFileTitle = file;
    //  ofn.nMaxFileTitle = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        SendMessage(GetDlgItem(hParent, editlineResID), WM_SETTEXT, 0, (LPARAM)file);
    }
}

//*****************************************************************************
//
// CImportMUIDialog
//

CImportMUIDialog::CImportMUIDialog(HWND parent)
    : CCommonDialog(HInstance, IDD_IMPORTMUI, parent)
{
}

void CImportMUIDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDC_IMPORTMUI_ORIGINAL, Config.LastMUIOriginal, MAX_PATH);
    ti.EditLine(IDC_IMPORTMUI_TRANSLATED, Config.LastMUITranslated, MAX_PATH);
}

BOOL CImportMUIDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {

        if (LOWORD(wParam) == IDC_IMPORTMUI_ORIGINAL_BROWSE || LOWORD(wParam) == IDC_IMPORTMUI_TRANSLATED_BROWSE)
        {
            int resID = (LOWORD(wParam) == IDC_IMPORTMUI_ORIGINAL_BROWSE) ? IDC_IMPORTMUI_ORIGINAL : IDC_IMPORTMUI_TRANSLATED;
            char path[MAX_PATH];
            GetDlgItemText(HWindow, resID, path, MAX_PATH);
            if (GetTargetDirectory(HWindow, "Choose directory", "Choose directory", path, path))
            {
                SetDlgItemText(HWindow, IDC_IMPORTMUI_ORIGINAL, path);
            }

            return TRUE;
        }

        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CNewDialog
//
/*
CNewDialog::CNewDialog(HWND parent)
 : CCommonDialog(HInstance, IDD_NEW, parent)
{
  ProjectFile[0] = 0;
  SourceFile[0] = 0;
  TargetFile[0] = 0;
  IncludeFile[0] = 0;
}

void
CNewDialog::Validate(CTransferInfo &ti)
{
  char errtext[1000];
  char source[MAX_PATH];
  char target[MAX_PATH];
  char include[MAX_PATH];
  ti.EditLine(IDC_NEW_ORIGINAL, source, MAX_PATH);
  ti.EditLine(IDC_NEW_DESTINATION, target, MAX_PATH);
  ti.EditLine(IDC_NEW_INCLUDE, include, MAX_PATH);

  // oba soubory musi existovat a nesmi se jednat o stejny soubor

  // zkusim source otevrit pro cteni
  HANDLE hSource = CreateFile(source, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (hSource != INVALID_HANDLE_VALUE)
  {
    // zkusim target otevrit pro zapis
    HANDLE hTarget = CreateFile(target, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hTarget != INVALID_HANDLE_VALUE)
    {
      CloseHandle(hTarget);

      // include musi jit cist
      HANDLE hInclude = CreateFile(include, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
      if (hInclude != INVALID_HANDLE_VALUE)
      {
        CloseHandle(hInclude);
      }
      else
      {
        DWORD err = GetLastError();
        sprintf_s(errtext, "Error opening Resource Symbols file %s.\n%s", include, GetErrorText(err));
        MessageBox(HWindow, errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_NEW_INCLUDE);
      }
    }
    else
    {
      DWORD err = GetLastError();
      sprintf_s(errtext, "Error opening Translated file %s.\n%s", target, GetErrorText(err));
      MessageBox(HWindow, errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
      ti.ErrorOn(IDC_NEW_DESTINATION);
    }
    CloseHandle(hSource);
  }
  else
  {
    DWORD err = GetLastError();
    sprintf_s(errtext, "Error opening Original file %s.\n%s", source, GetErrorText(err));
    MessageBox(HWindow, errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
    ti.ErrorOn(IDC_NEW_ORIGINAL);
  }
}

void
CNewDialog::Transfer(CTransferInfo &ti)
{
  ti.EditLine(IDC_NEW_PROJECT, ProjectFile, MAX_PATH);
  ti.EditLine(IDC_NEW_ORIGINAL, SourceFile, MAX_PATH);
  ti.EditLine(IDC_NEW_DESTINATION, TargetFile, MAX_PATH);
  ti.EditLine(IDC_NEW_INCLUDE, IncludeFile, MAX_PATH);
}

BOOL
CNewDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_COMMAND:
    {
      if (LOWORD(wParam) == IDC_NEW_PROJECT_BROWSE)
      {
        char fileName[MAX_PATH];
        fileName[0] = 0;
        GetDlgItemText(HWindow, IDC_NEW_PROJECT, fileName, MAX_PATH);
        OPENFILENAME ofn;
        memset(&ofn, 0, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = HWindow;
        ofn.lpstrFilter = "Translator Project (*.atp)\0*.atp\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = "New Project";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
        if (GetSaveFileName(&ofn))
        {
          SetDlgItemText(HWindow, IDC_NEW_PROJECT, fileName);
        }
      }

      if (LOWORD(wParam) == IDC_NEW_ORIGINAL_BROWSE)
      {
        BrowseFileName(HWindow, IDC_NEW_ORIGINAL, FILTER_EXECUTABLE);
        return TRUE;
      }

      if (LOWORD(wParam) == IDC_NEW_DESTINATION_BROWSE)
      {
        BrowseFileName(HWindow, IDC_NEW_DESTINATION, FILTER_EXECUTABLE);
        return TRUE;
      }

      if (LOWORD(wParam) == IDC_NEW_INCLUDE_BROWSE)
      {
        BrowseFileName(HWindow, IDC_NEW_INCLUDE, FILTER_INCLUDE);
        return TRUE;
      }
      break;
    }
  }

  return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
*/
//*****************************************************************************
//
// CPropertiesDialog
//

CLocales Locales;

BOOL CALLBACK EnumLocalesProc(LPTSTR lpLocaleString)
{
    DWORD locale;
    sscanf_s(lpLocaleString, "%x", &locale);
    Locales.Items.Add((WORD)locale);
    return TRUE;
}

CLocales::CLocales()
    : Items(100, 50)
{
}

void CLocales::EnumLocales()
{
    if (Items.Count == 0)
        EnumSystemLocales(EnumLocalesProc, LCID_SUPPORTED);
}

void CLocales::FillComboBox(HWND hCombo)
{
    char buff[500];
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        GetLocaleInfo(MAKELCID(Items[i], SORT_DEFAULT), LOCALE_SLANGUAGE, buff, 500);
        SPrintFToEnd_s(buff, " - 0x%04x", Items[i]);
        int index = SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buff);
        SendMessage(hCombo, CB_SETITEMDATA, index, Items[i]);
    }
    for (i = 0; i < Items.Count; i++)
    {
        int lang = SendMessage(hCombo, CB_GETITEMDATA, i, 0);
        if (lang == Data.SLGSignature.LanguageID)
        {
            SendMessage(hCombo, CB_SETCURSEL, i, 0);
            break;
        }
    }
}

CPropertiesDialog::CPropertiesDialog(HWND parent)
    : CCommonDialog(HInstance, IDD_PROPERTIES, parent)
{
    Locales.EnumLocales();
}

void CTransferInfo_EditLineW(CTransferInfo* ti, int ctrlID, wchar_t* buffer, DWORD bufferSizeInChars, BOOL select = TRUE)
{
    HWND HWindow;
    if (ti->GetControl(HWindow, ctrlID))
    {
        switch (ti->Type)
        {
        case ttDataToWindow:
        {
            SendMessageW(HWindow, EM_LIMITTEXT, bufferSizeInChars - 1, 0);
            SendMessageW(HWindow, WM_SETTEXT, 0, (LPARAM)buffer);
            if (select)
                SendMessageW(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessageW(HWindow, WM_GETTEXT, bufferSizeInChars, (LPARAM)buffer);
            break;
        }
        }
    }
}

void CPropertiesDialog::Validate(CTransferInfo& ti)
{
    wchar_t buff[500];
    CTransferInfo_EditLineW(&ti, IDC_PRP_AUTHOR, buff, 500);
    if (lstrlenW(buff) == 0)
    {
        MessageBox(HWindow, "Empty string is not allowed here.", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_PRP_AUTHOR);
    }

    CTransferInfo_EditLineW(&ti, IDC_PRP_WEB, buff, 500);
    if (lstrlenW(buff) == 0)
    {
        MessageBox(HWindow, "Empty string is not allowed here.", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_PRP_WEB);
    }

    CTransferInfo_EditLineW(&ti, IDC_PRP_COMMENT, buff, 500);

    if (Data.SLGSignature.HelpDirExist)
    {
        CTransferInfo_EditLineW(&ti, IDC_PRP_HELPDIR, buff, 100);
        if (lstrlenW(buff) == 0)
        {
            MessageBox(HWindow, "Empty string is not allowed here.", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_PRP_HELPDIR);
        }
    }
}

void CPropertiesDialog::Transfer(CTransferInfo& ti)
{
    HWND hCombo = GetDlgItem(HWindow, IDC_PRP_LOCALE);
    if (ti.Type == ttDataToWindow)
    {
        Locales.FillComboBox(hCombo);
    }
    else
    {
        int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
        Data.SLGSignature.LanguageID = (WORD)SendMessage(hCombo, CB_GETITEMDATA, index, 0);
        Data.SetDirty();
        Data.SLGSignature.SLTDataChanged();
    }
    CTransferInfo_EditLineW(&ti, IDC_PRP_AUTHOR, Data.SLGSignature.Author, 500);
    CTransferInfo_EditLineW(&ti, IDC_PRP_WEB, Data.SLGSignature.Web, 500);
    CTransferInfo_EditLineW(&ti, IDC_PRP_COMMENT, Data.SLGSignature.Comment, 500);
    if (Data.SLGSignature.HelpDirExist)
        CTransferInfo_EditLineW(&ti, IDC_PRP_HELPDIR, Data.SLGSignature.HelpDir, 100);
    else
        EnableWindow(GetDlgItem(HWindow, IDC_PRP_HELPDIR), FALSE);
    if (Data.SLGSignature.SLGIncompleteExist)
        CTransferInfo_EditLineW(&ti, IDC_PRP_SLGINCOMPLETE, Data.SLGSignature.SLGIncomplete, 200);
    else
        EnableWindow(GetDlgItem(HWindow, IDC_PRP_SLGINCOMPLETE), FALSE);
}

BOOL CPropertiesDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CFindDialog
//

CFindDialog::CFindDialog(HWND parent)
    : CCommonDialog(HInstance, IDD_FIND, parent)
{
}

void CFindDialog::Validate(CTransferInfo& ti)
{
}

void CFindDialog::Transfer(CTransferInfo& ti)
{
    wchar_t** history = Config.FindHistory;
    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_FIND_FIND))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
            SendMessage(hWnd, CB_LIMITTEXT, 1000 - 1, 0);
            for (int i = 0; i < FIND_HISTORY_SIZE; i++)
                if (history[i] != NULL)
                    SendMessageW(hWnd, CB_ADDSTRING, 0, (LPARAM)history[i]);
            if (history[0] != NULL)
                SendMessageW(hWnd, WM_SETTEXT, 0, (LPARAM)history[0]);
        }
        else
        {
            wchar_t buff[1000];
            SendMessageW(hWnd, WM_GETTEXT, 1000, (LPARAM)buff);
            int from = FIND_HISTORY_SIZE - 1;
            int i;
            for (i = 0; i < FIND_HISTORY_SIZE; i++)
                if (history[i] != NULL)
                    if (wcscmp(history[i], buff) == 0)
                    {
                        from = i;
                        break;
                    }
            if (from > 0)
            {
                int len = wcslen(buff) + 1;
                wchar_t* text = (wchar_t*)malloc(len * sizeof(wchar_t));
                if (text != NULL)
                {
                    free(history[from]);
                    for (i = from - 1; i >= 0; i--)
                        history[i + 1] = history[i];
                    history[0] = text;
                    wcscpy_s(history[0], len, buff);
                }
            }
        }
    }

    ti.CheckBox(IDC_FIND_TEXTS, Config.FindTexts);
    ti.CheckBox(IDC_FIND_SYMBOLS, Config.FindSymbols);
    ti.CheckBox(IDC_FIND_ORIGINAL, Config.FindOriginal);
    ti.CheckBox(IDC_FIND_WORDS, Config.FindWords);
    ti.CheckBox(IDC_FIND_CASE, Config.FindCase);
    ti.CheckBox(IDC_FIND_IGNOREAMPERSAND, Config.FindIgnoreAmpersand);
    ti.CheckBox(IDC_FIND_IGNOREDASH, Config.FindIgnoreDash);

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CFindDialog::EnableControls()
{
    BOOL texts = IsDlgButtonChecked(HWindow, IDC_FIND_TEXTS) == BST_CHECKED;
    BOOL symbols = IsDlgButtonChecked(HWindow, IDC_FIND_SYMBOLS) == BST_CHECKED;
    BOOL textLen = GetWindowTextLength(GetDlgItem(HWindow, IDC_FIND_FIND));

    EnableWindow(GetDlgItem(HWindow, IDOK), (texts | symbols) && textLen > 0);
}

BOOL CFindDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_FIND_TEXTS || LOWORD(wParam) == IDC_FIND_SYMBOLS ||
            LOWORD(wParam) == IDC_FIND_IGNOREAMPERSAND || LOWORD(wParam) == IDC_FIND_IGNOREDASH)
        {
            EnableControls();
            return 0;
        }

        if (LOWORD(wParam) == IDC_FIND_FIND && HIWORD(wParam) == CBN_EDITCHANGE)
        {
            EnableControls();
            return 0;
        }

        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// COptionsDialog
//

COptionsDialog::COptionsDialog(HWND parent)
    : CCommonDialog(HInstance, IDD_OPTIONS, parent)
{
}

void COptionsDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_OPTIONS_RELOAD, Config.ReloadProjectAtStartup);
}

//*****************************************************************************
//
// CImportDialog
//

CImportDialog ::CImportDialog(HWND parent, char* project)
    : CCommonDialog(HInstance, IDD_IMPORT, parent)
{
    Project = project;
}

void CImportDialog ::Validate(CTransferInfo& ti)
{
    char errtext[1000];
    char project[MAX_PATH];
    ti.EditLine(IDC_IMPORT_PROJECT, project, MAX_PATH);

    // zkusim project otevrit pro cteni
    HANDLE hProject = CreateFile(project, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hProject != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hProject);
    }
    else
    {
        DWORD err = GetLastError();
        sprintf_s(errtext, "Error opening project file %s.\n%s", project, GetErrorText(err));
        MessageBox(HWindow, errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_NEW_ORIGINAL);
    }

    /*
  // oba soubory musi existovat a nesmi se jednat o stejny soubor
  // zkusim source otevrit pro cteni
  HANDLE hSource = CreateFile(original, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (hSource != INVALID_HANDLE_VALUE)
  {
    // zkusim target otevrit pro zapis
    HANDLE hTarget = CreateFile(translated, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hTarget != INVALID_HANDLE_VALUE)
    {
      CloseHandle(hTarget);
    }
    else
    {
      DWORD err = GetLastError();
      sprintf_s(errtext, "Error opening translated file %s.\n%s", translated, GetErrorText(err));
      MessageBox(HWindow, errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
      ti.ErrorOn(IDC_NEW_DESTINATION);
    }
    CloseHandle(hSource);
  }
  else
  {
    DWORD err = GetLastError();
    sprintf_s(errtext, "Error opening original file %s.\n%s", original, GetErrorText(err));
    MessageBox(HWindow, errtext, ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
    ti.ErrorOn(IDC_NEW_ORIGINAL);
  }
  */
}

void CImportDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDC_IMPORT_PROJECT, Project, MAX_PATH);
}

BOOL CImportDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_IMPORT_PROJECT_BROWSE)
        {
            BrowseFileName(HWindow, IDC_IMPORT_PROJECT, FILTER_EXECUTABLE);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CValidateDialog
//

CValidateDialog::CValidateDialog(HWND parent)
    : CCommonDialog(HInstance, IDD_VALIDATE, parent)
{
}

struct CValidateDlgItems
{
    int ValID;
    BOOL* Value;
};

CValidateDlgItems ValidateDlgItems[] =
    {
        {IDC_VAL_DLGHOTKEYSCONFLICT, &Config.ValidateDlgHotKeyConflicts},
        {IDC_VAL_MENUHOTKEYSCONFLICT, &Config.ValidateMenuHotKeyConflicts},
        {IDC_VAL_PRINTFSPECIFIER, &Config.ValidatePrintfSpecifier},
        {IDC_VAL_CONTROLCHARS, &Config.ValidateControlChars},
        {IDC_VAL_STRINGSWITHCSV, &Config.ValidateStringsWithCSV},
        {IDC_VAL_PLURALSTR, &Config.ValidatePluralStr},
        {IDC_VAL_TEXTENDING, &Config.ValidateTextEnding},
        {IDC_VAL_HOTKEYS, &Config.ValidateHotKeys},
        {IDC_VAL_LAYOUT, &Config.ValidateLayout},
        {IDC_VAL_CLIPPING, &Config.ValidateClipping},
        {IDC_VAL_CONTROLTODLG, &Config.ValidateControlsToDialog},
        {IDC_VAL_MISALIGNED, &Config.ValidateMisalignedControls},
        {IDC_VAL_DIFSIZED, &Config.ValidateDifferentSizedControls},
        {IDC_VAL_DIFSPACING, &Config.ValidateDifferentSpacedControls},
        {IDC_VAL_STANDARDSIZE, &Config.ValidateStandardControlsSize},
        {IDC_VAL_LABELSSPACING, &Config.ValidateLabelsToControlsSpacing},
        {IDC_VAL_SIZESOFPROPPAGES, &Config.ValidateSizesOfPropPages},
        {IDC_VAL_DIALOGCONTROLSID, &Config.ValidateDialogControlsID},
        {IDC_VAL_HBUTTONSPACING, &Config.ValidateHButtonSpacing},
        {-1, NULL} // TERMINATOR
};

void CValidateDialog::Transfer(CTransferInfo& ti)
{
    for (int i = 0; ValidateDlgItems[i].Value != NULL; i++)
        ti.CheckBox(ValidateDlgItems[i].ValID, *ValidateDlgItems[i].Value);
}

void CValidateDialog::EnableControls()
{
    BOOL test = FALSE;
    for (int i = 0; ValidateDlgItems[i].Value != NULL; i++)
        test |= IsDlgButtonChecked(HWindow, ValidateDlgItems[i].ValID) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDOK), test);
}

BOOL CValidateDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_CHECKALL || LOWORD(wParam) == IDC_UNCHECKALL)
        {
            CTransferInfo ti(HWindow, ttDataToWindow);
            int val = LOWORD(wParam) == IDC_CHECKALL ? 1 : 0;
            for (int i = 0; ValidateDlgItems[i].Value != NULL; i++)
                ti.CheckBox(ValidateDlgItems[i].ValID, val);
            EnableControls();
        }
        for (int i = 0; ValidateDlgItems[i].Value != NULL; i++)
        {
            if (LOWORD(wParam) == ValidateDlgItems[i].ValID)
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

//*****************************************************************************
//
// CValidateDialog
//

CAgreementDialog::CAgreementDialog(HWND parent)
    : CCommonDialog(HInstance, IDD_AGREEMENT, parent)
{
    Agreement = NULL;
    char fileName[MAX_PATH];
    GetModuleFileName(HInstance, fileName, MAX_PATH);
    *(strrchr(fileName, '\\') + 1) = 0;
    strcat_s(fileName, "license.txt");
    HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ,
                                        FILE_SHARE_READ, NULL,
                                        OPEN_EXISTING,
                                        FILE_FLAG_SEQUENTIAL_SCAN,
                                        NULL));
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD size = GetFileSize(hFile, NULL);
        if (size > 0 && size < 10000)
        {
            Agreement = (char*)malloc(size + 1);
            if (Agreement != NULL)
            {
                DWORD read;
                if (ReadFile(hFile, Agreement, size, &read, NULL) && read == size)
                {
                    Agreement[size] = 0;
                }
                else
                {
                    free(Agreement);
                    Agreement = NULL;
                }
            }
        }
        HANDLES(CloseHandle(hFile));
    }
}

CAgreementDialog::~CAgreementDialog()
{
    if (Agreement != NULL)
        free(Agreement);
}

void CenterWindow(HWND hWindow)
{
    RECT masterRect;
    RECT slaveRect;
    int x, y, w, h;
    int mw, mh;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &masterRect, 0);

    GetWindowRect(hWindow, &slaveRect);
    w = slaveRect.right - slaveRect.left;
    h = slaveRect.bottom - slaveRect.top;
    mw = masterRect.right - masterRect.left;
    mh = masterRect.bottom - masterRect.top;
    x = masterRect.left + (mw - w) / 2;
    y = masterRect.top + (mh - h) / 2;
    SetWindowPos(hWindow, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void CAgreementDialog::EnableControls()
{
    BOOL enable = IsDlgButtonChecked(HWindow, IDC_I_AGREE) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDOK), enable);
}

BOOL CAgreementDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CenterWindow(HWindow);

        SetFocus(GetDlgItem(HWindow, IDC_I_AGREE));

        EnableControls();

        if (Agreement != NULL)
            SetWindowText(GetDlgItem(HWindow, IDC_LICENSE), Agreement);
        break;
    }

    case WM_COMMAND:
    {
        EnableControls();
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CAlignToDialog
//

void SetSmallDialogPosition(HWND hDlg, HWND hEditedDlg)
{
    // dialog umistime pod editovany dialog a pokud jsme mimo obrazovku, tak vpravo od nej
    RECT r;
    GetWindowRect(hDlg, &r);
    RECT editedR;
    GetWindowRect(hEditedDlg, &editedR);
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, FALSE);
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int x;
    int y;
    if (editedR.bottom + h < workArea.bottom)
    {
        x = editedR.left;
        y = editedR.bottom;
    }
    else
    {
        x = editedR.right;
        y = editedR.top;
    }
    MoveWindow(hDlg, x, y, w, h, FALSE);
}

CAlignToDialog::CAlignToDialog(HWND parent, HWND hEditedDlg, CAlignToParams* params, CLayoutEditor* layoutEditor)
    : CDialog(HInstance, IDD_ALIGNTO, parent)
{
    HEditedDlg = hEditedDlg;
    Params = params;
    LayoutEditor = layoutEditor;
}

void CAlignToDialog::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_AT_SEL_MOVE, atoeMove, (int&)Params->Operation);
    ti.RadioButton(IDC_AT_SEL_RESIZE, atoeResize, (int&)Params->Operation);
    ti.CheckBox(IDC_AT_SEL_GROUP, (int&)Params->MoveAsGroup);

    ti.RadioButton(IDC_AT_SEL_TOP, atpeTop, (int&)Params->SelPart);
    ti.RadioButton(IDC_AT_SEL_RIGHT, atpeRight, (int&)Params->SelPart);
    ti.RadioButton(IDC_AT_SEL_BOTTOM, atpeBottom, (int&)Params->SelPart);
    ti.RadioButton(IDC_AT_SEL_LEFT, atpeLeft, (int&)Params->SelPart);
    ti.RadioButton(IDC_AT_SEL_HC, atpeHCenter, (int&)Params->SelPart);
    ti.RadioButton(IDC_AT_SEL_VC, atpeVCenter, (int&)Params->SelPart);

    ti.RadioButton(IDC_AT_REF_TOP, atpeTop, (int&)Params->RefPart);
    ti.RadioButton(IDC_AT_REF_RIGHT, atpeRight, (int&)Params->RefPart);
    ti.RadioButton(IDC_AT_REF_BOTTOM, atpeBottom, (int&)Params->RefPart);
    ti.RadioButton(IDC_AT_REF_LEFT, atpeLeft, (int&)Params->RefPart);
    ti.RadioButton(IDC_AT_REF_HC, atpeHCenter, (int&)Params->RefPart);
    ti.RadioButton(IDC_AT_REF_VC, atpeVCenter, (int&)Params->RefPart);

    if (ti.Type == ttDataToWindow)
    {
        OnChange();
        EnableControls();
    }
}

void CAlignToDialog::EnableChildWindow(int id, BOOL enabled)
{
    if (!enabled)
        CheckDlgButton(HWindow, id, BST_UNCHECKED);
    EnableWindow(GetDlgItem(HWindow, id), enabled);
}

void CAlignToDialog::EnableControls()
{
    BOOL move = IsDlgButtonChecked(HWindow, IDC_AT_SEL_MOVE) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDC_AT_SEL_GROUP), move);
    BOOL vertical = IsDlgButtonChecked(HWindow, IDC_AT_SEL_TOP) == BST_CHECKED ||
                    IsDlgButtonChecked(HWindow, IDC_AT_SEL_BOTTOM) == BST_CHECKED ||
                    IsDlgButtonChecked(HWindow, IDC_AT_SEL_VC) == BST_CHECKED;
    BOOL horizontal = IsDlgButtonChecked(HWindow, IDC_AT_SEL_RIGHT) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_LEFT) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_HC) == BST_CHECKED;
    EnableChildWindow(IDC_AT_SEL_VC, move);
    EnableChildWindow(IDC_AT_SEL_HC, move);
    BOOL selChecked = IsDlgButtonChecked(HWindow, IDC_AT_SEL_TOP) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_RIGHT) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_BOTTOM) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_LEFT) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_VC) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_SEL_HC) == BST_CHECKED;
    if (!selChecked)
    {
        CheckDlgButton(HWindow, IDC_AT_SEL_TOP, BST_CHECKED);
        vertical = TRUE;
    }
    EnableChildWindow(IDC_AT_REF_TOP, vertical);
    EnableChildWindow(IDC_AT_REF_BOTTOM, vertical);
    EnableChildWindow(IDC_AT_REF_VC, vertical);
    EnableChildWindow(IDC_AT_REF_RIGHT, horizontal);
    EnableChildWindow(IDC_AT_REF_LEFT, horizontal);
    EnableChildWindow(IDC_AT_REF_HC, horizontal);
    BOOL refChecked = IsDlgButtonChecked(HWindow, IDC_AT_REF_TOP) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_REF_RIGHT) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_REF_BOTTOM) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_REF_LEFT) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_REF_VC) == BST_CHECKED ||
                      IsDlgButtonChecked(HWindow, IDC_AT_REF_HC) == BST_CHECKED;
    if (!refChecked)
    {
        CheckDlgButton(HWindow, IDC_AT_REF_TOP, IsDlgButtonChecked(HWindow, IDC_AT_SEL_TOP));
        CheckDlgButton(HWindow, IDC_AT_REF_RIGHT, IsDlgButtonChecked(HWindow, IDC_AT_SEL_RIGHT));
        CheckDlgButton(HWindow, IDC_AT_REF_BOTTOM, IsDlgButtonChecked(HWindow, IDC_AT_SEL_BOTTOM));
        CheckDlgButton(HWindow, IDC_AT_REF_LEFT, IsDlgButtonChecked(HWindow, IDC_AT_SEL_LEFT));
        CheckDlgButton(HWindow, IDC_AT_REF_VC, IsDlgButtonChecked(HWindow, IDC_AT_SEL_VC));
        CheckDlgButton(HWindow, IDC_AT_REF_HC, IsDlgButtonChecked(HWindow, IDC_AT_SEL_HC));
    }
}

void CAlignToDialog::OnChange()
{
    EnableControls();
    TransferData(ttDataFromWindow);
    LayoutEditor->ApplyAlignTo(Params);
}

BOOL CAlignToDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_AT_SEL_MOVE ||
            LOWORD(wParam) == IDC_AT_SEL_RESIZE ||
            LOWORD(wParam) == IDC_AT_SEL_GROUP ||
            LOWORD(wParam) == IDC_AT_SEL_TOP ||
            LOWORD(wParam) == IDC_AT_SEL_RIGHT ||
            LOWORD(wParam) == IDC_AT_SEL_BOTTOM ||
            LOWORD(wParam) == IDC_AT_SEL_LEFT ||
            LOWORD(wParam) == IDC_AT_SEL_HC ||
            LOWORD(wParam) == IDC_AT_SEL_VC ||
            LOWORD(wParam) == IDC_AT_REF_TOP ||
            LOWORD(wParam) == IDC_AT_REF_RIGHT ||
            LOWORD(wParam) == IDC_AT_REF_BOTTOM ||
            LOWORD(wParam) == IDC_AT_REF_LEFT ||
            LOWORD(wParam) == IDC_AT_REF_HC ||
            LOWORD(wParam) == IDC_AT_REF_VC)
        {
            OnChange();
            return TRUE;
        }
        break;
    }

    case WM_INITDIALOG:
    {
        SetSmallDialogPosition(HWindow, HEditedDlg);
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CSetSizeDialog
//

CSetSizeDialog::CSetSizeDialog(HWND parent, HWND hEditedDlg, CLayoutEditor* layoutEditor, int* width, int* height)
    : CDialog(HInstance, IDD_SETSIZE, parent)
{
    HEditedDlg = hEditedDlg;
    LayoutEditor = layoutEditor;
    Width = width;
    Height = height;
}

void CSetSizeDialog::Validate(CTransferInfo& ti)
{
    BOOL w;
    ti.CheckBox(IDC_SETSIZE_WIDTH, w);
    int wedit;
    ti.EditLine(IDC_SETSIZE_WIDTH_EL, wedit);
    if (w && wedit < 1)
    {
        MessageBox(HWindow, "Enter valid width value.", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_SETSIZE_WIDTH_EL);
    }

    if (ti.IsGood())
    {
        BOOL h;
        ti.CheckBox(IDC_SETSIZE_HEIGHT, h);
        int hedit;
        ti.EditLine(IDC_SETSIZE_HEIGHT_EL, hedit);
        if (h && hedit < 1)
        {
            MessageBox(HWindow, "Enter valid height value.", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_SETSIZE_HEIGHT_EL);
        }
    }
}

void CSetSizeDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataFromWindow)
    {
        BOOL w;
        ti.CheckBox(IDC_SETSIZE_WIDTH, w);
        if (w)
            ti.EditLine(IDC_SETSIZE_WIDTH_EL, *Width);
        else
            *Width = -1;
        BOOL h;
        ti.CheckBox(IDC_SETSIZE_HEIGHT, h);
        if (h)
            ti.EditLine(IDC_SETSIZE_HEIGHT_EL, *Height);
        else
            *Height = -1;
    }

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CSetSizeDialog::EnableChildWindow(int id, BOOL enabled)
{
    HWND hChild = GetDlgItem(HWindow, id);
    if (!enabled)
        SendMessage(hChild, WM_SETTEXT, 0, (LPARAM) "");
    EnableWindow(hChild, enabled);
}

void CSetSizeDialog::EnableControls()
{
    BOOL w = IsDlgButtonChecked(HWindow, IDC_SETSIZE_WIDTH) == BST_CHECKED;
    BOOL h = IsDlgButtonChecked(HWindow, IDC_SETSIZE_HEIGHT) == BST_CHECKED;
    EnableChildWindow(IDC_SETSIZE_WIDTH_EL, w);
    EnableChildWindow(IDC_SETSIZE_HEIGHT_EL, h);
}

BOOL CSetSizeDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetSmallDialogPosition(HWindow, HEditedDlg);
        break;
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDC_SETSIZE_WIDTH)
        {
            EnableControls();
            if (IsDlgButtonChecked(HWindow, IDC_SETSIZE_WIDTH) == BST_CHECKED)
                SetFocus(GetDlgItem(HWindow, IDC_SETSIZE_WIDTH_EL));
        }
        if (LOWORD(wParam) == IDC_SETSIZE_HEIGHT)
        {
            EnableControls();
            if (IsDlgButtonChecked(HWindow, IDC_SETSIZE_HEIGHT) == BST_CHECKED)
                SetFocus(GetDlgItem(HWindow, IDC_SETSIZE_HEIGHT_EL));
        }
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}
