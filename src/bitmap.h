// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define ROP_DSPDxax 0x00E20746
#define ROP_PSDPxax 0x00B8074A

//*****************************************************************************
//
// CBitmap
//

class CBitmap
{
public:
    HBITMAP HBmp;

    HDC HMemDC;
    HBITMAP HOldBmp;

protected:
    int Width;
    int Height;
    DWORD Planes;
    DWORD BitsPerPel;
    BOOL BlackAndWhite;

public:
    CBitmap();
    ~CBitmap();

    // destruction of both bitmap and DC
    void Destroy();

    // creates a DC compatible bitmap (will be compatible with screen, if hDC==NULL)
    BOOL CreateBmp(HDC hDC, int width, int height);
    // creates a bitmap
    BOOL CreateBmpBW(int width, int height);
    // loads a bitmap from resource (DDB will be compatible with the screen)
    BOOL CreateBmpFrom(HINSTANCE hInstance, int resID);
    // if a change of screen color depth is changed, the bitmap has to be created again
    // the dimensions will be enlarged and the selected handles will be kept; the bitmap
    // will be compatible with the screen
    BOOL ReCreateForScreenDC(int width = -1, int height = -1);

    // enlarges the bitmap to desired size; if the bitmap is large enough already, it won't
    // be shrinked - just returns TRUE
    BOOL Enlarge(int width, int height);
    // returns TRUE, if the bitmap needs to be enlarged
    BOOL NeedEnlarge(int width, int height);

    DWORD GetWidth() { return Width; }
    DWORD GetHeight() { return Height; }

protected:
    void Cleanup();
};
