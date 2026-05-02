// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CAboutDialog
{
protected:
    // Message handler for about box.
    static INT_PTR CALLBACK s_About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER(lParam);
        switch (message)
        {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
        }
        return (INT_PTR)FALSE;
    }

public:
#ifndef SALAMANDER
    static void ShowModal(HINSTANCE hInstance, HWND hWndParent)
    {
        //DialogBox(this->_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), this->_hWnd, About);
        DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWndParent, CAboutDialog::s_About);
    }
#endif // SALAMANDER

private:
    CAboutDialog() {}
};
