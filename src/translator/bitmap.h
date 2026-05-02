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

    // Destroy the bitmap and the DC
    void Destroy();

    // Create a bitmap compatible with the DC (if hDC == NULL, it will be compatible with the screen)
    BOOL CreateBmp(HDC hDC, int width, int height);
    // Create a bitmap
    BOOL CreateBmpBW(int width, int height);
    // Load a bitmap from a resource (it will be a DDB compatible with the screen)
    BOOL CreateBmpFrom(HINSTANCE hInstance, int resID);
    // If the screen color depth changed, the bitmap has to be recreated
    // The size and selected handles remain intact; the bitmap will be compatible with the screen
    BOOL ReCreateForScreenDC();

    // Enlarge the bitmap to the requested size; if it is already large enough,
    // it does not shrink it—just returns TRUE
    BOOL Enlarge(int width, int height);
    // Return TRUE if the bitmap needs to be enlarged
    BOOL NeedEnlarge(int width, int height);

    DWORD GetWidth() { return Width; }
    DWORD GetHeight() { return Height; }

protected:
    void Cleanup();
};
