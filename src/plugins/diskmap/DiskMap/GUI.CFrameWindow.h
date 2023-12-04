// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CWindow.h"

class CFrameWindow : public CWindow
{
protected:
    virtual HWND DoCreate(int left, int top, int width, int height, BOOL isTopmost) = 0;

public:
    CFrameWindow(HWND hWndParent) : CWindow(hWndParent)
    {
    }
    virtual ~CFrameWindow()
    {
    }

    virtual HWND Create(BOOL isTopmost)
    {
        return this->Create(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, isTopmost);
    }

    virtual HWND Create(int left, int top, int width, int height, BOOL isTopmost)
    {
        if (this->_hWnd)
            return this->_hWnd;

        return /*this->_hWnd = */ this->DoCreate(left, top, width, height, isTopmost); // vytvori hned WM_NCCREATE, ktery naplni _hWnd
    }

    virtual BOOL IsTopmost()
    {
        return (GetWindowLongPtr(this->_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    }
};
