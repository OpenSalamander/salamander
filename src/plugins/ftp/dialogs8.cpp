// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//****************************************************************************

void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* Text,
                     int textLen, int historySize, char* history[], BOOL secretValue)
{
    CALL_STACK_MESSAGE5("HistoryComboBox(, , %d, , %d, %d, %d)",
                        ctrlID, textLen, historySize, secretValue);
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
                if (!secretValue && Text[0] != 0)
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
                        char* newText = (char*)malloc(strlen(Text) + 1);
                        if (newText != NULL)
                        {
                            strcpy(newText, Text);
                            if (history[historySize - 1] != NULL)
                                free(history[historySize - 1]);
                            memmove(history + 1, history,
                                    (historySize - 1) * sizeof(char*));
                            history[0] = newText;
                        }
                        else
                            TRACE_E(LOW_MEMORY);
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

//
// ****************************************************************************
// CSendFTPCommandDlg
//

CSendFTPCommandDlg::CSendFTPCommandDlg(HWND parent)
    : CCenteredDialog(HLanguage, IDD_SENDCOMMANDDLG, IDD_SENDCOMMANDDLG, parent)
{
    Command[0] = 0;
    ChangePathInPanel = TRUE;
    RefreshWorkingPath = TRUE;
    if (!Config.SendSecretCommand && Config.CommandHistory[0] != NULL)
        lstrcpyn(Command, Config.CommandHistory[0], FTPCOMMAND_MAX_SIZE);
}

void CSendFTPCommandDlg::Validate(CTransferInfo& ti)
{
    char cmd[FTPCOMMAND_MAX_SIZE];
    BOOL secret = IsDlgButtonChecked(HWindow, IDC_SECRETCOMMAND) == BST_CHECKED;
    if (secret)
        ti.EditLine(IDE_FTPCOMMAND_PASSWD, cmd, FTPCOMMAND_MAX_SIZE);
    else
        ti.EditLine(IDC_FTPCOMMAND, cmd, FTPCOMMAND_MAX_SIZE);
    if (cmd[0] == 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CMDMAYNOTBEEMPTY),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(secret ? IDE_FTPCOMMAND_PASSWD : IDC_FTPCOMMAND);
    }
}

void CSendFTPCommandDlg::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_SECRETCOMMAND, Config.SendSecretCommand);
    ti.CheckBox(IDC_CHANGEPATHINPANEL, ChangePathInPanel);
    ti.CheckBox(IDC_REFRESHWORKINGPATH, RefreshWorkingPath);
    if (ti.Type == ttDataToWindow)
    {
        HistoryComboBox(HWindow, ti, IDC_FTPCOMMAND, Command, FTPCOMMAND_MAX_SIZE,
                        COMMAND_HISTORY_SIZE, Config.CommandHistory, Config.SendSecretCommand);
        ti.EditLine(IDE_FTPCOMMAND_PASSWD, Command, FTPCOMMAND_MAX_SIZE);
    }
    else
    {
        if (!Config.SendSecretCommand)
        {
            HistoryComboBox(HWindow, ti, IDC_FTPCOMMAND, Command, FTPCOMMAND_MAX_SIZE,
                            COMMAND_HISTORY_SIZE, Config.CommandHistory, Config.SendSecretCommand);
        }
        else
            ti.EditLine(IDE_FTPCOMMAND_PASSWD, Command, FTPCOMMAND_MAX_SIZE);
    }
}

void CSendFTPCommandDlg::EnableControls()
{
    BOOL secret = IsDlgButtonChecked(HWindow, IDC_SECRETCOMMAND) == BST_CHECKED;
    HWND combo = GetDlgItem(HWindow, IDC_FTPCOMMAND);
    HWND edit = GetWindow(combo, GW_CHILD);
    HWND passwdEdit = GetDlgItem(HWindow, IDE_FTPCOMMAND_PASSWD);

    if (secret)
    {
        if (!IsWindowVisible(passwdEdit))
        {
            BOOL focus = GetFocus() == edit;
            ShowWindow(combo, SW_HIDE);
            ShowWindow(passwdEdit, SW_SHOW);
            if (focus)
                SetFocus(passwdEdit);
        }
    }
    else
    {
        if (!IsWindowVisible(combo))
        {
            BOOL focus = GetFocus() == passwdEdit;
            ShowWindow(passwdEdit, SW_HIDE);
            ShowWindow(combo, SW_SHOW);
            if (focus)
                SetFocus(edit);
        }
    }
    //  SendMessage(edit, EM_SETPASSWORDCHAR, secret ? '*' : 0, 0);
    //  InvalidateRect(combo, NULL, TRUE);
    //  UpdateWindow(combo);
}

INT_PTR
CSendFTPCommandDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSendFTPCommandDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        INT_PTR ret = CCenteredDialog::DialogProc(uMsg, wParam, lParam);
        PostMessage(HWindow, WM_APP + 1000, 0, 0);
        return ret;
    }

    case WM_APP + 1000:
    {
        EnableControls();
        return TRUE; // zprava je zpracovana
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SECRETCOMMAND)
        {
            BOOL secret = IsDlgButtonChecked(HWindow, IDC_SECRETCOMMAND) == BST_CHECKED;
            HWND combo = GetDlgItem(HWindow, IDC_FTPCOMMAND);
            HWND edit = GetWindow(combo, GW_CHILD);
            HWND passwdEdit = GetDlgItem(HWindow, IDE_FTPCOMMAND_PASSWD);

            // musime prohodit aktualni texty mezi editem a combem
            char buf[FTPCOMMAND_MAX_SIZE];
            GetWindowText(secret ? edit : passwdEdit, buf, FTPCOMMAND_MAX_SIZE);
            SetWindowText(!secret ? edit : passwdEdit, buf);

            EnableControls();
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CServersListbox
//

CServersListbox::CServersListbox(CConfigPageServers* dlg, int ctrlID)
    : CWindow(dlg->HWindow, ctrlID)
{
    ParentDlg = dlg;
}

void CServersListbox::OpenContextMenu(int curSel, int menuX, int menuY)
{
    HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_SERVERSACTIONS));
    if (main != NULL)
    {
        HMENU subMenu = GetSubMenu(main, 0);
        if (subMenu != NULL)
        {
            int count = ParentDlg->TmpServerTypeList != NULL ? ParentDlg->TmpServerTypeList->Count : 0;
            MyEnableMenuItem(subMenu, IDB_EDITSERVER, count != 0);
            MyEnableMenuItem(subMenu, CM_COPYSERVERTO, count != 0);
            MyEnableMenuItem(subMenu, CM_RENAMESERVER, count != 0);
            MyEnableMenuItem(subMenu, CM_REMOVESERVER, count != 0);
            MyEnableMenuItem(subMenu, CM_EXPORTSERVER, count != 0);
            MyEnableMenuItem(subMenu, IDB_MOVESERVERUP, curSel > 0);
            MyEnableMenuItem(subMenu, IDB_MOVESERVERDOWN, curSel + 1 < count);
            DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                         menuX, menuY, HWindow, NULL);
            if (cmd != 0)
                PostMessage(ParentDlg->HWindow, WM_COMMAND, cmd, 0);
        }
        DestroyMenu(main);
    }
}

LRESULT
CServersListbox::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        switch (wParam)
        {
        case VK_INSERT:
            PostMessage(ParentDlg->HWindow, WM_COMMAND, IDB_NEWSERVER, 0);
            break;
        case VK_F2:
            PostMessage(ParentDlg->HWindow, WM_COMMAND, CM_RENAMESERVER, 0);
            break;
        case VK_DELETE:
            PostMessage(ParentDlg->HWindow, WM_COMMAND, CM_REMOVESERVER, 0);
            break;

        case VK_UP:
        case VK_DOWN:
        {
            if (GetKeyState(VK_MENU) & 0x8000) // stisknuty Alt
            {
                PostMessage(ParentDlg->HWindow, WM_COMMAND,
                            (LOWORD(wParam) == VK_UP ? IDB_MOVESERVERUP : IDB_MOVESERVERDOWN), 0);
            }
            break;
        }

        case VK_F10:
            if ((GetKeyState(VK_SHIFT) & 0x8000) == 0)
                break;
        case VK_APPS:
        {
            int curSel = (int)SendMessage(HWindow, LB_GETCURSEL, 0, 0);
            if (curSel != LB_ERR)
            {
                RECT r;
                SendMessage(HWindow, LB_GETITEMRECT, curSel, (LPARAM)&r);
                POINT p;
                p.x = r.left;
                p.y = r.bottom;
                ClientToScreen(HWindow, &p);
                OpenContextMenu(curSel, p.x + 10, p.y);
            }
            break;
        }
        }
        break; // klavesy poustime dale, listbox by je nemel zpracovavat, zadny problem
    }

    case WM_LBUTTONDBLCLK:
    {
        PostMessage(ParentDlg->HWindow, WM_COMMAND, IDB_EDITSERVER, 0);
        break;
    }

    case WM_RBUTTONDOWN:
    {
        if (GetFocus() != HWindow)
        {
            SendMessage(ParentDlg->HWindow, WM_NEXTDLGCTL, (WPARAM)HWindow, TRUE);
        }
        int curSel = (int)SendMessage(HWindow, LB_GETCURSEL, 0, 0);
        int item = (int)SendMessage(HWindow, LB_ITEMFROMPOINT, 0,
                                    MAKELPARAM(LOWORD((int)lParam), HIWORD((int)lParam))); // FIXME_X64 podezrele pretypovani
        if (HIWORD(item) == 0 && item >= 0 && ParentDlg->TmpServerTypeList != NULL &&
            item < ParentDlg->TmpServerTypeList->Count && curSel != item)
        {
            SendMessage(HWindow, LB_SETCURSEL, item, 0);
            SendMessage(ParentDlg->HWindow, WM_COMMAND, MAKELPARAM(IDL_SUPPORTEDSERVERS, LBN_SELCHANGE), 0);
            curSel = item;
        }

        if (HIWORD(item) == 0 && curSel != LB_ERR)
        {
            POINT p;
            GetCursorPos(&p);
            OpenContextMenu(curSel, p.x, p.y);
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConnectAdvancedDlg
//

CConnectAdvancedDlg::CConnectAdvancedDlg(HWND parent, CFTPServer* server,
                                         CFTPProxyServerList* sourceTmpFTPProxyServerList)
    : CCenteredDialog(HLanguage, IDD_CONNECTADVANCED, IDD_CONNECTADVANCED, parent)
{
    SourceTmpFTPProxyServerList = sourceTmpFTPProxyServerList;
    TmpFTPProxyServerList = new CFTPProxyServerList;
    if (TmpFTPProxyServerList != NULL)
    {
        if (!SourceTmpFTPProxyServerList->CopyMembersToList(*TmpFTPProxyServerList))
        {
            delete TmpFTPProxyServerList;
            TmpFTPProxyServerList = NULL;
        }
    }
    else
        TRACE_E(LOW_MEMORY);

    Server = server;
    MaxConBuf[0] = 0;
    TotSpeedBuf[0] = 0;
    LastUseMaxCon = -1;
    LastUseTotSpeed = -1;
    LastKeepConnectionAlive = -1;
    KASendCmd = -1;
    KASendEveryBuf[0] = 0;
    KAStopAfterBuf[0] = 0;
}

CConnectAdvancedDlg::~CConnectAdvancedDlg()
{
    if (TmpFTPProxyServerList != NULL)
        delete TmpFTPProxyServerList;
}

void CConnectAdvancedDlg::Validate(CTransferInfo& ti)
{
    int port;
    ti.EditLine(IDC_CONNECTTOPORT, port);
    if (!ti.IsGood())
        return; // uz nastala chyba
    if (port <= 0 || port >= 65536)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_PORTISUSHORT),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_CONNECTTOPORT);
        return;
    }

    // test if "max. concurrent connections" (if used) is valid number
    int enableMaxConcCon;
    ti.CheckBox(IDC_MAXCONCURRENTCON, enableMaxConcCon);
    int maxConcCon;
    if (enableMaxConcCon == 1)
    {
        ti.EditLine(IDE_MAXCONCURRENTCON, maxConcCon);
        if (!ti.IsGood())
            return; // uz nastala chyba
        if (ti.IsGood() && maxConcCon <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_MAXCONCURRENTCON);
            return;
        }
    }

    int useKeepAlive;
    ti.CheckBox(IDC_USEKEEPALIVE, useKeepAlive);
    if (useKeepAlive == 1)
    {
        int num;
        int arr[] = {IDE_KEEPALIVEEVERY, IDE_KEEPALIVESTOPAFTER, -1};
        int i;
        for (i = 0; arr[i] != -1; i++)
        {
            ti.EditLine(arr[i], num);
            if (!ti.IsGood())
                return; // uz nastala chyba
            if (num <= 0)
            {
                SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                                 LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(arr[i]);
                return;
            }
        }

        int stop, every;
        ti.EditLine(IDE_KEEPALIVEEVERY, every);
        ti.EditLine(IDE_KEEPALIVESTOPAFTER, stop);
        if (every > 10000)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_KAEVERYTOOBIG),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_KEEPALIVEEVERY);
            return;
        }
        if (stop > 10000)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_KASTOPTOOBIG),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_KEEPALIVESTOPAFTER);
            return;
        }
        if (stop * 60 < every)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_KAEVERYGRTHSTOP),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_KEEPALIVESTOPAFTER);
            return;
        }
    }

    // test if "server speed limit" (if used) is valid number
    int enableSrvSpeedLimit;
    ti.CheckBox(IDC_SRVSPEEDLIMIT, enableSrvSpeedLimit);
    double srvSpeedLimit;
    if (enableSrvSpeedLimit == 1)
    {
        char buff[] = "%g";
        ti.EditLine(IDE_SRVSPEEDLIMIT, srvSpeedLimit, buff);
        if (!ti.IsGood())
            return; // uz nastala chyba
        if (ti.IsGood() && srvSpeedLimit <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_SRVSPEEDLIMIT);
            return;
        }
    }

    if (ti.IsGood() && SendDlgItemMessage(HWindow, IDC_LISTCOMMAND, WM_GETTEXTLENGTH, 0, 0) == 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CMDMAYNOTBEEMPTY),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_LISTCOMMAND);
        return;
    }
}

void CConnectAdvancedDlg::Transfer(CTransferInfo& ti)
{
    if (TmpFTPProxyServerList != NULL)
    {
        ProxyComboBox(HWindow, ti, IDC_PROXYSERVER, Server->ProxyServerUID, TRUE,
                      TmpFTPProxyServerList);
        if (ti.Type == ttDataFromWindow)                                            // prelejeme data zpatky do zdroje
            TmpFTPProxyServerList->CopyMembersToList(*SourceTmpFTPProxyServerList); // CheckProxyServersUID() pro editovany seznam bookmark se vola tesne po ukonceni tohoto dialogu
    }

    if (ti.Type == ttDataToWindow)
        ti.EditLine(IDE_TARGETPATH, HandleNULLStr(Server->TargetPanelPath), MAX_PATH);
    else
    {
        char targetPath[MAX_PATH];
        ti.EditLine(IDE_TARGETPATH, targetPath, MAX_PATH);
        UpdateStr(Server->TargetPanelPath, targetPath);
    }
    HWND combo;
    if (ti.GetControl(combo, IDC_SERVERTYPE))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_SRVTYPEAUTODETECT));
            int index;
            Config.LockServerTypeList()->AddNamesToCombo(combo, Server->ServerType, index);
            Config.UnlockServerTypeList();
            SendMessage(combo, CB_SETCURSEL, index + 1, 0);
        }
        else
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            CServerTypeList* serverTypeList = Config.LockServerTypeList(); // vse v hl. threadu, nemelo by dojit ke zmene Config.ServerTypeList (od okamziku plneni comba)
            if (i != CB_ERR && i > 0 && i - 1 < serverTypeList->Count)
            {
                UpdateStr(Server->ServerType, serverTypeList->At(i - 1)->TypeName);
            }
            else
            {
                if (Server->ServerType != NULL)
                    free(Server->ServerType);
                Server->ServerType = NULL;
            }
            Config.UnlockServerTypeList();
        }
    }
    if (ti.GetControl(combo, IDC_TRANSFERMODE))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            int resIDs[] = {IDS_TRANSFMODEDEFAULT, IDS_TRANSFMODEBINARY, IDS_TRANSFMODEASCII,
                            IDS_TRANSFMODEAUTO, -1};
            int i;
            for (i = 0; resIDs[i] != -1; i++)
            {
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(resIDs[i]));
            }
            if (Server->TransferMode < 0)
                Server->TransferMode = 0;
            if (Server->TransferMode > 3)
                Server->TransferMode = 3;
            SendMessage(combo, CB_SETCURSEL, Server->TransferMode, 0);
        }
        else
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR && i >= 0 && i <= 3)
                Server->TransferMode = i;
            else
                TRACE_E("Unexpected situation in CConnectAdvancedDlg::Transfer().");
        }
    }
    if (ti.GetControl(combo, IDC_LISTCOMMAND))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LIST_CMD_TEXT);
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)NLST_CMD_TEXT);
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LIST_a_CMD_TEXT);
            SendMessage(combo, CB_LIMITTEXT, FTPCOMMAND_MAX_SIZE, 0);
            if (Server->ListCommand != NULL)
                SendMessage(combo, WM_SETTEXT, 0, (LPARAM)Server->ListCommand);
            else
                SendMessage(combo, CB_SETCURSEL, 0, 0); // vyber textu LIST_CMD_TEXT
        }
        else
        {
            char listCmd[FTPCOMMAND_MAX_SIZE];
            SendMessage(combo, WM_GETTEXT, FTPCOMMAND_MAX_SIZE, (LPARAM)listCmd);
            if (strcmp(listCmd, LIST_CMD_TEXT) != 0)
            {
                UpdateStr(Server->ListCommand, listCmd);
            }
            else
            {
                if (Server->ListCommand != NULL)
                    free(Server->ListCommand);
                Server->ListCommand = NULL;
            }
        }
    }
    ti.EditLine(IDC_CONNECTTOPORT, Server->Port);
    if (ti.Type == ttDataToWindow)
        ti.EditLine(IDE_INITFTPCOMMANDS, HandleNULLStr(Server->InitFTPCommands), FTP_MAX_PATH);
    else
    {
        char initFTPCommands[FTP_MAX_PATH];
        ti.EditLine(IDE_INITFTPCOMMANDS, initFTPCommands, FTP_MAX_PATH);
        UpdateStr(Server->InitFTPCommands, initFTPCommands);
    }

    ti.CheckBox(IDC_USEKEEPALIVE, Server->KeepConnectionAlive);
    if (ti.GetControl(combo, IDC_KEEPALIVESEND))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);
            int strID[] = {IDS_KEEPALIVECMDNOOP, IDS_KEEPALIVECMDPWD, IDS_KEEPALIVECMDNLST,
                           IDS_KEEPALIVECMDLIST, -1};
            int i;
            for (i = 0; strID[i] != -1; i++)
            {
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(strID[i]));
            }
            // overime jestli KeepAliveCommand neni mimo meze (snad jedine prima editace registry)
            if (Config.KeepAliveCommand >= i)
                Config.KeepAliveCommand = i - 1;
            if (Config.KeepAliveCommand < 0)
                Config.KeepAliveCommand = 0;
            if (Server->KeepAliveCommand >= i)
                Server->KeepAliveCommand = i - 1;
            if (Server->KeepAliveCommand < 0)
                Server->KeepAliveCommand = 0;
        }

        if (ti.Type == ttDataToWindow)
        {
            switch (Server->KeepConnectionAlive)
            {
            case 0: // UNCHECKED
            {
                SendMessage(combo, CB_SETCURSEL, -1, 0);
                SetDlgItemText(HWindow, IDE_KEEPALIVEEVERY, "");
                SetDlgItemText(HWindow, IDE_KEEPALIVESTOPAFTER, "");
                break;
            }

            case 1: // CHECKED
            {
                SendMessage(combo, CB_SETCURSEL, Server->KeepAliveCommand, 0);
                ti.EditLine(IDE_KEEPALIVEEVERY, Server->KeepAliveSendEvery);
                ti.EditLine(IDE_KEEPALIVESTOPAFTER, Server->KeepAliveStopAfter);
                break;
            }

            default: // INDETERMINATE
            {
                SendMessage(combo, CB_SETCURSEL, Config.KeepAliveCommand, 0);
                ti.EditLine(IDE_KEEPALIVEEVERY, Config.KeepAliveSendEvery);
                ti.EditLine(IDE_KEEPALIVESTOPAFTER, Config.KeepAliveStopAfter);
                break;
            }
            }
        }
        else
        {
            if (Server->KeepConnectionAlive == 1) // CHECKED
            {
                int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                if (i != CB_ERR)
                    Server->KeepAliveCommand = i;
                ti.EditLine(IDE_KEEPALIVEEVERY, Server->KeepAliveSendEvery);
                ti.EditLine(IDE_KEEPALIVESTOPAFTER, Server->KeepAliveStopAfter);
            }
        }
    }

    ti.CheckBox(IDC_USEPASSIVEMODE, Server->UsePassiveMode);
    ti.CheckBox(IDC_MAXCONCURRENTCON, Server->UseMaxConcurrentConnections);
    if (ti.Type == ttDataFromWindow && Server->UseMaxConcurrentConnections == 1)
    {
        ti.EditLine(IDE_MAXCONCURRENTCON, Server->MaxConcurrentConnections);
    }
    ti.CheckBox(IDC_SRVSPEEDLIMIT, Server->UseServerSpeedLimit);
    if (ti.Type == ttDataFromWindow && Server->UseServerSpeedLimit == 1)
    {
        char buff[] = "%g";
        ti.EditLine(IDE_SRVSPEEDLIMIT, Server->ServerSpeedLimit, buff);
        if (ti.Type == ttDataFromWindow &&
            Server->ServerSpeedLimit < 0.001)
            Server->ServerSpeedLimit = 0.001; // aspon 1 byte za sekundu
    }
    ti.CheckBox(IDC_USELISTINGSCACHE, Server->UseListingsCache);
    ti.CheckBox(IDC_ENCRYPTCONTROLCONN, Server->EncryptControlConnection);
    ti.CheckBox(IDC_ENCRYPTDATACONN, Server->EncryptDataConnection);
    if (ti.Type == ttDataFromWindow)
    {
        int compressData;
        ti.CheckBox(IDC_COMPRESSDATA, compressData);
        switch (compressData)
        {
        case 0:
            Server->CompressData = 0;
            break; // NO
        case 1:
            Server->CompressData = 6;
            break; // Yes, default compression is 6
        case 2:
            Server->CompressData = -1;
            break; // Take the default from the globals
        }
    }
    else
    {
        int compressData;
        switch (Server->CompressData)
        {
        case 0:
            compressData = 0;
            break; // NO
        case -1:
            compressData = 2;
            break; // Take the default from the globals
        default:
        case 6:
            compressData = 1;
            break; // Yes, default compression is 6
        }
        ti.CheckBox(IDC_COMPRESSDATA, compressData);
    }
} /* CConnectAdvancedDlg::Transfer */

void CheckboxCombo(HWND dlg, int checkboxID, int comboID, int* lastCheck, int* valueBuf,
                   int checkedVal, BOOL globValUsed, int globVal)
{
    int setVal;
    int check = IsDlgButtonChecked(dlg, checkboxID);
    EnableWindow(GetDlgItem(dlg, comboID), check == BST_CHECKED);
    if (*lastCheck != check)
    {
        if (*lastCheck == 1)
            *valueBuf = (int)SendDlgItemMessage(dlg, comboID, CB_GETCURSEL, 0, 0);
        switch (check)
        {
        case 0:
            setVal = -1;
            break; // vypnuto
        case 1:
        {
            if (*valueBuf == -1)
                *valueBuf = checkedVal;
            setVal = *valueBuf;
            break; // zapnuto
        }

        default: // treti stav
        {
            if (globValUsed)
                setVal = globVal;
            else
                setVal = -1; // nepouzite
            break;
        }
        }
        SendDlgItemMessage(dlg, comboID, CB_SETCURSEL, setVal, 0);
        *lastCheck = check;
    }
}

void CConnectAdvancedDlg::EnableControls()
{
    CheckboxEditLineInteger(HWindow, IDC_MAXCONCURRENTCON, IDE_MAXCONCURRENTCON, &LastUseMaxCon,
                            MaxConBuf, Server->MaxConcurrentConnections,
                            Config.UseMaxConcurrentConnections, Config.MaxConcurrentConnections);

    CheckboxEditLineDouble(HWindow, IDC_SRVSPEEDLIMIT, IDE_SRVSPEEDLIMIT, &LastUseTotSpeed,
                           TotSpeedBuf, Server->ServerSpeedLimit,
                           Config.UseServerSpeedLimit, Config.ServerSpeedLimit);

    int auxLastKeepConnectionAlive = LastKeepConnectionAlive;
    CheckboxCombo(HWindow, IDC_USEKEEPALIVE, IDC_KEEPALIVESEND, &auxLastKeepConnectionAlive,
                  &KASendCmd, Server->KeepAliveCommand, Config.KeepAlive, Config.KeepAliveCommand);
    auxLastKeepConnectionAlive = LastKeepConnectionAlive;
    CheckboxEditLineInteger(HWindow, IDC_USEKEEPALIVE, IDE_KEEPALIVEEVERY, &auxLastKeepConnectionAlive,
                            KASendEveryBuf, Server->KeepAliveSendEvery,
                            Config.KeepAlive, Config.KeepAliveSendEvery);
    auxLastKeepConnectionAlive = LastKeepConnectionAlive;
    CheckboxEditLineInteger(HWindow, IDC_USEKEEPALIVE, IDE_KEEPALIVESTOPAFTER, &auxLastKeepConnectionAlive,
                            KAStopAfterBuf, Server->KeepAliveStopAfter,
                            Config.KeepAlive, Config.KeepAliveStopAfter);
    LastKeepConnectionAlive = auxLastKeepConnectionAlive;
    EnableWindow(GetDlgItem(HWindow, IDC_ENCRYPTCONTROLCONN), TRUE);
    EnableWindow(GetDlgItem(HWindow, IDC_ENCRYPTDATACONN), IsDlgButtonChecked(HWindow, IDC_ENCRYPTCONTROLCONN));
}

INT_PTR
CConnectAdvancedDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConnectAdvancedDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGUI->AttachButton(HWindow, IDB_ADDPROXYSRV, BTF_DROPDOWN);

        SalamanderGeneral->InstallWordBreakProc(GetDlgItem(HWindow, IDE_TARGETPATH));
        INT_PTR ret = CCenteredDialog::DialogProc(uMsg, wParam, lParam);
        EnableControls();
        return ret;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDB_ADDPROXYSRV) // dropdown menu na Add buttonu
        {
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_ADDPROXYSERVER));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        // enablujeme polozky menu
                        HWND combo = GetDlgItem(HWindow, IDC_PROXYSERVER);
                        int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                        int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
                        int fixedItems = 2; // "not used" + "default"
                        EnableMenuItem(subMenu, CM_EDITPROXYSRV, MF_BYCOMMAND | ((sel != CB_ERR && count != CB_ERR ? sel >= fixedItems : FALSE) ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
                        EnableMenuItem(subMenu, CM_DELETEPROXYSRV, MF_BYCOMMAND | ((sel != CB_ERR && count != CB_ERR ? sel >= fixedItems : FALSE) ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
                        EnableMenuItem(subMenu, CM_MOVEUPPROXYSRV, MF_BYCOMMAND | ((sel != CB_ERR && count != CB_ERR ? sel > fixedItems : FALSE) ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
                        EnableMenuItem(subMenu, CM_MOVEDOWNPROXYSRV, MF_BYCOMMAND | ((sel != CB_ERR && count != CB_ERR ? sel >= fixedItems && sel + 1 < count : FALSE) ? MF_ENABLED : MF_DISABLED | MF_GRAYED));

                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_BROWSE)
        {
            char initDir[MAX_PATH];
            GetDlgItemText(HWindow, IDE_TARGETPATH, initDir, MAX_PATH);
            char path[MAX_PATH];
            GetWindowText(HWindow, path, MAX_PATH); // bude mit stejne caption
            if (SalamanderGeneral->GetTargetDirectory(HWindow, HWindow, path,
                                                      LoadStr(IDS_SELECTTARGETDIR), path,
                                                      FALSE, initDir))
            {
                SetDlgItemText(HWindow, IDE_TARGETPATH, path);
            }
            return TRUE;
        }

        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_MAXCONCURRENTCON || LOWORD(wParam) == IDC_SRVSPEEDLIMIT ||
             LOWORD(wParam) == IDC_USEKEEPALIVE || LOWORD(wParam) == IDC_ENCRYPTCONTROLCONN))
        {
            if (LOWORD(wParam) == IDC_ENCRYPTCONTROLCONN)
                CheckDlgButton(HWindow, IDC_ENCRYPTDATACONN, IsDlgButtonChecked(HWindow, IDC_ENCRYPTCONTROLCONN));
            EnableControls();
        }

        if (TmpFTPProxyServerList != NULL)
        {
            switch (LOWORD(wParam))
            {
            case IDB_ADDPROXYSRV:
            {
                TmpFTPProxyServerList->AddProxyServer(HWindow, GetDlgItem(HWindow, IDC_PROXYSERVER));
                return TRUE;
            }

            case CM_EDITPROXYSRV:
            {
                TmpFTPProxyServerList->EditProxyServer(HWindow, GetDlgItem(HWindow, IDC_PROXYSERVER), TRUE);
                return TRUE;
            }

            case CM_DELETEPROXYSRV:
            {
                TmpFTPProxyServerList->DeleteProxyServer(HWindow, GetDlgItem(HWindow, IDC_PROXYSERVER), TRUE);
                return TRUE;
            }

            case CM_MOVEUPPROXYSRV:
            {
                TmpFTPProxyServerList->MoveUpProxyServer(GetDlgItem(HWindow, IDC_PROXYSERVER), TRUE);
                return TRUE;
            }

            case CM_MOVEDOWNPROXYSRV:
            {
                TmpFTPProxyServerList->MoveDownProxyServer(GetDlgItem(HWindow, IDC_PROXYSERVER), TRUE);
                return TRUE;
            }
            }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CRenameDlg
//

CRenameDlg::CRenameDlg(HWND parent, char* name, BOOL newServer, BOOL addBookmark,
                       BOOL serverTypes, char* copyFromName)
    : CCenteredDialog(HLanguage, newServer ? IDD_NEWBOOKMARKDLG : IDD_RENAMEDLG, parent)
{
    Name = name;
    NewServer = newServer;
    CopyDataFromFocusedServer = FALSE;
    AddBookmark = addBookmark;
    ServerTypes = serverTypes;
    CopyFromName = copyFromName;
}

void CRenameDlg::Validate(CTransferInfo& ti)
{
    char buf[BOOKMSRVTYPE_MAX_SIZE];
    ti.EditLine(IDE_NAME, buf, ServerTypes ? SERVERTYPE_MAX_SIZE - 1 : BOOKMARKNAME_MAX_SIZE);
    if (buf[0] == 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MAYNOTBEEMPTY),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_NAME);
    }
}

void CRenameDlg::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDE_NAME, Name, ServerTypes ? SERVERTYPE_MAX_SIZE - 1 : BOOKMARKNAME_MAX_SIZE);
    if (NewServer)
        ti.CheckBox(IDC_COPYFOCUSEDSRV, CopyDataFromFocusedServer);
}

INT_PTR
CRenameDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRenameDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG)
    {
        if (ServerTypes)
            SetWindowText(HWindow, LoadStr(NewServer ? IDS_SRVTYPENEWTITLE : IDS_SRVTYPERENAMETITLE));
        char buf[BOOKMSRVTYPE_MAX_SIZE + 200];
        if (AddBookmark)
        {
            SetWindowText(HWindow, LoadStr(IDS_ADDBOOKMARKTITLE));
            SetDlgItemText(HWindow, IDT_SUBJECT, LoadStr(IDS_ADDBOOKMARKTEXT));
        }
        else
        {
            if (!NewServer) // rename
            {
                sprintf(buf, LoadStr(IDS_RENAMESRVTO), Name);
                SetDlgItemText(HWindow, IDT_SUBJECT, buf);
            }
            else // new
            {
                if (!ServerTypes || CopyFromName != NULL)
                {
                    char checkboxName[BOOKMSRVTYPE_MAX_SIZE];
                    lstrcpyn(checkboxName, ServerTypes ? CopyFromName : Name,
                             ServerTypes ? SERVERTYPE_MAX_SIZE - 1 : BOOKMARKNAME_MAX_SIZE);
                    SalamanderGeneral->DuplicateAmpersands(checkboxName, ServerTypes ? SERVERTYPE_MAX_SIZE - 1 : BOOKMARKNAME_MAX_SIZE);
                    sprintf(buf, LoadStr(ServerTypes ? IDS_SRVTYPECOPYFROM : IDS_COPYDATAFROM),
                            checkboxName[0] != 0 ? checkboxName : LoadStr(IDS_QUICKCONNECT));
                    SetDlgItemText(HWindow, IDC_COPYFOCUSEDSRV, buf);
                }
                else // new server type + empty list = nutne schovat/disablovat checkbox "copy from"
                {
                    ShowWindow(GetDlgItem(HWindow, IDC_COPYFOCUSEDSRV), SW_HIDE);
                }
                if (!ServerTypes && !CopyDataFromFocusedServer)
                    Name[0] = 0; // prazdne jmeno pro novy server (bude se nastavovat v Transfer())
                if (ServerTypes)
                    SetDlgItemText(HWindow, IDT_NEWNAME, LoadStr(IDS_SRVTYPENEWSUBJECT));
            }
        }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CEnterStrDlg
//

CEnterStrDlg::CEnterStrDlg(HWND parent, const char* title, const char* text, char* data,
                           int dataSize, BOOL hideChars, const char* connectingToAs, BOOL allowEmpty)
    : CCenteredDialog(HLanguage, IDD_ENTERSTRDLG, parent)
{
    Title = title;
    Text = text;
    Data = data;
    DataSize = dataSize;
    HideChars = hideChars;
    ConnectingToAs = connectingToAs;
    AllowEmpty = allowEmpty;
}

void CEnterStrDlg::Validate(CTransferInfo& ti)
{
    if (!AllowEmpty && SendDlgItemMessage(HWindow, IDE_STRING, WM_GETTEXTLENGTH, 0, 0) == 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MAYNOTBEEMPTY),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_STRING);
    }
}

void CEnterStrDlg::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDE_STRING, Data, DataSize);
}

INT_PTR
CEnterStrDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CEnterStrDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG)
    {
        if (!HideChars)
            SendDlgItemMessage(HWindow, IDE_STRING, EM_SETPASSWORDCHAR, NULL, 0);
        if (Title != NULL)
            SetWindowText(HWindow, Title);
        if (Text != NULL)
            SetDlgItemText(HWindow, IDT_STRING, Text);
        if (ConnectingToAs != NULL)
            SetDlgItemText(HWindow, IDT_CONNECTINGTOAS, ConnectingToAs);
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CLoginErrorDlg
//

CLoginErrorDlg::CLoginErrorDlg(HWND parent, const char* serverReply,
                               CProxyScriptParams* proxyScriptParams,
                               const char* connectingTo, const char* title,
                               const char* retryWithoutAskingText,
                               const char* errorTitle, BOOL disableUser,
                               BOOL hideApplyToAll, BOOL proxyUsed)
    : CCenteredDialog(HLanguage, proxyUsed ? IDD_LOGINERRORDLGEX : IDD_LOGINERRORDLG, parent)
{
    ServerReply = serverReply;
    ProxyScriptParams = proxyScriptParams;
    RetryWithoutAsking = FALSE;
    LoginChanged = FALSE;
    ConnectingTo = connectingTo;
    Title = title;
    RetryWithoutAskingText = retryWithoutAskingText;
    ErrorTitle = errorTitle;
    DisableUser = disableUser;
    HideApplyToAll = hideApplyToAll;
    ApplyToAll = TRUE;
    ProxyUsed = proxyUsed;
}

void CLoginErrorDlg::Validate(CTransferInfo& ti)
{
    if (SendDlgItemMessage(HWindow, IDE_USERNAME, WM_GETTEXTLENGTH, 0, 0) == 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MAYNOTBEEMPTY),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_USERNAME);
    }
}

void CLoginErrorDlg::Transfer(CTransferInfo& ti)
{
    CProxyScriptParams proxyScriptParamsBackup;

    if (ti.Type == ttDataFromWindow)
        proxyScriptParamsBackup = *ProxyScriptParams;
    ti.EditLine(IDE_USERNAME, ProxyScriptParams->User, USER_MAX_SIZE);
    ti.EditLine(IDE_PASSWORD, ProxyScriptParams->Password, PASSWORD_MAX_SIZE);
    ti.EditLine(IDE_ACCOUNT, ProxyScriptParams->Account, ACCOUNT_MAX_SIZE);
    if (ProxyUsed)
    {
        ti.EditLine(IDE_PROXYUSER, ProxyScriptParams->ProxyUser, USER_MAX_SIZE);
        ti.EditLine(IDE_PROXYPASSWD, ProxyScriptParams->ProxyPassword, PASSWORD_MAX_SIZE);
    }
    ti.CheckBox(IDC_RETRYWITHOUTASK, RetryWithoutAsking);
    if (!HideApplyToAll)
        ti.CheckBox(IDC_APPLYTOALL, ApplyToAll);

    if (ti.Type == ttDataFromWindow)
    {
        LoginChanged = (strcmp(proxyScriptParamsBackup.User, ProxyScriptParams->User) != 0 ||
                        strcmp(proxyScriptParamsBackup.Password, ProxyScriptParams->Password) != 0 ||
                        strcmp(proxyScriptParamsBackup.Account, ProxyScriptParams->Account) != 0 ||
                        ProxyUsed && strcmp(proxyScriptParamsBackup.ProxyUser, ProxyScriptParams->ProxyUser) != 0 ||
                        ProxyUsed && strcmp(proxyScriptParamsBackup.ProxyPassword, ProxyScriptParams->ProxyPassword) != 0);
    }
}

INT_PTR
CLoginErrorDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CLoginErrorDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    if (uMsg == WM_INITDIALOG)
    {
        if (Title != NULL)
            SetWindowText(HWindow, Title);

        SetDlgItemText(HWindow, IDE_SERVERREPLY, ServerReply);
        if (ConnectingTo != NULL)
            SetDlgItemText(HWindow, IDT_CONNECTINGTO, ConnectingTo);
        if (RetryWithoutAskingText != NULL)
            SetDlgItemText(HWindow, IDC_RETRYWITHOUTASK, RetryWithoutAskingText);
        if (ErrorTitle != NULL)
            SetDlgItemText(HWindow, IDT_ERRORTITLE, ErrorTitle);

        if (HideApplyToAll)
        {
            // zrusime "apply to all" checkbox
            RECT r1;
            GetWindowRect(GetDlgItem(HWindow, IDC_APPLYTOALL), &r1);
            RECT r2;
            GetWindowRect(GetDlgItem(HWindow, IDC_RETRYWITHOUTASK), &r2);
            int delta = r1.bottom - r2.bottom;
            DestroyWindow(GetDlgItem(HWindow, IDC_APPLYTOALL));

            // zmensime dialog
            RECT windowR;
            GetWindowRect(HWindow, &windowR);
            SetWindowPos(HWindow, NULL, 0, 0, windowR.right - windowR.left, windowR.bottom - windowR.top - delta,
                         SWP_NOZORDER | SWP_NOMOVE);
        }

        if (DisableUser)
        {
            EnableWindow(GetDlgItem(HWindow, IDE_USERNAME), FALSE);
            SetFocus(GetDlgItem(HWindow, IDE_PASSWORD)); // chceme svuj vlastni fokus
            CCenteredDialog::DialogProc(uMsg, wParam, lParam);
            return FALSE;
        }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfigPageConfirmations
//

CConfigPageConfirmations::CConfigPageConfirmations() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGCONFIRMATIONS, IDD_CFGCONFIRMATIONS, PSP_HASHELP, NULL)
{
}

void CConfigPageConfirmations::Transfer(CTransferInfo& ti)
{
    int showCloseCon = !Config.AlwaysNotCloseCon;
    ti.CheckBox(IDC_ALWAYSNOTCLOSE, showCloseCon);
    Config.AlwaysNotCloseCon = !showCloseCon;
    int showDisconnect = !Config.AlwaysDisconnect;
    ti.CheckBox(IDC_ALWAYSDISCONNECT, showDisconnect);
    Config.AlwaysDisconnect = !showDisconnect;
    int showReconnect = !Config.AlwaysReconnect;
    ti.CheckBox(IDC_ALWAYSRECONNECT, showReconnect);
    Config.AlwaysReconnect = !showReconnect;
    ti.CheckBox(IDC_WARNWHENCONLOST, Config.WarnWhenConLost);
    int showOverwrite = !Config.AlwaysOverwrite;
    ti.CheckBox(IDC_ALWAYSOVERWRITE, showOverwrite);
    Config.AlwaysOverwrite = !showOverwrite;
    ti.CheckBox(IDC_HINTLISTHIDDENFILES, Config.HintListHiddenFiles);
}

//
// ****************************************************************************
// CConfigPageAdvanced
//

CConfigPageAdvanced::CConfigPageAdvanced() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGADVANCED, IDD_CFGADVANCED, PSP_HASHELP, NULL)
{
}

void CConfigPageAdvanced::Validate(CTransferInfo& ti)
{
    int num;
    int arr[] = {IDE_SRVREPLIESTIMEOUT, IDE_DELAYBETWCONRETR, IDE_CONNECTRETRIES,
                 IDE_NODATATRTIMEOUT, IDE_RESUMEMINFILESIZE, IDE_RESUMEOVERLAP, -1};
    BOOL gzthzero[] = {TRUE, FALSE, FALSE, TRUE, TRUE, FALSE, -1}; // testujeme > 0 (TRUE) nebo >= 0 (FALSE)
    int i;
    for (i = 0; arr[i] != -1; i++)
    {
        ti.EditLine(arr[i], num);
        if (!ti.IsGood())
            return; // uz nastala chyba
        if (gzthzero[i] && num <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(arr[i]);
            return;
        }
        if (!gzthzero[i] && num < 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEPOSITIVE),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(arr[i]);
            return;
        }
    }

    ti.EditLine(IDE_MEMCACHESIZELIMIT, num);
    if (!ti.IsGood())
        return; // uz nastala chyba
    if (num < 100 || num > 1 * 1024 * 1024)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_INVALIDMEMCACHESIZE),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_MEMCACHESIZELIMIT);
        return;
    }

    ti.EditLine(IDE_RESUMEOVERLAP, num);
    if (!ti.IsGood())
        return; // uz nastala chyba
    if (num < 0 || num > 1024 * 1024 * 1024)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_INVALIDRESUMEOVERLAP),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_RESUMEOVERLAP);
        return;
    }
}

void CConfigPageAdvanced::Transfer(CTransferInfo& ti)
{
    int num = (int)(Config.CacheMaxSize / CQuadWord(1024, 0)).Value;
    ti.EditLine(IDE_MEMCACHESIZELIMIT, num);
    if (ti.Type == ttDataFromWindow)
        Config.CacheMaxSize = CQuadWord(num, 0) * CQuadWord(1024, 0);

    HANDLES(EnterCriticalSection(&Config.ConParamsCS));
    ti.EditLine(IDE_SRVREPLIESTIMEOUT, Config.ServerRepliesTimeout);
    ti.EditLine(IDE_DELAYBETWCONRETR, Config.DelayBetweenConRetries);
    ti.EditLine(IDE_CONNECTRETRIES, Config.ConnectRetries);
    ti.EditLine(IDE_RESUMEMINFILESIZE, Config.ResumeMinFileSize);
    ti.EditLine(IDE_RESUMEOVERLAP, Config.ResumeOverlap);
    ti.EditLine(IDE_NODATATRTIMEOUT, Config.NoDataTransferTimeout);
    HANDLES(LeaveCriticalSection(&Config.ConParamsCS));
}

//
// ****************************************************************************
// CConfigPageLogs
//

CConfigPageLogs::CConfigPageLogs() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGLOGS, IDD_CFGLOGS, PSP_HASHELP, NULL)
{
    LastLogMaxSize = -1;
    LogMaxSizeBuf[0] = 0;
    LastMaxClosedConLogs = -1;
    MaxClosedConLogsBuf[0] = 0;
}

void CConfigPageLogs::Validate(CTransferInfo& ti)
{
    int enable;
    ti.CheckBox(IDC_ENABLELOGGING, enable);
    if (enable)
    {
        int num;
        int arr[] = {IDE_LOGMAXSIZE, IDE_MAXCLOSEDCONLOGS, -1};
        int arr2[] = {IDC_LOGMAXSIZE, IDC_MAXCLOSEDCONLOGS};
        int canBeZero[] = {FALSE, TRUE};
        int i;
        for (i = 0; arr[i] != -1; i++)
        {
            int used;
            ti.CheckBox(arr2[i], used);
            if (used)
            {
                ti.EditLine(arr[i], num);
                if (!ti.IsGood())
                    return; // uz nastala chyba
                if (num < 0 || (!canBeZero[i] && num == 0))
                {
                    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(canBeZero[i] ? IDS_MUSTBEPOSITIVE : IDS_MUSTBEGRTHANZERO),
                                                     LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    ti.ErrorOn(arr[i]);
                    return;
                }
            }
        }
    }
}

void CConfigPageLogs::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_ENABLELOGGING, Config.EnableLogging);
    ti.CheckBox(IDC_LOGMAXSIZE, Config.UseLogMaxSize);
    ti.CheckBox(IDC_MAXCLOSEDCONLOGS, Config.UseMaxClosedConLogs);
    ti.CheckBox(IDC_DISABLELOGWORKERS, Config.DisableLoggingOfWorkers);

    if (ti.Type == ttDataFromWindow)
    {
        if (Config.EnableLogging)
        {
            if (Config.UseLogMaxSize)
            {
                ti.EditLine(IDE_LOGMAXSIZE, Config.LogMaxSize);
                Config.LogMaxSize = (Config.LogMaxSize * 1024) / 1024; // orizneme pripadne vetsi cislo
                if (Config.LogMaxSize <= 0)
                    Config.LogMaxSize = 1;
            }
            if (Config.UseMaxClosedConLogs)
                ti.EditLine(IDE_MAXCLOSEDCONLOGS, Config.MaxClosedConLogs);
        }
        Logs.ConfigChanged();
    }
}

void CConfigPageLogs::EnableControls()
{
    if (IsDlgButtonChecked(HWindow, IDC_ENABLELOGGING) == BST_CHECKED)
    {
        HWND hwnd = GetDlgItem(HWindow, IDC_LOGMAXSIZE);
        if (!IsWindowEnabled(hwnd))
            EnableWindow(hwnd, TRUE);
        hwnd = GetDlgItem(HWindow, IDC_MAXCLOSEDCONLOGS);
        if (!IsWindowEnabled(hwnd))
            EnableWindow(hwnd, TRUE);
        hwnd = GetDlgItem(HWindow, IDC_DISABLELOGWORKERS);
        if (!IsWindowEnabled(hwnd))
            EnableWindow(hwnd, TRUE);
        CheckboxEditLineInteger(HWindow, IDC_LOGMAXSIZE, IDE_LOGMAXSIZE, &LastLogMaxSize, LogMaxSizeBuf,
                                Config.LogMaxSize, 0, 0);
        CheckboxEditLineInteger(HWindow, IDC_MAXCLOSEDCONLOGS, IDE_MAXCLOSEDCONLOGS, &LastMaxClosedConLogs, MaxClosedConLogsBuf,
                                Config.MaxClosedConLogs, 0, 0);
    }
    else
    {
        if (LastLogMaxSize == -1)
        {
            CheckboxEditLineInteger(HWindow, IDC_LOGMAXSIZE, IDE_LOGMAXSIZE, &LastLogMaxSize, LogMaxSizeBuf,
                                    Config.LogMaxSize, 0, 0);
        }
        if (LastMaxClosedConLogs == -1)
        {
            CheckboxEditLineInteger(HWindow, IDC_MAXCLOSEDCONLOGS, IDE_MAXCLOSEDCONLOGS, &LastMaxClosedConLogs, MaxClosedConLogsBuf,
                                    Config.MaxClosedConLogs, 0, 0);
        }
        EnableWindow(GetDlgItem(HWindow, IDC_LOGMAXSIZE), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_MAXCLOSEDCONLOGS), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDE_LOGMAXSIZE), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDE_MAXCLOSEDCONLOGS), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_DISABLELOGWORKERS), FALSE);
    }
}

INT_PTR
CConfigPageLogs::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigPageLogs::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        INT_PTR ret = CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
        EnableControls();
        return ret;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_ENABLELOGGING ||
             LOWORD(wParam) == IDC_LOGMAXSIZE ||
             LOWORD(wParam) == IDC_MAXCLOSEDCONLOGS))
        {
            EnableControls();
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CProxyServerDlg
//

CProxyServerDlg::CProxyServerDlg(HWND parent, CFTPProxyServerList* tmpFTPProxyServerList,
                                 CFTPProxyServer* proxy, BOOL edit)
    : CCenteredDialog(HLanguage, IDD_PROXYSERVER, IDD_PROXYSERVER, parent)
{
    TmpFTPProxyServerList = tmpFTPProxyServerList;
    Proxy = proxy;
    Edit = edit;
}

void CProxyServerDlg::Validate(CTransferInfo& ti)
{
    char proxyName[200];
    ti.EditLine(IDE_PRXSRV_NAME, proxyName, 200);
    if (!TmpFTPProxyServerList->IsProxyNameOK(Proxy, proxyName))
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_INVPROXYNAME),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_PRXSRV_NAME);
        return;
    }

    HWND combo = GetDlgItem(HWindow, IDC_PRXSRV_TYPE);
    int proxyType = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (proxyType == CB_ERR || proxyType < 0 || proxyType > fpstOwnScript)
        proxyType = fpstSocks4;
    BOOL proxyHostNeeded = HaveHostAndPort((CFTPProxyServerType)proxyType);
    char proxyHost[HOST_MAX_SIZE];
    proxyHost[0] = 0;
    if (proxyHostNeeded)
    {
        ti.EditLine(IDE_PRXSRV_ADDRESS, proxyHost, HOST_MAX_SIZE);
        if (strchr(proxyHost, ':') != NULL)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_INVPROXYADDRESS),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_PRXSRV_ADDRESS);
            return;
        }

        int port;
        ti.EditLine(IDE_PRXSRV_PORT, port);
        if (!ti.IsGood())
            return; // uz nastala chyba
        if (port <= 0 || port >= 65536)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_PORTISUSHORT),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_PRXSRV_PORT);
            return;
        }
    }

    char proxyScript[PROXYSCRIPT_MAX_SIZE];
    if (proxyType == fpstOwnScript)
    {
        ti.EditLine(IDE_PRXSRV_SCRIPT, proxyScript, PROXYSCRIPT_MAX_SIZE);
        const char* errPos = NULL;
        char errDescr[300];
        if (!ProcessProxyScript(proxyScript, &errPos, -1, NULL, NULL, NULL, NULL, NULL, errDescr, &proxyHostNeeded))
        {
            SalamanderGeneral->SalMessageBox(HWindow, errDescr,
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            // oznacime misto chyby v textu skriptu
            SendDlgItemMessage(HWindow, IDE_PRXSRV_SCRIPT, EM_SETSEL, (WPARAM)(errPos - proxyScript),
                               (LPARAM)(errPos - proxyScript));
            SendDlgItemMessage(HWindow, IDE_PRXSRV_SCRIPT, EM_SCROLLCARET, 0, 0); // scroll caret into view
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_PRXSRV_SCRIPT), TRUE);
            ti.ErrorOn(IDE_PRXSRV_SCRIPT);
            return;
        }
    }
    if (proxyHostNeeded && proxyHost[0] == 0)
    {
        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_EMPTYPROXYADDRESS),
                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_PRXSRV_ADDRESS);
        return;
    }

    if (proxyType == fpstFTP_transparent)
    {
        char proxyUser[USER_MAX_SIZE];
        ti.EditLine(IDE_PRXSRV_USER, proxyUser, USER_MAX_SIZE);
        if (proxyUser[0] == 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_EMPTYUSERFORTRANSPRX),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_PRXSRV_USER);
            return;
        }
    }
}

void CProxyServerDlg::Transfer(CTransferInfo& ti)
{
    char proxyName[200];
    int proxyType;
    char proxyHost[HOST_MAX_SIZE];
    int proxyPort;
    char proxyUser[USER_MAX_SIZE];
    char proxyPlainPassword[PASSWORD_MAX_SIZE];
    int saveProxyPassword;
    char proxyScript[PROXYSCRIPT_MAX_SIZE];

    lstrcpyn(proxyName, HandleNULLStr(Proxy->ProxyName), 200);
    proxyType = Proxy->ProxyType;
    lstrcpyn(proxyHost, HandleNULLStr(Proxy->ProxyHost), HOST_MAX_SIZE);
    proxyPort = Proxy->ProxyPort;
    lstrcpyn(proxyUser, HandleNULLStr(Proxy->ProxyUser), USER_MAX_SIZE);

    if (ti.Type == ttDataToWindow)
    {
        BOOL lockedPassword = TRUE;

        proxyPlainPassword[0] = 0;
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        if (!Proxy->SaveProxyPassword || !passwordManager->IsUsingMasterPassword() || passwordManager->IsMasterPasswordSet())
        {
            if (Proxy->ProxyEncryptedPassword != NULL)
            { // scrambled/encrypted -> plain
                char* plainPassword;
                if (passwordManager->DecryptPassword(Proxy->ProxyEncryptedPassword, Proxy->ProxyEncryptedPasswordSize, &plainPassword))
                {
                    lstrcpyn(proxyPlainPassword, plainPassword, PASSWORD_MAX_SIZE);
                    memset(plainPassword, 0, lstrlen(plainPassword));
                    SalamanderGeneral->Free(plainPassword);
                    lockedPassword = FALSE;
                }
            }
            else
                lockedPassword = FALSE;
        }
        ShowHidePasswordControls(lockedPassword, FALSE);
    }

    saveProxyPassword = Proxy->SaveProxyPassword;
    lstrcpyn(proxyScript, HandleNULLStr(Proxy->ProxyScript), PROXYSCRIPT_MAX_SIZE);

    ti.EditLine(IDE_PRXSRV_NAME, proxyName, 200);
    HWND combo;
    if (ti.GetControl(combo, IDC_PRXSRV_TYPE))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(combo, CB_RESETCONTENT, 0, 0);

            int type = fpstSocks4 - 1;
            while (type != fpstOwnScript)
                SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)GetProxyTypeName((CFTPProxyServerType)++type));

            SendMessage(combo, CB_SETCURSEL, proxyType, 0);
        }
        else
        {
            int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR && sel >= 0 && sel <= fpstOwnScript)
                proxyType = sel;
            else
                proxyType = fpstSocks4;
        }
    }
    if (ti.Type == ttDataToWindow || HaveHostAndPort((CFTPProxyServerType)proxyType))
    {
        ti.EditLine(IDE_PRXSRV_ADDRESS, proxyHost, HOST_MAX_SIZE);
        ti.EditLine(IDE_PRXSRV_PORT, proxyPort);
    }
    else
    {
        proxyHost[0] = 0;
        proxyPort = 0;
    }
    ti.EditLine(IDE_PRXSRV_USER, proxyUser, USER_MAX_SIZE);
    if (ti.Type == ttDataToWindow || HavePassword((CFTPProxyServerType)proxyType))
    {
        ti.EditLine(IDE_PRXSRV_PASSWD, proxyPlainPassword, PASSWORD_MAX_SIZE);
        ti.CheckBox(IDC_PRXSRV_SAVEPASSWD, saveProxyPassword);
    }
    else
    {
        proxyPlainPassword[0] = 0;
        saveProxyPassword = FALSE;
    }

    if (proxyType == fpstOwnScript)
        ti.EditLine(IDE_PRXSRV_SCRIPT, proxyScript, PROXYSCRIPT_MAX_SIZE);
    else
    {
        if (ti.Type == ttDataToWindow)
            SendMessage(GetDlgItem(HWindow, IDE_PRXSRV_SCRIPT), EM_LIMITTEXT, PROXYSCRIPT_MAX_SIZE - 1, 0);
        else
            proxyScript[0] = 0;
    }
    if (ti.Type == ttDataToWindow)
        EnableControls(proxyType != fpstOwnScript, FALSE);
    if (ti.Type == ttDataFromWindow)
    {
        BOOL deallocPassword = FALSE;                                 // FALSE = heslo zatim beze zmeny
        BYTE* proxyEncryptedPassword = Proxy->ProxyEncryptedPassword; // muze byt i jen scrambled
        int proxyEncryptedPasswordSize = Proxy->ProxyEncryptedPasswordSize;
        if (!IsWindowVisible(GetDlgItem(HWindow, IDB_PRXSRV_PASSWD_CHANGE)))
        {
            if (proxyPlainPassword[0] == 0) // prazdne heslo ukladame jako NULL
            {
                proxyEncryptedPassword = NULL;
                proxyEncryptedPasswordSize = 0;
            }
            else
            {
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                BOOL encrypt = saveProxyPassword && passwordManager->IsUsingMasterPassword() && passwordManager->IsMasterPasswordSet();
                passwordManager->EncryptPassword(proxyPlainPassword, &proxyEncryptedPassword, &proxyEncryptedPasswordSize, encrypt);
                deallocPassword = TRUE;
            }
        }
        TmpFTPProxyServerList->SetProxyServer(Proxy,
                                              Proxy->ProxyUID,
                                              proxyName,
                                              (CFTPProxyServerType)proxyType,
                                              GetStrOrNULL(proxyHost),
                                              proxyPort,
                                              GetStrOrNULL(proxyUser),
                                              proxyEncryptedPassword,
                                              proxyEncryptedPasswordSize,
                                              saveProxyPassword,
                                              GetStrOrNULL(proxyScript));

        if (deallocPassword && proxyEncryptedPassword != NULL)
        {
            // uvolnime buffer alokovany v EncryptPassword()
            memset(proxyEncryptedPassword, 0, proxyEncryptedPasswordSize);
            SalamanderGeneral->Free(proxyEncryptedPassword);
        }
        memset(proxyPlainPassword, 0, lstrlen(proxyPlainPassword));
    }
}

void CProxyServerDlg::EnableControls(BOOL initScriptText, BOOL initProxyPort)
{
    HWND edit = GetDlgItem(HWindow, IDE_PRXSRV_SCRIPT);
    HWND combo = GetDlgItem(HWindow, IDC_PRXSRV_TYPE);
    int type = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (type == CB_ERR || type < 0 || type > fpstOwnScript)
        type = fpstSocks4;
    if (initScriptText)
    {
        SetWindowText(edit, type == fpstOwnScript ? "Connect to:" : GetProxyScriptText((CFTPProxyServerType)type, TRUE));
    }
    if (initProxyPort)
    {
        char num[20];
        HWND portEdit = GetDlgItem(HWindow, IDE_PRXSRV_PORT);
        SetWindowText(portEdit, _itoa(GetProxyDefaultPort((CFTPProxyServerType)type), num, 10));
    }

    DWORD style = GetWindowLong(edit, GWL_STYLE);
    if (type != fpstOwnScript)
    {
        if ((style & ES_READONLY) == 0)
            SendMessage(edit, EM_SETREADONLY, TRUE, 0);
    }
    else
    {
        if ((style & ES_READONLY) != 0)
            SendMessage(edit, EM_SETREADONLY, FALSE, 0);
    }
    BOOL enablePassword = HavePassword((CFTPProxyServerType)type);
    if ((IsWindowEnabled(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD)) != 0) != enablePassword)
        EnableWindow(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD), enablePassword);
    if ((IsWindowEnabled(GetDlgItem(HWindow, IDC_PRXSRV_SAVEPASSWD)) != 0) != enablePassword)
        EnableWindow(GetDlgItem(HWindow, IDC_PRXSRV_SAVEPASSWD), enablePassword);

    BOOL enableHostAndPort = HaveHostAndPort((CFTPProxyServerType)type);
    if ((IsWindowEnabled(GetDlgItem(HWindow, IDE_PRXSRV_ADDRESS)) != 0) != enableHostAndPort)
        EnableWindow(GetDlgItem(HWindow, IDE_PRXSRV_ADDRESS), enableHostAndPort);
    if ((IsWindowEnabled(GetDlgItem(HWindow, IDE_PRXSRV_PORT)) != 0) != enableHostAndPort)
        EnableWindow(GetDlgItem(HWindow, IDE_PRXSRV_PORT), enableHostAndPort);

    EnableWindow(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD_LOCKED), FALSE);
}

LRESULT
CProxyScriptControlWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_GETDLGCODE: // postarame se, aby nam porad neoznacovali text v editboxu
    {
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        return (ret & (~DLGC_HASSETSEL));
    }

    case WM_RBUTTONDOWN:
    {
        DWORD ret = (DWORD)SendMessage(HWindow, EM_CHARFROMPOS, 0, lParam);
        DWORD i = (unsigned short)LOWORD(ret);
        if (GetFocus() != HWindow)
        {
            SendMessage(GetParent(HWindow), WM_NEXTDLGCTL, (WPARAM)HWindow, TRUE);
            SendMessage(HWindow, EM_SETSEL, i, i); // vzdy zmena pozice caretu
        }
        else
        {
            DWORD start, end;
            SendMessage(HWindow, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            if ((i < start || i >= end) && (i < end || i >= start))
                SendMessage(HWindow, EM_SETSEL, i, i); // klik mimo selectionu -> zmena pozice caretu
        }
        break;
    }

    case WM_CONTEXTMENU:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x == -1 || y == -1)
        {
            POINT p;
            if (GetCaretPos(&p))
            {
                ClientToScreen(HWindow, &p);
                x = p.x;
                y = p.y;
            }
            else
            {
                RECT r;
                GetWindowRect(HWindow, &r);
                r.right = r.left;
                r.bottom = r.top;
                SalamanderGeneral->MultiMonEnsureRectVisible(&r, FALSE);
                x = r.left;
                y = r.top;
            }
        }

        HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_PRXSRVSCRIPTMENU));
        if (main != NULL)
        {
            HMENU subMenu = GetSubMenu(main, 0);
            if (subMenu != NULL)
            {
                BOOL canModify = (GetWindowLong(GetDlgItem(GetParent(HWindow), IDE_PRXSRV_SCRIPT), GWL_STYLE) & ES_READONLY) == 0;
                EnableMenuItem(subMenu, 0, MF_BYPOSITION | (canModify ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
                MyEnableMenuItem(subMenu, CM_PSS_UNDO, canModify && SendMessage(HWindow, EM_CANUNDO, 0, 0));
                DWORD start, end;
                SendMessage(HWindow, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
                MyEnableMenuItem(subMenu, CM_PSS_CUT, canModify && start != end);
                MyEnableMenuItem(subMenu, CM_PSS_COPY, start != end);
                MyEnableMenuItem(subMenu, CM_PSS_PASTE, canModify && (IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_UNICODETEXT)));
                MyEnableMenuItem(subMenu, CM_PSS_DELETE, canModify && start != end);
                DWORD len = GetWindowTextLength(HWindow);
                MyEnableMenuItem(subMenu, CM_PSS_SELECTALL, start - end != len && end - start != len);
                DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                             x, y, HWindow, NULL);
                int strID = -1;
                switch (cmd)
                {
                case CM_PSS_UNDO:
                    SendMessage(HWindow, EM_UNDO, 0, 0);
                    break;
                case CM_PSS_CUT:
                    SendMessage(HWindow, WM_CUT, 0, 0);
                    break;
                case CM_PSS_COPY:
                    SendMessage(HWindow, WM_COPY, 0, 0);
                    break;
                case CM_PSS_PASTE:
                    SendMessage(HWindow, WM_PASTE, 0, 0);
                    break;
                case CM_PSS_DELETE:
                    SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM) "");
                    break;
                case CM_PSS_SELECTALL:
                    SendMessage(HWindow, EM_SETSEL, 0, -1);
                    break;

                case CM_PSS_3XX:
                    strID = 0;
                    break;
                case CM_PSS_PROXYHOST:
                    strID = 1;
                    break;
                case CM_PSS_PROXYPORT:
                    strID = 2;
                    break;
                case CM_PSS_PROXYUSER:
                    strID = 3;
                    break;
                case CM_PSS_PROXYPASSWD:
                    strID = 4;
                    break;
                case CM_PSS_HOST:
                    strID = 5;
                    break;
                case CM_PSS_PORT:
                    strID = 6;
                    break;
                case CM_PSS_USER:
                    strID = 7;
                    break;
                case CM_PSS_PASSWD:
                    strID = 8;
                    break;
                case CM_PSS_ACCOUNT:
                    strID = 9;
                    break;
                }
                if (strID >= 0)
                {
                    const char* strArr[] = {
                        "\r\n3xx:",
                        "$(ProxyHost)",
                        "$(ProxyPort)",
                        "$(ProxyUser)",
                        "$(ProxyPassword)",
                        "$(Host)",
                        "$(Port)",
                        "$(User)",
                        "$(Password)",
                        "$(Account)",
                    };
                    if (strID < 10 /* pocet stringu v strArr - AKTUALIZOVAT !!! */)
                    {
                        const char* str = strArr[strID];
                        char buf2[PROXYSCRIPT_MAX_SIZE];
                        GetWindowText(HWindow, buf2, PROXYSCRIPT_MAX_SIZE);
                        DWORD pos = min(start, end);
                        if (pos <= strlen(buf2) &&
                            (pos == 0 || pos >= 2 && *(buf2 + pos - 2) == '\r' && *(buf2 + pos - 1) == '\n')) // zacatek radky
                        {                                                                                     // skipneme predsazeny EOL
                            if (*str == '\r')
                                str++;
                            if (*str == '\n')
                                str++;
                        }
                        SendMessage(HWindow, EM_REPLACESEL, TRUE, (LPARAM)str);
                    }
                }
            }
            DestroyMenu(main);
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void CProxyServerDlg::AlignPasswordControls()
{
    // non-password editline (s info textem) bude posunuta na misto kde zacina editline pro password; zaroven bude protazena az k tlacitku
    RECT editRect;
    GetWindowRect(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD), &editRect);
    RECT editLockedRect;
    GetWindowRect(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD_LOCKED), &editLockedRect);
    int width = editLockedRect.right - editRect.left;
    MapWindowPoints(NULL, HWindow, (POINT*)&editRect, 2);
    SetWindowPos(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD_LOCKED), NULL, editRect.left, editRect.top, width, editLockedRect.bottom - editLockedRect.top, SWP_NOZORDER);

    // editline pro password bude stejne dlouha jako ta nad ni
    GetWindowRect(GetDlgItem(HWindow, IDE_PRXSRV_USER), &editRect);
    SetWindowPos(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD), NULL, 0, 0, editRect.right - editRect.left, editRect.bottom - editRect.top, SWP_NOMOVE | SWP_NOZORDER);
}

void CProxyServerDlg::ShowHidePasswordControls(BOOL lockedPassword, BOOL focusEdit)
{
    BOOL showUnlockButton = lockedPassword;
    ShowWindow(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD), !showUnlockButton);
    ShowWindow(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD_LOCKED), showUnlockButton);
    ShowWindow(GetDlgItem(HWindow, IDB_PRXSRV_PASSWD_CHANGE), showUnlockButton);

    if (!showUnlockButton && focusEdit)
    {
        SetFocus(GetDlgItem(HWindow, IDE_PRXSRV_PASSWD));
        SendDlgItemMessage(HWindow, IDE_PRXSRV_PASSWD, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
        SendMessage(HWindow, DM_SETDEFID, IDE_PRXSRV_PASSWD, 0);
    }
}

INT_PTR
CProxyServerDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CProxyServerDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // posuneme password edit lines
        AlignPasswordControls();

        // Unlock tlacitko: vedle prikaz Unlock bude mit take Clear pro smaznuti hesla
        SalamanderGUI->AttachButton(HWindow, IDB_PRXSRV_PASSWD_CHANGE, BTF_DROPDOWN);

        // chceme od edit line s heslem dostavat na ctrl+rclick zpravu WM_APP_SHOWPASSWORD
        CPasswordEditLine* passwordEL = new CPasswordEditLine(HWindow, IDE_PRXSRV_PASSWD);

        CGUIHyperLinkAbstract* hint = SalamanderGUI->AttachHyperLink(HWindow, IDC_PRXSRV_SAVEPASSWD_HINT, STF_HYPERLINK_COLOR | STF_UNDERLINE);
        hint->SetActionPostCommand(IDC_PRXSRV_SAVEPASSWD_HINT);

        // pokud uzivatel nenastavil v Salamanderu master password, neni ukladani hesel bezpecne
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        // pokud uzivatel pouziva password manager, zmenime hlasku "it is not secure"
        if (passwordManager->IsUsingMasterPassword())
            SetDlgItemText(HWindow, IDC_PRXSRV_SAVEPASSWD_HINT, LoadStr(IDS_SAVEPASSWORD_PROTECTED));

        // zakazeme select-all pri fokusu a zajistime vlastni kontextove menu pro edit s promennymi skriptu
        CProxyScriptControlWindow* wnd = new CProxyScriptControlWindow(HWindow, IDE_PRXSRV_SCRIPT);
        if (wnd != NULL && wnd->HWindow == NULL)
            delete wnd; // nepodarilo se attachnout - nedealokuje se samo
        if (Edit)
            SetWindowText(HWindow, LoadStr(IDS_EDITPROXYSRVTITLE));
        break;
    }

    case WM_APP_SHOWPASSWORD:
    {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED |
                       MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
        params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
        params.Text = LoadStr(IDS_SHOWPASSWORD_CONFIRMATION);
        if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
        {
            CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
            // doptame se na MP i v pripade, ze ho jiz zname
            if (!passwordManager->IsUsingMasterPassword() || passwordManager->AskForMasterPassword(HWindow))
            {
                // heslo vytahneme primo z editline
                char plainPassword[PASSWORD_MAX_SIZE];
                GetWindowText((HWND)wParam, plainPassword, PASSWORD_MAX_SIZE);
                plainPassword[PASSWORD_MAX_SIZE - 1] = 0;

                char buff[1000];
                _snprintf_s(buff, _TRUNCATE, LoadStr(IDS_PASSWORDIS), plainPassword);
                params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_DEFBUTTON2 |
                               MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT;
                params.Text = buff;
                if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
                    SalamanderGeneral->CopyTextToClipboard(plainPassword, -1, FALSE, NULL);
                memset(plainPassword, 0, lstrlen(plainPassword));
                memset(buff, 0, 1000);
            }
        }
        return 0;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDB_PRXSRV_PASSWD_CHANGE) // dropdown menu na Unlock tlacitku
        {
            HMENU main = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_UNLOCKPASSWORD));
            if (main != NULL)
            {
                HMENU subMenu = GetSubMenu(main, 0);
                if (subMenu != NULL)
                {
                    CGUIMenuPopupAbstract* salMenu = SalamanderGUI->CreateMenuPopup();
                    if (salMenu != NULL)
                    {
                        salMenu->SetTemplateMenu(subMenu);

                        RECT r;
                        GetWindowRect(GetDlgItem(HWindow, (int)wParam), &r);
                        BOOL selectMenuItem = LOWORD(lParam);
                        DWORD flags = MENU_TRACK_RETURNCMD;
                        if (selectMenuItem)
                        {
                            salMenu->SetSelectedItemIndex(0);
                            flags |= MENU_TRACK_SELECT;
                        }
                        DWORD cmd = salMenu->Track(flags, r.left, r.bottom, HWindow, &r);
                        if (cmd != 0)
                            PostMessage(HWindow, WM_COMMAND, cmd, 0);
                        SalamanderGUI->DestroyMenuPopup(salMenu);
                    }
                }
                DestroyMenu(main);
            }
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_PRXSRV_TYPE:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
                EnableControls(TRUE, TRUE);
            break;
        }

        case IDC_PRXSRV_SAVEPASSWD_HINT:
        {
            // otevreme help Salamandera s napovedou k password manageru
            SalamanderGeneral->OpenHtmlHelpForSalamander(HWindow, HHCDisplayContext, HTMLHELP_SALID_PWDMANAGER, FALSE);
            break;
        }

        case IDC_PRXSRV_SAVEPASSWD:
        {
            if (HIWORD(wParam) == BN_CLICKED)
            {
                BOOL unlockVisible = IsWindowVisible(GetDlgItem(HWindow, IDB_PRXSRV_PASSWD_CHANGE));
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                if (IsDlgButtonChecked(HWindow, IDC_PRXSRV_SAVEPASSWD) == BST_CHECKED)
                {
                    // pokud uzivatel zaskrtnul "Save password" a editline s heslem je prave zobrazena, uzivatel pouziva master password a heslo neni zadane
                    if (!unlockVisible && passwordManager->IsUsingMasterPassword() && !passwordManager->IsMasterPasswordSet())
                    {
                        // doptame se na master password
                        if (!passwordManager->AskForMasterPassword(HWindow))
                            CheckDlgButton(HWindow, IDC_PRXSRV_SAVEPASSWD, BST_UNCHECKED); // pokud uzivatel nezadal validni master password, vratime checkbox do stavu pred zmenou
                    }
                }
                else
                {
                    if (unlockVisible && Proxy->ProxyEncryptedPassword == NULL)
                    { // prazdne heslo bez zapleho Save Password -> schovame Unlock a ukazeme prazdny editbox s heslem
                        CTransferInfo ti(HWindow, ttDataToWindow);
                        char emptyBuff[] = "";
                        ti.EditLine(IDE_PRXSRV_PASSWD, emptyBuff, PASSWORD_MAX_SIZE);

                        ShowHidePasswordControls(FALSE, FALSE);
                    }
                }
            }
            break;
        }

        case CM_CLEARPASSWORD:
        {
            int ret = SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CLEARPASSWORD_CONFIRMATION),
                                                       LoadStr(IDS_FTPPLUGINTITLE), MB_YESNO | MSGBOXEX_ESCAPEENABLED | /*MB_DEFBUTTON2 | */ MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT);
            if (ret == IDYES)
            {
                // uzivatel si pral smaznout heslo
                CTransferInfo ti(HWindow, ttDataToWindow);
                char emptyBuff[] = "";
                ti.EditLine(IDE_PRXSRV_PASSWD, emptyBuff, PASSWORD_MAX_SIZE);

                // vycistime save password checkbox
                BOOL clear = FALSE;
                ti.CheckBox(IDC_PRXSRV_SAVEPASSWD, clear);

                ShowHidePasswordControls(FALSE, TRUE);
            }
            break;
        }

        case CM_UNLOCKPASSWORD:
        case IDB_PRXSRV_PASSWD_CHANGE:
        {
            CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
            // bud se master password nepouziva nebo pouziva a jiz byl zadan nebo ho uzivatel zada prave ted
            if (!passwordManager->IsUsingMasterPassword() || passwordManager->IsMasterPasswordSet() || passwordManager->AskForMasterPassword(HWindow))
            {
                // pokud se MP pouziva, overime, ze s nim lze rozsifrovat toto heslo
                char* proxyPlainPassword = NULL;
                if (!passwordManager->IsUsingMasterPassword() ||
                    Proxy->ProxyEncryptedPassword != NULL && !passwordManager->DecryptPassword(Proxy->ProxyEncryptedPassword, Proxy->ProxyEncryptedPasswordSize, &proxyPlainPassword))
                {
                    int ret = SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CANNOT_DECRYPT_PASSWORD_DELETE),
                                                               LoadStr(IDS_FTPERRORTITLE), MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_DEFBUTTON2 | MB_ICONEXCLAMATION);
                    if (ret == IDNO)
                        break;
                    // uzivatel si pral smaznout heslo
                    CTransferInfo ti(HWindow, ttDataToWindow);
                    char emptyBuff[] = "";
                    ti.EditLine(IDE_PRXSRV_PASSWD, emptyBuff, PASSWORD_MAX_SIZE);
                    // vycistime save password checkbox
                    BOOL clear = FALSE;
                    ti.CheckBox(IDC_PRXSRV_SAVEPASSWD, clear);
                }
                if (proxyPlainPassword != NULL || Proxy->ProxyEncryptedPassword == NULL)
                {
                    // rozsifrovane heslo vlozime do editline
                    CTransferInfo ti(HWindow, ttDataToWindow);
                    char emptyBuff[] = "";
                    ti.EditLine(IDE_PRXSRV_PASSWD, Proxy->ProxyEncryptedPassword == NULL ? emptyBuff : proxyPlainPassword, PASSWORD_MAX_SIZE);

                    // vynulujeme buffer
                    if (proxyPlainPassword != NULL)
                    {
                        memset(proxyPlainPassword, 0, lstrlen(proxyPlainPassword));
                        SalamanderGeneral->Free(proxyPlainPassword);
                    }
                }
                ShowHidePasswordControls(FALSE, TRUE);
            }
            break;
        }
        }
        break;
    }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}
