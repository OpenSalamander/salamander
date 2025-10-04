// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

TIndirectArray<CDialog> ModelessDlgs(2, 2, dtNoDelete); // array of "Welcome Message" dialogs

void MyEnableMenuItem(HMENU subMenu, int cmd, BOOL enable)
{
    EnableMenuItem(subMenu, cmd, MF_BYCOMMAND | (enable ? MF_ENABLED : MF_GRAYED));
}

//
// ****************************************************************************
// CCenteredDialog
//

INT_PTR
CCenteredDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCenteredDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // horizontal and vertical centering of the dialog relative to the parent
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // I want the focus from DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCenteredDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//
// ****************************************************************************
// CCommonPropSheetPage
//

void CCommonPropSheetPage::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//
// ****************************************************************************
// CConfigPageGeneral
//

CConfigPageGeneral::CConfigPageGeneral() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGGENERAL, IDD_CFGGENERAL, PSP_HASHELP, NULL)
{
    LastTotSpeed = -1;
    TotSpeedBuf[0] = 0;
}

void CConfigPageGeneral::Validate(CTransferInfo& ti)
{
    // test if "total speed limit" (if used) is valid number
    int enableTotalSpeedLimit;
    ti.CheckBox(IDC_ENABLETOTSPEEDLIM, enableTotalSpeedLimit);
    double totalSpeedLimit;
    if (enableTotalSpeedLimit)
    {
        char buff[] = "%g";
        ti.EditLine(IDE_TOTALSPEEDLIMIT, totalSpeedLimit, buff);
        if (ti.IsGood() && totalSpeedLimit <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_TOTALSPEEDLIMIT);
        }
    }
}

void CConfigPageGeneral::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_WELCOMEMESSAGE, Config.ShowWelcomeMessage);
    ti.CheckBox(IDC_PRIORITYTOPANELCON, Config.PriorityToPanelConnections);
    ti.CheckBox(IDC_ENABLETOTSPEEDLIM, Config.EnableTotalSpeedLimit);

    if (ti.Type == ttDataFromWindow && Config.EnableTotalSpeedLimit)
    {
        char buff[] = "%g";
        ti.EditLine(IDE_TOTALSPEEDLIMIT, Config.TotalSpeedLimit, buff);
        if (ti.Type == ttDataFromWindow &&
            Config.TotalSpeedLimit < 0.001)
            Config.TotalSpeedLimit = 0.001; // at least 1 byte per second
    }

    ti.CheckBox(IDC_ACCEPTHEXESCSEQ, Config.ConvertHexEscSeq);
    ti.CheckBox(IDC_CLOSEOPERDLGIFSUCCESS, Config.CloseOperationDlgIfSuccessfullyFinished);
    ti.CheckBox(IDC_OPENSOLVEERRIFIDLE, Config.OpenSolveErrIfIdle);

    char passwd[PASSWORD_MAX_SIZE];
    if (ti.Type == ttDataFromWindow)
    {
        ti.EditLine(IDE_ANONYMOUSPASSWD, passwd, PASSWORD_MAX_SIZE);
        Config.SetAnonymousPasswd(passwd);
    }
    else
    {
        Config.GetAnonymousPasswd(passwd, PASSWORD_MAX_SIZE);
        ti.EditLine(IDE_ANONYMOUSPASSWD, passwd, PASSWORD_MAX_SIZE);
    }
}

void CheckboxEditLine(BOOL isInt, HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                      int checkedValInteger, double checkedValDouble, BOOL globValUsed,
                      int globValInteger, double globValDouble)
{
    char buf[31];
    int check = IsDlgButtonChecked(dlg, checkboxID);
    EnableWindow(GetDlgItem(dlg, editID), check == BST_CHECKED);
    if (*lastCheck != check)
    {
        if (*lastCheck == 1)
            GetDlgItemText(dlg, editID, valueBuf, 31);
        switch (check)
        {
        case 0:
            buf[0] = 0;
            break; // switched off (empty string)

        case 1: // enabled
        {
            if (valueBuf[0] == 0)
            {
                if (isInt)
                    sprintf(valueBuf, "%d", checkedValInteger);
                else
                    sprintf(valueBuf, "%g", checkedValDouble);
            }
            strcpy(buf, valueBuf);
            break;
        }

        default: // third state
        {
            if (globValUsed)
            {
                if (isInt)
                    sprintf(buf, "%d", globValInteger);
                else
                    sprintf(buf, "%g", globValDouble);
            }
            else
                buf[0] = 0; // not used (empty string)
            break;
        }
        }
        SetDlgItemText(dlg, editID, buf);
        *lastCheck = check;
    }
}

void CheckboxEditLineInteger(HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                             int checkedVal, BOOL globValUsed, int globVal)
{
    CheckboxEditLine(TRUE, dlg, checkboxID, editID, lastCheck, valueBuf,
                     checkedVal, 0, globValUsed, globVal, 0);
}

void CheckboxEditLineDouble(HWND dlg, int checkboxID, int editID, int* lastCheck, char* valueBuf,
                            double checkedVal, BOOL globValUsed, double globVal)
{
    CheckboxEditLine(FALSE, dlg, checkboxID, editID, lastCheck, valueBuf,
                     0, checkedVal, globValUsed, 0, globVal);
}

void CConfigPageGeneral::EnableControls()
{
    CheckboxEditLineDouble(HWindow, IDC_ENABLETOTSPEEDLIM, IDE_TOTALSPEEDLIMIT, &LastTotSpeed, TotSpeedBuf,
                           Config.TotalSpeedLimit, 0, 0);
}

INT_PTR
CConfigPageGeneral::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigPageGeneral::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
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
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_ENABLETOTSPEEDLIM)
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
// CConfigPageDefaults
//

CConfigPageDefaults::CConfigPageDefaults() : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGDEFAULTS, IDD_CFGDEFAULTS, PSP_HASHELP, NULL)
{
    LastMaxCon = -1;
    MaxConBuf[0] = 0;
    LastSrvSpeed = -1;
    SrvSpeedBuf[0] = 0;
    TmpFTPProxyServerList = NULL;
}

CConfigPageDefaults::~CConfigPageDefaults()
{
    if (TmpFTPProxyServerList != NULL)
        delete TmpFTPProxyServerList;
}

void CConfigPageDefaults::Validate(CTransferInfo& ti)
{
    // check for passive mode when using an HTTP proxy (an HTTP proxy supports only passive data transfers)
    int passiveMode;
    ti.CheckBox(IDC_PASSIVE, passiveMode);
    if (TmpFTPProxyServerList != NULL && !passiveMode)
    {
        int defaultProxySrvUID;
        ProxyComboBox(HWindow, ti, IDC_PROXYSERVER, defaultProxySrvUID, FALSE,
                      TmpFTPProxyServerList);
        if (TmpFTPProxyServerList->GetProxyType(defaultProxySrvUID) == fpstHTTP1_1)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_HTTPNEEDPASSIVETRMODE),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_PASSIVE);
            return;
        }
    }

    // test if "max. concurrent connections" (if used) is valid number
    int enableMaxConcCon;
    ti.CheckBox(IDC_MAXCONCURRENTCON, enableMaxConcCon);
    int maxConcCon;
    if (enableMaxConcCon)
    {
        ti.EditLine(IDE_MAXCONCURRENTCON, maxConcCon);
        if (!ti.IsGood())
            return; // an error has already occurred
        if (ti.IsGood() && maxConcCon <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_MAXCONCURRENTCON);
            return;
        }
    }

    // test if "server speed limit" (if used) is valid number
    int enableSrvSpeedLimit;
    ti.CheckBox(IDC_SRVSPEEDLIMIT, enableSrvSpeedLimit);
    double srvSpeedLimit;
    if (enableSrvSpeedLimit)
    {
        char buff[] = "%g";
        ti.EditLine(IDE_SRVSPEEDLIMIT, srvSpeedLimit, buff);
        if (!ti.IsGood())
            return; // an error has already occurred
        if (ti.IsGood() && srvSpeedLimit <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEGRTHANZERO),
                                             LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_SRVSPEEDLIMIT);
            return;
        }
    }

    int num;
    int arr[] = {IDE_KEEPALIVEEVERY, IDE_KEEPALIVESTOPAFTER, -1};
    int i;
    for (i = 0; arr[i] != -1; i++)
    {
        ti.EditLine(arr[i], num);
        if (!ti.IsGood())
            return; // an error has already occurred
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

    // test if "ASCII mask" (if used) is valid
    int transferMode = trmBinary;
    ti.RadioButton(IDC_AUTODETECT, trmAutodetect, transferMode);
    if (transferMode == trmAutodetect)
    {
        char masks[MAX_GROUPMASK];
        ti.EditLine(IDE_ASCIIMASKS, masks, MAX_GROUPMASK);

        CSalamanderMaskGroup* maskGroup = SalamanderGeneral->AllocSalamanderMaskGroup();
        if (maskGroup != NULL)
        {
            maskGroup->SetMasksString(masks, FALSE);
            int err;
            if (!maskGroup->PrepareMasks(err))
            {
                SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX),
                                                 LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDE_ASCIIMASKS);
                PostMessage(GetDlgItem(HWindow, IDE_ASCIIMASKS), EM_SETSEL, err, err); // marking the error position
            }
            SalamanderGeneral->FreeSalamanderMaskGroup(maskGroup);
        }
        if (!ti.IsGood())
            return; // an error occurred
    }
}

// support for combo boxes with proxy servers
void ProxyComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, int& proxyUID, BOOL addDefault,
                   CFTPProxyServerList* proxyServerList)
{
    CALL_STACK_MESSAGE2("ProxyComboBox(, , %d,)", ctrlID);
    HWND hwnd;
    if (ti.GetControl(hwnd, ctrlID))
    {
        if (ti.Type == ttDataToWindow)
            proxyServerList->InitCombo(hwnd, proxyUID, addDefault);
        else
            proxyServerList->GetProxyUIDFromCombo(hwnd, proxyUID, addDefault);
    }
}

void CConfigPageDefaults::Transfer(CTransferInfo& ti)
{
    if (TmpFTPProxyServerList != NULL)
    {
        ProxyComboBox(HWindow, ti, IDC_PROXYSERVER, Config.DefaultProxySrvUID, FALSE,
                      TmpFTPProxyServerList);
        if (ti.Type == ttDataFromWindow) // copy the data back into the configuration
        {
            TmpFTPProxyServerList->CopyMembersToList(Config.FTPProxyServerList);
            Config.FTPServerList.CheckProxyServersUID(Config.FTPProxyServerList);
            // No need to validate Config.DefaultProxySrvUID, it was refreshed a few lines above
        }
    }

    ti.CheckBox(IDC_PASSIVE, Config.PassiveMode);
    ti.CheckBox(IDC_KEEPALIVE, Config.KeepAlive);

    ti.CheckBox(IDC_MAXCONCURRENTCON, Config.UseMaxConcurrentConnections);
    if (ti.Type == ttDataFromWindow && Config.UseMaxConcurrentConnections)
    {
        ti.EditLine(IDE_MAXCONCURRENTCON, Config.MaxConcurrentConnections);
    }

    ti.CheckBox(IDC_SRVSPEEDLIMIT, Config.UseServerSpeedLimit);
    if (ti.Type == ttDataFromWindow && Config.UseServerSpeedLimit)
    {
        char buff[] = "%g";
        ti.EditLine(IDE_SRVSPEEDLIMIT, Config.ServerSpeedLimit, buff);
        if (ti.Type == ttDataFromWindow &&
            Config.ServerSpeedLimit < 0.001)
            Config.ServerSpeedLimit = 0.001; // at least 1 byte per second
    }

    ti.CheckBox(IDC_USELISTINGSCACHE, Config.UseListingsCache);

    if (ti.Type == ttDataFromWindow)
    {
        int compressData;
        ti.CheckBox(IDC_COMPRESSDATA, compressData);
        switch (compressData)
        {
        case 0:
            Config.CompressData = 0;
            break; // NO
        case 1:
            Config.CompressData = 6;
            break; // Yes, default compression is 6
        }
    }
    else
    {
        int compressData;
        switch (Config.CompressData)
        {
        case 0:
            compressData = 0;
            break; // NO
        default:
        case 6:
            compressData = 1;
            break; // Yes, default compression is 6
        }
        ti.CheckBox(IDC_COMPRESSDATA, compressData);
    }

    ti.EditLine(IDE_KEEPALIVEEVERY, Config.KeepAliveSendEvery);
    ti.EditLine(IDE_KEEPALIVESTOPAFTER, Config.KeepAliveStopAfter);

    HWND combo;
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
            // verify that KeepAliveCommand is within bounds (only direct registry editing could break it)
            if (Config.KeepAliveCommand >= i)
                Config.KeepAliveCommand = i - 1;
            if (Config.KeepAliveCommand < 0)
                Config.KeepAliveCommand = 0;
            SendMessage(combo, CB_SETCURSEL, Config.KeepAliveCommand, 0);
        }
        else
        {
            int i = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR)
                Config.KeepAliveCommand = i;
        }
    }

    ti.RadioButton(IDC_BINARYMODE, trmBinary, Config.TransferMode);
    ti.RadioButton(IDC_ASCIIMODE, trmASCII, Config.TransferMode);
    ti.RadioButton(IDC_AUTODETECT, trmAutodetect, Config.TransferMode);

    char masks[MAX_GROUPMASK];
    if (ti.Type == ttDataToWindow)
    {
        Config.ASCIIFileMasks->GetMasksString(masks);
        ti.EditLine(IDE_ASCIIMASKS, masks, MAX_GROUPMASK);
    }
    else
    {
        if (Config.TransferMode == trmAutodetect)
        {
            ti.EditLine(IDE_ASCIIMASKS, masks, MAX_GROUPMASK);
            Config.ASCIIFileMasks->SetMasksString(masks, FALSE);
        }
    }
}

void CConfigPageDefaults::EnableControls()
{
    BOOL enable = IsDlgButtonChecked(HWindow, IDC_AUTODETECT) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDE_ASCIIMASKS), enable);

    CheckboxEditLineInteger(HWindow, IDC_MAXCONCURRENTCON, IDE_MAXCONCURRENTCON, &LastMaxCon, MaxConBuf,
                            Config.MaxConcurrentConnections, 0, 0);
    CheckboxEditLineDouble(HWindow, IDC_SRVSPEEDLIMIT, IDE_SRVSPEEDLIMIT, &LastSrvSpeed, SrvSpeedBuf,
                           Config.ServerSpeedLimit, 0, 0);
}

INT_PTR
CConfigPageDefaults::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigPageDefaults::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        TmpFTPProxyServerList = new CFTPProxyServerList;
        if (TmpFTPProxyServerList != NULL)
        {
            if (!Config.FTPProxyServerList.CopyMembersToList(*TmpFTPProxyServerList))
            {
                delete TmpFTPProxyServerList;
                TmpFTPProxyServerList = NULL;
            }
        }
        else
            TRACE_E(LOW_MEMORY);

        SalamanderGUI->AttachButton(HWindow, IDB_ADDPROXYSRV, BTF_DROPDOWN);

        CGUIHyperLinkAbstract* hint = SalamanderGUI->AttachHyperLink(HWindow, IDT_ASCIIMASKHINTS, STF_DOTUNDERLINE);
        if (hint != NULL)
            hint->SetActionShowHint(LoadStr(IDS_MASKS_HINT));
        INT_PTR ret = CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
        EnableControls();
        return ret;
    }

    case WM_USER_BUTTONDROPDOWN:
    {
        if (LOWORD(wParam) == IDB_ADDPROXYSRV) // drop-down menu on the Add button
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
                        // enable menu items
                        HWND combo = GetDlgItem(HWindow, IDC_PROXYSERVER);
                        int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
                        int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
                        int fixedItems = 1; // this will be 2 for "not used" + "default"
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
        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_AUTODETECT ||
             LOWORD(wParam) == IDC_ASCIIMODE ||
             LOWORD(wParam) == IDC_BINARYMODE ||
             LOWORD(wParam) == IDC_MAXCONCURRENTCON ||
             LOWORD(wParam) == IDC_SRVSPEEDLIMIT))
        {
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
                TmpFTPProxyServerList->EditProxyServer(HWindow, GetDlgItem(HWindow, IDC_PROXYSERVER), FALSE);
                return TRUE;
            }

            case CM_DELETEPROXYSRV:
            {
                TmpFTPProxyServerList->DeleteProxyServer(HWindow, GetDlgItem(HWindow, IDC_PROXYSERVER), FALSE);
                return TRUE;
            }

            case CM_MOVEUPPROXYSRV:
            {
                TmpFTPProxyServerList->MoveUpProxyServer(GetDlgItem(HWindow, IDC_PROXYSERVER), FALSE);
                return TRUE;
            }

            case CM_MOVEDOWNPROXYSRV:
            {
                TmpFTPProxyServerList->MoveDownProxyServer(GetDlgItem(HWindow, IDC_PROXYSERVER), FALSE);
                return TRUE;
            }
            }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfigDlg
//

// helper object for centering the configuration dialog relative to the parent
class CCenteredPropertyWindow : public CWindow
{
protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_WINDOWPOSCHANGING:
        {
            WINDOWPOS* pos = (WINDOWPOS*)lParam;
            if (pos->flags & SWP_SHOWWINDOW)
            {
                HWND hParent = GetParent(HWindow);
                if (hParent != NULL)
                    SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);
            }
            break;
        }

        case WM_APP + 1000: // we should detach from the dialog (already centered)
        {
            DetachWindow();
            delete this; // a bit hacky, but nothing touches 'this' anymore, so it's fine
            return 0;
        }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

#ifndef LPDLGTEMPLATEEX
#include <pshpack1.h>
typedef struct DLGTEMPLATEEX
{
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x;
    short y;
    short cx;
    short cy;
} DLGTEMPLATEEX, *LPDLGTEMPLATEEX;
#include <poppack.h>
#endif // LPDLGTEMPLATEEX

// helper callback for centering the configuration dialog relative to the parent and removing the '?' button from the caption
int CALLBACK CenterCallback(HWND HWindow, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED) // attach to the dialog
    {
        CCenteredPropertyWindow* wnd = new CCenteredPropertyWindow;
        if (wnd != NULL)
        {
            wnd->AttachToWindow(HWindow);
            if (wnd->HWindow == NULL)
                delete wnd; // window is not attached, destroy it right here
            else
            {
                PostMessage(wnd->HWindow, WM_APP + 1000, 0, 0); // to detach CCenteredPropertyWindow from the dialog
            }
        }
    }
    if (uMsg == PSCB_PRECREATE) // remove the '?' button from the property sheet header
    {
        // Remove the DS_CONTEXTHELP style from the dialog box template
        if (((LPDLGTEMPLATEEX)lParam)->signature == 0xFFFF)
            ((LPDLGTEMPLATEEX)lParam)->style &= ~DS_CONTEXTHELP;
        else
            ((LPDLGTEMPLATE)lParam)->style &= ~DS_CONTEXTHELP;
    }
    return 0;
}

CConfigDlg::CConfigDlg(HWND parent)
    : CPropertyDialog(parent, HLanguage, LoadStr(IDS_CONFIGTITLE),
                      Config.LastCfgPage, PSH_HASHELP | PSH_USECALLBACK | PSH_NOAPPLYNOW,
                      NULL, &Config.LastCfgPage, CenterCallback)
{
    Add(&PageGeneral);
    Add(&PageDefaults);
    Add(&PageConfirmations);
    Add(&PageOperations);
    Add(&PageOperations2);
    Add(&PageAdvanced);
    Add(&PageLogs);
    Add(&PageServers);
}

//
// ****************************************************************************
// CBookmarksListbox
//

CBookmarksListbox::CBookmarksListbox(CConnectDlg* dlg, int ctrlID)
    : CWindow(dlg->HWindow, ctrlID)
{
    ParentDlg = dlg;
}

void CBookmarksListbox::MoveUpDown(BOOL moveUp)
{
    int i = (int)SendMessage(HWindow, LB_GETCURSEL, 0, 0);
    if (i != LB_ERR)
    {
        if (moveUp)
        {
            if (i > 0)
                ParentDlg->MoveItem(HWindow, i, i - 1);
        }
        else
        {
            if (i + 1 < SendMessage(HWindow, LB_GETCOUNT, 0, 0))
                ParentDlg->MoveItem(HWindow, i, i + 1);
        }
    }
}

void CBookmarksListbox::OpenContextMenu(int curSel, int menuX, int menuY)
{
    HMENU main = LoadMenu(HLanguage,
                          MAKEINTRESOURCE(ParentDlg->AddBookmarkMode == 0 ? IDM_SRVCONTEXTMENU : IDM_SRVCONTEXTMENU2));
    if (main != NULL)
    {
        HMENU subMenu = GetSubMenu(main, 0);
        if (subMenu != NULL)
        {
            MyEnableMenuItem(subMenu, IDB_RENAMEBOOKMARK, curSel > 0);
            MyEnableMenuItem(subMenu, IDB_REMOVEBOOKMARK, curSel > 0);
            MyEnableMenuItem(subMenu, CM_MOVEUPBOOKMARK, curSel > 1);
            MyEnableMenuItem(subMenu, CM_MOVEDOWNBOOKMARK, curSel > 0 && curSel < ParentDlg->TmpFTPServerList.Count);
            DWORD cmd = TrackPopupMenuEx(subMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                         menuX, menuY, HWindow, NULL);
            if (cmd == CM_MOVEUPBOOKMARK || cmd == CM_MOVEDOWNBOOKMARK)
            {
                MoveUpDown(cmd == CM_MOVEUPBOOKMARK);
            }
            else
            {
                if (cmd != 0)
                    PostMessage(ParentDlg->HWindow, WM_COMMAND, cmd, 0);
            }
        }
        DestroyMenu(main);
    }
}

LRESULT
CBookmarksListbox::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        switch (wParam)
        {
        case VK_INSERT:
            PostMessage(ParentDlg->HWindow, WM_COMMAND, IDB_NEWBOOKMARK, 0);
            break;
        case VK_F2:
            PostMessage(ParentDlg->HWindow, WM_COMMAND, IDB_RENAMEBOOKMARK, 0);
            break;
        case VK_DELETE:
            PostMessage(ParentDlg->HWindow, WM_COMMAND, IDB_REMOVEBOOKMARK, 0);
            break;

        case VK_UP:
        case VK_DOWN:
        {
            if (GetKeyState(VK_MENU) & 0x8000)
                MoveUpDown(LOWORD(wParam) == VK_UP); // Alt pressed
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
        break; // let the keys propagate, the listbox should not handle them, no problem
    }

    case WM_LBUTTONDBLCLK:
    {
        if (ParentDlg->AddBookmarkMode == 0)
            PostMessage(ParentDlg->HWindow, WM_COMMAND, IDOK, 0);
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
                                    MAKELPARAM(LOWORD((int)lParam), HIWORD((int)lParam))); // FIXME_X64 suspicious cast
        if (HIWORD(item) == 0 && item >= 0 && item <= ParentDlg->TmpFTPServerList.Count &&
            curSel != item)
        {
            SendMessage(HWindow, LB_SETCURSEL, item, 0);
            SendMessage(ParentDlg->HWindow, WM_COMMAND, MAKELPARAM(IDL_BOOKMARKS, LBN_SELCHANGE), 0);
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
// CConnectDlg
//

CConnectDlg::CConnectDlg(HWND parent, int addBookmarkMode)
    : CCenteredDialog(HLanguage, IDD_CONNECT, addBookmarkMode != 0 ? IDH_ORGBOOKMARKSDLG : IDD_CONNECT, parent)
{
    OK = Config.FTPServerList.CopyMembersToList(TmpFTPServerList);
    if (OK)
        OK = Config.FTPProxyServerList.CopyMembersToList(TmpFTPProxyServerList);
    CanChangeFocus = TRUE;
    DragIndex = -1;
    ExtraDragDropItemAdded = FALSE;
    AddBookmarkMode = addBookmarkMode;
    LastRawHostAddress[0] = 0;
}

void CConnectDlg::Validate(CTransferInfo& ti)
{
}

void CConnectDlg::Transfer(CTransferInfo& ti)
{
    HWND list;
    if (ti.GetControl(list, IDL_BOOKMARKS))
    {
        if (ti.Type == ttDataToWindow)
        {
            SendMessage(list, WM_SETREDRAW, FALSE, 0);
            SendMessage(list, LB_RESETCONTENT, 0, 0);
            SendMessage(list, LB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_QUICKCONNECT));
            TmpFTPServerList.AddNamesToListbox(list);
            if (Config.LastBookmark > TmpFTPServerList.Count)
            {
                Config.LastBookmark = TmpFTPServerList.Count;
            }
            if (Config.LastBookmark < 0)
                Config.LastBookmark = 0;
            SendMessage(list, LB_SETCURSEL, AddBookmarkMode != 2 ? Config.LastBookmark : TmpFTPServerList.Count, 0);
            SendMessage(list, WM_SETREDRAW, TRUE, 0);
            if (AddBookmarkMode == 0 && Config.LastBookmark == 0 /* quick connect */)
                PostMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_HOSTADDRESS), TRUE);
        }
        else
        {
            int i = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
            if (i != LB_ERR)
                Config.LastBookmark = i;

            // copy the data back into the configuration (button: Connect or Close)
            TmpFTPServerList.CopyMembersToList(Config.FTPServerList);
            TmpFTPProxyServerList.CopyMembersToList(Config.FTPProxyServerList);
            // verify that the "default" proxy server still exists
            if (Config.DefaultProxySrvUID != -1 &&
                !Config.FTPProxyServerList.IsValidUID(Config.DefaultProxySrvUID))
            {
                Config.DefaultProxySrvUID = -1; // "not used"
            }
        }
    }

    // restore the non-expanded variant of the Address string (history stores what the user typed, not the split result)
    if (ti.IsGood() && ti.Type == ttDataFromWindow && Config.LastBookmark == 0)
        SetWindowText(GetDlgItem(HWindow, IDE_HOSTADDRESS), LastRawHostAddress);
    char buf[HOST_MAX_SIZE < FTP_MAX_PATH ? FTP_MAX_PATH : HOST_MAX_SIZE];
    buf[0] = 0;
    HistoryComboBox(HWindow, ti, IDE_HOSTADDRESS, buf, HOST_MAX_SIZE,
                    HOSTADDRESS_HISTORY_SIZE, Config.HostAddressHistory,
                    Config.LastBookmark != 0 /* store in history only during Quick Connect*/);
    buf[0] = 0;
    HistoryComboBox(HWindow, ti, IDE_INITIALPATH, buf, FTP_MAX_PATH,
                    INITIALPATH_HISTORY_SIZE, Config.InitPathHistory,
                    Config.LastBookmark != 0 /* store in history only during Quick Connect*/);
}

void AddToAdvancedStr(char* buf, int bufSize, const char* str)
{
    int len = (int)strlen(buf);
    if (len != 0 && len + 2 < bufSize)
    {
        strcpy(buf + len, ", ");
        len += 2;
    }
    if (len + (int)strlen(str) < bufSize)
        strcpy(buf + len, str);
}

void CConnectDlg::SelChanged()
{
    CFTPServer* s;
    int i;
    if (!GetCurSelServer(&s, &i))
        return; // unexpected situation

    BOOL lockedPassword = TRUE;
    char password[PASSWORD_MAX_SIZE];
    password[0] = 0;
    if (s->AnonymousConnection)
    {
        Config.GetAnonymousPasswd(password, PASSWORD_MAX_SIZE);
        lockedPassword = FALSE;
    }
    else
    {
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        if (!s->SavePassword || !passwordManager->IsUsingMasterPassword() || passwordManager->IsMasterPasswordSet())
        {
            if (s->EncryptedPassword != NULL)
            { // scrambled/encrypted -> plain
                char* plainPassword;
                if (passwordManager->DecryptPassword(s->EncryptedPassword, s->EncryptedPasswordSize, &plainPassword))
                {
                    lstrcpyn(password, plainPassword, PASSWORD_MAX_SIZE);
                    memset(plainPassword, 0, lstrlen(plainPassword));
                    SalamanderGeneral->Free(plainPassword);
                    lockedPassword = FALSE;
                }
            }
            else
                lockedPassword = FALSE;
        }
    }

    ShowHidePasswordControls(lockedPassword, FALSE);

    CTransferInfo ti(HWindow, ttDataToWindow);
    ti.EditLine(IDE_HOSTADDRESS, HandleNULLStr(s->Address), HOST_MAX_SIZE);
    ti.EditLine(IDE_INITIALPATH, HandleNULLStr(s->InitialPath), FTP_MAX_PATH);
    ti.CheckBox(IDC_ANONYMOUSLOGIN, s->AnonymousConnection);
    ti.EditLine(IDE_USERNAME, HandleNULLStr(s->AnonymousConnection ? (char*)FTP_ANONYMOUS : s->UserName), USER_MAX_SIZE);
    ti.EditLine(IDE_PASSWORD, password, PASSWORD_MAX_SIZE);

    int savePasswd = (s->AnonymousConnection || i == 0) ? FALSE : s->SavePassword;
    ti.CheckBox(IDC_SAVEPASSWORD, savePasswd);

    char buf[300];
    buf[0] = 0;
    char num[100];
    if (s->ProxyServerUID != -2)
    {
        num[36] = 0;
        char proxyNameBuf[PROXYSRVNAME_MAX_SIZE];
        if (TmpFTPProxyServerList.GetProxyName(proxyNameBuf, PROXYSRVNAME_MAX_SIZE, s->ProxyServerUID))
        {
            _snprintf_s(num, 38, _TRUNCATE, LoadStr(IDS_ADVSTRPROXYSRV), proxyNameBuf);
            if (num[36] != 0)
                strcpy(num + 36, "...");
            AddToAdvancedStr(buf, 300, num);
        }
        else
            TRACE_E("Unexpected situation in CConnectDlg::SelChanged(): invalid ProxyServerUID!");
    }
    if (s->Port != IPPORT_FTP)
    {
        _snprintf_s(num, _TRUNCATE, LoadStr(IDS_ADVSTRPORT), s->Port);
        AddToAdvancedStr(buf, 300, num);
    }
    if (s->TransferMode != 0)
    {
        char* str;
        switch (s->TransferMode)
        {
        case 1:
            str = LoadStr(IDS_ADVSTRTRMODBINARY);
            break;
        case 2:
            str = LoadStr(IDS_ADVSTRTRMODASCII);
            break;
        default:
            str = LoadStr(IDS_ADVSTRTRMODAUTO);
            break;
        }
        AddToAdvancedStr(buf, 300, str);
    }
    if (s->UsePassiveMode != 2)
    {
        AddToAdvancedStr(buf, 300, LoadStr(s->UsePassiveMode == 0 ? IDS_ADVSTRSERVERPASVNO : IDS_ADVSTRSERVERPASVYES));
    }
    if (s->KeepConnectionAlive != 2)
    {
        AddToAdvancedStr(buf, 300, LoadStr(s->KeepConnectionAlive == 0 ? IDS_ADVSTRKEEPALIVENO : IDS_ADVSTRKEEPALIVEYES));
    }
    if (s->UseMaxConcurrentConnections != 2)
    {
        if (s->UseMaxConcurrentConnections == 1)
        {
            _snprintf_s(num, _TRUNCATE, LoadStr(IDS_ADVSTRMAXCONCURCON), s->MaxConcurrentConnections);
            AddToAdvancedStr(buf, 300, num);
        }
        else
            AddToAdvancedStr(buf, 300, LoadStr(IDS_ADVSTRUNLIMITEDCON));
    }
    if (s->UseServerSpeedLimit != 2)
    {
        if (s->UseServerSpeedLimit == 1)
        {
            _snprintf_s(num, _TRUNCATE, LoadStr(IDS_ADVSTRSPEEDLIM), s->ServerSpeedLimit);
            AddToAdvancedStr(buf, 300, num);
        }
        else
            AddToAdvancedStr(buf, 300, LoadStr(IDS_ADVSTRUNLIMITEDSPEED));
    }
    if (s->ServerType != NULL)
    {
        num[36] = 0;
        char typeBuf[SERVERTYPE_MAX_SIZE + 101];
        _snprintf_s(num, 38, _TRUNCATE, LoadStr(IDS_ADVSTRSERVERTYPE),
                    GetTypeNameForUser(s->ServerType, typeBuf, SERVERTYPE_MAX_SIZE + 101));
        if (num[36] != 0)
            strcpy(num + 36, "...");
        AddToAdvancedStr(buf, 300, num);
    }
    if (s->TargetPanelPath != NULL && *s->TargetPanelPath != 0)
    {
        num[36] = 0;
        _snprintf_s(num, 38, _TRUNCATE, LoadStr(IDS_ADVSTRTARGETPATH), s->TargetPanelPath);
        if (num[36] != 0)
            strcpy(num + 36, "...");
        AddToAdvancedStr(buf, 300, num);
    }
    if (s->UseListingsCache != 2)
    {
        AddToAdvancedStr(buf, 300, LoadStr(s->UseListingsCache == 0 ? IDS_ADVSTRUSECACHENO : IDS_ADVSTRUSECACHEYES));
    }
    if (s->EncryptControlConnection == 1)
    {
        AddToAdvancedStr(buf, 300, LoadStr(s->EncryptDataConnection == 1 ? IDS_ADVSTRSSLCONTRDATA : IDS_ADVSTRSSLCONTRONLY));
    }
    if (s->CompressData != -1)
    {
        AddToAdvancedStr(buf, 300, LoadStr(s->CompressData == 0 ? IDS_ADVSTRNOMODEZ : IDS_ADVSTRMODEZ));
    }

    if (s->ListCommand != NULL)
    {
        num[36] = 0;
        _snprintf_s(num, 38, _TRUNCATE, LoadStr(IDS_ADVSTRLISTCOMMAND), s->ListCommand);
        if (num[36] != 0)
            strcpy(num + 36, "...");
        AddToAdvancedStr(buf, 300, num);
    }
    if (s->InitFTPCommands != NULL && *s->InitFTPCommands != 0)
    {
        num[36] = 0;
        _snprintf_s(num, 38, _TRUNCATE, LoadStr(IDS_ADVSTRINITFTPCMDS), s->InitFTPCommands);
        if (num[36] != 0)
            strcpy(num + 36, "...");
        AddToAdvancedStr(buf, 300, num);
    }
    if (buf[0] == 0)
        strcpy(buf, LoadStr(IDS_ADVSTRNONE));
    ti.EditLine(IDE_ADVANCEDINFO, buf, 300);
}

void CConnectDlg::EnableControls()
{
    BOOL quickCon = SendMessage(GetDlgItem(HWindow, IDL_BOOKMARKS), LB_GETCURSEL, 0, 0) == 0;
    BOOL enable = IsDlgButtonChecked(HWindow, IDC_ANONYMOUSLOGIN) == BST_UNCHECKED &&
                  (!quickCon || AddBookmarkMode == 0);
    EnableWindow(GetDlgItem(HWindow, IDT_USERNAME), enable);
    EnableWindow(GetDlgItem(HWindow, IDE_USERNAME), enable);
    EnableWindow(GetDlgItem(HWindow, IDT_PASSWORD), enable);
    EnableWindow(GetDlgItem(HWindow, IDE_PASSWORD), enable);
    if (enable)
        enable = !quickCon;
    EnableWindow(GetDlgItem(HWindow, IDC_SAVEPASSWORD), enable);
    EnableWindow(GetDlgItem(HWindow, IDE_PASSWORD_LOCKED), FALSE);
    EnableWindow(GetDlgItem(HWindow, IDB_PASSWORD_CHANGE), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_SAVEPASSWORD_HINT), enable);

    enable = !quickCon;
    if (!enable && GetFocus() == GetDlgItem(HWindow, IDB_REMOVEBOOKMARK))
    {
        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDL_BOOKMARKS), TRUE);
    }
    EnableWindow(GetDlgItem(HWindow, IDB_RENAMEBOOKMARK), enable);
    EnableWindow(GetDlgItem(HWindow, IDB_REMOVEBOOKMARK), enable);

    if (AddBookmarkMode != 0) // "organize bookmarks"
    {
        enable = !quickCon;
        EnableWindow(GetDlgItem(HWindow, IDT_HOSTADDRESS), enable);
        EnableWindow(GetDlgItem(HWindow, IDE_HOSTADDRESS), enable);
        EnableWindow(GetDlgItem(HWindow, IDT_INITIALPATH), enable);
        EnableWindow(GetDlgItem(HWindow, IDE_INITIALPATH), enable);
        EnableWindow(GetDlgItem(HWindow, IDC_ANONYMOUSLOGIN), enable);
        EnableWindow(GetDlgItem(HWindow, IDB_ADVACED), enable);
        EnableWindow(GetDlgItem(HWindow, IDE_ADVANCEDINFO), enable);
    }
}

void CConnectDlg::AlignPasswordControls()
{
    // the non-password edit line (with the info text) will be moved to where the password edit line starts and stretched to the button
    RECT editRect;
    GetWindowRect(GetDlgItem(HWindow, IDE_PASSWORD), &editRect);
    RECT editLockedRect;
    GetWindowRect(GetDlgItem(HWindow, IDE_PASSWORD_LOCKED), &editLockedRect);
    int width = editLockedRect.right - editRect.left;
    MapWindowPoints(NULL, HWindow, (POINT*)&editRect, 2);
    SetWindowPos(GetDlgItem(HWindow, IDE_PASSWORD_LOCKED), NULL, editRect.left, editRect.top, width, editLockedRect.bottom - editLockedRect.top, SWP_NOZORDER);

    // the password edit line will be as long as the one above it
    GetWindowRect(GetDlgItem(HWindow, IDE_USERNAME), &editRect);
    SetWindowPos(GetDlgItem(HWindow, IDE_PASSWORD), NULL, 0, 0, editRect.right - editRect.left, editRect.bottom - editRect.top, SWP_NOMOVE | SWP_NOZORDER);
}

void CConnectDlg::ShowHidePasswordControls(BOOL lockedPassword, BOOL focusEdit)
{
    CFTPServer* s;
    int i;
    if (!GetCurSelServer(&s, &i))
        return; // unexpected situation

    BOOL quickCon = (i == 0);
    BOOL showUnlockButton = lockedPassword;
    ShowWindow(GetDlgItem(HWindow, IDE_PASSWORD), !showUnlockButton);
    ShowWindow(GetDlgItem(HWindow, IDE_PASSWORD_LOCKED), showUnlockButton);
    ShowWindow(GetDlgItem(HWindow, IDB_PASSWORD_CHANGE), showUnlockButton);

    if (!showUnlockButton && focusEdit)
    {
        SetFocus(GetDlgItem(HWindow, IDE_PASSWORD));
        SendDlgItemMessage(HWindow, IDE_PASSWORD, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
        SendMessage(HWindow, DM_SETDEFID, IDE_PASSWORD, 0);
    }
}

void CConnectDlg::RefreshList(BOOL focusLast)
{
    HWND list = GetDlgItem(HWindow, IDL_BOOKMARKS);
    int focus = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
    int topIndex = (int)SendMessage(list, LB_GETTOPINDEX, 0, 0);
    SendMessage(list, WM_SETREDRAW, FALSE, 0);
    SendMessage(list, LB_RESETCONTENT, 0, 0);
    SendMessage(list, LB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_QUICKCONNECT));
    TmpFTPServerList.AddNamesToListbox(list);
    int count = (int)SendMessage(list, LB_GETCOUNT, 0, 0);
    if (focus >= count)
        focus = count - 1;
    if (focusLast)
        focus = count - 1;
    SendMessage(list, LB_SETTOPINDEX, topIndex, 0);
    SendMessage(list, LB_SETCURSEL, focus, 0);
    SendMessage(list, WM_SETREDRAW, TRUE, 0);
}

void CConnectDlg::MoveItem(HWND list, int fromIndex, int toIndex, int topIndex)
{
    if (fromIndex > 0 && toIndex > 0 && fromIndex != toIndex &&                   // nobody is allowed to move the quick-connect entry
        fromIndex <= TmpFTPServerList.Count && toIndex <= TmpFTPServerList.Count) // movement stays within the array
    {
        fromIndex--;
        toIndex--;
        // swap the data in the array
        CFTPServer* s = TmpFTPServerList[fromIndex];
        TmpFTPServerList.Detach(fromIndex);
        if (TmpFTPServerList.IsGood())
        {
            TmpFTPServerList.Insert(toIndex, s);
            if (TmpFTPServerList.IsGood())
            {
                // swap the data in the listbox
                SendMessage(list, WM_SETREDRAW, FALSE, 0);
                if (topIndex == -1)
                    topIndex = (int)SendMessage(list, LB_GETTOPINDEX, 0, 0);
                SendMessage(list, LB_DELETESTRING, fromIndex + 1, 0);
                SendMessage(list, LB_INSERTSTRING, toIndex + 1, (LPARAM)HandleNULLStr(s->ItemName));
                SendMessage(list, LB_SETTOPINDEX, topIndex, 0);
                SendMessage(list, LB_SETCURSEL, toIndex + 1, 0);
                SendMessage(list, WM_SETREDRAW, TRUE, 0);
            }
            else
            {
                SendMessage(list, LB_DELETESTRING, fromIndex + 1, 0);
                TmpFTPServerList.ResetState();
                delete s; // left outside the array, delete it
            }
        }
        else
            TmpFTPServerList.ResetState();
    }
}

BOOL CConnectDlg::GetCurSelServer(CFTPServer** server, int* index)
{
    CFTPServer* s;
    int i = (int)SendMessage(GetDlgItem(HWindow, IDL_BOOKMARKS), LB_GETCURSEL, 0, 0);
    if (i == 0)
        s = &Config.QuickConnectServer;
    else
    {
        if (i >= 0 && i - 1 < TmpFTPServerList.Count)
            s = TmpFTPServerList.At(i - 1);
        else
        {
            TRACE_E("Unexpected situation in CConnectDlg::GetCurSelServer()!");
            return FALSE; // unexpected situation
        }
    }
    *server = s;
    if (index != NULL)
        *index = i;
    return TRUE;
}

UINT DragListboxMsg = 0; // message ID corresponding to the DRAGLISTMSGSTRING (drag&drop listbox)

INT_PTR
CConnectDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CConnectDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGeneral->InstallWordBreakProc(GetDlgItem(HWindow, IDE_HOSTADDRESS));
        SalamanderGeneral->InstallWordBreakProc(GetDlgItem(HWindow, IDE_INITIALPATH));
        if (AddBookmarkMode != 0)
        {
            SetWindowText(HWindow, LoadStr(IDS_ORGANIZEBOOKMARKS));
            HWND ok = GetDlgItem(HWindow, IDOK);
            HWND close = GetDlgItem(HWindow, IDB_CLOSE);
            SendMessage(HWindow, DM_SETDEFID, IDB_CLOSE, 0);
            SendMessage(ok, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
            SendMessage(close, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
            ShowWindow(ok, SW_HIDE);
        }

        // Unlock button: besides the Unlock command it will also have Clear to delete the password
        SalamanderGUI->AttachButton(HWindow, IDB_PASSWORD_CHANGE, BTF_DROPDOWN);

        // we want to receive WM_APP_SHOWPASSWORD from the password edit line on Ctrl+right click
        CPasswordEditLine* passwordEL = new CPasswordEditLine(HWindow, IDE_PASSWORD);

        // reposition the password edit lines
        AlignPasswordControls();

        CGUIHyperLinkAbstract* hint = SalamanderGUI->AttachHyperLink(HWindow, IDC_SAVEPASSWORD_HINT, STF_HYPERLINK_COLOR | STF_UNDERLINE);
        hint->SetActionPostCommand(IDC_SAVEPASSWORD_HINT);

        // if the user has not set a master password in Salamander, storing passwords is not safe
        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
        // if the user uses the password manager, change the "it is not secure" message
        if (passwordManager->IsUsingMasterPassword())
            SetDlgItemText(HWindow, IDC_SAVEPASSWORD_HINT, LoadStr(IDS_SAVEPASSWORD_PROTECTED));

        // attach to the listbox (because Alt+arrow keys do not reach WM_VKEYTOITEM)
        CBookmarksListbox* list = new CBookmarksListbox(this, IDL_BOOKMARKS);
        if (list != NULL)
        {
            if (list->HWindow == NULL)
                delete list; // if the control is missing, release the object (otherwise it releases itself)
            else
            {
                MakeDragList(list->HWindow);
                if (DragListboxMsg == 0)
                    DragListboxMsg = RegisterWindowMessage(DRAGLISTMSGSTRING);
            }
        }

        INT_PTR ret = CCenteredDialog::DialogProc(uMsg, wParam, lParam);
        SelChanged();
        EnableControls();
        return ret;
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
            // ask for the master password even if we already know it
            if (!passwordManager->IsUsingMasterPassword() || passwordManager->AskForMasterPassword(HWindow))
            {
                // pull the password directly from the edit line
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
        if (LOWORD(wParam) == IDB_PASSWORD_CHANGE) // drop-down menu on the Unlock button
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
                        GetWindowRect(GetDlgItem(HWindow, (DWORD)wParam), &r);
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
        case IDOK: // to deliver EN_KILLFOCUS even when Enter (default button = Connect) is pressed
        case IDB_CLOSE:
        {
            HWND button = GetDlgItem(HWindow, LOWORD(wParam));
            if (CanChangeFocus && button != NULL && GetFocus() != button)
            {
                SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)button, TRUE);
                PostMessage(HWindow, uMsg, wParam, lParam); // postpone this command
                CanChangeFocus = FALSE;                     // prevents an infinite loop
                return TRUE;                                // do nothing; wait for the kill focus in the edit boxes
            }
            CanChangeFocus = TRUE;

            if (LOWORD(wParam) == IDB_CLOSE)
            {
                if (!ValidateData() ||
                    !TransferData(ttDataFromWindow))
                    return TRUE;
                if (Modal)
                    EndDialog(HWindow, wParam);
                else
                    DestroyWindow(HWindow);
                return TRUE;
            }
            else // IDOK
            {
                CFTPServer* s;
                int i;
                if (!GetCurSelServer(&s, &i))
                    break; // unexpected situation

                if (s->Address == NULL || *s->Address == 0)
                {
                    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_HOSTMAYNOTBEEMPTY),
                                                     LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);

                    HWND ctrl = GetDlgItem(HWindow, IDE_HOSTADDRESS);
                    HWND wnd = GetFocus();
                    while (wnd != NULL && wnd != ctrl)
                        wnd = GetParent(wnd);
                    if (wnd == NULL) // set focus only if the control is not an ancestor of GetFocus
                    {                // for example, the edit line in the combo box
                        SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)ctrl, TRUE);
                    }
                    return TRUE;
                }

                // verify that active transfer mode is not requested with an HTTP 1.1 proxy
                if (s->UsePassiveMode == 0 || s->UsePassiveMode == 2 && Config.PassiveMode == 0)
                {
                    int proxyServerUID = s->ProxyServerUID;
                    if (proxyServerUID == -2)
                        proxyServerUID = Config.DefaultProxySrvUID;
                    if (TmpFTPProxyServerList.GetProxyType(proxyServerUID) == fpstHTTP1_1)
                    {
                        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_HTTPNEEDPASSIVETRMODE2),
                                                         LoadStr(IDS_FTPERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        return TRUE;
                    }
                }

                // if the connection needs passwords, we must be able to decrypt them
                // test the bookmark
                if (!s->EnsurePasswordCanBeDecrypted(HWindow))
                    return TRUE; // failed to enter the master password or it probably failed to decrypt the password

                // WARNING: s->EnsurePasswordCanBeDecrypted() might have cleared the password in 's' and its stored copy, which means
                //        returning to the dialog (return TRUE) requires performing a "refresh"

                // test the proxy server
                int proxyServerUID = s->ProxyServerUID;
                if (proxyServerUID == -2)
                    proxyServerUID = Config.DefaultProxySrvUID;
                if (!TmpFTPProxyServerList.EnsurePasswordCanBeDecrypted(HWindow, proxyServerUID))
                {
                    // "refresh", reason a few lines above (s->EnsurePasswordCanBeDecrypted)
                    SelChanged();
                    EnableControls();

                    return TRUE; // failed to enter the master password or they could not decrypt the password
                }
            }
            break;
        }

        case IDL_BOOKMARKS:
        {
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                SelChanged();
                EnableControls();
            }
            break;
        }

        case IDC_SAVEPASSWORD_HINT:
        {
            // open Salamander help with information about the password manager
            SalamanderGeneral->OpenHtmlHelpForSalamander(HWindow, HHCDisplayContext, HTMLHELP_SALID_PWDMANAGER, FALSE);
            break;
        }

        case CM_CLEARPASSWORD:
        {
            CFTPServer* s;
            int i;
            if (!GetCurSelServer(&s, &i))
                break; // unexpected situation

            int ret = SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CLEARPASSWORD_CONFIRMATION),
                                                       LoadStr(IDS_FTPPLUGINTITLE), MB_YESNO | MSGBOXEX_ESCAPEENABLED | /*MB_DEFBUTTON2 | */ MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT);
            if (ret == IDYES)
            {
                // user requested to delete the password
                UpdateEncryptedPassword(&s->EncryptedPassword, &s->EncryptedPasswordSize, NULL, 0);
                // clear the save password checkbox
                s->SavePassword = FALSE;

                ShowHidePasswordControls(FALSE, TRUE);
                SelChanged();
            }
            break;
        }

        case CM_UNLOCKPASSWORD:
        case IDB_PASSWORD_CHANGE:
        {
            CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
            // either the master password is not used, it is already set, or the user enters it now
            if (!passwordManager->IsUsingMasterPassword() || passwordManager->IsMasterPasswordSet() || passwordManager->AskForMasterPassword(HWindow))
            {
                CFTPServer* s;
                int i;
                if (!GetCurSelServer(&s, &i))
                    break; // unexpected situation

                // if the master password is used, verify that this password can be decrypted with it
                if (!passwordManager->IsUsingMasterPassword() ||
                    s->EncryptedPassword != NULL && !passwordManager->DecryptPassword(s->EncryptedPassword, s->EncryptedPasswordSize, NULL))
                {
                    int ret = SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_CANNOT_DECRYPT_PASSWORD_DELETE),
                                                               LoadStr(IDS_FTPERRORTITLE), MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_DEFBUTTON2 | MB_ICONEXCLAMATION);
                    if (ret == IDNO)
                        break;
                    // user requested to delete the password
                    UpdateEncryptedPassword(&s->EncryptedPassword, &s->EncryptedPasswordSize, NULL, 0);
                    // clear the save password checkbox
                    s->SavePassword = FALSE;
                }
                ShowHidePasswordControls(FALSE, TRUE);
                SelChanged();
            }
            break;
        }

        case IDB_ADVACED:        // advanced options dialog
        case IDB_NEWBOOKMARK:    // new bookmark dialog
        case CM_COPYSRVTO:       // "copy bookmark to" dialog
        case IDB_RENAMEBOOKMARK: // rename bookmark dialog
        case IDB_REMOVEBOOKMARK: // remove bookmark dialog
        case IDE_HOSTADDRESS:    // change in text/checkbox -> change in data
        case IDE_INITIALPATH:
        case IDE_USERNAME:
        case IDE_PASSWORD:
        case IDC_SAVEPASSWORD:
        case IDC_ANONYMOUSLOGIN:
        {
            CFTPServer* s;
            int i;
            if (!GetCurSelServer(&s, &i))
                break; // unexpected situation

            CTransferInfo ti(HWindow, ttDataFromWindow);
            switch (LOWORD(wParam))
            {
            case IDB_ADVACED:
            {
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                BOOL oldMPSet = passwordManager->IsUsingMasterPassword() && passwordManager->IsMasterPasswordSet();
                if (CConnectAdvancedDlg(HWindow, s, &TmpFTPProxyServerList).Execute() == IDOK)
                {
                    TmpFTPServerList.CheckProxyServersUID(TmpFTPProxyServerList);
                    SelChanged();
                    EnableControls();
                }
                else
                {
                    if (!oldMPSet && passwordManager->IsUsingMasterPassword() && passwordManager->IsMasterPasswordSet())
                    { // although Cancel, the user entered the master password, so a "refresh" is needed
                        SelChanged();
                        EnableControls();
                    }
                }
                return TRUE; // do not process further
            }

            case IDB_NEWBOOKMARK:
            case CM_COPYSRVTO:
            case IDB_RENAMEBOOKMARK:
            {
                if (LOWORD(wParam) == IDB_RENAMEBOOKMARK && i <= 0)
                    return TRUE; // quick connect cannot be renamed

                char name[BOOKMARKNAME_MAX_SIZE];
                name[0] = 0;
                if (s->ItemName != NULL)
                {
                    lstrcpyn(name, s->ItemName, BOOKMARKNAME_MAX_SIZE);
                }
                CRenameDlg dlg(HWindow, name,
                               LOWORD(wParam) == IDB_NEWBOOKMARK || LOWORD(wParam) == CM_COPYSRVTO);
                if (LOWORD(wParam) == CM_COPYSRVTO)
                    dlg.CopyDataFromFocusedServer = TRUE;
                if (dlg.Execute() == IDOK)
                {
                    if (LOWORD(wParam) == IDB_NEWBOOKMARK || LOWORD(wParam) == CM_COPYSRVTO) // new/copy to
                    {
                        if (dlg.CopyDataFromFocusedServer)
                        {
                            TmpFTPServerList.AddServer(name,
                                                       s->Address,
                                                       s->InitialPath,
                                                       s->AnonymousConnection,
                                                       s->UserName,
                                                       s->EncryptedPassword,
                                                       s->EncryptedPasswordSize,
                                                       s->SavePassword,
                                                       s->ProxyServerUID,
                                                       s->TargetPanelPath,
                                                       s->ServerType,
                                                       s->TransferMode,
                                                       s->Port,
                                                       s->UsePassiveMode,
                                                       s->KeepConnectionAlive,
                                                       s->UseMaxConcurrentConnections,
                                                       s->MaxConcurrentConnections,
                                                       s->UseServerSpeedLimit,
                                                       s->ServerSpeedLimit,
                                                       s->UseListingsCache,
                                                       s->InitFTPCommands,
                                                       s->ListCommand,
                                                       s->KeepAliveSendEvery,
                                                       s->KeepAliveStopAfter,
                                                       s->KeepAliveCommand,
                                                       s->EncryptControlConnection,
                                                       s->EncryptDataConnection,
                                                       s->CompressData);
                        }
                        else
                            TmpFTPServerList.AddServer(name);
                    }
                    else // rename
                    {
                        UpdateStr(s->ItemName, name);
                    }
                    RefreshList(LOWORD(wParam) == IDB_NEWBOOKMARK || LOWORD(wParam) == CM_COPYSRVTO);
                    SelChanged();
                    EnableControls();
                }
                return TRUE; // do not process further
            }

            case IDB_REMOVEBOOKMARK:
            {
                char buf[200 + BOOKMARKNAME_MAX_SIZE];
                sprintf(buf, LoadStr(IDS_REMOVECONFIRM), HandleNULLStr(s->ItemName));
                if (i > 0) // quick connect cannot be deleted (should always be false)
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = HWindow;
                    params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED |
                                   MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT;
                    params.Caption = LoadStr(IDS_FTPPLUGINTITLE);
                    params.Text = buf;
                    if (SalamanderGeneral->SalMessageBoxEx(&params) == IDYES)
                    {
                        TmpFTPServerList.Delete(i - 1);
                        RefreshList();
                        SelChanged();
                        EnableControls();
                    }
                }

                return TRUE; // do not process further
            }

            case IDE_HOSTADDRESS:
            {
                if (HIWORD(wParam) == CBN_KILLFOCUS)
                {
                    ti.EditLine(IDE_HOSTADDRESS, LastRawHostAddress, HOST_MAX_SIZE);
                    char buf[HOST_MAX_SIZE];
                    lstrcpyn(buf, LastRawHostAddress, HOST_MAX_SIZE);

                    char* str = buf;
                    while (*str != 0 && *str <= ' ')
                        str++; // skip whitespaces
                    // skip FS name
                    int isFTPS = 0; // 0 = nothing, 1 = enable, 2 = disable
                    if (SalamanderGeneral->StrNICmp(str, AssignedFSName, AssignedFSNameLen) == 0 &&
                        str[AssignedFSNameLen] == ':')
                    {
                        str += AssignedFSNameLen + 1;
                        isFTPS = 2;
                    }
                    else // if it is FTPS, enable SSL encryption
                    {
                        if (SalamanderGeneral->StrNICmp(str, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 &&
                            str[AssignedFSNameLenFTPS] == ':')
                        {
                            str += AssignedFSNameLenFTPS + 1;
                            isFTPS = 1;
                        }
                    }
                    if (isFTPS != 0)
                    {
                        s->EncryptControlConnection = isFTPS == 1 ? 1 : 0;
                        s->EncryptDataConnection = isFTPS == 1 ? 1 : 0;
                    }
                    char *user, *plainPassword, *host, *port, *path;
                    char firstCharOfPath = '/';
                    if (Config.ConvertHexEscSeq)
                        FTPConvertHexEscapeSequences(str);
                    FTPSplitPath(str, &user, &plainPassword, &host, &port, &path, &firstCharOfPath, 0);
                    if (user != NULL && *user != 0) // we have a user name, use it
                    {
                        if (strcmp(FTP_ANONYMOUS, user) == 0 &&
                            (plainPassword == NULL || *plainPassword == 0))
                        {
                            s->AnonymousConnection = TRUE;
                        }
                        else
                        {
                            s->AnonymousConnection = FALSE;
                            UpdateStr(s->UserName, user);
                        }
                    }
                    if (plainPassword != NULL && *plainPassword != 0) // we have a password, use it
                    {
                        // when the password manager is used, we do not know the master password and want to store the password
                        CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                        if (s->SavePassword && passwordManager->IsUsingMasterPassword() && !passwordManager->IsMasterPasswordSet())
                        {
                            // ask for the master password
                            if (!passwordManager->AskForMasterPassword(HWindow))
                            {
                                s->SavePassword = FALSE;
                                CheckDlgButton(HWindow, IDC_SAVEPASSWORD, BST_UNCHECKED);
                            }
                        }
                        s->AnonymousConnection = FALSE;

                        // store the password
                        BYTE* encryptedPassword = NULL; // may be just scrambled
                        int encryptedPasswordSize = 0;
                        // only stored passwords make sense to encrypt
                        BOOL encrypt = s->SavePassword && passwordManager->IsUsingMasterPassword() && passwordManager->IsMasterPasswordSet();
                        if (passwordManager->EncryptPassword(plainPassword, &encryptedPassword, &encryptedPasswordSize, encrypt))
                        {
                            UpdateEncryptedPassword(&s->EncryptedPassword, &s->EncryptedPasswordSize, encryptedPassword, encryptedPasswordSize);
                            // free the buffer allocated in EncryptPassword()
                            memset(encryptedPassword, 0, encryptedPasswordSize);
                            SalamanderGeneral->Free(encryptedPassword);
                        }
                    }
                    UpdateStr(s->Address, HandleNULLStr(host));
                    if (port != NULL && *port != 0) // we have a port, use it
                    {
                        char* t = port;
                        while (*t >= '0' && *t <= '9')
                            t++; // check whether it is a number
                        int p = atoi(port);
                        if (*t == 0 && p >= 1 && p <= 65535) // it is a number and within the allowed range
                            s->Port = atoi(port);
                    }
                    if (path != NULL) // we have a remote path, use it
                    {
                        char pathBuf[FTP_MAX_PATH];
                        CFTPServerPathType type;
                        type = GetFTPServerPathType(NULL, NULL, path);
                        if (type == ftpsptOpenVMS || type == ftpsptMVS || type == ftpsptIBMz_VM ||
                            type == ftpsptOS2) // VMS + MVS + IBM_z/VM + OS/2 (poorly recognizes the Unix path "/C:/path", but it probably will not bother anyone; it is a very unlikely Unix path)
                        {                      // they do not have '/' or '\\' at the start of the path
                            lstrcpyn(pathBuf, path, FTP_MAX_PATH);
                        }
                        else
                        {
                            pathBuf[0] = firstCharOfPath;
                            lstrcpyn(pathBuf + 1, path, FTP_MAX_PATH - 1);
                        }
                        UpdateStr(s->InitialPath, pathBuf);
                    }

                    SelChanged();
                    EnableControls();
                    memset(buf, 0, HOST_MAX_SIZE); // wipe the memory where the password appeared
                }
                break;
            }

            case IDE_INITIALPATH:
            {
                if (HIWORD(wParam) == CBN_KILLFOCUS)
                {
                    char buf[FTP_MAX_PATH];
                    ti.EditLine(IDE_INITIALPATH, buf, FTP_MAX_PATH);
                    unsigned len = (unsigned)strlen(buf);
                    UpdateStr(s->InitialPath, buf);
                    if (len != strlen(buf))
                    {
                        SelChanged();
                        EnableControls();
                    }
                }
                break;
            }

            case IDC_ANONYMOUSLOGIN:
            {
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    ti.CheckBox(IDC_ANONYMOUSLOGIN, s->AnonymousConnection);
                    EnableControls();
                    SelChanged();
                }
                break;
            }

            case IDE_USERNAME:
            {
                if (HIWORD(wParam) == EN_KILLFOCUS && !s->AnonymousConnection)
                {
                    char buf[USER_MAX_SIZE];
                    ti.EditLine(IDE_USERNAME, buf, USER_MAX_SIZE);
                    UpdateStr(s->UserName, buf);
                }
                break;
            }

            case IDE_PASSWORD:
            {
                CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                if (HIWORD(wParam) == EN_KILLFOCUS && !s->AnonymousConnection &&
                    (!s->SavePassword || !passwordManager->IsUsingMasterPassword() || passwordManager->IsMasterPasswordSet())) // just to be safe: exclude the case when the edit box is disabled (editing via the Unlock button)
                {
                    char plainPassword[PASSWORD_MAX_SIZE];
                    ti.EditLine(IDE_PASSWORD, plainPassword, PASSWORD_MAX_SIZE);

                    if (plainPassword[0] != 0)
                    {
                        BYTE* encryptedPassword = NULL; // may be just scrambled
                        int encryptedPasswordSize = 0;
                        // only stored passwords make sense to encrypt
                        BOOL encrypt = s->SavePassword && passwordManager->IsUsingMasterPassword() && passwordManager->IsMasterPasswordSet();
                        if (passwordManager->EncryptPassword(plainPassword, &encryptedPassword, &encryptedPasswordSize, encrypt))
                        {
                            UpdateEncryptedPassword(&s->EncryptedPassword, &s->EncryptedPasswordSize, encryptedPassword, encryptedPasswordSize);
                            // free the buffer allocated in EncryptPassword()
                            memset(encryptedPassword, 0, encryptedPasswordSize);
                            SalamanderGeneral->Free(encryptedPassword);
                        }
                        memset(plainPassword, 0, PASSWORD_MAX_SIZE); // wipe the memory where the password appeared
                    }
                    else
                        UpdateEncryptedPassword(&s->EncryptedPassword, &s->EncryptedPasswordSize, NULL, 0);
                }
                break;
            }

            case IDC_SAVEPASSWORD:
            {
                if (HIWORD(wParam) == BN_CLICKED && !s->AnonymousConnection)
                {
                    ti.CheckBox(IDC_SAVEPASSWORD, s->SavePassword);
                    // if the user checked "Save password" and the password edit line is currently visible, the user uses the master password and the password is not entered
                    BOOL unlockVisible = IsWindowVisible(GetDlgItem(HWindow, IDB_PASSWORD_CHANGE));
                    CSalamanderPasswordManagerAbstract* passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
                    if (!unlockVisible && s->SavePassword && passwordManager->IsUsingMasterPassword() && !passwordManager->IsMasterPasswordSet())
                    {
                        // ask for the master password
                        if (!passwordManager->AskForMasterPassword(HWindow))
                        {
                            // if the user did not enter a valid master password, revert the checkbox to the state before the change
                            s->SavePassword = FALSE; // reflect the check box change in the data
                            CheckDlgButton(HWindow, IDC_SAVEPASSWORD, BST_UNCHECKED);
                        }
                    }
                    else
                    {
                        if (unlockVisible && !s->SavePassword && s->EncryptedPassword == NULL)
                            SelChanged(); // empty password without Save Password enabled -> hide Unlock and show an empty password edit box
                    }
                }
                break;
            }
            }
            break;
        }
        }
        break;
    }
    }

    if (uMsg == DragListboxMsg && wParam == IDL_BOOKMARKS)
    {
        DRAGLISTINFO* pdli = (DRAGLISTINFO*)lParam;
        switch (pdli->uNotification)
        {
        case DL_BEGINDRAG:
        {
            DragIndex = LBItemFromPt(pdli->hWnd, pdli->ptCursor, TRUE);
            if (DragIndex > 0 && DragIndex <= TmpFTPServerList.Count)
            {
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
            }
            else
            {
                DragIndex = -1;
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
            }
            break;
        }

        case DL_DRAGGING:
        {
            if (!ExtraDragDropItemAdded)
            {
                SendMessage(pdli->hWnd, LB_ADDSTRING, 0, (LPARAM) ""); // add an empty string at the end (because of the insertion marker after the items)
                ExtraDragDropItemAdded = TRUE;
            }

            int i = LBItemFromPt(pdli->hWnd, pdli->ptCursor, TRUE);
            DrawInsert(HWindow, pdli->hWnd, i);
            if (i > 0 && i <= TmpFTPServerList.Count + 1 && DragIndex != i &&
                DragIndex + 1 != i)
            {
                if (DragCursor != NULL)
                {
                    SetCursor(DragCursor);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, 0);
                }
                else
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, DL_MOVECURSOR);
            }
            else
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, DL_STOPCURSOR);
            break;
        }

        case DL_DROPPED:
        {
            int topIndex = (int)SendMessage(pdli->hWnd, LB_GETTOPINDEX, 0, 0);
            DrawInsert(HWindow, pdli->hWnd, -1);
            int index = LBItemFromPt(pdli->hWnd, pdli->ptCursor, TRUE);

            // remove the empty string from the end (it was there for the insertion marker after the items)
            int count = (int)SendMessage(pdli->hWnd, LB_GETCOUNT, 0, 0);
            if (count != LB_ERR && count > 0 && ExtraDragDropItemAdded)
            {
                SendMessage(pdli->hWnd, LB_DELETESTRING, count - 1, 0);
                ExtraDragDropItemAdded = FALSE;
            }

            // move the item
            if (DragIndex != -1 && DragIndex != index && DragIndex + 1 != index)
            {
                if (index > DragIndex)
                    index--;
                MoveItem(pdli->hWnd, DragIndex, index, topIndex); // unrealistic drop is handled inside MoveItem
            }
            break;
        }

        case DL_CANCELDRAG:
        {
            DrawInsert(HWindow, pdli->hWnd, -1);
            DragIndex = -1;

            // remove the empty string from the end (it was there for the insertion marker after the items)
            int count = (int)SendMessage(pdli->hWnd, LB_GETCOUNT, 0, 0);
            if (count != LB_ERR && count > 0 && ExtraDragDropItemAdded)
            {
                SendMessage(pdli->hWnd, LB_DELETESTRING, count - 1, 0);
                ExtraDragDropItemAdded = FALSE;
            }
            break;
        }
        }
        return TRUE;
    }

    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CPasswordEditLine
//

CPasswordEditLine::CPasswordEditLine(HWND hDlg, int ctrlID)
    : CWindow(hDlg, ctrlID)
{
}

LRESULT CPasswordEditLine::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_RBUTTONDOWN:
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (IsWindowEnabled(HWindow) && controlPressed && !altPressed && !shiftPressed)
        {
            // verify that the edit line contains something
            char buff[2];
            GetWindowText(HWindow, buff, 2);
            if (buff[0] != 0)
            {
                PostMessage(GetParent(HWindow), WM_APP_SHOWPASSWORD, (WPARAM)HWindow, lParam);
                return 0;
            }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
