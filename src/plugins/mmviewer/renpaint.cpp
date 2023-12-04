// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mmviewer.rh"
#include "mmviewer.rh2"
#include "lang\lang.rh"
#include "output.h"
#include "renderer.h"
#include "mmviewer.h"

#ifdef RESPECT_WINDOWS_COLORS
// Background color
static COLORREF rgbBC = GetSysColor(COLOR_HIGHLIGHT);
static COLORREF rgbHeaderBC = GetSysColor(COLOR_ACTIVECAPTION);
// Text color
static COLORREF rgbTC = GetSysColor(COLOR_HIGHLIGHTTEXT);
static COLORREF rgbHeaderTC = GetSysColor(COLOR_CAPTIONTEXT);
#else
// Background color
static COLORREF rgbBC = RGB(0xc8, 0xdf, 0xf3);
static COLORREF rgbHeaderBC = RGB(0xec, 0xdf, 0xd3);
// Text color
static COLORREF rgbTC = RGB(0x0, 0x0, 0x0);
static COLORREF rgbHeaderTC = RGB(0x0, 0x0, 0x0);
#endif

//****************************************************************************
//
// CRendererWindow
//

int CRendererWindow::ComputeExtents(HDC hDC, SIZE& s, BOOL value, BOOL computeHeaderWidth)
{
    s.cx = 0;
    s.cy = 0;
    int headerWidth = 0;

    int i;
    for (i = 0; i < Output.GetCount(); i++)
    {
        const COutputItem* item = Output.GetItem(i);

        RECT r = {0, 0, 4096, 4096};

        HFONT hOldFont = (HFONT)GetCurrentObject(hDC, OBJ_FONT);

        if (item->Name && item->Value) //neni-li to header
        {
            const char* str = value ? item->Value : item->Name;
            SelectObject(hDC, value ? HBoldFont : HNormalFont);
            DrawText(hDC, str, (int)strlen(str), &r, DT_CALCRECT | DT_SINGLELINE | DT_WORDBREAK | DT_LEFT);

            if (s.cx < r.right)
                s.cx = r.right;
            /*
      if (s.cy < r.bottom)
        s.cy = r.bottom;
*/
        }
        else if (item->Name && computeHeaderWidth) //spocti pozadovanou sirku headeru (nekdy muze byt sirsi nez name+value dohromady)
        {
            SelectObject(hDC, HBoldFont);
            DrawText(hDC, item->Name, (int)strlen(item->Name), &r, DT_CALCRECT | DT_SINGLELINE | DT_LEFT);

            if (headerWidth < r.right)
                headerWidth = r.right;
        }

        SelectObject(hDC, hOldFont);
    }

    s.cy = (FontHeight + 1) * Output.GetCount();

    //udelej trochu mista okolo
    s.cx += 15;
    s.cy += 15;

    if (s.cx > 4096) //edit boxy si s velkou sirkou nevi rady
        s.cx = 4096;

    return headerWidth + 15;
}

void CRendererWindow::Paint(HDC hDC, BOOL moveEditBoxes, DWORD deferFlg)
{
    if (Output.GetCount() == 0)
        return;

    HFONT hOldFont;
    int oldBkMode;
    HDWP hdwp;

    if (!moveEditBoxes)
    {
        hOldFont = (HFONT)GetCurrentObject(hDC, OBJ_FONT);
        oldBkMode = SetBkMode(hDC, TRANSPARENT);
    }
    else
        hdwp = BeginDeferWindowPos(20);

    RECT r;
    GetClientRect(HWindow, &r);

    if ((r.left + r.right + r.top + r.bottom) == 0)
        return;

    int startH = (r.right - r.left) / 2 - width / 2;
    int startV = (r.bottom - r.top) / 2 - height / 2;

    if (startH < 0)
        startH = 5;
    if (startV < 0)
        startV = 5;

    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    if (!GetScrollInfo(HWindow, SB_VERT, &si)) //0 = no scrollbar
        si.nPos = 0;

    int yPos = si.nPos;
    if (!GetScrollInfo(HWindow, SB_HORZ, &si))
        si.nPos = 0;
    int xPos = si.nPos;

    startH -= xPos;
    startV -= yPos;

    int y = startV;

    int i;
    for (i = 0; i < Output.GetCount(); i++)
    {
        const COutputItem* item = Output.GetItem(i);

        if ((item->Flags & OIF_SEPARATOR) == 0)
        {
            if (!moveEditBoxes)
            {
                RECT rct = {startH, y, ((item->Flags & OIF_HEADER) == 0) ? startH + sLeft.cx : startH + width, y + FontHeight};

                SetTextColor(hDC, ((item->Flags & OIF_HEADER) == 0) ? rgbTC : rgbHeaderTC);
                SetBkColor(hDC, ((item->Flags & OIF_HEADER) == 0) ? rgbBC : rgbHeaderBC);
                SelectObject(hDC, ((item->Flags & OIF_HEADER) == 0) ? HNormalFont : HBoldFont);
                ExtTextOut(hDC, startH + 5, y + 1, ETO_CLIPPED | ETO_OPAQUE, &rct, item->Name, lstrlen(item->Name), NULL);

                ExcludeClipRect(hDC, rct.left, rct.top, rct.right, rct.bottom);
            }
            else
            {
                if ((item->Flags & OIF_HEADER) == 0)
                {
                    //assert(IsWindow(item->hwnd));

                    if (deferFlg == 0xFFFFFFFF)
                    {
                        LRESULT lines = SendMessage(item->hwnd, (UINT)EM_GETLINECOUNT, 0, 0);

                        if (lines > 1) //pridej scrollbarek ;-)
                        {
                            LONG style = GetWindowLong(item->hwnd, GWL_STYLE);
                            SetWindowLong(item->hwnd, GWL_STYLE, style | WS_VSCROLL);
                            //Nyni je treba takovyhle brutalni refresh
                            SetWindowPos(item->hwnd, HWND_TOP, startH + sLeft.cx, y, sRight.cx, FontHeight,
                                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOZORDER | SWP_NOSIZE);
                        }
                    }
                    else
                        hdwp = DeferWindowPos(hdwp, item->hwnd, HWND_TOP, startH + sLeft.cx, y, sRight.cx, FontHeight, SWP_NOZORDER | deferFlg);
                }
            }
        }

        y += FontHeight + 1;
    }

    if (!moveEditBoxes)
    {
        SetBkColor(hDC, GetSysColor(COLOR_WINDOW));
        ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);

        SelectObject(hDC, hOldFont);
        SetBkMode(hDC, oldBkMode);
    }
    else
        EndDeferWindowPos(hdwp);
}
