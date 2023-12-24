// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <limits.h>
#include <commctrl.h> // LPCOLORMAP needed
#include <tchar.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"
#include "messages.h"
#include "handles.h"

#include "array.h"

#include "winlib.h"

// precaution against runtime check failure in debug version: original version of macro casts rgb to WORD,
// so it reports data loss (RED component)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

const TCHAR* CWINDOW_CLASSNAME = _T("WinLib Universal Window");
const TCHAR* CWINDOW_CLASSNAME2 = _T("WinLib Universal Window2"); // doesn't have CS_VREDRAW | CS_HREDRAW

#ifndef _UNICODE
const WCHAR* CWINDOW_CLASSNAMEW = L"WinLib Universal Window Unicode";
const WCHAR* CWINDOW_CLASSNAME2W = L"WinLib Universal Window Unicode2"; // doesn't have CS_VREDRAW | CS_HREDRAW
#endif                                                                  // _UNICODE

CWinLibHelp* WinLibHelp = NULL;
CWindowsManager WindowsManager;
HINSTANCE HInstance = NULL;
BOOL WinLibReleased = FALSE; // TRUE = ReleaseWinLib() was called

TCHAR WinLibStrings[WLS_COUNT][101] = {
    _T("Invalid number!"),
    _T("Error")};

//
// ****************************************************************************

void SetWinLibStrings(const TCHAR* invalidNumber, const TCHAR* error)
{
    lstrcpyn(WinLibStrings[WLS_INVALID_NUMBER], invalidNumber, 101);
    lstrcpyn(WinLibStrings[WLS_ERROR], error, 101);
}

BOOL InitializeWinLib()
{
    InitCommonControls();

    if (!CWindow::RegisterUniversalClass())
    {
        TRACE_CT(_T("Unable to register universal window class."));
        return FALSE;
    }
    WinLibReleased = FALSE;
    return TRUE;
}

void ReleaseWinLib()
{
    // the opened windows must be disconnected from WinLib because WinLib ends ...
    int c = WindowsManager.GetCount();
    if (c > 0)
        TRACE_ET(_T("ReleaseWinLib(): WindowsManager still contains opened windows: ") << c);
    WinLibReleased = TRUE;
}

BOOL SetupWinLibHelp(CWinLibHelp* winLibHelp)
{
    WinLibHelp = winLibHelp;
    return TRUE;
}

// ****************************************************************************
// WinLibIsWindowsVersionOrGreater (kopie SalIsWindowsVersionOrGreater)
//
// Based on SDK 8.1 VersionHelpers.h
// Indicates if the current OS version matches, or is greater than, the provided
// version information. This function is useful in confirming a version of Windows
// Server that doesn't share a version number with a client release.
// http://msdn.microsoft.com/en-us/library/windows/desktop/dn424964%28v=vs.85%29.aspx
//

BOOL WinLibIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // replacement for memset (does not require RTL)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

//
// ****************************************************************************
// CWindow
//
// lpvParam - in case of CreateWindow calling CWindow::CWindowProc (it is in window class),
//            it must contain address of object of created window

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
                       LPVOID lpvParam)        // pointer to object of created window
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
            AttachToWindow(hWnd);                      // then add it -> subclassing
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
                     LPVOID lpvParam)        // pointer to object of created window
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

#ifndef _UNICODE

HWND CWindow::CreateExW(DWORD dwExStyle,        // extended window style
                        LPCWSTR lpszClassName,  // address of registered class name
                        LPCWSTR lpszWindowName, // address of window name
                        DWORD dwStyle,          // window style
                        int x,                  // horizontal position of window
                        int y,                  // vertical position of window
                        int nWidth,             // window width
                        int nHeight,            // window height
                        HWND hwndParent,        // handle of parent or owner window
                        HMENU hmenu,            // handle of menu or child-window identifier
                        HINSTANCE hinst,        // handle of application instance
                        LPVOID lpvParam)        // pointer to object of created window
{
    HWND hWnd = CreateWindowExW(dwExStyle,
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
            AttachToWindow(hWnd);                      // then add it -> subclassing
    }
    return hWnd;
}

HWND CWindow::CreateW(LPCWSTR lpszClassName,  // address of registered class name
                      LPCWSTR lpszWindowName, // address of window name
                      DWORD dwStyle,          // window style
                      int x,                  // horizontal position of window
                      int y,                  // vertical position of window
                      int nWidth,             // window width
                      int nHeight,            // window height
                      HWND hwndParent,        // handle of parent or owner window
                      HMENU hmenu,            // handle of menu or child-window identifier
                      HINSTANCE hinst,        // handle of application instance
                      LPVOID lpvParam)        // pointer to object of created window
{
    return CreateExW(0,
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

#endif // _UNICODE

void CWindow::AttachToWindow(HWND hWnd)
{
#ifdef _UNICODE
    DefWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
#else  // _UNICODE
    if (UnicodeWnd)
        DefWndProc = (WNDPROC)GetWindowLongPtrW(hWnd, GWLP_WNDPROC);
    else
        DefWndProc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
#endif // _UNICODE
    if (DefWndProc == NULL)
    {
        TRACE_ET(_T("Invalid handle of window. hWnd = ") << hWnd);
        DefWndProc = GetDefWindowProc();
        return;
    }
    if (!WindowsManager.AddWindow(hWnd, this))
    {
        TRACE_ET(_T("Error in connecting object to window."));
        DefWndProc = GetDefWindowProc();
        return;
    }
    HWindow = hWnd;
#ifdef _UNICODE
    SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)CWindowProc);
#else  // _UNICODE
    if (UnicodeWnd)
        SetWindowLongPtrW(HWindow, GWLP_WNDPROC, (LONG_PTR)CWindowProcW);
    else
        SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)CWindowProc);
#endif // _UNICODE

    if (DefWndProc == CWindow::CWindowProc
#ifndef _UNICODE
        || DefWndProc == CWindow::CWindowProcW
#endif    // _UNICODE
        ) // this would be recursion
    {
        TRACE_CT(_T("This should never happen."));
        DefWndProc = GetDefWindowProc();
    }
}

void CWindow::AttachToControl(HWND dlg, int ctrlID)
{
    if (dlg == NULL)
    {
        TRACE_ET(_T("Incorrect call to CWindow::AttachToControl."));
        return;
    }
    HWND hwnd = GetDlgItem(dlg, ctrlID);
    if (hwnd == NULL)
        TRACE_ET(_T("Control with ctrlID = ") << ctrlID << _T(" is not in dialog."));
    else
        AttachToWindow(hwnd);
}

void CWindow::DetachWindow()
{
    if (HWindow != NULL)
    {
        WindowsManager.DetachWindow(HWindow);
#ifdef _UNICODE
        SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)DefWndProc);
#else  // _UNICODE
        if (UnicodeWnd)
            SetWindowLongPtrW(HWindow, GWLP_WNDPROC, (LONG_PTR)DefWndProc);
        else
            SetWindowLongPtr(HWindow, GWLP_WNDPROC, (LONG_PTR)DefWndProc);
#endif // _UNICODE
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
        if (WinLibHelp != NULL && HelpID != -1)
        {
            WinLibHelp->OnHelp(HWindow, HelpID, (HELPINFO*)lParam,
                               (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                               (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return TRUE;
        }
        if (GetWindowLongPtr(HWindow, GWL_STYLE) & WS_CHILD)
            break;   // if F1 was not handled and it is child window, let F1 go to parent
        return TRUE; // if it is not child, let's finish processing F1
    }
    }
#ifndef _UNICODE
    if (UnicodeWnd)
        return CallWindowProcW((WNDPROC)DefWndProc, HWindow, uMsg, wParam, lParam);
#endif // _UNICODE
    return CallWindowProc((WNDPROC)DefWndProc, HWindow, uMsg, wParam, lParam);
}

#ifndef _UNICODE
LRESULT CALLBACK
CWindow::CWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CWindowProcInt(hwnd, uMsg, wParam, lParam, FALSE /*unicode*/);
}

LRESULT CALLBACK
CWindow::CWindowProcW(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return CWindowProcInt(hwnd, uMsg, wParam, lParam, TRUE /*unicode*/);
}
#endif // _UNICODE

LRESULT CALLBACK
#ifdef _UNICODE
CWindow::CWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
#else  // _UNICODE
CWindow::CWindowProcInt(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL unicode)
#endif // _UNICODE
{
    CWindow* wnd;
    switch (uMsg)
    {
    case WM_CREATE: // the first message - attaching object to window
    {
        // handling MDI_CHILD_WINDOW
        if (((CREATESTRUCT*)lParam)->dwExStyle & WS_EX_MDICHILD)                                 // CREATESTRUCTA and CREATESTRUCTW do not differ for dwExStyle and lpCreateParams
            wnd = (CWindow*)((MDICREATESTRUCT*)((CREATESTRUCT*)lParam)->lpCreateParams)->lParam; // MDICREATESTRUCTA and MDICREATESTRUCTW do not differ for lParam
        else
            wnd = (CWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        if (wnd == NULL)
        {
            TRACE_ET(_T("Unable to create window."));
            return FALSE;
        }
        else
        {
            wnd->HWindow = hwnd;
#ifndef _UNICODE
            if (wnd->UnicodeWnd != unicode)
                TRACE_C("Incompatible windows procedure.");
#endif                                                // _UNICODE
                                                      //--- adding window to the list of windows per hwnd
            if (!WindowsManager.AddWindow(hwnd, wnd)) // error
            {
                TRACE_ET(_T("Unable to create window."));
                return FALSE;
            }
        }
        break;
    }

    case WM_DESTROY: // the last meesage - detaching object from window
    {
        wnd = (CWindow*)WindowsManager.GetWindowPtr(hwnd);
#ifndef _UNICODE
        if (wnd != NULL && wnd->UnicodeWnd != unicode)
            TRACE_C("Incompatible windows procedure.");
#endif // _UNICODE
        if (wnd != NULL && wnd->Is(otWindow))
        {
            // Petr: I moved this down below wnd->WindowProc() so that during WM_DESTROY
            //       messages are still received (Lukas needed this)
            // WindowsManager.DetachWindow(hwnd);

            LRESULT res = wnd->WindowProc(uMsg, wParam, lParam);

            // and now into the old procedure again (because of subclassing)
            WindowsManager.DetachWindow(hwnd);

            // if the current WndProc is different than ours, we will not change it
            // because someone in the subclassing chain has already returned the original WndProc
#ifdef _UNICODE
            WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC);
            if (currentWndProc == CWindow::CWindowProc)
                SetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC, (LONG_PTR)wnd->DefWndProc);
#else _UNICODE
            if (wnd->UnicodeWnd) // if our window procedure is unicode, we must use "W" variants of API functions
            {
                WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtrW(wnd->HWindow, GWLP_WNDPROC);
                if (currentWndProc == CWindow::CWindowProcW || currentWndProc == CWindow::CWindowProc)
                    SetWindowLongPtrW(wnd->HWindow, GWLP_WNDPROC, (LONG_PTR)wnd->DefWndProc);
            }
            else
            {
                WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC);
                if (currentWndProc == CWindow::CWindowProc || currentWndProc == CWindow::CWindowProcW)
                    SetWindowLongPtr(wnd->HWindow, GWLP_WNDPROC, (LONG_PTR)wnd->DefWndProc);
            }
#endif // _UNICODE

            if (wnd->IsAllocated())
                delete wnd;
            else
                wnd->HWindow = NULL; // is not attached anymore
            if (res == 0)
                return 0; // application has processed it
            wnd = NULL;
        }
        break;
    }

    default:
    {
        wnd = (CWindow*)WindowsManager.GetWindowPtr(hwnd);
#ifdef __DEBUG_WINLIB
        if (wnd != NULL && !wnd->Is(otWindow))
        {
            TRACE_CT(_T("This should never happen."));
            wnd = NULL;
        }
#endif
#ifndef _UNICODE
        if (wnd != NULL && wnd->UnicodeWnd != unicode)
            TRACE_C("Incompatible windows procedure.");
#endif // _UNICODE
    }
    }
    //--- calling method WindowProc(...) of the object of window
    LRESULT lResult;
    if (wnd != NULL)
        lResult = wnd->WindowProc(uMsg, wParam, lParam);
    else // error or message came before WM_CREATE
    {
#ifndef _UNICODE
        lResult = unicode ? DefWindowProcW(hwnd, uMsg, wParam, lParam) : DefWindowProcA(hwnd, uMsg, wParam, lParam);
#else  // _UNICODE
        lResult = DefWindowProc(hwnd, uMsg, wParam, lParam);
#endif // _UNICODE
    }

    return lResult;
}

BOOL CWindow::RegisterUniversalClass()
{
    WNDCLASS CWindowClass;
    CWindowClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    CWindowClass.lpfnWndProc = CWindow::CWindowProc;
    CWindowClass.cbClsExtra = 0;
    CWindowClass.cbWndExtra = 0;
    CWindowClass.hInstance = HInstance;
    CWindowClass.hIcon = HANDLES(LoadIcon(NULL, IDI_APPLICATION));
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

#ifndef _UNICODE
    if (ret)
    {
        WNDCLASSW CWindowClassW;
        CWindowClassW.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        CWindowClassW.lpfnWndProc = CWindow::CWindowProcW;
        CWindowClassW.cbClsExtra = 0;
        CWindowClassW.cbWndExtra = 0;
        CWindowClassW.hInstance = HInstance;
        CWindowClassW.hIcon = CWindowClass.hIcon;
        CWindowClassW.hCursor = CWindowClass.hCursor;
        CWindowClassW.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        CWindowClassW.lpszMenuName = NULL;
        CWindowClassW.lpszClassName = CWINDOW_CLASSNAMEW;

        BOOL ret = RegisterClassW(&CWindowClassW) != 0;
        if (ret)
        {
            CWindowClassW.style = CS_DBLCLKS;
            CWindowClassW.lpszClassName = CWINDOW_CLASSNAME2W;
            ret = RegisterClassW(&CWindowClassW) != 0;
        }
    }
#endif // _UNICODE

    return ret;
}

BOOL CWindow::RegisterUniversalClass(UINT style, int cbClsExtra, int cbWndExtra,
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
    windowClass.hInstance = HInstance;
    windowClass.hIcon = hIcon;
    windowClass.hCursor = hCursor;
    windowClass.hbrBackground = hbrBackground;
    windowClass.lpszMenuName = lpszMenuName;
    windowClass.lpszClassName = lpszClassName;
    windowClass.hIconSm = hIconSm;

    return RegisterClassEx(&windowClass) != 0;
}

#ifndef _UNICODE
BOOL CWindow::RegisterUniversalClassW(UINT style, int cbClsExtra, int cbWndExtra,
                                      HICON hIcon, HCURSOR hCursor, HBRUSH hbrBackground,
                                      LPCWSTR lpszMenuName, LPCWSTR lpszClassName,
                                      HICON hIconSm)
{
    WNDCLASSEXW windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = style;
    windowClass.lpfnWndProc = CWindow::CWindowProcW;
    windowClass.cbClsExtra = cbClsExtra;
    windowClass.cbWndExtra = cbWndExtra;
    windowClass.hInstance = HInstance;
    windowClass.hIcon = hIcon;
    windowClass.hCursor = hCursor;
    windowClass.hbrBackground = hbrBackground;
    windowClass.lpszMenuName = lpszMenuName;
    windowClass.lpszClassName = lpszClassName;
    windowClass.hIconSm = hIconSm;

    return RegisterClassExW(&windowClass) != 0;
}
#endif // _UNICODE

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
        TRACE_ET(_T("CDialog::TransferData(): This error should be detected in Validate() and not in Transfer() because already transferred data cannot be changed to their original values! It means that user cannot leave dialog box without changes using Cancel button now!"));
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
#ifndef _UNICODE
    if (UnicodeWnd)
    {
        return DialogBoxParamW(Modul, MAKEINTRESOURCEW(ResID), Parent,
                               (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
    }
#endif // _UNICODE
    return DialogBoxParam(Modul, MAKEINTRESOURCE(ResID), Parent,
                          (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
}

HWND CDialog::Create()
{
    Modal = FALSE;
#ifndef _UNICODE
    if (UnicodeWnd)
    {
        return CreateDialogParamW(Modul, MAKEINTRESOURCEW(ResID), Parent,
                                  (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
    }
#endif // _UNICODE
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
        return TRUE; // focus from DefDlgProc needed
    }

    case WM_HELP:
    {
        if (WinLibHelp != NULL && HelpID != -1)
        {
            WinLibHelp->OnHelp(HWindow, HelpID, (HELPINFO*)lParam,
                               (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                               (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        }
        return TRUE; // let's not F1 be passed to parent, even if we don't call WinLibHelp->OnHelp()
    }

    case WM_CONTEXTMENU:
    {
        if (WinLibHelp != NULL)
            WinLibHelp->OnContextMenu((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDHELP:
        {
            if (WinLibHelp != NULL && HelpID != -1)
            {
                HELPINFO hi;
                memset(&hi, 0, sizeof(hi));
                hi.cbSize = sizeof(hi);
                hi.iContextType = HELPINFO_WINDOW;
                hi.dwContextId = ResID; // in WM_HELP ResID is used instead of HelpID, so let it be consistent
                GetCursorPos(&hi.MousePos);
                WinLibHelp->OnHelp(HWindow, HelpID, &hi, FALSE, FALSE);
            }
            else
                TRACE_ET(_T("CDialog::DialogProc(): ignoring IDHELP: SetupWinLibHelp() was not called or HelpID is -1!"));
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
    case WM_INITDIALOG: // the first message - attaching object to dialog
    {
        dlg = (CDialog*)lParam;
        if (dlg == NULL)
        {
            TRACE_ET(_T("Unable to create dialog."));
            return TRUE;
        }
        else
        {
            dlg->HWindow = hwndDlg;
            //--- adding window to the list of windows per hwnd
            if (!WindowsManager.AddWindow(hwndDlg, dlg)) // error
            {
                TRACE_ET(_T("Unable to create dialog."));
                return TRUE;
            }
            dlg->NotifDlgJustCreated(); // loaded as a place for layout modification of the dialog
        }
        break;
    }

    case WM_DESTROY: // the last message - detaching object from dialog
    {
        dlg = (CDialog*)WindowsManager.GetWindowPtr(hwndDlg);
        INT_PTR ret = FALSE; // in case it is not processed
        if (dlg != NULL && dlg->Is(otDialog))
        {
            // Petr: I moved this down below dlg->DialogProc() so that during WM_DESTROY
            //       messages are still received (Lukas needed this)
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
        dlg = (CDialog*)WindowsManager.GetWindowPtr(hwndDlg);
#ifdef __DEBUG_WINLIB
        if (dlg != NULL && !dlg->Is(otDialog))
        {
            TRACE_CT(_T("This should never happen."));
            dlg = NULL;
        }
#endif
    }
    }
    //--- calling method DialogProc(...) of the object of dialog
    INT_PTR dlgRes;
    if (dlg != NULL)
        dlgRes = dlg->DialogProc(uMsg, wParam, lParam);
    else
        dlgRes = FALSE; // error or message did not come between WM_INITDIALOG and WM_DESTROY

    return dlgRes;
}

//
// ****************************************************************************
// CWindowsManager
//

CWindowsManager::CWindowsManager() : TDirectArray<CWindowData>(50, 50)
{
    memset(LastHWnd, 0, WNDMGR_CACHE_SIZE * sizeof(HWND));
    memset(LastWnd, 0, WNDMGR_CACHE_SIZE * sizeof(CWindowsObject*));
#ifdef __DEBUG_WINLIB
    search = 0;
    cache = 0;
    maxWndCount = 0;
#endif
}

BOOL CWindowsManager::AddWindow(HWND hWnd, CWindowsObject* wnd)
{
    if (WinLibReleased)
        return FALSE;

    CS.Enter();

    int i;
    if (GetIndex(hWnd, i))
    {
        CS.Leave();

        TRACE_ET(_T("Attempt to add window which is already contained in WindowsManager. hwnd = ") << hWnd);
        return FALSE;
    }
    else
    {
        CWindowData WindowData;
        WindowData.HWnd = hWnd;
        WindowData.Wnd = wnd;
        Insert(i, WindowData);
        if (!IsGood())
        {
            ResetState();
            TRACE_ET(_T("Unable to add window to WindowsManager. hwnd = ") << hWnd);
            CS.Leave();
            return FALSE;
        }
        else
        {
            LastHWnd[GetCacheIndex(hWnd)] = hWnd;
            LastWnd[GetCacheIndex(hWnd)] = wnd;
#ifdef __DEBUG_WINLIB
            if (maxWndCount < Count)
                maxWndCount = Count;
#endif
            CS.Leave();
            return TRUE;
        }
    }
}

void CWindowsManager::DetachWindow(HWND hWnd)
{
    if (WinLibReleased)
        return;

    CS.Enter();
    if (LastHWnd[GetCacheIndex(hWnd)] == hWnd) // cache must be cleared
    {
        LastHWnd[GetCacheIndex(hWnd)] = NULL;
        LastWnd[GetCacheIndex(hWnd)] = NULL;
    }

    int i;
    if (GetIndex(hWnd, i))
    {
        Delete(i);
        if (!IsGood())
        {
            ResetState();
            TRACE_ET(_T("Unable to detach window from WindowsManager. hwnd = ") << hWnd);
            At(i).Wnd = NULL; // at least this way ...
        }
    }
    else
        TRACE_ET(_T("Attempt to detach window which is not present in WindowsManager. hwnd = ") << hWnd);
    CS.Leave();
}

CWindowsObject*
CWindowsManager::GetWindowPtr(HWND hWnd)
{
    if (WinLibReleased)
        return NULL;

    CS.Enter();
    if (LastHWnd[GetCacheIndex(hWnd)] == hWnd)
    {
#ifdef __DEBUG_WINLIB
        cache++;
#endif
        CWindowsObject* ret = LastWnd[GetCacheIndex(hWnd)];
        CS.Leave();
        return ret;
    }
#ifdef __DEBUG_WINLIB
    search++;
#endif
    int i;
    if (GetIndex(hWnd, i)) // found
    {
        LastHWnd[GetCacheIndex(hWnd)] = hWnd;
        LastWnd[GetCacheIndex(hWnd)] = At(i).Wnd;
        CWindowsObject* ret = At(i).Wnd;
        CS.Leave();
        return ret;
    }
    else
    {
        CS.Leave();
        return NULL; // not found
    }
}

int CWindowsManager::GetCount()
{
    if (WinLibReleased)
        return 0;

    CS.Enter();
    int c = Count;
    CS.Leave();
    return c;
}

//
// ****************************************************************************
// CWindowQueue
//

CWindowQueue::~CWindowQueue()
{
    if (!Empty())
        TRACE_ET(_T("Some window is still opened in ") << QueueName << _T(" queue!")); // should not happen...
    // multithreadness is not a problem here (soft is ending, threads are/were terminated)
    // at least some memory should be deallocated
    CWindowQueueItem* last;
    CWindowQueueItem* item = Head;
    while (item != NULL)
    {
        last = item;
        item = item->Next;
        delete last;
    }
    Head = NULL;
    Count = 0;
}

BOOL CWindowQueue::Add(CWindowQueueItem* item)
{
    CS.Enter();
    if (item != NULL)
    {
        item->Next = Head;
        Head = item;
        Count++;
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
        if (item->HWindow == hWindow) // found, removing
        {
            if (last != NULL)
                last->Next = item->Next;
            else
                Head = item->Next;
            delete item;
            Count--;
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
    CS.Enter();
    BOOL e = Head == NULL;
    CS.Leave();
    return e;
}

int CWindowQueue::GetWindowCount()
{
    CS.Enter();
    int c = Count;
    CS.Leave();
    return c;
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

//
// ****************************************************************************
// CTransferInfo
//

BOOL CTransferInfo::GetControl(HWND& ctrlHWnd, int ctrlID, BOOL ignoreIsGood)
{
    if (!ignoreIsGood && !IsGood())
        return FALSE; // not worth to process further
    ctrlHWnd = GetDlgItem(HDialog, ctrlID);
    if (ctrlHWnd == NULL)
    {
        TRACE_ET(_T("Control with ctrlID = ") << ctrlID << _T(" is not in dialog."));
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
        {                // as e.g. edit-line in combo-box
            SendMessage(HDialog, WM_NEXTDLGCTL, (WPARAM)ctrl, TRUE);
        }
    }
    else
        TRACE_ET(_T("Control with ctrlID = ") << ctrlID << _T(" is not in dialog."));
}

void CTransferInfo::EditLine(int ctrlID, TCHAR* buffer, DWORD bufferSizeInChars, BOOL select)
{
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, bufferSizeInChars - 1, 0);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)buffer);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, bufferSizeInChars, (LPARAM)buffer);
            break;
        }
        }
    }
}

#ifndef _UNICODE
void CTransferInfo::EditLineW(int ctrlID, WCHAR* buffer, DWORD bufferSizeInChars, BOOL select)
{
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
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
#endif // _UNICODE

void CTransferInfo::EditLine(int ctrlID, double& value, TCHAR* format, BOOL select)
{
    HWND HWindow;
    TCHAR buff[31];
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, 30, 0);
            _stprintf_s(buff, format, value);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)buff);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, 31, (LPARAM)buff);
            TCHAR* s = buff;
            BOOL decPoints = FALSE;
            BOOL expPart = FALSE;
            if (*s == _T('-') || *s == _T('+'))
                s++;        // skipping sign
            while (*s != 0) // converting comma to dot
            {
                if (!expPart && !decPoints && (*s == _T(',') || *s == _T('.')))
                {
                    decPoints = TRUE;
                    *s = _T('.');
                }
                else
                {
                    if (!expPart && (*s == _T('e') || *s == _T('E') || *s == _T('d') || *s == _T('D')))
                    {
                        expPart = TRUE;
                        if (*(s + 1) == _T('+') || *(s + 1) == _T('-'))
                            s++; // skipping +- after exponent
                    }
                    else
                    {
                        if (*s < _T('0') || *s > _T('9'))
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
            {
                TCHAR* stopString;                  // dummy
                value = _tcstod(buff, &stopString); // only if it's a number
            }
            else
                value = 0; // on error we give zero
            break;
        }
        }
    }
}

void CTransferInfo::EditLine(int ctrlID, int& value, BOOL select)
{
    HWND HWindow;
    TCHAR buff[16];
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, 15, 0);
            _itot_s(value, buff, 10);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)buff);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, 16, (LPARAM)buff);

            TCHAR* s = buff;
            if (*s == _T('-') || *s == _T('+'))
                s++;        // skipping sign
            while (*s != 0) // checking the number
            {
                if (*s < _T('0') || *s > _T('9'))
                {
                    MessageBox(HWindow, WinLibStrings[WLS_INVALID_NUMBER], WinLibStrings[WLS_ERROR],
                               MB_OK | MB_ICONEXCLAMATION);
                    ErrorOn(ctrlID);
                    break;
                }
                s++;
            }

            TCHAR* endptr;
            value = _tcstoul(buff, &endptr, 10); // replacement for atoi / _ttoi, which returns 2147483647 instead of 4000000000 (because it's SIGNED INT)
            break;
        }
        }
    }
}

void CTransferInfo::EditLine(int ctrlID, __int64& value, BOOL select, BOOL unsignedNum, BOOL hexMode,
                             BOOL ignoreOverflow, BOOL quiet)
{
    if (!unsignedNum && hexMode)
        TRACE_CT(_T("CTransferInfo::EditLine(): unexpected combination of parameters (signed number and hex mode)."));
    HWND HWindow;
    TCHAR buff[26];
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, EM_LIMITTEXT, 25, 0);
            if (unsignedNum)
                _ui64tot_s(value, buff, _countof(buff), hexMode ? 16 : 10);
            else
                _i64tot_s(value, buff, _countof(buff), hexMode ? 16 : 10);
            CharUpper(buff);
            SendMessage(HWindow, WM_SETTEXT, 0, (LPARAM)buff);
            if (select)
                SendMessage(HWindow, EM_SETSEL, 0, -1);
            break;
        }

        case ttDataFromWindow:
        {
            SendMessage(HWindow, WM_GETTEXT, 26, (LPARAM)buff);
            CharUpper(buff);

            TCHAR* s = buff;
            BOOL minus = !unsignedNum && *s == _T('-');
            if (!unsignedNum && *s == _T('-') || *s == _T('+'))
                s++; // skipping sign
            unsigned __int64 num = 0;
            BOOL overflow = FALSE;
            while (*s != 0) // checking the number
            {
                if ((*s < _T('0') || *s > _T('9')) && (!hexMode || *s < _T('A') || *s > _T('F')))
                {
                    if (!quiet)
                    {
                        MessageBox(HWindow, WinLibStrings[WLS_INVALID_NUMBER], WinLibStrings[WLS_ERROR],
                                   MB_OK | MB_ICONEXCLAMATION);
                    }
                    ErrorOn(ctrlID);
                    break;
                }
                else
                {
                    if (hexMode && num > 0x0fffffffffffffffui64 ||
                        !hexMode && num > 1844674407370955161ui64 || // max. is 18446744073709551615ui64
                        !hexMode && num == 1844674407370955161ui64 && *s > _T('5'))
                    {
                        overflow = TRUE; // unsigned overflow
                    }
                    else
                        num = num * (hexMode ? 16 : 10) + (*s >= _T('0') && *s <= _T('9') ? *s - _T('0') : 10 + *s - _T('A'));
                }
                s++;
            }
            if (*s != 0)
            {
                value = 0; // zero when error
                break;
            }

            // when overflow, give boundary values (inspiration: value = _ttoi64(buff))
            if (unsignedNum)
            {
                if (overflow)
                    value = 0xffffffffffffffffui64 /* _UI64_MAX */;
                else
                    value = num;
            }
            else
            {
                if (minus)
                {
                    if (overflow || num > (unsigned __int64)(-(-9223372036854775807i64 - 1) /* -_I64_MIN */))
                    {
                        value = (-9223372036854775807i64 - 1) /* _I64_MIN */;
                        overflow = TRUE; // signed overflow
                    }
                    else
                        value = -(__int64)num;
                }
                else
                {
                    if (overflow || num > (unsigned __int64)9223372036854775807i64 /* _I64_MAX */)
                    {
                        value = 9223372036854775807i64 /* _I64_MAX */;
                        overflow = TRUE; // signed overflow
                    }
                    else
                        value = num;
                }
            }
            if (overflow)
            {
                if (ignoreOverflow) // announce overflow only through TRACE_E
                {
                    TRACE_ET(_T("CTransferInfo::EditLine(") << ctrlID << _T("): ") << (unsignedNum ? _T("unsigned ") : _T("")) << _T("int64 overflow has occured!"));
                }
                else
                {
                    if (!quiet)
                    {
                        MessageBox(HWindow, WinLibStrings[WLS_INVALID_NUMBER], WinLibStrings[WLS_ERROR],
                                   MB_OK | MB_ICONEXCLAMATION);
                    }
                    ErrorOn(ctrlID);
                }
            }
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

void CTransferInfo::TrackBar(int ctrlID, int& value)
{
    HWND HWindow;
    if (GetControl(HWindow, ctrlID))
    {
        switch (Type)
        {
        case ttDataToWindow:
        {
            SendMessage(HWindow, TBM_SETPOS, TRUE, value);
            break;
        }
        case ttDataFromWindow:
        {
            value = (int)SendMessage(HWindow, TBM_GETPOS, 0, 0);
            break;
        }
        }
    }
}
