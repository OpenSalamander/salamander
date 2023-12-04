// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "GUI.CWindow.h"

#define NO_MARGIN -1

class CChildWindow : public CWindow
{
protected:
    int _x;
    int _y;
    int _width;
    int _height;

    int _marginLeft;
    int _marginTop;
    int _marginRight;
    int _marginBottom;

    virtual HWND DoCreate(int left, int top, int width, int height) = 0;

public:
    CChildWindow(HWND hWndParent) : CWindow(hWndParent)
    {
        _x = 0;
        _y = 0;
        _width = 0;
        _height = 0;

        _marginLeft = -1;
        _marginTop = -1;
        _marginRight = -1;
        _marginBottom = -1;
    }
    virtual ~CChildWindow()
    {
    }
    int GetX() const { return this->_x; }
    int GetY() const { return this->_y; }
    int GetHeight() const { return this->_height; }
    int GetWidth() const { return this->_width; }

    BOOL SetMargins(int left, int top, int right, int bottom)
    {
        this->_marginLeft = left;
        this->_marginTop = top;
        this->_marginRight = right;
        this->_marginBottom = bottom;
        return this->UpdateSize();
    }

    virtual BOOL UpdateSize()
    {
        RECT rct;
        GetClientRect(this->_hWndParent, &rct);
        if (this->_marginLeft >= 0)
        {
            this->_x = rct.left + this->_marginLeft;
        }
        if (this->_marginTop >= 0)
        {
            this->_y = rct.top + this->_marginTop;
        }
        if (this->_marginRight >= 0)
        {
            int right = rct.right - this->_marginRight;
            this->_width = right - this->_x;
        }
        if (this->_marginBottom >= 0)
        {
            int bottom = rct.bottom - this->_marginBottom;
            this->_height = bottom - this->_y;
        }
        if (this->_hWnd != NULL)
        {
            return MoveWindow(this->_hWnd, this->_x, this->_y, this->_width, this->_height, TRUE);
        }
        return TRUE;
    }
    /*virtual BOOL UpdateSize()
	{
		RECT rct;
		GetClientRect(this->_hWndParent, &rct);
		this->_width = rct.right - rct.left;
		return MoveWindow(this->_hWnd, this->_x, this->_y, this->_width, this->_height, FALSE);
	}*/

    virtual HWND Create(int left, int top, int width, int height)
    {
        if (this->_hWnd)
            return this->_hWnd;

        return /*this->_hWnd = */ this->DoCreate(left, top, width, height); // vytvori hned WM_NCCREATE, ktery naplni _hWnd
    }
};
