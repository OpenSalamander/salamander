// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// opatreni proti runtime check failure v debug verzi: puvodni verze makra pretypovava rgb na WORD,
// takze hlasi ztratu dat (RED slozky)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

class CZColors
{
public:
    static COLORREF Mix(COLORREF c1, COLORREF c2)
    {
        byte r1 = GetRValue(c1);
        byte g1 = GetGValue(c1);
        byte b1 = GetBValue(c1);

        byte r2 = GetRValue(c2);
        byte g2 = GetGValue(c2);
        byte b2 = GetBValue(c2);

        return RGB((r1 >> 1) + (r2 >> 1), (g1 >> 1) + (g2 >> 1), (b1 >> 1) + (b2 >> 1));
    }
    static COLORREF Mix34(COLORREF c1, COLORREF c2)
    {
        byte r1 = GetRValue(c1);
        byte g1 = GetGValue(c1);
        byte b1 = GetBValue(c1);

        byte r2 = GetRValue(c2);
        byte g2 = GetGValue(c2);
        byte b2 = GetBValue(c2);

        int r = (r1 >> 1) + (r1 >> 2) + (r2 >> 2);
        int g = (g1 >> 1) + (g1 >> 2) + (g2 >> 2);
        int b = (b1 >> 1) + (b1 >> 2) + (b2 >> 2);

        return RGB(r, g, b);
    }
    static COLORREF Mix58(COLORREF c1, COLORREF c2)
    {
        byte r1 = GetRValue(c1);
        byte g1 = GetGValue(c1);
        byte b1 = GetBValue(c1);

        byte r2 = GetRValue(c2);
        byte g2 = GetGValue(c2);
        byte b2 = GetBValue(c2);

        int r = (r1 >> 1) + (r1 >> 3) + (r2 >> 2) + (r2 >> 3);
        int g = (g1 >> 1) + (g1 >> 3) + (g2 >> 2) + (g2 >> 3);
        int b = (b1 >> 1) + (b1 >> 3) + (b2 >> 2) + (b2 >> 3);

        return RGB(r, g, b);
    }
    static COLORREF Dim(COLORREF c1)
    {
        byte r1 = GetRValue(c1);
        byte g1 = GetGValue(c1);
        byte b1 = GetBValue(c1);

        int r = (r1 >> 1) + (r1 >> 2);
        int g = (g1 >> 1) + (g1 >> 2);
        int b = (b1 >> 1) + (b1 >> 2);
        return RGB(r, g, b);
    }
    static COLORREF Grey(COLORREF c1)
    {
        byte r1 = GetRValue(c1);
        byte g1 = GetGValue(c1);
        byte b1 = GetBValue(c1);
        //Y' =     + 0.299    * R' + 0.587    * G' + 0.114    * B'
        //76*r
        //150*g
        //29*b
        //int brightness = (55*(int)red + 183*(int)green + 19*(int)blue) / 255;
        //if (brightness > 255) brightness = 255;
        //return (BYTE) brightness;
        int g = (76 * r1 + 150 * g1 + 29 * b1) / 255;
        return RGB(g, g, g);
    }
};
