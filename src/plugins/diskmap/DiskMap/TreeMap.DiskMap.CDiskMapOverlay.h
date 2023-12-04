// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#include "Utils.CPixMap.h"
//#include "Utils.CZBitmap.h"

class CDiskMapOverlay
{
protected:
public:
    CDiskMapOverlay() {}
    virtual ~CDiskMapOverlay() {}

    virtual BOOL Paint(CZBitmap* pix, HDC refDC) = 0;
};

class CSelectedCushionOverlay : public CDiskMapOverlay
{
protected:
    COLORREF _color;
    CCushionGraphics* _graphics;

    int _x;
    int _y;
    int _w;
    int _h;

    BOOL _selected;

public:
    CSelectedCushionOverlay(COLORREF color, CCushionGraphics* graphics)
    {
        this->_color = color;
        this->_graphics = graphics;
        this->_selected = FALSE;
    }

    BOOL Paint(CZBitmap* pix, HDC refDC)
    {
        if (this->_selected == FALSE)
            return TRUE; //ok, ale nic nekreslim

        int width = pix->GetWidth();
        int height = pix->GetHeight();

        BYTE* dta = pix->LockBits();

        if (dta != NULL)
        {
            this->_graphics->DrawCushion(dta, width, height, this->_x, this->_y, this->_w, this->_h, this->_color);

            pix->UnlockBits();
        }
        return TRUE;
    }

    void SetGraphics(CCushionGraphics* graphics)
    {
        this->_graphics = graphics;
    }

    void SetCushion(int x, int y, int w, int h)
    {
        this->_selected = TRUE;
        this->_x = x;
        this->_y = y;
        this->_w = w;
        this->_h = h;
    }

    void ClearCushion()
    {
        this->_selected = FALSE;
    }
};
