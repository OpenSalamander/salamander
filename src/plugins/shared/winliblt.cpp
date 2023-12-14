// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
//#include <commctrl.h>  // HIMAGELIST is needed
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

ATOM AtomObject = 0; // windows "property" with a pointer to the object (used in WindowsManager)
CWindowsManager WindowsManager;

char WinLibStrings[WLS_COUNT][101] = {
    "Invalid number!",
    "Error"};

FWinLibLTHelpCallback WinLibLTHelpCallback = NULL; // callback for connecting to HTML help

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

    AtomObject = GlobalAddAtom("object handle"); // all plugins will use the same atom, no collision
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
        // a difficult situation - after unload of the plugin, the soft can crash, because the window procedure is called
        //                         in the unloaded DLL (if the window was killed in the killed thread, it is OK)
        TRACE_E("Unable to release WinLibLT - some window or dialog (count = " << WindowsManager.WindowsCount << ") is still attached to WinLibLT!");
        // return;  // if it was a window in the killed thread, WinLibLT will be able to be released, otherwise the unregister function will return an error
    }

    // deregister classes, so that they can be registered again when the plugin is loaded again
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
// lpvParam - in case of calling CreateWindow from CWindow::CWindowProc
//            (it is in the window class), it must contain the address of the created window object

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
                       LPVOID lpvParam)        // pointer to the object of the created window
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
        if (WindowsManager.GetWindowPtr(hWnd) == NULL) // if it is not in WindowsManager yet
            AttachToWindow(hWnd);                      // we will add it -> subclassing
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
                     LPVOID lpvParam)        // pointer to the object of the created window
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

    if (DefWndProc == CWindow::CWindowProc) // this would be recursion
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
            break;   // if we won't process F1 and it is a child window, we will let F1 go to the parent
        return TRUE; // if it is not a child, we will end processing of F1
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
    case WM_CREATE: // first message - attaching the object to the window
    {
        // handling MDI_CHILD_WINDOW
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
            //--- including the window by hwnd into the list of windows
            if (!WindowsManager.AddWindow(hwnd, wnd)) // error
            {
                TRACE_E("Error during creating of window.");
                return FALSE;
            }
        }
        break;
    }

    case WM_DESTROY: // the last message - detaching the object from the window
    {
        wnd = (CWindow*)WindowsManager.GetWindowPtr(hwnd);
        if (wnd != NULL && wnd->Is(otWindow))
        {
            // Petr: I moved it down below wnd->WindowProc() so that during WM_DESTROY
            //       messages could still arrive (Lukas needed it)
            // WindowsManager.DetachWindow(hwnd);

            LRESULT res = wnd->WindowProc(uMsg, wParam, lParam);

            // now into the old procedure again (because of subclassing)
            WindowsManager.DetachWindow(hwnd);

            // if the current WndProc is different from ours, we won't change it,
            // because someone in the subclassing queue has already returned the original WndProc
            WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC);
            if (currentWndProc == CWindow::CWindowProc)
                SetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC, (LONG_PTR)wnd->DefWndProc);

            if (wnd->IsAllocated())
                delete wnd;
            else
                wnd->HWindow = NULL; // not connected anymore
            if (res == 0)
                return 0; // processed by application
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
    //--- calling the method WindowProc(...) of the related object of the window
    if (wnd != NULL)
        return wnd->WindowProc(uMsg, wParam, lParam);
    else
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    // an error or the message came before WM_CREATE
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
        return TRUE; // focus from DefDlgProc is needed
    }

    case WM_HELP:
    {
        if (WinLibLTHelpCallback != NULL && HelpID != -1 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            WinLibLTHelpCallback(HWindow, HelpID);
        }
        return TRUE; // F1 won't be passed to the parent even if we don't call WinLibLTHelpCallback()
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
    case WM_INITDIALOG: // first message - attaching the object to the window
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
            //--- including the window by hwndDlg into the list of windows
            if (!WindowsManager.AddWindow(hwndDlg, dlg)) // error
            {
                TRACE_E("Error during creating of dialog.");
                return TRUE;
            }
            dlg->NotifDlgJustCreated(); // loaded as a place for layout modification of the dialog
        }
        break;
    }

    case WM_DESTROY: // the last message - detaching the object from the window
    {
        dlg = (CDialog*)WindowsManager.GetWindowPtr(hwndDlg);
        INT_PTR ret = FALSE; // in case it won't be processed
        if (dlg != NULL && dlg->Is(otDialog))
        {
            // Petr: I moved it down below wnd->WindowProc() so that during WM_DESTROY
            //       messages could still arrive (Lukas needed it)
            // WindowsManager.DetachWindow(hwndDlg);

            ret = dlg->DialogProc(uMsg, wParam, lParam);

            WindowsManager.DetachWindow(hwndDlg);
            if (dlg->IsAllocated())
                delete dlg;
            else
                dlg->HWindow = NULL; // connection information
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
    //--- calling the method DialogProc(...) of the related object of the dialog
    if (dlg != NULL)
        return dlg->DialogProc(uMsg, wParam, lParam);
    else
        return FALSE; // the error or the message did not come between WM_INITDIALOG and WM_DESTROY
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

    ParentDialog = NULL; // set from CPropertyDialog::Execute()
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
        return TRUE; // focus from DefDlgProc is needed
    }

    case WM_HELP:
    {
        if (WinLibLTHelpCallback != NULL && HelpID != -1 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
        {
            WinLibLTHelpCallback(HWindow, HelpID);
            return TRUE;
        }
        break; // F1 to be passed to the parent
    }

    case WM_NOTIFY:
    {
        if (((NMHDR*)lParam)->code == PSN_KILLACTIVE) // page deactivation
        {
            if (ValidateData())
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
            else // we won't allow deactivation of the page
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_HELP)
        { // Help button pressed
            if (WinLibLTHelpCallback != NULL && HelpID != -1)
                WinLibLTHelpCallback(HWindow, HelpID);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_SETACTIVE) // page activation
        {
            if (ParentDialog != NULL && ParentDialog->LastPage != NULL)
            { // remembering the last page
                *ParentDialog->LastPage = ParentDialog->GetCurSel();
            }
            break;
        }

        if (((NMHDR*)lParam)->code == PSN_APPLY)
        { // ApplyNow or OK button pressed
            if (TransferData(ttDataFromWindow))
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, PSNRET_NOERROR);
            else
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_WIZFINISH)
        {   // Finish button pressed
            // PSN_KILLACTIVE not received - will validate
            if (!ValidateData())
            {
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                return TRUE;
            }

            // going through all pages for transfer
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
    case WM_INITDIALOG: // the first message - attaching the object to the dialog
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
            //--- including the window by hwndDlg into the list of windows
            if (!WindowsManager.AddWindow(hwndDlg, dlg)) // chyba
            {
                TRACE_E("Error during creating of dialog.");
                return TRUE;
            }
            dlg->NotifDlgJustCreated(); // loaded as a place for layout modification of the dialog
        }
        break;
    }

    case WM_DESTROY: // the last message - detaching the object from the dialog
    {
        dlg = (CPropSheetPage*)WindowsManager.GetWindowPtr(hwndDlg);
        INT_PTR ret = FALSE; // in case it won't be processed
        if (dlg != NULL && dlg->Is(otDialog))
        {
            // Petr: I moved it down below wnd->WindowProc() so that during WM_DESTROY
            //       messages could still arrive (Lukas needed it)
            // WindowsManager.DetachWindow(hwndDlg);

            ret = dlg->DialogProc(uMsg, wParam, lParam);

            WindowsManager.DetachWindow(hwndDlg);
            if (dlg->IsAllocated())
                delete dlg;
            else
                dlg->HWindow = NULL; // information about disconnection
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
    //--- calling the method DialogProc(...) of the related object of the dialog
    if (dlg != NULL)
        return dlg->DialogProc(uMsg, wParam, lParam);
    else
        return FALSE; // the error or the message did not come between WM_INITDIALOG and WM_DESTROY
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
        TRACE_E("Some window is still opened in " << QueueName << " queue!"); // should not happen...
    // multi-threadiness is not a problem here (plugin is ending, threads are/will be terminated)
    // we will deallocate at least some memory
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
        if (item->HWindow == hWindow) // found, to be removed
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
    // sending request to close all windows
    BroadcastMessage(WM_CLOSE, 0, 0);

    // waiting until/if all windows are closed
    DWORD ti = GetTickCount();
    DWORD w = force ? forceWaitTime : waitTime;
    while ((w == INFINITE || w > 0) && !Empty())
    {
        DWORD t = GetTickCount() - ti;
        if (w == INFINITE || t < w) // should we wait more?
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
        return FALSE; // not worth processing another
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
        if (wnd == NULL) // focus only if ctrl is not a child of GetFocus()
        {                // like e.g. edit-line in combo-box
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
                s++;        // skipping sign
            while (*s != 0) // conversion of comma to dot
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
                            s++; // skipping +- after exponent
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
                value = atof(buff); // only if it is a number
            else
                value = 0; // on error we will give zero
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
                s++;        // skipping sign
            while (*s != 0) // checking the number
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
            value = strtoul(buff, &endptr, 10); // replacement for atoi / _ttoi, which returns 2147483647 instead of 4000000000 (because it is SIGNED INT)
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
