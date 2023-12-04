// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

void CenterWindow(HWND hWindow, HWND hParent)
{
    // horizontalni i vertikalni vycentrovani dialogu k parentu
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
        // SalamanderGUI->ArrangeHorizontalLines(hWindow); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary

        SendDlgItemMessage(hWindow, IDC_INTERNET_ICON, STM_SETIMAGE, IMAGE_ICON,
                           (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_INTERNET)));

        InetConnectionType = (WORD*)lParam;
        InetProtocolType = ((WORD*)lParam) + 1;
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        CenterWindow(hWindow, GetParent(hWindow));

        int i;
        for (i = 0; i < inetCount; i++)
            CheckDlgButton(hWindow, InetConectionResID[i], *InetConnectionType == i ? BST_CHECKED : BST_UNCHECKED);
        for (i = 0; i < inetpCount; i++)
            CheckDlgButton(hWindow, InetProtocolResID[i], *InetProtocolType == i ? BST_CHECKED : BST_UNCHECKED);
        PostMessage(hWindow, WM_COMMAND, MAKELPARAM(0, BN_CLICKED), 0); // enablery
        break;
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_INTERNET, FALSE);
        return TRUE; // F1 nenechame propadnout do parenta v zadnem pripade
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
            { // pri kliknuti na FTP zarveme, ze updaty pluginu hlasime jen pres HTTP a jestli jim vazne HTTP nefunguje, jinak at ho koukaji pouzivat
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
            // ziskani dat z dialogu
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

    // nesim byt otevreny konfiguracni dialog
    if (HConfigurationDialog != NULL)
    {
        SalGeneral->SalMessageBox(hParent,
                                  LoadStr(IDS_CFG_CONFLICT1), LoadStr(IDS_PLUGINNAME),
                                  MB_ICONINFORMATION | MB_OK);
        return;
    }

    // nesmi bezet connectici thread
    if (HDownloadThread != NULL)
    {
        ShowMinNA_IfNotShownYet(HMainDialog, TRUE, FALSE); // okno flashneme, aby si ho user vsimnul
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
    // enabler pro checkboxy
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
    if (mode != achmNever && !autoConnectWasEnabled) // defaultne zapiname "tichy" rezim
        CheckDlgButton(hWindow, IDC_CFG_AUTOCONNECT, BST_CHECKED);
    BOOL autoCloseWasEnabled = IsWindowEnabled(GetDlgItem(hWindow, IDC_CFG_AUTOCLOSE));
    BOOL closeEnabled = IsDlgButtonChecked(hWindow, IDC_CFG_AUTOCONNECT) == BST_CHECKED;
    EnableWindow(GetDlgItem(hWindow, IDC_CFG_AUTOCLOSE), mode != achmNever && closeEnabled);
    if (!(mode != achmNever && closeEnabled))
        CheckDlgButton(hWindow, IDC_CFG_AUTOCLOSE, BST_UNCHECKED);
    if (mode != achmNever && (!autoConnectWasEnabled || !autoCloseWasEnabled && closeEnabled)) // defaultne zapiname "tichy" rezim
        CheckDlgButton(hWindow, IDC_CFG_AUTOCLOSE, BST_CHECKED);

    // enabler pro listbox
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
    // vlozeni dat do dialogu
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

    // nalejeme polozky filtru
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
        // SalamanderGUI->ArrangeHorizontalLines(hWindow); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary

        HConfigurationDialog = hWindow;
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        CenterWindow(hWindow, GetParent(hWindow));

        CfgInternetConnection = InternetConnection;
        CfgInternetProtocol = InternetProtocol;
        CfgData = Data;
        LoadCfgDlgControls(hWindow);

        return TRUE; // focus od std. dialogproc
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_CONFIGURATION, FALSE);
        return TRUE; // F1 nenechame propadnout do parenta v zadnem pripade
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
            // ziskani dat z dialogu
            BOOL updateNextOpenOrCheckTime = FALSE;
            int i;
            for (i = 0; i < achmCount; i++)
            {
                if (IsDlgButtonChecked(hWindow, AutoCheckModeID[i]) == BST_CHECKED &&
                    Data.AutoCheckMode != AutoCheckModeEval[i])
                {
                    Data.AutoCheckMode = AutoCheckModeEval[i];
                    updateNextOpenOrCheckTime = TRUE;
                    break; // jde o radioboxy, muze byt "checked" jen jeden
                }
            }
            BOOL autoConnect = IsDlgButtonChecked(hWindow, IDC_CFG_AUTOCONNECT) == BST_CHECKED;
            if (Data.AutoConnect != autoConnect) // zmena vyznamu NextOpenOrCheckTime: jen otevreni dialogu / otevreni s automatickou kontrolou verze
            {
                Data.AutoConnect = autoConnect;
                updateNextOpenOrCheckTime = TRUE;
                ErrorsSinceLastCheck = 0;
                //            TRACE_I("New ErrorsSinceLastCheck: 0");
            }
            if (updateNextOpenOrCheckTime)
            {
                if (!autoConnect || LastCheckTime.wYear == 0)                      // nejde o automatickou kontrolu nebo jsme kontrolu jeste nedelali
                    ZeroMemory(&NextOpenOrCheckTime, sizeof(NextOpenOrCheckTime)); // otevreni okna a prip. kontrola se provede pri prvnim load-on-startu (ASAP)
                else
                    GetFutureTime(&NextOpenOrCheckTime, &LastCheckTime, GetWaitDays()); // dalsi kontrolu vztahneme k datu posledni kontroly
                                                                                        //            TRACE_I("New NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
                // zabranime prip. nastaveni NextOpenOrCheckTime na zitrek v MainDlgProc::IDCANCEL
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
        // nalejeme text do tlacitka
        SetDlgItemText(HMainDialog, IDC_MAIN_CHECK, LoadStr(IDS_BTN_STOP));
    }
    else
    {
        // nalejeme text do tlacitka
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
        // SalamanderGUI->ArrangeHorizontalLines(hWindow); // melo by se volat, tady na to kasleme, nejsou tu horizontalni cary

        HWND hMainWindow = SalGeneral->GetMainWindowHWND();
        if (!IsIconic(hMainWindow))
        {
            // horizontalni i vertikalni vycentrovani dialogu k parentu
            CenterWindow(hWindow, hMainWindow);
        }

        // pokud jde o debug verzi, vlozime do systemoveho menu moznost otevrit
        // vlastni soubor - umoznime tak otestovat script jeste pred uploadem na weba
        HMENU hMenu = GetSystemMenu(hWindow, FALSE);
        if (hMenu != NULL)
        {
            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani AppendMenu() dole...
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

        // pripojime nas control ke staticu
        HWND hLogWnd = GetDlgItem(hWindow, IDC_MAIN_LOG);
        InitializeLogWindow(hLogWnd);
        // nastavime mu tenky ramecek
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

        // priradim oknu ikonku
        SendMessage(hWindow, WM_SETICON, ICON_BIG,
                    (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_CHECKVER)));

        // nalejeme text do tlacitka
        SetDlgItemText(hWindow, IDC_MAIN_CHECK, LoadStr(IDS_BTN_CHECK));

        MainDlgAutoOpen = tvData->AutoOpen;
        if (MainDlgAutoOpen)
        {
            if (Data.AutoConnect)
            {
                if (tvData->FirstLoadAfterInstall)
                {
                    // pri prvnim loadu po instalaci (bez loadu konfigu, proste instalace na komp bez Salama) se okno
                    // zobrazi po dvou vterinach, aby uzivatel v pripade, ze pouziva osobni firewall, videl, proc se
                    // Salamander cpe na internet (bez FW by prace checkveru mela trvat kratsi dobu = uzivatel
                    // vubec neuvidi okno CheckVeru)
                    if (!SetTimer(hWindow, 665, 2000, NULL))
                        ShowMinNA_IfNotShownYet(hWindow, TRUE, TRUE); // pokud nejde zalozit timer, radsi okno ukazeme hned
                }
                else
                {
                    // pri auto-open a auto-connect se okno zobrazi minimalizovane az po minute (do te doby by mela probehnout kontrola nebo se ohlasit chyba, kdyby se to nestihlo, ukazeme userovi okenko, at to muze jit resit rucne)
                    if (!SetTimer(hWindow, 666, 60000, NULL))
                        ShowWindow(hWindow, SW_SHOWMINNOACTIVE); // pokud nejde zalozit timer, radsi okno ukazeme hned
                }
                PostMessage(hWindow, WM_COMMAND,
                            tvData->FirstLoadAfterInstall ? CM_CHECK_FIRSTLOAD : IDC_MAIN_CHECK, 0);
                return FALSE; // dialog jeste neni videt
            }
            else
            {
                GetFutureTime(&NextOpenOrCheckTime, GetWaitDays()); // datum dalsiho otevreni dialogu zalezi na nastaveni
                                                                    //          TRACE_I("New NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
            }
        }
        return TRUE; // focus od std. dialogproc
    }

    case WM_TIMER:
    {
        if (wParam == 665)
        {
            ShowMinNA_IfNotShownYet(hWindow, TRUE, TRUE); // ukazeme okno, asi nas blokuje osobni firewall, tak at uzivatle vi, proc se cpeme na internet
            KillTimer(hWindow, 665);
        }
        if (wParam == 666)
        {
            ShowMinNA_IfNotShownYet(hWindow, TRUE, FALSE); // okno flashneme, aby si ho user vsimnul (ma do nej jit resit proc je to uz minutu zakousle)
            KillTimer(hWindow, 666);
        }
        break;
    }

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, IDD_MAIN, FALSE);
        return TRUE; // F1 nenechame propadnout do parenta v zadnem pripade
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
                // prave jsme v obsluze zavirani pluginu - nenechame se do niceho dalsiho uvrtat
                SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_PLUGIN_BUSY),
                                          LoadStr(IDS_PLUGINNAME),
                                          MB_ICONINFORMATION | MB_OK);
                return 0;
            }
            OnConfiguration(hWindow);
            if (ModulesHasCorrectData())
            {
                ClearLogWindow(); // vycisteni logu pred vypisem novych modulu
                ModulesCreateLog(NULL, FALSE);
            }
            return 0;
        }

        case IDC_MAIN_CHECK:
        case CM_CHECK_FIRSTLOAD:
        {
            // nesmi byt otevreny konfiguracni dialog
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
                // prave jsme v obsluze zavirani pluginu - nenechame se do niceho dalsiho uvrtat
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_PLUGIN_BUSY),
                                          LoadStr(IDS_PLUGINNAME),
                                          MB_ICONINFORMATION | MB_OK);
                return 0;
            }

            if (HDownloadThread != NULL)
            {
                // prave jede download thread - mame ho nechat vyhnit?
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                DWORD ret = SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_ABORT_DOWNLOAD),
                                                      LoadStr(IDS_PLUGINNAME),
                                                      MB_ICONQUESTION | MB_YESNO);
                if (ret != IDNO && HDownloadThread != NULL)
                {
                    IncMainDialogID(); // odpojime bezici session - uz nam nic neposle a
                                       // hned jak to bude mozne, skonci

                    CloseHandle(HDownloadThread);
                    HDownloadThread = NULL;
                    ModulesCleanup();
                    MainEnableControls(FALSE);
                    ClearLogWindow();
                    AddLogLine(LoadStr(IDS_INET_ABORTED), TRUE);
                }
                return 0;
            }

            if (lParam != 0) // pokud message neni posted z autorunu, smazneme log
                ClearLogWindow();
            ModulesCleanup();
            HDownloadThread = StartDownloadThread(LOWORD(wParam) == CM_CHECK_FIRSTLOAD);
            if (HDownloadThread != NULL)
            {
                // podarilo se rozjet nacitaci thread - ted uz budeme cekat
                // na prichod zpravy WM_USER_DOWNLOADTHREAD_EXIT s vysledkem
                // a zakazeme tlacitka
                MainEnableControls(TRUE);
            }
            return 0;
        }

        case IDCANCEL:
        {
            if (HDownloadThread != NULL)
            {
                // prave jede download thread - mame ho nechat vyhnit?
                ShowMinNA_IfNotShownYet(hWindow, FALSE, TRUE);
                DWORD ret = SalGeneral->SalMessageBox(hWindow, LoadStr(IDS_ABORT_DOWNLOAD),
                                                      LoadStr(IDS_PLUGINNAME),
                                                      MB_ICONQUESTION | MB_YESNO);
                if (ret == IDNO)
                    return 0;

                if (HDownloadThread != NULL)
                {
                    IncMainDialogID(); // odpojime bezici session - uz nam nic neposle a
                                       // hned jak to bude mozne, skonci
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
                GetFutureTime(&NextOpenOrCheckTime, 1); // pokud otevreni dialogu a prip. kontrola uspesne neprobehla (a tedy nenastavilo se NextOpenOrCheckTime), stanovime dalsi pokus na zitrek
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
            ClearLogWindow(); // vycisteni logu pred vypisem novych modulu
            ModulesCreateLog(&someNewModuleWasFound, TRUE);
            if (autoClose && someNewModuleWasFound)
                autoClose = FALSE;

            GetLocalTime(&LastCheckTime);                       // kontrola prave probehla, ulozime jeji datum
            GetFutureTime(&NextOpenOrCheckTime, GetWaitDays()); // datum dalsi kontroly zalezi na nastaveni
            ErrorsSinceLastCheck = 0;
            //        TRACE_I("After check: LastCheckTime: " << LastCheckTime.wDay << "." << LastCheckTime.wMonth << "." << LastCheckTime.wYear);
            //        TRACE_I("After check: NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
            //        TRACE_I("After check: ErrorsSinceLastCheck: 0");
        }
        else
        {
            if (autoClose && ++ErrorsSinceLastCheck >= 4) // hlasime az ctvrtou chybu (prvni tri ignorujeme, snazime se usera co nejmene prudit)
            {
                autoClose = FALSE;
                ErrorsSinceLastCheck = 0;
            }
        }
        if (HDownloadThread != NULL)
        {
            // pockame, az thread dobehne
            WaitForSingleObject(HDownloadThread, INFINITE);
            CloseHandle(HDownloadThread);
            HDownloadThread = NULL;
        }
        // povolime tlacitka
        MainEnableControls(FALSE);
        if (autoClose)
            PostMessage(hWindow, WM_COMMAND, IDCANCEL, 0);
        else
        {
            if (MainDlgAutoOpen)
            {
                ShowMinNA_IfNotShownYet(hWindow, FALSE, FALSE);
                if (someNewModuleWasFound && IsIconic(hWindow))
                    ShowWindow(hWindow, SW_RESTORE); // Petr: aktivuje dialog, proto jsem to krome nejdulezitejsi udalosti "nova verze nalezena" odstavit (user dialog restorne po kliknuti na taskbar)
                if (someNewModuleWasFound)
                    SetForegroundWindow(hWindow); // takove vynucovani pozornosti je ospravedlnitelne jen pri nejdulezitejsi udalosti "nova verze nalezena", jinak uz se dneska okno jen flashuje...
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
