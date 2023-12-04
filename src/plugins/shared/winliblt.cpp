// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"
//#include <windows.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif // _MSC_VER
#include <limits.h>
#include <stdio.h>
//#include <commctrl.h>  // potrebuju HIMAGELIST
#include <ostream>
#ifdef __BORLANDC__
#include <stdlib.h>
#endif // __BORLANDC__

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "spl_base.h"
#include "dbg.h"

#ifdef ENABLE_PROPERTYDIALOG
#include "arraylt.h"
#endif // ENABLE_PROPERTYDIALOG

#include "winliblt.h"

#ifdef _MSC_VER
#ifndef itoa
#define itoa _itoa
#endif // itoa
#endif // _MSC_VER

char CWINDOW_CLASSNAME[100] = "";
char CWINDOW_CLASSNAME2[100] = ""; // nema CS_VREDRAW | CS_HREDRAW

ATOM AtomObject = 0; // "property" okna s ukazatelem na objekt (pouzivane ve WindowsManageru)
CWindowsManager WindowsManager;

char WinLibStrings[WLS_COUNT][101] = {
    "Invalid number!",
    "Error"};

FWinLibLTHelpCallback WinLibLTHelpCallback = NULL; // callbacku pro pripojeni na HTML help

//
// ****************************************************************************

void SetWinLibStrings(const char* invalidNumber, const char* error)
{
    lstrcpyn(WinLibStrings[WLS_INVALID_NUMBER], invalidNumber, 100);
    lstrcpyn(WinLibStrings[WLS_ERROR], error, 100);
}

void SetupWinLibHelp(FWinLibLTHelpCallback helpCallback)
{
    WinLibLTHelpCallback = helpCallback;
}

BOOL InitializeWinLib(const char* pluginName, HINSTANCE dllInstance)
{
    lstrcpyn(CWINDOW_CLASSNAME, pluginName, 50);
    strcat(CWINDOW_CLASSNAME, " - WinLib Universal Window");
    lstrcpyn(CWINDOW_CLASSNAME2, pluginName, 50);
    strcat(CWINDOW_CLASSNAME2, " - WinLib Universal Window2");

    AtomObject = GlobalAddAtom("object handle"); // vsechny pluginy budou pouzivat stejny atom, zadna kolize
    if (AtomObject == 0)
    {
        TRACE_E("GlobalAddAtom has failed");
        return FALSE;
    }

    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrls.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES |
                      ICC_TAB_CLASSES | ICC_COOL_CLASSES;
    if (!InitCommonControlsEx(&initCtrls))
    {
        TRACE_E("InitCommonControlsEx failed");
        return FALSE;
    }

    if (!CWindow::RegisterUniversalClass(dllInstance))
    {
        DWORD err = GetLastError();
        TRACE_C("Registration of the universal window has failed. Error=" << err);
        return FALSE;
    }
    return TRUE;
}

void ReleaseWinLib(HINSTANCE dllInstance)
{
    if (WindowsManager.WindowsCount != 0)
    {
        // pruser - po unloadu pluginu muze spadnout soft, protoze se zavola window-procedura
        //          v unloadnutem DLL (jde-li o okna zabita v ramci zabitych threadu, je to OK)
        TRACE_E("Unable to release WinLibLT - some window or dialog (count = " << WindowsManager.WindowsCount << ") is still attached to WinLibLT!");
        // return;  // pokud slo o okno v killnutem threadu, pujde WinLibLT uvolnit, jinak unregister-funkce vrati error
    }

    // provedeme odregistrovani trid, aby pri dalsim loadu pluginu mohly byt znovu zaregistrovany
    if (CWINDOW_CLASSNAME2[0] != 0 && CWINDOW_CLASSNAME[0] != 0)
    {
        if (!UnregisterClass(CWINDOW_CLASSNAME2, dllInstance))
            TRACE_E("UnregisterClass(CWINDOW_CLASSNAME2) failed!");
        if (!UnregisterClass(CWINDOW_CLASSNAME, dllInstance))
            TRACE_E("UnregisterClass(CWINDOW_CLASSNAME) failed!");
    }

    if (AtomObject != 0)
        GlobalDeleteAtom(AtomObject);
}

//
// ****************************************************************************
// CWindow
//
// lpvParam - v pripade, ze se pri CreateWindow zavola CWindow::CWindowProc
//            (je v tride okna), musi obsahovat adresu objektu vytvareneho okna

HWND CWindow::CreateEx(DWORD dwExStyle,        // extended window style
                       LPCTSTR lpszClassName,  // address of registered class name
                       LPCTSTR lpszWindowName, // address of window name
                       DWORD dwStyle,          // window style
                       int x,                  // horizontal position of window
                       int y,                  // vertical position of window
                       int nWidth,             // window width
                       int nHeight,            // window height
                       HWND hwndParent,        // handle of parent or owner window
                       HMENU hmenu,            // handle of menu or child-window identifier
                       HINSTANCE hinst,        // handle of application instance
                       LPVOID lpvParam)        // ukazatel na objekt vytvareneho okna
{
    HWND hWnd = CreateWindowEx(dwExStyle,
                               lpszClassName,
                               lpszWindowName,
                               dwStyle,
                               x,
                               y,
                               nWidth,
                               nHeight,
                               hwndParent,
                               hmenu,
                               hinst,
                               lpvParam);
    if (hWnd != 0)
    {
        if (WindowsManager.GetWindowPtr(hWnd) == NULL) // pokud se jeste neni ve WindowsManageru
            AttachToWindow(hWnd);                      // tak ho pridame -> subclassing
    }
    return hWnd;
}

HWND CWindow::Create(LPCTSTR lpszClassName,  // address of registered class name
                     LPCTSTR lpszWindowName, // address of window name
                     DWORD dwStyle,          // window style
                     int x,                  // horizontal position of window
                     int y,                  // vertical position of window
                     int nWidth,             // window width
                     int nHeight,            // window height
                     HWND hwndParent,        // handle of parent or owner window
                     HMENU hmenu,            // handle of menu or child-window identifier
                     HINSTANCE hinst,        // handle of application instance
                     LPVOID lpvParam)        // ukazatel na objekt vytvareneho okna
{
    return CreateEx(0,
                    lpszClassName,
                    lpszWindowName,
                    dwStyle,
                    x,
                    y,
                    nWidth,
                    nHeight,
                    hwndParent,
                    hmenu,
                    hinst,
                    lpvParam);
}

void CWindow::AttachToWindow(HWND hWnd)
{
    DefWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
    if (DefWndProc == NULL)
    {
        TRACE_E("Bad window handle. hWnd = " << hWnd);
        DefWndProc = DefWindowProc;
        return;
    }
    if (!WindowsManager.AddWindow(hWnd, this))
    {
        TRACE_E("Error during attaching object to window.");
        DefWndProc = DefWindowProc;
        return;
    }
    HWindow = hWnd;
    SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)CWindowProc);

    if (DefWndProc == CWindow::CWindowProc) // to by byla rekurze
    {
        TRACE_C("This should never happen.");
        DefWndProc = DefWindowProc;
    }
}

void CWindow::AttachToControl(HWND dlg, int ctrlID)
{
    if (dlg == NULL)
    {
        TRACE_E("Incorrect call to CWindow::AttachToControl.");
        return;
    }
    HWND hwnd = GetDlgItem(dlg, ctrlID);
    if (hwnd == NULL)
        TRACE_E("Control with ctrlID = " << ctrlID << " is not in dialog.");
    else
        AttachToWindow(hwnd);
}

void CWindow::DetachWindow()
{
    if (HWindow != NULL)
    {
        WindowsManager.DetachWindow(HWindow);
        SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)DefWndProc);
        HWindow = NULL;
    }
}

LRESULT
CWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_HELP:
    {
        if (WinLibLTHelpCallback != NULL && HelpID != -1 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            WinLibLTHelpCallback(HWindow, HelpID);
            return TRUE;
        }
        if (GetWindowLong(HWindow, GWL_STYLE) & WS_CHILD)
            break;   // pokud F1 nezpracujeme a pokud je to child okno, nechame F1 propadnout do parenta
        return TRUE; // pokud to neni child, ukoncime zpracovani F1
    }
    }
    return CallWindowProc((WNDPROC)DefWndProc, HWindow, uMsg, wParam, lParam);
}

LRESULT CALLBACK
CWindow::CWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CWindow* wnd;
    switch (uMsg)
    {
    case WM_CREATE: // prvni zprava - pripojeni objektu k oknu
    {
        // osetrim MDI_CHILD_WINDOW
        if (((CREATESTRUCT*)lParam)->dwExStyle & WS_EX_MDICHILD)
            wnd = (CWindow*)((MDICREATESTRUCT*)((CREATESTRUCT*)lParam)->lpCreateParams)->lParam;
        else
            wnd = (CWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        if (wnd == NULL)
        {
            TRACE_E("Error during creating of window.");
            return FALSE;
        }
        else
        {
            wnd->HWindow = hwnd;
            //--- zarazeni okna podle hwnd do seznamu oken
            if (!WindowsManager.AddWindow(hwnd, wnd)) // chyba
            {
                TRACE_E("Error during creating of window.");
                return FALSE;
            }
        }
        break;
    }

    case WM_DESTROY: // posledni zprava - odpojeni objektu od okna
    {
        wnd = (CWindow*)WindowsManager.GetWindowPtr(hwnd);
        if (wnd != NULL && wnd->Is(otWindow))
        {
            // Petr: posunul jsem dolu pod wnd->WindowProc(), aby behem WM_DESTROY
            //       jeste dochazely zpravy (potreboval Lukas)
            // WindowsManager.DetachWindow(hwnd);

            LRESULT res = wnd->WindowProc(uMsg, wParam, lParam);

            // ted uz zase do stare procedury (kvuli subclassingu)
            WindowsManager.DetachWindow(hwnd);

            // pokud aktualni WndProc je jina nez nase, nebudeme ji menit,
            // protoze nekdo v rade subclasseni uz vratil puvodni WndProc
            WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC);
            if (currentWndProc == CWindow::CWindowProc)
                SetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC, (LONG_PTR)wnd->DefWndProc);

            if (wnd->IsAllocated())
                delete wnd;
            else
                wnd->HWindow = NULL; // uz neni pripojeny
            if (res == 0)
                return 0; // aplikace ji zpracovala
            wnd = NULL;
        }
        break;
    }

    default:
    {
        wnd = (CWindow*)WindowsManager.GetWindowPtr(hwnd);
#if defined(_DEBUG) || defined(__DEBUG_WINLIB)
        if (wnd != NULL && !wnd->Is(otWindow))
        {
            TRACE_C("This should never happen.");
            wnd = NULL;
        }
#endif
    }
    }
    //--- zavolani metody WindowProc(...) prislusneho objektu okna
    if (wnd != NULL)
        return wnd->WindowProc(uMsg, wParam, lParam);
    else
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    // chyba nebo message prisla pred WM_CREATE
}

BOOL CWindow::RegisterUniversalClass(HINSTANCE dllInstance)
{
    WNDCLASS CWindowClass;
    CWindowClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    CWindowClass.lpfnWndProc = CWindow::CWindowProc;
    CWindowClass.cbClsExtra = 0;
    CWindowClass.cbWndExtra = 0;
    CWindowClass.hInstance = dllInstance;
    CWindowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    CWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    CWindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    CWindowClass.lpszMenuName = NULL;
    CWindowClass.lpszClassName = CWINDOW_CLASSNAME;

    BOOL ret = RegisterClass(&CWindowClass) != 0;
    if (ret)
    {
        CWindowClass.style = CS_DBLCLKS;
        CWindowClass.lpszClassName = CWINDOW_CLASSNAME2;
        ret = RegisterClass(&CWindowClass) != 0;
    }

    return ret;
}

BOOL CWindow::RegisterUniversalClass(UINT style, int cbClsExtra, int cbWndExtra, HINSTANCE dllInstance,
                                     HICON hIcon, HCURSOR hCursor, HBRUSH hbrBackground,
                                     LPCTSTR lpszMenuName, LPCTSTR lpszClassName,
                                     HICON hIconSm)
{
    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = style;
    windowClass.lpfnWndProc = CWindow::CWindowProc;
    windowClass.cbClsExtra = cbClsExtra;
    windowClass.cbWndExtra = cbWndExtra;
    windowClass.hInstance = dllInstance;
    windowClass.hIcon = hIcon;
    windowClass.hCursor = hCursor;
    windowClass.hbrBackground = hbrBackground;
    windowClass.lpszMenuName = lpszMenuName;
    windowClass.lpszClassName = lpszClassName;
    windowClass.hIconSm = hIconSm;

    return RegisterClassEx(&windowClass) != 0;
}

//
// ****************************************************************************
// CDialog
//

BOOL CDialog::ValidateData()
{
    CTransferInfo ti(HWindow, ttDataFromWindow);
    Validate(ti);
    if (!ti.IsGood())
    {
        ti.EnsureControlIsFocused(ti.FailCtrlID);
        return FALSE;
    }
    else
        return TRUE;
}

BOOL CDialog::TransferData(CTransferType type)
{
    CTransferInfo ti(HWindow, type);
    Transfer(ti);
    if (!ti.IsGood())
    {
        TRACE_E("CDialog::TransferData(): This error should be detected in Validate() and not in Transfer() because already transferred data cannot be changed to their original values! It means that user cannot leave dialog box without changes using Cancel button now!");
        ti.EnsureControlIsFocused(ti.FailCtrlID);
        return FALSE;
    }
    else
        return TRUE;
}

INT_PTR
CDialog::Execute()
{
    Modal = TRUE;
    return DialogBoxParam(Modul, MAKEINTRESOURCE(ResID), Parent,
                          (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
}

HWND CDialog::Create()
{
    Modal = FALSE;
    return CreateDialogParam(Modul, MAKEINTRESOURCE(ResID), Parent,
                             (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
}

INT_PTR
CDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        TransferData(ttDataToWindow);
        return TRUE; // chci focus od DefDlgProc
    }

    case WM_HELP:
    {
        if (WinLibLTHelpCallback != NULL && HelpID != -1 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            WinLibLTHelpCallback(HWindow, HelpID);
        }
        return TRUE; // F1 nenechame propadnout do parenta ani pokud nevolame WinLibLTHelpCallback()
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDHELP:
        {
            if (WinLibLTHelpCallback != NULL && HelpID != -1 &&
                (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            {
                WinLibLTHelpCallback(HWindow, HelpID);
            }
            else
                TRACE_E("CDialog::DialogProc(): ignoring IDHELP: SetupWinLibHelp() was not called or HelpID is -1!");
            return TRUE;
        }

        case IDOK:
            if (!ValidateData() ||
                !TransferData(ttDataFromWindow))
                return TRUE;
        case IDCANCEL:
        {
            if (Modal)
                EndDialog(HWindow, wParam);
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

INT_PTR CALLBACK
CDialog::CDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CDialog* dlg;
    switch (uMsg)
    {
    case WM_INITDIALOG: // prvni zprava - pripojeni objektu k dialogu
    {
        dlg = (CDialog*)lParam;
        if (dlg == NULL)
        {
            TRACE_E("Error during creating of dialog.");
            return TRUE;
        }
        else
        {
            dlg->HWindow = hwndDlg;
            //--- zarazeni okna podle hwndDlg do seznamu oken
            if (!WindowsManager.AddWindow(hwndDlg, dlg)) // chyba
            {
                TRACE_E("Error during creating of dialog.");
                return TRUE;
            }
            dlg->NotifDlgJustCreated(); // zavedeno jako misto pro upravu layoutu dialogu
        }
        break;
    }

    case WM_DESTROY: // posledni zprava - odpojeni objektu od dialogu
    {
        dlg = (CDialog*)WindowsManager.GetWindowPtr(hwndDlg);
        INT_PTR ret = FALSE; // pro pripad, ze ji nezpracuje
        if (dlg != NULL && dlg->Is(otDialog))
        {
            // Petr: posunul jsem dolu pod wnd->WindowProc(), aby behem WM_DESTROY
            //       jeste dochazely zpravy (potreboval Lukas)
            // WindowsManager.DetachWindow(hwndDlg);

            ret = dlg->DialogProc(uMsg, wParam, lParam);

            WindowsManager.DetachWindow(hwndDlg);
            if (dlg->IsAllocated())
                delete dlg;
            else
                dlg->HWindow = NULL; // informace o odpojeni
        }
        return ret;
    }

    default:
    {
        dlg = (CDialog*)WindowsManager.GetWindowPtr(hwndDlg);
#if defined(_DEBUG) || defined(__DEBUG_WINLIB)
        if (dlg != NULL && !dlg->Is(otDialog))
        {
            TRACE_C("This should never happen.");
            dlg = NULL;
        }
#endif
    }
    }
    //--- zavolani metody DialogProc(...) prislusneho objektu dialogu
    if (dlg != NULL)
        return dlg->DialogProc(uMsg, wParam, lParam);
    else
        return FALSE; // chyba nebo message neprisla mezi WM_INITDIALOG a WM_DESTROY
}

//
// ****************************************************************************
// CPropSheetPage
//

#ifdef ENABLE_PROPERTYDIALOG

CPropSheetPage::CPropSheetPage(char* title, HINSTANCE modul, int resID,
                               DWORD flags, HICON icon, CObjectOrigin origin)
    : CDialog(modul, resID, NULL, origin)
{
    Init(title, modul, resID, icon, flags, origin);
}

CPropSheetPage::CPropSheetPage(char* title, HINSTANCE modul, int resID, int helpID,
                               DWORD flags, HICON icon, CObjectOrigin origin)
    : CDialog(modul, resID, helpID, NULL, origin)
{
    Init(title, modul, resID, icon, flags, origin);
}

void CPropSheetPage::Init(char* title, HINSTANCE modul, int resID,
                          HICON icon, DWORD flags, CObjectOrigin origin)
{
    Title = NULL;
    if (title != NULL)
    {
        int len = (int)strlen(title);
        Title = new char[len + 1];
        if (Title != NULL)
            strcpy(Title, title);
        else
            TRACE_E("Low memory!");
    }
    Flags = flags;
    Icon = icon;

    ParentDialog = NULL; // nastavuje se z CPropertyDialog::Execute()
}

CPropSheetPage::~CPropSheetPage()
{
    if (Title != NULL)
        delete[] Title;
}

BOOL CPropSheetPage::ValidateData()
{
    CTransferInfo ti(HWindow, ttDataFromWindow);
    Validate(ti);
    if (!ti.IsGood())
    {
        if (PropSheet_GetCurrentPageHwnd(Parent) != HWindow)
            PropSheet_SetCurSel(Parent, HWindow, 0);

        ti.EnsureControlIsFocused(ti.FailCtrlID);
        return FALSE;
    }
    else
        return TRUE;
}

BOOL CPropSheetPage::TransferData(CTransferType type)
{
    CTransferInfo ti(HWindow, type);
    Transfer(ti);
    if (!ti.IsGood())
    {
        if (ti.Type == ttDataFromWindow &&
            PropSheet_GetCurrentPageHwnd(Parent) != HWindow)
            PropSheet_SetCurSel(Parent, HWindow, 0);

        ti.EnsureControlIsFocused(ti.FailCtrlID);
        return FALSE;
    }
    else
        return TRUE;
}

HPROPSHEETPAGE
CPropSheetPage::CreatePropSheetPage()
{
    PROPSHEETPAGE psp;
    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = Flags;
    psp.hInstance = Modul;
    psp.pszTemplate = MAKEINTRESOURCE(ResID);
    psp.hIcon = Icon;
    psp.pszTitle = Title;
    psp.pfnDlgProc = CPropSheetPage::CPropSheetPageProc;
    psp.lParam = (LPARAM)this;
    psp.pfnCallback = NULL;
    psp.pcRefParent = NULL;
    return CreatePropertySheetPage(&psp);
}

INT_PTR
CPropSheetPage::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        ParentDialog->HWindow = Parent;
        TransferData(ttDataToWindow);
        return TRUE; // chci focus od DefDlgProc
    }

    case WM_HELP:
    {
        if (WinLibLTHelpCallback != NULL && HelpID != -1 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            WinLibLTHelpCallback(HWindow, HelpID);
            return TRUE;
        }
        break; // F1 nechame propadnout do parenta
    }

    case WM_NOTIFY:
    {
        if (((NMHDR*)lParam)->code == PSN_KILLACTIVE) // deaktivace stranky
        {
            if (ValidateData())
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
            else // nepovolime deaktivaci stranky
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_HELP)
        { // stisknuto tlacitko Help
            if (WinLibLTHelpCallback != NULL && HelpID != -1)
                WinLibLTHelpCallback(HWindow, HelpID);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_SETACTIVE) // aktivace stranky
        {
            if (ParentDialog != NULL && ParentDialog->LastPage != NULL)
            { // zapamatovani posledni stranky
                *ParentDialog->LastPage = ParentDialog->GetCurSel();
            }
            break;
        }

        if (((NMHDR*)lParam)->code == PSN_APPLY)
        { // stisknuto tlacitko ApplyNow nebo OK
            if (TransferData(ttDataFromWindow))
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, PSNRET_NOERROR);
            else
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_WIZFINISH)
        { // stisknuto tlacitko Finish
            // neprislo PSN_KILLACTIVE - provedu validaci
            if (!ValidateData())
            {
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                return TRUE;
            }

            // obehnu vsechny stranky pro transfer
            for (int i = 0; i < ParentDialog->Count; i++)
            {
                if (ParentDialog->At(i)->HWindow != NULL)
                {
                    if (!ParentDialog->At(i)->TransferData(ttDataFromWindow))
                    {
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                        return TRUE;
                    }
                }
            }
            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

INT_PTR CALLBACK
CPropSheetPage::CPropSheetPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam)
{
    CPropSheetPage* dlg;
    switch (uMsg)
    {
    case WM_INITDIALOG: // prvni zprava - pripojeni objektu k dialogu
    {
        dlg = (CPropSheetPage*)((PROPSHEETPAGE*)lParam)->lParam;
        if (dlg == NULL)
        {
            TRACE_E("Error during creating of dialog.");
            return TRUE;
        }
        else
        {
            dlg->HWindow = hwndDlg;
            dlg->Parent = ::GetParent(hwndDlg);
            //--- zarazeni okna podle hwndDlg do seznamu oken
            if (!WindowsManager.AddWindow(hwndDlg, dlg)) // chyba
            {
                TRACE_E("Error during creating of dialog.");
                return TRUE;
            }
            dlg->NotifDlgJustCreated(); // zavedeno jako misto pro upravu layoutu dialogu
        }
        break;
    }

    case WM_DESTROY: // posledni zprava - odpojeni objektu od dialogu
    {
        dlg = (CPropSheetPage*)WindowsManager.GetWindowPtr(hwndDlg);
        INT_PTR ret = FALSE; // pro pripad, ze ji nezpracuje
        if (dlg != NULL && dlg->Is(otDialog))
        {
            // Petr: posunul jsem dolu pod wnd->WindowProc(), aby behem WM_DESTROY
            //       jeste dochazely zpravy (potreboval Lukas)
            // WindowsManager.DetachWindow(hwndDlg);

            ret = dlg->DialogProc(uMsg, wParam, lParam);

            WindowsManager.DetachWindow(hwndDlg);
            if (dlg->IsAllocated())
                delete dlg;
            else
                dlg->HWindow = NULL; // informace o odpojeni
        }
        return ret;
    }

    default:
    {
        dlg = (CPropSheetPage*)WindowsManager.GetWindowPtr(hwndDlg);
#if defined(_DEBUG) || defined(__DEBUG_WINLIB)
        if (dlg != NULL && !dlg->Is(otPropSheetPage))
        {
            TRACE_C("This should never happen.");
            dlg = NULL;
        }
#endif
    }
    }
    //--- zavolani metody DialogProc(...) prislusneho objektu dialogu
    if (dlg != NULL)
        return dlg->DialogProc(uMsg, wParam, lParam);
    else
        return FALSE; // chyba nebo message neprisla mezi WM_INITDIALOG a WM_DESTROY
}

//
// ****************************************************************************
// CPropertyDialog
//

INT_PTR
CPropertyDialog::Execute()
{
    if (Count > 0)
    {
        PROPSHEETHEADER psh;
        psh.dwSize = sizeof(PROPSHEETHEADER);
        psh.dwFlags = Flags;
        psh.hwndParent = Parent;
        psh.hInstance = Modul;
        psh.hIcon = Icon;
        psh.pszCaption = Caption;
        psh.nPages = Count;
        if (StartPage < 0 || StartPage >= Count)
            StartPage = 0;
        psh.nStartPage = StartPage;
        HPROPSHEETPAGE* pages = new HPROPSHEETPAGE[Count];
        if (pages == NULL)
        {
            TRACE_E("Low memory!");
            return -1;
        }
        psh.phpage = pages;
        for (int i = 0; i < Count; i++)
        {
            psh.phpage[i] = At(i)->CreatePropSheetPage();
            At(i)->ParentDialog = this;
        }
        psh.pfnCallback = Callback;
        INT_PTR ret = PropertySheet(&psh);
        delete pages;
        return ret;
    }
    else
    {
        TRACE_E("Incorrect call to CPropertyDialog::Execute.");
        return -1;
    }
}

int CPropertyDialog::GetCurSel()
{
    HWND tabCtrl = PropSheet_GetTabControl(HWindow);
    return TabCtrl_GetCurSel(tabCtrl);
}

#endif // ENABLE_PROPERTYDIALOG

//
// ****************************************************************************
// CWindowsManager
//

BOOL CWindowsManager::AddWindow(HWND hWnd, CWindowsObject* wnd)
{
    if (AtomObject == 0)
    {
        TRACE_E("Uninitialized AtomObject - you should call InitializeWinLib() before first use of WinLib.");
        return FALSE;
    }
    if (!SetProp(hWnd, (LPCTSTR)AtomObject, (HANDLE)wnd))
    {
        DWORD err = GetLastError();
        TRACE_E("SetProp has failed (err=" << err << ")");
        return FALSE;
    }
    WindowsCount++;
    return TRUE;
}

void CWindowsManager::DetachWindow(HWND hWnd)
{
    if (RemoveProp(hWnd, (LPCTSTR)AtomObject))
    {
        WindowsCount--;
    }
    else
    {
        TRACE_E("RemoveProp has failed. hwnd = " << hWnd);
    }
}

CWindowsObject*
CWindowsManager::GetWindowPtr(HWND hWnd)
{
    return (CWindowsObject*)GetProp(hWnd, (LPCTSTR)AtomObject);
}

//
// ****************************************************************************
// CWindowQueue
//

CWindowQueue::~CWindowQueue()
{
    if (!Empty())
        TRACE_E("Some window is still opened in " << QueueName << " queue!"); // nemelo by nastat...
    // tady uz multi-threadovost nehrozi (konci plugin, thready jsou/byly ukonceny)
    // dealokujeme aspon nejakou pamet
    CWindowQueueItem* last;
    CWindowQueueItem* item = Head;
    while (item != NULL)
    {
        last = item;
        item = item->Next;
        delete last;
    }
}

BOOL CWindowQueue::Add(CWindowQueueItem* item)
{
    CS.Enter();
    if (item != NULL)
    {
        item->Next = Head;
        Head = item;
        CS.Leave();
        return TRUE;
    }
    CS.Leave();
    return FALSE;
}

void CWindowQueue::Remove(HWND hWindow)
{
    CS.Enter();
    CWindowQueueItem* last = NULL;
    CWindowQueueItem* item = Head;
    while (item != NULL)
    {
        if (item->HWindow == hWindow) // nalezeno, odstranime
        {
            if (last != NULL)
                last->Next = item->Next;
            else
                Head = item->Next;
            delete item;
            CS.Leave();
            return;
        }
        last = item;
        item = item->Next;
    }
    CS.Leave();
}

BOOL CWindowQueue::Empty()
{
    BOOL e;
    CS.Enter();
    e = Head == NULL;
    CS.Leave();
    return e;
}

void CWindowQueue::BroadcastMessage(DWORD uMsg, WPARAM wParam, LPARAM lParam)
{
    CS.Enter();
    CWindowQueueItem* item = Head;
    while (item != NULL)
    {
        PostMessage(item->HWindow, uMsg, wParam, lParam);
        item = item->Next;
    }
    CS.Leave();
}

BOOL CWindowQueue::CloseAllWindows(BOOL force, int waitTime, int forceWaitTime)
{
    // posleme zadost o zavreni vsech oken
    BroadcastMessage(WM_CLOSE, 0, 0);

    // pockame az/jestli se zavrou
    DWORD ti = GetTickCount();
    DWORD w = force ? forceWaitTime : waitTime;
    while ((w == INFINITE || w > 0) && !Empty())
    {
        DWORD t = GetTickCount() - ti;
        if (w == INFINITE || t < w) // mame jeste cekat
        {
            if (w == INFINITE || 50 < w - t)
                Sleep(50);
            else
            {
                Sleep(w - t);
                break;
            }
        }
        else
            break;
    }
    return force || Empty();
}

//
// ****************************************************************************
// CTransferInfo
//

BOOL CTransferInfo::GetControl(HWND& ctrlHWnd, int ctrlID, BOOL ignoreIsGood)
{
    if (!ignoreIsGood && !IsGood())
        return FALSE; // dalsi nema cenu zpracovavat
    ctrlHWnd = GetDlgItem(HDialog, ctrlID);
    if (ctrlHWnd == NULL)
    {
        TRACE_E("Control with ctrlID = " << ctrlID << " is not in dialog.");
        FailCtrlID = ctrlID;
        return FALSE;
    }
    else
        return TRUE;
}

void CTransferInfo::EnsureControlIsFocused(int ctrlID)
{
    HWND ctrl = GetDlgItem(HDialog, ctrlID);
    if (ctrl != NULL)
    {
        HWND wnd = GetFocus();
        while (wnd != NULL && wnd != ctrl)
            wnd = ::GetParent(wnd);
        if (wnd == NULL) // fokusime jen pokud neni ctrl predek GetFocusu
        {                // jako napr. edit-line v combo-boxu
            SendMessage(HDialog, WM_NEXTDLGCTL, (WPARAM)ctrl, TRUE);
        }
    }
    else
        TRACE_E("Control with ctrlID = " << ctrlID << " is not in dialog.");
}

void CTransferInfo::EditLine(int ctrlID, char* buffer, DWORD bufferSize, BOOL select)
{
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, bufferSize - 1, 0);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)buffer);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, bufferSize, (LPARAM)buffer);
            break;
        }
        }
    }
}

void CTransferInfo::EditLine(int ctrlID, double& value, char* format, BOOL select)
{
    HWND HWindow;
    char buff[31];
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, 30, 0);
            sprintf(buff, format, value);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)buff);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, 31, (LPARAM)buff);
            char* s = buff;
            BOOL decPoints = FALSE;
            BOOL expPart = FALSE;
            if (*s == '-' || *s == '+')
                s++;        // preskok znamenka
            while (*s != 0) // prevod carky na tecku
            {
                if (!expPart && !decPoints && (*s == ',' || *s == '.'))
                {
                    decPoints = TRUE;
                    *s = '.';
                }
                else
                {
                    if (!expPart && (*s == 'e' || *s == 'E'))
                    {
                        expPart = TRUE;
                        if (*(s + 1) == '+' || *(s + 1) == '-')
                            s++; // preskok +- za E
                    }
                    else
                    {
                        if (*s < '0' || *s > '9')
                        {
                            MessageBox(HWindow, WinLibStrings[WLS_INVALID_NUMBER], WinLibStrings[WLS_ERROR],
                                       MB_OK | MB_ICONEXCLAMATION);
                            ErrorOn(ctrlID);
                            break;
                        }
                    }
                }
                s++;
            }
            if (*s == 0)
                value = atof(buff); // jen pokud je cislo
            else
                value = 0; // pri chybe dame nulu
            break;
        }
        }
    }
}

void CTransferInfo::EditLine(int ctrlID, int& value, BOOL select)
{
    HWND HWindow;
    char buff[16];
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, 15, 0);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)itoa(value, buff, 10));
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, 16, (LPARAM)buff);

            char* s = buff;
            if (*s == '-' || *s == '+')
                s++;        // preskok znamenka
            while (*s != 0) // kontrola cisla
            {
                if (*s < '0' || *s > '9')
                {
                    MessageBox(HWindow, WinLibStrings[WLS_INVALID_NUMBER], WinLibStrings[WLS_ERROR],
                               MB_OK | MB_ICONEXCLAMATION);
                    ErrorOn(ctrlID);
                    break;
                }
                s++;
            }

            char* endptr;
            value = strtoul(buff, &endptr, 10); // nahrada za atoi / _ttoi, ktere misto 4000000000 vraci 2147483647 (protoze je to SIGNED INT)
            break;
        }
        }
    }
}

void CTransferInfo::RadioButton(int ctrlID, int ctrlValue, int& value)
{
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, BM_SETCHECK, ctrlValue == value, 0);
            break;
        }

        case ttDataFromWindow:
        {
            if (SendMessage(HWindow, BM_GETCHECK, 0, 0) == 1)
                value = ctrlValue;
            break;
        }
        }
    }
}

void CTransferInfo::CheckBox(int ctrlID, int& value)
{
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, BM_SETCHECK, value, 0);
            break;
        }

        case ttDataFromWindow:
        {
            value = (int)SendMessage(HWindow, BM_GETCHECK, 0, 0);
            break;
        }
        }
    }
}
