// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <commctrl.h>

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"
#include "str.h"
#include "winlib.h"
#include "sheets.h"
#include "tablist.h"
#include "tserver.h"
#include "dialog.h"
#include "registry.h"
#include "config.h"

#include "tserver.rh"
#include "tserver.rh2"

DWORD LastPSPIndex = 0; // naposledy otevrena stranka

//****************************************************************************
//
// CAboutDialog
//

void CAboutDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        HWND hCtrl;
        if (ti.GetControl(hCtrl, IDC_ABOUT_TEXT1))
            SetWindowText(hCtrl, AboutText1);
    }
}

//****************************************************************************
//
// CCenterDialog
//

BOOL CCenterDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RECT dRect;
        GetWindowRect(HWindow, &dRect);
        int w = dRect.right - dRect.left;
        int h = dRect.bottom - dRect.top;
        RECT wRect;
        GetWindowRect(MainWindow->HWindow, &wRect);
        int x = wRect.left + (wRect.right - wRect.left) / 2 - w / 2;
        int y = wRect.top + (wRect.bottom - wRect.top) / 2 - h / 2;
        MoveWindow(HWindow, x, y, w, h, TRUE);
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPSPGeneral
//

void CPSPGeneral::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_PSP1_TOOLBAR_CAPTION, ConfigData.UseToolbarCaption);
    ti.CheckBox(IDC_PSP1_ALWAYS_ON_TOP, ConfigData.AlwaysOnTop);
    ti.CheckBox(IDC_PSP1_USE_MAX_MESSAGES, ConfigData.UseMaxMessagesCount);
    ti.CheckBox(IDC_PSP1_SCROLL_LATEST, ConfigData.ScrollToLatestMessage);
    ti.CheckBox(IDC_PSP1_SHOW_ON_ERROR, ConfigData.ShowOnErrorMessage);
    ti.CheckBox(IDC_PSP1_AUTO_CLEAR, ConfigData.AutoClear);
    ti.EditLine(IDC_PSP1_MAX_MESSAGES, ConfigData.MaxMessagesCount);

    if (ti.Type == ttDataToWindow)
    {
        SendDlgItemMessage(HWindow, IDC_PSP1_HOTKEY, HKM_SETHOTKEY, ConfigData.HotKey, 0);
        SendDlgItemMessage(HWindow, IDC_PSP1_HOTKEYCLEAR, HKM_SETHOTKEY, ConfigData.HotKeyClear, 0);
    }
    if (ti.Type == ttDataFromWindow)
    {
        ConfigData.HotKey = (WORD)SendDlgItemMessage(HWindow, IDC_PSP1_HOTKEY, HKM_GETHOTKEY, 0, 0);
        ConfigData.HotKeyClear = (WORD)SendDlgItemMessage(HWindow, IDC_PSP1_HOTKEYCLEAR, HKM_GETHOTKEY, 0, 0);
    }
    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CPSPGeneral::Validate(CTransferInfo& ti)
{
    BOOL useMax;
    ti.CheckBox(IDC_PSP1_USE_MAX_MESSAGES, useMax);
    if (useMax)
    {
        int value;
        ti.EditLine(IDC_PSP1_MAX_MESSAGES, value);
        if (value < 100 || value > 1000000)
        {
            MESSAGE_EW(HWindow, L"Value must be in range 100 - 1000000.", MB_OK);
            SetFocus(GetDlgItem(HWindow, IDC_PSP1_MAX_MESSAGES));
            ti.ErrorOn(IDC_PSP1_MAX_MESSAGES);
        }
    }
}

void CPSPGeneral::EnableControls()
{
    BOOL use = IsDlgButtonChecked(HWindow, IDC_PSP1_USE_MAX_MESSAGES) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDC_PSP1_MAX_MESSAGES), use);
}

BOOL CPSPGeneral::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == EN_CHANGE)
            SendMessage(Parent, PSM_CHANGED, (WPARAM)HWindow, 0);
        if (HIWORD(wParam) == BN_CLICKED || LOWORD(wParam) == IDC_PSP1_USE_MAX_MESSAGES)
            EnableControls();
        break;
    }

    case WM_NOTIFY:
    {
        switch (((NMHDR*)lParam)->code)
        {
        case PSN_APPLY:
        {
            int ret = CPropSheetPage::DialogProc(uMsg, wParam, lParam);
            LONG oldExStyle = GetWindowLong(MainWindow->HWindow, GWL_EXSTYLE);
            if (((oldExStyle & WS_EX_TOOLWINDOW) != 0) != ConfigData.UseToolbarCaption)
            {
                if (ConfigData.UseToolbarCaption)
                {
                    oldExStyle |= WS_EX_TOOLWINDOW;
                    ShowWindow(MainWindow->HWindow, SW_HIDE);
                }
                else
                {
                    oldExStyle &= ~WS_EX_TOOLWINDOW;
                }
                SetWindowLong(MainWindow->HWindow, GWL_EXSTYLE, oldExStyle);

                WINDOWPLACEMENT wp;
                GetWindowPlacement(MainWindow->HWindow, &wp);
                wp.flags |= SW_SHOW;
                wp.rcNormalPosition.bottom++;
                SetWindowPlacement(MainWindow->HWindow, &wp);
                wp.rcNormalPosition.bottom--;
                SetWindowPlacement(MainWindow->HWindow, &wp);
            }
            if (ConfigData.AlwaysOnTop)
            {
                SetWindowPos(MainWindow->HWindow, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE);
            }
            else
            {
                SetWindowPos(MainWindow->HWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE);
            }
            if (MainWindow->HasHotKey)
                UnregisterHotKey(MainWindow->HWindow, HOT_KEY_ID);
            MainWindow->HasHotKey = RegisterHotKey(MainWindow->HWindow, HOT_KEY_ID, HIBYTE(ConfigData.HotKey), LOBYTE(ConfigData.HotKey));
            if (MainWindow->HasHotKeyClear)
                UnregisterHotKey(MainWindow->HWindow, HOT_KEYCLEAR_ID);
            MainWindow->HasHotKeyClear = RegisterHotKey(MainWindow->HWindow, HOT_KEYCLEAR_ID, HIBYTE(ConfigData.HotKeyClear), LOBYTE(ConfigData.HotKeyClear));
            Registry.Save();
            return ret;
        }
        }
        break;
    }
    }
    return CPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPSPView
//

void CPSPView::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_PSP2_TYPE, ConfigData.ViewColumnVisible_Type);
    ti.CheckBox(IDC_PSP2_PID, ConfigData.ViewColumnVisible_PID);
    ti.CheckBox(IDC_PSP2_UPID, ConfigData.ViewColumnVisible_UPID);
    ti.CheckBox(IDC_PSP2_PNAME, ConfigData.ViewColumnVisible_PName);
    ti.CheckBox(IDC_PSP2_TID, ConfigData.ViewColumnVisible_TID);
    ti.CheckBox(IDC_PSP2_UTID, ConfigData.ViewColumnVisible_UTID);
    ti.CheckBox(IDC_PSP2_TNAME, ConfigData.ViewColumnVisible_TName);
    ti.CheckBox(IDC_PSP2_DATE, ConfigData.ViewColumnVisible_Date);
    ti.CheckBox(IDC_PSP2_TIME, ConfigData.ViewColumnVisible_Time);
    ti.CheckBox(IDC_PSP2_COUNTER, ConfigData.ViewColumnVisible_Counter);
    ti.CheckBox(IDC_PSP2_MODUL, ConfigData.ViewColumnVisible_Modul);
    ti.CheckBox(IDC_PSP2_LINE, ConfigData.ViewColumnVisible_Line);
    ti.CheckBox(IDC_PSP2_MESSAGE, ConfigData.ViewColumnVisible_Message);
}

BOOL CPSPView::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED || HIWORD(wParam) == EN_CHANGE)
            SendMessage(Parent, PSM_CHANGED, (WPARAM)HWindow, 0);
        break;
    }

    case WM_NOTIFY:
    {
        switch (((NMHDR*)lParam)->code)
        {
        case PSN_APPLY:
        {
            int ret = CPropSheetPage::DialogProc(uMsg, wParam, lParam);
            PostMessage(MainWindow->TabList->HWindow, WM_USER_HEADER_CHANGE, 0, 0);
            Registry.Save();
            return ret;
        }
        }
        break;
    }
    }

    return CPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CSetupDialog
//

BOOL DoSetupDialog(HWND hWindow)
{
#define PAGES_COUNT 2
    const WCHAR* strBuff[PAGES_COUNT + 1] =
        {
            L"Configuration", // caption
            L"General",       // prvni zalozka
            L"View",
        };

    MainWindow->TabList->GetHeaderWidths();

    CPropertyDialog propDlg(hWindow, HInstance, strBuff[0], LastPSPIndex, 0, NULL, &LastPSPIndex);

    CPSPGeneral pageGeneral(strBuff[1], HInstance, IDD_PSP_GENERAL, PSP_USETITLE);
    propDlg.Add(&pageGeneral);

    CPSPView pageView(strBuff[2], HInstance, IDD_PSP_VIEW, PSP_USETITLE);
    propDlg.Add(&pageView);

    IconControlEnable = FALSE;
    propDlg.Execute();
    IconControlEnable = TRUE;
    return TRUE;
}

//****************************************************************************
//
// CDetailsDialog
//

CDetailsDialog::CDetailsDialog(HWND parent, const CGlobalDataMessage* message)
    : CDialog(HInstance, IDD_DETAILS, parent)
{
    Message = message;
};

void CDetailsDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        ti.EditLine(IDC_DETAILS_MODULE, Message->File, 2 * MAX_PATH);
        int val = Message->Line;
        ti.EditLine(IDC_DETAILS_LINE, val);
        ti.EditLine(IDC_DETAILS_MSG, Message->Message, 5000);
    }
}

BOOL CDetailsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ACTIVATEAPP:
    {
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}
