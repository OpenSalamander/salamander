// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

void CenterWindow(HWND hWindow, HWND hParent)
{
    // horizontally and vertically center the dialog relative to its parent
    if (hParent != NULL)
        SalGeneral->MultiMonCenterWindow(hWindow, hParent, TRUE);
}

//****************************************************************************
//
// InternetProc
//

WORD* InetConnectionType;
WORD* InetProtocolType;
int InetConectionResID[inetCount] = {IDC_INET_PHONE, IDC_INET_LAN, IDC_INET_NONE};
int InetProtocolResID[inetpCount] = {IDC_INET_HTTP, IDC_INET_FTP, IDC_INET_FTP_PASSIVE};
BOOL DoNotUseFTPShown = FALSE;

INT_PTR CALLBACK InternetProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("InternetProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // SalamanderGUI->ArrangeHorizontalLines(hWindow); // should be called, but we skip it here, there are no horizontal lines

        SendDlgItemMessage(hWindow, IDC_INTERNET_ICON, STM_SETIMAGE, IMAGE_ICON,
                           (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_INTERNET)));

        InetConnectionType = (WORD*)lParam;
        InetProtocolType = ((WORD*)lParam) + 1;
        // horizontally and vertically center the dialog relative to its parent
        CenterWindow(hWindow, GetParent(hWindow));

        int i;
        for (i = 0; i < inetCount; i++)
            CheckDlgButton(hWindow, InetConectionResID[i], *InetConnectionType == i ? BST_CHECKED : BST_UNCHECKED);
        for (i = 0; i < inetpCount; i++)
            CheckDlgButton(hWindow, InetProtocolResID[i], *InetProtocolType == i ? BST_CHECKED : BST_UNCHECKED);
        PostMessage(hWindow, WM_COMMAND, MAKELPARAM(0, BN_CLICKED), 0); // enable/disable controls
        break;
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_INTERNET, FALSE);
        return TRUE; // never let F1 reach the parent
    }

    case WM_APP + 55:
    {
        if (!DoNotUseFTPShown)
        {
            DoNotUseFTPShown = TRUE;
            if (SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_DONOTUSEFTP), LoadStr(IDS_PLUGINNAME),
                                          MB_ICONQUESTION | MB_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT) == IDYES)
            {
                CheckDlgButton(hWindow, IDC_INET_FTP, BST_UNCHECKED);
                CheckDlgButton(hWindow, IDC_INET_FTP_PASSIVE, BST_UNCHECKED);
                CheckDlgButton(hWindow, IDC_INET_HTTP, BST_CHECKED);
                SendMessage(hWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hWindow, IDC_INET_HTTP), TRUE);
            }
            DoNotUseFTPShown = FALSE;
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            BOOL enableProtocol = IsDlgButtonChecked(hWindow, IDC_INET_NONE) != BST_CHECKED;
            int i;
            for (i = 0; i < inetpCount; i++)
                EnableWindow(GetDlgItem(hWindow, InetProtocolResID[i]), enableProtocol);

            if (LOWORD(wParam) == IDC_INET_FTP || LOWORD(wParam) == IDC_INET_FTP_PASSIVE)
            { // clicking FTP triggers a warning that plugin updates are delivered only over HTTP; if HTTP really fails, otherwise use it
                PostMessage(hWindow, WM_APP + 55, 0, 0);
            }
        }
        switch (LOWORD(wParam))
        {
        case IDHELP:
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_INTERNET, FALSE);
            return TRUE;
        }

        case IDOK:
        {
            // retrieve data from the dialog
            int i;
            for (i = 0; i < inetCount; i++)
                if (IsDlgButtonChecked(hWindow, InetConectionResID[i]) == BST_CHECKED)
                    *InetConnectionType = i;
            for (i = 0; i < inetpCount; i++)
                if (IsDlgButtonChecked(hWindow, InetProtocolResID[i]) == BST_CHECKED)
                    *InetProtocolType = i;
        }
        case IDCANCEL:
        {
            HConfigurationDialog = NULL;
            EndDialog(hWindow, wParam);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE; // not processed
}

BOOL OpenInternetDialog(HWND hParent, CInternetConnection* internetConnection, CInternetProtocol* internetProtocol)
{
    DWORD param = MAKELPARAM(*internetConnection, *internetProtocol);
    BOOL ret = DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_INTERNET), hParent,
                              InternetProc, (LPARAM)&param) == IDOK;
    if (ret)
    {
        *internetConnection = (CInternetConnection)LOWORD(param);
        *internetProtocol = (CInternetProtocol)HIWORD(param);
    }

    return ret;
}

//****************************************************************************
//
// CfgDlgProc
//

void OnConfiguration(HWND hParent)
{
    CALL_STACK_MESSAGE1("OnConfiguration()");

    // the configuration dialog must not already be open
    if (HConfigurationDialog != NULL)
    {
        SalGeneral->SalMessageBox(hParent,
                                  LoadStr(IDS_CFG_CONFLICT1), LoadStr(IDS_PLUGINNAME),
                                  MB_ICONINFORMATION | MB_OK);
        return;
    }

    // the connection thread must not be running
    if (HDownloadThread != NULL)
    {
        ShowMinNA_IfNotShownYet(HMainDialog, TRUE, FALSE); // flash the window so the user notices it
        SalGeneral->SalMessageBox(hParent, LoadStr(IDS_CFG_CONFLICT3),
                                  LoadStr(IDS_PLUGINNAME),
                                  MB_ICONINFORMATION | MB_OK);
        return;
    }

    if (DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CONFIGURATION), hParent, CfgDlgProc, NULL) == IDOK)
    {
        ConfigurationChanged = TRUE;
    }
}

int AutoCheckModeID[achmCount] = {IDC_CFG_NEVER, IDC_CFG_DAY, IDC_CFG_WEEK, IDC_CFG_MONTH, IDC_CFG_3MONTHS, IDC_CFG_6MONTHS};
CAutoCheckModeEnum AutoCheckModeEval[achmCount] = {achmNever, achmDay, achmWeek, achmMonth, achm3Month, achm6Month};
CInternetConnection CfgInternetConnection;
CInternetProtocol CfgInternetProtocol;
CDataDefaults CfgData;

void EnableControls(HWND hWindow)
{
    // helper that toggles the checkbox states
    CAutoCheckModeEnum mode = achmNever;
    int i;
    for (i = 0; i < achmCount; i++)
        if (IsDlgButtonChecked(hWindow, AutoCheckModeID[i]) == BST_CHECKED)
            mode = AutoCheckModeEval[i];

    for (i = 0; i < achmCount; i++)
        EnableWindow(GetDlgItem(hWindow, AutoCheckModeID[i]), CfgInternetConnection != inetNone);

    BOOL autoConnectWasEnabled = IsWindowEnabled(GetDlgItem(hWindow, IDC_CFG_AUTOCONNECT));
    EnableWindow(GetDlgItem(hWindow, IDC_CFG_AUTOCONNECT), mode != achmNever);
    if (!(mode != achmNever))
        CheckDlgButton(hWindow, IDC_CFG_AUTOCONNECT, BST_UNCHECKED);
    if (mode != achmNever && !autoConnectWasEnabled) // enable the "silent" mode by default
        CheckDlgButton(hWindow, IDC_CFG_AUTOCONNECT, BST_CHECKED);
    BOOL autoCloseWasEnabled = IsWindowEnabled(GetDlgItem(hWindow, IDC_CFG_AUTOCLOSE));
    BOOL closeEnabled = IsDlgButtonChecked(hWindow, IDC_CFG_AUTOCONNECT) == BST_CHECKED;
    EnableWindow(GetDlgItem(hWindow, IDC_CFG_AUTOCLOSE), mode != achmNever && closeEnabled);
    if (!(mode != achmNever && closeEnabled))
        CheckDlgButton(hWindow, IDC_CFG_AUTOCLOSE, BST_UNCHECKED);
    if (mode != achmNever && (!autoConnectWasEnabled || !autoCloseWasEnabled && closeEnabled)) // enable the "silent" mode by default
        CheckDlgButton(hWindow, IDC_CFG_AUTOCLOSE, BST_CHECKED);

    // enable/disable logic for the list box
    HWND hListBox = GetDlgItem(hWindow, IDC_CFG_FILTER);
    int index = (int)SendMessage(hListBox, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR)
    {
        HWND hFocus = GetFocus();
        if (hFocus == hListBox || hFocus == GetDlgItem(hWindow, IDC_CFG_REMOVE))
        {
            SetFocus(GetDlgItem(hWindow, IDOK));
            SendMessage(hWindow, DM_SETDEFID, IDOK, 0);
            SendMessage(GetDlgItem(hWindow, IDC_CFG_REMOVE), BM_SETSTYLE,
                        BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
        }
        EnableWindow(GetDlgItem(hWindow, IDC_CFG_REMOVE), FALSE);
        EnableWindow(hListBox, FALSE);
    }
}

void LoadCfgDlgControls(HWND hWindow)
{
    // populate the dialog with data
    int resID = 0;
    switch (CfgInternetConnection)
    {
    case inetPhone:
        resID = IDS_INTERNET_PHONE;
        break;
    case inetLAN:
        resID = IDS_INTERNET_LAN;
        break;
    case inetNone:
        resID = IDS_INTERNET_NONE;
        break;
    default:
        TRACE_E("InternetConnection=" << InternetConnection);
        break;
    }
    SetDlgItemText(hWindow, IDC_CFG_INTERNET, LoadStr(resID));

    int i;
    for (i = 0; i < achmCount; i++)
        CheckDlgButton(hWindow, AutoCheckModeID[i],
                       AutoCheckModeEval[i] == CfgData.AutoCheckMode ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWindow, IDC_CFG_AUTOCONNECT, CfgData.AutoConnect ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWindow, IDC_CFG_AUTOCLOSE, CfgData.AutoClose ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWindow, IDC_CFG_BETA, CfgData.CheckBetaVersion ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWindow, IDC_CFG_PB, CfgData.CheckPBVersion ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWindow, IDC_CFG_RELEASE, CfgData.CheckReleaseVersion ? BST_CHECKED : BST_UNCHECKED);

    // fill in the filter items
    FiltersFillListBox(GetDlgItem(hWindow, IDC_CFG_FILTER));
    EnableControls(hWindow);
}

BOOL MainDlgAutoOpen2 = FALSE;

INT_PTR CALLBACK CfgDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CfgDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // SalamanderGUI->ArrangeHorizontalLines(hWindow); // should be called, but we skip it here, there are no horizontal lines

        HConfigurationDialog = hWindow;
        // horizontally and vertically center the dialog relative to its parent
        CenterWindow(hWindow, GetParent(hWindow));

        CfgInternetConnection = InternetConnection;
        CfgInternetProtocol = InternetProtocol;
        CfgData = Data;
        LoadCfgDlgControls(hWindow);

        return TRUE; // focus handled by the standard dialog proc
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_CONFIGURATION, FALSE);
        return TRUE; // never let F1 reach the parent
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls(hWindow);
        switch (LOWORD(wParam))
        {
        case IDHELP:
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_CONFIGURATION, FALSE);
            return TRUE;
        }

        case IDC_CFG_REMOVE:
        {
            HWND hListBox = GetDlgItem(hWindow, IDC_CFG_FILTER);
            int index = (int)SendMessage(hListBox, LB_GETCURSEL, 0, 0);
            if (index != LB_ERR)
            {
                char itemName[1024];
                if (SendMessage(hListBox, LB_GETTEXT, index, (LPARAM)itemName) != LB_ERR)
                {
                    char buff[1024];
                    sprintf(buff, LoadStr(IDS_REMOVE_CNFRM), itemName);
                    int ret = SalGeneral->SalMessageBox(hWindow, buff, LoadStr(IDS_PLUGINNAME),
                                                        MB_ICONQUESTION | MB_YESNO);
                    if (ret == IDYES)
                    {
                        SendMessage(hListBox, LB_DELETESTRING, index, 0);
                        SendMessage(hListBox, LB_SETCURSEL, 0, 0);
                        EnableControls(hWindow);
                    }
                }
            }
            return 0;
        }

        case IDC_CFG_CHANGE:
        {
            int oldCfgInternetConnection = CfgInternetConnection;
            if (OpenInternetDialog(hWindow, &CfgInternetConnection, &CfgInternetProtocol))
            {
                if (oldCfgInternetConnection != CfgInternetConnection)
                {
                    CfgData = DataDefaults[CfgInternetConnection];
                    LoadCfgDlgControls(hWindow);
                }
            }
            return 0;
        }

        case IDC_CFG_DEFAULTS:
        {
            BOOL reset = TRUE;
            HWND hListBox = GetDlgItem(hWindow, IDC_CFG_FILTER);
            int index = (int)SendMessage(hListBox, LB_GETCURSEL, 0, 0);
            if (index != LB_ERR)
            {
                int ret = SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_SETDEF_CNFRM), LoadStr(IDS_PLUGINNAME),
                                                    MB_ICONQUESTION | MB_YESNO);
                if (ret == IDNO)
                    reset = FALSE;
            }
            if (reset)
            {
                DestroyFilters();
                CfgData = DataDefaults[CfgInternetConnection];
                LoadCfgDlgControls(hWindow);
            }
            return 0;
        }

        case IDOK:
        {
            // retrieve data from the dialog
            BOOL updateNextOpenOrCheckTime = FALSE;
            int i;
            for (i = 0; i < achmCount; i++)
            {
                if (IsDlgButtonChecked(hWindow, AutoCheckModeID[i]) == BST_CHECKED &&
                    Data.AutoCheckMode != AutoCheckModeEval[i])
                {
                    Data.AutoCheckMode = AutoCheckModeEval[i];
                    updateNextOpenOrCheckTime = TRUE;
                    break; // radio buttons: only one can be checked
                }
            }
            BOOL autoConnect = IsDlgButtonChecked(hWindow, IDC_CFG_AUTOCONNECT) == BST_CHECKED;
            if (Data.AutoConnect != autoConnect) // meaning of NextOpenOrCheckTime changed: just open the dialog / open it with an automatic version check
            {
                Data.AutoConnect = autoConnect;
                updateNextOpenOrCheckTime = TRUE;
                ErrorsSinceLastCheck = 0;
                //            TRACE_I("New ErrorsSinceLastCheck: 0");
            }
            if (updateNextOpenOrCheckTime)
            {
                if (!autoConnect || LastCheckTime.wYear == 0)                      // no automatic check or we have not performed one yet
                    ZeroMemory(&NextOpenOrCheckTime, sizeof(NextOpenOrCheckTime)); // opening the window and optionally performing a check on the first load-on-start (ASAP)
                else
                    GetFutureTime(&NextOpenOrCheckTime, &LastCheckTime, GetWaitDays()); // base the next check on the date of the last check
                                                                                        //            TRACE_I("New NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
                // prevent MainDlgProc::IDCANCEL from possibly setting NextOpenOrCheckTime to tomorrow
                MainDlgAutoOpen2 = FALSE;
            }
            Data.AutoClose = IsDlgButtonChecked(hWindow, IDC_CFG_AUTOCLOSE) == BST_CHECKED;
            Data.CheckBetaVersion = IsDlgButtonChecked(hWindow, IDC_CFG_BETA) == BST_CHECKED;
            Data.CheckPBVersion = IsDlgButtonChecked(hWindow, IDC_CFG_PB) == BST_CHECKED;
            Data.CheckReleaseVersion = IsDlgButtonChecked(hWindow, IDC_CFG_RELEASE) == BST_CHECKED;
            FiltersLoadFromListBox(GetDlgItem(hWindow, IDC_CFG_FILTER));
            InternetConnection = CfgInternetConnection;
            InternetProtocol = CfgInternetProtocol;
        }
        case IDCANCEL:
        {
            HConfigurationDialog = NULL;
            EndDialog(hWindow, wParam);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE; // not processed
}

//****************************************************************************
//
// MainDlgProc
//

void MainEnableControls(BOOL downloading)
{
    if (downloading)
    {
        // set the button text
        SetDlgItemText(HMainDialog, IDC_MAIN_CHECK, LoadStr(IDS_BTN_STOP));
    }
    else
    {
        // set the button text
        SetDlgItemText(HMainDialog, IDC_MAIN_CHECK, LoadStr(IDS_BTN_CHECK));
    }
    HWND hCfgButton = GetDlgItem(HMainDialog, IDC_MAIN_CFG);
    HWND hCheckButton = GetDlgItem(HMainDialog, IDC_MAIN_CHECK);

    if (downloading)
    {
        HWND hFocus = GetFocus();
        if (hFocus == hCfgButton)
        {
            SetFocus(hCheckButton);
            SendMessage(HMainDialog, DM_SETDEFID, IDC_MAIN_CHECK, 0);
            SendMessage(hCfgButton, BM_SETSTYLE, BS_PUSHBUTTON, MAKELPARAM(TRUE, 0));
        }
        EnableWindow(hCfgButton, FALSE);
    }
    else
    {
        EnableWindow(hCfgButton, TRUE);
    }
}

BOOL MainDlgAutoOpen = FALSE;

void ShowMinNA_IfNotShownYet(HWND hWindow, BOOL flashIfNotShownYet, BOOL restoreWindow)
{
    if (hWindow != NULL && !IsWindowVisible(hWindow))
    {
        ShowWindow(hWindow, SW_SHOWMINNOACTIVE);
        if (flashIfNotShownYet)
            FlashWindow(hWindow, TRUE);
        MainDlgAutoOpen = FALSE;
        if (restoreWindow)
        {
            if (IsIconic(hWindow))
                ShowWindow(hWindow, SW_RESTORE);
            SetForegroundWindow(hWindow);
        }
    }
}

INT_PTR CALLBACK MainDlgProc(HWND hWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("MainDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // SalamanderGUI->ArrangeHorizontalLines(hWindow); // should be called, but we skip it here, there are no horizontal lines

        HWND hMainWindow = SalGeneral->GetMainWindowHWND();
        if (!IsIconic(hMainWindow))
        {
            // horizontally and vertically center the dialog relative to its parent
            CenterWindow(hWindow, hMainWindow);
        }

        // in debug builds, add an option to the system menu to open
        // a custom file - allows testing the script before uploading it to the web
        HMENU hMenu = GetSystemMenu(hWindow, FALSE);
        if (hMenu != NULL)
        {
            /* used by the export_mnu.py script, which generates salmenu.mnu for the Translator
   keep synchronized with the AppendMenu() call below...
MENU_TEMPLATE_ITEM AppendToSystemMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_MENU_OPENFILE
	{MNTT_IT, IDS_MENU_ABOUT
	{MNTT_PE, 0
};
*/
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING | MF_ENABLED, CM_OPENFILE, LoadStr(IDS_MENU_OPENFILE));
            AppendMenu(hMenu, MF_STRING | MF_ENABLED, CM_ABOUT, LoadStr(IDS_MENU_ABOUT));

            EnableMenuItem(hMenu, SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(hMenu, SC_RESTORE, MF_BYCOMMAND | MF_GRAYED);
        }

        CTVData* tvData = (CTVData*)lParam;
        SetWindowPos(hWindow, tvData->AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        // attach our control to the static control
        HWND hLogWnd = GetDlgItem(hWindow, IDC_MAIN_LOG);
        InitializeLogWindow(hLogWnd);
        // give it a thin border
        LONG exStyle = GetWindowLong(hLogWnd, GWL_EXSTYLE);
        exStyle = (exStyle & ~WS_EX_CLIENTEDGE) | WS_EX_STATICEDGE;
        SetWindowLong(hLogWnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hLogWnd, 0, 0, 0, 100, 100, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

        char buff[500];
        sprintf(buff, LoadStr(IDS_COPYRIGHT1), VERSINFO_VERSION);
        AddLogLine(buff, FALSE);
        AddLogLine(LoadStr(IDS_COPYRIGHT2), FALSE);

        if (LastCheckTime.wYear != 0)
        {
            char date[50];
            if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &LastCheckTime, NULL, date, 50) == 0)
                sprintf(date, "%u.%u.%u", LastCheckTime.wDay, LastCheckTime.wMonth, LastCheckTime.wYear);
            sprintf(buff, LoadStr(IDS_LAST_CHECK), date);
            AddLogLine(buff, FALSE);
        }

        MainDlgAutoOpen2 = tvData->AutoOpen;
        if (MainDlgAutoOpen2)
            AddLogLine(LoadStr(IDS_SKIP_CHECK), FALSE);

        // assign the window icon
        SendMessage(hWindow, WM_SETICON, ICON_BIG,
                    (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_CHECKVER)));

        // set the button text
        SetDlgItemText(hWindow, IDC_MAIN_CHECK, LoadStr(IDS_BTN_CHECK));

        MainDlgAutoOpen = tvData->AutoOpen;
        if (MainDlgAutoOpen)
        {
            if (Data.AutoConnect)
            {
                if (tvData->FirstLoadAfterInstall)
                {
                    // on the first load after installation (without loading a configuration, i.e. installation on a machine without Salamander) the window
                    // is shown after two seconds so that the user, if using a personal firewall, sees why
                    // Salamander is accessing the internet (without a firewall, checkver should finish faster and the user
                    // would not see the CheckVer window at all)
                    if (!SetTimer(hWindow, 665, 2000, NULL))
                        ShowMinNA_IfNotShownYet(hWindow, TRUE, TRUE); // if the timer cannot be created, show the window immediately
                }
                else
                {
                    // during auto-open and auto-connect the window is shown minimized only after one minute (by then the check should succeed or report an error; if it does not finish in time, show the window so the user can resolve it manually)
                    if (!SetTimer(hWindow, 666, 60000, NULL))
                        ShowWindow(hWindow, SW_SHOWMINNOACTIVE); // if the timer cannot be created, show the window immediately
                }
                PostMessage(hWindow, WM_COMMAND,
                            tvData->FirstLoadAfterInstall ? CM_CHECK_FIRSTLOAD : IDC_MAIN_CHECK, 0);
                return FALSE; // the dialog is not visible yet
            }
            else
            {
                GetFutureTime(&NextOpenOrCheckTime, GetWaitDays()); // the date of the next dialog opening depends on the settings
                                                                    //          TRACE_I("New NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
            }
        }
        return TRUE; // focus handled by the standard dialog proc
    }

    case WM_TIMER:
    {
        if (wParam == 665)
        {
            ShowMinNA_IfNotShownYet(hWindow, TRUE, TRUE); // show the window; we are probably blocked by a personal firewall, so let the user know why we are accessing the internet
            KillTimer(hWindow, 665);
        }
        if (wParam == 666)
        {
            ShowMinNA_IfNotShownYet(hWindow, TRUE, FALSE); // flash the window so the user notices it (they should resolve why it has been stuck for a minute)
            KillTimer(hWindow, 666);
        }
        break;
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_MAIN, FALSE);
        return TRUE; // never let F1 reach the parent
    }

    case WM_SIZE:
    {
        if (wParam == SIZE_RESTORED)
            MainDlgAutoOpen = FALSE;
        break;
    }

    case WM_SYSCOMMAND:
    {
        if (LOWORD(wParam) == CM_OPENFILE)
        {
            char file[MAX_PATH];
            lstrcpy(file, "salupdate_en.txt");
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hWindow;
            ofn.lpstrFilter = "*.txt\0*.txt\0*.*\0*.*\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.nFilterIndex = 1;
            //  ofn.lpstrFileTitle = file;
            //  ofn.nMaxFileTitle = MAX_PATH;
            ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_EXPLORER;

            char buf[MAX_PATH];
            GetModuleFileName(DLLInstance, buf, MAX_PATH);
            char* s = strrchr(buf, '\\');
            if (s != NULL)
            {
                *s = 0;
                ofn.lpstrInitialDir = buf;
            }

            if (SalGeneral->SafeGetOpenFileName(&ofn))
            {
                ModulesCleanup();
                ClearLogWindow();
                if (LoadScripDataFromFile(file))
                    ModulesCreateLog(NULL, TRUE);
            }
            return 0;
        }
        if (LOWORD(wParam) == CM_ABOUT)
        {
            PluginInterface.About(hWindow);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDHELP:
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_MAIN, FALSE);
            return TRUE;
        }

        case IDC_MAIN_CFG:
        {
            if (PluginIsReleased)
            {
                // currently handling plugin shutdown - do not let ourselves get dragged into anything else
                SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_PLUGIN_BUSY),
                                          LoadStr(IDS_PLUGINNAME),
                                          MB_ICONINFORMATION | MB_OK);
                return 0;
            }
            OnConfiguration(hWindow);
            if (ModulesHasCorrectData())
            {
                ClearLogWindow(); // clear the log before printing new modules
                ModulesCreateLog(NULL, FALSE);
            }
            return 0;
        }

        case IDC_MAIN_CHECK:
        case CM_CHECK_FIRSTLOAD:
        {
            // the configuration dialog must not already be open
            if (HConfigurationDialog != NULL)
            {
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_CFG_CONFLICT2),
                                          LoadStr(IDS_PLUGINNAME),
                                          MB_ICONINFORMATION | MB_OK);
                return 0;
            }

            if (PluginIsReleased)
            {
                // currently handling plugin shutdown - do not let ourselves get dragged into anything else
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_PLUGIN_BUSY),
                                          LoadStr(IDS_PLUGINNAME),
                                          MB_ICONINFORMATION | MB_OK);
                return 0;
            }

            if (HDownloadThread != NULL)
            {
                // the download thread is running right now - should we let it finish on its own?
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                DWORD ret = SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_ABORT_DOWNLOAD),
                                                      LoadStr(IDS_PLUGINNAME),
                                                      MB_ICONQUESTION | MB_YESNO);
                if (ret != IDNO && HDownloadThread != NULL)
                {
                    IncMainDialogID(); // detach the running session - it will not send anything else and
                                       // will finish as soon as possible

                    CloseHandle(HDownloadThread);
                    HDownloadThread = NULL;
                    ModulesCleanup();
                    MainEnableControls(FALSE);
                    ClearLogWindow();
                    AddLogLine(LoadStr(IDS_INET_ABORTED), TRUE);
                }
                return 0;
            }

            if (lParam != 0) // if the message was not posted from autorun, clear the log
                ClearLogWindow();
            ModulesCleanup();
            HDownloadThread = StartDownloadThread(LOWORD(wParam) == CM_CHECK_FIRSTLOAD);
            if (HDownloadThread != NULL)
            {
                // the loading thread started successfully - now wait
                // for the WM_USER_DOWNLOADTHREAD_EXIT message with the result
                // and disable the buttons
                MainEnableControls(TRUE);
            }
            return 0;
        }

        case IDCANCEL:
        {
            if (HDownloadThread != NULL)
            {
                // the download thread is running right now - should we let it finish on its own?
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                DWORD ret = SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_ABORT_DOWNLOAD),
                                                      LoadStr(IDS_PLUGINNAME),
                                                      MB_ICONQUESTION | MB_YESNO);
                if (ret == IDNO)
                    return 0;

                if (HDownloadThread != NULL)
                {
                    IncMainDialogID(); // detach the running session - it will not send anything else and
                                       // will finish as soon as possible
                    CloseHandle(HDownloadThread);
                    HDownloadThread = NULL;
                    ModulesCleanup();
                    MainEnableControls(FALSE);
                    ClearLogWindow();
                    AddLogLine(LoadStr(IDS_INET_ABORTED), TRUE);
                }
            }

            if (MainDlgAutoOpen2 && IsTimeExpired(&NextOpenOrCheckTime) && Data.AutoCheckMode != achmNever)
            {
                GetFutureTime(&NextOpenOrCheckTime, 1); // if opening the dialog and the check did not finish successfully (and therefore did not set NextOpenOrCheckTime), schedule another attempt for tomorrow
                                                        //            TRACE_I("New on Close: NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
            }

            ReleaseLogWindow(hWindow);
            PostQuitMessage(0);
            return 0;
        }
        }
        break;
    }

    case WM_USER_DOWNLOADTHREAD_EXIT:
    {
        BOOL success = (BOOL)wParam;
        BOOL autoClose = MainDlgAutoOpen && Data.AutoClose;
        BOOL someNewModuleWasFound = FALSE;
        ModulesCleanup();
        if (success)
        {
            ClearLogWindow(); // clear the log before printing new modules
            ModulesCreateLog(&someNewModuleWasFound, TRUE);
            if (autoClose && someNewModuleWasFound)
                autoClose = FALSE;

            GetLocalTime(&LastCheckTime);                       // the check just took place; store its date
            GetFutureTime(&NextOpenOrCheckTime, GetWaitDays()); // the date of the next check depends on the settings
            ErrorsSinceLastCheck = 0;
            //        TRACE_I("After check: LastCheckTime: " << LastCheckTime.wDay << "." << LastCheckTime.wMonth << "." << LastCheckTime.wYear);
            //        TRACE_I("After check: NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
            //        TRACE_I("After check: ErrorsSinceLastCheck: 0");
        }
        else
        {
            if (autoClose && ++ErrorsSinceLastCheck >= 4) // report only the fourth error (ignore the first three to bother the user as little as possible)
            {
                autoClose = FALSE;
                ErrorsSinceLastCheck = 0;
            }
        }
        if (HDownloadThread != NULL)
        {
            // wait until the thread finishes
            WaitForSingleObject(HDownloadThread, INFINITE);
            CloseHandle(HDownloadThread);
            HDownloadThread = NULL;
        }
        // enable the buttons
        MainEnableControls(FALSE);
        if (autoClose)
            PostMessage(hWindow, WM_COMMAND, IDCANCEL, 0);
        else
        {
            if (MainDlgAutoOpen)
            {
                ShowMinNA_IfNotShownYet(hWindow, FALSE, FALSE);
                if (someNewModuleWasFound && IsIconic(hWindow))
                    ShowWindow(hWindow, SW_RESTORE); // Petr: restores the dialog; therefore use it only for the most important event "new version found" (the user can restore the dialog from the taskbar)
                if (someNewModuleWasFound)
                    SetForegroundWindow(hWindow); // forcing attention is justified only for the most important event "new version found"; otherwise the window is merely flashed today...
                else
                    FlashWindow(hWindow, TRUE);
            }
        }
        return 0;
    }

    case WM_USER_KEYDOWN:
    {
        HWND hLogWnd = GetDlgItem(hWindow, IDC_MAIN_LOG);
        switch (wParam)
        {
        case VK_PRIOR:
            SendMessage(hLogWnd, WM_VSCROLL, SB_PAGEUP, 0);
            break;
        case VK_NEXT:
            SendMessage(hLogWnd, WM_VSCROLL, SB_PAGEDOWN, 0);
            break;
        case VK_UP:
            SendMessage(hLogWnd, WM_VSCROLL, SB_LINEUP, 0);
            break;
        case VK_DOWN:
            SendMessage(hLogWnd, WM_VSCROLL, SB_LINEDOWN, 0);
            break;
        case VK_HOME:
            SendMessage(hLogWnd, WM_VSCROLL, SB_THUMBPOSITION, 0);
            break;
        case VK_END:
            SendMessage(hLogWnd, WM_VSCROLL, SB_THUMBPOSITION | 0xFFFF0000, 0);
            break;
        }
        return 0;
    }
    }
    return FALSE; // not processed
}
