// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "gdi.h"

// ****************************************************************************
//
// CBackbufferedDC -- DC s back bufferem, pro hladke kresleni slozitejsich
// grafickych celku
//

CBackbufferedDC::CBackbufferedDC()
{
    CALL_STACK_MESSAGE_NONE
    HWindow = NULL;
    HBitmap = NULL;
    DC = NULL;
}

CBackbufferedDC::CBackbufferedDC(HWND window)
{
    CALL_STACK_MESSAGE_NONE
    HWindow = NULL;
    HBitmap = NULL;
    DC = NULL;
    SetWindow(window);
}

CBackbufferedDC::~CBackbufferedDC()
{
    CALL_STACK_MESSAGE_NONE
    Destroy();
}

void CBackbufferedDC::Destroy()
{
    CALL_STACK_MESSAGE_NONE
    if (HBitmap)
        DeleteObject(HBitmap);
    HWindow = NULL;
    HBitmap = NULL;
    DC = NULL;
}

void CBackbufferedDC::SetWindow(HWND window)
{
    CALL_STACK_MESSAGE_NONE
    HWindow = window;
    Update();
}

void CBackbufferedDC::Update()
{
    CALL_STACK_MESSAGE_NONE
    if (!HWindow)
        return;
    GetClientRect(HWindow, &ClientRect);
    HDC dc = GetDC(HWindow);
    if (dc != NULL)
    {
        if (HBitmap)
            DeleteObject(HBitmap);
        HBitmap = CreateCompatibleBitmap(dc, ClientRect.right, ClientRect.bottom);
        ReleaseDC(HWindow, dc);
    }
}

void CBackbufferedDC::BeginPaint()
{
    CALL_STACK_MESSAGE_NONE
    if (!HWindow)
        return;

    ::BeginPaint(HWindow, &PS);
    if (HBitmap)
    {
        DC = CreateCompatibleDC(PS.hdc);
        if (DC)
            OldHBitmap = (HBITMAP)SelectObject(DC, HBitmap);
    }
}

void CBackbufferedDC::EndPaint()
{
    CALL_STACK_MESSAGE_NONE
    if (!HWindow)
        return;

    if (DC)
    {
        BitBlt(PS.hdc, 0, 0, ClientRect.right, ClientRect.bottom, DC, 0, 0,
               SRCCOPY);
        SelectObject(DC, OldHBitmap);
        DeleteDC(DC);
        DC = NULL;
    }
    ::EndPaint(HWindow, &PS);
}

CBackbufferedDC::operator HDC()
{
    CALL_STACK_MESSAGE_NONE
    return DC == NULL ? PS.hdc : DC;
}
