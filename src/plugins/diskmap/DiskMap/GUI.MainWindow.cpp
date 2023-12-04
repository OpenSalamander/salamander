// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "GUI.MainWindow.h"

CZLocalizer* CMainWindow::s_localizer = NULL;

LRESULT CMainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (this->_shellmenu && this->_shellmenu->HandleMenuMessage(message, wParam, lParam))
    {
        return 0;
    }

    switch (message)
    {
        /*case WM_MOUSEWHEEL:
		InvalidateRect(this->_hWnd, NULL, TRUE); 
		return 0;*/
        /*
/* void Cls_OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL fMinimized) * /
#define HANDLE_WM_ACTIVATE(hwnd, wParam, lParam, fn) \
	((fn)((hwnd), (UINT)LOWORD(wParam), (HWND)(lParam), (BOOL)HIWORD(wParam)), 0L)
#define FORWARD_WM_ACTIVATE(hwnd, state, hwndActDeact, fMinimized, fn) \
	(void)(fn)((hwnd), WM_ACTIVATE, MAKEWPARAM((state), (fMinimized)), (LPARAM)(HWND)(hwndActDeact))
*/

    case WM_SYSCOLORCHANGE:
        return this->OnSysColorChange(), 0;
    case WM_SETTINGCHANGE:
        return this->OnSystemChange(), 0;

    case WM_ACTIVATE:
        return this->OnActivate((UINT)LOWORD(wParam), (HWND)(lParam), (BOOL)HIWORD(wParam)), 0;

    case WM_CONTEXTMENU:
        return this->OnContextMenu((HWND)wParam, (short)LOWORD(lParam), (short)HIWORD(lParam)), 0;

    case WM_KEYDOWN:
        return this->OnKey((UINT)(wParam), TRUE, (int)(short)LOWORD(lParam), (UINT)HIWORD(lParam))
                   ? 0
                   : DefWindowProc(hWnd, message, wParam, lParam);

    case WM_INITMENU:
        return this->OnInitMenu((HMENU)(wParam)), 0;
    case WM_COMMAND:
        return this->OnCommand((int)LOWORD(wParam), (HWND)lParam, (UINT)HIWORD(wParam))
                   ? 0
                   : DefWindowProc(hWnd, message, wParam, lParam);

    case WM_ENTERSIZEMOVE:
        return this->OnEnterSizeMove(), 0;
    case WM_EXITSIZEMOVE:
        return this->OnExitSizeMove(), 0;
    case WM_SIZE:
        return this->OnSize(wParam, (short)LOWORD(lParam), (short)HIWORD(lParam)), 0;
    case WM_GETMINMAXINFO:
        return this->OnGetMinMaxInfo((LPMINMAXINFO)(lParam)), 0;

    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT:
        return this->OnPaint(), 0;

    case WM_CREATE:
        return this->OnCreate((LPCREATESTRUCT)lParam) ? 0 : (LRESULT)-1L;
    case WM_DESTROY:
        return this->OnDestroy(), 0;
    case WM_APPCOMMAND:
        return this->OnAppCommand((HWND)wParam, (short)GET_APPCOMMAND_LPARAM(lParam), (WORD)GET_DEVICE_LPARAM(lParam), (DWORD)GET_KEYSTATE_LPARAM(lParam))
                   ? TRUE
                   : DefWindowProc(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
