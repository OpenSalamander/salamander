// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class CWindow
{
private:
    LRESULT MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        return this->WndProc(hWnd, message, wParam, lParam);
    }

protected:
    static HINSTANCE s_hInstance;    //HINSTANCE for code
    static HINSTANCE s_hResInstance; //HINSTANCE for resources

    HWND _hWnd;
    HWND _hWndParent;

    HWND MyCreateWindow(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HMENU hMenu)
    {
        return CreateWindowEx(
            dwExStyle,
            lpClassName,
            lpWindowName,
            dwStyle,
            x, y,
            nWidth, nHeight,
            this->_hWndParent,
            hMenu,
            CWindow::s_hInstance,
            this);
    }
    static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CWindow* pWindow;

        //set object pointer to GWLP_USERDATA
        if (uMsg == WM_NCCREATE)
        {
            pWindow = (CWindow*)((LPCREATESTRUCT)lParam)->lpCreateParams;
            pWindow->_hWnd = hwnd;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pWindow);
        }
        else
        {
            pWindow = (CWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }
        LRESULT res = pWindow ? pWindow->MyWndProc(hwnd, uMsg, wParam, lParam) : DefWindowProc(hwnd, uMsg, wParam, lParam);

        if (pWindow != NULL && uMsg == WM_NCDESTROY)
        {
            pWindow->_hWnd = 0;
        }
        return res;
    }

    virtual void OnPaint()
    {
        PAINTSTRUCT ps;
        if (BeginPaint(this->_hWnd, &ps))
        {
            this->DoPaint(&ps);
            EndPaint(this->_hWnd, &ps);
        }
    }

    virtual void DoPaint(PAINTSTRUCT* pps) = 0;

    virtual LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;

public:
    CWindow(HWND hWndParent)
    {
        this->_hWnd = NULL;
        this->_hWndParent = hWndParent;
    }
    virtual ~CWindow()
    {
    }
    static void SetHInstance(HINSTANCE hInstance, HINSTANCE hResInstance)
    {
        CWindow::s_hInstance = hInstance;
        CWindow::s_hResInstance = hResInstance;
    }

    HWND GetHandle() { return this->_hWnd; }

    virtual BOOL Destroy()
    {
        if (this->_hWnd)
            return DestroyWindow(this->_hWnd);
        return FALSE;
    }
    virtual BOOL Show(int nCmdShow)
    {
        return ShowWindow(this->_hWnd, nCmdShow) && UpdateWindow(this->_hWnd);
    }
    virtual BOOL Hide()
    {
        return ShowWindow(this->_hWnd, SW_HIDE);
    }
    virtual BOOL IsVisible()
    {
        return IsWindowVisible(this->_hWnd);
    }
    virtual void Repaint(BOOL bErase = FALSE) { InvalidateRect(this->_hWnd, NULL, bErase); }
};
