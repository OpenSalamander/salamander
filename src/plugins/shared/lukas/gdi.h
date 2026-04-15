// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ****************************************************************************
//
// CBackbufferedDC -- DC s back bufferem, pro hladke kresleni slozitejsich
// grafickych celku
//

class CBackbufferedDC
{
public:
    CBackbufferedDC();
    CBackbufferedDC(HWND window);
    ~CBackbufferedDC();
    void Destroy();

    // Sets the window the DC is bound to
    void SetWindow(HWND window);

    // Updates internal data after changes in window size, screen
    // resolution, etc.; do not call between BeginPaint and EndPaint
    void Update();

    // Starts drawing into the window; _must_ be paired with EndPaint;
    // cannot be called repeatedly
    void BeginPaint();

    // Ends drawing and copies the back buffer contents to the screen
    void EndPaint();

    // DC for drawing into the window, valid only between BeginPaint and EndPaint
    operator HDC();

    // Returns a RECT with the buffer dimensions
    const RECT& GetRect() { return ClientRect; }

private:
    HDC DC;
    HWND HWindow;
    HBITMAP HBitmap;
    HBITMAP OldHBitmap;
    PAINTSTRUCT PS;
    RECT ClientRect;
};

inline BOOL FastFillRect(HDC hdc, const RECT& r)
{
    return ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, NULL, 0, 0);
}

inline BOOL FastFillRect(HDC hdc, int x1, int y1, int x2, int y2)
{
    RECT r;
    r.left = x1;
    r.top = y1;
    r.right = x2;
    r.bottom = y2;
    return FastFillRect(hdc, r);
}
