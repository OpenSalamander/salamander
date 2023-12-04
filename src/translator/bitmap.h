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

    // destukce bitmapy i DC
    void Destroy();

    // vytvori bitmapu kompatibilni s DC (pokud je hDC==NULL, bude kompatibili s obrazovkou)
    BOOL CreateBmp(HDC hDC, int width, int height);
    // vytvori bitmapu
    BOOL CreateBmpBW(int width, int height);
    // nacte bitmapu z resource (pujde o DDB kompatibilni s obrazovkou)
    BOOL CreateBmpFrom(HINSTANCE hInstance, int resID);
    // pokud doslo k zemene barevne hloubky obrazovky, je treba znovu vytvorit bitmapu
    // rozmery a vybrane handly zustanou zachovany; bitmapa bude kompatibilni s obrazovkou
    BOOL ReCreateForScreenDC();

    // zvetsi bitampu na pozadovanou velikost; pokud je uz bitmapa dostatecne
    // velika, nezmensuje ji - pouze vrati TRUE
    BOOL Enlarge(int width, int height);
    // vrati TRUE, pokud je treba bitmapu zvetsit
    BOOL NeedEnlarge(int width, int height);

    DWORD GetWidth() { return Width; }
    DWORD GetHeight() { return Height; }

protected:
    void Cleanup();
};
