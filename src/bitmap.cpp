// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "bitmap.h"

//*****************************************************************************
//
// CBitmap
//

CBitmap::CBitmap()
{
    Cleanup();
}

void CBitmap::Cleanup()
{
    HBmp = NULL;
    Width = 0;
    Height = 0;
    Planes = 0;
    BitsPerPel = 0;
    BlackAndWhite = FALSE;
    HMemDC = NULL;
    HOldBmp = NULL;
}

CBitmap::~CBitmap()
{
    Destroy();
}

void CBitmap::Destroy()
{
    if (HMemDC != NULL)
    {
        if (HOldBmp != NULL)
            SelectObject(HMemDC, HOldBmp);
        HANDLES(DeleteDC(HMemDC));
    }
    if (HBmp != NULL)
        HANDLES(DeleteObject(HBmp));

    Cleanup();
}

BOOL CBitmap::CreateBmpBW(int width, int height)
{
    int planes = 1;
    int bitsPerPel = 1;
    if (HBmp != NULL)
    {
        TRACE_E("Bitmap already exists");
        return FALSE;
    }
    if (width < 1)
    {
        TRACE_E("Changing width of bitmap");
        width = 1;
    }
    if (height < 1)
    {
        TRACE_E("Changing height of bitmap");
        height = 1;
    }

    HBmp = HANDLES(CreateBitmap(width, height, planes, bitsPerPel, NULL));
    if (HBmp == NULL)
    {
        TRACE_E("CreateBitmap failed!");
        return FALSE;
    }
    Planes = planes;
    BitsPerPel = bitsPerPel;
    Width = width;
    Height = height;

    if (HMemDC == NULL)
    {
        HMemDC = HANDLES(CreateCompatibleDC(NULL));
        if (HMemDC == NULL)
        {
            TRACE_E("CreateCompatibleDC failed!");
            return FALSE;
        }
        HOldBmp = (HBITMAP)SelectObject(HMemDC, HBmp);
    }
    BlackAndWhite = TRUE;
    return TRUE;
}

// creates a DC compatible bitmap
BOOL CBitmap::CreateBmp(HDC hDC, int width, int height)
{
    if (HBmp != NULL)
    {
        TRACE_E("Bitmap already exists");
        return FALSE;
    }
    if (width < 1)
    {
        TRACE_E("Changing width of bitmap");
        width = 1;
    }
    if (height < 1)
    {
        TRACE_E("Changing height of bitmap");
        height = 1;
    }
    BOOL releaseDC = FALSE;
    if (hDC == NULL)
    {
        hDC = HANDLES(GetDC(NULL));
        if (hDC == NULL)
            return FALSE;
        releaseDC = TRUE;
    }
    HBmp = HANDLES(CreateCompatibleBitmap(hDC, width, height));
    if (HBmp == NULL)
    {
        TRACE_E("CreateCompatibleBitmap failed!");
        if (releaseDC)
            HANDLES(ReleaseDC(NULL, hDC));
        return FALSE;
    }
    if (releaseDC)
        HANDLES(ReleaseDC(NULL, hDC));

    // fetch information for later changes
    BITMAP bmp;
    GetObject(HBmp, sizeof(bmp), &bmp);
    Planes = bmp.bmPlanes;
    BitsPerPel = bmp.bmBitsPixel;
    Width = width;
    Height = height;

    if (HMemDC == NULL)
    {
        HMemDC = HANDLES(CreateCompatibleDC(NULL));
        if (HMemDC == NULL)
        {
            TRACE_E("CreateCompatibleDC failed!");
            return FALSE;
        }
        HOldBmp = (HBITMAP)SelectObject(HMemDC, HBmp);
    }
    return TRUE;
}

BOOL CBitmap::ReCreateForScreenDC(int width, int height)
{
    if (HBmp == NULL)
    {
        TRACE_E("Bitmap does not exist yet!");
        return FALSE;
    }
    HDC hDC = HANDLES(GetDC(NULL));
    if (hDC == NULL)
        return FALSE;

    if (width == -1 || width < Width)
        width = Width;
    if (height == -1 || height < Height)
        height = Height;

    HBITMAP hTmpBmp = HANDLES(CreateCompatibleBitmap(hDC, width, height));
    if (hTmpBmp == NULL)
    {
        TRACE_E("CreateCompatibleBitmap failed!");
        HANDLES(ReleaseDC(NULL, hDC));
        return FALSE;
    }
    if (HMemDC != NULL && HOldBmp != NULL)
        SelectObject(HMemDC, HOldBmp);
    HANDLES(DeleteObject(HBmp));
    HBmp = hTmpBmp;
    Width = width;
    Height = height;

    // fetch information for later changes
    BITMAP bmp;
    GetObject(HBmp, sizeof(bmp), &bmp);
    Planes = bmp.bmPlanes;
    BitsPerPel = bmp.bmBitsPixel;

    if (HMemDC != NULL)
        HOldBmp = (HBITMAP)SelectObject(HMemDC, HBmp);

    HANDLES(ReleaseDC(NULL, hDC));
    return TRUE;
}

BOOL CBitmap::NeedEnlarge(int width, int height)
{
    if (HBmp == NULL)
    {
        TRACE_E("Bitmap does not exist yet!");
        return FALSE;
    }
    if (width > Width || height > Height)
        return TRUE;
    return FALSE;
}

BOOL CBitmap::Enlarge(int width, int height)
{
    if (HBmp == NULL)
    {
        TRACE_E("Bitmap does not exist yet!");
        return FALSE;
    }
    if (width > Width || height > Height)
    {
        if (Width > width)
            width = Width;
        if (Height > height)
            height = Height;
        // try to create a larger bitmap
        // !CAUTION! Sal 2.5b6 was slow when painting to Viewer (PgDn) compared to 2.0
        // Caused by creating the cache with CreateBitmap() instead of CreateCompatibleBitmap()
        // According to MSDN, CreateBitmap() should only be used for B&W bitmaps.
        HBITMAP hTmpBmp;
        if (HMemDC == NULL || BlackAndWhite)
        {
            if (!BlackAndWhite)
                TRACE_E("HMemDC == NULL, using slow CreateBitmap()");
            hTmpBmp = HANDLES(CreateBitmap(width, height, Planes, BitsPerPel, NULL));
        }
        else
        {
            hTmpBmp = HANDLES(CreateCompatibleBitmap(HMemDC, width, height));
        }
        if (hTmpBmp == NULL)
        {
            TRACE_E("CreateCompatibleBitmap or CreateBitmap failed!");
            return FALSE;
        }
        if (HMemDC != NULL)
        {
            if (HOldBmp != NULL)
                SelectObject(HMemDC, HOldBmp);
        }
        HANDLES(DeleteObject(HBmp));
        HBmp = hTmpBmp;
        Width = width;
        Height = height;
        if (HMemDC != NULL)
        {
            HOldBmp = (HBITMAP)SelectObject(HMemDC, HBmp);
        }
    }
    return TRUE;
}
